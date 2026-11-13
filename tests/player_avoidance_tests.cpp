#include "astrabot/runtime/player_avoidance.hpp"

#include <cstdio>
#include <limits>

namespace
{
using astrabot::runtime::ActorId;
using astrabot::runtime::PlayerAvoidance;
using astrabot::runtime::PlayerAvoidanceTouch;
using astrabot::runtime::playerAvoidancePriority;
using astrabot::world::FrameIdentity;

bool check(bool value, const char *message)
{
	if (!value)
		std::fprintf(stderr, "%s\n", message);
	return value;
}

PlayerAvoidanceTouch contact()
{
	return {{3U, 7U}, {2U, 4U, 10U}, {7U, 9U}, 12U, 8U, 0.0f, false};
}

bool prioritiesAndDuration()
{
	if (!check(playerAvoidancePriority(true, true, 9U) == 0U, "humans precede scenario bots") ||
		!check(playerAvoidancePriority(false, true, 9U) == 1U, "scenario bots precede ordinary bots") ||
		!check(playerAvoidancePriority(false, false, 9U) == 10U, "ordinary priority uses creation ID") ||
		!check(playerAvoidancePriority(false, false, 0U) == std::numeric_limits<std::uint32_t>::max(),
			"unknown creation ID has lowest priority"))
		return false;
	PlayerAvoidance avoidance;
	auto touch = contact();
	ActorId other{};
	return check(avoidance.touch(touch), "higher-priority player touch starts avoidance") &&
		check(avoidance.target(touch.actor, {2U, 4U, 11U}, 0.329f, &other) && other.slot == 7U &&
				other.actorGeneration == 9U,
			"avoidance retains the touched actor generation across ticks") &&
		check(!avoidance.target(touch.actor, touch.frame, 0.33f, &other), "avoidance expires at 0.33 seconds");
}

bool priorityAndDefusingGates()
{
	PlayerAvoidance avoidance;
	auto touch = contact();
	touch.selfPriority = 1U;
	if (!check(!avoidance.touch(touch), "higher-priority self does not yield"))
		return false;
	touch.selfPriority = 8U;
	if (!check(avoidance.touch(touch), "equal priorities yield as in BotTouch"))
		return false;
	avoidance.reset();
	touch.defusing = true;
	ActorId other{};
	return check(!avoidance.touch(touch) && !avoidance.target(touch.actor, touch.frame, 0.1f, &other),
		"defusing bot ignores touch");
}

bool strongerIncumbentAndRefresh()
{
	PlayerAvoidance avoidance;
	auto touch = contact();
	ActorId other{};
	if (!avoidance.touch(touch))
		return false;
	touch.other = {8U, 2U};
	touch.otherPriority = 10U;
	touch.now = 0.2f;
	if (!check(!avoidance.touch(touch), "weaker contact does not replace stronger incumbent") ||
		!check(avoidance.target(touch.actor, touch.frame, 0.25f, &other) && other.slot == 7U,
			"incumbent remains selected") ||
		!check(!avoidance.target(touch.actor, touch.frame, 0.34f, &other),
			"ignored contact does not extend incumbent lifetime"))
		return false;
	if (!check(avoidance.touch(touch), "expired stronger incumbent cannot veto new touch"))
		return false;
	touch.now = 0.4f;
	if (!avoidance.touch(touch))
		return false;
	return check(avoidance.target(touch.actor, touch.frame, 0.6f, &other) && other.slot == 8U,
		"repeated accepted touch refreshes lifetime");
}

bool lifecycleAndInvalidInputs()
{
	const auto touch = contact();
	ActorId other{};
	const ActorId owners[] = {{3U, 8U}, {4U, 7U}};
	for (const auto &owner : owners)
	{
		PlayerAvoidance avoidance;
		avoidance.touch(touch);
		if (!check(!avoidance.target(owner, touch.frame, 0.1f, &other), "owner reuse invalidates avoidance"))
			return false;
	}
	const FrameIdentity frames[] = {{3U, 4U, 10U}, {2U, 5U, 10U}};
	for (const auto &frame : frames)
	{
		PlayerAvoidance avoidance;
		avoidance.touch(touch);
		if (!check(
				!avoidance.target(touch.actor, frame, 0.1f, &other), "map or round transition invalidates avoidance"))
			return false;
	}
	PlayerAvoidance avoidance;
	avoidance.touch(touch);
	if (!check(!avoidance.target(touch.actor, touch.frame, -0.1f, &other), "time reversal clears avoidance"))
		return false;
	auto invalid = touch;
	invalid.other = touch.actor;
	if (!check(!avoidance.touch(invalid), "self contact rejected"))
		return false;
	invalid = touch;
	invalid.now = std::numeric_limits<float>::quiet_NaN();
	return check(!avoidance.touch(invalid), "nonfinite contact time rejected");
}

bool incumbentPriorityTracksScenarioChange()
{
	PlayerAvoidance avoidance;
	auto touch = contact();
	touch.otherPriority = 1U;
	if (!avoidance.touch(touch))
		return false;
	touch.other = {8U, 2U};
	touch.otherPriority = 6U;
	touch.currentTargetPriority = 10U;
	touch.currentTargetPriorityKnown = true;
	touch.now = 0.1f;
	ActorId other{};
	return check(avoidance.touch(touch) && avoidance.target(touch.actor, touch.frame, 0.2f, &other) && other.slot == 8U,
		"former C4 carrier loses precedence when incumbent live priority changes");
}
}

bool fixedReferenceProjection()
{
	using astrabot::runtime::playerAvoidanceMove;
	struct Case
	{
		float yaw, x, y, forward, side;
	};
	const Case cases[] = {{0, 1, 0, -240, 0}, {0, -1, 0, 240, 0}, {0, 0, 1, 0, 240}, {0, 0, -1, 0, -240},
		{0, 1, 1, -240, 240}, {90, 0, 1, -240, 0}, {90, 1, 0, 0, -240}, {180, -1, 0, -240, 0}, {270, 0, -1, -240, 0},
		{0, 1, 0.4f, -240, 0}, {0, 1, 0.8f, -240, 240}};
	for (const auto &c : cases)
	{
		const auto move = playerAvoidanceMove(c.yaw, c.x, c.y, 0, 0);
		if (!check(move.valid && move.forward == c.forward && move.side == c.side,
				"fixed-reference make-way retreats along both view axes and respects the deadband"))
			return false;
	}
	const auto lateral = playerAvoidanceMove(0, 0, 1, 75, 0);
	return check(lateral.valid && lateral.forward == 75 && lateral.side == 240,
			   "make-way preserves the path's unaffected forward axis") &&
		check(!playerAvoidanceMove(0, 0, 0, 0, 0).valid &&
				!playerAvoidanceMove(std::numeric_limits<float>::infinity(), 1, 0, 0, 0).valid,
			"invalid make-way geometry cannot produce a movement replacement");
}

int main()
{
	return prioritiesAndDuration() && priorityAndDefusingGates() && strongerIncumbentAndRefresh() &&
			lifecycleAndInvalidInputs() && incumbentPriorityTracksScenarioChange() && fixedReferenceProjection()
		? 0
		: 1;
}
