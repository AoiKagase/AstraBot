#include "astrabot/compat/random_source.hpp"
#include "astrabot/metamod/abi_contract.hpp"
#include "observation_adapter.hpp"

#include <cstdio>
#include <vector>

namespace
{
bool check(bool condition, const char *message)
{
	if (!condition)
	{
		std::fprintf(stderr, "FAIL: %s\n", message);
	}
	return condition;
}

struct TraceCollector : public astrabot::compat::IObservationTraceSink
{
	void record(const astrabot::compat::ObservationTraceRecord &value) override
	{
		records.push_back(value);
	}

	std::vector<astrabot::compat::ObservationTraceRecord> records;
};

void configureEntity(edict_t *entity)
{
	entity->free = 0;
	entity->v.health = 100.0f;
	entity->v.team = 1;
	entity->v.fov = 90.0f;
	entity->v.origin[0] = 128.0f;
	entity->v.origin[1] = 64.0f;
	entity->v.origin[2] = 16.0f;
}

bool collect(
	astrabot::metamod::ObservationAdapter *adapter,
	edict_t *entity,
	const astrabot::world::ActorKey &actor)
{
	astrabot::compat::CompatibilityObservation observation = {};
	return adapter->collectActor(
		entity, actor, {3U, 7U, 44U}, {18U, 0U, 0U}, &observation) ==
		astrabot::metamod::ObservationAdapterResult::Accepted;
}

bool testTraceSequenceAndActorMetadata()
{
	enginefuncs_t engine{};
	globalvars_t globals{};
	edict_t entity{};
	configureEntity(&entity);
	astrabot::metamod::ObservationAdapter compatibility;
	compatibility.configure(&engine, &globals);
	TraceCollector trace;
	compatibility.setTraceSink(&trace);
	if (!check(collect(&compatibility, &entity, {1U, 1U}),
		"compatibility observation is collected"))
	{
		return false;
	}
	const std::size_t firstCount = trace.records.size();
	entity.v.team = 2;
	if (!check(collect(&compatibility, &entity, {2U, 1U}),
		"second actor observation is collected"))
	{
		return false;
	}
	return check(firstCount > 0U, "first observation emits bounded trace") &&
		check(trace.records[0].sequence == 1U,
			"trace sequence starts at one") &&
		check(trace.records[firstCount].sequence == firstCount + 1U,
			"second observation continues sequence") &&
		check(trace.records[0].context.actor == astrabot::world::ActorKey{1U, 1U},
			"first actor metadata is retained") &&
		check(trace.records[firstCount].context.actor == astrabot::world::ActorKey{2U, 1U},
			"second actor metadata is isolated") &&
		check(trace.records[0].context.timing.commandSequence == 18U,
			"timing metadata is retained");
}

bool testEnhancedObservationAndRngAreIsolated()
{
	enginefuncs_t engine{};
	globalvars_t globals{};
	edict_t entity{};
	configureEntity(&entity);
	astrabot::metamod::ObservationAdapter enhanced;
	enhanced.configure(&engine, &globals);
	TraceCollector enhancedTrace;
	enhanced.setTraceSink(&enhancedTrace);
	if (!check(collect(&enhanced, &entity, {9U, 1U}),
		"enhanced observation is collected"))
	{
		return false;
	}
	const astrabot::compat::RandomActor actor = {1U, 1U};
	const astrabot::compat::RandomTimingContext timing = {1U, 1U, 1U};
	astrabot::compat::ScriptedRandomSource compatibility({
		astrabot::compat::RandomTapeEntry::floatEntry(
			"RNG-COMPAT", actor, timing, 0.0f, 1.0f, 0.25f)});
	astrabot::compat::ScriptedRandomSource enhancedRng({
		astrabot::compat::RandomTapeEntry::floatEntry(
			"RNG-ENHANCED", actor, timing, 0.0f, 1.0f, 0.75f)});
	enhancedRng.nextFloat(astrabot::compat::RandomRequest::floatRequest(
		"RNG-ENHANCED", actor, timing, 0.0f, 1.0f));
	return check(!enhancedTrace.records.empty(),
			"enhanced trace is present") &&
		check(enhancedTrace.records[0].sequence == 1U,
			"enhanced adapter owns an independent trace sequence") &&
		check(compatibility.position() == 0U,
			"observation collection does not consume compatibility RNG") &&
		check(enhancedRng.position() == 1U,
			"enhanced RNG remains independently consumable");
}
}

int main()
{
	return testTraceSequenceAndActorMetadata() &&
		testEnhancedObservationAndRngAreIsolated() ? 0 : 1;
}
