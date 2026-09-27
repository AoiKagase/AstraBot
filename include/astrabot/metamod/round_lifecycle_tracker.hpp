#ifndef ASTRABOT_METAMOD_ROUND_LIFECYCLE_TRACKER_HPP
#define ASTRABOT_METAMOD_ROUND_LIFECYCLE_TRACKER_HPP

#include <cstdint>
#include <algorithm>
#include <cmath>

namespace astrabot
{
namespace metamod
{
enum class RoundLifecycleEvent : std::uint8_t
{
	GameCommencing,
	RestartScheduled,
	RoundEnd,
	RoundStart
};

struct RoundFreezeSchedule
{
	bool scheduled;
	float startTime;
	float untilTime;

	static RoundFreezeSchedule create(float startTime, float freezeSeconds) noexcept
	{
		if (!std::isfinite(startTime))
		{
			return {false, 0.0f, 0.0f};
		}
		if (!std::isfinite(freezeSeconds) || freezeSeconds < 0.0f)
		{
			freezeSeconds = 0.0f;
		}
		freezeSeconds = (std::min)(freezeSeconds, 60.0f);
		const float untilTime = startTime + freezeSeconds;
		if (!std::isfinite(untilTime) || untilTime < startTime)
		{
			return {false, startTime, startTime};
		}
		return {freezeSeconds > 0.0f && untilTime > startTime,
			startTime, untilTime};
	}

	bool isActive(float currentTime) const noexcept
	{
		return scheduled && std::isfinite(currentTime) &&
			std::isfinite(startTime) && std::isfinite(untilTime) &&
			currentTime >= startTime && currentTime < untilTime;
	}
};

class RoundLifecycleTracker
{
  public:
	RoundLifecycleTracker() noexcept
		: generationPreparedForStart_(false), roundStartObserved_(false)
	{
	}

	void reset() noexcept
	{
		generationPreparedForStart_ = false;
		roundStartObserved_ = false;
	}

	bool isDuplicateRoundStart() const noexcept
	{
		return roundStartObserved_ && !generationPreparedForStart_;
	}

	bool shouldBeginNewGeneration(RoundLifecycleEvent event) noexcept
	{
		switch (event)
		{
		case RoundLifecycleEvent::GameCommencing:
		case RoundLifecycleEvent::RestartScheduled:
			if (generationPreparedForStart_)
			{
				return false;
			}
			generationPreparedForStart_ = true;
			roundStartObserved_ = false;
			return true;

		case RoundLifecycleEvent::RoundEnd:
			if (!roundStartObserved_ || generationPreparedForStart_)
			{
				return false;
			}
			generationPreparedForStart_ = true;
			roundStartObserved_ = false;
			return true;

		case RoundLifecycleEvent::RoundStart:
			if (generationPreparedForStart_)
			{
				generationPreparedForStart_ = false;
				roundStartObserved_ = true;
				return false;
			}
			if (roundStartObserved_)
			{
				return false;
			}
			roundStartObserved_ = true;
			return true;
		}
		return false;
	}

  private:
	bool generationPreparedForStart_;
	bool roundStartObserved_;
};
} // namespace metamod
} // namespace astrabot

#endif
