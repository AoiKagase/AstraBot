// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include "host/bot_agents.hpp"

#include <cassert>

namespace {

using astrabot::core::Generation;
using astrabot::core::MapGeneration;
using astrabot::core::PlayerId;
using astrabot::host::BotAgentError;
using astrabot::host::BotAgentRegistry;

void testBindingAndLookup() {
    BotAgentRegistry registry;
    const PlayerId player{2, Generation{1}};
    const auto first = registry.bind(player, MapGeneration{3});
    assert(first);
    assert(first.changed);
    assert(first.binding.agent.value == 1);
    assert(registry.mappingCount() == 1);
    assert(registry.findByPlayer(player) == first.binding);
    assert(registry.findByAgent(first.binding.agent) == first.binding);

    const auto duplicate = registry.bind(player, MapGeneration{3});
    assert(!duplicate);
    assert(duplicate.error == BotAgentError::AlreadyMapped);
}

void testStaleUnbindAndMapCleanup() {
    BotAgentRegistry registry;
    const PlayerId firstPlayer{1, Generation{1}};
    const PlayerId reusedPlayer{1, Generation{2}};
    const auto first = registry.bind(firstPlayer, MapGeneration{1});
    assert(first);

    const auto stale = registry.unbind(reusedPlayer);
    assert(!stale);
    assert(stale.error == BotAgentError::StalePlayerGeneration);
    assert(registry.mappingCount() == 1);

    assert(registry.unbind(firstPlayer));
    assert(registry.mappingCount() == 0);
    assert(!registry.findByAgent(first.binding.agent).isValid());

    const auto second = registry.bind(reusedPlayer, MapGeneration{2});
    assert(second);
    assert(second.binding.agent.value > first.binding.agent.value);
    registry.clearMappings();
    assert(registry.mappingCount() == 0);
    assert(registry.nextAgentValue() > second.binding.agent.value);
    assert(registry.unbind(reusedPlayer).isNoOp());
}

void testInvalidValuesAndReset() {
    BotAgentRegistry registry;
    assert(registry.bind(PlayerId::invalid(), MapGeneration{1}).error ==
           BotAgentError::InvalidPlayer);
    assert(registry.bind(PlayerId{1, Generation{1}}, MapGeneration::invalid()).error ==
           BotAgentError::InvalidMap);
    assert(registry.unbind(PlayerId::invalid()).error ==
           BotAgentError::InvalidPlayer);

    const auto binding = registry.bind(
        PlayerId{1, Generation{1}}, MapGeneration{1});
    assert(binding);
    registry.reset();
    assert(registry.mappingCount() == 0);
    assert(registry.nextAgentValue() == 1);
    assert(!registry.findByPlayer(binding.binding.player).isValid());
}

} // namespace

int main() {
    testBindingAndLookup();
    testStaleUnbindAndMapCleanup();
    testInvalidValuesAndReset();
    return 0;
}
