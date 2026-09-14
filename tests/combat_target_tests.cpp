// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include "core/combat.hpp"

#include <cassert>
#include <cmath>
#include <limits>

namespace
{

namespace c = astrabot::core::combat;
namespace p = astrabot::core::perception;
namespace w = astrabot::core::world;

constexpr astrabot::core::MapGeneration map{3};
constexpr p::RoundGeneration round{7};
constexpr astrabot::core::TickId tick{11};
constexpr std::uint64_t now = 1100000;
constexpr astrabot::core::PlayerId actor{1, {1}};
constexpr astrabot::core::PlayerId targetA{2, {1}};
constexpr astrabot::core::PlayerId targetB{3, {1}};
constexpr astrabot::core::PlayerId targetC{4, {1}};
constexpr astrabot::core::PlayerId reporter{5, {1}};

struct Fixture
{
	w::MemorySnapshot visual{};
	w::SoundSnapshot sounds{};
	w::ReportSnapshot reports{};
	c::CombatInput input{};

	Fixture()
	{
		reset();
	}

	void reset()
	{
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
		input.world.visual = &visual;
		input.world.sounds = &sounds;
		input.world.reports = &reports;
		reports.stamp = input.world.stamp;
		input.world.roster[actor.slot - 1U] = {actor, p::Team::CounterTerrorist};
		input.world.roster[targetA.slot - 1U] = {targetA, p::Team::Terrorist};
		input.world.roster[targetB.slot - 1U] = {targetB, p::Team::Terrorist};
		input.world.roster[targetC.slot - 1U] = {targetC, p::Team::Terrorist};
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
	}
};

p::ObservationIdentity visionIdentity(std::uint64_t observed, std::uint64_t received, std::uint64_t sequence)
{
	return {map, round, p::ObservationSource::Vision, sequence, observed, received};
}

void addVisual(Fixture& fixture, std::size_t index, astrabot::core::PlayerId target, p::Point position,
			   double confidence, std::uint64_t observed, std::uint64_t sequence)
{
	fixture.visual.memories[index] = {target, position, observed, confidence,
									  visionIdentity(observed, observed, sequence)};
	if (fixture.visual.count <= index)
		fixture.visual.count = index + 1;
}

void addReport(Fixture& fixture, astrabot::core::PlayerId target, p::Point position, double confidence,
			   std::uint64_t observed, std::uint64_t sequence)
{
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

void assertTrack(const c::CombatDecision& decision, astrabot::core::PlayerId target, p::ObservationSource source)
{
	assert(decision.action == c::CombatAction::Track);
	assert(decision.target == target);
	assert(decision.source == source);
	assert(decision.reason == c::CombatReason::Accepted);
	assert(decision.buttons == 0);
	assert(!decision.fireMode.has_value());
	assert(decision.validUntilMicros == now);
	assert(decision.validateForP5());
	assert(!decision.hasAttackInput());
}

void testDirectVisionAndReportFallback()
{
	Fixture fixture;
	addVisual(fixture, 0, targetA, {100.0, 0.0, 0.0}, 0.25, now - 100000, 1);
	auto decision = c::selectTarget(fixture.input);
	assertTrack(decision, targetA, p::ObservationSource::Vision);
	assert(decision.targetAgeMicros == 100000);
	assert(std::abs(decision.confidence - 0.25) < 1e-12);

	fixture.reset();
	addReport(fixture, targetA, {100.0, 0.0, 0.0}, 0.5, now - 200000, 1);
	decision = c::selectTarget(fixture.input);
	assertTrack(decision, targetA, p::ObservationSource::TeamReport);
	assert(decision.targetAgeMicros == 200000);

	addVisual(fixture, 0, targetB, {0.0, 100.0, 0.0}, 0.01, now - 250000, 2);
	decision = c::selectTarget(fixture.input);
	assertTrack(decision, targetB, p::ObservationSource::Vision);
}

void testDeterministicOrdering()
{
	Fixture fixture;
	addVisual(fixture, 0, targetA, {100.0, 0.0, 0.0}, 0.4, now - 100000, 1);
	addVisual(fixture, 1, targetB, {0.0, 100.0, 0.0}, 0.8, now - 100000, 2);
	assert(c::selectTarget(fixture.input).target == targetB);

	fixture.reset();
	addVisual(fixture, 0, targetA, {100.0, 0.0, 0.0}, 0.8, now - 100000, 1);
	addVisual(fixture, 1, targetB, {0.0, 100.0, 0.0}, 0.8, now - 200000, 2);
	assert(c::selectTarget(fixture.input).target == targetA);

	fixture.reset();
	addVisual(fixture, 0, targetA, {0.0, 100.0, 0.0}, 0.8, now - 100000, 1);
	addVisual(fixture, 1, targetB, {100.0, 0.0, 0.0}, 0.8, now - 100000, 2);
	assert(c::selectTarget(fixture.input).target == targetB);

	fixture.reset();
	addVisual(fixture, 0, targetB, {100.0, 0.0, 0.0}, 0.8, now - 100000, 2);
	addVisual(fixture, 1, targetC, {100.0, 0.0, 0.0}, 0.8, now - 100000, 3);
	assert(c::selectTarget(fixture.input).target == targetB);

	fixture.reset();
	addReport(fixture, targetA, {100.0, 0.0, 0.0}, 0.5, now - 100000, 1);
	addVisual(fixture, 0, targetB, {100.0, 0.0, 0.0}, 0.01, now - 250000, 2);
	assert(c::selectTarget(fixture.input).target == targetB);
}

void testRelationsFailClosed()
{
	Fixture fixture;
	addVisual(fixture, 0, targetA, {100.0, 0.0, 0.0}, 1.0, now, 1);
	fixture.input.world.roster[targetA.slot - 1U].team = p::Team::CounterTerrorist;
	auto decision = c::selectTarget(fixture.input);
	assert(decision.action == c::CombatAction::NoOp);
	assert(decision.reason == c::CombatReason::Ally);
	assert(!decision.target.isValid() && !decision.hasAttackInput());

	fixture.reset();
	addVisual(fixture, 0, actor, {100.0, 0.0, 0.0}, 1.0, now, 1);
	decision = c::selectTarget(fixture.input);
	assert(decision.reason == c::CombatReason::Ally);

	fixture.reset();
	addVisual(fixture, 0, targetA, {100.0, 0.0, 0.0}, 1.0, now, 1);
	fixture.input.world.roster[targetA.slot - 1U] = {};
	decision = c::selectTarget(fixture.input);
	assert(decision.reason == c::CombatReason::UnknownRelation);

	fixture.reset();
	addVisual(fixture, 0, targetA, {100.0, 0.0, 0.0}, 1.0, now, 1);
	fixture.input.world.roster[targetA.slot - 1U] = {targetA, p::Team::Terrorist};
	fixture.input.team = p::Team::Unknown;
	decision = c::selectTarget(fixture.input);
	assert(decision.reason == c::CombatReason::UnknownRelation);

	fixture.reset();
	addVisual(fixture, 0, targetA, {100.0, 0.0, 0.0}, 1.0, now, 1);
	fixture.input.world.roster[targetA.slot - 1U] = {{targetA.slot, {2}}, p::Team::Terrorist};
	decision = c::selectTarget(fixture.input);
	assert(decision.reason == c::CombatReason::UnknownRelation);

	fixture.reset();
	addVisual(fixture, 0, targetA, {100.0, 0.0, 0.0}, 1.0, now, 1);
	fixture.input.world.roster[targetA.slot - 1U].team = p::Team::Spectator;
	decision = c::selectTarget(fixture.input);
	assert(decision.reason == c::CombatReason::UnknownRelation);

	fixture.reset();
	addVisual(fixture, 0, actor, {100.0, 0.0, 0.0}, 1.0, now, 1);
	fixture.input.world.roster[actor.slot - 1U] = {};
	decision = c::selectTarget(fixture.input);
	assert(decision.reason == c::CombatReason::UnknownRelation);
}

void testStaleAndAnonymousEvidence()
{
	Fixture fixture;
	addVisual(fixture, 0, targetA, {100.0, 0.0, 0.0}, 0.0, now, 1);
	auto decision = c::selectTarget(fixture.input);
	assert(decision.reason == c::CombatReason::StaleTarget);

	fixture.reset();
	addVisual(fixture, 0, targetA, {100.0, 0.0, 0.0}, 1.0, now + 1, 1);
	decision = c::selectTarget(fixture.input);
	assert(decision.reason == c::CombatReason::StaleTarget);

	fixture.reset();
	addVisual(fixture, 0, targetA, {100.0, 0.0, 0.0}, 1.0, now, 1);
	fixture.visual.memories[0].identity.source = p::ObservationSource::Sound;
	decision = c::selectTarget(fixture.input);
	assert(decision.reason == c::CombatReason::StaleTarget);

	fixture.reset();
	addVisual(fixture, 0, targetA, {100.0, 0.0, 0.0}, 1.0, now, 1);
	fixture.input.map.value = map.value + 1;
	decision = c::selectTarget(fixture.input);
	assert(decision.reason == c::CombatReason::StaleInput);

	fixture.reset();
	addVisual(fixture, 0, targetA, {100.0, 0.0, 0.0}, 1.0, now, 1);
	fixture.input.round.value = round.value + 1;
	decision = c::selectTarget(fixture.input);
	assert(decision.reason == c::CombatReason::StaleInput);

	fixture.reset();
	addVisual(fixture, 0, targetA, {100.0, 0.0, 0.0}, 1.1, now, 1);
	decision = c::selectTarget(fixture.input);
	assert(decision.reason == c::CombatReason::StaleTarget);

	fixture.reset();
	addVisual(fixture, 0, targetA, {100.0, 0.0, 0.0}, (std::numeric_limits<double>::quiet_NaN)(), now, 1);
	decision = c::selectTarget(fixture.input);
	assert(decision.reason == c::CombatReason::StaleTarget);

	fixture.reset();
	addVisual(fixture, 0, targetA, {0.0, 0.0, 0.0}, 1.0, now, 1);
	decision = c::selectTarget(fixture.input);
	assert(decision.reason == c::CombatReason::StaleTarget);

	fixture.reset();
	addReport(fixture, targetA, {100.0, 0.0, 0.0}, 0.6, now, 1);
	decision = c::selectTarget(fixture.input);
	assert(decision.reason == c::CombatReason::StaleTarget);

	fixture.reset();
	fixture.sounds.count = 1;
	decision = c::selectTarget(fixture.input);
	assert(decision.reason == c::CombatReason::AnonymousSound);
	assert(!decision.target.isValid() && !decision.hasAttackInput());

	fixture.reset();
	decision = c::selectTarget(fixture.input);
	assert(decision.reason == c::CombatReason::NoTarget);
}

void testValidationAndDeterminism()
{
	Fixture fixture;
	addVisual(fixture, 0, targetA, {100.0, 0.0, 0.0}, 1.0, now, 1);
	const auto first = c::selectTarget(fixture.input);
	const auto second = c::selectTarget(fixture.input);
	assert(first.action == second.action && first.target == second.target && first.source == second.source &&
		   first.targetAgeMicros == second.targetAgeMicros && first.confidence == second.confidence &&
		   first.reason == second.reason && first.view == second.view && first.buttons == second.buttons);

	fixture.reports.count = fixture.reports.reports.size() + 1;
	const auto invalid = c::selectTarget(fixture.input);
	assert(invalid.reason == c::CombatReason::InvalidWorldSnapshot);
	assert(!invalid.hasAttackInput());

	fixture.reset();
	fixture.reports.stamp.tick.value += 1;
	const auto staleReports = c::selectTarget(fixture.input);
	assert(staleReports.reason == c::CombatReason::StaleInput);
	assert(!staleReports.hasAttackInput());

	fixture.reset();
	fixture.input.alive = false;
	const auto dead = c::selectTarget(fixture.input);
	assert(dead.reason == c::CombatReason::Dead);
	assert(!dead.target.isValid() && !dead.hasAttackInput());
}

void testCombatLockReacquiresAfterTargetGenerationChange()
{
	Fixture fixture;
	c::CombatLock lock{};
	fixture.input.lock = &lock;
	addVisual(fixture, 0, targetA, {100.0, 0.0, 0.0}, 1.0, now, 1);

	const auto first = c::selectTarget(fixture.input);
	assertTrack(first, targetA, p::ObservationSource::Vision);
	assert(lock.target == targetA);
	assert(lock.targetGeneration == targetA.generation);
	assert(lock.acquiredMicros == now);
	assert(lock.lastConfirmedMicros == now);
	assert(lock.generation == 1);

	fixture.input.tick.value += 1;
	fixture.input.timeMicros += 10'000;
	fixture.input.world.stamp.tick = fixture.input.tick;
	fixture.input.world.stamp.timeMicros = fixture.input.timeMicros;
	fixture.visual.stamp = fixture.input.world.stamp;
	fixture.sounds.stamp = fixture.input.world.stamp;
	fixture.input.weapon.tick = fixture.input.tick;
	fixture.input.weapon.observedMicros = fixture.input.timeMicros;
	fixture.visual.count = 0;
	addVisual(fixture, 0, targetA, {100.0, 0.0, 0.0}, 1.0, fixture.input.timeMicros, 2);

	const auto retained = c::selectTarget(fixture.input);
	assert(retained.action == c::CombatAction::Track);
	assert(retained.target == targetA);
	assert(retained.source == p::ObservationSource::Vision);
	assert(retained.reason == c::CombatReason::Accepted);
	assert(retained.validUntilMicros == fixture.input.timeMicros);
	assert(lock.generation == 1);
	assert(lock.acquiredMicros == now);
	assert(lock.lastConfirmedMicros == fixture.input.timeMicros);

	fixture.input.tick.value += 1;
	fixture.input.timeMicros += 10'000;
	fixture.input.world.stamp.tick = fixture.input.tick;
	fixture.input.world.stamp.timeMicros = fixture.input.timeMicros;
	fixture.visual.stamp = fixture.input.world.stamp;
	fixture.sounds.stamp = fixture.input.world.stamp;
	fixture.input.weapon.tick = fixture.input.tick;
	fixture.input.weapon.observedMicros = fixture.input.timeMicros;
	fixture.visual.count = 0;
	const astrabot::core::PlayerId respawnedTarget{targetA.slot, {2}};
	fixture.input.world.roster[targetA.slot - 1U] = {respawnedTarget, p::Team::Terrorist};
	addVisual(fixture, 0, respawnedTarget, {100.0, 0.0, 0.0}, 1.0, fixture.input.timeMicros, 3);

	const auto reacquired = c::selectTarget(fixture.input);
	assert(reacquired.action == c::CombatAction::Track);
	assert(reacquired.target == respawnedTarget);
	assert(reacquired.source == p::ObservationSource::Vision);
	assert(reacquired.reason == c::CombatReason::Accepted);
	assert(reacquired.validUntilMicros == fixture.input.timeMicros);
	assert(lock.target == respawnedTarget);
	assert(lock.targetGeneration == respawnedTarget.generation);
	assert(lock.generation == 2);
	assert(lock.acquiredMicros == fixture.input.timeMicros);
	assert(lock.lastConfirmedMicros == fixture.input.timeMicros);
}

void testVisualFreshnessAndTargetGeneration()
{
	Fixture fixture;

	addVisual(fixture, 0, targetA, {100.0, 0.0, 0.0}, 1.0, now - c::kMaxVisualAgeMicros, 1);
	const auto boundary = c::selectTarget(fixture.input);
	assertTrack(boundary, targetA, p::ObservationSource::Vision);
	const auto boundaryAim = c::aimTarget(fixture.input);
	assert(boundaryAim.action == c::CombatAction::Track);
	const auto boundaryFire = c::authorizeFire(fixture.input, boundaryAim, {});
	assert(boundaryFire.decision.action == c::CombatAction::Fire);

	fixture.reset();
	addVisual(fixture, 0, targetA, {100.0, 0.0, 0.0}, 1.0, now - c::kMaxVisualAgeMicros - 1, 1);
	const auto expired = c::selectTarget(fixture.input);
	assert(expired.action == c::CombatAction::NoOp);
	assert(expired.reason == c::CombatReason::StaleTarget);

	fixture.reset();
	const astrabot::core::PlayerId replacement{targetA.slot, {2}};
	addVisual(fixture, 0, targetA, {100.0, 0.0, 0.0}, 1.0, now, 1);
	addVisual(fixture, 1, targetB, {0.0, 100.0, 0.0}, 1.0, now, 2);
	fixture.input.world.roster[targetA.slot - 1U] = {replacement, p::Team::Terrorist};
	const auto afterReplacement = c::selectTarget(fixture.input);
	assertTrack(afterReplacement, targetB, p::ObservationSource::Vision);
}

} // namespace

int main()
{
	testDirectVisionAndReportFallback();
	testDeterministicOrdering();
	testRelationsFailClosed();
	testStaleAndAnonymousEvidence();
	testValidationAndDeterminism();
	testCombatLockReacquiresAfterTargetGenerationChange();
	testVisualFreshnessAndTargetGeneration();
	return 0;
}
