// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include "adapter/metamod/movement.hpp"
#include "host/bot_agents.hpp"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <vector>

namespace {

using astrabot::adapter::cstrike::JoinPhase;
using astrabot::adapter::metamod::MovementCoordinator;
using astrabot::adapter::metamod::MovementError;
using astrabot::adapter::metamod::MovementOutcome;
using astrabot::core::BotCommand;
using astrabot::core::Button;
using astrabot::core::MapGeneration;
using astrabot::core::PlayerId;
using astrabot::core::TickId;
using astrabot::host::PlayerRegistry;

struct EngineCall {
    edict_t* entity{nullptr};
    float angles[3]{};
    float forward{0.0F};
    float side{0.0F};
    float up{0.0F};
    unsigned short buttons{0};
    byte impulse{0};
    byte msec{0};
};

std::vector<EngineCall> gCalls;
std::vector<astrabot::debug::MovementTrace> gTraces;
std::vector<std::uint16_t> gWeaponSelections;
std::uint64_t gClockUs = 0;
edict_t* gIndexedEntity = nullptr;
edict_t* indexedEntity(int index) noexcept {
    return index == 1 ? gIndexedEntity : nullptr;
}

bool captureWeaponSelection(edict_t* entity, astrabot::core::WeaponSelection weapon) noexcept {
    assert(entity != nullptr);
    gWeaponSelections.push_back(weapon);
    return true;
}

bool rejectWeaponSelection(edict_t* entity, astrabot::core::WeaponSelection weapon) noexcept {
    assert(entity != nullptr);
    gWeaponSelections.push_back(weapon);
    return false;
}

void captureRunPlayerMove(
    edict_t* entity,
    const float* angles,
    float forward,
    float side,
    float up,
    unsigned short buttons,
    byte impulse,
    byte msec) {
    assert(entity != nullptr);
    assert(angles != nullptr);
    EngineCall call{};
    call.entity = entity;
    call.angles[0] = angles[0];
    call.angles[1] = angles[1];
    call.angles[2] = angles[2];
    call.forward = forward;
    call.side = side;
    call.up = up;
    call.buttons = buttons;
    call.impulse = impulse;
    call.msec = msec;
    gCalls.push_back(call);
}

std::chrono::steady_clock::time_point fakeNow() noexcept {
    return std::chrono::steady_clock::time_point(
        std::chrono::microseconds(gClockUs));
}

void captureMovementTrace(
    const astrabot::debug::MovementTrace& trace) noexcept {
    gTraces.push_back(trace);
}

struct Fixture final {
    enginefuncs_t engine{};
    PlayerRegistry registry{};
    edict_t entity{};
    PlayerId player{};
    MapGeneration map{};
    MovementCoordinator movement{};

    Fixture() {
        engine.pfnRunPlayerMove = &captureRunPlayerMove;
        assert(registry.activateMap(32));
        assert(registry.startFrame());
        const auto registration = registry.registerPlayer(1);
        assert(registration);
        player = registration.event.player;
        map = registry.mapGeneration();
        entity.v.deadflag = DEAD_NO;
        movement.configure(&engine, &registry);
        movement.setClockForTest(&fakeNow);
        movement.setTraceSink(&captureMovementTrace);
        gClockUs = 1000000U;
        gCalls.clear();
        gTraces.clear();
        gWeaponSelections.clear();
    }

    BotCommand command() const {
        BotCommand value = BotCommand::neutral(7);
        value.view.pitch = 12.5F;
        value.view.yaw = -34.0F;
        value.view.roll = 3.0F;
        value.movement.forward = 120.0F;
        value.movement.side = -45.0F;
        value.movement.up = 8.0F;
        value.buttons = static_cast<astrabot::core::ButtonMask>(
            Button::Forward | Button::Jump);
        value.impulse = 4;
        return value;
    }

    void armAndAdvance(std::uint64_t deltaUs) {
        movement.beginFrame();
        gClockUs += deltaUs;
        movement.beginFrame();
    }

