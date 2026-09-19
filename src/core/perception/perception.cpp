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
		kind == world::EntityKind::Objective || kind == world::EntityKind::Weapon ||
		kind == world::EntityKind::Projectile || kind == world::EntityKind::Other;
}

bool isValidSoundKind(world::SoundKind kind)
{
	return kind == world::SoundKind::Unknown ||
		kind == world::SoundKind::Footstep || kind == world::SoundKind::WeaponFire ||
		kind == world::SoundKind::Grenade || kind == world::SoundKind::Objective ||
		kind == world::SoundKind::Other;
}

bool isValidEventType(world::PerceptionEventType type)
{
	return type != world::PerceptionEventType::None;
}

std::uint32_t elapsedTicks(
	const world::FrameIdentity &now, const world::FrameIdentity &then)
{
	return now.tick >= then.tick ? now.tick - then.tick :
		(std::numeric_limits<std::uint32_t>::max)();
}
}

PerceptionInput::PerceptionInput()
	: identity_(), actors_(), actorCount_(0U), entities_(), entityCount_(0U),
	  sounds_(), soundCount_(0U), events_(), eventCount_(0U)
{
}

PerceptionInput::PerceptionInput(const world::SnapshotIdentity &identity)
	: PerceptionInput()
{
	setIdentity(identity);
}

PerceptionInputResult PerceptionInput::setIdentity(
	const world::SnapshotIdentity &identity)
{
	if (!identity.isValid())
		return PerceptionInputResult::InvalidIdentity;
	identity_ = identity;
	actorCount_ = 0U;
	entityCount_ = 0U;
	soundCount_ = 0U;
	eventCount_ = 0U;
	return PerceptionInputResult::Accepted;
}

const world::SnapshotIdentity &PerceptionInput::identity() const
{
	return identity_;
}

PerceptionInputResult PerceptionInput::addActor(
	const world::ActorObservation &observation)
{
	if (!observation.actor.isValid() || !isValidObservationState(observation.state) ||
		!isValidTeamRelation(observation.relation) ||
		!PerceptionAssembler::isValidActorObservation(observation))
		return PerceptionInputResult::InvalidObservation;
	for (std::size_t index = 0U; index < actorCount_; ++index)
		if (actors_[index].actor.slot == observation.actor.slot)
			return PerceptionInputResult::DuplicateActor;
	if (actorCount_ >= actors_.size())
		return PerceptionInputResult::ResourceLimit;
	actors_[actorCount_++] = observation;
	return PerceptionInputResult::Accepted;
}

PerceptionInputResult PerceptionInput::addEntity(
	const world::EntityObservation &observation)
{
	if (!observation.entity.isValid() || !isValidObservationState(observation.state) ||
		!isValidEntityKind(observation.kind) ||
		!PerceptionAssembler::isValidEntityObservation(observation))
		return PerceptionInputResult::InvalidObservation;
	for (std::size_t index = 0U; index < entityCount_; ++index)
		if (entities_[index].entity.id == observation.entity.id)
			return PerceptionInputResult::DuplicateEntity;
	if (entityCount_ >= entities_.size())
		return PerceptionInputResult::ResourceLimit;
	entities_[entityCount_++] = observation;
	return PerceptionInputResult::Accepted;
}

PerceptionInputResult PerceptionInput::addSound(
	const world::AudibleEvent &sound)
{
	if (!PerceptionAssembler::isValidSound(sound))
		return PerceptionInputResult::InvalidObservation;
	for (std::size_t index = 0U; index < soundCount_; ++index)
		if (sounds_[index].id == sound.id)
			return PerceptionInputResult::DuplicateSound;
	if (soundCount_ >= sounds_.size())
		return PerceptionInputResult::ResourceLimit;
	sounds_[soundCount_++] = sound;
	return PerceptionInputResult::Accepted;
}

