// SPDX-License-Identifier: MPL-2.0
#include "adapter/cstrike/nav/ladder_frame.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
namespace astrabot::adapter::cstrike {
namespace {
using V=nav::model::NavVector3;
V value(const Vector& v) noexcept { return {v.x,v.y,v.z}; }
bool same(V a,V b) noexcept {
    return std::abs(double(a.x)-b.x)<=0.05 && std::abs(double(a.y)-b.y)<=0.05 && std::abs(double(a.z)-b.z)<=0.05;
}
}
LadderFrameResult inspectLadderFrame(LadderFrameWorld w,edict_t* actor,nav::local::Binding binding,
    const nav::runtime::MovementSnapshot& s,const BoundLadderPlan& bound,V target,
    const nav::query::NavSpatialIndex& index,core::MapGeneration indexMap,int maximum,std::uint32_t budget,
    std::optional<core::BotCommand> command,std::optional<std::uint8_t> exitMsec,bool groundPath) noexcept {
    LadderFrameResult result;
    const bool suppliedCommand=command.has_value();
    const auto& p=bound.passage; const auto& link=bound.plan.link; auto* e=w.ladder.engine;
    const auto fail=[&](LadderFrameReason reason) { result.reason=reason; return result; };
    if(!actor || !binding.agent.isValid() || !binding.actor.isValid() || !binding.map.isValid() ||
       !binding.routeGeneration || !s.tick.isValid() || s.agent!=binding.agent || s.actor!=binding.actor || s.map!=binding.map ||
       s.kind!=nav::runtime::ActorKind::ManagedBot || s.connected!=true || s.joined!=true || s.alive!=true ||
       !s.position || !s.position->isFinite() || !s.velocity || !s.velocity->isFinite() || !s.view || !s.view->isFinite() ||
       !s.hull || s.hull->minimum!=V{-16,-16,-36} || s.hull->maximum!=V{16,16,36} || !s.grounded || s.ducked!=false ||
       !s.speedLimit || !std::isfinite(*s.speedLimit) || *s.speedLimit<=0 || !target.isFinite() ||
       (groundPath && (command || exitMsec || s.grounded!=true)) ||
       (command && exitMsec && command->msec!=*exitMsec) || budget>((exitMsec || groundPath) ? 21U:command ? 7U:4U) ||
       std::hypot(double(target.x)-s.position->x,double(target.y)-s.position->y)>96 ||
       std::abs(double(target.z)-s.position->z)>4100 ||
       maximum<1 || maximum>8192 || binding.actor.slot>=maximum || p.map!=binding.map || indexMap!=binding.map ||
       link.sourceId!=ladderSourceId || !link.generation || !link.linkId || link.traversal!=nav::model::NavTraversalKind::Ladder)
        return result;
    if(!e || !w.current || !w.ladder.currentMap || !e->pfnTraceModel || !e->pfnTraceHull ||
       !e->pfnPEntityOfEntIndex || !e->pfnIndexOfEdict || !e->pfnSzFromIndex || !e->pfnCVarGetPointer)
        return fail(LadderFrameReason::Unavailable);
    const int serial=actor->serialnumber,mode=actor->v.movetype;
    if(mode!=MOVETYPE_WALK && mode!=MOVETYPE_FLY) return result;
    const auto physics=[&]() -> std::optional<nav::local::LadderAirPhysics> {
        const char* names[]{"sv_gravity","sv_airaccelerate","sv_maxspeed","sv_maxvelocity"};
        double values[4]{};
        const cvar_t* cvars[4]{};
        for(unsigned i=0;i<4;++i) {
            const auto* cvar=e->pfnCVarGetPointer(names[i]);
            if(!cvar || !std::isfinite(cvar->value) || cvar->value<=0) return {};
            values[i]=cvar->value;
            cvars[i]=cvar;
        }
        for(unsigned i=0;i<4;++i) if(cvars[i]->value!=values[i]) return {};
        const double gravity=values[0]*(actor->v.gravity==0 ? 1:actor->v.gravity);
        const double speed=(std::min)(values[2],double(actor->v.maxspeed));
        if(!std::isfinite(gravity) || gravity<=0 || gravity>4000 || values[1]>1000 || speed<=0 || speed>2000 ||
           !std::isfinite(actor->v.friction) || actor->v.friction<=0 || actor->v.friction>10 || values[3]>10000) return {};
        return nav::local::LadderAirPhysics{gravity,values[1],actor->v.friction,speed,values[3]};
    };
    const float actorGravity=actor->v.gravity,actorFriction=actor->v.friction;
    const auto initialPhysics=physics();
    if(!initialPhysics) return fail(LadderFrameReason::Unavailable);
    const auto fresh=[&]() {
        const auto current=physics();
        if(!current || current->gravity!=initialPhysics->gravity || current->airAcceleration!=initialPhysics->airAcceleration ||
           current->friction!=initialPhysics->friction || current->maximumSpeed!=initialPhysics->maximumSpeed ||
           current->maximumVelocity!=initialPhysics->maximumVelocity) {
            result.reason=LadderFrameReason::StaleActor; return false;
        }
        if(!ladderPassageCurrent(w.ladder,p,maximum)) { result.reason=LadderFrameReason::StaleWorld; return false; }
        if(!w.current(w.context,binding,s.tick) || actor->free || actor->serialnumber!=serial ||
           e->pfnPEntityOfEntIndex(binding.actor.slot)!=actor || e->pfnIndexOfEdict(actor)!=binding.actor.slot ||
           actor->v.deadflag!=DEAD_NO || !(actor->v.flags&FL_FAKECLIENT) ||
           (actor->v.flags&(FL_FROZEN|FL_ONTRAIN)) || actor->v.iuser1!=0 || value(actor->v.punchangle)!=V{} ||
           actor->v.gravity!=actorGravity || actor->v.friction!=actorFriction ||
           bool(actor->v.flags&FL_ONGROUND)!=*s.grounded || (actor->v.flags&FL_DUCKING) ||
           actor->v.movetype!=mode || actor->v.waterlevel!=0 || (actor->v.flags&FL_WATERJUMP) ||
           value(actor->v.basevelocity)!=V{} || value(actor->v.origin)!=*s.position || value(actor->v.velocity)!=*s.velocity ||
           value(actor->v.v_angle)!=*s.view || value(actor->v.mins)!=s.hull->minimum ||
           value(actor->v.maxs)!=s.hull->maximum || actor->v.maxspeed!=*s.speedLimit ||
           !w.current(w.context,binding,s.tick) || w.ladder.currentMap(w.ladder.context)!=binding.map) {
            result.reason=LadderFrameReason::StaleActor; return false;
        }
        return true;
    };
    if(!fresh()) return result;
    auto* ladder=e->pfnPEntityOfEntIndex(static_cast<int>(static_cast<std::uint32_t>(p.entityId)));
    const auto trace=[&](V a,V b,int modelHull,TraceResult& t) {
        if(!fresh()) return false;
        if(result.queries>=budget) { result.reason=LadderFrameReason::BudgetExceeded; return false; }
        ++result.queries;
        const float from[]{a.x,a.y,a.z},to[]{b.x,b.y,b.z};
        t.flFraction=(std::numeric_limits<float>::quiet_NaN)();
        if(modelHull>=0) e->pfnTraceModel(from,to,modelHull,ladder,&t);
        else e->pfnTraceHull(from,to,0,1,actor,&t);
        if(!fresh()) return false;
        const double f=t.flFraction;
        if(!std::isfinite(f) || f<0 || f>1 || !value(t.vecEndPos).isFinite() || !value(t.vecPlaneNormal).isFinite() ||
           !same(value(t.vecEndPos),{static_cast<float>(a.x+(double(b.x)-a.x)*f),
                static_cast<float>(a.y+(double(b.y)-a.y)*f),static_cast<float>(a.z+(double(b.z)-a.z)*f)})) {
            result.reason=LadderFrameReason::InvalidTrace; return false;
        }
        return true;
    };
    LadderFrameObservation observation; observation.physics=*initialPhysics; auto& q=observation.inspection;
    q.stamp={s.agent,s.actor,s.map,s.tick,binding.routeGeneration,0}; q.step=binding.step;
    q.sourceId=link.sourceId; q.generation=link.generation; q.linkId=link.linkId;
    q.origin=*s.position; q.velocity=*s.velocity; q.hull=*s.hull; q.target=target;
    TraceResult contact{};
    if(!trace(*s.position,*s.position,1,contact)) return result;
    const bool touching=contact.fStartSolid || contact.fAllSolid;
    if((touching && contact.pHit!=ladder) || (!touching && contact.flFraction!=1)) return fail(LadderFrameReason::InvalidTrace);
    observation.contact={link.sourceId,link.generation,link.linkId,touching};
    observation.climbing=mode==MOVETYPE_FLY;
    if(touching) {
        const V center{static_cast<float>((double(p.candidate.minimum.x)+p.candidate.maximum.x)/2),
            static_cast<float>((double(p.candidate.minimum.y)+p.candidate.maximum.y)/2),
            static_cast<float>((double(p.candidate.minimum.z)+p.candidate.maximum.z)/2)};
        TraceResult face{};
        if(!trace(*s.position,center,0,face)) return result;
        if(face.fStartSolid || face.fAllSolid || face.flFraction==1 || face.pHit!=ladder ||
           std::abs(double(face.vecPlaneNormal.x)-p.normal.x)>0.001 ||
           std::abs(double(face.vecPlaneNormal.y)-p.normal.y)>0.001 ||
           std::abs(double(face.vecPlaneNormal.z)-p.normal.z)>0.001) return fail(LadderFrameReason::WrongFace);
    }
    const bool jumpExit=exitMsec && link.direction==nav::enrichment::NavLinkDirection::Down && !*s.grounded;
    const bool lowerExit=exitMsec && link.direction==nav::enrichment::NavLinkDirection::Down && !jumpExit;
    std::optional<core::MovementIntent> lowerIntent;
    if(lowerExit) {
        const auto& end=bound.plan.end; const auto& normal=bound.plan.normal;
        const double dx=double(end.x)-s.position->x,dy=double(end.y)-s.position->y;
        // Standard CS only kicks outward when the foot point is solid. A
        // downward button supplies this motion; analog walking cannot detach.
        if(!touching || !*s.grounded || !*exitMsec || *exitMsec>120 ||
           std::abs(double(s.position->z)-end.z)>0.05 || target!=end ||
           dx*normal.x+dy*normal.y<=0 || std::abs(dx*normal.y-dy*normal.x)>1)
            return fail(LadderFrameReason::NoExit);
        core::MovementIntent intent;
        intent.view=core::IntentVector{0,std::atan2(-normal.y,-normal.x)*180/3.14159265358979323846,0};
        intent.direction={-normal.x,-normal.y,0}; intent.speed=(std::min)(initialPhysics->maximumSpeed,200.0);
        intent.back=core::ActionRequest::Hold;
        const auto motor=core::Motor::command(intent,{},static_cast<float>(initialPhysics->maximumSpeed),
            std::uint64_t(*exitMsec)*1000,true);
        if(!motor) return fail(LadderFrameReason::NoExit);
        lowerIntent=intent; if(!command) command=motor.command;
    }
    if(exitMsec && !lowerExit) {
        const auto exit=jumpExit ? nav::local::planJumpLadderExit(bound.plan,s,touching,*initialPhysics,*exitMsec,index,indexMap,command):
            nav::local::planUpperLadderExit(bound.plan,s,touching,double(p.candidate.maximum.z)+36,
                *initialPhysics,*exitMsec,index,indexMap,command);
        if(!exit) { result.exitReason=exit.reason; return fail(LadderFrameReason::NoExit); }
        if(jumpExit) observation.jumpExit=exit.value; else observation.upperExit=exit.value;
        command=exit.value->command;
    } else {
        V from=*s.position,to=target;
        if(*s.grounded) { from.z+=0.05f; to.z+=0.05f; }
        TraceResult path{};
        if(!trace(from,to,-1,path)) return result;
        if(path.fStartSolid || path.fAllSolid || path.flFraction!=1) return fail(LadderFrameReason::Blocked);
        q.pathClear=true;
    }
    if(*s.grounded) {
        V high=*s.position,low=high; high.z+=0.05f; low.z-=4;
        TraceResult floor{};
        if(!trace(high,low,-1,floor)) return result;
        const auto n=value(floor.vecPlaneNormal);
        auto* world=e->pfnPEntityOfEntIndex(0);
        if(!fresh()) return result;
        if(!world || world->free || floor.pHit!=world || floor.fStartSolid || floor.fAllSolid || floor.flFraction==1 ||
           n.z<0.7f || std::abs(double(n.x)*n.x+double(n.y)*n.y+double(n.z)*n.z-1)>0.02)
            return fail(LadderFrameReason::NoSupport);
        const float height=floor.vecEndPos.z-36;
        q.worldFloor=nav::runtime::FloorObservation{height,n,true};
        const auto area=index.containing({s.position->x,s.position->y,height},2);
        if(!area) return fail(LadderFrameReason::NoSupport);
        if(!area.value->has_value()) {
            // A ladder shaft may be outside NAV while the hull rests on world
            // geometry. Preserve absent NAV support; never assign the target ID.
            if(!touching && !groundPath) return fail(LadderFrameReason::NoSupport);
        } else q.support=nav::local::GroundedTarget{*s.position,(**area.value).areaId,{height,n,true}};
    }
    if(groundPath) {
        if(touching || !q.worldFloor || std::abs(q.worldFloor->normal.x)>0.001f ||
           std::abs(q.worldFloor->normal.y)>0.001f || std::abs(q.worldFloor->normal.z-1)>0.001f ||
           std::abs(double(target.z)-s.position->z)>0.051) return fail(LadderFrameReason::NoSupport);
        const double range=std::hypot(double(target.x)-s.position->x,double(target.y)-s.position->y);
        const auto samples=(std::max)(1U,static_cast<unsigned>(std::ceil(range/16)));
        if(samples>6) return fail(LadderFrameReason::InvalidInput);
        for(unsigned i=1;i<=samples;++i) {
            const double fraction=double(i)/samples;
            V high{static_cast<float>(s.position->x+(double(target.x)-s.position->x)*fraction),
                static_cast<float>(s.position->y+(double(target.y)-s.position->y)*fraction),s.position->z+0.05f};
            auto low=high; low.z-=4.05f; TraceResult floor{};
            if(!trace(high,low,-1,floor)) return result;
            auto* world=e->pfnPEntityOfEntIndex(0); if(!fresh()) return result;
            const auto n=value(floor.vecPlaneNormal);
            // The last short approach to the upper mount crosses the lip.
            // Require a clear shallow descent into this exact model; absence
            // of floor alone never grants permission to walk into a void.
            if(i==samples && target==bound.plan.mount && link.direction==nav::enrichment::NavLinkDirection::Down &&
               floor.flFraction==1 && !floor.fStartSolid && !floor.fAllSolid) {
                TraceResult capture{};
                if(!trace(low,low,1,capture)) return result;
                if(capture.pHit!=ladder || (!capture.fStartSolid && !capture.fAllSolid)) return fail(LadderFrameReason::NoSupport);
                continue;
            }
            if(!world || world->free || floor.pHit!=world || floor.fStartSolid || floor.fAllSolid || floor.flFraction==1 ||
               std::abs(n.x)>0.001f || std::abs(n.y)>0.001f || std::abs(n.z-1)>0.001f ||
               std::abs(double(floor.vecEndPos.z)-s.position->z)>0.05) return fail(LadderFrameReason::NoSupport);
            const auto area=index.containing({high.x,high.y,floor.vecEndPos.z-36},2);
            if(!area || (area.value->has_value() && (**area.value).areaId!=link.from && (**area.value).areaId!=link.to) ||
               (i==samples && (target==bound.plan.end || target==bound.plan.start) &&
                (!area.value->has_value() || (**area.value).areaId!=(target==bound.plan.end ? link.to:link.from))))
                return fail(LadderFrameReason::NoSupport);
        }
        q.groundPathClear=true;
    }
    if(command) {
        V displacement{},velocity{};
        const double dt=double(command->msec)/1000;
        if(touching && (command->buttons&static_cast<core::ButtonMask>(core::Button::Jump))) {
            if(*s.grounded) return fail(LadderFrameReason::InvalidInput);
            const auto predicted=nav::local::ladderJumpAirStep(*command,p.normal,*initialPhysics);
            if(!predicted) return fail(LadderFrameReason::InvalidInput);
            displacement=predicted->displacement; velocity=predicted->velocity;
        } else if(touching) {
            if(!e->pfnPointContents) return fail(LadderFrameReason::Unavailable);
            if(!fresh()) return result;
            if(result.queries>=budget) return fail(LadderFrameReason::BudgetExceeded);
            const float point[]{s.position->x,s.position->y,s.position->z-37};
            ++result.queries; const int contents=e->pfnPointContents(point);
            if(!fresh()) return result;
            if(contents!=CONTENTS_SOLID && contents!=CONTENTS_EMPTY) return fail(LadderFrameReason::InvalidTrace);
            observation.floorPointSolid=contents==CONTENTS_SOLID;
            if(lowerExit && !*observation.floorPointSolid) return fail(LadderFrameReason::NoExit);
            const auto predicted=nav::local::ladderVelocity(*command,p.normal,initialPhysics->maximumSpeed,*observation.floorPointSolid);
            if(!predicted) return fail(LadderFrameReason::InvalidInput);
            velocity=*predicted;
            displacement={static_cast<float>(velocity.x*dt),static_cast<float>(velocity.y*dt),static_cast<float>(velocity.z*dt)};
        } else {
            if(*s.grounded) return fail(LadderFrameReason::InvalidInput); // Ground WALK has a separate controller/guard.
            const auto predicted=nav::local::ladderAirStep(*command,*s.velocity,*initialPhysics);
            if(!predicted) return fail(LadderFrameReason::InvalidInput);
            displacement=predicted->displacement; velocity=predicted->velocity;
        }
        LadderCommandPrediction predicted; predicted.command=*command; predicted.velocity=velocity;
        V start=*s.position;
        if(*s.grounded) start.z+=0.05f;
        const V end{start.x+displacement.x,start.y+displacement.y,start.z+displacement.z};
        TraceResult motion{};
        if(!trace(start,end,-1,motion)) return result;
        if(motion.fStartSolid || motion.fAllSolid) return fail(LadderFrameReason::Blocked);
        predicted.endpoint=value(motion.vecEndPos);
        if(motion.flFraction<1) {
            const auto n=value(motion.vecPlaneNormal); auto* world=e->pfnPEntityOfEntIndex(0);
            if(!fresh()) return result;
            if(!world || world->free || motion.pHit!=world || displacement.z>=0 ||
               std::abs(n.x)>0.001f || std::abs(n.y)>0.001f || std::abs(n.z-1)>0.001f)
                return fail(LadderFrameReason::Blocked);
            V raised=predicted.endpoint; raised.z+=0.05f;
            const double remaining=1-double(motion.flFraction);
            const V slide{static_cast<float>(raised.x+displacement.x*remaining),
                static_cast<float>(raised.y+displacement.y*remaining),raised.z};
            TraceResult rest{};
            if(!trace(raised,slide,-1,rest)) return result;
            if(rest.fStartSolid || rest.fAllSolid || rest.flFraction!=1) return fail(LadderFrameReason::Blocked);
            predicted.endpoint={rest.vecEndPos.x,rest.vecEndPos.y,motion.vecEndPos.z};
            predicted.velocity.z=0; predicted.floorCollision=true;
        }
        observation.prediction=predicted;
    }
    if(lowerIntent) {
        const auto& predicted=*observation.prediction;
        if(!predicted.floorCollision || std::abs(double(predicted.endpoint.z)-bound.plan.end.z)>0.05)
            return fail(LadderFrameReason::NoExit);
        // Verify actual world support at the predicted endpoint, including a
        // slide beyond the initial collision. Prediction never becomes support.
        auto high=predicted.endpoint,low=high; high.z+=0.05f; low.z-=4;
        TraceResult floor{};
        if(!trace(high,low,-1,floor)) return result;
        auto* world=e->pfnPEntityOfEntIndex(0);
        if(!fresh()) return result;
        const auto n=value(floor.vecPlaneNormal);
        if(!world || world->free || floor.pHit!=world || floor.fStartSolid || floor.fAllSolid || floor.flFraction==1 ||
           std::abs(n.x)>0.001f || std::abs(n.y)>0.001f || std::abs(n.z-1)>0.001f ||
           std::abs(double(floor.vecEndPos.z)-bound.plan.end.z)>0.05)
            return fail(LadderFrameReason::NoSupport);
        if(!suppliedCommand) q.exitIntent=*lowerIntent;
    }
    if(observation.jumpExit && touching) {
        // The forecast assumes air motion after the first command. Prove its
        // endpoint has left the selected model rather than forecasting through
        // another automatic ladder acquisition on the next PM update.
        const auto endpoint=observation.prediction->endpoint;
        TraceResult released{};
        if(!trace(endpoint,endpoint,1,released)) return result;
        if(released.fStartSolid || released.fAllSolid || released.flFraction!=1) return fail(LadderFrameReason::NoExit);
    }
    if(observation.upperExit || observation.jumpExit) {
        const auto& exit=observation.jumpExit ? *observation.jumpExit:*observation.upperExit;
        for(unsigned i=0;i<exit.columnCount;++i) {
            TraceResult column{};
            if(!trace(exit.columns[i].bottom,exit.columns[i].top,-1,column)) return result;
            if(column.fStartSolid || column.fAllSolid || column.flFraction!=1) return fail(LadderFrameReason::Blocked);
        }
        auto top=exit.landing,bottom=exit.landing; top.z+=0.05f; bottom.z-=4;
        TraceResult floor{};
        if(!trace(top,bottom,-1,floor)) return result;
        auto* world=e->pfnPEntityOfEntIndex(0);
        if(!fresh()) return result;
        const auto n=value(floor.vecPlaneNormal);
        if(!world || world->free || floor.pHit!=world || floor.fStartSolid || floor.fAllSolid || floor.flFraction==1 ||
           std::abs(n.x)>0.001f || std::abs(n.y)>0.001f || std::abs(n.z-1)>0.001f ||
           std::abs(double(floor.vecEndPos.z)-exit.landing.z)>0.05) return fail(LadderFrameReason::NoSupport);
        const auto area=index.containing({exit.landing.x,exit.landing.y,floor.vecEndPos.z-36},2);
        if(!area || !area.value->has_value() || (**area.value).areaId!=link.to) return fail(LadderFrameReason::NoSupport);
        q.pathClear=true;
        if(!suppliedCommand) q.exitIntent=exit.intent; // Generated input only, after full candidate verification.
    }
    if(!fresh()) return result;
    q.queries=result.queries; result.reason=LadderFrameReason::None; result.value=observation; return result;
}
}
