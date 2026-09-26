#include "astrabot/metamod/movement_execution_gate.hpp"

#include <in_buttons.h>

#include <cmath>
#include <cstdint>
#include <iostream>

namespace
{
using astrabot::metamod::MovementExecutionDecision;
using astrabot::metamod::MovementExecutionGate;
using astrabot::metamod::MovementExecutionObservation;
using astrabot::metamod::MovementExecutionPhase;

bool check(bool condition, const char *message)
{
	if (!condition)
	{
		std::cerr << "FAIL: " << message << '\n';
		return false;
	}
	return true;
}

MovementExecutionObservation observation()
{
	MovementExecutionObservation value{};
	value.forward = 120.0f;
	value.side = -40.0f;
	value.up = 8.0f;
	value.buttons = static_cast<std::uint16_t>(
		IN_FORWARD | IN_MOVELEFT | IN_ATTACK | IN_USE | IN_DUCK | IN_JUMP);
	value.msec = 12U;
	value.freezetimeDuck = 0.0f;
	value.freezetimeJump = 0.0f;
	return value;
}
}

int main()
{
	{
		MovementExecutionObservation value = observation();
		value.roundFreezeActive = false;
		value.maxSpeedAvailable = true;
		value.maxSpeed = 1.0f;
		const MovementExecutionDecision result = MovementExecutionGate::evaluate(value);
		if (!check(result.phase == MovementExecutionPhase::Live &&
				result.forward == value.forward && result.buttons == value.buttons &&
				result.msec == value.msec,
				"stale low maxspeed outside round freeze passes movement"))
		{
			return 1;
		}
	}

	{
		MovementExecutionObservation value = observation();
		value.explicitFrozen = true;
		const MovementExecutionDecision result = MovementExecutionGate::evaluate(value);
		if (!check(result.phase == MovementExecutionPhase::ControlFrozen &&
				result.forward == 0.0f && result.side == 0.0f && result.up == 0.0f &&
				result.buttons == 0U && result.msec == 0U && result.invalidateTemplate,
				"explicit engine frozen state is fully neutralized"))
		{
			return 1;
		}
	}

	{
		MovementExecutionObservation value = observation();
		value.roundFreezeActive = true;
		value.maxSpeedAvailable = true;
		value.maxSpeed = 1.0f;
		const MovementExecutionDecision result = MovementExecutionGate::evaluate(value);
		const std::uint16_t expected = static_cast<std::uint16_t>(IN_USE);
		if (!check(result.phase == MovementExecutionPhase::RoundFreeze &&
				result.forward == 0.0f && result.side == 0.0f && result.up == 0.0f &&
				result.buttons == expected && result.msec == value.msec &&
				result.invalidateTemplate,
				"authoritative round freeze removes movement and attack but preserves use"))
		{
			return 1;
		}
	}

	{
		MovementExecutionObservation value = observation();
		value.roundFreezeActive = true;
		value.maxSpeedAvailable = true;
		value.maxSpeed = 1.0f;
		value.freezetimeDuck = 1.0f;
		value.freezetimeJump = 1.0f;
		const MovementExecutionDecision result = MovementExecutionGate::evaluate(value);
		const std::uint16_t expected = static_cast<std::uint16_t>(IN_USE | IN_DUCK | IN_JUMP);
		if (!check(result.phase == MovementExecutionPhase::RoundFreeze &&
				result.buttons == expected && result.msec == value.msec,
				"round freeze posture allowance follows public CVars"))
		{
			return 1;
		}
	}

	{
		MovementExecutionObservation value = observation();
		value.roundFreezeActive = true;
		value.maxSpeedAvailable = true;
		value.maxSpeed = 240.0f;
		const MovementExecutionDecision result = MovementExecutionGate::evaluate(value);
		if (!check(result.phase == MovementExecutionPhase::RoundFreeze &&
				result.forward == 0.0f && result.side == 0.0f && result.up == 0.0f &&
				result.msec == value.msec,
				"tracked round freeze remains authoritative after maxspeed reset"))
		{
			return 1;
		}
	}

	{
		MovementExecutionObservation value = observation();
		value.maxSpeedAvailable = true;
		value.maxSpeed = 240.0f;
		const MovementExecutionDecision result = MovementExecutionGate::evaluate(value);
		if (!check(result.phase == MovementExecutionPhase::Live &&
				result.forward == value.forward && result.side == value.side &&
				result.up == value.up && result.buttons == value.buttons &&
				result.msec == value.msec && !result.invalidateTemplate,
				"live state passes the current command through"))
		{
			return 1;
		}
	}

	{
		const astrabot::metamod::MovementVelocity previous = {72.0f, -31.0f, -120.0f};
		const astrabot::metamod::MovementVelocity frozen =
			MovementExecutionGate::stabilizeVelocity(
				MovementExecutionPhase::RoundFreeze, previous);
		if (!check(frozen.x == 0.0f && frozen.y == 0.0f && frozen.z == previous.z,
				"round freeze clears horizontal drift and preserves vertical falling"))
		{
			return 1;
		}
		const astrabot::metamod::MovementVelocity live =
			MovementExecutionGate::stabilizeVelocity(
				MovementExecutionPhase::Live, previous);
		if (!check(live.x == previous.x && live.y == previous.y && live.z == previous.z,
				"live movement preserves velocity"))
		{
			return 1;
		}
	}
	std::cout << "movement execution gate: PASS\n";
	return 0;
}
