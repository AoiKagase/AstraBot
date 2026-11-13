#ifndef ASTRABOT_COMPAT_SMOKE_OBSERVATION_HPP
#define ASTRABOT_COMPAT_SMOKE_OBSERVATION_HPP

#include "astrabot/world/world_snapshot.hpp"
#include <array>
#include <cmath>
#include <algorithm>

namespace astrabot
{
namespace compat
{
class SmokeObservations
{
public:
	void reset()
	{
		clouds_ = {};
		next_ = 0U;
	}

	void observe(world::EntityKey entity, world::WorldVector position, float until)
	{
		if (!entity.isValid() || !std::isfinite(until) || !std::isfinite(position.x) || !std::isfinite(position.y) ||
			!std::isfinite(position.z))
		{
			return;
		}
		for (Cloud &cloud : clouds_)
		{
			if (cloud.entity == entity)
			{
				cloud = {entity, position, until};
				return;
			}
		}
		clouds_[next_++ % clouds_.size()] = {entity, position, until};
	}

	bool blocks(world::WorldVector from, world::WorldVector to, float now) const
	{
// Keep literal evaluation precision on x87; each macro is undefined below.
#define ASTRABOT_SMOKE_OBSERVATION_MINIMUM_RAY_LENGTH 0.001f
		constexpr float kSmokeRadius = 115.0f;
#define ASTRABOT_SMOKE_OBSERVATION_OBSCURED_RADIUS_FRACTION 0.7f
		if (!std::isfinite(now))
		{
			return false;
		}
		const float dx = to.x - from.x;
		const float dy = to.y - from.y;
		const float dz = to.z - from.z;
		const float length = std::sqrt(dx * dx + dy * dy + dz * dz);
		if (!std::isfinite(length) || length <= ASTRABOT_SMOKE_OBSERVATION_MINIMUM_RAY_LENGTH)
		{
			return false;
		}
		// Publicly observed CSBot smoke geometry: radius 115, obscured length > 0.7 radius.
		const float radius = kSmokeRadius;
		float covered = 0.0f;
		for (const Cloud &cloud : clouds_)
		{
			if (!cloud.entity.isValid() || now >= cloud.until)
			{
				continue;
			}
			const float x = cloud.position.x - from.x;
			const float y = cloud.position.y - from.y;
			const float z = cloud.position.z - from.z;
			const float along = (x * dx + y * dy + z * dz) / length;
			const float perpendicularSquared = (std::max)(0.0f, x * x + y * y + z * z - along * along);
			if (perpendicularSquared >= radius * radius)
			{
				continue;
			}
			const float halfChord = std::sqrt(radius * radius - perpendicularSquared);
			const float begin = (std::max)(0.0f, along - halfChord);
			const float end = (std::min)(length, along + halfChord);
			covered += (std::max)(0.0f, end - begin);
		}
		return covered > ASTRABOT_SMOKE_OBSERVATION_OBSCURED_RADIUS_FRACTION * radius;
	}
#undef ASTRABOT_SMOKE_OBSERVATION_MINIMUM_RAY_LENGTH
#undef ASTRABOT_SMOKE_OBSERVATION_OBSCURED_RADIUS_FRACTION

private:
	static constexpr unsigned kMaximumClouds = 32U;
	struct Cloud
	{
		world::EntityKey entity;
		world::WorldVector position;
		float until;
	};
	std::array<Cloud, kMaximumClouds> clouds_{};
	std::size_t next_ = 0U;
};
}
}
#endif
