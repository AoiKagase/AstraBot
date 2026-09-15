#include "astrabot/runtime/lifecycle.hpp"

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
}

int main()
{
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
