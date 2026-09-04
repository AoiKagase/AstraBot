// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include "debug/host_trace.hpp"

namespace astrabot::debug {
namespace {

constexpr char kAttachedIdentityLine[] =
    "astrabot version=0.1.0 adapter=metamod-p interface=5:13 outcome=attached";

} // namespace

const char* attachedIdentityLine() noexcept {
    return kAttachedIdentityLine;
}

void emitAttached(TraceSink sink) noexcept {
    if (sink != nullptr) {
        sink(kAttachedIdentityLine);
    }
}

void emitLifecycle(
    astrabot::host::LifecycleEventKind attemptedKind,
    const astrabot::host::LifecycleResult& result,
    astrabot::host::MapGeneration currentMap,
    astrabot::host::PlayerId attemptedPlayer,
    astrabot::host::TickId attemptedTick,
    LifecycleTraceSink sink) noexcept {
    if (sink == nullptr || (result.event.sequence == 0 && result.isNoOp())) {
        return;
    }

    const astrabot::host::LifecycleEvent& event = result.event;
    const astrabot::host::PlayerId player =
        result.event.sequence != 0 ? event.player : attemptedPlayer;
    sink({
        result.event.sequence != 0 ? event.kind : attemptedKind,
        result.error,
        result.event.sequence != 0 ? event.map : currentMap,
        player.slot,
        player.generation,
        result.event.sequence != 0 ? event.tick : attemptedTick,
        event.sequence,
        result.accepted,
        result.changed(),
    });
}

void emitFakeClient(
    const FakeClientTrace& trace, FakeClientTraceSink sink) noexcept {
    if (sink != nullptr) {
        sink(trace);
    }
}

void emitJoin(const JoinTrace& trace, JoinTraceSink sink) noexcept {
    if (sink != nullptr && trace.changed) {
        sink(trace);
    }
}

} // namespace astrabot::debug
