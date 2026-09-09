// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include "adapter/metamod/movement.hpp"

#include <algorithm>
#include <limits>

namespace astrabot::adapter::metamod {

void MovementCoordinator::configure(
    enginefuncs_t* engineFunctions,
    host::PlayerRegistry* registry,
    host::BotAgentRegistry* agents) noexcept {
    engineFunctions_ = engineFunctions;
    registry_ = registry;
    agents_ = agents;
    resetMap();
}

void MovementCoordinator::reset() noexcept {
    resetMap();
    engineFunctions_ = nullptr;
    registry_ = nullptr;
    agents_ = nullptr;
    traceSink_ = nullptr;
    weaponSelectionHandler_ = nullptr;
}

void MovementCoordinator::resetMap() noexcept {
    for (auto& pending : pending_) {
        pending.reset();
    }
    clockArmed_ = false;
    frameDeltaUs_ = 0;
    lastFrame_ = {};
    dispatchedThisFrame_.fill(false);
    frameQueued_.fill({});
    frameDispatched_.fill({});
    frameRejected_.fill({});
    callCounts_.fill(0);
    activeDispatchSource_ = debug::MovementTraceSource::None;
}

void MovementCoordinator::forget(core::PlayerId player) noexcept {
    if (!player.isValid() || player.slot > host::kMaxClientSlots) {
        return;
    }
    auto& pending = pending_[static_cast<std::size_t>(player.slot - 1U)];
    if (pending.has_value() && pending->player == player) {
        pending.reset();
    }
    dispatchedThisFrame_[player.slot-1U] = false;
    callCounts_[player.slot-1U] = 0;
}

bool MovementCoordinator::cancel(core::PlayerId player, core::MapGeneration map, core::TickId tick) noexcept {
    if(!player.isValid() || player.slot>host::kMaxClientSlots) return false;
    auto& pending=pending_[static_cast<std::size_t>(player.slot-1U)];
    if(!pending || pending->player!=player || pending->mapGeneration!=map || pending->commandTick!=tick) return false;
    pending.reset(); return true;
}

MovementResult MovementCoordinator::submit(
    core::PlayerId player,
    core::MapGeneration mapGeneration,
    core::TickId tick,
    const core::BotCommand& command) noexcept {
    if (registry_ == nullptr || engineFunctions_ == nullptr) {
        return reject(
            MovementError::NotConfigured,
            player,
            mapGeneration,
            tick,
            command.msec);
    }
    if (!registry_->isMapActive()) {
        return reject(
            MovementError::MapInactive,
            player,
            mapGeneration,
            tick,
            command.msec);
    }
    if (!mapGeneration.isValid() || registry_->mapGeneration() != mapGeneration) {
        return reject(
            registry_->mapGeneration().isValid()
                ? MovementError::MapGenerationMismatch
                : MovementError::MapInactive,
            player,
            mapGeneration,
            tick,
            command.msec);
    }
    if (!player.isValid() || player.slot > host::kMaxClientSlots) {
        return reject(
            MovementError::InvalidPlayer,
            player,
            mapGeneration,
            tick,
            command.msec);
    }
    if (command.buttons > (std::numeric_limits<unsigned short>::max)()) {
        return reject(
            MovementError::ButtonOutOfRange,
            player,
            mapGeneration,
            tick,
            command.msec);
    }

    auto& pending = pending_[static_cast<std::size_t>(player.slot - 1U)];
    if (pending.has_value()) {
        return reject(
            MovementError::QueueOccupied,
            player,
            mapGeneration,
            tick,
            command.msec);
    }

    const host::CommandResult registryResult =
        registry_->submitCommand(player, tick, command);
    if (!registryResult.succeeded()) {
        MovementResult result = reject(
            MovementError::RegistryRejected,
            player,
            mapGeneration,
            tick,
            command.msec);
        result.registryResult = registryResult;
        return result;
    }

    const auto* submittedEntity = engineFunctions_->pfnPEntityOfEntIndex
        ? engineFunctions_->pfnPEntityOfEntIndex(player.slot) : nullptr;
    pending = PendingCommand{player, mapGeneration, tick,
        submittedEntity ? static_cast<std::uint32_t>(submittedEntity->serialnumber) : 0U,
        command};
    emit(
        MovementOutcome::Queued,
        MovementError::None,
        mapGeneration,
        player,
        tick,
        core::TickId::invalid(),
        command.msec,
        false, std::nullopt, debug::MovementTraceSource::Command, 0, 0,
        command.movement.forward, command.movement.side, command.movement.up,
        static_cast<std::uint16_t>(command.buttons), command.impulse);
    return MovementResult{MovementOutcome::Queued, MovementError::None, registryResult};
}

MovementResult MovementCoordinator::rejectIngress(
    MovementError error,
    core::PlayerId player,
    core::MapGeneration mapGeneration,
    core::TickId tick,
    std::uint8_t originalMsec) noexcept {
    return reject(error, player, mapGeneration, tick, originalMsec);
}

void MovementCoordinator::beginFrame() noexcept {
    dispatchedThisFrame_.fill(false);
    frameQueued_.fill({});
    frameDispatched_.fill({});
    frameRejected_.fill({});
    const auto now = now_();
    if (!clockArmed_) {
        lastFrame_ = now;
        clockArmed_ = true;
        frameDeltaUs_ = 0;
        return;
    }

    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        now - lastFrame_);
    lastFrame_ = now;
    frameDeltaUs_ = elapsed.count() <= 0
        ? 0U
        : static_cast<std::uint64_t>(elapsed.count());
}

