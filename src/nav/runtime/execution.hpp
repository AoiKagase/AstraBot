// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "nav/query/route_types.hpp"
#include <array>
#include <algorithm>
#include <limits>

namespace astrabot::nav::runtime {
// Search completion is not evidence that a motion primitive can execute it.
enum class ExecutionState { Idle, Planning, Running, Arrived, Failed, Recovering };
enum class ExecutionFailure { None, Search, Corridor, Motion, Observation, Transport, ExclusionCapacity };

class Execution final {
public:
    static constexpr std::uint64_t retryDelayUs = 2'000'000;
    static constexpr std::uint64_t edgeRetryDelayUs = 2'000'000;
    ExecutionState state{ExecutionState::Idle};
    ExecutionFailure failure{ExecutionFailure::None};
    std::optional<query::NavDirectedEdge> failedEdge{};
    std::uint64_t retryAtUs{};
    std::uint64_t nextSearchAtUs{};

    void setSearchTime(std::uint64_t now) noexcept { searchNowUs_=now; }

    void begin() noexcept {
        state=ExecutionState::Planning; failure=ExecutionFailure::None;
        failedEdge.reset(); retryAtUs=0;
        // A new route must not erase a still-cooling directed edge. Expiry is
        // evaluated by edgeCooling() against the current synchronous search.
    }
    void fail(model::NavAreaId goal, ExecutionFailure why, std::uint64_t now,
              std::optional<query::NavDirectedEdge> edge={}, bool structural=false) noexcept {
        state=ExecutionState::Failed; failure=why; failedEdge=edge;
        retryAtUs=now>(std::numeric_limits<std::uint64_t>::max)()-retryDelayUs
            ? (std::numeric_limits<std::uint64_t>::max)() : now+retryDelayUs;
        nextSearchAtUs=now>(std::numeric_limits<std::uint64_t>::max)()-250'000
            ? (std::numeric_limits<std::uint64_t>::max)() : now+250'000;
        if(goal.isValid()) {
            auto* slot=&goals_[nextGoal_];
            for(auto& item:goals_) if(item.goal==goal) { slot=&item; break; }
            *slot={goal,retryAtUs}; nextGoal_=(nextGoal_+1)%goals_.size();
        }
        if(structural && edge && !blocked(*edge)) {
            // Bounded and fail-closed: never silently evict a known bad edge.
            if(blockedCount_<blocked_.size()) blocked_[blockedCount_++]=*edge;
            else { saturated_=true; failure=ExecutionFailure::ExclusionCapacity; }
        }
        if(!structural && edge) coolEdge(*edge,now);
    }
    bool cooling(model::NavAreaId goal,std::uint64_t now) const noexcept {
        for(const auto& item:goals_) if(item.goal==goal && now<item.until) return true;
        return false;
    }

    bool edgeCooling(const query::NavDirectedEdge& edge) const noexcept {
        for(const auto& item:edgeRetries_) {
            if(item.edge && sameEdge(*item.edge,edge) && searchNowUs_<item.until) return true;
        }
        return false;
    }

    std::uint64_t edgeCooldownRemaining(const query::NavDirectedEdge& edge,
                                        std::uint64_t now) const noexcept {
        for(const auto& item:edgeRetries_) if(item.edge && sameEdge(*item.edge,edge))
            return item.until>now ? item.until-now : 0;
        return 0;
    }

    void clearEdgeCooldown(const query::NavDirectedEdge& edge) noexcept {
        for(auto& item:edgeRetries_) if(item.edge && sameEdge(*item.edge,edge)) {
            item.edge.reset(); item.until=0; return;
        }
    }
    bool blocked(const query::NavDirectedEdge& edge) const noexcept {
        for(std::size_t i=0;i<blockedCount_;++i) {
            const auto& b=blocked_[i];
            if(b.source==edge.source && b.target==edge.target && b.direction==edge.direction &&
               b.traversal==edge.traversal && b.external.has_value()==edge.external.has_value() &&
               (!b.external || (b.external->sourceId==edge.external->sourceId &&
                b.external->generation==edge.external->generation &&
                b.external->linkId==edge.external->linkId))) return true;
        }
        return false;
    }
    bool saturated() const noexcept { return saturated_; }
    bool canSearch(std::uint64_t now) const noexcept {
        searchNowUs_=now;
        return !saturated_ && now>=nextSearchAtUs;
    }
private:
    struct EdgeRetry { std::optional<query::NavDirectedEdge> edge{}; std::uint64_t until{}; };
    static bool sameEdge(const query::NavDirectedEdge& a,const query::NavDirectedEdge& b) noexcept {
        return a.source==b.source && a.target==b.target && a.direction==b.direction &&
            a.traversal==b.traversal && a.external.has_value()==b.external.has_value() &&
            (!a.external || (a.external->sourceId==b.external->sourceId &&
                a.external->generation==b.external->generation && a.external->linkId==b.external->linkId));
    }
    void coolEdge(const query::NavDirectedEdge& edge,std::uint64_t now) noexcept {
        for(auto& item:edgeRetries_) if(item.edge && sameEdge(*item.edge,edge)) {
            item.until=now>(std::numeric_limits<std::uint64_t>::max)()-edgeRetryDelayUs
                ? (std::numeric_limits<std::uint64_t>::max)() : now+edgeRetryDelayUs;
            return;
        }
        for(auto& item:edgeRetries_) if(!item.edge || item.until<=now) {
            item={edge,now>(std::numeric_limits<std::uint64_t>::max)()-edgeRetryDelayUs
                ? (std::numeric_limits<std::uint64_t>::max)() : now+edgeRetryDelayUs};
            return;
        }
        // The temporary table is bounded. Preserve the oldest entry so a
        // burst of actors cannot turn a transient failure into an unbounded
        // allocation or erase a newer failure.
        auto oldest=std::min_element(edgeRetries_.begin(),edgeRetries_.end(),
            [](const auto& a,const auto& b){ return a.until<b.until; });
        *oldest={edge,now>(std::numeric_limits<std::uint64_t>::max)()-edgeRetryDelayUs
            ? (std::numeric_limits<std::uint64_t>::max)() : now+edgeRetryDelayUs};
    }
    struct GoalRetry { model::NavAreaId goal{}; std::uint64_t until{}; };
    std::array<GoalRetry,16> goals_{};
    std::size_t nextGoal_{};
    std::array<query::NavDirectedEdge,128> blocked_{};
    std::size_t blockedCount_{};
    bool saturated_{};
    std::array<EdgeRetry,128> edgeRetries_{};
    mutable std::uint64_t searchNowUs_{};
};

// Compose exclusions with existing dynamic/experience costs; never replace them.
struct ExecutionPolicy final {
    const Execution* execution{};
    query::NavRoutePolicy base{};
    static query::NavCostDecision cost(const query::NavCostContext& c,const void* context) {
        const auto& self=*static_cast<const ExecutionPolicy*>(context);
        if(self.execution && (self.execution->saturated() || self.execution->blocked(c.edge) ||
            self.execution->edgeCooling(c.edge)))
            return {true,{}};
        return self.base.cost ? self.base.cost(c,self.base.context) :
            query::NavCostDecision{false,{c.geometricDistance,0,0,
                c.edge.external ? c.edge.external->additionalCost : 0,0}};
    }
    query::NavRoutePolicy policy() const noexcept { return {this,&cost,nullptr}; }
};
}
