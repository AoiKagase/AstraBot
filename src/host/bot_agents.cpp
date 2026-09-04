// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include "host/bot_agents.hpp"

#include <limits>

namespace astrabot::host {

BotAgentResult BotAgentRegistry::bind(
    PlayerId player, MapGeneration map) noexcept {
    if (!player.isValid()) {
        return BotAgentResult::rejected(BotAgentError::InvalidPlayer);
    }
    if (!map.isValid()) {
        return BotAgentResult::rejected(BotAgentError::InvalidMap);
    }
    const std::uint16_t index = entryIndex(player);
    if (index >= entries_.size()) {
        return BotAgentResult::rejected(BotAgentError::InvalidPlayer);
    }
    Entry& entry = entries_[index];
    if (entry.occupied) {
        return BotAgentResult::rejected(BotAgentError::AlreadyMapped);
    }
    if (mappingCount_ >= kMaxBotAgents ||
        nextAgentValue_ == std::numeric_limits<std::uint32_t>::max()) {
        return BotAgentResult::rejected(BotAgentError::CapacityExhausted);
    }

    const BotAgentId agent{nextAgentValue_};
    ++nextAgentValue_;
    entry.binding = {agent, player, map};
    entry.occupied = true;
    ++mappingCount_;
    return BotAgentResult::acceptedBinding(entry.binding);
}

BotAgentResult BotAgentRegistry::unbind(PlayerId player) noexcept {
    if (!player.isValid()) {
        return BotAgentResult::rejected(BotAgentError::InvalidPlayer);
    }

    const std::uint16_t index = entryIndex(player);
    if (index >= entries_.size()) {
        return BotAgentResult::rejected(BotAgentError::InvalidPlayer);
    }
    Entry& entry = entries_[index];
    if (!entry.occupied) {
        return BotAgentResult::acceptedNoOp();
    }
    if (entry.binding.player.generation != player.generation) {
        return BotAgentResult::rejected(BotAgentError::StalePlayerGeneration);
    }

    const BotAgentBinding oldBinding = entry.binding;
    entry = {};
    --mappingCount_;
    return BotAgentResult::acceptedBinding(oldBinding);
}

void BotAgentRegistry::clearMappings() noexcept {
    entries_ = {};
    mappingCount_ = 0;
}

void BotAgentRegistry::reset() noexcept {
    clearMappings();
    nextAgentValue_ = 1;
}

BotAgentBinding BotAgentRegistry::findByPlayer(PlayerId player) const noexcept {
    if (!player.isValid()) {
        return {};
    }
    const std::uint16_t index = entryIndex(player);
    if (index >= entries_.size() || !entries_[index].occupied ||
        entries_[index].binding.player != player) {
        return {};
    }
    return entries_[index].binding;
}

BotAgentBinding BotAgentRegistry::findByAgent(BotAgentId agent) const noexcept {
    if (!agent.isValid()) {
        return {};
    }
    for (const Entry& entry : entries_) {
        if (entry.occupied && entry.binding.agent == agent) {
            return entry.binding;
        }
    }
    return {};
}

std::uint16_t BotAgentRegistry::entryIndex(PlayerId player) noexcept {
    return player.slot == 0 ? kMaxBotAgents :
                              static_cast<std::uint16_t>(player.slot - 1U);
}

} // namespace astrabot::host
