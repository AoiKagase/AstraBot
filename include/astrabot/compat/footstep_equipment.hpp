#ifndef ASTRABOT_COMPAT_FOOTSTEP_EQUIPMENT_HPP
#define ASTRABOT_COMPAT_FOOTSTEP_EQUIPMENT_HPP

#include "astrabot/world/world_snapshot.hpp"
#include <cmath>
#include <cstring>

namespace astrabot
{
namespace compat
{
constexpr float kStandardFootstepHearingRange = 1100.0f;

inline bool isStandardFootstepSample(const char *sample)
{
	if (!sample)
	{
		return false;
	}
	const char *prefixes[] = {"player/pl_step", "player/pl_metal", "player/pl_dirt", "player/pl_duct",
		"player/pl_grate", "player/pl_tile", "player/pl_slosh", "player/pl_wade", "player/pl_ladder", "player/pl_snow"};
	for (const char *prefix : prefixes)
	{
		const auto length = std::strlen(prefix);
		if (std::strncmp(sample, prefix, length) != 0)
		{
			continue;
		}
		const char maximum = std::strcmp(prefix, "player/pl_tile") == 0 ? '5' : '4';
		return sample[length] >= '1' && sample[length] <= maximum && std::strcmp(sample + length + 1, ".wav") == 0;
	}
	return false;
}

inline bool isFootstepSniper(std::uint16_t weapon)
{
	constexpr std::uint16_t kScout = 3;
	constexpr std::uint16_t kSg550 = 13;
	constexpr std::uint16_t kAwp = 18;
	constexpr std::uint16_t kG3sg1 = 24;
	return weapon == kScout || weapon == kSg550 || weapon == kAwp || weapon == kG3sg1;
}

struct FootstepEquipmentReceipt
{
	world::ActorKey actor{};
	world::FrameIdentity frame{};
	float seconds = 0;
	float distance = -1;

	bool current(world::ActorKey observer, world::FrameIdentity currentFrame, float now) const
	{
		constexpr float kMaximumDecisionDelaySeconds = .25f;
		return actor.isValid() && actor == observer && frame.mapGeneration == currentFrame.mapGeneration &&
			frame.roundGeneration == currentFrame.roundGeneration && frame.tick <= currentFrame.tick &&
			std::isfinite(now) && std::isfinite(seconds) && seconds >= 0 && now >= seconds &&
			now - seconds <= kMaximumDecisionDelaySeconds && std::isfinite(distance) && distance >= 0;
	}
};
}
}

#endif
