// SPDX-License-Identifier: MPL-2.0
#include "adapter/cstrike/sound.hpp"
#include "adapter/metamod/lifecycle.hpp"
#include <event_flags.h>
#include <cmath>
#include <cstring>
#include <limits>

namespace astrabot::adapter::cstrike {
namespace {
namespace p = core::perception;
bool entityValid(const edict_t* entity) noexcept { return entity && !entity->free; }
p::Point position(const Vector& value) noexcept { return {value.x,value.y,value.z}; }
bool same(p::Point a,p::Point b) noexcept { return a.x == b.x && a.y == b.y && a.z == b.z; }
}
void SoundAdapter::clearQueue() noexcept { head_ = count_ = 0; diagnostics_.queued = 0; }
void SoundAdapter::reset() noexcept {
    ++revision_; clearQueue(); memory_.reset(); events_.reset(); listeners_ = {}; epochs_ = {};
    map_ = {}; round_ = {1}; timeHighWater_ = 0; keyCount_ = keyCursor_ = 0; diagnostics_ = {};
    // Preserve sequence even across transient resets in the same process.
}
void SoundAdapter::beginMap(core::MapGeneration map) noexcept {
    if (!map.isValid() || map == map_ || (map_.isValid() && map.value < map_.value)) return;
    if (map_.isValid()) events_.reset(); // No ServerDeactivate/precache cycle was observed.
    ++revision_; diagnostics_.retired += count_; clearQueue(); memory_.reset(); listeners_ = {}; epochs_ = {};
    map_ = map; round_ = {1}; timeHighWater_ = 0; keyCount_ = keyCursor_ = 0;
}
void SoundAdapter::beginRound(p::RoundGeneration round) noexcept {
    if (!round.isValid() || round.value <= round_.value) return;
    ++revision_; round_ = round; diagnostics_.retired += count_; clearQueue(); memory_.beginRound(round);
    keyCount_ = keyCursor_ = 0;
}
void SoundAdapter::forget(core::PlayerId player) noexcept {
    if (!player.isValid() || player.slot > listeners_.size()) return;
    ++revision_; memory_.forget(player);
    auto& listener = listeners_[player.slot-1U];
    if (listener.player == player) { listener = {}; ++epochs_[player.slot-1U]; }
}
void SoundAdapter::precache(int type,const char* name,std::uint16_t index) noexcept {
    if (type != 1) { ++diagnostics_.eventRejected; return; }
    const auto result = events_.bind(index,name);
    if (result == EventBinding::Registered) ++diagnostics_.eventBindings;
    else if (result != EventBinding::Duplicate) ++diagnostics_.eventRejected;
}
bool SoundAdapter::timestamp(float time,std::uint64_t& micros) noexcept {
    const double value = static_cast<double>(time)*1000000;
    if (!std::isfinite(value) || value < 0 || value >= 18446744073709551616.0 || value < static_cast<double>(timeHighWater_)) {
        ++revision_; ++diagnostics_.invalid; diagnostics_.retired += count_; clearQueue();
        memory_.invalidate(core::world::SoundReason::InvalidFrame); return false;
    }
    micros = static_cast<std::uint64_t>(value); timeHighWater_ = micros; return true;
}
bool SoundAdapter::synchronize(metamod::LifecycleCoordinator& owner) noexcept {
    if (!owner.registry().isMapActive()) return false;
    beginMap(owner.registry().mapGeneration()); beginRound(owner.round());
    const auto revision = revision_;
    std::array<Listener,p::kPlayerCapacity> current{};
    for (std::uint16_t slot=1; slot<=owner.registry().clientMax(); ++slot) {
        const auto player = owner.registry().currentPlayer(slot);
        const auto binding = owner.agents().findByPlayer(player);
        if (!binding.isValid() || owner.removalPending(player)) continue;
        const auto* join = owner.joinState(player); auto* entity = owner.entityFor(player);
        if (revision != revision_ || !owner.registry().isMapActive()) return false;
        const auto* team = owner.teams().find(player);
        if (!join || join->phase() != JoinPhase::Joined || !entityValid(entity) ||
            entity->v.deadflag != DEAD_NO || !std::isfinite(entity->v.health) || entity->v.health <= 0 ||
            entity->v.iuser1 != 0 || (entity->v.flags & FL_SPECTATOR) != 0 || (team && team->team == p::Team::Spectator)) continue;
        const auto point = position(entity->v.origin);
        if (!p::finite(point)) continue;
        current[slot-1U] = {player,binding.agent,point,0};
    }
    for (std::size_t i=0; i<current.size(); ++i) {
        if (current[i].player != listeners_[i].player || current[i].agent != listeners_[i].agent) {
            memory_.forget(listeners_[i].player); ++epochs_[i];
        }
        current[i].epoch = epochs_[i];
    }
    listeners_ = current; return true;
}
void SoundAdapter::capture(metamod::LifecycleCoordinator& owner,float time,Key key,p::SoundKind kind) noexcept {
    if (capturing_) { ++diagnostics_.reentrant; return; }
    capturing_ = true;
    struct Guard { bool& active; ~Guard() { active = false; } } guard{capturing_};
    if (!synchronize(owner)) { ++diagnostics_.invalid; return; }
    if (!timestamp(time,key.time)) return;
    p::SoundRegion region{};
    if (!quantizeSound(key.source,region) || !std::isfinite(key.volume) || key.volume <= 0 || key.volume > 1 ||
        !std::isfinite(key.attenuation) || key.attenuation < 0) { ++diagnostics_.invalid; return; }
    for (std::size_t i=0; i<keyCount_; ++i) {
        const auto& prior = keys_[i];
        if (key.time == prior.time && key.emitter == prior.emitter && key.serial == prior.serial && key.channel == prior.channel &&
            key.pitch == prior.pitch && key.event == prior.event && key.ambient == prior.ambient && key.sample == prior.sample &&
            same(key.source,prior.source) && key.volume == prior.volume && key.attenuation == prior.attenuation) {
            ++diagnostics_.duplicates; return;
        }
    }
    if (count_ == queue_.size()) { ++diagnostics_.overflow; return; }
    if (sequence_ == (std::numeric_limits<std::uint64_t>::max)()) { ++diagnostics_.invalid; return; }
    auto& pending = queue_[(head_+count_)%queue_.size()];
    pending = {}; pending.kind = kind; pending.source = key.source; pending.volume = key.volume; pending.attenuation = key.attenuation;
    pending.identity = {map_,round_,p::ObservationSource::Sound,++sequence_,key.time,key.time};
    pending.audience = listeners_; ++count_; ++diagnostics_.captured; diagnostics_.queued = count_;
    keys_[keyCursor_] = key; keyCursor_ = (keyCursor_+1)%keys_.size(); if (keyCount_ < keys_.size()) ++keyCount_;
}
void SoundAdapter::emit(metamod::LifecycleCoordinator& owner,float time,const edict_t* emitter,const float* origin,
    int channel,const char* sample,float volume,float attenuation,int flags,int pitch,bool ambient) noexcept {
    const auto kind = soundSample(sample);
    if (kind == p::SoundKind::Unknown) { ++diagnostics_.unknown; return; }
    if ((!ambient && !entityValid(emitter)) || (ambient && !origin) || flags != 0 || channel < 0 || channel > 7 || pitch < 1 || pitch > 255) {
        ++diagnostics_.invalid; return;
    }
    Key key{}; key.emitter = reinterpret_cast<std::uintptr_t>(emitter); key.serial = emitter ? emitter->serialnumber:0;
    key.channel = channel; key.pitch = pitch; key.ambient = ambient; key.volume = volume; key.attenuation = attenuation;
    key.source = ambient ? p::Point{origin[0],origin[1],origin[2]} : position(emitter->v.origin);
    std::memcpy(key.sample.data(),sample,std::strlen(sample)+1); // Known, length-bounded profile name.
    capture(owner,time,key,kind);
}
void SoundAdapter::playback(metamod::LifecycleCoordinator& owner,float time,int flags,const edict_t* invoker,
    std::uint16_t event,float delay,const float* origin) noexcept {
    if (events_.find(event) != EventKind::Gunshot) { ++diagnostics_.unknown; return; }
    // Standard server weapon events use zero delay and 0/FEV_NOTHOST. Local-only,
    // delayed and other event modes need separate timing/audience evidence.
    if ((flags != 0 && flags != FEV_NOTHOST) || delay != 0 || !origin || !entityValid(invoker)) { ++diagnostics_.invalid; return; }
    Key key{}; key.event = event; key.emitter = reinterpret_cast<std::uintptr_t>(invoker); key.serial = invoker->serialnumber;
    key.source = {origin[0],origin[1],origin[2]};
    if (same(key.source,{})) key.source = position(invoker->v.origin);
    key.volume = 1; key.attenuation = 0.8; key.pitch = 100;
    capture(owner,time,key,p::SoundKind::Gunshot);
}
void SoundAdapter::frame(metamod::LifecycleCoordinator& owner,float time) noexcept {
    diagnostics_.frameEvents = diagnostics_.frameAudienceChecks = 0;
    if (!synchronize(owner)) return;
    std::uint64_t now{}; if (!timestamp(time,now)) return;
    if (!owner.world_.collectingAt(map_,round_,owner.registry().currentTick(),now)) { diagnostics_.retired += count_; clearQueue(); return; }
    while (count_ != 0 && diagnostics_.frameEvents < eventsPerFrame) {
        auto& pending = queue_[head_];
        if (pending.identity.observedMicros > now) break;
        ++diagnostics_.frameEvents; ++diagnostics_.processed;
        if (pending.identity.map != map_ || pending.identity.round != round_) ++diagnostics_.retired;
        else if (now-pending.identity.observedMicros >= 3000000) ++diagnostics_.expired;
        else {
            p::SoundObservation observation{pending.identity,pending.kind,{}};
            observation.identity.receivedMicros = now;
            if (quantizeSound(pending.source,observation.region)) for (std::size_t i=0; i<listeners_.size(); ++i) {
                const auto& heard = pending.audience[i]; const auto& current = listeners_[i];
                if (!heard.player.isValid()) continue;
                ++diagnostics_.frameAudienceChecks;
                if (heard.player != current.player || heard.agent != current.agent || heard.epoch != current.epoch) {
                    ++diagnostics_.recipientRejected; continue;
                }
                if (soundAudible(pending.source,heard.position,pending.volume,pending.attenuation))
                    (void)owner.world_.stage(heard.player,observation);
            }
        }
        head_ = (head_+1)%queue_.size(); --count_;
    }
    diagnostics_.queued = count_;
}
} // namespace astrabot::adapter::cstrike
