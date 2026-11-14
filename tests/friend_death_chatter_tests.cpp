#include "astrabot/compat/friend_death_chatter.hpp"
#include <cstdio>
#include <cstring>
namespace
{
struct Random : astrabot::compat::ICompatibilityRandomSource
{
	int draws = 0;
	bool requestCorrect = true;
	float delay = .75f;
	astrabot::compat::RandomFloatResult nextFloat(const astrabot::compat::RandomRequest &r) override
	{
		++draws;
		requestCorrect = requestCorrect && std::strcmp(r.semanticId, "RNG-CSBOT-KILLED-FRIEND-DELAY") == 0 &&
			r.floatLower == .5f && r.floatUpper == 1;
		return {astrabot::compat::RandomStatus::Ok, delay};
	}
	astrabot::compat::RandomLongResult nextLong(const astrabot::compat::RandomRequest &) override
	{
		return {astrabot::compat::RandomStatus::Unavailable, 0};
	}
};
}
int main()
{
	using namespace astrabot::compat;
	int failures = 0;
	const auto check = [&](bool ok, const char *message)
	{
		if (!ok)
		{
			std::printf("FAIL: %s\n", message);
			++failures;
		}
	};
	ReactionContext context{{1, 2}, {3, 4, 5}, 6, 10, true};
	FriendDeathChatterEvent event{};
	event.context = context;
	event.victimFriendly = true;
	event.victimHuman = true;
	event.careerKnown = true;
	event.career = true;
	event.killerHumanFriendly = true;
	Random random;
	check(commanderDownEligible(event), "career human friendly death reports CommanderDown");
	const auto delay = prepareKilledFriend(event, random);
	check(delay.valid && delay.seconds == .75f && random.draws == 1 && random.requestCorrect,
		"native delayed rebuke request");
	for (int reject = 0; reject < 5; ++reject)
	{
		auto invalid = event;
		if (reject == 0)
			invalid.retaliationReturned = true;
		if (reject == 1)
			invalid.context.alive = false;
		if (reject == 2)
			invalid.victimFriendly = false;
		if (reject == 3)
			invalid.killerHumanFriendly = false;
		if (reject == 4)
			invalid.killerSelf = true;
		const int before = random.draws;
		check(!prepareKilledFriend(invalid, random).valid && random.draws == before,
			"ineligible death does not draw rebuke delay");
	}
	for (int reject = 0; reject < 4; ++reject)
	{
		auto invalid = event;
		if (reject == 0)
			invalid.victimHuman = false;
		if (reject == 1)
			invalid.careerKnown = false;
		if (reject == 2)
			invalid.career = false;
		if (reject == 3)
			invalid.retaliationReturned = true;
		check(!commanderDownEligible(invalid), "CommanderDown requires native career/human/early-return gates");
	}
	const auto all = (1U << static_cast<unsigned>(ChatterPhrase::CommanderDown)) |
		(1U << static_cast<unsigned>(ChatterPhrase::KilledFriend));
	FriendDeathChatterPolicy policy;
	check(policy.admitCommander(context, all, false), "admit immediate CommanderDown");
	check(!policy.admitCommander(context, all, false), "duplicate intention cannot replace pending CommanderDown");
	check(policy.admitKilled(context, delay, all), "distinct rebuke can coexist with CommanderDown");
	FriendDeathChatterSpeechInput speech{};
	speech.context = context;
	speech.allowedPhrases = all;
	speech.friendsRemaining = 1;
	auto decision = policy.update(speech);
	check(decision.ready && decision.phrase == ChatterPhrase::CommanderDown && decision.startTime == 10,
		"immediate intention precedes delayed rebuke");
	policy.discard(decision);
	speech.context.now = 10.749f;
	check(!policy.update(speech).ready, "rebuke waits full delay");
	speech.context.now = 10.75f;
	decision = policy.update(speech);
	check(decision.ready && decision.phrase == ChatterPhrase::KilledFriend, "rebuke begins at exact delay boundary");
	policy.discard(decision);
	check(!policy.active(), "both topics acknowledged independently");
	check(!policy.admitCommander(context, all, true), "pending NiceShot intention rejects CommanderDown");
	check(!policy.admitKilled(context, delay, 0),
		"OFF/MINIMAL/missing phrase mask rejects admission after delay preparation");
	policy.admitCommander(context, all, false);
	speech.context.now = 30;
	check(policy.update(speech).ready, "CommanderDown remains valid at exact20second deadline");
	speech.context.now = 30.001f;
	check(!policy.update(speech).ready && !policy.active(), "CommanderDown expires after20seconds");
	context.now = 40;
	policy.admitKilled(context, delay, all);
	speech.context = context;
	speech.context.now = 42;
	check(policy.update(speech).ready, "KilledFriend valid at exact2second deadline");
	speech.context.now = 42.001f;
	check(!policy.update(speech).ready && !policy.active(),
		"KilledFriend expiry is from creation rather than delayed start");
	context.now = 50;
	policy.admitCommander(context, all, false);
	speech.context = context;
	speech.teamBusyUntil = 51;
	speech.teamPhrase = ChatterPhrase::NiceShotSir;
	check(!policy.update(speech).ready && !policy.active(), "teammate intention suppresses pending CommanderDown");
	policy.admitKilled(context, delay, all);
	check(!policy.update(speech).ready && policy.active(), "different speech topic waits rather than discards rebuke");
	speech.teamPhrase = ChatterPhrase::KilledFriend;
	check(!policy.update(speech).ready && !policy.active(), "teammate rebuke suppresses same undefined subject topic");
	policy.admitCommander(context, all, false);
	speech.teamBusyUntil = 0;
	speech.canStart = false;
	speech.friendsRemaining = 0;
	check(!policy.update(speech).ready && policy.active(), "unselected no-friends context does not consume topic");
	speech.canStart = true;
	check(!policy.update(speech).ready && !policy.active(), "selected no-friends statement is discarded");
	for (int invalidation = 0; invalidation < 7; ++invalidation)
	{
		policy.reset();
		policy.admitCommander(context, all, false);
		speech = {};
		speech.context = context;
		speech.allowedPhrases = all;
		speech.friendsRemaining = 1;
		if (invalidation == 0)
			++speech.context.actor.generation;
		if (invalidation == 1)
			++speech.context.entityGeneration;
		if (invalidation == 2)
			++speech.context.frame.roundGeneration;
		if (invalidation == 3)
			++speech.context.frame.mapGeneration;
		if (invalidation == 4)
			speech.context.now -= 1;
		if (invalidation == 5)
			speech.context.alive = false;
		if (invalidation == 6)
			speech.roundOver = true;
		check(!policy.update(speech).ready && !policy.active(), "qualified lifecycle invalidation clears speech");
	}
	// A rebuke created first and a commander report added at the same start time
	// retain insertion order; an old confirmation cannot discard a newer topic.
	policy.reset();
	context.now = 60;
	policy.admitKilled(context, delay, all);
	context.now = 60.75f;
	policy.admitCommander(context, all, false);
	speech = {};
	speech.context = context;
	speech.allowedPhrases = all;
	speech.friendsRemaining = 1;
	decision = policy.update(speech);
	check(decision.phrase == ChatterPhrase::KilledFriend, "equal start times preserve existing statement order");
	policy.discard(decision);
	policy.admitKilled(context, delay, all);
	policy.discard(decision);
	check(policy.active(), "old confirmation cannot remove new pending statement");
	FriendDeathChatterPolicy ownership;
	context.now = 70;
	ownership.admitCommander(context, all, false);
	speech.context = context;
	auto oldConfirmation = ownership.update(speech);
	ownership.reset();
	ownership.admitCommander(context, all, false);
	ownership.discard(oldConfirmation);
	check(ownership.active(), "old confirmation after same-owner reset cannot discard new speech");
	ownership = FriendDeathChatterPolicy{};
	++context.actor.generation;
	ownership.admitCommander(context, all, false);
	ownership.discard(oldConfirmation);
	check(ownership.active(), "old owner confirmation cannot discard replacement actor speech");
	ConfirmedChatterSpeech speaking;
	context.now = 80;
	speaking.maintain(context);
	const auto speechRevision = speaking.revision();
	check(speaking.record(context, ChatterPhrase::NiceShotSir, 80.3f, speechRevision),
		"confirmed speech records real delivery");
	check(speaking.blocks(ChatterPhrase::CommanderDown, context) &&
			!speaking.blocks(ChatterPhrase::KilledFriend, context),
		"confirmed speech retains native own intention redundancy");
	context.now = 80.4f;
	check(speaking.blocks(ChatterPhrase::CommanderDown, context),
		"own spoken statement remains until next update removes it");
	speaking.maintain(context);
	check(!speaking.blocks(ChatterPhrase::CommanderDown, context), "next update releases finished speech");
	const auto staleRevision = speaking.revision();
	speaking.reset();
	speaking.maintain(context);
	check(!speaking.record(context, ChatterPhrase::KilledFriend, 81, staleRevision),
		"reset during transport rejects stale speech confirmation");
	check(speaking.record(context, ChatterPhrase::KilledFriend, 81, speaking.revision()),
		"new transport records current receipt");
	check(speaking.blocks(ChatterPhrase::KilledFriend, context), "confirmed rebuke rejects same own topic");
	++context.entityGeneration;
	check(!speaking.blocks(ChatterPhrase::KilledFriend, context),
		"replacement entity does not inherit own speaking state");
	speaking.maintain(context);
	const auto currentReceiptRevision = speaking.revision();
	check(
		speaking.canComplete(context, context, currentReceiptRevision), "current owner transport receipt can complete");
	auto replaced = context;
	++replaced.actor.generation;
	check(!speaking.canComplete(context, replaced, currentReceiptRevision),
		"replacement owner rejects stale transport completion");
	replaced = context;
	++replaced.frame.roundGeneration;
	check(
		!speaking.canComplete(context, replaced, currentReceiptRevision), "new round rejects old transport completion");
	check(usesUndefinedChatterPlace(ChatterPhrase::CommanderDown) &&
			usesUndefinedChatterPlace(ChatterPhrase::KilledFriend) &&
			!usesUndefinedChatterPlace(ChatterPhrase::EnemySpotted),
		"fixed death speech preserves native undefined place criteria");
	check(isIntentionPhrase(ChatterPhrase::CommanderDown) && isIntentionPhrase(ChatterPhrase::NiceShot) &&
			!isIntentionPhrase(ChatterPhrase::KilledFriend),
		"shared native intention type classification");
	return failures == 0 ? 0 : 1;
}
