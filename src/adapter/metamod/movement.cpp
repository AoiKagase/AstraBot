// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include "adapter/metamod/movement.hpp"

#include <algorithm>
#include <limits>

namespace astrabot::adapter::metamod {

void MovementCoordinator::configure(
    enginefuncs_t* engineFunctions,
    host::PlayerRegistry* registry) noexcept {
    engineFunctions_ = engineFunctions;
    registry_ = registry;
    resetMap();
}

void MovementCoordinator::reset() noexcept {
    resetMap();
    engineFunctions_ = nullptr;
    registry_ = nullptr;
    traceSink_ = nullptr;
}

void MovementCoordinator::resetMap() noexcept {
    for (auto& pending : pending_) {
        pending.reset();
    }
    clockArmed_ = false;
    frameDeltaUs_ = 0;
    lastFrame_ = {};
}

void MovementCoordinator::forget(core::PlayerId player) noexcept {
    if (!player.isValid() || player.slot > host::kMaxClientSlots) {
        return;
    }
    auto& pending = pending_[static_cast<std::size_t>(player.slot - 1U)];
    if (pending.has_value() && pending->player == player) {
        pending.reset();
    }
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

    pending = PendingCommand{player, mapGeneration, tick, command};
    emit(
        MovementOutcome::Queued,
        MovementError::None,
        mapGeneration,
        player,
        tick,
        core::TickId::invalid(),
        command.msec,
        false);
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
    core::TickId dispatchTick) noexcept {
    if(!activePlayer.isValid() || activePlayer.slot>host::kMaxClientSlots) return {};
    auto& pending=pending_[activePlayer.slot-1U];
    if(!pending) return {};
    const PendingCommand command=*pending;
    pending.reset();
    return dispatchOne(command,joinPhase,activePlayer,entity,mapGeneration,dispatchTick);
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

    const auto dispatchDelta=frameDeltaUs_; // callbacks may reset the map clock
    const std::uint8_t engineMsec = quantizeMsec(dispatchDelta);
    const float viewAngles[3]{
        pending.command.view.pitch,
        pending.command.view.yaw,
        pending.command.view.roll};
    engineFunctions_->pfnRunPlayerMove(
        entity,
        viewAngles,
        pending.command.movement.forward,
        pending.command.movement.side,
        pending.command.movement.up,
        static_cast<unsigned short>(pending.command.buttons),
        pending.command.impulse,
        engineMsec);
    emit(
        MovementOutcome::Dispatched,
        MovementError::None,
        pending.mapGeneration,
        pending.player,
        pending.commandTick,
        dispatchTick,
        pending.command.msec,
        true,dispatchDelta);
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
    bool engineCall, std::optional<std::uint64_t> dispatchDelta) noexcept {
    const auto delta=dispatchDelta.value_or(frameDeltaUs_);
    const debug::MovementTrace trace{
        outcome,
        error,
        mapGeneration,
        player,
        commandTick,
        dispatchTick,
        originalMsec,
        delta,
        engineCall ? quantizeMsec(delta) : 0U,
        engineCall};
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

} // namespace astrabot::adapter::metamod
