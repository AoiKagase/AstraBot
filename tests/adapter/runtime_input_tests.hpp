// SPDX-License-Identifier: MPL-2.0
// Included after the existing fake-engine perception fixture.
namespace runtime_input_test {
using namespace astrabot;
bool invalidWeapon{}, disconnectOnRead{};
void clientData(const edict_t*, int sendWeapons, clientdata_s* client) {
    assert(sendWeapons == 1);
    client->m_iId = invalidWeapon ? 0 : 28;
    client->vuser4[1] = 90;
    client->iuser3 = 1;
    if (disconnectOnRead) adapter::metamod::lifecycleCoordinator().clientDisconnect(&gFixture->entity);
}
int weaponData(edict_t*, weapon_data_s* data) {
    data[28].m_iId = 28;
    data[28].m_iClip = 30;
    data[28].m_flNextPrimaryAttack = 0.25F;
    return 1;
}
void trace(const float* start, const float* end, int flags, edict_t* observer, TraceResult* result) {
    if (flags) captureGround(start, end, flags, observer, result);
    else p401::trace(start, end, flags, observer, result);
}
void run() {
    Fixture fixture;
    enginefuncs_t hooks{};
    p401::setup(fixture, hooks, 1);
    auto& owner = adapter::metamod::lifecycleCoordinator();
    owner.setMovementClockForTest(&navNow);
    owner.setMovementTraceSink(&navTransportTrace);
    gNavTransportTraces.clear();
    const auto step = [&]() { gNavClockUs += 100'000; p401::step(fixture); };
    fixture.hookDll.pfnUpdateClientData = &clientData;
    fixture.hookDll.pfnGetWeaponData = &weaponData;
    fixture.engine.pfnTraceLine = &trace;
    invalidWeapon = disconnectOnRead = false;
    route_test::Area area{1,{{-100,-100,0},{200,200,0},0,0}};
    assert(owner.navConsole().publish(owner.registry().mapGeneration(), route_test::snapshot({area})).isNone());
    const int before = gRunPlayerMoveCalls;
    step();
    assert(gRunPlayerMoveCalls == before); // Commands are for the next tick.
    const auto player = owner.joinState().player();
    assert(owner.runtimeResult().executableCount == 1);
    const auto& decision = owner.runtimeResult().decisions[0];
    assert(decision.player == player && !decision.teamExecuted && decision.tacticalExecuted && decision.actionExecuted);
    assert(decision.team.strategy == core::team::Strategy::None);
    assert(owner.runtimeResult().stageCount == 7);
    const auto world = owner.world().latest(player);
    assert(world);
    adapter::metamod::RuntimeFrame frame{owner.registry().mapGeneration(), owner.round(),
        owner.registry().currentTick(), world->stamp.timeMicros, 100'000, {}};
    auto input = std::make_unique<adapter::metamod::RuntimeActorInput>();
    assert(adapter::metamod::buildRuntimeInputs(owner, frame, &fixture.hookDll, input.get(), 1) == 1);
    assert(input->valid(frame));
    assert(input->combat.weapon.active.value == 28 && input->combat.weapon.clipAmmo == 30);
    assert(input->combat.weapon.reserveAmmo == 90);
    assert(input->combat.weapon.primaryAttackReadyMicros == frame.nowMicros + 250'000);
    assert(!input->action.objective.canPlant && !input->tactical.economy.available);
    assert(!input->teamObjectiveAvailable && !input->team.objective.known);
    const auto first = *input;
    assert(adapter::metamod::buildRuntimeInputs(owner, frame, &fixture.hookDll, input.get(), 1) == 1);
    assert(input->combat.weapon == first.combat.weapon);
    fixture.entity.v.v_angle.y = 270.0F;
    assert(adapter::metamod::buildRuntimeInputs(owner, frame, &fixture.hookDll, input.get(), 1) == 1);
    assert(input->valid(frame) && input->combat.view.yaw == -90.0F);
    fixture.entity.v.v_angle.y = 0.0F;
    ++frame.round.value;
    assert(adapter::metamod::buildRuntimeInputs(owner, frame, &fixture.hookDll, input.get(), 1) == 1);
    assert(!input->valid(frame));
    --frame.round.value;
    step();
    assert(gRunPlayerMoveCalls == before + 1);
    for (const auto& transport : gNavTransportTraces) {
        if (transport.engineCall) assert(transport.dispatchTick.isAfter(transport.commandTick));
    }
    invalidWeapon = true;
    const auto beforeInvalid = gRunPlayerMoveCalls;
    step();
    assert(gRunPlayerMoveCalls == beforeInvalid);
    assert(owner.runtimeResult().executableCount == 0);
    assert(owner.runtimeResult().decisions[0].rejection != adapter::metamod::RuntimeRejectReason::None);
    invalidWeapon = false;
    step();
    assert(owner.runtimeResult().executableCount == 1);
    disconnectOnRead = true;
    step();
    assert(!owner.registry().currentPlayer(player.slot).isValid());
    assert(owner.runtimeResult().executableCount == 0);
    disconnectOnRead = false;
    detach();
}
}
