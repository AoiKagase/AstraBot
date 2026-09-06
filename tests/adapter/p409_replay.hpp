// SPDX-License-Identifier: MPL-2.0
namespace p409 {
namespace e=perception_evidence;
e::Row matrix(unsigned actors,unsigned us) {
    Fixture fixture; enginefuncs_t hooks{},post{}; p405::setup(fixture,hooks,post,actors);
    auto& owner=astrabot::adapter::metamod::lifecycleCoordinator();
    auto target=owner.registry().currentPlayer(32);
    e::Row row{"hooks",actors,us,false,{}};
    const auto sample=[&](const char* phase) {
        for(std::uint16_t slot=1;slot<=actors;++slot)
            row.sample(phase,owner.world(),owner.distributions(),owner.registry().currentPlayer(slot),target,
                       p401::traceCalls,owner.sound().diagnostics().frameAudienceChecks,owner.sound().diagnostics().queued);
    };
    sample("sight");
    const auto send=owner.report(owner.registry().currentPlayer(1),target); assert(send.accepted()==(actors>1));
    // Emission input sequence is deterministic and distinct within one frame.
    for(unsigned i=0;i<266;++i) { p401::human.v.origin.x=100+static_cast<float>(i)*0.25f; p404::footstep(post); }
    assert(owner.sound().diagnostics().queued==256 && owner.sound().diagnostics().overflow==10);
    p405::smoke(hooks,post);
    for(unsigned i=1;i<=actors;++i) p405::fade(hooks,fixture.matrixEntity(i));
    p401::mode=1; p401::human.v.origin.x=4000;
    for(unsigned i=0;i<160;++i) {
        p401::step(fixture,static_cast<float>(us)/1000000); sample("load");
        if(i==7) assert(owner.sound().diagnostics().queued==0);
        if(i==0) for(std::uint16_t slot=1;slot<=actors;++slot) assert(owner.world().latest(owner.registry().currentPlayer(slot))->sounds->count==16);
    }
    p401::step(fixture,23); sample("expired");
    assert(owner.visualEffects().diagnostics().regions==0);
    p401::mode=0; p401::human.v.origin.x=210;
    for(unsigned i=0;i<10;++i) p401::step(fixture,0.1f);
    sample("resee");
    p401::mode=1; p403::roundEvent(hooks); p401::step(fixture,0.1f); sample("round");
    owner.clientDisconnect(&p401::human); p401::human.free=1; p401::step(fixture);
    p401::human.free=0; ++p401::human.serialnumber; p401::step(fixture);
    const auto old=target; target=owner.registry().currentPlayer(32); assert(target!=old); sample("reuse");
    owner.serverDeactivate(); assert(!owner.world().latest(owner.registry().currentPlayer(1)));
    detach(); return row;
}
e::Row movement(unsigned us,bool recover) {
    using namespace astrabot;
    Fixture fixture; enginefuncs_t hooks{},post{};
    fixture.engine.pfnIndexOfEdict=&p401::index; fixture.engine.pfnPEntityOfEntIndex=&p401::indexed;
    fixture.engine.pfnTraceLine=&p401::trace;
    for(unsigned slot=2;slot<=16;++slot) fixture.matrixEntity(slot)->free=1;
    p401::human={}; p401::mode=0; p401::extraTrace=nullptr;
    gDoorActive=false; gSteeringMode=-1; gPlayerObstacle=gMatrixPlayer=false;
    gSimulateCrouch=gSimulateJump=gSimulateLadder=false;
    gNavClockUs=1000000; gFreezeNav=false; gFrozenActor=nullptr;
    prepareNavWalk(fixture,hooks,us); p404::hooksFor(fixture,post);
    fixture.entity.v.health=100; fixture.entity.v.view_ofs=Vector(0,0,28); fixture.entity.v.v_angle=Vector(0,0,0);
    p401::human.v.flags=FL_CLIENT; p401::human.v.health=100; p401::human.serialnumber=22;
    p401::human.v.origin=Vector(150,80,36); p401::human.v.view_ofs=Vector(0,0,28);
    auto& owner=adapter::metamod::lifecycleCoordinator(); const auto actor=owner.fakeClient().activePlayer();
    e::Row row{recover ? "recovery":"arrival",1,us,true,{}};
    const auto frame=[&](const char* phase) {
        p404::footstep(post); p401::traceCalls=0; fixture.engineGlobals.time+=static_cast<float>(us)/1000000;
        const auto before=gHullCalls; navFrame(fixture,us); assert(gHullCalls-before<=21);
        row.sample(phase,owner.world(),owner.distributions(),actor,owner.registry().currentPlayer(32),p401::traceCalls,
                   owner.sound().diagnostics().frameAudienceChecks,owner.sound().diagnostics().queued);
    };
    frame("warmup"); runNav({"astrabot_goto","4"}); gFreezeNav=recover; bool waiting=false;
    for(std::uint64_t elapsed=0;elapsed<8000000;elapsed+=us) {
        if(recover && elapsed>=700000) gFreezeNav=false;
        p401::mode=(elapsed/200000)%2==0 ? 0U:1U; frame("motion");
        waiting=waiting || owner.navConsole().motionTrace().decision.recovery.state==nav::local::RecoveryState::Wait;
    }
    assert(!recover || waiting); assert(owner.navConsole().motionTrace().decision.state==nav::local::WalkState::Arrived);
    assert(owner.vision().observations().diagnostics(actor)->updates>1 && owner.world().latest(actor)->sounds->count>0);
    const auto moves=gNavMoves.size(); for(unsigned i=0;i<10;++i) frame("stopped"); assert(gNavMoves.size()==moves);
    row.events.push_back("{\"terminal\":\"Arrived\",\"stopped\":true,\"recovered\":"+std::string(waiting ? "true":"false")+",\"commands\":"+std::to_string(moves)+"}");
    gFreezeNav=false; gSimulateNav=false; detach(); return row;
}
void run(const char* path) {
    std::vector<e::Row> rows;
    for(unsigned actors:{1U,8U,16U}) for(unsigned us:{8000U,16000U,100000U}) rows.push_back(matrix(actors,us));
    for(unsigned us:{8000U,16000U,100000U}) for(bool recover:{false,true}) rows.push_back(movement(us,recover));
    // Reentry guards are deliberately rerun in the integrated executable.
    p405::reentrant(); p406::reentrant(); p408::publication(); p408::reentrant();
    e::writeEvidence(path,"adapter",rows);
}
}