PerceptionInputResult PerceptionInput::addEvent(const PerceptionEvent &event)
{
	if (event.id == 0U || !isValidEventType(event.type))
		return PerceptionInputResult::InvalidObservation;
	for (std::size_t index = 0U; index < eventCount_; ++index)
		if (events_[index].id == event.id)
			return PerceptionInputResult::DuplicateEvent;
	if (eventCount_ >= events_.size())
		return PerceptionInputResult::ResourceLimit;
	events_[eventCount_++] = event;
	return PerceptionInputResult::Accepted;
}

std::size_t PerceptionInput::actorCount() const { return actorCount_; }
const world::ActorObservation *PerceptionInput::actorAt(std::size_t index) const
{
	return index < actorCount_ ? &actors_[index] : nullptr;
}
std::size_t PerceptionInput::entityCount() const { return entityCount_; }
const world::EntityObservation *PerceptionInput::entityAt(std::size_t index) const
{
	return index < entityCount_ ? &entities_[index] : nullptr;
}
std::size_t PerceptionInput::soundCount() const { return soundCount_; }
const world::AudibleEvent *PerceptionInput::soundAt(std::size_t index) const
{
	return index < soundCount_ ? &sounds_[index] : nullptr;
}
std::size_t PerceptionInput::eventCount() const { return eventCount_; }
const PerceptionEvent *PerceptionInput::eventAt(std::size_t index) const
{
	return index < eventCount_ ? &events_[index] : nullptr;
}

PerceptionAssemblerConfig::PerceptionAssemblerConfig()
	: maximumObservationAgeTicks(64U), maximumMemoryAgeTicks(256U),
	  maximumNoiseAgeTicks(600U), noiseReactionDelayTicks(8U),
	  maximumHearingDistance(2000U)
{
}

bool PerceptionAssemblerConfig::isValid() const
{
	return maximumObservationAgeTicks <= world::WorldLimits::kMaximumAgeTicks &&
		maximumMemoryAgeTicks <= world::WorldLimits::kMaximumAgeTicks &&
		maximumNoiseAgeTicks <= world::WorldLimits::kMaximumAgeTicks &&
		noiseReactionDelayTicks <= maximumNoiseAgeTicks &&
		maximumHearingDistance <= static_cast<std::uint32_t>(world::WorldLimits::kMaximumCoordinate);
}

PerceptionAssembler::PerceptionAssembler()
	: config_(), memory_(), snapshot_(), lastIdentity_(), traceSink_(nullptr),
	  traceSequence_(0U), hasPublished_(false)
{
	for (MemoryRecord &record : memory_)
		clearMemory(&record);
}

PerceptionAssembler::PerceptionAssembler(const PerceptionAssemblerConfig &config)
	: config_(config), memory_(), snapshot_(), lastIdentity_(), traceSink_(nullptr),
	  traceSequence_(0U), hasPublished_(false)
{
	for (MemoryRecord &record : memory_)
		clearMemory(&record);
}

bool PerceptionAssembler::isFiniteVector(const world::WorldVector &value)
{
	return std::isfinite(value.x) && std::isfinite(value.y) &&
		std::isfinite(value.z);
}

bool PerceptionAssembler::isBoundedVector(
	const world::WorldVector &value, float limit)
{
	return isFiniteVector(value) && std::fabs(value.x) <= limit &&
		std::fabs(value.y) <= limit && std::fabs(value.z) <= limit;
}

bool PerceptionAssembler::isValidObservationState(world::ObservationState state)
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
		isBoundedVector(observation.position, world::WorldLimits::kMaximumCoordinate) &&
		isFiniteVector(observation.velocity) &&
		isBoundedVector(observation.velocity, world::WorldLimits::kMaximumVelocity) &&
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
	return isBoundedVector(observation.position, world::WorldLimits::kMaximumCoordinate) &&
		isBoundedVector(observation.velocity, world::WorldLimits::kMaximumVelocity) &&
		std::isfinite(observation.confidence.value) &&
		observation.confidence.ageTicks <= world::WorldLimits::kMaximumAgeTicks;
}

