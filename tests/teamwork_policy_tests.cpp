#include "astrabot/compat/teamwork_policy.hpp"
#include <cstdio>

namespace
{
using namespace astrabot::compat;
struct CallbackRogueRandom : ICompatibilityRandomSource
{
	ScriptedRandomSource tape{{RandomTapeEntry::floatEntry("Teamwork.RogueInterval", {}, {}, 10, 30, 10),
		RandomTapeEntry::floatEntry("Teamwork.RogueChance", {}, {}, 0, 100, 20)}};
	unsigned calls = 0;
	std::function<void(unsigned)> callback;
	RandomFloatResult nextFloat(const RandomRequest &request) override
	{
		const auto result = tape.nextFloat(request);
		++calls;
		if (callback)
			callback(calls);
		return result;
	}
	RandomLongResult nextLong(const RandomRequest &request) override
	{
		return tape.nextLong(request);
	}
};
int testLazyRogue()
{
	int failures = 0;
	const auto check = [&](bool value, const char *message)
	{
		if (!value)
		{
			++failures;
			std::fprintf(stderr, "FAIL: %s\n", message);
		}
	};
	TeamworkInput input;
	input.actor = {1, 1};
	input.frame = {1, 1, 1};
	input.now = 1;
	input.teamwork = .75f;
	input.allowRoguesKnown = input.allowRogues = true;
	input.roundTimeKnown = true;
	input.elapsedRound = 6;
	input.candidate = {2, 1};
	input.candidateVisibleHumanFriend = input.candidateReachable = true;
	input.candidateRange = input.candidateTravel = 100;
	TeamworkPolicy policy;
	ScriptedRandomSource tape({RandomTapeEntry::floatEntry("Teamwork.RogueInterval", {}, {}, 10, 30, 10),
		RandomTapeEntry::floatEntry("Teamwork.RogueChance", {}, {}, 0, 100, 20),
		RandomTapeEntry::floatEntry("Teamwork.AutoFollow", {}, {}, 0, 1, .1f),
		RandomTapeEntry::floatEntry("Teamwork.RogueInterval", {}, {}, 10, 30, 30),
		RandomTapeEntry::floatEntry("Teamwork.RogueChance", {}, {}, 0, 100, 25)});
	auto result = policy.queryRogue(input, tape, {}, {});
	check(result.available && result.rogueKnown && result.rogue && !result.startFollowing && tape.position() == 2,
		"lazy Rogue never consumes AutoFollow or starts following");
	result = policy.queryRogue(input, tape, {}, {});
	check(result.available && result.rogueKnown && result.rogue && tape.position() == 2,
		"same-frame lazy Rogue query reuses the shared cache");
	result = policy.update(input, tape, {}, {});
	check(result.available && result.rogue && result.startFollowing && tape.position() == 3,
		"first normal update after lazy query still runs AutoFollow without losing Rogue cache");
	result = policy.queryRogue(input, tape, {}, {});
	check(result.available && result.rogue && !result.startFollowing && tape.position() == 3,
		"lazy Rogue remains queryable after a same-frame normal update");
	input.now = 11;
	check(policy.queryRogue(input, tape, {}, {}).rogue && tape.position() == 3,
		"lazy cache deadline is strict without a full-update tick advance");
	input.now = 11.1f;
	result = policy.queryRogue(input, tape, {}, {});
	check(result.available && result.rogueKnown && !result.rogue && tape.verifyComplete(),
		"expired lazy query refreshes interval then chance with the native strict teamwork threshold");
	TeamworkPolicy toggled;
	input.now = 1;
	input.allowRogues = false;
	input.roundTimeKnown = false;
	ScriptedRandomSource toggleTape({RandomTapeEntry::floatEntry("Teamwork.RogueInterval", {}, {}, 10, 30, 10),
		RandomTapeEntry::floatEntry("Teamwork.RogueChance", {}, {}, 0, 100, 20)});
	result = toggled.queryRogue(input, toggleTape, {}, {});
	check(result.available && result.rogueKnown && !result.rogue && toggleTape.position() == 0,
		"disabled rogue query returns false with no RNG");
	input.allowRogues = true;
	check(toggled.queryRogue(input, toggleTape, {}, {}).rogue,
		"same-tick enabling rogues can lazily draw the first status");
	input.allowRogues = false;
	toggled.queryRogue(input, toggleTape, {}, {});
	input.allowRogues = true;
	check(toggled.queryRogue(input, toggleTape, {}, {}).rogue && toggleTape.verifyComplete(),
		"temporary disabling does not destroy the native cached rogue timer/status");
	for (unsigned interrupt : {1U, 2U})
	{
		TeamworkPolicy reset;
		CallbackRogueRandom callback;
		callback.callback = [&](unsigned call)
		{
			if (call == interrupt)
				reset.reset();
		};
		check(!reset.queryRogue(input, callback, {}, {}).available && callback.calls == interrupt,
			"reset inside either Rogue draw aborts before stale publication or another draw");
		ScriptedRandomSource fresh({RandomTapeEntry::floatEntry("Teamwork.RogueInterval", {}, {}, 10, 30, 20),
			RandomTapeEntry::floatEntry("Teamwork.RogueChance", {}, {}, 0, 100, 99)});
		result = reset.queryRogue(input, fresh, {}, {});
		check(result.available && result.rogueKnown && !result.rogue && fresh.verifyComplete(),
			"fresh owner after RNG reset cannot inherit the stale interval/chance");
		TeamworkPolicy lost;
		CallbackRogueRandom stale;
		bool current = true;
		stale.callback = [&](unsigned call)
		{
			if (call == interrupt)
				current = false;
		};
		check(!lost.queryRogue(input, stale, {}, {},
					   [&]
					   {
						   return current;
					   })
					.available &&
				stale.calls == interrupt,
			"external receipt loss in either draw aborts the lazy decision");
	}
	TeamworkPolicy nested;
	CallbackRogueRandom old;
	auto replacement = input;
	++replacement.actor.generation;
	bool freshAccepted = false;
	old.callback = [&](unsigned call)
	{
		if (call != 1)
			return;
		nested.reset();
		ScriptedRandomSource fresh({RandomTapeEntry::floatEntry("Teamwork.RogueInterval", {}, {}, 10, 30, 20),
			RandomTapeEntry::floatEntry("Teamwork.RogueChance", {}, {}, 0, 100, 99)});
		const auto value = nested.queryRogue(replacement, fresh, {}, {});
		freshAccepted = value.available && value.rogueKnown && !value.rogue && fresh.verifyComplete();
	};
	check(!nested.queryRogue(input, old, {}, {}).available && freshAccepted,
		"nested fresh-owner query cancels the outer old-owner decision");
	ScriptedRandomSource none({});
	result = nested.queryRogue(replacement, none, {}, {});
	check(result.available && result.rogueKnown && !result.rogue && !none.failed() && none.position() == 0,
		"outer callback continuation cannot overwrite the freshly cached owner");
	TeamworkPolicy sameObject;
	input.now = 1;
	input.frame = {1, 1, 1};
	input.allowRogues = true;
	input.roundTimeKnown = false;
	ScriptedRandomSource resetTape({RandomTapeEntry::floatEntry("Teamwork.RogueInterval", {}, {}, 10, 30, 10),
		RandomTapeEntry::floatEntry("Teamwork.RogueChance", {}, {}, 0, 100, 20),
		RandomTapeEntry::floatEntry("Teamwork.RogueInterval", {}, {}, 10, 30, 30),
		RandomTapeEntry::floatEntry("Teamwork.RogueChance", {}, {}, 0, 100, 99)});
	sameObject.queryRogue(input, resetTape, {}, {});
	sameObject.resetValues();
	input.now = 2;
	++input.frame.tick;
	result = sameObject.queryRogue(input, resetTape, {}, {});
	check(result.available && result.rogueKnown && !result.rogue && resetTape.position() == 2,
		"native same-object ResetValues clears rogue status but preserves its running countdown");
	input.now = 11;
	result = sameObject.queryRogue(input, resetTape, {}, {});
	check(result.available && !result.rogue && resetTape.position() == 2,
		"ResetValues keeps the exact native countdown deadline");
	input.now = 11.1f;
	result = sameObject.queryRogue(input, resetTape, {}, {});
	check(result.available && !result.rogue && resetTape.verifyComplete(),
		"same-object timer draws again only after its retained deadline");
	TeamworkPolicy round;
	input.now = 1;
	input.frame = {1, 1, 1};
	ScriptedRandomSource roundTape({RandomTapeEntry::floatEntry("Teamwork.RogueInterval", {}, {}, 10, 30, 10),
		RandomTapeEntry::floatEntry("Teamwork.RogueChance", {}, {}, 0, 100, 20),
		RandomTapeEntry::floatEntry("Teamwork.RogueInterval", {}, {}, 10, 30, 20),
		RandomTapeEntry::floatEntry("Teamwork.RogueChance", {}, {}, 0, 100, 99)});
	round.queryRogue(input, roundTape, {}, {});
	input.now = 2;
	++input.frame.roundGeneration;
	result = round.queryRogue(input, roundTape, {}, {});
	check(result.available && !result.rogue && roundTape.position() == 2,
		"a new round on the same actor preserves native rogue countdown and resets its value");
	++input.frame.mapGeneration;
	result = round.queryRogue(input, roundTape, {}, {});
	check(result.available && !result.rogue && roundTape.verifyComplete(),
		"new map owner does not reuse an old-map rogue countdown");
	TeamworkPolicy resetDuringChance;
	input.now = 1;
	input.frame = {1, 1, 1};
	CallbackRogueRandom chance;
	chance.callback = [&](unsigned call)
	{
		if (call == 2)
			resetDuringChance.resetValues();
	};
	check(!resetDuringChance.queryRogue(input, chance, {}, {}).available,
		"ResetValues in chance draw cancels stale old status");
	ScriptedRandomSource noFurtherDraw({});
	result = resetDuringChance.queryRogue(input, noFurtherDraw, {}, {});
	check(result.available && result.rogueKnown && !result.rogue && !noFurtherDraw.failed(),
		"ResetValues during chance retains committed interval but cannot publish old chance result");

	return failures;
}
}

