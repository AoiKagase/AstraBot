// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "adapter/metamod/plugin_entry.hpp"
#include "nav/runtime/route_session.hpp"
#include "nav/query/spatial_index.hpp"
#include "nav/local/walk.hpp"
#include "nav/local/intent_pump.hpp"
#include "adapter/metamod/movement.hpp"
namespace astrabot::adapter::metamod { class LifecycleCoordinator; }
namespace astrabot::adapter::cstrike {
enum class NavCommand { Load, GoTo, Status, Cancel };
enum class MotionEvent { None, Decision, Queued, Dispatched, Rejected, Cancelled };
enum class MotionReason { None, InvalidCorridor, InvalidGoal, MissingObservation,
    StaleCommand, Deviation, MotorRejected, TransportRejected, Cancelled, DoorChanged };
struct MotionTrace {
    nav::local::WalkDecision decision{};
    std::optional<nav::query::NavDirectedEdge> selectedEdge{};
    core::BotCommand command{}; // Queued command; msec is a hint, transport measures dispatch.
    MotionEvent event{MotionEvent::None};
    MotionReason reason{MotionReason::None};
    nav::corridor::Error corridorError{nav::corridor::Error::None};
    metamod::MovementError transportError{metamod::MovementError::None};
    core::TickId commandTick{}, dispatchTick{};
    std::uint64_t intentAgeUs{}, missedDecisions{}, queued{}, dispatched{}, rejected{}, sequence{};
    std::uint64_t useGuardChecks{};
    std::uint64_t contactGuardQueries{};
};
class NavConsole final : public nav::runtime::IWorldQueries {
public:
    void bindMovement(metamod::MovementCoordinator* movement) noexcept { movement_=movement; }
    void configure(enginefuncs_t*, mutil_funcs_t*, globalvars_t*) noexcept;
    void reset() noexcept;
    void invalidate(nav::runtime::SessionReason) noexcept;
    void observe(metamod::LifecycleCoordinator&) noexcept;
    void beforeDispatch(metamod::LifecycleCoordinator&) noexcept;
    std::optional<MotionTrace> dispatchTicket() const noexcept {
        return pendingMotion_ ? std::optional<MotionTrace>{motionTrace_}:std::nullopt;
    }
    void afterDispatch(const metamod::MovementResult&, core::TickId,const std::optional<MotionTrace>&) noexcept;
    void moveFrame(metamod::LifecycleCoordinator&) noexcept;
    void execute(NavCommand, metamod::LifecycleCoordinator&) noexcept;
    // Publication binds an independently obtained immutable mesh to the current map.
    nav::diagnostics::NavError publish(core::MapGeneration,
        std::shared_ptr<const nav::model::NavMeshSnapshot>) noexcept;
    const nav::runtime::DecisionTrace* trace() const noexcept { return session_ ? &session_->trace():nullptr; }
    const MotionTrace& motionTrace() const noexcept { return motionTrace_; }
    static constexpr std::size_t motionHistoryLimit=128;
    std::size_t motionHistoryCount() const noexcept { return motionCount_; }
    const MotionTrace* motionHistory(std::size_t index) const noexcept {
        return index<motionCount_ ? &motionHistory_[(motionNext_+motionHistoryLimit-motionCount_+index)%motionHistoryLimit]:nullptr;
    }
    nav::runtime::WorldQueryResult query(const nav::runtime::QueryRequest&) override;
private:
    nav::runtime::MovementSnapshot snapshot(metamod::LifecycleCoordinator&) noexcept;
    void printUpdate(const nav::runtime::SessionUpdate&) noexcept;
    void line(const char*) noexcept;
    static void sink(void*,const char*) noexcept;
    bool load(const char*,core::MapGeneration) noexcept;
    void startMotion(const nav::runtime::MovementSnapshot&) noexcept;
    void stopMotion() noexcept;
    void clearPending() noexcept;
    void recordMotion(MotionEvent, MotionReason=MotionReason::None) noexcept;
    void printMotion() noexcept;
    void submitMotion(const nav::runtime::MovementSnapshot&,metamod::LifecycleCoordinator&,
        const core::MovementIntent&,bool firstFrame,std::uint64_t age) noexcept;
    struct Segment { nav::model::NavVector3 start{}, end{}; };
    struct PendingMotion {
        nav::local::Binding binding{};
        core::TickId tick{};
        std::uint64_t remainingFreshUs{};
        nav::runtime::MovementSnapshot observation{};
        core::BotCommand command{};
        std::optional<Segment> segment{};
        std::optional<nav::local::DoorContact> contact{};
    };
    metamod::MovementCoordinator* movement_{}; // Owned by the containing lifecycle coordinator.
    std::optional<nav::local::Walk> walk_{};
    std::optional<nav::local::IntentPump> pump_{};
    std::optional<Segment> segment_{};
    std::optional<PendingMotion> pendingMotion_{};
    std::optional<nav::local::Binding> neutralBinding_{};
    core::TickId requestTick_{};
    core::TickId guardTick_{};
    std::uint32_t guardQueries_{};
    std::uint64_t intentWallAgeUs_{};
    MotionTrace motionTrace_{};
    std::array<MotionTrace,motionHistoryLimit> motionHistory_{};
    std::size_t motionNext_{}, motionCount_{};
    std::uint64_t motionSequence_{};
    enginefuncs_t* engine_{};
    mutil_funcs_t* utility_{};
    globalvars_t* globals_{};
    nav::runtime::NavigationSnapshot navigation_{};
    std::shared_ptr<const nav::query::NavSpatialIndex> index_{};
    std::optional<nav::runtime::RouteSession> session_{};
    bool inRequest_{};
    std::optional<nav::runtime::SessionReason> deferredInvalidation_{};
    edict_t* queryingEntity_{}; // borrowed only for synchronous request
    const host::PlayerRegistry* queryingPlayers_{};
    const metamod::LifecycleCoordinator* queryingOwner_{};
};
}
