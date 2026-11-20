#include "astrabot/compat/weapon_actions.hpp"
#include <cstdio>
#include <limits>

namespace
{
bool check(bool value, const char *description)
{
	if (!value)
	{
		std::fprintf(stderr, "%s\n", description);
	}
	return value;
}

astrabot::compat::WeaponActionInput input(std::uint16_t weapon)
{
	astrabot::compat::WeaponActionInput value = {};
	value.actor = {1U, 1U};
	value.frame = {1U, 1U, 1U};
	value.weaponId = weapon;
	value.clipKnown = true;
	value.clip = 10U;
	value.fovKnown = true;
	value.fieldOfView = 90.0f;
	value.targetDistance = 600.0f;
	value.reactionSeconds = 0.3f;
	value.targetVisible = true;
	value.aimAligned = true;
	value.stationary = true;
	value.requestFire = true;
	value.attacking = true;
	value.grenadeAimKnown = true;
	value.grenadePitch = -20.0f;
	value.grenadeYaw = 45.0f;
	return value;
}
} // namespace

bool testReferenceFireCadence()
{
	using namespace astrabot::compat;
	struct Case
	{
		std::uint16_t weapon;
		float skill, range, lower, upper;
	};
	const Case cases[] = {{28U, .5f, 399.99f, 0, 0}, {28U, .499f, 900, 0, 0}, {28U, .5f, 400, .15f, .5f},
		{28U, 1, 800, .15f, .5f}, {28U, .5f, 800.01f, .3f, .7f}, {28U, 1, 900, .3f, .7f}, {20U, 1, 900, 0, 0},
		{18U, 1, 900, .15f, .5f}, {1U, .75f, 900, .15f, .4f}, {1U, .751f, 900, 0, 0}, {19U, 1, 600, .15f, .5f},
		{21U, 1, 900, .3f, .7f}};
	for (const auto &c : cases)
	{
		std::vector<RandomTapeEntry> tape;
		const RandomTimingContext timing = {90U, 91U, 92U};
		const float delay = (c.lower + c.upper) * .5f;
		if (delay > 0)
			for (unsigned i = 0; i < 2; ++i)
				tape.push_back(
					RandomTapeEntry::floatEntry("Combat.FireInterval", {1U, 1U}, timing, c.lower, c.upper, delay));
		ScriptedRandomSource random(tape);
		const FireCadenceContext cadence = {c.skill, &random, timing, c.range};
		WeaponActionPlanner planner;
		auto value = input(c.weapon);
		value.timestamp = 10.0f;
		value.fieldOfView = 40.0f;
		value.targetDistance = c.range;
		if (!check(planner.update(value, cadence).primary, "cadence starts with an eligible shot"))
			return false;
		if (!check(!planner.update(value, cadence).primary && !random.failed() &&
					random.position() == (delay > 0 ? 1U : 0U),
				"duplicate full-update frame cannot fire or draw again"))
			return false;
		value.frame.tick++;
		value.timestamp = 10.01f;
		auto result = planner.update(value, cadence);
		if (delay == 0)
		{
			if (!check(result.primary && random.verifyComplete(), "spray branch neither pauses nor draws RNG"))
				return false;
			continue;
		}
		if (!check(!result.primary && result.waitingForCadence && random.position() == 1U,
				"middle/far skilled shot releases trigger during its reference delay"))
			return false;
		value.frame.tick++;
		value.timestamp = 10.0f + (delay - .1f);
		if (!check(planner.update(value, cadence).waitingForCadence && random.position() == 1U,
				"strict reference deadline and fixed full-think subtraction are preserved"))
			return false;
		value.frame.tick++;
		value.timestamp = std::nextafter(value.timestamp, std::numeric_limits<float>::infinity());
		if (!check(planner.update(value, cadence).primary && random.verifyComplete(),
				"first update after deadline fires and draws exactly once"))
			return false;
	}
	return true;
}

