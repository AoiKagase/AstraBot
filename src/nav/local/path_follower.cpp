// SPDX-License-Identifier: MPL-2.0
#include "nav/local/path_follower.hpp"

#include <algorithm>
#include <cmath>

namespace astrabot::nav::local {
namespace {
using Point = query::NavQueryPoint;
constexpr std::size_t maxObservedAdvance = 16;
// Dense CS NAV meshes can require many short transitions to cover the
// controller's 300-unit look-ahead horizon. Bound work by transitions while
// letting the caller's distance limit stop the scan sooner when possible.
constexpr std::size_t maxLookAhead = 64;
// GoldSrc standing/crouched hulls share a 16-unit horizontal half width.
// This conservative bound must not grow with look-ahead or goal tolerance.
constexpr double boundaryHalfWidth = 16.0;
// A physically supported actor may straddle a NAV seam by its hull radius
// plus the NAV lookup tolerance.  Keep this wider allowance separate from
// the centre-point look-ahead radius so it cannot make a target jump ahead.
constexpr double supportBoundaryTolerance = 34.0;
constexpr double boundaryVertical = 20.0;
bool finite(Point p) noexcept {
    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}
bool contains(const model::NavExtent& e, Point p) noexcept {
    // Center membership deliberately permits NAV micro patches. World support
    // and full actor hull clearance are verified by TerrainSampler separately.
    return p.x >= e.northWest.x && p.x <= e.southEast.x &&
           p.y >= e.northWest.y && p.y <= e.southEast.y;
}
bool ordinary(const corridor::Transition& t) noexcept {
    return !t.edge.external && (t.effectiveTraversal == model::NavTraversalKind::Walk ||
                               t.effectiveTraversal == model::NavTraversalKind::Crouch);
}
// A micro portal is the zero-width seam produced where adjacent NAV patches
// touch at a corner. Guidance may target the seam itself; the world sweep
// remains responsible for proving that the hull can pass it.
bool microPortal(const corridor::Transition& t) noexcept {
    return std::abs(t.sourceHigh.x-t.sourceLow.x)<=1e-4 &&
           std::abs(t.sourceHigh.y-t.sourceLow.y)<=1e-4;
}
Point midpoint(Point a, Point b) noexcept {
    return {(a.x+b.x)*0.5,(a.y+b.y)*0.5,(a.z+b.z)*0.5};
}
double distance(Point a, Point b) noexcept { return std::hypot(a.x-b.x,a.y-b.y); }
Point toward(Point a, Point b, double length) noexcept {
    const double d=distance(a,b);
    const double f=d>length ? length/d : 1.0;
    return {a.x+(b.x-a.x)*f,a.y+(b.y-a.y)*f,a.z+(b.z-a.z)*f};
}
Point forward(const corridor::Transition& t) noexcept {
    switch(t.edge.direction) {
    case 0: return {0,-1,0};
    case 1: return {1,0,0};
    case 2: return {0,1,0};
    default: return {-1,0,0};
    }
}
double projection(Point p, Point origin, Point axis) noexcept {
    return (p.x-origin.x)*axis.x+(p.y-origin.y)*axis.y;
}
// Ordered intersections prevent look-ahead from cutting across a corridor bend.
// NAV checks are route geometry, never a replacement for a world sweep.
bool crosses(Point from, Point to, const corridor::Transition& t, double& previous) noexcept {
    // A micro portal may collapse to one point on either axis. Its cardinal
    // edge retains orientation even when both endpoint coordinates coincide.
    const bool vertical=t.edge.direction==1 || t.edge.direction==3;
    const double delta=vertical ? to.x-from.x : to.y-from.y;
    const double boundary=vertical ? t.sourceLow.x : t.sourceLow.y;
    const double origin=vertical ? from.x : from.y;
    if(std::abs(delta)<1e-9) return false;
    const double f=(boundary-origin)/delta;
    if(f < previous-1e-7 || f < -1e-7 || f > 1.0+1e-7) return false;
    const double tangent=vertical ? from.y+(to.y-from.y)*f : from.x+(to.x-from.x)*f;
    const double low=vertical ? t.sourceLow.y : t.sourceLow.x;
    const double high=vertical ? t.sourceHigh.y : t.sourceHigh.x;
    if(tangent<low-1e-7 || tangent>high+1e-7) return false;
    previous=f;
    return true;
}
}

PathFollower::PathFollower(std::shared_ptr<const corridor::Corridor> corridor,
                           Point goal) noexcept : corridor_(std::move(corridor)),goal_(goal) {}

const corridor::Transition* PathFollower::activeTransition() const noexcept {
    return corridor_ && step_<corridor_->transitions().size() ?
           &corridor_->transitions()[step_] : nullptr;
}

FollowResult PathFollower::update(Point position, model::NavAreaId area,
                                  bool supportVerified, double lookAhead,
                                  double goalTolerance) noexcept {
    FollowResult result; result.step=step_; result.routeProgress=routeProgress_;
    const auto* transitionsPtr = corridor_ ? &corridor_->transitions() : nullptr;
    const auto* goalExtent = transitionsPtr && !transitionsPtr->empty() ?
        &transitionsPtr->back().targetExtent : nullptr;
    if(!corridor_ || !finite(position) || !finite(goal_) || !std::isfinite(lookAhead) ||
       lookAhead<=0 || !std::isfinite(goalTolerance) || goalTolerance<0 ||
       (goalExtent && !contains(*goalExtent,goal_))) return result;
    if(!supportVerified) { result.status=FollowStatus::MissingSupport; return result; }
    const auto& steps=*transitionsPtr;
    const auto diagnose = [&]() {
        Point origin=goal_;
        if(step_<steps.size() && ordinary(steps[step_])) {
            const auto& active=steps[step_];
            result.routeForward=forward(active);
            origin=midpoint(active.sourceLow,active.sourceHigh);
            const double span=std::abs(result.routeForward.x)>0 ?
                active.sourceExtent.southEast.x-active.sourceExtent.northWest.x :
                active.sourceExtent.southEast.y-active.sourceExtent.northWest.y;
            result.routeProjection=projection(position,origin,result.routeForward);
            if(area==active.edge.source)
                routeProgress_=std::max(routeProgress_,static_cast<double>(step_)+
                    std::clamp(1.0+result.routeProjection/std::max(span,1.0),0.0,1.0));
        } else if(step_==steps.size()) {
            const double d=distance(position,goal_);
            if(d>1e-9) result.routeForward={(goal_.x-position.x)/d,(goal_.y-position.y)/d,0};
            routeProgress_=std::max(routeProgress_,static_cast<double>(step_));
        }
        result.routeProjection=projection(position,origin,result.routeForward);
        result.routeProgress=routeProgress_;
    };
    const auto setTarget = [&](Point target) {
        const auto aimed=toward(position,target,lookAhead);
        // Ordinary guidance is monotonic in the measured route direction.
        // Keep this invariant at the final assignment boundary as well as in
        // candidate selection: a transient portal miss must not turn a stale
        // source-side point into a reverse movement command.
        if (result.status!=FollowStatus::SpecialTransition &&
            projection(aimed,position,result.routeForward)<=1e-7) {
            result.target.reset();
            return false;
        }
        result.target=aimed;
        result.targetProjection=projection(*result.target,position,result.routeForward);
        return true;
    };
    diagnose();
    const auto recoverBoundary = [&](const model::NavExtent& extent) {
        if (contains(extent, position)) return false;
        const double x = std::clamp(position.x, double(extent.northWest.x), double(extent.southEast.x));
        const double y = std::clamp(position.y, double(extent.northWest.y), double(extent.southEast.y));
        // Micro NAV patches are commonly the overlap of two larger physical
        // areas. Allow the supported hull to straddle that patch by the full
        // hull-plus-query tolerance; retain the tighter correction for a
        // normal area so a genuinely off-route actor is still rejected.
        const double patchWidth=std::min(double(extent.southEast.x)-extent.northWest.x,
                                         double(extent.southEast.y)-extent.northWest.y);
        const double tolerance=patchWidth<2.0*boundaryHalfWidth ?
            supportBoundaryTolerance:boundaryHalfWidth;
        if (std::abs(x-position.x)>tolerance || std::abs(y-position.y)>tolerance)
            return false;
        const auto projected = query::projectToArea(extent,
            {static_cast<float>(x), static_cast<float>(y), 0});
        if (!finite(projected) || std::abs(projected.z-position.z)>boundaryVertical)
            return false;
        // Boundary correction may be lateral, but must never pull back along
        // an already observed forward route direction.
        if (projection(projected,position,result.routeForward)<-1e-7) return false;
        result.status = FollowStatus::Moving;
        result.boundaryRecovery = true;
        if (!setTarget(projected)) {
            result.boundaryRecovery = false;
            return false;
        }
        return true;
    };
    const auto before=step_;
    // A sample may cross several tiny ordinary patches between server frames.
    // Search only a bounded contiguous ordinary prefix, never beyond a special.
    const auto end=step_+std::min(maxObservedAdvance,steps.size()-step_);
    for(auto i=step_; i<end && ordinary(steps[i]); ++i) {
        const auto& targetExtent=steps[i].targetExtent;
        const Point targetCenter{
            (targetExtent.northWest.x+targetExtent.southEast.x)*0.5,
            (targetExtent.northWest.y+targetExtent.southEast.y)*0.5,
            position.z};
        const bool nearTarget=distance(position,targetCenter)<=boundaryHalfWidth+
            std::max(1.0, std::min(std::abs(targetExtent.southEast.x-targetExtent.northWest.x),
                                   std::abs(targetExtent.southEast.y-targetExtent.northWest.y))*0.5);
        if(steps[i].edge.target==area && (contains(targetExtent,position) || nearTarget)) {
            step_=i+1;
            break;
        }
    }
    result.step=step_; result.changed=before!=step_;
    if(result.changed) confirmedTarget_.reset();
    diagnose();
    if(step_==steps.size()) {
        if(area!=corridor_->goal() || (goalExtent && !contains(*goalExtent,position))) {
            if (area==corridor_->goal() && goalExtent && recoverBoundary(*goalExtent))
                return result;
            if (step_>0 && ordinary(steps.back()) && area==steps.back().edge.source &&
                recoverBoundary(steps.back().sourceExtent))
                return result;
            if(step_>0 && ordinary(steps.back()) && area==steps.back().edge.source &&
               contains(steps.back().sourceExtent,position)) {
                double intersection=0;
                if(!crosses(position,goal_,steps.back(),intersection)) {
                    result.status=FollowStatus::OutsideCorridor; return result;
                }
                result.status=FollowStatus::Moving;
                if (!setTarget(goal_)) {
                    result.status=FollowStatus::OutsideCorridor;
                    return result;
                }
                return result;
            }
            result.status=FollowStatus::OutsideCorridor; return result;
        }
        result.arrived=distance(position,goal_)<=goalTolerance && std::abs(position.z-goal_.z)<=18.0;
        result.status=result.arrived ? FollowStatus::Arrived : FollowStatus::Moving;
        if(!result.arrived && !setTarget(goal_)) {
            result.status=FollowStatus::OutsideCorridor;
            result.target.reset();
        }

        return result;
    }
    const auto& active=steps[step_];
    // CSBot projects movement from the nearest point on the local path, not
    // from the NAV area's cardinal edge alone.  On diagonal portals and tight
    // seams the cardinal direction can point behind the actor even though the
    // next portal is directly ahead; derive a segment direction as a bounded
    // fallback before rejecting the target.
    if (ordinary(active)) {
        const auto segmentStart=midpoint(active.sourceLow,active.sourceHigh);
        const auto segmentEnd=midpoint(active.targetLow,active.targetHigh);
        const double sx=segmentEnd.x-segmentStart.x;
        const double sy=segmentEnd.y-segmentStart.y;
        const double length=std::hypot(sx,sy);
        if (length>1e-3) {
            const Point segmentDirection{sx/length,sy/length,0};
            const double cardinalAhead=projection(segmentEnd,position,result.routeForward);
            if (cardinalAhead<=1e-7 ||
                projection(segmentEnd,position,segmentDirection)>1e-7) {
                result.routeForward=segmentDirection;
                result.routeProjection=projection(position,segmentStart,segmentDirection);
            }
        }
    }
    const bool inSource=area==active.edge.source && contains(active.sourceExtent,position);
    // One-area back drift does not rewind ownership. Guide back toward the
    // active portal through the previous ordinary patch; never cross a special.
    const bool backDrift=step_>0 && ordinary(steps[step_-1]) &&
        area==steps[step_-1].edge.source && contains(steps[step_-1].sourceExtent,position);
    if(!inSource && !backDrift) {
        if (area==active.edge.source && recoverBoundary(active.sourceExtent))
            return result;
        if (step_>0 && ordinary(steps[step_-1]) && area==steps[step_-1].edge.source &&
            recoverBoundary(steps[step_-1].sourceExtent))
            return result;
        // A bounded contiguous ordinary prefix can contain multiple micro
        // patches. Guide back to the observed area's boundary, but do not
        // advance the cursor until the measured position is actually inside.
        for (auto i=step_; i<end && ordinary(steps[i]); ++i) {
            if (steps[i].edge.target==area && recoverBoundary(steps[i].targetExtent))
                return result;
        }
        // Physical support already identified the actor's NAV area. A narrow
        // corridor may leave the center just outside the stored rectangle;
        // continue toward the active portal while preserving forward motion.
        const auto& extent=active.targetExtent;
        const Point center{
            (extent.northWest.x+extent.southEast.x)*0.5,
            (extent.northWest.y+extent.southEast.y)*0.5,
            position.z};
        bool ordinaryArea=area==active.edge.source;
        for(auto i=step_;i<end && ordinary(steps[i]);++i)
            ordinaryArea=ordinaryArea || area==steps[i].edge.target;
        if(ordinary(active) && ordinaryArea && distance(position,center)<=boundaryHalfWidth*2.0 &&
           projection(center,position,result.routeForward)>=-1e-7) {
            result.status=FollowStatus::Moving;
            if (!setTarget(center)) {
                result.status=FollowStatus::OutsideCorridor;
                result.target.reset();
                return result;
            }
            return result;
        }
        result.status=FollowStatus::OutsideCorridor;
        return result;
    }
    if(!ordinary(active)) {
        if(backDrift) {
            // Re-enter the source of the special traversal through the previous
            // ordinary portal. Returning that portal's midpoint can pull back
            // along the next leg of a bend; use the special entry only when the
            // straight approach actually crosses the ordinary portal ahead.
            const auto& previous=steps[step_-1];
            const auto entry=active.edge.external ? active.sourceLow :
                             midpoint(active.sourceLow,active.sourceHigh);
            result.routeForward=forward(previous);
            result.routeProjection=projection(position,
                midpoint(previous.sourceLow,previous.sourceHigh),result.routeForward);
            double intersection=0;
            if(projection(entry,position,result.routeForward)<=1e-7 ||
               !crosses(position,entry,previous,intersection)) {
                result.status=FollowStatus::OutsideCorridor;
                return result;
            }
            result.status=FollowStatus::Moving;
            if (!setTarget(entry)) {
                result.status=FollowStatus::OutsideCorridor;
                result.target.reset();
                return result;
            }
            return result;
        }
        result.status=FollowStatus::SpecialTransition; result.specialTransition=true;
        result.target=active.edge.external ? active.sourceLow : midpoint(active.sourceLow,active.sourceHigh);
        return result;
    }
    result.status=FollowStatus::Moving;
    // Project to the current portal, then aim a short distance into its target
    // patch. This avoids stopping exactly at the boundary between two frames.
    const auto portal=midpoint(active.targetLow,active.targetHigh);
    const auto portalStep = [&] {
        const auto axis=forward(active);
        return Point{portal.x+axis.x*5.0,portal.y+axis.y*5.0,portal.z};
    }();
    const auto& e=active.targetExtent;
    const Point center{(e.northWest.x+e.southEast.x)*0.5,
                       (e.northWest.y+e.southEast.y)*0.5,portal.z};
    std::optional<Point> best;
    auto bestThrough=step_;
    const auto reachable = [&](Point candidate, std::size_t through) {
        // A candidate after a corner may have a negative projection on the
        // current segment's axis even though it is forward along the path.
        // Keep the forward guard for the active segment, then use ordered
        // portal crossings to establish progress through subsequent segments.
        if(through==step_ && projection(candidate,position,result.routeForward)<=1e-7)
            return false;
        if(backDrift && projection(candidate,position,forward(steps[step_-1]))<=1e-7)
            return false;
        double previous=0;
        const auto begin=backDrift ? step_-1 : step_;
        for(auto j=begin;j<=through;++j)
            if(!crosses(position,candidate,steps[j],previous)) return false;
        return true;
    };
    // A zero-width portal must remain the actual aim point. Extending it
    // into the next patch makes the straight line miss a corner seam and
    // falsely reports the route as outside the corridor.
    const auto immediate=microPortal(active) ? portal :
        toward(portalStep,center,std::min(lookAhead,24.0));
    if(reachable(immediate,step_)) best=immediate;
    // A hull-sized NAV patch can collapse its portal to one point.  The
    // ordered NAV intersection is advisory in that case: a rounded/float
    // boundary can reject the exact point even though the actor is still
    // supported by the active source area.  Keep the forward point so the
    // GroundProbe performs the authoritative hull sweep.  Restrict this to
    // the active source and micro portals; ordinary portals still require
    // ordered intersections so a bend cannot be cut.
    // Narrow NAV patches can retain a non-degenerate source portal after
    // hull shrinking even though the tangent span is no longer traversable
    // by the strict cardinal intersection test.  The physical GroundProbe
    // below is the authority for passage; keep a forward portal aim so a
    // thin corridor does not become a zero-input recovery loop.  Limit this
    // to micro-transit fits and the measured source area to avoid cutting
    // ordinary bends or special transitions.
    const bool microTransit = active.sourceFit==corridor::AreaFit::MicroTransit ||
        active.targetFit==corridor::AreaFit::MicroTransit;
    const auto nearSource = [&] {
        if (contains(active.sourceExtent,position)) return true;
        // GroundProbe has already associated the measured support with the
        // source area.  Permit the actor hull to straddle a thin NAV patch
        // boundary by the same bounded tolerance used for support seams;
        // the subsequent hull sweep remains authoritative for passage.
        return position.x >= double(active.sourceExtent.northWest.x)-supportBoundaryTolerance &&
            position.x <= double(active.sourceExtent.southEast.x)+supportBoundaryTolerance &&
            position.y >= double(active.sourceExtent.northWest.y)-supportBoundaryTolerance &&
            position.y <= double(active.sourceExtent.southEast.y)+supportBoundaryTolerance;
    };
    if(!best && microTransit && area==active.edge.source &&
        nearSource() &&
        projection(portal,position,result.routeForward)>1e-7) {
        best=portal;
        bestThrough=step_;
    }
    // Only retain NAV-confirmed guidance while its original ownership and
    // current forward geometry remain valid. Physical checks remain mandatory.
    if(confirmedTarget_ && confirmedStep_==step_ && reachable(*confirmedTarget_,confirmedThrough_)) {
        best=*confirmedTarget_; bestThrough=confirmedThrough_; result.retainedTarget=true;
    }
    // Keep a previously world-validated forward aim through a transient
    // portal geometry miss. Never retain it if it would pull behind the
    // measured route direction; execution performs the physical recheck.
    if(!best && confirmedTarget_ && confirmedStep_==step_ &&
       projection(*confirmedTarget_,position,result.routeForward)>1e-7) {
        best=*confirmedTarget_;
        bestThrough=confirmedThrough_;
        result.retainedTarget=true;
    }    const auto horizon=step_+std::min(maxLookAhead,steps.size()-step_);
    for(auto i=step_; i<horizon && ordinary(steps[i]); ++i) {
        Point candidate=midpoint(steps[i].targetLow,steps[i].targetHigh);
        if(i+1==steps.size()) candidate=goal_;
        else if(!ordinary(steps[i+1])) candidate=midpoint(steps[i+1].sourceLow,steps[i+1].sourceHigh);
        else {
            const auto& next=steps[i].targetExtent;
            candidate=toward(candidate,{(next.northWest.x+next.southEast.x)*0.5,
                (next.northWest.y+next.southEast.y)*0.5,candidate.z},24.0);
        }
        const bool valid=reachable(candidate,i);
        if(valid && (!best || i >= bestThrough)) {
            best=candidate; bestThrough=i; result.retainedTarget=false;
        }
        // Only inspect a bounded prefix; farther geometry is unnecessary once
        // a reachable aim covers this frame's requested look-ahead distance.
        if(valid && distance(position,candidate)>=lookAhead) break;
    }
    if(!best && ordinary(active) && inSource) {
        // Match CSBot's FindPathPoint fallback: when a portal point is too
        // close to the actor, keep a bounded point ahead on the current path
        // so the world feeler can resolve the obstacle on the next frame.
        const double length=std::hypot(result.routeForward.x,result.routeForward.y);
        if(length>1e-3) {
            const double distanceAhead=std::min(lookAhead,30.0);
            best=Point{position.x+result.routeForward.x*distanceAhead,
                       position.y+result.routeForward.y*distanceAhead,position.z};
            bestThrough=step_;
            result.retainedTarget=true;
        }
    }
    if(!best && ordinary(active) && area==active.edge.source && nearSource()) {
        // A supported actor can sit just outside the stored rectangle while
        // still being on the same physical corridor (thin wood gaps and
        // adjacent NAV seams are common on CS maps). ZBot keeps a short
        // forward point in this case; requiring an exact ordered crossing
        // would produce OutsideCorridor and a needless RecoveryReplan.
        const double forwardLength=std::min(std::max(lookAhead,30.0),64.0);
        if(std::isfinite(forwardLength) && forwardLength>0.0 &&
           std::isfinite(result.routeForward.x) && std::isfinite(result.routeForward.y)) {
            const double axisLength=std::hypot(result.routeForward.x,result.routeForward.y);
            if(axisLength>1e-3) {
                best=Point{position.x+result.routeForward.x/axisLength*forwardLength,
                           position.y+result.routeForward.y/axisLength*forwardLength,position.z};
                bestThrough=step_;
                result.retainedTarget=true;
            }
        }
    }
    if(!best) { result.status=FollowStatus::OutsideCorridor; return result; }
    if (bestThrough>step_ && projection(*best,position,result.routeForward)<=1e-7) {
        const double dx=best->x-position.x, dy=best->y-position.y;
        const double length=std::hypot(dx,dy);
        if (length>1e-3) {
            result.routeForward={dx/length,dy/length,0};
            result.routeProjection=0;
        }
    }
    confirmedTarget_=*best; confirmedStep_=step_; confirmedThrough_=bestThrough;
        if (!setTarget(*best)) {
            result.status=FollowStatus::OutsideCorridor;
            result.target.reset();
            return result;
        }
        return result;
}

bool PathFollower::completeSpecial(std::size_t expectedStep, model::NavAreaId area,
                                   bool verified, std::uint8_t transitions) noexcept {
    const auto* active=activeTransition();
    if(!active || ordinary(*active) || !verified || expectedStep!=step_ ||
       (transitions!=1 && transitions!=2)) return false;
    const auto& steps=corridor_->transitions();
    if(transitions==1) {
        if(active->edge.target!=area) return false;
    } else {
        if(step_+1>=steps.size() || !ordinary(steps[step_+1]) ||
           steps[step_+1].edge.source!=active->edge.target || steps[step_+1].edge.target!=area) return false;
    }
    step_+=transitions;
    confirmedTarget_.reset();
    routeProgress_=std::max(routeProgress_,static_cast<double>(step_));
    return true;
}
} // namespace astrabot::nav::local
