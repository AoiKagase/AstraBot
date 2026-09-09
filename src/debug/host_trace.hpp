// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#pragma once

#include "adapter/cstrike/join_state.hpp"
#include "core/combat.hpp"
#include "core/perception_identity.hpp"
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
    // Requested values and observed GameDLL state are intentionally kept
    // separate. A command being sent is not evidence that TeamInfo or a
    // post-class spawn actually happened.
    astrabot::core::perception::Team observedTeam{
        astrabot::core::perception::Team::Unknown};
    std::int32_t modelIndex{0};
    bool teamInfoReceived{false};
    bool classSelectionCompleted{false};
    bool postClassFrameAdvanced{false};
    bool entityPresent{false};
    bool alive{false};
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
    WeaponSelectionUnavailable,
    WeaponSelectionRejected,
};

enum class MovementTraceSource : std::uint8_t {
    None,
    Join,
    Command,
    Idle,
    Dead,
};

struct MovementTrace {
    MovementTraceOutcome outcome{MovementTraceOutcome::None};
    MovementTraceError error{MovementTraceError::None};
    astrabot::core::MapGeneration map{};
    astrabot::core::PlayerId player{};
    astrabot::core::BotAgentId agent{};
    astrabot::core::TickId commandTick{};
    astrabot::core::TickId dispatchTick{};
    std::uint8_t originalMsec{0};
    std::uint64_t frameDeltaUs{0};
    std::uint8_t engineMsec{0};
    bool engineCall{false};
    MovementTraceSource source{MovementTraceSource::None};
    std::uint64_t callCount{0};
    std::uint32_t edictSerial{0};
    float forward{0.0F};
    float side{0.0F};
    float up{0.0F};
    std::uint16_t buttons{0};
    std::uint8_t impulse{0};
};

using MovementTraceSink = void (*)(const MovementTrace& trace) noexcept;

// Adapter-owned, value-only combat observability. This deliberately records
// provenance and acceptance facts, never entity pointers or hidden positions.
struct CombatTrace {
    astrabot::core::MapGeneration map{};
    astrabot::core::perception::RoundGeneration round{};
    astrabot::core::PlayerId player{};
    astrabot::core::BotAgentId agent{};
    astrabot::core::PlayerId target{};
    astrabot::core::perception::ObservationSource source{
        astrabot::core::perception::ObservationSource::Unknown};
    std::uint64_t targetAgeMicros{0};
    astrabot::core::combat::CombatAction action{
        astrabot::core::combat::CombatAction::NoOp};
    astrabot::core::combat::CombatReason reason{
        astrabot::core::combat::CombatReason::None};
    astrabot::core::combat::WeaponId activeWeapon{};
    std::int32_t clipAmmo{0};
    std::int32_t reserveAmmo{0};
    bool reloading{false};
    bool cooldownReady{false};
    std::uint64_t cooldownRemainingMicros{0};
    astrabot::core::TickId inputTick{};
    std::uint64_t sequence{0};
    MovementTraceError transportError{MovementTraceError::None};
    astrabot::host::HostError hostError{astrabot::host::HostError::None};
    bool commandBuilt{false};
    bool commandAccepted{false};
};

using CombatTraceSink = void (*)(const CombatTrace& trace) noexcept;

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
void emitCombat(const CombatTrace& trace, CombatTraceSink sink) noexcept;

} // namespace astrabot::debug
