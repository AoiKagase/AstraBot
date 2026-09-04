// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include "host/player_registry.hpp"

#include <cassert>

namespace {

using astrabot::core::BotCommand;
using astrabot::core::Generation;
using astrabot::core::PlayerId;
using astrabot::core::TickId;
using astrabot::host::HostError;
using astrabot::host::LifecycleChange;
using astrabot::host::LifecycleEventKind;
using astrabot::host::PlayerRegistry;

void testMapLifecycleAndSequence() {
    PlayerRegistry registry;

    const auto activated = registry.activateMap(2);
    assert(activated.changed());
    assert(activated.event.kind == LifecycleEventKind::MapActivated);
    assert(activated.event.sequence == 1);
    assert(activated.event.map == astrabot::core::MapGeneration{1});
    assert(!activated.event.tick.isValid());

    const auto duplicateActivate = registry.activateMap(2);
    assert(duplicateActivate.isNoOp());
    assert(duplicateActivate.event.sequence == 0);
    assert(registry.eventSequence() == 1);

    const auto firstFrame = registry.startFrame();
    assert(firstFrame.changed());
    assert(firstFrame.event.kind == LifecycleEventKind::FrameStarted);
    assert(firstFrame.event.tick == TickId{1});
    assert(firstFrame.event.sequence == 2);

    const auto deactivated = registry.deactivateMap();
    assert(deactivated.changed());
    assert(deactivated.event.kind == LifecycleEventKind::MapDeactivated);
    assert(deactivated.event.map == astrabot::core::MapGeneration{1});
    assert(deactivated.event.sequence == 3);
    assert(!registry.isMapActive());
    assert(!registry.isFrameStarted());
    assert(!registry.currentTick().isValid());

    const auto duplicateDeactivate = registry.deactivateMap();
    assert(duplicateDeactivate.isNoOp());
    assert(duplicateDeactivate.change == LifecycleChange::NoOp);
    assert(registry.eventSequence() == 3);

    const auto secondActivate = registry.activateMap(2);
    assert(secondActivate.changed());
    assert(secondActivate.event.map == astrabot::core::MapGeneration{2});
    assert(secondActivate.event.sequence == 4);
    assert(registry.mapGeneration() == astrabot::core::MapGeneration{2});
}

void testPlayerGenerationAndDisconnect() {
    PlayerRegistry registry;
    assert(registry.activateMap(2).succeeded());

    const auto connected = registry.registerPlayer(1);
    assert(connected.changed());
    assert(connected.event.kind == LifecycleEventKind::PlayerConnected);
    const PlayerId first = connected.event.player;
    assert(first == (PlayerId{1, Generation{1}}));
    assert(registry.isConnected(1));

    const auto duplicateConnect = registry.registerPlayer(1);
    assert(!duplicateConnect);
    assert(duplicateConnect.error == HostError::AlreadyConnected);
    assert(registry.eventSequence() == connected.event.sequence);

    const auto disconnected = registry.disconnectPlayer(first);
    assert(disconnected.changed());
    assert(disconnected.event.kind == LifecycleEventKind::PlayerDisconnected);
    assert(disconnected.event.player == first);
    assert(!registry.isConnected(1));

    const auto duplicateDisconnect = registry.disconnectPlayer(first);
    assert(duplicateDisconnect.isNoOp());
    assert(registry.eventSequence() == disconnected.event.sequence);

    const auto reconnected = registry.registerPlayer(1);
    assert(reconnected.changed());
    const PlayerId second = reconnected.event.player;
    assert(second == (PlayerId{1, Generation{2}}));

    const auto staleDisconnect = registry.disconnectPlayer(first);
    assert(!staleDisconnect);
    assert(staleDisconnect.error == HostError::StalePlayerGeneration);
    assert(registry.isConnected(1));

    assert(!registry.registerPlayer(0));
    assert(registry.registerPlayer(3).error == HostError::InvalidPlayer);
}

void testCommandAcceptanceAndRejection() {
    PlayerRegistry registry;
    assert(registry.activateMap(1));
    const PlayerId player = registry.registerPlayer(1).event.player;
    const BotCommand valid = BotCommand::neutral(16);

    const auto beforeFrame = registry.submitCommand(player, TickId{1}, valid);
    assert(!beforeFrame);
    assert(beforeFrame.error == HostError::FrameNotStarted);

    assert(registry.startFrame().event.tick == TickId{1});
    const auto accepted = registry.submitCommand(player, TickId{1}, valid);
    assert(accepted);
    assert(accepted.tick == TickId{1});

    const auto duplicate = registry.submitCommand(player, TickId{1}, valid);
    assert(!duplicate);
    assert(duplicate.error == HostError::DuplicateTick);

    assert(registry.startFrame().event.tick == TickId{2});
    const auto stale = registry.submitCommand(player, TickId{1}, valid);
    assert(!stale);
    assert(stale.error == HostError::StaleTick);

    const auto invalid = registry.submitCommand(
        player, TickId{2}, BotCommand::neutral(0));
    assert(!invalid);
    assert(invalid.error == HostError::InvalidCommand);

    const auto future = registry.submitCommand(player, TickId{3}, valid);
    assert(!future);
    assert(future.error == HostError::InvalidLifecycle);

    const PlayerId stalePlayer{1, Generation{1}};
    assert(registry.disconnectPlayer(player));
    const PlayerId reused = registry.registerPlayer(1).event.player;
    assert(reused.generation.value > stalePlayer.generation.value);
    const auto staleGeneration = registry.submitCommand(
        stalePlayer, TickId{2}, valid);
    assert(!staleGeneration);
    assert(staleGeneration.error == HostError::StalePlayerGeneration);
}

void testMapCleanupAndInactiveRejection() {
    PlayerRegistry registry;
    assert(registry.activateMap());
    const PlayerId player = registry.registerPlayer(1).event.player;
    assert(registry.startFrame());
    assert(registry.submitCommand(player, TickId{1}, BotCommand::neutral(16)));

    assert(registry.deactivateMap());
    assert(!registry.currentPlayer(1).isValid());
    assert(!registry.isConnected(1));
    assert(!registry.startFrame());
    assert(registry.startFrame().error == HostError::NotMapActive);
    assert(registry.registerPlayer(1).error == HostError::NotMapActive);
    assert(registry.submitCommand(player, TickId{1}, BotCommand::neutral(16)).error ==
           HostError::NotMapActive);
}

} // namespace

int main() {
    testMapLifecycleAndSequence();
    testPlayerGenerationAndDisconnect();
    testCommandAcceptanceAndRejection();
    testMapCleanupAndInactiveRejection();
    return 0;
}
