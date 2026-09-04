// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include "adapter/metamod/lifecycle.hpp"

#include <cstdint>

namespace astrabot::adapter::metamod {
namespace {

LifecycleCoordinator gCoordinator{};

void markIgnored() noexcept {
    if (gpMetaGlobals != nullptr) {
        gpMetaGlobals->mres = MRES_IGNORED;
    }
}

} // namespace

void LifecycleCoordinator::configure(enginefuncs_t* engineFunctions) noexcept {
    engineFunctions_ = engineFunctions;
}

void LifecycleCoordinator::reset() noexcept {
    registry_.reset();
    engineFunctions_ = nullptr;
    traceSink_ = nullptr;
}

void LifecycleCoordinator::serverActivate(int clientMax) noexcept {
    const host::LifecycleResult result =
        clientMax < 1 || clientMax > host::kMaxClientSlots
            ? host::LifecycleResult::rejected(host::HostError::InvalidLifecycle)
            : registry_.activateMap(static_cast<std::uint16_t>(clientMax));
    emit(host::LifecycleEventKind::MapActivated, result);
}

void LifecycleCoordinator::serverDeactivate() noexcept {
    const host::LifecycleResult result = registry_.deactivateMap();
    emit(host::LifecycleEventKind::MapDeactivated, result);
}

void LifecycleCoordinator::clientDisconnect(edict_t* entity) noexcept {
    if (entity == nullptr || engineFunctions_ == nullptr ||
        engineFunctions_->pfnIndexOfEdict == nullptr) {
        const host::LifecycleResult result =
            host::LifecycleResult::rejected(host::HostError::InvalidPlayer);
        emit(host::LifecycleEventKind::PlayerDisconnected, result);
        return;
    }

    const int index = engineFunctions_->pfnIndexOfEdict(entity);
    const host::LifecycleResult result =
        index < 1 || index > static_cast<int>(host::kMaxClientSlots)
            ? host::LifecycleResult::rejected(host::HostError::InvalidPlayer)
            : registry_.disconnectSlot(static_cast<std::uint16_t>(index));
    emit(host::LifecycleEventKind::PlayerDisconnected, result);
}

void LifecycleCoordinator::startFrame() noexcept {
    const host::LifecycleResult result = registry_.startFrame();
    emit(host::LifecycleEventKind::FrameStarted, result);
}

void LifecycleCoordinator::emit(
    host::LifecycleEventKind attemptedKind,
    const host::LifecycleResult& result,
    host::PlayerId attemptedPlayer,
    host::TickId attemptedTick) noexcept {
    debug::emitLifecycle(
        attemptedKind,
        result,
        registry_.mapGeneration(),
        attemptedPlayer,
        attemptedTick,
        traceSink_);
}

LifecycleCoordinator& lifecycleCoordinator() noexcept {
    return gCoordinator;
}

void setLifecycleTraceSink(debug::LifecycleTraceSink sink) noexcept {
    gCoordinator.setTraceSink(sink);
}

void serverActivateHook(edict_t* entityList, int edictCount, int clientMax) {
    (void)entityList;
    (void)edictCount;
    gCoordinator.serverActivate(clientMax);
    markIgnored();
}

void serverDeactivateHook() {
    gCoordinator.serverDeactivate();
    markIgnored();
}

void clientDisconnectHook(edict_t* entity) {
    gCoordinator.clientDisconnect(entity);
    markIgnored();
}

void startFrameHook() {
    gCoordinator.startFrame();
    markIgnored();
}

} // namespace astrabot::adapter::metamod
