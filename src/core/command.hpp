// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#pragma once

#include "core/identity.hpp"

#include <cstdint>

namespace astrabot::core {

constexpr float kMaxMovement = 400.0F;
constexpr float kMinPitch = -89.0F;
constexpr float kMaxPitch = 89.0F;
constexpr float kMinYaw = -180.0F;
constexpr float kMaxYaw = 180.0F;
constexpr float kMinRoll = -50.0F;
constexpr float kMaxRoll = 50.0F;

enum class Button : std::uint32_t {
    None = 0,
    Attack = 1U << 0U,
    Jump = 1U << 1U,
    Duck = 1U << 2U,
    Forward = 1U << 3U,
    Back = 1U << 4U,
    Use = 1U << 5U,
    MoveLeft = 1U << 6U,
    MoveRight = 1U << 7U,
    Reload = 1U << 8U,
};

using ButtonMask = std::uint32_t;

constexpr ButtonMask kKnownButtonMask =
    static_cast<ButtonMask>(Button::Attack) |
    static_cast<ButtonMask>(Button::Jump) |
    static_cast<ButtonMask>(Button::Duck) |
    static_cast<ButtonMask>(Button::Forward) |
    static_cast<ButtonMask>(Button::Back) |
    static_cast<ButtonMask>(Button::Use) |
    static_cast<ButtonMask>(Button::MoveLeft) |
    static_cast<ButtonMask>(Button::MoveRight) |
    static_cast<ButtonMask>(Button::Reload);

constexpr ButtonMask operator|(Button left, Button right) noexcept {
    return static_cast<ButtonMask>(left) | static_cast<ButtonMask>(right);
}

struct ViewAngles {
    float pitch{0.0F};
    float yaw{0.0F};
    float roll{0.0F};

    friend bool operator==(const ViewAngles& left, const ViewAngles& right) noexcept {
        return left.pitch == right.pitch && left.yaw == right.yaw && left.roll == right.roll;
    }
    friend bool operator!=(const ViewAngles& left, const ViewAngles& right) noexcept {
        return !(left == right);
    }
};

struct Movement {
    float forward{0.0F};
    float side{0.0F};
    float up{0.0F};

    friend bool operator==(const Movement& left, const Movement& right) noexcept {
        return left.forward == right.forward && left.side == right.side && left.up == right.up;
    }
    friend bool operator!=(const Movement& left, const Movement& right) noexcept {
        return !(left == right);
    }
};

enum class CommandError : std::uint8_t {
    None = 0,
    InvalidMsec,
    NonFiniteView,
    ViewOutOfRange,
    NonFiniteMovement,
    MovementOutOfRange,
    UnknownButtons,
};

struct CommandValidation {
    CommandError error{CommandError::None};

    constexpr bool isValid() const noexcept { return error == CommandError::None; }
    constexpr explicit operator bool() const noexcept { return isValid(); }
};

struct BotCommand {
    ViewAngles view{};
    Movement movement{};
    ButtonMask buttons{0};
    std::uint8_t impulse{0};
    std::uint8_t msec{0};

    static constexpr BotCommand neutral(std::uint8_t durationMsec) noexcept {
        BotCommand command{};
        command.msec = durationMsec;
        return command;
    }

    CommandValidation validate() const noexcept;

    friend bool operator==(const BotCommand& left, const BotCommand& right) noexcept {
        return left.view == right.view && left.movement == right.movement &&
               left.buttons == right.buttons && left.impulse == right.impulse &&
               left.msec == right.msec;
    }
    friend bool operator!=(const BotCommand& left, const BotCommand& right) noexcept {
        return !(left == right);
    }
};

} // namespace astrabot::core
