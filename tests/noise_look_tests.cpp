#include "astrabot/compat/noise_look.hpp"
#include <cstdio>
#include <cmath>
int main()
{
	using namespace astrabot;
	using namespace compat;
	unsigned failures = 0;
	const auto check = [&](bool ok, const char *message)
	{
		if (!ok)
		{
			++failures;
			std::fprintf(stderr, "FAIL %s\n", message);
		}
	};
	NoiseLookInput in{};
	in.actor = {1, 7};
	in.frame = {2, 3, 10};
	in.now = 10.25f;
	in.noiseTime = 10;
	in.hasNoise = true;
	in.reactionSeconds = .25f;
	in.distance = 300;
	in.clearLine = true;
	in.enemiesRemaining = 3;
	check(noiseLookKind(in, 0) == NoiseLookKind::Ordinary, "ordinary noise waits exactly for reaction time");
	in.now = 10.125f;
	check(noiseLookKind(in, 0) == NoiseLookKind::None, "ordinary noise cannot bypass reaction delay");
	in.highPriority = true;
	check(noiseLookKind(in, 0) == NoiseLookKind::Startle,
		"near clear high-priority noise startles before reaction delay");
	in.distance = 1000;
	check(noiseLookKind(in, 0) == NoiseLookKind::None, "1000-unit boundary is not a close dangerous noise");
	in.now = 10.5f;
	in.highPriority = false;
	check(noiseLookKind(in, 0) == NoiseLookKind::Ordinary, "freshness includes exactly half a second");
	in.now = 10.75f;
	check(noiseLookKind(in, 0) == NoiseLookKind::None, "stale sound never starts a new look");
	in.now = 9;
	check(noiseLookKind(in, 0) == NoiseLookKind::None, "future receipt time is not a fresh sound");
	in.now = 10.25f;
	in.visibleEnemy = true;
	check(noiseLookKind(in, 0) == NoiseLookKind::None, "visible enemy aim has precedence");
	in.visibleEnemy = false;
	check(noiseLookKind(in, 11) == NoiseLookKind::None, "startle inhibition prevents repeated noise requests");
	NoiseLookPlanner planner;
	planner.synchronize(in.actor, in.frame, in.now);
	const RandomTimingContext timing{1, 0, 1};
	std::vector<RandomTapeEntry> tape = {RandomTapeEntry::floatEntry("NoiseLook.Duration", {1, 7}, timing, 1, 2, 1.5f)};
	ScriptedRandomSource random(tape);
	check(planner.request(in, NoiseLookKind::Ordinary, {0, 300, 36}, random, timing) && random.verifyComplete(),
		"moving look uses reference 1-2 second duration request");
	float pitch = 0, yaw = 0;
	check(planner.update(in.now, {0, 0, 0}, {0, 0, 36}, 0, 0, &pitch, &yaw) && yaw > 0 && yaw < 90,
		"non-attacking aim spring turns gradually toward the sound");
	for (unsigned n = 1; n < 90; ++n)
	{
		in.now += 1.0f / 30.0f;
		planner.update(in.now, {0, 0, 0}, {0, 0, 36}, pitch, yaw, &pitch, &yaw);
	}
	check(!planner.active(), "look duration eventually expires after alignment");
	in.now = 20;
	in.noiseTime = 20;
	in.highPriority = true;
	in.distance = 300;
	planner.synchronize(in.actor, in.frame, in.now);
	ScriptedRandomSource startle({RandomTapeEntry::floatEntry("NoiseLook.StartleInhibit", {1, 7}, timing, 2, 4, 3)});
	check(planner.request(in, NoiseLookKind::Startle, {300, 0, 36}, startle, timing) && startle.verifyComplete(),
		"startle draws only its 2-4 second inhibit interval");
	check(planner.update(20, {0, 0, 0}, {0, 0, 36}, 0, 0, &pitch, &yaw), "startle begins aligned hold");
	check(!planner.update(20.5f, {0, 0, 0}, {0, 0, 36}, 0, 0, &pitch, &yaw) && planner.inhibited(22.9f) &&
			!planner.inhibited(23),
		"short startle ends while look-around inhibition persists");
	in.hiding = true;
	in.highPriority = false;
	in.now = 30;
	in.noiseTime = 30;
	in.enemiesRemaining = 1;
	planner.synchronize(in.actor, in.frame, in.now);
	ScriptedRandomSource hiding({RandomTapeEntry::floatEntry("NoiseLook.Duration", {1, 7}, timing, 5, 15, 10)});
	check(planner.request(in, NoiseLookKind::Ordinary, {300, 0, 36}, hiding, timing) && hiding.verifyComplete(),
		"last-enemy hiding look lasts 5-15 seconds");
	check(!planner.update(30, {250, 0, 0}, {250, 0, 36}, 0, 0, &pitch, &yaw),
		"ordinary look clears within 100 units of its point");
	in.enemiesRemaining = 2;
	ScriptedRandomSource otherHiding({RandomTapeEntry::floatEntry("NoiseLook.Duration", {1, 7}, timing, 2, 5, 3)});
	check(
		planner.request(in, NoiseLookKind::Ordinary, {300, 0, 36}, otherHiding, timing) && otherHiding.verifyComplete(),
		"other hiding look lasts 2-5 seconds");
	auto nextFrame = in.frame;
	++nextFrame.roundGeneration;
	planner.synchronize(in.actor, nextFrame, 31);
	check(!planner.active() && !planner.inhibited(31), "round identity clears look and inhibit state");
	in.hiding = false;
	in.now = 40;
	in.noiseTime = 40;
	planner.synchronize(in.actor, in.frame, in.now);
	ScriptedRandomSource reuse({RandomTapeEntry::floatEntry("NoiseLook.Duration", {1, 7}, timing, 1, 2, 1.5f)});
	check(
		planner.request(in, NoiseLookKind::Ordinary, {300, 0, 36}, reuse, timing), "actor reuse fixture starts a look");
	planner.synchronize({1, 8}, in.frame, 40.1f);
	check(!planner.active(), "actor generation reuse discards the old point");
	in.actor = {1, 8};
	in.now = 41;
	planner.synchronize(in.actor, in.frame, 41);
	ScriptedRandomSource backwards({RandomTapeEntry::floatEntry("NoiseLook.Duration", {1, 8}, timing, 1, 2, 1.5f)});
	check(planner.request(in, NoiseLookKind::Ordinary, {300, 0, 36}, backwards, timing),
		"clock reversal fixture starts a look");
	planner.synchronize(in.actor, in.frame, 39);
	check(!planner.active() && !planner.inhibited(39), "clock reversal clears look and inhibit state");
	check(isStandardShotSample("weapons/ak47-1.wav") && !isStandardShotSample("weapons/ak47_clipin.wav") &&
			!isStandardShotSample("weapons/c4_beep1.wav") && !isStandardShotSample("weapons/custom-shot.wav"),
		"only known shot samples establish dangerous gunfire");
	return failures ? 1 : 0;
}
