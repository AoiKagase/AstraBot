#include "astrabot/runtime/bot_timing_scheduler.hpp"

#include <cmath>
#include <cstdio>

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

bool closeEnough(float left, float right)
{
	return std::fabs(left - right) < 0.000001f;
}
}

int main()
{
	using astrabot::runtime::BotTimingEvent;
	using astrabot::runtime::BotTimingScheduler;

	BotTimingScheduler scheduler;
	scheduler.reset(10.0f);
	const astrabot::runtime::BotTimingStep beforeDue = scheduler.advance(10.0f);
	if (!check(beforeDue.eventCount == 0U, "no timing event occurs before the first command deadline"))
	{
		return 1;
	}

	const float firstDueTime = 10.0334f;
	const astrabot::runtime::BotTimingStep firstDue = scheduler.advance(firstDueTime);
	if (!check(firstDue.eventCount == 2U, "first command due frame emits two events") ||
		!check(firstDue.events[0] == BotTimingEvent::Upkeep, "upkeep is the first timing event") ||
		!check(firstDue.events[1] == BotTimingEvent::CommandExecute,
			   "command execute follows upkeep when full update is not due") ||
		!check(closeEnough(
			   scheduler.nextCommandDeadline(),
			   firstDueTime + BotTimingScheduler::kCommandInterval),
			   "command deadline is rebased from the current time") ||
		!check(closeEnough(
			   scheduler.nextFullUpdateDeadline(),
			   10.0f + BotTimingScheduler::kFullUpdateInterval),
			   "full update deadline is not changed by a command-only tick"))
	{
		return 1;
	}

	const float fullDueTime = 10.1001f;
	const astrabot::runtime::BotTimingStep fullDue = scheduler.advance(fullDueTime);
	if (!check(fullDue.eventCount == 4U, "full due frame emits four ordered timing events") ||
		!check(fullDue.events[0] == BotTimingEvent::Upkeep, "upkeep is the first full due event") ||
		!check(fullDue.events[1] == BotTimingEvent::CommandReset,
			   "command reset follows upkeep when full update is due") ||
		!check(fullDue.events[2] == BotTimingEvent::FullUpdate,
			   "full update follows command reset") ||
		!check(fullDue.events[3] == BotTimingEvent::CommandExecute,
			   "command execute is the final full due event") ||
		!check(closeEnough(
			   scheduler.nextCommandDeadline(),
			   fullDueTime + BotTimingScheduler::kCommandInterval),
			   "command deadline is rebased after the full due frame") ||
		!check(closeEnough(
			   scheduler.nextFullUpdateDeadline(),
			   fullDueTime + BotTimingScheduler::kFullUpdateInterval),
			   "full update deadline is rebased from the current time"))
	{
		return 1;
	}

	return 0;
}