int main()
{
	using namespace astrabot::compat;
	int failures = testLazyRogue();
	const auto check = [&](bool value, const char *message)
	{
		if (!value)
		{
			++failures;
			std::fprintf(stderr, "FAIL: %s\n", message);
		}
	};
	TeamworkInput input{};
	input.actor = {1, 1};
	input.frame = {1, 1, 1};
	input.now = 1;
	input.teamwork = 0.75f;
	input.allowRoguesKnown = true;
	input.allowRogues = true;
	TeamworkPolicy rogue;
	ScriptedRandomSource rogueTape({RandomTapeEntry::floatEntry("Teamwork.RogueInterval", {}, {}, 10, 30, 10),
		RandomTapeEntry::floatEntry("Teamwork.RogueChance", {}, {}, 0, 100, 20),
		RandomTapeEntry::floatEntry("Teamwork.RogueInterval", {}, {}, 10, 30, 30),
		RandomTapeEntry::floatEntry("Teamwork.RogueChance", {}, {}, 0, 100, 25)});
	auto result = rogue.update(input, rogueTape, {}, {});
	check(result.available && result.rogueKnown && result.rogue, "rogue probability uses inverse teamwork");
	input.frame.tick++;
	input.now = 11;
	check(rogue.update(input, rogueTape, {}, {}).rogue, "rogue timer has not elapsed at exact deadline");
	input.frame.tick++;
	input.now = 11.1f;
	result = rogue.update(input, rogueTape, {}, {});
	check(result.rogueKnown && !result.rogue && rogueTape.verifyComplete(),
		"reevaluation uses strict probability comparison and interval-first draw order");
	TeamworkPolicy follow;
	input = {};
	input.actor = {1, 1};
	input.frame = {1, 1, 1};
	input.now = 1;
	input.teamwork = 0.75f;
	input.roundTimeKnown = true;
	input.elapsedRound = 5;
	input.allowRoguesKnown = true;
	input.candidate = {2, 1};
	input.candidateVisibleHumanFriend = true;
	input.candidateReachable = true;
	input.candidateRange = 100;
	input.candidateTravel = 150;
	ScriptedRandomSource followTape({RandomTapeEntry::floatEntry("Teamwork.AutoFollow", {}, {}, 0, 1, 0.1f),
		RandomTapeEntry::floatEntry("Teamwork.AutoFollow", {}, {}, 0, 1, 0.1f)});
	check(!follow.update(input, followTape, {}, {}).startFollowing, "round time must be greater than five seconds");
	input.frame.tick++;
	input.now = 2;
	input.elapsedRound = 6;
	result = follow.update(input, followTape, {}, {});
	check(result.startFollowing && result.humanPermissionInferred,
		"visible reachable human follows with explicitly inferred private allowance");
	input.frame.tick++;
	input.now = 3;
	input.following = true;
	input.followDuration = 45;
	check(!follow.update(input, followTape, {}, {}).stopFollowing, "follow duration threshold is strict");
	input.frame.tick++;
	input.now = 4;
	input.followDuration = 45.1f;
	check(follow.update(input, followTape, {}, {}).stopFollowing, "low teamwork stops after 15+40*teamwork seconds");
	input.frame.tick++;
	input.now = 14;
	input.following = false;
	input.followDuration = 0;
	check(!follow.update(input, followTape, {}, {}).startFollowing,
		"stop-follow cooldown uses strict now greater than deadline");
	input.frame.tick++;
	input.now = 14.1f;
	check(follow.update(input, followTape, {}, {}).startFollowing && followTape.verifyComplete(),
		"auto-follow resumes only after ten-second stop cooldown");
	TeamworkPolicy decline;
	input.frame = {1, 1, 1};
	input.now = 1;
	ScriptedRandomSource declineTape({RandomTapeEntry::floatEntry("Teamwork.AutoFollow", {}, {}, 0, 1, 0.9f),
		RandomTapeEntry::floatEntry("Teamwork.AutoFollow", {}, {}, 0, 1, 0.1f)});
	check(!decline.update(input, declineTape, {}, {}).startFollowing, "chance rejection does not follow");
	input.frame.tick++;
	input.now = 23.5f;
	check(!decline.update(input, declineTape, {}, {}).startFollowing,
		"decline cooldown is 15+(1-teamwork)*30 and strict at boundary");
	input.frame.tick++;
	input.now = 23.6f;
	check(decline.update(input, declineTape, {}, {}).startFollowing && declineTape.verifyComplete(),
		"decline cooldown expires without repeated random draws");
	TeamworkPolicy human;
	input.frame = {1, 1, 1};
	input.now = 1;
	input.busy = true;
	ScriptedRandomSource humanTape({RandomTapeEntry::floatEntry("Teamwork.AutoFollow", {}, {}, 0, 1, 0.1f),
		RandomTapeEntry::floatEntry("Teamwork.AutoFollow", {}, {}, 0, 1, 0.1f),
		RandomTapeEntry::floatEntry("Teamwork.AutoFollow", {}, {}, 0, 1, 0.1f)});
	human.update(input, humanTape, {}, {});
	human.observeHumanPolicy(input.candidate, 1, false);
	input.frame.tick++;
	input.now = 2;
	input.busy = false;
	check(!human.update(input, humanTape, {}, {}).startFollowing,
		"public hold/go radio inhibits human auto-follow proxy for sixty seconds");
	input.frame.tick++;
	input.now = 61;
	check(!human.update(input, humanTape, {}, {}).startFollowing, "human inhibition remains active at exact deadline");
	input.frame.tick++;
	input.now = 61.1f;
	check(human.update(input, humanTape, {}, {}).startFollowing && humanTape.verifyComplete(),
		"public inhibition expiry permits inferred following");
	TeamworkPolicy noHidden;
	input.frame = {1, 1, 1};
	input.now = 1;
	input.candidateVisibleHumanFriend = false;
	ScriptedRandomSource visibilityTape({RandomTapeEntry::floatEntry("Teamwork.AutoFollow", {}, {}, 0, 1, 0.1f)});
	check(!noHidden.update(input, visibilityTape, {}, {}).startFollowing,
		"hidden human position cannot become an auto-follow target");
	return failures == 0 ? 0 : 1;
}
