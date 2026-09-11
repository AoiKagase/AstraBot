// SPDX-License-Identifier: MPL-2.0
#include "adapter/cstrike/nav/jump_motion.hpp"
#include "adapter/cstrike/nav/console.hpp"
#include "adapter/metamod/lifecycle.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace astrabot::adapter::cstrike {
std::optional<nav::local::JumpPhysics> standardJumpPhysics(enginefuncs_t* engine,edict_t* entity,
    nav::local::Binding binding,core::TickId tick) noexcept {
    if(!engine || !engine->pfnCVarGetPointer || !entity || entity->free || !tick.isValid() ||
       entity->v.movetype!=MOVETYPE_WALK || entity->v.waterlevel!=0 || (entity->v.flags&FL_WATERJUMP) ||
       entity->v.basevelocity.x!=0 || entity->v.basevelocity.y!=0 || entity->v.basevelocity.z!=0 ||
       !std::isfinite(entity->v.gravity)) return {};
    const auto* support=entity->v.groundentity;
    if(support && (support->free || support->v.velocity.x!=0 || support->v.velocity.y!=0 || support->v.velocity.z!=0 ||
        support->v.avelocity.x!=0 || support->v.avelocity.y!=0 || support->v.avelocity.z!=0)) return {};
    const auto* gravity=engine->pfnCVarGetPointer("sv_gravity"); if(!gravity) return {};
    const double base=gravity->value,multiplier=entity->v.gravity==0 ? 1:entity->v.gravity;
    const auto* height=engine->pfnCVarGetPointer("mp_jump_height");
    const double jumpHeight=height ? height->value:45; // Ordinary CS branch in the pinned model.
    const double effective=base*multiplier;
    if(!std::isfinite(effective) || effective<=0 || effective>4000 || !std::isfinite(jumpHeight) || jumpHeight<=0 || jumpHeight>64)
        return {};
    const nav::runtime::HullDimensions standing{{-16,-16,-36},{16,16,36}}, crouching{{-16,-16,-18},{16,16,18}};
    const auto expected=(entity->v.flags&FL_DUCKING) ? crouching:standing;
    if(entity->v.mins.x!=expected.minimum.x || entity->v.mins.y!=expected.minimum.y ||
       entity->v.mins.z!=expected.minimum.z || entity->v.maxs.x!=expected.maximum.x ||
       entity->v.maxs.y!=expected.maximum.y || entity->v.maxs.z!=expected.maximum.z) return {};
    return nav::local::JumpPhysics{binding,tick,effective,std::sqrt(1600*jumpHeight),standing,crouching,1.0/3.0};
}

