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
    return nav::local::JumpPhysics{binding,tick,effective,std::sqrt(1600*jumpHeight)};
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
    const bool press=(command.buttons&static_cast<core::ButtonMask>(core::Button::Jump))!=0;
    const double speed=std::hypot(command.movement.forward,command.movement.side);
    if(command.impulse || command.movement.up!=0 ||
       (command.buttons&~static_cast<core::ButtonMask>(core::Button::Jump))) return MotionReason::JumpChanged;
    if(ticket.state==JumpState::Complete || ticket.state==JumpState::Failed || ticket.state==JumpState::Aborted)
        return !press && speed==0 ? MotionReason::None:MotionReason::JumpChanged;
    if(!current_->walk_ || current_->walk_->step()!=pending.binding.step || !s.velocity || !s.velocity->isFinite() || s.ducked!=false)
        return MotionReason::JumpChanged;
    auto* entity=owner.entityFor(s.actor);
    const auto physics=standardJumpPhysics(engine_,entity,pending.binding,s.tick);
    if(!physics || physics->gravity!=ticket.physics.gravity || physics->verticalImpulse!=ticket.physics.verticalImpulse)
        return MotionReason::JumpChanged;
    const auto delta=movement_->frameDeltaUs();
    if(!delta || delta>120000) return MotionReason::StaleCommand;
    const double dt=double(delta/1000+(delta%1000>=500 ? 1U:0U))/1000;
    if(dt<=0) return MotionReason::StaleCommand;
    GuardQueries queries(*this,current_->guardQueries_,current_->motionTrace_.jumpGuardQueries);
    GroundProbeLimits ground{21-current_->guardQueries_,4,48,16,18,18,64,4,18,0.7};
    if(press) {
        if(ticket.state!=JumpState::Takeoff || ticket.pressTick!=pending.tick || s.grounded!=true ||
           !entity || !std::isfinite(entity->v.fuser2) || entity->v.fuser2>0 ||
           (entity->v.oldbuttons&static_cast<int>(core::Button::Jump)) || current_->guardQueries_>=20)
            return MotionReason::JumpChanged;
        auto flight=jumpLimits.flight; flight.maxQueries=20-current_->guardQueries_; // Reserve the actual frame sweep.
        const auto launch=JumpProbe::launch(s,pending.binding,ticket.plan,jumpLimits.motion,*physics,flight,*index_,navigation_.map,queries);
        if(!launch) return MotionReason::JumpChanged;
    } else if(s.grounded==true) {
        if(ticket.state==JumpState::Takeoff) return speed==0 ? MotionReason::None:MotionReason::JumpChanged;
        if(ticket.state==JumpState::Airborne || ticket.state==JumpState::Recover) {
            if(speed!=0) return MotionReason::JumpChanged;
            return JumpProbe::land(s,pending.binding,ticket.plan,jumpLimits.motion,ground,*index_,navigation_.map,queries)
                ? MotionReason::None:MotionReason::JumpChanged;
        }
        const double yaw=command.view.yaw*3.14159265358979323846/180;
        const double vx=command.movement.forward*std::cos(yaw)+command.movement.side*std::sin(yaw);
        const double vy=command.movement.forward*std::sin(yaw)-command.movement.side*std::cos(yaw);
        const float x=static_cast<float>(s.position->x+vx*dt),y=static_cast<float>(s.position->y+vy*dt);
        if(speed>0) {
            if(!pending.segment) return MotionReason::Deviation;
            const auto a=pending.segment->start,b=pending.segment->end;
            const double dx=double(b.x)-a.x,dy=double(b.y)-a.y,length=std::hypot(dx,dy);
            if(length<=0) return MotionReason::Deviation;
            for(const auto position : {*s.position,nav::model::NavVector3{x,y,s.position->z}}) {
                const double px=double(position.x)-a.x,py=double(position.y)-a.y;
                const double along=(px*dx+py*dy)/length,lateral=std::abs(px*dy-py*dx)/length;
                if(along< -0.01 || along>length+0.001 || lateral>0.5) return MotionReason::Deviation;
            }
        }
        const auto path=GroundProbe::inspect(s,pending.binding.routeGeneration,ticket.plan.source,x,y,*index_,navigation_.map,queries,ground);
        return path && path.target->area==ticket.plan.source ? MotionReason::None:MotionReason::JumpChanged;
    } else if(ticket.state!=JumpState::Takeoff && ticket.state!=JumpState::Airborne) return MotionReason::JumpChanged;
    // Standard air acceleration cannot accelerate into a wish direction already
    // moving above its 30-unit cap. Every submitted flight frame is swept using
    // the actual velocity and transport's rounded msec, including landing contact.
    const double yaw=command.view.yaw*3.14159265358979323846/180;
    const double vx=command.movement.forward*std::cos(yaw)+command.movement.side*std::sin(yaw);
    const double vy=command.movement.forward*std::sin(yaw)-command.movement.side*std::cos(yaw);
    if(speed>0 && (vx*s.velocity->x+vy*s.velocity->y)/speed<30) return MotionReason::JumpChanged;
    const double initialZ=press ? physics->verticalImpulse:s.velocity->z;
    const nav::model::NavVector3 end{static_cast<float>(s.position->x+double(s.velocity->x)*dt),
        static_cast<float>(s.position->y+double(s.velocity->y)*dt),
        static_cast<float>(s.position->z+initialZ*dt-0.5*physics->gravity*dt*dt)};
    if(!end.isFinite()) return MotionReason::JumpChanged;
    const QueryRequest q{{s.agent,s.actor,s.map,s.tick,pending.binding.routeGeneration,1},QueryKind::SweptHull,*s.position,end,s.hull};
    WorldQueryResult r; try { r=queries.query(q); } catch(...) { return MotionReason::JumpChanged; }
    if(!(r.stamp==q.stamp) || r.kind!=q.kind || r.error!=QueryError::None || !r.hull || r.hull->startSolid ||
       !std::isfinite(r.hull->fraction) || r.hull->fraction<0 || r.hull->fraction>1 || !r.hull->end.isFinite() || !r.hull->normal.isFinite())
        return MotionReason::JumpChanged;
    const auto& hit=*r.hull;
    const auto close=[](double a,double b) { return std::abs(a-b)<=0.01; };
    if(!close(hit.end.x,s.position->x+(double(end.x)-s.position->x)*hit.fraction) ||
       !close(hit.end.y,s.position->y+(double(end.y)-s.position->y)*hit.fraction) ||
       !close(hit.end.z,s.position->z+(double(end.z)-s.position->z)*hit.fraction)) return MotionReason::JumpChanged;
    if(hit.fraction==1) return MotionReason::None;
    if(press || initialZ>=0 || hit.normal.z<0.7f || current_->guardQueries_>=21) return MotionReason::JumpChanged;
    auto contact=s; contact.position=hit.end; contact.grounded=true;
    ground.maxQueries=21-current_->guardQueries_;
    return JumpProbe::land(contact,pending.binding,ticket.plan,jumpLimits.motion,ground,*index_,navigation_.map,queries)
        ? MotionReason::None:MotionReason::JumpChanged;
}
}