bool PerceptionAssembler::isValidSound(const world::AudibleEvent &sound)
{
	return sound.id != 0U && isBoundedVector(sound.origin, world::WorldLimits::kMaximumCoordinate) &&
		std::isfinite(sound.loudness) && sound.loudness >= 0.0f && sound.loudness <= 1.0f &&
		std::isfinite(sound.distance) && sound.distance >= 0.0f &&
		sound.distance <= world::WorldLimits::kMaximumCoordinate &&
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
	const world::FrameIdentity &left, const world::FrameIdentity &right)
{
	return left == right;
}

bool PerceptionAssembler::isFrameAfter(
	const world::FrameIdentity &candidate, const world::FrameIdentity &current)
{
	if (candidate.mapGeneration != current.mapGeneration)
		return candidate.mapGeneration > current.mapGeneration;
	if (candidate.roundGeneration != current.roundGeneration)
		return candidate.roundGeneration > current.roundGeneration;
	return candidate.tick > current.tick;
}

void PerceptionAssembler::clearMemory(MemoryRecord *record)
{
	if (record == nullptr)
		return;
	record->active = false;
	record->actor = {};
	record->state = world::MemoryState::None;
	record->knowledge = world::KnowledgeState::Unknown;
	record->position = {};
	record->confidence = {};
	record->visibleParts = world::VisibleNone;
	record->lastFrame = {};
	record->lastSeenFrame = {};
}

void PerceptionAssembler::expireMemory(MemoryRecord *record)
{
	if (record == nullptr || !record->active)
		return;
	record->state = world::MemoryState::Expired;
	record->knowledge = world::KnowledgeState::Unknown;
	record->position = {};
	record->confidence.value = 0.0f;
	record->visibleParts = world::VisibleNone;
}

world::MemorySample PerceptionAssembler::sampleFor(const MemoryRecord &record)
{
	world::MemorySample sample = {};
	sample.state = record.state;
	sample.knowledge = record.knowledge;
	sample.actor = record.actor;
	sample.position = record.position;
	sample.confidence = record.confidence;
	sample.visibleParts = record.visibleParts;
	sample.lastSeenFrame = record.lastSeenFrame;
	if (record.state == world::MemoryState::Expired)
	{
		sample.knowledge = world::KnowledgeState::Unknown;
		sample.position = {};
		sample.confidence.value = 0.0f;
		sample.visibleParts = world::VisibleNone;
	}
	return sample;
}

void PerceptionAssembler::clearNoise(world::NoiseMemory *noise)
{
	if (noise == nullptr)
		return;
	*noise = {};
	noise->kind = world::SoundKind::Unknown;
	noise->priority = world::NoisePriority::Low;
}

PerceptionResult PerceptionAssembler::validateInput(
	const PerceptionInput &input) const
{
	if (!config_.isValid())
		return PerceptionResult::InvalidConfig;
	if (!input.identity().isValid())
		return PerceptionResult::InvalidIdentity;
	for (std::size_t index = 0U; index < input.actorCount(); ++index)
	{
		const world::ActorObservation *observation = input.actorAt(index);
		if (observation == nullptr || !observation->actor.isValid() ||
			!isValidObservationState(observation->state) ||
			!isValidTeamRelation(observation->relation) ||
			!isValidActorObservation(*observation))
			return PerceptionResult::InvalidObservation;
		if (observation->confidence.ageTicks > config_.maximumObservationAgeTicks)
			return PerceptionResult::StaleObservation;
	}
	for (std::size_t index = 0U; index < input.entityCount(); ++index)
	{
		const world::EntityObservation *observation = input.entityAt(index);
		if (observation == nullptr || !observation->entity.isValid() ||
			!isValidObservationState(observation->state) ||
			!isValidEntityKind(observation->kind) ||
			!isValidEntityObservation(*observation))
			return PerceptionResult::InvalidObservation;
		if (observation->confidence.ageTicks > config_.maximumObservationAgeTicks)
			return PerceptionResult::StaleObservation;
	}
	for (std::size_t index = 0U; index < input.soundCount(); ++index)
	{
		const world::AudibleEvent *sound = input.soundAt(index);
		if (sound == nullptr || !isValidSound(*sound))
			return PerceptionResult::InvalidObservation;
		if (sound->confidence.ageTicks > config_.maximumObservationAgeTicks)
			return PerceptionResult::StaleObservation;
	}
	for (std::size_t index = 0U; index < input.eventCount(); ++index)
	{
		const PerceptionEvent *event = input.eventAt(index);
		if (event == nullptr || event->id == 0U || !isValidEventType(event->type))
			return PerceptionResult::InvalidEvent;
	}
	return PerceptionResult::Published;
}

