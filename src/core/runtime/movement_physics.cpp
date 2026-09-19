#include "astrabot/runtime/movement_physics.hpp"

#include <cmath>

namespace astrabot
{
	namespace runtime
	{
		SpawnReadiness spawnReadiness(const MovementPhysicsState &state) noexcept
		{
			if (!state.entityValid || !state.fakeClient || state.spectator || state.dead ||
				!state.grounded ||
				((state.team != 1 && state.team != 2) && !state.teamConfirmed) ||
					!std::isfinite(state.health) || state.health <= 0.0f || state.solid != 3 ||
					state.movetype != 3)
			{
				return SpawnReadiness::NotReady;
			}
			return SpawnReadiness::Ready;
		}
	}
}
