// SPDX-License-Identifier: MPL-2.0
namespace p404 {
namespace p = astrabot::core::perception;
void hooksFor(Fixture& fixture,enginefuncs_t& post) {
    fixture.globals.status = MRES_IGNORED;
    int version = ENGINE_INTERFACE_VERSION;
    assert(fixture.callbacks.pfnGetEngineFunctions_Post && fixture.callbacks.pfnGetEngineFunctions_Post(&post,&version));
}
void precache(Fixture& fixture,enginefuncs_t& post,unsigned short index,const char* name) {
    fixture.globals.orig_ret = &index;
    gEngineHooks->pfnPrecacheEvent(1,name);
    assert(post.pfnPrecacheEvent(1,name) == 0);
    fixture.globals.orig_ret = nullptr; assert(fixture.globals.mres == MRES_IGNORED);
}
void footstep(enginefuncs_t& post,float volume=0.5f,float attenuation=1) {
    gEngineHooks->pfnEmitSound(&p401::human,CHAN_BODY,"player/pl_step1.wav",volume,attenuation,0,100);
    post.pfnEmitSound(&p401::human,CHAN_BODY,"player/pl_step1.wav",volume,attenuation,0,100);
}
void publication() {
    using namespace astrabot;
    using namespace p401;
    Fixture fixture; enginefuncs_t hooks{},post{}; setup(fixture,hooks,1); hooksFor(fixture,post);
    auto& owner = adapter::metamod::lifecycleCoordinator();
    const auto observer = owner.fakeClient().activePlayer();
    const auto sounds = [&]() { return owner.sound().memory().latest(observer); };
    assert(sounds() && sounds()->count == 0);
    const auto lastSeen = owner.vision().memory().latest(observer)->memories[0].lastSeenMicros;
    footstep(post); footstep(post);
    assert(owner.sound().diagnostics().queued == 1 && owner.sound().diagnostics().duplicates == 1);
    assert(sounds()->count == 0); // Hook ingestion does not publish before StartFrame.
    human.v.origin.x = 4000; fixture.entity.v.origin.x = 6000; mode = 1;
    step(fixture,0.1f); // Both moved after emission; audible at the captured position.
    assert(sounds()->count == 1 && sounds()->sounds[0].observation.region.x == 0);
    assert(sounds()->sounds[0].observation.kind == p::SoundKind::Footstep);
    assert(sounds()->sounds[0].confidence > 0 && sounds()->sounds[0].confidence < 0.5);
    assert(owner.vision().memory().latest(observer)->memories[0].lastSeenMicros == lastSeen);
    fixture.entity.v.origin.x = 0; human.v.origin.x = 100;
    gEngineHooks->pfnEmitSound(&human,CHAN_BODY,"unknown.wav",1,1,0,100);
    post.pfnEmitSound(&human,CHAN_BODY,"unknown.wav",1,1,0,100);
    const auto unknown = owner.sound().diagnostics().unknown; assert(unknown != 0);
    footstep(post,0); footstep(post,1,-1);
    assert(owner.sound().diagnostics().invalid >= 2 && owner.sound().diagnostics().queued == 0);
    float source[]{256,0,0};
    gEngineHooks->pfnEmitAmbientSound(nullptr,source,"weapons/c4_explode1.wav",1,1,0,100);
    post.pfnEmitAmbientSound(nullptr,source,"weapons/c4_explode1.wav",1,1,0,100);
    precache(fixture,post,413,"events/ak47.sc");
    assert(owner.sound().events().find(413) == adapter::cstrike::EventKind::Gunshot);
    float zero[3]{};
    gEngineHooks->pfnPlaybackEvent(0,&human,413,0,zero,zero,0,0,0,0,0,0);
    post.pfnPlaybackEvent(0,&human,413,0,zero,zero,0,0,0,0,0,0);
    gEngineHooks->pfnPlaybackEvent(0,&human,999,0,zero,zero,0,0,0,0,0,0);
    post.pfnPlaybackEvent(0,&human,999,0,zero,zero,0,0,0,0,0,0);
    assert(owner.sound().diagnostics().unknown == unknown+1);
    fixture.globals.status = MRES_SUPERCEDE; footstep(post);
    assert(owner.sound().diagnostics().queued == 2); fixture.globals.status = MRES_IGNORED;
    step(fixture,0.1f); assert(sounds()->count == 3);
    assert(sounds()->sounds[1].observation.region.x == 1 && sounds()->sounds[1].observation.kind == p::SoundKind::Explosion);
    assert(sounds()->sounds[2].observation.region.x == 0 && sounds()->sounds[2].observation.kind == p::SoundKind::Gunshot);
    footstep(post); assert(owner.sound().diagnostics().queued == 1);
    p403::roundEvent(hooks); assert(owner.sound().diagnostics().queued == 0 && !sounds());
    step(fixture,0.1f); assert(sounds() && sounds()->count == 0);
    footstep(post); const auto oldTime = fixture.engineGlobals.time;
    fixture.engineGlobals.time -= 1; step(fixture,0);
    assert(owner.sound().diagnostics().queued == 0 && !sounds());
    fixture.engineGlobals.time = oldTime; step(fixture,0.1f); assert(sounds() && sounds()->count == 0);
    footstep(post); owner.clientDisconnect(&fixture.entity);
    assert(!sounds()); step(fixture,0.1f); assert(!sounds());
    owner.serverDeactivate(); assert(owner.sound().events().find(413) == adapter::cstrike::EventKind::Unknown);
    detach();
}
void queueAndRetirement() {
    using namespace astrabot;
    using namespace p401;
    Fixture fixture; enginefuncs_t hooks{},post{}; setup(fixture,hooks,1); hooksFor(fixture,post);
    auto& owner = adapter::metamod::lifecycleCoordinator(); const auto observer = owner.fakeClient().activePlayer();
    mode = 1;
    for (unsigned i=0; i<266; ++i) { human.v.origin.x = 100+static_cast<float>(i)*0.25f; footstep(post); }
    assert(owner.sound().diagnostics().queued == 256 && owner.sound().diagnostics().overflow == 10);
    const auto moveCalls = gRunPlayerMoveCalls;
    std::uint64_t firstRetainedSequence{};
    for (unsigned frame=0; frame<8; ++frame) {
        step(fixture,0.001f);
        assert(owner.sound().diagnostics().frameEvents == 32 && owner.sound().diagnostics().frameAudienceChecks == 32);
        assert(owner.sound().diagnostics().queued == 256-(frame+1)*32);
        const auto* snapshot = owner.sound().memory().latest(observer);
        assert(snapshot && snapshot->count == 16);
        if (frame == 0) firstRetainedSequence = snapshot->sounds[0].observation.identity.sequence;
        assert(snapshot->sounds[0].observation.identity.sequence == firstRetainedSequence+frame*32);
        assert(snapshot->sounds[15].observation.identity.sequence == firstRetainedSequence+frame*32+15);
        assert(owner.sound().memory().diagnostics().frameVisits <= 32*16);
    }
    assert(gRunPlayerMoveCalls == moveCalls);
    for (unsigned i=0; i<64; ++i) { human.v.origin.x = static_cast<float>(i); footstep(post); }
    fixture.entity.v.health = 0; step(fixture,0.001f);
    assert(!owner.sound().memory().latest(observer));
    fixture.entity.v.health = 100; step(fixture,0.001f);
    assert(owner.sound().memory().latest(observer)->count == 0); // Pre-death queued audience cannot revive.
    assert(owner.sound().diagnostics().recipientRejected >= 64);
    human.v.origin.x = 100; footstep(post); step(fixture,3.1f);
    assert(owner.sound().memory().latest(observer)->count == 0 && owner.sound().diagnostics().expired > 0);
    detach();
}
void matrix() {
    using namespace astrabot;
    using namespace p401;
    for (unsigned actors : {1U,8U,16U}) for (float dt : {0.008f,0.016f,0.1f}) {
        Fixture fixture; enginefuncs_t hooks{},post{}; setup(fixture,hooks,actors); hooksFor(fixture,post);
        auto& owner = adapter::metamod::lifecycleCoordinator(); mode = 1;
        if (actors > 1) fixture.matrixEntity(actors)->v.origin.x = 10000;
        const auto moves = gRunPlayerMoveCalls;
        for (unsigned frame=0; frame<80; ++frame) {
            footstep(post); step(fixture,dt);
            const auto& d = owner.sound().diagnostics();
            assert(d.frameEvents <= 32 && d.frameAudienceChecks <= 32*32 && d.queued <= 256);
            assert(owner.sound().memory().diagnostics().frameVisits <= 32*16);
        }
        assert(gRunPlayerMoveCalls == moves);
        for (unsigned slot=1; slot<=actors; ++slot) {
            const auto* snapshot = owner.sound().memory().latest(owner.registry().currentPlayer(static_cast<std::uint16_t>(slot)));
            assert(snapshot && snapshot->count <= 16);
            assert((snapshot->count != 0) == (actors == 1 || slot != actors));
        }
        detach();
    }
}
enginefuncs_t* reentrantPost{};
unsigned retirement{};
void retireDuringVision() {
    p401::extraTrace = nullptr;
    footstep(*reentrantPost);
    auto& owner = astrabot::adapter::metamod::lifecycleCoordinator();
    if (retirement == 0) owner.clientDisconnect(&gFixture->entity);
    else if (retirement == 1) owner.serverDeactivate();
    else gFixture->entity.v.health = 0;
}
void reentrantRetirement() {
    for (retirement=0; retirement<3; ++retirement) {
        Fixture fixture; enginefuncs_t hooks{},post{};
        p401::setup(fixture,hooks,1); hooksFor(fixture,post); reentrantPost = &post;
        auto& owner = astrabot::adapter::metamod::lifecycleCoordinator();
        const auto observer = owner.fakeClient().activePlayer();
        p401::extraTrace = &retireDuringVision;
        p401::step(fixture,0.2f);
        assert(p401::extraTrace == nullptr);
        assert(!owner.sound().memory().latest(observer));
        if (retirement == 2) {
            fixture.entity.v.health = 100;
            p401::step(fixture,0.1f);
            assert(owner.sound().memory().latest(observer)->count == 0);
        }
        detach();
    }
    reentrantPost = nullptr;
}
void hookTransaction() {
    Fixture fixture; enginefuncs_t hooks{},post{};
    p401::setup(fixture,hooks,1); hooksFor(fixture,post);
    auto& owner = astrabot::adapter::metamod::lifecycleCoordinator();
    const auto before = [&]() { hooks.pfnEmitSound(&p401::human,CHAN_BODY,"player/pl_step1.wav",0.5f,1,0,100); };
    const auto after = [&]() { post.pfnEmitSound(&p401::human,CHAN_BODY,"player/pl_step1.wav",0.5f,1,0,100); };
    before(); ++p401::human.serialnumber; after();
    assert(owner.sound().diagnostics().queued == 0);
    before(); p403::roundEvent(hooks); after();
    assert(owner.sound().diagnostics().queued == 0);
    before(); owner.serverDeactivate(); owner.serverActivate(32); after();
    assert(owner.sound().diagnostics().queued == 0);
    detach();
}
void run() { publication(); queueAndRetirement(); matrix(); reentrantRetirement(); hookTransaction(); p401::movementCoexistence(); }
} // namespace p404
