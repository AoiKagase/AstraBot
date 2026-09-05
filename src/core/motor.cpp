// SPDX-License-Identifier: MPL-2.0
#include "core/motor.hpp"
#include <algorithm>
namespace astrabot::core {
MotorResult Motor::command(const MovementIntent& intent, ViewAngles observed, float maximum,
    std::uint64_t frameUs, bool firstFrame) noexcept {
    if(!valid(intent)) return {{},MotorError::InvalidIntent};
    if(!std::isfinite(maximum) || maximum<0 || !std::isfinite(observed.pitch) ||
       !std::isfinite(observed.yaw) || !std::isfinite(observed.roll)) return {{},MotorError::InvalidObservation};
    if(!frameUs) return {{},MotorError::NoElapsedTime};
    const auto view=intent.view.value_or(IntentVector{observed.pitch,observed.yaw,observed.roll});
    BotCommand result;
    result.view={static_cast<float>(std::clamp(view.x,double(kMinPitch),double(kMaxPitch))),
        static_cast<float>(std::remainder(view.y,360.0)),
        static_cast<float>(std::clamp(view.z,double(kMinRoll),double(kMaxRoll)))};
    constexpr double radians=3.14159265358979323846/180.0;
    const double yaw=result.view.yaw*radians, c=std::cos(yaw), s=std::sin(yaw);
    const double speed=std::min(intent.speed,double(std::min(maximum,kMaxMovement)));
    double forward=(intent.direction.x*c+intent.direction.y*s)*speed;
    double side=(intent.direction.x*s-intent.direction.y*c+intent.lateralCorrection)*speed;
    double up=intent.direction.z*speed;
    const double magnitude=std::hypot(forward,side,up), limit=speed;
    if(magnitude>limit) { const double scale=limit/magnitude; forward*=scale; side*=scale; up*=scale; }
    result.movement={static_cast<float>(forward),static_cast<float>(side),static_cast<float>(up)};
    const auto set=[&](ActionRequest action, Button button) {
        if(action==ActionRequest::Hold || (action==ActionRequest::Press && firstFrame))
            result.buttons|=static_cast<ButtonMask>(button);
    };
    set(intent.duck,Button::Duck); set(intent.jump,Button::Jump); set(intent.use,Button::Use);
    const auto rounded=frameUs>=255000 ? 255 : std::max<std::uint64_t>(1,(frameUs+500)/1000);
    result.msec=static_cast<std::uint8_t>(rounded);
    return {result,MotorError::None};
}
}
