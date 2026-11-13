#ifndef ASTRABOT_OBJECTIVES_PLANT_ATTEMPT_HPP
#define ASTRABOT_OBJECTIVES_PLANT_ATTEMPT_HPP

#include <cstdint>

namespace astrabot
{
namespace objectives
{
enum class PlantAttemptResult : std::uint8_t
{
	OutsideSite,
	RetryPending,
	Ready,
	Continuing,
	TimedOut,
	Confirmed,
	InvalidTime
};

// Delivered-input attempt timing is distinct from CompatibilityStateMachine's
// PlantBomb state residence timer. Repeated delivered inputs do not restart it.
class PlantAttemptController
{
public:
	static constexpr float kPlantAttemptTimeoutSeconds = 5.0f;
	static constexpr float kPlantAttemptRetrySeconds = 1.0f;
	static bool expired(float elapsedSeconds) noexcept;
	static bool canBeginObjective(bool hasPlantAssignment, bool carryingBomb,
		bool isTerrorist, bool selectedTargetAvailable) noexcept;
	PlantAttemptResult evaluate(bool actorOverlapsSite, bool plantedConfirmed, float now) noexcept;
	void recordDispatched(float now) noexcept;
	bool active() const noexcept
	{
		return active_;
	}

private:
	bool active_ = false;
	float startedAt_ = 0.0f;
	float retryAfter_ = 0.0f;
};
} // namespace objectives
} // namespace astrabot

#endif
