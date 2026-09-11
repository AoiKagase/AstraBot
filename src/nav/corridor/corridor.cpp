// SPDX-License-Identifier: MPL-2.0
#include "nav/corridor/corridor.hpp"
#include "nav/local/traversal_constraints.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <new>

namespace astrabot::nav::corridor {
namespace {
using Point = query::NavQueryPoint;
bool finite(Point p) noexcept { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); }
bool same(enrichment::NavLinkPoint a, enrichment::NavLinkPoint b) noexcept {
    return a.x==b.x && a.y==b.y && a.z==b.z;
}
bool same(const query::NavDirectedEdge& a, const query::NavDirectedEdge& b) noexcept {
    if(a.source!=b.source || a.target!=b.target || a.direction!=b.direction ||
       a.traversal!=b.traversal || a.external.has_value()!=b.external.has_value()) return false;
    if(!a.external) return true;
    const auto& x=*a.external; const auto& y=*b.external;
    return x.sourceId==y.sourceId && x.generation==y.generation && x.linkId==y.linkId &&
        x.from==y.from && x.to==y.to && same(x.entry,y.entry) && same(x.exit,y.exit) &&
        x.traversal==y.traversal && x.direction==y.direction && x.additionalCost==y.additionalCost;
}
bool contains(const model::NavExtent& e, Point p) noexcept {
    return p.x>=e.northWest.x && p.x<=e.southEast.x && p.y>=e.northWest.y && p.y<=e.southEast.y;
}
// Double throughout: shrinking a float boundary must not round the hull margin away.
Point support(const model::NavExtent& e, double x, double y) noexcept {
    const double u=(x-e.northWest.x)/(double(e.southEast.x)-e.northWest.x);
    const double v=(y-e.northWest.y)/(double(e.southEast.y)-e.northWest.y);
    const double north=(1-u)*e.northWest.z+u*e.northEastZ;
    const double south=(1-u)*e.southWestZ+u*e.southEast.z;
    return {x,y,(1-v)*north+v*south};
}
bool fits(const model::NavExtent& e, HullClearance h) noexcept {
    return double(e.southEast.x)-e.northWest.x>=2*h.halfX &&
           double(e.southEast.y)-e.northWest.y>=2*h.halfY;
}
bool insideHull(const model::NavExtent& e, Point p, HullClearance h) noexcept {
    return finite(p) && p.x>=double(e.northWest.x)+h.halfX && p.x<=double(e.southEast.x)-h.halfX &&
        p.y>=double(e.northWest.y)+h.halfY && p.y<=double(e.southEast.y)-h.halfY;
}
PortalFailureReason portal(Transition& t, HullClearance hull, PortalPolicy policy) noexcept {
    t.effectiveTraversal=t.edge.traversal;
    const auto& a=t.sourceExtent; const auto& b=t.targetExtent;
    if(!a.isFinite() || !b.isFinite() ||
       a.southEast.x<=a.northWest.x || a.southEast.y<=a.northWest.y ||
       b.southEast.x<=b.northWest.x || b.southEast.y<=b.northWest.y)
        return PortalFailureReason::NoPortalSpan;
    const bool sourceFits=fits(a,hull), targetFits=fits(b,hull);
    t.sourceFit=sourceFits ? AreaFit::HullSafe : AreaFit::MicroTransit;
    t.targetFit=targetFits ? AreaFit::HullSafe : AreaFit::MicroTransit;
    if(!sourceFits && policy==PortalPolicy::Strict) return PortalFailureReason::SourceHullFit;
    if(!targetFits && policy==PortalPolicy::Strict) return PortalFailureReason::TargetHullFit;
    if(t.edge.external) {
        if((!sourceFits || !targetFits) && policy==PortalPolicy::Strict)
            return !sourceFits ? PortalFailureReason::SourceHullFit:
                                PortalFailureReason::TargetHullFit;
        const auto& e=*t.edge.external;
        const Point entry{e.entry.x,e.entry.y,e.entry.z}, exit{e.exit.x,e.exit.y,e.exit.z};
        if(!contains(a,entry) || !contains(b,exit))
            return PortalFailureReason::InvalidExternalEndpoint;
        t.sourceLow=t.sourceHigh=entry; t.targetLow=t.targetHigh=exit;
        if(t.edge.traversal==model::NavTraversalKind::Walk) {
            if(e.direction==enrichment::NavLinkDirection::Up)
                t.effectiveTraversal=model::NavTraversalKind::Jump;
            else if(e.direction==enrichment::NavLinkDirection::Down)
                t.effectiveTraversal=model::NavTraversalKind::Drop;
        }
        return PortalFailureReason::None;
    }
    const auto hints=local::constraints(t.edge.traversal,t.sourceAttributes,t.targetAttributes);
    if(t.edge.traversal!=model::NavTraversalKind::Walk || t.edge.direction>3 ||
       ((!sourceFits || !targetFits) && (!hints || hints.kind==model::NavTraversalKind::Crouch)))
        return PortalFailureReason::UnsupportedTraversal;
    if(hints) t.effectiveTraversal=hints.kind;
    const auto d=t.edge.direction;
    double boundary=0, opposite=0;
    switch(d) {
    case 0: boundary=a.northWest.y; opposite=b.southEast.y; break;
    case 1: boundary=a.southEast.x; opposite=b.northWest.x; break;
    case 2: boundary=a.southEast.y; opposite=b.northWest.y; break;
    case 3: boundary=a.northWest.x; opposite=b.southEast.x; break;
    default: return PortalFailureReason::UnsupportedTraversal;
    }
    const double gap=(opposite-boundary)*((d==1 || d==2) ? 1.0:-1.0);
    if(gap<0 || gap>32 || (gap!=0 && policy==PortalPolicy::Strict))
        return PortalFailureReason::BoundaryMismatch;
    const bool vertical=d==1 || d==3;
    const double margin=vertical ? hull.halfY : hull.halfX;
    const double sourceLow=vertical ? a.northWest.y : a.northWest.x;
    const double sourceHigh=vertical ? a.southEast.y : a.southEast.x;
    const double targetLow=vertical ? b.northWest.y : b.northWest.x;
    const double targetHigh=vertical ? b.southEast.y : b.southEast.x;
    double low=std::max(sourceLow+(sourceFits ? margin:0.0),
                        targetLow+(targetFits ? margin:0.0));
    double high=std::min(sourceHigh-(sourceFits ? margin:0.0),
                         targetHigh-(targetFits ? margin:0.0));
    if(!(low<high)) {
        // A ReGameDLL/ZBot NAV patch can be a zero-width boundary or can
        // become point-like after the actor hull is projected out. That is
        // not, by itself, a solid wall. Preserve the measured overlap as a
        // micro portal and let the runtime hull sweep/support probe decide
        // whether the actor can actually cross it.
        const double rawLow=std::max(sourceLow,targetLow);
        const double rawHigh=std::min(sourceHigh,targetHigh);
        if(policy!=PortalPolicy::AllowMicroTransit || rawLow>rawHigh)
            return PortalFailureReason::NoPortalSpan;
        low=high=(rawLow+rawHigh)*0.5;
    }
    const double x0=vertical ? boundary:low, y0=vertical ? low:boundary;
    const double x1=vertical ? boundary:high, y1=vertical ? high:boundary;
    t.sourceLow=support(a,x0,y0); t.sourceHigh=support(a,x1,y1);
    t.targetLow=support(b,vertical ? opposite:low,vertical ? low:opposite);
    t.targetHigh=support(b,vertical ? opposite:high,vertical ? high:opposite);
    const double lowFall=t.sourceLow.z-t.targetLow.z, highFall=t.sourceHigh.z-t.targetHigh.z;
    const double lowRise=-lowFall, highRise=-highFall;
    if(gap==0 && (lowRise>18 || highRise>18)) {
        // GoldSrc's ordinary step is about 18 units. A larger measured
        // upward transition must use the existing, observed jump primitive;
        // treating it as Walk makes GroundProbe stop at the riser forever.
        if(!hints || (hints.kind!=model::NavTraversalKind::Walk &&
                      hints.kind!=model::NavTraversalKind::Jump) || hints.noJump ||
           lowRise>44 || highRise>44)
            return PortalFailureReason::UnsupportedTraversal;
        t.effectiveTraversal=model::NavTraversalKind::Jump;
    }
    if(gap!=0 || (policy==PortalPolicy::AllowMicroTransit && (lowFall>18 || highFall>18))) {
        // The source NAV patch need not contain the hull: its measured support
        // is mandatory in updateDrop. Landing still requires a hull-safe target.
        if(!hints || hints.kind!=model::NavTraversalKind::Walk || !targetFits ||
           lowFall<=18 || highFall<=18 || lowFall>128 || highFall>128)
            return PortalFailureReason::UnsupportedTraversal;
        t.effectiveTraversal=model::NavTraversalKind::Drop;
    }
    return PortalFailureReason::None;
}
Point project(const Transition& t, Point p) noexcept {
    if(t.edge.external) return t.targetLow;
    const auto a=t.sourceLow, b=t.sourceHigh;
    const bool vertical=a.x==b.x;
    const double denominator=vertical ? double(b.y)-a.y:double(b.x)-a.x;
    if(std::abs(denominator)<=0.000001) return a;
    const double f=vertical ? std::clamp((p.y-a.y)/denominator,0.0,1.0) :
                              std::clamp((p.x-a.x)/denominator,0.0,1.0);
    return {a.x+(b.x-a.x)*f,a.y+(b.y-a.y)*f,a.z+(b.z-a.z)*f};
}
}
BuildResult Corridor::build(const query::NavGraph& graph, const query::NavRouteResult& route,
                            HullClearance hull, Limits limits, PortalPolicy policy) noexcept {
    if(!std::isfinite(hull.halfX) || !std::isfinite(hull.halfY) || hull.halfX<0 || hull.halfY<0)
        return {{},Error::InvalidHull,0,PortalFailureReason::None};
    const auto count=route.steps.size();
    if(route.status!=query::NavRouteStatus::Complete || route.areas.empty() ||
       count!=route.areas.size()-1) return {{},Error::InvalidRoute,0,PortalFailureReason::None};
    if(count>limits.maxTransitions || limits.maxBytes<sizeof(Corridor) ||
       count>(limits.maxBytes-sizeof(Corridor))/sizeof(Transition)) return {{},Error::LimitExceeded,0,PortalFailureReason::None};
    if(!graph.find(route.areas.front()) || !graph.find(route.areas.back()))
        return {{},Error::InvalidRoute,0,PortalFailureReason::None};
    const auto& goal=graph.area(*graph.find(route.areas.back()));
    if(!goal.extent.isFinite() || goal.extent.southEast.x<=goal.extent.northWest.x ||
       goal.extent.southEast.y<=goal.extent.northWest.y || !fits(goal.extent,hull))
        return {{},Error::InvalidGoalArea,0,PortalFailureReason::InvalidGoalArea};
    std::size_t index=0, checks=0;
    try {
        std::shared_ptr<Corridor> result(new Corridor);
        result->transitions_.reserve(count);
        result->start_=route.areas.front(); result->goal_=route.areas.back();
        result->hull_=hull;
        result->startAttributes_=graph.area(*graph.find(result->start_)).attributes;
        result->logicalBytes_=sizeof(Corridor)+count*sizeof(Transition);
        for(;index<count;++index) {
            const auto& edge=route.steps[index].edge;
            const auto from=graph.find(edge.source), to=graph.find(edge.target);
            if(!from || !to || edge.source!=route.areas[index] || edge.target!=route.areas[index+1])
                return {{},Error::InvalidRoute,index,PortalFailureReason::None};
            bool found=false;
            for(auto e=graph.edgeBegin(*from);e<graph.edgeEnd(*from);++e) {
                if(checks==limits.maxEdgeChecks) return {{},Error::LimitExceeded,index,PortalFailureReason::None};
                ++checks;
                if(same(edge,graph.edge(e))) { found=true; break; }
            }
            if(!found) return {{},Error::InvalidRoute,index,PortalFailureReason::None};
            Transition transition;
            transition.edge=edge;
            transition.sourceExtent=graph.area(*from).extent; transition.targetExtent=graph.area(*to).extent;
            transition.sourceAttributes=graph.area(*from).attributes;
            transition.targetAttributes=graph.area(*to).attributes;
            const auto portalReason=portal(transition,hull,policy);
            if(portalReason!=PortalFailureReason::None)
                return {{},Error::InvalidPortal,index,portalReason};
            result->transitions_.push_back(std::move(transition));
        }
        return {std::move(result),Error::None,0,PortalFailureReason::None};
    } catch(const std::bad_alloc&) { return {{},Error::AllocationFailure,index,PortalFailureReason::None}; }
      catch(...) { return {{},Error::LimitExceeded,index,PortalFailureReason::None}; }
}
TargetResult Corridor::target(std::size_t cursor, Point position, std::size_t lookAhead) const noexcept {
    if(cursor>=transitions_.size() || lookAhead==0) return {{},Error::InvalidCursor};
    const auto& current=transitions_[cursor];
    const bool inSource=finite(position) && contains(current.sourceExtent,position);
    if(!inSource)
        return {{},Error::InvalidPosition};
    const auto end=cursor+std::min(lookAhead,transitions_.size()-cursor);
    auto stop=cursor;
    while(stop+1<end && !transitions_[stop].edge.external && !transitions_[stop+1].edge.external &&
          transitions_[stop].effectiveTraversal==model::NavTraversalKind::Walk &&
          transitions_[stop+1].effectiveTraversal==model::NavTraversalKind::Walk) ++stop;
    auto aim=project(transitions_[stop],position);
    while(stop>cursor) aim=project(transitions_[--stop],aim);
    return {aim,Error::None};
}
bool Cursor::advance(std::size_t expected, model::NavAreaId area, bool supportVerified) noexcept {
    if(!corridor_ || index_>=corridor_->transitions().size() || expected!=index_ || !supportVerified ||
       area!=corridor_->transitions()[index_].edge.target) return false;
    ++index_; return true;
}
TargetResult Cursor::target(Point p, std::size_t lookAhead) const noexcept {
    return corridor_ ? corridor_->target(index_,p,lookAhead) : TargetResult{{},Error::InvalidCursor};
}
} // namespace astrabot::nav::corridor
