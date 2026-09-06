// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "core/command.hpp"
#include <optional>
#include <cmath>

namespace astrabot::core {
struct IntentVector { double x{}, y{}, z{}; };
enum class ActionRequest { None, Press, Hold, Release };
struct MovementIntent {
    // Direction length <= 1 (zero only at zero speed), speed [0,400],
    // lateral correction [-1,1]. View is pitch/yaw/roll in degrees.
    IntentVector direction{};
    double speed{}, lateralCorrection{};
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
    explicit operator bool() const noexcept { return command.has_value() && error==MotorError::None; }
};
class Motor final {
public:
    static bool valid(const MovementIntent&) noexcept;
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
    if(!finite(i.direction) || !std::isfinite(i.speed) || i.speed<0 || i.speed>kMaxMovement ||
       !std::isfinite(i.lateralCorrection) || std::abs(i.lateralCorrection)>1 ||
       (i.view && !finite(*i.view)) || !action(i.duck) || !action(i.jump) || !action(i.use) ||
       !action(i.forward) || !action(i.back)) return false;
    const auto moving=[](ActionRequest a) { return a==ActionRequest::Press || a==ActionRequest::Hold; };
    if((moving(i.forward) && moving(i.back)) || ((moving(i.forward) || moving(i.back)) && i.speed==0)) return false;
    const double n=i.direction.x*i.direction.x+i.direction.y*i.direction.y+i.direction.z*i.direction.z;
    return n<=1.000001 && (i.speed==0 || n>0);
}
}
