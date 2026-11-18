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
		constexpr float kMaximumFreezeSeconds = 60.0f;
		if (!std::isfinite(startTime))
		{
			return {false, 0.0f, 0.0f};
		}
		if (!std::isfinite(freezeSeconds) || freezeSeconds < 0.0f)
		{
			freezeSeconds = 0.0f;
		}
		freezeSeconds = (std::min)(freezeSeconds, kMaximumFreezeSeconds);
		const float untilTime = startTime + freezeSeconds;
		if (!std::isfinite(untilTime) || untilTime < startTime)
		{
			return {false, startTime, startTime};
		}
		return {freezeSeconds > 0.0f && untilTime > startTime, startTime, untilTime};
	}

	bool isActive(float currentTime) const noexcept
	{
		return scheduled && std::isfinite(currentTime) && std::isfinite(startTime) && std::isfinite(untilTime) &&
			currentTime >= startTime && currentTime < untilTime;
	}
};

class RoundLifecycleTracker
{
public:
	RoundLifecycleTracker() noexcept : generationPreparedForStart_(false), roundStartObserved_(false), roundOver_(false)
	{
	}

	void reset() noexcept
	{
		generationPreparedForStart_ = false;
		roundStartObserved_ = false;
		roundOver_ = false;
		newMatchPending_ = false;
		ctScoreCleared_ = false;
	}

	bool takeNewMatch() noexcept
	{
		const bool pending = newMatchPending_;
		newMatchPending_ = false;
		ctScoreCleared_ = false;
		return pending;
	}

	// RestartRound resets the CT and then T scoreboard in the same frame, before respawning players.
	bool observeBroadcastScore(bool counterTerrorist, int score, std::uint32_t tick) noexcept
	{
		if (!newMatchPending_)
			return false;
		if (!counterTerrorist && score == 0 && ctScoreCleared_ && ctScoreTick_ == tick)
			return takeNewMatch();
		ctScoreCleared_ = counterTerrorist && score == 0;
		ctScoreTick_ = tick;
		return false;
	}

	bool isRoundOver() const noexcept
	{
		return roundOver_;
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
			newMatchPending_ = true;
			roundOver_ = true;
			if (generationPreparedForStart_)
			{
				return false;
			}
			generationPreparedForStart_ = true;
			roundStartObserved_ = false;
			return true;

		case RoundLifecycleEvent::RoundEnd:
			roundOver_ = true;
			if (!roundStartObserved_ || generationPreparedForStart_)
			{
				return false;
			}
			generationPreparedForStart_ = true;
			roundStartObserved_ = false;
			return true;

		case RoundLifecycleEvent::RoundStart:
			roundOver_ = false;
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
	bool roundOver_;
	bool newMatchPending_ = false;
	bool ctScoreCleared_ = false;
	std::uint32_t ctScoreTick_ = 0;
};
} // namespace metamod
} // namespace astrabot

#endif
