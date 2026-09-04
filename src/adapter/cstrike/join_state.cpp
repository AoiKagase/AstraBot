// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include "adapter/cstrike/join_state.hpp"

#include <cstddef>

namespace astrabot::adapter::cstrike {
namespace {

constexpr std::uint8_t kVguiTeamMenu = 2;
constexpr std::uint8_t kVguiClassTMenu = 26;
constexpr std::uint8_t kVguiClassCtMenu = 27;

bool hasTextPrefix(const char* value, const char* prefix) noexcept {
    if (value == nullptr || prefix == nullptr) {
        return false;
    }
    std::size_t index = 0;
    while (prefix[index] != '\0') {
        if (value[index] != prefix[index]) {
            return false;
        }
        ++index;
    }
    return true;
}

} // namespace

JoinAction JoinState::begin(
    host::PlayerId player,
    host::MapGeneration map,
    JoinRequest request,
    host::TickId startTick) noexcept {
    if (active()) {
        return JoinAction::failed(JoinError::AlreadyJoining);
    }
    player_ = player;
    map_ = map;
    request_ = request;
    if (!player.isValid()) {
        return fail(JoinError::InvalidPlayer);
    }
    if (!map.isValid()) {
        return fail(JoinError::InvalidMap);
    }
    if (!request.valid()) {
        return fail(JoinError::InvalidRequest);
    }

    phase_ = JoinPhase::WaitingTeamMenu;
    error_ = JoinError::None;
    deadline_ = makeDeadline(startTick);
    attempts_ = 0;
    teamConfirmed_ = false;
    pendingSelection_ = false;
    pendingValue_ = 0;
    pendingTick_ = host::TickId::invalid();
    repeatedPrompt_ = false;
    return {JoinActionKind::None, JoinError::None, 0, true};
}

JoinAction JoinState::onMessage(
    const MessageEvent& event,
    host::TickId tick) noexcept {
    if (!active() || !isTarget(event)) {
        return JoinAction::noOp();
    }

    if (event.kind == MessageKind::TeamInfo) {
        if (isExpectedTeam(event.text.data())) {
            const bool changed = !teamConfirmed_;
            teamConfirmed_ = true;
            if (phase_ == JoinPhase::WaitingConfirmation) {
                phase_ = JoinPhase::Joined;
                return JoinAction::joined();
            }
            return {JoinActionKind::None, JoinError::None, 0, changed};
        }
        if (isOppositeTeam(event.text.data())) {
            return fail(JoinError::WrongTeam);
        }
        return JoinAction::noOp();
    }

    if (phase_ == JoinPhase::WaitingTeamMenu && teamConfirmed_ &&
        isClassMenu(event)) {
        if (!hasSelection(event, request_.classNumber)) {
            return fail(JoinError::MenuOptionUnavailable);
        }
        pendingSelection_ = true;
        pendingValue_ = request_.classNumber;
        pendingTick_ = tick;
        phase_ = JoinPhase::WaitingClassMenu;
        return {JoinActionKind::None, JoinError::None, 0, true};
    }

    if (phase_ == JoinPhase::WaitingTeamMenu && isTeamMenu(event)) {
        if (teamConfirmed_) {
            phase_ = JoinPhase::WaitingClassMenu;
            pendingSelection_ = false;
            pendingTick_ = host::TickId::invalid();
            return {JoinActionKind::None, JoinError::None, 0, true};
        }
        if (!hasSelection(event, request_.teamSelection())) {
            return fail(JoinError::MenuOptionUnavailable);
        }
        if (pendingSelection_ && pendingValue_ == request_.teamSelection() &&
            pendingTick_.isValid() && pendingTick_ == tick) {
            return JoinAction::noOp();
        }
        pendingSelection_ = true;
        pendingValue_ = request_.teamSelection();
        pendingTick_ = tick;
        return {JoinActionKind::None, JoinError::None, 0, true};
    }

    if (phase_ == JoinPhase::TeamCommandPending && isTeamMenu(event)) {
        if (hasSelection(event, request_.teamSelection())) {
            repeatedPrompt_ = true;
        }
        return JoinAction::noOp();
    }

    if (phase_ == JoinPhase::WaitingClassMenu && isClassMenu(event)) {
        if (!hasSelection(event, request_.classNumber)) {
            return fail(JoinError::MenuOptionUnavailable);
        }
        if (pendingSelection_ && pendingValue_ == request_.classNumber &&
            pendingTick_.isValid() && pendingTick_ == tick) {
            return JoinAction::noOp();
        }
        pendingSelection_ = true;
        pendingValue_ = request_.classNumber;
        pendingTick_ = tick;
        return {JoinActionKind::None, JoinError::None, 0, true};
    }

    if ((phase_ == JoinPhase::ClassCommandPending ||
         phase_ == JoinPhase::WaitingConfirmation) && isClassMenu(event)) {
        if (hasSelection(event, request_.classNumber)) {
            repeatedPrompt_ = true;
        }
        return JoinAction::noOp();
    }

    return JoinAction::noOp();
}

JoinAction JoinState::onFrame(host::TickId tick) noexcept {
    if (!active()) {
        return JoinAction::noOp();
    }
    if (tick.isValid() && deadline_.isValid() && tick.value >= deadline_.value) {
        return fail(JoinError::Timeout);
    }
    if (!pendingSelection_) {
        return JoinAction::noOp();
    }
    if (attempts_ >= kMaxAttempts) {
        return fail(JoinError::CommandAttemptsExhausted);
    }

    const bool isTeam = phase_ == JoinPhase::WaitingTeamMenu;
    const bool isClass = phase_ == JoinPhase::WaitingClassMenu;
    if (!isTeam && !isClass) {
        return JoinAction::noOp();
    }

    pendingSelection_ = false;
    pendingTick_ = host::TickId::invalid();
    repeatedPrompt_ = false;
    ++attempts_;
    phase_ = isTeam ? JoinPhase::TeamCommandPending : JoinPhase::ClassCommandPending;
    return JoinAction::send(pendingValue_);
}

JoinAction JoinState::commandCompleted(bool dispatched) noexcept {
    if (phase_ != JoinPhase::TeamCommandPending &&
        phase_ != JoinPhase::ClassCommandPending) {
        return JoinAction::noOp();
    }
    if (!dispatched) {
        return fail(JoinError::CommandDispatchFailed);
    }

    if (repeatedPrompt_) {
        pendingSelection_ = true;
        pendingValue_ = phase_ == JoinPhase::TeamCommandPending
                            ? request_.teamSelection()
                            : request_.classNumber;
        pendingTick_ = host::TickId::invalid();
        phase_ = phase_ == JoinPhase::TeamCommandPending
                     ? JoinPhase::WaitingTeamMenu
                     : JoinPhase::WaitingClassMenu;
        repeatedPrompt_ = false;
        return {JoinActionKind::None, JoinError::None, 0, true};
    }

    if (phase_ == JoinPhase::TeamCommandPending) {
        phase_ = JoinPhase::WaitingClassMenu;
        return {JoinActionKind::None, JoinError::None, 0, true};
    }

    phase_ = JoinPhase::WaitingConfirmation;
    if (teamConfirmed_) {
        phase_ = JoinPhase::Joined;
        return JoinAction::joined();
    }
    return {JoinActionKind::None, JoinError::None, 0, true};
}

JoinAction JoinState::commandFailed(JoinError reason) noexcept {
    return fail(reason);
}

JoinAction JoinState::cancel(JoinError reason) noexcept {
    if (!active()) {
        return JoinAction::noOp();
    }
    phase_ = JoinPhase::Cancelled;
    error_ = reason;
    pendingSelection_ = false;
    pendingTick_ = host::TickId::invalid();
    repeatedPrompt_ = false;
    return JoinAction::cancelled(reason);
}

JoinAction JoinState::fail(JoinError reason) noexcept {
    phase_ = JoinPhase::Failed;
    error_ = reason;
    pendingSelection_ = false;
    pendingTick_ = host::TickId::invalid();
    repeatedPrompt_ = false;
    return JoinAction::failed(reason);
}

void JoinState::reset() noexcept {
    phase_ = JoinPhase::Idle;
    error_ = JoinError::None;
    player_ = {};
    map_ = {};
    request_ = {};
    deadline_ = {};
    attempts_ = 0;
    teamConfirmed_ = false;
    pendingSelection_ = false;
    pendingValue_ = 0;
    pendingTick_ = host::TickId::invalid();
    repeatedPrompt_ = false;
}

bool JoinState::isTarget(const MessageEvent& event) const noexcept {
    if (event.kind == MessageKind::TeamInfo) {
        return event.playerSlot == player_.slot;
    }
    return event.recipientSlot == player_.slot;
}

bool JoinState::isTeamMenu(const MessageEvent& event) const noexcept {
    if (event.kind == MessageKind::VguiMenu) {
        return event.menuType == kVguiTeamMenu;
    }
    return event.kind == MessageKind::ShowMenu &&
           (textEquals(event.text.data(), "#Team_Select") ||
            textEquals(event.text.data(), "#IG_Team_Select"));
}

bool JoinState::isClassMenu(const MessageEvent& event) const noexcept {
    if (event.kind == MessageKind::VguiMenu) {
        const std::uint8_t expected = request_.team == Team::Terrorist
                                          ? kVguiClassTMenu
                                          : kVguiClassCtMenu;
        return event.menuType == expected;
    }
    if (event.kind != MessageKind::ShowMenu) {
        return false;
    }
    return request_.team == Team::Terrorist
               ? textEquals(event.text.data(), "#Terrorist_Select")
               : textEquals(event.text.data(), "#CT_Select");
}

bool JoinState::hasSelection(
    const MessageEvent& event,
    std::uint8_t value) const noexcept {
    if (value == 0U || value > 16U) {
        return false;
    }
    const std::uint16_t mask = static_cast<std::uint16_t>(1U << (value - 1U));
    return (event.validSlots & mask) != 0U;
}

bool JoinState::isExpectedTeam(const char* value) const noexcept {
    return request_.team == Team::Terrorist
               ? textEqualsIgnoreCase(value, "TERRORIST")
               : textEqualsIgnoreCase(value, "CT");
}

bool JoinState::isOppositeTeam(const char* value) const noexcept {
    return request_.team == Team::Terrorist
               ? textEqualsIgnoreCase(value, "CT")
               : textEqualsIgnoreCase(value, "TERRORIST");
}

bool JoinState::textEquals(const char* left, const char* right) noexcept {
    if (left == nullptr || right == nullptr) {
        return false;
    }
    std::size_t index = 0;
    while (left[index] != '\0' && right[index] != '\0') {
        if (left[index] != right[index]) {
            return false;
        }
        ++index;
    }
    return left[index] == '\0' && right[index] == '\0';
}

bool JoinState::textEqualsIgnoreCase(
    const char* left,
    const char* right) noexcept {
    if (left == nullptr || right == nullptr) {
        return false;
    }
    std::size_t index = 0;
    while (left[index] != '\0' && right[index] != '\0') {
        char leftValue = left[index];
        char rightValue = right[index];
        if (leftValue >= 'a' && leftValue <= 'z') {
            leftValue = static_cast<char>(leftValue - ('a' - 'A'));
        }
        if (rightValue >= 'a' && rightValue <= 'z') {
            rightValue = static_cast<char>(rightValue - ('a' - 'A'));
        }
        if (leftValue != rightValue) {
            return false;
        }
        ++index;
    }
    return left[index] == '\0' && right[index] == '\0';
}

host::TickId JoinState::makeDeadline(host::TickId start) noexcept {
    if (start.value > UINT64_MAX - kTimeoutTicks) {
        return {UINT64_MAX};
    }
    return {start.value + kTimeoutTicks};
}

} // namespace astrabot::adapter::cstrike
