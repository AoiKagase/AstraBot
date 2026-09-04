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

const char* attachedIdentityLine() noexcept;
void emitAttached(TraceSink sink) noexcept;
void emitLifecycle(
    astrabot::host::LifecycleEventKind attemptedKind,
    const astrabot::host::LifecycleResult& result,
    astrabot::host::MapGeneration currentMap,
    astrabot::host::PlayerId attemptedPlayer,
    astrabot::host::TickId attemptedTick,
    LifecycleTraceSink sink) noexcept;

} // namespace astrabot::debug
