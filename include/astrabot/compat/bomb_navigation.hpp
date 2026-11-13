#ifndef ASTRABOT_COMPAT_BOMB_NAVIGATION_HPP
#define ASTRABOT_COMPAT_BOMB_NAVIGATION_HPP

#include "astrabot/compat/random_source.hpp"
#include "astrabot/nav/nav_query.hpp"
#include <array>
#include <cmath>
#include <limits>

namespace astrabot
{
namespace compat
{
inline nav::NavVector bombAreaCenter(const nav::NavArea &area)
{
	return {(area.extent.lo.x + area.extent.hi.x) * 0.5f, (area.extent.lo.y + area.extent.hi.y) * 0.5f,
		(area.extent.lo.z + area.extent.hi.z) * 0.5f};
}

// Reference FindMinimumCostArea/FarAwayFromPositionFunctor: keep 32 cheapest
// reciprocal-distance costs on areas at least 150 by 150, then random-select.
// Reachability belongs to the subsequent FASTEST path attempt, not this draw.
inline bool selectBombEscapeArea(const nav::NavDocument &document, nav::NavVector bomb,
	ICompatibilityRandomSource &random, RandomActor actor, RandomTimingContext timing, nav::AreaId *goal)
{
	constexpr unsigned kMaximumEscapeCandidates = 32U;
	constexpr float kMinimumEscapeAreaWidth = 150.0f;
	if (goal == nullptr || document.areas().empty() || !std::isfinite(bomb.x) || !std::isfinite(bomb.y) ||
		!std::isfinite(bomb.z))
		return false;
	struct Candidate
	{
		nav::AreaId area;
		float cost;
	};
	std::array<Candidate, kMaximumEscapeCandidates> candidates{};
	std::size_t count = 0U;
	for (const nav::NavArea &area : document.areas())
	{
		if (area.extent.hi.x - area.extent.lo.x < kMinimumEscapeAreaWidth ||
			area.extent.hi.y - area.extent.lo.y < kMinimumEscapeAreaWidth)
			continue;
		const nav::NavVector center = bombAreaCenter(area);
		const float x = center.x - bomb.x, y = center.y - bomb.y, z = center.z - bomb.z;
		const float distance = std::sqrt(x * x + y * y + z * z);
		const float cost = distance > 0.0f ? 1.0f / distance : std::numeric_limits<float>::infinity();
		if (count < candidates.size())
			candidates[count++] = {area.id, cost};
		else
		{
			std::size_t expensive = 0U;
			for (std::size_t i = 1U; i < count; ++i)
				if (candidates[i].cost > candidates[expensive].cost)
					expensive = i;
			if (candidates[expensive].cost > cost)
				candidates[expensive] = {area.id, cost};
		}
	}
	const std::size_t available = count != 0U ? count : document.areas().size();
	const auto selection = random.nextLong(
		RandomRequest::longRequest("Bomb.EscapeArea", actor, timing, 0, static_cast<int>(available - 1U)));
	if (selection.status != RandomStatus::Ok || selection.value < 0 ||
		static_cast<std::size_t>(selection.value) >= available)
		return false;
	*goal = count != 0U ? candidates[static_cast<std::size_t>(selection.value)].area
						: document.areas()[static_cast<std::size_t>(selection.value)].id;
	return true;
}

inline bool estimateBombTravel(const nav::NavSnapshot &navigation, nav::NavVector self, nav::NavVector bomb,
	float publicMaxSpeed, float maximumSnapDistance, float *seconds)
{
	if (seconds == nullptr || !navigation.isValid() || !std::isfinite(publicMaxSpeed) || publicMaxSpeed <= 0.0f ||
		!std::isfinite(maximumSnapDistance) || maximumSnapDistance <= 0.0f)
		return false;
	nav::NavQuery query(navigation);
	nav::NavAreaMatch start{}, finish{};
	if (query.findNearest3D(self, maximumSnapDistance, &start) != nav::NavQueryResult::Found ||
		query.findNearest3D(bomb, maximumSnapDistance, &finish) != nav::NavQueryResult::Found)
		return false;
	nav::NavCorridor corridor{};
	if (query.buildCorridor(start.area, finish.area, nav::NavRouteType::Fastest, &corridor) !=
			nav::NavQueryResult::Found ||
		!std::isfinite(corridor.cost) || corridor.cost < 0.0f)
		return false;
	const float x = self.x - bomb.x, y = self.y - bomb.y, z = self.z - bomb.z;
	const float straight = std::sqrt(x * x + y * y + z * z);
	// Area-center paths can have zero cost for two points in one area.
	const float distance = corridor.cost > straight ? corridor.cost : straight;
	const float estimate = distance / publicMaxSpeed;
	if (!std::isfinite(estimate) || estimate < 0.0f)
		return false;
	*seconds = estimate;
	return true;
}
}
}
#endif
