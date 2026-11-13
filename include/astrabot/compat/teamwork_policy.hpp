#ifndef ASTRABOT_COMPAT_TEAMWORK_POLICY_HPP
#define ASTRABOT_COMPAT_TEAMWORK_POLICY_HPP

#include "astrabot/compat/random_source.hpp"
#include "astrabot/world/world_snapshot.hpp"
#include <cmath>
#include <vector>

namespace astrabot
{
namespace compat
{
struct TeamworkInput
{
	world::ActorKey actor{};
	world::FrameIdentity frame{};
	float now = 0.0f, elapsedRound = 0.0f, teamwork = 0.75f;
	bool roundTimeKnown = false, allowRoguesKnown = false, allowRogues = false;
	bool busy = false, following = false, atPlantedBombsite = false;
	float followDuration = 0.0f;
	world::ActorKey candidate{};
	bool candidateVisibleHumanFriend = false, candidateReachable = false;
	float candidateRange = 0.0f, candidateTravel = 0.0f;
	std::uint32_t followerCount = 0U;
};
struct TeamworkDecision
{
	bool available = false, rogueKnown = false, rogue = false;
	bool startFollowing = false, stopFollowing = false;
	bool humanPermissionInferred = false;
};

class TeamworkPolicy
{
public:
	void reset()
	{
		*this = TeamworkPolicy();
	}

	void followingStopped(float now)
	{
		constexpr float kFollowingStoppedCooldownSeconds = 10.0f;
		if (std::isfinite(now) && now >= 0.0f)
			allowAutoFollowAt_ = now + kFollowingStoppedCooldownSeconds;
	}

	// Public accepted radio is a proxy for the human's private allowance timer.
	// Keep it explicitly inferred; no player private object is inspected.
	void observeHumanPolicy(world::ActorKey human, float now, bool allow)
	{
		constexpr float kHumanPolicyCooldownSeconds = 60.0f;
		if (!human.isValid() || !std::isfinite(now) || now < 0.0f)
			return;
		for (HumanPolicy &entry : humanPolicy_)
			if (entry.actor == human)
			{
				entry.allowAt = allow ? 0.0f : now + kHumanPolicyCooldownSeconds;
				return;
			}
		humanPolicy_.push_back({human, allow ? 0.0f : now + kHumanPolicyCooldownSeconds});
	}

