#ifndef ASTRABOT_COMPAT_CHATTER_PLACE_HISTORY_HPP
#define ASTRABOT_COMPAT_CHATTER_PLACE_HISTORY_HPP
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
namespace astrabot
{
namespace compat
{
// One manager-owned history across both teams; map/round owners reset explicitly.
class ChatterPlaceHistory
{
public:
	bool allow(std::uint32_t id, float now)
	{
		constexpr float kRepeatSeconds = 20.0f;
		if (!id || !std::isfinite(now) || now < 0.0f)
			return false;
		for (std::size_t i = 0; i < count_; ++i)
		{
			if (entries_[i].id != id)
				continue;
			if (now - entries_[i].timestamp < kRepeatSeconds)
				return false;
			entries_[i].timestamp = now;
			return true;
		}
		// Native full-table entries are untracked and are never repeat-suppressed.
		if (count_ < entries_.size())
			entries_[count_++] = {id, now};
		return true;
	}
	void reset()
	{
		count_ = 0;
	}

private:
	struct Entry
	{
		std::uint32_t id;
		float timestamp;
	};
	static constexpr std::size_t kMaximumPlaces = 64;
	std::array<Entry, kMaximumPlaces> entries_{};
	std::size_t count_ = 0;
};
}
}
#endif
