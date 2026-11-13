#include "astrabot/runtime/bot_timing_scheduler.hpp"

namespace astrabot
{
namespace runtime
{
namespace
{
void appendEvent(BotTimingStep *step, BotTimingEvent event)
{
	if (step != nullptr && step->eventCount < step->events.size())
	{
		step->events[step->eventCount] = event;
		++step->eventCount;
	}
}
}

constexpr float BotTimingScheduler::kCommandInterval;
constexpr float BotTimingScheduler::kFullUpdateInterval;

BotTimingScheduler::BotTimingScheduler()
	: nextCommandDeadline_(0.0f), nextFullUpdateDeadline_(0.0f), previousCommandTime_(0.0f), initialized_(false)
{
}

void BotTimingScheduler::reset(float spawnTime)
{
	nextCommandDeadline_ = spawnTime + kCommandInterval;
	nextFullUpdateDeadline_ = spawnTime + kFullUpdateInterval;
	previousCommandTime_ = spawnTime;
	initialized_ = true;
}

BotTimingStep BotTimingScheduler::advance(float currentTime)
{
	BotTimingStep step = {{}, 0U};
	if (!initialized_ || currentTime < nextCommandDeadline_)
	{
		return step;
	}

	nextCommandDeadline_ = currentTime + kCommandInterval;
	appendEvent(&step, BotTimingEvent::Upkeep);

	if (currentTime >= nextFullUpdateDeadline_)
	{
		nextFullUpdateDeadline_ = currentTime + kFullUpdateInterval;
		appendEvent(&step, BotTimingEvent::CommandReset);
		appendEvent(&step, BotTimingEvent::FullUpdate);
	}

	appendEvent(&step, BotTimingEvent::CommandExecute);
	return step;
}

std::uint8_t BotTimingScheduler::consumeCommandMsec(float currentTime)
{
	constexpr float kMillisecondsPerSecond = 1000.0f;
	constexpr int kMaximumCommandMilliseconds = 255;
	int milliseconds = static_cast<int>((currentTime - previousCommandTime_) * kMillisecondsPerSecond);
	if (milliseconds > kMaximumCommandMilliseconds)
	{
		milliseconds = kMaximumCommandMilliseconds;
	}
	previousCommandTime_ = currentTime;
	return static_cast<std::uint8_t>(milliseconds);
}

bool BotTimingScheduler::initialized() const
{
	return initialized_;
}

float BotTimingScheduler::nextCommandDeadline() const
{
	return nextCommandDeadline_;
}

float BotTimingScheduler::nextFullUpdateDeadline() const
{
	return nextFullUpdateDeadline_;
}
}
}
