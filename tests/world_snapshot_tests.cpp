#include "astrabot/perception/perception.hpp"

#include <cmath>
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
		value.navRevision = 19U;
		return value;
	}

	astrabot::world::ActorObservation visibleActor(
		astrabot::world::ActorKey actor)
	{
		astrabot::world::ActorObservation observation = {};
		observation.actor = actor;
		observation.state = astrabot::world::ObservationState::ObservedPresent;
		observation.relation = astrabot::world::TeamRelation::Hostile;
		observation.position = {128.0f, -64.0f, 32.0f};
		observation.velocity = {10.0f, 20.0f, 0.0f};
		observation.viewPitch = 5.0f;
		observation.viewYaw = 90.0f;
		observation.confidence = {1.5f, 0U};
		return observation;
	}
}

bool testVisibleAndUnknownContacts()
{
	using astrabot::perception::PerceptionAssembler;
	using astrabot::perception::PerceptionInput;
	using astrabot::perception::PerceptionResult;
	using astrabot::world::ActorKey;
	using astrabot::world::ContactLookupResult;
	using astrabot::world::ObservationState;
	using astrabot::world::WorldSnapshot;

	PerceptionInput input(identity(100U));
	if (!check(input.addActor(visibleActor({2U, 4U})) ==
			astrabot::perception::PerceptionInputResult::Accepted,
			"visible actor input is accepted"))
	{
		return false;
	}

	astrabot::world::ActorObservation unknown = {};
	unknown.actor = {3U, 4U};
	unknown.state = ObservationState::Unknown;
	unknown.relation = astrabot::world::TeamRelation::Hostile;
	unknown.position = {999.0f, 999.0f, 999.0f};
	unknown.confidence = {1.0f, 0U};
	if (!check(input.addActor(unknown) ==
			astrabot::perception::PerceptionInputResult::Accepted,
			"explicit unknown actor input is accepted"))
	{
		return false;
	}

	PerceptionAssembler assembler;
	WorldSnapshot snapshot;
	if (!check(assembler.publish(input, &snapshot) == PerceptionResult::Published,
			"snapshot is published"))
	{
		return false;
	}

	if (!check(snapshot.isValid() && snapshot.identity().frame.tick == 100U &&
			snapshot.identity().observer.slot == 1U &&
			snapshot.identity().navRevision == 19U,
			"snapshot keeps frame and observer identity"))
	{
		return false;
	}

	const astrabot::world::ActorObservation *observation = nullptr;
	if (!check(snapshot.findActor({2U, 4U}, &observation) ==
			ContactLookupResult::Found && observation != nullptr &&
			observation->isConfirmed() && observation->confidence.value == 1.0f &&
			observation->position.x == 128.0f,
			"visible actor is bounded and confirmed only from observation"))
	{
		return false;
	}

	if (!check(snapshot.findActor({3U, 4U}, &observation) ==
			ContactLookupResult::Found && observation != nullptr &&
			observation->state == ObservationState::Unknown &&
			!observation->isConfirmed() &&
			observation->position.x == 0.0f &&
			observation->confidence.value == 0.0f,
			"explicit unknown contact cannot carry confirmed hidden position"))
	{
		return false;
	}

	const ActorKey omitted = {4U, 4U};
	return check(snapshot.findActor(omitted, &observation) ==
			ContactLookupResult::Unknown && observation == nullptr,
		"omitted actor remains unknown instead of absent or confirmed");
}

bool testEntitiesSoundsAndFiniteValidation()
{
	using astrabot::perception::PerceptionAssembler;
	using astrabot::perception::PerceptionInput;
	using astrabot::perception::PerceptionInputResult;
	using astrabot::perception::PerceptionResult;
	using astrabot::world::EntityKind;
	using astrabot::world::EntityObservation;
	using astrabot::world::ObservationState;
	using astrabot::world::SoundKind;
	using astrabot::world::WorldSnapshot;

	PerceptionInput input(identity(101U));
	EntityObservation entity = {};
	entity.entity = {44U, 2U};
	entity.kind = EntityKind::Objective;
	entity.state = ObservationState::ObservedPresent;
	entity.position = {16.0f, 32.0f, 48.0f};
	entity.velocity = {};
	entity.confidence = {-0.5f, 1U};
	if (!check(input.addEntity(entity) == PerceptionInputResult::Accepted,
			"bounded entity input is accepted"))
	{
		return false;
	}

	astrabot::world::AudibleEvent sound = {};
	sound.id = 9U;
	sound.kind = SoundKind::WeaponFire;
	sound.origin = {1.0f, 2.0f, 3.0f};
	sound.loudness = 0.75f;
	sound.confidence = {0.6f, 2U};
	if (!check(input.addSound(sound) == PerceptionInputResult::Accepted,
			"bounded sound input is accepted"))
	{
		return false;
	}

	astrabot::world::ActorObservation invalid = visibleActor({5U, 1U});
	invalid.position.x = std::numeric_limits<float>::quiet_NaN();
	if (!check(input.addActor(invalid) ==
			PerceptionInputResult::InvalidObservation,
			"non-finite actor input is rejected"))
	{
		return false;
	}

	PerceptionAssembler assembler;
	WorldSnapshot snapshot;
	if (!check(assembler.publish(input, &snapshot) == PerceptionResult::Published,
			"entity and sound snapshot is published"))
	{
		return false;
	}

	if (!check(snapshot.entityCount() == 1U && snapshot.soundCount() == 1U,
			"snapshot keeps bounded entity and sound collections"))
	{
		return false;
	}

	const EntityObservation *storedEntity = snapshot.entityAt(0U);
	const astrabot::world::AudibleEvent *storedSound = snapshot.soundAt(0U);
	return check(storedEntity != nullptr && storedEntity->confidence.value == 0.0f &&
		storedSound != nullptr && storedSound->confidence.ageTicks == 2U,
		"confidence and age are normalized in snapshot values");
}

int main()
{
	if (!testVisibleAndUnknownContacts() ||
			!testEntitiesSoundsAndFiniteValidation())
	{
		return 1;
	}

	return 0;
}
