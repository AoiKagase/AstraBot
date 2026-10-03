#include "astrabot/runtime/movement_physics.hpp"

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

bool testFallLandingTrackerReportsMeasuredLandingDamage()
{
	using astrabot::runtime::FallLandingTracker;
	using astrabot::runtime::MovementPhysicsState;
	FallLandingTracker tracker;
	MovementPhysicsState state = {};
	state.entityValid = true;
	state.health = 100.0f;
	state.grounded = true;
	if (!check(!tracker.observe(1U, state).landingConfirmed,
			"standing does not report a landing"))
	{
		return false;
	}
	state.grounded = false;
	tracker.observe(1U, state);
	state.grounded = true;
	auto safeLanding = tracker.observe(1U, state);
	if (!check(safeLanding.landingConfirmed && safeLanding.hasLandingDamage &&
			safeLanding.landingDamage == 0.0f,
			"landing reports a measured zero damage result"))
	{
		return false;
	}
	state.grounded = false;
	state.health = 100.0f;
	tracker.observe(1U, state);
	state.grounded = true;
	state.health = 90.0f;
	auto damagedLanding = tracker.observe(1U, state);
	return check(damagedLanding.landingConfirmed &&
			damagedLanding.hasLandingDamage &&
			damagedLanding.landingDamage == 10.0f,
			"landing reports health lost during the fall interval");
}
}

int main()
{
	using astrabot::runtime::MovementPhysicsState;
	using astrabot::runtime::PhysicsVector;
	using astrabot::runtime::SpawnReadiness;
	if (!testFallLandingTrackerReportsMeasuredLandingDamage())
	{
		return 1;
	}
	if (!check(astrabot::runtime::isMovementDirectionReversal(
				PhysicsVector{1.0f, 0.0f, 0.0f},
				PhysicsVector{-1.0f, 0.0f, 0.0f}),
			"opposite horizontal movement directions are a reversal") ||
		!check(!astrabot::runtime::isMovementDirectionReversal(
				PhysicsVector{1.0f, 0.0f, 0.0f},
				PhysicsVector{0.0f, 1.0f, 0.0f}),
			"orthogonal directions are not a reversal") ||
		!check(!astrabot::runtime::isMovementDirectionReversal(
				PhysicsVector{0.0f, 0.0f, 0.0f},
				PhysicsVector{-1.0f, 0.0f, 0.0f}),
			"unavailable zero direction is not a reversal"))
	{
		return 1;
	}

	MovementPhysicsState state{};
	state.entityValid = true;
	state.fakeClient = true;
	state.team = 1;
	state.health = 100.0f;
	state.solid = 3;
	state.movetype = 3;
	state.grounded = true;
	if (!check(astrabot::runtime::spawnReadiness(state) == SpawnReadiness::Ready,
			   "live slidebox walk entity is spawn ready"))
	{
		return 1;
	}

	state.team = 0;
	if (!check(astrabot::runtime::spawnReadiness(state) == SpawnReadiness::NotReady,
			   "unassigned entity is not spawn ready"))
	{
		return 1;
	}
	state.teamConfirmed = true;
	if (!check(astrabot::runtime::spawnReadiness(state) == SpawnReadiness::Ready,
			   "TeamInfo-confirmed entity is spawn ready without raw team field"))
	{
		return 1;
	}
	state.teamConfirmed = false;
	state.team = 1;

	state.spectator = true;
	if (!check(astrabot::runtime::spawnReadiness(state) == SpawnReadiness::NotReady,
			   "spectator entity is not spawn ready"))
	{
		return 1;
	}

	state.spectator = false;
	state.dead = true;
	if (!check(astrabot::runtime::spawnReadiness(state) == SpawnReadiness::NotReady,
			   "dead entity is not spawn ready"))
	{
		return 1;
	}

	state.dead = false;
	state.grounded = false;
	if (!check(astrabot::runtime::spawnReadiness(state) == SpawnReadiness::NotReady,
			   "initial airborne entity is not spawn ready"))
	{
		return 1;
	}

	if (!check(astrabot::runtime::movementReadiness(state) == SpawnReadiness::Ready,
			"continuing alive joined airborne entity is movement ready"))
	{
		return 1;
	}
	state.team = 0;
	if (!check(astrabot::runtime::movementReadiness(state) == SpawnReadiness::NotReady,
			"airborne entity still requires a confirmed team")) return 1;
	state.teamConfirmed = true;
	if (!check(astrabot::runtime::movementReadiness(state) == SpawnReadiness::Ready,
			"airborne TeamInfo-confirmed entity remains movement ready")) return 1;
	state.spectator = true;
	if (!check(astrabot::runtime::movementReadiness(state) == SpawnReadiness::NotReady,
			"airborne spectator never becomes movement ready")) return 1;
	state.spectator = false;
	state.dead = true;
	if (!check(astrabot::runtime::movementReadiness(state) == SpawnReadiness::NotReady,
			"airborne dead entity never becomes movement ready")) return 1;
	state.dead = false;
	state.health = 0.0f;
	if (!check(astrabot::runtime::movementReadiness(state) == SpawnReadiness::NotReady,
			"airborne zero-health entity never becomes movement ready")) return 1;
	state.health = std::numeric_limits<float>::quiet_NaN();
	if (!check(astrabot::runtime::movementReadiness(state) == SpawnReadiness::NotReady,
			"airborne nonfinite health never becomes movement ready")) return 1;
	state.health = 100.0f;
	state.solid = 0;
	if (!check(astrabot::runtime::movementReadiness(state) == SpawnReadiness::NotReady,
			"airborne nonsolid entity never becomes movement ready")) return 1;
	state.solid = 3;
	state.movetype = 0;
	if (!check(astrabot::runtime::movementReadiness(state) == SpawnReadiness::NotReady,
			"airborne invalid movetype never becomes movement ready")) return 1;
	state.movetype = 3;
	state.fakeClient = false;
	if (!check(astrabot::runtime::movementReadiness(state) == SpawnReadiness::NotReady,
			"human entity never becomes managed movement ready")) return 1;
	state.fakeClient = true;
	state.entityValid = false;
	if (!check(astrabot::runtime::movementReadiness(state) == SpawnReadiness::NotReady,
			"invalid entity never becomes movement ready")) return 1;

	return 0;
}
