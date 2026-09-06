// SPDX-License-Identifier: MPL-2.0
#include "nav/local/recovery.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
namespace astrabot::nav::local {
namespace {
bool sameOwner(Binding a,Binding b) noexcept { return a.agent==b.agent && a.actor==b.actor && a.map==b.map; }
std::uint64_t add(std::uint64_t a,std::uint64_t b) noexcept {
    return b>(std::numeric_limits<std::uint64_t>::max)()-a ? (std::numeric_limits<std::uint64_t>::max)():a+b;
}
}
void Recovery::clearWindow() noexcept {
    window_=credited_=false; windowUs_=0;
    decision_.commandedUs=0; decision_.displacement=decision_.projected=decision_.travel=0;
}
bool Recovery::bindRoute(Binding b) noexcept {
    if(!b.agent.isValid() || !b.actor.isValid() || !b.map.isValid() || !b.routeGeneration ||
       (bound_ && (!sameOwner(b,binding_) || b.routeGeneration<binding_.routeGeneration))) return false;
    if(!bound_ || b.routeGeneration!=binding_.routeGeneration) {
        clearWindow(); reference_=false; dispatchTick_={};
    }
    bound_=true; binding_=b; return true;
}
bool Recovery::report(const ProgressDispatch& d) noexcept {
    if(!bound_ || !sameOwner(d.binding,binding_) || d.binding.routeGeneration!=binding_.routeGeneration ||
       !d.commandTick.isValid() || !d.dispatchTick.isAfter(d.commandTick) ||
       (dispatchTick_.isValid() && !d.dispatchTick.isAfter(dispatchTick_))) return false;
    dispatchTick_=d.dispatchTick;
    if(decision_.state!=RecoveryState::Monitoring) return true;
    const auto length=std::hypot(d.direction.x,d.direction.y);
    if(!d.dispatched || d.expected==ExpectedProgress::Pause || !d.origin.isFinite() ||
       !std::isfinite(length) || length<=0 || !d.durationUs || d.durationUs>255000) {
        clearWindow(); return true;
    }
    window_=true;
    if(!reference_) {
        reference_=true; anchor_=previous_=d.origin; furthest_=0;
        decision_.forward={d.direction.x/length,d.direction.y/length,0};
    }
    // Changing a target or crouch tag does not restart the window.
    windowUs_=(std::max)(windowUs_,d.expected==ExpectedProgress::Crouch ? crouchWindowUs:walkWindowUs);
    decision_.commandedUs=add(decision_.commandedUs,d.durationUs); credited_=true;
    return true;
}
void Recovery::pause(Binding b) noexcept {
    if(bound_ && sameOwner(b,binding_) && b.routeGeneration==binding_.routeGeneration &&
       decision_.state==RecoveryState::Monitoring) clearWindow();
}
RecoveryDecision Recovery::abort(StuckCause cause) noexcept {
    const bool first=decision_.state!=RecoveryState::Aborted;
    decision_.state=RecoveryState::Aborted; decision_.cause=cause;
    decision_.terminalEvent=first; decision_.deadlineUs=0; return decision_;
}
void Recovery::replanned() noexcept {
    decision_.attempts=1;
    if(decision_.state!=RecoveryState::Aborted) decision_.state=RecoveryState::Monitoring;
    decision_.deadlineUs=0; clearWindow(); reference_=false;
}
RecoveryDecision Recovery::observe(Binding b,core::TickId tick,std::uint64_t now,model::NavVector3 p) noexcept {
    decision_.terminalEvent=decision_.measuredProgress=false;
    if(!bound_ || !sameOwner(b,binding_) || b.routeGeneration!=binding_.routeGeneration ||
       !tick.isValid() || (observationTick_.isValid() && !tick.isAfter(observationTick_)) || now<nowUs_ || !p.isFinite())
        return decision_;
    observationTick_=tick; nowUs_=now;
    if(b.step!=binding_.step) { clearWindow(); reference_=false; }
    binding_.step=b.step;
    if(decision_.state==RecoveryState::Aborted || decision_.state==RecoveryState::Replan) return decision_;
    if(decision_.state!=RecoveryState::Monitoring) {
        if(now<decision_.deadlineUs) return decision_;
        if(decision_.state==RecoveryState::Wait) decision_.state=RecoveryState::Sidestep;
        else if(decision_.state==RecoveryState::Sidestep) decision_.state=RecoveryState::Reverse;
        else decision_.state=RecoveryState::Replan;
        decision_.deadlineUs=decision_.state==RecoveryState::Replan ? 0:add(now,stageUs);
        return decision_;
    }
    if(!window_ || !credited_) return decision_;
    credited_=false;
    const double dx=double(p.x)-anchor_.x,dy=double(p.y)-anchor_.y;
    decision_.displacement=std::hypot(dx,dy);
    decision_.projected=dx*decision_.forward.x+dy*decision_.forward.y;
    decision_.travel+=std::hypot(double(p.x)-previous_.x,double(p.y)-previous_.y); previous_=p;
    if(decision_.projected>=progressDistance && decision_.displacement>=progressDistance) {
        decision_.attempts=0; decision_.cause=StuckCause::None; decision_.symptom=StuckSymptom::None;
        // Retain the selected corridor direction and forward high-water point
        // across windows: a reversed steering target cannot refill the budget.
        anchor_=p; furthest_=0; decision_.measuredProgress=true; clearWindow(); return decision_;
    }
    // A verified lateral avoidance can make displacement without advancing the
    // portal. Credit only a NEW displacement high-water mark, never a repeated
    // excursion. This postpones detection but does not replenish replan attempts.
    if(decision_.displacement>=furthest_+progressDistance) {
        furthest_=decision_.displacement; decision_.commandedUs=0; decision_.travel=0;
        return decision_;
    }
    if(decision_.commandedUs<windowUs_) return decision_;
    decision_.cause=StuckCause::Unknown; // Failed progress alone never proves a collision.
    decision_.symptom=decision_.travel-decision_.displacement>=progressDistance ? StuckSymptom::Oscillation:StuckSymptom::NoProgress;
    if(decision_.attempts) return abort(decision_.cause);
    decision_.state=RecoveryState::Wait; decision_.deadlineUs=add(now,stageUs);
    return decision_;
}
}
