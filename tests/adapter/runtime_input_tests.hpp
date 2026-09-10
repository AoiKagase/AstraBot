// SPDX-License-Identifier: MPL-2.0
#include "adapter/cstrike/weapon_protocol.hpp"
// Included after the existing fake-engine perception fixture.
namespace runtime_input_test {
using namespace astrabot;
bool invalidWeapon{}, disconnectOnRead{}, includeAlternative{};
std::uint16_t observedWeapon{28};
void clientData(const edict_t*, int sendWeapons, clientdata_s* client) {
    assert(sendWeapons == 1);
    client->m_iId = invalidWeapon ? 0 : observedWeapon;
    client->vuser4[1] = 90;
    client->iuser3 = 1;
    if (disconnectOnRead) adapter::metamod::lifecycleCoordinator().clientDisconnect(&gFixture->entity);
}
int weaponData(edict_t*, weapon_data_s* data) {
    data[observedWeapon].m_iId = observedWeapon;
    data[observedWeapon].m_iClip = observedWeapon == 29 ? -1 : 30;
    data[observedWeapon].m_flNextPrimaryAttack = 0.25F;
    if (includeAlternative) {
        data[16].m_iId = 16;
        data[16].m_iClip = 12;
    }
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
    invalidWeapon = disconnectOnRead = includeAlternative = false;
    observedWeapon = 28;
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
    adapter::metamod::RuntimeInputBuildStatus initialStatus{};
    assert(adapter::metamod::buildRuntimeInputs(owner, frame, &fixture.hookDll, input.get(), 1, &initialStatus) == 1);
    assert(initialStatus.map == frame.map && initialStatus.round == frame.round &&
           initialStatus.tick == frame.tick && initialStatus.nowMicros == frame.nowMicros);
    assert(initialStatus.updateClientDataAvailable && initialStatus.weaponDataAvailable);
    assert(initialStatus.updateClientDataCalled && initialStatus.weaponDataCalled);
    assert(input->valid(frame));
    assert(input->combat.weapon.active.value == 28 && input->combat.weapon.clipAmmo == 30);
    assert(input->combat.weapon.reserveAmmo == 90);
    assert(input->combat.weapon.primaryAttackReadyMicros == frame.nowMicros + 250'000);
    assert(!input->action.objective.canPlant && !input->tactical.economy.available);
    assert(!input->teamObjectiveAvailable && !input->team.objective.known);
    const auto navigationState = owner.navConsole().runtimeState(owner, player);
    assert(navigationState);
    assert(input->actionObservation.routeSafe == !navigationState->roamRejected);
    assert(input->actionObservation.actionComplete == navigationState->roamArrived);
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
    invalidWeapon = false;
    includeAlternative = true;
    observedWeapon = 28;
    assert(adapter::metamod::buildRuntimeInputs(owner, frame, &fixture.hookDll, input.get(), 1) == 1);
    assert(input->valid(frame) && input->combat.weapon.canSwitch);
    assert(input->action.weapon.canSwitch);
    includeAlternative = false;

    observedWeapon = 5;
    assert(adapter::metamod::buildRuntimeInputs(owner, frame, &fixture.hookDll, input.get(), 1) == 1);
    assert(input->combat.weapon.activeClass == core::combat::WeaponSnapshot::WeaponClass::Shotgun);
    observedWeapon = 14;
    assert(adapter::metamod::buildRuntimeInputs(owner, frame, &fixture.hookDll, input.get(), 1) == 1);
    assert(input->combat.weapon.activeClass == core::combat::WeaponSnapshot::WeaponClass::Rifle);
    // Weapon id 14 is CS GALIL; keep the protocol contract explicit.
    assert(adapter::cstrike::protocol::weaponClass(WEAPON_GALIL) ==
           core::combat::WeaponSnapshot::WeaponClass::Rifle);
    observedWeapon = 20;
    assert(adapter::metamod::buildRuntimeInputs(owner, frame, &fixture.hookDll, input.get(), 1) == 1);
    assert(input->combat.weapon.activeClass == core::combat::WeaponSnapshot::WeaponClass::MachineGun);
    observedWeapon = 21;
    assert(adapter::metamod::buildRuntimeInputs(owner, frame, &fixture.hookDll, input.get(), 1) == 1);
    assert(input->combat.weapon.activeClass == core::combat::WeaponSnapshot::WeaponClass::Shotgun);
    observedWeapon = 29;
    assert(adapter::metamod::buildRuntimeInputs(owner, frame, &fixture.hookDll, input.get(), 1) == 1);
    assert(input->combat.weapon.activeClass == core::combat::WeaponSnapshot::WeaponClass::Melee);
    assert(!input->combat.weapon.canReload);
    observedWeapon = 28;
    invalidWeapon = true;

    const auto beforeInvalid = gRunPlayerMoveCalls;
    step();
    assert(gRunPlayerMoveCalls == beforeInvalid);
    assert(owner.runtimeResult().executableCount == 0);
    assert(owner.runtimeResult().decisions[0].rejection != adapter::metamod::RuntimeRejectReason::None);
    invalidWeapon = false;
    auto* updateClientData = fixture.hookDll.pfnUpdateClientData;
    fixture.hookDll.pfnUpdateClientData = nullptr;
    adapter::metamod::RuntimeInputBuildStatus callbackStatus{};
    assert(adapter::metamod::buildRuntimeInputs(
        owner, frame, &fixture.hookDll, input.get(), 1, &callbackStatus) == 1);
    assert(callbackStatus.reason ==
           adapter::metamod::RuntimeInputBuildReason::MissingUpdateClientData);
    assert(callbackStatus.map == frame.map && callbackStatus.round == frame.round &&
           callbackStatus.tick == frame.tick && callbackStatus.nowMicros == frame.nowMicros);
    assert(!callbackStatus.updateClientDataAvailable && callbackStatus.weaponDataAvailable);
    assert(!callbackStatus.updateClientDataCalled);

    fixture.hookDll.pfnUpdateClientData = updateClientData;
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
