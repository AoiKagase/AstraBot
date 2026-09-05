// SPDX-License-Identifier: MPL-2.0
#include "nav/local/intent_pump.hpp"
#include <limits>
namespace astrabot::nav::local {
void IntentPump::stop(PumpReason reason) noexcept {
    intent_={}; hasIntent_=false; first_=false; reason_=reason;
}
FrameSchedule IntentPump::beginFrame(const runtime::MovementSnapshot& s) noexcept {
    eligible_=false;
    if(retired_ || !binding_.agent.isValid() || !binding_.actor.isValid() || !binding_.map.isValid() ||
       !binding_.routeGeneration || s.agent!=binding_.agent || s.actor!=binding_.actor || s.map!=binding_.map ||
       s.kind!=runtime::ActorKind::ManagedBot || s.connected!=true || s.alive!=true || s.joined!=true) {
        retired_=true; stop(PumpReason::InvalidActor); return {false,false,reason_,0};
    }
    if(!s.tick.isValid() || !s.tick.isAfter(tick_)) return {false,false,PumpReason::DuplicateFrame,0};
    tick_=s.tick; frameUs_=s.elapsedUs; due_=false; published_=false; taken_=false;
    const auto maximum=(std::numeric_limits<std::uint64_t>::max)()-decisionPeriodUs;
    if(s.elapsedUs>maximum-timeUs_) {
        retired_=true; stop(PumpReason::ClockOverflow); return {false,false,reason_,0};
    }
    timeUs_+=s.elapsedUs;
    if(!frameUs_) return {true,false,PumpReason::None,0};
    eligible_=true;
    if(!started_) {
        started_=true; due_=true; nextDecisionUs_=timeUs_+decisionPeriodUs;
        return {true,true,PumpReason::None,0};
    }
    if(timeUs_>=nextDecisionUs_) {
        const auto lateness=timeUs_-nextDecisionUs_;
        const auto missed=lateness/decisionPeriodUs;
        nextDecisionUs_=timeUs_+decisionPeriodUs-lateness%decisionPeriodUs;
        due_=true; return {true,true,PumpReason::None,missed};
    }
    return {true,false,PumpReason::None,0};
}
bool IntentPump::publish(Binding binding, core::TickId tick, const MovementIntent& intent) noexcept {
    if(!eligible_ || !due_ || published_ || taken_ || tick!=tick_ || binding.agent!=binding_.agent ||
       binding.actor!=binding_.actor || binding.map!=binding_.map || binding.routeGeneration!=binding_.routeGeneration ||
       binding.step<binding_.step) { stop(PumpReason::StaleDecision); return false; }
    if(!core::Motor::valid(intent)) { stop(PumpReason::InvalidIntent); return false; }
    binding_=binding; intent_=intent; intentTimeUs_=timeUs_; hasIntent_=true; first_=true;
    reason_=PumpReason::None; published_=true; return true;
}
PumpOutput IntentPump::take() noexcept {
    if(!eligible_ || taken_ || retired_) return {false,false,{},reason_,tick_,frameUs_,0};
    taken_=true;
    const auto age=hasIntent_ ? timeUs_-intentTimeUs_:0;
    if(hasIntent_ && age>maxIntentAgeUs) stop(PumpReason::StaleIntent);
    const PumpOutput output{true,first_,intent_,reason_,tick_,frameUs_,age};
    first_=false; return output;
}
}
