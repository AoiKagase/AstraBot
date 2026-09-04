// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#pragma once

#include <cstdint>

namespace astrabot::core {

using EntityId = std::uint32_t;

struct Generation {
    std::uint32_t value{0};

    static constexpr Generation invalid() noexcept { return {}; }
    constexpr bool isValid() const noexcept { return value != 0; }

    friend constexpr bool operator==(Generation left, Generation right) noexcept {
        return left.value == right.value;
    }
    friend constexpr bool operator!=(Generation left, Generation right) noexcept {
        return !(left == right);
    }
    friend constexpr bool operator<(Generation left, Generation right) noexcept {
        return left.value < right.value;
    }
};

struct MapGeneration {
    std::uint32_t value{0};

    static constexpr MapGeneration invalid() noexcept { return {}; }
    constexpr bool isValid() const noexcept { return value != 0; }

    friend constexpr bool operator==(MapGeneration left, MapGeneration right) noexcept {
        return left.value == right.value;
    }
    friend constexpr bool operator!=(MapGeneration left, MapGeneration right) noexcept {
        return !(left == right);
    }
    friend constexpr bool operator<(MapGeneration left, MapGeneration right) noexcept {
        return left.value < right.value;
    }
};

struct PlayerId {
    std::uint16_t slot{0};
    Generation generation{};

    static constexpr PlayerId invalid() noexcept { return {}; }
    constexpr bool isValid() const noexcept {
        return slot != 0 && generation.isValid();
    }
    constexpr bool sameSlot(PlayerId other) const noexcept {
        return slot != 0 && slot == other.slot;
    }
    constexpr bool sameGeneration(PlayerId other) const noexcept {
        return sameSlot(other) && generation == other.generation;
    }

    friend constexpr bool operator==(PlayerId left, PlayerId right) noexcept {
        return left.slot == right.slot && left.generation == right.generation;
    }
    friend constexpr bool operator!=(PlayerId left, PlayerId right) noexcept {
        return !(left == right);
    }
    friend constexpr bool operator<(PlayerId left, PlayerId right) noexcept {
        return left.slot < right.slot ||
               (left.slot == right.slot && left.generation < right.generation);
    }
};

struct BotAgentId {
    std::uint32_t value{0};

    static constexpr BotAgentId invalid() noexcept { return {}; }
    constexpr bool isValid() const noexcept { return value != 0; }

    friend constexpr bool operator==(BotAgentId left, BotAgentId right) noexcept {
        return left.value == right.value;
    }
    friend constexpr bool operator!=(BotAgentId left, BotAgentId right) noexcept {
        return !(left == right);
    }
    friend constexpr bool operator<(BotAgentId left, BotAgentId right) noexcept {
        return left.value < right.value;
    }
};

struct TickId {
    std::uint64_t value{0};

    static constexpr TickId invalid() noexcept { return {}; }
    constexpr bool isValid() const noexcept { return value != 0; }
    constexpr bool isBefore(TickId other) const noexcept { return value < other.value; }
    constexpr bool isAfter(TickId other) const noexcept { return value > other.value; }

    friend constexpr bool operator==(TickId left, TickId right) noexcept {
        return left.value == right.value;
    }
    friend constexpr bool operator!=(TickId left, TickId right) noexcept {
        return !(left == right);
    }
    friend constexpr bool operator<(TickId left, TickId right) noexcept {
        return left.value < right.value;
    }
};

} // namespace astrabot::core
