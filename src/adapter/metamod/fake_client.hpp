// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#pragma once

// This header is adapter-private.  SDK types must not cross into portable
// Core or host headers.
#include "adapter/metamod/plugin_entry.hpp"

#include "adapter/cstrike/join_state.hpp"
#include "debug/host_trace.hpp"
#include "host/bot_agents.hpp"
#include "host/player_registry.hpp"

#include <cstdint>

namespace astrabot::adapter::metamod {

struct FakeClientResult {
    debug::FakeClientError error{debug::FakeClientError::None};
    host::PlayerId player{};
    core::BotAgentId agent{};
    host::LifecycleResult playerRegistration{};
    host::LifecycleResult playerRollback{};
    bool accepted{false};
    bool changed{false};

    static constexpr FakeClientResult noOp() noexcept { return { {}, {}, {}, {}, {}, true, false }; }
    constexpr bool succeeded() const noexcept {
        return accepted && error == debug::FakeClientError::None;
    }
};

struct RemovalResult {
    debug::RemovalOutcome outcome{debug::RemovalOutcome::None};
    debug::RemovalError error{debug::RemovalError::None};
    host::PlayerId player{};
    bool accepted{false};
    bool changed{false};

    constexpr bool succeeded() const noexcept {
        return accepted && error == debug::RemovalError::None;
    }
};

class FakeClientCoordinator final {
public:
    void configure(
        enginefuncs_t* engineFunctions,
        mutil_funcs_t* utilityFunctions,
        DLL_FUNCTIONS* gameDllFunctions,
        host::PlayerRegistry* players,
        host::BotAgentRegistry* agents) noexcept;
    void reset() noexcept;
    void resetMap() noexcept;

    void queuePrimaryCreate() noexcept;
    void queuePrimaryCreate(cstrike::JoinRequest request) noexcept;
    FakeClientResult processPrimaryCreate() noexcept;
    FakeClientResult create(const char* name) noexcept;
    FakeClientResult create(
        const char* name,
        cstrike::JoinRequest request) noexcept;

    edict_t* activeEntity() const noexcept { return entityFor(activePlayer_); }
    edict_t* entityFor(host::PlayerId) const noexcept;
    host::PlayerId activePlayer() const noexcept { return activePlayer_; }
    cstrike::JoinRequest primaryJoinRequest() const noexcept {
        return primaryJoinRequest_;
    }
    cstrike::JoinRequest activeJoinRequest() const noexcept {
        return activeJoinRequest_;
    }
    void forget(host::PlayerId player) noexcept;
    RemovalResult requestRemoval() noexcept;
    bool cleanupActiveDirect(bool connected) noexcept;
    bool removalPending() const noexcept { return removalPending_; }
    bool operationActive() const noexcept { return operationActive_; }
    void acknowledgeDisconnect(host::PlayerId player) noexcept;
    bool kickAndCleanup(host::PlayerId player) noexcept;

    void setTraceSink(debug::FakeClientTraceSink sink) noexcept {
        traceSink_ = sink;
    }
    void setRemovalTraceSink(debug::RemovalTraceSink sink) noexcept {
        removalTraceSink_ = sink;
    }

private:
    enginefuncs_t* engineFunctions_{nullptr};
    mutil_funcs_t* utilityFunctions_{nullptr};
    DLL_FUNCTIONS* gameDllFunctions_{nullptr};
    host::PlayerRegistry* players_{nullptr};
    host::BotAgentRegistry* agents_{nullptr};
    debug::FakeClientTraceSink traceSink_{nullptr};
    bool primaryQueued_{false};
    bool attempted_{false};
    bool operationActive_{false};
    bool removalPending_{false};
    cstrike::JoinRequest primaryJoinRequest_{
        cstrike::Team::Terrorist,
        1};
    cstrike::JoinRequest activeJoinRequest_{
        cstrike::Team::Terrorist,
        1};
    debug::RemovalTraceSink removalTraceSink_{nullptr};
    edict_t* activeEntity_{nullptr};
    host::PlayerId activePlayer_{};
    host::MapGeneration activeMap_{};
    int activeSerial_{};
    bool sameEntity() const noexcept;

    void trace(
        debug::FakeClientStage stage,
        debug::FakeClientError error,
        host::PlayerId player = host::PlayerId::invalid(),
        core::BotAgentId agent = core::BotAgentId::invalid(),
        bool accepted = false,
        bool changed = false) noexcept;
    FakeClientResult rejected(
        debug::FakeClientError error,
        debug::FakeClientStage stage = debug::FakeClientStage::Rejected,
        host::PlayerId player = host::PlayerId::invalid()) noexcept;
    bool configured() const noexcept;
    static bool copyName(const char* source, char* destination, std::uint16_t capacity) noexcept;
    void cleanup(edict_t* entity, bool connected) noexcept;
    bool issueKick(edict_t* entity, debug::RemovalError& error) noexcept;
    void emitRemoval(
        debug::RemovalOutcome outcome,
        debug::RemovalError error,
        host::PlayerId player,
        bool mappingPresent,
        bool entityPresent) noexcept;
};

} // namespace astrabot::adapter::metamod
