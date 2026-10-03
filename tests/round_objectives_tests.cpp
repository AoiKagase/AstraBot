#include "astrabot/objectives/round_objectives.hpp"
#include "astrabot/objectives/bomb_site_route_selection.hpp"
#include <limits>
#include "astrabot/compat/random_source.hpp"

#include "astrabot/perception/perception.hpp"

#include <cstdio>

namespace
{
	bool check(bool condition, const char *description)
	{
		if (condition)
		{
			return true;
		}

		std::fprintf(stderr, "check failed: %s\n", description);
		return false;
	}

	astrabot::objectives::ScenarioIdentity identity(
		astrabot::objectives::ScenarioKind kind,
		astrabot::objectives::TeamRole team,
		std::uint32_t roundGeneration,
		std::uint32_t tick)
	{
		astrabot::objectives::ScenarioIdentity value = {};
		value.frame = {3U, roundGeneration, tick};
		value.scenarioGeneration = 1U;
		value.kind = kind;
		value.team = team;
		return value;
	}

	astrabot::world::WorldSnapshot snapshot(
		const astrabot::objectives::ScenarioIdentity &scenario)
	{
		astrabot::world::SnapshotIdentity snapshotIdentity = {};
		snapshotIdentity.frame = scenario.frame;
		snapshotIdentity.observer = {1U, 5U};
		astrabot::perception::PerceptionInput input(snapshotIdentity);
		astrabot::perception::PerceptionAssembler assembler;
		astrabot::world::WorldSnapshot value;
		assembler.publish(input, &value);
		return value;
	}

	astrabot::objectives::ScenarioObservation observation(
		const astrabot::objectives::ScenarioIdentity &scenario)
	{
		astrabot::objectives::ScenarioObservation value = {};
		value.scenario = scenario;
		value.phase = astrabot::objectives::RoundPhase::Live;
		value.buyAvailability = astrabot::objectives::Availability::Unavailable;
		value.eventCount = 0U;
		return value;
	}

	astrabot::objectives::ScenarioEvent event(
		astrabot::objectives::ScenarioEventKind kind,
		const astrabot::objectives::ScenarioIdentity &scenario)
	{
		astrabot::objectives::ScenarioEvent value = {};
		value.id = scenario.frame.tick;
		value.frame = scenario.frame;
		value.kind = kind;
		value.state = astrabot::objectives::EventState::Observed;
		value.actor = {2U, 9U};
		value.team = astrabot::objectives::TeamRole::Terrorist;
		return value;
	}
}

bool testBombHostageAndBuyProposals()
{
	using astrabot::behavior::BehaviorState;
	using astrabot::objectives::ObjectiveKind;
	using astrabot::objectives::ObjectiveResult;
	using astrabot::objectives::RoundObjectivePlanner;

	const auto bomb = identity(
		astrabot::objectives::ScenarioKind::Bomb,
		astrabot::objectives::TeamRole::Terrorist,
		4U,
		10U);
	RoundObjectivePlanner planner({1U, 5U});
	astrabot::objectives::ObjectiveProposal proposal = {};
	if (!check(planner.plan(
			snapshot(bomb),
			BehaviorState::Roam,
			observation(bomb),
			nullptr,
			&proposal) == ObjectiveResult::Proposed &&
			proposal.objective.kind == ObjectiveKind::Attack,
			"terrorist live bomb round proposes attack"))
	{
		return false;
	}

	auto planted = observation(identity(
		astrabot::objectives::ScenarioKind::Bomb,
		astrabot::objectives::TeamRole::CounterTerrorist,
		4U,
		11U));
	planted.eventCount = 1U;
	planted.events[0] = event(
		astrabot::objectives::ScenarioEventKind::BombPlanted,
		planted.scenario);
	if (!check(planner.plan(
			snapshot(planted.scenario),
			BehaviorState::Roam,
			planted,
			nullptr,
			&proposal) == ObjectiveResult::Proposed &&
			proposal.objective.kind == ObjectiveKind::Defuse,
			"counter-terrorist observes planted bomb and proposes defuse"))
	{
		return false;
	}

	auto hostage = observation(identity(
		astrabot::objectives::ScenarioKind::Hostage,
		astrabot::objectives::TeamRole::CounterTerrorist,
		4U,
		12U));
	hostage.eventCount = 1U;
	hostage.events[0] = event(
		astrabot::objectives::ScenarioEventKind::HostageLocated,
		hostage.scenario);
	if (!check(planner.plan(
			snapshot(hostage.scenario),
			BehaviorState::Roam,
			hostage,
			nullptr,
			&proposal) == ObjectiveResult::Proposed &&
			proposal.objective.kind == ObjectiveKind::Rescue,
			"hostage scenario proposes rescue"))
	{
		return false;
	}

	auto buy = observation(identity(
		astrabot::objectives::ScenarioKind::Bomb,
		astrabot::objectives::TeamRole::Terrorist,
		4U,
		13U));
	buy.phase = astrabot::objectives::RoundPhase::Freeze;
	buy.buyAvailability = astrabot::objectives::Availability::Available;
	return check(planner.plan(
			snapshot(buy.scenario),
			BehaviorState::Roam,
			buy,
			nullptr,
			&proposal) == ObjectiveResult::Proposed &&
			proposal.objective.kind == ObjectiveKind::Buy,
		"available freeze-period buy state proposes buy objective");
}

