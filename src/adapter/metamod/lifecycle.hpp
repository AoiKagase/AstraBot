// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#pragma once

// This header is adapter-private.  SDK types must not cross into portable
// Core or host headers.
#include "adapter/metamod/plugin_entry.hpp"

#include "debug/host_trace.hpp"
#include "host/player_registry.hpp"

namespace astrabot::adapter::metamod {

class LifecycleCoordinator final {
public:
    void configure(enginefuncs_t* engineFunctions) noexcept;
    void reset() noexcept;

    void serverActivate(int clientMax) noexcept;
    void serverDeactivate() noexcept;
    void clientDisconnect(edict_t* entity) noexcept;
    void startFrame() noexcept;

    void setTraceSink(debug::LifecycleTraceSink sink) noexcept {
        traceSink_ = sink;
    }

    host::PlayerRegistry& registry() noexcept { return registry_; }
    const host::PlayerRegistry& registry() const noexcept { return registry_; }

private:
    void emit(
        host::LifecycleEventKind attemptedKind,
        const host::LifecycleResult& result,
        host::PlayerId attemptedPlayer = host::PlayerId::invalid(),
        host::TickId attemptedTick = host::TickId::invalid()) noexcept;

    host::PlayerRegistry registry_{};
    enginefuncs_t* engineFunctions_{nullptr};
    debug::LifecycleTraceSink traceSink_{nullptr};
};

LifecycleCoordinator& lifecycleCoordinator() noexcept;
void setLifecycleTraceSink(debug::LifecycleTraceSink sink) noexcept;

// These functions are assigned only to the four P1-03 DLL hook fields.
void serverActivateHook(edict_t* entityList, int edictCount, int clientMax);
void serverDeactivateHook();
void clientDisconnectHook(edict_t* entity);
void startFrameHook();

} // namespace astrabot::adapter::metamod
