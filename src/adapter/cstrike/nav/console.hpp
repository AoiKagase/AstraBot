// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "adapter/metamod/plugin_entry.hpp"
#include "nav/runtime/route_session.hpp"
#include "nav/runtime/replan.hpp"
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
    void invalidateActor(core::PlayerId,nav::runtime::SessionReason) noexcept;
    void observe(metamod::LifecycleCoordinator&) noexcept;
    void beforeDispatch(metamod::LifecycleCoordinator&) noexcept;
    void beforeDispatch(metamod::LifecycleCoordinator&,core::PlayerId) noexcept;
    std::optional<MotionTrace> dispatchTicket() const noexcept {
        return current_->pendingMotion_ ? std::optional<MotionTrace>{current_->motionTrace_}:std::nullopt;
    }
    std::optional<MotionTrace> dispatchTicket(core::PlayerId) const noexcept;
    void afterDispatch(const metamod::MovementResult&, core::TickId,const std::optional<MotionTrace>&) noexcept;
    void afterDispatch(core::PlayerId,const metamod::MovementResult&,core::TickId,const std::optional<MotionTrace>&) noexcept;
    void moveFrame(metamod::LifecycleCoordinator&) noexcept;
    void moveFrame(metamod::LifecycleCoordinator&,core::PlayerId) noexcept;
    void execute(NavCommand, metamod::LifecycleCoordinator&) noexcept;
    // Publication binds an independently obtained immutable mesh to the current map.
    nav::diagnostics::NavError publish(core::MapGeneration,
        std::shared_ptr<const nav::model::NavMeshSnapshot>) noexcept;
    const nav::runtime::DecisionTrace* trace() const noexcept { return current_->session_ ? &current_->session_->trace():nullptr; }
    const nav::runtime::DecisionTrace* trace(core::PlayerId) const noexcept;
    const nav::runtime::ReplanAttempt* replan(core::PlayerId player) const noexcept {
        const auto* actor=findActor(player); return actor ? &actor->replan_:nullptr;
    }
    const MotionTrace& motionTrace() const noexcept { return current_->motionTrace_; }
    const MotionTrace* motionTrace(core::PlayerId) const noexcept;
    static constexpr std::size_t motionHistoryLimit=128;
    std::size_t motionHistoryCount() const noexcept { return current_->motionCount_; }
    std::size_t motionHistoryCount(core::PlayerId) const noexcept;
    const MotionTrace* motionHistory(std::size_t index) const noexcept {
        return index<current_->motionCount_ ? &current_->motionHistory_[(current_->motionNext_+motionHistoryLimit-current_->motionCount_+index)%motionHistoryLimit]:nullptr;
    }
    const MotionTrace* motionHistory(core::PlayerId,std::size_t) const noexcept;
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
    void printReplan() noexcept;
    bool runReplan(metamod::LifecycleCoordinator&) noexcept;
    void requestRoute(const nav::runtime::MovementSnapshot&,nav::model::NavAreaId,
        metamod::LifecycleCoordinator&,const nav::runtime::RouteOptions&) noexcept;
    void invalidateCurrent(nav::runtime::SessionReason) noexcept;
    bool applyDeferredInvalidation() noexcept;
    bool selectActor(core::PlayerId) noexcept;
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
    struct ActorState {
    core::PlayerId actor{};
    std::optional<nav::local::Walk> walk_{};
    std::optional<nav::local::IntentPump> pump_{};
    std::optional<Segment> segment_{};
    std::optional<PendingMotion> pendingMotion_{};
    std::optional<nav::local::Binding> neutralBinding_{};
    core::TickId requestTick_{};
    core::TickId guardTick_{};
    std::uint32_t guardQueries_{};
    std::uint64_t intentWallAgeUs_{};
    nav::runtime::ReplanAttempt replan_{};
    std::uint64_t navigationTimeUs_{};
    core::TickId navigationTimeTick_{};
    MotionTrace motionTrace_{};
    std::array<MotionTrace,motionHistoryLimit> motionHistory_{};
    std::size_t motionNext_{}, motionCount_{};
    std::uint64_t motionSequence_{};
    std::optional<nav::runtime::RouteSession> session_{};
    };
    // Fixed slot capacity, lazy allocation, stable addresses through reentrant
    // invalidation/reset. Mesh, graph and index remain shared across all actors.
    ActorState idle_{};
    std::array<std::unique_ptr<ActorState>,host::kMaxClientSlots> actors_{};
    ActorState* current_{&idle_};
    struct ActorScope {
        ActorState*& current;
        ActorState* previous;
        ActorScope(ActorState*& slot,ActorState* actor) noexcept : current(slot),previous(slot) { current=actor; }
        ~ActorScope() { current=previous; }
    };
    ActorState* findActor(core::PlayerId) noexcept;
    const ActorState* findActor(core::PlayerId) const noexcept;
    enginefuncs_t* engine_{};
    mutil_funcs_t* utility_{};
    globalvars_t* globals_{};
    nav::runtime::NavigationSnapshot navigation_{};
    std::shared_ptr<const nav::query::NavSpatialIndex> index_{};
    bool inRequest_{};
    std::optional<nav::runtime::SessionReason> deferredInvalidation_{};
    bool deferredAll_{}, deferredReset_{};
    edict_t* queryingEntity_{}; // borrowed only for synchronous request
    const host::PlayerRegistry* queryingPlayers_{};
    const metamod::LifecycleCoordinator* queryingOwner_{};
};
}
