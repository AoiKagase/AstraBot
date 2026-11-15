#ifndef ASTRABOT_COMPAT_ENEMY_COUNT_CHATTER_HPP
#define ASTRABOT_COMPAT_ENEMY_COUNT_CHATTER_HPP

#include "astrabot/compat/random_source.hpp"
#include "astrabot/compat/chatter_schedule.hpp"
#include "astrabot/world/world_snapshot.hpp"
#include <cmath>

namespace astrabot
{
namespace compat
{
struct EnemyCountDeathInput
{
	world::ActorKey actor{};
	world::FrameIdentity frame{};
	std::uint32_t entityGeneration = 1;
	RandomTimingContext timing{};
	float now = 0;
	int nearbyEnemies = 0;
	bool alive = false;
	bool victimEnemy = false;
	bool killerFriendly = false;
	bool enabled = false;
	ChatterScheduleAdmission admission{};
};

struct EnemyCountSpeechInput
{
	world::ActorKey actor{};
	world::FrameIdentity frame{};
	std::uint32_t entityGeneration = 1;
	float now = 0;
	float teamBusyUntil = 0;
	int friendsRemaining = 0;
	int enemiesRemaining = 0;
	bool alive = false;
	bool enabled = false;
	bool roundOver = false;
	bool teamReportingRemaining = false;
	bool canStart = true;
};

struct EnemyCountSpeechDecision
{
	bool ready = false;
	int enemiesRemaining = 0;
	float startTime = 0;
};

// One pending statement is sufficient: native remaining-count topics are redundant.
class EnemyCountChatterPolicy
{
public:
	bool observe(const EnemyCountDeathInput &in, ICompatibilityRandomSource &rng)
	{
		constexpr int kMaximumNearbyEnemies = 1;
		constexpr float kMinimumDelaySeconds = 2.0f;
		constexpr float kMaximumDelaySeconds = 4.0f;
		if (!synchronize(in.actor, in.frame, in.entityGeneration, in.now) || !in.alive || !in.victimEnemy ||
			!in.killerFriendly || in.nearbyEnemies < 0 || in.nearbyEnemies > kMaximumNearbyEnemies)
		{
			return false;
		}
		// EnemiesRemaining draws before AddStatement checks mute and redundancy.
		const auto delay = rng.nextFloat(RandomRequest::floatRequest("RNG-CSBOT-ENEMIES-REMAINING-DELAY",
			{in.actor.slot, in.actor.generation}, in.timing, kMinimumDelaySeconds, kMaximumDelaySeconds));
		if (delay.status != RandomStatus::Ok || !std::isfinite(delay.value) || delay.value < kMinimumDelaySeconds ||
			delay.value > kMaximumDelaySeconds || !in.enabled || pending_)
		{
			return false;
		}
		pending_ = true;
		created_ = in.now;
		start_ = in.now + delay.value;
		schedule_ = in.admission.make(
			ChatterScheduleSource::EnemyCount, ChatterStatementTopic::EnemiesRemaining, created_, start_);
		return true;
	}

	EnemyCountSpeechDecision update(const EnemyCountSpeechInput &in)
	{
		constexpr int kMaximumReportedEnemies = 3;
		if (!synchronize(in.actor, in.frame, in.entityGeneration, in.now))
			return {};
		if (!in.alive || !in.enabled || in.roundOver || (in.now < in.teamBusyUntil && in.teamReportingRemaining))
		{
			discard();
		}
		if (!pending_ || in.now < start_ || in.now < in.teamBusyUntil || !in.canStart)
			return {};
		if (in.friendsRemaining <= 0 || in.enemiesRemaining < 0 || in.enemiesRemaining > kMaximumReportedEnemies)
		{
			discard();
			return {};
		}
		return {true, in.enemiesRemaining, start_};
	}

	void discard()
	{
		pending_ = false;
	}

	void reset()
	{
		*this = EnemyCountChatterPolicy{};
	}

	ChatterScheduleEntry scheduledStatement() const
	{
		return pending_ ? schedule_ : ChatterScheduleEntry{};
	}

	float scheduledStart() const
	{
		return start_;
	}

	bool active() const
	{
		return pending_;
	}

private:
	bool synchronize(
		const world::ActorKey &actor, const world::FrameIdentity &frame, std::uint32_t entityGeneration, float now)
	{
		constexpr float kLifetimeSeconds = 5.0f;
		if (!actor.isValid() || !frame.isValid() || entityGeneration == 0 || !std::isfinite(now) || now < 0)
		{
			reset();
			return false;
		}
		if (!(actor == actor_) || entityGeneration != entityGeneration_ ||
			frame.mapGeneration != frame_.mapGeneration || frame.roundGeneration != frame_.roundGeneration ||
			now < lastNow_)
		{
			reset();
			actor_ = actor;
			entityGeneration_ = entityGeneration;
		}
		frame_ = frame;
		lastNow_ = now;
		if (pending_ && now > created_ + kLifetimeSeconds)
			discard();
		return true;
	}

	world::ActorKey actor_{};
	world::FrameIdentity frame_{};
	std::uint32_t entityGeneration_ = 0;
	float lastNow_ = 0;
	float created_ = 0;
	float start_ = 0;
	bool pending_ = false;
	ChatterScheduleEntry schedule_{};
};
}
}

#endif
