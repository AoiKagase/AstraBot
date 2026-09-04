// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#pragma once

// This header is adapter-private.  SDK types must not cross into portable
// Core or host headers.
#include "adapter/metamod/plugin_entry.hpp"
#include "adapter/metamod/fake_client.hpp"

#include "adapter/cstrike/join_state.hpp"
#include "adapter/cstrike/messages.hpp"
#include "debug/host_trace.hpp"
#include "host/bot_agents.hpp"
#include "host/player_registry.hpp"

#include <array>
#include <cstdint>

namespace astrabot::adapter::metamod {

class LifecycleCoordinator final {
public:
    void configure(
        enginefuncs_t* engineFunctions,
        mutil_funcs_t* utilityFunctions,
        DLL_FUNCTIONS* gameDllFunctions,
        cstrike::UserMessageIds userMessageIds) noexcept;
    void reset() noexcept;

    void serverActivate(int clientMax) noexcept;
    void serverDeactivate() noexcept;
    void clientDisconnect(edict_t* entity) noexcept;
    void startFrame() noexcept;

    void messageBegin(
        int messageDestination,
        int messageType,
        const float* origin,
        edict_t* recipient) noexcept;
    void messageEnd() noexcept;
    void writeByte(int value) noexcept;
    void writeChar(int value) noexcept;
    void writeShort(int value) noexcept;
    void writeString(const char* value) noexcept;
    const char* commandArgs() noexcept;
    const char* commandArgv(int index) noexcept;
    int commandArgc() noexcept;

    cstrike::JoinAction requestJoin(cstrike::JoinRequest request) noexcept;
    bool dispatchMenuForTest(std::uint8_t selection) noexcept;

    void setTraceSink(debug::LifecycleTraceSink sink) noexcept {
        traceSink_ = sink;
    }
    void setFakeClientTraceSink(debug::FakeClientTraceSink sink) noexcept {
        fakeClient_.setTraceSink(sink);
    }
    void setJoinTraceSink(debug::JoinTraceSink sink) noexcept {
        joinTraceSink_ = sink;
    }

    host::PlayerRegistry& registry() noexcept { return registry_; }
    const host::PlayerRegistry& registry() const noexcept { return registry_; }
    host::BotAgentRegistry& agents() noexcept { return agents_; }
    const host::BotAgentRegistry& agents() const noexcept { return agents_; }
    FakeClientCoordinator& fakeClient() noexcept { return fakeClient_; }
    cstrike::JoinState& joinState() noexcept { return joinState_; }
    const cstrike::JoinState& joinState() const noexcept { return joinState_; }
    const cstrike::MessageDecoder& messageDecoder() const noexcept {
        return messageDecoder_;
    }

private:
    void emit(
        host::LifecycleEventKind attemptedKind,
        const host::LifecycleResult& result,
        host::PlayerId attemptedPlayer = host::PlayerId::invalid(),
        host::TickId attemptedTick = host::TickId::invalid()) noexcept;
    void emitJoin(const cstrike::JoinAction& action) noexcept;
    void handleMessage(const cstrike::MessageEvent& event) noexcept;
    void handleJoinAction(const cstrike::JoinAction& action) noexcept;
    void cleanupFailedJoin(cstrike::JoinError error) noexcept;
    bool dispatchMenu(std::uint8_t selection) noexcept;
    static void onMessage(
        void* context,
        const cstrike::MessageEvent& event) noexcept;

    host::PlayerRegistry registry_{};
    host::BotAgentRegistry agents_{};
    FakeClientCoordinator fakeClient_{};
    enginefuncs_t* engineFunctions_{nullptr};
    mutil_funcs_t* utilityFunctions_{nullptr};
    DLL_FUNCTIONS* gameDllFunctions_{nullptr};
    cstrike::MessageDecoder messageDecoder_{};
    cstrike::JoinState joinState_{};
    edict_t* activeJoinEntity_{nullptr};
    bool commandContextActive_{false};
    bool cleanupPending_{false};
    cstrike::JoinError pendingCleanupError_{cstrike::JoinError::None};
    std::array<char, 16> commandArgv0_{};
    std::array<char, 16> commandArgv1_{};
    std::array<char, 16> commandArgs_{};
    debug::LifecycleTraceSink traceSink_{nullptr};
    debug::JoinTraceSink joinTraceSink_{nullptr};
};

LifecycleCoordinator& lifecycleCoordinator() noexcept;
void setLifecycleTraceSink(debug::LifecycleTraceSink sink) noexcept;

// These functions are assigned only to the four P1-03 DLL hook fields.
void serverActivateHook(edict_t* entityList, int edictCount, int clientMax);
void serverDeactivateHook();
void clientDisconnectHook(edict_t* entity);
void startFrameHook();

void messageBeginHook(
    int messageDestination,
    int messageType,
    const float* origin,
    edict_t* recipient);
void messageEndHook();
void writeByteHook(int value);
void writeCharHook(int value);
void writeShortHook(int value);
void writeStringHook(const char* value);
const char* commandArgsHook();
const char* commandArgvHook(int index);
int commandArgcHook();

} // namespace astrabot::adapter::metamod