bool testCadenceGatesAndReset()
{
	using namespace astrabot::compat;
	ScriptedRandomSource random({});
	const FireCadenceContext cadence = {1, &random, {}, 900};
	for (unsigned gate = 0; gate < 5; ++gate)
	{
		WeaponActionPlanner planner;
		auto value = input(28U);
		value.timestamp = 10;
		if (gate == 0)
			value.targetVisible = false;
		if (gate == 1)
			value.aimAligned = false;
		if (gate == 2)
			value.clip = 0;
		if (gate == 3)
			value.reloading = {true, true};
		if (gate == 4)
			value.requestFire = false;
		if (!check(!planner.update(value, cadence).primary && random.verifyComplete(),
				"ineligible fire neither consumes cadence RNG nor sends primary"))
			return false;
	}
	WeaponActionPlanner unavailable;
	auto value = input(28U);
	value.timestamp = 10;
	const FireCadenceContext missing = {1, nullptr, {}, 900};
	if (!check(!unavailable.update(value, missing).primary, "unavailable cadence RNG fails closed"))
		return false;
	for (unsigned reset = 0; reset < 5; ++reset)
	{
		std::vector<RandomTapeEntry> tape;
		tape.push_back(RandomTapeEntry::floatEntry("Combat.FireInterval", {1U, 1U}, {}, .3f, .7f, .7f));
		ScriptedRandomSource source(tape);
		FireCadenceContext ctx = {1, &source, {}, 900};
		WeaponActionPlanner planner;
		value = input(28U);
		value.timestamp = 10;
		if (!check(planner.update(value, ctx).primary, "reset fixture fires before entering cadence wait"))
			return false;
		value.frame.tick++;
		value.timestamp = 10.01f;
		if (reset == 0)
			value.actor.generation++;
		if (reset == 1)
			value.frame.roundGeneration++;
		if (reset == 2)
			value.frame.mapGeneration++;
		if (reset == 3)
			value.weaponId = 20U;
		if (reset == 4)
		{
			planner.reset();
			value.timestamp = 1;
		}
		ctx.skill = .49f;
		const auto afterReset = planner.update(value, ctx);
		if (!check((reset == 3 ? afterReset.waitingForCadence && !afterReset.primary : afterReset.primary) &&
					source.verifyComplete(),
				"lifecycle resets clear cadence, while weapon switches preserve it"))
			return false;
	}
	return true;
}

bool testWeaponSwitchKeepsFireDeadline()
{
	using namespace astrabot::compat;
	const auto sample = RandomTapeEntry::floatEntry("Combat.FireInterval", {1, 1}, {}, .3f, .7f, .7f);
	ScriptedRandomSource random({sample, sample});
	FireCadenceContext cadence{1, &random, {}, 900};
	WeaponActionPlanner planner;
	auto v = input(28);
	v.timestamp = 10;
	if (!check(planner.update(v, cadence).primary, "switch fixture starts a distant rifle shot"))
		return false;
	v.frame.tick++;
	v.timestamp = 10.01f;
	v.weaponId = 22;
	auto result = planner.update(v, cadence);
	if (!check(!result.primary && result.waitingForCadence && random.position() == 1,
			"weapon switch cannot erase the existing fire deadline or consume another interval"))
		return false;
	v.frame.tick++;
	v.timestamp = 10.6f;
	if (!check(planner.update(v, cadence).waitingForCadence, "switch retains strict deadline equality"))
		return false;
	v.frame.tick++;
	v.timestamp = 10.61f;
	if (!check(planner.update(v, cadence).primary && random.verifyComplete(),
			"switched weapon resumes after original deadline"))
		return false;
	v.frame.tick++;
	v.timestamp = 10.62f;
	v.weaponId = 29;
	v.targetDistance = 74;
	v.enemyFacingObserver = {true, false};
	result = planner.update(v, cadence);
	return check(result.waitingForCadence && !result.primary && !result.secondary,
		"switching to knife also preserves the shared fire deadline");
}

