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

int main()
{
	if (!testFireIntentDoesNotClaimEngineSuccess() ||
			!testSafetyCooldownAndReloadBoundaries() ||
			!testInvalidTargetDamageAndGeneration())
	{
		return 1;
	}

	return 0;
}
