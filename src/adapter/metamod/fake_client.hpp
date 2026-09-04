// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#pragma once

// This header is adapter-private.  SDK types must not cross into portable
// Core or host headers.
#include "adapter/metamod/plugin_entry.hpp"

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

    void queuePrimaryCreate() noexcept { primaryQueued_ = true; }
    FakeClientResult processPrimaryCreate() noexcept;
    FakeClientResult create(const char* name) noexcept;

    void setTraceSink(debug::FakeClientTraceSink sink) noexcept {
        traceSink_ = sink;
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
};

} // namespace astrabot::adapter::metamod
