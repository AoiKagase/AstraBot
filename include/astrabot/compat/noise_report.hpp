#ifndef ASTRABOT_COMPAT_NOISE_REPORT_HPP
#define ASTRABOT_COMPAT_NOISE_REPORT_HPP

#include "astrabot/compat/random_source.hpp"
#include "astrabot/world/world_snapshot.hpp"
#include <cmath>

namespace astrabot
{
namespace compat
{
struct HeardNoiseReport
{
	bool active = false;
	world::ActorKey actor{};
	world::FrameIdentity frame{};
	float created = 0;
	std::uint16_t place = 0;
	std::uint64_t navRevision = 0;
};

struct NoiseReportInput
{
	world::ActorKey actor{};
	world::FrameIdentity frame{};
	RandomTimingContext timing{};
	float now = 0;
	float lastSawEnemy = 0;
	bool alive = false;
	bool wellPastSafe = false;
	bool roundOver = false;
	world::NoisePriority priority = world::NoisePriority::High;
};

// Called once when a new investigation destination has actually been attended.
class NoiseReportPolicy
{
public:
	bool attend(const NoiseReportInput &in, ICompatibilityRandomSource &rng)
	{
		constexpr float kLongEnemyAbsenceSeconds = 30.0f;
		constexpr float kReportCooldownSeconds = 20.0f;
		constexpr float kChanceMaximum = 100.0f;
		constexpr float kReportChance = 33.0f;
		if (!in.actor.isValid() || !in.frame.isValid() || !std::isfinite(in.now) || in.now < 0 ||
			!std::isfinite(in.lastSawEnemy) || in.lastSawEnemy < 0 || in.lastSawEnemy > in.now)
		{
			*this = NoiseReportPolicy{};
			return false;
		}
		if (!(actor_ == in.actor) || frame_.mapGeneration != in.frame.mapGeneration ||
			frame_.roundGeneration != in.frame.roundGeneration || in.now < lastNow_)
		{
			*this = NoiseReportPolicy{};
			actor_ = in.actor;
		}
		frame_ = in.frame;
		lastNow_ = in.now;
		if (!in.alive || in.roundOver || !in.wellPastSafe || in.priority == world::NoisePriority::High ||
			in.now - in.lastSawEnemy <= kLongEnemyAbsenceSeconds || in.now <= cooldownUntil_)
		{
			return false;
		}
		// Native CountdownTimer is strict and is started before the chance draw.
		cooldownUntil_ = in.now + kReportCooldownSeconds;
		const auto result = rng.nextFloat(RandomRequest::floatRequest("RNG-CSBOT-HEARD-NOISE-REPORT",
			{in.actor.slot, in.actor.generation}, in.timing, 0.0f, kChanceMaximum));
		return result.status == RandomStatus::Ok && std::isfinite(result.value) && result.value >= 0 &&
			result.value < kReportChance;
	}

private:
	world::ActorKey actor_{};
	world::FrameIdentity frame_{};
	float lastNow_ = 0;
	float cooldownUntil_ = -1;
};
}
}

#endif
