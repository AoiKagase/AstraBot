// SPDX-License-Identifier: MPL-2.0
#include "adapter/cstrike/nav/console.hpp"
#include "adapter/metamod/lifecycle.hpp"
#include <algorithm>
#include <cmath>
namespace astrabot::adapter::cstrike {
namespace {
bool outside(const BoundLadderPlan& bound,nav::model::NavVector3 v) noexcept {
    const auto& c=bound.passage.candidate;
    return v.x<=c.minimum.x-16 || v.x>=c.maximum.x+16 || v.y<=c.minimum.y-16 ||
        v.y>=c.maximum.y+16 || v.z<=c.minimum.z-36 || v.z>=c.maximum.z+36;
}
bool exiting(nav::local::LadderState s) noexcept { return s==nav::local::LadderState::Exit; }
std::optional<std::uint8_t> duration(std::uint64_t us) noexcept {
    if(!us || us>120000) return {};
    return static_cast<std::uint8_t>((std::max)(std::uint64_t{1},us/1000+(us%1000>=500 ? 1U:0U)));
}
}
LadderFrameResult NavConsole::inspectOwnedLadder(metamod::LifecycleCoordinator& owner,const nav::runtime::MovementSnapshot& s,
    nav::local::Binding binding,const BoundLadderPlan& bound,nav::model::NavVector3 target,std::uint32_t budget,
    std::optional<core::BotCommand> command,std::optional<std::uint8_t> exitMsec,bool ground) noexcept {
    struct Context { const NavConsole* console; const metamod::LifecycleCoordinator* owner; const LadderDiscovery* publication; };
    const auto publication=ladders_; const auto index=index_;
    if(!publication || !index) return {};
    const Context context{this,&owner,publication.get()};
    const auto map=[](const void* p) noexcept {
        const auto& c=*static_cast<const Context*>(p); const auto& r=c.owner->registry();
        return r.isMapActive() ? r.mapGeneration():core::MapGeneration{};
    };
    const auto current=[](const void* p,nav::local::Binding b,core::TickId tick) noexcept {
        const auto& c=*static_cast<const Context*>(p); const auto& n=*c.console; const auto& r=c.owner->registry();
        if(n.deferredInvalidation_ || n.ladders_.get()!=c.publication || !n.current_->session_ || !n.current_->session_->executable() ||
           !n.current_->walk_ || n.current_->walk_->step()!=b.step || n.current_->actor!=b.actor ||
           !r.isMapActive() || r.mapGeneration()!=b.map || r.currentTick()!=tick || r.currentPlayer(b.actor.slot)!=b.actor ||
           !c.owner->entityFor(b.actor)) return false;
        const auto& t=n.current_->session_->trace();
        return t.actor==b.actor && t.agent==b.agent && t.map==b.map && t.routeGeneration==b.routeGeneration;
    };
    return inspectLadderFrame({{engine_,&context,map},&context,current},owner.entityFor(s.actor),binding,s,bound,target,
        *index,navigation_.map,globals_ ? globals_->maxEntities:0,budget,command,exitMsec,ground);
}
std::optional<nav::local::LadderObservation> NavConsole::observeLadder(metamod::LifecycleCoordinator& owner,
    const nav::runtime::MovementSnapshot& s,nav::local::Binding binding,std::uint32_t reserved) noexcept {
    if(!current_->walk_ || !ladders_ || !s.position || reserved>=21) return {};
    const auto selected=current_->walk_->selectedLadderLink(); if(!selected) return {};
    const auto map=[](const void* p) noexcept { return static_cast<const metamod::LifecycleCoordinator*>(p)->registry().mapGeneration(); };
    const auto pub=ladders_;
    const auto bound=bindLadderPlan({engine_,&owner,map},s.map,pub->links.fingerprint,*pub,*selected,globals_ ? globals_->maxEntities:0);
    current_->motionTrace_.ladderBindingReason=bound.reason;
    if(!bound) return {};
    const auto& old=current_->motionTrace_.decision;
    const auto state=old.binding.step==binding.step && old.ladderState ? *old.ladderState:nav::local::LadderState::Approach;
    const auto target=current_->walk_->ladderTarget(bound.value->plan,*s.position);
    const bool ground=s.grounded==true && outside(*bound.value,*s.position);
    const auto exit=exiting(state) && !ground ? duration(s.elapsedUs):std::optional<std::uint8_t>{};
    if(exiting(state) && !ground && !exit) return {};
    const auto limit=(std::min)(21U-reserved,(ground || exit) ? 21U:4U);
    const auto observed=inspectOwnedLadder(owner,s,binding,*bound.value,target,limit,{},exit,ground);
    current_->motionTrace_.ladderFrameReason=observed.reason;
    if(!observed) return {};
    return nav::local::LadderObservation{bound.value->plan,observed.value->inspection,observed.value->contact,observed.value->climbing};
}
void NavConsole::reportLadderTransport(const nav::local::WalkDecision& decision,core::TickId queued,core::TickId dispatched,bool success) noexcept {
    if(!current_->walk_ || !decision.ladderPlan || decision.ladderPressTick!=queued) return;
    const auto& link=decision.ladderPlan->link;
    (void)current_->walk_->reportLadderDispatch({decision.binding,link.sourceId,link.generation,link.linkId,queued,dispatched,success});
}
MotionReason NavConsole::guardLadder(metamod::LifecycleCoordinator& owner,const nav::runtime::MovementSnapshot& s,const PendingMotion& pending) noexcept {
    if(!pending.ladder || !s.position || !s.velocity || !current_->walk_ || !ladders_ || current_->guardQueries_>=21) return MotionReason::LadderChanged;
    auto command=pending.command; const auto msec=duration(movement_->frameDeltaUs()); if(!msec) return MotionReason::StaleCommand;
    command.msec=*msec;
    const bool neutral=command.buttons==0 && command.impulse==0 && command.movement==core::Movement{};
    const auto state=pending.ladderState;
    if(state==nav::local::LadderState::Complete || state==nav::local::LadderState::Failed || state==nav::local::LadderState::Aborted)
        return neutral ? MotionReason::None:MotionReason::LadderChanged;
    if(current_->walk_->step()!=pending.binding.step) return MotionReason::LadderChanged;
    const auto map=[](const void* p) noexcept { return static_cast<const metamod::LifecycleCoordinator*>(p)->registry().mapGeneration(); };
    const auto pub=ladders_;
    const auto bound=bindLadderPlan({engine_,&owner,map},s.map,pub->links.fingerprint,*pub,pending.ladder->link,globals_ ? globals_->maxEntities:0);
    if(!bound) return MotionReason::LadderChanged;
    const bool ground=s.grounded==true && outside(*bound.value,*s.position);
    const bool press=(command.buttons&static_cast<core::ButtonMask>(core::Button::Jump))!=0;
    if(press && (!exiting(state) || pending.ladderPressTick!=pending.tick || ground)) return MotionReason::LadderChanged;
    if(ground) {
        if(command.buttons || command.impulse || command.movement.up!=0) return MotionReason::LadderChanged;
        const double yaw=command.view.yaw*3.14159265358979323846/180;
        const double vx=command.movement.forward*std::cos(yaw)+command.movement.side*std::sin(yaw);
        const double vy=command.movement.forward*std::sin(yaw)-command.movement.side*std::cos(yaw);
        const double dx=double(pending.ladderTarget.x)-s.position->x,dy=double(pending.ladderTarget.y)-s.position->y;
        const double range=std::hypot(dx,dy),speed=std::hypot(vx,vy);
        if(speed>0 && (range<=0 || vx*dx+vy*dy<=0 || std::abs(vx*dy-vy*dx)>speed*0.5 || speed*double(*msec)/1000>range+0.001))
            return MotionReason::Deviation;
    }
    // Decisions approve the bounded exit candidate. Each intervening dispatch
    // proves its exact current-frame command, without spending another complete
    // trajectory budget or treating a straight line to the landing as flight.
    const auto maximum=(std::min)(21U-current_->guardQueries_,ground ? 21U:7U);
    const auto proof=inspectOwnedLadder(owner,s,pending.binding,*bound.value,ground ? pending.ladderTarget:*s.position,maximum,
        ground ? std::optional<core::BotCommand>{}:command,{},ground);
    current_->guardQueries_+=proof.queries; current_->motionTrace_.ladderGuardQueries+=proof.queries;
    return proof ? MotionReason::None:MotionReason::LadderChanged;
}
}
