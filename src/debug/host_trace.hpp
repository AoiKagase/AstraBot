// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#pragma once

#include "adapter/cstrike/join_state.hpp"
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

struct JoinTrace {
    astrabot::adapter::cstrike::JoinPhase phase{
        astrabot::adapter::cstrike::JoinPhase::Idle};
    astrabot::adapter::cstrike::JoinError error{
        astrabot::adapter::cstrike::JoinError::None};
    astrabot::host::MapGeneration map{};
    astrabot::host::PlayerId player{};
    astrabot::adapter::cstrike::Team team{
        astrabot::adapter::cstrike::Team::Terrorist};
    std::uint8_t classNumber{0};
    astrabot::host::TickId tick{};
    astrabot::host::EventSequence sequence{0};
    std::uint8_t attempts{0};
    bool accepted{false};
    bool changed{false};
};

using JoinTraceSink = void (*)(const JoinTrace& trace) noexcept;

enum class RemovalOutcome : std::uint8_t {
    None = 0,
    NoOp,
    KickQueued,
    Cleaned,
    Rejected,
};

enum class RemovalError : std::uint8_t {
    None = 0,
    NotConfigured,
    NoActiveClient,
    AlreadyPending,
    InvalidUserId,
    KickUnavailable,
    CommandBuildFailed,
    DirectCleanupFailed,
};

struct RemovalTrace {
    RemovalOutcome outcome{RemovalOutcome::None};
    RemovalError error{RemovalError::None};
    astrabot::host::MapGeneration map{};
    astrabot::host::PlayerId player{};
    astrabot::host::TickId tick{};
    astrabot::host::EventSequence sequence{0};
    bool mappingPresent{false};
    bool entityPresent{false};
};

using RemovalTraceSink = void (*)(const RemovalTrace& trace) noexcept;

enum class MovementTraceOutcome : std::uint8_t {
    None = 0,
    Queued,
    Dispatched,
    Rejected,
};

enum class MovementTraceError : std::uint8_t {
    None = 0,
    NotConfigured,
    MapInactive,
    MapGenerationMismatch,
    InvalidPlayer,
    QueueOccupied,
    RegistryRejected,
    NotJoined,
    MissingEntity,
    NotConnected,
    DeadPlayer,
    ButtonOutOfRange,
    NoFrameDelta,
    DispatchTooEarly,
    EngineUnavailable,
    MappingMismatch,
};

struct MovementTrace {
    MovementTraceOutcome outcome{MovementTraceOutcome::None};
    MovementTraceError error{MovementTraceError::None};
    astrabot::core::MapGeneration map{};
    astrabot::core::PlayerId player{};
    astrabot::core::TickId commandTick{};
    astrabot::core::TickId dispatchTick{};
    std::uint8_t originalMsec{0};
    std::uint64_t frameDeltaUs{0};
    std::uint8_t engineMsec{0};
    bool engineCall{false};
};

using MovementTraceSink = void (*)(const MovementTrace& trace) noexcept;

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
void emitJoin(
    const JoinTrace& trace,
    JoinTraceSink sink) noexcept;
void emitRemoval(
    const RemovalTrace& trace,
    RemovalTraceSink sink) noexcept;
void emitMovement(
    const MovementTrace& trace,
    MovementTraceSink sink) noexcept;

} // namespace astrabot::debug
