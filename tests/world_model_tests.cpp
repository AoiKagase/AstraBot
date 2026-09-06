// SPDX-License-Identifier: MPL-2.0
#include "core/world_model.hpp"
#include <cassert>
#include <cmath>
#include <memory>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <limits>
namespace w = astrabot::core::world;
namespace p = astrabot::core::perception;
using astrabot::core::PlayerId;
constexpr PlayerId actor{1,{1}}, other{2,{1}}, target{32,{1}};
w::MemoryFrame frame() {
    w::MemoryFrame f{}; f.map={1}; f.round={1}; f.tick={1}; f.timeMicros=1000000;
    f.players[0]={actor,{1},true}; f.players[1]={other,{2},true}; f.players[31]={target,{},true}; return f;
}
p::ObservationBatch visible(const w::MemoryFrame& f,std::uint64_t sequence=1) {
    p::ObservationBatch b{}; b.stamp={{1},actor,f.map,f.tick,f.timeMicros,f.round};
    b.identity={f.map,f.round,p::ObservationSource::Vision,sequence,f.timeMicros,f.timeMicros};
    b.count=1; b.observations[0]={target,{100,10,36}}; return b;
}
p::SoundObservation sound(const w::MemoryFrame& f,std::uint64_t sequence=1) {
    return {{f.map,f.round,p::ObservationSource::Sound,sequence,f.timeMicros-500000,f.timeMicros},p::SoundKind::Gunshot,{1,-1,0}};
}
void history() {
    auto world=std::make_unique<w::WorldModel>(); auto f=frame(); assert(world->advance(f));
    const auto batch=visible(f); assert(world->stage(batch)); assert(world->stage(actor,sound(f)));
    assert(!world->latest(actor)); assert(world->publish({12,3}));
    auto s=world->latest(actor); assert(s && s->visual == world->visual().latest(actor) && s->sounds == world->sounds().latest(actor));
    assert(s->visual->count == 1 && s->sounds->count == 1 && s->queues.soundPending == 12 && s->queues.soundOverflow == 3);
    assert(s->oldestSoundAgeMicros == 500000 && s->maxReceiptDelayMicros == 500000 && s->oldestVisualAgeMicros == 0);
    assert(world->latest(other)->visual->count == 0 && world->latest(other)->sounds->count == 0);
    ++f.tick.value; f.timeMicros += 1500000; assert(world->advance(f)); assert(!world->latest(actor));
    assert(world->stage(batch)); assert(world->publish()); s=world->latest(actor);
    assert(s && s->visual->memories[0].lastSeenMicros == batch.stamp.timeMicros);
    assert(std::abs(s->visual->memories[0].confidence-0.7)<1e-12);
    assert(std::abs(s->sounds->sounds[0].confidence-1.0/6)<1e-12);
    assert(world->diagnostics().rejected[static_cast<std::size_t>(w::WorldReason::VisualRejected)] == 1);
    ++f.tick.value; f.timeMicros=3500000; assert(world->advance(f)); assert(world->publish());
    assert(world->latest(actor)->sounds->count == 0 && world->latest(actor)->visual->count == 1);
    ++f.tick.value; f.timeMicros=6000000; assert(world->advance(f)); assert(world->publish());
    assert(world->latest(actor)->visual->count == 0);
    assert(!world->stage(batch) && !world->publish());
}
std::string replay(bool reverse) {
    auto world=std::make_unique<w::WorldModel>(); auto f=frame(); std::ostringstream out; out << std::setprecision(17);
    for (unsigned step=0;step<12;++step) {
        f.tick.value=step+1; f.timeMicros=1000000+step*500000; assert(world->advance(f));
        auto first=sound(f,step*2+1), second=sound(f,step*2+2); second.region.x=2;
        const auto batch=visible(f,step+1);
        if (reverse) { assert(world->stage(actor,second)); if(step%3==0) assert(world->stage(batch)); assert(world->stage(actor,first)); }
        else { assert(world->stage(actor,first)); if(step%3==0) assert(world->stage(batch)); assert(world->stage(actor,second)); }
        assert(world->publish()); const auto s=world->latest(actor); assert(s);
        out << s->stamp.timeMicros << ':' << s->oldestVisualAgeMicros << ':' << s->oldestSoundAgeMicros << ':' << s->visual->count << ':' << s->sounds->count;
        for(std::size_t i=0;i<s->visual->count;++i) { const auto& v=s->visual->memories[i]; out<<':'<<v.lastKnownPosition.x<<':'<<v.lastSeenMicros<<':'<<v.confidence<<':'<<v.identity.sequence; }
        for(std::size_t i=0;i<s->sounds->count;++i) { const auto& v=s->sounds->sounds[i]; out<<':'<<v.observation.region.x<<':'<<v.observation.identity.sequence<<':'<<v.confidence; }
        out<<'\n';
    }
    return out.str();
}
void conflictsAndRetirement() {
    auto world=std::make_unique<w::WorldModel>(); auto f=frame(); assert(world->advance(f));
    auto b=visible(f); assert(world->stage(b)); assert(world->stage(b));
    auto s=sound(f); assert(world->stage(actor,s)); s.region.x=99; assert(world->stage(actor,s));
    assert(world->publish()); assert(world->latest(actor)->visual->count == 1 && world->latest(actor)->sounds->count == 0);
    assert(world->diagnostics().rejected[static_cast<std::size_t>(w::WorldReason::Duplicate)] == 1);
    assert(world->diagnostics().rejected[static_cast<std::size_t>(w::WorldReason::Conflict)] == 1);
    ++f.tick.value; ++f.timeMicros; assert(world->advance(f)); assert(world->stage(visible(f,2))); world->forget(target);
    assert(world->publish()); assert(world->latest(actor)->visual->count == 0);
    world->forget(actor); assert(!world->latest(actor));
    world->beginRound({2}); f.round={2}; ++f.tick.value; ++f.timeMicros; assert(world->advance(f));
    assert(world->stage(b)); assert(world->publish()); assert(world->latest(actor)->visual->count == 0);
    auto prior=f; --prior.timeMicros; ++prior.tick.value; assert(!world->advance(prior)); assert(!world->latest(actor));
    ++f.tick.value; ++f.timeMicros; assert(world->advance(f)); assert(world->publish());
    f.map={2}; f.round={1}; f.tick={1}; f.timeMicros=0; assert(world->advance(f)); assert(world->publish());
    assert(world->latest(actor)->visual->count == 0 && world->latest(actor)->sounds->count == 0);
}
void invalidInputs() {
    auto world=std::make_unique<w::WorldModel>(); auto f=frame(); assert(world->advance(f));
    auto b=visible(f); b.observations[0].position.x=(std::numeric_limits<double>::quiet_NaN)();
    assert(world->stage(b)); auto s=sound(f); s.identity.receivedMicros=f.timeMicros+1;
    assert(world->stage(actor,s)); assert(world->publish());
    assert(world->latest(actor)->visual->count == 0 && world->latest(actor)->sounds->count == 0);
    ++f.tick.value; ++f.timeMicros; assert(world->advance(f));
    assert(world->stage(visible(f,2))); assert(world->stage(actor,sound(f,2))); assert(world->publish());
    b=visible(f,2); s=sound(f,2);
    ++f.tick.value; ++f.timeMicros; assert(world->advance(f));
    assert(world->stage(b)); s.identity.receivedMicros=f.timeMicros; assert(world->stage(actor,s));
    assert(world->publish()); assert(world->latest(actor)->visual->memories[0].identity.sequence == 2);
    assert(world->latest(actor)->sounds->count == 1);
    f.players[0].player.generation={2}; ++f.tick.value; ++f.timeMicros;
    assert(world->advance(f)); assert(world->stage(b)); assert(world->stage(actor,s)); assert(world->publish());
    assert(!world->latest(actor)); assert(world->latest({1,{2}})->visual->count == 0);
    assert(world->latest({1,{2}})->sounds->count == 0);
}
void capacity() {
    auto world=std::make_unique<w::WorldModel>(); auto f=frame();
    for(std::uint16_t i=1;i<=32;++i) f.players[i-1U]={{i,{1}},{i},true};
    assert(world->advance(f));
    for(std::uint16_t i=1;i<=32;++i) for(unsigned j=1;j<=32;++j) assert(world->stage({i,{1}},sound(f,j)));
    assert(!world->stage(actor,sound(f,99)));
    for(unsigned i=0;i<32;++i) assert(world->stage(visible(f)));
    assert(!world->stage(visible(f))); assert(world->publish());
    assert(world->diagnostics().frameProcessed == 1056);
    for(std::uint16_t i=1;i<=32;++i) assert(world->latest({i,{1}})->sounds->count == 16);
    ++f.tick.value; ++f.timeMicros; assert(world->advance(f)); assert(world->sounds().diagnostics().frameVisits == 512);
    assert(world->publish()); std::cout << "WorldModel bytes: " << sizeof(w::WorldModel) << '\n';
}
int main() { history(); assert(replay(false)==replay(true)); conflictsAndRetirement(); invalidInputs(); capacity(); }
