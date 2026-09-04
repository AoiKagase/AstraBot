// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include "adapter/cstrike/join_state.hpp"
#include "adapter/cstrike/messages.hpp"

#include <cassert>
#include <cstring>
#include <vector>

namespace {

using astrabot::adapter::cstrike::JoinActionKind;
using astrabot::adapter::cstrike::JoinError;
using astrabot::adapter::cstrike::JoinPhase;
using astrabot::adapter::cstrike::JoinRequest;
using astrabot::adapter::cstrike::JoinState;
using astrabot::adapter::cstrike::MessageDecoder;
using astrabot::adapter::cstrike::MessageEvent;
using astrabot::adapter::cstrike::MessageKind;
using astrabot::adapter::cstrike::Team;
using astrabot::adapter::cstrike::TokenizedCommand;
using astrabot::core::Generation;
using astrabot::core::MapGeneration;
using astrabot::core::PlayerId;
using astrabot::core::TickId;

std::vector<MessageEvent> gMessages;

void captureMessage(void* /* context */, const MessageEvent& event) noexcept {
    gMessages.push_back(event);
}

MessageEvent menu(
    MessageKind kind,
    std::uint16_t recipient,
    std::uint16_t validSlots,
    std::uint8_t menuType,
    const char* text) {
    MessageEvent event{};
    event.kind = kind;
    event.recipientSlot = recipient;
    event.validSlots = validSlots;
    event.menuType = menuType;
    if (text != nullptr) {
        std::size_t index = 0;
        while (index + 1U < event.text.size() && text[index] != '\0') {
            event.text[index] = text[index];
            ++index;
        }
        event.text[index] = '\0';
    }
    return event;
}

MessageEvent teamInfo(std::uint16_t slot, const char* team) {
    MessageEvent event{};
    event.kind = MessageKind::TeamInfo;
    event.playerSlot = slot;
    std::size_t index = 0;
    while (index + 1U < event.text.size() && team[index] != '\0') {
        event.text[index] = team[index];
        ++index;
    }
    event.text[index] = '\0';
    return event;
}

void testTokenizer() {
    TokenizedCommand command{};
    std::uint8_t selection = 0;
    assert(astrabot::adapter::cstrike::parseMenuSelect(
               "menuselect 1", command, selection) ==
           astrabot::adapter::cstrike::CommandParseError::None);
    assert(selection == 1U);
    assert(command.count == 2U);
    assert(std::strcmp(command.args.data(), "1") == 0);

    assert(astrabot::adapter::cstrike::parseMenuSelect(
               "  menuselect \"2\"  ", command, selection) ==
           astrabot::adapter::cstrike::CommandParseError::None);
    assert(selection == 2U);
    assert(astrabot::adapter::cstrike::tokenizeCommand(
               "menuselect 1; quit", command) ==
           astrabot::adapter::cstrike::CommandParseError::CommandSeparator);
    assert(astrabot::adapter::cstrike::tokenizeCommand(
               "menuselect \"\"", command) ==
           astrabot::adapter::cstrike::CommandParseError::None);
    assert(command.tokens[1][0] == '\0');
    assert(astrabot::adapter::cstrike::parseMenuSelect(
               "menuselect", command, selection) ==
           astrabot::adapter::cstrike::CommandParseError::InvalidMenuselect);
    assert(astrabot::adapter::cstrike::parseMenuSelect(
               "menuselect 10", command, selection) ==
           astrabot::adapter::cstrike::CommandParseError::InvalidMenuselect);
    assert(astrabot::adapter::cstrike::tokenizeCommand(
               "menuselect \"1", command) ==
           astrabot::adapter::cstrike::CommandParseError::UnterminatedQuote);

    assert(astrabot::adapter::cstrike::tokenizeCommand(
               "a b c d e", command) ==
           astrabot::adapter::cstrike::CommandParseError::TooManyTokens);
    assert(astrabot::adapter::cstrike::tokenizeCommand(
               "menuselect 1234567890123456", command) ==
           astrabot::adapter::cstrike::CommandParseError::TokenTooLong);
    char tooLong[astrabot::adapter::cstrike::kMaxCommandBytes + 1U]{};
    for (std::size_t index = 0;
         index < astrabot::adapter::cstrike::kMaxCommandBytes;
         ++index) {
        tooLong[index] = 'x';
    }
    assert(astrabot::adapter::cstrike::tokenizeCommand(tooLong, command) ==
           astrabot::adapter::cstrike::CommandParseError::CommandTooLong);
}

void testMessageDecoder() {
    MessageDecoder decoder{};
    decoder.configure({11, 12, 13}, &captureMessage, nullptr);
    gMessages.clear();

    decoder.begin(11, 4);
    decoder.writeByte(2);
    decoder.writeShort(0x0003);
    decoder.writeChar(-1);
    decoder.writeByte(0);
    decoder.writeString("#Team_Select");
    decoder.end();
    assert(gMessages.size() == 1U);
    assert(gMessages[0].kind == MessageKind::VguiMenu);
    assert(gMessages[0].recipientSlot == 4U);
    assert(gMessages[0].menuType == 2U);

    decoder.begin(12, 4);
    decoder.writeShort(1);
    decoder.writeChar(-1);
    decoder.writeByte(1);
    decoder.writeString("#Team_");
    decoder.end();
    decoder.begin(12, 4);
    decoder.writeShort(1);
    decoder.writeChar(-1);
    decoder.writeByte(0);
    decoder.writeString("Select");
    decoder.end();
    assert(gMessages.size() == 2U);
    assert(std::strcmp(gMessages[1].text.data(), "#Team_Select") == 0);

    decoder.begin(13, 0);
    decoder.writeByte(4);
    decoder.writeString("CT");
    decoder.end();
    assert(gMessages.size() == 3U);
    assert(gMessages[2].playerSlot == 4U);
    assert(std::strcmp(gMessages[2].text.data(), "CT") == 0);

    decoder.begin(12, 4);
    decoder.writeShort(1);
    decoder.writeChar(-1);
    decoder.writeByte(0);
    decoder.writeString("#Unknown_Select");
    decoder.end();
    assert(decoder.lastError() ==
           astrabot::adapter::cstrike::MessageDecodeError::UnknownMenuToken);
    assert(gMessages.size() == 3U);

    decoder.begin(999, 4);
    assert(decoder.lastError() ==
           astrabot::adapter::cstrike::MessageDecodeError::UnknownMessage);
}

void testJoinState() {
    JoinState state{};
    const PlayerId player{1, Generation{1}};
    assert(state.begin(player, MapGeneration{1}, {Team::Terrorist, 1}, TickId{1}).changed);
    assert(state.phase() == JoinPhase::WaitingTeamMenu);

    auto action = state.onMessage(menu(MessageKind::VguiMenu, 1, 0x0001, 2, ""), TickId{1});
    assert(action.changed);
    action = state.onFrame(TickId{2});
    assert(action.kind == JoinActionKind::SendMenuSelect);
    assert(action.selection == 1U);
    action = state.commandCompleted(true);
    assert(action.changed);
    assert(state.phase() == JoinPhase::WaitingClassMenu);

    action = state.onMessage(menu(MessageKind::VguiMenu, 1, 0x0001, 26, ""), TickId{3});
    assert(action.changed);
    action = state.onFrame(TickId{3});
    assert(action.kind == JoinActionKind::SendMenuSelect);
    action = state.commandCompleted(true);
    assert(state.phase() == JoinPhase::WaitingConfirmation);
    action = state.onMessage(teamInfo(1, "TERRORIST"), TickId{3});
    assert(action.kind == JoinActionKind::Joined);
    assert(state.phase() == JoinPhase::Joined);
}

void testAlreadyAssignedAndInvalidMenu() {
    JoinState assigned{};
    assert(assigned.begin(
                PlayerId{1, Generation{1}},
                MapGeneration{1},
                JoinRequest{Team::CounterTerrorist, 2},
                TickId{1})
               .changed);
    auto action = assigned.onMessage(teamInfo(1, "CT"), TickId{1});
    assert(action.changed);
    action = assigned.onMessage(menu(MessageKind::ShowMenu, 1, 0x0002, 0, "#CT_Select"), TickId{2});
    assert(action.changed);
    action = assigned.onFrame(TickId{2});
    assert(action.kind == JoinActionKind::SendMenuSelect);
    assert(action.selection == 2U);

    JoinState unavailable{};
    assert(unavailable.begin(
                         PlayerId{1, Generation{1}},
                         MapGeneration{1},
                         JoinRequest{Team::Terrorist, 4},
                         TickId{1})
               .changed);
    action = unavailable.onMessage(menu(MessageKind::VguiMenu, 1, 0x0001, 2, ""), TickId{1});
    assert(unavailable.onFrame(TickId{2}).kind == JoinActionKind::SendMenuSelect);
    assert(unavailable.commandCompleted(true).changed);
    action = unavailable.onMessage(menu(MessageKind::VguiMenu, 1, 0x0001, 26, ""), TickId{2});
    assert(action.kind == JoinActionKind::Failed);
    assert(action.error == JoinError::MenuOptionUnavailable);

    JoinState invalidRequest{};
    assert(invalidRequest.begin(
                         PlayerId{1, Generation{1}},
                         MapGeneration{1},
                         JoinRequest{static_cast<Team>(2), 1},
                         TickId{1})
               .error == JoinError::InvalidRequest);

    JoinState wrongTeam{};
    assert(wrongTeam.begin(
                       PlayerId{1, Generation{1}},
                       MapGeneration{1},
                       JoinRequest{Team::Terrorist, 1},
                       TickId{1})
               .changed);
    action = wrongTeam.onMessage(teamInfo(1, "CT"), TickId{1});
    assert(action.kind == JoinActionKind::Failed);
    assert(action.error == JoinError::WrongTeam);
}

void testCounterTerroristAndDuplicatePrompt() {
    JoinState state{};
    assert(state.begin(
                     PlayerId{2, Generation{1}},
                     MapGeneration{1},
                     JoinRequest{Team::CounterTerrorist, 4},
                     TickId{1})
               .changed);

    const MessageEvent teamPrompt =
        menu(MessageKind::ShowMenu, 2, 0x0002, 0, "#IG_Team_Select");
    assert(state.onMessage(teamPrompt, TickId{2}).changed);
    assert(!state.onMessage(teamPrompt, TickId{2}).changed);
    auto action = state.onFrame(TickId{2});
    assert(action.kind == JoinActionKind::SendMenuSelect);
    assert(action.selection == 2U);
    action = state.commandCompleted(true);
    assert(action.changed);
    assert(state.phase() == JoinPhase::WaitingClassMenu);

    const MessageEvent classPrompt =
        menu(MessageKind::VguiMenu, 2, 0x0008, 27, "");
    assert(state.onMessage(classPrompt, TickId{3}).changed);
    action = state.onFrame(TickId{3});
    assert(action.kind == JoinActionKind::SendMenuSelect);
    assert(action.selection == 4U);
    action = state.commandCompleted(true);
    assert(state.phase() == JoinPhase::WaitingConfirmation);
    assert(action.changed);
    action = state.onMessage(teamInfo(2, "CT"), TickId{3});
    assert(action.kind == JoinActionKind::Joined);
    assert(state.phase() == JoinPhase::Joined);

    JoinState unassigned{};
    assert(unassigned.begin(
                          PlayerId{1, Generation{1}},
                          MapGeneration{1},
                          JoinRequest{Team::Terrorist, 1},
                          TickId{1})
               .changed);
    assert(!unassigned.onMessage(teamInfo(1, "UNASSIGNED"), TickId{1}).changed);
    assert(unassigned.phase() == JoinPhase::WaitingTeamMenu);
    assert(!unassigned.onMessage(
                   menu(MessageKind::VguiMenu, 2, 0x0001, 2, ""),
                   TickId{1})
                .changed);
    assert(unassigned.phase() == JoinPhase::WaitingTeamMenu);
}

void testAttemptLimitAndTimeout() {
    JoinState retried{};
    assert(retried.begin(
                       PlayerId{1, Generation{1}},
                       MapGeneration{1},
                       JoinRequest{Team::Terrorist, 1},
                       TickId{1})
               .changed);
    const MessageEvent prompt = menu(MessageKind::VguiMenu, 1, 0x0001, 2, "");
    for (int attempt = 0; attempt < 3; ++attempt) {
        assert(retried.onMessage(prompt, TickId{2}).changed);
        assert(retried.onFrame(TickId{2}).kind == JoinActionKind::SendMenuSelect);
        assert(retried.onMessage(prompt, TickId{2}).changed == false);
        assert(retried.commandCompleted(true).changed);
    }
    const auto exhausted = retried.onFrame(TickId{3});
    assert(exhausted.kind == JoinActionKind::Failed);
    assert(exhausted.error == JoinError::CommandAttemptsExhausted);

    JoinState timeout{};
    assert(timeout.begin(
                     PlayerId{1, Generation{1}},
                     MapGeneration{1},
                     JoinRequest{Team::Terrorist, 1},
                     TickId{1})
               .changed);
    const auto expired = timeout.onFrame(TickId{129});
    assert(expired.kind == JoinActionKind::Failed);
    assert(expired.error == JoinError::Timeout);
}

} // namespace

int main() {
    testTokenizer();
    testMessageDecoder();
    testJoinState();
    testAlreadyAssignedAndInvalidMenu();
    testCounterTerroristAndDuplicatePrompt();
    testAttemptLimitAndTimeout();
    return 0;
}
