#ifndef ASTRABOT_ADAPTER_METAMOD_MOVEMENT_EXECUTION_GATE_HPP
#define ASTRABOT_ADAPTER_METAMOD_MOVEMENT_EXECUTION_GATE_HPP

#include <cstdint>

namespace astrabot
{
namespace metamod
{
enum class MovementExecutionPhase
{
	Live,
	RoundFreeze,
	ControlFrozen
};

struct MovementExecutionObservation
{
	bool explicitFrozen;
	bool maxSpeedAvailable;
	float maxSpeed;
	float forward;
	float side;
	float up;
	std::uint16_t buttons;
	std::uint8_t msec;
	float freezetimeDuck;
	float freezetimeJump;
};

struct MovementExecutionDecision
{
	MovementExecutionPhase phase;
	float forward;
	float side;
	float up;
	std::uint16_t buttons;
	std::uint8_t msec;
	bool invalidateTemplate;
};

class MovementExecutionGate
{
public:
	static MovementExecutionDecision evaluate(
		const MovementExecutionObservation &observation) noexcept;
};
}
}

#endif
