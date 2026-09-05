// SPDX-License-Identifier: MPL-2.0
#include "nav/corridor/corridor.hpp"
#include "nav/query/route_search.hpp"
#include "route_fixture.hpp"
#include <cassert>
#include <cmath>
#include <limits>

using namespace astrabot::nav;
namespace {
constexpr corridor::Limits limits{100, 1000000, 1000};
route_test::Area square(std::uint32_t id, float x, float y) {
    return {id, {{x,y,0},{x+100,y+100,0},0,0}, {}};
}
auto graph(std::vector<route_test::Area> areas) {
    auto r=query::NavGraph::build(route_test::snapshot(areas),{100,100,1000000});
    assert(r); return *r.value;
}
auto route(const query::NavGraph& g, std::uint32_t from, std::uint32_t to) {
    auto r=query::NavRouteSearch::search(g,{{from},{to},{100,1000000},false});
    assert(r); return *r.value;
}
auto build(const query::NavGraph& g, std::uint32_t from=1, std::uint32_t to=2,
           corridor::HullClearance hull={16,16}) {
    return corridor::Corridor::build(g,route(g,from,to),hull,limits);
}
void cardinalAndSlopes() {
    const float xs[]={0,100,0,-100}, ys[]={-100,0,100,0};
    for(std::uint8_t d=0;d<4;++d) {
        auto a=square(1,0,0), b=square(2,xs[d],ys[d]);
        a.targets[d]={2}; b.targets[(d+2U)%4U]={1};
        auto g=graph({b,a}); auto r=build(*g); assert(r);
        const auto& p=r.value->transitions()[0];
        assert(p.edge.direction==d && p.requiresWorldProbe);
        const auto reverse=build(*g,2,1); assert(reverse);
        const auto& q=reverse.value->transitions()[0];
        assert(p.sourceLow.x==q.targetLow.x && p.sourceLow.y==q.targetLow.y);
        const bool vertical=d==1 || d==3;
        assert((vertical ? p.sourceHigh.y-p.sourceLow.y : p.sourceHigh.x-p.sourceLow.x)==68);
    }
    auto a=square(1,0,0), b=square(2,100,20);
    a.targets[1]={2}; b.extent.southEast.y=80;
    a.extent.northEastZ=10; a.extent.southEast.z=30;
    b.extent.northWest.z=40; b.extent.southWestZ=100;
    const auto g=graph({a,b}); const auto r=build(*g); assert(r);
    const auto& p=r.value->transitions()[0];
    assert(p.sourceLow.y==36 && p.sourceHigh.y==64);
    assert(std::abs(p.sourceLow.z-17.2)<1e-10);
    assert(std::abs(p.targetLow.z-56)<1e-10);
    assert(std::abs(p.targetHigh.z-84)<1e-10);
    assert(p.sourceLow.z!=p.targetLow.z); // discontinuity retained, no step inference
}
void invalidAndBudgets() {
    auto a=square(1,0,0), b=square(2,100,0); a.targets[1]={2};
    auto g=graph({a,b}); auto r=route(*g,1,2);
    auto good=build(*g); assert(good);
    assert(corridor::Corridor::build(*g,r,{16,16},{1,good.value->logicalBytes(),1}));
    for(auto l : {corridor::Limits{0,1000000,10}, {1,good.value->logicalBytes()-1,10}, {1,1000000,0}})
        assert(corridor::Corridor::build(*g,r,{16,16},l).error==corridor::Error::LimitExceeded);
    for(auto h : {corridor::HullClearance{-1,1}, {1,std::numeric_limits<double>::infinity()}})
        assert(build(*g,1,2,h).error==corridor::Error::InvalidHull);
    assert(build(*g,1,2,{16,50}).error==corridor::Error::InvalidPortal);
    r.status=query::NavRouteStatus::ExpansionLimit;
    assert(corridor::Corridor::build(*g,r,{16,16},limits).error==corridor::Error::InvalidRoute);
    r=route(*g,1,2); r.steps[0].edge.direction=3;
    assert(corridor::Corridor::build(*g,r,{16,16},limits).error==corridor::Error::InvalidRoute);
    r=route(*g,1,2); r.areas[1]={1};
    assert(corridor::Corridor::build(*g,r,{16,16},limits).error==corridor::Error::InvalidRoute);
    assert(build(*g,2,1).error==corridor::Error::InvalidRoute); // unreachable
    for(float x : {99.0F,101.0F}) {
        b=square(2,x,0); assert(build(*graph({a,b})).error==corridor::Error::InvalidPortal);
    }
    b=square(2,100,100); // tangent touching only
    assert(build(*graph({a,b}),1,2,{0,0}).error==corridor::Error::InvalidPortal);
    b=square(2,-100,0); // connection direction contradicts geometry
    assert(build(*graph({a,b})).error==corridor::Error::InvalidPortal);
    const auto same=build(*g,1,1); assert(same && same.value->transitions().empty());
    corridor::Cursor cursor(same.value); assert(cursor.exhausted());
    assert(cursor.target({50,50,0},1).error==corridor::Error::InvalidCursor);
}
void lookAheadAndReplay() {
    auto a=square(1,0,0), b=square(2,100,0), c=square(3,100,100), d=square(4,200,100);
    a.targets[1]={2}; b.targets[2]={3}; c.targets[1]={4};
    auto g=graph({d,b,a,c}); auto r=build(*g,1,4); assert(r);
    corridor::Cursor cursor(r.value);
    for(double jitter : {-0.01,0.0,0.01}) {
        auto t=cursor.target({50,50+jitter,0},100); assert(t);
        assert(t.value->x==100 && t.value->y>=16 && t.value->y<=84);
        auto replay=cursor.target({50,50+jitter,0},100);
        assert(replay.value->x==t.value->x && replay.value->y==t.value->y && replay.value->z==t.value->z);
        assert(cursor.index()==0);
    }
    assert(cursor.target({50,50,0},0).error==corridor::Error::InvalidCursor);
    assert(cursor.target({std::numeric_limits<double>::quiet_NaN(),0,0},1).error==corridor::Error::InvalidPosition);
    assert(cursor.target({-1,50,0},1).error==corridor::Error::InvalidPosition);
    assert(!cursor.advance(0,{2},false) && !cursor.advance(1,{2},true));
    assert(cursor.advance(0,{2},true) && !cursor.advance(0,{2},true));
    auto t=cursor.target({150,50,0},3); assert(t && t.value->y==100 && t.value->x>=116 && t.value->x<=184);
    assert(cursor.advance(1,{3},true)); assert(cursor.advance(2,{4},true)); assert(cursor.exhausted());
    assert(!cursor.advance(3,{4},true));
}
void externalOwnership() {
    auto mesh=route_test::snapshot({square(1,0,0),square(2,0,0)});
    enrichment::NavMapFingerprint fp{}; fp[0]=1;
    enrichment::NavTraversalLinkSet links{fp,{
        {1,2,3,{1},{2},{25,25,0},{75,75,0},model::NavTraversalKind::Jump,enrichment::NavLinkDirection::Forward,0},
        {1,2,4,{1},{2},{30,30,0},{70,70,0},model::NavTraversalKind::Jump,enrichment::NavLinkDirection::Forward,10}}};
    auto g=query::NavGraph::compose(mesh,fp,links,{2,2,1000000},{2,1000000}); assert(g);
    auto selected=route(**g.value,1,2); auto built=corridor::Corridor::build(**g.value,selected,{16,16},limits); assert(built);
    assert(built.value->transitions()[0].edge.external->linkId==3);
    selected.steps[0].edge.external->linkId=99;
    assert(corridor::Corridor::build(**g.value,selected,{16,16},limits).error==corridor::Error::InvalidRoute);
    links.links.clear(); g.value.reset(); mesh.reset();
    const auto t=built.value->target(0,{50,50,0},10); assert(t);
    assert(t.value->x==25 && t.value->y==25);
    assert(built.value->transitions()[0].targetLow.x==75);
}
}
int main() { cardinalAndSlopes(); invalidAndBudgets(); lookAheadAndReplay(); externalOwnership(); }
