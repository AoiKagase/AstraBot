// SPDX-License-Identifier: MPL-2.0
#include "core/motor.hpp"
#include <algorithm>
#include <limits>
namespace astrabot::core {
double Motor::locomotionSpeed(LocomotionMode mode,double limit) noexcept {
    if(!std::isfinite(limit) || limit<0 || limit>kMaxMovement) return 0;
    if(mode==LocomotionMode::Run) return limit;
    if(mode==LocomotionMode::Walk) return 0.4*limit;
    return 0;
}
double Motor::requestedSpeed(const MovementIntent& intent) noexcept {
    return intent.locomotion==LocomotionMode::Explicit ? intent.speed:
        locomotionSpeed(intent.locomotion,intent.decisionSpeedLimit);
}
bool Motor::bindLocomotion(MovementIntent& intent,LocomotionMode mode,double limit,
    double distance) noexcept {
    if((mode!=LocomotionMode::Run && mode!=LocomotionMode::Walk) ||
       !std::isfinite(distance) || distance<=0) return false;
    const double speed=locomotionSpeed(mode,limit);
    if(speed<=0) return false;
    const double lifetime=std::floor(distance*1000000.0/speed);
    if(!std::isfinite(lifetime) || lifetime<1) return false;
    intent.speed=0; intent.decisionSpeedLimit=limit; intent.locomotion=mode;
    intent.validForUs=static_cast<std::uint64_t>((std::min)(lifetime,
        double(kMaximumMovementIntentUs)));
    return valid(intent);
}
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
    const double requested=requestedSpeed(intent);
    const double speed=std::min(requested,double(std::min(maximum,kMaxMovement)));
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
    if(speed>0) { set(intent.forward,Button::Forward); set(intent.back,Button::Back); }
    const auto rounded=frameUs>=255000 ? 255 : std::max<std::uint64_t>(1,(frameUs+500)/1000);
    result.msec=static_cast<std::uint8_t>(rounded);
    return {result,MotorError::None,speed};
}
}
