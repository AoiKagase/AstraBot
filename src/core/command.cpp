// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include "core/command.hpp"

#include <cmath>

namespace astrabot::core {
namespace {

bool isFinite(const ViewAngles& view) noexcept {
    return std::isfinite(view.pitch) && std::isfinite(view.yaw) && std::isfinite(view.roll);
}

bool isFinite(const Movement& movement) noexcept {
    return std::isfinite(movement.forward) && std::isfinite(movement.side) &&
           std::isfinite(movement.up);
}

bool isInRange(const ViewAngles& view) noexcept {
    return view.pitch >= kMinPitch && view.pitch <= kMaxPitch &&
           view.yaw >= kMinYaw && view.yaw <= kMaxYaw &&
           view.roll >= kMinRoll && view.roll <= kMaxRoll;
}

bool isInRange(const Movement& movement) noexcept {
    return movement.forward >= -kMaxMovement && movement.forward <= kMaxMovement &&
           movement.side >= -kMaxMovement && movement.side <= kMaxMovement &&
           movement.up >= -kMaxMovement && movement.up <= kMaxMovement;
}

} // namespace

CommandValidation BotCommand::validate() const noexcept {
    if (msec == 0) {
        return {CommandError::InvalidMsec};
    }
    if (!isFinite(view)) {
        return {CommandError::NonFiniteView};
    }
    if (!isInRange(view)) {
        return {CommandError::ViewOutOfRange};
    }
    if (!isFinite(movement)) {
        return {CommandError::NonFiniteMovement};
    }
    if (!isInRange(movement)) {
        return {CommandError::MovementOutOfRange};
    }
    if ((buttons & ~kKnownButtonMask) != 0U) {
        return {CommandError::UnknownButtons};
    }
    return {};
}

} // namespace astrabot::core
