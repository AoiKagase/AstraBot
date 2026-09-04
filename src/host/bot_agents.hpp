// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#pragma once

#include "host/game_host.hpp"
#include "host/player_registry.hpp"

#include <array>
#include <cstdint>

namespace astrabot::host {

constexpr std::uint16_t kMaxBotAgents = kMaxClientSlots;

enum class BotAgentError : std::uint8_t {
    None = 0,
    InvalidAgent,
    InvalidPlayer,
    InvalidMap,
    AlreadyMapped,
    NotMapped,
    StalePlayerGeneration,
    CapacityExhausted,
    InvalidLifecycle,
};

struct BotAgentBinding {
    BotAgentId agent{};
    PlayerId player{};
    MapGeneration map{};

    constexpr bool isValid() const noexcept {
        return agent.isValid() && player.isValid() && map.isValid();
    }

    friend constexpr bool operator==(
        BotAgentBinding left, BotAgentBinding right) noexcept {
        return left.agent == right.agent && left.player == right.player &&
               left.map == right.map;
    }
    friend constexpr bool operator!=(
        BotAgentBinding left, BotAgentBinding right) noexcept {
        return !(left == right);
    }
};

struct BotAgentResult {
    BotAgentBinding binding{};
    BotAgentError error{BotAgentError::None};
    bool accepted{false};
    bool changed{false};

    static constexpr BotAgentResult acceptedBinding(
        BotAgentBinding value) noexcept {
        return {value, BotAgentError::None, true, true};
    }
    static constexpr BotAgentResult acceptedNoOp(
        BotAgentBinding value = {}) noexcept {
        return {value, BotAgentError::None, true, false};
    }
    static constexpr BotAgentResult rejected(BotAgentError reason) noexcept {
        return {{}, reason, false, false};
    }

    constexpr bool succeeded() const noexcept {
        return accepted && error == BotAgentError::None;
    }
    constexpr bool isNoOp() const noexcept {
        return succeeded() && !changed;
    }
    constexpr explicit operator bool() const noexcept { return succeeded(); }
};

class BotAgentRegistry final {
public:
    BotAgentResult bind(
        PlayerId player, MapGeneration map) noexcept;
    BotAgentResult unbind(PlayerId player) noexcept;

    void clearMappings() noexcept;
    void reset() noexcept;

    BotAgentBinding findByPlayer(PlayerId player) const noexcept;
    BotAgentBinding findByAgent(BotAgentId agent) const noexcept;
    std::uint16_t mappingCount() const noexcept { return mappingCount_; }
    std::uint32_t nextAgentValue() const noexcept { return nextAgentValue_; }

private:
    struct Entry {
        BotAgentBinding binding{};
        bool occupied{false};
    };

    std::array<Entry, kMaxBotAgents> entries_{};
    std::uint32_t nextAgentValue_{1};
    std::uint16_t mappingCount_{0};

    static std::uint16_t entryIndex(PlayerId player) noexcept;
};

} // namespace astrabot::host
