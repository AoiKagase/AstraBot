// SPDX-License-Identifier: MPL-2.0
#include "adapter/cstrike/visual_effects.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace astrabot::adapter::cstrike {
namespace p = core::perception;
namespace {
bool same(p::Point a,p::Point b) noexcept { return a.x == b.x && a.y == b.y && a.z == b.z; }
bool intersects(p::Point a,p::Point b,p::Point center,double radius) noexcept {
    // Scale before projection: finite double endpoints need not have finite differences.
    const double scale = (std::max)({1.0,std::abs(a.x),std::abs(a.y),std::abs(a.z),
        std::abs(b.x),std::abs(b.y),std::abs(b.z),std::abs(center.x),std::abs(center.y),std::abs(center.z),radius});
    const p::Point d{b.x/scale-a.x/scale,b.y/scale-a.y/scale,b.z/scale-a.z/scale};
    const p::Point c{center.x/scale-a.x/scale,center.y/scale-a.y/scale,center.z/scale-a.z/scale};
    const double length = d.x*d.x+d.y*d.y+d.z*d.z;
    const double t = length == 0 ? 0 : std::clamp((c.x*d.x+c.y*d.y+c.z*d.z)/length,0.0,1.0);
    return std::hypot(c.x-t*d.x,c.y-t*d.y,c.z-t*d.z) <= radius/scale;
}
}
bool VisualEffectSettings::valid() const noexcept { return std::isfinite(smokeRadius) && smokeRadius > 0 && smokeMicros != 0; }
void VisualEffects::clear() noexcept { smoke_ = {}; flash_ = {}; overflowUntil_ = 0; diagnostics_.regions = 0; diagnostics_.overflowActive = false; ++revision_; }
void VisualEffects::reset() noexcept { clear(); map_ = {}; round_ = {}; time_ = 0; ready_ = false; diagnostics_ = {}; }
bool VisualEffects::advance(core::MapGeneration map,p::RoundGeneration round,std::uint64_t now) noexcept {
    if (!settings_.valid() || !map.isValid() || !round.isValid() ||
        (map_.isValid() && (map.value < map_.value || (map == map_ && (round.value < round_.value || now < time_))))) {
        invalidate(); return false;
    }
    if (map != map_ || round != round_) clear();
    map_ = map; round_ = round; time_ = now; ready_ = true; diagnostics_.clockInvalid = false;
    for (auto& region : smoke_) if (region.end && now >= region.end) { region = {}; ++diagnostics_.expired; ++revision_; }
    for (auto& effect : flash_) if (effect.end && now >= effect.end) { effect = {}; ++diagnostics_.expired; ++revision_; }
    if (overflowUntil_ && now >= overflowUntil_) { overflowUntil_ = 0; ++revision_; }
    diagnostics_.regions = 0;
    for (const auto& region : smoke_) if (region.end) ++diagnostics_.regions;
    diagnostics_.overflowActive = overflowUntil_ != 0;
    return true;
}
bool VisualEffects::smoke(p::Point center) noexcept {
    if (!ready_ || !p::finite(center) || time_ > (std::numeric_limits<std::uint64_t>::max)()-settings_.smokeMicros) {
        ++diagnostics_.rejected; return false;
    }
    for (const auto& region : smoke_) if (region.end && region.start == time_ && same(region.center,center)) {
        ++diagnostics_.duplicates; return false;
    }
    const auto end = time_+settings_.smokeMicros;
    for (auto& region : smoke_) if (!region.end) {
        region = {center,time_,end}; ++revision_; ++diagnostics_.smokeAccepted; ++diagnostics_.regions; return true;
    }
    overflowUntil_ = (std::max)(overflowUntil_,end); ++revision_; ++diagnostics_.overflow;
    diagnostics_.overflowActive = true; return false;
}
bool VisualEffects::flash(core::PlayerId player,std::uint64_t duration) noexcept {
    if (!ready_ || !player.isValid() || player.slot > flash_.size() || duration == 0 ||
        time_ > (std::numeric_limits<std::uint64_t>::max)()-duration) { ++diagnostics_.rejected; return false; }
    auto& effect = flash_[player.slot-1U];
    if (effect.player == player && effect.start == time_ && effect.end == time_+duration) { ++diagnostics_.duplicates; return false; }
    const auto end = effect.player == player ? (std::max)(effect.end,time_+duration) : time_+duration;
    effect = {player,time_,end}; ++revision_; ++diagnostics_.flashAccepted; return true;
}
void VisualEffects::forget(core::PlayerId player) noexcept {
    if (player.isValid() && player.slot <= flash_.size() && flash_[player.slot-1U].player == player) {
        flash_[player.slot-1U] = {}; ++revision_;
    }
}
p::Reason VisualEffects::blocked(core::PlayerId player,p::Point start,p::Point end) const noexcept {
    if (!ready_) return p::Reason::InvalidFrame;
    if (!p::finite(start) || !p::finite(end)) return p::Reason::InvalidGeometry;
    if (overflowUntil_) return p::Reason::EffectsOverflow;
    if (player.isValid() && player.slot <= flash_.size()) {
        const auto& effect = flash_[player.slot-1U];
        if (effect.player == player && effect.end > time_) return p::Reason::FlashBlind;
    }
    for (const auto& region : smoke_) if (region.end && intersects(start,end,region.center,settings_.smokeRadius)) return p::Reason::SmokeOccluded;
    return p::Reason::None;
}
}
