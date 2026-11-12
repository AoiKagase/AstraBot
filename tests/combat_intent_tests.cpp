#include "astrabot/combat/combat_intent.hpp"

#include "astrabot/perception/perception.hpp"

#include <cstdio>
#include <limits>

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

	astrabot::world::SnapshotIdentity identity(
		std::uint32_t tick,
		astrabot::world::ActorKey observer = {1U, 5U})
	{
		astrabot::world::SnapshotIdentity value = {};
		value.frame = {3U, 4U, tick};
		value.observer = observer;
		return value;
	}

	astrabot::world::WorldSnapshot snapshot(
		std::uint32_t tick,
		astrabot::world::ActorKey observer = {1U, 5U})
	{
		astrabot::perception::PerceptionInput input(identity(tick, observer));
		astrabot::world::ActorObservation self = {};
		self.actor = observer;
		self.state = astrabot::world::ObservationState::ObservedPresent;
		self.relation = astrabot::world::TeamRelation::Friendly;
		self.position = {0.0f, 0.0f, 0.0f};
		self.confidence = {1.0f, 0U};
		input.addActor(self);
		astrabot::perception::PerceptionAssembler assembler;
		astrabot::world::WorldSnapshot value;
		assembler.publish(input, &value);
		return value;
	}

	astrabot::combat::CombatDecisionContext context(
		const astrabot::world::WorldSnapshot &value)
	{
		astrabot::combat::CombatDecisionContext result = {};
		result.actor = value.identity().observer;
		result.frame = value.identity().frame;
		result.origin = {0.0f, 0.0f, 0.0f};
		return result;
	}

	astrabot::combat::WeaponInventory inventory()
	{
		astrabot::combat::WeaponInventory value;
		astrabot::combat::WeaponRecord weapon = {};
		weapon.weapon = {7U};
		weapon.slot = 1U;
		weapon.kind = astrabot::combat::WeaponKind::Rifle;
		weapon.availability = astrabot::combat::WeaponAvailability::Available;
		weapon.priority = 10U;
		weapon.ammo = {30U, 90U, 30U};
		weapon.reloadState = astrabot::combat::ReloadState::NotReloading;
		value.add(weapon);
		return value;
	}

	astrabot::combat::TargetBelief target()
	{
		astrabot::combat::TargetBelief value = {};
		value.target = {2U, 9U};
		value.state = astrabot::combat::TargetBeliefState::Visible;
		value.position = {128.0f, 0.0f, 0.0f};
		value.confidence = 0.9f;
		value.ageTicks = 0U;
		value.friendlyFireRisk = false;
		return value;
	}
}

bool testFireIntentDoesNotClaimEngineSuccess()
{
	using astrabot::combat::CombatController;
	using astrabot::combat::CombatDecision;
	using astrabot::combat::CombatResult;

	const auto value = snapshot(10U);
	CombatController controller({1U, 5U});
	CombatDecision decision = {};
	if (!check(controller.decide(
			value,
			inventory(),
			target(),
			context(value),
			&decision) == CombatResult::FireIntentReady &&
			decision.hasAim && decision.hasFire &&
			decision.fire.weapon.value == 7U &&
			decision.aim.target.slot == 2U &&
			decision.aim.yaw == 0.0f &&
			decision.fire.isIntent() &&
			!decision.fire.isDamageConfirmation(),
			"confirmed target produces fire intent only"))
	{
		return false;
	}

	astrabot::combat::TargetBelief unknown = target();
	unknown.state = astrabot::combat::TargetBeliefState::Unknown;
	decision = {};
	return check(controller.decide(
			 snapshot(11U),
			inventory(),
			unknown,
			context(snapshot(11U)),
			&decision) == CombatResult::InvalidTarget &&
			!decision.hasFire,
		"unknown target fails closed without a fire intent");
}

