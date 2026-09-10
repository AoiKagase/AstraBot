// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#pragma once

#include "adapter/cstrike/join_state.hpp"
#include "core/command.hpp"
#include "debug/host_trace.hpp"
#include "host/player_registry.hpp"
#include "host/bot_agents.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>

#include <extdll.h>
#include <meta_api.h>

namespace astrabot::adapter::metamod {

using MovementOutcome = debug::MovementTraceOutcome;
using MovementError = debug::MovementTraceError;

struct MovementResult {
    MovementOutcome outcome{MovementOutcome::None};
    MovementError error{MovementError::None};
    std::optional<host::CommandResult> registryResult{};

    bool queued() const noexcept {
        return outcome == MovementOutcome::Queued;
    }

    bool dispatched() const noexcept {
        return outcome == MovementOutcome::Dispatched;
    }

    bool rejected() const noexcept {
        return outcome == MovementOutcome::Rejected;
    }

    static MovementResult rejectedResult(MovementError movementError) noexcept {
        return MovementResult{MovementOutcome::Rejected, movementError, std::nullopt};
    }
};

class MovementCoordinator final {
public:
    using ClockNow = std::chrono::steady_clock::time_point (*)() noexcept;
    using WeaponSelectionHandler = bool (*)(
        edict_t*, core::WeaponSelection) noexcept;

    void configure(
        enginefuncs_t* engineFunctions,
        host::PlayerRegistry* registry,
        host::BotAgentRegistry* agents = nullptr) noexcept;
    void reset() noexcept;
    void resetMap() noexcept;
    void forget(core::PlayerId player) noexcept;
    bool cancel(core::PlayerId, core::MapGeneration, core::TickId commandTick) noexcept;
    std::uint64_t frameDeltaUs() const noexcept { return frameDeltaUs_; }

    MovementResult submit(
        core::PlayerId player,
        core::MapGeneration mapGeneration,
        core::TickId tick,
        const core::BotCommand& command) noexcept;
    MovementResult rejectIngress(
        MovementError error,
        core::PlayerId player,
        core::MapGeneration mapGeneration,
        core::TickId tick,
        std::uint8_t originalMsec) noexcept;

    void beginFrame(
        std::optional<std::uint64_t> engineFrameDeltaUs = std::nullopt) noexcept;
    // Dispatch only this player's slot. Caller supplies its freshly resolved
    // generation/serial-validated entity and own join phase; other queues remain.
    MovementResult dispatchAtFrameEnd(
        cstrike::JoinPhase joinPhase,
        core::PlayerId activePlayer,
        edict_t* entity,
        core::MapGeneration mapGeneration,
        core::TickId dispatchTick,
        bool removalPending = false) noexcept;
    // Advance a fake client's GameDLL join state without submitting gameplay
    // movement. Join-time entities may still be dead or spectator.
    bool dispatchJoinProgress(
        core::PlayerId activePlayer,
        edict_t* entity,
        core::MapGeneration mapGeneration,
        debug::MovementTraceSource source = debug::MovementTraceSource::Join) noexcept;
    const debug::MovementTrace& frameQueueTrace(core::PlayerId player) const noexcept;
    const debug::MovementTrace& frameDispatchTrace(core::PlayerId player) const noexcept;
    const debug::MovementTrace& frameRejectionTrace(core::PlayerId player) const noexcept;

    debug::MovementTraceSource activeDispatchSource() const noexcept {
        return activeDispatchSource_;
    }

    void setTraceSink(debug::MovementTraceSink sink) noexcept {
        traceSink_ = sink;
    }
    // Weapon IDs are translated by the adapter-owned handler. Core and the
    // transport never interpret CS private weapon objects or command names.
    void setWeaponSelectionHandler(WeaponSelectionHandler handler) noexcept {
        weaponSelectionHandler_ = handler;
    }

    void setClockForTest(ClockNow now) noexcept {
        now_ = now == nullptr ? &MovementCoordinator::steadyNow : now;
        clockArmed_ = false;
        frameDeltaUs_ = 0;
    }

private:
    struct PendingCommand {
        core::PlayerId player{};
        core::MapGeneration mapGeneration{};
        core::TickId commandTick{};
        std::uint32_t edictSerial{0};
        core::BotCommand command{};
    };

    static std::chrono::steady_clock::time_point steadyNow() noexcept;
    static std::uint8_t quantizeMsec(std::uint64_t deltaUs) noexcept;

    MovementResult reject(
        MovementError error,
        core::PlayerId player,
        core::MapGeneration mapGeneration,
        core::TickId commandTick,
        std::uint8_t originalMsec) noexcept;
    MovementResult dispatchOne(
        const PendingCommand& pending,
        cstrike::JoinPhase joinPhase,
        core::PlayerId activePlayer,
        edict_t* entity,
        core::MapGeneration mapGeneration,
        core::TickId dispatchTick) noexcept;
    bool dispatchNeutral(
        core::PlayerId activePlayer,
        edict_t* entity,
        core::MapGeneration mapGeneration,
        debug::MovementTraceSource source,
        bool requireDead) noexcept;

    void emit(
        MovementOutcome outcome,
        MovementError error,
        core::MapGeneration mapGeneration,
        core::PlayerId player,
        core::TickId commandTick,
        core::TickId dispatchTick,
        std::uint8_t originalMsec,
        bool engineCall, std::optional<std::uint64_t> dispatchDelta=std::nullopt,
        debug::MovementTraceSource source=debug::MovementTraceSource::None,
        std::uint64_t callCount=0,
        std::uint32_t edictSerial=0,
        float forward=0.0F, float side=0.0F, float up=0.0F,
        std::uint16_t buttons=0, std::uint8_t impulse=0) noexcept;

    enginefuncs_t* engineFunctions_{nullptr};
    host::PlayerRegistry* registry_{nullptr};
    host::BotAgentRegistry* agents_{nullptr};
    std::array<std::optional<PendingCommand>, host::kMaxClientSlots> pending_{};
    std::array<bool, host::kMaxClientSlots> dispatchedThisFrame_{};
    std::array<std::uint64_t, host::kMaxClientSlots> callCounts_{};
    std::array<debug::MovementTrace, host::kMaxClientSlots> frameQueued_{};
    std::array<debug::MovementTrace, host::kMaxClientSlots> frameDispatched_{};
    std::array<debug::MovementTrace, host::kMaxClientSlots> frameRejected_{};
    debug::MovementTraceSource activeDispatchSource_{debug::MovementTraceSource::None};
    ClockNow now_{&MovementCoordinator::steadyNow};
    std::chrono::steady_clock::time_point lastFrame_{};
    std::uint64_t frameDeltaUs_{0};
    bool clockArmed_{false};
    debug::MovementTraceSink traceSink_{nullptr};
    WeaponSelectionHandler weaponSelectionHandler_{nullptr};
};

} // namespace astrabot::adapter::metamod
