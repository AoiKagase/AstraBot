#include "astrabot/compat/combat_behavior.hpp"
#include "astrabot/compat/combat_movement.hpp"
#include "astrabot/compat/weapon_actions.hpp"
#include <cstdio>
using namespace astrabot;
namespace
{
bool check(bool value, const char *name)
{
	if (!value)
		std::fprintf(stderr, "%s\n", name);
	return value;
}

compat::RandomTapeEntry draw(const char *id, float low, float high, float value)
{
	return compat::RandomTapeEntry::floatEntry(id, {1, 1}, {}, low, high, value);
}

compat::CombatMovementInput input(compat::ICompatibilityRandomSource *random)
{
	compat::CombatMovementInput in{};
	in.actor = {1, 1};
	in.target = {2, 1};
	in.frame = {1, 1, 1};
	in.now = 10;
	in.skill = 1;
	in.aggression = .5f;
	in.range = 900;
	in.weapon = 28;
	in.enemyFacing = true;
	in.random = random;
	return in;
}

bool crouchClear(void *data)
{
	++*static_cast<unsigned *>(data);
	return true;
}

bool testAimPolicy()
{
	struct Case
	{
		unsigned weapon;
		float skill;
		bool known;
		float accuracy;
		float z;
	};
	const Case cases[] = {{28, .79f, false, 0, 91}, {28, .8f, false, 0, 100}, {18, 1, false, 0, 91},
		{3, 1, false, 0, 100}, {5, 1, false, 0, 91}, {21, 1, false, 0, 91}, {20, 1, false, 0, 91},
		{28, 1, true, .401f, 91}, {28, 1, true, .4f, 100}, {28, 1, false, 1, 100}, {1, 1, true, 1, 100}};
	for (const auto &c : cases)
	{
		auto p = compat::combatAimPoint({100, 0, 100}, {0, 0, 0}, world::VisibleHead | world::VisibleChest,
			static_cast<std::uint16_t>(c.weapon), c.skill, 36, c.known, c.accuracy);
		if (!check(p.z == c.z, "weapon/skill/recoil chest policy matches reference thresholds"))
			return false;
	}
	const auto left = compat::combatAimPoint({100, 0, 100}, {0, 0, 0}, world::VisibleLeftSide, 28, 1, 36, false, 0);
	const auto right = compat::combatAimPoint({100, 0, 100}, {0, 0, 0}, world::VisibleRightSide, 28, 1, 36, false, 0);
	const auto feet = compat::combatAimPoint({100, 0, 100}, {0, 0, 0}, world::VisibleFeet, 28, 1, 18, false, 0);
	if (!check(left.y == 16 && right.y == -16 && left.z == 82 && right.z == 82 && feet.z == 64,
			"partial cover and ducked feet use the reference offsets"))
		return false;
	return check(compat::combatAimOnTarget({900, 0, 0}, {0, 0, 0}, 1.5f, false) &&
			!compat::combatAimOnTarget({900, 0, 0}, {0, 0, 0}, 1.5f, true),
		"range-based firearm tolerance is wider than sniper tolerance");
}

bool testCrouchHold()
{
	compat::ScriptedRandomSource random({draw("Combat.Range", 325, 425, 375), draw("Combat.CrouchChance", 0, 100, 0),
		draw("Combat.DodgeChance", 0, 100, 99)});
	auto in = input(&random);
	unsigned probes = 0;
	in.crouchVisible = &crouchClear;
	in.crouchContext = &probes;
	compat::CombatMovementPlanner planner;
	auto d = planner.update(in);
	if (!check(d.active && d.entered && d.crouch && d.forward == 0 && d.side == 0 && probes == 1 &&
				random.verifyComplete(),
			"far crouch choice requires a clear crouched sightline and holds position"))
		return false;
	in.frame.tick++;
	in.now += .1f;
	d = planner.update(in);
	if (!check(d.active && !d.entered && d.crouch && probes == 1 && random.verifyComplete(),
			"crouch hold is persistent without repeated RNG or sightline probes"))
		return false;
	in.frame.tick++;
	in.target = {};
	d = planner.update(in);
	return check(!d.active && !planner.active(), "target loss ends combat movement ownership");
}

bool testAttackEntry()
{
	struct Case
	{
		bool hide, sniper, knife, inherit, clear;
		float range, crouchRoll;
		bool hold, crouched;
		unsigned probes;
	};
	const Case cases[] = {
		{false, false, false, true, true, 750, 10, false, true, 0},
		{false, false, false, false, true, 750.1f, 10, true, true, 1},
		{false, true, false, false, false, 100, 49, false, false, 1},
		{true, false, false, false, true, 100, 99, true, true, 0},
		{true, false, true, true, true, 100, 99, false, false, 0},
	};
	for (const auto &c : cases)
	{
		std::vector<compat::RandomTapeEntry> tape;
		if (c.hide)
			tape.push_back(draw("Attack.HideCrouchChance", 0, 100, 59));
		tape.push_back(draw("Attack.PinnedDown", 7, 10, 8));
		tape.push_back(draw("Attack.ShieldToggle", 2, 10, 3));
		if (!c.hide && !c.knife)
			tape.push_back(draw("Combat.CrouchChance", 0, 100, c.crouchRoll));
		tape.push_back(draw("Combat.DodgeChance", 0, 100, 50));
		tape.push_back(draw("Attack.Coward", 0, 100, 51));
		compat::ScriptedRandomSource random(tape);
		unsigned probes = 0;
		compat::AttackEntryInput entry{};
		entry.actor = {1, 1};
		entry.random = &random;
		entry.now = 10;
		entry.skill = entry.aggression = .5f;
		entry.range = c.range;
		entry.hasEnemy = true;
		entry.atHidingSpot = c.hide;
		entry.sniper = c.sniper;
		entry.knife = c.knife;
		entry.crouched = c.inherit;
		entry.walking = true;
		entry.crouchContext = &probes;
		entry.crouchVisible = c.clear ? &crouchClear : +[](void *data) {
			++*static_cast<unsigned *>(data);
			return false;
		};
		const auto plan = compat::planAttackEntry(entry);
		if (!check(plan.valid && plan.pinnedAfter == 18 && plan.shieldToggleAfter == 13 && plan.coward &&
				plan.hold == c.hold && plan.crouched == c.crouched && plan.savedCrouched == c.inherit &&
				plan.savedWalking && plan.walking == c.knife && plan.dodge == c.knife && probes == c.probes &&
				random.verifyComplete(),
			"Attack entry preserves posture and orders the native random draws and conditional low eye trace"))
			return false;
	}
	return true;
}

bool testDodge()
{
	std::vector<compat::RandomTapeEntry> tape = {draw("Combat.Range", 325, 425, 375),
		draw("Combat.CrouchChance", 0, 100, 100), draw("Combat.DodgeChance", 0, 100, 0),
		draw("Combat.JumpChance", 0, 100, 100),
		compat::RandomTapeEntry::longEntry("Combat.DodgeState", {1, 1}, {}, 0, 2, 1),
		draw("Combat.DodgeDuration", .3f, 1, .5f),
		compat::RandomTapeEntry::longEntry("Combat.DodgeState", {1, 1}, {}, 0, 2, 1),
		compat::RandomTapeEntry::longEntry("Combat.DodgeState", {1, 1}, {}, 0, 2, 2),
		draw("Combat.DodgeDuration", .3f, 1, .7f)};
	compat::ScriptedRandomSource random(tape);
	auto in = input(&random);
	in.moving = true;
	compat::CombatMovementPlanner planner;
	auto d = planner.update(in);
	if (!check(d.active && !d.crouch && d.forward == 0 && d.side == -1 && !d.jump && random.position() == 6,
			"skilled dodge chooses relative strafe without NAV forward travel"))
		return false;
	in.frame.tick++;
	in.now = 10.1f;
	d = planner.update(in);
	if (!check(d.side == -1 && random.position() == 6, "dodge persists until its timed boundary"))
		return false;
	in.frame.tick++;
	in.now = 10.5f;
	d = planner.update(in);
	return check(
		d.side == 1 && random.verifyComplete(), "expired dodge rejects repeated state before choosing another");
}

bool testRangeMovement()
{
	for (float skill : {.5f, .65f, .66f})
	{
		compat::ScriptedRandomSource random({draw("Combat.Range", 325, 425, 375),
			draw("Combat.CrouchChance", 0, 100, 100), draw("Combat.DodgeChance", 0, 100, 0)});
		auto in = input(&random);
		in.skill = skill;
		in.range = 200;
		in.enemyFacing = false;
		compat::CombatMovementPlanner planner;
		const auto d = planner.update(in);
		if (!check(d.active && d.forward == (skill < .66f ? -1.0f : 0.0f) && d.side == 0 && random.verifyComplete(),
				"range correction uses the reference skill .66 and combat-range hysteresis"))
			return false;
	}
	compat::ScriptedRandomSource unavailable({});
	auto in = input(&unavailable);
	compat::CombatMovementPlanner planner;
	return check(!planner.update(in).active, "unavailable movement RNG fails closed");
}

bool testFireBeforeDodgeAndPlanarRange()
{
	compat::ScriptedRandomSource random({draw("Combat.Range", 325, 425, 375), draw("Combat.CrouchChance", 0, 100, 100),
		draw("Combat.DodgeChance", 0, 100, 0), draw("Combat.FireInterval", .3f, .7f, .5f),
		draw("Combat.JumpChance", 0, 100, 0),
		compat::RandomTapeEntry::longEntry("Combat.DodgeState", {1, 1}, {}, 0, 2, 1),
		draw("Combat.DodgeDuration", .3f, 1, .5f)});
	auto in = input(&random);
	in.range = 2100;
	in.planarRange = 900;
	in.moving = false;
	compat::CombatMovementPlanner movement;
	const auto entry = movement.prepare(in);
	compat::WeaponActionInput weapon{};
	weapon.actor = in.actor;
	weapon.frame = in.frame;
	weapon.timestamp = in.now;
	weapon.weaponId = 28;
	weapon.targetDistance = in.range;
	weapon.clipKnown = true;
	weapon.clip = 30;
	weapon.targetVisible = true;
	weapon.aimAligned = true;
	weapon.requestFire = true;
	compat::WeaponActionPlanner actions;
	compat::FireCadenceContext cadence{1, &random, {}, 900};
	const auto shot = actions.update(weapon, cadence);
	const auto dodge = movement.finish(in);
	return check(entry.active && shot.primary && dodge.side == -1 && !dodge.jump && random.verifyComplete(),
		"fire RNG precedes dodge RNG, stationary jump draw is retained, and dodge uses planar distance");
}
}
int main()
{
	return testAimPolicy() && testAttackEntry() && testCrouchHold() && testDodge() && testRangeMovement() &&
			testFireBeforeDodgeAndPlanarRange()
		? 0
		: 1;
}
