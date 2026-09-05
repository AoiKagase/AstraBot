// SPDX-License-Identifier: MPL-2.0
#include "nav/corridor/corridor.hpp"
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
bool portal(Transition& t, HullClearance hull) noexcept {
    const auto& a=t.sourceExtent; const auto& b=t.targetExtent;
    if(!fits(a,hull) || !fits(b,hull)) return false;
    if(t.edge.external) {
        const auto& e=*t.edge.external;
        const Point entry{e.entry.x,e.entry.y,e.entry.z}, exit{e.exit.x,e.exit.y,e.exit.z};
        if(!insideHull(a,entry,hull) || !insideHull(b,exit,hull)) return false;
        t.sourceLow=t.sourceHigh=entry; t.targetLow=t.targetHigh=exit;
        return true;
    }
    if(t.edge.traversal!=model::NavTraversalKind::Walk || t.edge.direction>3) return false;
    const auto d=t.edge.direction;
    double boundary=0, opposite=0;
    switch(d) {
    case 0: boundary=a.northWest.y; opposite=b.southEast.y; break;
    case 1: boundary=a.southEast.x; opposite=b.northWest.x; break;
    case 2: boundary=a.southEast.y; opposite=b.northWest.y; break;
    case 3: boundary=a.northWest.x; opposite=b.southEast.x; break;
    default: return false;
    }
    // No epsilon bridging of disconnected NAV boundaries.
    if(boundary!=opposite) return false;
    const bool vertical=d==1 || d==3;
    const double low=vertical ? std::max(a.northWest.y,b.northWest.y)+hull.halfY :
                                std::max(a.northWest.x,b.northWest.x)+hull.halfX;
    const double high=vertical ? std::min(a.southEast.y,b.southEast.y)-hull.halfY :
                                 std::min(a.southEast.x,b.southEast.x)-hull.halfX;
    if(!(low<high)) return false;
    const double x0=vertical ? boundary:low, y0=vertical ? low:boundary;
    const double x1=vertical ? boundary:high, y1=vertical ? high:boundary;
    t.sourceLow=support(a,x0,y0); t.sourceHigh=support(a,x1,y1);
    t.targetLow=support(b,x0,y0); t.targetHigh=support(b,x1,y1);
    return true;
}
Point project(const Transition& t, Point p) noexcept {
    if(t.edge.external) return t.sourceLow;
    const auto a=t.sourceLow, b=t.sourceHigh;
    const bool vertical=a.x==b.x;
    const double f=vertical ? std::clamp((p.y-a.y)/(b.y-a.y),0.0,1.0) :
                              std::clamp((p.x-a.x)/(b.x-a.x),0.0,1.0);
    return {a.x+(b.x-a.x)*f,a.y+(b.y-a.y)*f,a.z+(b.z-a.z)*f};
}
}
BuildResult Corridor::build(const query::NavGraph& graph, const query::NavRouteResult& route,
                            HullClearance hull, Limits limits) noexcept {
    if(!std::isfinite(hull.halfX) || !std::isfinite(hull.halfY) || hull.halfX<0 || hull.halfY<0)
        return {{},Error::InvalidHull,0};
    const auto count=route.steps.size();
    if(route.status!=query::NavRouteStatus::Complete || route.areas.empty() ||
       count!=route.areas.size()-1) return {{},Error::InvalidRoute,0};
    if(count>limits.maxTransitions || limits.maxBytes<sizeof(Corridor) ||
       count>(limits.maxBytes-sizeof(Corridor))/sizeof(Transition)) return {{},Error::LimitExceeded,0};
    if(!graph.find(route.areas.front())) return {{},Error::InvalidRoute,0};
    std::size_t index=0, checks=0;
    try {
        std::shared_ptr<Corridor> result(new Corridor);
        result->transitions_.reserve(count);
        result->start_=route.areas.front(); result->goal_=route.areas.back();
        result->logicalBytes_=sizeof(Corridor)+count*sizeof(Transition);
        for(;index<count;++index) {
            const auto& edge=route.steps[index].edge;
            const auto from=graph.find(edge.source), to=graph.find(edge.target);
            if(!from || !to || edge.source!=route.areas[index] || edge.target!=route.areas[index+1])
                return {{},Error::InvalidRoute,index};
            bool found=false;
            for(auto e=graph.edgeBegin(*from);e<graph.edgeEnd(*from);++e) {
                if(checks==limits.maxEdgeChecks) return {{},Error::LimitExceeded,index};
                ++checks;
                if(same(edge,graph.edge(e))) { found=true; break; }
            }
            if(!found) return {{},Error::InvalidRoute,index};
            Transition transition;
            transition.edge=edge;
            transition.sourceExtent=graph.area(*from).extent; transition.targetExtent=graph.area(*to).extent;
            transition.sourceAttributes=graph.area(*from).attributes;
            transition.targetAttributes=graph.area(*to).attributes;
            if(!portal(transition,hull)) return {{},Error::InvalidPortal,index};
            result->transitions_.push_back(std::move(transition));
        }
        return {std::move(result),Error::None,0};
    } catch(const std::bad_alloc&) { return {{},Error::AllocationFailure,index}; }
      catch(...) { return {{},Error::LimitExceeded,index}; }
}
TargetResult Corridor::target(std::size_t cursor, Point position, std::size_t lookAhead) const noexcept {
    if(cursor>=transitions_.size() || lookAhead==0) return {{},Error::InvalidCursor};
    if(!finite(position) || !contains(transitions_[cursor].sourceExtent,position))
        return {{},Error::InvalidPosition};
    const auto end=cursor+std::min(lookAhead,transitions_.size()-cursor);
    auto stop=cursor;
    while(stop+1<end && !transitions_[stop].edge.external) ++stop;
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
