// SPDX-License-Identifier: MPL-2.0
#include "core/visual_memory.hpp"

namespace astrabot::core::world {
namespace {
bool valid(PlayerId p) noexcept { return p.isValid() && p.slot <= perception::kPlayerCapacity; }
}
void VisualMemoryModel::reset() noexcept {
    states_ = {}; frame_ = {}; generations_ = {}; diagnostics_ = {}; ready_ = false;
}
void VisualMemoryModel::reject(MemoryReason reason) noexcept {
    diagnostics_.reason = reason; ++diagnostics_.rejected;
}
void VisualMemoryModel::beginRound(perception::RoundGeneration round) noexcept {
    if (!round.isValid() || round.value <= frame_.round.value) return;
    for (const auto& state : states_) diagnostics_.retired += state.snapshot.count;
    states_ = {}; ready_ = false; frame_.round = round;
    diagnostics_.reason = MemoryReason::RoundChanged;
}
void VisualMemoryModel::invalidate(MemoryReason reason) noexcept {
    for (const auto& state : states_) diagnostics_.retired += state.snapshot.count;
    states_ = {}; ready_ = false; reject(reason);
}
void VisualMemoryModel::forget(PlayerId player) noexcept {
    if (!valid(player)) return;
    auto& input = frame_.players[player.slot-1U];
    if (input.player == player) input.eligible = false;
    for (auto& state : states_) {
        auto& snapshot = state.snapshot;
        if (snapshot.stamp.observer == player) {
            diagnostics_.retired += snapshot.count; state = {}; continue;
        }
        std::size_t kept = 0;
        for (std::size_t i=0; i<snapshot.count; ++i) {
            if (snapshot.memories[i].target == player) ++diagnostics_.retired;
            else snapshot.memories[kept++] = snapshot.memories[i];
        }
        for (std::size_t i=kept; i<snapshot.count; ++i) snapshot.memories[i] = {};
        snapshot.count = kept;
    }
}
const MemorySnapshot* VisualMemoryModel::latest(PlayerId observer) const noexcept {
    if (!ready_ || !valid(observer)) return nullptr;
    const auto& snapshot = states_[observer.slot-1U].snapshot;
    return snapshot.stamp.observer == observer ? &snapshot : nullptr;
}
bool VisualMemoryModel::advance(const MemoryFrame& input) noexcept {
    diagnostics_.frameVisits = 0; diagnostics_.frameObservations = 0;
    diagnostics_.reason = MemoryReason::None;
    if (!settings_.valid()) { invalidate(MemoryReason::InvalidSettings); return false; }
    if (!input.map.isValid() || !input.tick.isValid() || !input.round.isValid() ||
        (input.map == frame_.map && input.round.value < frame_.round.value) ||
        (frame_.map.isValid() && (input.map.value < frame_.map.value ||
         (input.map == frame_.map && (input.tick.value <= frame_.tick.value ||
                                      input.timeMicros < frame_.timeMicros))))) {
        invalidate(MemoryReason::InvalidFrame); return false;
    }
    if (input.map != frame_.map) {
        for (const auto& state : states_) diagnostics_.retired += state.snapshot.count;
        states_ = {}; generations_ = {};
    }
    else beginRound(input.round);
    frame_ = input; ready_ = true; ++diagnostics_.frames;
    for (std::size_t i=0; i<frame_.players.size(); ++i) {
        auto& p = frame_.players[i];
        if (!p.player.isValid() && !p.eligible) { p = {}; continue; }
        if (!valid(p.player) || p.player.slot != i+1 || p.player.generation < generations_[i]) {
            p = {}; reject(MemoryReason::StaleIdentity);
        } else generations_[i] = p.player.generation;
    }
    for (std::size_t slot=0; slot<states_.size(); ++slot) {
        auto& state = states_[slot];
        auto& snapshot = state.snapshot;
        const auto& owner = frame_.players[slot];
        if (!owner.eligible || !owner.agent.isValid() ||
            snapshot.stamp.observer != owner.player || snapshot.stamp.agent != owner.agent) {
            diagnostics_.retired += snapshot.count; state = {};
        }
        if (!owner.eligible || !owner.agent.isValid()) continue;
        snapshot.stamp = {owner.agent,owner.player,frame_.map,frame_.tick,frame_.timeMicros,frame_.round};
        std::size_t kept = 0;
        for (std::size_t i=0; i<snapshot.count; ++i) {
            auto memory = snapshot.memories[i]; ++diagnostics_.frameVisits;
            const auto& target = frame_.players[memory.target.slot-1U];
            if (!target.eligible || target.player != memory.target) { ++diagnostics_.retired; continue; }
            const auto age = frame_.timeMicros-memory.lastSeenMicros;
            if (age >= settings_.retentionMicros) { ++diagnostics_.expired; continue; }
            memory.confidence = 1.0-static_cast<double>(age)/static_cast<double>(settings_.retentionMicros);
            snapshot.memories[kept++] = memory;
        }
        for (std::size_t i=kept; i<snapshot.count; ++i) snapshot.memories[i] = {};
        snapshot.count = kept;
    }
    return true;
}
bool VisualMemoryModel::observe(const perception::ObservationBatch& batch) noexcept {
    const auto& stamp = batch.stamp;
    if (!ready_ || !valid(stamp.observer) || batch.count > batch.observations.size()) {
        reject(MemoryReason::InvalidBatch); return false;
    }
    auto& state = states_[stamp.observer.slot-1U];
    auto& snapshot = state.snapshot;
    if (snapshot.stamp.observer != stamp.observer || snapshot.stamp.agent != stamp.agent || stamp.map != frame_.map || stamp.round != frame_.round) {
        reject(MemoryReason::StaleIdentity); return false;
    }
    if (stamp.tick == state.consumed) { reject(MemoryReason::DuplicateBatch); return false; }
    if (stamp.tick != frame_.tick || stamp.timeMicros != frame_.timeMicros) {
        reject(MemoryReason::StaleBatch); return false;
    }
    const auto& identity = batch.identity;
    if (!identity.validAt(frame_.timeMicros) || identity.map != frame_.map || identity.round != frame_.round ||
        identity.source != perception::ObservationSource::Vision || identity.observedMicros != stamp.timeMicros ||
        identity.receivedMicros != frame_.timeMicros) {
        reject(MemoryReason::InvalidBatch); return false;
    }
    if (identity.sequence <= state.sequence) { reject(MemoryReason::StaleBatch); return false; }
    // Validate the whole batch before changing any remembered position.
    std::array<bool, perception::kPlayerCapacity> seen{};
    for (std::size_t i=0; i<batch.count; ++i) {
        const auto& observation = batch.observations[i];
        if (!valid(observation.target) || observation.target == stamp.observer || !perception::finite(observation.position)) {
            reject(MemoryReason::InvalidBatch); return false;
        }
        const auto index = observation.target.slot-1U;
        const auto& target = frame_.players[index];
        if (!target.eligible || target.player != observation.target) { reject(MemoryReason::StaleIdentity); return false; }
        if (seen[index]) { reject(MemoryReason::InvalidBatch); return false; }
        seen[index] = true;
    }
    state.consumed = stamp.tick;
    state.sequence = identity.sequence;
    for (std::size_t i=0; i<batch.count; ++i) {
        const auto& observation = batch.observations[i];
        std::size_t index = 0;
        while (index < snapshot.count && snapshot.memories[index].target != observation.target) ++index;
        if (index == snapshot.count) ++snapshot.count; // At most 31 distinct eligible non-owner slots.
        snapshot.memories[index] = {observation.target,observation.position,identity.observedMicros,1.0,identity};
        ++diagnostics_.updates; ++diagnostics_.frameObservations;
    }
    return true;
}
} // namespace astrabot::core::world
