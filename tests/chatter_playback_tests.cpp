#include "astrabot/compat/chatter_playback.hpp"
#include <cstdio>
#include <cstring>
#include <limits>
namespace
{
using namespace astrabot::compat;
bool check(bool ok, const char *message)
{
	if (!ok)
		std::fprintf(stderr, "check failed: %s\n", message);
	return ok;
}
// The RNG boundary receives float32 arguments even on x87.
bool sameFloat32(float actual, float expected)
{
	return std::memcmp(&actual, &expected, sizeof(float)) == 0;
}
ChatterPlaybackInput context(float now, int enemies = 2, int friends = 1)
{
	ChatterPlaybackInput in{};
	in.now = now;
	in.enemies = enemies;
	in.nearbyFriends = friends;
	in.enemiesRemaining = enemies;
	in.friendsRemaining = 1;
	return in;
}
}
int main()
{
	using namespace astrabot::compat;
	ChatterProgram two;
	two.append({ChatterPartKind::Fixed, ChatterPhrase::Affirmative});
	two.append({ChatterPartKind::Fixed, ChatterPhrase::Help});
	ChatterPlaybackState state;
	auto step = advanceChatterPlayback(two, state, context(10));
	if (!check(step.kind == ChatterPlaybackKind::Phrase && step.phrase == ChatterPhrase::Affirmative && state.speaking,
			"selected statement starts exactly its first phrase and retains logical speaking"))
		return 1;
	const auto oldStep = step.step;
	if (!check(finishChatterPlaybackPart(state, step.step, .4f), "first real clip duration commits"))
		return 1;
	if (!check(advanceChatterPlayback(two, state, context(10.5f)).kind == ChatterPlaybackKind::Waiting,
			"next phrase waits through exact duration plus gap boundary"))
		return 1;
	step = advanceChatterPlayback(two, state, context(10.625f));
	if (!check(step.kind == ChatterPlaybackKind::Phrase && step.phrase == ChatterPhrase::Help,
			"a later selected update begins the second phrase"))
		return 1;
	if (!check(!finishChatterPlaybackPart(state, oldStep, 20) && finishChatterPlaybackPart(state, step.step, .4f),
			"old part completion cannot delay or finish the current part"))
		return 1;
	if (!check(advanceChatterPlayback(two, state, context(11.125f)).kind == ChatterPlaybackKind::Waiting &&
				advanceChatterPlayback(two, state, context(11.25f)).kind == ChatterPlaybackKind::Finished,
			"end of last clip requires a separate strict completion update"))
		return 1;
	if (!check(advanceChatterPlayback(two, state, context(12)).kind == ChatterPlaybackKind::Waiting,
			"completed logical statement publishes completion only once"))
		return 1;
	ChatterProgram capacity;
	if (!check(!capacity.append({ChatterPartKind::Fixed, ChatterPhrase::None}),
			"null fixed phrase is ignored without consuming a part"))
		return 1;
	for (unsigned i = 0; i < 4; ++i)
		capacity.append({ChatterPartKind::Fixed, ChatterPhrase::Affirmative});
	if (!check(!capacity.append({ChatterPartKind::Fixed, ChatterPhrase::Help}),
			"extra appended phrase cannot replace native four-part program"))
		return 1;
	ChatterPlaybackState four;
	for (unsigned i = 0; i < 4; ++i)
	{
		auto part = advanceChatterPlayback(capacity, four, context(20 + static_cast<float>(i)));
		if (!check(part.phrase == ChatterPhrase::Affirmative, "first four appended parts all survive"))
			return 1;
		finishChatterPlaybackPart(four, part.step, .4f);
	}
	if (!check(advanceChatterPlayback(capacity, four, context(24)).kind == ChatterPlaybackKind::Finished,
			"fifth append was not spoken"))
		return 1;
	ChatterProgram accumulation;
	accumulation.append({ChatterPartKind::Fixed, ChatterPhrase::Affirmative});
	accumulation.append({ChatterPartKind::AccumulateEnemies});
	accumulation.append({ChatterPartKind::CurrentEnemyCount});
	ChatterPlaybackState accumulating;
	step = advanceChatterPlayback(accumulation, accumulating, context(30, 2, 1));
	finishChatterPlaybackPart(accumulating, step.step, .4f);
	if (!check(advanceChatterPlayback(accumulation, accumulating, context(30.625f, 2, 1)).kind ==
					ChatterPlaybackKind::Waiting &&
				accumulating.speaking,
			"accumulation is one silent part that keeps speaking ownership"))
		return 1;
	if (!check(
			advanceChatterPlayback(accumulation, accumulating, context(32, 2, 1)).kind == ChatterPlaybackKind::Waiting,
			"accumulation lasts through exact two seconds since statement start"))
		return 1;
	step = advanceChatterPlayback(accumulation, accumulating, context(32.125f, 3, 1));
	if (!check(step.kind == ChatterPlaybackKind::Phrase && step.phrase == ChatterPhrase::Help,
			"current enemy count is evaluated after accumulation and can request Help"))
		return 1;
	ChatterProgram immediate;
	immediate.append({ChatterPartKind::AccumulateEnemies});
	immediate.append({ChatterPartKind::CurrentEnemyCount});
	ChatterPlaybackState many;
	if (!check(advanceChatterPlayback(immediate, many, context(40, 4, 3)).kind == ChatterPlaybackKind::Waiting,
			"even many enemies do not skip two parts in the initial update"))
		return 1;
	step = advanceChatterPlayback(immediate, many, context(40.125f, 4, 3));
	if (!check(step.phrase == ChatterPhrase::EnemySpotted && step.enemyCount == 4,
			"more than three enemies release accumulation on a later update with exact count criteria"))
		return 1;
	for (const auto &test : {ChatterPlaybackInput{50, 2, 1, 2, 1, false}, ChatterPlaybackInput{50, 1, 0, 1, 1, false}})
	{
		ChatterProgram current;
		current.append({ChatterPartKind::CurrentEnemyCount});
		ChatterPlaybackState progress;
		auto output = advanceChatterPlayback(current, progress, test);
		if (!check(test.enemies == 2 ? output.kind == ChatterPlaybackKind::Phrase &&
						output.phrase == ChatterPhrase::EnemySpotted && output.enemyCount == 2
									 : output.kind == ChatterPlaybackKind::Null,
				"current count reports multiple supported enemies and skips a lone enemy"))
			return 1;
	}
	for (int count = -1; count <= 4; ++count)
	{
		ChatterProgram remaining;
		remaining.append({ChatterPartKind::RemainingEnemyCount});
		ChatterPlaybackState progress;
		auto in = context(60);
		in.enemiesRemaining = count;
		auto output = advanceChatterPlayback(remaining, progress, in);
		const auto expected = count == 0 ? ChatterPhrase::NoEnemiesLeft
			: count == 1				 ? ChatterPhrase::OneEnemyLeft
			: count == 2				 ? ChatterPhrase::TwoEnemiesLeft
			: count == 3				 ? ChatterPhrase::ThreeEnemiesLeft
										 : ChatterPhrase::None;
		if (!check(output.phrase == expected &&
					(count < 0 || count > 3 ? output.kind == ChatterPlaybackKind::Null
											: output.kind == ChatterPlaybackKind::Phrase),
				"remaining context resolves only literal native zero through three phrases"))
			return 1;
	}
	for (auto kind : {ChatterPartKind::ShortDelay, ChatterPartKind::LongDelay})
	{
		ChatterProgram delays;
		delays.append({kind});
		delays.append({ChatterPartKind::Fixed, ChatterPhrase::Help});
		ChatterPlaybackState progress;
		auto request = advanceChatterPlayback(delays, progress, context(70));
		if (!check(request.kind == ChatterPlaybackKind::Delay && progress.speaking &&
					sameFloat32(request.low, (kind == ChatterPartKind::ShortDelay ? .1f : 1.0f)) &&
					sameFloat32(request.high, (kind == ChatterPartKind::ShortDelay ? .5f : 2.0f)),
				"selected delay requests only native bounds while retaining logical speech"))
			return 1;
		if (!check(advanceChatterPlayback(delays, progress, context(70.125f)).kind == ChatterPlaybackKind::Waiting,
				"reentrant update cannot request a second in-flight delay"))
			return 1;
		if (!check(!finishChatterPlaybackDelay(progress, request.step, std::numeric_limits<float>::quiet_NaN()) &&
					cancelChatterPlaybackStep(progress, request.step),
				"unavailable delay can be cancelled without fabricating duration or consuming its context"))
			return 1;
		auto retry = advanceChatterPlayback(delays, progress, context(70.25f));
		if (!check(retry.kind == ChatterPlaybackKind::Delay &&
					!finishChatterPlaybackDelay(progress, request.step, request.high) &&
					finishChatterPlaybackDelay(progress, retry.step, retry.high),
				"retry owns a fresh token and only its valid delay can commit"))
			return 1;
		if (!check(advanceChatterPlayback(delays, progress, context(70.25f + retry.high)).kind ==
						ChatterPlaybackKind::Waiting &&
					advanceChatterPlayback(delays, progress, context(70.375f + retry.high)).phrase ==
						ChatterPhrase::Help,
				"delay ends only strictly later than the committed endpoint"))
			return 1;
	}
	ChatterPlaybackState zero;
	if (!check(advanceChatterPlayback(two, zero, context(0)).kind == ChatterPlaybackKind::Waiting && zero.speaking,
			"time zero starts logical speaking but cannot pass strict nextTime zero"))
		return 1;
	ChatterPlaybackState alone;
	auto noFriends = context(80);
	noFriends.friendsRemaining = 0;
	if (!check(advanceChatterPlayback(two, alone, noFriends).kind == ChatterPlaybackKind::Finished && !alone.speaking,
			"no-friend ordinary statement ends before native speaking begins"))
		return 1;

	// A Place part preserves metadata identity without aliasing the NAV-local place.
	ChatterProgram located;
	if (!check(!located.append({ChatterPartKind::Place, ChatterPhrase::None, 0, 93}) &&
				!located.append({ChatterPartKind::Place, ChatterPhrase::None, 17, 0}),
			"undefined place references do not consume a native part"))
		return 1;
	located.append({ChatterPartKind::Place, ChatterPhrase::None, 17, 93});
	located.append({ChatterPartKind::Fixed, ChatterPhrase::Help});
	ChatterPlaybackState locationState;
	auto location = advanceChatterPlayback(located, locationState, context(100));
	if (!check(location.kind == ChatterPlaybackKind::Phrase && location.phrase == ChatterPhrase::None &&
				location.placePhrase == 93,
			"place playback returns the default metadata reference rather than the NAV-local identifier"))
		return 1;
	if (!check(!finishChatterPlaybackPart(locationState, location.step, -1) &&
				!finishChatterPlaybackPart(locationState, location.step, std::numeric_limits<float>::quiet_NaN()) &&
				finishChatterPlaybackPart(locationState, location.step, 2.0f),
			"invalid duration cannot consume a pending part; radio supplies native two seconds"))
		return 1;
	if (!check(advanceChatterPlayback(located, locationState, context(102.1f)).kind == ChatterPlaybackKind::Waiting &&
				advanceChatterPlayback(located, locationState, context(102.125f)).phrase == ChatterPhrase::Help,
			"mapped radio part retains its strict two-second duration and phrase gap"))
		return 1;
	ChatterProgram silent;
	silent.append({ChatterPartKind::CurrentEnemyCount});
	silent.append({ChatterPartKind::Fixed, ChatterPhrase::Affirmative});
	ChatterPlaybackState silentState;
	auto skipped = advanceChatterPlayback(silent, silentState, context(110, 1, 0));
	if (!check(skipped.kind == ChatterPlaybackKind::Null && silentState.speaking &&
				!finishChatterPlaybackPart(silentState, skipped.step, 1),
			"null context stays logically speaking without a fabricated in-flight clip"))
		return 1;
	if (!check(
			advanceChatterPlayback(silent, silentState, context(110.125f, 1, 0)).phrase == ChatterPhrase::Affirmative,
			"null context advances only on the next selected invocation"))
		return 1;
	for (auto kind : {ChatterPartKind::ShortDelay, ChatterPartKind::LongDelay})
	{
		ChatterProgram delays;
		delays.append({kind});
		delays.append({ChatterPartKind::Fixed, ChatterPhrase::Help});
		ChatterPlaybackState boundary;
		auto request = advanceChatterPlayback(delays, boundary, context(120));
		if (!check(!finishChatterPlaybackDelay(
					   boundary, request.step, kind == ChatterPartKind::ShortDelay ? .099f : .999f) &&
					!finishChatterPlaybackDelay(
						boundary, request.step, kind == ChatterPartKind::ShortDelay ? .501f : 2.001f) &&
					finishChatterPlaybackDelay(boundary, request.step, request.low),
				"only a native-range RNG result can commit, including the float32 minimum"))
			return 1;
		const float exact = kind == ChatterPartKind::ShortDelay ? 120.1f : 121.0f;
		const float after = kind == ChatterPartKind::ShortDelay ? 120.125f : 121.125f;
		if (!check(advanceChatterPlayback(delays, boundary, context(exact)).kind == ChatterPlaybackKind::Waiting &&
					advanceChatterPlayback(delays, boundary, context(after)).phrase == ChatterPhrase::Help,
				"minimum delay retains the exact endpoint and releases strictly after it"))
			return 1;
	}
	ChatterPlaybackState emoteState;
	auto emote = context(130);
	emote.friendsRemaining = 0;
	emote.emote = true;
	if (!check(
			advanceChatterPlayback(two, emoteState, emote).phrase == ChatterPhrase::Affirmative && emoteState.speaking,
			"native Emote exception can speak with no living teammate"))
		return 1;
	for (float invalid : {-1.0f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()})
	{
		ChatterPlaybackState untouched;
		if (!check(advanceChatterPlayback(two, untouched, context(invalid)).kind == ChatterPlaybackKind::Waiting &&
					!untouched.speaking && untouched.index == -1,
				"invalid timing cannot start or advance logical speech"))
			return 1;
	}
	ChatterProgram empty;
	ChatterPlaybackState emptyState;
	if (!check(advanceChatterPlayback(empty, emptyState, context(140)).kind == ChatterPlaybackKind::Finished &&
				!emptyState.speaking,
			"empty program ends without a false speaking owner"))
		return 1;

	return 0;
}
