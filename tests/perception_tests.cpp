// SPDX-License-Identifier: MPL-2.0
#include "core/perception.hpp"
#include <cassert>
#include <cmath>
#include <limits>
#include <vector>
using namespace astrabot::core;
using namespace astrabot::core::perception;
namespace {
struct Port final : IVisibilityQueries {
    std::vector<SightRequest> calls;
    Reason answer{Reason::None};
    bool headBlocked{};
    Vision* cancel{};
    Reason trace(const SightRequest& q) noexcept override {
        calls.push_back(q);
        if (cancel) cancel->forget(q.target);
        return headBlocked && q.end.z > 0 ? Reason::Occluded : answer;
    }
};
InputFrame frame() {
    InputFrame f{}; f.map = {1}; f.tick = {1};
    f.players[0] = {{1,{1}},{16},true,{0,0,0},{0,0,0},{1,0,0}};
    f.players[1] = {{2,{1}},{},true,{100,0,16},{100,0,0},{}};
    return f;
}
void advance(InputFrame& f, std::uint64_t dt=100000) { ++f.tick.value; f.timeMicros += dt; }
void roundIdentity() {
    auto f = frame(); Vision v; Port port; v.update(f,port);
    const auto observer = f.players[0].player;
    const auto old = *v.latest(observer);
    assert(old.identity.validAt(f.timeMicros) && old.identity.round == f.round);
    v.beginRound({2}); assert(!v.latest(observer));
    advance(f); v.update(f,port); assert(!v.latest(observer) && v.frameReason() == Reason::InvalidFrame);
    f.round = {2}; v.update(f,port); assert(v.latest(observer));
    const auto current = *v.latest(observer);
    assert(current.identity.round == f.round && current.identity.sequence > old.identity.sequence);
    v.beginRound({2}); assert(v.latest(observer)->identity.sequence == current.identity.sequence);
    advance(f,1); v.update(f,port);
    assert(v.latest(observer)->identity.sequence == current.identity.sequence);
    advance(f); v.update(f,port);
    assert(v.latest(observer)->identity.sequence > current.identity.sequence);
}
void geometryAndPublication() {
    auto f = frame(); Vision v; Port p;
    v.update(f,p);
    auto* batch = v.latest(f.players[0].player);
    assert(batch && batch->count == 1 && batch->observations[0].target == f.players[1].player);
    assert(batch->stamp.agent == f.players[0].agent && batch->stamp.map == f.map && batch->stamp.tick == f.tick);
    assert(batch->observations[0].position.z == 16 && p.calls.size() == 1);
    const auto stamp = batch->stamp;
    advance(f,1000); f.players[1].eye.x = 999;
    v.update(f,p);
    assert(v.latest(f.players[0].player)->stamp.tick == stamp.tick);
    assert(v.latest(f.players[0].player)->observations[0].position.x == 100);
    p.headBlocked = true; advance(f); v.update(f,p);
    assert(v.latest(f.players[0].player)->observations[0].position.z == 0);
    assert(v.diagnostics(f.players[0].player)->traces == 2);
    p.answer = Reason::Occluded; advance(f); v.update(f,p);
    assert(v.latest(f.players[0].player)->count == 0);
    for (const auto& observation : v.latest(f.players[0].player)->observations)
        assert(!observation.target.isValid() && observation.position.x == 0);
    assert(v.diagnostics(f.players[0].player)->reason == Reason::Occluded);

    for (const auto point : {Point{4096,0,0},Point{100,100,0},Point{100,0,100}}) {
        Vision fresh; Port clear; auto boundary = frame();
        boundary.players[1].eye = boundary.players[1].center = point;
        fresh.update(boundary,clear);
        assert(fresh.latest(boundary.players[0].player)->count == 1);
    }
    for (const auto point : {Point{4096.01,0,0},Point{100,100.01,0},Point{-100,0,0},Point{0,0,0},
            Point{(std::numeric_limits<double>::quiet_NaN)(),0,0}}) {
        Vision fresh; Port clear; auto rejected = frame();
        rejected.players[1].eye = rejected.players[1].center = point;
        fresh.update(rejected,clear);
        assert(fresh.latest(rejected.players[0].player)->count == 0 && clear.calls.empty());
    }
    Vision narrow({200000,50,60}); Port clear; auto shortRange = frame();
    narrow.update(shortRange,clear);
    assert(narrow.latest(shortRange.players[0].player)->count == 0);
    assert(narrow.diagnostics(shortRange.players[0].player)->reason == Reason::OutOfRange);
    Vision bad({0,4096,90}); bad.update(shortRange,clear);
    assert(bad.frameReason() == Reason::InvalidSettings && !bad.latest(shortRange.players[0].player));
}
void invalidation() {
    auto f = frame(); Port p; Vision v;
    v.update(f,p);
    const auto old = f.players[1].player;
    advance(f,1); f.players[1].alive = false; v.update(f,p);
    assert(v.latest(f.players[0].player)->count == 0);
    assert(v.latest(f.players[0].player)->stamp.tick.value == 1);
    f.players[1].alive = true; ++f.players[1].player.generation.value; advance(f); v.update(f,p);
    assert(v.latest(f.players[0].player)->observations[0].target != old);
    const auto current = f.players[1].player;
    f.players[1].player = old; advance(f); v.update(f,p);
    assert(v.frameReason() == Reason::StaleIdentity && v.latest(f.players[0].player)->count == 0);
    f.players[1].player = current; advance(f); v.update(f,p);
    v.forget(f.players[1].player);
    assert(v.latest(f.players[0].player)->count == 0);
    advance(f); v.update(f,p);
    const auto freshTick = v.latest(f.players[0].player)->stamp.tick;
    v.update(f,p);
    assert(v.frameReason() == Reason::InvalidFrame && v.latest(f.players[0].player)->stamp.tick == freshTick);
    advance(f); f.players[0].alive = false; v.update(f,p);
    assert(!v.latest(f.players[0].player));
    f.players[0].alive = true; advance(f); v.update(f,p);
    const auto observer = f.players[0].player;
    ++f.players[0].player.generation.value; advance(f); v.update(f,p);
    assert(!v.latest(observer) && v.latest(f.players[0].player));
    const auto newObserver = f.players[0].player;
    f.players[0].player = observer; advance(f); v.update(f,p);
    assert(!v.latest(observer) && v.frameReason() == Reason::StaleIdentity);
    f.players[0].player = newObserver;
    f.map = {2}; f.tick = {1}; f.timeMicros = 0; v.update(f,p);
    assert(v.latest(f.players[0].player)->stamp.map == f.map);
    p.cancel = &v; advance(f); v.update(f,p);
    assert(v.latest(f.players[0].player)->count == 0); // no post-forget resurrection
    p.cancel = nullptr;
    f.players[1].player.slot = 33; advance(f); v.update(f,p);
    assert(v.latest(f.players[0].player)->count == 0);
    assert(v.diagnostics(f.players[0].player)->reason == Reason::StaleIdentity);
    for (auto reason : {Reason::MissingEngine,Reason::StaleIdentity,Reason::InvalidTrace,static_cast<Reason>(255)}) {
        auto fail = frame(); Vision fresh; Port port; port.answer = reason;
        fresh.update(fail,port);
        assert(fresh.latest(fail.players[0].player)->count == 0);
        assert(fresh.diagnostics(fail.players[0].player)->reason != Reason::None);
    }
}
void scheduling() {
    for (const unsigned actors : {1U,8U,16U,32U}) for (const std::uint64_t dt : {8000ULL,16000ULL,100000ULL}) {
        InputFrame f{}; f.map = {1}; f.tick = {1}; Vision v; Port p;
        // Colocated observers and forward targets give a worst-case two-ray
        // blocked scan without querying self; trace counts still stay bounded.
        for (std::size_t i=0; i<32; ++i) f.players[i] = {
            {static_cast<std::uint16_t>(i+1),{1}},
            {i < actors ? static_cast<std::uint32_t>(i+1) : 0U},true,
            {static_cast<double>(i),0,16},{static_cast<double>(i),0,0},{1,0,0}};
        p.answer = Reason::Occluded;
        for (unsigned step=0; step<160; ++step) {
            p.calls.clear(); v.update(f,p);
            assert(v.frameUpdates() <= 4 && p.calls.size() <= 248);
            for (unsigned i=0; i<actors; ++i) {
                const auto* d = v.diagnostics(f.players[i].player);
                assert(d && d->traces <= 62 && d->candidates <= 31);
            }
            advance(f,dt);
        }
        std::uint64_t least = 10000, most = 0;
        for (unsigned i=0; i<actors; ++i) {
            const auto* d = v.diagnostics(f.players[i].player);
            assert(d->updates > 0);
            least = d->updates < least ? d->updates : least;
            most = d->updates > most ? d->updates : most;
            if (actors >= 8 && dt == 100000) {
                assert(d->deferredFrames > 0 && d->intervalMicros >= 200000);
            }
        }
        assert(most-least <= 2);
        advance(f,10000000); v.update(f,p);
        assert(v.frameUpdates() <= 4); // never catch up missed periods
    }
    auto f = frame(); Port p; p.answer = Reason::Occluded; Vision v;
    for (std::size_t i=1; i<32; ++i)
        f.players[i] = {{static_cast<std::uint16_t>(i+1),{1}},{},true,{100,0,10},{100,0,0},{}};
    v.update(f,p);
    assert(p.calls.size() == 62 && v.diagnostics(f.players[0].player)->candidates == 31);
    p.calls.clear(); p.answer = Reason::None; advance(f); v.update(f,p);
    const auto* batch = v.latest(f.players[0].player);
    assert(batch && batch->count == 31 && p.calls.size() == 31);
    for (std::size_t i=0; i<batch->count; ++i)
        assert(batch->observations[i].target == f.players[i+1].player);
}
}
int main() { geometryAndPublication(); invalidation(); scheduling(); roundIdentity(); }
