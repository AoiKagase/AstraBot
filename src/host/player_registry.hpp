// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#pragma once

#include "host/game_host.hpp"

#include <array>
#include <cstdint>

namespace astrabot::host {

constexpr std::uint16_t kMaxClientSlots = 32;

class PlayerRegistry final {
public:
    LifecycleResult activateMap(
        std::uint16_t clientMax = kMaxClientSlots) noexcept;
    LifecycleResult deactivateMap() noexcept;

    LifecycleResult registerPlayer(std::uint16_t slot) noexcept;
    LifecycleResult disconnectPlayer(PlayerId player) noexcept;
    LifecycleResult disconnectSlot(std::uint16_t slot) noexcept;

    LifecycleResult startFrame() noexcept;
    CommandResult submitCommand(
        PlayerId player,
        TickId tick,
        const BotCommand& command) noexcept;

    void reset() noexcept;

    bool isMapActive() const noexcept { return mapActive_; }
    bool isFrameStarted() const noexcept { return frameStarted_; }
    std::uint16_t clientMax() const noexcept { return clientMax_; }
    MapGeneration mapGeneration() const noexcept { return mapGeneration_; }
    TickId currentTick() const noexcept { return currentTick_; }
    EventSequence eventSequence() const noexcept { return eventSequence_; }

    bool isConnected(std::uint16_t slot) const noexcept;
    PlayerId currentPlayer(std::uint16_t slot) const noexcept;

private:
    struct SlotState {
        Generation generation{};
        PlayerId player{};
        TickId lastCommandTick{};
        bool connected{false};
    };

    std::array<SlotState, kMaxClientSlots> slots_{};
    MapGeneration mapGeneration_{};
    EventSequence eventSequence_{0};
    TickId currentTick_{};
    std::uint16_t clientMax_{0};
    bool mapActive_{false};
    bool frameStarted_{false};

    bool isValidSlot(std::uint16_t slot) const noexcept;
    static Generation nextGeneration(Generation previous) noexcept;
    LifecycleResult event(
        LifecycleEventKind kind,
        PlayerId player = PlayerId::invalid()) noexcept;
    void clearMapState() noexcept;
};

} // namespace astrabot::host
