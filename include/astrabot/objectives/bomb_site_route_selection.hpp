#pragma once

#include "astrabot/compat/random_source.hpp"
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace astrabot
{
namespace objectives
{
namespace bomb_approach_detail
{
constexpr float kMinimumPlantDelaySeconds = 10.0f;
constexpr float kMaximumPlantDelaySeconds = 30.0f;
constexpr float kPlantTimeReserveSeconds = 5.0f;
}
inline bool isBetterBombSiteRoute(float candidatePathCost, float selectedPathCost) noexcept
{
	return std::isfinite(candidatePathCost) && candidatePathCost >= 0.0f && candidatePathCost < selectedPathCost;
}

// A bounded preplant move. This deliberately does not reproduce whole-map Hunt.
// Identity belongs to the actual carrier and round, not assignment generations.
struct BombApproachState
{
	std::uint32_t mapGeneration = 0U;
	std::uint32_t roundGeneration = 0U;
	std::uint32_t actorSlot = 0U;
	std::uint32_t actorGeneration = 0U;
	bool initialized = false;
	bool finished = false;
	float plantAt = 0.0f;
	std::uint32_t preferredSiteIdentity = 0U;
};

enum class DefenseStrategy { Unknown, Hold, Rush };

// CCSBotManager::RestartRound draws the plant deadline and then the shared rush decision.
struct RoundPlantDecision
{
	std::uint32_t map = 0U;
	std::uint32_t round = 0U;
	bool sampled = false;
	bool known = false;
	float plantAt = 0.0f;
	DefenseStrategy defense = DefenseStrategy::Unknown;

	void begin(std::uint32_t mapGeneration, std::uint32_t roundGeneration, float restartTime,
		compat::ICompatibilityRandomSource &random)
	{
		if (sampled && map == mapGeneration && round == roundGeneration)
		{
			return;
		}
		*this = {};
		map = mapGeneration;
		round = roundGeneration;
		sampled = true;
		if (map == 0U || round == 0U || !std::isfinite(restartTime))
		{
			return;
		}
		const auto delay =
			random.nextFloat(compat::RandomRequest::floatRequest("Bomb.RoundPlantDelay", {}, {0U, 0U, round},
				bomb_approach_detail::kMinimumPlantDelaySeconds, bomb_approach_detail::kMaximumPlantDelaySeconds));
		if (map != mapGeneration || round != roundGeneration)
			return;
		known = delay.status == compat::RandomStatus::Ok && std::isfinite(delay.value) &&
			delay.value >= bomb_approach_detail::kMinimumPlantDelaySeconds &&
			delay.value <= bomb_approach_detail::kMaximumPlantDelaySeconds;
		if (known)
		{
			plantAt = restartTime + delay.value;
		}
		const auto rush = random.nextFloat(compat::RandomRequest::floatRequest(
			"Round.DefenseRush", {}, {0U, 0U, roundGeneration}, 0.0f, 100.0f));
		if (map == mapGeneration && round == roundGeneration && rush.status == compat::RandomStatus::Ok &&
			std::isfinite(rush.value) && rush.value >= 0.0f && rush.value <= 100.0f)
			defense = rush.value <= 33.3f ? DefenseStrategy::Rush : DefenseStrategy::Hold;
	}
};

enum class BombApproachPhase
{
	Cancel,
	Approach,
	Plant
};

inline void beginBombApproach(BombApproachState *state, std::uint32_t mapGeneration, std::uint32_t roundGeneration,
	compat::RandomActor actor, float now, float roundStartTime, float roundRemaining, float nearestRouteSeconds,
	compat::ICompatibilityRandomSource *random, compat::RandomTimingContext timing)
{
	if (state == nullptr)
		return;
	if (state->initialized && state->mapGeneration == mapGeneration && state->roundGeneration == roundGeneration &&
		state->actorSlot == actor.slot && state->actorGeneration == actor.generation)
		return;
	*state = {};
	state->mapGeneration = mapGeneration;
	state->roundGeneration = roundGeneration;
	state->actorSlot = actor.slot;
	state->actorGeneration = actor.generation;
	state->initialized = true;
	state->finished = true;
	state->plantAt = now;
	// Keep travel plus plant time available; missing authority fails to nearest.
	if (!std::isfinite(now) || !std::isfinite(roundStartTime) || !std::isfinite(roundRemaining) ||
		!std::isfinite(nearestRouteSeconds) || nearestRouteSeconds < 0.0f || random == nullptr ||
		roundRemaining <= bomb_approach_detail::kMaximumPlantDelaySeconds + nearestRouteSeconds +
				bomb_approach_detail::kPlantTimeReserveSeconds)
		return;
	const compat::RandomRequest request = compat::RandomRequest::floatRequest("Bomb.PreplantDelay", actor, timing,
		bomb_approach_detail::kMinimumPlantDelaySeconds, bomb_approach_detail::kMaximumPlantDelaySeconds);
	const compat::RandomFloatResult delay = random->nextFloat(request);
	if (delay.status != compat::RandomStatus::Ok || !std::isfinite(delay.value) ||
		delay.value < bomb_approach_detail::kMinimumPlantDelaySeconds ||
		delay.value > bomb_approach_detail::kMaximumPlantDelaySeconds)
		return;
	state->plantAt = roundStartTime + delay.value;
	state->finished = !std::isfinite(state->plantAt) || now >= state->plantAt;
}

inline BombApproachPhase bombApproachPhase(BombApproachState *state, float now, float roundRemaining,
	float nearestRouteSeconds, bool carrying, bool planted, bool atSite)
{
	if (state == nullptr || !carrying || planted)
	{
		if (state != nullptr)
			*state = {};
		return BombApproachPhase::Cancel;
	}
	if (!state->initialized || state->finished)
		return BombApproachPhase::Plant;
	if (atSite || !std::isfinite(now) || !std::isfinite(roundRemaining) || !std::isfinite(nearestRouteSeconds) ||
		nearestRouteSeconds < 0.0f || now >= state->plantAt ||
		roundRemaining <= nearestRouteSeconds + bomb_approach_detail::kPlantTimeReserveSeconds)
	{
		state->finished = true;
		return BombApproachPhase::Plant;
	}
	return BombApproachPhase::Approach;
}

// Return the supplied array index; count is the no-reachable-site sentinel.
// Selection is sampled once by the caller and kept in BombApproachState.
inline std::size_t chooseBombApproachSite(const float *costs, std::size_t count,
	compat::ICompatibilityRandomSource *random, compat::RandomActor actor, compat::RandomTimingContext timing)
{
	if (costs == nullptr)
		return count;
	std::size_t reachable = 0U, nearest = count;
	float nearestCost = (std::numeric_limits<float>::max)();
	for (std::size_t index = 0U; index < count; ++index)
	{
		if (!std::isfinite(costs[index]) || costs[index] < 0.0f)
			continue;
		++reachable;
		if (isBetterBombSiteRoute(costs[index], nearestCost))
		{
			nearest = index;
			nearestCost = costs[index];
		}
	}
	if (reachable <= 1U || random == nullptr ||
		reachable > static_cast<std::size_t>((std::numeric_limits<std::int32_t>::max)()))
		return nearest;
	const compat::RandomRequest request = compat::RandomRequest::longRequest(
		"Bomb.PreplantSite", actor, timing, 0, static_cast<std::int32_t>(reachable - 1U));
	const compat::RandomLongResult choice = random->nextLong(request);
	if (choice.status != compat::RandomStatus::Ok || choice.value < 0 ||
		static_cast<std::size_t>(choice.value) >= reachable)
		return nearest;
	std::size_t ordinal = 0U;
	for (std::size_t index = 0U; index < count; ++index)
	{
		if (!std::isfinite(costs[index]) || costs[index] < 0.0f)
			continue;
		if (ordinal++ == static_cast<std::size_t>(choice.value))
			return index;
	}
	return nearest;
}

} // namespace objectives
} // namespace astrabot
