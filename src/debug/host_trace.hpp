// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#pragma once

#include "host/game_host.hpp"

#include <cstdint>

namespace astrabot::debug {

using TraceSink = void (*)(const char* line) noexcept;

struct LifecycleTrace {
    astrabot::host::LifecycleEventKind kind{
        astrabot::host::LifecycleEventKind::None};
    astrabot::host::HostError error{astrabot::host::HostError::None};
    astrabot::host::MapGeneration map{};
    std::uint16_t slot{0};
    astrabot::core::Generation playerGeneration{};
    astrabot::host::TickId tick{};
    astrabot::host::EventSequence sequence{0};
    bool accepted{false};
    bool changed{false};
};

using LifecycleTraceSink = void (*)(const LifecycleTrace& trace) noexcept;

enum class FakeClientStage : std::uint8_t {
    None = 0,
    Requested,
    Allocated,
    PlayerFactory,
    Metadata,
    Connected,
    PutInServer,
    Published,
    Rejected,
    RolledBack,
};

enum class FakeClientError : std::uint8_t {
    None = 0,
    NotConfigured,
    MissingFunction,
    NotMapActive,
    AlreadyCreated,
    Reentrant,
    InvalidName,
    CreateFailed,
    InvalidEntity,
    InvalidSlot,
    SlotOccupied,
    PlayerFactoryFailed,
    InfoBufferFailed,
    ConnectRejected,
    PlayerRegistrationFailed,
    AgentBindingFailed,
};

struct FakeClientTrace {
    FakeClientStage stage{FakeClientStage::None};
    FakeClientError error{FakeClientError::None};
    astrabot::host::MapGeneration map{};
    std::uint16_t slot{0};
    astrabot::core::Generation playerGeneration{};
    astrabot::core::BotAgentId agent{};
    astrabot::host::EventSequence sequence{0};
    bool accepted{false};
    bool changed{false};
};

using FakeClientTraceSink = void (*)(const FakeClientTrace& trace) noexcept;

const char* attachedIdentityLine() noexcept;
void emitAttached(TraceSink sink) noexcept;
void emitLifecycle(
    astrabot::host::LifecycleEventKind attemptedKind,
    const astrabot::host::LifecycleResult& result,
    astrabot::host::MapGeneration currentMap,
    astrabot::host::PlayerId attemptedPlayer,
    astrabot::host::TickId attemptedTick,
    LifecycleTraceSink sink) noexcept;
void emitFakeClient(
    const FakeClientTrace& trace, FakeClientTraceSink sink) noexcept;

} // namespace astrabot::debug
