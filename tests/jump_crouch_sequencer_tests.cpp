#include "astrabot/runtime/jump_crouch_sequencer.hpp"

#include <array>
#include <cstdio>

namespace
{
using astrabot::runtime::JumpCrouchDecision;
using astrabot::runtime::JumpCrouchObservation;
using astrabot::runtime::JumpCrouchPhase;
using astrabot::runtime::JumpCrouchSequencer;
using astrabot::runtime::JumpCrouchTransition;

bool check(bool condition, const char *description)
{
	if (condition)
	{
		return true;
	}
	std::fprintf(stderr, "check failed: %s\n", description);
	return false;
}

JumpCrouchObservation observation(
	float time,
	bool grounded,
	bool jumpRequested = false)
{
	return {time, true, grounded, false, false, false, jumpRequested};
}

bool testDelayedDuckReleaseAndTimeout()
{
	JumpCrouchSequencer sequencer;
	JumpCrouchDecision decision = sequencer.update(observation(10.0f, true, true));
	if (!check(decision.transition == JumpCrouchTransition::Armed &&
			decision.phase == JumpCrouchPhase::AwaitingAirborne &&
			!decision.applyDuck,
		"grounded Jump arms without immediate duck"))
	{
		return false;
	}

	decision = sequencer.update(observation(10.049f, false));
	if (!check(decision.transition == JumpCrouchTransition::None &&
			!decision.applyDuck,
		"airborne input before 0.05 seconds remains standing"))
	{
		return false;
	}

	decision = sequencer.update(observation(10.051f, false));
	if (!check(decision.transition == JumpCrouchTransition::DuckStarted &&
			decision.phase == JumpCrouchPhase::Crouching &&
			decision.applyDuck,
		"airborne input after 0.05 seconds starts duck"))
	{
		return false;
	}

	decision = sequencer.update(observation(10.599f, false));
	if (!check(decision.transition == JumpCrouchTransition::None &&
			decision.applyDuck,
		"duck remains held before 0.60 seconds"))
	{
		return false;
	}

	decision = sequencer.update(observation(10.601f, false));
	if (!check(decision.transition == JumpCrouchTransition::DuckReleased &&
			decision.phase == JumpCrouchPhase::Crouching &&
			!decision.applyDuck,
		"duck releases after 0.60 seconds while traversal remains active"))
	{
		return false;
	}

	decision = sequencer.update(observation(10.751f, false));
	return check(decision.transition == JumpCrouchTransition::Timeout &&
			decision.phase == JumpCrouchPhase::Idle && !decision.applyDuck,
		"jump-crouch state times out after 0.75 seconds");
}

bool testLandingAndCancellationResetState()
{
	JumpCrouchSequencer sequencer;
	sequencer.update(observation(20.0f, true, true));
	sequencer.update(observation(20.051f, false));
	JumpCrouchDecision decision = sequencer.update(observation(20.2f, true));
	if (!check(decision.transition == JumpCrouchTransition::Landed &&
			decision.phase == JumpCrouchPhase::Idle && !decision.applyDuck,
		"landing clears active jump-crouch state"))
	{
		return false;
	}

	std::array<JumpCrouchObservation, 4U> cancellations = {{
		{30.1f, false, false, false, false, false, false},
		{30.1f, true, false, true, false, false, false},
		{30.1f, true, false, false, true, false, false},
		{30.1f, true, false, false, false, true, false}}};
	for (const JumpCrouchObservation &cancel : cancellations)
	{
		sequencer.update(observation(30.0f, true, true));
		decision = sequencer.update(cancel);
		if (!check(decision.transition == JumpCrouchTransition::Cancelled &&
				decision.phase == JumpCrouchPhase::Idle && !decision.applyDuck,
			"invalid actor, death, ladder, and freeze cancel state"))
		{
			return false;
		}
	}
	return true;
}

bool testResetAndRearm()
{
	JumpCrouchSequencer sequencer;
	sequencer.update(observation(40.0f, true, true));
	sequencer.reset();
	if (!check(sequencer.phase() == JumpCrouchPhase::Idle,
		"lifecycle reset returns sequencer to Idle"))
	{
		return false;
	}

	const JumpCrouchDecision decision =
		sequencer.update(observation(41.0f, true, true));
	return check(decision.transition == JumpCrouchTransition::Armed &&
			decision.phase == JumpCrouchPhase::AwaitingAirborne,
		"a new Jump can arm after reset or previous completion");
}
} // namespace

int main()
{
	return testDelayedDuckReleaseAndTimeout() &&
		testLandingAndCancellationResetState() && testResetAndRearm() ? 0 : 1;
}
