// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#pragma once

#include "core/command.hpp"

#include <cstdint>

namespace astrabot::host {

using core::BotCommand;
using core::BotAgentId;
using core::MapGeneration;
using core::PlayerId;
using core::TickId;

using EventSequence = std::uint64_t;

enum class HostError : std::uint8_t {
    None = 0,
    InvalidLifecycle,
    InvalidPlayer,
    StalePlayerGeneration,
    NotMapActive,
    AlreadyConnected,
    NotConnected,
    StaleTick,
    DuplicateTick,
    InvalidCommand,
    Rejected,
    Unsupported,
};

enum class LifecycleEventKind : std::uint8_t {
    None = 0,
    MapActivated,
    MapDeactivated,
    PlayerConnected,
    PlayerDisconnected,
};

struct LifecycleEvent {
    LifecycleEventKind kind{LifecycleEventKind::None};
    EventSequence sequence{0};
    MapGeneration map{};
    PlayerId player{};
};

struct LifecycleResult {
    LifecycleEvent event{};
    HostError error{HostError::None};
    bool accepted{false};

    static constexpr LifecycleResult acceptedEvent(LifecycleEvent value) noexcept {
        return {value, HostError::None, true};
    }
    static constexpr LifecycleResult rejected(HostError reason) noexcept {
        return {{}, reason, false};
    }

    constexpr bool succeeded() const noexcept { return accepted && error == HostError::None; }
    constexpr explicit operator bool() const noexcept { return succeeded(); }
};

struct SimulationTime {
    TickId tick{};
    std::uint64_t elapsedMicros{0};
};

struct CommandResult {
    TickId tick{};
    HostError error{HostError::None};
    bool accepted{false};

    static constexpr CommandResult acceptedCommand(TickId value) noexcept {
        return {value, HostError::None, true};
    }
    static constexpr CommandResult rejected(HostError reason, TickId value = {}) noexcept {
        return {value, reason, false};
    }

    constexpr bool succeeded() const noexcept { return accepted && error == HostError::None; }
    constexpr explicit operator bool() const noexcept { return succeeded(); }
};

class IGameHost {
public:
    virtual ~IGameHost() = default;

    virtual SimulationTime simulationTime() const noexcept = 0;

    virtual LifecycleResult activateMap() noexcept = 0;
    virtual LifecycleResult deactivateMap() noexcept = 0;
    virtual LifecycleResult playerConnected(PlayerId player) noexcept = 0;
    virtual LifecycleResult playerDisconnected(PlayerId player) noexcept = 0;

    virtual CommandResult submitCommand(
        PlayerId player,
        TickId tick,
        const BotCommand& command) noexcept = 0;
};

} // namespace astrabot::host
