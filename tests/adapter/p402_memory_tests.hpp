// SPDX-License-Identifier: MPL-2.0
// Reuses the P4-01 fake-engine fixture and actual StartFrame hook.
namespace p402 {
void memoryLifecycle() {
    using namespace astrabot;
    using namespace p401;
    Fixture fixture; enginefuncs_t hooks{}; setup(fixture,hooks,1);
    auto& owner = adapter::metamod::lifecycleCoordinator();
    const auto observer = owner.fakeClient().activePlayer();
    const auto latest = [&]() { return owner.vision().memory().latest(observer); };
    assert(latest() && latest()->count == 1);
    const auto first = latest()->memories[0];
    mode = 1; human.v.origin.x = 200; step(fixture,2.5f);
    assert(latest()->count == 1 && latest()->memories[0].lastKnownPosition.x == first.lastKnownPosition.x);
    assert(latest()->memories[0].lastSeenMicros == first.lastSeenMicros);
    assert(std::abs(latest()->memories[0].confidence-0.5) < 0.001);
    step(fixture,2.51f); assert(latest()->count == 0);
    mode = 0; step(fixture,0.2f); assert(latest()->count == 1);
    assert(latest()->memories[0].lastKnownPosition.x == 200 && latest()->memories[0].confidence == 1);
    const auto fresh = latest()->memories[0];
    step(fixture,0.001f); assert(latest()->memories[0].lastSeenMicros == fresh.lastSeenMicros);
    assert(latest()->memories[0].confidence < 1);
    human.v.health = 0; step(fixture,0.001f); assert(latest()->count == 0);
    human.v.health = 100; step(fixture,0.2f); assert(latest()->count == 1);
    human.v.iuser1 = 1; step(fixture,0.001f); assert(latest()->count == 0);
    human.v.iuser1 = 0; step(fixture,0.2f); assert(latest()->count == 1);
    human.v.flags |= FL_SPECTATOR; step(fixture,0.001f); assert(latest()->count == 0);
    human.v.flags = FL_CLIENT; step(fixture,0.2f); assert(latest()->count == 1);
    ++human.serialnumber; step(fixture,0.001f); assert(latest()->count == 0);
    step(fixture,0.2f); assert(latest()->count == 1);
    mode = 9; step(fixture,0.2f); assert(latest()->count == 0);
    mode = 0; human.free = 0; ++human.serialnumber; step(fixture,0.2f); assert(latest()->count == 1);
    mode = 11; step(fixture,0.2f); assert(latest()->count == 0);
    mode = 0; step(fixture,0.2f); assert(latest()->count == 1);
    mode = 12; step(fixture,0.2f); assert(!latest());
    mode = 0; fixture.entity.v.health = 100; step(fixture,0.2f); step(fixture,0.2f);
    assert(latest() && latest()->count == 1);
    const auto time = fixture.engineGlobals.time;
    fixture.engineGlobals.time -= 1; step(fixture,0.001f); assert(!latest());
    assert(owner.vision().memory().diagnostics().reason == core::world::MemoryReason::InvalidFrame);
    fixture.engineGlobals.time = time; mode = 1; step(fixture,0.2f);
    assert(latest() && latest()->count == 0);
    mode = 0; step(fixture,0.2f); assert(latest()->count == 1);
    fixture.engineGlobals.time = (std::numeric_limits<float>::quiet_NaN)(); step(fixture); assert(!latest());
    fixture.engineGlobals.time = time+1; mode = 1; step(fixture,0.2f); assert(latest() && latest()->count == 0);
    mode = 0; step(fixture,0.2f); assert(latest()->count == 1);
    mode = 10; step(fixture,0.2f); assert(!latest());
    mode = 0; assert(owner.registry().activateMap(32)); step(fixture,0.2f); assert(!latest());
    detach();
}
void matrix() {
    using namespace astrabot;
    using namespace p401;
    for (const unsigned actors : {1U,8U,16U}) for (const float dt : {0.008f,0.016f,0.1f}) {
        Fixture fixture; enginefuncs_t hooks{}; setup(fixture,hooks,actors);
        auto& owner = adapter::metamod::lifecycleCoordinator();
        const auto moves = gRunPlayerMoveCalls;
        std::array<std::uint64_t,16> seen{};
        for (unsigned i=0; i<actors; ++i) {
            const auto observer = owner.registry().currentPlayer(static_cast<std::uint16_t>(i+1));
            const auto* snapshot = owner.vision().memory().latest(observer);
            assert(snapshot && snapshot->count == 1); seen[i] = snapshot->memories[0].lastSeenMicros;
        }
        mode = 1; human.v.origin.x = 300;
        for (unsigned frame=0; frame<20; ++frame) {
            step(fixture,dt);
            const auto& d = owner.vision().memory().diagnostics();
            assert(d.frameVisits <= 32*31 && d.frameObservations == 0);
            for (unsigned i=0; i<actors; ++i) {
                const auto observer = owner.registry().currentPlayer(static_cast<std::uint16_t>(i+1));
                const auto* snapshot = owner.vision().memory().latest(observer);
                assert(snapshot && snapshot->count == 1);
                assert(snapshot->memories[0].lastSeenMicros == seen[i]);
                assert(snapshot->memories[0].lastKnownPosition.x == 100 && snapshot->memories[0].confidence < 1);
            }
        }
        assert(gRunPlayerMoveCalls == moves);
        mode = 0;
        for (unsigned frame=0; frame<40; ++frame) {
            step(fixture,dt);
            assert(owner.vision().memory().diagnostics().frameObservations <= 4*31);
            unsigned expectedTraces = 0;
            for (unsigned i=0; i<actors; ++i) {
                const auto observer = owner.registry().currentPlayer(static_cast<std::uint16_t>(i+1));
                const auto& vision = owner.vision().observations();
                const auto* batch = vision.latest(observer);
                if (batch && batch->stamp.tick == owner.registry().currentTick()) expectedTraces += vision.diagnostics(observer)->traces;
            }
            assert(traceCalls == expectedTraces); // Memory adds zero traces.
        }
        for (unsigned i=0; i<actors; ++i) {
            const auto* snapshot = owner.vision().memory().latest(owner.registry().currentPlayer(static_cast<std::uint16_t>(i+1)));
            assert(snapshot && snapshot->count == 1 && snapshot->memories[0].lastKnownPosition.x == 300);
        }
        detach();
    }
}
void run() { memoryLifecycle(); matrix(); p401::movementCoexistence(); }
} // namespace p402
