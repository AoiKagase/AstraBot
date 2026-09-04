// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include "core/command.hpp"
#include "core/identity.hpp"
#include "host/game_host.hpp"

#include <cassert>
#include <limits>

namespace {

using astrabot::core::BotAgentId;
using astrabot::core::BotCommand;
using astrabot::core::Button;
using astrabot::core::CommandError;
using astrabot::core::Generation;
using astrabot::core::MapGeneration;
using astrabot::core::Movement;
using astrabot::core::PlayerId;
using astrabot::core::TickId;
using astrabot::core::ViewAngles;
using astrabot::host::CommandResult;
using astrabot::host::HostError;
using astrabot::host::LifecycleEvent;
using astrabot::host::LifecycleEventKind;
using astrabot::host::LifecycleResult;

void testIdentityValues() {
    const PlayerId first{4, Generation{1}};
    const PlayerId same{4, Generation{1}};
    const PlayerId reusedSlot{4, Generation{2}};
    const PlayerId laterSlot{5, Generation{1}};

    assert(first.isValid());
    assert(first == same);
    assert(first != reusedSlot);
    assert(first.sameSlot(reusedSlot));
    assert(!first.sameGeneration(reusedSlot));
    assert(first < laterSlot);
    assert(reusedSlot < laterSlot);
    assert(!PlayerId::invalid().isValid());

    const BotAgentId agent{9};
    assert(agent.isValid());
    assert(BotAgentId::invalid() < agent);

    const TickId tick{12};
    assert(tick.isValid());
    assert(tick.isBefore(TickId{13}));
    assert(tick.isAfter(TickId{11}));
    assert(TickId::invalid() < tick);

    const MapGeneration map{3};
    assert(map.isValid());
    assert(map < MapGeneration{4});
}

void testCommandDefaultsAndBounds() {
    const BotCommand defaults{};
    assert(!defaults.validate());
    assert(defaults.validate().error == CommandError::InvalidMsec);

    BotCommand command = BotCommand::neutral(16);
    assert(command.validate());
    assert(command.view == ViewAngles{});
    assert(command.movement == Movement{});

    command.movement.forward = 400.0F;
    command.movement.side = -400.0F;
    command.movement.up = 400.0F;
    command.view = {89.0F, -180.0F, 50.0F};
    command.buttons = Button::Attack | Button::Forward;
    assert(command.validate());

    command.movement.forward = 400.1F;
    assert(command.validate().error == CommandError::MovementOutOfRange);

    command = BotCommand::neutral(16);
    command.view.pitch = std::numeric_limits<float>::quiet_NaN();
    assert(command.validate().error == CommandError::NonFiniteView);

    command = BotCommand::neutral(16);
    command.buttons = 1U << 31U;
    assert(command.validate().error == CommandError::UnknownButtons);

    command = BotCommand::neutral(0);
    assert(command.validate().error == CommandError::InvalidMsec);
}

void testHostResults() {
    const LifecycleEvent event{
        LifecycleEventKind::MapActivated,
        1,
        MapGeneration{7},
        PlayerId::invalid(),
    };
    const LifecycleResult accepted = LifecycleResult::acceptedEvent(event);
    assert(accepted);
    assert(accepted.event.map == MapGeneration{7});
    assert(accepted.error == HostError::None);

    const LifecycleResult rejected = LifecycleResult::rejected(HostError::InvalidLifecycle);
    assert(!rejected);
    assert(rejected.error == HostError::InvalidLifecycle);

    const CommandResult command = CommandResult::acceptedCommand(TickId{8});
    assert(command);
    assert(command.tick == TickId{8});

    const CommandResult stale = CommandResult::rejected(HostError::StaleTick, TickId{7});
    assert(!stale);
    assert(stale.error == HostError::StaleTick);
}

} // namespace

int main() {
    testIdentityValues();
    testCommandDefaultsAndBounds();
    testHostResults();
    return 0;
}
