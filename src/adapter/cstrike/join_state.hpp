// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#pragma once

#include "adapter/cstrike/messages.hpp"
#include "host/game_host.hpp"

#include <cstdint>

namespace astrabot::adapter::cstrike {

enum class Team : std::uint8_t {
    Terrorist = 0,
    CounterTerrorist,
};

struct JoinRequest {
    Team team{Team::Terrorist};
    std::uint8_t classNumber{0};

    constexpr bool valid() const noexcept {
        return (team == Team::Terrorist || team == Team::CounterTerrorist) &&
               classNumber >= 1U && classNumber <= 4U;
    }
    constexpr std::uint8_t teamSelection() const noexcept {
        return team == Team::Terrorist ? 1U : 2U;
    }
};

enum class JoinPhase : std::uint8_t {
    Idle = 0,
    WaitingTeamMenu,
    TeamCommandPending,
    WaitingClassMenu,
    ClassCommandPending,
    WaitingConfirmation,
    Joined,
    Failed,
    Cancelled,
};

enum class JoinError : std::uint8_t {
    None = 0,
    InvalidRequest,
    AlreadyJoining,
    InvalidPlayer,
    InvalidMap,
    MenuOptionUnavailable,
    WrongTeam,
    CommandAttemptsExhausted,
    Timeout,
    CommandDispatchFailed,
    CommandContextReentrant,
    Disconnected,
    MapDeactivated,
    KickFailed,
    MessageUnavailable,
};

enum class JoinActionKind : std::uint8_t {
    None = 0,
    SendMenuSelect,
    Joined,
    Failed,
    Cancelled,
};

struct JoinAction {
    JoinActionKind kind{JoinActionKind::None};
    JoinError error{JoinError::None};
    std::uint8_t selection{0};
    bool changed{false};

    static constexpr JoinAction noOp() noexcept { return {}; }
    static constexpr JoinAction send(std::uint8_t value) noexcept {
        return {JoinActionKind::SendMenuSelect, JoinError::None, value, true};
    }
    static constexpr JoinAction joined() noexcept {
        return {JoinActionKind::Joined, JoinError::None, 0, true};
    }
    static constexpr JoinAction failed(JoinError reason) noexcept {
        return {JoinActionKind::Failed, reason, 0, true};
    }
    static constexpr JoinAction cancelled(JoinError reason) noexcept {
        return {JoinActionKind::Cancelled, reason, 0, true};
    }
};

class JoinState final {
public:
    JoinAction begin(
        host::PlayerId player,
        host::MapGeneration map,
        JoinRequest request,
        host::TickId startTick) noexcept;
    JoinAction onMessage(
        const MessageEvent& event,
        host::TickId tick) noexcept;
    JoinAction onFrame(host::TickId tick) noexcept;
    JoinAction commandCompleted(bool dispatched) noexcept;
    JoinAction commandFailed(JoinError reason) noexcept;
    JoinAction cancel(JoinError reason) noexcept;
    JoinAction fail(JoinError reason) noexcept;
    void reset() noexcept;

    JoinPhase phase() const noexcept { return phase_; }
    JoinError error() const noexcept { return error_; }
    host::PlayerId player() const noexcept { return player_; }
    host::MapGeneration map() const noexcept { return map_; }
    JoinRequest request() const noexcept { return request_; }
    host::TickId deadline() const noexcept { return deadline_; }
    std::uint8_t attempts() const noexcept { return attempts_; }
    bool teamConfirmed() const noexcept { return teamConfirmed_; }
    bool pendingSelection() const noexcept { return pendingSelection_; }
    bool active() const noexcept {
        return phase_ != JoinPhase::Idle && phase_ != JoinPhase::Joined &&
               phase_ != JoinPhase::Failed && phase_ != JoinPhase::Cancelled;
    }

private:
    static constexpr std::uint8_t kMaxAttempts = 3;
    static constexpr std::uint64_t kTimeoutTicks = 128;

    JoinPhase phase_{JoinPhase::Idle};
    JoinError error_{JoinError::None};
    host::PlayerId player_{};
    host::MapGeneration map_{};
    JoinRequest request_{};
    host::TickId deadline_{};
    std::uint8_t attempts_{0};
    bool teamConfirmed_{false};
    bool pendingSelection_{false};
    std::uint8_t pendingValue_{0};
    host::TickId pendingTick_{};
    bool repeatedPrompt_{false};

    bool isTarget(const MessageEvent& event) const noexcept;
    bool isTeamMenu(const MessageEvent& event) const noexcept;
    bool isClassMenu(const MessageEvent& event) const noexcept;
    bool hasSelection(const MessageEvent& event, std::uint8_t value) const noexcept;
    bool isExpectedTeam(const char* value) const noexcept;
    bool isOppositeTeam(const char* value) const noexcept;
    static bool textEquals(const char* left, const char* right) noexcept;
    static bool textEqualsIgnoreCase(const char* left, const char* right) noexcept;
    static host::TickId makeDeadline(host::TickId start) noexcept;
};

} // namespace astrabot::adapter::cstrike
