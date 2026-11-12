#ifndef ASTRABOT_COMPAT_PUBLIC_BOMB_RADAR_HPP
#define ASTRABOT_COMPAT_PUBLIC_BOMB_RADAR_HPP

#include "astrabot/world/world_snapshot.hpp"
#include <cmath>

namespace astrabot { namespace compat {
// Per-recipient T radar knowledge, never a server-wide bomb-location cache.
class PublicBombRadar
{
public:
	void reset() { known_ = false; }
	void drop(const world::FrameIdentity &frame, const world::WorldVector &position)
	{
		if (!frame.isValid() || !std::isfinite(position.x) ||
			!std::isfinite(position.y) || !std::isfinite(position.z)) return;
		if (known_ && frame.mapGeneration == frame_.mapGeneration &&
			frame.roundGeneration == frame_.roundGeneration && frame.tick < frame_.tick) return;
		frame_ = frame; position_ = position; known_ = true;
	}
	bool read(const world::FrameIdentity &frame, world::WorldVector *position) const
	{
		if (!known_ || position == nullptr || !frame.isValid() ||
			frame.mapGeneration != frame_.mapGeneration ||
			frame.roundGeneration != frame_.roundGeneration || frame.tick < frame_.tick) return false;
		*position = position_; return true;
	}
private:
	bool known_ = false;
	world::FrameIdentity frame_{};
	world::WorldVector position_{};
};
} }
#endif