JumpPhysicsAssessment assessStandardJumpPhysics(enginefuncs_t* engine,edict_t* entity,
    nav::local::Binding binding,core::TickId tick) noexcept {
    JumpPhysicsAssessment result{};
    const auto fail=[&](JumpPhysicsReason reason) noexcept {
        result.reason=reason; return result;
    };
    if(!engine || !engine->pfnCVarGetPointer) return fail(JumpPhysicsReason::MissingHost);
    if(!entity || entity->free) return fail(JumpPhysicsReason::MissingActor);
    result.moveType=entity->v.movetype;
    result.waterLevel=entity->v.waterlevel;
    result.flags=entity->v.flags;
    result.baseVelocity={entity->v.basevelocity.x,entity->v.basevelocity.y,entity->v.basevelocity.z};
    result.actorHull=nav::runtime::HullDimensions{
        {entity->v.mins.x,entity->v.mins.y,entity->v.mins.z},
        {entity->v.maxs.x,entity->v.maxs.y,entity->v.maxs.z}};
    if(!tick.isValid()) return fail(JumpPhysicsReason::InvalidTick);
    if(entity->v.movetype!=MOVETYPE_WALK) return fail(JumpPhysicsReason::MoveType);
    if(entity->v.waterlevel!=0) return fail(JumpPhysicsReason::Water);
    if(entity->v.flags&FL_WATERJUMP) return fail(JumpPhysicsReason::WaterJump);
    if(entity->v.basevelocity.x!=0 || entity->v.basevelocity.y!=0 || entity->v.basevelocity.z!=0)
        return fail(JumpPhysicsReason::BaseVelocity);
    if(!std::isfinite(entity->v.gravity)) return fail(JumpPhysicsReason::InvalidGravity);
    const auto* support=entity->v.groundentity;
    result.movingSupport=support && (support->free || support->v.velocity.x!=0 ||
        support->v.velocity.y!=0 || support->v.velocity.z!=0 || support->v.avelocity.x!=0 ||
        support->v.avelocity.y!=0 || support->v.avelocity.z!=0);
    if(result.movingSupport) return fail(JumpPhysicsReason::MovingSupport);
    const auto* gravity=engine->pfnCVarGetPointer("sv_gravity");
    if(!gravity) return fail(JumpPhysicsReason::MissingGravity);
    const double base=gravity->value,multiplier=entity->v.gravity==0 ? 1:entity->v.gravity;
    const auto* height=engine->pfnCVarGetPointer("mp_jump_height");
    const double jumpHeight=height ? height->value:45;
    const double effective=base*multiplier;
    if(!std::isfinite(effective) || effective<=0 || effective>4000)
        return fail(JumpPhysicsReason::InvalidGravity);
    if(!std::isfinite(jumpHeight) || jumpHeight<=0 || jumpHeight>64)
        return fail(JumpPhysicsReason::InvalidJumpHeight);
    const nav::runtime::HullDimensions standing{{-16,-16,-36},{16,16,36}};
    const nav::runtime::HullDimensions crouching{{-16,-16,-18},{16,16,18}};
    const auto sameHull=[](const nav::runtime::HullDimensions& a,
                           const nav::runtime::HullDimensions& b) noexcept {
        return a.minimum==b.minimum && a.maximum==b.maximum;
    };
    const bool standingHull=sameHull(*result.actorHull,standing);
    const bool crouchingHull=sameHull(*result.actorHull,crouching);
    if(!standingHull && !crouchingHull) {
        result.posture=JumpActorPosture::Invalid;
        return fail(JumpPhysicsReason::NonCanonicalHull);
    }
    const bool duckFlag=(entity->v.flags&FL_DUCKING)!=0;
    result.posture=duckFlag==crouchingHull
        ? (duckFlag ? JumpActorPosture::Crouching:JumpActorPosture::Standing)
        : JumpActorPosture::Transition;
    result.physics=nav::local::JumpPhysics{binding,tick,effective,
        std::sqrt(1600*jumpHeight),standing,crouching,1.0/3.0};
    return result;
}
const char* jumpPhysicsReasonName(JumpPhysicsReason reason) noexcept {
    switch(reason) {
    case JumpPhysicsReason::None:return "None";
    case JumpPhysicsReason::MissingHost:return "MissingHost";
    case JumpPhysicsReason::MissingActor:return "MissingActor";
    case JumpPhysicsReason::InvalidTick:return "InvalidTick";
    case JumpPhysicsReason::MoveType:return "MoveType";
    case JumpPhysicsReason::Water:return "Water";
    case JumpPhysicsReason::WaterJump:return "WaterJump";
    case JumpPhysicsReason::BaseVelocity:return "BaseVelocity";
    case JumpPhysicsReason::MovingSupport:return "MovingSupport";
    case JumpPhysicsReason::MissingGravity:return "MissingGravity";
    case JumpPhysicsReason::InvalidGravity:return "InvalidGravity";
    case JumpPhysicsReason::InvalidJumpHeight:return "InvalidJumpHeight";
    case JumpPhysicsReason::NonCanonicalHull:return "NonCanonicalHull";
    case JumpPhysicsReason::TicketStale:return "TicketStale";
    case JumpPhysicsReason::BindingChanged:return "BindingChanged";
    case JumpPhysicsReason::GravityChanged:return "GravityChanged";
    case JumpPhysicsReason::ImpulseChanged:return "ImpulseChanged";
    case JumpPhysicsReason::CrouchMultiplierChanged:return "CrouchMultiplierChanged";
    case JumpPhysicsReason::PostureIncompatible:return "PostureIncompatible";
    }
    return "Unknown";
}
const char* jumpActorPostureName(JumpActorPosture posture) noexcept {
    switch(posture) {
    case JumpActorPosture::Unknown:return "Unknown";
    case JumpActorPosture::Standing:return "Standing";
    case JumpActorPosture::Crouching:return "Crouching";
    case JumpActorPosture::Transition:return "Transition";
    case JumpActorPosture::Invalid:return "Invalid";
    }
    return "Unknown";
}

