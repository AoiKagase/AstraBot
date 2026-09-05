// SPDX-License-Identifier: MPL-2.0
#include "nav/runtime/route_session.hpp"
#include <cmath>
#include <limits>
#include <new>

namespace astrabot::nav::runtime {
namespace {
SessionReason health(const MovementSnapshot& s) noexcept {
    if (!s.tick.isValid()) return SessionReason::InvalidSnapshot;
    if (!s.connected || !s.alive || !s.joined) return SessionReason::InvalidSnapshot;
    if (s.connected != true) return SessionReason::Disconnected;
    if (s.alive != true) return SessionReason::Dead;
    if (s.joined != true) return SessionReason::NotJoined;
    if (!s.position || !s.position->isFinite() || (s.velocity && !s.velocity->isFinite()) ||
        (s.view && !s.view->isFinite()) ||
        (s.speedLimit && (!std::isfinite(*s.speedLimit) || *s.speedLimit < 0)))
        return SessionReason::InvalidSnapshot;
    if (s.hull) {
        const auto& h=*s.hull;
        if (!h.minimum.isFinite() || !h.maximum.isFinite() || h.minimum.x>=h.maximum.x ||
            h.minimum.y>=h.maximum.y || h.minimum.z>=h.maximum.z) return SessionReason::InvalidSnapshot;
    }
    return SessionReason::None;
}
struct BusyGuard {
    bool& busy;
    explicit BusyGuard(bool& value) noexcept : busy(value) { busy=true; }
    ~BusyGuard() { busy=false; }
};
SessionUpdate rejected(SessionReason reason) noexcept { SessionUpdate r; r.reason=reason; return r; }
} // namespace
RouteSession::RouteSession(core::BotAgentId agent, core::PlayerId actor, core::MapGeneration map) noexcept {
    trace_.agent=agent; trace_.actor=actor; trace_.map=map;
}
SessionReason RouteSession::identity(const MovementSnapshot& s) const noexcept {
    if (!trace_.agent.isValid() || !trace_.actor.isValid() || !trace_.map.isValid() ||
        s.kind!=ActorKind::ManagedBot || !s.agent.isValid() || !s.actor.isValid()) return SessionReason::InvalidActor;
    if (s.agent!=trace_.agent || s.actor!=trace_.actor) return SessionReason::ActorChanged;
    if (s.map!=trace_.map) return SessionReason::MapChanged;
    return SessionReason::None;
}
bool RouteSession::executable() const noexcept {
    return !busy_ && trace_.state==SessionState::Ready && trace_.route &&
        trace_.route->status==query::NavRouteStatus::Complete;
}
SessionUpdate RouteSession::cancelFor(SessionReason reason) noexcept {
    if (trace_.state!=SessionState::Ready) return {};
    trace_.state=SessionState::Cancelled; trace_.reason=reason; trace_.terminal=true;
    graph_.reset();
    SessionUpdate update; update.accepted=true; update.reason=reason;
    update.events[0]=trace_; update.count=1; return update;
}
SessionUpdate RouteSession::cancel() noexcept {
    if (busy_) return rejected(SessionReason::QueryFailed);
    return cancelFor(SessionReason::Cancelled);
}
SessionUpdate RouteSession::observe(const MovementSnapshot& s) noexcept {
    if (busy_) return rejected(SessionReason::QueryFailed);
    if (retired_!=SessionReason::None) return rejected(retired_);
    if (!s.tick.isValid()) return rejected(SessionReason::InvalidSnapshot);
    if (s.tick < latestTick_) return rejected(SessionReason::StaleSnapshot);
    auto reason=identity(s);
    if (reason!=SessionReason::None) {
        if (reason==SessionReason::ActorChanged || reason==SessionReason::MapChanged) retired_=reason;
        trace_.tick=s.tick; trace_.elapsedUs=s.elapsedUs; latestTick_=s.tick;
        auto result=cancelFor(reason);
        if (!result.count) return rejected(reason);
        return result;
    }
    reason=health(s);
    if (s.tick.isValid()) { latestTick_=s.tick; trace_.tick=s.tick; trace_.elapsedUs=s.elapsedUs; }
    if (reason!=SessionReason::None) return cancelFor(reason);
    SessionUpdate result; result.accepted=true; return result;
}
SessionUpdate RouteSession::request(const MovementSnapshot& s, model::NavAreaId goal,
    const NavigationSnapshot& navigation, IWorldQueries& port, const RouteOptions& options) noexcept {
    if (busy_) return rejected(SessionReason::QueryFailed);
    if (retired_!=SessionReason::None) return rejected(retired_);
    const auto who=identity(s);
    if (who!=SessionReason::None) return rejected(who);
    if (s.tick < latestTick_) return rejected(SessionReason::StaleSnapshot);
    auto update=cancelFor(SessionReason::GoalReplaced);
    BusyGuard busy(busy_);
    const auto generation=trace_.routeGeneration;
    trace_={}; trace_.agent=s.agent; trace_.actor=s.actor; trace_.map=s.map;
    trace_.tick=s.tick; trace_.elapsedUs=s.elapsedUs; trace_.goal=goal;
    trace_.routeGeneration=generation;
    graph_.reset();
    const auto fail=[&](SessionReason reason) {
        trace_.state=SessionState::Failed; trace_.reason=reason; trace_.terminal=true;
        update.accepted=false; update.reason=reason; update.events[update.count++]=trace_;
        return update;
    };
    if (generation==std::numeric_limits<std::uint64_t>::max()) return fail(SessionReason::GenerationExhausted);
    ++trace_.routeGeneration;
    if (s.tick.isValid()) latestTick_=s.tick;
    const auto valid=health(s);
    if (valid!=SessionReason::None) return fail(valid);
    if (navigation.map!=s.map) return fail(SessionReason::MapChanged);
    if (!navigation.graph) return fail(SessionReason::MissingGraph);
    graph_=navigation.graph;
    if (!goal.isValid() || !graph_->find(goal)) return fail(SessionReason::InvalidGoal);
    if (s.grounded!=true) return fail(SessionReason::UnknownGround);
    if (!options.maxWorldQueries) return fail(SessionReason::QueryBudgetExceeded);
    if(!std::isfinite(options.groundNavTolerance) || options.groundNavTolerance<0)
        return fail(SessionReason::InvalidSnapshot);
    const QueryRequest request{{s.agent,s.actor,s.map,s.tick,trace_.routeGeneration,1},
        QueryKind::GroundedArea,*s.position,*s.position,s.hull,options.groundNavTolerance};
    try {
        const auto reply=port.query(request);
        if (!(reply.stamp==request.stamp) || reply.kind!=request.kind) return fail(SessionReason::StaleQuery);
        if (reply.error==QueryError::BudgetExceeded) return fail(SessionReason::QueryBudgetExceeded);
        if (reply.error!=QueryError::None) return fail(SessionReason::QueryFailed);
        if (!reply.ground || !reply.ground->floor || !reply.ground->floor->supported)
            return fail(SessionReason::UnknownGround);
        const auto& floor=*reply.ground->floor;
        if (!std::isfinite(floor.height) || !floor.normal.isFinite() ||
            (floor.normal.x==0 && floor.normal.y==0 && floor.normal.z==0)) return fail(SessionReason::QueryFailed);
        if (!reply.ground->area || !reply.ground->area->isValid() || !graph_->find(*reply.ground->area))
            return fail(SessionReason::NoCurrentArea);
        trace_.currentArea=reply.ground->area;
        auto route=query::NavRouteSearch::search(*graph_, {*trace_.currentArea,goal,options.limits,options.diagnosticPartial}, options.policy);
        if (!route) { trace_.navError=route.error; return fail(SessionReason::NavFailure); }
        trace_.route=std::make_shared<const query::NavRouteResult>(std::move(*route.value));
        if (trace_.route->status==query::NavRouteStatus::Unreachable) return fail(SessionReason::Unreachable);
        if (trace_.route->status==query::NavRouteStatus::ExpansionLimit) return fail(SessionReason::ExpansionLimit);
        trace_.state=SessionState::Ready; update.accepted=true; update.reason=SessionReason::None;
        update.events[update.count++]=trace_; return update;
    } catch (const std::bad_alloc&) { return fail(SessionReason::AllocationFailure); }
      catch (...) { return fail(SessionReason::QueryFailed); }
}
} // namespace astrabot::nav::runtime
