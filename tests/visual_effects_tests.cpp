// SPDX-License-Identifier: MPL-2.0
#include "adapter/cstrike/visual_effects.hpp"
#include <cassert>
#include <limits>
namespace a = astrabot::adapter::cstrike;
namespace p = astrabot::core::perception;
constexpr astrabot::core::PlayerId actor{1,{1}}, other{2,{1}};
int main() {
    a::VisualEffects effects({10,1000});
    assert(effects.advance({1},{1},0));
    assert(effects.blocked(actor,{-20,0,0},{20,0,0}) == p::Reason::None);
    assert(effects.smoke({0,0,0}) && effects.diagnostics().regions == 1);
    assert(!effects.smoke({0,0,0}) && effects.diagnostics().duplicates == 1);
    assert(effects.blocked(actor,{-20,0,0},{20,0,0}) == p::Reason::SmokeOccluded);
    assert(effects.blocked(actor,{-20,10,0},{20,10,0}) == p::Reason::SmokeOccluded);
    assert(effects.blocked(actor,{-20,10.01,0},{20,10.01,0}) == p::Reason::None);
    assert(effects.blocked(actor,{0,0,0},{0,0,0}) == p::Reason::SmokeOccluded);
    assert(effects.blocked(actor,{20,0,0},{30,0,0}) == p::Reason::None);
    assert(effects.blocked(actor,{-20,0,20},{20,0,20}) == p::Reason::None); // Head clear, body blocked.
    assert(effects.advance({1},{1},999));
    assert(effects.blocked(actor,{-20,0,0},{20,0,0}) == p::Reason::SmokeOccluded);
    assert(effects.advance({1},{1},1000));
    assert(effects.diagnostics().regions == 0 && effects.diagnostics().expired == 1);
    assert(effects.flash(actor,100));
    assert(effects.blocked(actor,{50,0,0},{60,0,0}) == p::Reason::FlashBlind);
    assert(effects.blocked(other,{50,0,0},{60,0,0}) == p::Reason::None);
    assert(effects.blocked({1,{2}},{50,0,0},{60,0,0}) == p::Reason::None);
    assert(!effects.flash(actor,100));
    assert(effects.advance({1},{1},1099));
    assert(effects.blocked(actor,{50,0,0},{60,0,0}) == p::Reason::FlashBlind);
    assert(effects.advance({1},{1},1100));
    assert(effects.blocked(actor,{50,0,0},{60,0,0}) == p::Reason::None);
    for (unsigned i=0;i<32;++i) assert(effects.smoke({static_cast<double>(i)*30,0,0}));
    assert(!effects.smoke({10000,0,0}) && effects.diagnostics().overflowActive);
    assert(effects.blocked(actor,{20000,0,0},{20010,0,0}) == p::Reason::EffectsOverflow);
    assert(effects.advance({1},{1},2100));
    assert(!effects.diagnostics().overflowActive && effects.diagnostics().regions == 0);
    assert(effects.flash(actor,100)); effects.forget(actor);
    assert(effects.blocked(actor,{0,0,0},{1,0,0}) == p::Reason::None);
    assert(effects.smoke({0,0,0})); assert(effects.advance({1},{2},2100));
    assert(effects.diagnostics().regions == 0);
    assert(effects.smoke({0,0,0})); assert(effects.advance({2},{1},0));
    assert(effects.diagnostics().regions == 0);
    assert(effects.advance({2},{1},100)); assert(effects.smoke({0,0,0}));
    assert(!effects.advance({2},{1},99));
    assert(effects.blocked(actor,{0,0,0},{1,0,0}) == p::Reason::InvalidFrame);
    assert(!effects.smoke({0,0,0})); assert(effects.advance({2},{1},100));
    assert(effects.blocked(actor,{0,0,0},{1,0,0}) == p::Reason::SmokeOccluded);
    assert(!effects.smoke({(std::numeric_limits<double>::quiet_NaN)(),0,0}));
    a::VisualEffects invalid({0,1}); assert(!invalid.advance({1},{1},0));
    a::VisualEffects invalidTime({1,0}); assert(!invalidTime.advance({1},{1},0));
    assert(effects.advance({2},{1},(std::numeric_limits<std::uint64_t>::max)()));
    assert(!effects.smoke({0,0,0}) && !effects.flash(actor,1));
}
