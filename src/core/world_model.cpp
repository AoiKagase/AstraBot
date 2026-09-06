// SPDX-License-Identifier: MPL-2.0
#include "core/world_model.hpp"
#include <algorithm>
#include <cmath>
#include <tuple>

namespace astrabot::core::world {
namespace p = perception;
namespace {
auto idKey(const p::ObservationIdentity& id) noexcept { return std::make_tuple(id.map.value,id.round.value,id.source,id.sequence); }
bool sameId(const p::ObservationIdentity& a,const p::ObservationIdentity& b) noexcept {
    return idKey(a) == idKey(b) && a.observedMicros == b.observedMicros && a.receivedMicros == b.receivedMicros;
}
bool sameStamp(const p::Stamp& a,const p::Stamp& b) noexcept {
    return a.agent == b.agent && a.observer == b.observer && a.map == b.map && a.round == b.round && a.tick == b.tick && a.timeMicros == b.timeMicros;
}
}
void WorldModel::reset() noexcept {
    clearDistributions();
    beginUpdate(); visual_.reset(); sounds_.reset(); frame_ = {}; queues_ = {}; diagnostics_ = {};
}
void WorldModel::beginUpdate() noexcept {
    published_ = collecting_ = false; visionCount_ = soundCount_ = 0;
    diagnostics_.staged = diagnostics_.frameProcessed = 0; diagnostics_.reason = WorldReason::None;
}
bool WorldModel::reject(WorldReason reason) noexcept {
    diagnostics_.reason = reason; ++diagnostics_.rejected[static_cast<std::size_t>(reason)]; return false;
}
bool WorldModel::advance(const MemoryFrame& frame) noexcept {
    beginUpdate();
    const bool visual = visual_.advance(frame), sounds = sounds_.advance(frame);
    if (!visual || !sounds) {
        visual_.invalidate(MemoryReason::InvalidFrame); sounds_.invalidate(SoundReason::InvalidFrame);
        return reject(WorldReason::InvalidFrame);
    }
    frame_ = frame; collecting_ = true; return true;
}
bool WorldModel::stage(const p::ObservationBatch& batch) noexcept {
    if (!collecting_) return reject(WorldReason::NotCollecting);
    if (visionCount_ == visionInputs_.size()) return reject(WorldReason::QueueFull);
    visionInputs_[visionCount_++] = batch; ++diagnostics_.staged; return true;
}
bool WorldModel::stage(PlayerId receiverId,const p::SoundObservation& sound) noexcept {
    if (!collecting_) return reject(WorldReason::NotCollecting);
    if (soundCount_ == soundInputs_.size()) return reject(WorldReason::QueueFull);
    soundInputs_[soundCount_++] = {receiverId,sound}; ++diagnostics_.staged; return true;
}
const p::ObservationIdentity& WorldModel::identity(InputRef ref) const noexcept {
    return ref.visual ? visionInputs_[ref.index].identity : soundInputs_[ref.index].sound.identity;
}
PlayerId WorldModel::receiver(InputRef ref) const noexcept { return ref.visual ? visionInputs_[ref.index].stamp.observer : soundInputs_[ref.index].receiver; }
bool WorldModel::equal(InputRef a,InputRef b) const noexcept {
    if (a.visual != b.visual || !sameId(identity(a),identity(b))) return false;
    if (!a.visual) {
        const auto& x = soundInputs_[a.index].sound; const auto& y = soundInputs_[b.index].sound;
        return x.kind == y.kind && x.region.x == y.region.x && x.region.y == y.region.y && x.region.z == y.region.z;
    }
    const auto& x = visionInputs_[a.index]; const auto& y = visionInputs_[b.index];
    if (!sameStamp(x.stamp,y.stamp) || x.count != y.count || x.count > x.observations.size()) return false;
    for (std::size_t i=0;i<x.count;++i) {
        const auto& u = x.observations[i]; const auto& v = y.observations[i];
        if (u.target != v.target || u.position.x != v.position.x || u.position.y != v.position.y || u.position.z != v.position.z) return false;
    }
    return true;
}
bool WorldModel::publish(SourceQueueDiagnostics queues) noexcept {
    if (!collecting_) return reject(WorldReason::NotCollecting);
    std::size_t count = 0;
    for (std::size_t i=0;i<visionCount_;++i) order_[count++] = {static_cast<std::uint16_t>(i),true};
    for (std::size_t i=0;i<soundCount_;++i) order_[count++] = {static_cast<std::uint16_t>(i),false};
    const auto key = [&](InputRef ref) { const auto player = receiver(ref); return std::make_tuple(idKey(identity(ref)),player.slot,player.generation.value); };
    std::sort(order_.begin(),order_.begin()+count,[&](InputRef a,InputRef b) { return key(a) < key(b); });
    for (std::size_t i=0;i<count;) {
        const auto ref = order_[i]; std::size_t end = i+1; bool conflict = false;
        while (end<count && key(order_[end]) == key(ref)) { conflict = conflict || !equal(ref,order_[end]); ++end; }
        diagnostics_.frameProcessed += end-i;
        if (conflict) { (void)reject(WorldReason::Conflict); i = end; continue; }
        if (end-i > 1) (void)reject(WorldReason::Duplicate);
        const bool accepted = ref.visual ? visual_.observe(visionInputs_[ref.index]) : sounds_.observe(receiver(ref),soundInputs_[ref.index].sound);
        if (accepted) {
            ++diagnostics_.accepted;
            const auto& id = identity(ref);
            diagnostics_.maxDelayMicros = (std::max)(diagnostics_.maxDelayMicros,id.receivedMicros-id.observedMicros);
        } else (void)reject(ref.visual ? WorldReason::VisualRejected : WorldReason::SoundRejected);
        i = end;
    }
    for (std::size_t row=0;row<distributions_.size();++row) {
        const auto* memory=visual_.latest(frame_.players[row].player);
        for (auto& entry:distributions_[row]) {
            if(!entry.target.isValid()) continue;
            bool retained=false;
            if(memory && entry.observer==memory->stamp.observer)
                for(std::size_t i=0;i<memory->count;++i)
                    retained=retained || (entry.target==memory->memories[i].target && sameId(entry.identity,memory->memories[i].identity));
            if (!retained) entry={};
            else entry.value.delayMicros=frame_.timeMicros-entry.value.updatedMicros;
        }
    }
    queues_ = queues; collecting_ = false; published_ = true; ++diagnostics_.publications; return true;
}
void WorldModel::forget(PlayerId player) noexcept { visual_.forget(player); sounds_.forget(player); }
void WorldModel::beginRound(p::RoundGeneration round) noexcept { beginUpdate(); clearDistributions(); visual_.beginRound(round); sounds_.beginRound(round); }
void WorldModel::clearDistributions() noexcept {
    for (auto& row : distributions_) for (auto& entry : row) if(entry.target.isValid()) entry = {};
}
bool WorldModel::setDistribution(PlayerId observer,PlayerId target,const p::ObservationIdentity& id,const PositionDistribution& value) noexcept {
    if (!published_) return false;
    if (value.count>value.areas.size() || !value.navRevision || value.updatedMicros>frame_.timeMicros ||
        !std::isfinite(value.unknownMass) || value.unknownMass<0) return false;
    double total=value.unknownMass;
    for(std::size_t i=0;i<value.count;++i) {
        if(!value.areas[i].area || !std::isfinite(value.areas[i].weight) || value.areas[i].weight<0 ||
            (i && value.areas[i-1].area>=value.areas[i].area)) return false;
        total+=value.areas[i].weight;
    }
    if(value.available ? std::abs(total-1)>1e-9 : value.count!=0 || total!=0) return false;
    const auto* memory = visual_.latest(observer); if (!memory) return false;
    for (std::size_t i=0;i<memory->count;++i) if (memory->memories[i].target == target && sameId(memory->memories[i].identity,id)) {
        auto& entry=distributions_[observer.slot-1U][target.slot-1U];
        entry = {observer,target,id,value}; entry.value.delayMicros=frame_.timeMicros-value.updatedMicros; return true;
    }
    return false;
}
std::optional<WorldSnapshot> WorldModel::latest(PlayerId player) const noexcept {
    if (!published_) return std::nullopt;
    const auto* visual = visual_.latest(player); const auto* sounds = sounds_.latest(player);
    if (!visual || !sounds || !sameStamp(visual->stamp,sounds->stamp) || visual->stamp.map != frame_.map ||
        visual->stamp.round != frame_.round || visual->stamp.tick != frame_.tick || visual->stamp.timeMicros != frame_.timeMicros) return std::nullopt;
    WorldSnapshot snapshot{visual->stamp,visual,sounds,0,0,0,queues_};
    for (std::size_t i=0;i<visual->count;++i) {
        const auto& memory = visual->memories[i];
        const auto& entry = distributions_[player.slot-1U][memory.target.slot-1U];
        if (entry.observer == player && entry.target == memory.target && sameId(entry.identity,memory.identity)) snapshot.distributions[i] = &entry.value;
    }
    for (std::size_t i=0;i<visual->count;++i) snapshot.oldestVisualAgeMicros = (std::max)(snapshot.oldestVisualAgeMicros,frame_.timeMicros-visual->memories[i].lastSeenMicros);
    for (std::size_t i=0;i<sounds->count;++i) {
        const auto& id = sounds->sounds[i].observation.identity;
        snapshot.oldestSoundAgeMicros = (std::max)(snapshot.oldestSoundAgeMicros,frame_.timeMicros-id.observedMicros);
        snapshot.maxReceiptDelayMicros = (std::max)(snapshot.maxReceiptDelayMicros,id.receivedMicros-id.observedMicros);
    }
    return snapshot;
}
}
