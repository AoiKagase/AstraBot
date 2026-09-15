#include "astrabot/runtime/bot_command.hpp"

#include <cmath>

namespace astrabot
{
namespace runtime
{
	namespace
	{
		constexpr float kMaxAngle = 360.0f;
		constexpr float kMaxMovement = 1000.0f;

		bool isFiniteAndBounded(float value, float absoluteLimit)
		{
			return std::isfinite(value) && std::fabs(value) <= absoluteLimit;
		}
	}

	bool BotCommand::isValid() const
	{
		return actor.slot >= LifecycleSession::kFirstClientSlot &&
			actor.slot <= LifecycleSession::kLastClientSlot &&
			actor.actorGeneration != LifecycleSession::kInvalidGeneration &&
			lifecycle.slot == actor.slot &&
			lifecycle.mapGeneration != LifecycleSession::kInvalidGeneration &&
			lifecycle.roundGeneration != LifecycleSession::kInvalidGeneration &&
			lifecycle.slotGeneration != LifecycleSession::kInvalidGeneration &&
			sequence != 0U &&
			isFiniteAndBounded(viewAngles.pitch, kMaxAngle) &&
			isFiniteAndBounded(viewAngles.yaw, kMaxAngle) &&
			isFiniteAndBounded(viewAngles.roll, kMaxAngle) &&
			isFiniteAndBounded(movement.forward, kMaxMovement) &&
			isFiniteAndBounded(movement.side, kMaxMovement) &&
			isFiniteAndBounded(movement.up, kMaxMovement) &&
			movement.msec != 0U;
	}
}
}
