// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "nav/local/primitive.hpp"
#include "nav/query/route_types.hpp"

namespace astrabot::nav::runtime {
enum class ReplanState { Idle, Pending, Consumed, Exhausted, Expired, Invalid };
// One goal owns one automatic retry. Route generation changes never replenish
// it. The host resets this value only on explicit goal/lifecycle invalidation.
class ReplanAttempt final {
public:
    static constexpr std::uint64_t factLifetimeUs=1000000;
    static constexpr unsigned maxAttempts=1;
    struct PolicySnapshot {
        std::optional<query::NavDirectedEdge> blocked{};
        static query::NavCostDecision cost(const query::NavCostContext& c,const void* context) noexcept {
            const auto& self=*static_cast<const PolicySnapshot*>(context);
            return {self.blocked && sameEdge(*self.blocked,c.edge),
                {c.geometricDistance,c.edge.external ? c.edge.external->additionalCost:0,0,0}};
        }
        query::NavRoutePolicy policy() const noexcept { return {this,&cost,nullptr}; }
    };
    bool schedule(local::Binding binding,query::NavDirectedEdge edge,core::TickId tick,
                  std::uint64_t nowUs) noexcept {
        if(state_==ReplanState::Pending) return false;
        if(attempts_) { state_=ReplanState::Exhausted; return false; }
        if(!binding.agent.isValid() || !binding.actor.isValid() || !binding.map.isValid() ||
           !binding.routeGeneration || !tick.isValid() || !edge.source.isValid() || !edge.target.isValid()) {
            state_=ReplanState::Invalid; return false;
        }
        binding_=binding; edge_=edge; tick_=tick; observedUs_=nowUs; state_=ReplanState::Pending;
        return true;
    }
    std::optional<PolicySnapshot> consume(local::Binding binding,core::TickId tick,std::uint64_t nowUs) noexcept {
        if(state_!=ReplanState::Pending) return {};
        if(binding.agent!=binding_.agent || binding.actor!=binding_.actor || binding.map!=binding_.map ||
           binding.routeGeneration!=binding_.routeGeneration || binding.step!=binding_.step || nowUs<observedUs_) {
            state_=ReplanState::Invalid; edge_.reset(); return {};
        }
        if(!tick.isAfter(tick_)) return {};
        if(nowUs-observedUs_>=factLifetimeUs) { state_=ReplanState::Expired; edge_.reset(); return {}; }
        ++attempts_; state_=ReplanState::Consumed;
        return PolicySnapshot{edge_};
    }
    PolicySnapshot snapshot(core::MapGeneration map,std::uint64_t nowUs) const noexcept {
        return {map==binding_.map && nowUs>=observedUs_ && nowUs-observedUs_<factLifetimeUs ? edge_:std::nullopt};
    }
    ReplanState state() const noexcept { return state_; }
    unsigned attempts() const noexcept { return attempts_; }
private:
    static bool sameEdge(const query::NavDirectedEdge& a,const query::NavDirectedEdge& b) noexcept {
        if(a.source!=b.source || a.target!=b.target || a.traversal!=b.traversal || a.external.has_value()!=b.external.has_value()) return false;
        if(!a.external) return a.direction==b.direction;
        return a.external->sourceId==b.external->sourceId && a.external->generation==b.external->generation &&
            a.external->linkId==b.external->linkId && a.external->direction==b.external->direction;
    }
    local::Binding binding_{};
    std::optional<query::NavDirectedEdge> edge_{};
    core::TickId tick_{};
    std::uint64_t observedUs_{};
    unsigned attempts_{};
    ReplanState state_{ReplanState::Idle};
};
}