bool testSafetyCooldownAndReloadBoundaries()
{
	using astrabot::combat::CombatController;
	using astrabot::combat::CombatDecision;
	using astrabot::combat::CombatResult;

	const auto first = snapshot(20U);
	CombatController controller({1U, 5U});
	CombatDecision decision = {};
	astrabot::combat::TargetBelief unsafe = target();
	unsafe.friendlyFireRisk = true;
	if (!check(controller.decide(
			first,
			inventory(),
			unsafe,
			context(first),
			&decision) == CombatResult::FriendlyFireRisk &&
			!decision.hasFire,
			"friendly-fire risk suppresses fire intent"))
	{
		return false;
	}

	astrabot::combat::WeaponInventory reloadInventory;
	astrabot::combat::WeaponRecord reloadWeapon = {};
	reloadWeapon.weapon = {9U};
	reloadWeapon.slot = 2U;
	reloadWeapon.kind = astrabot::combat::WeaponKind::Pistol;
	reloadWeapon.availability =
		astrabot::combat::WeaponAvailability::Available;
	reloadWeapon.ammo = {0U, 30U, 30U};
	reloadWeapon.reloadState = astrabot::combat::ReloadState::NotReloading;
	reloadInventory.add(reloadWeapon);
	const auto reloadSnapshot = snapshot(21U);
	decision = {};
	if (!check(controller.decide(
			reloadSnapshot,
			reloadInventory,
			target(),
			context(reloadSnapshot),
			&decision) == CombatResult::ReloadIntentReady &&
			decision.hasReload && decision.reload.weapon.value == 9U &&
			!decision.reload.isAmmoConfirmation(),
			"empty clip emits reload intent without ammo success"))
	{
		return false;
	}

	astrabot::combat::WeaponInventory cooldownInventory = inventory();
	const astrabot::combat::WeaponRecord *weapon =
		cooldownInventory.weaponAt(0U);
	if (!check(weapon != nullptr, "cooldown test has weapon record"))
	{
		return false;
	}

	astrabot::combat::WeaponRecord cooldown = *weapon;
	cooldown.cooldownUntilTick = 100U;
	cooldownInventory.clear();
	cooldownInventory.add(cooldown);
	const auto cooldownSnapshot = snapshot(22U);
	decision = {};
	return check(controller.decide(
			cooldownSnapshot,
			cooldownInventory,
			target(),
			context(cooldownSnapshot),
			&decision) == CombatResult::NoAction && !decision.hasFire,
		"cooldown prevents fire intent");
}

bool testInvalidTargetDamageAndGeneration()
{
	using astrabot::combat::CombatController;
	using astrabot::combat::CombatDecision;
	using astrabot::combat::CombatResult;

	const auto first = snapshot(30U);
	CombatController controller({1U, 5U});
	CombatDecision decision = {};
	astrabot::combat::TargetBelief invalid = target();
	invalid.position.x = std::numeric_limits<float>::quiet_NaN();
	if (!check(controller.decide(
			first,
			inventory(),
			invalid,
			context(first),
			&decision) == CombatResult::InvalidObservation,
			"non-finite target position is rejected"))
	{
		return false;
	}

	astrabot::combat::DamageObservation damage = {};
	damage.actor = {1U, 5U};
	damage.target = {2U, 9U};
	damage.frame = first.identity().frame;
	damage.state = astrabot::combat::DamageObservationState::Unknown;
	damage.amount = 100.0f;
	damage.targetDied = true;
	if (!check(!damage.isConfirmed() && !damage.isConfirmedDeath() &&
			controller.observeDamage(damage) == CombatResult::NoAction,
			"unknown damage cannot confirm death"))
	{
		return false;
	}

	const auto oldActor = snapshot(31U, {1U, 4U});
	return check(controller.decide(
			oldActor,
			inventory(),
			target(),
			context(oldActor),
			&decision) == CombatResult::StaleGeneration,
		"stale actor generation rejects combat decision");
}

bool testProfileAimAndSharedRandom()
{
	using namespace astrabot;
	const auto first = snapshot(40U);
	auto ctx = context(first);
	ctx.aimTrackingEnabled = true;
	ctx.currentYaw = 90.0f;
	ctx.currentPitch = 0.0f;
	ctx.elapsedSeconds = 0.033f;
	ctx.timestamp = 0.0f;
	ctx.skill = 1.0f;
	ctx.fieldOfView = 90.0f;
	const compat::RandomActor actor = {ctx.actor.slot, ctx.actor.generation};
	std::vector<compat::RandomTapeEntry> tape;
	tape.push_back(compat::RandomTapeEntry::floatEntry("AimOffsetX", actor, ctx.randomTiming, 0.0f,
													   0.0f, 0.0f));
	tape.push_back(compat::RandomTapeEntry::floatEntry("AimOffsetY", actor, ctx.randomTiming, 0.0f,
													   0.0f, 0.0f));
	tape.push_back(compat::RandomTapeEntry::floatEntry("AimOffsetZ", actor, ctx.randomTiming, 0.0f,
													   0.0f, 0.0f));
	tape.push_back(compat::RandomTapeEntry::floatEntry("AimOffsetRefresh", actor, ctx.randomTiming,
													   0.25f, 1.0f, 0.5f));
	compat::ScriptedRandomSource random(tape);
	ctx.randomSource = &random;
	combat::CombatController controller;
	combat::CombatDecision decision = {};
	if (!check(controller.decide(first, inventory(), target(), ctx, &decision) ==
					   combat::CombatResult::NoAction &&
				   decision.hasAim && !decision.hasFire && decision.aim.yaw > 80.0f &&
				   decision.aim.yaw < 90.0f && random.verifyComplete(),
			   "profile aim consumes typed shared random tape and turns before firing"))
		return false;
	combat::CombatController unavailable;
	ctx.randomSource = nullptr;
	return check(unavailable.decide(first, inventory(), target(), ctx, &decision) ==
						 combat::CombatResult::InvalidTarget &&
					 !decision.hasFire,
				 "unavailable shared random never becomes perfect aim");
}

