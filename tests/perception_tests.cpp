#include "astrabot/perception/perception.hpp"

#include <cstdio>
#include <limits>
#include <vector>

namespace
{
	bool check(bool condition, const char *description)
	{
		if (condition)
		{
			return true;
		}

		std::fprintf(stderr, "check failed: %s\n", description);
		return false;
	}

	astrabot::world::SnapshotIdentity identity(std::uint32_t tick)
	{
		astrabot::world::SnapshotIdentity value = {};
		value.frame.mapGeneration = 7U;
		value.frame.roundGeneration = 3U;
		value.frame.tick = tick;
		value.observer = {1U, 11U};
		return value;
	}

	astrabot::world::ActorObservation visibleActor(
		astrabot::world::ActorKey actor)
	{
		astrabot::world::ActorObservation observation = {};
		observation.actor = actor;
		observation.state = astrabot::world::ObservationState::ObservedPresent;
		observation.relation = astrabot::world::TeamRelation::Hostile;
		observation.position = {64.0f, 32.0f, 16.0f};
		observation.velocity = {};
		observation.confidence = {0.8f, 0U};
		return observation;
	}

	astrabot::perception::PerceptionInput emptyInput(std::uint32_t tick)
	{
		return astrabot::perception::PerceptionInput(identity(tick));
	}
}

bool testDuplicateAndStaleFramesPreservePublication()
{
	using astrabot::perception::PerceptionAssembler;
	using astrabot::perception::PerceptionInput;
	using astrabot::perception::PerceptionResult;
	using astrabot::world::WorldSnapshot;

	PerceptionAssembler assembler;
	PerceptionInput first(identity(10U));
	first.addActor(visibleActor({2U, 1U}));
	WorldSnapshot snapshot;
	if (!check(assembler.publish(first, &snapshot) == PerceptionResult::Published,
			"first frame is published"))
	{
		return false;
	}

	PerceptionInput duplicate(identity(10U));
	duplicate.addActor(visibleActor({3U, 1U}));
	if (!check(assembler.publish(duplicate, &snapshot) ==
			PerceptionResult::DuplicateFrame,
			"duplicate frame is rejected"))
	{
		return false;
	}

	const astrabot::world::ActorObservation *actor = nullptr;
	if (!check(snapshot.findActor({2U, 1U}, &actor) ==
			astrabot::world::ContactLookupResult::Found,
			"duplicate rejection preserves prior snapshot"))
	{
		return false;
	}

	PerceptionInput stale(identity(9U));
	if (!check(assembler.publish(stale, &snapshot) == PerceptionResult::StaleFrame,
			"older frame is rejected"))
	{
		return false;
	}

	PerceptionInput tooOld(identity(11U));
	astrabot::world::ActorObservation old = visibleActor({4U, 1U});
	old.confidence.ageTicks = 100U;
	tooOld.addActor(old);
	astrabot::perception::PerceptionAssemblerConfig config = {};
	config.maximumObservationAgeTicks = 4U;
	config.maximumMemoryAgeTicks = 8U;
	PerceptionAssembler configured(config);
	if (!check(configured.publish(tooOld, &snapshot) ==
			PerceptionResult::StaleObservation,
			"observation older than configured bound is rejected"))
	{
		return false;
	}

	return true;
}

