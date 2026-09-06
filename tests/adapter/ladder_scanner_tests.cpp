// SPDX-License-Identifier: MPL-2.0
#include "adapter/cstrike/nav/ladder_scanner.hpp"
#include <cassert>
#include <limits>
using namespace astrabot;
using namespace astrabot::adapter::cstrike;
namespace {
edict_t entities[5]{};
unsigned calls{};
bool stale{};
edict_t* entityAt(int index) { ++calls; return index>=0 && index<5 ? &entities[index]:nullptr; }
int entityIndex(const edict_t* e) { return static_cast<int>(e-entities); }
const char* nameAt(int index) {
    if(stale && index==1) ++entities[1].serialnumber;
    return index==1 ? "func_ladder":index==2 ? "func_wall":nullptr;
}
enginefuncs_t reset() {
    for(auto& e:entities) e={};
    calls=0; stale=false;
    enginefuncs_t e{}; e.pfnPEntityOfEntIndex=entityAt; e.pfnIndexOfEdict=entityIndex; e.pfnSzFromIndex=nameAt;
    return e;
}
void ladder(int slot) {
    auto& e=entities[slot]; e.serialnumber=7; e.v.classname=1;
    e.v.absmin=Vector(0,0,0); e.v.absmax=Vector(8,64,128);
}
void scans() {
    auto e=reset(); auto r=scanLadderCandidates(&e,{1},5);
    assert(r && r.candidates.count==0 && r.candidates.map==core::MapGeneration{1} && calls==4);
    ladder(1); ladder(3); entities[2].v.classname=2; entities[4].free=1; entities[4].v.classname=1;
    calls=0; r=scanLadderCandidates(&e,{2},5);
    assert(r && r.candidates.count==2 && r.inspected==4 && calls==6);
    assert(r.candidates.values[0].entityId==((std::uint64_t{7}<<32)|1));
    assert(r.candidates.values[1].entityId==((std::uint64_t{7}<<32)|3));
    assert(r.candidates.values[0].maximum.z==128);
    ++entities[1].serialnumber;
    assert(scanLadderCandidates(&e,{2},5).candidates.values[0].entityId!=r.candidates.values[0].entityId);
    assert(r.candidates.values[0].entityId==((std::uint64_t{7}<<32)|1)); // value snapshot
}
void failures() {
    for(int mode=0;mode<10;++mode) {
        auto e=reset(); ladder(1); ladder(3);
        auto expected=LadderScanReason::UnsupportedGeometry;
        if(mode==0) entities[3].v.absmax.z=0;
        if(mode==1) entities[3].v.absmin.x=(std::numeric_limits<float>::quiet_NaN)();
        if(mode==2) entities[3].v.angles.y=90;
        if(mode==3) entities[3].v.velocity.x=1;
        if(mode==4) entities[3].v.avelocity.z=1;
        if(mode==5) { stale=true; expected=LadderScanReason::InvalidEntity; }
        if(mode==6) { e.pfnSzFromIndex=nullptr; expected=LadderScanReason::Unavailable; }
        if(mode==7) { e.pfnIndexOfEdict=nullptr; expected=LadderScanReason::Unavailable; }
        if(mode==8) { e.pfnPEntityOfEntIndex=nullptr; expected=LadderScanReason::Unavailable; }
        if(mode==9) { entities[3].v.classname=3; expected=LadderScanReason::InvalidEntity; }
        const auto r=scanLadderCandidates(&e,{1},5);
        assert(!r && r.reason==expected && r.candidates.count==0 && !r.candidates.map.isValid());
    }
    auto e=reset(); ladder(1); ladder(3);
    assert(scanLadderCandidates(&e,{1},5,5,1).reason==LadderScanReason::CandidateLimit);
    assert(scanLadderCandidates(&e,{1},5,5,0).reason==LadderScanReason::CandidateLimit);
    calls=0;
    assert(scanLadderCandidates(&e,{1},5,4).reason==LadderScanReason::EntityLimit && calls==0);
    assert(scanLadderCandidates(&e,{1},8193).reason==LadderScanReason::EntityLimit && calls==0);
    assert(scanLadderCandidates(&e,{},5).reason==LadderScanReason::InvalidInput);
    assert(scanLadderCandidates(&e,{1},-1).reason==LadderScanReason::InvalidInput);
    assert(scanLadderCandidates(&e,{1},5,8193).reason==LadderScanReason::InvalidInput);
    assert(scanLadderCandidates(&e,{1},5,8192,129).reason==LadderScanReason::InvalidInput);
}
}
void testLadderProbe();
void testLadderDiscovery();
int main() { scans(); failures(); testLadderProbe(); testLadderDiscovery(); }
