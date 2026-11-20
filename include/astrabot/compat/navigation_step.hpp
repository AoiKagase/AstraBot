#ifndef ASTRABOT_COMPAT_NAVIGATION_STEP_HPP
#define ASTRABOT_COMPAT_NAVIGATION_STEP_HPP

#include "astrabot/world/world_snapshot.hpp"

namespace astrabot
{
namespace compat
{
enum class NavigationStepOutcome : std::uint8_t
{
	NotEvaluated,
	Progressing,
	EndOfPath,
	PathFailure,
	ObservationUnavailable,
	ObjectivePathPrepared
};
struct NavigationStep
{
	world::ActorKey actor{};
	world::FrameIdentity frame{};
	std::uint32_t goalGeneration = 0U;
	std::uint32_t pathSequence = 0U;
	std::uint32_t fullUpdateSequence = 0U;
	NavigationStepOutcome outcome = NavigationStepOutcome::NotEvaluated;
	bool pathDestroyedAfterMovement = false;
};
enum class DefuseReceipt : std::uint8_t
{
	Unknown,
	Self,
	Other,
	None
};
}
}
#endif
