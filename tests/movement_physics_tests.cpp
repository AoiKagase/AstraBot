#include "astrabot/runtime/movement_physics.hpp"

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
}

int main()
{
	using astrabot::runtime::MovementPhysicsState;
	using astrabot::runtime::PhysicsVector;
	using astrabot::runtime::SpawnReadiness;
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
			   "airborne entity is not navigation ready"))
	{
		return 1;
	}

	return 0;
}