int main()
{
	using namespace astrabot::compat;
	bool ok = check(nativeMagazineNeedsReload(0, 10, 0.0f, true, true),
		"an empty AWP clip calls for reload even during skilled attack");
	ok &= check(!nativeMagazineNeedsReload(18, 30, 3.0f, false, false) &&
			nativeMagazineNeedsReload(18, 30, 3.25f, false, false) &&
			!nativeMagazineNeedsReload(19, 30, 3.25f, false, false),
		"partial reload needs strictly more than three seconds and at most sixty percent");
	ok &= check(!nativeMagazineNeedsReload(6, 10, 8.0f, false, true) &&
			!nativeMagazineNeedsReload(18, 30, 8.0f, true, false) &&
			!nativeMagazineNeedsReload(-1, 10, 8.0f, false, false) &&
			!nativeMagazineNeedsReload(0, 0, 8.0f, false, false),
		"partial AWP or skilled-attack magazines and items without a known clip do not reload");
	for (float fov : {40.0f, 10.0f})
	{
		WeaponActionPlanner scoped;
		auto v = input(18U);
		v.timestamp = 10;
		v.targetDistance = 1600;
		v.fieldOfView = fov;
		const auto held = scoped.update(v);
		ok &= check(held.primary && !held.secondary, "CSBot AttackState retains an existing zoom level at long range");
	}
	{
		WeaponActionPlanner noPistol;
		auto v = input(18);
		v.timestamp = 10;
		v.targetDistance = 200;
		v.fieldOfView = 90;
		v.pistolAvailable = false;
		const auto action = noPistol.update(v);
		ok &= check(action.primary && !action.secondary && !action.switchToPistol,
			"missing pistol does not suppress a close-range stationary sniper shot");
	}
	ok &= testWeaponSwitchKeepsFireDeadline();
	ok &= testReferenceFireCadence();
	ok &= testCadenceGatesAndReset();
	{
		ScriptedRandomSource source({});
		FireCadenceContext cadence{1, &source, {}, 500};
		WeaponActionPlanner rapid;
		auto v = input(1U);
		v.timestamp = 10;
		ok &=
			check(rapid.update(v, cadence).primary && rapid.rapidFiring(), "skilled pistol enables upkeep rapid fire");
		v.frame.tick++;
		v.timestamp += .1f;
		v.clip--;
		ok &= check(rapid.update(v, cadence).primary && source.verifyComplete(),
			"rapid pistol does not wait for a clip-receipt release at full-think cadence");
		v.frame.tick++;
		v.targetVisible = false;
		ok &= check(!rapid.update(v, cadence).primary && !rapid.rapidFiring(), "lost target stops rapid fire");
	}
	WeaponActionPlanner sniper;
	auto value = input(18U);
	auto result = sniper.update(value);
	ok &= check(result.secondary && !result.primary, "AWP first requests zoom");
	value.frame.tick++;
	value.timestamp = 0.1f;
	result = sniper.update(value);
	ok &= check(result.waitingForObservation && !result.secondary, "zoom request waits for FOV receipt");
	value.frame.tick++;
	value.timestamp = 0.2f;
	value.fieldOfView = 40.0f;
	result = sniper.update(value);
	ok &= check(!result.primary, "scope settle delay includes profile reaction");
	value.frame.tick++;
	value.timestamp = 0.6f;
	result = sniper.update(value);
	ok &= check(result.primary, "observed scope and elapsed settle permit stationary shot");
	value.frame.tick++;
	value.timestamp = 0.7f;
	value.stationary = false;
	ok &= check(!sniper.update(value).primary, "sniper suppresses moving shot");
	WeaponActionPlanner immediateScope;
	auto fastScope = input(18U);
	fastScope.reactionSeconds = ProfileRecord::kMinimumReactionSeconds;
	ok &= check(immediateScope.update(fastScope).secondary, "an adjusted zero profile can request zoom");
	fastScope.fieldOfView = 40.0f;
	fastScope.frame.tick++;
	fastScope.timestamp = 0.09f;
	ok &= check(!immediateScope.update(fastScope).primary, "signed reaction keeps a shortened scope wait");
	fastScope.frame.tick++;
	fastScope.timestamp = 0.11f;
	ok &= check(immediateScope.update(fastScope).primary, "scope settle uses the signed profile reaction");
	WeaponActionPlanner pistol;
	value = input(1U);
	ok &= check(pistol.update(value).primary, "pistol starts trigger press");
	value.frame.tick++;
	value.timestamp = 0.1f;
	ok &= check(pistol.update(value).primary, "unchanged observed clip does not invent a successful shot");
	value.frame.tick++;
	value.clip--;
	result = pistol.update(value);
	ok &= check(result.releasePrimary && !result.primary, "observed pistol shot releases trigger");
	value.frame.tick++;
	ok &= check(pistol.update(value).primary, "pistol may press after release");
	WeaponActionPlanner knife;
	value = input(29U);
	value.targetDistance = 74.0f;
	value.enemyFacingObserver = {true, false};
	ok &= check(knife.update(value).secondary, "known rear target selects knife backstab");
	value.frame.tick++;
	value.targetDistance = 75.0f;
	result = knife.update(value);
	ok &= check(!result.primary && !result.secondary, "knife requires strictly below reference range");
	WeaponActionPlanner modes;
	value = input(22U);
	value.attacking = false;
	value.requestFire = false;
	value.requestSilencer = true;
	value.desiredSilencer = true;
	value.noNearbyEnemies = true;
	value.secondsSinceEnemy = 4.0f;
	result = modes.update(value);
	ok &= check(result.unavailableMode && !result.secondary, "unknown silencer never toggles blindly");
	value.frame.tick++;
	value.silenced = {true, false};
	value.shield = {true, false};
	value.reloading = {true, false};
	ok &= check(modes.update(value).secondary, "known safe silencer mismatch requests secondary");
	WeaponActionPlanner grenade;
	value = input(4U);
	value.requestGrenade = true;
	result = grenade.update(value);
	ok &= check(!result.primary && result.waitingForObservation, "grenade requires verified trajectory");
	value.frame.tick++;
	value.trajectory = {true, true, true, true};
	result = grenade.update(value);
	ok &= check(result.primary && result.stopMovement, "verified grenade begins pin hold");
	value.frame.tick++;
	result = grenade.update(value);
	ok &= check(result.releasePrimary, "aligned verified grenade releases after pin hold");
	value.frame.tick++;
	ok &=
		check(!grenade.update(value).primary, "throw release waits for observed weapon change before another pin hold");
	WeaponActionPlanner unknown;
	auto unknownInput = input(99U);
	ok &= check(!unknown.update(unknownInput).primary, "unknown weapon ID never becomes generic firearm");
	value.frame = {1U, 2U, 1U};
	value.trajectory = {};
	ok &= check(!grenade.update(value).primary, "round reset cannot reuse throw receipt");
	WeaponActionPlanner ignoredScope;
	value = input(18U);
	ok &= check(ignoredScope.update(value).secondary, "scope retry starts with one request");
	value.frame.tick++;
	value.timestamp = 0.6f;
	ok &= check(ignoredScope.update(value).secondary, "unacknowledged scope request is retried after bounded interval");
	value.frame.tick++;
	value.timestamp = 1.2f;
	ok &= check(ignoredScope.update(value).secondary, "scope request permits final bounded retry");
	value.frame.tick++;
	value.timestamp = 1.8f;
	result = ignoredScope.update(value);
	ok &= check(result.switchToPistol && !result.secondary,
		"scope retry exhaustion requests weapon fallback instead of permanent pending state");
	WeaponActionPlanner movingSniper;
	value = input(18U);
	value.fieldOfView = 40.0f;
	value.timestamp = 2.0f;
	value.stationary = false;
	result = movingSniper.update(value);
	ok &= check(result.stopMovement && !result.primary, "sniper explicitly stops NAV movement before firing");
	value.frame.tick++;
	value.timestamp = 2.1f;
	value.stationary = true;
	ok &= check(movingSniper.update(value).primary, "observed stationary sniper may fire after stop request");
	WeaponActionPlanner canceledGrenade;
	value = input(4U);
	value.requestGrenade = true;
	value.trajectory = {true, true, true, true};
	value.aimAligned = false;
	ok &= check(canceledGrenade.update(value).primary, "grenade setup holds while aiming");
	value.frame.tick++;
	value.timestamp = 0.1f;
	value.requestGrenade = false;
	value.trajectory = {};
	result = canceledGrenade.update(value);
	ok &= check(
		result.primary && result.hasRetainedGrenadeAim && result.grenadePitch == -20.0f && result.grenadeYaw == 45.0f,
		"canceled grenade retains last verified aim until finite release deadline");
	value.frame.tick++;
	value.timestamp = 3.1f;
	result = canceledGrenade.update(value);
	ok &= check(!result.primary && result.releasePrimary && result.hasRetainedGrenadeAim,
		"canceled grenade releases with retained verified aim at bounded deadline");
	WeaponActionPlanner blockedSecondary;
	value = input(18U);
	value.secondaryReady = {true, false};
	result = blockedSecondary.update(value);
	ok &= check(
		!result.secondary && result.waitingForObservation, "known secondary cooldown prevents speculative toggle");
	value.frame.tick++;
	value.timestamp = 0.1f;
	value.secondaryReady.value = true;
	ok &= check(blockedSecondary.update(value).secondary, "public ready receipt permits the first scope request");
	WeaponActionPlanner missingGrenadeAim;
	value = input(4U);
	value.requestGrenade = true;
	value.trajectory = {true, true, true, true};
	value.grenadeAimKnown = false;
	result = missingGrenadeAim.update(value);
	ok &= check(!result.primary && result.waitingForObservation,
		"trajectory without dispatchable verified aim never pulls grenade pin");
	WeaponActionPlanner aimingSniper;
	value = input(18U);
	value.requestFire = false;
	value.aimAligned = false;
	value.stationary = false;
	ok &=
		check(aimingSniper.update(value).stopMovement, "visible sniper target stops NAV while aim is still converging");
	WeaponActionPlanner twoFlashbangs;
	value = input(25U);
	value.requestGrenade = true;
	value.trajectory = {true, true, true, true};
	value.reserveKnown = true;
	value.reserve = 2U;
	value.reserveObservationTick = value.frame.tick;
	ok &= check(twoFlashbangs.update(value).primary, "first flash begins from known reserve");
	value.frame.tick++;
	value.reserveObservationTick = value.frame.tick;
	ok &= check(twoFlashbangs.update(value).releasePrimary, "first flash releases normally");
	value.frame.tick++;
	ok &= check(!twoFlashbangs.update(value).primary, "unchanged flash reserve does not rearm");
	value.frame.tick++;
	value.reserveKnown = false;
	ok &= check(!twoFlashbangs.update(value).primary, "unknown flash reserve does not rearm");
	value.frame.tick++;
	value.reserveKnown = true;
	value.reserve = 1U;
	value.reserveObservationTick = 1U;
	ok &= check(!twoFlashbangs.update(value).primary, "old smaller reserve receipt does not rearm");
	value.frame.tick++;
	value.reserveObservationTick = value.frame.tick;
	ok &= check(twoFlashbangs.update(value).primary, "new smaller nonzero reserve receipt rearms same flash weapon ID");
	value.frame.tick++;
	value.reserveObservationTick = value.frame.tick;
	ok &= check(twoFlashbangs.update(value).releasePrimary, "second flash releases through its own cycle");
	value.frame.tick++;
	value.reserveObservationTick = value.frame.tick;
	ok &= check(!twoFlashbangs.update(value).primary, "same reserve receipt cannot rearm a third flash");
	value.frame.tick++;
	value.reserve = 0U;
	value.reserveObservationTick = value.frame.tick;
	ok &= check(!twoFlashbangs.update(value).primary, "zero reserve receipt confirms no rearmable grenade");
	return ok ? 0 : 1;
}