bool testLoadedFirearmConvergesAtFullUpdateCadence()
{
	using namespace astrabot;
	for (float interval : {0.1f, 0.125f, 0.25f})
	{
		std::vector<compat::RandomTapeEntry> tape;
		for (unsigned refresh = 0U; refresh < 10U; ++refresh)
		{
			for (const char *id : {"AimOffsetX", "AimOffsetY", "AimOffsetZ"})
				tape.push_back(compat::RandomTapeEntry::floatEntry(id,{1U,5U},{},0.0f,0.0f,0.0f));
			tape.push_back(compat::RandomTapeEntry::floatEntry("AimOffsetRefresh",{1U,5U},{},.25f,1.0f,.5f));
		}
		compat::ScriptedRandomSource random(tape);
		combat::CombatController controller;
		float yaw = 90.0f, pitch = 20.0f;
		bool fired = false, waited = false;
		for (unsigned update = 0U; update < 20U && !fired; ++update)
		{
			const auto world = snapshot(100U + update);
			auto ctx = context(world);
			ctx.aimTrackingEnabled = true; ctx.skill = 1.0f; ctx.fieldOfView = 90.0f;
			ctx.elapsedSeconds = interval; ctx.timestamp = interval * update;
			ctx.currentYaw = yaw; ctx.currentPitch = pitch; ctx.randomSource = &random;
			combat::CombatDecision decision{};
			const auto result = controller.decide(world,inventory(),target(),ctx,&decision);
			if (!check(decision.hasAim && !random.failed(),"visible loaded firearm retains bounded aim and valid shared RNG")) return false;
			waited = waited || result == combat::CombatResult::NoAction;
			fired = result == combat::CombatResult::FireIntentReady;
			yaw = decision.aim.yaw; pitch = decision.aim.pitch;
		}
		if (!check(fired && (interval > .125f || waited),"loaded firearm leaves aim NoAction within reference range tolerance")) return false;
	}
	return true;
}

int main()
{
	for (float skill : {0.5f, 0.501f, 1.0f})
	{
		const auto world = snapshot(200U);
		auto ctx = context(world);
		ctx.aimTrackingEnabled = true; ctx.elapsedSeconds = 1.0f / 30.0f;
		ctx.skill = skill; ctx.fieldOfView = 90.0f; ctx.predictMovement = true;
		ctx.targetVelocity = {0, 300, 0}; ctx.observerVelocity = {0, 50, 0};
		const float error = 128.0f * .1f * (1.0f - skill);
		std::vector<astrabot::compat::RandomTapeEntry> tape;
		for (const char *id : {"AimOffsetX", "AimOffsetY", "AimOffsetZ"})
			tape.push_back(astrabot::compat::RandomTapeEntry::floatEntry(id,{1,5},{},-error,error,0));
		tape.push_back(astrabot::compat::RandomTapeEntry::floatEntry("AimOffsetRefresh",{1,5},{},.25f,1,.5f));
		astrabot::compat::ScriptedRandomSource random(tape); ctx.randomSource = &random;
		astrabot::combat::CombatController controller; astrabot::combat::CombatDecision decision{};
		controller.decide(world, inventory(), target(), ctx, &decision);
		if (!check(decision.hasAim && std::fabs(decision.aim.point.y - (skill > .5f ? 25.0f : 0.0f)) < .001f &&
			random.verifyComplete(), "reference movement lead uses relative velocity only above skill .5")) return 1;
		ctx.frame.tick++; ctx.timestamp += 1.0f / 30.0f;
		astrabot::combat::AimIntent aim{};
		if (!check(controller.upkeepAim(target(),ctx,&aim) && random.verifyComplete(),
			"upkeep advances aim without repeating a full decision or RNG refresh")) return 1;
		ctx.actor.generation++;
		if (!check(!controller.upkeepAim(target(),ctx,&aim), "upkeep rejects recycled actor generation")) return 1;
	}
	if (!testLoadedFirearmConvergesAtFullUpdateCadence()) return 1;
	if (!testProfileAimAndSharedRandom() || !testFireIntentDoesNotClaimEngineSuccess() ||
			!testSafetyCooldownAndReloadBoundaries() ||
			!testInvalidTargetDamageAndGeneration())
	{
		return 1;
	}

	return 0;
}
