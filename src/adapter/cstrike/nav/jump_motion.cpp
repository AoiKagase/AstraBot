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
namespace {
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
    const auto physics=standardJumpPhysics(engine_,entity,pending.binding,s.tick);
    const auto sameHull=[](const std::optional<nav::runtime::HullDimensions>& a,
                           const std::optional<nav::runtime::HullDimensions>& b) {
        return a.has_value()==b.has_value() && (!a || (a->minimum==b->minimum && a->maximum==b->maximum));
    };
    if(!physics || ticket.physics.tick!=pending.tick || physics->gravity!=ticket.physics.gravity ||
       physics->verticalImpulse!=ticket.physics.verticalImpulse ||
       physics->crouchSpeedMultiplier!=ticket.physics.crouchSpeedMultiplier ||
       !sameHull(physics->standingHull,ticket.physics.standingHull) ||
       !sameHull(physics->crouchingHull,ticket.physics.crouchingHull))
        return reject(JumpGuardReason::Physics);
    const auto hints=constraints(nav::model::NavTraversalKind::Jump,ticket.plan.sourceAttributes,ticket.plan.targetAttributes);
    const auto capability=deriveJumpLimits(jumpLimits.motion,*physics,s,hints);
    if(!capability) return reject(JumpGuardReason::Capability);
    const auto motion=*capability;
    if(!sameHull(ticket.plan.flightHull,motion.flightHull)) return reject(JumpGuardReason::FlightHull);
    const bool duck=(command.buttons&static_cast<core::ButtonMask>(core::Button::Duck))!=0;
    if(ticket.state==JumpState::Complete) {
        if(press || speed!=0 || s.grounded!=true) return reject(JumpGuardReason::CompletionState);
        if(duck || s.ducked==false) return accept();
        if(!physics->standingHull || !s.hull || !s.position || current_->guardQueries_>=21)
            return reject(current_->guardQueries_>=21 ? JumpGuardReason::QueryBudget:JumpGuardReason::StandClearance);
        auto origin=*s.position;
        origin.z+=s.hull->minimum.z-physics->standingHull->minimum.z;
        const QueryRequest q{{s.agent,s.actor,s.map,s.tick,pending.binding.routeGeneration,1},
            QueryKind::Clearance,origin,origin,physics->standingHull};
        GuardQueries queries(*this,current_->guardQueries_,current_->motionTrace_.jumpGuardQueries);
        WorldQueryResult r; try { r=queries.query(q); } catch(...) { return reject(JumpGuardReason::StandClearance); }
        return r.stamp==q.stamp && r.kind==q.kind && r.error==QueryError::None && r.clearance && r.clearance->clear
            ? accept():reject(r.error==QueryError::BudgetExceeded ? JumpGuardReason::QueryBudget:JumpGuardReason::StandClearance);
    }
    const bool airbornePhase=s.grounded==false || ticket.state==JumpState::Airborne || ticket.state==JumpState::Recover;
    const bool expectedDuck=airbornePhase ? ticket.plan.flightHull.has_value():hints.sourceDuck;
    if(duck!=expectedDuck || (press && *s.ducked!=hints.sourceDuck)) return reject(JumpGuardReason::Posture);
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
        const auto launch=JumpProbe::launch(s,pending.binding,ticket.plan,motion,*physics,flight,*index_,navigation_.map,queries);
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
    const double initialZ=press ? physics->verticalImpulse:s.velocity->z;
    const nav::model::NavVector3 end{static_cast<float>(s.position->x+double(s.velocity->x)*dt),
        static_cast<float>(s.position->y+double(s.velocity->y)*dt),
        static_cast<float>(s.position->z+initialZ*dt-0.5*physics->gravity*dt*dt)};
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
