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
	void reset() { *this = TeamworkPolicy(); }
	void followingStopped(float now)
	{
		if (std::isfinite(now) && now >= 0.0f) allowAutoFollowAt_ = now + 10.0f;
	}
	// Public accepted radio is a proxy for the human's private allowance timer.
	// Keep it explicitly inferred; no player private object is inspected.
	void observeHumanPolicy(world::ActorKey human, float now, bool allow)
	{
		if (!human.isValid() || !std::isfinite(now) || now < 0.0f) return;
		for (HumanPolicy &entry : humanPolicy_)
			if (entry.actor == human) { entry.allowAt = allow ? 0.0f : now + 60.0f; return; }
		humanPolicy_.push_back({human, allow ? 0.0f : now + 60.0f});
	}
	TeamworkDecision update(const TeamworkInput &input, ICompatibilityRandomSource &random,
		RandomActor randomActor, RandomTimingContext timing)
	{
		TeamworkDecision result{};
		if (!input.actor.isValid() || !input.frame.isValid() || !std::isfinite(input.now) || input.now < 0.0f ||
			!std::isfinite(input.teamwork) || input.teamwork < 0.0f || input.teamwork > 1.0f ||
			!std::isfinite(input.followDuration) || input.followDuration < 0.0f) return result;
		if (!(actor_ == input.actor) || frame_.mapGeneration != input.frame.mapGeneration || frame_.roundGeneration != input.frame.roundGeneration)
		{ reset(); actor_ = input.actor; }
		else if (input.now < lastTime_ || (frame_.isValid() && input.frame.tick <= frame_.tick)) return result;
		frame_ = input.frame; lastTime_ = input.now;
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
						const auto interval = random.nextFloat(RandomRequest::floatRequest("Teamwork.RogueInterval",randomActor,timing,10.0f,30.0f));
						if (interval.status != RandomStatus::Ok || !std::isfinite(interval.value) || interval.value < 10 || interval.value > 30)
						{ result.available = false; return result; }
						nextRogueAt_ = input.now + interval.value; pendingRogueChance_ = true;
					}
					const auto chance = random.nextFloat(RandomRequest::floatRequest("Teamwork.RogueChance",randomActor,timing,0.0f,100.0f));
					if (chance.status != RandomStatus::Ok || !std::isfinite(chance.value) || chance.value < 0 || chance.value > 100)
					{ result.available = false; return result; }
					rogue_ = chance.value < 100.0f * (1.0f - input.teamwork);
					rogueKnown_ = true; pendingRogueChance_ = false;
				}
				result.rogue = rogue_; result.rogueKnown = rogueKnown_;
			}
			else result.rogueKnown = true;
		}
		if (input.following)
		{
			if (input.teamwork < 0.85f && input.followDuration > 15.0f + 40.0f * input.teamwork)
			{ result.stopFollowing = true; followingStopped(input.now); }
			return result;
		}
		if (!input.roundTimeKnown || !std::isfinite(input.elapsedRound) || input.elapsedRound <= 5.0f ||
			input.teamwork <= 0.4f || input.now <= allowAutoFollowAt_ || input.busy || input.atPlantedBombsite) return result;
		const auto chance = random.nextFloat(RandomRequest::floatRequest("Teamwork.AutoFollow",randomActor,timing,0.0f,1.0f));
		if (chance.status != RandomStatus::Ok || !std::isfinite(chance.value) || chance.value < 0 || chance.value > 1)
		{ result.available = false; return result; }
		if (!(input.teamwork > chance.value))
		{ allowAutoFollowAt_ = input.now + 15.0f + (1.0f-input.teamwork)*30.0f; return result; }
		if (!input.candidate.isValid() || !input.candidateVisibleHumanFriend || !input.candidateReachable ||
			input.followerCount >= 2U || !std::isfinite(input.candidateRange) || input.candidateRange < 0 || input.candidateRange >= 300.0f ||
			!std::isfinite(input.candidateTravel) || input.candidateTravel < 0 || input.candidateTravel >= 300.0f) return result;
		for (const HumanPolicy &entry : humanPolicy_)
			if (entry.actor == input.candidate && input.now <= entry.allowAt) return result;
		result.startFollowing = true;
		result.humanPermissionInferred = true;
		return result;
	}
private:
	struct HumanPolicy { world::ActorKey actor; float allowAt; };
	world::ActorKey actor_{};
	world::FrameIdentity frame_{};
	float lastTime_ = 0.0f, allowAutoFollowAt_ = 0.0f, nextRogueAt_ = 0.0f;
	bool rogueKnown_ = false, rogue_ = false, pendingRogueChance_ = false;
	std::vector<HumanPolicy> humanPolicy_;
};
}
}
#endif
