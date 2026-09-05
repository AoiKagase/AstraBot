// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "core/identity.hpp"
#include "nav/corridor/corridor.hpp"

namespace astrabot::nav::local {
enum class ActionRequest { None, Press, Hold, Release };
struct MovementIntent {
    // Direction length <= 1 (zero only at zero speed), speed [0,400]; lateral
    // correction [-1,1]. Motor must also clamp to observed actor limits.
    query::NavQueryPoint direction{};
    double speed{}, lateralCorrection{};
    std::optional<query::NavQueryPoint> view{};
    ActionRequest duck{}, jump{}, use{};
};
struct Binding {
    core::BotAgentId agent{};
    core::PlayerId actor{};
    core::MapGeneration map{};
    std::uint64_t routeGeneration{};
    std::size_t step{};
};
enum class PrimitiveState { Idle, Running, Complete, Failed, Aborted };
enum class PrimitiveEvent { None, Entered, Complete, Failed, Aborted };
enum class PrimitiveReason { None, InvalidBinding, InvalidTransition, AlreadyEntered,
    UnsupportedTraversal, StaleUpdate, InvalidFeedback, InvalidIntent,
    MissingSupport, ControllerFailure, Cancelled };
enum class Progress { Running, Complete, Failed };
// Feedback is produced by the trusted primitive controller (P3-03/05/06), not
// by A* or proximity alone. Completion needs verified support in selected target.
struct Feedback {
    Binding binding{};
    core::TickId tick{};
    Progress progress{Progress::Running};
    MovementIntent intent{};
    std::optional<model::NavAreaId> supportedArea{};
    bool supportVerified{};
};
struct PrimitiveUpdate {
    bool accepted{};
    PrimitiveEvent event{PrimitiveEvent::None};
    PrimitiveState state{PrimitiveState::Idle};
    PrimitiveReason reason{PrimitiveReason::None};
    MovementIntent intent{}; // Rejections and every terminal outcome are neutral.
};
// One value-owned lifecycle per transition. No virtual controller, SDK pointer,
// movement dispatch, or arrival inference. Construct the next instance only
// after its owner has consumed this instance's terminal event.
class Primitive final {
public:
    PrimitiveUpdate enter(Binding, const corridor::Transition&, core::TickId) noexcept;
    PrimitiveUpdate update(const Feedback&) noexcept;
    PrimitiveUpdate abort() noexcept;
    PrimitiveState state() const noexcept { return state_; }
    const std::optional<corridor::Transition>& transition() const noexcept { return transition_; }
    const Binding& binding() const noexcept { return binding_; }
    // Recognized lifecycle tags, not implemented movement controllers. No
    // normalization to Walk. Tags are the existing traversal vocabulary.
    static bool supported(model::NavTraversalKind) noexcept;
private:
    Binding binding_{};
    std::optional<corridor::Transition> transition_{};
    core::TickId tick_{};
    PrimitiveState state_{PrimitiveState::Idle};
    PrimitiveUpdate finish(PrimitiveState, PrimitiveEvent, PrimitiveReason) noexcept;
};
} // namespace astrabot::nav::local
