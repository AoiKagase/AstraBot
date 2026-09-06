// SPDX-License-Identifier: MPL-2.0
#include "core/perception.hpp"
#include <cmath>
#include <limits>

namespace astrabot::core::perception {
namespace {
constexpr double pi = 3.14159265358979323846;
std::uint64_t add(std::uint64_t a, std::uint64_t b) noexcept {
    const auto max = (std::numeric_limits<std::uint64_t>::max)();
    return b > max-a ? max : a+b;
}
bool validPlayer(PlayerId p) noexcept { return p.isValid() && p.slot <= kPlayerCapacity; }
bool eligible(const PlayerSample& p, std::size_t index) noexcept {
    return validPlayer(p.player) && p.player.slot == index+1 && p.alive;
}
void count(Diagnostics& d, Reason reason) noexcept {
    if (reason == Reason::None) return;
    d.reason = reason;
    ++d.reasons[static_cast<std::size_t>(reason)];
}
Reason geometry(Point eye, Point forward, Point end, VisionSettings settings) noexcept {
    if (!finite(eye) || !finite(forward) || !finite(end)) return Reason::InvalidGeometry;
    const double length = std::hypot(forward.x, forward.y, forward.z);
    const Point delta{end.x-eye.x, end.y-eye.y, end.z-eye.z};
    const double distance = std::hypot(delta.x, delta.y, delta.z);
    if (!std::isfinite(length) || length <= 0 || !std::isfinite(distance) || distance <= 0)
        return Reason::InvalidGeometry;
    if (distance > settings.maxDistance) return Reason::OutOfRange;
    const double dot = (delta.x/distance)*(forward.x/length) +
        (delta.y/distance)*(forward.y/length) + (delta.z/distance)*(forward.z/length);
    return dot + 1e-12 >= std::cos(settings.fullFovDegrees*pi/360)
        ? Reason::None : Reason::OutsideFov;
}
}
bool finite(Point p) noexcept {
    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}
bool VisionSettings::valid() const noexcept {
    return intervalMicros > 0 && std::isfinite(maxDistance) && maxDistance > 0 &&
        std::isfinite(fullFovDegrees) && fullFovDegrees > 0 && fullFovDegrees <= 180;
}
void Vision::reset() noexcept {
    states_ = {}; generations_ = {}; map_ = {}; tick_ = {}; timeMicros_ = 0; cursor_ = 0;
    frameUpdates_ = 0; frameReason_ = Reason::None; ++revision_;
}
void Vision::forget(PlayerId player) noexcept {
    ++revision_; // Cancels synchronous in-flight publication, too.
    for (auto& state : states_) {
        if (state.observer == player) { state = {}; continue; }
        auto& batch = state.batch;
        std::size_t kept = 0;
        for (std::size_t i=0; i<batch.count; ++i)
            if (batch.observations[i].target != player)
                batch.observations[kept++] = batch.observations[i];
        for (std::size_t i=kept; i<batch.count; ++i) batch.observations[i] = {};
        batch.count = kept;
    }
}
const ObservationBatch* Vision::latest(PlayerId player) const noexcept {
    if (!validPlayer(player)) return nullptr;
    const auto& state = states_[player.slot-1U];
    return state.observer == player && state.published ? &state.batch : nullptr;
}
const Diagnostics* Vision::diagnostics(PlayerId player) const noexcept {
    if (!validPlayer(player)) return nullptr;
    const auto& state = states_[player.slot-1U];
    return state.observer == player ? &state.diagnostics : nullptr;
}
void Vision::update(const InputFrame& input, IVisibilityQueries& queries) noexcept {
    InputFrame frame = input;
    frameUpdates_ = 0; frameReason_ = Reason::None;
    if (!settings_.valid()) { reset(); frameReason_ = Reason::InvalidSettings; return; }
    if (!frame.map.isValid() || !frame.tick.isValid() ||
        (map_.isValid() && (frame.map.value < map_.value ||
         (frame.map == map_ && (frame.tick.value <= tick_.value || frame.timeMicros < timeMicros_))))) {
        frameReason_ = Reason::InvalidFrame;
        return; // Replayed input cannot overwrite a newer batch.
    }
    if (frame.map != map_) reset();
    map_ = frame.map; tick_ = frame.tick; timeMicros_ = frame.timeMicros;
    // A newer tick cannot make a retired slot generation current again.
    // Keep high-water generations through forget/death until map retirement.
    for (std::size_t i=0; i<frame.players.size(); ++i) {
        auto& sample = frame.players[i];
        if (!sample.player.isValid() && !sample.alive) continue;
        if (!validPlayer(sample.player) || sample.player.slot != i+1 ||
            sample.player.generation.value < generations_[i].value) {
            sample = {}; frameReason_ = Reason::StaleIdentity;
        } else generations_[i] = sample.player.generation;
    }
    // Retirement runs every frame, independently of the visibility budget.
    for (auto& state : states_) {
        if (!validPlayer(state.observer)) continue;
        const auto& p = frame.players[state.observer.slot-1U];
        if (!eligible(p, state.observer.slot-1U) || p.player != state.observer || p.agent != state.agent) {
            state = {}; continue;
        }
        auto& batch = state.batch;
        std::size_t kept = 0;
        for (std::size_t i=0; i<batch.count; ++i) {
            const auto target = batch.observations[i].target;
            const auto& candidate = frame.players[target.slot-1U];
            if (eligible(candidate, target.slot-1U) && candidate.player == target)
                batch.observations[kept++] = batch.observations[i];
        }
        for (std::size_t i=kept; i<batch.count; ++i) batch.observations[i] = {};
        batch.count = kept;
    }
    for (std::size_t i=0; i<states_.size(); ++i) {
        const auto& p = frame.players[i];
        if (!eligible(p, i) || !p.agent.isValid()) continue;
        auto& state = states_[i];
        if (state.observer != p.player || state.agent != p.agent) {
            state = {}; state.observer = p.player; state.agent = p.agent;
            const auto phase = (settings_.intervalMicros/16)*(p.agent.value%16);
            state.nextMicros = add(frame.timeMicros, phase);
        }
    }
    const auto start = cursor_;
    for (std::size_t offset=0; offset<states_.size(); ++offset) {
        const auto index = (start+offset)%states_.size();
        auto& state = states_[index];
        if (!state.observer.isValid() || frame.timeMicros < state.nextMicros) continue;
        if (frameUpdates_ == kBotsPerFrame) { ++state.diagnostics.deferredFrames; continue; }
        const auto& observer = frame.players[index];
        ObservationBatch batch{};
        batch.stamp = {observer.agent, observer.player, frame.map, frame.tick, frame.timeMicros};
        Diagnostics d{};
        count(d,frameReason_);
        d.latenessMicros = frame.timeMicros-state.nextMicros;
        d.intervalMicros = state.published ? frame.timeMicros-state.batch.stamp.timeMicros : 0;
        d.updates = state.diagnostics.updates+1;
        d.deferredFrames = state.diagnostics.deferredFrames;
        const auto revision = revision_;
        for (std::size_t j=0; j<frame.players.size(); ++j) {
            const auto& target = frame.players[j];
            if (j == index || !target.alive) continue;
            if (!eligible(target,j)) { count(d,Reason::StaleIdentity); continue; }
            ++d.candidates;
            for (const auto point : {target.eye, target.center}) {
                auto reason = geometry(observer.eye,observer.forward,point,settings_);
                if (reason == Reason::None) {
                    ++d.traces;
                    reason = queries.trace({batch.stamp,target.player,observer.eye,point});
                    if (revision_ != revision) return;
                    if (reason >= Reason::Count) reason = Reason::InvalidTrace;
                }
                if (reason == Reason::None) {
                    batch.observations[batch.count++] = {target.player,point};
                    break;
                }
                count(d,reason);
                if (reason == Reason::StaleIdentity || reason == Reason::MissingEngine) break;
            }
        }
        state.batch = batch; state.diagnostics = d; state.published = true;
        state.nextMicros = add(frame.timeMicros,settings_.intervalMicros);
        ++frameUpdates_; cursor_ = (index+1)%states_.size();
    }
}
} // namespace astrabot::core::perception
