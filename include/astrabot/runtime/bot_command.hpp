#ifndef ASTRABOT_RUNTIME_BOT_COMMAND_HPP
#define ASTRABOT_RUNTIME_BOT_COMMAND_HPP

#include "astrabot/runtime/actor_registry.hpp"

#include <cstdint>

namespace astrabot
{
namespace runtime
{
	struct ViewAngles
	{
		float pitch;
		float yaw;
		float roll;
	};

	struct MovementInput
	{
		float forward;
		float side;
		float up;
		std::uint16_t buttons;
		std::uint8_t impulse;
		std::uint8_t msec;
	};

	struct BotCommand
	{
		ActorId actor;
		LifecycleToken lifecycle;
		std::uint32_t sequence;
		std::uint32_t issueFrame;
		ViewAngles viewAngles;
		MovementInput movement;

		bool isValid() const;
	};

	enum class DispatchResult
	{
		Dispatched,
		NoCommand,
		StaleActor,
		EngineUnavailable,
		InvalidCommand
	};

	struct CommandReceipt
	{
		ActorId actor;
		std::uint32_t sequence;
		std::uint32_t dispatchFrame;
		DispatchResult result;
	};
}
}

#endif
