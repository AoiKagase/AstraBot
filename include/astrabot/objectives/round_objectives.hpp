#ifndef ASTRABOT_OBJECTIVES_ROUND_OBJECTIVES_HPP
#define ASTRABOT_OBJECTIVES_ROUND_OBJECTIVES_HPP

#include "astrabot/behavior/behavior_state.hpp"
#include "astrabot/objectives/objective_state.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace astrabot
{
namespace objectives
{
struct ObjectiveLimits;

enum class Availability
{
	Unknown,
	Unavailable,
	Available
};

enum class RoundPhase
{
	Unknown,
	Freeze,
	Live,
	PostRound
};

enum class ScenarioEventKind
{
	Unknown,
	BombCarried,
	BombPlanted,
	BombDefused,
	BombExploded,
	HostageLocated,
	HostagePickedUp,
	HostageRescued,
	HostageKilled,
	RoundStarted,
	RoundEnded
};

enum class EventState
{
	Unknown,
	Observed,
	Unavailable
};

struct ScenarioEvent
{
	std::uint32_t id;
	world::FrameIdentity frame;
	ScenarioEventKind kind;
	EventState state;
	world::ActorKey actor;
	TeamRole team;
};

struct ScenarioObservation
{
	static constexpr std::size_t kMaximumEvents = 16U;

	ScenarioIdentity scenario;
	RoundPhase phase;
	Availability buyAvailability;
	std::array<ScenarioEvent, kMaximumEvents> events;
	std::size_t eventCount;

	bool isValid() const;
};

struct RoundObjectiveConfig
{
	std::uint32_t proposalLifetimeTicks;

	RoundObjectiveConfig();
	bool isValid() const;
};

enum class ObjectiveResult
{
	Proposed,
	RecoveryPending,
	NoObjective,
	Completed,
	InvalidArgument,
	InvalidConfig,
	InvalidSnapshot,
	InvalidObservation,
	StaleGeneration,
	StaleFrame
};

class RoundObjectivePlanner
{
public:
	RoundObjectivePlanner();
	explicit RoundObjectivePlanner(const world::ActorKey &actor);
	RoundObjectivePlanner(
		const world::ActorKey &actor,
		const RoundObjectiveConfig &config);

	ObjectiveResult plan(
		const world::WorldSnapshot &snapshot,
		behavior::BehaviorState behaviorState,
		const ScenarioObservation &observation,
		const ObjectiveCompletionFeedback *feedback,
		ObjectiveProposal *proposal);

	const world::ActorKey &actor() const;
	bool isInitialized() const;

private:
	static bool isValidPhase(RoundPhase phase);
	static bool isValidAvailability(Availability availability);
	static bool isValidEventKind(ScenarioEventKind kind);
	static bool isValidEventState(EventState state);
	static bool sameRound(
		const world::FrameIdentity &left,
		const world::FrameIdentity &right);
	static bool isFrameAfter(
		const world::FrameIdentity &candidate,
		const world::FrameIdentity &current);
	static bool makeProposal(
		const ScenarioObservation &observation,
		ObjectiveKind kind,
		ObjectivePriority priority,
		std::uint32_t subjectId,
		ObjectiveProposal *proposal,
		std::uint32_t lifetimeTicks);
	static bool findEvent(
		const ScenarioObservation &observation,
		ScenarioEventKind kind,
		ScenarioEvent *event);

	ObjectiveResult proposalForScenario(
		const world::WorldSnapshot &snapshot,
		behavior::BehaviorState behaviorState,
		const ScenarioObservation &observation,
		ObjectiveProposal *proposal);

	RoundObjectiveConfig config_;
	world::ActorKey actor_;
	world::FrameIdentity lastFrame_;
	ObjectiveState state_;
	bool initialized_;
};
}
}

#endif
