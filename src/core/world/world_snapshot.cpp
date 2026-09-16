#include "astrabot/world/world_snapshot.hpp"

#include <cmath>

namespace astrabot
{
namespace world
{
namespace
{
bool isFiniteVector(const WorldVector &value)
{
	return std::isfinite(value.x) &&
		std::isfinite(value.y) &&
		std::isfinite(value.z);
}

bool isBoundedVector(const WorldVector &value, float limit)
{
	return isFiniteVector(value) &&
		std::fabs(value.x) <= limit &&
		std::fabs(value.y) <= limit &&
		std::fabs(value.z) <= limit;
}
}

bool ActorKey::isValid() const
{
	return slot > 0U && slot <= WorldLimits::kMaximumActors && generation != 0U;
}

bool ActorKey::operator==(const ActorKey &other) const
{
	return slot == other.slot && generation == other.generation;
}

bool EntityKey::isValid() const
{
	return id != 0U && generation != 0U;
}

bool EntityKey::operator==(const EntityKey &other) const
{
	return id == other.id && generation == other.generation;
}

bool FrameIdentity::isValid() const
{
	return mapGeneration != 0U && roundGeneration != 0U;
}

bool FrameIdentity::operator==(const FrameIdentity &other) const
{
	return mapGeneration == other.mapGeneration &&
		roundGeneration == other.roundGeneration &&
		tick == other.tick;
}

bool SnapshotIdentity::isValid() const
{
	return frame.isValid() && observer.isValid();
}

bool SnapshotIdentity::operator==(const SnapshotIdentity &other) const
{
	return frame == other.frame && observer == other.observer &&
		navRevision == other.navRevision;
}

bool ContactConfidence::isValid() const
{
	return std::isfinite(value) && value >= 0.0f && value <= 1.0f &&
		ageTicks <= WorldLimits::kMaximumAgeTicks;
}

bool MemorySample::isUsable() const
{
	return state == MemoryState::Remembered && actor.isValid() &&
		isBoundedVector(position, WorldLimits::kMaximumCoordinate) &&
		confidence.isValid();
}

bool ActorObservation::isConfirmed() const
{
	return state == ObservationState::ObservedPresent &&
		confidence.isValid() && confidence.value > 0.0f &&
		isBoundedVector(position, WorldLimits::kMaximumCoordinate);
}

bool ActorObservation::hasCurrentPosition() const
{
	return state == ObservationState::ObservedPresent &&
		isBoundedVector(position, WorldLimits::kMaximumCoordinate);
}

bool EntityObservation::isConfirmed() const
{
	return state == ObservationState::ObservedPresent &&
		confidence.isValid() && confidence.value > 0.0f &&
		isBoundedVector(position, WorldLimits::kMaximumCoordinate);
}

WorldSnapshot::WorldSnapshot()
	: identity_(),
	  actors_(),
	  actorCount_(0U),
	  entities_(),
	  entityCount_(0U),
	  sounds_(),
	  soundCount_(0U),
	  memories_(),
	  memoryCount_(0U)
{
}

bool WorldSnapshot::isValid() const
{
	return identity_.isValid();
}

const SnapshotIdentity &WorldSnapshot::identity() const
{
	return identity_;
}

std::size_t WorldSnapshot::actorCount() const
{
	return actorCount_;
}

const ActorObservation *WorldSnapshot::actorAt(std::size_t index) const
{
	return index < actorCount_ ? &actors_[index] : nullptr;
}

ContactLookupResult WorldSnapshot::findActor(
	const ActorKey &actor,
	const ActorObservation **observation) const
{
	if (observation == nullptr || !actor.isValid())
	{
		return ContactLookupResult::InvalidArgument;
	}

	*observation = nullptr;
	for (std::size_t index = 0U; index < actorCount_; ++index)
	{
		if (actors_[index].actor == actor)
		{
			*observation = &actors_[index];
			return ContactLookupResult::Found;
		}
	}

	return ContactLookupResult::Unknown;
}

std::size_t WorldSnapshot::entityCount() const
{
	return entityCount_;
}

const EntityObservation *WorldSnapshot::entityAt(std::size_t index) const
{
	return index < entityCount_ ? &entities_[index] : nullptr;
}

ContactLookupResult WorldSnapshot::findEntity(
	const EntityKey &entity,
	const EntityObservation **observation) const
{
	if (observation == nullptr || !entity.isValid())
	{
		return ContactLookupResult::InvalidArgument;
	}

	*observation = nullptr;
	for (std::size_t index = 0U; index < entityCount_; ++index)
	{
		if (entities_[index].entity == entity)
		{
			*observation = &entities_[index];
			return ContactLookupResult::Found;
		}
	}

	return ContactLookupResult::Unknown;
}

std::size_t WorldSnapshot::soundCount() const
{
	return soundCount_;
}

const AudibleEvent *WorldSnapshot::soundAt(std::size_t index) const
{
	return index < soundCount_ ? &sounds_[index] : nullptr;
}

MemoryLookupResult WorldSnapshot::memoryFor(
	const ActorKey &actor,
	const MemorySample **memory) const
{
	if (memory == nullptr || !actor.isValid())
	{
		return MemoryLookupResult::InvalidArgument;
	}

	*memory = nullptr;
	for (std::size_t index = 0U; index < memoryCount_; ++index)
	{
		if (memories_[index].actor == actor)
		{
			*memory = &memories_[index];
			return memories_[index].state == MemoryState::Expired ?
				MemoryLookupResult::Expired : MemoryLookupResult::Found;
		}
	}

	return MemoryLookupResult::NotFound;
}
}
}
