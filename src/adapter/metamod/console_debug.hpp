// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#pragma once

#include "debug/host_trace.hpp"
#include "host/player_registry.hpp"

#include <extdll.h>
#include <meta_api.h>

#include <cstdint>
#include <array>

namespace astrabot::adapter::metamod {

class LifecycleCoordinator;

// Adapter-private operator diagnostics.  The command and sinks deliberately
// stay outside Core so live logging cannot become part of a portable decision
// contract.
class ConsoleDebug final {
public:
    void configure(
        enginefuncs_t* engine,
        mutil_funcs_t* utility,
        LifecycleCoordinator* lifecycle) noexcept;
    void reset() noexcept;

    bool enabled() const noexcept { return enabled_; }

    // These methods are the production TraceSink endpoints.  They are public
    // only within the adapter boundary so the Metamod test fixture can drive
    // the same formatting path without exposing a Core API.
    void lifecycleTrace(const debug::LifecycleTrace& trace) noexcept;
    void fakeClientTrace(const debug::FakeClientTrace& trace) noexcept;
    void joinTrace(const debug::JoinTrace& trace) noexcept;
    void removalTrace(const debug::RemovalTrace& trace) noexcept;
    void movementTrace(const debug::MovementTrace& trace) noexcept;

    static ConsoleDebug& instance() noexcept;

private:
    static void command();
    static void addBotCommand();
    static void lifecycleSink(const debug::LifecycleTrace& trace) noexcept;
    static void fakeClientSink(const debug::FakeClientTrace& trace) noexcept;
    static void joinSink(const debug::JoinTrace& trace) noexcept;
    static void removalSink(const debug::RemovalTrace& trace) noexcept;
    static void movementSink(const debug::MovementTrace& trace) noexcept;

    void line(const char* text) noexcept;
    void commandLine(const char* text) noexcept;

    enginefuncs_t* engine_{nullptr};
    mutil_funcs_t* utility_{nullptr};
    LifecycleCoordinator* lifecycle_{nullptr};
    bool enabled_{false};
    std::uint32_t nextBotOrdinal_{1};
    std::array<std::uint64_t, host::kMaxClientSlots> lastMovementLogCall_{};
    std::array<debug::MovementTraceSource, host::kMaxClientSlots> lastMovementSource_{};
    std::array<debug::MovementTraceOutcome, host::kMaxClientSlots> lastMovementOutcome_{};
    std::array<debug::MovementTraceError, host::kMaxClientSlots> lastMovementError_{};
    std::array<core::MapGeneration, host::kMaxClientSlots> lastMovementMap_{};
    std::array<core::PlayerId, host::kMaxClientSlots> lastMovementPlayer_{};
    std::array<core::BotAgentId, host::kMaxClientSlots> lastMovementAgent_{};
};

} // namespace astrabot::adapter::metamod
