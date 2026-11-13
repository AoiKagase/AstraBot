#ifndef ASTRABOT_COMPAT_PUBLIC_BOMB_CLOCK_HPP
#define ASTRABOT_COMPAT_PUBLIC_BOMB_CLOCK_HPP

#include "astrabot/world/world_snapshot.hpp"
#include <cmath>

namespace astrabot
{
namespace compat
{
// A public notice plus mp_c4timer is an estimate, never the GameDLL's exact
// explosion timestamp. Repeated planted notices do not restart the estimate.
class PublicBombClock
{
public:
	void reset()
	{
		*this = PublicBombClock();
	}

	bool observe(world::FrameIdentity frame, double timestamp, bool planted)
	{
		if (!frame.isValid() || !std::isfinite(timestamp) || timestamp < 0.0)
			return false;
		const bool sameRound =
			frame_.mapGeneration == frame.mapGeneration && frame_.roundGeneration == frame.roundGeneration;
		if (sameRound && frame_.isValid() && (frame.tick < frame_.tick || timestamp < lastNotice_))
			return false;
		if (!sameRound || !planted_ || !planted)
			plantedAt_ = timestamp;
		frame_ = frame;
		lastNotice_ = timestamp;
		planted_ = planted;
		return true;
	}

	bool estimate(world::FrameIdentity frame, double now, float fuseSeconds, float *remaining) const
	{
		if (remaining == nullptr || !planted_ || !frame.isValid() || frame.mapGeneration != frame_.mapGeneration ||
			frame.roundGeneration != frame_.roundGeneration || frame.tick < frame_.tick || !std::isfinite(now) ||
			now < lastNotice_ || !std::isfinite(fuseSeconds) || fuseSeconds <= 0.0f)
			return false;
		const double value = double(fuseSeconds) - (now - plantedAt_);
		*remaining = value > 0.0 ? static_cast<float>(value) : 0.0f;
		return true;
	}

private:
	world::FrameIdentity frame_ = {};
	double plantedAt_ = 0.0;
	double lastNotice_ = 0.0;
	bool planted_ = false;
};
}
}
#endif
