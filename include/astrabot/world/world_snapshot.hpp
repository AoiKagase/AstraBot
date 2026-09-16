#ifndef ASTRABOT_WORLD_WORLD_SNAPSHOT_HPP
#define ASTRABOT_WORLD_WORLD_SNAPSHOT_HPP

#include <array>
#include <cstddef>
#include <cstdint>

namespace astrabot
{
namespace perception
{
class PerceptionAssembler;
}

namespace world
{
struct WorldLimits
{
	static constexpr std::size_t kMaximumActors = 32U;
	static constexpr std::size_t kMaximumEntities = 256U;
	static constexpr std::size_t kMaximumSounds = 64U;
	static constexpr std::uint32_t kMaximumAgeTicks = 4096U;
	static constexpr float kMaximumCoordinate = 32768.0f;
	static constexpr float kMaximumVelocity = 4096.0f;
	static constexpr float kMaximumAngle = 360.0f;
};

struct ActorKey
{
	std::uint32_t slot;
	std::uint32_t generation;

	bool isValid() const;
	bool operator==(const ActorKey &other) const;
};

struct EntityKey
{
	std::uint32_t id;
	std::uint32_t generation;

	bool isValid() const;
	bool operator==(const EntityKey &other) const;
};

struct FrameIdentity
{
	std::uint32_t mapGeneration;
	std::uint32_t roundGeneration;
	std::uint32_t tick;

	bool isValid() const;
	bool operator==(const FrameIdentity &other) const;
};

struct SnapshotIdentity
{
	FrameIdentity frame;
	ActorKey observer;
	std::uint64_t navRevision;

	bool isValid() const;
	bool operator==(const SnapshotIdentity &other) const;
};

enum class ObservationState
{
	Unknown,
	ObservedAbsent,
	ObservedPresent
};

enum class TeamRelation
{
	Unknown,
	Friendly,
	Hostile,
	Neutral
};

enum class EntityKind
{
	Unknown,
	Objective,
	Weapon,
	Projectile,
	Other
};

enum class SoundKind
{
	Unknown,
	Footstep,
	WeaponFire,
	Grenade,
	Objective,
	Other
};

enum class MemoryState
{
	None,
	Remembered,
	Expired
};

enum class ContactLookupResult
{
	Found,
	Unknown,
	InvalidArgument
};

enum class MemoryLookupResult
{
	Found,
	Expired,
	NotFound,
	InvalidArgument
};

struct WorldVector
{
	float x;
	float y;
	float z;
};

using WorldPosition = WorldVector;
using WorldVelocity = WorldVector;

struct ContactConfidence
{
	float value;
	std::uint32_t ageTicks;

	bool isValid() const;
};

struct MemorySample
{
	MemoryState state;
	ActorKey actor;
	WorldPosition position;
	ContactConfidence confidence;

	bool isUsable() const;
};

struct ActorObservation
{
	ActorKey actor;
	ObservationState state;
	TeamRelation relation;
	WorldPosition position;
	WorldVelocity velocity;
	float viewPitch;
	float viewYaw;
	ContactConfidence confidence;
	MemorySample memory;

	bool isConfirmed() const;
	bool hasCurrentPosition() const;
};

struct EntityObservation
{
	EntityKey entity;
	EntityKind kind;
	ObservationState state;
	WorldPosition position;
	WorldVelocity velocity;
	ContactConfidence confidence;

	bool isConfirmed() const;
};

struct AudibleEvent
{
	std::uint32_t id;
	SoundKind kind;
	WorldPosition origin;
	float loudness;
	ContactConfidence confidence;
};

class WorldSnapshot
{
public:
	WorldSnapshot();

	bool isValid() const;
	const SnapshotIdentity &identity() const;

	std::size_t actorCount() const;
	const ActorObservation *actorAt(std::size_t index) const;
	ContactLookupResult findActor(
		const ActorKey &actor,
		const ActorObservation **observation) const;

	std::size_t entityCount() const;
	const EntityObservation *entityAt(std::size_t index) const;
	ContactLookupResult findEntity(
		const EntityKey &entity,
		const EntityObservation **observation) const;

	std::size_t soundCount() const;
	const AudibleEvent *soundAt(std::size_t index) const;

	MemoryLookupResult memoryFor(
		const ActorKey &actor,
		const MemorySample **memory) const;

private:
	friend class astrabot::perception::PerceptionAssembler;

	SnapshotIdentity identity_;
	std::array<ActorObservation, WorldLimits::kMaximumActors> actors_;
	std::size_t actorCount_;
	std::array<EntityObservation, WorldLimits::kMaximumEntities> entities_;
	std::size_t entityCount_;
	std::array<AudibleEvent, WorldLimits::kMaximumSounds> sounds_;
	std::size_t soundCount_;
	std::array<MemorySample, WorldLimits::kMaximumActors> memories_;
	std::size_t memoryCount_;
};
}
}

#endif
