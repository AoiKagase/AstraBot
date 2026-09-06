// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include "adapter/cstrike/messages.hpp"

#include <cstddef>

namespace astrabot::adapter::cstrike {
namespace {

constexpr std::uint8_t kVguiMenuFields = 5;
constexpr std::uint8_t kShowMenuFields = 4;
constexpr std::uint8_t kTeamInfoFields = 2;

bool isWhitespace(char value) noexcept {
    return value == ' ' || value == '\t' || value == '\r' || value == '\n';
}

bool textEquals(const char* left, const char* right) noexcept {
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

bool isKnownShowMenuToken(const char* value) noexcept {
    return textEquals(value, "#Team_Select") ||
           textEquals(value, "#IG_Team_Select") ||
           textEquals(value, "#Terrorist_Select") ||
           textEquals(value, "#CT_Select");
}

} // namespace

CommandParseError tokenizeCommand(
    const char* command,
    TokenizedCommand& result) noexcept {
    result = {};
    if (command == nullptr) {
        return CommandParseError::NullCommand;
    }

    std::uint16_t length = 0;
    while (length <= kMaxCommandBytes && command[length] != '\0') {
        if (command[length] == ';' || command[length] == '\\') {
            return command[length] == ';'
                       ? CommandParseError::CommandSeparator
                       : CommandParseError::TokenTooLong;
        }
        ++length;
    }
    if (length == 0U) {
        return CommandParseError::EmptyCommand;
    }
    if (length > kMaxCommandBytes - 1U) {
        return CommandParseError::CommandTooLong;
    }

    std::uint16_t index = 0;
    while (index < length) {
        while (index < length && isWhitespace(command[index])) {
            ++index;
        }
        if (index >= length) {
            break;
        }
        if (result.count >= kMaxCommandTokens) {
            return CommandParseError::TooManyTokens;
        }

        const std::uint8_t tokenIndex = result.count;
        std::uint8_t tokenLength = 0;
        bool quoted = false;
        bool openedQuote = false;
        while (index < length && (!isWhitespace(command[index]) || quoted)) {
            const char value = command[index];
            if (value == '"') {
                quoted = !quoted;
                openedQuote = true;
                ++index;
                continue;
            }
            if (value == ';' || value == '\\') {
                return value == ';' ? CommandParseError::CommandSeparator
                                    : CommandParseError::TokenTooLong;
            }
            if (tokenLength + 1U >= kMaxCommandTokenBytes) {
                return CommandParseError::TokenTooLong;
            }
            result.tokens[tokenIndex][tokenLength++] = value;
            ++index;
            if (openedQuote && !quoted && index < length &&
                !isWhitespace(command[index])) {
                return CommandParseError::UnterminatedQuote;
            }
        }
        if (quoted) {
            return CommandParseError::UnterminatedQuote;
        }
        result.tokens[tokenIndex][tokenLength] = '\0';
        ++result.count;
    }

    if (result.count == 0U) {
        return CommandParseError::EmptyCommand;
    }
    if (result.count >= 2U) {
        for (std::uint8_t index2 = 0;
             index2 < kMaxCommandTokenBytes &&
             result.tokens[1][index2] != '\0';
             ++index2) {
            result.args[index2] = result.tokens[1][index2];
        }
    }
    return CommandParseError::None;
}

CommandParseError parseMenuSelect(
    const char* command,
    TokenizedCommand& result,
    std::uint8_t& selection) noexcept {
    selection = 0;
    const CommandParseError error = tokenizeCommand(command, result);
    if (error != CommandParseError::None) {
        return error;
    }
    if (result.count != 2U || !textEquals(result.tokens[0].data(), "menuselect")) {
        return CommandParseError::InvalidMenuselect;
    }
    const char* value = result.tokens[1].data();
    if (value[0] < '1' || value[0] > '9' || value[1] != '\0') {
        return CommandParseError::InvalidMenuselect;
    }
    selection = static_cast<std::uint8_t>(value[0] - '0');
    return CommandParseError::None;
}

void MessageDecoder::configure(
    UserMessageIds ids,
    MessageEventSink sink,
    void* context) noexcept {
    ids_ = ids;
    sink_ = sink;
    sinkContext_ = context;
    reset();
}

void MessageDecoder::reset() noexcept {
    kind_ = MessageKind::None;
    recipientSlot_ = 0;
    fieldIndex_ = 0;
    active_ = false;
    malformed_ = false;
    lastError_ = MessageDecodeError::None;
    event_ = {};
    lastEvent_ = {};
    showMenuFragment_ = {};
}

void MessageDecoder::begin(
    int messageType,
    std::uint16_t recipientSlot) noexcept {
    if (active_) {
        fail(MessageDecodeError::InvalidShape);
    }
    resetCurrent();

    if (messageType > 0 && messageType == ids_.vguiMenu) {
        kind_ = MessageKind::VguiMenu;
    } else if (messageType > 0 && messageType == ids_.showMenu) {
        kind_ = MessageKind::ShowMenu;
    } else if (messageType > 0 && messageType == ids_.teamInfo) {
        kind_ = MessageKind::TeamInfo;
    } else if (messageType > 0 && messageType == ids_.hltv) {
        kind_ = MessageKind::Hltv;
    } else if (messageType > 0 && messageType == ids_.screenFade) {
        kind_ = MessageKind::ScreenFade;
    } else {
        kind_ = MessageKind::None;
        lastError_ = MessageDecodeError::UnknownMessage;
        return;
    }

    recipientSlot_ = recipientSlot;
    event_.kind = kind_;
    event_.recipientSlot = recipientSlot;
    active_ = true;
}

void MessageDecoder::writeByte(int value) noexcept {
    write(FieldKind::Byte, value);
}

void MessageDecoder::writeChar(int value) noexcept {
    write(FieldKind::Char, value);
}

void MessageDecoder::writeShort(int value) noexcept {
    write(FieldKind::Short, value);
}

void MessageDecoder::writeString(const char* value) noexcept {
    if (!active_) {
        return;
    }
    if (!expects(FieldKind::String)) {
        fail(MessageDecodeError::UnexpectedField);
        return;
    }
    writeText(value);
}

void MessageDecoder::end() noexcept {
    if (!active_) {
        return;
    }
    const MessageKind completedKind = kind_;
    const bool valid = !malformed_ && complete();
    if (!valid) {
        if (!malformed_) {
            fail(MessageDecodeError::InvalidShape);
        }
        active_ = false;
        return;
    }

    event_.needMore = event_.needMore == 0U ? 0U : 1U;
    active_ = false;
    if (completedKind == MessageKind::ShowMenu) {
        handleShowMenu();
    } else {
        emit(event_);
    }
}

void MessageDecoder::write(FieldKind expected, int value) noexcept {
    if (!active_) {
        return;
    }
    if (!expects(expected)) {
        fail(MessageDecodeError::UnexpectedField);
        return;
    }

    switch (kind_) {
    case MessageKind::ScreenFade:
        if (fieldIndex_ < 3) {
            if (value < -32768 || value > 65535) { fail(MessageDecodeError::InvalidShape); return; }
            event_.fadeTimesFlags[fieldIndex_] = static_cast<std::uint16_t>(value);
        } else {
            if (value < 0 || value > 255) { fail(MessageDecodeError::InvalidShape); return; }
            event_.fadeColor[fieldIndex_-3U] = static_cast<std::uint8_t>(value);
        }
        break;
    case MessageKind::Hltv:
        if (value < 0 || value > 255) { fail(MessageDecodeError::InvalidShape); return; }
        event_.hltv[fieldIndex_] = static_cast<std::uint8_t>(value);
        break;
    case MessageKind::VguiMenu:
        if (fieldIndex_ == 0U) {
            event_.menuType = static_cast<std::uint8_t>(value);
        } else if (fieldIndex_ == 1U) {
            event_.validSlots = static_cast<std::uint16_t>(value);
        } else if (fieldIndex_ == 2U) {
            event_.displayTime = static_cast<std::int16_t>(value);
        } else if (fieldIndex_ == 3U) {
            event_.needMore = static_cast<std::uint8_t>(value);
        }
        break;
    case MessageKind::ShowMenu:
        if (fieldIndex_ == 0U) {
            event_.validSlots = static_cast<std::uint16_t>(value);
        } else if (fieldIndex_ == 1U) {
            event_.displayTime = static_cast<std::int16_t>(value);
        } else if (fieldIndex_ == 2U) {
            event_.needMore = static_cast<std::uint8_t>(value);
        }
        break;
    case MessageKind::TeamInfo:
        if (fieldIndex_ == 0U) {
            if (value < 1 || value > 32) { fail(MessageDecodeError::InvalidShape); return; }
            event_.playerSlot = static_cast<std::uint16_t>(value);
        }
        break;
    case MessageKind::None:
        fail(MessageDecodeError::UnknownMessage);
        return;
    }
    ++fieldIndex_;
}

void MessageDecoder::writeText(const char* value) noexcept {
    if (value == nullptr) {
        fail(MessageDecodeError::NullString);
        return;
    }

    std::uint16_t length = 0;
    while (length <= kMaxMessageTextBytes && value[length] != '\0') {
        ++length;
    }
    if (length > kMaxMessageTextBytes) {
        fail(MessageDecodeError::StringTooLong);
        return;
    }

    for (std::uint16_t index = 0; index < length; ++index) {
        event_.text[index] = value[index];
    }
    event_.text[length] = '\0';
    ++fieldIndex_;
}

bool MessageDecoder::expects(FieldKind expected) const noexcept {
    if (kind_ == MessageKind::ScreenFade) return fieldIndex_ < 7 && expected == (fieldIndex_ < 3 ? FieldKind::Short : FieldKind::Byte);
    if (kind_ == MessageKind::Hltv) return fieldIndex_ < 2 && expected == FieldKind::Byte;
    if (kind_ == MessageKind::VguiMenu) {
        static constexpr FieldKind fields[] = {
            FieldKind::Byte,
            FieldKind::Short,
            FieldKind::Char,
            FieldKind::Byte,
            FieldKind::String,
        };
        return fieldIndex_ < kVguiMenuFields &&
               fields[fieldIndex_] == expected;
    }
    if (kind_ == MessageKind::ShowMenu) {
        static constexpr FieldKind fields[] = {
            FieldKind::Short,
            FieldKind::Char,
            FieldKind::Byte,
            FieldKind::String,
        };
        return fieldIndex_ < kShowMenuFields &&
               fields[fieldIndex_] == expected;
    }
    if (kind_ == MessageKind::TeamInfo) {
        static constexpr FieldKind fields[] = {
            FieldKind::Byte,
            FieldKind::String,
        };
        return fieldIndex_ < kTeamInfoFields &&
               fields[fieldIndex_] == expected;
    }
    return false;
}

bool MessageDecoder::complete() const noexcept {
    switch (kind_) {
    case MessageKind::ScreenFade:
        return fieldIndex_ == 7;
    case MessageKind::Hltv:
        return fieldIndex_ == 2;
    case MessageKind::VguiMenu:
        return fieldIndex_ == kVguiMenuFields;
    case MessageKind::ShowMenu:
        return fieldIndex_ == kShowMenuFields;
    case MessageKind::TeamInfo:
        return fieldIndex_ == kTeamInfoFields;
    case MessageKind::None:
        return false;
    }
    return false;
}

void MessageDecoder::fail(MessageDecodeError error) noexcept {
    malformed_ = true;
    lastError_ = error;
}

void MessageDecoder::emit(MessageEvent value) noexcept {
    lastEvent_ = value;
    if (sink_ != nullptr) {
        sink_(sinkContext_, value);
    }
}

void MessageDecoder::resetCurrent() noexcept {
    kind_ = MessageKind::None;
    recipientSlot_ = 0;
    fieldIndex_ = 0;
    active_ = false;
    malformed_ = false;
    lastError_ = MessageDecodeError::None;
    event_ = {};
}

void MessageDecoder::handleShowMenu() noexcept {
    if (event_.needMore != 0U) {
        if (!showMenuFragment_.active) {
            showMenuFragment_.active = true;
            showMenuFragment_.recipientSlot = event_.recipientSlot;
            showMenuFragment_.validSlots = event_.validSlots;
            showMenuFragment_.displayTime = event_.displayTime;
        } else if (showMenuFragment_.recipientSlot != event_.recipientSlot ||
                   showMenuFragment_.validSlots != event_.validSlots) {
            showMenuFragment_ = {};
            lastError_ = MessageDecodeError::InvalidShape;
            return;
        }

        const std::size_t currentLength = showMenuFragment_.length;
        std::size_t fragmentLength = 0;
        while (fragmentLength <= kMaxMessageTextBytes &&
               event_.text[fragmentLength] != '\0') {
            ++fragmentLength;
        }
        if (currentLength + fragmentLength > kMaxMessageTextBytes) {
            showMenuFragment_ = {};
            lastError_ = MessageDecodeError::FragmentOverflow;
            return;
        }
        for (std::size_t index = 0; index < fragmentLength; ++index) {
            showMenuFragment_.text[currentLength + index] = event_.text[index];
        }
        showMenuFragment_.length = static_cast<std::uint16_t>(
            currentLength + fragmentLength);
        showMenuFragment_.text[showMenuFragment_.length] = '\0';
        return;
    }

    if (showMenuFragment_.active) {
        const std::size_t currentLength = showMenuFragment_.length;
        std::size_t fragmentLength = 0;
        while (fragmentLength <= kMaxMessageTextBytes &&
               event_.text[fragmentLength] != '\0') {
            ++fragmentLength;
        }
        if (currentLength + fragmentLength > kMaxMessageTextBytes) {
            showMenuFragment_ = {};
            lastError_ = MessageDecodeError::FragmentOverflow;
            return;
        }
        for (std::size_t index = 0; index < fragmentLength; ++index) {
            showMenuFragment_.text[currentLength + index] = event_.text[index];
        }
        showMenuFragment_.length = static_cast<std::uint16_t>(
            currentLength + fragmentLength);
        showMenuFragment_.text[showMenuFragment_.length] = '\0';
        event_.recipientSlot = showMenuFragment_.recipientSlot;
        event_.validSlots = showMenuFragment_.validSlots;
        event_.displayTime = showMenuFragment_.displayTime;
        for (std::uint16_t index = 0; index <= showMenuFragment_.length; ++index) {
            event_.text[index] = showMenuFragment_.text[index];
        }
        showMenuFragment_ = {};
    }
    if (!isKnownShowMenuToken(event_.text.data())) {
        lastError_ = MessageDecodeError::UnknownMenuToken;
        return;
    }
    emit(event_);
}

} // namespace astrabot::adapter::cstrike