MovementResult MovementCoordinator::dispatchAtFrameEnd(
    cstrike::JoinPhase joinPhase,
    core::PlayerId activePlayer,
    edict_t* entity,
    core::MapGeneration mapGeneration,
    core::TickId dispatchTick,
    bool removalPending) noexcept {
    if(!activePlayer.isValid() || activePlayer.slot>host::kMaxClientSlots) return {};
    if (removalPending) {
        pending_[activePlayer.slot - 1U].reset();
        return reject(MovementError::NotJoined, activePlayer, mapGeneration,
            dispatchTick, 0);
    }
    if (agents_ != nullptr) {
        const auto binding = agents_->findByPlayer(activePlayer);
        if (!binding.isValid() || binding.player != activePlayer ||
            binding.map != mapGeneration) {
            return reject(MovementError::MappingMismatch, activePlayer,
                mapGeneration, dispatchTick, 0);
        }
    }
    auto& pending=pending_[activePlayer.slot-1U];
    if(!pending) {
        // Fake clients have no network command stream.  ReGameDLL advances
        // gravity, animation and other player simulation from RunPlayerMove,
        // so an otherwise idle joined bot still needs one neutral usercmd per
        // server frame.
        if (!dispatchedThisFrame_[activePlayer.slot-1U] &&
            joinPhase == cstrike::JoinPhase::Joined && entity != nullptr &&
            !entity->free) {
            (void)dispatchNeutral(activePlayer, entity, mapGeneration,
                entity->v.deadflag == DEAD_NO
                    ? debug::MovementTraceSource::Idle
                    : debug::MovementTraceSource::Dead,
                entity->v.deadflag != DEAD_NO);
        }
        return {};
    }
    const PendingCommand command=*pending;
    pending.reset();
    const auto result=dispatchOne(command,joinPhase,activePlayer,entity,mapGeneration,dispatchTick);
    // A rejected action command must not freeze a still-valid joined actor.
    // The fallback is a newly-created zero-input simulation step; it never
    // reuses the rejected command and is limited to the same generation and
    // dispatch interval.
    const auto binding = agents_ ? agents_->findByPlayer(activePlayer)
                                 : host::BotAgentBinding{};
    const bool fallbackAllowed = result.error != MovementError::MapInactive &&
        result.error != MovementError::MapGenerationMismatch &&
        result.error != MovementError::NotConnected &&
        result.error != MovementError::NotJoined &&
        result.error != MovementError::MissingEntity &&
        result.error != MovementError::MappingMismatch;
    if (result.rejected() && fallbackAllowed &&
        command.player == activePlayer &&
        !dispatchedThisFrame_[activePlayer.slot-1U] &&
        (!agents_ || (binding.isValid() && binding.player == activePlayer &&
                      binding.map == mapGeneration)) &&
        joinPhase == cstrike::JoinPhase::Joined && entity != nullptr &&
        !entity->free) {
        if (entity->v.deadflag == DEAD_NO) {
            (void)dispatchNeutral(activePlayer, entity, mapGeneration,
                debug::MovementTraceSource::Idle, false);
        } else if (result.error == MovementError::DeadPlayer) {
            (void)dispatchNeutral(activePlayer, entity, mapGeneration,
                debug::MovementTraceSource::Dead, true);
        }
    }
    return result;
}

