#ifndef ASTRABOT_BEHAVIOR_BEHAVIOR_STATE_HPP
#define ASTRABOT_BEHAVIOR_BEHAVIOR_STATE_HPP

#include "astrabot/behavior/objective_proposal.hpp"

#include <cstdint>

namespace astrabot
{
namespace behavior
{
enum class BehaviorState
{
	Initial,
	Roam,
	Seek,
	Engage,
	Retreat,
	Dead,
	Recovering
};

enum class Availability
{
	Unknown,
	Available,
	Unavailable
};

enum class LifeStatus
{
	Unknown,
	Alive,
	Dead
};

enum class TransitionReason
{
	None,
	Initial,
	EnemyVisible,
	EnemyLost,
	NoConfirmedContact,
	LowHealth,
	DeathObserved,
	RoundReset,
	MapGenerationChanged,
	ActorGenerationChanged,
	UnavailableObservation,
	RecoveryComplete,
	RecoveryTimeout
};

enum class BehaviorResult
{
	Transitioned,
	Unchanged,
	RecoveryPending,
	InvalidArgument,
	InvalidConfig,
	InvalidSnapshot,
	InvalidObservation,
	InvalidIdentity,
	StaleGeneration,
	DuplicateFrame,
	StaleFrame
};

struct BehaviorStateConfig
{
	float retreatHealth;
	std::uint32_t maximumRecoveryTicks;
	std::uint32_t maximumTargetMemoryTicks;

	BehaviorStateConfig();
	bool isValid() const;
};

struct ActorDecisionContext
{
	world::ActorKey actor;
	world::FrameIdentity frame;
	LifeStatus life;
	Availability roundAvailability;
	bool roundActive;
	Availability healthAvailability;
	float health;

	bool isValid() const;
};

struct BehaviorTransition
{
	world::ActorKey actor;
	world::FrameIdentity frame;
	BehaviorState from;
	BehaviorState to;
	TransitionReason reason;
	bool hasObjectiveProposal;
	ObjectiveProposal proposal;
};

class BehaviorStateMachine
{
public:
	BehaviorStateMachine();
	BehaviorStateMachine(
		const world::ActorKey &actor,
		const BehaviorStateConfig &config);

	BehaviorResult update(
		const world::WorldSnapshot &snapshot,
		const ActorDecisionContext &context,
		BehaviorTransition *transition);

	BehaviorState state() const;
	const world::ActorKey &actor() const;
	bool isInitialized() const;

private:
	friend struct ActorDecisionContext;

	static bool isValidAvailability(Availability availability);
	static bool isValidLifeStatus(LifeStatus life);
	static bool sameFrame(
		const world::FrameIdentity &left,
		const world::FrameIdentity &right);
	static bool isFrameAfter(
		const world::FrameIdentity &candidate,
		const world::FrameIdentity &current);
	static bool findVisibleEnemy(
		const world::WorldSnapshot &snapshot,
		const world::ActorKey &observer,
		world::ActorKey *enemy);
	static bool isTargetMemoryCurrent(
		const world::FrameIdentity &frame,
		const world::FrameIdentity &targetFrame,
		std::uint32_t maximumAgeTicks);
	static ObjectivePriority priorityFor(BehaviorState state);
	static ObjectiveKind objectiveFor(BehaviorState state);
	static bool makeProposal(
		const world::WorldSnapshot &snapshot,
		const world::ActorKey &actor,
		BehaviorState state,
		TransitionReason reason,
		const world::ActorKey &target,
		bool hasTarget,
		ObjectiveProposal *proposal);

	BehaviorResult transitionTo(
		const world::WorldSnapshot &snapshot,
		BehaviorState nextState,
		TransitionReason reason,
		const world::ActorKey &target,
		bool hasTarget,
		BehaviorTransition *transition);

	BehaviorStateConfig config_;
	world::ActorKey actor_;
	world::FrameIdentity lastFrame_;
	world::ActorKey rememberedTarget_;
	world::FrameIdentity targetFrame_;
	BehaviorState state_;
	std::uint32_t recoveryTicks_;
	bool hasRememberedTarget_;
	bool initialized_;
};
}
}

#endif
