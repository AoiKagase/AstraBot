#include "astrabot/runtime/movement_physics.hpp"

#include <cmath>

namespace astrabot
{
namespace runtime
{
LandingDamageObservation FallLandingTracker::observe(
	std::uint32_t actorGeneration,
	const MovementPhysicsState &state) noexcept
{
	LandingDamageObservation result = {};
	if (actorGeneration == 0U || !state.entityValid || state.dead ||
		!std::isfinite(state.health))
	{
		reset();
		return result;
	}
	if (!hasActorGeneration_ || actorGeneration_ != actorGeneration)
	{
		reset();
		actorGeneration_ = actorGeneration;
		hasActorGeneration_ = true;
	}
	if (!state.grounded)
	{
		if (!airborne_)
		{
			airborneHealth_ = state.health;
			hasAirborneHealth_ = true;
		}
		airborne_ = true;
		return result;
	}
	if (airborne_)
	{
		result.landingConfirmed = true;
		result.hasLandingDamage = hasAirborneHealth_;
		result.landingDamage = hasAirborneHealth_ && airborneHealth_ > state.health
			? airborneHealth_ - state.health
			: 0.0f;
		airborne_ = false;
		hasAirborneHealth_ = false;
	}
	return result;
}

void FallLandingTracker::reset() noexcept
{
	actorGeneration_ = 0U;
	hasActorGeneration_ = false;
	airborne_ = false;
	hasAirborneHealth_ = false;
	airborneHealth_ = 0.0f;
}

SpawnReadiness movementReadiness(const MovementPhysicsState &state) noexcept
		{
			if (!state.entityValid || !state.fakeClient || state.spectator || state.dead ||
				((state.team != 1 && state.team != 2) && !state.teamConfirmed) ||
					!std::isfinite(state.health) || state.health <= 0.0f || state.solid != 3 ||
					state.movetype != 3)
			{
				return SpawnReadiness::NotReady;
			}
			return SpawnReadiness::Ready;
		}

SpawnReadiness spawnReadiness(const MovementPhysicsState &state) noexcept
{
	return state.grounded ? movementReadiness(state) : SpawnReadiness::NotReady;
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
