// SPDX-License-Identifier: MPL-2.0
namespace p408 {
std::string actorText(astrabot::core::PlayerId player) {
    return std::to_string(player.slot)+":"+std::to_string(player.generation.value);
}
void publication() {
    Fixture fixture; enginefuncs_t hooks{}; p401::setup(fixture,hooks,2);
    auto& owner=astrabot::adapter::metamod::lifecycleCoordinator();
    const auto reporter=owner.registry().currentPlayer(1),receiver=owner.registry().currentPlayer(2),target=owner.registry().currentPlayer(32);
    sendTeamInfo(hooks,13,2,"CT"); sendTeamInfo(hooks,13,2,"TERRORIST");
    p405::fade(hooks,&fixture.secondEntity); p401::step(fixture,0.2f);
    assert(!owner.world().latest(receiver)->known(target));
    assert(owner.world().latest(receiver)->reports->count==0);
    const auto senderText=actorText(reporter),targetText=actorText(target);
    const auto traces=p401::traceCalls;
    runNav({"astrabot_report",senderText.c_str(),targetText.c_str()});
    assert(p401::traceCalls==traces);
    assert(owner.world().reports().diagnostics().queued==1 && owner.world().latest(receiver)->reports->count==0);
    p401::mode=1; p401::human.v.origin.x=4000; p401::step(fixture,0.1f);
    const auto knowledge=owner.world().latest(receiver)->known(target);
    assert(knowledge && knowledge->source==astrabot::core::perception::ObservationSource::TeamReport);
    assert(knowledge->position.x==100 && knowledge->reporter==reporter && knowledge->confidence<0.5);
    assert(owner.report(receiver,target).reason==astrabot::core::world::ReportReason::NoDirectSight);
    assert(owner.report(reporter,target).reason==astrabot::core::world::ReportReason::Duplicate);
    runNav({"astrabot_report","1:1","100,20,30"}); assert(owner.world().reports().diagnostics().queued==0);
    sendTeamInfo(hooks,13,1,"CT"); assert(owner.world().latest(receiver)->reports->count==0);
    sendTeamInfo(hooks,13,1,"TERRORIST"); p401::step(fixture); assert(owner.world().latest(receiver)->reports->count==0);
    detach();
}
bool disconnectDuringLookup{};
edict_t* indexed(int slot) {
    if(slot==32 && disconnectDuringLookup) {
        disconnectDuringLookup=false;
        astrabot::adapter::metamod::lifecycleCoordinator().clientDisconnect(&gFixture->entity);
    }
    return p401::indexed(slot);
}
void reentrant() {
    Fixture fixture; enginefuncs_t hooks{}; p401::setup(fixture,hooks,2);
    auto& owner=astrabot::adapter::metamod::lifecycleCoordinator();
    const auto reporter=owner.registry().currentPlayer(1),target=owner.registry().currentPlayer(32);
    fixture.engine.pfnPEntityOfEntIndex=&indexed; disconnectDuringLookup=true;
    assert(!owner.report(reporter,target).accepted()); assert(!disconnectDuringLookup);
    assert(owner.world().reports().diagnostics().queued==0);
    fixture.engine.pfnPEntityOfEntIndex=&p401::indexed; detach();
}
void matrix() {
    for(unsigned actors:{1U,8U,16U}) for(float dt:{0.008f,0.016f,0.1f}) {
        Fixture fixture; enginefuncs_t hooks{}; p401::setup(fixture,hooks,actors);
        auto& owner=astrabot::adapter::metamod::lifecycleCoordinator();
        const auto reporter=owner.registry().currentPlayer(1),target=owner.registry().currentPlayer(32);
        for(unsigned i=0;i<40;++i) {
            (void)owner.report(reporter,target); p401::step(fixture,dt);
            const auto& d=owner.world().reports().diagnostics();
            assert(d.frameDelivered<=32 && d.frameVisits<=32*31 && d.queued<=256);
        }
        assert((owner.world().reports().diagnostics().delivered>0)==(actors>1));
        detach();
    }
}
void run() { publication(); reentrant(); matrix(); p401::movementCoexistence(); }
}
