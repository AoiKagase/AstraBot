#ifndef ASTRABOT_RUNTIME_JUMP_CROUCH_SEQUENCER_HPP
#define ASTRABOT_RUNTIME_JUMP_CROUCH_SEQUENCER_HPP

#include <cstdint>

namespace astrabot
{
namespace runtime
{
enum class JumpCrouchPhase
{
	Idle,
	AwaitingAirborne,
	Crouching
};

enum class JumpCrouchTransition
{
	None,
	Armed,
	DuckStarted,
	DuckReleased,
	Landed,
	Timeout,
	Cancelled
};

struct JumpCrouchObservation
{
	float time;
	bool actorValid;
	bool grounded;
	bool dead;
	bool onLadder;
	bool movementFrozen;
	bool jumpRequested;
};

struct JumpCrouchDecision
{
	JumpCrouchPhase phase;
	JumpCrouchTransition transition;
	bool applyDuck;
	float elapsed;
};

struct JumpCrouchCommandDecision
{
	JumpCrouchDecision sequencing;
	std::uint16_t buttons;
};

class JumpCrouchSequencer
{
public:
	static constexpr float kDuckDelay = 0.05f;
	static constexpr float kDuckRelease = 0.60f;
	static constexpr float kTraversalTimeout = 0.75f;

	JumpCrouchSequencer();

	JumpCrouchDecision update(const JumpCrouchObservation &observation);
	JumpCrouchCommandDecision updateCommand(
		const JumpCrouchObservation &observation,
		std::uint16_t buttons,
		std::uint16_t duckButton);
	void reset();
	JumpCrouchPhase phase() const;

private:
	JumpCrouchDecision finish(
		JumpCrouchTransition transition,
		float elapsed);

	JumpCrouchPhase phase_;
	float launchTime_;
	bool duckStarted_;
	bool duckReleased_;
};
}
}

#endif
