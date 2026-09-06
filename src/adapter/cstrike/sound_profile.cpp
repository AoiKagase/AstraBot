// SPDX-License-Identifier: MPL-2.0
#include "adapter/cstrike/sound_profile.hpp"
#include <cmath>
#include <cstring>
#include <limits>

namespace astrabot::adapter::cstrike {
namespace {
namespace p = core::perception;
bool bounded(const char* name,std::size_t capacity) noexcept {
    if (!name) return false;
    for (std::size_t i=0; i<capacity; ++i) if (name[i] == '\0') return i != 0;
    return false;
}
EventKind eventKind(const char* name) noexcept {
    // Source-backed precache names, never hard-coded engine event numbers.
    static constexpr const char* gunshots[]{
        "events/ak47.sc","events/aug.sc","events/awp.sc","events/deagle.sc",
        "events/elite_left.sc","events/elite_right.sc","events/famas.sc","events/fiveseven.sc",
        "events/g3sg1.sc","events/galil.sc","events/glock18.sc","events/m249.sc",
        "events/m3.sc","events/m4a1.sc","events/mac10.sc","events/mp5n.sc","events/p228.sc",
        "events/p90.sc","events/scout.sc","events/sg550.sc","events/sg552.sc","events/tmp.sc",
        "events/ump45.sc","events/usp.sc","events/xm1014.sc"
    };
    for (const auto* candidate : gunshots) if (std::strcmp(name,candidate) == 0) return EventKind::Gunshot;
    if (std::strcmp(name,"events/createsmoke.sc") == 0) return EventKind::Smoke; // Mapping only; effects are P4-05.
    return EventKind::Unknown;
}
}
core::perception::SoundKind soundSample(const char* name) noexcept {
    if (!bounded(name,96)) return p::SoundKind::Unknown;
    static constexpr const char* prefixes[]{"player/pl_step","player/pl_metal","player/pl_dirt","player/pl_duct",
        "player/pl_grate","player/pl_tile","player/pl_slosh","player/pl_wade","player/pl_ladder","player/pl_snow"};
    for (const auto* prefix : prefixes) {
        const auto length = std::strlen(prefix);
        if (std::strncmp(name,prefix,length) != 0) continue;
        const char last = std::strcmp(prefix,"player/pl_tile") == 0 ? '5':'4';
        if (name[length] >= '1' && name[length] <= last && std::strcmp(name+length+1,".wav") == 0)
            return p::SoundKind::Footstep;
    }
    for (const auto* sample : {"weapons/c4_explode1.wav","weapons/flashbang-1.wav","weapons/flashbang-2.wav","weapons/sg_explode.wav"})
        if (std::strcmp(name,sample) == 0) return p::SoundKind::Explosion;
    return p::SoundKind::Unknown;
}
bool quantizeSound(p::Point point,p::SoundRegion& region) noexcept {
    region = {};
    if (!p::finite(point)) return false;
    const double x = std::floor(point.x/p::SoundRegion::width), y = std::floor(point.y/p::SoundRegion::width), z = std::floor(point.z/p::SoundRegion::width);
    constexpr double low = static_cast<double>((std::numeric_limits<std::int32_t>::min)());
    constexpr double high = static_cast<double>((std::numeric_limits<std::int32_t>::max)());
    if (x < low || x > high || y < low || y > high || z < low || z > high) return false;
    region = {static_cast<std::int32_t>(x),static_cast<std::int32_t>(y),static_cast<std::int32_t>(z)}; return true;
}
bool soundAudible(p::Point source,p::Point listener,double volume,double attenuation) noexcept {
    if (!p::finite(source) || !p::finite(listener) || !std::isfinite(volume) || volume <= 0 || volume > 1 ||
        !std::isfinite(attenuation) || attenuation < 0) return false;
    // Deliberate bounded approximation, not GoldSrc's PAS/acoustic simulation.
    constexpr double base = 1024, maximum = 8192;
    const double range = attenuation <= base*volume/maximum ? maximum : base*volume/attenuation;
    const double distance = std::hypot(source.x-listener.x,source.y-listener.y,source.z-listener.z);
    return std::isfinite(distance) && distance <= range;
}
EventBinding EventCatalog::bind(std::uint16_t index,const char* name) noexcept {
    if (index == 0) return EventBinding::Invalid;
    Entry* prior = nullptr;
    for (std::size_t i=0; i<count_; ++i) if (entries_[i].index == index) { prior = &entries_[i]; break; }
    if (!bounded(name,64)) {
        if (prior) { prior->kind = EventKind::Unknown; prior->conflict = true; }
        return EventBinding::Invalid;
    }
    if (prior) {
        if (prior->conflict || std::strcmp(prior->name.data(),name) != 0) {
            prior->kind = EventKind::Unknown; prior->conflict = true; return EventBinding::Conflict;
        }
        return EventBinding::Duplicate;
    }
    if (count_ == entries_.size()) return EventBinding::Full;
    auto& entry = entries_[count_++]; entry.index = index; entry.kind = eventKind(name);
    std::memcpy(entry.name.data(),name,std::strlen(name)+1);
    return entry.kind == EventKind::Unknown ? EventBinding::Unsupported : EventBinding::Registered;
}
EventKind EventCatalog::find(std::uint16_t index) const noexcept {
    if (index == 0) return EventKind::Unknown;
    for (std::size_t i=0; i<count_; ++i) if (entries_[i].index == index) return entries_[i].kind;
    return EventKind::Unknown;
}
} // namespace astrabot::adapter::cstrike