	TeamworkDecision update(const TeamworkInput &input, ICompatibilityRandomSource &random, RandomActor randomActor,
		RandomTimingContext timing)
	{
		constexpr float kMinimumRogueIntervalSeconds = 10.0f;
		constexpr float kMaximumRogueIntervalSeconds = 30.0f;
		constexpr int kMinimumRogueIntervalBound = 10;
		constexpr int kMaximumRogueIntervalBound = 30;
		constexpr float kChancePercentScale = 100.0f;
		constexpr int kMaximumChancePercent = 100;
// Keep literal evaluation precision on x87; each macro is undefined below.
#define ASTRABOT_TEAMWORK_POLICY_PERSISTENT_FOLLOW_TEAMWORK 0.85f
		constexpr float kFollowBaseDurationSeconds = 15.0f;
		constexpr float kFollowTeamworkDurationSeconds = 40.0f;
		constexpr float kAutoFollowRoundDelaySeconds = 5.0f;
#define ASTRABOT_TEAMWORK_POLICY_MINIMUM_AUTO_FOLLOW_TEAMWORK 0.4f
		constexpr float kAutoFollowRetryBaseSeconds = 15.0f;
		constexpr float kAutoFollowRetryScaleSeconds = 30.0f;
		constexpr unsigned kMaximumFollowers = 2U;
		constexpr float kMaximumCandidateRange = 300.0f;
		constexpr float kMaximumCandidateTravel = 300.0f;
		TeamworkDecision result{};
		if (!input.actor.isValid() || !input.frame.isValid() || !std::isfinite(input.now) || input.now < 0.0f ||
			!std::isfinite(input.teamwork) || input.teamwork < 0.0f || input.teamwork > 1.0f ||
			!std::isfinite(input.followDuration) || input.followDuration < 0.0f)
			return result;
		if (!(actor_ == input.actor) || frame_.mapGeneration != input.frame.mapGeneration ||
			frame_.roundGeneration != input.frame.roundGeneration)
		{
			reset();
			actor_ = input.actor;
		}
		else if (input.now < lastTime_ || (frame_.isValid() && input.frame.tick <= frame_.tick))
			return result;
		frame_ = input.frame;
		lastTime_ = input.now;
		result.available = true;
		if (input.allowRoguesKnown)
		{
			if (input.allowRogues)
			{
				result.rogueKnown = rogueKnown_;
				result.rogue = rogue_;
				if (pendingRogueChance_ || !rogueKnown_ || input.now > nextRogueAt_)
				{
					if (!pendingRogueChance_)
					{
						const auto interval = random.nextFloat(RandomRequest::floatRequest("Teamwork.RogueInterval",
							randomActor, timing, kMinimumRogueIntervalSeconds, kMaximumRogueIntervalSeconds));
						if (interval.status != RandomStatus::Ok || !std::isfinite(interval.value) ||
							interval.value < kMinimumRogueIntervalBound || interval.value > kMaximumRogueIntervalBound)
						{
							result.available = false;
							return result;
						}
						nextRogueAt_ = input.now + interval.value;
						pendingRogueChance_ = true;
					}
					const auto chance = random.nextFloat(RandomRequest::floatRequest(
						"Teamwork.RogueChance", randomActor, timing, 0.0f, kChancePercentScale));
					if (chance.status != RandomStatus::Ok || !std::isfinite(chance.value) || chance.value < 0 ||
						chance.value > kMaximumChancePercent)
					{
						result.available = false;
						return result;
					}
					rogue_ = chance.value < kChancePercentScale * (1.0f - input.teamwork);
					rogueKnown_ = true;
					pendingRogueChance_ = false;
				}
				result.rogue = rogue_;
				result.rogueKnown = rogueKnown_;
			}
			else
				result.rogueKnown = true;
		}
		if (input.following)
		{
			if (input.teamwork < ASTRABOT_TEAMWORK_POLICY_PERSISTENT_FOLLOW_TEAMWORK &&
				input.followDuration > kFollowBaseDurationSeconds + kFollowTeamworkDurationSeconds * input.teamwork)
			{
				result.stopFollowing = true;
				followingStopped(input.now);
			}
			return result;
		}
		if (!input.roundTimeKnown || !std::isfinite(input.elapsedRound) ||
			input.elapsedRound <= kAutoFollowRoundDelaySeconds ||
			input.teamwork <= ASTRABOT_TEAMWORK_POLICY_MINIMUM_AUTO_FOLLOW_TEAMWORK ||
			input.now <= allowAutoFollowAt_ || input.busy || input.atPlantedBombsite)
			return result;
		const auto chance =
			random.nextFloat(RandomRequest::floatRequest("Teamwork.AutoFollow", randomActor, timing, 0.0f, 1.0f));
		if (chance.status != RandomStatus::Ok || !std::isfinite(chance.value) || chance.value < 0 || chance.value > 1)
		{
			result.available = false;
			return result;
		}
		if (!(input.teamwork > chance.value))
		{
			allowAutoFollowAt_ =
				input.now + kAutoFollowRetryBaseSeconds + (1.0f - input.teamwork) * kAutoFollowRetryScaleSeconds;
			return result;
		}
		if (!input.candidate.isValid() || !input.candidateVisibleHumanFriend || !input.candidateReachable ||
			input.followerCount >= kMaximumFollowers || !std::isfinite(input.candidateRange) ||
			input.candidateRange < 0 || input.candidateRange >= kMaximumCandidateRange ||
			!std::isfinite(input.candidateTravel) || input.candidateTravel < 0 ||
			input.candidateTravel >= kMaximumCandidateTravel)
			return result;
		for (const HumanPolicy &entry : humanPolicy_)
			if (entry.actor == input.candidate && input.now <= entry.allowAt)
				return result;
		result.startFollowing = true;
		result.humanPermissionInferred = true;
		return result;
	}
#undef ASTRABOT_TEAMWORK_POLICY_PERSISTENT_FOLLOW_TEAMWORK
#undef ASTRABOT_TEAMWORK_POLICY_MINIMUM_AUTO_FOLLOW_TEAMWORK

private:
	struct HumanPolicy
	{
		world::ActorKey actor;
		float allowAt;
	};
	world::ActorKey actor_{};
	world::FrameIdentity frame_{};
	float lastTime_ = 0.0f, allowAutoFollowAt_ = 0.0f, nextRogueAt_ = 0.0f;
	bool rogueKnown_ = false, rogue_ = false, pendingRogueChance_ = false;
	std::vector<HumanPolicy> humanPolicy_;
};
}
}
#endif
