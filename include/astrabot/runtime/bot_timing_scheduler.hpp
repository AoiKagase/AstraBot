#ifndef ASTRABOT_RUNTIME_BOT_TIMING_SCHEDULER_HPP
#define ASTRABOT_RUNTIME_BOT_TIMING_SCHEDULER_HPP

#include <array>
#include <cstdint>

namespace astrabot
{
namespace runtime
{
enum class BotTimingEvent
{
	Upkeep,
	CommandReset,
	FullUpdate,
	CommandExecute
};

struct BotTimingStep
{
	std::array<BotTimingEvent, 4U> events;
	std::uint8_t eventCount;
};

class BotTimingScheduler
{
public:
	static constexpr float kCommandInterval = 1.0f / 30.0f;
	static constexpr float kFullUpdateInterval = 1.0f / 10.0f;

	BotTimingScheduler();
	void reset(float spawnTime);
	BotTimingStep advance(float currentTime);
	std::uint8_t consumeCommandMsec(float currentTime);
	bool initialized() const;
	float nextCommandDeadline() const;
	float nextFullUpdateDeadline() const;

private:
	float nextCommandDeadline_;
	float nextFullUpdateDeadline_;
	float previousCommandTime_;
	bool initialized_;
};
}
}

#endif
