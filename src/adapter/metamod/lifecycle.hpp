// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#pragma once

// This header is adapter-private.  SDK types must not cross into portable
// Core or host headers.
#include "adapter/metamod/plugin_entry.hpp"
#include "adapter/metamod/fake_client.hpp"
#include "adapter/metamod/movement.hpp"

#include "adapter/cstrike/join_state.hpp"
#include "adapter/cstrike/messages.hpp"
#include "adapter/cstrike/nav/console.hpp"
#include "debug/host_trace.hpp"
#include "host/bot_agents.hpp"
#include "host/player_registry.hpp"

#include <array>
#include <cstdint>

namespace astrabot::adapter::metamod {

struct LifecycleStatus {
    std::uint32_t mapActivations{0};
    std::uint32_t mapReplays{0};
    std::uint32_t createAttempts{0};
    std::uint32_t removalRequests{0};
    std::uint32_t cleanupCompletions{0};
    debug::RemovalError lastRemovalError{debug::RemovalError::None};
};

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
    RemovalResult removeActive() noexcept;
    RemovalResult remove(core::PlayerId) noexcept;
    FakeClientResult createBot(const char*, cstrike::JoinRequest) noexcept;
    edict_t* entityFor(core::PlayerId) const noexcept;
    core::PlayerId playerForEntity(edict_t*) const noexcept;
    void queuePrimaryCreate(cstrike::JoinRequest request) noexcept;
    MovementResult submitCommand(
        core::PlayerId player,
        core::MapGeneration mapGeneration,
        core::TickId tick,
        const core::BotCommand& command) noexcept;

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
    cstrike::JoinAction requestJoin(core::PlayerId, cstrike::JoinRequest) noexcept;
    bool dispatchMenuForTest(std::uint8_t selection) noexcept;

    void setTraceSink(debug::LifecycleTraceSink sink) noexcept {
        traceSink_ = sink;
    }
    void setFakeClientTraceSink(debug::FakeClientTraceSink sink) noexcept {
        for(auto& client:clients_) client.fake.setTraceSink(sink);
    }
    void setJoinTraceSink(debug::JoinTraceSink sink) noexcept {
        joinTraceSink_ = sink;
    }
    void setRemovalTraceSink(debug::RemovalTraceSink sink) noexcept {
        removalTraceSink_ = sink;
        for(auto& client:clients_) client.fake.setRemovalTraceSink(sink);
    }
    void setMovementTraceSink(debug::MovementTraceSink sink) noexcept {
        movement_.setTraceSink(sink);
    }
    void setMovementClockForTest(MovementCoordinator::ClockNow now) noexcept {
        movement_.setClockForTest(now);
    }

    host::PlayerRegistry& registry() noexcept { return registry_; }
    const host::PlayerRegistry& registry() const noexcept { return registry_; }
    host::BotAgentRegistry& agents() noexcept { return agents_; }
    const host::BotAgentRegistry& agents() const noexcept { return agents_; }
    FakeClientCoordinator& fakeClient() noexcept { return clients_[0].fake; }
    cstrike::JoinState& joinState() noexcept { return clients_[0].join; }
    const cstrike::JoinState& joinState() const noexcept { return clients_[0].join; }
    const cstrike::JoinState* joinState(core::PlayerId) const noexcept;
    const cstrike::MessageDecoder& messageDecoder() const noexcept {
        return activeDecoder_ ? *activeDecoder_:messageDecoder_;
    }
    LifecycleStatus status() const noexcept { return status_; }
    cstrike::NavConsole& navConsole() noexcept { return navConsole_; }

private:
    struct ClientState {
        FakeClientCoordinator fake{};
        cstrike::JoinState join{};
        cstrike::MessageDecoder decoder{};
        bool cleanupPending{};
        cstrike::JoinError cleanupError{cstrike::JoinError::None};
    };
    ClientState* findClient(core::PlayerId) noexcept;
    const ClientState* findClient(core::PlayerId) const noexcept;
    void emit(
        host::LifecycleEventKind attemptedKind,
        const host::LifecycleResult& result,
        host::PlayerId attemptedPlayer = host::PlayerId::invalid(),
        host::TickId attemptedTick = host::TickId::invalid()) noexcept;
    void emitJoin(const ClientState&, const cstrike::JoinAction& action) noexcept;
    void handleMessage(const cstrike::MessageEvent& event) noexcept;
    void handleJoinAction(ClientState&, const cstrike::JoinAction& action) noexcept;
    void cleanupFailedJoin(ClientState&, cstrike::JoinError error) noexcept;
    void cleanupActiveAfterRemoval(
        ClientState&, const RemovalResult& result,
        host::PlayerId player) noexcept;
    void emitRemoval(
        debug::RemovalOutcome outcome,
        debug::RemovalError error,
        host::PlayerId player,
        bool mappingPresent,
        bool entityPresent) noexcept;
    bool dispatchMenu(ClientState&, std::uint8_t selection) noexcept;
    static void onMessage(
        void* context,
        const cstrike::MessageEvent& event) noexcept;

    host::PlayerRegistry registry_{};
    host::BotAgentRegistry agents_{};
    std::array<ClientState,host::kMaxClientSlots> clients_{};
    MovementCoordinator movement_{};
    cstrike::NavConsole navConsole_{};
    enginefuncs_t* engineFunctions_{nullptr};
    mutil_funcs_t* utilityFunctions_{nullptr};
    DLL_FUNCTIONS* gameDllFunctions_{nullptr};
    cstrike::MessageDecoder messageDecoder_{};
    cstrike::MessageDecoder* activeDecoder_{};
    core::MapGeneration messageMap_{};
    std::array<core::PlayerId,host::kMaxClientSlots> messagePlayers_{};
    bool commandContextActive_{false};
    core::PlayerId commandPlayer_{};
    LifecycleStatus status_{};
    std::array<char, 16> commandArgv0_{};
    std::array<char, 16> commandArgv1_{};
    std::array<char, 16> commandArgs_{};
    debug::LifecycleTraceSink traceSink_{nullptr};
    debug::JoinTraceSink joinTraceSink_{nullptr};
    debug::RemovalTraceSink removalTraceSink_{nullptr};
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