void PerceptionAssembler::advanceMemory(
	std::array<MemoryRecord, world::WorldLimits::kMaximumActors> *memory,
	const world::FrameIdentity &frame) const
{
	if (memory == nullptr)
		return;
	for (MemoryRecord &record : *memory)
	{
		if (!record.active || record.state != world::MemoryState::Remembered)
			continue;
		const std::uint64_t age = static_cast<std::uint64_t>(record.confidence.ageTicks) +
			elapsedTicks(frame, record.lastFrame);
		if (age > config_.maximumMemoryAgeTicks)
		{
			record.confidence.ageTicks = world::WorldLimits::kMaximumAgeTicks;
			expireMemory(&record);
		}
		else
			record.confidence.ageTicks = static_cast<std::uint32_t>(age);
	}
}

void PerceptionAssembler::updateMemory(
	std::array<MemoryRecord, world::WorldLimits::kMaximumActors> *memory,
	const world::ActorObservation &observation,
	const world::FrameIdentity &frame) const
{
	if (memory == nullptr || !observation.actor.isValid())
		return;
	MemoryRecord &record = (*memory)[actorIndex(observation.actor)];
	if (record.active && !(record.actor == observation.actor))
		clearMemory(&record);
	if (observation.state != world::ObservationState::ObservedPresent)
		return;
	record.active = true;
	record.actor = observation.actor;
	record.state = world::MemoryState::Remembered;
	record.knowledge = world::KnowledgeState::Observed;
	record.position = observation.position;
	record.confidence.value = normalizeConfidence(observation.confidence.value);
	record.confidence.ageTicks = observation.confidence.ageTicks;
	record.visibleParts = observation.visibleParts;
	record.lastFrame = frame;
	record.lastSeenFrame = frame;
}

void PerceptionAssembler::advanceNoise(
	world::NoiseMemory *noise, const world::FrameIdentity &frame) const
{
	if (noise == nullptr || !noise->active)
		return;
	const std::uint32_t age = frame.tick >= noise->timestampTick
		? frame.tick - noise->timestampTick
		: (std::numeric_limits<std::uint32_t>::max)();
	if (age > config_.maximumNoiseAgeTicks)
	{
		clearNoise(noise);
		return;
	}
	noise->ready = age >= config_.noiseReactionDelayTicks;
	noise->confidence.ageTicks = age;
}

void PerceptionAssembler::updateNoise(
	world::NoiseMemory *noise, const world::AudibleEvent &sound,
	const world::ActorKey &observer, const world::FrameIdentity &frame) const
{
	(void)observer;
	if (noise == nullptr || sound.distance > static_cast<float>(config_.maximumHearingDistance))
		return;
	const std::uint32_t soundTick = sound.timestampTick == 0U
		? frame.tick : sound.timestampTick;
	if (noise->active)
	{
		const std::uint32_t age = frame.tick >= noise->timestampTick
			? frame.tick - noise->timestampTick
			: (std::numeric_limits<std::uint32_t>::max)();
		const std::uint32_t shortTermTicks = 90U;
		if (age < shortTermTicks)
		{
			if (static_cast<std::uint8_t>(sound.priority) <
				static_cast<std::uint8_t>(noise->priority))
				return;
			if (sound.priority == noise->priority && sound.distance >= noise->distance)
				return;
		}
	}
	noise->active = true;
	noise->ready = config_.noiseReactionDelayTicks == 0U;
	noise->kind = sound.kind;
	noise->position = sound.origin;
	noise->distance = sound.distance;
	noise->priority = sound.priority;
	noise->timestampTick = soundTick;
	noise->confidence = sound.confidence;
	noise->source = {};
	noise->positionApproximated = true;
}

