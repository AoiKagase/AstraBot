#include "astrabot/compat/observation.hpp"
#include "astrabot/metamod/abi_contract.hpp"
#include "observation_adapter.hpp"

#include <cstdio>

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

struct Fixture
{
	Fixture() : engine{}, globals{}, entity{}, adapter() {}

	enginefuncs_t engine;
	globalvars_t globals;
	edict_t entity;
	astrabot::metamod::ObservationAdapter adapter;
};

astrabot::world::FrameIdentity frame()
{
	return {3U, 7U, 44U};
}

astrabot::compat::ObservationTimingContext timing()
{
	return {18U, 0U, 0U};
}

void configureFixture(Fixture *fixture)
{
	fixture->adapter.configure(&fixture->engine, &fixture->globals);
	fixture->entity.free = 0;
	fixture->entity.v.health = 87.0f;
	fixture->entity.v.armorvalue = 50.0f;
	fixture->entity.v.team = 2;
	fixture->entity.v.deadflag = DEAD_NO;
	fixture->entity.v.origin[0] = 128.0f;
	fixture->entity.v.origin[1] = 64.0f;
	fixture->entity.v.origin[2] = 32.0f;
	fixture->entity.v.velocity[0] = 12.0f;
	fixture->entity.v.v_angle[1] = 90.0f;
	fixture->entity.v.fov = 110.0f;
	fixture->entity.v.flags = FL_ONGROUND;
	fixture->entity.v.maxspeed = 250.0f;
	fixture->entity.v.button = 1;
	fixture->entity.v.oldbuttons = 0;
	fixture->entity.v.solid = SOLID_SLIDEBOX;
	fixture->entity.v.movetype = MOVETYPE_WALK;
	fixture->entity.v.weapons = (1 << 6);
}

bool collect(Fixture *fixture, astrabot::compat::CompatibilityObservation *observation)
{
	return fixture->adapter.collectActor(
		&fixture->entity, {4U, 9U}, frame(), timing(), observation) ==
		astrabot::metamod::ObservationAdapterResult::Accepted;
}

bool testPublicPlayerFieldsAreExactAtCollectionTick()
{
	Fixture fixture;
	configureFixture(&fixture);
	astrabot::compat::CompatibilityObservation observation = {};
	if (!check(collect(&fixture, &observation), "public actor observation is accepted"))
	{
		return false;
	}
	return check(observation.player.health.value == 87.0f,
			"public health is copied") &&
		check(observation.player.armor.value == 50.0f,
			"public armor is copied") &&
		check(observation.player.fov.value == 110.0f,
			"public FOV is copied") &&
		check(observation.player.health.context.quality ==
			astrabot::compat::ObservationQuality::ExactEngineApi,
			"public player quality is explicit") &&
		check(observation.player.health.context.freshness ==
			astrabot::compat::ObservationFreshness::SameTick,
			"public player freshness is same tick");
}

bool testPrivateWeaponFieldsAreUnavailableNotSyntheticExact()
{
	Fixture fixture;
	configureFixture(&fixture);
	astrabot::compat::CompatibilityObservation observation = {};
	if (!check(collect(&fixture, &observation), "weapon fixture is accepted"))
	{
		return false;
	}
	return check(!observation.weapon.activeWeaponId.valid,
			"active private weapon is invalid") &&
		check(!observation.weapon.activeWeaponId.isAvailable(),
			"active private weapon is unavailable") &&
		check(observation.weapon.accuracy.context.quality ==
			astrabot::compat::ObservationQuality::Unavailable,
			"accuracy is not promoted to exact");
}

bool testObjectiveProxyKeepsInferredQuality()
{
	Fixture fixture;
	configureFixture(&fixture);
	astrabot::compat::CompatibilityObservation observation = {};
	if (!check(collect(&fixture, &observation), "objective fixture is accepted"))
	{
		return false;
	}
	return check(observation.objective.carryingC4.value,
			"public C4 bit is observed") &&
		check(observation.objective.carryingC4.context.quality ==
			astrabot::compat::ObservationQuality::Inferred,
			"C4 bit remains inferred");
}

bool testAdapterPreservesActorAndFrameIdentity()
{
	Fixture fixture;
	configureFixture(&fixture);
	astrabot::compat::CompatibilityObservation observation = {};
	if (!check(collect(&fixture, &observation), "identity fixture is accepted"))
	{
		return false;
	}
	return check(observation.actor == astrabot::world::ActorKey{4U, 9U},
			"actor identity is preserved") &&
		check(observation.frame == frame(), "frame identity is preserved") &&
		check(observation.player.health.context.timing.commandSequence == 18U,
			"timing context is preserved");
}

bool testAdapterHandlesMissingEnginePointers()
{
	astrabot::metamod::ObservationAdapter adapter;
	edict_t entity = {};
	astrabot::compat::CompatibilityObservation observation = {};
	const astrabot::metamod::ObservationAdapterResult result = adapter.collectActor(
		&entity, {4U, 9U}, frame(), timing(), &observation);
	return check(result == astrabot::metamod::ObservationAdapterResult::EngineUnavailable,
		"missing engine context is explicit");
}

bool testPlantedBombEntityIsInferred()
{
	Fixture fixture;
	configureFixture(&fixture);
	fixture.entity.v.dmgtime = 30.0f;
	astrabot::compat::ObjectiveObservation objective = {};
	const astrabot::metamod::ObservationAdapterResult result =
		fixture.adapter.collectPlantedBomb(
			&fixture.entity, "grenade", "models/w_c4.mdl", 5.0f,
			{4U, 9U}, frame(), timing(), &objective);
	return check(result == astrabot::metamod::ObservationAdapterResult::Accepted,
		"planted bomb entity is accepted") &&
		check(objective.bombPlanted.isAvailable() && objective.bombPlanted.value,
			"planted bomb state is available") &&
		check(objective.bombPlanted.context.quality ==
			astrabot::compat::ObservationQuality::Inferred,
			"planted bomb state is inferred") &&
		check(objective.bombTimer.value == 30.0f,
			"planted bomb timer is preserved");
}
}

int main()
{
	return testPublicPlayerFieldsAreExactAtCollectionTick() &&
		testPrivateWeaponFieldsAreUnavailableNotSyntheticExact() &&
		testObjectiveProxyKeepsInferredQuality() &&
		testAdapterPreservesActorAndFrameIdentity() &&
		testAdapterHandlesMissingEnginePointers() &&
		testPlantedBombEntityIsInferred() ? 0 : 1;
}
