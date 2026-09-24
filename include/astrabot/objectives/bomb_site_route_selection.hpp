#pragma once

#include <cmath>

namespace astrabot
{
	namespace objectives
	{
	inline bool isBetterBombSiteRoute(
		float candidatePathCost,
		float selectedPathCost) noexcept
	{
		return std::isfinite(candidatePathCost) && candidatePathCost >= 0.0f &&
			candidatePathCost < selectedPathCost;
	}
	}
}
