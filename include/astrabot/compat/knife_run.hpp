#ifndef ASTRABOT_COMPAT_KNIFE_RUN_HPP
#define ASTRABOT_COMPAT_KNIFE_RUN_HPP

#include "astrabot/world/world_snapshot.hpp"
#include <cmath>
#include <cstring>

namespace astrabot
{
namespace compat
{
inline bool isStandardDryFireSample(const char *sample)
{
	return sample &&
		(std::strcmp(sample, "weapons/dryfire_pistol.wav") == 0 ||
			std::strcmp(sample, "weapons/dryfire_rifle.wav") == 0);
}

// Native CountdownTimer expires strictly after its timestamp.
class KnifeRunTimer
{
public:
	void reset()
	{
		*this = KnifeRunTimer{};
	}

	void start(world::ActorKey actor, world::FrameIdentity frame, std::uint32_t entityGeneration, float now)
	{
		reset();
		if (!actor.isValid() || !frame.isValid() || entityGeneration == 0 || !std::isfinite(now) || now < 0)
		{
			return;
		}
		actor_ = actor;
		frame_ = frame;
		entityGeneration_ = entityGeneration;
		lastTime_ = now;
		constexpr float kForcedRunSeconds = 5.0f;
		until_ = now + kForcedRunSeconds;
	}

	bool active(
		world::ActorKey actor, world::FrameIdentity frame, std::uint32_t entityGeneration, float now, bool alive)
	{
		if (!actor_.isValid() || !(actor_ == actor) || !frame.isValid() ||
			frame.mapGeneration != frame_.mapGeneration || frame.roundGeneration != frame_.roundGeneration ||
			frame.tick < frame_.tick || entityGeneration != entityGeneration_ || !alive || !std::isfinite(now) ||
			now < lastTime_ || now > until_)
		{
			reset();
			return false;
		}
		frame_ = frame;
		lastTime_ = now;
		return true;
	}

private:
	world::ActorKey actor_{};
	world::FrameIdentity frame_{};
	std::uint32_t entityGeneration_ = 0;
	float lastTime_ = 0;
	float until_ = 0;
};
}
}
#endif
