#ifndef ASTRABOT_COMPAT_KILL_PRAISE_HPP
#define ASTRABOT_COMPAT_KILL_PRAISE_HPP

#include "astrabot/compat/outgoing_radio.hpp"
#include "astrabot/compat/random_source.hpp"
#include <cmath>

namespace astrabot
{
namespace compat
{
inline bool killPraiseViewCone(float dx, float dy, float pitchDegrees, float yawDegrees)
{
	constexpr float kDegreesToRadians = 3.14159265358979323846f / 180.0f;
	constexpr float kMinimumViewDot = 0.5f;
	const float length = std::sqrt(dx * dx + dy * dy);
	if (!std::isfinite(length) || length <= 0 || !std::isfinite(pitchDegrees) || !std::isfinite(yawDegrees))
		return false;
	const float pitch = pitchDegrees * kDegreesToRadians;
	const float yaw = yawDegrees * kDegreesToRadians;
	const float forwardScale = std::cos(pitch);
	const float dot = (dx / length) * std::cos(yaw) * forwardScale + (dy / length) * std::sin(yaw) * forwardScale;
	return dot > kMinimumViewDot;
}

struct KillPraiseDeathInput
{
	world::ActorKey actor{};
	world::FrameIdentity frame{};
	std::uint32_t entityGeneration = 1;
	RandomTimingContext timing{};
	float now = 0;
	int nearbyEnemies = 0;
	std::uint32_t allowedPhrases = 0;
	bool alive = false;
	bool victimEnemy = false;
	bool killerFriendly = false;
	bool killerSelf = false;
	bool killerBot = false;
	bool victimVisible = false;
	bool careerKnown = false;
	bool career = false;
	bool intentionPending = false;
	ChatterScheduleAdmission admission{};
};

struct KillPraiseSpeechInput
{
	world::ActorKey actor{};
	world::FrameIdentity frame{};
	std::uint32_t entityGeneration = 1;
	float now = 0;
	float teamBusyUntil = 0;
	int friendsRemaining = 0;
	std::uint32_t allowedPhrases = 0;
	bool alive = false;
	bool roundOver = false;
	bool teamPraising = false;
	bool canStart = true;
};

struct KillPraiseDecision
{
	bool ready = false;
	ChatterPhrase phrase = ChatterPhrase::None;
	float startTime = 0;
};

class KillPraisePolicy
{
public:
	bool observe(const KillPraiseDeathInput &in, ICompatibilityRandomSource &rng)
	{
		constexpr int kMaximumNearbyEnemies = 1;
		constexpr float kMinimumDelay = 2.0f;
		constexpr float kMaximumDelay = 3.0f;
		constexpr float kChanceMaximum = 100.0f;
		constexpr float kBotPraiseChance = 40.0f;
		if (!synchronize(in.actor, in.frame, in.entityGeneration, in.now) || !in.alive || !in.victimEnemy ||
			!in.killerFriendly || in.killerSelf || !in.victimVisible || in.nearbyEnemies < 0 ||
			in.nearbyEnemies > kMaximumNearbyEnemies)
			return false;
		const auto delay = rng.nextFloat(RandomRequest::floatRequest("RNG-CSBOT-KILL-PRAISE-DELAY",
			{in.actor.slot, in.actor.generation}, in.timing, kMinimumDelay, kMaximumDelay));
		if (delay.status != RandomStatus::Ok || !std::isfinite(delay.value) || delay.value < kMinimumDelay ||
			delay.value > kMaximumDelay)
			return false;
		ChatterPhrase phrase = ChatterPhrase::None;
		if (in.killerBot)
		{
			const auto chance = rng.nextFloat(RandomRequest::floatRequest("RNG-CSBOT-KILL-PRAISE-BOT-CHANCE",
				{in.actor.slot, in.actor.generation}, in.timing, 0.0f, kChanceMaximum));
			if (chance.status != RandomStatus::Ok || !std::isfinite(chance.value) || chance.value < 0 ||
				chance.value >= kBotPraiseChance)
				return false;
			phrase = ChatterPhrase::NiceShot;
		}
		else if (in.careerKnown)
		{
			phrase = in.career ? ChatterPhrase::NiceShotCommander : ChatterPhrase::NiceShotSir;
		}
		if (phrase == ChatterPhrase::None || !(in.allowedPhrases & (1U << static_cast<unsigned>(phrase))) || in.intentionPending || active())
			return false;
		phrase_ = phrase;
		created_ = in.now;
		start_ = in.now + delay.value;
		schedule_ =
			in.admission.make(ChatterScheduleSource::KillPraise, ChatterStatementTopic::MyIntention, created_, start_);
		return true;
	}

	KillPraiseDecision update(const KillPraiseSpeechInput &in)
	{
		if (!synchronize(in.actor, in.frame, in.entityGeneration, in.now))
			return {};
		if (!in.alive || in.roundOver || (in.now < in.teamBusyUntil && in.teamPraising) ||
			(active() && !(in.allowedPhrases & (1U << static_cast<unsigned>(phrase_)))))
			discard();
		if (!active() || in.now < start_ || in.now < in.teamBusyUntil || !in.canStart)
			return {};
		if (in.friendsRemaining <= 0)
		{
			discard();
			return {};
		}
		return {true, phrase_, start_};
	}

	void discard()
	{
		phrase_ = ChatterPhrase::None;
	}

	void reset()
	{
		*this = KillPraisePolicy{};
	}

	bool active() const
	{
		return phrase_ != ChatterPhrase::None;
	}

	ChatterScheduleEntry scheduledStatement() const
	{
		return active() ? schedule_ : ChatterScheduleEntry{};
	}

	float scheduledStart() const
	{
		return start_;
	}

private:
	bool synchronize(
		const world::ActorKey &actor, const world::FrameIdentity &frame, std::uint32_t entityGeneration, float now)
	{
		constexpr float kLifetimeSeconds = 3.0f;
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
		if (active() && now > created_ + kLifetimeSeconds)
			discard();
		return true;
	}

	world::ActorKey actor_{};
	world::FrameIdentity frame_{};
	std::uint32_t entityGeneration_ = 0;
	float lastNow_ = 0;
	float created_ = 0;
	float start_ = 0;
	ChatterPhrase phrase_ = ChatterPhrase::None;
	ChatterScheduleEntry schedule_{};
};
}
}

#endif
