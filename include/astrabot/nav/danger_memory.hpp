#ifndef ASTRABOT_NAV_DANGER_MEMORY_HPP
#define ASTRABOT_NAV_DANGER_MEMORY_HPP

#include "astrabot/nav/nav_model.hpp"

#include <cmath>
#include <cstdint>
#include <limits>
#include <unordered_set>
#include <vector>

namespace astrabot
{
namespace nav
{
// Runtime-only, team-specific death observations. Nothing is written to NAV.
class DangerMemory
{
public:
	void reset(std::uint32_t mapGeneration = 0U)
	{
		generation_ = mapGeneration;
		entries_.clear();
		lastTime_ = 0.0;
		hasTime_ = false;
	}

	std::uint32_t mapGeneration() const
	{
		return generation_;
	}

	bool recordDeath(const NavDocument &document, std::uint32_t mapGeneration, std::uint8_t team, AreaId startArea,
		const NavVector &deathPosition, double nowSeconds, float amount = 1.0f)
	{
		constexpr double kDeathPropagationRadius = 500.0;
		if (mapGeneration == 0U || !validTeam(team) || startArea == 0U || !validTime(nowSeconds) ||
			!std::isfinite(amount) || amount < 0.0f || !std::isfinite(deathPosition.x) ||
			!std::isfinite(deathPosition.y) || !std::isfinite(deathPosition.z) ||
			document.findArea(startArea) == nullptr ||
			(mapGeneration == generation_ && hasTime_ && nowSeconds < lastTime_))
			return false;

		// Compute transactionally: even an overflowing contribution cannot
		// partially modify another area's danger or clear the previous map.
		std::vector<Entry> next = mapGeneration == generation_ ? entries_ : std::vector<Entry>{};
		if (!add(next, team, startArea, nowSeconds, amount))
			return false;
		std::vector<AreaId> queue{startArea};
		std::unordered_set<AreaId> visited{startArea};
		for (std::size_t head = 0U; head < queue.size(); ++head)
		{
			const NavArea *area = document.findArea(queue[head]);
			for (const auto &connections : area->connections)
			{
				for (AreaId adjacentId : connections)
				{
					if (!visited.insert(adjacentId).second)
						continue;
					const NavArea *adjacent = document.findArea(adjacentId);
					if (adjacent == nullptr)
						continue;
					const double dx = (double(adjacent->extent.lo.x) + adjacent->extent.hi.x) * 0.5 - deathPosition.x;
					const double dy = (double(adjacent->extent.lo.y) + adjacent->extent.hi.y) * 0.5 - deathPosition.y;
					const double dz = (double(adjacent->extent.lo.z) + adjacent->extent.hi.z) * 0.5 - deathPosition.z;
					const double distance = std::sqrt(dx * dx + dy * dy + dz * dz);
					if (!std::isfinite(distance))
						return false;
					if (distance > kDeathPropagationRadius)
						continue;
					// Intentionally grows with distance: this is the reference
					// propagation formula, not inverse-distance falloff.
					if (!add(next, team, adjacentId, nowSeconds, double(amount) * distance / kDeathPropagationRadius))
						return false;
					queue.push_back(adjacentId);
				}
			}
		}
		entries_.swap(next);
		generation_ = mapGeneration;
		lastTime_ = nowSeconds;
		hasTime_ = true;
		return true;
	}

	// false preserves the caller's output. Unobserved valid areas have zero
	// danger, independently for each team. Time is monotonic within one map.
	bool query(double nowSeconds, std::uint8_t team, AreaId area, float *danger)
	{
		if (danger == nullptr || generation_ == 0U || !validTeam(team) || area == 0U || !validTime(nowSeconds) ||
			(hasTime_ && nowSeconds < lastTime_))
			return false;
		double value = 0.0;
		for (const Entry &entry : entries_)
			if (entry.team == team && entry.area == area)
			{
				value = decayed(entry, nowSeconds);
				break;
			}
		*danger = static_cast<float>(value);
		lastTime_ = nowSeconds;
		hasTime_ = true;
		return true;
	}

private:
	struct Entry
	{
		std::uint8_t team;
		AreaId area;
		double value;
		double observedAt;
	};

	static bool validTeam(std::uint8_t team)
	{
		constexpr auto kTerroristTeam = 1U;
		constexpr auto kCounterTerroristTeam = 2U;
		return team == kTerroristTeam || team == kCounterTerroristTeam;
	}

	static bool validTime(double time)
	{
		return std::isfinite(time) && time >= 0.0;
	}

	static double decayed(const Entry &entry, double now)
	{
		constexpr double kDangerDecaySecondsPerUnit = 120.0;
		const double value = entry.value - (now - entry.observedAt) / kDangerDecaySecondsPerUnit;
		return value > 0.0 ? value : 0.0;
	}

	static bool add(std::vector<Entry> &entries, std::uint8_t team, AreaId area, double now, double amount)
	{
		for (Entry &entry : entries)
			if (entry.team == team && entry.area == area)
			{
				const double value = decayed(entry, now) + amount;
				if (!std::isfinite(value) || value > std::numeric_limits<float>::max())
					return false;
				entry.value = value;
				entry.observedAt = now;
				return true;
			}
		entries.push_back({team, area, amount, now});
		return true;
	}

	std::uint32_t generation_ = 0U;
	std::vector<Entry> entries_;
	double lastTime_ = 0.0;
	bool hasTime_ = false;
};
}
}

#endif
