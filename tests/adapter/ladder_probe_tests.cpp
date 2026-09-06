// SPDX-License-Identifier: MPL-2.0
#include "adapter/cstrike/nav/ladder_probe.hpp"
#include "adapter/cstrike/nav/ladder_discovery.hpp"
#include "adapter/cstrike/nav/ladder_frame.hpp"
#include "nav/query/graph.hpp"
#include "nav/query/route_search.hpp"
#include "nav/corridor/corridor.hpp"
#include "../nav/route_fixture.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#ifdef ASTRABOT_LADDER_HOST_TESTS
#include "adapter/cstrike/nav/console.hpp"
#include <sstream>
#endif
using namespace astrabot;
using namespace astrabot::adapter::cstrike;
namespace {
using V=nav::model::NavVector3;
edict_t ladderEntity{},worldEntity{},actorEntity{};
edict_t* motionActor{};
bool frameCurrent{true};
cvar_t frameCvars[4]{};
cvar_t* frameCvar(const char* name) {
    const char* names[]{"sv_gravity","sv_airaccelerate","sv_maxspeed","sv_maxvelocity"};
    for(unsigned i=0;i<4;++i) if(std::strcmp(name,names[i])==0) return &frameCvars[i];
    return nullptr;
}
constexpr nav::local::Binding frameBinding{{1},{2,{3}},{1},4,5};
bool currentFrame(const void*,nav::local::Binding b,core::TickId t) noexcept {
    return frameCurrent && b.agent==frameBinding.agent && b.actor==frameBinding.actor &&
        b.map==frameBinding.map && b.routeGeneration==4 && b.step==5 && t==core::TickId{6};
}
core::MapGeneration activeMap{1};
unsigned traces{},faultAt{};
int fault{};
#ifdef ASTRABOT_LADDER_HOST_TESTS
NavConsole* publishing{};
#endif
struct Box { V low,high; } floors[2];
core::MapGeneration mapNow(const void*) noexcept { return activeMap; }
edict_t* entityAt(int n) { return n==1 ? &ladderEntity:n==0 ? &worldEntity:n==2 ? &actorEntity:nullptr; }
int indexOf(const edict_t* e) { return e==&ladderEntity ? 1:e==&actorEntity ? 2:0; }
const char* classname(int n) { return n==1 ? "func_ladder":n==2 ? "*1":nullptr; }
// Independent slab intersection, including Minkowski expansion for engine hull1.
void boxTrace(const float* a,const float* b,Box box,int hull,edict_t* hit,TraceResult& out) {
    const float lo[]{box.low.x-(hull ? 16:0),box.low.y-(hull ? 16:0),box.low.z-(hull ? 36:0)};
    const float hi[]{box.high.x+(hull ? 16:0),box.high.y+(hull ? 16:0),box.high.z+(hull ? 36:0)};
    bool inside=true; double enter=0,leave=1; Vector normal{};
    for(int axis=0;axis<3;++axis) {
        inside=inside && a[axis]>lo[axis] && a[axis]<hi[axis];
        const double delta=double(b[axis])-a[axis];
        if(delta==0) { if(a[axis]<=lo[axis] || a[axis]>=hi[axis]) return; continue; }
        double first=(lo[axis]-a[axis])/delta,last=(hi[axis]-a[axis])/delta;
        float sign=-1; if(first>last) { std::swap(first,last); sign=1; }
        if(first>enter) { enter=first; normal=Vector(0,0,0); normal[axis]=sign; }
        leave=(std::min)(leave,last); if(enter>leave) return;
    }
    if(inside) { out.fStartSolid=out.fAllSolid=1; out.flFraction=0; out.vecEndPos=Vector(a[0],a[1],a[2]); out.pHit=hit; return; }
    if(enter<0 || enter>out.flFraction || enter>1 || leave<=0) return;
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
    if(fault==12) actorEntity.v.origin.x+=1;
    if(fault==13) ++actorEntity.serialnumber;
    if(fault==14) frameCurrent=false;
    if(fault==15) actorEntity.v.movetype=MOVETYPE_WALK;
    if(fault==16) frameCvars[0].value+=1;
    if(fault==17) frameCvars[1].value+=1;
    if(fault==18) frameCvars[2].value=100;
    if(fault==19) frameCvars[3].value+=1;
    if(fault==20) actorEntity.v.friction=0.5f;
#ifdef ASTRABOT_LADDER_HOST_TESTS
    if(fault==11 && publishing) publishing->invalidate(nav::runtime::SessionReason::Cancelled);
#endif
}
void traceModel(const float* a,const float* b,int hull,edict_t* e,TraceResult* t) {
    assert(e==&ladderEntity && (hull==0 || hull==1)); begin(b,t);
    boxTrace(a,b,{{0,0,0},{8,64,128}},hull,e,*t); inject(a,b,t);
}
void traceHull(const float* a,const float* b,int ignore,int hull,edict_t* ignored,TraceResult* t) {
    assert((ignore==0 || ignore==1) && hull==1 && (!ignored || ignored==&actorEntity || ignored==motionActor)); begin(b,t);
    for(const auto& floor:floors) boxTrace(a,b,floor,1,&worldEntity,*t);
    inject(a,b,t);
}
int pointContents(const float* point) {
    ++traces; TraceResult dummy{}; inject(point,point,&dummy);
    if(traces==faultAt && fault==3) return 0;
    for(const auto& floor:floors) if(point[0]>floor.low.x && point[0]<floor.high.x &&
        point[1]>floor.low.y && point[1]<floor.high.y && point[2]>floor.low.z && point[2]<floor.high.z)
        return CONTENTS_SOLID;
    return CONTENTS_EMPTY;
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
    e.pfnTraceModel=traceModel; e.pfnTraceHull=traceHull; e.pfnCVarGetPointer=frameCvar;
    e.pfnPointContents=pointContents; return e;
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
void testLadderFrame() {
    for(bool grounded:{false,true}) {
        const auto index=setup(LadderFace::MinX,LadderExit::AcrossTop); auto e=engine();
        nav::enrichment::NavMapFingerprint fingerprint{};
        const auto batch=discoverLadderLinks({&e,nullptr,mapNow},{1},*index,{1},fingerprint,9,2);
        assert(batch);
        const auto bound=bindLadderPlan({&e,nullptr,mapNow},{1},fingerprint,*batch.value,batch.value->links.links.front(),2);
        assert(bound);
        const V origin=grounded ? bound.value->plan.start:V{bound.value->plan.mount.x,bound.value->plan.mount.y,80};
        const auto resetActor=[&]() {
            actorEntity={}; actorEntity.serialnumber=8; frameCurrent=true; traces=fault=faultAt=0; activeMap={1};
            const float values[]{800,10,320,2000};
            for(unsigned i=0;i<4;++i) { frameCvars[i]={}; frameCvars[i].value=values[i]; }
            ladderEntity.serialnumber=7;
            auto& v=actorEntity.v; v.flags=FL_FAKECLIENT|(grounded ? FL_ONGROUND:0);
            v.origin=Vector(origin.x,origin.y,origin.z); v.mins=Vector(-16,-16,-36); v.maxs=Vector(16,16,36);
            v.movetype=grounded ? MOVETYPE_WALK:MOVETYPE_FLY; v.maxspeed=250; v.friction=1;
        };
        resetActor();
        nav::runtime::MovementSnapshot s; s.agent=frameBinding.agent; s.actor=frameBinding.actor; s.map={1}; s.tick={6};
        s.kind=nav::runtime::ActorKind::ManagedBot; s.connected=s.joined=s.alive=true; s.grounded=grounded; s.ducked=false;
        s.position=origin; s.velocity=s.view=V{}; s.hull=nav::runtime::HullDimensions{{-16,-16,-36},{16,16,36}}; s.speedLimit=250.0f;
        const LadderFrameWorld world{{&e,nullptr,mapNow},nullptr,currentFrame};
        const auto run=[&](unsigned budget) { return inspectLadderFrame(world,&actorEntity,frameBinding,s,*bound.value,origin,*index,{1},3,budget); };
        const auto ok=run(4); assert(ok && ok.queries==3 && traces==3);
        assert(ok.value->climbing==!grounded && ok.value->contact.touching==!grounded);
        assert(ok.value->inspection.support.has_value()==grounded && ok.value->inspection.pathClear==true);
        assert(ok.value->physics.gravity==800 && ok.value->physics.maximumSpeed==250 && ok.value->physics.maximumVelocity==2000);
        assert(!ok.value->inspection.exitIntent && ok.value->inspection.stamp.tick==s.tick);
        for(unsigned budget=0;budget<3;++budget) {
            resetActor(); const auto r=run(budget);
            assert(!r && !r.value && r.reason==LadderFrameReason::BudgetExceeded && r.queries==budget);
        }
        for(unsigned at=1;at<=3;++at) for(int mode:{1,2,3,7,12,13,14,16,17,18,19,20}) {
            resetActor(); faultAt=at; fault=mode; const auto r=run(4);
            assert(!r && !r.value && r.queries==at && traces==at);
        }
        resetActor(); s.tick={7}; assert(run(4).reason==LadderFrameReason::StaleActor && traces==0); s.tick={6};
        resetActor(); actorEntity.v.basevelocity.x=1; assert(run(4).reason==LadderFrameReason::StaleActor && traces==0);
        resetActor(); actorEntity.v.punchangle.x=1; assert(run(4).reason==LadderFrameReason::StaleActor && traces==0);
        resetActor(); actorEntity.v.flags|=FL_FROZEN; assert(run(4).reason==LadderFrameReason::StaleActor && traces==0);
        resetActor(); frameCvars[0].value=0; assert(run(4).reason==LadderFrameReason::Unavailable && traces==0);
        resetActor(); frameCvars[2].value=120; actorEntity.v.gravity=0.5f;
        const auto scaled=run(4); assert(scaled && scaled.value->physics.gravity==400 && scaled.value->physics.maximumSpeed==120);
        resetActor(); assert(run(5).reason==LadderFrameReason::InvalidInput && traces==0);
        if(!grounded) {
            resetActor(); faultAt=2; fault=6; assert(run(4).reason==LadderFrameReason::WrongFace);
            resetActor(); faultAt=2; fault=15; assert(run(4).reason==LadderFrameReason::StaleActor);
        } else {
            resetActor(); faultAt=3; fault=10; assert(run(4).reason==LadderFrameReason::NoSupport);
        }
        resetActor(); faultAt=grounded ? 2:3; fault=5; assert(run(4).reason==LadderFrameReason::Blocked);
        resetActor();
        assert(inspectLadderFrame(world,&actorEntity,frameBinding,s,*bound.value,{origin.x+97,origin.y,origin.z},
            *index,{1},3).reason==LadderFrameReason::InvalidInput && traces==0);
        if(grounded) {
            // World support can exist outside NAV in the shaft. Its absence
            // from the NAV packet must never turn into target-area support.
            s.position=V{bound.value->plan.mount.x,bound.value->plan.mount.y,36};
            const auto inspectMount=[&](unsigned budget) {
                resetActor(); actorEntity.v.origin=Vector(s.position->x,s.position->y,s.position->z);
                return inspectLadderFrame(world,&actorEntity,frameBinding,s,*bound.value,*s.position,*index,{1},3,budget);
            };
            for(unsigned budget=0;budget<4;++budget) {
                const auto r=inspectMount(budget); assert(!r && r.queries==budget && r.reason==LadderFrameReason::BudgetExceeded);
            }
            const auto r=inspectMount(4); assert(r && r.queries==4 && !r.value->inspection.support);
            floors[0].high.x=100; // Independent world floor extends under shaft; NAV does not.
            core::BotCommand down; down.msec=16; down.movement.forward=-200;
            down.buttons=static_cast<core::ButtonMask>(core::Button::Back);
            const auto inspectCommand=[&](unsigned budget,unsigned at=0,int mode=0) {
                resetActor(); actorEntity.v.origin=Vector(s.position->x,s.position->y,s.position->z);
                faultAt=at; fault=mode;
                return inspectLadderFrame(world,&actorEntity,frameBinding,s,*bound.value,*s.position,*index,{1},3,budget,down);
            };
            const auto command=inspectCommand(7);
            assert(command && command.queries==7 && command.value->floorPointSolid==true && command.value->prediction);
            const auto& prediction=*command.value->prediction;
            assert(prediction.floorCollision && std::abs(prediction.endpoint.x-(s.position->x-3.2f))<0.001f);
            assert(prediction.endpoint.z==36 && prediction.velocity.z==0 && prediction.velocity.x==-200);
            assert(!command.value->inspection.support); // Predicted floor collision is not observed arrival.
            for(unsigned budget=0;budget<7;++budget) {
                const auto shortBudget=inspectCommand(budget);
                assert(!shortBudget && shortBudget.reason==LadderFrameReason::BudgetExceeded && shortBudget.queries==budget);
            }
            for(unsigned at=1;at<=7;++at) for(int mode:{1,2,12,14,16}) {
                const auto stale=inspectCommand(7,at,mode); assert(!stale && !stale.value && stale.queries==at);
            }
            assert(inspectCommand(7,5,3).reason==LadderFrameReason::InvalidTrace);
            assert(inspectCommand(7,6,10).reason==LadderFrameReason::Blocked);
            assert(inspectCommand(7,7,5).reason==LadderFrameReason::Blocked);
        } else {
            resetActor(); core::BotCommand up; up.msec=16; up.movement.forward=200;
            up.buttons=static_cast<core::ButtonMask>(core::Button::Forward);
            auto command=inspectLadderFrame(world,&actorEntity,frameBinding,s,*bound.value,*s.position,*index,{1},3,7,up);
            assert(command && command.queries==5 && command.value->floorPointSolid==false);
            assert(command.value->prediction && std::abs(command.value->prediction->endpoint.z-83.2f)<0.001f);
            resetActor(); core::BotCommand jump; jump.msec=16; jump.buttons=static_cast<core::ButtonMask>(core::Button::Jump);
            const auto leap=inspectLadderFrame(world,&actorEntity,frameBinding,s,*bound.value,*s.position,*index,{1},3,7,jump);
            if(!leap) { std::fprintf(stderr,"host rejected airborne ladder jump\n"); std::exit(1); }
            assert(leap.value->prediction && !leap.value->floorPointSolid && !leap.value->inspection.exitIntent);
            assert(std::abs(leap.value->prediction->endpoint.x-(s.position->x-4.32f))<0.001f);
            assert(std::abs(leap.value->prediction->endpoint.z-79.8976f)<0.001f);
            assert(leap.value->prediction->velocity.x==-270 && std::abs(leap.value->prediction->velocity.z+12.8f)<0.001f);
            resetActor(); s.position=V{-15,32,168}; s.velocity=V{0,0,200};
            actorEntity.v.origin=Vector(-15,32,168); actorEntity.v.velocity=Vector(0,0,200);
            command=inspectLadderFrame(world,&actorEntity,frameBinding,s,*bound.value,*s.position,*index,{1},3,7,up);
            assert(command && !command.value->contact.touching && command.value->climbing && command.queries==3);
            assert(command.value->prediction && std::abs(command.value->prediction->endpoint.z-171.0976f)<0.001f);
            assert(!command.value->floorPointSolid.has_value());
        }
    }
}
void testUpperExitFrame() {
    for(const auto exit:{LadderExit::SameFace,LadderExit::AcrossTop}) {
        const auto index=setup(LadderFace::MinX,exit); auto e=engine();
        nav::enrichment::NavMapFingerprint fingerprint{};
        const auto batch=discoverLadderLinks({&e,nullptr,mapNow},{1},*index,{1},fingerprint,9,2); assert(batch);
        const auto bound=bindLadderPlan({&e,nullptr,mapNow},{1},fingerprint,*batch.value,batch.value->links.links.front(),2); assert(bound);
        nav::runtime::MovementSnapshot s; s.agent=frameBinding.agent; s.actor=frameBinding.actor; s.map={1}; s.tick={6};
        s.kind=nav::runtime::ActorKind::ManagedBot; s.connected=s.joined=s.alive=true; s.grounded=s.ducked=false;
        s.position=bound.value->plan.dismount; s.position->z-=1;
        s.velocity=s.view=V{}; s.hull=nav::runtime::HullDimensions{{-16,-16,-36},{16,16,36}}; s.speedLimit=250.0f;
        const auto reset=[&]() {
            traces=fault=faultAt=0; activeMap={1}; frameCurrent=true; ladderEntity.serialnumber=7;
            actorEntity={}; actorEntity.serialnumber=8; auto& v=actorEntity.v;
            v.flags=FL_FAKECLIENT; v.movetype=MOVETYPE_FLY; v.maxspeed=250; v.friction=1;
            v.origin=Vector(s.position->x,s.position->y,s.position->z); v.mins=Vector(-16,-16,-36); v.maxs=Vector(16,16,36);
            const float values[]{800,10,320,2000};
            for(unsigned i=0;i<4;++i) { frameCvars[i]={}; frameCvars[i].value=values[i]; }
        };
        const auto inspect=[&](unsigned budget,std::uint8_t msec,unsigned at=0,int mode=0) {
            reset(); faultAt=at; fault=mode;
            return inspectLadderFrame({{&e,nullptr,mapNow},nullptr,currentFrame},&actorEntity,frameBinding,s,*bound.value,
                bound.value->plan.end,*index,{1},3,budget,{},msec);
        };
        for(const auto msec:std::array<std::uint8_t,3>{8,16,100}) {
            const auto r=inspect(21,msec);
            assert(r && r.queries<=21 && r.value->upperExit && r.value->inspection.exitIntent && r.value->prediction);
            assert(r.value->prediction->command.msec==msec && r.value->inspection.exitIntent->forward==core::ActionRequest::Hold);
            assert(!r.value->inspection.support); // Future landing is never published as current support.
        }
        const auto good=inspect(21,16); assert(good); const auto count=good.queries;
        for(unsigned budget=0;budget<count;++budget) {
            const auto r=inspect(budget,16); assert(!r && !r.value && r.queries==budget && r.reason==LadderFrameReason::BudgetExceeded);
        }
        for(unsigned at=1;at<=count;++at) for(int mode:{1,2,12,14,16}) {
            const auto r=inspect(21,16,at,mode); assert(!r && !r.value && r.queries==at);
        }
        assert(inspect(21,16,5,5).reason==LadderFrameReason::Blocked);
        assert(inspect(21,16,count,10).reason==LadderFrameReason::NoSupport);
        reset(); core::BotCommand held; held.msec=16; held.view.pitch=-44;
        held.buttons=static_cast<core::ButtonMask>(core::Button::Forward); held.movement.forward=200;
        const auto guarded=inspectLadderFrame({{&e,nullptr,mapNow},nullptr,currentFrame},&actorEntity,frameBinding,s,*bound.value,
            bound.value->plan.end,*index,{1},3,21,held,std::uint8_t{16});
        if(!guarded) { std::fprintf(stderr,"exit forecast cannot validate held first command: %d exit=%d queries=%u\n",int(guarded.reason),int(guarded.exitReason),guarded.queries); std::exit(1); }
        assert(guarded.value->prediction && guarded.value->prediction->command.view.pitch==-44);
        assert(!guarded.value->inspection.exitIntent); // Guarding existing input cannot authorize a different regenerated intent.
    }
}
void testLowerExitFrame() {
    const auto index=setup(LadderFace::MinX,LadderExit::AcrossTop); auto e=engine();
    nav::enrichment::NavMapFingerprint fingerprint{};
    const auto batch=discoverLadderLinks({&e,nullptr,mapNow},{1},*index,{1},fingerprint,9,2); assert(batch);
    const auto selected=std::find_if(batch.value->links.links.begin(),batch.value->links.links.end(),
        [](const auto& l) { return l.direction==nav::enrichment::NavLinkDirection::Down; });
    assert(selected!=batch.value->links.links.end());
    const auto bound=bindLadderPlan({&e,nullptr,mapNow},{1},fingerprint,*batch.value,*selected,2); assert(bound);
    nav::runtime::MovementSnapshot s; s.agent=frameBinding.agent; s.actor=frameBinding.actor; s.map={1}; s.tick={6};
    s.kind=nav::runtime::ActorKind::ManagedBot; s.connected=s.joined=s.alive=s.grounded=true; s.ducked=false;
    s.position=bound.value->plan.dismount; s.velocity=s.view=V{};
    s.hull=nav::runtime::HullDimensions{{-16,-16,-36},{16,16,36}}; s.speedLimit=250.0f;
    floors[0].high.x=100; // Actual world floor under the shaft; NAV stays immutable.
    const auto inspect=[&](unsigned budget,std::uint8_t msec,unsigned at=0,int mode=0) {
        traces=fault=faultAt=0; activeMap={1}; frameCurrent=true; ladderEntity.serialnumber=7;
        actorEntity={}; actorEntity.serialnumber=8; auto& v=actorEntity.v;
        v.flags=FL_FAKECLIENT|(*s.grounded ? FL_ONGROUND:0); v.movetype=MOVETYPE_FLY; v.maxspeed=250; v.friction=1;
        v.origin=Vector(s.position->x,s.position->y,s.position->z); v.mins=Vector(-16,-16,-36); v.maxs=Vector(16,16,36);
        const float values[]{800,10,320,2000};
        for(unsigned i=0;i<4;++i) { frameCvars[i]={}; frameCvars[i].value=values[i]; }
        faultAt=at; fault=mode;
        return inspectLadderFrame({{&e,nullptr,mapNow},nullptr,currentFrame},&actorEntity,frameBinding,s,*bound.value,
            bound.value->plan.end,*index,{1},3,budget,{},msec);
    };
    for(const auto msec:std::array<std::uint8_t,3>{8,16,100}) {
        const auto r=inspect(21,msec);
        if(!r) { std::fprintf(stderr,"lower exit rejected: reason=%d queries=%u\n",int(r.reason),r.queries); std::exit(1); }
        assert(r && r.value->inspection.exitIntent && r.value->prediction && !r.value->upperExit);
        assert(r.value->inspection.exitIntent->back==core::ActionRequest::Hold && r.value->floorPointSolid==true);
        assert(r.value->prediction->floorCollision && r.value->prediction->velocity.x==-200);
        assert(std::abs(r.value->prediction->endpoint.x-(s.position->x-0.2f*msec))<0.001f);
        assert(!r.value->inspection.support); // Predicted endpoint is not current NAV support.
    }
    const auto good=inspect(21,16); assert(good); const auto count=good.queries;
    for(unsigned budget=0;budget<count;++budget) {
        const auto r=inspect(budget,16); assert(!r && !r.value && r.queries==budget && r.reason==LadderFrameReason::BudgetExceeded);
    }
    for(unsigned at=1;at<=count;++at) for(int mode:{1,2,12,14,16}) {
        const auto r=inspect(21,16,at,mode); assert(!r && !r.value && r.queries==at);
    }
    assert(inspect(21,16,count,10).reason==LadderFrameReason::NoSupport);
    assert(!inspect(21,0));
    floors[0].high.x=-20; // Hull overlap is not solid foot-point proof for a kick.
    assert(!inspect(21,16));
    s.grounded=false; s.position->z+=8;
    for(const auto msec:std::array<std::uint8_t,3>{8,16,100}) {
        const auto r=inspect(21,msec);
        if(!r) { std::fprintf(stderr,"host rejected lower jump exit candidate\n"); std::exit(1); }
        assert(r.value->inspection.exitIntent && r.value->inspection.exitIntent->jump==core::ActionRequest::Press);
        assert(r.value->prediction && r.value->prediction->command.buttons==static_cast<core::ButtonMask>(core::Button::Jump));
        assert(!r.value->inspection.support && !r.value->floorPointSolid);
    }
    const auto leap=inspect(21,16); assert(leap && leap.value->jumpExit); const auto leapCount=leap.queries;
    for(unsigned budget=0;budget<leapCount;++budget) {
        const auto r=inspect(budget,16); assert(!r && !r.value && r.queries==budget && r.reason==LadderFrameReason::BudgetExceeded);
    }
    for(unsigned at=1;at<=leapCount;++at) for(int mode:{1,2,12,14,16}) {
        const auto r=inspect(21,16,at,mode); assert(!r && !r.value && r.queries==at);
    }
    assert(inspect(21,16,4,5).reason==LadderFrameReason::NoExit); // Predicted release still touches the model.
    assert(inspect(21,16,5,5).reason==LadderFrameReason::Blocked);
    assert(inspect(21,16,leapCount,10).reason==LadderFrameReason::NoSupport);
    assert(!inspect(21,1)); // Too short to clear model contact; no repeated jump assumption.
    floors[0].high.x=100; s.grounded=true; s.position=V{-19,32,36};
    // Detached and outside NAV, but standing on a measured world floor.
    actorEntity.v.flags=FL_FAKECLIENT|FL_ONGROUND; actorEntity.v.movetype=MOVETYPE_WALK;
    actorEntity.v.origin=Vector(-19,32,36); fault=faultAt=traces=0;
    const auto approach=inspectLadderFrame({{&e,nullptr,mapNow},nullptr,currentFrame},&actorEntity,frameBinding,s,*bound.value,
        bound.value->plan.end,*index,{1},3,21,{},{},true);
    if(!approach) { std::fprintf(stderr,"measured outside-NAV exit approach unavailable\n"); std::exit(1); }
    assert(approach.value->inspection.worldFloor && approach.value->inspection.groundPathClear==true);
    assert(!approach.value->inspection.support && !approach.value->inspection.exitIntent && !approach.value->contact.touching);
    const auto groundCount=approach.queries;
    const auto savedLadder=ladderEntity;
    const auto groundProof=[&](unsigned budget,unsigned injectAt,int injected) {
        ladderEntity=savedLadder; activeMap={1}; fault=injected; faultAt=injectAt; traces=0;
        return inspectLadderFrame({{&e,nullptr,mapNow},nullptr,currentFrame},&actorEntity,frameBinding,s,*bound.value,
            bound.value->plan.end,*index,{1},3,budget,{},{},true);
    };
    for(unsigned budget=0;budget<groundCount;++budget) {
        const auto limited=groundProof(budget,0,0);
        assert(!limited && limited.queries<=budget);
    }
    for(unsigned at=1;at<=groundCount;++at) for(int change:{1,2}) assert(!groundProof(21,at,change));
    assert(groundProof(21,groundCount,10).reason==LadderFrameReason::NoSupport);
    assert(groundProof(21,0,0));
}
void testLadderDiscovery() {
    testLadderFrame();
    testUpperExitFrame();
    testLowerExitFrame();
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
    const auto route=nav::query::NavRouteSearch::search(**graph.value,{{1},{2},{2,100000},false}); assert(route);
    const auto corridor=nav::corridor::Corridor::build(**graph.value,*route.value,{16,16},{1,100000,100});
    if(!corridor) { std::fprintf(stderr,"published ladder endpoints cannot form standing corridor\n"); std::exit(1); }
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
// Independent fixture; never calls the production movement predictor.
void invalidateLadderMotionFixture(bool duringTrace) {
    if(duringTrace) { fault=2; faultAt=traces+1; }
    else ++ladderEntity.serialnumber;
}
unsigned ladderMotionQueryCount() { return traces; }
void simulateLadderMotion(edict_t& actor,const core::BotCommand& command) {
    const double dt=command.msec/1000.0,rad=3.14159265358979323846/180;
    const double yaw=command.view.yaw*rad,pitch=command.view.pitch*rad;
    float from[]{actor.v.origin.x,actor.v.origin.y,actor.v.origin.z};
    TraceResult contact{}; contact.flFraction=1;
    boxTrace(from,from,{{0,0,0},{8,64,128}},1,&ladderEntity,contact);
    double vx=actor.v.velocity.x,vy=actor.v.velocity.y,vz=actor.v.velocity.z;
    bool air=false;
    if(contact.fStartSolid) {
        actor.v.movetype=MOVETYPE_FLY;
        if(command.buttons&IN_JUMP) {
            actor.v.movetype=MOVETYPE_WALK; vx=-270; vy=0; vz=0; air=true;
        } else {
            const double speed=(command.buttons&IN_FORWARD ? 200:0)-(command.buttons&IN_BACK ? 200:0);
            vx=0; vy=speed*std::cos(pitch)*std::sin(yaw);
            vz=speed*(std::cos(pitch)*std::cos(yaw)-std::sin(pitch));
        }
        actor.v.flags&=~FL_ONGROUND;
    } else {
        actor.v.movetype=MOVETYPE_WALK; air=!(actor.v.flags&FL_ONGROUND);
        if(!air) {
            vx=command.movement.forward*std::cos(yaw)+command.movement.side*std::sin(yaw);
            vy=command.movement.forward*std::sin(yaw)-command.movement.side*std::cos(yaw); vz=0;
        }
    }
    if(air) {
        const double wx=command.movement.forward*std::cos(yaw)+command.movement.side*std::sin(yaw);
        const double wy=command.movement.forward*std::sin(yaw)-command.movement.side*std::cos(yaw);
        const double length=std::hypot(wx,wy);
        if(length>0) {
            const double wish=(std::min)(length,250.0);
            const double add=(std::max)(0.0,(std::min)(10*wish*dt,(std::min)(wish,30.0)-(vx*wx+vy*wy)/length));
            vx+=add*wx/length; vy+=add*wy/length;
        }
        vz-=400*dt;
    }
    float to[]{float(from[0]+vx*dt),float(from[1]+vy*dt),float(from[2]+vz*dt)};
    TraceResult hit{}; hit.flFraction=1; hit.vecEndPos=Vector(to[0],to[1],to[2]);
    for(const auto& floor:floors)
        if(from[2]>floor.high.z+36.001f || to[2]<from[2]) boxTrace(from,to,floor,1,&worldEntity,hit);
    actor.v.origin=hit.vecEndPos;
    if(air) vz-=400*dt;
    if(hit.flFraction<1 && hit.vecPlaneNormal.z>0.7) { vz=0; actor.v.flags|=FL_ONGROUND; }
    actor.v.velocity=Vector(float(vx),float(vy),float(vz)); actor.v.oldbuttons=command.buttons;
    if(actor.v.flags&FL_ONGROUND) {
        const float p[]{actor.v.origin.x,actor.v.origin.y,actor.v.origin.z+0.05f};
        const float below[]{p[0],p[1],p[2]-0.1f}; TraceResult floor{}; floor.flFraction=1;
        for(const auto& box:floors) boxTrace(p,below,box,1,&worldEntity,floor);
        if(floor.flFraction==1) actor.v.flags&=~FL_ONGROUND;
    }
}
std::shared_ptr<const nav::model::NavMeshSnapshot> configureLadderMotionFixture(enginefuncs_t& e,edict_t* actor,core::MapGeneration map,bool down) {
    (void)setup(LadderFace::MinX,down ? LadderExit::SameFace:LadderExit::AcrossTop); activeMap=map; motionActor=actor;
    e.pfnPEntityOfEntIndex=[](int slot) -> edict_t* { return slot==0 ? &worldEntity:slot==1 ? motionActor:slot==40 ? &ladderEntity:nullptr; };
    e.pfnIndexOfEdict=[](const edict_t* value) { return value==motionActor ? 1:value==&ladderEntity ? 40:0; };
    e.pfnSzFromIndex=classname; e.pfnTraceModel=traceModel; e.pfnTraceHull=traceHull;
    e.pfnPointContents=pointContents; e.pfnCVarGetPointer=frameCvar;
    const float values[]{800,10,320,2000}; for(unsigned i=0;i<4;++i) { frameCvars[i]={}; frameCvars[i].value=values[i]; }
    std::vector<route_test::Area> areas;
    for(unsigned i=0;i<2;++i) {
        const auto& f=floors[i]; const float z=f.high.z;
        areas.push_back({i+1,{{f.low.x,f.low.y,z},{f.high.x,f.high.y,z},z,z}});
    }
    return route_test::snapshot(areas);
}
#endif