bool MovementCoordinator::dispatchJoinProgress(
    core::PlayerId activePlayer,
    edict_t* entity,
    core::MapGeneration mapGeneration,
    debug::MovementTraceSource source) noexcept {
    if (registry_ == nullptr || !registry_->isMapActive() ||
        !mapGeneration.isValid() || registry_->mapGeneration() != mapGeneration ||
        !activePlayer.isValid() || activePlayer.slot > host::kMaxClientSlots ||
        !registry_->isConnected(activePlayer.slot) ||
        registry_->currentPlayer(activePlayer.slot) != activePlayer ||
        entity == nullptr || entity->free || engineFunctions_ == nullptr ||
        engineFunctions_->pfnRunPlayerMove == nullptr) {
        return false;
    }
    if (engineFunctions_->pfnPEntityOfEntIndex &&
        engineFunctions_->pfnPEntityOfEntIndex(activePlayer.slot) != entity) {
        return false;
    }
    if (engineFunctions_->pfnIndexOfEdict &&
        engineFunctions_->pfnIndexOfEdict(entity) != activePlayer.slot) {
        return false;
    }

    const float viewAngles[3]{
        entity->v.v_angle.x,
        entity->v.v_angle.y,
        entity->v.v_angle.z};
    const auto index=activePlayer.slot-1U;
    const auto engineMsec=quantizeMsec(frameDeltaUs_);
    activeDispatchSource_=source;
    engineFunctions_->pfnRunPlayerMove(
        entity, viewAngles, 0.0F, 0.0F, 0.0F, 0, 0,
        engineMsec);
    activeDispatchSource_=debug::MovementTraceSource::None;
    dispatchedThisFrame_[index] = true;
    const auto callCount=++callCounts_[index];
    emit(MovementOutcome::Dispatched,MovementError::None,mapGeneration,
        activePlayer,registry_->currentTick(),registry_->currentTick(),0,true,
        frameDeltaUs_,source,callCount,entity->serialnumber,0.0F,0.0F,0.0F,0,0);
    return true;
}

bool MovementCoordinator::dispatchNeutral(
    core::PlayerId activePlayer,
    edict_t* entity,
    core::MapGeneration mapGeneration,
    debug::MovementTraceSource source,
    bool requireDead) noexcept {
    if (entity == nullptr || entity->free ||
        (requireDead ? entity->v.deadflag == DEAD_NO
                     : entity->v.deadflag != DEAD_NO)) {
        return false;
    }
    return dispatchJoinProgress(activePlayer, entity, mapGeneration, source);
}