bool testMemoryExpiryAndGenerationInvalidation()
{
	using astrabot::perception::PerceptionAssembler;
	using astrabot::perception::PerceptionInput;
	using astrabot::perception::PerceptionResult;
	using astrabot::world::MemoryLookupResult;
	using astrabot::world::MemoryState;
	using astrabot::world::WorldSnapshot;

	astrabot::perception::PerceptionAssemblerConfig config = {};
	config.maximumObservationAgeTicks = 8U;
	config.maximumMemoryAgeTicks = 2U;
	PerceptionAssembler assembler(config);
	WorldSnapshot snapshot;

	PerceptionInput visible(identity(20U));
	visible.addActor(visibleActor({2U, 7U}));
	if (!check(assembler.publish(visible, &snapshot) == PerceptionResult::Published,
			"memory source frame is published"))
	{
		return false;
	}

	PerceptionInput next = emptyInput(21U);
	if (!check(assembler.publish(next, &snapshot) == PerceptionResult::Published,
			"frame without contact is published"))
	{
		return false;
	}

	const astrabot::world::MemorySample *memory = nullptr;
	if (!check(snapshot.memoryFor({2U, 7U}, &memory) == MemoryLookupResult::Found &&
			memory != nullptr && memory->state == MemoryState::Remembered &&
			memory->confidence.ageTicks == 1U,
			"remembered contact ages independently from current visibility"))
	{
		return false;
	}

	if (!check(assembler.publish(emptyInput(22U), &snapshot) ==
			PerceptionResult::Published &&
			assembler.publish(emptyInput(23U), &snapshot) ==
			PerceptionResult::Published,
			"memory expiry frames are published"))
	{
		return false;
	}

	if (!check(snapshot.memoryFor({2U, 7U}, &memory) == MemoryLookupResult::Expired &&
			memory != nullptr && memory->state == MemoryState::Expired &&
			!memory->isUsable(),
			"expired memory is explicit and unusable"))
	{
		return false;
	}

	PerceptionInput replacement(identity(24U));
	replacement.addActor(visibleActor({2U, 8U}));
	if (!check(assembler.publish(replacement, &snapshot) ==
			PerceptionResult::Published,
			"new actor generation is published"))
	{
		return false;
	}

	if (!check(snapshot.memoryFor({2U, 7U}, &memory) == MemoryLookupResult::NotFound,
			"old actor generation memory is invalidated"))
	{
		return false;
	}

	if (!check(snapshot.memoryFor({2U, 8U}, &memory) == MemoryLookupResult::Found &&
			memory != nullptr && memory->actor.generation == 8U,
			"replacement actor receives generation-scoped memory"))
	{
		return false;
	}

	return check(assembler.invalidateActor({2U, 8U}) == PerceptionResult::Invalidated,
		"actor-specific invalidation succeeds");
}

bool testInputLimitsAndInvalidIdentity()
{
	using astrabot::perception::PerceptionInput;
	using astrabot::perception::PerceptionInputResult;
	using astrabot::world::AudibleEvent;
	using astrabot::world::SoundKind;

	PerceptionInput input(identity(30U));
	AudibleEvent sound = {};
	sound.kind = SoundKind::Footstep;
	sound.origin = {};
	sound.loudness = 0.5f;
	sound.confidence = {0.5f, 0U};
	for (std::uint32_t id = 1U;
			id <= astrabot::world::WorldLimits::kMaximumSounds;
			++id)
	{
		sound.id = id;
		if (!check(input.addSound(sound) == PerceptionInputResult::Accepted,
				"sound remains within fixed collection bound"))
		{
			return false;
		}
	}

	sound.id = 1000U;
	if (!check(input.addSound(sound) == PerceptionInputResult::ResourceLimit,
			"sound over-limit input is rejected"))
	{
		return false;
	}

	astrabot::world::SnapshotIdentity invalid = identity(31U);
	invalid.observer.generation = 0U;
	return check(PerceptionInput(invalid).setIdentity(invalid) ==
			PerceptionInputResult::InvalidIdentity,
		"invalid observer generation is rejected");
}

