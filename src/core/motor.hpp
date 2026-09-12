// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "core/command.hpp"
#include <optional>
#include <cmath>
#include <cstdint>

namespace astrabot::core {
struct IntentVector { double x{}, y{}, z{}; };
enum class ActionRequest { None, Press, Hold, Release };
enum class LocomotionMode : std::uint8_t { Explicit, Run, Walk };
inline constexpr std::uint64_t kMaximumMovementIntentUs=120000;
struct MovementIntent {
    // Explicit owns a special-traversal speed. Run/Walk own no arbitrary
    // speed: Motor resolves them from the decision-time and dispatch-time
    // player speed limits. validForUs bounds reuse of physically proved
    // ground motion; zero selects the global maximum for Explicit intents.
    IntentVector direction{};
    double speed{}, decisionSpeedLimit{}, lateralCorrection{};
    std::uint64_t validForUs{};
    LocomotionMode locomotion{LocomotionMode::Explicit};
    std::optional<IntentVector> view{};
    ActionRequest duck{}, jump{}, use{};
    // Explicit buttons for modes that read buttons rather than analog movement.
    // Never inferred for ordinary Walk.
    ActionRequest forward{}, back{};
};
enum class MotorError { None, InvalidIntent, InvalidObservation, NoElapsedTime };
struct MotorResult {
    std::optional<BotCommand> command{};
    MotorError error{MotorError::None};
    double resolvedSpeed{};
    explicit operator bool() const noexcept { return command.has_value() && error==MotorError::None; }
};
class Motor final {
public:
    static bool valid(const MovementIntent&) noexcept;
    static double locomotionSpeed(LocomotionMode,double decisionSpeedLimit) noexcept;
    static double requestedSpeed(const MovementIntent&) noexcept;
    static bool bindLocomotion(MovementIntent&,LocomotionMode,double decisionSpeedLimit,
        double validatedDistance) noexcept;
    // Caller marks the first eligible frame consuming a new intent. Press is
    // emitted only there; Hold repeats, None/Release clear the action each frame.
    // msec is a measured-frame hint; the adapter's dispatch clock remains final.
    static MotorResult command(const MovementIntent&, ViewAngles observedView,
        float observedSpeedLimit, std::uint64_t frameUs, bool firstFrame) noexcept;
};
inline bool Motor::valid(const MovementIntent& i) noexcept {
    const auto finite=[](IntentVector p) { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); };
    const auto action=[](ActionRequest a) { return a==ActionRequest::None || a==ActionRequest::Press ||
        a==ActionRequest::Hold || a==ActionRequest::Release; };
    const bool mode=i.locomotion==LocomotionMode::Explicit || i.locomotion==LocomotionMode::Run ||
        i.locomotion==LocomotionMode::Walk;
    if(!mode || !finite(i.direction) || !std::isfinite(i.speed) || i.speed<0 || i.speed>kMaxMovement ||
       !std::isfinite(i.decisionSpeedLimit) || i.decisionSpeedLimit<0 || i.decisionSpeedLimit>kMaxMovement ||
       !std::isfinite(i.lateralCorrection) || std::abs(i.lateralCorrection)>1 ||
       (i.view && !finite(*i.view)) || !action(i.duck) || !action(i.jump) || !action(i.use) ||
       !action(i.forward) || !action(i.back)) return false;
    if(i.validForUs>kMaximumMovementIntentUs ||
       (i.locomotion==LocomotionMode::Explicit && i.decisionSpeedLimit!=0) ||
       (i.locomotion!=LocomotionMode::Explicit &&
        (i.speed!=0 || i.decisionSpeedLimit<=0 || i.validForUs==0))) return false;
    const double requested=requestedSpeed(i);
    const auto moving=[](ActionRequest a) { return a==ActionRequest::Press || a==ActionRequest::Hold; };
    if((moving(i.forward) && moving(i.back)) || ((moving(i.forward) || moving(i.back)) && requested==0)) return false;
    const double n=i.direction.x*i.direction.x+i.direction.y*i.direction.y+i.direction.z*i.direction.z;
    return n<=1.000001 && (requested==0 || n>0);
}
}
