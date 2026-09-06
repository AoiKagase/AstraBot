// SPDX-License-Identifier: MPL-2.0
#include "core/visual_memory.hpp"
#include <cassert>
#include <chrono>
#include <iostream>
#include <limits>
using namespace astrabot::core;
namespace w = astrabot::core::world;
namespace p = astrabot::core::perception;
namespace {
PlayerId player(std::uint16_t slot, std::uint32_t generation=1) { return {slot,{generation}}; }
w::MemoryFrame frame() {
    w::MemoryFrame f{}; f.map = {1}; f.tick = {1}; f.timeMicros = 1000000;
    for (std::uint16_t i=1; i<=32; ++i) f.players[i-1U] = {player(i),{i},true};
    return f;
}
p::ObservationBatch batch(const w::MemoryFrame& f, std::uint16_t observer=1, std::uint16_t target=32) {
    p::ObservationBatch b{};
    const auto& owner = f.players[observer-1U];
    b.stamp = {owner.agent,owner.player,f.map,f.tick,f.timeMicros};
    b.stamp.round = f.round;
    b.identity = {f.map,f.round,p::ObservationSource::Vision,f.tick.value*32+observer,f.timeMicros,f.timeMicros};
    b.count = 1; b.observations[0] = {f.players[target-1U].player,{100,1,64}};
    return b;
}
void advance(w::VisualMemoryModel& m,w::MemoryFrame& f,std::uint64_t delta) {
    ++f.tick.value; f.timeMicros += delta; assert(m.advance(f));
}
void decayAndReacquire() {
    w::VisualMemoryModel m; auto f = frame(); assert(m.advance(f));
    const auto first = batch(f); assert(m.observe(first));
    auto snapshot = m.latest(player(1)); assert(snapshot && snapshot->count == 1);
    assert(snapshot->memories[0].confidence == 1 && snapshot->memories[0].lastSeenMicros == 1000000);
    assert(!m.observe(first) && m.diagnostics().reason == w::MemoryReason::DuplicateBatch);
    advance(m,f,2500000);
    assert(snapshot->memories[0].confidence == 0.5 && snapshot->memories[0].lastKnownPosition.x == 100);
    auto empty = batch(f); empty.count = 0; assert(m.observe(empty));
    assert(snapshot->count == 1 && snapshot->memories[0].lastSeenMicros == 1000000);
    advance(m,f,2499999); assert(snapshot->count == 1 && snapshot->memories[0].confidence > 0);
    advance(m,f,1); assert(snapshot->count == 0 && m.diagnostics().expired == 1);
    assert(!m.observe(first)); assert(snapshot->count == 0);
    auto fresh = batch(f); fresh.observations[0].position.x = 200;
    assert(m.observe(fresh)); assert(snapshot->memories[0].lastKnownPosition.x == 200);
    advance(m,f,1000000); fresh = batch(f); fresh.observations[0].position.x = 300;
    assert(m.observe(fresh)); assert(snapshot->memories[0].confidence == 1);
    assert(m.latest(player(2))->count == 0);
    w::VisualMemoryModel custom({10}); f = frame(); assert(custom.advance(f)); assert(custom.observe(batch(f)));
    advance(custom,f,5); assert(custom.latest(player(1))->memories[0].confidence == 0.5);
    advance(custom,f,5); assert(custom.latest(player(1))->count == 0);
}
void invalidAndRetired() {
    w::VisualMemoryModel m; auto f = frame(); assert(m.advance(f)); assert(m.observe(batch(f)));
    const auto old = batch(f);
    m.forget(player(32)); assert(m.latest(player(1))->count == 0);
    assert(!m.observe(old));
    advance(m,f,1); assert(m.observe(batch(f)));
    f.players[31].eligible = false; advance(m,f,1); assert(m.latest(player(1))->count == 0);
    f.players[31].eligible = true; f.players[31].player = player(32,2); advance(m,f,1);
    auto b = batch(f); b.observations[0].target = player(32); assert(!m.observe(b));
    assert(m.observe(batch(f)));
    f.players[31].player = player(32); advance(m,f,1); assert(m.latest(player(1))->count == 0);
    assert(!m.observe(batch(f))); // Old generation cannot be revived with a newer tick.
    f.players[31].player = player(32,2); advance(m,f,1); assert(m.observe(batch(f)));
    f.players[0].eligible = false; advance(m,f,1); assert(!m.latest(player(1)));
    f.players[0].eligible = true; advance(m,f,1); assert(m.latest(player(1))->count == 0);
    assert(!m.observe(old)); assert(m.observe(batch(f)));
    f.players[0].agent = {99}; advance(m,f,1); assert(m.latest(player(1))->count == 0);
    assert(m.observe(batch(f))); m.forget(player(1)); assert(!m.latest(player(1)));
    advance(m,f,1); assert(m.observe(batch(f)));
    ++f.map.value; f.tick = {1}; f.timeMicros = 0; assert(m.advance(f));
    assert(m.latest(player(1))->count == 0 && !m.observe(old));
    assert(m.observe(batch(f))); auto staleMap = f; staleMap.map = {1};
    assert(!m.advance(staleMap) && !m.latest(player(1)));
    advance(m,f,1); assert(m.latest(player(1))->count == 0);
}
void malformedAndClock() {
    w::VisualMemoryModel m; auto f = frame(); assert(m.advance(f));
    auto b = batch(f); b.count = 32; assert(!m.observe(b));
    b = batch(f); b.count = 2; b.observations[1] = b.observations[0]; assert(!m.observe(b));
    b.observations[1].target = player(2); b.observations[1].position.x = (std::numeric_limits<double>::quiet_NaN)();
    assert(!m.observe(b)); assert(m.latest(player(1))->count == 0); // atomic rejection
    b = batch(f); b.observations[0].target = player(1); assert(!m.observe(b));
    b = batch(f); b.observations[0].target = player(33); assert(!m.observe(b));
    b = batch(f); ++b.stamp.timeMicros; assert(!m.observe(b));
    b = batch(f); ++b.stamp.tick.value; assert(!m.observe(b));
    b = batch(f); assert(m.observe(b));
    advance(m,f,100); auto rollback = f; ++rollback.tick.value; --rollback.timeMicros;
    assert(!m.advance(rollback) && !m.latest(player(1)));
    assert(!m.observe(b)); assert(!m.advance(rollback)); // Retains time high-water.
    advance(m,f,1); assert(m.latest(player(1))->count == 0 && !m.observe(b));
    assert(m.observe(batch(f))); m.invalidate(w::MemoryReason::InvalidFrame);
    assert(!m.latest(player(1)) && !m.observe(batch(f)));
    assert(!m.advance(f)); advance(m,f,1); assert(m.latest(player(1))->count == 0);
    w::VisualMemoryModel zero({0}); assert(!zero.advance(f));
    assert(zero.diagnostics().reason == w::MemoryReason::InvalidSettings);
}
void roundAndProvenance() {
    w::VisualMemoryModel m; auto f = frame(); assert(m.advance(f));
    const auto old = batch(f); assert(m.observe(old));
    m.beginRound({2}); assert(!m.latest(player(1)) && !m.observe(old));
    f.round = {2}; advance(m,f,100);
    assert(m.latest(player(1))->count == 0 && !m.observe(old));
    auto b = batch(f); b.stamp.round = {1}; assert(!m.observe(b));
    b = batch(f); b.identity.round = {1}; assert(!m.observe(b));
    b = batch(f); b.identity.source = p::ObservationSource::Sound; assert(!m.observe(b));
    b = batch(f); --b.identity.observedMicros; assert(!m.observe(b));
    b = batch(f); ++b.identity.receivedMicros; assert(!m.observe(b));
    b = batch(f); assert(m.observe(b));
    const auto remembered = m.latest(player(1))->memories[0];
    assert(remembered.identity.sequence == b.identity.sequence && remembered.identity.round == f.round);
    advance(m,f,1); auto repeated = batch(f); repeated.identity.sequence = b.identity.sequence;
    assert(!m.observe(repeated));
    assert(m.latest(player(1))->memories[0].identity.observedMicros == remembered.lastSeenMicros);
    m.beginRound({2}); // Duplicate invalidation must not erase this round.
    assert(m.latest(player(1))->count == 1);
    auto rollback = f; ++rollback.tick.value; rollback.round = {1};
    assert(!m.advance(rollback) && !m.latest(player(1)));
    advance(m,f,1); assert(m.latest(player(1))->count == 0 && !m.observe(old));
}
void capacityAndBudget() {
    const auto start = std::chrono::steady_clock::now();
    w::VisualMemoryModel m; auto f = frame();
    for (unsigned round=0; round<100; ++round) {
        ++f.tick.value; f.timeMicros += 16000; assert(m.advance(f));
        assert(m.diagnostics().frameVisits <= 32*31);
        for (std::uint16_t owner=1; owner<=32; ++owner) {
            auto b = batch(f,owner); b.count = 0;
            for (std::uint16_t target=1; target<=32; ++target)
                if (target != owner) b.observations[b.count++] = {player(target),{double(target),0,0}};
            assert(m.observe(b)); assert(m.latest(player(owner))->count == 31);
        }
        assert(m.diagnostics().frameObservations == 32*31);
    }
    std::cout << "100 full 32x31 memory frames: "
              << std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count()
              << " us; model bytes: " << sizeof(m) << '\n';
}
}
int main() { decayAndReacquire(); invalidAndRetired(); malformedAndClock(); roundAndProvenance(); capacityAndBudget(); }
