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
	DuplicateEvent,
	ResourceLimit
};

struct VisionObservation
{
	world::ActorKey target;
	bool fovPassed;
	bool losPassed;
	std::uint8_t visibleParts;
	bool visible;
};

struct PerceptionEvent
{
	std::uint32_t id;
	world::PerceptionEventType type;
	world::ActorKey actor;
	world::ActorKey subject;
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
	PerceptionInputResult addEvent(const PerceptionEvent &event);

	std::size_t actorCount() const;
	const world::ActorObservation *actorAt(std::size_t index) const;
	std::size_t entityCount() const;
	const world::EntityObservation *entityAt(std::size_t index) const;
	std::size_t soundCount() const;
	const world::AudibleEvent *soundAt(std::size_t index) const;
	std::size_t eventCount() const;
	const PerceptionEvent *eventAt(std::size_t index) const;

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
	std::array<PerceptionEvent, world::WorldLimits::kMaximumSounds> events_;
	std::size_t eventCount_;
};

struct PerceptionAssemblerConfig
{
	std::uint32_t maximumObservationAgeTicks;
	std::uint32_t maximumMemoryAgeTicks;
	std::uint32_t maximumNoiseAgeTicks;
	std::uint32_t noiseReactionDelayTicks;
	std::uint32_t maximumHearingDistance;

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
	NotFound,
	InvalidEvent
};

enum class PerceptionTraceKind
{
	Vision,
	Knowledge,
	Noise,
	Event
};

struct PerceptionTraceRecord
{
	std::uint64_t sequence;
	PerceptionTraceKind kind;
	world::ActorKey actor;
	world::ActorKey target;
	world::FrameIdentity frame;
	bool visible;
	bool fovPassed;
	bool losPassed;
	std::uint8_t visibleParts;
	world::KnowledgeState knowledge;
	world::WorldPosition position;
	bool hasPosition;
	world::SoundKind soundKind;
	world::NoisePriority noisePriority;
	bool heard;
	bool rejected;
	world::PerceptionEventType eventType;
};

class IPerceptionTraceSink
{
public:
	virtual ~IPerceptionTraceSink() {}
	virtual void record(const PerceptionTraceRecord &record) = 0;
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
	void setTraceSink(IPerceptionTraceSink *sink);

private:
	friend class PerceptionInput;

	struct MemoryRecord
	{
		bool active;
		world::ActorKey actor;
		world::MemoryState state;
		world::KnowledgeState knowledge;
		world::WorldPosition position;
		world::ContactConfidence confidence;
		std::uint8_t visibleParts;
		world::FrameIdentity lastFrame;
		world::FrameIdentity lastSeenFrame;
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
	static void clearNoise(world::NoiseMemory *noise);

	PerceptionResult validateInput(const PerceptionInput &input) const;
	void advanceMemory(
		std::array<MemoryRecord, world::WorldLimits::kMaximumActors> *memory,
		const world::FrameIdentity &frame) const;
	void updateMemory(
		std::array<MemoryRecord, world::WorldLimits::kMaximumActors> *memory,
		const world::ActorObservation &observation,
		const world::FrameIdentity &frame) const;
	void advanceNoise(world::NoiseMemory *noise,
		const world::FrameIdentity &frame) const;
	void updateNoise(world::NoiseMemory *noise,
		const world::AudibleEvent &sound,
		const world::ActorKey &observer,
		const world::FrameIdentity &frame) const;
	void applyEvent(
		std::array<MemoryRecord, world::WorldLimits::kMaximumActors> *memory,
		world::NoiseMemory *noise,
		const PerceptionEvent &event,
		const world::FrameIdentity &frame) const;
	void emit(const PerceptionTraceRecord &record) const;

	PerceptionAssemblerConfig config_;
	std::array<MemoryRecord, world::WorldLimits::kMaximumActors> memory_;
	world::WorldSnapshot snapshot_;
	world::SnapshotIdentity lastIdentity_;
	IPerceptionTraceSink *traceSink_;
	mutable std::uint64_t traceSequence_;
	bool hasPublished_;
};
}
}

#endif