bool testP06VisionBeliefLossAndExpiry()
{
	using namespace astrabot;
	perception::PerceptionAssemblerConfig config = {};
	config.maximumObservationAgeTicks = 8U;
	config.maximumMemoryAgeTicks = 5U;
	config.maximumNoiseAgeTicks = 8U;
	config.noiseReactionDelayTicks = 0U;
	perception::PerceptionAssembler assembler(config);
	world::WorldSnapshot snapshot;
	const world::ActorKey observer = {1U, 11U};
	const world::ActorKey enemy = {2U, 7U};

	world::ActorObservation self = visibleActor(observer);
	self.position = {};
	self.relation = world::TeamRelation::Friendly;
	self.visible = true;
	self.fovPassed = true;
	self.losPassed = true;
	world::ActorObservation seen = visibleActor(enemy);
	seen.position = {128.0f, 0.0f, 0.0f};
	seen.visible = true;
	seen.fovPassed = true;
	seen.losPassed = true;
	seen.visibleParts = static_cast<std::uint8_t>(
		world::VisibleChest | world::VisibleHead);
	perception::PerceptionInput first(identity(10U));
	first.addActor(self);
	first.addActor(seen);
	if (!check(assembler.publish(first, &snapshot) == perception::PerceptionResult::Published,
			"visible enemy publication succeeds"))
		return false;
	const world::ActorObservation *published = nullptr;
	if (!check(snapshot.findActor(enemy, &published) == world::ContactLookupResult::Found &&
			published != nullptr && published->visible &&
			published->visibleParts == (world::VisibleChest | world::VisibleHead),
			"vision preserves visible body regions"))
		return false;
	const world::MemorySample *memory = nullptr;
	if (!check(snapshot.memoryFor(enemy, &memory) == world::MemoryLookupResult::Found &&
			memory != nullptr && memory->knowledge == world::KnowledgeState::Observed &&
			memory->position.x == 128.0f,
			"visible contact becomes observed last-known knowledge"))
		return false;

	world::ActorObservation absent = seen;
	absent.state = world::ObservationState::ObservedAbsent;
	absent.position = {};
	absent.velocity = {};
	absent.confidence = {0.0f, 0U};
	absent.visible = false;
	absent.fovPassed = false;
	absent.losPassed = false;
	absent.visibleParts = world::VisibleNone;
	perception::PerceptionInput lost(identity(11U));
	lost.addActor(self);
	lost.addActor(absent);
	if (!check(assembler.publish(lost, &snapshot) == perception::PerceptionResult::Published,
			"lost enemy publication succeeds"))
		return false;
	if (!check(snapshot.findActor(enemy, &published) == world::ContactLookupResult::Found &&
			published != nullptr && !published->visible && !published->hasCurrentPosition(),
			"occluded contact does not expose ground-truth position"))
		return false;
	if (!check(snapshot.memoryFor(enemy, &memory) == world::MemoryLookupResult::Found &&
			memory != nullptr && memory->isUsable() && memory->position.x == 128.0f,
			"last-known position survives visibility loss"))
		return false;

	perception::PerceptionInput expired(identity(16U));
	expired.addActor(self);
	return check(assembler.publish(expired, &snapshot) == perception::PerceptionResult::Published &&
		snapshot.memoryFor(enemy, &memory) == world::MemoryLookupResult::Expired &&
		memory != nullptr && !memory->isUsable(),
		"last-known knowledge expires at the configured deterministic age");
}

bool testP06NoisePriorityExpiryAndHeardNotSeen()
{
	using namespace astrabot;
	perception::PerceptionAssemblerConfig config = {};
	config.maximumObservationAgeTicks = 8U;
	config.maximumMemoryAgeTicks = 32U;
	config.maximumNoiseAgeTicks = 3U;
	config.noiseReactionDelayTicks = 1U;
	perception::PerceptionAssembler assembler(config);
	world::WorldSnapshot snapshot;
	world::ActorObservation self = visibleActor({1U, 11U});
	self.position = {};
	self.relation = world::TeamRelation::Friendly;
	self.visible = true;

	world::AudibleEvent high = {};
	high.id = 1U;
	high.kind = world::SoundKind::WeaponFire;
	high.origin = {256.0f, 0.0f, 0.0f};
	high.loudness = 1.0f;
	high.priority = world::NoisePriority::High;
	high.distance = 100.0f;
	high.timestampTick = 10U;
	high.confidence = {1.0f, 0U};
	perception::PerceptionInput input(identity(10U));
	input.addActor(self);
	input.addSound(high);
	if (!check(assembler.publish(input, &snapshot) == perception::PerceptionResult::Published,
			"noise publication succeeds"))
		return false;
	if (!check(snapshot.noise().active && !snapshot.noise().ready &&
			snapshot.noise().source.slot == 0U &&
			snapshot.noise().kind == world::SoundKind::WeaponFire,
			"heard-but-not-seen exposes category and position without identity"))
		return false;

	world::AudibleEvent weak = high;
	weak.id = 2U;
	weak.priority = world::NoisePriority::Low;
	weak.distance = 10.0f;
	perception::PerceptionInput next(identity(11U));
	next.addActor(self);
	next.addSound(weak);
	if (!check(assembler.publish(next, &snapshot) == perception::PerceptionResult::Published &&
			snapshot.noise().distance == 100.0f &&
			snapshot.noise().priority == world::NoisePriority::High,
			"lower-priority noise does not replace recent high-priority noise"))
		return false;

	world::AudibleEvent samePriorityFar = high;
	samePriorityFar.id = 3U;
	samePriorityFar.distance = 200.0f;
	perception::PerceptionInput farther(identity(12U));
	farther.addActor(self);
	farther.addSound(samePriorityFar);
	if (!check(assembler.publish(farther, &snapshot) == perception::PerceptionResult::Published &&
		snapshot.noise().distance == 100.0f,
		"equally prioritized farther noise does not replace the current noise"))
		return false;

	world::AudibleEvent samePriorityNear = high;
	samePriorityNear.id = 4U;
	samePriorityNear.distance = 50.0f;
	samePriorityNear.timestampTick = 13U;
	perception::PerceptionInput nearer(identity(13U));
	nearer.addActor(self);
	nearer.addSound(samePriorityNear);
	if (!check(assembler.publish(nearer, &snapshot) == perception::PerceptionResult::Published &&
		snapshot.noise().distance == 50.0f,
		"equally prioritized nearer noise replaces the current noise"))
		return false;

	perception::PerceptionInput ready(identity(14U));
	ready.addActor(self);
	if (!check(assembler.publish(ready, &snapshot) == perception::PerceptionResult::Published &&
		snapshot.noise().ready,
		"noise becomes usable after the reaction delay"))
		return false;
	perception::PerceptionInput stale(identity(17U));
	stale.addActor(self);
	return check(assembler.publish(stale, &snapshot) == perception::PerceptionResult::Published &&
		!snapshot.noise().active,
		"noise expires independently from enemy last-known memory");
}

