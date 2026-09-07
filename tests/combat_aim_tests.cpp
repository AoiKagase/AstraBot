// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include "core/combat.hpp"

#include <cassert>
#include <cmath>
#include <limits>

namespace {

namespace c = astrabot::core::combat;
namespace p = astrabot::core::perception;
namespace w = astrabot::core::world;

constexpr astrabot::core::MapGeneration map{3};
constexpr p::RoundGeneration round{7};
constexpr astrabot::core::TickId tick{11};
constexpr std::uint64_t now = 1100000;
constexpr astrabot::core::PlayerId actor{1, {1}};
constexpr astrabot::core::PlayerId target{2, {1}};
constexpr astrabot::core::PlayerId reporter{3, {1}};
constexpr double kEpsilon = 1.0e-5;

struct Fixture {
    w::MemorySnapshot visual{};
    w::SoundSnapshot sounds{};
    w::ReportSnapshot reports{};
    c::CombatInput input{};

    Fixture() { reset(); }

    void reset() {
        visual = {};
        sounds = {};
        reports = {};
        input = {};
        input.map = map;
        input.round = round;
        input.tick = tick;
        input.timeMicros = now;
        input.player = actor;
        input.agent = {1};
        input.alive = true;
        input.team = p::Team::CounterTerrorist;
        input.eye = {0.0, 0.0, 0.0};
        input.view = {0.0F, 0.0F, 0.0F};
        input.world.stamp = {input.agent, actor, map, tick, now, round};
        visual.stamp = input.world.stamp;
        sounds.stamp = input.world.stamp;
        reports.stamp = input.world.stamp;
        input.world.visual = &visual;
        input.world.sounds = &sounds;
        input.world.reports = &reports;
        input.world.roster[actor.slot - 1U] = {actor, p::Team::CounterTerrorist};
        input.world.roster[target.slot - 1U] = {target, p::Team::Terrorist};
        input.world.roster[reporter.slot - 1U] = {reporter, p::Team::CounterTerrorist};
        input.weapon.map = map;
        input.weapon.round = round;
        input.weapon.tick = tick;
        input.weapon.observedMicros = now;
        input.weapon.active = {5};
        input.weapon.owned[0] = {5};
        input.weapon.ownedCount = 1;
        input.weapon.clipAmmo = 12;
        input.weapon.reserveAmmo = 48;
        input.difficulty.decisionQuality = 100;
    }
};

p::ObservationIdentity visionIdentity(std::uint64_t observed, std::uint64_t received,
                                      std::uint64_t sequence) {
    return {map, round, p::ObservationSource::Vision, sequence, observed, received};
}

void addVisual(Fixture& fixture, p::Point position, double confidence = 1.0,
               std::uint64_t observed = now, std::uint64_t sequence = 1) {
    fixture.visual.memories[0] = {target, position, observed, confidence,
                                  visionIdentity(observed, observed, sequence)};
    fixture.visual.count = 1;
}

void addReport(Fixture& fixture, p::Point position, double confidence = 0.5,
               std::uint64_t observed = now, std::uint64_t sequence = 1) {
    w::TeamReport report{};
    report.reporter = reporter;
    report.receiver = actor;
    report.target = target;
    report.position = position;
    report.origin = visionIdentity(observed, observed, sequence);
    report.identity = {map, round, p::ObservationSource::TeamReport, sequence, observed, now};
    report.sentMicros = now;
    fixture.reports.reports[0] = {report, confidence};
    fixture.reports.count = 1;
}

void assertTrack(const c::CombatDecision& decision) {
    assert(decision.action == c::CombatAction::Track);
    assert(decision.target == target);
    assert(decision.buttons == 0);
    assert(!decision.fireMode.has_value());
    assert(!decision.firePlan.has_value());
    assert(decision.inputTick == tick);
    assert(decision.validUntilMicros == now);
    assert(decision.validateForP5());
    assert(!decision.hasAttackInput());
}

double shortestDelta(double delta) {
    while (delta > 180.0) delta -= 360.0;
    while (delta < -180.0) delta += 360.0;
    return delta;
}

p::Point pointAt(double degrees, double radius = 100.0) {
    constexpr double kRadians = 0.017453292519943295;
    return {radius * std::cos(degrees * kRadians), radius * std::sin(degrees * kRadians), 0.0};
}

void testGeometryAndYawWrap() {
    Fixture fixture;
    addVisual(fixture, {100.0, 0.0, 0.0});
    auto decision = c::aimTarget(fixture.input);
    assertTrack(decision);
    assert(std::abs(decision.view.pitch) < kEpsilon);
    assert(std::abs(decision.view.yaw) < kEpsilon);

    fixture.reset();
    addVisual(fixture, {100.0, 0.0, 100.0});
    decision = c::aimTarget(fixture.input);
    assert(std::abs(decision.view.pitch + 45.0F) < kEpsilon);

    fixture.reset();
    fixture.input.view.yaw = 179.0F;
    addVisual(fixture, pointAt(-179.0));
    decision = c::aimTarget(fixture.input);
    assert(std::abs(shortestDelta(static_cast<double>(decision.view.yaw) - 179.0) - 2.0) < 0.01);
    assert(decision.view.yaw < -178.0F);

    fixture.reset();
    addVisual(fixture, {0.0, 0.0, 1000.0});
    decision = c::aimTarget(fixture.input);
    assert(decision.view.pitch == astrabot::core::kMinPitch);
}

void testReactionWindowAndReportedTarget() {
    Fixture fixture;
    fixture.input.difficulty.reactionDelayMicros = 200000;
    addVisual(fixture, {100.0, 0.0, 0.0}, 1.0, now - 100000);
    auto decision = c::aimTarget(fixture.input);
    assertTrack(decision);
    assert(decision.source == p::ObservationSource::Vision);
    assert(decision.reason == c::CombatReason::ReactionDelay);

    fixture.reset();
    fixture.input.difficulty.reactionDelayMicros = 100000;
    addVisual(fixture, {100.0, 0.0, 0.0}, 1.0, now - 100000);
    decision = c::aimTarget(fixture.input);
    assert(decision.reason == c::CombatReason::Accepted);

    fixture.reset();
    addReport(fixture, {0.0, 100.0, 0.0}, 0.5, now - 100000);
    decision = c::aimTarget(fixture.input);
    assertTrack(decision);
    assert(decision.source == p::ObservationSource::TeamReport);
    assert(std::abs(decision.view.yaw - 90.0F) < kEpsilon);
}

void testDeterministicBoundedErrors() {
    Fixture fixture;
    addVisual(fixture, {100.0, 0.0, 0.0});
    fixture.input.difficulty.observationErrorDegrees = 10.0F;
    fixture.input.difficulty.predictionErrorDegrees = 20.0F;
    fixture.input.difficulty.aimNoiseDegrees = 30.0F;
    fixture.input.difficulty.decisionQuality = 50;
    const auto first = c::aimTarget(fixture.input);
    const auto second = c::aimTarget(fixture.input);
    assert(first.view == second.view);
    assert(std::abs(first.view.pitch) <= 30.0F + kEpsilon);
    assert(std::abs(first.view.yaw) <= 30.0F + kEpsilon);

    fixture.input.difficulty.decisionQuality = 100;
    const auto perfect = c::aimTarget(fixture.input);
    assert(std::abs(perfect.view.pitch) < kEpsilon);
    assert(std::abs(perfect.view.yaw) < kEpsilon);
}

void testFailClosedAndNeverFire() {
    Fixture fixture;
    addVisual(fixture, {0.0, 0.0, 0.0});
    auto decision = c::aimTarget(fixture.input);
    assert(decision.action == c::CombatAction::NoOp);
    assert(decision.reason == c::CombatReason::StaleTarget);
    assert(!decision.hasAttackInput());

    fixture.reset();
    addVisual(fixture, {(std::numeric_limits<double>::quiet_NaN)(), 0.0, 0.0});
    decision = c::aimTarget(fixture.input);
    assert(decision.reason == c::CombatReason::StaleTarget);
    assert(!decision.hasAttackInput());

    fixture.reset();
    fixture.input.view.pitch = (std::numeric_limits<float>::quiet_NaN)();
    addVisual(fixture, {100.0, 0.0, 0.0});
    decision = c::aimTarget(fixture.input);
    assert(decision.reason == c::CombatReason::NonFinitePose);
    assert(!decision.hasAttackInput());
}

} // namespace

int main() {
    testGeometryAndYawWrap();
    testReactionWindowAndReportedTarget();
    testDeterministicBoundedErrors();
    testFailClosedAndNeverFire();
    return 0;
}
