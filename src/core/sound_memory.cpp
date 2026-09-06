// SPDX-License-Identifier: MPL-2.0
#include "core/sound_memory.hpp"

namespace astrabot::core::world {
namespace {
bool valid(PlayerId p) noexcept { return p.isValid() && p.slot <= perception::kPlayerCapacity; }
bool known(perception::SoundKind kind) noexcept {
    return kind == perception::SoundKind::Footstep || kind == perception::SoundKind::Gunshot ||
        kind == perception::SoundKind::Explosion;
}
}
void SoundMemoryModel::reset() noexcept {
    frame_ = {}; states_ = {}; generations_ = {}; diagnostics_ = {}; ready_ = false;
}
bool SoundMemoryModel::reject(SoundReason reason) noexcept {
    diagnostics_.reason = reason; ++diagnostics_.rejected; return false;
}
void SoundMemoryModel::clear() noexcept {
    for (auto& state : states_) {
        diagnostics_.retired += state.snapshot.count;
        state.snapshot = {};
        if (state.observedMicros < frame_.timeMicros) state.observedMicros = frame_.timeMicros;
    }
    ready_ = false;
}
void SoundMemoryModel::invalidate(SoundReason reason) noexcept { clear(); (void)reject(reason); }
void SoundMemoryModel::beginRound(perception::RoundGeneration round) noexcept {
    if (!round.isValid() || round.value <= frame_.round.value) return;
    clear(); frame_.round = round; diagnostics_.reason = SoundReason::RoundChanged;
}
void SoundMemoryModel::forget(PlayerId observer) noexcept {
    if (!valid(observer)) return;
    auto& player = frame_.players[observer.slot-1U];
    if (player.player == observer) player.eligible = false;
    auto& state = states_[observer.slot-1U];
    if (state.snapshot.stamp.observer != observer) return;
    diagnostics_.retired += state.snapshot.count; state.snapshot = {};
    if (state.observedMicros < frame_.timeMicros) state.observedMicros = frame_.timeMicros;
}
const SoundSnapshot* SoundMemoryModel::latest(PlayerId observer) const noexcept {
    if (!ready_ || !valid(observer)) return nullptr;
    const auto& snapshot = states_[observer.slot-1U].snapshot;
    return snapshot.stamp.observer == observer ? &snapshot : nullptr;
}
bool SoundMemoryModel::advance(const MemoryFrame& input) noexcept {
    diagnostics_.frameVisits = 0; diagnostics_.frameObservations = 0; diagnostics_.reason = SoundReason::None;
    if (!settings_.valid()) { invalidate(SoundReason::InvalidSettings); return false; }
    if (!input.map.isValid() || !input.round.isValid() || !input.tick.isValid() ||
        (frame_.map.isValid() && (input.map.value < frame_.map.value ||
         (input.map == frame_.map && (input.round.value < frame_.round.value ||
          input.tick.value <= frame_.tick.value || input.timeMicros < frame_.timeMicros))))) {
        invalidate(SoundReason::InvalidFrame); return false;
    }
    if (input.map != frame_.map) { clear(); states_ = {}; generations_ = {}; }
    else beginRound(input.round);
    frame_ = input; ready_ = true;
    for (std::size_t i=0; i<frame_.players.size(); ++i) {
        auto& player = frame_.players[i];
        if (!player.player.isValid() && !player.eligible) { player = {}; continue; }
        if (!valid(player.player) || player.player.slot != i+1 || player.player.generation < generations_[i]) {
            player = {}; (void)reject(SoundReason::StaleIdentity);
        } else generations_[i] = player.player.generation;
    }
    for (std::size_t i=0; i<states_.size(); ++i) {
        auto& state = states_[i]; auto& snapshot = state.snapshot; const auto& player = frame_.players[i];
        if (!player.eligible || !player.agent.isValid() || snapshot.stamp.observer != player.player || snapshot.stamp.agent != player.agent) {
            diagnostics_.retired += snapshot.count; snapshot = {};
        }
        if (!player.eligible || !player.agent.isValid()) continue;
        snapshot.stamp = {player.agent,player.player,frame_.map,frame_.tick,frame_.timeMicros,frame_.round};
        std::size_t kept = 0;
        for (std::size_t j=0; j<snapshot.count; ++j) {
            auto sound = snapshot.sounds[j]; ++diagnostics_.frameVisits;
            const auto age = frame_.timeMicros-sound.observation.identity.observedMicros;
            if (age >= settings_.retentionMicros) { ++diagnostics_.expired; continue; }
            sound.confidence = 0.5*(1.0-static_cast<double>(age)/static_cast<double>(settings_.retentionMicros));
            snapshot.sounds[kept++] = sound;
        }
        for (std::size_t j=kept; j<snapshot.count; ++j) snapshot.sounds[j] = {};
        snapshot.count = kept;
    }
    return true;
}
bool SoundMemoryModel::observe(PlayerId observer, const perception::SoundObservation& sound) noexcept {
    if (!ready_ || !valid(observer)) return reject(SoundReason::StaleIdentity);
    auto& state = states_[observer.slot-1U]; auto& snapshot = state.snapshot;
    if (snapshot.stamp.observer != observer) return reject(SoundReason::StaleIdentity);
    const auto& id = sound.identity;
    if (!id.validAt(frame_.timeMicros) || id.source != perception::ObservationSource::Sound ||
        id.receivedMicros != frame_.timeMicros || !known(sound.kind)) return reject(SoundReason::InvalidObservation);
    if (id.map != frame_.map || id.round != frame_.round) return reject(SoundReason::StaleIdentity);
    if (id.sequence == state.sequence) return reject(SoundReason::DuplicateObservation);
    if (id.sequence < state.sequence || id.observedMicros < state.observedMicros) return reject(SoundReason::StaleObservation);
    const auto age = frame_.timeMicros-id.observedMicros;
    if (age >= settings_.retentionMicros) return reject(SoundReason::ExpiredObservation);
    state.sequence = id.sequence; state.observedMicros = id.observedMicros;
    if (snapshot.count == snapshot.sounds.size()) {
        for (std::size_t i=1; i<snapshot.count; ++i) snapshot.sounds[i-1] = snapshot.sounds[i];
        --snapshot.count; ++diagnostics_.evicted;
    }
    snapshot.sounds[snapshot.count++] = {sound,0.5*(1.0-static_cast<double>(age)/static_cast<double>(settings_.retentionMicros))};
    ++diagnostics_.updates; ++diagnostics_.frameObservations; diagnostics_.reason = SoundReason::None; return true;
}
} // namespace astrabot::core::world
