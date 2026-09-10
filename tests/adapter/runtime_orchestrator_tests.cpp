// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include "adapter/metamod/runtime_orchestrator.hpp"

#include <cassert>
#include <memory>

namespace {

namespace a = astrabot::adapter::metamod;
namespace c = astrabot::core;
namespace p = astrabot::core::perception;
namespace w = astrabot::core::world;

struct Fixture final {
	c::PlayerId player{1, {1}};
	c::BotAgentId agent{1};
	c::MapGeneration map{1};
	p::RoundGeneration round{1};
	c::TickId tick{1};
	std::uint64_t now{1'000'000};
	w::MemorySnapshot visual{};
	w::SoundSnapshot sounds{};
	w::WorldSnapshot world{};
	a::RuntimeFrame frame{};
	a::RuntimeActorInput input{};

	Fixture() { refresh(); }

	void refresh() {
		frame = {map, round, tick, now, 16'000, {}};
		const p::Stamp stamp{agent, player, map, tick, now, round};
		visual = {};
		sounds = {};
		visual.stamp = stamp;
		sounds.stamp = stamp;
		world = {};
		world.stamp = stamp;
		world.visual = &visual;
		world.sounds = &sounds;
		world.roster[player.slot - 1U] = {player, p::Team::CounterTerrorist};

		input = {};
		input.player = player;
		input.agent = agent;
		input.primary = true;
		input.world = world;
		input.team.map = map;
		input.team.round = round;
		input.team.tick = tick;
		input.team.nowMicros = now;
		input.team.team = p::Team::CounterTerrorist;
		input.team.objective.known = true;
		input.team.objective.phase = c::team::ObjectivePhase::None;
		input.team.objective.family = c::team::ObjectiveFamily::BombDefusal;
		input.team.objective.state = c::team::ObjectiveState::Active;
		input.team.memberCount = 1;
		input.team.members[0].player = player;
		input.team.members[0].agent = agent;
		input.team.members[0].position = {0.0, 0.0, 0.0};
		input.team.members[0].healthPercent = 100.0F;
		input.team.members[0].connected = true;
		input.team.members[0].alive = true;
		input.team.members[0].team = p::Team::CounterTerrorist;

		input.tactical.self.player = player;
		input.tactical.self.agent = agent;
		input.tactical.self.team = p::Team::CounterTerrorist;
		input.tactical.self.position = {0.0, 0.0, 0.0};
		input.tactical.self.currentArea = {1};
		input.tactical.self.healthPercent = 100.0F;
		input.tactical.self.alive = true;
		input.tactical.objective.kind = c::tactical::ObjectiveKind::None;
		input.tactical.objective.bomb = c::tactical::BombState::None;
		input.tactical.economy.weaponValue = 20;

		input.action.map = map;
		input.action.round = round;
		input.action.tick = tick;
		input.action.nowMicros = now;
		input.action.player = player;
		input.action.agent = agent;
		input.action.alive = true;
		input.action.healthPercent = 100.0F;
		input.action.currentArea = {1};
		input.action.currentPosition = {0.0, 0.0, 0.0};
		input.action.enemy.confidence = 0.0;
		input.action.weapon.active = {5};
		input.action.weapon.activeClass = c::combat::WeaponSnapshot::WeaponClass::Rifle;
		input.action.weapon.clipAmmo = 12;
		input.action.weapon.reserveAmmo = 48;
		input.action.weapon.canReload = true;
		input.action.objective.kind = c::action::ObjectiveKind::None;
		input.action.objective.bomb = c::action::BombState::None;

		input.combat.map = map;
		input.combat.round = round;
		input.combat.tick = tick;
		input.combat.timeMicros = now;
		input.combat.player = player;
		input.combat.agent = agent;
		input.combat.alive = true;
		input.combat.team = p::Team::CounterTerrorist;
		input.combat.eye = {0.0, 0.0, 36.0};
		input.combat.view = {0.0F, 0.0F, 0.0F};
		input.combat.world = world;
		input.combat.weapon.map = map;
		input.combat.weapon.round = round;
		input.combat.weapon.tick = tick;
		input.combat.weapon.observedMicros = now;
		input.combat.weapon.active = {5};
		input.combat.weapon.activeClass = c::combat::WeaponSnapshot::WeaponClass::Rifle;
		input.combat.weapon.owned[0] = {5};
		input.combat.weapon.ownedCount = 1;
		input.combat.weapon.clipAmmo = 12;
		input.combat.weapon.reserveAmmo = 48;

	input.opponent = c::learning::OpponentObservation{player, map, round, tick, 1,
		    c::combat::WeaponSnapshot::WeaponClass::Rifle, 0.5, 0.2, 0.1, 0.3};
	}

	void advance(std::uint64_t delta = 50'000) {
		++tick.value;
		now += delta;
		refresh();
	}
};

void testOrderedCadenceAndOneShotCombat() {
	Fixture fixture;
	assert(fixture.input.valid(fixture.frame));
	a::RuntimeOrchestrator orchestrator;
	const auto &first = orchestrator.run(fixture.frame, &fixture.input, 1);
	assert(first.accepted);
	assert(first.stageCount == 7);
	assert(first.stageTrace[0] == a::RuntimeStage::PerceptionPublished);
	assert(first.stageTrace[1] == a::RuntimeStage::ExperienceUpdated);
	assert(first.stageTrace[2] == a::RuntimeStage::TeamDirector);
	assert(first.stageTrace[3] == a::RuntimeStage::TacticalPlanner);
	assert(first.stageTrace[4] == a::RuntimeStage::ActionPlanner);
	assert(first.stageTrace[5] == a::RuntimeStage::Combat);
	assert(first.stageTrace[6] == a::RuntimeStage::Navigation);
	assert(first.executableCount == 1);
	assert(orchestrator.takeCombatDecision(fixture.player, fixture.agent, fixture.map,
	                                       fixture.round, fixture.tick));
	assert(!orchestrator.takeCombatDecision(fixture.player, fixture.agent, fixture.map,
	                                        fixture.round, fixture.tick));

	fixture.advance();
	const auto &second = orchestrator.run(fixture.frame, &fixture.input, 1);
	assert(second.accepted);
	assert(!second.decisions[0].tacticalExecuted);
	assert(!second.decisions[0].actionExecuted);

	fixture.advance(100'000);
	const auto &third = orchestrator.run(fixture.frame, &fixture.input, 1);
	assert(third.decisions[0].actionExecuted);
}

void testInvalidAndGenerationReset() {
	Fixture fixture;
	auto partialIdentity = fixture.frame;
	partialIdentity.mapIdentity.hasBspHash = true;
	assert(!partialIdentity.valid());

	a::RuntimeOrchestrator orchestrator;
	assert(orchestrator.run(fixture.frame, &fixture.input, 1).accepted);

	fixture.advance();
	++fixture.player.generation.value;
	++fixture.agent.value;
	fixture.refresh();
	const auto &replaced = orchestrator.run(fixture.frame, &fixture.input, 1);
	assert(replaced.accepted);
	assert(replaced.decisions[0].tacticalExecuted);
	assert(replaced.decisions[0].actionExecuted);

	// A frame stamp is single-use. Replaying it must not leave the previous
	// combat value available for a later navigation pass.
	const auto &sameTick = orchestrator.run(fixture.frame, &fixture.input, 1);
	assert(!sameTick.accepted);
	assert(!orchestrator.takeCombatDecision(fixture.player, fixture.agent, fixture.map,
	                                        fixture.round, fixture.tick));

	fixture.advance();
	fixture.input.action.tick.value = 99;
	const auto &invalid = orchestrator.run(fixture.frame, &fixture.input, 1);
	assert(invalid.accepted);
	assert(invalid.executableCount == 0);
	assert(invalid.decisions[0].rejection == a::RuntimeRejectReason::InvalidActorInput);
	assert(!orchestrator.takeCombatDecision(fixture.player, fixture.agent, fixture.map,
	                                        fixture.round, fixture.tick));

	auto invalidFrame = fixture.frame;
	++invalidFrame.tick.value;
	invalidFrame.mapIdentity.bspBytes = 1U;
	const auto &invalidFrameResult = orchestrator.run(invalidFrame, &fixture.input, 1);
	assert(!invalidFrameResult.accepted);
	assert(orchestrator.diagnostics().entries[orchestrator.diagnostics().count - 1U].reason ==
	       a::RuntimeRejectReason::InvalidFrame);

	fixture.map = {2};
	fixture.tick = {1};
	fixture.now = 10;
	fixture.refresh();
	const auto &nextMap = orchestrator.run(fixture.frame, &fixture.input, 1);
	assert(nextMap.accepted);
	assert(orchestrator.contextualDanger().map() == fixture.map);
	assert(orchestrator.opponentProfiles().map() == fixture.map);
}

void testDuplicateActorAndAgentAreRejected() {
	Fixture fixture;
	a::RuntimeOrchestrator orchestrator;
	std::array<a::RuntimeActorInput, 2> inputs{fixture.input, fixture.input};
	inputs[1].player = {2, {1}};
	const auto &result = orchestrator.run(fixture.frame, inputs.data(), inputs.size());
	assert(result.accepted);
	assert(result.decisionCount == 2);
	assert(result.decisions[0].rejection == a::RuntimeRejectReason::DuplicateActor);
	assert(result.decisions[1].player == fixture.player);
	assert(orchestrator.diagnostics().count != 0);
	assert(orchestrator.diagnostics().entries[0].reason == a::RuntimeRejectReason::DuplicateActor);
}

void testAllValidatedActorsExecute() {
    Fixture fixture;
    a::RuntimeActorInput secondary = fixture.input;
    secondary.player = {2, {1}};
    secondary.agent = {2};
    secondary.world.stamp.actor = secondary.agent;
    secondary.world.stamp.player = secondary.player;
    secondary.world.roster[1] = {secondary.player, p::Team::CounterTerrorist};
    secondary.team.members[0].player = secondary.player;
    secondary.team.members[0].agent = secondary.agent;
    secondary.tactical.self.player = secondary.player;
    secondary.tactical.self.agent = secondary.agent;
    secondary.action.player = secondary.player;
    secondary.action.agent = secondary.agent;
    secondary.combat.player = secondary.player;
    secondary.combat.agent = secondary.agent;
    secondary.combat.world = secondary.world;
    secondary.opponent = c::learning::OpponentObservation{
        secondary.player, fixture.map, fixture.round, fixture.tick, 2,
        c::combat::WeaponSnapshot::WeaponClass::Rifle, 0.5, 0.2, 0.1, 0.3};

    const std::array<a::RuntimeActorInput, 2> inputs{fixture.input, secondary};
    a::RuntimeOrchestrator orchestrator;
    const auto &result = orchestrator.run(fixture.frame, inputs.data(), inputs.size());
    assert(result.accepted);
    assert(result.executableCount == 2);
    assert(result.decisionCount == 2);
    assert(result.acceptedActorCount == 2);
    assert(result.nonPrimaryRejectedCount == 0);
    assert(result.decisions[0].rejection == a::RuntimeRejectReason::None);
    assert(result.decisions[1].rejection == a::RuntimeRejectReason::None);
    assert(orchestrator.takeCombatDecision(fixture.player, fixture.agent,
                                           fixture.map, fixture.round, fixture.tick));
    assert(orchestrator.takeCombatDecision(secondary.player, secondary.agent,
                                           fixture.map, fixture.round, fixture.tick));

    fixture.input.primary = false;
    a::RuntimeOrchestrator compatibility;
    const auto &legacy = compatibility.run(fixture.frame, &fixture.input, 1);
    assert(legacy.executableCount == 1);
    assert(legacy.nonPrimaryRejectedCount == 0);
}

void testDeterministicReplayAndDisconnect() {
	Fixture fixture;
	const auto left = std::make_unique<a::RuntimeOrchestrator>();
	const auto right = std::make_unique<a::RuntimeOrchestrator>();
	const auto &l = left->run(fixture.frame, &fixture.input, 1);
	const auto &r = right->run(fixture.frame, &fixture.input, 1);
	assert(l.decisions[0].tactical.intent.type == r.decisions[0].tactical.intent.type);
	assert(l.decisions[0].action.intent.action == r.decisions[0].action.intent.action);
	assert(l.decisions[0].combat.action == r.decisions[0].combat.action);
	left->onDisconnect(fixture.player);
    assert(!left->takeCombatDecision(fixture.player, fixture.agent, fixture.map, fixture.round,
                                     fixture.tick));
}

} // namespace

void testTransientActorEventsRetainAndRetireProfiles() {
    Fixture fixture;
    a::RuntimeOrchestrator orchestrator;
    assert(orchestrator.run(fixture.frame, &fixture.input, 1).accepted);
    assert(orchestrator.opponentProfiles().find(fixture.player) != nullptr);
    orchestrator.onDeath(fixture.player);
    assert(orchestrator.opponentProfiles().find(fixture.player) != nullptr);
    fixture.advance();
    orchestrator.onInputUnavailable(fixture.player);
    assert(orchestrator.opponentProfiles().find(fixture.player) != nullptr);
    ++fixture.round.value;
    fixture.refresh();
    assert(orchestrator.run(fixture.frame, &fixture.input, 1).accepted);
    assert(orchestrator.opponentProfiles().find(fixture.player) != nullptr);
    const auto retired = fixture.player;
    fixture.advance();
    fixture.player = {retired.slot, {retired.generation.value + 1U}};
    fixture.agent = {retired.generation.value + 1U};
    fixture.refresh();
    assert(orchestrator.run(fixture.frame, &fixture.input, 1).accepted);
    assert(orchestrator.opponentProfiles().find(fixture.player) != nullptr);
    orchestrator.onDeath(retired);
    orchestrator.onInputUnavailable(retired);
    orchestrator.onDisconnect(retired);
    assert(orchestrator.opponentProfiles().find(fixture.player) != nullptr);
    orchestrator.onDisconnect(fixture.player);
    assert(orchestrator.opponentProfiles().find(fixture.player) == nullptr);
}

int main() {
	{
		auto fixtureStorage = std::make_unique<Fixture>();
		auto& fixture = *fixtureStorage;
		auto runtime = std::make_unique<a::RuntimeOrchestrator>();
		assert(runtime->run(fixture.frame, &fixture.input, 1).decisions[0].teamExecuted);
		fixture.advance(1'000);
		fixture.input.teamObjectiveAvailable = false;
		fixture.input.team.objective.known = false;
        const auto& neutral = runtime->run(fixture.frame, &fixture.input, 1);
        assert(neutral.executableCount == 1 && !neutral.decisions[0].teamExecuted);
        assert(neutral.decisions[0].team.strategy == c::team::Strategy::None);
        assert(neutral.decisions[0].team.shared.map == fixture.map);
        assert(neutral.decisions[0].team.shared.round == fixture.round);
        assert(neutral.decisions[0].team.shared.tick == fixture.tick);
        assert(neutral.decisions[0].team.shared.nowMicros == fixture.now);
        assert(neutral.decisions[0].player == fixture.player);
        assert(neutral.decisions[0].agent == fixture.agent);
        assert(!neutral.decisions[0].team.shared.objective.known);
        fixture.advance(1'000);
        assert(runtime->run(fixture.frame, &fixture.input, 1).decisions[0].teamExecuted);
	}
	testOrderedCadenceAndOneShotCombat();
	testInvalidAndGenerationReset();
	testDuplicateActorAndAgentAreRejected();
    testAllValidatedActorsExecute();
    testDeterministicReplayAndDisconnect();
    testTransientActorEventsRetainAndRetireProfiles();
}
