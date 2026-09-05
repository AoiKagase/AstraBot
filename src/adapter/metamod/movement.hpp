// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#pragma once

#include "adapter/cstrike/join_state.hpp"
#include "core/command.hpp"
#include "debug/host_trace.hpp"
#include "host/player_registry.hpp"

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

    void configure(
        enginefuncs_t* engineFunctions,
        host::PlayerRegistry* registry) noexcept;
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

    void beginFrame() noexcept;
    // Dispatch only this player's slot. Caller supplies its freshly resolved
    // generation/serial-validated entity and own join phase; other queues remain.
    MovementResult dispatchAtFrameEnd(
        cstrike::JoinPhase joinPhase,
        core::PlayerId activePlayer,
        edict_t* entity,
        core::MapGeneration mapGeneration,
        core::TickId dispatchTick) noexcept;

    void setTraceSink(debug::MovementTraceSink sink) noexcept {
        traceSink_ = sink;
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
    void emit(
        MovementOutcome outcome,
        MovementError error,
        core::MapGeneration mapGeneration,
        core::PlayerId player,
        core::TickId commandTick,
        core::TickId dispatchTick,
        std::uint8_t originalMsec,
        bool engineCall, std::optional<std::uint64_t> dispatchDelta=std::nullopt) noexcept;

    enginefuncs_t* engineFunctions_{nullptr};
    host::PlayerRegistry* registry_{nullptr};
    std::array<std::optional<PendingCommand>, host::kMaxClientSlots> pending_{};
    ClockNow now_{&MovementCoordinator::steadyNow};
    std::chrono::steady_clock::time_point lastFrame_{};
    std::uint64_t frameDeltaUs_{0};
    bool clockArmed_{false};
    debug::MovementTraceSink traceSink_{nullptr};
};

} // namespace astrabot::adapter::metamod
