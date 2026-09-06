// SPDX-License-Identifier: MPL-2.0
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include "adapter/cstrike/nav/console.hpp"
#include "adapter/metamod/lifecycle.hpp"
#include "adapter/cstrike/nav/world_queries.hpp"
#include "adapter/cstrike/nav/jump_motion.hpp"
#ifdef snprintf
#undef snprintf
#endif
namespace astrabot::adapter::cstrike {
namespace {
constexpr nav::local::WalkLimits walkLimits{{21,4,48,16,18,18,64,4,18,0.7},160,1,1,3,1000000,3000000,12,8,40,25,{120000,400000,3000000},
    {{{-16,-16,-36},{16,16,36}},{{-16,-16,-18},{16,16,18}},1000000}};
std::uint64_t add(std::uint64_t a,std::uint64_t b) noexcept {
    const auto maximum=(std::numeric_limits<std::uint64_t>::max)();
    return b>maximum-a ? maximum:a+b;
}
bool ready(const nav::runtime::MovementSnapshot& s,bool airborne=false) noexcept {
    return s.kind==nav::runtime::ActorKind::ManagedBot && s.connected==true && s.alive==true &&
        s.joined==true && (s.grounded==true || (airborne && s.grounded==false)) && s.position && s.position->isFinite() &&
        s.view && s.view->isFinite() && s.hull && s.hull->minimum.isFinite() && s.hull->maximum.isFinite() &&
        s.speedLimit && std::isfinite(*s.speedLimit) && *s.speedLimit>=0;
}
bool segmentAllows(nav::model::NavVector3 start,nav::model::NavVector3 end,
                   nav::model::NavVector3 position,double travel) noexcept {
    const double dx=double(end.x)-start.x, dy=double(end.y)-start.y;
    const double length=std::hypot(dx,dy);
    if(length<=0) return false;
    const double px=double(position.x)-start.x, py=double(position.y)-start.y;
    const double along=(px*dx+py*dy)/length;
    const double lateral=std::abs(px*dy-py*dx)/length;
    const double expectedZ=start.z+(double(end.z)-start.z)*(along/length);
    return along>=-0.01 && along<=length && lateral<=0.5 &&
        std::abs(double(position.z)-expectedZ)<=walkLimits.probe.supportTolerance &&
        along+travel<=length+0.001;
}
const char* walkState(nav::local::WalkState state) noexcept {
    switch(state) {
    case nav::local::WalkState::Running:return "Running";
    case nav::local::WalkState::Arrived:return "Arrived";
    case nav::local::WalkState::Failed:return "Failed";
    case nav::local::WalkState::Aborted:return "Aborted";
    }
    return "Unknown";
}
}
void NavConsole::recordMotion(MotionEvent event,MotionReason reason) noexcept {
    if(event==MotionEvent::Decision) current_->motionTrace_.selectedEdge.reset();
    if(current_->session_ && current_->session_->trace().route && current_->session_->trace().routeGeneration==current_->motionTrace_.decision.binding.routeGeneration &&
       current_->motionTrace_.decision.binding.step<current_->session_->trace().route->steps.size())
        current_->motionTrace_.selectedEdge=current_->session_->trace().route->steps[current_->motionTrace_.decision.binding.step].edge;
    current_->motionTrace_.event=event; current_->motionTrace_.reason=reason;
    current_->motionSequence_=add(current_->motionSequence_,1); current_->motionTrace_.sequence=current_->motionSequence_;
    current_->motionHistory_[current_->motionNext_]=current_->motionTrace_; current_->motionNext_=(current_->motionNext_+1)%motionHistoryLimit;
    current_->motionCount_=(std::min)(current_->motionCount_+1,motionHistoryLimit);
    if(event==MotionEvent::Rejected || event==MotionEvent::Cancelled ||
       (event==MotionEvent::Decision && current_->motionTrace_.decision.terminalEvent)) printMotion();
}
void NavConsole::printMotion() noexcept {
    if(!current_->motionTrace_.decision.binding.routeGeneration) return;
    const auto& d=current_->motionTrace_.decision;
    const auto target=d.target ? d.target->origin:nav::model::NavVector3{};
    char text[1536]{};
    std::snprintf(text,sizeof(text),
        "walk actor=%u:%u map=%u route=%llu step=%zu tick=%llu state=%s reason=%u probe=%u event=%u motion_reason=%u corridor=%u transport=%u command_tick=%llu dispatch_tick=%llu age_us=%llu speed=%.6g direction=(%.6g,%.6g) target_present=%u target=(%.6g,%.6g,%.6g) support=%u queries=%u samples=%u step_probes=%u queued=%llu dispatched=%llu rejected=%llu missed=%llu history=%zu omitted=%llu edge=%u:%u command=(%.6g,%.6g,%u) door=%llu door_state=%u door_reason=%u use_checks=%llu contact_pulse=%u contact_guards=%llu clearance=(%.6g,%.6g) narrow=%u avoiding=%u lateral=%.6g",
        unsigned(d.binding.actor.slot),unsigned(d.binding.actor.generation.value),unsigned(d.binding.map.value),
        static_cast<unsigned long long>(d.binding.routeGeneration),d.binding.step,static_cast<unsigned long long>(d.tick.value),
        walkState(d.state),unsigned(d.reason),unsigned(d.probeReason),unsigned(current_->motionTrace_.event),unsigned(current_->motionTrace_.reason),
        unsigned(current_->motionTrace_.corridorError),unsigned(current_->motionTrace_.transportError),
        static_cast<unsigned long long>(current_->motionTrace_.commandTick.value),static_cast<unsigned long long>(current_->motionTrace_.dispatchTick.value),
        static_cast<unsigned long long>(current_->motionTrace_.intentAgeUs),d.intent.speed,d.intent.direction.x,d.intent.direction.y,
        unsigned(d.target.has_value()),double(target.x),double(target.y),double(target.z),
        d.support ? unsigned(d.support->area.value):0U,d.queries,d.samples,d.steps,
        static_cast<unsigned long long>(current_->motionTrace_.queued),static_cast<unsigned long long>(current_->motionTrace_.dispatched),
        static_cast<unsigned long long>(current_->motionTrace_.rejected),static_cast<unsigned long long>(current_->motionTrace_.missedDecisions),
        current_->motionCount_,static_cast<unsigned long long>(current_->motionSequence_-current_->motionCount_),
        current_->motionTrace_.selectedEdge ? unsigned(current_->motionTrace_.selectedEdge->source.value):0U,
        current_->motionTrace_.selectedEdge ? unsigned(current_->motionTrace_.selectedEdge->target.value):0U,
        double(current_->motionTrace_.command.movement.forward),double(current_->motionTrace_.command.movement.side),unsigned(current_->motionTrace_.command.msec),
        static_cast<unsigned long long>(d.doorId),d.doorState ? unsigned(*d.doorState):0U,unsigned(d.doorReason),
        static_cast<unsigned long long>(current_->motionTrace_.useGuardChecks),unsigned(d.contact.has_value()),
        static_cast<unsigned long long>(current_->motionTrace_.contactGuardQueries),d.leftClearance,d.rightClearance,
        unsigned(d.narrow),unsigned(d.avoiding),d.intent.lateralCorrection);
    const auto used=std::strlen(text);
    std::snprintf(text+used,sizeof(text)-used," blocker=%llu blocker_kind=%u blocker_action=%u blocker_reason=%u blocker_player=%u:%u",
        static_cast<unsigned long long>(d.blocker ? d.blocker->id:0),
        d.blocker ? unsigned(d.blocker->kind):0U,unsigned(d.blockerAction),unsigned(d.blockerReason),
        d.blocker && d.blocker->player ? unsigned(d.blocker->player->slot):0U,
        d.blocker && d.blocker->player ? unsigned(d.blocker->player->generation.value):0U);
    line(text);
    if(d.posture || d.constraintReason!=nav::local::ConstraintReason::None) {
        char posture[128]{};
        std::snprintf(posture,sizeof(posture),"walk posture_present=%u posture=%u posture_reason=%u constraint_reason=%u duck=%u",
            unsigned(d.posture.has_value()),d.posture ? unsigned(*d.posture):0U,unsigned(d.postureReason),unsigned(d.constraintReason),unsigned(d.intent.duck));
        line(posture);
    }
    if(d.jumpState) {
        char jump[384]{};
        std::snprintf(jump,sizeof(jump),"jump state=%u reason=%u probe=%u geometry=%u press_tick=%llu gravity=%.6g impulse=%.6g guard_queries=%llu profile=standard-cs",
            unsigned(*d.jumpState),unsigned(d.jumpReason),unsigned(d.jumpProbeReason),unsigned(d.jumpGeometryReason),
            static_cast<unsigned long long>(d.jumpPressTick.value),d.jumpPhysics ? d.jumpPhysics->gravity:0,
            d.jumpPhysics ? d.jumpPhysics->verticalImpulse:0,static_cast<unsigned long long>(current_->motionTrace_.jumpGuardQueries));
        line(jump);
    }
}
void NavConsole::clearPending() noexcept {
    if(current_->pendingMotion_ && movement_)
        (void)movement_->cancel(current_->pendingMotion_->binding.actor,current_->pendingMotion_->binding.map,current_->pendingMotion_->tick);
    current_->pendingMotion_.reset();
}
void NavConsole::stopMotion() noexcept {
    current_->neutralDuck_=current_->motionTrace_.decision.intent.duck==core::ActionRequest::Hold;
    if(current_->walk_ || current_->pendingMotion_) current_->neutralBinding_=current_->motionTrace_.decision.binding;
    clearPending();
    if(current_->walk_ && current_->walk_->state()==nav::local::WalkState::Running) {
        current_->motionTrace_.decision=current_->walk_->abort(); recordMotion(MotionEvent::Cancelled,MotionReason::Cancelled);
        current_->neutralDuck_=current_->motionTrace_.decision.intent.duck==core::ActionRequest::Hold;
    }
    current_->walk_.reset(); current_->pump_.reset(); current_->segment_.reset(); current_->intentWallAgeUs_=0;
}
void NavConsole::startMotion(const nav::runtime::MovementSnapshot& s) noexcept {
    current_->motionTrace_={}; current_->motionNext_=current_->motionCount_=0; current_->motionSequence_=0; current_->requestTick_=s.tick;
    if(!current_->session_ || !current_->session_->executable()) return;
    const auto& route=current_->session_->trace();
    current_->motionTrace_.decision.binding={s.agent,s.actor,s.map,route.routeGeneration,0};
    if(current_->neutralBinding_) current_->neutralBinding_=current_->motionTrace_.decision.binding;
    current_->motionTrace_.decision.tick=s.tick;
    const auto fail=[&](MotionReason reason) {
        current_->motionTrace_.decision.state=nav::local::WalkState::Failed;
        current_->motionTrace_.decision.terminalEvent=true;
        recordMotion(MotionEvent::Decision,reason);
    };
    if(!ready(s) || !movement_ || !navigation_.graph || !index_) { fail(MotionReason::MissingObservation); return; }
    const auto& hull=*s.hull;
    if(hull.minimum.x>=hull.maximum.x || hull.minimum.y>=hull.maximum.y || hull.minimum.z>=hull.maximum.z) {
        fail(MotionReason::MissingObservation); return;
    }
    const nav::corridor::HullClearance clearance{
        (std::max)(std::abs(double(hull.minimum.x)),std::abs(double(hull.maximum.x))),
        (std::max)(std::abs(double(hull.minimum.y)),std::abs(double(hull.maximum.y)))};
    const auto corridor=nav::corridor::Corridor::build(*navigation_.graph,*route.route,clearance,
        {100000,256U*1024U*1024U,1000000});
    if(!corridor) { current_->motionTrace_.corridorError=corridor.error; fail(MotionReason::InvalidCorridor); return; }
    const auto vertex=navigation_.graph->find(route.goal);
    if(!vertex) { fail(MotionReason::InvalidGoal); return; }
    const auto& e=navigation_.graph->area(*vertex).extent;
    const double lowX=double(e.northWest.x)-hull.minimum.x+1, highX=double(e.southEast.x)-hull.maximum.x-1;
    const double lowY=double(e.northWest.y)-hull.minimum.y+1, highY=double(e.southEast.y)-hull.maximum.y-1;
    if(lowX>highX || lowY>highY) { fail(MotionReason::InvalidGoal); return; }
    // An area-id goal chooses a hull-safe XY projection of the current point.
    // Intermediate steering still follows selected portals, never area centers.
    const nav::model::NavVector3 xy{static_cast<float>(std::clamp(double(s.position->x),lowX,highX)),
        static_cast<float>(std::clamp(double(s.position->y),lowY,highY)),0};
    const auto floor=nav::query::projectToArea(e,xy);
    const nav::model::NavVector3 goal{xy.x,xy.y,static_cast<float>(floor.z)};
    auto profile=walkLimits; profile.jump=jumpLimits;
    current_->walk_.emplace(current_->motionTrace_.decision.binding,corridor.value,goal,profile);
    current_->pump_.emplace(current_->motionTrace_.decision.binding); current_->intentWallAgeUs_=0; current_->neutralBinding_.reset();
}
void NavConsole::beforeDispatch(metamod::LifecycleCoordinator& owner) noexcept {
    observe(owner);
    if(current_->guardTick_!=owner.registry().currentTick()) { current_->guardTick_=owner.registry().currentTick(); current_->guardQueries_=0; }
    if(!current_->pendingMotion_ || !movement_) return;
    const auto s=snapshot(owner);
    const auto& pending=*current_->pendingMotion_;
    MotionReason reason=MotionReason::None;
    const auto elapsed=(std::max)(s.elapsedUs,movement_->frameDeltaUs());
    const bool stationary=pending.command.movement==core::Movement{} && pending.command.buttons==0;
    if(!ready(s,pending.jump.has_value() || stationary) || s.actor!=pending.binding.actor || s.agent!=pending.binding.agent || s.map!=pending.binding.map ||
       !s.tick.isAfter(pending.tick) || s.hull->minimum!=pending.observation.hull->minimum ||
       s.hull->maximum!=pending.observation.hull->maximum) reason=MotionReason::MissingObservation;
    else if(!s.elapsedUs || !movement_->frameDeltaUs() || elapsed>pending.remainingFreshUs)
        reason=MotionReason::StaleCommand;
    else {
        const auto& m=pending.command.movement;
        const double speed=std::hypot(double(m.forward),double(m.side));
        const auto delta=movement_->frameDeltaUs();
        const auto msec=std::clamp(delta/1000+(delta%1000>=500 ? 1U:0U),std::uint64_t{1},std::uint64_t{255});
        if(speed>double(*s.speedLimit)+0.001 || (speed>0 && !pending.jump && !pending.contact && (!pending.segment ||
           !segmentAllows(pending.segment->start,pending.segment->end,*s.position,speed*double(msec)/1000))))
            reason=MotionReason::Deviation;
    }
    if(reason!=MotionReason::None) {
        if(current_->walk_ && (pending.command.buttons&static_cast<core::ButtonMask>(core::Button::Jump)))
            (void)current_->walk_->reportJumpDispatch({pending.binding,pending.tick,s.tick,false});
        current_->motionTrace_.commandTick=pending.tick; current_->motionTrace_.dispatchTick=s.tick;
        clearPending(); if(current_->pump_) current_->pump_->submissionRejected();
        current_->motionTrace_.rejected=add(current_->motionTrace_.rejected,1); recordMotion(MotionEvent::Rejected,reason);
        return;
    }
    if(pending.jump) {
        const auto queued=pending.tick; const auto binding=pending.binding;
        const bool press=(pending.command.buttons&static_cast<core::ButtonMask>(core::Button::Jump))!=0;
        inRequest_=true; queryingEntity_=owner.entityFor(current_->actor); queryingPlayers_=&owner.registry(); queryingOwner_=&owner;
        const auto guarded=guardJump(owner,s,pending);
        inRequest_=false; queryingEntity_=nullptr; queryingPlayers_=nullptr; queryingOwner_=nullptr;
        if(deferredInvalidation_) { (void)applyDeferredInvalidation(); return; }
        if(guarded!=MotionReason::None) {
            if(press && current_->walk_) (void)current_->walk_->reportJumpDispatch({binding,queued,s.tick,false});
            current_->motionTrace_.commandTick=queued; current_->motionTrace_.dispatchTick=s.tick;
            clearPending(); if(current_->pump_) current_->pump_->submissionRejected();
            current_->motionTrace_.rejected=add(current_->motionTrace_.rejected,1); recordMotion(MotionEvent::Rejected,guarded);
        }
        return;
    }
    if(s.ducked==true && (pending.command.buttons&static_cast<core::ButtonMask>(core::Button::Duck))==0) {
        const auto queued=pending.tick;
        auto center=*s.position;
        center.z+=s.hull->minimum.z-walkLimits.crouch.standing.minimum.z;
        const nav::runtime::QueryRequest q{{s.agent,s.actor,s.map,s.tick,pending.binding.routeGeneration,++current_->guardQueries_},
            nav::runtime::QueryKind::Clearance,center,center,walkLimits.crouch.standing};
        inRequest_=true;
        const auto result=queryNavWorld(engine_,owner.entityFor(current_->actor),index_.get(),q,globals_ ? globals_->maxEntities:0);
        inRequest_=false;
        if(deferredInvalidation_) { (void)applyDeferredInvalidation(); return; }
        if(result.error!=nav::runtime::QueryError::None || !result.clearance || !result.clearance->clear) {
            current_->motionTrace_.commandTick=queued; current_->motionTrace_.dispatchTick=s.tick;
            clearPending(); if(current_->pump_) current_->pump_->submissionRejected();
            current_->motionTrace_.rejected=add(current_->motionTrace_.rejected,1);
            recordMotion(MotionEvent::Rejected,MotionReason::PostureChanged); return;
        }
    }
    if(pending.contact) {
        const auto contact=*pending.contact; const auto queued=pending.tick;
        const auto command=pending.command;
        bool valid=false;
        if(!current_->guardQueries_) {
            ++current_->guardQueries_; current_->motionTrace_.contactGuardQueries=add(current_->motionTrace_.contactGuardQueries,1);
            nav::runtime::QueryRequest q{{s.agent,s.actor,s.map,s.tick,pending.binding.routeGeneration,1},
                nav::runtime::QueryKind::Door,*s.position,contact.end,s.hull,walkLimits.probe.navTolerance,contact.id};
            inRequest_=true;
            const auto result=queryNavWorld(engine_,owner.entityFor(current_->actor),index_.get(),q,
                globals_ ? globals_->maxEntities:0);
            inRequest_=false;
            if(deferredInvalidation_) {
                (void)applyDeferredInvalidation(); return;
            }
            if(result.error==nav::runtime::QueryError::None && result.door && result.door->id==contact.id &&
               result.door->canTouch && !result.door->open && result.hull && !result.hull->startSolid) {
                const double dx=double(contact.end.x)-s.position->x,dy=double(contact.end.y)-s.position->y;
                const auto& h=*result.hull;
                const double length=std::hypot(dx,dy),distance=std::hypot(double(h.end.x)-s.position->x,double(h.end.y)-s.position->y);
                const auto delta=movement_->frameDeltaUs();
                const auto msec=std::clamp(delta/1000+(delta%1000>=500 ? 1U:0U),std::uint64_t{1},std::uint64_t{255});
                const double speed=std::hypot(command.movement.forward,command.movement.side);
                const double travel=speed*double(msec)/1000;
                constexpr double radians=3.14159265358979323846/180;
                const double yaw=command.view.yaw*radians;
                const double vx=command.movement.forward*std::cos(yaw)+command.movement.side*std::sin(yaw);
                const double vy=command.movement.forward*std::sin(yaw)-command.movement.side*std::cos(yaw);
                valid=length>0 && length<=0.751 && distance<=0.125 && h.fraction>=0 && h.fraction<1 &&
                    travel>=distance+0.001 && travel<=0.75 && command.buttons==0 && command.movement.up==0 &&
                    std::abs(double(h.end.z)-s.position->z)<=0.1 && std::abs(double(contact.end.z)-s.position->z)<=0.1 &&
                    std::abs((double(h.end.x)-s.position->x)*dy-(double(h.end.y)-s.position->y)*dx)/length<=0.01 &&
                    std::abs(distance-length*h.fraction)<=0.02 && h.normal.x*dx/length+h.normal.y*dy/length<= -0.7 &&
                    std::abs(h.normal.z)<=0.2 && speed>0 && (vx*dx+vy*dy)/(speed*length)>0.999;
            }
        }
        if(!valid) {
            current_->motionTrace_.commandTick=queued; current_->motionTrace_.dispatchTick=s.tick;
            clearPending(); if(current_->pump_) current_->pump_->submissionRejected();
            current_->motionTrace_.rejected=add(current_->motionTrace_.rejected,1); recordMotion(MotionEvent::Rejected,MotionReason::DoorChanged);
        }
        return;
    }
    if((pending.command.buttons&static_cast<core::ButtonMask>(core::Button::Use))!=0) {
        // A queued press must still select the same generation at dispatch.
        // No NAV query ordinal is consumed: this bounded, trace-free selection
        // guard is counted separately and executes at most once per frame.
        const auto expected=pending.command.view;
        const auto queued=pending.tick;
        inRequest_=true;
        const auto view=doorUseView(engine_,owner.entityFor(current_->actor),
            current_->motionTrace_.decision.doorId,globals_ ? globals_->maxEntities:0);
        inRequest_=false;
        if(deferredInvalidation_) {
            (void)applyDeferredInvalidation(); return;
        }
        current_->motionTrace_.useGuardChecks=add(current_->motionTrace_.useGuardChecks,1);
        if(!view || std::abs(double(view->x)-expected.pitch)>0.01 ||
           std::abs(double(view->y)-expected.yaw)>0.01 || std::abs(double(view->z)-expected.roll)>0.01) {
            current_->motionTrace_.commandTick=queued; current_->motionTrace_.dispatchTick=s.tick;
            clearPending(); if(current_->pump_) current_->pump_->submissionRejected();
            current_->motionTrace_.rejected=add(current_->motionTrace_.rejected,1); recordMotion(MotionEvent::Rejected,MotionReason::DoorChanged);
        }
    }
}
void NavConsole::afterDispatch(const metamod::MovementResult& result,core::TickId tick,
    const std::optional<MotionTrace>& ticket) noexcept {
    if(!ticket) return;
    const auto& b=ticket->decision.binding;
    const auto& current=current_->motionTrace_.decision.binding;
    const bool sameRoute=b.agent==current.agent && b.actor==current.actor && b.map==current.map &&
        b.routeGeneration==current.routeGeneration;
    const auto saved=current_->motionTrace_;
    if(!sameRoute) current_->motionTrace_=*ticket; // reentrant goto must not receive old-route feedback
    if(sameRoute && current_->walk_ && (ticket->command.buttons&static_cast<core::ButtonMask>(core::Button::Jump)))
        (void)current_->walk_->reportJumpDispatch({b,ticket->commandTick,tick,result.dispatched()});
    if(current_->pendingMotion_ && current_->pendingMotion_->tick==ticket->commandTick && current_->pendingMotion_->binding.actor==b.actor &&
       current_->pendingMotion_->binding.map==b.map && current_->pendingMotion_->binding.routeGeneration==b.routeGeneration)
        current_->pendingMotion_.reset();
    current_->motionTrace_.commandTick=ticket->commandTick; current_->motionTrace_.dispatchTick=tick;
    current_->motionTrace_.transportError=result.error;
    if(result.dispatched()) {
        current_->motionTrace_.dispatched=add(current_->motionTrace_.dispatched,1); recordMotion(MotionEvent::Dispatched);
    } else {
        if(sameRoute && current_->pump_) current_->pump_->submissionRejected();
        current_->motionTrace_.rejected=add(current_->motionTrace_.rejected,1); recordMotion(MotionEvent::Rejected,MotionReason::TransportRejected);
    }
    if(!sameRoute) current_->motionTrace_=saved;
}
void NavConsole::moveFrame(metamod::LifecycleCoordinator& owner) noexcept {
    observe(owner);
    if(inRequest_ || !movement_) return;
    if(owner.registry().currentTick().isAfter(current_->navigationTimeTick_)) {
        current_->navigationTimeTick_=owner.registry().currentTick();
        current_->navigationTimeUs_=add(current_->navigationTimeUs_,movement_->frameDeltaUs());
    }
    if(runReplan(owner)) return;
    if(current_->neutralBinding_) {
        const auto s=snapshot(owner);
        if(!ready(s,true) || s.actor!=current_->neutralBinding_->actor || s.agent!=current_->neutralBinding_->agent || s.map!=current_->neutralBinding_->map) {
            current_->neutralBinding_.reset(); return;
        }
        if(!s.elapsedUs || !s.tick.isAfter(current_->motionTrace_.commandTick)) return;
        current_->motionTrace_.decision.binding=*current_->neutralBinding_; current_->neutralBinding_.reset();
        core::MovementIntent neutral;
        if(current_->neutralDuck_ || s.ducked==true) neutral.duck=core::ActionRequest::Hold;
        submitMotion(s,owner,neutral,true,0); return;
    }
    if(inRequest_ || !current_->walk_ || !current_->pump_ || !movement_ || !current_->session_ || !current_->session_->executable() ||
       current_->walk_->state()!=nav::local::WalkState::Running) return;
    const auto s=snapshot(owner);
    if(!s.tick.isAfter(current_->requestTick_)) return; // Route request owns ordinal 1 on its tick.
    const auto schedule=current_->pump_->beginFrame(s);
    if(!schedule.accepted) { stopMotion(); return; }
    current_->intentWallAgeUs_=add(current_->intentWallAgeUs_,movement_->frameDeltaUs());
    if(schedule.decisionDue) {
        queryingEntity_=owner.entityFor(current_->actor); queryingPlayers_=&owner.registry(); queryingOwner_=&owner; inRequest_=true;
        const auto index=index_; // pins navigation across synchronous host reentry
        const auto reserved=current_->guardTick_==s.tick ? current_->guardQueries_:0;
        auto binding=current_->motionTrace_.decision.binding; binding.step=current_->walk_->step();
        const auto physics=standardJumpPhysics(engine_,queryingEntity_,binding,s.tick);
        const auto decision=current_->walk_->update(s,*index,navigation_.map,*this,current_->pump_->timeUs(),reserved,physics);
        inRequest_=false; queryingEntity_=nullptr; queryingPlayers_=nullptr; queryingOwner_=nullptr;
        if(deferredInvalidation_) {
            (void)applyDeferredInvalidation(); return;
        }
        current_->motionTrace_.decision=decision; current_->motionTrace_.missedDecisions=add(current_->motionTrace_.missedDecisions,schedule.missedDeadlines);
        current_->segment_=decision.target && s.position ? std::optional<Segment>{{*s.position,decision.target->origin}}:std::nullopt;
        current_->intentWallAgeUs_=0;
        recordMotion(MotionEvent::Decision);
        if(decision.terminalEvent && decision.reason==nav::local::WalkReason::DynamicBlocked &&
           decision.blockerAction==nav::local::BlockerAction::Replan &&
           decision.blockerReason==nav::local::BlockerReason::TimedOut && decision.blocker &&
           decision.blocker->id && current_->motionTrace_.selectedEdge) {
            const auto& blocker=*decision.blocker;
            const bool player=blocker.kind==nav::runtime::BlockerKind::Player ||
                blocker.kind==nav::runtime::BlockerKind::Teammate || blocker.kind==nav::runtime::BlockerKind::Enemy;
            if((player && blocker.player && blocker.player->isValid() && !blocker.player->sameSlot(s.actor)) ||
               (blocker.kind==nav::runtime::BlockerKind::Other && !blocker.player)) {
                (void)current_->replan_.schedule(decision.binding,*current_->motionTrace_.selectedEdge,s.tick,current_->navigationTimeUs_);
                printReplan();
            }
        }
        if(!current_->pump_->publish(decision.binding,s.tick,decision.intent)) return;
    }
    const auto output=current_->pump_->take();
    if(!output.emit || !ready(s,current_->motionTrace_.decision.jumpState.has_value())) return;
    const auto age=(std::max)(output.intentAgeUs,current_->intentWallAgeUs_);
    if(age>nav::local::IntentPump::maxIntentAgeUs) { current_->pump_->stop(nav::local::PumpReason::StaleIntent); return; }
    submitMotion(s,owner,output.intent,output.firstFrame,age);
}
void NavConsole::submitMotion(const nav::runtime::MovementSnapshot& s,metamod::LifecycleCoordinator& owner,
    const core::MovementIntent& intent,bool firstFrame,std::uint64_t age) noexcept {
    const auto contact=firstFrame ? current_->motionTrace_.decision.contact:std::nullopt;
    auto effective=current_->motionTrace_.decision.contact && !firstFrame ? core::MovementIntent{}:intent;
    const auto& decision=current_->motionTrace_.decision;
    if(s.grounded==true && (decision.jumpState==nav::local::JumpState::Airborne || decision.jumpState==nav::local::JumpState::Recover)) {
        effective={}; effective.jump=core::ActionRequest::Release;
    }
    if(!firstFrame && effective.duck==core::ActionRequest::Release)
        effective.duck=s.ducked==true ? core::ActionRequest::Hold:core::ActionRequest::None;
    if(s.ducked==true && effective.duck==core::ActionRequest::None) effective.duck=core::ActionRequest::Hold;
    const auto command=core::Motor::command(effective,{s.view->x,s.view->y,s.view->z},*s.speedLimit,s.elapsedUs,firstFrame);
    if(!command) {
        if(current_->pump_) current_->pump_->submissionRejected();
        current_->motionTrace_.rejected=add(current_->motionTrace_.rejected,1); recordMotion(MotionEvent::Rejected,MotionReason::MotorRejected); return;
    }
    const auto result=owner.submitCommand(s.actor,s.map,s.tick,*command.command);
    current_->motionTrace_.command=*command.command;
    current_->motionTrace_.commandTick=s.tick; current_->motionTrace_.dispatchTick={}; current_->motionTrace_.intentAgeUs=age;
    current_->motionTrace_.transportError=result.error;
    if(result.queued()) {
        current_->pendingMotion_=PendingMotion{current_->motionTrace_.decision.binding,s.tick,nav::local::IntentPump::maxIntentAgeUs-age,
            s,*command.command,current_->segment_,contact};
        if(decision.jumpState && decision.jumpPlan && decision.jumpPhysics)
            current_->pendingMotion_->jump=JumpTicket{*decision.jumpPlan,*decision.jumpPhysics,*decision.jumpState,decision.jumpPressTick};
        current_->motionTrace_.queued=add(current_->motionTrace_.queued,1); recordMotion(MotionEvent::Queued);
    } else {
        if(current_->walk_ && (command.command->buttons&static_cast<core::ButtonMask>(core::Button::Jump)))
            (void)current_->walk_->reportJumpDispatch({decision.binding,s.tick,s.tick,false});
        if(current_->pump_) current_->pump_->submissionRejected();
        current_->motionTrace_.rejected=add(current_->motionTrace_.rejected,1);
        recordMotion(MotionEvent::Rejected,MotionReason::TransportRejected);
    }
}
}