namespace {
bool samePhysicsScalar(double value,double expected) noexcept {
    if(value==expected) return true;
    if(!std::isfinite(value) || !std::isfinite(expected)) return false;
    double lower=expected,upper=expected;
    for(int i=0;i<4;++i) {
        lower=std::nextafter(lower,-std::numeric_limits<double>::infinity());
        upper=std::nextafter(upper,std::numeric_limits<double>::infinity());
    }
    return value>=lower && value<=upper;
}
bool sameBinding(const nav::local::Binding& a,const nav::local::Binding& b) noexcept {
    return a.agent==b.agent && a.actor==b.actor && a.map==b.map &&
        a.routeGeneration==b.routeGeneration && a.step==b.step;
}
class GuardQueries final : public nav::runtime::IWorldQueries {
public:
    GuardQueries(NavConsole& port,std::uint32_t& count,std::uint64_t& total) noexcept : port_(port),count_(count),total_(total) {}
    nav::runtime::WorldQueryResult query(const nav::runtime::QueryRequest& request) override {
        if(count_>=21) {
            nav::runtime::WorldQueryResult r; r.stamp=request.stamp; r.kind=request.kind;
            r.error=nav::runtime::QueryError::BudgetExceeded; return r;
        }
        auto wire=request; wire.stamp.ordinal=++count_;
        if(total_!=(std::numeric_limits<std::uint64_t>::max)()) ++total_;
        auto r=port_.query(wire);
        if(r.stamp==wire.stamp) r.stamp=request.stamp; else r.stamp={};
        return r;
    }
private:
    NavConsole& port_; std::uint32_t& count_; std::uint64_t& total_;
};
}
JumpPhysicsReason validateJumpPhysicsTicket(const nav::local::JumpPhysics& ticket,
    const nav::local::JumpPhysics& current,nav::local::Binding binding,
    core::TickId pendingTick) noexcept {
    const auto sameHull=[](const std::optional<nav::runtime::HullDimensions>& a,
                           const std::optional<nav::runtime::HullDimensions>& b) noexcept {
        return a.has_value()==b.has_value() &&
            (!a || (a->minimum==b->minimum && a->maximum==b->maximum));
    };
    if(ticket.tick!=pendingTick) return JumpPhysicsReason::TicketStale;
    if(!sameBinding(ticket.binding,binding)) return JumpPhysicsReason::BindingChanged;
    if(!samePhysicsScalar(current.gravity,ticket.gravity)) return JumpPhysicsReason::GravityChanged;
    if(!samePhysicsScalar(current.verticalImpulse,ticket.verticalImpulse))
        return JumpPhysicsReason::ImpulseChanged;
    if(!samePhysicsScalar(current.crouchSpeedMultiplier,ticket.crouchSpeedMultiplier))
        return JumpPhysicsReason::CrouchMultiplierChanged;
    if(!sameHull(current.standingHull,ticket.standingHull) ||
       !sameHull(current.crouchingHull,ticket.crouchingHull))
        return JumpPhysicsReason::PostureIncompatible;
    return JumpPhysicsReason::None;
}
MotionReason NavConsole::guardJump(metamod::LifecycleCoordinator& owner,const nav::runtime::MovementSnapshot& s,
    const PendingMotion& pending) noexcept {
    using namespace nav::local;
    using namespace nav::runtime;
    const auto& ticket=*pending.jump; const auto& command=pending.command;
    current_->motionTrace_.jumpGuardReason=JumpGuardReason::Unknown;
    const auto reject=[&](JumpGuardReason why,MotionReason reason=MotionReason::JumpChanged) noexcept {
        current_->motionTrace_.jumpGuardReason=why; return reason;
    };
    const auto accept=[&]() noexcept {
        current_->motionTrace_.jumpGuardReason=JumpGuardReason::None;
        return MotionReason::None;
    };
    const bool press=(command.buttons&static_cast<core::ButtonMask>(core::Button::Jump))!=0;
    const double speed=std::hypot(command.movement.forward,command.movement.side);
    if(command.impulse || command.movement.up!=0 ||
       (command.buttons&~(static_cast<core::ButtonMask>(core::Button::Jump)|static_cast<core::ButtonMask>(core::Button::Duck))))
        return reject(JumpGuardReason::CommandShape);
    if(ticket.state==JumpState::Failed || ticket.state==JumpState::Aborted)
        return !press && speed==0 ? accept():reject(JumpGuardReason::TerminalState);
    if(!current_->walk_ || current_->walk_->step()!=pending.binding.step || !s.velocity || !s.velocity->isFinite() || !s.ducked)
        return reject(JumpGuardReason::Binding);
    auto* entity=owner.entityFor(s.actor);
    auto assessment=assessStandardJumpPhysics(engine_,entity,pending.binding,s.tick);
    current_->motionTrace_.jumpDispatchPhysics=assessment;
    const auto sameHull=[](const std::optional<nav::runtime::HullDimensions>& a,
                           const std::optional<nav::runtime::HullDimensions>& b) {
        return a.has_value()==b.has_value() && (!a || (a->minimum==b->minimum && a->maximum==b->maximum));
    };
    const auto rejectPhysics=[&](JumpPhysicsReason why) noexcept {
        current_->motionTrace_.jumpDispatchPhysics.reason=why;
        return reject(JumpGuardReason::Physics);
    };
    if(!assessment || !assessment.physics) return reject(JumpGuardReason::Physics);
    const auto& physics=*assessment.physics;
    const auto ticketReason=validateJumpPhysicsTicket(
        ticket.physics,physics,pending.binding,pending.tick);
    if(ticketReason!=JumpPhysicsReason::None) return rejectPhysics(ticketReason);
    if(!sameHull(assessment.actorHull,s.hull))
        return rejectPhysics(JumpPhysicsReason::PostureIncompatible);
    const auto hints=constraints(nav::model::NavTraversalKind::Jump,ticket.plan.sourceAttributes,ticket.plan.targetAttributes);
    const auto capability=deriveJumpLimits(jumpLimits.motion,physics,s,hints);
    if(!capability) return reject(JumpGuardReason::Capability);
    const auto motion=*capability;
    if(!sameHull(ticket.plan.flightHull,motion.flightHull)) return reject(JumpGuardReason::FlightHull);
    const bool duck=(command.buttons&static_cast<core::ButtonMask>(core::Button::Duck))!=0;
    if(ticket.state==JumpState::Complete) {
        if(press || speed!=0 || s.grounded!=true) return reject(JumpGuardReason::CompletionState);
        if(duck || s.ducked==false) return accept();
        if(!physics.standingHull || !s.hull || !s.position || current_->guardQueries_>=21)
            return reject(current_->guardQueries_>=21 ? JumpGuardReason::QueryBudget:JumpGuardReason::StandClearance);
        auto origin=*s.position;
        origin.z+=s.hull->minimum.z-physics.standingHull->minimum.z;
        const QueryRequest q{{s.agent,s.actor,s.map,s.tick,pending.binding.routeGeneration,1},
            QueryKind::Clearance,origin,origin,physics.standingHull};
        GuardQueries queries(*this,current_->guardQueries_,current_->motionTrace_.jumpGuardQueries);
        WorldQueryResult r; try { r=queries.query(q); } catch(...) { return reject(JumpGuardReason::StandClearance); }
        return r.stamp==q.stamp && r.kind==q.kind && r.error==QueryError::None && r.clearance && r.clearance->clear
            ? accept():reject(r.error==QueryError::BudgetExceeded ? JumpGuardReason::QueryBudget:JumpGuardReason::StandClearance);
    }
    const bool airbornePhase=s.grounded==false || ticket.state==JumpState::Airborne || ticket.state==JumpState::Recover;
    const bool expectedDuck=airbornePhase ? ticket.plan.flightHull.has_value():hints.sourceDuck;
    const bool sourcePostureMismatch=press && *s.ducked!=hints.sourceDuck &&
        assessment.posture!=JumpActorPosture::Transition;
    if(duck!=expectedDuck || sourcePostureMismatch)
        return reject(JumpGuardReason::Posture);
    const auto delta=movement_->frameDeltaUs();
    if(!delta || delta>120000) return reject(JumpGuardReason::Timing,MotionReason::StaleCommand);
    const double dt=double(delta/1000+(delta%1000>=500 ? 1U:0U))/1000;
    if(dt<=0) return reject(JumpGuardReason::Timing,MotionReason::StaleCommand);
    GuardQueries queries(*this,current_->guardQueries_,current_->motionTrace_.jumpGuardQueries);
    GroundProbeLimits ground{21-current_->guardQueries_,4,48,16,18,18,64,4,18,0.7};
    if(press) {
        if(ticket.state!=JumpState::Takeoff || ticket.pressTick!=pending.tick || s.grounded!=true ||
           !entity || !std::isfinite(entity->v.fuser2) || entity->v.fuser2>0 ||
           (entity->v.oldbuttons&static_cast<int>(core::Button::Jump)) || current_->guardQueries_>=20)
            return reject(current_->guardQueries_>=20 ? JumpGuardReason::QueryBudget:JumpGuardReason::TakeoffState);
        auto flight=jumpLimits.flight; flight.maxQueries=20-current_->guardQueries_; // Reserve the actual frame sweep.
        const auto launch=JumpProbe::launch(s,pending.binding,ticket.plan,motion,physics,flight,*index_,navigation_.map,queries);
        if(!launch) return reject(JumpGuardReason::LaunchProbe);
    } else if(s.grounded==true) {
        if(ticket.state==JumpState::Takeoff) return speed==0 ? accept():reject(JumpGuardReason::TakeoffState);
        if(ticket.state==JumpState::Airborne || ticket.state==JumpState::Recover) {
            if(speed!=0) return reject(JumpGuardReason::LandingProbe);
            return JumpProbe::land(s,pending.binding,ticket.plan,motion,ground,*index_,navigation_.map,queries)
                ? accept():reject(JumpGuardReason::LandingProbe);
        }
        const double yaw=command.view.yaw*3.14159265358979323846/180;
        const double vx=command.movement.forward*std::cos(yaw)+command.movement.side*std::sin(yaw);
        const double vy=command.movement.forward*std::sin(yaw)-command.movement.side*std::cos(yaw);
        const float x=static_cast<float>(s.position->x+vx*dt),y=static_cast<float>(s.position->y+vy*dt);
        if(speed>0) {
            if(!pending.segment) return reject(JumpGuardReason::Segment,MotionReason::Deviation);
            const auto a=pending.segment->start,b=pending.segment->end;
            const double dx=double(b.x)-a.x,dy=double(b.y)-a.y,length=std::hypot(dx,dy);
            if(length<=0) return reject(JumpGuardReason::Segment,MotionReason::Deviation);
            for(const auto position : {*s.position,nav::model::NavVector3{x,y,s.position->z}}) {
                const double px=double(position.x)-a.x,py=double(position.y)-a.y;
                const double along=(px*dx+py*dy)/length,lateral=std::abs(px*dy-py*dx)/length;
                if(along< -0.01 || along>length+0.001 || lateral>0.5)
                    return reject(JumpGuardReason::Segment,MotionReason::Deviation);
            }
        }
        const auto path=GroundProbe::inspect(s,pending.binding.routeGeneration,ticket.plan.source,x,y,*index_,navigation_.map,queries,ground);
        return path && path.target->area==ticket.plan.source ? accept():
            reject(path.reason==ProbeReason::BudgetExceeded ? JumpGuardReason::QueryBudget:JumpGuardReason::GroundProbe);
    } else if(ticket.state!=JumpState::Takeoff && ticket.state!=JumpState::Airborne)
        return reject(JumpGuardReason::AirState);
    // Standard air acceleration cannot accelerate into a wish direction already
    // moving above its 30-unit cap. Every submitted flight frame is swept using
    // the actual velocity and transport's rounded msec, including landing contact.
    const double yaw=command.view.yaw*3.14159265358979323846/180;
    const double vx=command.movement.forward*std::cos(yaw)+command.movement.side*std::sin(yaw);
    const double vy=command.movement.forward*std::sin(yaw)-command.movement.side*std::cos(yaw);
    if(speed>0 && (vx*s.velocity->x+vy*s.velocity->y)/speed<30)
        return reject(JumpGuardReason::AirAcceleration);
    const double initialZ=press ? physics.verticalImpulse:s.velocity->z;
    const nav::model::NavVector3 end{static_cast<float>(s.position->x+double(s.velocity->x)*dt),
        static_cast<float>(s.position->y+double(s.velocity->y)*dt),
        static_cast<float>(s.position->z+initialZ*dt-0.5*physics.gravity*dt*dt)};
    if(!end.isFinite()) return reject(JumpGuardReason::FlightPrediction);
    const QueryRequest q{{s.agent,s.actor,s.map,s.tick,pending.binding.routeGeneration,1},QueryKind::SweptHull,*s.position,end,(duck && s.grounded==false) ? ticket.plan.flightHull:s.hull};
    WorldQueryResult r; try { r=queries.query(q); } catch(...) { return reject(JumpGuardReason::FlightSweep); }
    if(!(r.stamp==q.stamp) || r.kind!=q.kind || r.error!=QueryError::None || !r.hull || r.hull->startSolid ||
       !std::isfinite(r.hull->fraction) || r.hull->fraction<0 || r.hull->fraction>1 || !r.hull->end.isFinite() || !r.hull->normal.isFinite())
        return reject(r.error==QueryError::BudgetExceeded ? JumpGuardReason::QueryBudget:JumpGuardReason::FlightSweep);
    const auto& hit=*r.hull;
    const auto close=[](double a,double b) { return std::abs(a-b)<=0.01; };
    if(!close(hit.end.x,s.position->x+(double(end.x)-s.position->x)*hit.fraction) ||
       !close(hit.end.y,s.position->y+(double(end.y)-s.position->y)*hit.fraction) ||
       !close(hit.end.z,s.position->z+(double(end.z)-s.position->z)*hit.fraction))
        return reject(JumpGuardReason::FlightSweep);
    if(hit.fraction==1) return accept();
    if(press || initialZ>=0 || hit.normal.z<0.7f || current_->guardQueries_>=21)
        return reject(current_->guardQueries_>=21 ? JumpGuardReason::QueryBudget:JumpGuardReason::LandingContact);
    auto contact=s; contact.position=hit.end; contact.grounded=true;
    if(duck && ticket.plan.flightHull) { contact.hull=ticket.plan.flightHull; contact.ducked=true; }
    ground.maxQueries=21-current_->guardQueries_;
    return JumpProbe::land(contact,pending.binding,ticket.plan,motion,ground,*index_,navigation_.map,queries)
        ? accept():reject(JumpGuardReason::LandingProbe);
}
}
