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

		bool isMovementDirectionReversal(
			const PhysicsVector &previous,
			const PhysicsVector &current) noexcept
		{
			const float previousLength = std::hypot(previous.x, previous.y);
			const float currentLength = std::hypot(current.x, current.y);
			if (!std::isfinite(previousLength) || !std::isfinite(currentLength) ||
					previousLength <= 0.0f || currentLength <= 0.0f)
			{
				return false;
			}
			const float normalizedDot =
				(previous.x * current.x + previous.y * current.y) /
				(previousLength * currentLength);
			return std::isfinite(normalizedDot) && normalizedDot <= -0.5f;
		}
	}
}
