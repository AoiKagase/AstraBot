// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include "host/player_registry.hpp"

#include <limits>

namespace astrabot::host {

namespace {

constexpr std::uint16_t kFirstClientSlot = 1;

} // namespace

LifecycleResult PlayerRegistry::activateMap(std::uint16_t clientMax) noexcept {
    if (mapActive_) {
        return LifecycleResult::acceptedNoOp();
    }
    if (clientMax < kFirstClientSlot || clientMax > kMaxClientSlots) {
        return LifecycleResult::rejected(HostError::InvalidLifecycle);
    }
    if (mapGeneration_.value == std::numeric_limits<std::uint32_t>::max() ||
        eventSequence_ == std::numeric_limits<EventSequence>::max()) {
        return LifecycleResult::rejected(HostError::InvalidLifecycle);
    }

    mapActive_ = true;
    frameStarted_ = false;
    currentTick_ = TickId::invalid();
    clientMax_ = clientMax;
    ++mapGeneration_.value;
    for (SlotState& slot : slots_) {
        slot.player = PlayerId::invalid();
        slot.lastCommandTick = TickId::invalid();
        slot.connected = false;
    }
    return event(LifecycleEventKind::MapActivated);
}

LifecycleResult PlayerRegistry::deactivateMap() noexcept {
    if (!mapActive_) {
        return LifecycleResult::acceptedNoOp();
    }
    if (eventSequence_ == std::numeric_limits<EventSequence>::max()) {
        return LifecycleResult::rejected(HostError::InvalidLifecycle);
    }

    const LifecycleResult result = event(LifecycleEventKind::MapDeactivated);
    clearMapState();
    return result;
}

LifecycleResult PlayerRegistry::registerPlayer(std::uint16_t slot) noexcept {
    if (!mapActive_) {
        return LifecycleResult::rejected(HostError::NotMapActive);
    }
    if (!isValidSlot(slot)) {
        return LifecycleResult::rejected(HostError::InvalidPlayer);
    }

    SlotState& state = slots_[slot - kFirstClientSlot];
    if (state.connected) {
        return LifecycleResult::rejected(HostError::AlreadyConnected);
    }
    if (state.generation.value == std::numeric_limits<std::uint32_t>::max() ||
        eventSequence_ == std::numeric_limits<EventSequence>::max()) {
        return LifecycleResult::rejected(HostError::InvalidLifecycle);
    }

    state.generation = nextGeneration(state.generation);
    state.player = PlayerId{slot, state.generation};
    state.lastCommandTick = TickId::invalid();
    state.connected = true;
    return event(LifecycleEventKind::PlayerConnected, state.player);
}

LifecycleResult PlayerRegistry::disconnectPlayer(PlayerId player) noexcept {
    if (!mapActive_) {
        return LifecycleResult::rejected(HostError::NotMapActive);
    }
    if (!player.isValid() || !isValidSlot(player.slot)) {
        return LifecycleResult::rejected(HostError::InvalidPlayer);
    }

    SlotState& state = slots_[player.slot - kFirstClientSlot];
    if (state.generation != player.generation) {
        return LifecycleResult::rejected(HostError::StalePlayerGeneration);
    }
    if (!state.connected) {
        return LifecycleResult::acceptedNoOp();
    }
    if (eventSequence_ == std::numeric_limits<EventSequence>::max()) {
        return LifecycleResult::rejected(HostError::InvalidLifecycle);
    }

    state.connected = false;
    state.lastCommandTick = TickId::invalid();
    const LifecycleResult result = event(
        LifecycleEventKind::PlayerDisconnected, state.player);
    state.player = PlayerId::invalid();
    return result;
}

LifecycleResult PlayerRegistry::disconnectSlot(std::uint16_t slot) noexcept {
    if (!mapActive_) {
        return LifecycleResult::rejected(HostError::NotMapActive);
    }
    if (!isValidSlot(slot)) {
        return LifecycleResult::rejected(HostError::InvalidPlayer);
    }

    const SlotState& state = slots_[slot - kFirstClientSlot];
    if (!state.connected) {
        return LifecycleResult::acceptedNoOp();
    }
    return disconnectPlayer(state.player);
}

LifecycleResult PlayerRegistry::startFrame() noexcept {
    if (!mapActive_) {
        return LifecycleResult::rejected(HostError::NotMapActive);
    }
    if (currentTick_.value == std::numeric_limits<std::uint64_t>::max()) {
        return LifecycleResult::rejected(HostError::InvalidLifecycle);
    }
    if (eventSequence_ == std::numeric_limits<EventSequence>::max()) {
        return LifecycleResult::rejected(HostError::InvalidLifecycle);
    }

    ++currentTick_.value;
    frameStarted_ = true;
    LifecycleResult result = event(LifecycleEventKind::FrameStarted);
    result.event.tick = currentTick_;
    return result;
}

CommandResult PlayerRegistry::submitCommand(
    PlayerId player,
    TickId tick,
    const BotCommand& command) noexcept {
    if (!mapActive_) {
        return CommandResult::rejected(HostError::NotMapActive, tick);
    }
    if (!player.isValid() || !isValidSlot(player.slot)) {
        return CommandResult::rejected(HostError::InvalidPlayer, tick);
    }

    SlotState& state = slots_[player.slot - kFirstClientSlot];
    if (!state.connected) {
        return CommandResult::rejected(HostError::NotConnected, tick);
    }
    if (state.player.generation != player.generation) {
        return CommandResult::rejected(HostError::StalePlayerGeneration, tick);
    }
    if (!frameStarted_) {
        return CommandResult::rejected(HostError::FrameNotStarted, tick);
    }
    if (tick.value < currentTick_.value) {
        return CommandResult::rejected(HostError::StaleTick, tick);
    }
    if (tick.value > currentTick_.value) {
        return CommandResult::rejected(HostError::InvalidLifecycle, tick);
    }
    if (state.lastCommandTick == tick) {
        return CommandResult::rejected(HostError::DuplicateTick, tick);
    }
    if (!command.validate()) {
        return CommandResult::rejected(HostError::InvalidCommand, tick);
    }

    state.lastCommandTick = tick;
    return CommandResult::acceptedCommand(tick);
}

void PlayerRegistry::reset() noexcept {
    slots_ = {};
    mapGeneration_ = MapGeneration::invalid();
    eventSequence_ = 0;
    currentTick_ = TickId::invalid();
    clientMax_ = 0;
    mapActive_ = false;
    frameStarted_ = false;
}

bool PlayerRegistry::isConnected(std::uint16_t slot) const noexcept {
    return isValidSlot(slot) && slots_[slot - kFirstClientSlot].connected;
}

PlayerId PlayerRegistry::currentPlayer(std::uint16_t slot) const noexcept {
    if (!isValidSlot(slot)) {
        return PlayerId::invalid();
    }
    return slots_[slot - kFirstClientSlot].player;
}

bool PlayerRegistry::isValidSlot(std::uint16_t slot) const noexcept {
    return slot >= kFirstClientSlot && slot <= clientMax_ &&
           slot <= kMaxClientSlots;
}

Generation PlayerRegistry::nextGeneration(Generation previous) noexcept {
    return Generation{previous.value + 1};
}

LifecycleResult PlayerRegistry::event(
    LifecycleEventKind kind, PlayerId player) noexcept {
    ++eventSequence_;
    return LifecycleResult::acceptedEvent({
        kind,
        eventSequence_,
        mapGeneration_,
        player,
        currentTick_,
    });
}

void PlayerRegistry::clearMapState() noexcept {
    for (SlotState& slot : slots_) {
        slot.player = PlayerId::invalid();
        slot.lastCommandTick = TickId::invalid();
        slot.connected = false;
    }
    clientMax_ = 0;
    currentTick_ = TickId::invalid();
    frameStarted_ = false;
    mapActive_ = false;
}

} // namespace astrabot::host
