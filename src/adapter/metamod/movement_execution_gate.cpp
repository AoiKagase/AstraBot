#include "astrabot/metamod/movement_execution_gate.hpp"

#include <in_buttons.h>

#include <cmath>

namespace astrabot
{
namespace metamod
{
MovementExecutionDecision MovementExecutionGate::evaluate(
	const MovementExecutionObservation &observation) noexcept
{
	MovementExecutionDecision result = {
		MovementExecutionPhase::Live,
		observation.forward,
		observation.side,
		observation.up,
		observation.buttons,
		observation.msec,
		false};
	if (observation.explicitFrozen)
	{
		result.phase = MovementExecutionPhase::ControlFrozen;
		result.forward = 0.0f;
		result.side = 0.0f;
		result.up = 0.0f;
		result.buttons = 0U;
		result.msec = 0U;
		result.invalidateTemplate = true;
		return result;
	}

	const bool roundFreeze = observation.maxSpeedAvailable &&
		std::isfinite(observation.maxSpeed) && observation.maxSpeed > 0.0f &&
		observation.maxSpeed <= 1.0f;
	if (!roundFreeze)
	{
		return result;
	}

	result.phase = MovementExecutionPhase::RoundFreeze;
	result.forward = 0.0f;
	result.side = 0.0f;
	result.up = 0.0f;
	result.buttons = static_cast<std::uint16_t>(
		result.buttons & ~(IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT | IN_ATTACK));
	if (!(std::isfinite(observation.freezetimeDuck) && observation.freezetimeDuck > 0.0f))
	{
		result.buttons = static_cast<std::uint16_t>(result.buttons & ~IN_DUCK);
	}
	if (!(std::isfinite(observation.freezetimeJump) && observation.freezetimeJump > 0.0f))
	{
		result.buttons = static_cast<std::uint16_t>(result.buttons & ~IN_JUMP);
	}
	result.invalidateTemplate = true;
	return result;
}
}
}
