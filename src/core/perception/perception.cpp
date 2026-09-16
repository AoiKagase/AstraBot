#include "astrabot/perception/perception.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace astrabot
{
namespace perception
{
namespace
{
bool isValidObservationState(world::ObservationState state)
{
	return state == world::ObservationState::Unknown ||
		state == world::ObservationState::ObservedAbsent ||
		state == world::ObservationState::ObservedPresent;
}

bool isValidTeamRelation(world::TeamRelation relation)
{
	return relation == world::TeamRelation::Unknown ||
		relation == world::TeamRelation::Friendly ||
		relation == world::TeamRelation::Hostile ||
		relation == world::TeamRelation::Neutral;
}

bool isValidEntityKind(world::EntityKind kind)
{
	return kind == world::EntityKind::Unknown ||
		kind == world::EntityKind::Objective ||
		kind == world::EntityKind::Weapon ||
		kind == world::EntityKind::Projectile ||
		kind == world::EntityKind::Other;
}

bool isValidSoundKind(world::SoundKind kind)
{
	return kind == world::SoundKind::Unknown ||
		kind == world::SoundKind::Footstep ||
		kind == world::SoundKind::WeaponFire ||
		kind == world::SoundKind::Grenade ||
		kind == world::SoundKind::Objective ||
		kind == world::SoundKind::Other;
}
}

PerceptionInput::PerceptionInput()
	: identity_(),
	  actors_(),
	  actorCount_(0U),
	  entities_(),
	  entityCount_(0U),
	  sounds_(),
	  soundCount_(0U)
{
}

PerceptionInput::PerceptionInput(const world::SnapshotIdentity &identity)
	: identity_(),
	  actors_(),
	  actorCount_(0U),
	  entities_(),
	  entityCount_(0U),
	  sounds_(),
	  soundCount_(0U)
{
	setIdentity(identity);
}

PerceptionInputResult PerceptionInput::setIdentity(
	const world::SnapshotIdentity &identity)
{
	if (!identity.isValid())
	{
		return PerceptionInputResult::InvalidIdentity;
	}

	identity_ = identity;
	actorCount_ = 0U;
	entityCount_ = 0U;
	soundCount_ = 0U;
	return PerceptionInputResult::Accepted;
}

const world::SnapshotIdentity &PerceptionInput::identity() const
{
	return identity_;
}

PerceptionInputResult PerceptionInput::addActor(
	const world::ActorObservation &observation)
{
	if (!observation.actor.isValid() ||
			!isValidObservationState(observation.state) ||
			!isValidTeamRelation(observation.relation) ||
			!PerceptionAssembler::isValidActorObservation(observation))
	{
		return PerceptionInputResult::InvalidObservation;
	}

	for (std::size_t index = 0U; index < actorCount_; ++index)
	{
		if (actors_[index].actor.slot == observation.actor.slot)
		{
			return PerceptionInputResult::DuplicateActor;
		}
	}

	if (actorCount_ >= actors_.size())
	{
		return PerceptionInputResult::ResourceLimit;
	}

	actors_[actorCount_] = observation;
	++actorCount_;
	return PerceptionInputResult::Accepted;
}

PerceptionInputResult PerceptionInput::addEntity(
	const world::EntityObservation &observation)
{
	if (!observation.entity.isValid() ||
			!isValidObservationState(observation.state) ||
			!isValidEntityKind(observation.kind) ||
			!PerceptionAssembler::isValidEntityObservation(observation))
	{
		return PerceptionInputResult::InvalidObservation;
	}

	for (std::size_t index = 0U; index < entityCount_; ++index)
	{
		if (entities_[index].entity.id == observation.entity.id)
		{
			return PerceptionInputResult::DuplicateEntity;
		}
	}

	if (entityCount_ >= entities_.size())
	{
		return PerceptionInputResult::ResourceLimit;
	}

	entities_[entityCount_] = observation;
	++entityCount_;
	return PerceptionInputResult::Accepted;
}

PerceptionInputResult PerceptionInput::addSound(
	const world::AudibleEvent &sound)
{
	if (!PerceptionAssembler::isValidSound(sound))
	{
		return PerceptionInputResult::InvalidObservation;
	}

	for (std::size_t index = 0U; index < soundCount_; ++index)
	{
		if (sounds_[index].id == sound.id)
		{
			return PerceptionInputResult::DuplicateSound;
		}
	}

	if (soundCount_ >= sounds_.size())
	{
		return PerceptionInputResult::ResourceLimit;
	}

	sounds_[soundCount_] = sound;
	++soundCount_;
	return PerceptionInputResult::Accepted;
}

std::size_t PerceptionInput::actorCount() const
{
	return actorCount_;
}

const world::ActorObservation *PerceptionInput::actorAt(std::size_t index) const
{
	return index < actorCount_ ? &actors_[index] : nullptr;
}

std::size_t PerceptionInput::entityCount() const
{
	return entityCount_;
}

const world::EntityObservation *PerceptionInput::entityAt(std::size_t index) const
{
	return index < entityCount_ ? &entities_[index] : nullptr;
}

std::size_t PerceptionInput::soundCount() const
{
	return soundCount_;
}

const world::AudibleEvent *PerceptionInput::soundAt(std::size_t index) const
{
	return index < soundCount_ ? &sounds_[index] : nullptr;
}

PerceptionAssemblerConfig::PerceptionAssemblerConfig()
	: maximumObservationAgeTicks(64U),
	  maximumMemoryAgeTicks(256U)
{
}

bool PerceptionAssemblerConfig::isValid() const
{
	return maximumObservationAgeTicks <= world::WorldLimits::kMaximumAgeTicks &&
		maximumMemoryAgeTicks <= world::WorldLimits::kMaximumAgeTicks;
}

PerceptionAssembler::PerceptionAssembler()
	: config_(),
	  memory_(),
	  snapshot_(),
	  lastIdentity_(),
	  hasPublished_(false)
{
	for (MemoryRecord &record : memory_)
	{
		clearMemory(&record);
	}
}

PerceptionAssembler::PerceptionAssembler(
	const PerceptionAssemblerConfig &config)
	: config_(config),
	  memory_(),
	  snapshot_(),
	  lastIdentity_(),
	  hasPublished_(false)
{
	for (MemoryRecord &record : memory_)
	{
		clearMemory(&record);
	}
}

bool PerceptionAssembler::isFiniteVector(const world::WorldVector &value)
{
	return std::isfinite(value.x) &&
		std::isfinite(value.y) &&
		std::isfinite(value.z);
}

bool PerceptionAssembler::isBoundedVector(
	const world::WorldVector &value,
	float limit)
{
	return isFiniteVector(value) &&
		std::fabs(value.x) <= limit &&
		std::fabs(value.y) <= limit &&
		std::fabs(value.z) <= limit;
}

bool PerceptionAssembler::isValidObservationState(
	world::ObservationState state)
{
	return ::astrabot::perception::isValidObservationState(state);
}

bool PerceptionAssembler::isValidTeamRelation(world::TeamRelation relation)
{
	return ::astrabot::perception::isValidTeamRelation(relation);
}

bool PerceptionAssembler::isValidEntityKind(world::EntityKind kind)
{
	return ::astrabot::perception::isValidEntityKind(kind);
}

bool PerceptionAssembler::isValidSoundKind(world::SoundKind kind)
{
	return ::astrabot::perception::isValidSoundKind(kind);
}

bool PerceptionAssembler::isValidActorObservation(
	const world::ActorObservation &observation)
{
	return isFiniteVector(observation.position) &&
		isBoundedVector(observation.position,
			world::WorldLimits::kMaximumCoordinate) &&
		isFiniteVector(observation.velocity) &&
		isBoundedVector(observation.velocity,
			world::WorldLimits::kMaximumVelocity) &&
		std::isfinite(observation.viewPitch) &&
		std::fabs(observation.viewPitch) <= world::WorldLimits::kMaximumAngle &&
		std::isfinite(observation.viewYaw) &&
		std::fabs(observation.viewYaw) <= world::WorldLimits::kMaximumAngle &&
		std::isfinite(observation.confidence.value) &&
		observation.confidence.ageTicks <= world::WorldLimits::kMaximumAgeTicks;
}

bool PerceptionAssembler::isValidEntityObservation(
	const world::EntityObservation &observation)
{
	return isBoundedVector(observation.position,
			world::WorldLimits::kMaximumCoordinate) &&
		isBoundedVector(observation.velocity,
			world::WorldLimits::kMaximumVelocity) &&
		std::isfinite(observation.confidence.value) &&
		observation.confidence.ageTicks <= world::WorldLimits::kMaximumAgeTicks;
}

bool PerceptionAssembler::isValidSound(const world::AudibleEvent &sound)
{
	return sound.id != 0U && isBoundedVector(sound.origin,
			world::WorldLimits::kMaximumCoordinate) &&
		std::isfinite(sound.loudness) && sound.loudness >= 0.0f &&
		sound.loudness <= 1.0f &&
		std::isfinite(sound.confidence.value) &&
		sound.confidence.ageTicks <= world::WorldLimits::kMaximumAgeTicks &&
		isValidSoundKind(sound.kind);
}

float PerceptionAssembler::normalizeConfidence(float value)
{
	return std::max(0.0f, std::min(1.0f, value));
}

std::size_t PerceptionAssembler::actorIndex(const world::ActorKey &actor)
{
	return static_cast<std::size_t>(actor.slot - 1U);
}

bool PerceptionAssembler::sameFrame(
	const world::FrameIdentity &left,
	const world::FrameIdentity &right)
{
	return left == right;
}

bool PerceptionAssembler::isFrameAfter(
	const world::FrameIdentity &candidate,
	const world::FrameIdentity &current)
{
	if (candidate.mapGeneration != current.mapGeneration)
	{
		return candidate.mapGeneration > current.mapGeneration;
	}

	if (candidate.roundGeneration != current.roundGeneration)
	{
		return candidate.roundGeneration > current.roundGeneration;
	}

	return candidate.tick > current.tick;
}

void PerceptionAssembler::clearMemory(MemoryRecord *record)
{
	if (record == nullptr)
	{
		return;
	}

	record->active = false;
	record->actor = {};
	record->state = world::MemoryState::None;
	record->position = {};
	record->confidence = {};
	record->lastFrame = {};
}

void PerceptionAssembler::expireMemory(MemoryRecord *record)
{
	if (record == nullptr || !record->active)
	{
		return;
	}

	record->state = world::MemoryState::Expired;
	record->position = {};
	record->confidence.value = 0.0f;
}

world::MemorySample PerceptionAssembler::sampleFor(const MemoryRecord &record)
{
	world::MemorySample sample = {};
	sample.state = record.state;
	sample.actor = record.actor;
	sample.position = record.position;
	sample.confidence = record.confidence;
	if (record.state == world::MemoryState::Expired)
	{
		sample.position = {};
		sample.confidence.value = 0.0f;
	}
	return sample;
}

PerceptionResult PerceptionAssembler::validateInput(
	const PerceptionInput &input) const
{
	if (!config_.isValid())
	{
		return PerceptionResult::InvalidConfig;
	}

	if (!input.identity().isValid())
	{
		return PerceptionResult::InvalidIdentity;
	}

	for (std::size_t index = 0U; index < input.actorCount(); ++index)
	{
		const world::ActorObservation *observation = input.actorAt(index);
		if (observation == nullptr || !observation->actor.isValid() ||
				!isValidObservationState(observation->state) ||
				!isValidTeamRelation(observation->relation) ||
				!isValidActorObservation(*observation))
		{
			return PerceptionResult::InvalidObservation;
		}
		if (observation->confidence.ageTicks >
				config_.maximumObservationAgeTicks)
		{
			return PerceptionResult::StaleObservation;
		}
	}

	for (std::size_t index = 0U; index < input.entityCount(); ++index)
	{
		const world::EntityObservation *observation = input.entityAt(index);
		if (observation == nullptr || !observation->entity.isValid() ||
				!isValidObservationState(observation->state) ||
				!isValidEntityKind(observation->kind) ||
				!isValidEntityObservation(*observation))
		{
			return PerceptionResult::InvalidObservation;
		}
		if (observation->confidence.ageTicks >
				config_.maximumObservationAgeTicks)
		{
			return PerceptionResult::StaleObservation;
		}
	}

	for (std::size_t index = 0U; index < input.soundCount(); ++index)
	{
		const world::AudibleEvent *sound = input.soundAt(index);
		if (sound == nullptr || !isValidSound(*sound))
		{
			return PerceptionResult::InvalidObservation;
		}
		if (sound->confidence.ageTicks > config_.maximumObservationAgeTicks)
		{
			return PerceptionResult::StaleObservation;
		}
	}

	return PerceptionResult::Published;
}

void PerceptionAssembler::advanceMemory(
	std::array<MemoryRecord, world::WorldLimits::kMaximumActors> *memory,
	const world::FrameIdentity &frame) const
{
	if (memory == nullptr)
	{
		return;
	}

	for (MemoryRecord &record : *memory)
	{
		if (!record.active || record.state != world::MemoryState::Remembered)
		{
			continue;
		}

		const std::uint32_t elapsed = frame.tick >= record.lastFrame.tick ?
			frame.tick - record.lastFrame.tick :
			std::numeric_limits<std::uint32_t>::max();
		const std::uint64_t age =
			static_cast<std::uint64_t>(record.confidence.ageTicks) + elapsed;
		if (age > config_.maximumMemoryAgeTicks)
		{
			record.confidence.ageTicks = world::WorldLimits::kMaximumAgeTicks;
			expireMemory(&record);
			continue;
		}

		record.confidence.ageTicks = static_cast<std::uint32_t>(age);
	}
}

void PerceptionAssembler::updateMemory(
	std::array<MemoryRecord, world::WorldLimits::kMaximumActors> *memory,
	const world::ActorObservation &observation,
	const world::FrameIdentity &frame) const
{
	if (memory == nullptr || !observation.actor.isValid())
	{
		return;
	}

	MemoryRecord &record = (*memory)[actorIndex(observation.actor)];
	if (record.active && !(record.actor == observation.actor))
	{
		clearMemory(&record);
	}

	if (observation.state != world::ObservationState::ObservedPresent)
	{
		return;
	}

	record.active = true;
	record.actor = observation.actor;
	record.state = world::MemoryState::Remembered;
	record.position = observation.position;
	record.confidence.value = normalizeConfidence(observation.confidence.value);
	record.confidence.ageTicks = observation.confidence.ageTicks;
	record.lastFrame = frame;
}

PerceptionResult PerceptionAssembler::publish(
	const PerceptionInput &input,
	world::WorldSnapshot *snapshot)
{
	if (snapshot == nullptr)
	{
		return PerceptionResult::InvalidArgument;
	}

	const PerceptionResult validation = validateInput(input);
	if (validation != PerceptionResult::Published)
	{
		return validation;
	}

	if (hasPublished_ &&
			!(input.identity().observer == lastIdentity_.observer))
	{
		return PerceptionResult::InvalidIdentity;
	}

	if (hasPublished_ && sameFrame(
			input.identity().frame,
			lastIdentity_.frame))
	{
		return PerceptionResult::DuplicateFrame;
	}

	if (hasPublished_ && !isFrameAfter(
			input.identity().frame,
			lastIdentity_.frame))
	{
		return PerceptionResult::StaleFrame;
	}

	std::array<MemoryRecord, world::WorldLimits::kMaximumActors>
		candidateMemory = memory_;
	if (hasPublished_ &&
			(input.identity().frame.mapGeneration !=
				lastIdentity_.frame.mapGeneration ||
			 input.identity().frame.roundGeneration !=
				lastIdentity_.frame.roundGeneration))
	{
		for (MemoryRecord &record : candidateMemory)
		{
			clearMemory(&record);
		}
	}
	else
	{
		advanceMemory(&candidateMemory, input.identity().frame);
	}

	world::WorldSnapshot candidate;
	candidate.identity_ = input.identity();
	candidate.actorCount_ = input.actorCount();
	candidate.entityCount_ = input.entityCount();
	candidate.soundCount_ = input.soundCount();

	for (std::size_t index = 0U; index < input.actorCount(); ++index)
	{
		candidate.actors_[index] = *input.actorAt(index);
		candidate.actors_[index].confidence.value = normalizeConfidence(
			candidate.actors_[index].confidence.value);
		candidate.actors_[index].memory = {};
		if (candidate.actors_[index].state !=
				world::ObservationState::ObservedPresent)
		{
			candidate.actors_[index].position = {};
			candidate.actors_[index].velocity = {};
			candidate.actors_[index].viewPitch = 0.0f;
			candidate.actors_[index].viewYaw = 0.0f;
			candidate.actors_[index].relation = world::TeamRelation::Unknown;
			candidate.actors_[index].confidence = {};
		}
		updateMemory(
			&candidateMemory,
			candidate.actors_[index],
			input.identity().frame);
		const MemoryRecord &record = candidateMemory[actorIndex(
			candidate.actors_[index].actor)];
		if (record.active && record.actor == candidate.actors_[index].actor)
		{
			candidate.actors_[index].memory = sampleFor(record);
		}
	}

	for (std::size_t index = 0U; index < input.entityCount(); ++index)
	{
		candidate.entities_[index] = *input.entityAt(index);
		candidate.entities_[index].confidence.value = normalizeConfidence(
			candidate.entities_[index].confidence.value);
		if (candidate.entities_[index].state !=
				world::ObservationState::ObservedPresent)
		{
			candidate.entities_[index].position = {};
			candidate.entities_[index].velocity = {};
			candidate.entities_[index].confidence = {};
		}
	}

	for (std::size_t index = 0U; index < input.soundCount(); ++index)
	{
		candidate.sounds_[index] = *input.soundAt(index);
		candidate.sounds_[index].confidence.value = normalizeConfidence(
			candidate.sounds_[index].confidence.value);
	}

	for (const MemoryRecord &record : candidateMemory)
	{
		if (!record.active)
		{
			continue;
		}
		candidate.memories_[candidate.memoryCount_] = sampleFor(record);
		++candidate.memoryCount_;
	}

	memory_ = candidateMemory;
	snapshot_ = candidate;
	lastIdentity_ = input.identity();
	hasPublished_ = true;
	*snapshot = snapshot_;
	return PerceptionResult::Published;
}

PerceptionResult PerceptionAssembler::invalidateActor(const world::ActorKey &actor)
{
	if (!actor.isValid())
	{
		return PerceptionResult::InvalidArgument;
	}

	MemoryRecord &record = memory_[actorIndex(actor)];
	if (!record.active || !(record.actor == actor))
	{
		return PerceptionResult::NotFound;
	}

	clearMemory(&record);
	return PerceptionResult::Invalidated;
}

const PerceptionAssemblerConfig &PerceptionAssembler::config() const
{
	return config_;
}

const world::WorldSnapshot &PerceptionAssembler::snapshot() const
{
	return snapshot_;
}
}
}
