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

	const float firstDueTime = 10.0333334f;
	const astrabot::runtime::BotTimingStep firstDue = scheduler.advance(firstDueTime);
	if (!check(firstDue.eventCount == 4U, "first due frame emits four ordered timing events") ||
		!check(firstDue.events[0] == BotTimingEvent::Upkeep, "upkeep is the first timing event") ||
		!check(firstDue.events[1] == BotTimingEvent::CommandReset,
			   "command reset follows upkeep when full update is due") ||
		!check(firstDue.events[2] == BotTimingEvent::FullUpdate,
			   "full update follows command reset") ||
		!check(firstDue.events[3] == BotTimingEvent::CommandExecute,
			   "command execute is the final timing event") ||
		!check(closeEnough(
			   scheduler.nextCommandDeadline(),
			   firstDueTime + BotTimingScheduler::kCommandInterval),
			   "command deadline is rebased from the current time") ||
		!check(closeEnough(
			   scheduler.nextFullUpdateDeadline(),
			   firstDueTime + BotTimingScheduler::kFullUpdateInterval),
			   "full update deadline is rebased from the current time"))
	{
		return 1;
	}

	return 0;
}
