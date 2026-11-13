#include "astrabot/compat/teamwork_policy.hpp"
#include <cstdio>

int main()
{
	using namespace astrabot::compat;
	int failures = 0;
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
