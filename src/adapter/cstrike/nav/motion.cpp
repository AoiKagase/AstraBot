// SPDX-License-Identifier: MPL-2.0
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include "adapter/cstrike/nav/console.hpp"
#include "adapter/metamod/lifecycle.hpp"
#include "adapter/cstrike/nav/world_queries.hpp"
#ifdef snprintf
#undef snprintf
#endif
namespace astrabot::adapter::cstrike {
namespace {
constexpr nav::local::WalkLimits walkLimits{{21,4,48,16,18,18,64,4,18,0.7},160,1,1,3,1000000,3000000};
std::uint64_t add(std::uint64_t a,std::uint64_t b) noexcept {
    const auto maximum=(std::numeric_limits<std::uint64_t>::max)();
    return b>maximum-a ? maximum:a+b;
}
bool ready(const nav::runtime::MovementSnapshot& s) noexcept {
    return s.kind==nav::runtime::ActorKind::ManagedBot && s.connected==true && s.alive==true &&
        s.joined==true && s.grounded==true && s.position && s.position->isFinite() &&
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
    if(event==MotionEvent::Decision) motionTrace_.selectedEdge.reset();
    if(session_ && session_->trace().route && session_->trace().routeGeneration==motionTrace_.decision.binding.routeGeneration &&
       motionTrace_.decision.binding.step<session_->trace().route->steps.size())
        motionTrace_.selectedEdge=session_->trace().route->steps[motionTrace_.decision.binding.step].edge;
    motionTrace_.event=event; motionTrace_.reason=reason;
    motionSequence_=add(motionSequence_,1); motionTrace_.sequence=motionSequence_;
    motionHistory_[motionNext_]=motionTrace_; motionNext_=(motionNext_+1)%motionHistoryLimit;
    motionCount_=(std::min)(motionCount_+1,motionHistoryLimit);
    if(event==MotionEvent::Rejected || event==MotionEvent::Cancelled ||
       (event==MotionEvent::Decision && motionTrace_.decision.terminalEvent)) printMotion();
}
void NavConsole::printMotion() noexcept {
    if(!motionTrace_.decision.binding.routeGeneration) return;
    const auto& d=motionTrace_.decision;
    const auto target=d.target ? d.target->origin:nav::model::NavVector3{};
    char text[1280]{};
    std::snprintf(text,sizeof(text),
        "walk actor=%u:%u map=%u route=%llu step=%zu tick=%llu state=%s reason=%u probe=%u event=%u motion_reason=%u corridor=%u transport=%u command_tick=%llu dispatch_tick=%llu age_us=%llu speed=%.6g direction=(%.6g,%.6g) target_present=%u target=(%.6g,%.6g,%.6g) support=%u queries=%u samples=%u step_probes=%u queued=%llu dispatched=%llu rejected=%llu missed=%llu history=%zu omitted=%llu edge=%u:%u command=(%.6g,%.6g,%u) door=%llu door_state=%u door_reason=%u use_checks=%llu contact_pulse=%u contact_guards=%llu",
        unsigned(d.binding.actor.slot),unsigned(d.binding.actor.generation.value),unsigned(d.binding.map.value),
        static_cast<unsigned long long>(d.binding.routeGeneration),d.binding.step,static_cast<unsigned long long>(d.tick.value),
        walkState(d.state),unsigned(d.reason),unsigned(d.probeReason),unsigned(motionTrace_.event),unsigned(motionTrace_.reason),
        unsigned(motionTrace_.corridorError),unsigned(motionTrace_.transportError),
        static_cast<unsigned long long>(motionTrace_.commandTick.value),static_cast<unsigned long long>(motionTrace_.dispatchTick.value),
        static_cast<unsigned long long>(motionTrace_.intentAgeUs),d.intent.speed,d.intent.direction.x,d.intent.direction.y,
        unsigned(d.target.has_value()),double(target.x),double(target.y),double(target.z),
        d.support ? unsigned(d.support->area.value):0U,d.queries,d.samples,d.steps,
        static_cast<unsigned long long>(motionTrace_.queued),static_cast<unsigned long long>(motionTrace_.dispatched),
        static_cast<unsigned long long>(motionTrace_.rejected),static_cast<unsigned long long>(motionTrace_.missedDecisions),
        motionCount_,static_cast<unsigned long long>(motionSequence_-motionCount_),
        motionTrace_.selectedEdge ? unsigned(motionTrace_.selectedEdge->source.value):0U,
        motionTrace_.selectedEdge ? unsigned(motionTrace_.selectedEdge->target.value):0U,
        double(motionTrace_.command.movement.forward),double(motionTrace_.command.movement.side),unsigned(motionTrace_.command.msec),
        static_cast<unsigned long long>(d.doorId),d.doorState ? unsigned(*d.doorState):0U,unsigned(d.doorReason),
        static_cast<unsigned long long>(motionTrace_.useGuardChecks),unsigned(d.contact.has_value()),
        static_cast<unsigned long long>(motionTrace_.contactGuardQueries));
    line(text);
}
void NavConsole::clearPending() noexcept {
    if(pendingMotion_ && movement_)
        (void)movement_->cancel(pendingMotion_->binding.actor,pendingMotion_->binding.map,pendingMotion_->tick);
    pendingMotion_.reset();
}
void NavConsole::stopMotion() noexcept {
    if(walk_ || pendingMotion_) neutralBinding_=motionTrace_.decision.binding;
    clearPending();
    if(walk_ && walk_->state()==nav::local::WalkState::Running) {
        motionTrace_.decision=walk_->abort(); recordMotion(MotionEvent::Cancelled,MotionReason::Cancelled);
    }
    walk_.reset(); pump_.reset(); segment_.reset(); intentWallAgeUs_=0;
}
void NavConsole::startMotion(const nav::runtime::MovementSnapshot& s) noexcept {
    motionTrace_={}; motionNext_=motionCount_=0; motionSequence_=0; requestTick_=s.tick;
    if(!session_ || !session_->executable()) return;
    const auto& route=session_->trace();
    motionTrace_.decision.binding={s.agent,s.actor,s.map,route.routeGeneration,0};
    if(neutralBinding_) neutralBinding_=motionTrace_.decision.binding;
    motionTrace_.decision.tick=s.tick;
    const auto fail=[&](MotionReason reason) {
        motionTrace_.decision.state=nav::local::WalkState::Failed;
        motionTrace_.decision.terminalEvent=true;
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
    if(!corridor) { motionTrace_.corridorError=corridor.error; fail(MotionReason::InvalidCorridor); return; }
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
    walk_.emplace(motionTrace_.decision.binding,corridor.value,goal,walkLimits);
    pump_.emplace(motionTrace_.decision.binding); intentWallAgeUs_=0; neutralBinding_.reset();
}
void NavConsole::beforeDispatch(metamod::LifecycleCoordinator& owner) noexcept {
    observe(owner);
    if(guardTick_!=owner.registry().currentTick()) { guardTick_=owner.registry().currentTick(); guardQueries_=0; }
    if(!pendingMotion_ || !movement_) return;
    const auto s=snapshot(owner);
    const auto& pending=*pendingMotion_;
    MotionReason reason=MotionReason::None;
    const auto elapsed=(std::max)(s.elapsedUs,movement_->frameDeltaUs());
    if(!ready(s) || s.actor!=pending.binding.actor || s.agent!=pending.binding.agent || s.map!=pending.binding.map ||
       !s.tick.isAfter(pending.tick) || s.hull->minimum!=pending.observation.hull->minimum ||
       s.hull->maximum!=pending.observation.hull->maximum) reason=MotionReason::MissingObservation;
    else if(!s.elapsedUs || !movement_->frameDeltaUs() || elapsed>pending.remainingFreshUs)
        reason=MotionReason::StaleCommand;
    else {
        const auto& m=pending.command.movement;
        const double speed=std::hypot(double(m.forward),double(m.side));
        const auto delta=movement_->frameDeltaUs();
        const auto msec=std::clamp(delta/1000+(delta%1000>=500 ? 1U:0U),std::uint64_t{1},std::uint64_t{255});
        if(speed>double(*s.speedLimit)+0.001 || (speed>0 && !pending.contact && (!pending.segment ||
           !segmentAllows(pending.segment->start,pending.segment->end,*s.position,speed*double(msec)/1000))))
            reason=MotionReason::Deviation;
    }
    if(reason!=MotionReason::None) {
        motionTrace_.commandTick=pending.tick; motionTrace_.dispatchTick=s.tick;
        clearPending(); if(pump_) pump_->submissionRejected();
        motionTrace_.rejected=add(motionTrace_.rejected,1); recordMotion(MotionEvent::Rejected,reason);
        return;
    }
    if(pending.contact) {
        const auto contact=*pending.contact; const auto queued=pending.tick;
        const auto command=pending.command;
        bool valid=false;
        if(!guardQueries_) {
            ++guardQueries_; motionTrace_.contactGuardQueries=add(motionTrace_.contactGuardQueries,1);
            nav::runtime::QueryRequest q{{s.agent,s.actor,s.map,s.tick,pending.binding.routeGeneration,1},
                nav::runtime::QueryKind::Door,*s.position,contact.end,s.hull,walkLimits.probe.navTolerance,contact.id};
            inRequest_=true;
            const auto result=queryNavWorld(engine_,owner.fakeClient().activeEntity(),index_.get(),q,
                globals_ ? globals_->maxEntities:0);
            inRequest_=false;
            if(deferredInvalidation_) {
                const auto why=*deferredInvalidation_; deferredInvalidation_.reset(); invalidate(why); return;
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
            motionTrace_.commandTick=queued; motionTrace_.dispatchTick=s.tick;
            clearPending(); if(pump_) pump_->submissionRejected();
            motionTrace_.rejected=add(motionTrace_.rejected,1); recordMotion(MotionEvent::Rejected,MotionReason::DoorChanged);
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
        const auto view=doorUseView(engine_,owner.fakeClient().activeEntity(),
            motionTrace_.decision.doorId,globals_ ? globals_->maxEntities:0);
        inRequest_=false;
        if(deferredInvalidation_) {
            const auto why=*deferredInvalidation_; deferredInvalidation_.reset(); invalidate(why); return;
        }
        motionTrace_.useGuardChecks=add(motionTrace_.useGuardChecks,1);
        if(!view || std::abs(double(view->x)-expected.pitch)>0.01 ||
           std::abs(double(view->y)-expected.yaw)>0.01 || std::abs(double(view->z)-expected.roll)>0.01) {
            motionTrace_.commandTick=queued; motionTrace_.dispatchTick=s.tick;
            clearPending(); if(pump_) pump_->submissionRejected();
            motionTrace_.rejected=add(motionTrace_.rejected,1); recordMotion(MotionEvent::Rejected,MotionReason::DoorChanged);
        }
    }
}
void NavConsole::afterDispatch(const metamod::MovementResult& result,core::TickId tick,
    const std::optional<MotionTrace>& ticket) noexcept {
    if(!ticket) return;
    const auto& b=ticket->decision.binding;
    const auto& current=motionTrace_.decision.binding;
    const bool sameRoute=b.agent==current.agent && b.actor==current.actor && b.map==current.map &&
        b.routeGeneration==current.routeGeneration;
    const auto saved=motionTrace_;
    if(!sameRoute) motionTrace_=*ticket; // reentrant goto must not receive old-route feedback
    if(pendingMotion_ && pendingMotion_->tick==ticket->commandTick && pendingMotion_->binding.actor==b.actor &&
       pendingMotion_->binding.map==b.map && pendingMotion_->binding.routeGeneration==b.routeGeneration)
        pendingMotion_.reset();
    motionTrace_.commandTick=ticket->commandTick; motionTrace_.dispatchTick=tick;
    motionTrace_.transportError=result.error;
    if(result.dispatched()) {
        motionTrace_.dispatched=add(motionTrace_.dispatched,1); recordMotion(MotionEvent::Dispatched);
    } else {
        if(sameRoute && pump_) pump_->submissionRejected();
        motionTrace_.rejected=add(motionTrace_.rejected,1); recordMotion(MotionEvent::Rejected,MotionReason::TransportRejected);
    }
    if(!sameRoute) motionTrace_=saved;
}
void NavConsole::moveFrame(metamod::LifecycleCoordinator& owner) noexcept {
    observe(owner);
    if(inRequest_ || !movement_) return;
    if(neutralBinding_) {
        const auto s=snapshot(owner);
        if(!ready(s) || s.actor!=neutralBinding_->actor || s.agent!=neutralBinding_->agent || s.map!=neutralBinding_->map) {
            neutralBinding_.reset(); return;
        }
        if(!s.elapsedUs || !s.tick.isAfter(motionTrace_.commandTick)) return;
        motionTrace_.decision.binding=*neutralBinding_; neutralBinding_.reset();
        submitMotion(s,owner,{},true,0); return;
    }
    if(inRequest_ || !walk_ || !pump_ || !movement_ || !session_ || !session_->executable() ||
       walk_->state()!=nav::local::WalkState::Running) return;
    const auto s=snapshot(owner);
    if(!s.tick.isAfter(requestTick_)) return; // Route request owns ordinal 1 on its tick.
    const auto schedule=pump_->beginFrame(s);
    if(!schedule.accepted) { stopMotion(); return; }
    intentWallAgeUs_=add(intentWallAgeUs_,movement_->frameDeltaUs());
    if(schedule.decisionDue) {
        queryingEntity_=owner.fakeClient().activeEntity(); inRequest_=true;
        const auto index=index_; // pins navigation across synchronous host reentry
        const auto reserved=guardTick_==s.tick ? guardQueries_:0;
        const auto decision=walk_->update(s,*index,navigation_.map,*this,pump_->timeUs(),reserved);
        inRequest_=false; queryingEntity_=nullptr;
        if(deferredInvalidation_) {
            const auto reason=*deferredInvalidation_; deferredInvalidation_.reset(); invalidate(reason); return;
        }
        motionTrace_.decision=decision; motionTrace_.missedDecisions=add(motionTrace_.missedDecisions,schedule.missedDeadlines);
        segment_=decision.target && s.position ? std::optional<Segment>{{*s.position,decision.target->origin}}:std::nullopt;
        intentWallAgeUs_=0;
        recordMotion(MotionEvent::Decision);
        if(!pump_->publish(decision.binding,s.tick,decision.intent)) return;
    }
    const auto output=pump_->take();
    if(!output.emit || !ready(s)) return;
    const auto age=(std::max)(output.intentAgeUs,intentWallAgeUs_);
    if(age>nav::local::IntentPump::maxIntentAgeUs) { pump_->stop(nav::local::PumpReason::StaleIntent); return; }
    submitMotion(s,owner,output.intent,output.firstFrame,age);
}
void NavConsole::submitMotion(const nav::runtime::MovementSnapshot& s,metamod::LifecycleCoordinator& owner,
    const core::MovementIntent& intent,bool firstFrame,std::uint64_t age) noexcept {
    const auto contact=firstFrame ? motionTrace_.decision.contact:std::nullopt;
    const auto effective=motionTrace_.decision.contact && !firstFrame ? core::MovementIntent{}:intent;
    const auto command=core::Motor::command(effective,{s.view->x,s.view->y,s.view->z},*s.speedLimit,s.elapsedUs,firstFrame);
    if(!command) {
        if(pump_) pump_->submissionRejected();
        motionTrace_.rejected=add(motionTrace_.rejected,1); recordMotion(MotionEvent::Rejected,MotionReason::MotorRejected); return;
    }
    const auto result=owner.submitCommand(s.actor,s.map,s.tick,*command.command);
    motionTrace_.command=*command.command;
    motionTrace_.commandTick=s.tick; motionTrace_.dispatchTick={}; motionTrace_.intentAgeUs=age;
    motionTrace_.transportError=result.error;
    if(result.queued()) {
        pendingMotion_=PendingMotion{motionTrace_.decision.binding,s.tick,nav::local::IntentPump::maxIntentAgeUs-age,
            s,*command.command,segment_,contact};
        motionTrace_.queued=add(motionTrace_.queued,1); recordMotion(MotionEvent::Queued);
    } else {
        if(pump_) pump_->submissionRejected();
        motionTrace_.rejected=add(motionTrace_.rejected,1);
        recordMotion(MotionEvent::Rejected,MotionReason::TransportRejected);
    }
}
}