void PerceptionAssembler::applyEvent(
	std::array<MemoryRecord, world::WorldLimits::kMaximumActors> *memory,
	world::NoiseMemory *noise, const PerceptionEvent &event,
	const world::FrameIdentity &frame) const
{
	if (event.type == world::PerceptionEventType::RoundStart ||
		event.type == world::PerceptionEventType::RoundEnd)
	{
		if (memory != nullptr)
			for (MemoryRecord &record : *memory)
				clearMemory(&record);
		clearNoise(noise);
		return;
	}
	if (memory == nullptr || !event.subject.isValid())
		return;
	MemoryRecord &record = (*memory)[actorIndex(event.subject)];
	if (event.type == world::PerceptionEventType::PlayerDeath)
	{
		if (record.active && record.actor == event.subject)
		{
			record.knowledge = world::KnowledgeState::Believed;
			record.visibleParts = world::VisibleNone;
			record.lastFrame = frame;
		}
	}
	else if (event.type == world::PerceptionEventType::PlayerSpawn ||
		event.type == world::PerceptionEventType::PlayerRespawn)
	{
		clearMemory(&record);
	}
}

void PerceptionAssembler::emit(const PerceptionTraceRecord &record) const
{
	if (traceSink_ == nullptr)
		return;
	PerceptionTraceRecord value = record;
	value.sequence = ++traceSequence_;
	traceSink_->record(value);
}

