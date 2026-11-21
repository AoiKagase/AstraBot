#ifndef ASTRABOT_COMPAT_HUNT_ENCOUNTER_HPP
#define ASTRABOT_COMPAT_HUNT_ENCOUNTER_HPP

#include "astrabot/nav/nav_model.hpp"
#include <algorithm>
#include <array>
#include <cmath>

namespace astrabot
{
namespace compat
{
inline float huntEncounterProgress(nav::NavVector from, nav::NavVector to, nav::NavVector origin)
{
	const float dx = to.x - from.x, dy = to.y - from.y, dz = to.z - from.z;
	const float axis = std::abs(dx) > std::abs(dy) ? dx : dy;
	if (axis == 0.0f)
		return 0.0f;
	const float traveled = std::abs(dx) > std::abs(dy) ? origin.x - from.x : origin.y - from.y;
	const float progress = traveled / axis + 50.0f / std::sqrt(dx * dx + dy * dy + dz * dz);
	return (std::max)(0.0f, (std::min)(1.0f, progress));
}

class HuntEncounterPlanner
{
public:
	struct Candidates
	{
		std::array<std::uint32_t, 8> ids{};
		int count = 0;
	};

	bool beginPeripheralScan(float now)
	{
		if (now - lastPeripheralScan_ < 0.29f)
			return false;
		lastPeripheralScan_ = now;
		return true;
	}

	bool selectionDue(float now) const { return now >= nextSelection_; }

	void schedule(float now, float skill, float interval)
	{
		const float remaining = 1.0f - skill;
		const float square = remaining * remaining;
		nextSelection_ = now + interval * square * square;
	}

	bool recentlyChecked(std::uint32_t id, float now) const
	{
		for (std::size_t i = 0; i < checkedCount_; ++i)
			if (checked_[i].id == id)
				return now - checked_[i].time <= 10.0f;
		return false;
	}

	void markChecked(std::uint32_t id, float now)
	{
		std::size_t oldest = 0;
		for (std::size_t i = 0; i < checkedCount_; ++i)
		{
			if (checked_[i].id == id)
			{
				checked_[i].time = now;
				return;
			}
			if (checked_[i].time < checked_[oldest].time)
				oldest = i;
		}
		const auto slot = checkedCount_ < checked_.size() ? checkedCount_++ : oldest;
		checked_[slot] = {id, now};
	}

	Candidates eligible(const nav::NavEncounter &encounter, float progress, float now) const
	{
		Candidates result{};
		std::size_t count = 0;
		for (const auto &spot : encounter.spots)
		{
			if (recentlyChecked(spot.hidingSpotId, now))
				continue;
			if (spot.t > progress)
				break;
			result.ids[count++ % result.ids.size()] = spot.hidingSpotId;
		}
		result.count = static_cast<int>((std::min)(count, result.ids.size()));
		return result;
	}

private:
	struct CheckedSpot
	{
		std::uint32_t id;
		float time;
	};
	std::array<CheckedSpot, 64> checked_{};
	std::size_t checkedCount_ = 0;
	float lastPeripheralScan_ = 0.0f;
	float nextSelection_ = 0.0f;
};
} // namespace compat
} // namespace astrabot

#endif
