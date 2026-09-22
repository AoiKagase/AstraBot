#include "astrabot/runtime/jump_crouch_sequencer.hpp"

#include <cmath>

namespace astrabot
{
namespace runtime
{
constexpr float JumpCrouchSequencer::kDuckDelay;
constexpr float JumpCrouchSequencer::kDuckRelease;
constexpr float JumpCrouchSequencer::kTraversalTimeout;

JumpCrouchSequencer::JumpCrouchSequencer()
	: phase_(JumpCrouchPhase::Idle), launchTime_(0.0f), duckStarted_(false),
	  duckReleased_(false)
{
}

JumpCrouchDecision JumpCrouchSequencer::update(
	const JumpCrouchObservation &observation)
{
	if (phase_ != JumpCrouchPhase::Idle &&
		(!observation.actorValid || observation.dead || observation.onLadder ||
			observation.movementFrozen))
	{
		const float elapsed = std::isfinite(observation.time)
			? observation.time - launchTime_
			: 0.0f;
		return finish(JumpCrouchTransition::Cancelled, elapsed);
	}

	if (phase_ == JumpCrouchPhase::Idle)
	{
		if (observation.actorValid && !observation.dead && !observation.onLadder &&
			!observation.movementFrozen && observation.grounded &&
			observation.jumpRequested && std::isfinite(observation.time))
		{
			phase_ = JumpCrouchPhase::AwaitingAirborne;
			launchTime_ = observation.time;
			duckStarted_ = false;
			duckReleased_ = false;
			return {phase_, JumpCrouchTransition::Armed, false, 0.0f};
		}
		return {phase_, JumpCrouchTransition::None, false, 0.0f};
	}

	const float elapsed = observation.time - launchTime_;
	if (!std::isfinite(elapsed) || elapsed < 0.0f)
	{
		return finish(JumpCrouchTransition::Cancelled, 0.0f);
	}

	if (phase_ == JumpCrouchPhase::AwaitingAirborne)
	{
		if (elapsed >= kTraversalTimeout)
		{
			return finish(JumpCrouchTransition::Timeout, elapsed);
		}
		if (observation.grounded)
		{
			return {phase_, JumpCrouchTransition::None, false, elapsed};
		}
		phase_ = JumpCrouchPhase::Crouching;
		if (elapsed >= kDuckRelease)
		{
			duckReleased_ = true;
			return {phase_, JumpCrouchTransition::DuckReleased, false, elapsed};
		}
		if (elapsed >= kDuckDelay)
		{
			duckStarted_ = true;
			return {phase_, JumpCrouchTransition::DuckStarted, true, elapsed};
		}
		return {phase_, JumpCrouchTransition::None, false, elapsed};
	}

	if (observation.grounded)
	{
		return finish(JumpCrouchTransition::Landed, elapsed);
	}
	if (elapsed >= kTraversalTimeout)
	{
		return finish(JumpCrouchTransition::Timeout, elapsed);
	}
	if (!duckReleased_ && elapsed >= kDuckRelease)
	{
		duckReleased_ = true;
		return {phase_, JumpCrouchTransition::DuckReleased, false, elapsed};
	}
	if (!duckStarted_ && elapsed >= kDuckDelay)
	{
		duckStarted_ = true;
		return {phase_, JumpCrouchTransition::DuckStarted, true, elapsed};
	}
	return {
		phase_,
		JumpCrouchTransition::None,
		duckStarted_ && !duckReleased_,
		elapsed};
}

JumpCrouchCommandDecision JumpCrouchSequencer::updateCommand(
	const JumpCrouchObservation &observation,
	std::uint16_t buttons,
	std::uint16_t duckButton)
{
	const JumpCrouchDecision sequencing = update(observation);
	if (sequencing.applyDuck)
	{
		buttons = static_cast<std::uint16_t>(buttons | duckButton);
	}
	return {sequencing, buttons};
}

void JumpCrouchSequencer::reset()
{
	phase_ = JumpCrouchPhase::Idle;
	launchTime_ = 0.0f;
	duckStarted_ = false;
	duckReleased_ = false;
}

JumpCrouchPhase JumpCrouchSequencer::phase() const
{
	return phase_;
}

JumpCrouchDecision JumpCrouchSequencer::finish(
	JumpCrouchTransition transition,
	float elapsed)
{
	reset();
	return {JumpCrouchPhase::Idle, transition, false, elapsed};
}
}
}
