// SPDX-License-Identifier: MPL-2.0
namespace p407 {
void publication() {
    Fixture fixture; enginefuncs_t hooks{}; p401::setup(fixture,hooks,1);
    auto& owner=astrabot::adapter::metamod::lifecycleCoordinator(); const auto actor=owner.fakeClient().activePlayer();
    assert(!owner.world().latest(actor)->distributions[0]);
    route_test::Area a{1,{{0,0,0},{150,100,0},0,0}}, b{2,{{200,0,0},{350,100,0},0,0}};
    a.targets[1]={2};
    assert(owner.navConsole().publish(owner.registry().mapGeneration(),route_test::snapshot({b,a})).isNone());
    p401::step(fixture,0.001f);
    auto snapshot=owner.world().latest(actor); assert(snapshot && snapshot->distributions[0]);
    assert(snapshot->distributions[0]->count==1 && snapshot->distributions[0]->areas[0].area==1);
    p401::mode=1; p401::human.v.origin.x=3000; p401::step(fixture,0.2f);
    snapshot=owner.world().latest(actor); const auto* distribution=snapshot->distributions[0];
    assert(distribution && distribution->count==2 && distribution->areas[0].weight==0.5);
    assert(distribution->areas[1].area==2 && distribution->areas[1].weight==0.5);
    assert(snapshot->visual->memories[0].lastKnownPosition.x==100);
    p401::mode=0; p401::human.v.origin.x=210; p401::step(fixture,0.2f);
    distribution=owner.world().latest(actor)->distributions[0]; assert(distribution && distribution->count==1);
    assert(distribution->areas[0].area==2 && distribution->areas[0].weight==1);
    a.targets={};
    assert(owner.navConsole().publish(owner.registry().mapGeneration(),route_test::snapshot({a})).isNone());
    assert(!owner.world().latest(actor)->distributions[0]); // NAV retirement is immediate.
    p401::step(fixture,0.001f); distribution=owner.world().latest(actor)->distributions[0];
    assert(distribution && !distribution->available);
    p403::roundEvent(hooks); assert(!owner.world().latest(actor));
    detach();
}
void matrix() {
    for(unsigned actors:{1U,8U,16U}) for(float dt:{0.008f,0.016f,0.1f}) {
        Fixture fixture; enginefuncs_t hooks{}; p401::setup(fixture,hooks,actors);
        auto& owner=astrabot::adapter::metamod::lifecycleCoordinator();
        route_test::Area a{1,{{0,0,0},{150,100,0},0,0}},b{2,{{200,0,0},{350,100,0},0,0}};
        a.targets[1]={2}; assert(owner.navConsole().publish(owner.registry().mapGeneration(),route_test::snapshot({a,b})).isNone());
        p401::mode=1;
        for(unsigned i=0;i<40;++i) {
            p401::step(fixture,dt); const auto& d=owner.distributions().diagnostics();
            assert(d.frameConnections<=256 && d.frameMappings<=32 && d.frameVisits<=2048);
        }
        for(std::uint16_t slot=1;slot<=actors;++slot) {
            const auto snapshot=owner.world().latest(owner.registry().currentPlayer(slot));
            assert(snapshot && snapshot->distributions[0] && snapshot->distributions[0]->available);
        }
        detach();
    }
}
void run() { publication(); matrix(); p401::movementCoexistence(); }
}
