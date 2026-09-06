// SPDX-License-Identifier: MPL-2.0
// Included in the existing fake-engine fixture namespace.
namespace p401 {
namespace p = astrabot::core::perception;
edict_t human{}, wall{}, door{};
unsigned traceCalls{}, mode{};
void (*extraTrace)(){};
edict_t* indexed(int slot) {
    if (slot == 32) return &human;
    return slot >= 1 && slot <= 16 ? gFixture->matrixEntity(static_cast<unsigned>(slot)) : nullptr;
}
int index(const edict_t* entity) {
    if (entity == &human) return 32;
    for (unsigned slot=1; slot<=16; ++slot)
        if (entity == gFixture->matrixEntity(slot)) return static_cast<int>(slot);
    return 0;
}
void trace(const float* start,const float* end,int flags,edict_t* observer,TraceResult* result) {
    assert(flags == 0 && observer && observer != &human);
    ++traceCalls; *result = {}; result->flFraction = 1;
    result->vecEndPos = Vector(end[0],end[1],end[2]);
    if (mode == 1 || mode == 2 || mode == 3 || (mode == 4 && end[2] > 36) || mode == 5) {
        result->flFraction = 0.5f;
        result->vecEndPos = Vector((start[0]+end[0])/2,(start[1]+end[1])/2,(start[2]+end[2])/2);
        result->pHit = mode == 2 ? &door : mode == 3 ? &gFixture->secondEntity : mode == 5 ? &human : &wall;
    }
    if (mode == 6) result->flFraction = (std::numeric_limits<float>::quiet_NaN)();
    if (mode == 7) result->vecEndPos.x += 10;
    if (mode == 8) result->fStartSolid = 1;
    if (mode == 9) {
        astrabot::adapter::metamod::lifecycleCoordinator().clientDisconnect(&human);
        human.free = 1;
    }
    if (mode == 10) astrabot::adapter::metamod::lifecycleCoordinator().serverDeactivate();
    if (mode == 11) ++human.serialnumber;
    if (mode == 12) observer->v.health = 0;
    if (extraTrace) extraTrace();
}
void step(Fixture& fixture,float dt=0.1f) {
    traceCalls = 0; fixture.engineGlobals.time += dt;
    astrabot::adapter::metamod::startFrameHook(); // actual registered lifecycle hook
    assert(traceCalls <= 248);
}
void setup(Fixture& fixture,enginefuncs_t& hooks,unsigned actors) {
    using namespace astrabot;
    gSimulateNav = false;
    fixture.matrixSlot = 1; fixture.multiClient = true;
    fixture.engine.pfnIndexOfEdict = &index;
    fixture.engine.pfnPEntityOfEntIndex = &indexed;
    fixture.engine.pfnTraceLine = &trace;
    human = {}; mode = 0; traceCalls = 0;
    extraTrace = nullptr;
    for (unsigned slot=1; slot<=16; ++slot) fixture.matrixEntity(slot)->free = 1;
    activate(fixture);
    int version = ENGINE_INTERFACE_VERSION;
    assert(GetEngineFunctions(&hooks,&version)); gEngineHooks = &hooks;
    auto& owner = adapter::metamod::lifecycleCoordinator();
    for (unsigned slot=1; slot<=actors; ++slot) {
        fixture.matrixSlot = slot;
        auto* entity = fixture.matrixEntity(slot); entity->free = 0;
        const auto created = owner.createBot("AstraBot-Vision",{adapter::cstrike::Team::Terrorist,1});
        assert(created.succeeded());
        sendVguiMenu(hooks,11,entity,2,1); step(fixture);
        sendVguiMenu(hooks,11,entity,26,1); step(fixture);
        sendTeamInfo(hooks,13,static_cast<int>(slot),"TERRORIST");
        assert(owner.joinState(created.player)->phase() == adapter::cstrike::JoinPhase::Joined);
        entity->v.flags = FL_FAKECLIENT | FL_ONGROUND;
        entity->v.health = 100; entity->v.deadflag = DEAD_NO;
        entity->v.origin = Vector(0,static_cast<float>(slot),36);
        entity->v.view_ofs = Vector(0,0,28); entity->v.v_angle = Vector(0,0,0);
        entity->v.mins = Vector(-16,-16,-36); entity->v.maxs = Vector(16,16,36);
        entity->v.maxspeed = 250;
    }
    human.v.flags = FL_CLIENT; human.v.health = 100; human.serialnumber = 5;
    human.v.origin = Vector(100,1,36); human.v.view_ofs = Vector(0,0,28);
    human.v.mins = Vector(-16,-16,-36); human.v.maxs = Vector(16,16,36);
    for (unsigned i=0; i<10; ++i) step(fixture);
}
void observationsAndRetirement() {
    using namespace astrabot;
    Fixture fixture; enginefuncs_t hooks{}; setup(fixture,hooks,1);
    auto& owner = adapter::metamod::lifecycleCoordinator();
    const auto observer = owner.fakeClient().activePlayer();
    const auto target = owner.registry().currentPlayer(32);
    const auto latest = [&]() { return owner.vision().observations().latest(observer); };
    const auto diagnostics = [&]() { return owner.vision().observations().diagnostics(observer); };
    assert(target.isValid() && latest() && latest()->count == 1);
    assert(latest()->observations[0].target == target && latest()->observations[0].position.z == 64);
    const auto stamp = latest()->stamp;
    step(fixture,0.001f);
    assert(latest()->stamp.tick == stamp.tick && latest()->stamp.timeMicros == stamp.timeMicros);
    // Closed wall, closed door, another player; then open door, partial body,
    // and directly hitting the intended target all use the real adapter query.
    for (unsigned m=0; m<=8; ++m) {
        mode = m; step(fixture,0.2f);
        assert(latest());
        const bool visible = m == 0 || m == 4 || m == 5;
        assert(latest()->count == (visible ? 1U : 0U));
        if (m == 4) assert(latest()->observations[0].position.z == 36 && diagnostics()->traces == 2);
        if (m == 6 || m == 7) assert(diagnostics()->reason == p::Reason::InvalidTrace);
    }
    mode = 0;
    fixture.engine.pfnTraceLine = nullptr; step(fixture,0.2f);
    assert(latest()->count == 0 && diagnostics()->reason == p::Reason::MissingEngine);
    fixture.engine.pfnTraceLine = &trace; step(fixture,0.2f); assert(latest()->count == 1);
    human.v.health = 0; step(fixture,0.001f); assert(latest()->count == 0);
    human.v.health = 100; human.v.iuser1 = 1; step(fixture,0.2f); assert(latest()->count == 0);
    human.v.iuser1 = 0; human.v.flags |= FL_SPECTATOR; step(fixture,0.2f); assert(latest()->count == 0);
    human.v.flags = FL_CLIENT; step(fixture,0.2f); assert(latest()->count == 1);
    ++human.serialnumber; step(fixture,0.001f);
    assert(latest()->count == 0 && owner.registry().currentPlayer(32) != target);
    step(fixture,0.2f); assert(latest()->count == 1);
    mode = 9; step(fixture,0.2f); assert(latest()->count == 0 && !owner.registry().currentPlayer(32).isValid());
    mode = 0; human.free = 0; ++human.serialnumber; step(fixture,0.2f); assert(latest()->count == 1);
    mode = 11; step(fixture,0.2f); assert(latest()->count == 0);
    mode = 0; step(fixture,0.2f); assert(latest()->count == 1);
    mode = 12; step(fixture,0.2f); assert(!latest());
    mode = 0; fixture.entity.v.health = 100; step(fixture,0.2f); step(fixture,0.2f); assert(latest());
    const auto oldMap = owner.registry().mapGeneration();
    mode = 10; step(fixture,0.2f); assert(!latest() && !owner.registry().isMapActive());
    mode = 0; assert(owner.registry().activateMap(32));
    step(fixture,0.2f); assert(owner.registry().mapGeneration() != oldMap && !latest());
    detach();
}
void matrix() {
    using namespace astrabot;
    for (const unsigned actors : {1U,8U,16U}) for (const float dt : {0.008f,0.016f,0.1f}) {
        Fixture fixture; enginefuncs_t hooks{}; setup(fixture,hooks,actors);
        auto& owner = adapter::metamod::lifecycleCoordinator();
        std::array<std::uint64_t,16> before{};
        for (unsigned i=0; i<actors; ++i)
            before[i] = owner.vision().observations().diagnostics(owner.registry().currentPlayer(static_cast<std::uint16_t>(i+1)))->updates;
        const auto moves = gRunPlayerMoveCalls;
        for (unsigned frame=0; frame<160; ++frame) step(fixture,dt);
        assert(gRunPlayerMoveCalls == moves); // vision generates no movement
        for (unsigned i=0; i<actors; ++i) {
            const auto player = owner.registry().currentPlayer(static_cast<std::uint16_t>(i+1));
            const auto& vision = owner.vision().observations();
            assert(vision.latest(player) && vision.latest(player)->stamp.observer == player);
            assert(vision.latest(player)->count == 1);
            assert(vision.latest(player)->observations[0].target == owner.registry().currentPlayer(32));
            const auto* d = vision.diagnostics(player);
            assert(d && d->updates > before[i] && d->candidates == actors && d->traces <= 2*actors);
            if (actors >= 8 && dt == 0.1f) assert(d->deferredFrames > 0 && d->intervalMicros >= 199000);
        }
        const auto retired = owner.registry().currentPlayer(1);
        if (actors >= 8) {
            fixture.secondEntity.v.origin = Vector(100,1,36);
            for (unsigned i=0; i<24; ++i) step(fixture,dt);
            const auto* batch = owner.vision().observations().latest(retired);
            assert(batch && batch->count == 2); // another managed bot and the human
            assert(batch->observations[0].target == owner.registry().currentPlayer(2));
            assert(batch->observations[1].target == owner.registry().currentPlayer(32));
        }
        owner.clientDisconnect(&fixture.entity); fixture.entity.free = 1;
        assert(!owner.vision().observations().latest(retired));
        for (unsigned i=0; i<8; ++i) step(fixture,dt);
        for (unsigned i=1; i<actors; ++i)
            assert(owner.vision().observations().latest(owner.registry().currentPlayer(static_cast<std::uint16_t>(i+1))));
        detach();
    }
}
void movementCoexistence() {
    using namespace astrabot;
    Fixture fixture; enginefuncs_t hooks{};
    fixture.matrixSlot = 1;
    fixture.engine.pfnIndexOfEdict = &index;
    fixture.engine.pfnPEntityOfEntIndex = &indexed;
    fixture.engine.pfnTraceLine = &trace;
    for (unsigned slot=2; slot<=16; ++slot) fixture.matrixEntity(slot)->free = 1;
    human = {}; mode = 0;
    gDoorActive = false; gSteeringMode = -1; gPlayerObstacle = gMatrixPlayer = false;
    gSimulateCrouch = gSimulateJump = gSimulateLadder = false;
    gNavClockUs = 1000000; gFreezeNav = false; gFrozenActor = nullptr;
    prepareNavWalk(fixture,hooks);
    fixture.entity.v.health = 100; fixture.entity.v.view_ofs = Vector(0,0,28);
    fixture.entity.v.v_angle = Vector(0,0,0);
    human.v.flags = FL_CLIENT; human.v.health = 100; human.serialnumber = 22;
    human.v.origin = Vector(150,80,36); human.v.view_ofs = Vector(0,0,28);
    auto& owner = adapter::metamod::lifecycleCoordinator();
    const auto observer = owner.fakeClient().activePlayer();
    const auto moves = gRunPlayerMoveCalls;
    runNav({"astrabot_goto","2"});
    bool observed = false;
    for (unsigned i=0; i<400; ++i) {
        fixture.engineGlobals.time += 0.016f;
        navFrame(fixture);
        const auto* batch = owner.vision().observations().latest(observer);
        observed = observed || (batch && batch->count == 1);
        if (owner.navConsole().motionTrace().decision.state == nav::local::WalkState::Arrived) break;
    }
    assert(observed && gRunPlayerMoveCalls > moves);
    const auto* memory = owner.vision().memory().latest(observer);
    assert(memory && memory->count == 1);
    assert(owner.vision().observations().diagnostics(observer)->updates > 1);
    assert(owner.navConsole().motionTrace().decision.state == nav::local::WalkState::Arrived);
    gSimulateNav = false; detach();
}
void run() { observationsAndRetirement(); matrix(); movementCoexistence(); }
} // namespace p401