bool testP06EventsIsolationAndTrace()
{
	using namespace astrabot;
	struct Trace : perception::IPerceptionTraceSink
	{
		std::vector<perception::PerceptionTraceRecord> records;
		void record(const perception::PerceptionTraceRecord &record) override
		{
			records.push_back(record);
		}
	};
	perception::PerceptionAssembler assembler;
	Trace trace;
	assembler.setTraceSink(&trace);
	world::WorldSnapshot snapshot;
	const world::ActorKey observer = {1U, 11U};
	const world::ActorKey enemy = {2U, 7U};
	world::ActorObservation self = visibleActor(observer);
	self.relation = world::TeamRelation::Friendly;
	self.visible = true;
	world::ActorObservation seen = visibleActor(enemy);
	seen.visible = true;
	perception::PerceptionInput seenInput(identity(20U));
	seenInput.addActor(self);
	seenInput.addActor(seen);
	if (!check(assembler.publish(seenInput, &snapshot) == perception::PerceptionResult::Published,
			"event fixture source publication succeeds"))
		return false;

	perception::PerceptionInput death(identity(21U));
	death.addActor(self);
	death.addEvent({1U, world::PerceptionEventType::PlayerDeath, observer, enemy});
	if (!check(assembler.publish(death, &snapshot) == perception::PerceptionResult::Published,
			"death event publication succeeds"))
		return false;
	const world::MemorySample *memory = nullptr;
	if (!check(snapshot.memoryFor(enemy, &memory) == world::MemoryLookupResult::Found &&
			memory != nullptr && memory->knowledge == world::KnowledgeState::Believed,
			"death event downgrades current belief without inventing a new position"))
		return false;

	perception::PerceptionInput roundEnd(identity(22U));
	roundEnd.addActor(self);
	roundEnd.addEvent({2U, world::PerceptionEventType::RoundEnd, observer, {}});
	if (!check(assembler.publish(roundEnd, &snapshot) == perception::PerceptionResult::Published &&
		snapshot.memoryFor(enemy, &memory) == world::MemoryLookupResult::NotFound,
		"round-end event clears prior-life perception memory"))
		return false;

	perception::PerceptionAssembler other;
	world::WorldSnapshot otherSnapshot;
	perception::PerceptionInput isolated(identity(20U));
	isolated.addActor(self);
	if (!check(other.publish(isolated, &otherSnapshot) == perception::PerceptionResult::Published &&
		otherSnapshot.memoryFor(enemy, &memory) == world::MemoryLookupResult::NotFound,
		"per-Bot belief isolation is preserved"))
		return false;
	return check(!trace.records.empty(), "perception trace remains bounded and observable");
}

int main()
{
	if (!testDuplicateAndStaleFramesPreservePublication() ||
			!testMemoryExpiryAndGenerationInvalidation() ||
			!testInputLimitsAndInvalidIdentity() ||
			!testP06VisionBeliefLossAndExpiry() ||
			!testP06NoisePriorityExpiryAndHeardNotSeen() ||
			!testP06EventsIsolationAndTrace())
	{
		return 1;
	}

	return 0;
}
