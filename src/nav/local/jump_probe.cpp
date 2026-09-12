// SPDX-License-Identifier: MPL-2.0
#include "nav/local/jump_probe.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace astrabot::nav::local {
std::optional<JumpLimits> deriveJumpLimits(JumpLimits motion,const JumpPhysics& physics,
    const runtime::MovementSnapshot& s,TraversalConstraints hints) noexcept {
    if(!hints || hints.noJump || physics.tick!=s.tick || physics.binding.agent!=s.agent ||
       physics.binding.actor!=s.actor || physics.binding.map!=s.map || !s.hull ||
       !s.speedLimit || !std::isfinite(*s.speedLimit) || *s.speedLimit<=0 ||
       !std::isfinite(physics.gravity) || physics.gravity<=0 ||
       !std::isfinite(physics.verticalImpulse) || physics.verticalImpulse<=0) return {};
    const auto validHull=[](const runtime::HullDimensions& h) {
        return h.minimum.isFinite() && h.maximum.isFinite() && h.minimum.x<h.maximum.x &&
            h.minimum.y<h.maximum.y && h.minimum.z<h.maximum.z;
    };
    if(!validHull(*s.hull)) return {};
    if(physics.standingHull || physics.crouchingHull) {
        if(!physics.standingHull || !physics.crouchingHull || !s.ducked ||
           !validHull(*physics.standingHull) || !validHull(*physics.crouchingHull)) return {};
        const auto& expected=*s.ducked ? *physics.crouchingHull:*physics.standingHull;
        if(s.hull->minimum!=expected.minimum || s.hull->maximum!=expected.maximum) return {};
    }
    motion.standingHull=physics.standingHull;
    motion.maximumRise=physics.verticalImpulse*physics.verticalImpulse/(2*physics.gravity);
    if(!std::isfinite(motion.maximumRise) || motion.maximumRise<=0) return {};
    // Ground approach follows the engine Run contract. Distance limits the
    // command lifetime; it must not synthesize a fractional approach speed.
    motion.approachSpeed=double(*s.speedLimit);
    motion.maximumSpeed=(std::max)(motion.maximumSpeed,motion.approachSpeed);
    if(hints.sourceDuck) {
        if(!std::isfinite(physics.crouchSpeedMultiplier) || physics.crouchSpeedMultiplier<=0 ||
           physics.crouchSpeedMultiplier>1) return {};
        const double speed=*s.speedLimit*physics.crouchSpeedMultiplier;
        motion.minimumSpeed=(std::min)(motion.minimumSpeed,speed*0.8);
        motion.maximumDistance=(std::min)(motion.maximumDistance,speed*2*physics.verticalImpulse/physics.gravity);
    }
    if(hints.sourceDuck || hints.targetDuck) {
        if(!physics.standingHull || !physics.crouchingHull) return {};
        motion.flightHull=physics.crouchingHull;
    } else motion.flightHull.reset();
    return motion;
}
namespace {
bool same(Binding a,Binding b) noexcept {
    return a.agent==b.agent && a.actor==b.actor && a.map==b.map &&
        a.routeGeneration==b.routeGeneration && a.step==b.step;
}
double distance(model::NavVector3 a,model::NavVector3 b) noexcept {
    return std::hypot(double(a.x)-b.x,double(a.y)-b.y);
}

ProbeReason floorProbeReason(const runtime::FloorObservation& floor) noexcept {
    switch(floor.status) {
    case runtime::FloorObservationStatus::TraceNoHit: return ProbeReason::TraceNoHit;
    case runtime::FloorObservationStatus::StartSolid: return ProbeReason::StartSolid;
    case runtime::FloorObservationStatus::AllSolid: return ProbeReason::AllSolid;
    case runtime::FloorObservationStatus::InvalidTrace: return ProbeReason::InvalidResult;
    case runtime::FloorObservationStatus::UnsupportedNormal: return ProbeReason::UnsupportedFloor;
    case runtime::FloorObservationStatus::HeightMismatch: return ProbeReason::FloorHeightMismatch;
    case runtime::FloorObservationStatus::NavContainmentMissing: return ProbeReason::NavContainmentMissing;
    case runtime::FloorObservationStatus::Unknown:
    case runtime::FloorObservationStatus::Supported: break;
    }
    if(!floor.supported) return ProbeReason::ActorNotGrounded;
    if(!floor.normal.isFinite() || floor.normal.z<0.7f) return ProbeReason::UnsupportedFloor;
    return ProbeReason::None;
}
bool positive(double n) noexcept { return std::isfinite(n) && n>0; }
bool representable(double n) noexcept {
    return std::isfinite(n) && std::abs(n)<=(std::numeric_limits<float>::max)();
}
// Maps a GroundProbe's local ordinals into the enclosing decision budget.
// Stale replies cannot become current merely by reversing the ordinal mapping.
class PreparationQueries final : public runtime::IWorldQueries {
public:
    PreparationQueries(runtime::IWorldQueries& port,const query::NavSpatialIndex& index,
        model::NavAreaId source,double tolerance,std::uint32_t& issued) noexcept
        : port_(port),index_(index),source_(source),tolerance_(tolerance),issued_(issued) {}
    runtime::WorldQueryResult query(const runtime::QueryRequest& request) override {
        auto actual=request; actual.stamp.ordinal=++issued_;
        auto reply=port_.query(actual);
        if(!(reply.stamp==actual.stamp)) { reply.stamp={}; return reply; }
        reply.stamp=request.stamp;
        if(reply.error==runtime::QueryError::None && reply.kind==runtime::QueryKind::Floor && reply.floor) {
            const auto match=index_.containing({request.start.x,request.start.y,reply.floor->height},tolerance_);
            if(!match || !*match.value || (**match.value).areaId!=source_) reply.floor.reset();
        }
        return reply;
    }
private:
    runtime::IWorldQueries& port_;
    const query::NavSpatialIndex& index_;
    model::NavAreaId source_;
    double tolerance_;
    std::uint32_t& issued_;
};
JumpProbeReason preparationReason(ProbeReason reason) noexcept {
    switch(reason) {
    case ProbeReason::None: return JumpProbeReason::None;
    case ProbeReason::StaleNavigation: return JumpProbeReason::StaleNavigation;
    case ProbeReason::BudgetExceeded: return JumpProbeReason::BudgetExceeded;
    case ProbeReason::StaleQuery: return JumpProbeReason::StaleQuery;
    case ProbeReason::QueryUnavailable: return JumpProbeReason::QueryUnavailable;
    case ProbeReason::QueryFailed: return JumpProbeReason::QueryFailed;
    case ProbeReason::InvalidResult: return JumpProbeReason::InvalidResult;
    case ProbeReason::ActorNotGrounded: return JumpProbeReason::ActorNotGrounded;
    case ProbeReason::FloorHeightMismatch: return JumpProbeReason::FloorHeightMismatch;
    case ProbeReason::NavContainmentMissing: return JumpProbeReason::NavContainmentMissing;
    case ProbeReason::ActorGroundFlagMismatch: return JumpProbeReason::ActorNotGrounded;
    case ProbeReason::TraceNoHit:
    case ProbeReason::StartSolid:
    case ProbeReason::AllSolid:
    case ProbeReason::UnsupportedFloor: return JumpProbeReason::NoSupport;
    case ProbeReason::NoSupport: case ProbeReason::UnsafeDrop: return JumpProbeReason::NoSupport;
    case ProbeReason::NoArea: case ProbeReason::WrongStartArea: return JumpProbeReason::WrongArea;
    case ProbeReason::Blocked: return JumpProbeReason::Blocked;
    default: return JumpProbeReason::InvalidInput;
    }
}
}
JumpTrajectoryResult solveJumpTrajectory(model::NavVector3 origin,model::NavVector3 velocity,
    model::NavVector3 landing,JumpLimits motion,JumpPhysics physics) noexcept {
    JumpTrajectoryResult result;
    const auto positive=[](double value) noexcept { return std::isfinite(value) && value>0; };
    if(!origin.isFinite() || !velocity.isFinite() || !landing.isFinite() || !positive(physics.gravity) ||
       !positive(physics.verticalImpulse) || !positive(motion.maximumDistance) ||
       !positive(motion.maximumRise) || !positive(motion.landingRadius) || !motion.airborneTimeoutUs)
        return result;
    const double rise=double(landing.z)-origin.z;
    const double discriminant=physics.verticalImpulse*physics.verticalImpulse-2*physics.gravity*rise;
    if((rise<0 && !motion.flightHull) || rise>motion.maximumRise || !std::isfinite(discriminant) || discriminant<=0) {
        result.reason=JumpTrajectoryReason::BallisticSolution; return result;
    }
    const double seconds=(physics.verticalImpulse+std::sqrt(discriminant))/physics.gravity;
    if(!positive(seconds) || seconds>=double(motion.airborneTimeoutUs)/1'000'000.0) {
        result.reason=JumpTrajectoryReason::BallisticSolution; return result;
    }
    const double x=origin.x+velocity.x*seconds,y=origin.y+velocity.y*seconds;
    if(!std::isfinite(x) || !std::isfinite(y) || std::abs(x)>(std::numeric_limits<float>::max)() ||
       std::abs(y)>(std::numeric_limits<float>::max)()) {
        result.reason=JumpTrajectoryReason::BallisticSolution; return result;
    }
    result.seconds=seconds;
    result.touchdown={static_cast<float>(x),static_cast<float>(y),landing.z};
    result.landingError=std::hypot(double(result.touchdown.x)-landing.x,double(result.touchdown.y)-landing.y);
    if(result.landingError>motion.landingRadius) { result.reason=JumpTrajectoryReason::LandingRadius; return result; }
    if(std::hypot(double(result.touchdown.x)-origin.x,double(result.touchdown.y)-origin.y)>motion.maximumDistance) {
        result.reason=JumpTrajectoryReason::MaximumDistance; return result;
    }
    result.reason=JumpTrajectoryReason::None; return result;
}
JumpProbeResult JumpProbe::land(const runtime::MovementSnapshot& s,Binding binding,
    JumpPlan plan,JumpLimits motion,GroundProbeLimits limits,const query::NavSpatialIndex& index,
    core::MapGeneration indexMap,runtime::IWorldQueries& port) noexcept {
    JumpProbeResult result;
    const auto fail=[&](JumpProbeReason reason) { result.reason=reason; return result; };
    if(s.ducked!=plan.flightHull.has_value() || (plan.flightHull && (!s.hull ||
       s.hull->minimum!=plan.flightHull->minimum || s.hull->maximum!=plan.flightHull->maximum)))
        return fail(JumpProbeReason::InvalidInput);
    if(s.agent!=binding.agent || s.actor!=binding.actor || s.map!=binding.map || !s.ducked ||
       !plan.takeoff.isFinite() || !plan.landing.isFinite() || !plan.target.isValid() ||
       !positive(motion.landingRadius) || !positive(motion.supportTolerance) ||
       !limits.maxQueries || limits.maxQueries>21 || limits.maxQueries>motion.maxQueries)
        return fail(JumpProbeReason::InvalidInput);
    const auto support=GroundProbe::locate(s,binding.routeGeneration,index,indexMap,port,limits);
 result.supportReason=support.reason;
    result.supportInitialReason=support.initialReason;
    result.supportFallbackAttempted=support.supportFallbackAttempted;
    result.supportFallbackAccepted=support.supportFallbackAccepted;
    result.supportInitialTrace=support.initialTrace;
    result.supportFallbackTrace=support.fallbackTrace;
    result.queries=support.queries;
    if(!support) return fail(preparationReason(support.reason));
    if(support.target->area!=jumpLandingArea(plan)) return fail(JumpProbeReason::WrongArea);
    if(distance(*s.position,plan.landing)>motion.landingRadius ||
       std::abs(double(s.position->z)-plan.landing.z)>motion.supportTolerance)
        return fail(JumpProbeReason::CannotLand);
    JumpInspection proof; proof.stamp=support.stamp; proof.step=binding.step; proof.queries=result.queries;
    proof.origin=*s.position; proof.hull=*s.hull; proof.support=support.target;
    proof.takeoff=plan.takeoff; proof.landing=plan.landing;
    result.inspection=proof; result.touchdown=*s.position; return result;
}
JumpProbeResult JumpProbe::prepare(const runtime::MovementSnapshot& s,Binding binding,
    JumpPlan plan,JumpLimits motion,GroundProbeLimits limits,const query::NavSpatialIndex& index,
    core::MapGeneration indexMap,runtime::IWorldQueries& port) noexcept {
    JumpProbeResult result;
    const auto fail=[&](JumpProbeReason reason) {
        result.reason=reason; result.inspection.reset();
        if(result.provenance==JumpProofProvenance::None) {
            if(reason==JumpProbeReason::BudgetExceeded) result.provenance=JumpProofProvenance::QueryBudget;
            else if(reason==JumpProbeReason::StaleNavigation || reason==JumpProbeReason::StalePhysics ||
                    reason==JumpProbeReason::StaleQuery) result.provenance=JumpProofProvenance::Stale;
            else if(reason==JumpProbeReason::QueryFailed || reason==JumpProbeReason::InvalidResult)
                result.provenance=JumpProofProvenance::QueryUnavailable;
        }
        return result;
    };
    if(s.ducked!=constraints(model::NavTraversalKind::Jump,plan.sourceAttributes,plan.targetAttributes).sourceDuck)
        return fail(JumpProbeReason::InvalidInput);
    if(s.agent!=binding.agent || s.actor!=binding.actor || s.map!=binding.map || !s.ducked ||
       !s.position || !s.position->isFinite() || !plan.takeoff.isFinite() || !plan.landing.isFinite() ||
       !plan.source.isValid() || !plan.target.isValid() || plan.source==plan.target ||
       !positive(motion.takeoffRadius) || !positive(motion.approachSpeed) || !positive(motion.maximumSpeed) ||
       motion.approachSpeed>motion.maximumSpeed || motion.maximumSpeed>400 ||
       !positive(motion.maximumDistance) || !positive(limits.maxDistance) ||
       !limits.maxQueries || limits.maxQueries>21 || limits.maxQueries>motion.maxQueries)
        return fail(JumpProbeReason::InvalidInput);
    if(!constraints(model::NavTraversalKind::Jump,plan.sourceAttributes,plan.targetAttributes))
        return fail(JumpProbeReason::UnsupportedConstraints);
    const double length=distance(plan.takeoff,plan.landing),range=distance(*s.position,plan.takeoff);
    if(length<=0 || length>motion.maximumDistance) return fail(JumpProbeReason::InvalidInput);
    const auto support=GroundProbe::locate(s,binding.routeGeneration,index,indexMap,port,limits);
 result.supportReason=support.reason;
    result.supportInitialReason=support.initialReason;
    result.supportFallbackAttempted=support.supportFallbackAttempted;
    result.supportFallbackAccepted=support.supportFallbackAccepted;
    result.supportInitialTrace=support.initialTrace;
    result.supportFallbackTrace=support.fallbackTrace;
    result.queries=support.queries;
    if(!support) return fail(preparationReason(support.reason));
    if(support.target->area!=plan.source) return fail(JumpProbeReason::WrongArea);
    double x=0,y=0;
    if(range>motion.takeoffRadius) {
        const double scale=(std::min)(1.0,limits.maxDistance/range);
        x=s.position->x+(double(plan.takeoff.x)-s.position->x)*scale;
        y=s.position->y+(double(plan.takeoff.y)-s.position->y)*scale;
    } else {
        const double ux=(double(plan.landing.x)-plan.takeoff.x)/length,uy=(double(plan.landing.y)-plan.takeoff.y)/length;
        const double px=double(s.position->x)-plan.takeoff.x,py=double(s.position->y)-plan.takeoff.y;
        const double along=px*ux+py*uy,side=px*uy-py*ux;
        const double remaining=std::sqrt((std::max)(0.0,motion.takeoffRadius*motion.takeoffRadius-side*side))-along;
        const double travel=(std::min)({motion.approachSpeed*0.120,(std::max)(0.0,remaining),limits.maxDistance});
        // A clipped probe cannot authorize the controller's longer cached command.
        if(travel+0.001<(std::min)(motion.approachSpeed*0.120,(std::max)(0.0,remaining)))
            return fail(JumpProbeReason::BudgetExceeded);
        x=s.position->x+ux*travel; y=s.position->y+uy*travel;
    }
    if(!representable(x) || !representable(y)) return fail(JumpProbeReason::InvalidInput);
    if(result.queries>=limits.maxQueries) return fail(JumpProbeReason::BudgetExceeded);
    PreparationQueries queries(port,index,plan.source,limits.navTolerance,result.queries);
    limits.maxQueries-=result.queries;
    const auto path=GroundProbe::inspect(s,binding.routeGeneration,plan.source,static_cast<float>(x),static_cast<float>(y),
        index,indexMap,queries,limits);
    result.supportReason=path.reason;
    if(!path) return fail(preparationReason(path.reason));
    if(path.target->area!=plan.source) return fail(JumpProbeReason::WrongArea);
    JumpInspection proof; proof.stamp=support.stamp; proof.step=binding.step; proof.queries=result.queries;
    proof.origin=*s.position; proof.hull=*s.hull; proof.support=support.target; proof.approach=path.target;
    proof.takeoff=plan.takeoff; proof.landing=plan.landing; proof.approachClear=proof.takeoffClear=true;
    proof.approachProof=proof.takeoffProof=JumpProof::Passed;
    proof.provenance=JumpProofProvenance::GroundProbe;
    proof.validatedDistance=distance(*s.position,path.target->origin);
    proof.validForUs=static_cast<std::uint64_t>((std::min)(120'000.0,
        proof.validatedDistance/motion.maximumSpeed*1'000'000.0));
    result.inspection=proof; result.takeoffProof=JumpProof::Passed;
    result.provenance=JumpProofProvenance::GroundProbe; return result;
}
JumpProbeResult JumpProbe::launch(const runtime::MovementSnapshot& s,Binding binding,
 JumpPlan plan,JumpLimits motion,JumpPhysics physics,JumpProbeLimits limits,
 const query::NavSpatialIndex& index,core::MapGeneration indexMap,runtime::IWorldQueries& port) noexcept {
 JumpProbeResult result;
 result.supportEvidence[0].role=JumpSupportRole::Source;
 result.supportEvidence[1].role=JumpSupportRole::PlannedLanding;
 result.supportEvidence[2].role=JumpSupportRole::PredictedLanding;
    const auto fail=[&](JumpProbeReason reason) {
        result.reason=reason; result.inspection.reset();
        if(result.provenance==JumpProofProvenance::None) {
            if(reason==JumpProbeReason::BudgetExceeded) result.provenance=JumpProofProvenance::QueryBudget;
            else if(reason==JumpProbeReason::StaleNavigation || reason==JumpProbeReason::StalePhysics ||
                    reason==JumpProbeReason::StaleQuery) result.provenance=JumpProofProvenance::Stale;
            else if(reason==JumpProbeReason::QueryFailed || reason==JumpProbeReason::InvalidResult)
                result.provenance=JumpProofProvenance::QueryUnavailable;
        }
        return result;
    };
    if(!binding.agent.isValid() || !binding.actor.isValid() || !binding.map.isValid() || !binding.routeGeneration ||
       s.agent!=binding.agent || s.actor!=binding.actor || s.map!=binding.map || !s.tick.isValid() ||
       s.kind!=runtime::ActorKind::ManagedBot || s.connected!=true || s.alive!=true || s.joined!=true ||
       s.grounded!=true || s.ducked!=constraints(model::NavTraversalKind::Jump,plan.sourceAttributes,plan.targetAttributes).sourceDuck || !s.position || !s.position->isFinite() ||
       !s.velocity || !s.velocity->isFinite() || !s.view || !s.view->isFinite() ||
       !s.speedLimit || !positive(*s.speedLimit) || !s.hull ||
       !s.hull->minimum.isFinite() || !s.hull->maximum.isFinite() ||
       s.hull->minimum.x>=s.hull->maximum.x || s.hull->minimum.y>=s.hull->maximum.y ||
       s.hull->minimum.z>=s.hull->maximum.z || !plan.source.isValid() || !plan.target.isValid() ||
       plan.source==plan.target || !plan.takeoff.isFinite() || !plan.landing.isFinite())
        return fail(JumpProbeReason::InvalidInput);
    if(indexMap!=s.map) return fail(JumpProbeReason::StaleNavigation);
    if(!same(binding,physics.binding) || physics.tick!=s.tick) return fail(JumpProbeReason::StalePhysics);
    if(plan.flightHull && (!physics.crouchingHull ||
       plan.flightHull->minimum!=physics.crouchingHull->minimum ||
       plan.flightHull->maximum!=physics.crouchingHull->maximum)) return fail(JumpProbeReason::StalePhysics);
    if(!constraints(model::NavTraversalKind::Jump,plan.sourceAttributes,plan.targetAttributes))
        return fail(JumpProbeReason::UnsupportedConstraints);
    for(double v : {physics.gravity,physics.verticalImpulse,motion.minimumSpeed,motion.maximumSpeed,
                   motion.takeoffRadius,motion.landingRadius,motion.maximumDistance,motion.maximumRise,
                   motion.supportTolerance,motion.facingDegrees,limits.maxSegmentSeconds,limits.maxChordRise})
        if(!positive(v)) return fail(JumpProbeReason::InvalidInput);
    if(motion.minimumSpeed>motion.maximumSpeed || motion.maximumSpeed>400 || motion.facingDegrees>45 ||
       !motion.airborneTimeoutUs || !std::isfinite(limits.navTolerance) || limits.navTolerance<0 ||
       !limits.maxQueries || limits.maxQueries>21 || !limits.maxSegments || limits.maxSegments>8 ||
       limits.maxQueries>motion.maxQueries) return fail(JumpProbeReason::InvalidInput);
    const double length=distance(plan.takeoff,plan.landing);
    if(length<=0 || length>motion.maximumDistance || (plan.landing.z<plan.takeoff.z && !plan.flightHull) ||
       double(plan.landing.z)-plan.takeoff.z>motion.maximumRise) return fail(JumpProbeReason::InvalidInput);
    if(distance(*s.position,plan.takeoff)>motion.takeoffRadius ||
       std::abs(double(s.position->z)-plan.takeoff.z)>motion.supportTolerance)
        return fail(JumpProbeReason::OutsideTakeoff);
    const double ux=(double(plan.landing.x)-plan.takeoff.x)/length,uy=(double(plan.landing.y)-plan.takeoff.y)/length;
    const double cap=(std::min)(motion.maximumSpeed,double(*s.speedLimit));
    if(cap<motion.minimumSpeed) return fail(JumpProbeReason::InvalidVelocity);
    const double desired=(std::min)((std::max)(motion.approachSpeed,motion.minimumSpeed),cap);
    const double launchVx=ux*desired,launchVy=uy*desired;
    if(std::abs(double(s.velocity->z))>0.01)
        return fail(JumpProbeReason::InvalidVelocity);
    const runtime::QueryStamp stamp{s.agent,s.actor,s.map,s.tick,binding.routeGeneration,0};
    const auto fetch=[&](runtime::QueryKind kind,model::NavVector3 start,model::NavVector3 end,
        std::optional<runtime::HullDimensions> hull={})
        ->std::optional<runtime::WorldQueryResult> {
        if(!start.isFinite() || !end.isFinite()) { result.reason=JumpProbeReason::InvalidInput; return {}; }
        if(result.queries==limits.maxQueries) { result.reason=JumpProbeReason::BudgetExceeded; return {}; }
        runtime::QueryRequest q{stamp,kind,start,end,hull ? hull:s.hull,limits.navTolerance}; q.stamp.ordinal=++result.queries;
        try {
            const auto r=port.query(q);
            if(!(r.stamp==q.stamp) || r.kind!=kind) {
                result.reason=JumpProbeReason::StaleQuery;
                if(kind==runtime::QueryKind::GroundedArea) result.supportReason=ProbeReason::StaleQuery;
            } else if(r.error==runtime::QueryError::BudgetExceeded) {
                result.reason=JumpProbeReason::BudgetExceeded;
                if(kind==runtime::QueryKind::GroundedArea) result.supportReason=ProbeReason::BudgetExceeded;
            } else if(r.error==runtime::QueryError::Unavailable) {
                result.reason=JumpProbeReason::QueryUnavailable;
                if(kind==runtime::QueryKind::GroundedArea) result.supportReason=ProbeReason::QueryUnavailable;
            } else if(r.error==runtime::QueryError::InvalidResult) {
                result.reason=JumpProbeReason::InvalidResult;
                if(kind==runtime::QueryKind::GroundedArea) result.supportReason=ProbeReason::InvalidResult;
            } else if(r.error!=runtime::QueryError::None) {
                result.reason=JumpProbeReason::QueryFailed;
                if(kind==runtime::QueryKind::GroundedArea) result.supportReason=ProbeReason::QueryFailed;
            }
            else return r;
        } catch(...) { result.reason=JumpProbeReason::QueryFailed; }
        return {};
    };
    const auto ground=[&](JumpSupportRole role,model::NavVector3 origin,model::NavAreaId area,
        std::optional<runtime::HullDimensions> hull={})->std::optional<GroundedTarget> {
        auto& evidence=result.supportEvidence[static_cast<std::size_t>(role)];
        evidence={};
        evidence.role=role;
        evidence.origin=origin;
        evidence.expectedArea=area;
        evidence.hull=hull ? hull:s.hull;
        const auto r=fetch(runtime::QueryKind::GroundedArea,origin,origin,hull);
        if(!r) return {};
        if(!r->ground || !r->ground->floor) {
            evidence.reason=ProbeReason::QueryUnavailable;
            if(role==JumpSupportRole::Source) result.supportReason=ProbeReason::QueryUnavailable;
            result.reason=JumpProbeReason::QueryUnavailable;
            return {};
        }
        auto floor=*r->ground->floor;
        evidence.initialTrace=floor.trace;
        if(role==JumpSupportRole::Source) result.supportInitialTrace=floor.trace;
        auto supportReason=floorProbeReason(floor);
        const auto initialSupportReason=supportReason;
        bool fallbackCandidate=false;
        evidence.initialReason=supportReason;
        if(role==JumpSupportRole::Source) result.supportInitialReason=supportReason;
        if(supportReason==ProbeReason::AllSolid || supportReason==ProbeReason::StartSolid) {
            evidence.fallbackAttempted=true;
            if(role==JumpSupportRole::Source) result.supportFallbackAttempted=true;
            const auto& activeHull=hull ? *hull:*s.hull;
            const double feet=double(origin.z)+activeHull.minimum.z;
            const double tolerance=limits.supportTolerance>0 ? limits.supportTolerance:motion.supportTolerance;
            const double depth=limits.supportProbeDepth>0 ? limits.supportProbeDepth:tolerance;
            const auto fallback=fetch(runtime::QueryKind::Floor,
                {origin.x,origin.y,static_cast<float>(feet+tolerance)},
                {origin.x,origin.y,static_cast<float>(feet-depth)},hull);
            if(fallback && fallback->floor) {
                const auto fallbackFloor=*fallback->floor;
                evidence.fallbackTrace=fallbackFloor.trace;
                if(role==JumpSupportRole::Source) result.supportFallbackTrace=fallbackFloor.trace;
                evidence.fallbackReason=floorProbeReason(fallbackFloor);
                if(evidence.fallbackReason==ProbeReason::None) {
                    floor=fallbackFloor;
                    supportReason=ProbeReason::None;
                    fallbackCandidate=true;
                } else supportReason=initialSupportReason;
            }
        }
        evidence.reason=supportReason;
        if(role==JumpSupportRole::Source) result.supportReason=supportReason;
        if(result.supportReason!=ProbeReason::None) {
            result.reason=preparationReason(result.supportReason);
            return {};
        }
        const double normal=double(floor.normal.x)*floor.normal.x+double(floor.normal.y)*floor.normal.y+double(floor.normal.z)*floor.normal.z;
        if(!std::isfinite(floor.height) || !floor.normal.isFinite() || normal<0.99 || normal>1.01) {
            result.reason=JumpProbeReason::InvalidResult; return {};
        }
        if(!floor.supported || floor.normal.z<0.7f ||
           std::abs(double(origin.z)+(hull ? hull->minimum.z:s.hull->minimum.z)-floor.height)>motion.supportTolerance) {
            evidence.reason=ProbeReason::FloorHeightMismatch;
            if(role==JumpSupportRole::Source) result.supportReason=ProbeReason::FloorHeightMismatch;
            result.reason=JumpProbeReason::FloorHeightMismatch; return {};
        }
        const auto match=index.containing({origin.x,origin.y,floor.height},limits.navTolerance);
        if(!r->ground->area || !match || !*match.value) {
            evidence.reason=ProbeReason::NavContainmentMissing;
            if(role==JumpSupportRole::Source) result.supportReason=ProbeReason::NavContainmentMissing;
            result.reason=JumpProbeReason::NavContainmentMissing; return {};
        }
        if(*r->ground->area!=area || (**match.value).areaId!=area) {
            evidence.reason=ProbeReason::WrongStartArea;
            if(role==JumpSupportRole::Source) result.supportReason=ProbeReason::WrongStartArea;
            result.reason=JumpProbeReason::WrongArea; return {};
        }
        evidence.reason=ProbeReason::None;
        if(fallbackCandidate) {
            evidence.fallbackAccepted=true;
            if(role==JumpSupportRole::Source) result.supportFallbackAccepted=true;
        }
        return GroundedTarget{origin,area,floor};
    };
    const auto sweep=[&](model::NavVector3 start,model::NavVector3 end,std::optional<runtime::HullDimensions> hull={}) {
        const auto r=fetch(runtime::QueryKind::SweptHull,start,end,hull); if(!r) return false;
        if(!r->hull || !std::isfinite(r->hull->fraction) || r->hull->fraction<0 || r->hull->fraction>1 ||
           !r->hull->end.isFinite() || !r->hull->normal.isFinite()) result.reason=JumpProbeReason::InvalidResult;
        else if(r->hull->startSolid || r->hull->fraction!=1) {
            result.reason=JumpProbeReason::Blocked;
            if(r->blocker && r->blocker->kind==runtime::BlockerKind::Geometry) {
                result.takeoffProof=result.flightProof=result.landingProof=JumpProof::StaticBlocked;
                result.provenance=JumpProofProvenance::StaticBsp;
            } else {
                result.takeoffProof=result.flightProof=result.landingProof=JumpProof::TransientUnknown;
                result.provenance=r->blocker ? JumpProofProvenance::DynamicBlocker:
                                              JumpProofProvenance::QueryUnavailable;
            }
        }
        else if(distance(r->hull->end,end)>0.001 || std::abs(double(r->hull->end.z)-end.z)>0.001)
            result.reason=JumpProbeReason::InvalidResult;
        else return true;
        return false;
    };
    const auto source=ground(JumpSupportRole::Source,*s.position,plan.source); if(!source) return fail(result.reason);
    const auto destination=ground(JumpSupportRole::PlannedLanding,plan.landing,jumpLandingArea(plan),plan.flightHull);
    if(!destination) {
        result.landingFailure=JumpLandingFailure::PlannedSupport;
        return fail(result.reason);
    }
    const double landingZ=double(destination->floor.height)-(plan.flightHull ? plan.flightHull->minimum.z:s.hull->minimum.z);
    const double rise=landingZ-s.position->z;
    const double discriminant=physics.verticalImpulse*physics.verticalImpulse-2*physics.gravity*rise;
    if(!representable(landingZ) || (rise<0 && !plan.flightHull) || rise>motion.maximumRise || !std::isfinite(discriminant) || discriminant<=0) {
        result.landingFailure=JumpLandingFailure::BallisticSolution;
        return fail(JumpProbeReason::CannotLand);
    }
    const model::NavVector3 expectedLanding{plan.landing.x,plan.landing.y,static_cast<float>(landingZ)};
    const auto trajectory=solveJumpTrajectory(*s.position,{s.velocity->x,s.velocity->y,0},
        expectedLanding,motion,physics);
    if(!trajectory) {
        result.landingFailure=trajectory.reason==JumpTrajectoryReason::LandingRadius ? JumpLandingFailure::LandingRadius:
            (trajectory.reason==JumpTrajectoryReason::MaximumDistance ? JumpLandingFailure::MaximumDistance:
             JumpLandingFailure::BallisticSolution);
        if(trajectory.reason==JumpTrajectoryReason::LandingRadius) {
            // The candidate is sound but the actor has not yet reached the
            // required horizontal velocity.  Preserve source support and a
            // bounded takeoff circle so SimpleJump can keep issuing Run.
            JumpInspection pending;
            pending.stamp=stamp; pending.step=binding.step; pending.queries=result.queries;
            pending.origin=*s.position; pending.hull=*s.hull; pending.velocity=*s.velocity;
            pending.support=source; pending.approach=source; pending.takeoff=plan.takeoff;
            pending.landing=expectedLanding; pending.predictedLanding=trajectory.touchdown;
            pending.landingError=trajectory.landingError; pending.validatedDistance=motion.takeoffRadius;
            pending.validForUs=120'000; pending.provenance=JumpProofProvenance::GroundProbe;
            result.inspection=pending; result.provenance=JumpProofProvenance::GroundProbe;
            result.touchdown=trajectory.touchdown; result.flightSeconds=trajectory.seconds;
            result.landingError=trajectory.landingError; result.trajectoryReady=false;
            result.reason=JumpProbeReason::None; return result;
        }
        return fail(JumpProbeReason::CannotLand);
    }
    const double time=trajectory.seconds;
    const double x=trajectory.touchdown.x,y=trajectory.touchdown.y;
    if(!positive(time) || time>=double(motion.airborneTimeoutUs)/1000000 || !representable(x) || !representable(y)) {
        result.landingFailure=JumpLandingFailure::BallisticSolution;
        return fail(JumpProbeReason::CannotLand);
    }
    const model::NavVector3 touchdown{static_cast<float>(x),static_cast<float>(y),static_cast<float>(landingZ)};
    if(distance(touchdown,plan.landing)>motion.landingRadius) {
        result.landingFailure=JumpLandingFailure::LandingRadius;
        return fail(JumpProbeReason::CannotLand);
    }
    if(distance(touchdown,*s.position)>motion.maximumDistance) {
        result.landingFailure=JumpLandingFailure::MaximumDistance;
        return fail(JumpProbeReason::CannotLand);
    }
    // A second support measurement checks where the observed velocity actually
    // lands. A different-height surface invalidates this trajectory, never retries.
    const auto landing=ground(JumpSupportRole::PredictedLanding,touchdown,jumpLandingArea(plan),plan.flightHull);
    if(!landing) {
        result.landingFailure=JumpLandingFailure::PredictedSupport;
        return fail(result.reason);
    }
    if(std::abs(double(landing->floor.height)-destination->floor.height)>0.001) {
        result.landingFailure=JumpLandingFailure::FloorHeightMismatch;
        return fail(JumpProbeReason::CannotLand);
    }
    const double count=std::ceil(time/limits.maxSegmentSeconds);
    const auto remaining=limits.maxQueries-result.queries;
    if(!std::isfinite(count) || count<1 || count>limits.maxSegments ||
       remaining<2 || count>(remaining-2)/2) return fail(JumpProbeReason::BudgetExceeded);
    const auto segments=static_cast<std::uint32_t>(count);
    const double dt=time/segments;
    const double bulge=physics.gravity*dt*dt/8;
    const double hullHeight=double(s.hull->maximum.z)-s.hull->minimum.z;
    if(!positive(bulge) || bulge>limits.maxChordRise || bulge>hullHeight)
        return fail(JumpProbeReason::BudgetExceeded);
    if(!s.elapsedUs || s.elapsedUs>120'000) return fail(JumpProbeReason::InvalidInput);
    const double commandDt=double(s.elapsedUs)/1'000'000.0;
    const double errorX=launchVx-s.velocity->x,errorY=launchVy-s.velocity->y;
    const double errorLength=std::hypot(errorX,errorY);
    const double commandX=errorLength>1e-3 ? errorX/errorLength:ux;
    const double commandY=errorLength>1e-3 ? errorY/errorLength:uy;
    const model::NavVector3 projected{
        static_cast<float>(s.position->x+s.velocity->x*commandDt+commandX*(*s.speedLimit)*commandDt),
        static_cast<float>(s.position->y+s.velocity->y*commandDt+commandY*(*s.speedLimit)*commandDt),
        s.position->z};
    if(!projected.isFinite() || distance(projected,plan.takeoff)>motion.takeoffRadius)
        return fail(JumpProbeReason::OutsideTakeoff);
    const double validatedDistance=(std::hypot(double(s.velocity->x),double(s.velocity->y))+*s.speedLimit)*commandDt;
    if(!sweep(*s.position,projected) || !sweep(touchdown,touchdown,plan.flightHull)) return fail(result.reason);
    auto previous=*s.position;
    for(std::uint32_t i=1;i<=segments;++i) {
        const double t=time*i/segments;
        const double px=s.position->x+s.velocity->x*t,py=s.position->y+s.velocity->y*t;
        const double pz=s.position->z+physics.verticalImpulse*t-0.5*physics.gravity*t*t;
        if(!representable(px) || !representable(py) || !representable(pz)) return fail(JumpProbeReason::InvalidInput);
        const auto next=i==segments ? touchdown:model::NavVector3{static_cast<float>(px),static_cast<float>(py),static_cast<float>(pz)};
        // The parabola is above its chord by at most g*dt^2/8. Two vertically
        // overlapping swept boxes cover every intermediate vertical offset.
        // Native standing hulls are retained: no arbitrary expanded TraceHull.
        auto upperStart=previous,upperEnd=next;
        if(!representable(double(previous.z)+bulge) || !representable(double(next.z)+bulge))
            return fail(JumpProbeReason::InvalidInput);
        upperStart.z=std::nextafter(static_cast<float>(double(previous.z)+bulge),std::numeric_limits<float>::infinity());
        upperEnd.z=std::nextafter(static_cast<float>(double(next.z)+bulge),std::numeric_limits<float>::infinity());
        if(!upperStart.isFinite() || !upperEnd.isFinite() || double(upperStart.z)-previous.z>hullHeight ||
           double(upperEnd.z)-next.z>hullHeight) return fail(JumpProbeReason::InvalidInput);
        const auto hull=(time*(i-1)/segments>=0.240) ? plan.flightHull:std::optional<runtime::HullDimensions>{};
        if(!sweep(previous,next,hull) || !sweep(upperStart,upperEnd,hull)) return fail(result.reason);
        previous=next; ++result.segments;
    }
    JumpInspection proof; proof.stamp=stamp; proof.step=binding.step; proof.queries=result.queries;
    proof.origin=*s.position; proof.hull=*s.hull; proof.velocity=*s.velocity; proof.support=source;
    proof.supportEvidence=result.supportEvidence;
    proof.landingFailure=result.landingFailure;
    proof.takeoff=plan.takeoff; proof.landing=plan.landing;
    proof.takeoffClear=proof.flightClear=proof.landingClear=true;
    proof.approachProof=proof.takeoffProof=proof.flightProof=proof.landingProof=JumpProof::Passed;
    proof.provenance=JumpProofProvenance::SweptHull;
    proof.validatedDistance=validatedDistance; proof.validForUs=s.elapsedUs;
    proof.predictedLanding=touchdown; proof.landingError=trajectory.landingError; proof.trajectoryReady=true;
    result.inspection=proof; result.touchdown=touchdown; result.flightSeconds=time;
    result.landingError=trajectory.landingError; result.trajectoryReady=true;
    result.takeoffProof=result.flightProof=result.landingProof=JumpProof::Passed;
    result.provenance=JumpProofProvenance::SweptHull; return result;
}
}
