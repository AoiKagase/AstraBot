// SPDX-License-Identifier: MPL-2.0
namespace p405 {
namespace p = astrabot::core::perception;
void fade(enginefuncs_t& hooks,edict_t* target,int color=255,int flags=0,int units=4096) {
    hooks.pfnMessageBegin(MSG_ONE,15,nullptr,target);
    hooks.pfnWriteShort(units); hooks.pfnWriteShort(units); hooks.pfnWriteShort(flags);
    hooks.pfnWriteByte(color); hooks.pfnWriteByte(color); hooks.pfnWriteByte(color); hooks.pfnWriteByte(255);
    hooks.pfnMessageEnd();
}
void smoke(enginefuncs_t& hooks,enginefuncs_t& post,float x=50,float y=1,float z=64,int mode=1) {
    float origin[]{x,y,z}, angles[3]{};
    hooks.pfnPlaybackEvent(0,nullptr,501,0,origin,angles,0,0,0,mode,0,0);
    post.pfnPlaybackEvent(0,nullptr,501,0,origin,angles,0,0,0,mode,0,0);
}
void setup(Fixture& fixture,enginefuncs_t& hooks,enginefuncs_t& post,unsigned actors=1) {
    p401::setup(fixture,hooks,actors); p404::hooksFor(fixture,post);
    p404::precache(fixture,post,501,"events/createsmoke.sc");
    assert(astrabot::adapter::metamod::lifecycleCoordinator().smokeCapability());
}
void flashAndMemory() {
    Fixture fixture; enginefuncs_t hooks{},post{}; setup(fixture,hooks,post,2);
    auto& owner = astrabot::adapter::metamod::lifecycleCoordinator();
    const auto actor = owner.registry().currentPlayer(1), other = owner.registry().currentPlayer(2);
    assert(owner.vision().memory().latest(actor));
    assert(owner.flashCapability());
    const auto before = owner.vision().memory().latest(actor)->memories[0].lastSeenMicros;
    fade(hooks,&fixture.entity);
    assert(owner.visualEffects().diagnostics().flashAccepted == 1);
    p401::step(fixture,0.2f);
    assert(owner.vision().observations().latest(actor));
    assert(owner.vision().observations().latest(other));
    assert(owner.vision().observations().latest(actor)->count == 0);
    assert(owner.vision().observations().latest(other)->count > 0);
    assert(owner.vision().memory().latest(actor)->memories[0].lastSeenMicros == before);
    assert(owner.vision().memory().latest(actor)->memories[0].confidence < 1);
    p401::step(fixture,1.81f);
    assert(owner.vision().observations().latest(actor));
    assert(owner.vision().observations().latest(actor)->count > 0);
    fade(hooks,&fixture.entity,0); fade(hooks,&fixture.entity,255,1);
    assert(owner.visualEffects().diagnostics().flashAccepted == 1);
    p401::step(fixture,0.2f); assert(owner.vision().observations().latest(actor)->count > 0);
    fade(hooks,&fixture.entity); fixture.entity.v.health = 0; p401::step(fixture);
    fixture.entity.v.health = 100; p401::step(fixture);
    assert(owner.visualEffects().blocked(actor,{0,0,0},{1,0,0}) == p::Reason::None);
    p401::step(fixture); // Re-enter the existing staggered vision schedule after revival.
    assert(owner.vision().observations().latest(actor));
    assert(owner.vision().observations().latest(actor)->count > 0);
    detach();
}
void smokeAndSamples() {
    Fixture fixture; enginefuncs_t hooks{},post{}; setup(fixture,hooks,post);
    auto& owner = astrabot::adapter::metamod::lifecycleCoordinator();
    const auto actor = owner.fakeClient().activePlayer();
    const auto before = owner.vision().memory().latest(actor)->memories[0].lastSeenMicros;
    smoke(hooks,post); smoke(hooks,post);
    assert(owner.visualEffects().diagnostics().regions == 1 && owner.visualEffects().diagnostics().duplicates == 1);
    smoke(hooks,post,50,1,64,4); assert(owner.visualEffects().diagnostics().regions == 1);
    p401::step(fixture,0.2f);
    assert(p401::traceCalls == 0 && owner.vision().observations().latest(actor)->count == 0);
    assert(owner.vision().memory().latest(actor)->memories[0].lastSeenMicros == before);
    p401::step(fixture,5); assert(owner.vision().memory().latest(actor)->count == 0);
    p401::step(fixture,17);
    assert(owner.visualEffects().diagnostics().regions == 0 && owner.vision().observations().latest(actor)->count == 1);
    smoke(hooks,post,100,1,178); // Head segment touches smoke, torso segment clears.
    p401::step(fixture,0.2f);
    assert(owner.vision().observations().latest(actor)->count == 1);
    assert(owner.vision().observations().latest(actor)->observations[0].position.z == 36);
    p403::roundEvent(hooks); assert(owner.visualEffects().diagnostics().regions == 0);
    p401::step(fixture,0.2f);
    for (unsigned i=0;i<33;++i) smoke(hooks,post,10000+static_cast<float>(i)*300,0,0);
    assert(owner.visualEffects().diagnostics().regions == 32 && owner.visualEffects().diagnostics().overflowActive);
    p401::step(fixture,0.2f); assert(owner.vision().observations().latest(actor)->count == 0);
    p401::step(fixture,22); assert(!owner.visualEffects().diagnostics().overflowActive);
    assert(owner.vision().observations().latest(actor)->count == 1);
    smoke(hooks,post); owner.serverDeactivate(); assert(owner.visualEffects().diagnostics().regions == 0);
    assert(!owner.smokeCapability());
    detach();
}
enginefuncs_t* injectionPost{};
unsigned injection{};
void inject() {
    p401::extraTrace = nullptr;
    if (injection == 0) fade(*gEngineHooks,&gFixture->entity);
    else smoke(*gEngineHooks,*injectionPost);
}
void reentrant() {
    for (injection=0;injection<2;++injection) {
        Fixture fixture; enginefuncs_t hooks{},post{}; setup(fixture,hooks,post); injectionPost = &post;
        auto& owner = astrabot::adapter::metamod::lifecycleCoordinator(); const auto actor = owner.fakeClient().activePlayer();
        const auto before = owner.vision().memory().latest(actor)->memories[0].lastSeenMicros;
        p401::extraTrace = &inject; p401::step(fixture,0.2f);
        assert(p401::extraTrace == nullptr);
        assert(!owner.vision().observations().latest(actor));
        assert(owner.vision().memory().latest(actor)->memories[0].lastSeenMicros == before);
        detach();
    }
    injectionPost = nullptr;
}
void matrix() {
    for (unsigned actors : {1U,8U,16U}) for (float dt : {0.008f,0.016f,0.1f}) {
        Fixture fixture; enginefuncs_t hooks{},post{}; setup(fixture,hooks,post,actors);
        auto& owner = astrabot::adapter::metamod::lifecycleCoordinator();
        smoke(hooks,post);
        const auto moves = gRunPlayerMoveCalls;
        for (unsigned i=0;i<40;++i) {
            p401::step(fixture,dt);
            assert(p401::traceCalls == 0 && owner.visualEffects().diagnostics().regions == 1);
            assert(owner.vision().observations().frameUpdates() <= 4);
        }
        for (unsigned slot=1;slot<=actors;++slot) {
            const auto* batch = owner.vision().observations().latest(owner.registry().currentPlayer(static_cast<std::uint16_t>(slot)));
            assert(batch && batch->count == 0);
        }
        assert(gRunPlayerMoveCalls == moves);
        detach();
    }
}
void run() { flashAndMemory(); smokeAndSamples(); reentrant(); matrix(); }
}
