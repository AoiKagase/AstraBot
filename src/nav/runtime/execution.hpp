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
    ExecutionState state{ExecutionState::Idle};
    ExecutionFailure failure{ExecutionFailure::None};
    std::optional<query::NavDirectedEdge> failedEdge{};
    std::uint64_t retryAtUs{};
    std::uint64_t nextSearchAtUs{};

    void begin() noexcept {
        state=ExecutionState::Planning; failure=ExecutionFailure::None;
        failedEdge.reset(); retryAtUs=0;
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
    }
    bool cooling(model::NavAreaId goal,std::uint64_t now) const noexcept {
        for(const auto& item:goals_) if(item.goal==goal && now<item.until) return true;
        return false;
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
    bool canSearch(std::uint64_t now) const noexcept { return !saturated_ && now>=nextSearchAtUs; }
private:
    struct GoalRetry { model::NavAreaId goal{}; std::uint64_t until{}; };
    std::array<GoalRetry,16> goals_{};
    std::size_t nextGoal_{};
    std::array<query::NavDirectedEdge,128> blocked_{};
    std::size_t blockedCount_{};
    bool saturated_{};
};

// Compose exclusions with existing dynamic/experience costs; never replace them.
struct ExecutionPolicy final {
    const Execution* execution{};
    query::NavRoutePolicy base{};
    static query::NavCostDecision cost(const query::NavCostContext& c,const void* context) {
        const auto& self=*static_cast<const ExecutionPolicy*>(context);
        if(self.execution && (self.execution->saturated() || self.execution->blocked(c.edge)))
            return {true,{}};
        return self.base.cost ? self.base.cost(c,self.base.context) :
            query::NavCostDecision{false,{c.geometricDistance,0,0,
                c.edge.external ? c.edge.external->additionalCost : 0,0}};
    }
    query::NavRoutePolicy policy() const noexcept { return {this,&cost,nullptr}; }
};
}
