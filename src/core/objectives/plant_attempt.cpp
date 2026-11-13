#include "astrabot/objectives/plant_attempt.hpp"

#include <cmath>

namespace astrabot
{
namespace objectives
{
bool PlantAttemptController::expired(float elapsedSeconds) noexcept
{
	return std::isfinite(elapsedSeconds) && elapsedSeconds >= kPlantAttemptTimeoutSeconds;
}

bool PlantAttemptController::canBeginObjective(
	bool hasPlantAssignment, bool carryingBomb, bool isTerrorist, bool selectedTargetAvailable) noexcept
{
	return hasPlantAssignment && carryingBomb && isTerrorist && selectedTargetAvailable;
}

PlantAttemptResult PlantAttemptController::evaluate(bool actorOverlapsSite, bool plantedConfirmed, float now) noexcept
{
	if (plantedConfirmed)
	{
		active_ = false;
		startedAt_ = 0.0f;
		retryAfter_ = 0.0f;
		return PlantAttemptResult::Confirmed;
	}
	if (!std::isfinite(now))
	{
		return PlantAttemptResult::InvalidTime;
	}
	if (!actorOverlapsSite)
	{
		active_ = false;
		startedAt_ = 0.0f;
		return PlantAttemptResult::OutsideSite;
	}
	if (active_)
	{
		const float elapsed = now - startedAt_;
		if (!std::isfinite(elapsed) || elapsed < 0.0f)
		{
			active_ = false;
			startedAt_ = 0.0f;
		}
		else if (expired(elapsed))
		{
			active_ = false;
			startedAt_ = 0.0f;
			retryAfter_ = now + kPlantAttemptRetrySeconds;
			return PlantAttemptResult::TimedOut;
		}
		else
		{
			return PlantAttemptResult::Continuing;
		}
	}
	if (!std::isfinite(retryAfter_))
	{
		retryAfter_ = now;
	}
	return now < retryAfter_ ? PlantAttemptResult::RetryPending : PlantAttemptResult::Ready;
}

void PlantAttemptController::recordDispatched(float now) noexcept
{
	if (active_ || !std::isfinite(now) || now < retryAfter_)
	{
		return;
	}
	active_ = true;
	startedAt_ = now;
}

} // namespace objectives
} // namespace astrabot
