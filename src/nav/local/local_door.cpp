// SPDX-License-Identifier: MPL-2.0
#include "nav/local/local_door.hpp"

#include <algorithm>
#include <cmath>

namespace astrabot::nav::local {
namespace {
bool same(const Binding& a,const Binding& b) noexcept {
    return a.agent==b.agent && a.actor==b.actor && a.map==b.map &&
        a.routeGeneration==b.routeGeneration && a.step==b.step;
}
WalkDecision hold(Binding binding,core::TickId tick) noexcept {
    WalkDecision out;
    out.accepted=true; out.binding=binding; out.tick=tick;
    out.disposition=MotionDisposition::Hold;
    return out;
}
bool verified(const runtime::WorldQueryResult& r,
              const runtime::QueryRequest& q) noexcept {
    return r.stamp==q.stamp && r.kind==runtime::QueryKind::Door &&
        r.error==runtime::QueryError::None && r.door && r.door->id;
}
}

void LocalDoor::reset() noexcept {
    if(wait_) (void)wait_->abort();
    wait_.reset(); binding_={}; doorId_=0; passive_=false; contactSent_=false;
}

std::optional<WalkDecision> LocalDoor::update(
    const runtime::MovementSnapshot& s,Binding binding,
    model::NavVector3 desiredTarget,const query::NavSpatialIndex& index,
    core::MapGeneration indexMap,GroundProbeLimits probeLimits,
    runtime::IWorldQueries& port,std::uint64_t nowUs,std::uint32_t& used,
    std::uint32_t maximum) noexcept {
    auto out=hold(binding,s.tick);
    if(!binding.agent.isValid() || !binding.actor.isValid() || !binding.map.isValid() ||
       !binding.routeGeneration || s.agent!=binding.agent || s.actor!=binding.actor ||
       s.map!=binding.map || s.map!=indexMap || !s.tick.isValid() || !s.position ||
       !s.position->isFinite() || !s.hull || !desiredTarget.isFinite()) {
        out.probeReason=ProbeReason::InvalidInput;
        return out;
    }
    if(used>=maximum) {
        out.probeReason=ProbeReason::BudgetExceeded;
        return out;
    }
    runtime::QueryRequest q{{s.agent,s.actor,s.map,s.tick,binding.routeGeneration,++used},
        runtime::QueryKind::Door,*s.position,desiredTarget,s.hull,
        probeLimits.navTolerance,doorId_};
    out.queries=1;
    runtime::WorldQueryResult r;
    try { r=port.query(q); }
    catch(...) { out.probeReason=ProbeReason::QueryFailed; return out; }
    if(!verified(r,q)) {
        if(r.stamp==q.stamp && r.kind==q.kind && r.error==runtime::QueryError::BudgetExceeded)
            out.probeReason=ProbeReason::BudgetExceeded;
        else if(r.stamp==q.stamp && r.kind==q.kind && r.error==runtime::QueryError::Unavailable)
            out.probeReason=ProbeReason::QueryUnavailable;
        else if(r.stamp==q.stamp && r.kind==q.kind && r.error==runtime::QueryError::None && !r.door)
            return std::nullopt;
        else out.probeReason=ProbeReason::InvalidResult;
        return out;
    }
    const auto& door=*r.door;
    out.doorId=door.id;
    if(door.open) {
        out.doorState=DoorWaitState::Clear;
        reset();
        return out;
    }
    if(door.canUse && !door.useView) return out;
    if(wait_ && (!same(binding,binding_) || door.id!=doorId_)) reset();
    if(!wait_) {
        if(!door.canUse && !door.canTouch) return out;
        passive_=door.canTouch && !door.canUse;
        wait_.emplace(binding,timeoutUs);
        binding_=binding; doorId_=door.id; contactSent_=false;
    }
    DoorWaitFeedback feedback{binding,q.stamp,r,nowUs,{}};
    feedback.passive=passive_;
    if(door.canUse && door.useView)
        feedback.useView=core::IntentVector{door.useView->x,door.useView->y,door.useView->z};
    const auto decision=wait_->update(feedback);
    out.doorState=decision.state; out.doorReason=decision.reason;
    out.intent=decision.intent;
    if(decision.state==DoorWaitState::Clear) { reset(); return out; }
    if(decision.state==DoorWaitState::Failed || decision.state==DoorWaitState::Aborted) {
        out.reason=WalkReason::DoorBlocked;
        return out;
    }
    if(!passive_ || contactSent_) return out;

    if(!r.hull || r.hull->startSolid || !r.hull->end.isFinite() ||
       !r.hull->normal.isFinite() || !std::isfinite(r.hull->fraction) ||
       r.hull->fraction<0 || r.hull->fraction>=1) {
        out.probeReason=ProbeReason::InvalidResult;
        return out;
    }
    const double dx=double(desiredTarget.x)-s.position->x;
    const double dy=double(desiredTarget.y)-s.position->y;
    const double length=std::hypot(dx,dy);
    if(length<=0 || length>probeLimits.maxDistance ||
       std::abs(double(desiredTarget.z)-s.position->z)>0.1) {
        out.probeReason=ProbeReason::InvalidInput;
        return out;
    }
    const double ux=dx/length,uy=dy/length;
    const double hx=double(r.hull->end.x)-s.position->x;
    const double hy=double(r.hull->end.y)-s.position->y;
    const double distance=hx*ux+hy*uy;
    if(distance<0 || distance>length || std::abs(hx*uy-hy*ux)>0.01 ||
       std::abs(double(r.hull->end.z)-s.position->z)>0.1 ||
       std::abs(distance-length*r.hull->fraction)>0.02 ||
       r.hull->normal.x*ux+r.hull->normal.y*uy>-0.7 ||
       std::abs(r.hull->normal.z)>0.2) {
        out.probeReason=ProbeReason::InvalidResult;
        return out;
    }

    if(used>=maximum) { out.probeReason=ProbeReason::BudgetExceeded; return out; }
    auto supportLimits=probeLimits;
    supportLimits.maxQueries=maximum-used;
    const auto support=TerrainSampler::locate(s,binding.routeGeneration,index,indexMap,
        port,supportLimits);
    used+=support.queries;
    out.queries+=support.queries; out.samples=support.samples; out.steps=support.steps;
    out.probeReason=support.reason;
    if(!support) return out;
    if(!s.elapsedUs) { out.probeReason=ProbeReason::InvalidInput; return out; }
    out.intent.direction={ux,uy,0};
    if(distance>0.125) {
        if(used>=maximum) { out.probeReason=ProbeReason::BudgetExceeded; return out; }
        auto approachLimits=probeLimits;
        approachLimits.maxQueries=maximum-used;
        approachLimits.maxSamples=(std::min)(approachLimits.maxSamples,
            probeLimits.maxSamples>out.samples ? probeLimits.maxSamples-out.samples:0U);
        const auto travel=(std::min)(distance-0.0625,
            double(approachLimits.maxSamples)*approachLimits.sampleSpacing);
        const auto x=static_cast<float>(s.position->x+ux*travel);
        const auto y=static_cast<float>(s.position->y+uy*travel);
        const auto approach=TerrainSampler::inspect(s,binding.routeGeneration,
            support.target->area,x,y,index,indexMap,port,approachLimits);
        used+=approach.queries; out.queries+=approach.queries;
        out.samples+=approach.samples; out.steps+=approach.steps;
        out.probeReason=approach.reason;
        if(!approach) { out.intent={}; return out; }
        out.target=approach.target;
        if(!core::Motor::bindLocomotion(out.intent,core::LocomotionMode::Run,
                double(s.speedLimit.value_or(250)),travel)) {
            out.intent={}; return out;
        }
        return out;
    }
    out.intent.speed=(std::min)(400.0,500000.0/double(s.elapsedUs));
    out.contact=DoorContact{door.id,
        {static_cast<float>(s.position->x+ux*0.75),
         static_cast<float>(s.position->y+uy*0.75),s.position->z}};
    contactSent_=true;
    return out;
}
}
