// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "nav/runtime/world_query.hpp"
#include "nav/query/route_search.hpp"
#include <array>

namespace astrabot::nav::runtime {
// Caller publishes the graph with its map generation. Graph itself retains its mesh.
struct NavigationSnapshot {
    core::MapGeneration map{};
    std::shared_ptr<const query::NavGraph> graph{};
};
enum class SessionState { Idle, Ready, Failed, Cancelled };
enum class SessionReason {
    None, InvalidActor, ActorChanged, MapChanged, Disconnected, Dead, NotJoined,
    InvalidSnapshot, StaleSnapshot, MissingGraph, InvalidGoal, QueryBudgetExceeded,
    StaleQuery, QueryFailed, UnknownGround, NoCurrentArea, Unreachable,
    ExpansionLimit, NavFailure, AllocationFailure, GoalReplaced, Cancelled, GenerationExhausted
};
struct RouteOptions {
    query::NavRouteLimits limits{}; // No implicit expansion/memory allowance.
    std::uint32_t maxWorldQueries{1};
    bool diagnosticPartial{false};
    query::NavRoutePolicy policy{}; // Pure policy context borrowed only during request().
};
struct DecisionTrace {
    core::BotAgentId agent{};
    core::PlayerId actor{};
    core::MapGeneration map{};
    core::TickId tick{};
    std::uint64_t routeGeneration{}, elapsedUs{};
    model::NavAreaId goal{};
    std::optional<model::NavAreaId> currentArea{};
    SessionState state{SessionState::Idle};
    SessionReason reason{SessionReason::None};
    bool terminal{};
    diagnostics::NavError navError{};
    // Immutable owned selected edges; cheap trace copies outlive graph/session cancellation.
    std::shared_ptr<const query::NavRouteResult> route{};
};
struct SessionUpdate {
    bool accepted{};
    SessionReason reason{SessionReason::None};
    // At most an old-generation cancellation plus one new-generation outcome.
    std::array<DecisionTrace,2> events{};
    std::size_t count{};
};
// Single-thread owner. A changed actor/map observation retires this instance;
// host lifecycle must construct a session for the new identity. No SDK callbacks
// or movement commands are retained. Requests at the same tick are explicit goals.
class RouteSession final {
public:
    RouteSession(core::BotAgentId agent, core::PlayerId actor, core::MapGeneration map) noexcept;
    SessionUpdate request(const MovementSnapshot&, model::NavAreaId goal,
                          const NavigationSnapshot&, IWorldQueries&, const RouteOptions&) noexcept;
    SessionUpdate observe(const MovementSnapshot&) noexcept;
    SessionUpdate cancel() noexcept;
    bool executable() const noexcept;
    const DecisionTrace& trace() const noexcept { return trace_; }
    std::size_t cursor() const noexcept { return 0; } // Portal cursor advancement belongs to P3-02.
private:
    DecisionTrace trace_{};
    std::shared_ptr<const query::NavGraph> graph_{};
    core::TickId latestTick_{};
    SessionReason retired_{SessionReason::None};
    bool busy_{};
    SessionUpdate cancelFor(SessionReason) noexcept;
    SessionReason identity(const MovementSnapshot&) const noexcept;
};
} // namespace astrabot::nav::runtime
