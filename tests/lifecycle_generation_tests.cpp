#include "astrabot/runtime/lifecycle.hpp"
#include "astrabot/metamod/round_lifecycle_tracker.hpp"

#include <cstdio>
#include <limits>

namespace
{
bool check(bool condition, const char *description)
	{
		if (condition)
		{
			return true;
		}

		std::fprintf(stderr, "check failed: %s\n", description);
		return false;
}

bool testRoundLifecycleSignalsAdvanceExactlyOncePerRound()
{
	using astrabot::metamod::RoundLifecycleEvent;
	using astrabot::metamod::RoundLifecycleTracker;
	RoundLifecycleTracker tracker;
	if (!check(tracker.shouldBeginNewGeneration(RoundLifecycleEvent::RoundStart),
			"first Round_Start begins a generation when no prepare event arrived"))
	{
		return false;
	}
	if (!check(!tracker.shouldBeginNewGeneration(RoundLifecycleEvent::RoundStart),
			"duplicate Round_Start does not double-advance the generation"))
	{
		return false;
	}
	if (!check(tracker.shouldBeginNewGeneration(RoundLifecycleEvent::RoundEnd),
			"Round_End invalidates the completed round state"))
	{
		return false;
	}
	if (!check(!tracker.shouldBeginNewGeneration(RoundLifecycleEvent::RoundStart),
			"the next Round_Start consumes the generation prepared at Round_End"))
	{
		return false;
	}
	if (!check(tracker.shouldBeginNewGeneration(RoundLifecycleEvent::RoundEnd) &&
			!tracker.shouldBeginNewGeneration(RoundLifecycleEvent::RestartScheduled) &&
			!tracker.shouldBeginNewGeneration(RoundLifecycleEvent::RoundStart),
			"restart scheduling and Round_Start do not double-advance a round"))
	{
		return false;
	}
	tracker.reset();
	return check(tracker.shouldBeginNewGeneration(RoundLifecycleEvent::GameCommencing) &&
			!tracker.shouldBeginNewGeneration(RoundLifecycleEvent::RoundStart),
			"Game_Commencing preparation is consumed by the first Round_Start");
}

bool testRoundFreezeWindowUsesRoundStartAndFiniteExpiry()
{
	using astrabot::metamod::RoundFreezeSchedule;
	const RoundFreezeSchedule freeze = RoundFreezeSchedule::create(20.0f, 5.0f);
	if (!check(freeze.scheduled && freeze.isActive(20.0f) &&
			freeze.isActive(24.99f) && !freeze.isActive(25.0f),
			"freezetime is active from Round_Start through its exclusive expiry"))
	{
		return false;
	}
	const RoundFreezeSchedule noFreeze = RoundFreezeSchedule::create(20.0f, -1.0f);
	const RoundFreezeSchedule capped = RoundFreezeSchedule::create(20.0f, 120.0f);
	return check(!noFreeze.scheduled && !noFreeze.isActive(20.0f),
			"negative freezetime produces no freeze") &&
		check(capped.scheduled && capped.untilTime == 80.0f,
			"freezetime is bounded to sixty seconds");
}
}

int main()
{
	if (!testRoundLifecycleSignalsAdvanceExactlyOncePerRound())
	{
		return 1;
	}
	if (!testRoundFreezeWindowUsesRoundStartAndFiniteExpiry())
	{
		return 1;
	}
	using astrabot::runtime::LifecycleSession;
	using astrabot::runtime::LifecycleToken;

	LifecycleSession session;
	if (!check(session.mapGeneration() == 0U, "new session has no map generation"))
	{
		return 1;
	}
	if (!check(session.activateMap(), "map activation succeeds"))
	{
		return 1;
	}
	const LifecycleSession::Generation mapGeneration = session.mapGeneration();
	const LifecycleSession::Generation roundGeneration = session.roundGeneration();
	if (!check(mapGeneration != 0U, "map activation creates a valid generation"))
	{
		return 1;
	}
	if (!check(roundGeneration != 0U, "map activation creates an initial round"))
	{
		return 1;
	}
	if (!check(session.connectSlot(1U), "first slot connection succeeds"))
	{
		return 1;
	}
	const LifecycleToken firstToken = session.tokenForSlot(1U);
	if (!check(session.isCurrent(firstToken), "connected slot token is current"))
	{
		return 1;
	}
	if (!check(session.beginRound(), "explicit round start succeeds"))
	{
		return 1;
	}
	if (!check(!session.isCurrent(firstToken), "round start invalidates old token"))
	{
		return 1;
	}
	if (!check(session.disconnectSlot(1U), "slot disconnect succeeds"))
	{
		return 1;
	}
	if (!check(!session.disconnectSlot(1U), "repeated slot disconnect is harmless"))
	{
		return 1;
	}
	if (!check(!session.isCurrent(session.tokenForSlot(1U)), "disconnected slot has no current token"))
	{
		return 1;
	}
	if (!check(session.connectSlot(1U), "slot reuse succeeds"))
	{
		return 1;
	}
	const LifecycleToken reusedToken = session.tokenForSlot(1U);
	if (!check(reusedToken.slotGeneration != firstToken.slotGeneration,
			"slot reuse advances slot generation"))
	{
		return 1;
	}
	if (!check(session.isCurrent(reusedToken), "reused slot token is current"))
	{
		return 1;
	}

	const LifecycleSession::Generation beforeFrameReset = session.roundGeneration();
	if (!check(session.observeFrame(100U, 1.0f), "first frame observation succeeds"))
	{
		return 1;
	}
	if (!check(session.observeFrame(101U, 1.1f), "monotonic frame observation succeeds"))
	{
		return 1;
	}
	if (!check(session.roundGeneration() == beforeFrameReset,
			"monotonic frame does not change round generation"))
	{
		return 1;
	}
	if (!check(session.observeFrame(0U, 0.0f), "frame reset observation succeeds"))
	{
		return 1;
	}
	if (!check(session.roundGeneration() != beforeFrameReset,
			"frame reset invalidates the old round"))
	{
		return 1;
	}
	if (!check(!session.observeFrame(1U, std::numeric_limits<float>::quiet_NaN()),
			"non-finite frame observation is rejected"))
	{
		return 1;
	}

	const LifecycleToken mapToken = session.tokenForSlot(1U);
	session.deactivateMap();
	if (!check(!session.isCurrent(mapToken), "map deactivation invalidates tokens"))
	{
		return 1;
	}
	session.deactivateMap();
	if (!check(!session.beginRound(), "round start after deactivation is rejected"))
	{
		return 1;
	}
	if (!check(!session.connectSlot(2U), "slot connection after deactivation is rejected"))
	{
		return 1;
	}
	return 0;
}
