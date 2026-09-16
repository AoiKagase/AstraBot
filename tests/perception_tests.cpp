#include "astrabot/perception/perception.hpp"

#include <cstdio>
#include <limits>

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

int main()
{
	if (!testDuplicateAndStaleFramesPreservePublication() ||
			!testMemoryExpiryAndGenerationInvalidation() ||
			!testInputLimitsAndInvalidIdentity())
	{
		return 1;
	}

	return 0;
}
