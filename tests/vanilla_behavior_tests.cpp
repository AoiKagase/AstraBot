#include "astrabot/compat/combat_behavior.hpp"
#include "astrabot/compat/hunt_encounter.hpp"
#include <cmath>
#include <cstdio>

namespace
{
bool check(bool v, const char *message)
{
	if (!v)
		std::fprintf(stderr, "%s\n", message);
	return v;
}
} // namespace
int main()
{
	using namespace astrabot;
	compat::HuntEncounterPlanner encounter;
	bool encounterChecks = check(!encounter.beginPeripheralScan(0.25f) && encounter.beginPeripheralScan(0.3f) &&
		!encounter.beginPeripheralScan(0.5f), "peripheral scans use the native interval even without an encounter");
	const nav::NavVector from{0, 0, 30}, to{120, 0, 190};
	encounterChecks &= check(compat::huntEncounterProgress(from, to, {30, 100, 0}) == .5f &&
		compat::huntEncounterProgress(to, from, {200, 0, 0}) == 0.0f &&
		compat::huntEncounterProgress(from, to, {120, 0, 0}) == 1.0f,
		"encounter progress uses the dominant horizontal axis and 3D lead length with clamping");
	nav::NavEncounter path{};
	for (std::uint32_t i = 1; i <= 10; ++i)
		path.spots.push_back({i, .5f});
	encounter.markChecked(2, 1.0f);
	auto spots = encounter.eligible(path, .5f, 11.0f);
	encounterChecks &= check(spots.count == 8 && spots.ids[0] == 10 && spots.ids[1] == 3,
		"checked age includes the ten-second boundary and the most recent eight candidates retain ring order");
	for (std::uint32_t i = 20; i < 84; ++i)
		encounter.markChecked(i, 11.0f);
	encounterChecks &= check(!encounter.recentlyChecked(2, 11.0f) && encounter.recentlyChecked(83, 11.0f),
		"a full shared visibility history replaces the least recent entry");
	encounter.schedule(12.0f, .5f, 16.0f);
	encounterChecks &= check(!encounter.selectionDue(12.99f) && encounter.selectionDue(13.0f),
		"encounter interval scales by the fourth power of one minus skill");
	compat::ReactionQueue queue;
	compat::ThreatCandidate candidate = {{2U, 1U}, {100.0f, 0.0f, 32.0f}, 100.0f, false};
	compat::ThreatCandidate selected = {};
	const world::ActorKey observer = {1U, 1U};
	compat::ReactionQueue reloadQueue;
	unsigned completedSamples = 0;
	auto completeReload = [](void *context, compat::ThreatCandidate &sample) {
		++*static_cast<unsigned *>(context);
		sample.reloading = true;
		return true;
	};
	if (!check(!reloadQueue.update(observer, {1,1,1}, 1.0f, .1f, &candidate, 1, false, &selected, true,
			completeReload, &completedSamples) &&
			reloadQueue.update(observer, {1,1,2}, 1.1f, .1f, nullptr, 0, false, &selected, true,
				completeReload, &completedSamples) && selected.reloading && completedSamples == 1U,
			"selected threat reload is sampled once and survives into delayed recognition"))
		return 1;
	bool ok = encounterChecks && check(!queue.update(observer, {1U, 1U, 1U}, 0.0f, 0.2f, &candidate, 1U, false, &selected),
		"new contact waits for reaction");
	ok &= check(!queue.update(observer, {1U, 1U, 2U}, 0.1f, 0.2f, &candidate, 1U, false, &selected),
		"reaction has not elapsed");
	ok &= check(
		queue.update(observer, {1U, 1U, 3U}, 0.2f, 0.2f, &candidate, 1U, false, &selected) && selected.actor.slot == 2U,
		"delayed contact becomes recognized");
	candidate.visiblePoint.x = 200.0f;
	ok &= check(queue.update(observer, {1U, 1U, 4U}, 0.3f, 0.2f, &candidate, 1U, false, &selected) &&
			selected.visiblePoint.x == 100.0f,
		"reaction uses historical position");
	ok &= check(!queue.update(observer, {1U, 1U, 5U}, 0.4f, 0.2f, &candidate, 1U, true, &selected),
		"blind observer cannot acquire contact");
	ok &= check(!queue.update(observer, {1U, 2U, 1U}, 0.5f, 0.2f, &candidate, 1U, false, &selected),
		"round generation clears delayed knowledge");
	compat::ThreatCandidate threats[2] = {candidate, {{3U, 1U}, {1000.0f, 0.0f, 0.0f}, 1000.0f, true}};
	ok &= check(
		queue.update(observer, {1U, 2U, 2U}, 0.6f, 0.0f, threats, 2U, false, &selected) && selected.actor.slot == 3U,
		"attacker threat precedes nearer passive enemy");
	ok &= check(!queue.update(observer, {1U, 2U, 3U}, 0.7f, 0.0f, nullptr, 0U, false, &selected),
		"empty visible set clears threat");
	compat::ReactionQueue briefExposure;
	candidate.sourceEntity = {2U, 4U};
	ok &= check(!briefExposure.update(observer, {1U, 3U, 1U}, 1.0f, 0.3f, &candidate, 1U, false, &selected, true),
		"one visible sample has not reached the reaction queue head");
	ok &= check(!briefExposure.update(observer, {1U, 3U, 2U}, 1.12f, 0.3f, nullptr, 0U, false, &selected, true),
		"occlusion before the reaction deadline does not recognize the threat early");
	ok &= check(!briefExposure.update(observer, {1U, 3U, 12U}, 1.36f, 0.3f, nullptr, 0U, false, &selected, true),
		"elapsed time and frame-number gaps do not advance the full-update queue");
	ok &= check(briefExposure.update(observer, {1U, 3U, 15U}, 1.42f, 0.3f, nullptr, 0U, false, &selected, true) &&
			selected.actor == candidate.actor && selected.visiblePoint.x == candidate.visiblePoint.x &&
			selected.sourceEntity == candidate.sourceEntity,
		"delayed recognition keeps the sampled target and point after brief exposure ends");
	compat::ReactionQueue priorLife;
	ok &= check(!priorLife.update(observer, {1U, 3U, 1U}, 1.0f, 0.1f, &candidate, 1U, false, &selected, true),
		"one visible sample waits even if the observed player later dies");
	priorLife.forget(candidate.sourceEntity);
	ok &= check(!priorLife.update(observer, {1U, 3U, 2U}, 1.36f, 0.1f, &candidate, 1U, false, &selected, true),
		"death clears the older sighting even when a respawn reuses the entity identity");
	for (float delay : {-0.1f, 0.049f, 1.95f, 10.0f})
	{
		compat::ReactionQueue immediate;
		ok &= check(immediate.update(observer, {1U, 3U, 1U}, 1.0f, delay, &candidate, 1U, false, &selected, true),
			"native delay rounds to the current slot or wraps a full turn");
	}
	compat::ReactionQueue halfTick;
	ok &= check(!halfTick.update(observer, {1U, 3U, 1U}, 1.0f, 0.05f, &candidate, 1U, false, &selected, true) &&
			halfTick.update(observer, {1U, 3U, 2U}, 1.01f, 0.05f, nullptr, 0U, false, &selected, true),
		"half a tick rounds up even when the second update is early");
	compat::ReactionQueue wrap;
	for (unsigned tick = 1; tick <= 42; ++tick)
	{
		candidate.visiblePoint.x = static_cast<float>(tick);
		const bool ready = wrap.update(observer, {1U, 3U, tick}, tick * 0.13f, 1.94f, &candidate, 1U,
			false, &selected, true);
		ok &= check(ready == (tick > 19U) && (!ready || selected.visiblePoint.x == static_cast<float>(tick - 19U)),
			"19 updates reaches the oldest native sample across ring wraps");
	}
	ok &= check(!wrap.update(observer, {1U, 4U, 43U}, 6.0f, 1.94f, &candidate, 1U, false, &selected, true),
		"round change clears the native ring");
	compat::ReactionQueue afterBlindness;
	ok &= check(!afterBlindness.update(observer, {1U, 3U, 1U}, 1.0f, 0.3f, &candidate, 1U, false, &selected, true),
		"visible contact starts a three-update native reaction");
	for (unsigned tick = 2; tick <= 3; ++tick)
		ok &= check(!afterBlindness.update(observer, {1U, 3U, tick}, tick * 1.0f, 0.3f, nullptr, 0U, true, &selected, true),
			"blinded full updates do not advance the native reaction queue");
	for (unsigned tick = 4; tick <= 6; ++tick)
		ok &= check(afterBlindness.update(observer, {1U, 3U, tick}, tick * 1.0f, 0.3f, nullptr, 0U, false,
						&selected, true) == (tick == 6U),
			"the native reaction finishes only after three unblinded updates");
	compat::ReactionQueue attentionQueue;
	candidate.visibleParts = world::VisibleLeftSide;
	candidate.distance = 180.0f;
	// Compare against the exact stored float32 value, including on x87.
	constexpr float unstartedElapsed = 99999.8984375f;
	auto attention = attentionQueue.nativeAttention(observer, {1U, 4U, 1U}, 0.0f);
	ok &= check(!attention.skipsDraw(candidate) && attention.elapsed == unstartedElapsed &&
			compat::nativeThreatNoticeChance(candidate, attention.elapsed, .25f) > 100.0f,
		"an unstarted timer still requests a draw even when its threshold exceeds 100");
	attentionQueue.update(observer, {1U, 4U, 1U}, 0.0f, 0.0f, &candidate, 1U, false, &selected, true);
	attention = attentionQueue.nativeAttention(observer, {1U, 4U, 2U}, 0.12f);
	ok &= check(attention.elapsed == unstartedElapsed && attention.skipsDraw(candidate),
		"timestamp zero is unstarted while the previous write may still contain an enemy");
	attentionQueue.update(observer, {1U, 4U, 2U}, 1.0f, 0.0f, &candidate, 1U, false, &selected, true);
	attentionQueue.update(observer, {1U, 4U, 3U}, 1.1f, 0.0f, nullptr, 0U, true, &selected, true);
	attention = attentionQueue.nativeAttention(observer, {1U, 4U, 4U}, 1.25f);
	ok &= check(attention.elapsed == .25f && attention.skipsDraw(candidate),
		"blindness advances neither the attention clock nor the last sample");
	auto newGeneration = candidate;
	newGeneration.sourceEntity.generation++;
	ok &= check(!attention.skipsDraw(newGeneration), "a new entity in the same slot draws again");
	attentionQueue.update(observer, {1U, 4U, 4U}, 1.25f, 0.0f, nullptr, 0U, false, &selected, true);
	attention = attentionQueue.nativeAttention(observer, {1U, 4U, 5U}, 1.37f);
	const float edgeChance = compat::nativeThreatNoticeChance(candidate, attention.elapsed, .25f);
	ok &= check(!attention.skipsDraw(candidate) && std::fabs(edgeChance - 6.0f) < .001f,
		"an empty sample resumes draws using elapsed time and the left-side weight");
	candidate.speedSquared = 40000.0f;
	ok &= check(!attention.skipsDraw(candidate), "200 units per second still draws");
	candidate.speedSquared = 40001.0f;
	ok &= check(attention.skipsDraw(candidate), "running faster than 200 bypasses the draw");
	candidate.speedSquared = 0.0f;
	candidate.distance = 1000.0f;
	candidate.crouching = true;
	ok &= check(std::fabs(compat::nativeThreatNoticeChance(candidate, .25f, 0.0f) - .5f) < .001f &&
			std::fabs(compat::nativeThreatNoticeChance(candidate, 0.0f, 0.0f) - .1f) < .00001f,
		"the far stationary crouch and minimum floor use native thresholds");
	attentionQueue.resetForLife();
	attention = attentionQueue.nativeAttention(observer, {1U, 5U, 1U}, 1.5f);
	ok &= check(attention.elapsed == .25f && !attention.skipsDraw(candidate),
		"the queue can reset for a new life while the attention clock persists");
	attentionQueue.reset();
	ok &= check(attentionQueue.nativeAttention({1U, 2U}, {2U, 1U, 1U}, 1.5f).elapsed == unstartedElapsed,
		"replacing an observer drops its attention clock");
	compat::ReactionQueue threatRetention;
	candidate.distance = 100.0f;
	candidate.attackingObserver = true;
	auto closer = candidate;
	closer.actor.slot = closer.sourceEntity.id = 3U;
	closer.attackingObserver = false;
	threatRetention.update(observer, {1U, 5U, 1U}, 1.0f, 0.0f, &candidate, 1U, false, &selected, true);
	const float ranges[] = {72.0f, 70.0f, 69.0f};
	for (unsigned entry = 0; entry < 3U; ++entry)
	{
		closer.distance = ranges[entry];
		const compat::ThreatCandidate competing[] = {candidate, closer};
		const world::FrameIdentity frame{1U, 5U, entry + 2U};
		const float now = 1.125f + entry * .125f;
		const auto &winner = entry == 2U ? closer : candidate;
		ok &= check(threatRetention.update(observer, frame, now, 0.0f, competing, 2U, false, &selected, true) &&
				selected.actor == winner.actor && threatRetention.nativeAttention(observer, frame, now).isPrevious(winner),
			"only a candidate strictly within 70 percent replaces the previous threat's draw exemption");
	}
	compat::AimStep aim = compat::trackAim(179.0f, 0.0f, -179.0f, 0.0f, 0.01f, 0.5f);
	ok &= check(aim.yaw > 179.0f && aim.yaw < 180.0f, "aim takes shortest angular turn with finite acceleration");
	aim = compat::trackAim(0.0f, 0.0f, 90.0f, 0.0f, 0.1f, 1.0f);
	ok &= check(aim.yaw > 0.0f && aim.yaw < 90.0f && !aim.aligned, "aim inertia prevents instantaneous target snap");
	ok &= check(!compat::weaponEngagementAllowed(combat::WeaponKind::Knife, 200.0f), "knife cannot fire beyond reach");
	ok &= check(!compat::weaponEngagementAllowed(combat::WeaponKind::Grenade, 100.0f),
		"grenades require dedicated throw solution");
	ok &= check(
		compat::weaponEngagementAllowed(combat::WeaponKind::Rifle, 200.0f), "known firearm uses normal engagement");
	return ok ? 0 : 1;
}