MovementResult MovementCoordinator::dispatchOne(
    const PendingCommand& pending,
    cstrike::JoinPhase joinPhase,
    core::PlayerId activePlayer,
    edict_t* entity,
    core::MapGeneration mapGeneration,
    core::TickId dispatchTick) noexcept {
    if (registry_ == nullptr || !registry_->isMapActive()) {
        return reject(
            MovementError::MapInactive,
            pending.player,
            pending.mapGeneration,
            pending.commandTick,
            pending.command.msec);
    }
    if (pending.mapGeneration != mapGeneration) {
        return reject(
            MovementError::MapGenerationMismatch,
            pending.player,
            pending.mapGeneration,
            pending.commandTick,
            pending.command.msec);
    }
    if (!registry_->isConnected(pending.player.slot) ||
        registry_->currentPlayer(pending.player.slot) != pending.player) {
        return reject(
            MovementError::NotConnected,
            pending.player,
            pending.mapGeneration,
            pending.commandTick,
            pending.command.msec);
    }
    if (joinPhase != cstrike::JoinPhase::Joined) {
        return reject(
            MovementError::NotJoined,
            pending.player,
            pending.mapGeneration,
            pending.commandTick,
            pending.command.msec);
    }
    if (!dispatchTick.isValid() ||
        dispatchTick.value <= pending.commandTick.value) {
        return reject(
            MovementError::DispatchTooEarly,
            pending.player,
            pending.mapGeneration,
            pending.commandTick,
            pending.command.msec);
    }
    if (activePlayer != pending.player) {
        return reject(
            MovementError::MappingMismatch,
            pending.player,
            pending.mapGeneration,
            pending.commandTick,
            pending.command.msec);
    }
    if (entity == nullptr || entity->free) {
        return reject(
            MovementError::MissingEntity,
            pending.player,
            pending.mapGeneration,
            pending.commandTick,
            pending.command.msec);
    }
    if (engineFunctions_->pfnPEntityOfEntIndex &&
        engineFunctions_->pfnPEntityOfEntIndex(pending.player.slot) != entity) {
        return reject(
            MovementError::MappingMismatch,
            pending.player,
            pending.mapGeneration,
            pending.commandTick,
            pending.command.msec);
    }
    if (pending.edictSerial != 0U &&
        static_cast<std::uint32_t>(entity->serialnumber) != pending.edictSerial) {
        return reject(
            MovementError::MappingMismatch,
            pending.player,
            pending.mapGeneration,
            pending.commandTick,
            pending.command.msec);
    }
    if (entity->v.deadflag != DEAD_NO) {
        return reject(
            MovementError::DeadPlayer,
            pending.player,
            pending.mapGeneration,
            pending.commandTick,
            pending.command.msec);
    }
    if (engineFunctions_ == nullptr || engineFunctions_->pfnRunPlayerMove == nullptr) {
        return reject(
            MovementError::EngineUnavailable,
            pending.player,
            pending.mapGeneration,
            pending.commandTick,
            pending.command.msec);
    }
    if (!clockArmed_) {
        return reject(
            MovementError::NoFrameDelta,
            pending.player,
            pending.mapGeneration,
            pending.commandTick,
            pending.command.msec);
    }

    const auto entitySerial = entity->serialnumber;
    if (pending.command.weaponSelect != core::kNoWeaponSelection) {
        if (weaponSelectionHandler_ == nullptr) {
            return reject(
                MovementError::WeaponSelectionUnavailable,
                pending.player,
                pending.mapGeneration,
                pending.commandTick,
                pending.command.msec);
        }
        if (!weaponSelectionHandler_(entity, pending.command.weaponSelect)) {
            return reject(
                MovementError::WeaponSelectionRejected,
                pending.player,
                pending.mapGeneration,
                pending.commandTick,
                pending.command.msec);
        }
    }
    // ClientCommand is an engine callback and may synchronously disconnect the
    // player, change the map, or replace the slot entity. Revalidate every
    // identity boundary before sending the queued movement to the engine.
    if (registry_ == nullptr || !registry_->isMapActive()) {
        return reject(
            MovementError::MapInactive,
            pending.player,
            pending.mapGeneration,
            pending.commandTick,
            pending.command.msec);
    }
    if (registry_->mapGeneration() != mapGeneration) {
        return reject(
            MovementError::MapGenerationMismatch,
            pending.player,
            pending.mapGeneration,
            pending.commandTick,
            pending.command.msec);
    }
    if (!registry_->isConnected(pending.player.slot) ||
        registry_->currentPlayer(pending.player.slot) != pending.player) {
        return reject(
            MovementError::NotConnected,
            pending.player,
            pending.mapGeneration,
            pending.commandTick,
            pending.command.msec);
    }
    if (activePlayer != pending.player) {
        return reject(
            MovementError::MappingMismatch,
            pending.player,
            pending.mapGeneration,
            pending.commandTick,
            pending.command.msec);
    }
    if (entity == nullptr || entity->free) {
        return reject(
            MovementError::MissingEntity,
            pending.player,
            pending.mapGeneration,
            pending.commandTick,
            pending.command.msec);
    }
    if (engineFunctions_->pfnPEntityOfEntIndex &&
        engineFunctions_->pfnPEntityOfEntIndex(pending.player.slot) != entity) {
        return reject(
            MovementError::MappingMismatch,
            pending.player,
            pending.mapGeneration,
            pending.commandTick,
            pending.command.msec);
    }
    if (entity->serialnumber != entitySerial) {
        return reject(
            MovementError::MappingMismatch,
            pending.player,
            pending.mapGeneration,
            pending.commandTick,
            pending.command.msec);
    }
    if (entity->v.deadflag != DEAD_NO) {
        return reject(
            MovementError::DeadPlayer,
            pending.player,
            pending.mapGeneration,
            pending.commandTick,
            pending.command.msec);
    }
    if (engineFunctions_ == nullptr || engineFunctions_->pfnRunPlayerMove == nullptr) {
        return reject(
            MovementError::EngineUnavailable,
            pending.player,
            pending.mapGeneration,
            pending.commandTick,
            pending.command.msec);
    }
    if (engineFunctions_->pfnPEntityOfEntIndex &&
        engineFunctions_->pfnPEntityOfEntIndex(activePlayer.slot) != entity) {
        return reject(
            MovementError::MappingMismatch,
            pending.player,
            pending.mapGeneration,
            pending.commandTick,
            pending.command.msec);
    }
    if (engineFunctions_->pfnIndexOfEdict &&
        engineFunctions_->pfnIndexOfEdict(entity) != activePlayer.slot) {
        return reject(
            MovementError::MappingMismatch,
            pending.player,
            pending.mapGeneration,
            pending.commandTick,
            pending.command.msec);
    }

    const auto dispatchDelta=frameDeltaUs_; // callbacks may reset the map clock
    const std::uint8_t engineMsec = quantizeMsec(dispatchDelta);
    const float viewAngles[3]{
        pending.command.view.pitch,
        pending.command.view.yaw,
        pending.command.view.roll};
    activeDispatchSource_=debug::MovementTraceSource::Command;
    engineFunctions_->pfnRunPlayerMove(
        entity,
        viewAngles,
        pending.command.movement.forward,
        pending.command.movement.side,
        pending.command.movement.up,
        static_cast<unsigned short>(pending.command.buttons),
        pending.command.impulse,
        engineMsec);
    activeDispatchSource_=debug::MovementTraceSource::None;
    const auto index=activePlayer.slot-1U;
    dispatchedThisFrame_[index] = true;
    const auto callCount=++callCounts_[index];
    emit(
        MovementOutcome::Dispatched,
        MovementError::None,
        pending.mapGeneration,
        pending.player,
        pending.commandTick,
        dispatchTick,
        pending.command.msec,
        true,dispatchDelta,debug::MovementTraceSource::Command,callCount);
    return MovementResult{MovementOutcome::Dispatched, MovementError::None, std::nullopt};
}