PerceptionResult PerceptionAssembler::publish(
	const PerceptionInput &input, world::WorldSnapshot *snapshot)
{
	if (snapshot == nullptr)
		return PerceptionResult::InvalidArgument;
	const PerceptionResult validation = validateInput(input);
	if (validation != PerceptionResult::Published)
		return validation;
	if (hasPublished_ && !(input.identity().observer == lastIdentity_.observer))
		return PerceptionResult::InvalidIdentity;
	if (hasPublished_ && sameFrame(input.identity().frame, lastIdentity_.frame))
		return PerceptionResult::DuplicateFrame;
	if (hasPublished_ && !isFrameAfter(input.identity().frame, lastIdentity_.frame))
		return PerceptionResult::StaleFrame;

	std::array<MemoryRecord, world::WorldLimits::kMaximumActors> candidateMemory = memory_;
	world::NoiseMemory candidateNoise = snapshot_.noise();
	if (hasPublished_ &&
		(input.identity().frame.mapGeneration != lastIdentity_.frame.mapGeneration ||
		 input.identity().frame.roundGeneration != lastIdentity_.frame.roundGeneration))
	{
		for (MemoryRecord &record : candidateMemory)
			clearMemory(&record);
		clearNoise(&candidateNoise);
	}
	else
	{
		advanceMemory(&candidateMemory, input.identity().frame);
		advanceNoise(&candidateNoise, input.identity().frame);
	}

	for (std::size_t index = 0U; index < input.eventCount(); ++index)
	{
		const PerceptionEvent *event = input.eventAt(index);
		applyEvent(&candidateMemory, &candidateNoise, *event, input.identity().frame);
		PerceptionTraceRecord trace = {};
		trace.kind = PerceptionTraceKind::Event;
		trace.actor = input.identity().observer;
		trace.frame = input.identity().frame;
		trace.eventType = event->type;
		emit(trace);
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
		if (candidate.actors_[index].state != world::ObservationState::ObservedPresent)
		{
			candidate.actors_[index].position = {};
			candidate.actors_[index].velocity = {};
			candidate.actors_[index].viewPitch = 0.0f;
			candidate.actors_[index].viewYaw = 0.0f;
			candidate.actors_[index].confidence = {};
			candidate.actors_[index].visible = false;
			candidate.actors_[index].fovPassed = false;
			candidate.actors_[index].losPassed = false;
			candidate.actors_[index].visibleParts = world::VisibleNone;
		}
		updateMemory(&candidateMemory, candidate.actors_[index], input.identity().frame);
		const MemoryRecord &record = candidateMemory[actorIndex(candidate.actors_[index].actor)];
		if (record.active && record.actor == candidate.actors_[index].actor)
			candidate.actors_[index].memory = sampleFor(record);

		PerceptionTraceRecord vision = {};
		vision.kind = PerceptionTraceKind::Vision;
		vision.actor = input.identity().observer;
		vision.target = candidate.actors_[index].actor;
		vision.frame = input.identity().frame;
		vision.visible = candidate.actors_[index].visible;
		vision.fovPassed = candidate.actors_[index].fovPassed;
		vision.losPassed = candidate.actors_[index].losPassed;
		vision.visibleParts = candidate.actors_[index].visibleParts;
		emit(vision);

		PerceptionTraceRecord knowledge = {};
		knowledge.kind = PerceptionTraceKind::Knowledge;
		knowledge.actor = input.identity().observer;
		knowledge.target = candidate.actors_[index].actor;
		knowledge.frame = input.identity().frame;
		knowledge.knowledge = candidate.actors_[index].memory.knowledge;
		knowledge.position = candidate.actors_[index].memory.position;
		knowledge.hasPosition = candidate.actors_[index].memory.isUsable();
		emit(knowledge);
	}

	for (std::size_t index = 0U; index < input.entityCount(); ++index)
	{
		candidate.entities_[index] = *input.entityAt(index);
		candidate.entities_[index].confidence.value = normalizeConfidence(
			candidate.entities_[index].confidence.value);
		if (candidate.entities_[index].state != world::ObservationState::ObservedPresent)
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
		if (candidate.sounds_[index].timestampTick == 0U)
			candidate.sounds_[index].timestampTick = input.identity().frame.tick;
		const world::NoiseMemory beforeNoise = candidateNoise;
		updateNoise(&candidateNoise, candidate.sounds_[index], input.identity().observer,
			input.identity().frame);
		PerceptionTraceRecord noise = {};
		noise.kind = PerceptionTraceKind::Noise;
		noise.actor = input.identity().observer;
		noise.frame = input.identity().frame;
		noise.soundKind = candidate.sounds_[index].kind;
		noise.noisePriority = candidate.sounds_[index].priority;
		noise.position = candidate.sounds_[index].origin;
		noise.hasPosition = true;
		noise.heard = candidateNoise.active &&
			(candidateNoise.timestampTick == candidate.sounds_[index].timestampTick);
		noise.rejected = !noise.heard ||
			(beforeNoise.active && candidateNoise.timestampTick == beforeNoise.timestampTick &&
			 candidateNoise.kind == beforeNoise.kind);
		emit(noise);
	}

	candidate.noise_ = candidateNoise;
	candidate.memoryCount_ = 0U;
	for (const MemoryRecord &record : candidateMemory)
	{
		if (!record.active || candidate.memoryCount_ >= candidate.memories_.size())
			continue;
		candidate.memories_[candidate.memoryCount_++] = sampleFor(record);
	}
	*snapshot = candidate;
	snapshot_ = candidate;
	memory_ = candidateMemory;
	lastIdentity_ = input.identity();
	hasPublished_ = true;
	return PerceptionResult::Published;
}

PerceptionResult PerceptionAssembler::invalidateActor(const world::ActorKey &actor)
{
	if (!actor.isValid())
		return PerceptionResult::InvalidArgument;
	MemoryRecord &record = memory_[actorIndex(actor)];
	if (!record.active || !(record.actor == actor))
		return PerceptionResult::NotFound;
	clearMemory(&record);
	snapshot_.memoryCount_ = 0U;
	for (const MemoryRecord &candidate : memory_)
	{
		if (!candidate.active || snapshot_.memoryCount_ >= snapshot_.memories_.size())
			continue;
		snapshot_.memories_[snapshot_.memoryCount_++] = sampleFor(candidate);
	}
	return PerceptionResult::Invalidated;
}

const PerceptionAssemblerConfig &PerceptionAssembler::config() const { return config_; }
const world::WorldSnapshot &PerceptionAssembler::snapshot() const { return snapshot_; }
void PerceptionAssembler::setTraceSink(IPerceptionTraceSink *sink) { traceSink_ = sink; }
}
}
