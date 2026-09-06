// SPDX-License-Identifier: MPL-2.0
#include "core/sound_memory.hpp"
#include <cassert>
#include <iostream>
namespace p = astrabot::core::perception;
namespace w = astrabot::core::world;
using astrabot::core::PlayerId;
namespace {
constexpr PlayerId actor{1,{1}}, other{2,{1}};
w::MemoryFrame frame() {
    w::MemoryFrame f{}; f.map = {1}; f.tick = {1}; f.timeMicros = 1000000;
    f.players[0] = {actor,{1},true}; f.players[1] = {other,{2},true}; return f;
}
p::SoundObservation sound(const w::MemoryFrame& f,std::uint64_t seq=1) {
    return {{f.map,f.round,p::ObservationSource::Sound,seq,f.timeMicros,f.timeMicros},p::SoundKind::Footstep,{-1,2,0}};
}
void advance(w::SoundMemoryModel& model,w::MemoryFrame& f,std::uint64_t dt) {
    ++f.tick.value; f.timeMicros += dt; assert(model.advance(f));
}
void historyAndExpiry() {
    w::SoundMemoryModel model; auto f = frame(); assert(model.advance(f));
    const auto initial = sound(f); assert(model.observe(actor,initial));
    assert(model.latest(actor)->sounds[0].confidence == 0.5);
    assert(model.latest(other)->count == 0);
    assert(!model.observe(actor,initial) && model.diagnostics().reason == w::SoundReason::DuplicateObservation);
    advance(model,f,1500000); assert(model.latest(actor)->sounds[0].confidence == 0.25);
    auto delayed = sound(f,2); delayed.identity.observedMicros = initial.identity.observedMicros;
    assert(model.observe(actor,delayed)); assert(model.latest(actor)->sounds[1].confidence == 0.25);
    assert(model.latest(actor)->sounds[1].observation.region.x == -1);
    advance(model,f,1499999); assert(model.latest(actor)->count == 2);
    advance(model,f,1); assert(model.latest(actor)->count == 0 && model.diagnostics().expired == 2);
    delayed.identity.sequence = 3; delayed.identity.receivedMicros = f.timeMicros;
    assert(!model.observe(actor,delayed) && model.diagnostics().reason == w::SoundReason::ExpiredObservation);
    assert(model.observe(actor,sound(f,4)));
    w::SoundMemoryModel shortLife({10}); f = frame(); assert(shortLife.advance(f));
    assert(shortLife.observe(actor,sound(f))); advance(shortLife,f,10); assert(shortLife.latest(actor)->count == 0);
}
void invalidAndRetired() {
    w::SoundMemoryModel model; auto f = frame(); assert(model.advance(f));
    auto observation = sound(f); observation.kind = p::SoundKind::Unknown; assert(!model.observe(actor,observation));
    observation = sound(f); observation.identity.source = p::ObservationSource::Vision; assert(!model.observe(actor,observation));
    observation = sound(f); ++observation.identity.receivedMicros; assert(!model.observe(actor,observation));
    observation = sound(f); ++observation.identity.observedMicros; assert(!model.observe(actor,observation));
    observation = sound(f); observation.identity.sequence = 0; assert(!model.observe(actor,observation));
    observation = sound(f,2); assert(model.observe(actor,observation)); assert(!model.observe(actor,sound(f,1)));
    const auto old = observation;
    model.forget(other); assert(model.latest(actor)->count == 1); // Anonymous sound has no source player to retire.
    model.forget(actor); assert(!model.latest(actor) && !model.observe(actor,old));
    f.players[0].player = {1,{2}}; advance(model,f,1);
    assert(!model.latest(actor) && !model.observe(actor,sound(f,3)));
    assert(model.latest(f.players[0].player)->count == 0);
    assert(model.observe(f.players[0].player,sound(f,4)));
    model.beginRound({2}); assert(!model.latest(f.players[0].player));
    f.round = {2}; advance(model,f,1); assert(!model.observe(f.players[0].player,old));
    assert(model.observe(f.players[0].player,sound(f,5)));
    auto beforeRollback = sound(f,5);
    auto rollback = f; ++rollback.tick.value; --rollback.timeMicros;
    assert(!model.advance(rollback) && !model.latest(f.players[0].player));
    assert(!model.advance(rollback)); advance(model,f,1); assert(model.latest(f.players[0].player)->count == 0);
    beforeRollback.identity.receivedMicros = f.timeMicros;
    assert(!model.observe(f.players[0].player,beforeRollback));
    assert(model.latest(f.players[0].player)->count == 0);
    auto staleRound = f; ++staleRound.tick.value; staleRound.round = {1}; assert(!model.advance(staleRound));
    f.map = {2}; f.round = {1}; f.timeMicros = 0; f.tick = {1}; assert(model.advance(f));
    assert(!model.observe(f.players[0].player,old));
    w::SoundMemoryModel invalid({0}); assert(!invalid.advance(f));
    assert(invalid.diagnostics().reason == w::SoundReason::InvalidSettings);
}
void capacity() {
    w::SoundMemoryModel model; auto f = frame();
    for (std::uint16_t i=1; i<=32; ++i) f.players[i-1U] = {{i,{1}},{i},true};
    assert(model.advance(f));
    for (std::uint16_t i=1; i<=32; ++i) for (unsigned seq=1; seq<=20; ++seq) assert(model.observe({i,{1}},sound(f,seq)));
    for (std::uint16_t i=1; i<=32; ++i) {
        const auto* snapshot = model.latest({i,{1}});
        assert(snapshot && snapshot->count == 16 && snapshot->sounds[0].observation.identity.sequence == 5);
    }
    assert(model.diagnostics().evicted == 32*4);
    advance(model,f,1); assert(model.diagnostics().frameVisits == 32*16);
    assert(model.diagnostics().frameObservations == 0);
    std::cout << "SoundMemoryModel bytes: " << sizeof(model) << "; bounded decay visits: " << model.diagnostics().frameVisits << '\n';
}
}
int main() { historyAndExpiry(); invalidAndRetired(); capacity(); }
