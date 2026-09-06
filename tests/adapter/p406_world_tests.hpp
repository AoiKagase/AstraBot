// SPDX-License-Identifier: MPL-2.0
namespace p406 {
void publication() {
    Fixture fixture; enginefuncs_t hooks{},post{};
    p401::setup(fixture,hooks,2); p404::hooksFor(fixture,post);
    auto& owner = astrabot::adapter::metamod::lifecycleCoordinator();
    const auto actor = owner.registry().currentPlayer(1);
    auto snapshot = owner.world().latest(actor); assert(snapshot);
    assert(snapshot->visual == owner.vision().memory().latest(actor));
    assert(snapshot->sounds == owner.sound().memory().latest(actor));
    const auto seen = snapshot->visual->memories[0].lastSeenMicros;
    const auto published = owner.world().diagnostics().publications;
    p404::footstep(post);
    assert(owner.world().diagnostics().publications == published);
    assert(owner.world().latest(actor)->sounds->count == 0);
    p401::mode = 1; p401::human.v.origin.x = 2000;
    p401::step(fixture,0.2f); snapshot = owner.world().latest(actor); assert(snapshot);
    assert(snapshot->sounds->count == 1 && snapshot->visual->count == 1);
    assert(snapshot->visual->memories[0].lastSeenMicros == seen);
    assert(snapshot->visual->memories[0].lastKnownPosition.x == 100);
    assert(snapshot->oldestVisualAgeMicros > 0 && snapshot->oldestSoundAgeMicros > 0);
    assert(snapshot->maxReceiptDelayMicros > 0);
    assert(snapshot->queues.soundPending == 0);
    assert(snapshot->stamp.tick == owner.registry().currentTick());
    p403::roundEvent(hooks); assert(!owner.world().latest(actor));
    p401::step(fixture); snapshot = owner.world().latest(actor);
    assert(snapshot && snapshot->visual->count == 0 && snapshot->sounds->count == 0);
    owner.clientDisconnect(&fixture.entity); assert(!owner.world().latest(actor));
    detach();
}
unsigned during{};
void inspectDuringTrace() {
    auto& owner = astrabot::adapter::metamod::lifecycleCoordinator();
    assert(!owner.world().latest(owner.fakeClient().activePlayer())); ++during;
}
void reentrant() {
    for (unsigned mode : {0U,9U,10U,12U}) {
        Fixture fixture; enginefuncs_t hooks{}; p401::setup(fixture,hooks,1);
        auto& owner = astrabot::adapter::metamod::lifecycleCoordinator();
        const auto actor = owner.fakeClient().activePlayer(); during = 0;
        p401::extraTrace = &inspectDuringTrace; p401::mode = mode;
        p401::step(fixture,0.2f); assert(during > 0); p401::extraTrace = nullptr;
        const auto snapshot = owner.world().latest(actor);
        if (mode == 0) assert(snapshot && snapshot->visual->count == 1);
        if (mode == 9) assert(!snapshot || snapshot->visual->count == 0);
        if (mode == 10 || mode == 12) assert(!snapshot);
        detach();
    }
}
void matrix() {
    for (unsigned actors : {1U,8U,16U}) for (float dt : {0.008f,0.016f,0.1f}) {
        Fixture fixture; enginefuncs_t hooks{},post{};
        p401::setup(fixture,hooks,actors); p404::hooksFor(fixture,post);
        auto& owner = astrabot::adapter::metamod::lifecycleCoordinator();
        for (unsigned i=0;i<40;++i) {
            p404::footstep(post); p401::step(fixture,dt);
            assert(owner.world().diagnostics().frameProcessed <= 1056);
            assert(owner.world().visual().diagnostics().frameVisits <= 32*31);
            assert(owner.world().sounds().diagnostics().frameVisits <= 32*16);
            assert(owner.sound().diagnostics().frameAudienceChecks <= 1024);
            for (std::uint16_t slot=1;slot<=actors;++slot) {
                const auto snapshot = owner.world().latest(owner.registry().currentPlayer(slot));
                assert(snapshot && snapshot->stamp.tick == owner.registry().currentTick());
                assert(snapshot->visual->count <= 31 && snapshot->sounds->count <= 16);
            }
        }
        detach();
    }
}
void run() { publication(); reentrant(); matrix(); p401::movementCoexistence(); }
}