bool testUnknownFeedbackAndRoundStaleness()
{
	using astrabot::behavior::BehaviorState;
	using astrabot::objectives::ObjectiveResult;
	using astrabot::objectives::RoundObjectivePlanner;

	const auto unknown = identity(
		astrabot::objectives::ScenarioKind::Unknown,
		astrabot::objectives::TeamRole::Unknown,
		4U,
		20U);
	RoundObjectivePlanner planner({1U, 5U});
	astrabot::objectives::ObjectiveProposal proposal = {};
	if (!check(planner.plan(
			snapshot(unknown),
			BehaviorState::Roam,
			observation(unknown),
			nullptr,
			&proposal) == ObjectiveResult::RecoveryPending &&
			!proposal.isProposal(),
			"unknown scenario enters recovery without omniscient objective"))
	{
		return false;
	}

	auto stale = observation(identity(
		astrabot::objectives::ScenarioKind::Bomb,
		astrabot::objectives::TeamRole::Terrorist,
		3U,
		21U));
	return check(planner.plan(
			snapshot(stale.scenario),
			BehaviorState::Roam,
			stale,
			nullptr,
			&proposal) == ObjectiveResult::StaleFrame,
		"stale round generation cannot reuse objective planner state");
}

namespace
{
class SeedRandom : public astrabot::compat::ICompatibilityRandomSource
{
  public:
	explicit SeedRandom(std::uint32_t seed) : state(seed)
	{
	}
	astrabot::compat::RandomFloatResult nextFloat(const astrabot::compat::RandomRequest &r) override
	{
		++calls;
		state = state * 1664525U + 1013904223U;
		return {astrabot::compat::RandomStatus::Ok,
				r.floatLower + (r.floatUpper - r.floatLower) * static_cast<float>(state >> 8U) / 16777216.0f};
	}
	astrabot::compat::RandomLongResult nextLong(const astrabot::compat::RandomRequest &r) override
	{
		++calls;
		state = state * 1664525U + 1013904223U;
		return {astrabot::compat::RandomStatus::Ok,
				r.longLower + static_cast<std::int32_t>((state >> 8U) %
														static_cast<std::uint32_t>(r.longUpper - r.longLower + 1))};
	}
	std::uint32_t state;
	unsigned calls = 0U;
};
} // namespace

bool testBombApproachVarietyAndPersistence()
{
	using namespace astrabot::objectives;
	bool sawA = false, sawB = false;
	for (std::uint32_t seed = 1U; seed <= 32U; ++seed)
	{
		SeedRandom random(seed), replay(seed);
		BombApproachState state{}, same{};
		const astrabot::compat::RandomActor actor = {2U, 1U};
		const astrabot::compat::RandomTimingContext timing = {0U, 0U, 1U};
		beginBombApproach(&state, 1U, seed, actor, 100.0f, 100.0f, 120.0f, 20.0f, &random, timing);
		beginBombApproach(&same, 1U, seed, actor, 100.0f, 100.0f, 120.0f, 20.0f, &replay, timing);
		const float costs[] = {4304.7f, 5032.3f};
		const std::size_t selected = chooseBombApproachSite(costs, 2U, &random, actor, timing);
		const std::size_t repeated = chooseBombApproachSite(costs, 2U, &replay, actor, timing);
		sawB = sawB || selected == 0U;
		sawA = sawA || selected == 1U;
		if (!check(selected == repeated && state.plantAt == same.plantAt,
				   "same seed reproduces approach site and plant timer") ||
			!check(state.plantAt >= 110.0f && state.plantAt <= 130.0f,
				   "plant decision starts after bounded preplant travel"))
			return false;
		const float deadline = state.plantAt;
		const unsigned calls = random.calls;
		for (unsigned frame = 2U; frame < 20U; ++frame)
		{
			beginBombApproach(&state, 1U, seed, actor, 101.0f, 100.0f, 119.0f, 20.0f, &random, {0U, 0U, frame});
			if (!check(state.plantAt == deadline && random.calls == calls,
					   "same carrier round does not restart timer or resample per update") ||
				!check(bombApproachPhase(&state, 101.0f, 119.0f, 20.0f, true, false, false) ==
						   BombApproachPhase::Approach,
					   "approach stays active before deadline"))
				return false;
		}
		if (!check(bombApproachPhase(&state, deadline, 90.0f, 20.0f, true, false, false) == BombApproachPhase::Plant,
				   "deadline resumes nearest-site planting") ||
			!check(bombApproachPhase(&state, deadline - 1.0f, 90.0f, 20.0f, true, false, false) ==
					   BombApproachPhase::Plant,
				   "finished approach cannot switch back"))
			return false;
	}
	return check(sawA && sawB, "multiple seeds and rounds can approach either reachable site despite B being cheaper");
}

