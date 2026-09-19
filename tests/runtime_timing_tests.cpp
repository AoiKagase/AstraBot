#include "astrabot/runtime/bot_timing_scheduler.hpp"

#include <array>
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <vector>

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

bool checkEvents(
	const astrabot::runtime::BotTimingStep &step,
	std::initializer_list<astrabot::runtime::BotTimingEvent> expected,
	const char *description)
{
	if (step.eventCount != expected.size())
	{
		return check(false, description);
	}

	std::size_t index = 0U;
	for (const astrabot::runtime::BotTimingEvent event : expected)
	{
		if (step.events[index] != event)
		{
			return check(false, description);
		}
		++index;
	}
	return true;
}

bool sameStep(
	const astrabot::runtime::BotTimingStep &left,
	const astrabot::runtime::BotTimingStep &right)
{
	if (left.eventCount != right.eventCount)
	{
		return false;
	}
	for (std::size_t index = 0U; index < left.eventCount; ++index)
	{
		if (left.events[index] != right.events[index])
		{
			return false;
		}
	}
	return true;
}

struct CommandTemplateFixture
{
	int marker = 0;
	bool valid = false;
	std::vector<int> executed;
	std::vector<std::uint32_t> sequences;
};

void processCommandTemplateStep(
	const astrabot::runtime::BotTimingStep &step,
	CommandTemplateFixture *fixture)
{
	for (std::size_t index = 0U; index < step.eventCount; ++index)
	{
		switch (step.events[index])
		{
		case astrabot::runtime::BotTimingEvent::CommandReset:
			fixture->valid = false;
			break;
		case astrabot::runtime::BotTimingEvent::FullUpdate:
			++fixture->marker;
			fixture->valid = true;
			break;
		case astrabot::runtime::BotTimingEvent::CommandExecute:
			fixture->executed.push_back(fixture->valid ? fixture->marker : -1);
			fixture->sequences.push_back(
				static_cast<std::uint32_t>(fixture->sequences.size() + 1U));
			break;
		case astrabot::runtime::BotTimingEvent::Upkeep:
			break;
		}
	}
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

	BotTimingScheduler highFps;
	highFps.reset(0.0f);
	const std::array<float, 7U> highFpsTimes = {
		0.001f, 0.010f, 0.020f, 0.0334f, 0.034f, 0.0668f, 0.1002f};
	int highFpsCommandCount = 0;
	int highFpsFullUpdateCount = 0;
	for (const float timestamp : highFpsTimes)
	{
		const astrabot::runtime::BotTimingStep step = highFps.advance(timestamp);
		for (std::size_t index = 0U; index < step.eventCount; ++index)
		{
			if (step.events[index] == BotTimingEvent::CommandExecute)
			{
				++highFpsCommandCount;
			}
			if (step.events[index] == BotTimingEvent::FullUpdate)
			{
				++highFpsFullUpdateCount;
			}
		}
	}
	if (!check(highFpsCommandCount == 3, "high FPS does not over-execute command cadence") ||
		!check(highFpsFullUpdateCount == 1, "high FPS runs one full update in the sample window"))
	{
		return 1;
	}

	BotTimingScheduler lowFps;
	lowFps.reset(0.0f);
	const std::array<float, 3U> lowFpsTimes = {0.050f, 0.1001f, 0.2002f};
	int lowFpsCommandCount = 0;
	int lowFpsFullUpdateCount = 0;
	for (const float timestamp : lowFpsTimes)
	{
		const astrabot::runtime::BotTimingStep step = lowFps.advance(timestamp);
		for (std::size_t index = 0U; index < step.eventCount; ++index)
		{
			if (step.events[index] == BotTimingEvent::CommandExecute)
			{
				++lowFpsCommandCount;
			}
			if (step.events[index] == BotTimingEvent::FullUpdate)
			{
				++lowFpsFullUpdateCount;
			}
		}
	}
	if (!check(lowFpsCommandCount == 3, "low FPS runs one command per due frame") ||
		!check(lowFpsFullUpdateCount == 2, "low FPS keeps full updates nested under commands"))
	{
		return 1;
	}

	BotTimingScheduler irregular;
	irregular.reset(0.0f);
	if (!check(irregular.advance(0.010f).eventCount == 0U, "irregular frame one is not due") ||
		!check(irregular.advance(0.027f).eventCount == 0U, "irregular frame two is not due") ||
		!check(checkEvents(irregular.advance(0.067f),
							 {BotTimingEvent::Upkeep, BotTimingEvent::CommandExecute},
							 "irregular frame three emits one command tick") ,
				   "irregular frame three is valid") ||
		!check(irregular.advance(0.072f).eventCount == 0U, "irregular frame four is not due") ||
		!check(checkEvents(irregular.advance(0.162f),
							 {BotTimingEvent::Upkeep, BotTimingEvent::CommandReset,
							  BotTimingEvent::FullUpdate, BotTimingEvent::CommandExecute},
							 "irregular frame five emits the full ordered tick"),
				   "irregular frame five is valid") ||
		!check(irregular.advance(0.173f).eventCount == 0U, "irregular frame six is not due"))
	{
		return 1;
	}

	BotTimingScheduler longFrame;
	longFrame.reset(0.0f);
	const astrabot::runtime::BotTimingStep delayed = longFrame.advance(0.140f);
	if (!checkEvents(delayed,
					 {BotTimingEvent::Upkeep, BotTimingEvent::CommandReset,
					  BotTimingEvent::FullUpdate, BotTimingEvent::CommandExecute},
					 "long frame executes one non-catch-up tick") ||
		!check(closeEnough(longFrame.nextCommandDeadline(),
							  0.140f + BotTimingScheduler::kCommandInterval),
			   "long frame rebases command deadline from now") ||
		!check(closeEnough(longFrame.nextFullUpdateDeadline(),
							  0.140f + BotTimingScheduler::kFullUpdateInterval),
			   "long frame rebases full deadline from now") ||
		!check(longFrame.advance(0.141f).eventCount == 0U,
			   "long frame does not replay missed updates"))
	{
		return 1;
	}

	BotTimingScheduler msec;
	msec.reset(10.0f);
	if (!check(msec.consumeCommandMsec(10.0334f) == 33U,
			   "first msec uses spawn timestamp and truncates elapsed time") ||
		!check(msec.consumeCommandMsec(10.0340f) == 0U,
			   "short msec has no artificial minimum") ||
		!check(msec.consumeCommandMsec(10.3000f) == 255U,
			   "long msec clamps at 255") ||
		!check(msec.consumeCommandMsec(10.3000f) == 0U,
			   "msec timestamp advances after every consumption"))
	{
		return 1;
	}

	BotTimingScheduler persistenceScheduler;
	persistenceScheduler.reset(0.0f);
	CommandTemplateFixture persistence;
	processCommandTemplateStep(persistenceScheduler.advance(0.1001f), &persistence);
	processCommandTemplateStep(persistenceScheduler.advance(0.1335f), &persistence);
	processCommandTemplateStep(persistenceScheduler.advance(0.1669f), &persistence);
	processCommandTemplateStep(persistenceScheduler.advance(0.2003f), &persistence);
	if (!check(persistence.executed == std::vector<int>{1, 1, 1, 2},
			   "command template persists until the next full-update reset") ||
		!check(persistence.sequences == std::vector<std::uint32_t>{1U, 2U, 3U, 4U},
			   "each persisted command execution receives a new sequence"))
	{
		return 1;
	}

	BotTimingScheduler compatibility;
	BotTimingScheduler enhanced;
	compatibility.reset(0.0f);
	enhanced.reset(0.0f);
	const std::array<float, 6U> modeTimes = {
		0.010f, 0.0334f, 0.1001f, 0.140f, 0.2002f, 0.2336f};
	for (const float timestamp : modeTimes)
	{
		const astrabot::runtime::BotTimingStep compatibilityStep =
			compatibility.advance(timestamp);
		const astrabot::runtime::BotTimingStep enhancedStep = enhanced.advance(timestamp);
		if (!check(sameStep(compatibilityStep, enhancedStep),
				   "mode labels do not change scheduler event sequence") ||
			!check(closeEnough(compatibility.nextCommandDeadline(),
							   enhanced.nextCommandDeadline()),
				   "mode labels do not change command deadline") ||
			!check(closeEnough(compatibility.nextFullUpdateDeadline(),
							   enhanced.nextFullUpdateDeadline()),
				   "mode labels do not change full-update deadline"))
		{
			return 1;
		}
	}

	return 0;
}
