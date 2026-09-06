// SPDX-License-Identifier: MPL-2.0
#include "adapter/cstrike/nav/ladder_probe.hpp"
#include "adapter/cstrike/nav/ladder_discovery.hpp"
#include "nav/query/graph.hpp"
#include "../nav/route_fixture.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#ifdef ASTRABOT_LADDER_HOST_TESTS
#include "adapter/cstrike/nav/console.hpp"
#include <sstream>
#endif
using namespace astrabot;
using namespace astrabot::adapter::cstrike;
namespace {
using V=nav::model::NavVector3;
edict_t ladderEntity{},worldEntity{};
core::MapGeneration activeMap{1};
unsigned traces{},faultAt{};
int fault{};
#ifdef ASTRABOT_LADDER_HOST_TESTS
NavConsole* publishing{};
#endif
struct Box { V low,high; } floors[2];
core::MapGeneration mapNow(const void*) noexcept { return activeMap; }
edict_t* entityAt(int n) { return n==1 ? &ladderEntity:n==0 ? &worldEntity:nullptr; }
int indexOf(const edict_t* e) { return e==&ladderEntity ? 1:0; }
const char* classname(int n) { return n==1 ? "func_ladder":n==2 ? "*1":nullptr; }
// Independent slab intersection, including Minkowski expansion for engine hull1.
void boxTrace(const float* a,const float* b,Box box,int hull,edict_t* hit,TraceResult& out) {
    const float lo[]{box.low.x-(hull ? 16:0),box.low.y-(hull ? 16:0),box.low.z-(hull ? 36:0)};
    const float hi[]{box.high.x+(hull ? 16:0),box.high.y+(hull ? 16:0),box.high.z+(hull ? 36:0)};
    bool inside=true; double enter=0,leave=1; Vector normal{};
    for(int axis=0;axis<3;++axis) {
        inside=inside && a[axis]>lo[axis] && a[axis]<hi[axis];
        const double delta=double(b[axis])-a[axis];
        if(delta==0) { if(a[axis]<lo[axis] || a[axis]>hi[axis]) return; continue; }
        double first=(lo[axis]-a[axis])/delta,last=(hi[axis]-a[axis])/delta;
        float sign=-1; if(first>last) { std::swap(first,last); sign=1; }
        if(first>enter) { enter=first; normal=Vector(0,0,0); normal[axis]=sign; }
        leave=(std::min)(leave,last); if(enter>leave) return;
    }
    if(inside) { out.fStartSolid=out.fAllSolid=1; out.flFraction=0; out.vecEndPos=Vector(a[0],a[1],a[2]); out.pHit=hit; return; }
    if(enter<0 || enter>out.flFraction || enter>1 || leave<0) return;
    out.flFraction=static_cast<float>(enter); out.pHit=hit; out.vecPlaneNormal=normal;
    out.vecEndPos=Vector(a[0]+(b[0]-a[0])*out.flFraction,a[1]+(b[1]-a[1])*out.flFraction,a[2]+(b[2]-a[2])*out.flFraction);
}
void begin(const float* b,TraceResult* t) { *t={}; t->flFraction=1; t->vecEndPos=Vector(b[0],b[1],b[2]); ++traces; }
void inject(const float* a,const float* b,TraceResult* t) {
    if(traces!=faultAt) return;
    if(fault==1) activeMap={2};
    if(fault==2) ++ladderEntity.serialnumber;
    if(fault==3) t->flFraction=(std::numeric_limits<float>::quiet_NaN)();
    if(fault==4) { t->flFraction=1; t->vecEndPos=Vector(b[0],b[1],b[2]); t->pHit=nullptr; t->fStartSolid=t->fAllSolid=0; }
    if(fault==5) { t->fStartSolid=1; t->flFraction=0; t->vecEndPos=Vector(a[0],a[1],a[2]); }
    if(fault==6) t->vecPlaneNormal=Vector(0,0,1);
    if(fault==7) t->vecEndPos.x+=2;
    if(fault==8) ladderEntity.v.absmax.z+=1;
    if(fault==9) ++ladderEntity.v.modelindex;
    if(fault==10) t->pHit=&ladderEntity;
#ifdef ASTRABOT_LADDER_HOST_TESTS
    if(fault==11 && publishing) publishing->invalidate(nav::runtime::SessionReason::Cancelled);
#endif
}
void traceModel(const float* a,const float* b,int hull,edict_t* e,TraceResult* t) {
    assert(e==&ladderEntity && (hull==0 || hull==1)); begin(b,t);
    boxTrace(a,b,{{0,0,0},{8,64,128}},hull,e,*t); inject(a,b,t);
}
void traceHull(const float* a,const float* b,int ignore,int hull,edict_t* ignored,TraceResult* t) {
    assert(ignore==0 && hull==1 && !ignored); begin(b,t);
    for(const auto& floor:floors) boxTrace(a,b,floor,1,&worldEntity,*t);
    inject(a,b,t);
}
std::shared_ptr<const nav::query::NavSpatialIndex> setup(LadderFace face,LadderExit exit) {
    ladderEntity={}; ladderEntity.serialnumber=7; ladderEntity.v.classname=1;
    ladderEntity.v.model=2; ladderEntity.v.modelindex=1; ladderEntity.v.skin=CONTENTS_LADDER;
    ladderEntity.v.absmin=Vector(0,0,0); ladderEntity.v.absmax=Vector(8,64,128);
    traces=0; fault=faultAt=0; activeMap={1};
    const bool x=face==LadderFace::MinX || face==LadderFace::MaxX;
    const float sign=face==LadderFace::MinX || face==LadderFace::MinY ? -1.0f:1.0f;
    const float edge=sign<0 ? 0:x ? 8.0f:64.0f;
    const auto region=[&](float low,float high,float z) {
        const float a=edge+sign*low,b=edge+sign*high;
        return x ? Box{{(std::min)(a,b),-96,z-8},{(std::max)(a,b),160,z}}:
            Box{{-96,(std::min)(a,b),z-8},{160,(std::max)(a,b),z}};
    };
    floors[0]=region(20,100,0);
    floors[1]=exit==LadderExit::AcrossTop ? region(-100,-20,128):region(32,100,128);
    std::vector<route_test::Area> areas;
    for(unsigned i=0;i<2;++i) {
        const auto& f=floors[i]; const float z=f.high.z;
        areas.push_back({i+1,{{f.low.x,f.low.y,z},{f.high.x,f.high.y,z},z,z}});
    }
    auto index=nav::query::NavSpatialIndex::build(route_test::snapshot(areas),{100,199,100000}); assert(index); return *index.value;
}
enginefuncs_t engine() {
    enginefuncs_t e{}; e.pfnPEntityOfEntIndex=entityAt; e.pfnIndexOfEdict=indexOf; e.pfnSzFromIndex=classname;
    e.pfnTraceModel=traceModel; e.pfnTraceHull=traceHull; return e;
}
constexpr LadderCandidate candidate{(std::uint64_t{7}<<32)|1,{0,0,0},{8,64,128}};
}
void testLadderProbe() {
    for(auto face:{LadderFace::MinX,LadderFace::MaxX,LadderFace::MinY,LadderFace::MaxY})
        for(auto exit:{LadderExit::AcrossTop,LadderExit::SameFace}) {
            const auto index=setup(face,exit); auto e=engine();
            const auto r=inspectLadderPassage({&e,nullptr,mapNow},{1},candidate,face,exit,*index,{1},2);
            assert(r && r.queries==12 && traces==12 && r.passage->bottom.area.value==1 && r.passage->top.area.value==2);
            assert(r.passage->entityId==candidate.entityId && r.passage->map==activeMap);
            assert(r.passage->bottom.origin.z==36 && r.passage->top.origin.z==164);
            assert(r.passage->face==face && r.passage->exit==exit);
        }
    for(unsigned budget=0;budget<12;++budget) {
        const auto index=setup(LadderFace::MinX,LadderExit::AcrossTop); auto e=engine();
        const auto r=inspectLadderPassage({&e,nullptr,mapNow},{1},candidate,LadderFace::MinX,LadderExit::AcrossTop,*index,{1},2,budget);
        assert(!r && !r.passage && r.reason==LadderProbeReason::BudgetExceeded && traces==budget && r.queries==budget);
    }
    for(int mode=1;mode<=9;++mode) {
        const auto index=setup(LadderFace::MinX,LadderExit::AcrossTop); auto e=engine(); fault=mode; faultAt=2;
        const auto r=inspectLadderPassage({&e,nullptr,mapNow},{1},candidate,LadderFace::MinX,LadderExit::AcrossTop,*index,{1},2);
        assert(!r && !r.passage && traces==2);
        const auto expected=mode==1 ? LadderProbeReason::StaleMap:mode==2 || mode==8 || mode==9 ? LadderProbeReason::StaleEntity:
            mode==3 || mode==7 ? LadderProbeReason::InvalidTrace:mode==5 ? LadderProbeReason::Blocked:LadderProbeReason::NoFace;
        assert(r.reason==expected);
    }
    for(unsigned at:{4U,6U,8U,9U,10U,11U,12U}) {
        const auto index=setup(LadderFace::MinX,LadderExit::AcrossTop); auto e=engine(); faultAt=at; fault=at<10 ? 4:5;
        const auto r=inspectLadderPassage({&e,nullptr,mapNow},{1},candidate,LadderFace::MinX,LadderExit::AcrossTop,*index,{1},2);
        assert(!r && !r.passage && traces==at);
        assert(r.reason==(at<8 ? LadderProbeReason::NoSupport:at<10 ? LadderProbeReason::NoFace:LadderProbeReason::Blocked));
    }
    for(unsigned at=1;at<=12;++at) for(int mode:{1,2}) {
        const auto index=setup(LadderFace::MinX,LadderExit::AcrossTop); auto e=engine(); faultAt=at; fault=mode;
        const auto r=inspectLadderPassage({&e,nullptr,mapNow},{1},candidate,LadderFace::MinX,LadderExit::AcrossTop,*index,{1},2);
        assert(!r && !r.passage && traces==at);
        assert(r.reason==(mode==1 ? LadderProbeReason::StaleMap:LadderProbeReason::StaleEntity));
    }
    for(int mode=0;mode<4;++mode) {
        const auto index=setup(LadderFace::MinX,LadderExit::AcrossTop); auto e=engine();
        if(mode==0) ladderEntity.v.modelindex=0;
        if(mode==1) ladderEntity.v.modelindex=512;
        if(mode==2) ladderEntity.v.model=1;
        if(mode==3) ladderEntity.v.skin=0;
        const auto r=inspectLadderPassage({&e,nullptr,mapNow},{1},candidate,LadderFace::MinX,LadderExit::AcrossTop,*index,{1},2);
        assert(!r && r.reason==LadderProbeReason::StaleEntity && traces==0);
    }
    const auto index=setup(LadderFace::MinX,LadderExit::AcrossTop); auto e=engine();
    fault=10; faultAt=4;
    auto r=inspectLadderPassage({&e,nullptr,mapNow},{1},candidate,LadderFace::MinX,LadderExit::AcrossTop,*index,{1},2);
    assert(!r && r.reason==LadderProbeReason::NoSupport);
    traces=0; fault=faultAt=0;
    r=inspectLadderPassage({&e,nullptr,mapNow},{1},candidate,LadderFace::MinX,LadderExit::AcrossTop,*index,{2},2);
    assert(!r && r.reason==LadderProbeReason::StaleMap && traces==0);
    const nav::query::NavSpatialIndex empty;
    r=inspectLadderPassage({&e,nullptr,mapNow},{1},candidate,LadderFace::MinX,LadderExit::AcrossTop,empty,{1},2);
    assert(!r && r.reason==LadderProbeReason::NoArea && traces==4);
    traces=0;
    r=inspectLadderPassage({&e,nullptr,mapNow},{1},candidate,LadderFace::MinX,LadderExit::AcrossTop,*index,{1},2,13);
    assert(!r && r.reason==LadderProbeReason::InvalidInput && traces==0);
    e.pfnTraceModel=nullptr;
    r=inspectLadderPassage({&e,nullptr,mapNow},{1},candidate,LadderFace::MinX,LadderExit::AcrossTop,*index,{1},2);
    assert(!r && r.reason==LadderProbeReason::Unavailable && traces==0);
}
void testLadderDiscovery() {
    const auto index=setup(LadderFace::MinX,LadderExit::AcrossTop); auto e=engine();
    nav::enrichment::NavMapFingerprint fingerprint{}; fingerprint[0]=0x7a;
    auto batch=discoverLadderLinks({&e,nullptr,mapNow},{1},*index,{1},fingerprint,9,2);
    assert(batch && batch.candidates==1 && batch.queries==traces && !batch.value->passages.empty());
    assert(batch.value->links.fingerprint==fingerprint && batch.value->generation==9);
    std::vector<route_test::Area> areas;
    for(unsigned i=0;i<2;++i) {
        const auto& f=floors[i]; const float z=f.high.z;
        areas.push_back({i+1,{{f.low.x,f.low.y,z},{f.high.x,f.high.y,z},z,z}});
    }
    const auto mesh=route_test::snapshot(areas);
    const auto graph=nav::query::NavGraph::compose(mesh,fingerprint,batch.value->links,{100,2048,1000000},{2048,1000000});
    assert(graph && (*graph.value)->edgeCount()==batch.value->passages.size()*2);
    for(std::size_t i=0;i<batch.value->passages.size();++i) {
        const auto& up=batch.value->links.links[2*i]; const auto& down=batch.value->links.links[2*i+1];
        assert(up.from==down.to && up.to==down.from && up.linkId+1==down.linkId);
        assert(up.sourceId==ladderSourceId && up.generation==9 && up.entry.z==0 && up.exit.z==128);
        assert(up.direction==nav::enrichment::NavLinkDirection::Up && down.direction==nav::enrichment::NavLinkDirection::Down);
        assert(ladderPassageCurrent({&e,nullptr,mapNow},batch.value->passages[i],2));
        const auto before=traces;
        const auto a=bindLadderPlan({&e,nullptr,mapNow},{1},fingerprint,*batch.value,up,2);
        const auto b=bindLadderPlan({&e,nullptr,mapNow},{1},fingerprint,*batch.value,down,2);
        assert(a && b && traces==before);
        assert(a.value->plan.start==b.value->plan.end && a.value->plan.end==b.value->plan.start);
        assert(a.value->plan.mount==b.value->plan.dismount && a.value->plan.dismount==b.value->plan.mount);
        assert(a.value->plan.normal==b.value->plan.normal && a.value->plan.link.linkId==up.linkId);
        assert(a.value->passage.entityId==candidate.entityId);
    }
    for(int mode=0;mode<9;++mode) {
        auto selected=batch.value->links.links.front();
        if(mode==0) ++selected.sourceId;
        if(mode==1) ++selected.generation;
        if(mode==2) selected.linkId=999999;
        if(mode==3) selected.entry.x+=1;
        if(mode==4) selected.exit.z+=1;
        if(mode==5) selected.from={999};
        if(mode==6) selected.direction=nav::enrichment::NavLinkDirection::Down;
        if(mode==7) selected.traversal=nav::model::NavTraversalKind::Walk;
        if(mode==8) selected.additionalCost=1;
        const auto r=bindLadderPlan({&e,nullptr,mapNow},{1},fingerprint,*batch.value,selected,2);
        assert(!r && !r.value);
        assert(r.reason==(mode<2 ? LadderBindingReason::WrongPublication:
            mode==2 ? LadderBindingReason::MissingLink:LadderBindingReason::ChangedLink));
    }
    auto damaged=*batch.value; damaged.links.links.pop_back();
    const auto selected=batch.value->links.links.front();
    assert(bindLadderPlan({&e,nullptr,mapNow},{1},fingerprint,damaged,selected,2).reason==LadderBindingReason::InvalidInput);
    damaged=*batch.value; damaged.links.links[1]=selected;
    assert(bindLadderPlan({&e,nullptr,mapNow},{1},fingerprint,damaged,selected,2).reason==LadderBindingReason::ChangedLink);
    damaged=*batch.value; damaged.passages[0].bottom.origin.x+=1;
    assert(bindLadderPlan({&e,nullptr,mapNow},{1},fingerprint,damaged,selected,2).reason==LadderBindingReason::ChangedLink);
    assert(bindLadderPlan({&e,nullptr,mapNow},{2},fingerprint,*batch.value,selected,2).reason==LadderBindingReason::WrongPublication);
    auto other=fingerprint; ++other[0];
    assert(bindLadderPlan({&e,nullptr,mapNow},{1},other,*batch.value,selected,2).reason==LadderBindingReason::WrongPublication);
    assert(!nav::query::NavGraph::compose(mesh,other,batch.value->links,{100,2048,1000000},{2048,1000000}));
    const auto old=batch.value;
    ++ladderEntity.serialnumber;
    assert(bindLadderPlan({&e,nullptr,mapNow},{1},fingerprint,*old,selected,2).reason==LadderBindingReason::StaleWorld);
    assert(!ladderPassageCurrent({&e,nullptr,mapNow},old->passages[0],2));
    assert(old->passages[0].entityId==candidate.entityId);
    for(unsigned budget:{0U,1U,11U}) {
        (void)setup(LadderFace::MinX,LadderExit::AcrossTop);
        batch=discoverLadderLinks({&e,nullptr,mapNow},{1},*index,{1},fingerprint,9,2,{budget,1024});
        assert(!batch && !batch.value && batch.reason==LadderDiscoveryReason::BudgetExceeded && traces<=budget);
    }
    (void)setup(LadderFace::MinX,LadderExit::AcrossTop);
    batch=discoverLadderLinks({&e,nullptr,mapNow},{1},*index,{1},fingerprint,9,2,{12288,0});
    assert(!batch && batch.reason==LadderDiscoveryReason::BudgetExceeded);
    const nav::query::NavSpatialIndex empty;
    batch=discoverLadderLinks({&e,nullptr,mapNow},{1},empty,{1},fingerprint,9,2);
    assert(!batch && batch.reason==LadderDiscoveryReason::UnlinkedCandidate && !batch.value);
    (void)setup(LadderFace::MinX,LadderExit::AcrossTop); fault=1; faultAt=12;
    batch=discoverLadderLinks({&e,nullptr,mapNow},{1},*index,{1},fingerprint,9,2);
    assert(!batch && !batch.value);
    (void)setup(LadderFace::MinX,LadderExit::AcrossTop); ladderEntity.free=1;
    batch=discoverLadderLinks({&e,nullptr,mapNow},{1},*index,{1},fingerprint,10,2,{0,0});
    assert(batch && batch.value->passages.empty() && batch.value->links.links.empty() && batch.queries==0);
}
#ifdef ASTRABOT_LADDER_HOST_TESTS
void testLadderPublication() {
    (void)setup(LadderFace::MinX,LadderExit::AcrossTop); auto e=engine();
    std::vector<route_test::Area> areas;
    for(unsigned i=0;i<2;++i) {
        const auto& f=floors[i]; const float z=f.high.z;
        areas.push_back({i+1,{{f.low.x,f.low.y,z},{f.high.x,f.high.y,z},z,z}});
    }
    const auto mesh=route_test::snapshot(areas);
    NavConsole console; console.configure(&e,nullptr,nullptr);
    assert(console.publish({1},mesh).isNone());
    const LadderWorld world{&e,nullptr,mapNow};
    std::istringstream bsp("abc");
    assert(console.publishLadders(bsp,world,{1},2) && console.ladders());
    const auto first=*console.ladders();
    assert(!first.links.links.empty() && first.links.fingerprint[0]==0xba);
    std::istringstream second("abc");
    assert(console.publishLadders(second,world,{1},2));
    assert(console.ladders()->generation>first.generation && first.links.fingerprint==console.ladders()->links.fingerprint);
    std::istringstream broken("abc"); broken.setstate(std::ios::badbit);
    assert(!console.publishLadders(broken,world,{1},2) && !console.ladders());
    (void)setup(LadderFace::MinX,LadderExit::AcrossTop); fault=11; faultAt=12; publishing=&console;
    std::istringstream stale("abc");
    assert(!console.publishLadders(stale,world,{1},2) && !console.ladders());
    publishing=nullptr; fault=faultAt=0;
    std::istringstream retired("abc");
    assert(!console.publishLadders(retired,world,{1},2)); // Deferred invalidation retired NAV too.
    assert(first.links.links.front().generation==first.generation); // Old owned evidence remains immutable.
}
#endif