bool testBombApproachFallbackAndHumanPlant()
{
	using namespace astrabot::objectives;
	SeedRandom random(7U);
	const astrabot::compat::RandomActor actor = {2U, 1U};
	const astrabot::compat::RandomTimingContext timing = {0U, 0U, 1U};
	const float costs[] = {-1.0f, 5032.3f, std::numeric_limits<float>::infinity()};
	if (!check(chooseBombApproachSite(costs, 3U, &random, actor, timing) == 1U,
			   "unreachable sites cannot become approach destinations") ||
		!check(chooseBombApproachSite(costs, 3U, nullptr, actor, timing) == 1U,
			   "missing RNG falls back to reachable cheapest site"))
		return false;
	BombApproachState state{};
	beginBombApproach(&state, 1U, 1U, actor, 100.0f, 100.0f, 25.0f, 20.0f, &random, timing);
	if (!check(state.finished && state.plantAt == 100.0f,
			   "urgent round reserves travel plus plant margin instead of delaying"))
		return false;
	beginBombApproach(&state, 1U, 2U, actor, 100.0f, 100.0f, 120.0f, 20.0f, &random, timing);
	if (!check(bombApproachPhase(&state, 101.0f, 25.0f, 20.0f, true, false, false) == BombApproachPhase::Plant,
			   "urgency interrupts approach before plant reserve is lost"))
		return false;
	beginBombApproach(&state, 1U, 3U, actor, 100.0f, 100.0f, 120.0f, 20.0f, &random, timing);
	if (!check(bombApproachPhase(&state, 101.0f, 119.0f, 20.0f, true, true, false) == BombApproachPhase::Cancel,
			   "human planted C4 cancels old carrier approach immediately"))
		return false;
	beginBombApproach(&state, 1U, 3U, {3U, 1U}, 102.0f, 100.0f, 118.0f, 20.0f, &random, timing);
	if (!check(!state.finished && state.actorSlot == 3U, "actual carrier change starts a fresh approach") ||
		!check(bombApproachPhase(&state, 103.0f, 117.0f, 20.0f, false, false, false) == BombApproachPhase::Cancel,
			   "dropped C4 releases approach"))
		return false;
	beginBombApproach(&state, 1U, 4U, actor, 100.0f, 100.0f, 120.0f, 20.0f, nullptr, timing);
	if (!check(state.finished, "missing RNG uses immediate nearest-site fallback"))
		return false;
	beginBombApproach(&state, 1U, 5U, actor, 140.0f, 100.0f, 120.0f, 20.0f, &random, timing);
	if (!check(state.finished, "late bomb pickup does not start another 10 to 30 second wait"))
		return false;
	beginBombApproach(&state, 1U, 6U, actor, 100.0f, 100.0f, 120.0f, 20.0f, &random, timing);
	return check(bombApproachPhase(&state, 101.0f, 119.0f, 20.0f, true, false, true) == BombApproachPhase::Plant,
				 "arrival in a plant site permits early planting");
}

int main()
{
	if (!testBombApproachVarietyAndPersistence() || !testBombApproachFallbackAndHumanPlant() ||
		!testBombHostageAndBuyProposals() || !testUnknownFeedbackAndRoundStaleness())
	{
		return 1;
	}

	return 0;
}
