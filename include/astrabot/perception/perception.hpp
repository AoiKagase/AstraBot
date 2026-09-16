#ifndef ASTRABOT_PERCEPTION_PERCEPTION_HPP
#define ASTRABOT_PERCEPTION_PERCEPTION_HPP

#include "astrabot/world/world_snapshot.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace astrabot
{
namespace perception
{
enum class PerceptionInputResult
{
	Accepted,
	InvalidArgument,
	InvalidIdentity,
	InvalidObservation,
	DuplicateActor,
	DuplicateEntity,
	DuplicateSound,
	ResourceLimit
};

class PerceptionInput
{
public:
	PerceptionInput();
	explicit PerceptionInput(const world::SnapshotIdentity &identity);

	PerceptionInputResult setIdentity(const world::SnapshotIdentity &identity);
	const world::SnapshotIdentity &identity() const;

	PerceptionInputResult addActor(const world::ActorObservation &observation);
	PerceptionInputResult addEntity(const world::EntityObservation &observation);
	PerceptionInputResult addSound(const world::AudibleEvent &sound);

	std::size_t actorCount() const;
	const world::ActorObservation *actorAt(std::size_t index) const;
	std::size_t entityCount() const;
	const world::EntityObservation *entityAt(std::size_t index) const;
	std::size_t soundCount() const;
	const world::AudibleEvent *soundAt(std::size_t index) const;

private:
	world::SnapshotIdentity identity_;
	std::array<world::ActorObservation, world::WorldLimits::kMaximumActors>
		actors_;
	std::size_t actorCount_;
	std::array<world::EntityObservation, world::WorldLimits::kMaximumEntities>
		entities_;
	std::size_t entityCount_;
	std::array<world::AudibleEvent, world::WorldLimits::kMaximumSounds> sounds_;
	std::size_t soundCount_;
};

struct PerceptionAssemblerConfig
{
	std::uint32_t maximumObservationAgeTicks;
	std::uint32_t maximumMemoryAgeTicks;

	PerceptionAssemblerConfig();
	bool isValid() const;
};

enum class PerceptionResult
{
	Published,
	InvalidArgument,
	InvalidConfig,
	InvalidIdentity,
	InvalidObservation,
	StaleObservation,
	DuplicateFrame,
	StaleFrame,
	ResourceLimit,
	Invalidated,
	NotFound
};

class PerceptionAssembler
{
public:
	PerceptionAssembler();
	explicit PerceptionAssembler(const PerceptionAssemblerConfig &config);

	PerceptionResult publish(
		const PerceptionInput &input,
		world::WorldSnapshot *snapshot);
	PerceptionResult invalidateActor(const world::ActorKey &actor);

	const PerceptionAssemblerConfig &config() const;
	const world::WorldSnapshot &snapshot() const;

private:
	friend class PerceptionInput;

	struct MemoryRecord
	{
		bool active;
		world::ActorKey actor;
		world::MemoryState state;
		world::WorldPosition position;
		world::ContactConfidence confidence;
		world::FrameIdentity lastFrame;
	};

	static bool isFiniteVector(const world::WorldVector &value);
	static bool isBoundedVector(const world::WorldVector &value, float limit);
	static bool isValidObservationState(world::ObservationState state);
	static bool isValidTeamRelation(world::TeamRelation relation);
	static bool isValidEntityKind(world::EntityKind kind);
	static bool isValidSoundKind(world::SoundKind kind);
	static bool isValidActorObservation(
		const world::ActorObservation &observation);
	static bool isValidEntityObservation(
		const world::EntityObservation &observation);
	static bool isValidSound(const world::AudibleEvent &sound);
	static float normalizeConfidence(float value);
	static std::size_t actorIndex(const world::ActorKey &actor);
	static bool sameFrame(
		const world::FrameIdentity &left,
		const world::FrameIdentity &right);
	static bool isFrameAfter(
		const world::FrameIdentity &candidate,
		const world::FrameIdentity &current);
	static void clearMemory(MemoryRecord *record);
	static void expireMemory(MemoryRecord *record);
	static world::MemorySample sampleFor(const MemoryRecord &record);

	PerceptionResult validateInput(const PerceptionInput &input) const;
	void advanceMemory(
		std::array<MemoryRecord, world::WorldLimits::kMaximumActors> *memory,
		const world::FrameIdentity &frame) const;
	void updateMemory(
		std::array<MemoryRecord, world::WorldLimits::kMaximumActors> *memory,
		const world::ActorObservation &observation,
		const world::FrameIdentity &frame) const;

	PerceptionAssemblerConfig config_;
	std::array<MemoryRecord, world::WorldLimits::kMaximumActors> memory_;
	world::WorldSnapshot snapshot_;
	world::SnapshotIdentity lastIdentity_;
	bool hasPublished_;
};
}
}

#endif