    astrabot::adapter::metamod::MovementResult dispatch(
        TickId tick = TickId{2}) {
        return movement.dispatchAtFrameEnd(
            JoinPhase::Joined,
            player,
            &entity,
            map,
            tick);
    }
};

void testJoinedActorReceivesNeutralHeartbeat() {
    Fixture fixture{};
    fixture.armAndAdvance(10000U);
    assert(fixture.dispatch(TickId{2}).outcome == MovementOutcome::None);
    assert(gCalls.size() == 1);
    assert(gCalls.front().forward == 0.0F && gCalls.front().side == 0.0F &&
           gCalls.front().up == 0.0F && gCalls.front().buttons == 0U &&
           gCalls.front().impulse == 0U && gCalls.front().msec == 10U);
    assert(gTraces.back().source == astrabot::debug::MovementTraceSource::Idle);
    assert(gTraces.back().physical.valid);
    assert(gTraces.back().physical.beforeOriginX ==
        gTraces.back().physical.afterOriginX);
    assert(gTraces.back().physical.beforeOriginY ==
        gTraces.back().physical.afterOriginY);
    assert(fixture.dispatch(TickId{3}).outcome == MovementOutcome::None);
    assert(gCalls.size() == 1);
}

void testRemovalPendingSuppressesMovement() {
    Fixture fixture{};
    fixture.armAndAdvance(10000U);
    const auto tick = fixture.registry.currentTick();
    assert(fixture.movement.submit(
        fixture.player, fixture.map, tick, fixture.command()).queued());

    const auto queued = fixture.movement.dispatchAtFrameEnd(
        JoinPhase::Joined, fixture.player, &fixture.entity, fixture.map,
        TickId{2}, true);
    assert(queued.rejected() && queued.error == MovementError::NotJoined);
    assert(gCalls.empty());

    const auto idle = fixture.movement.dispatchAtFrameEnd(
        JoinPhase::Joined, fixture.player, &fixture.entity, fixture.map,
        TickId{3}, true);
    assert(idle.rejected() && idle.error == MovementError::NotJoined);
    assert(gCalls.empty());
}

void testManagedBindingAndEdictIdentityRejectWithoutFallback() {
    Fixture fixture{};
    astrabot::host::BotAgentRegistry agents;
    MovementCoordinator movement;
    movement.configure(&fixture.engine, &fixture.registry, &agents);
    movement.setClockForTest(&fakeNow);
    movement.beginFrame();
    gClockUs += 10000U;
    movement.beginFrame();
    const auto unbound = movement.dispatchAtFrameEnd(
        JoinPhase::Joined, fixture.player, &fixture.entity, fixture.map, TickId{2});
    assert(unbound.rejected() && unbound.error == MovementError::MappingMismatch);
    assert(gCalls.empty());
    fixture.engine.pfnPEntityOfEntIndex = &indexedEntity;
    gIndexedEntity = &fixture.entity;
    fixture.entity.serialnumber = 7;
    movement.resetMap();
    movement.beginFrame();
    gClockUs += 10000U;
    movement.beginFrame();
    const auto queued = movement.submit(
        fixture.player, fixture.map, fixture.registry.currentTick(), fixture.command());
    assert(queued.queued());
    fixture.entity.serialnumber = 8;
    gCalls.clear();
    const auto stale = movement.dispatchAtFrameEnd(
        JoinPhase::Joined, fixture.player, &fixture.entity, fixture.map, TickId{2});
    assert(stale.rejected() && stale.error == MovementError::MappingMismatch);
    assert(gCalls.empty());
    gIndexedEntity = nullptr;
}

void testMsecQuantizationAndAbiConversion() {
    Fixture fixture{};
    fixture.armAndAdvance(16500U);
    const auto result = fixture.movement.submit(
        fixture.player, fixture.map, fixture.registry.currentTick(), fixture.command());
    assert(result.queued());
    assert(fixture.dispatch().dispatched());
    assert(gCalls.size() == 1);
    const EngineCall& call = gCalls.front();
    assert(call.entity == &fixture.entity);
    assert(call.angles[0] == 12.5F);
    assert(call.angles[1] == -34.0F);
    assert(call.angles[2] == 3.0F);
    assert(call.forward == 120.0F);
    assert(call.side == -45.0F);
    assert(call.up == 8.0F);
    assert(call.buttons == static_cast<unsigned short>(
        static_cast<astrabot::core::ButtonMask>(Button::Forward | Button::Jump)));
    assert(call.impulse == 4);
    assert(call.msec == 17);
    assert(gTraces.size() == 2);
    assert(gTraces[0].outcome == MovementOutcome::Queued);
    assert(gTraces[1].outcome == MovementOutcome::Dispatched);
    assert(gTraces[1].frameDeltaUs == 16500U);
    assert(gTraces[1].engineMsec == 17);
    assert(gTraces[1].engineCall);
    assert(gTraces[1].physical.valid);
    assert(gTraces[1].physical.beforeOriginX ==
        gTraces[1].physical.afterOriginX);
    assert(gTraces[1].physical.beforeVelocityX ==
        gTraces[1].physical.afterVelocityX);
    assert(gTraces[1].player == fixture.player);
    assert(gTraces[1].map == fixture.map);
    assert(gTraces[1].forward == 120.0F);
    assert(gTraces[1].side == -45.0F);
    assert(gTraces[1].up == 8.0F);
    assert(gTraces[1].buttons == static_cast<std::uint16_t>(
        static_cast<astrabot::core::ButtonMask>(Button::Forward | Button::Jump)));
    assert(gTraces[1].impulse == 4);
}

void testForgetClearsFrameTraceCaches() {
    Fixture fixture{};
    fixture.armAndAdvance(16500U);
    assert(fixture.movement.submit(
        fixture.player, fixture.map, fixture.registry.currentTick(),
        fixture.command()).queued());
    assert(fixture.dispatch().dispatched());
    assert(fixture.movement.frameQueueTrace(fixture.player).outcome ==
           MovementOutcome::Queued);
    assert(fixture.movement.frameDispatchTrace(fixture.player).outcome ==
           MovementOutcome::Dispatched);

    fixture.movement.forget(fixture.player);
    assert(fixture.movement.frameQueueTrace(fixture.player).outcome ==
           MovementOutcome::None);
    assert(fixture.movement.frameDispatchTrace(fixture.player).outcome ==
           MovementOutcome::None);
    assert(fixture.movement.frameRejectionTrace(fixture.player).outcome ==
           MovementOutcome::None);
}

void testClockArmAndBoundaryClamp() {
    const std::uint64_t deltas[] = {0U, 499U, 500U, 255499U, 255500U, 500000U};
    const byte expected[] = {1, 1, 1, 255, 255, 255};
    for (std::size_t index = 0; index < sizeof(deltas) / sizeof(deltas[0]); ++index) {
        Fixture fixture{};
        fixture.movement.beginFrame();
        assert(gCalls.empty());
        gClockUs += deltas[index];
        fixture.movement.beginFrame();
        const auto result = fixture.movement.submit(
            fixture.player, fixture.map, fixture.registry.currentTick(), fixture.command());
        assert(result.queued());
        assert(fixture.dispatch().dispatched());
        assert(gCalls.size() == 1);
        assert(gCalls.front().msec == expected[index]);
    }
}

void testOneCallAndPendingClear() {
    Fixture fixture{};
    fixture.armAndAdvance(10000U);
    const TickId tick = fixture.registry.currentTick();
    assert(fixture.movement.submit(
        fixture.player, fixture.map, tick, fixture.command()).queued());
    assert(fixture.dispatch(TickId{2}).dispatched());
    assert(gCalls.size() == 1);
    assert(fixture.dispatch(TickId{3}).outcome == MovementOutcome::None);
    const auto duplicate = fixture.movement.submit(
        fixture.player, fixture.map, tick, fixture.command());
    assert(duplicate.rejected());
    assert(duplicate.registryResult.has_value());
    assert(duplicate.registryResult->error == astrabot::host::HostError::DuplicateTick);
    assert(gCalls.size() == 1);
}
void testExactPendingCancellation() {
    Fixture f; f.armAndAdvance(10000);
    const auto tick=f.registry.currentTick();
    assert(f.movement.submit(f.player,f.map,tick,f.command()).queued());
    assert(!f.movement.cancel(f.player,f.map,TickId{tick.value+1}));
    assert(!f.movement.cancel(f.player,MapGeneration{f.map.value+1},tick));
    auto reused=f.player; ++reused.generation.value;
    assert(!f.movement.cancel(reused,f.map,tick));
    assert(f.movement.cancel(f.player,f.map,tick));
    assert(!f.movement.cancel(f.player,f.map,tick));
    assert(f.dispatch().outcome==MovementOutcome::None && gCalls.empty());
}

void testSubmissionRejectionAndGenerationChecks() {
    Fixture fixture{};
    fixture.armAndAdvance(10000U);
    const TickId tick = fixture.registry.currentTick();
    const BotCommand valid = fixture.command();

    BotCommand invalid = valid;
    invalid.msec = 0;
    assert(fixture.movement.submit(
        fixture.player, fixture.map, tick, invalid).rejected());
    assert(gTraces.back().error == MovementError::RegistryRejected);
    assert(fixture.movement.submit(
        fixture.player, fixture.map, TickId{2}, valid).rejected());
    assert(fixture.movement.submit(
        fixture.player, MapGeneration{99}, tick, valid).rejected());

    const PlayerId stale{fixture.player.slot,
                         astrabot::core::Generation{fixture.player.generation.value + 1U}};
    assert(fixture.movement.submit(stale, fixture.map, tick, valid).rejected());
    assert(gCalls.empty());
}

void testDispatchGuardsAndCleanup() {
    Fixture fixture{};
    fixture.armAndAdvance(10000U);
    const TickId tick = fixture.registry.currentTick();
    assert(fixture.movement.submit(
        fixture.player, fixture.map, tick, fixture.command()).queued());
    assert(fixture.dispatch( tick).rejected());
    assert(gCalls.size() == 1);
    assert(gCalls.front().forward == 0.0F && gCalls.front().side == 0.0F &&
           gCalls.front().up == 0.0F && gCalls.front().buttons == 0U);

    assert(fixture.registry.startFrame());
    gCalls.clear();
    gTraces.clear();
    gClockUs += 10000U;
    fixture.movement.beginFrame();
    const TickId nextTick = fixture.registry.currentTick();
    assert(fixture.movement.submit(
        fixture.player, fixture.map, nextTick, fixture.command()).queued());
    fixture.entity.v.deadflag = DEAD_DEAD;
    assert(fixture.dispatch(TickId{3}).rejected());
    assert(gCalls.size() == 1);
    assert(gCalls.front().forward == 0.0F && gCalls.front().side == 0.0F &&
           gCalls.front().up == 0.0F && gCalls.front().buttons == 0U);
    assert(gTraces.back().source == astrabot::debug::MovementTraceSource::Dead);

    Fixture disconnected{};
    disconnected.armAndAdvance(10000U);
    const TickId disconnectedTick = disconnected.registry.currentTick();
    assert(disconnected.movement.submit(
        disconnected.player,
        disconnected.map,
        disconnectedTick,
        disconnected.command()).queued());
    assert(disconnected.registry.disconnectPlayer(disconnected.player));
    assert(disconnected.dispatch(TickId{2}).rejected());
    assert(gCalls.empty());
    assert(gTraces.back().error == MovementError::NotConnected);

    Fixture inactive{};
    inactive.armAndAdvance(10000U);
    const TickId inactiveTick = inactive.registry.currentTick();
    assert(inactive.movement.submit(
        inactive.player,
        inactive.map,
        inactiveTick,
        inactive.command()).queued());
    assert(inactive.registry.deactivateMap());
    assert(inactive.dispatch(TickId{2}).rejected());
    assert(gCalls.empty());
    assert(gTraces.back().error == MovementError::MapInactive);

    fixture.movement.resetMap();
    assert(fixture.dispatch(TickId{3}).outcome == MovementOutcome::None);
    assert(gCalls.empty());
}

void testEngineUnavailableAndTraceUniqueness() {
    Fixture fixture{};
    fixture.armAndAdvance(10000U);
    const TickId tick = fixture.registry.currentTick();
    assert(fixture.movement.submit(
        fixture.player, fixture.map, tick, fixture.command()).queued());
    fixture.engine.pfnRunPlayerMove = nullptr;
    assert(fixture.dispatch().rejected());
    assert(gCalls.empty());
    assert(gTraces.size() == 2);
    assert(gTraces.back().error == MovementError::EngineUnavailable);
    assert(!gTraces.back().engineCall);
}

void testWeaponSelectionHandlerOwnsDispatchGate() {
    Fixture fixture{};
    fixture.movement.setWeaponSelectionHandler(&captureWeaponSelection);
    auto command = fixture.command();
    command.weaponSelect = 7;
    fixture.armAndAdvance(16500U);
    assert(fixture.movement.submit(
        fixture.player, fixture.map, fixture.registry.currentTick(), command).queued());
    assert(fixture.dispatch().dispatched());
    assert(gWeaponSelections.size() == 1);
    assert(gWeaponSelections.front() == 7);
    assert(gCalls.size() == 1);

    fixture.movement.resetMap();
    gCalls.clear();
    gWeaponSelections.clear();
    fixture.movement.setWeaponSelectionHandler(&rejectWeaponSelection);
    assert(fixture.registry.startFrame());
    command.weaponSelect = 8;
    fixture.armAndAdvance(16500U);
    assert(fixture.movement.submit(
        fixture.player, fixture.map, fixture.registry.currentTick(), command).queued());
    const auto rejected = fixture.dispatch(TickId{3});
    assert(rejected.rejected());
    assert(rejected.error == MovementError::WeaponSelectionRejected);
    assert(gWeaponSelections.size() == 1);
    assert(gWeaponSelections.front() == 8);
    assert(gCalls.size() == 1);
    assert(gCalls.front().forward == 0.0F);
    assert(gCalls.front().side == 0.0F);
    assert(gCalls.front().up == 0.0F);
    assert(gTraces.back().source == astrabot::debug::MovementTraceSource::Idle);
}
void testIndependentPlayerQueues() {
    Fixture fixture{}; fixture.armAndAdvance(16000);
    const auto second=fixture.registry.registerPlayer(2).event.player;
    edict_t other{}; auto command=fixture.command(); command.movement.forward=25;
    const auto tick=fixture.registry.currentTick();
    assert(fixture.movement.submit(second,fixture.map,tick,command).queued());
    assert(fixture.movement.submit(fixture.player,fixture.map,tick,fixture.command()).queued());
    assert(fixture.movement.dispatchAtFrameEnd(JoinPhase::WaitingTeamMenu,fixture.player,&fixture.entity,fixture.map,{2}).error==MovementError::NotJoined);
    assert(gCalls.empty());
    assert(fixture.movement.dispatchAtFrameEnd(JoinPhase::Joined,second,&other,fixture.map,{2}).dispatched());
    assert(gCalls.size()==1 && gCalls.front().entity==&other && gCalls.front().forward==25);
    assert(fixture.dispatch().outcome==MovementOutcome::None);
}

void testInitialFrameDoesNotQueueTimedMovement() {
    Fixture fixture{};
    fixture.movement.beginFrame();
    const auto result = fixture.movement.submit(
        fixture.player, fixture.map, fixture.registry.currentTick(), fixture.command());
    assert(result.rejected());
    assert(result.error == MovementError::NoFrameDelta);
    assert(gCalls.empty());
}

} // namespace

int main() {
    testJoinedActorReceivesNeutralHeartbeat();
    testRemovalPendingSuppressesMovement();
    testManagedBindingAndEdictIdentityRejectWithoutFallback();
    testMsecQuantizationAndAbiConversion();
    testForgetClearsFrameTraceCaches();
    testClockArmAndBoundaryClamp();
    testOneCallAndPendingClear();
    testExactPendingCancellation();
    testSubmissionRejectionAndGenerationChecks();
    testDispatchGuardsAndCleanup();
    testEngineUnavailableAndTraceUniqueness();
    testIndependentPlayerQueues();
    testInitialFrameDoesNotQueueTimedMovement();
    testWeaponSelectionHandlerOwnsDispatchGate();
    return 0;
}