MovementResult MovementCoordinator::reject(
    MovementError error,
    core::PlayerId player,
    core::MapGeneration mapGeneration,
    core::TickId commandTick,
    std::uint8_t originalMsec) noexcept {
    emit(
        MovementOutcome::Rejected,
        error,
        mapGeneration,
        player,
        commandTick,
        core::TickId::invalid(),
        originalMsec,
        false);
    return MovementResult::rejectedResult(error);
}

void MovementCoordinator::emit(
    MovementOutcome outcome,
    MovementError error,
    core::MapGeneration mapGeneration,
    core::PlayerId player,
    core::TickId commandTick,
    core::TickId dispatchTick,
    std::uint8_t originalMsec,
    bool engineCall, std::optional<std::uint64_t> dispatchDelta,
    debug::MovementTraceSource source, std::uint64_t callCount,
    std::uint32_t edictSerial, float forward, float side, float up,
    std::uint16_t buttons, std::uint8_t impulse) noexcept {
    const auto delta=dispatchDelta.value_or(frameDeltaUs_);
    const debug::MovementTrace trace{
        outcome,
        error,
        mapGeneration,
        player,
        agents_ ? agents_->findByPlayer(player).agent : core::BotAgentId{},
        commandTick,
        dispatchTick,
        originalMsec,
        delta,
        engineCall ? quantizeMsec(delta) : 0U,
        engineCall,
        source,
        callCount,
        edictSerial,
        forward,
        side,
        up,
        buttons,
        impulse};
    if (player.isValid() && player.slot <= host::kMaxClientSlots) {
        const auto index = static_cast<std::size_t>(player.slot - 1U);
        if (outcome == MovementOutcome::Queued) frameQueued_[index] = trace;
        if (outcome == MovementOutcome::Dispatched) frameDispatched_[index] = trace;
        if (outcome == MovementOutcome::Rejected) frameRejected_[index] = trace;
    }
    debug::emitMovement(trace, traceSink_);
}

std::chrono::steady_clock::time_point MovementCoordinator::steadyNow() noexcept {
    return std::chrono::steady_clock::now();
}

std::uint8_t MovementCoordinator::quantizeMsec(std::uint64_t deltaUs) noexcept {
    std::uint64_t rounded = deltaUs / 1000U;
    if ((deltaUs % 1000U) >= 500U) {
        ++rounded;
    }
    rounded = std::clamp<std::uint64_t>(rounded, 1U, 255U);
    return static_cast<std::uint8_t>(rounded);
}

const debug::MovementTrace& MovementCoordinator::frameQueueTrace(core::PlayerId player) const noexcept {
    static const debug::MovementTrace empty{};
    return player.isValid() && player.slot <= host::kMaxClientSlots ? frameQueued_[player.slot - 1U] : empty;
}
const debug::MovementTrace& MovementCoordinator::frameDispatchTrace(core::PlayerId player) const noexcept {
    static const debug::MovementTrace empty{};
    return player.isValid() && player.slot <= host::kMaxClientSlots ? frameDispatched_[player.slot - 1U] : empty;
}
const debug::MovementTrace& MovementCoordinator::frameRejectionTrace(core::PlayerId player) const noexcept {
    static const debug::MovementTrace empty{};
    return player.isValid() && player.slot <= host::kMaxClientSlots ? frameRejected_[player.slot - 1U] : empty;
}
} // namespace astrabot::adapter::metamod
