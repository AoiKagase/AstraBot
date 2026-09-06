// SPDX-License-Identifier: MPL-2.0
#include "adapter/cstrike/sound_profile.hpp"
#include <cassert>
#include <limits>
namespace a = astrabot::adapter::cstrike;
namespace p = astrabot::core::perception;
int main() {
    assert(a::soundSample("player/pl_step1.wav") == p::SoundKind::Footstep);
    assert(a::soundSample("player/pl_tile5.wav") == p::SoundKind::Footstep);
    assert(a::soundSample("player/pl_metal5.wav") == p::SoundKind::Unknown);
    assert(a::soundSample("weapons/c4_explode1.wav") == p::SoundKind::Explosion);
    assert(a::soundSample("weapons/unknown.wav") == p::SoundKind::Unknown && a::soundSample(nullptr) == p::SoundKind::Unknown);
    p::SoundRegion region{};
    assert(a::quantizeSound({-0.1,255.99,-256},region) && region.x == -1 && region.y == 0 && region.z == -1);
    assert(a::quantizeSound({256,-256.1,0},region) && region.x == 1 && region.y == -2);
    const auto nan = (std::numeric_limits<double>::quiet_NaN)();
    assert(!a::quantizeSound({nan,0,0},region));
    assert(!a::quantizeSound({(std::numeric_limits<double>::max)(),0,0},region));
    assert(a::soundAudible({0,0,0},{512,0,0},0.5,1));
    assert(!a::soundAudible({0,0,0},{512.01,0,0},0.5,1));
    assert(a::soundAudible({0,0,0},{8192,0,0},1,0));
    assert(!a::soundAudible({0,0,0},{8193,0,0},1,0));
    assert(!a::soundAudible({0,0,0},{0,0,0},0,0));
    assert(!a::soundAudible({0,0,0},{0,0,0},1,-1));
    assert(!a::soundAudible({nan,0,0},{0,0,0},1,1));
    a::EventCatalog catalog;
    assert(catalog.bind(413,"events/ak47.sc") == a::EventBinding::Registered);
    assert(catalog.find(413) == a::EventKind::Gunshot && catalog.find(1) == a::EventKind::Unknown);
    assert(catalog.bind(413,"events/ak47.sc") == a::EventBinding::Duplicate);
    assert(catalog.bind(413,"events/usp.sc") == a::EventBinding::Conflict);
    assert(catalog.find(413) == a::EventKind::Unknown);
    assert(catalog.bind(413,"events/ak47.sc") == a::EventBinding::Conflict);
    assert(catalog.bind(5,"events/createsmoke.sc") == a::EventBinding::Registered && catalog.find(5) == a::EventKind::Smoke);
    assert(catalog.supports(a::EventKind::Smoke));
    assert(catalog.bind(6,"events/unknown.sc") == a::EventBinding::Unsupported);
    assert(catalog.bind(0,"events/ak47.sc") == a::EventBinding::Invalid);
    assert(catalog.bind(7,nullptr) == a::EventBinding::Invalid);
    catalog.reset(); assert(catalog.find(413) == a::EventKind::Unknown);
    assert(!catalog.supports(a::EventKind::Smoke));
    for (std::uint16_t i=1; i<=a::EventCatalog::capacity; ++i) assert(catalog.bind(i,"events/ak47.sc") == a::EventBinding::Registered);
    assert(catalog.bind(999,"events/ak47.sc") == a::EventBinding::Full);
}
