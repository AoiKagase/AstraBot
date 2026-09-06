// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#pragma once

#include <array>
#include <cstdint>

namespace astrabot::adapter::cstrike {

constexpr std::uint16_t kMaxMessageTextBytes = 256;
constexpr std::uint16_t kMaxCommandBytes = 64;
constexpr std::uint8_t kMaxCommandTokens = 4;
constexpr std::uint8_t kMaxCommandTokenBytes = 16;

enum class MessageKind : std::uint8_t {
    None = 0,
    VguiMenu,
    ShowMenu,
    TeamInfo,
    Hltv,
};

struct UserMessageIds {
    int vguiMenu{0};
    int showMenu{0};
    int teamInfo{0};
    int hltv{0}; // Optional capability, discovered from the GameDLL.

    constexpr bool valid() const noexcept {
        return vguiMenu > 0 && showMenu > 0 && teamInfo > 0;
    }
};

struct MessageEvent {
    MessageKind kind{MessageKind::None};
    std::uint16_t recipientSlot{0};
    std::uint16_t playerSlot{0};
    std::uint16_t validSlots{0};
    std::int16_t displayTime{0};
    std::uint8_t menuType{0};
    std::uint8_t needMore{0};
    std::array<std::uint8_t,2> hltv{};
    std::array<char, kMaxMessageTextBytes + 1U> text{};
};

enum class MessageDecodeError : std::uint8_t {
    None = 0,
    UnknownMessage,
    UnexpectedField,
    NullString,
    StringTooLong,
    InvalidShape,
    FragmentOverflow,
    UnknownMenuToken,
};

enum class CommandParseError : std::uint8_t {
    None = 0,
    NullCommand,
    EmptyCommand,
    CommandTooLong,
    TooManyTokens,
    TokenTooLong,
    UnterminatedQuote,
    CommandSeparator,
    InvalidMenuselect,
};

struct TokenizedCommand {
    std::uint8_t count{0};
    std::array<std::array<char, kMaxCommandTokenBytes>, kMaxCommandTokens>
        tokens{};
    std::array<char, kMaxCommandTokenBytes> args{};
};

CommandParseError tokenizeCommand(
    const char* command,
    TokenizedCommand& result) noexcept;
CommandParseError parseMenuSelect(
    const char* command,
    TokenizedCommand& result,
    std::uint8_t& selection) noexcept;

using MessageEventSink = void (*)(
    void* context,
    const MessageEvent& event) noexcept;

class MessageDecoder final {
public:
    void configure(
        UserMessageIds ids,
        MessageEventSink sink,
        void* context) noexcept;
    void reset() noexcept;

    void begin(int messageType, std::uint16_t recipientSlot) noexcept;
    void writeByte(int value) noexcept;
    void writeChar(int value) noexcept;
    void writeShort(int value) noexcept;
    void writeString(const char* value) noexcept;
    void end() noexcept;

    bool active() const noexcept { return active_; }
    MessageDecodeError lastError() const noexcept { return lastError_; }
    const MessageEvent& lastEvent() const noexcept { return lastEvent_; }

private:
    enum class FieldKind : std::uint8_t {
        Byte,
        Char,
        Short,
        String,
    };

    struct Fragment {
        bool active{false};
        std::uint16_t recipientSlot{0};
        std::uint16_t validSlots{0};
        std::int16_t displayTime{0};
        std::uint16_t length{0};
        std::array<char, kMaxMessageTextBytes + 1U> text{};
    };

    UserMessageIds ids_{};
    MessageEventSink sink_{nullptr};
    void* sinkContext_{nullptr};
    MessageKind kind_{MessageKind::None};
    std::uint16_t recipientSlot_{0};
    std::uint8_t fieldIndex_{0};
    bool active_{false};
    bool malformed_{false};
    MessageDecodeError lastError_{MessageDecodeError::None};
    MessageEvent event_{};
    MessageEvent lastEvent_{};
    Fragment showMenuFragment_{};

    void write(FieldKind expected, int value) noexcept;
    void writeText(const char* value) noexcept;
    bool expects(FieldKind expected) const noexcept;
    bool complete() const noexcept;
    void fail(MessageDecodeError error) noexcept;
    void emit(MessageEvent value) noexcept;
    void resetCurrent() noexcept;
    void handleShowMenu() noexcept;
};

} // namespace astrabot::adapter::cstrike
