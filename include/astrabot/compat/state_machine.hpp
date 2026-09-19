#ifndef ASTRABOT_COMPAT_STATE_MACHINE_HPP
#define ASTRABOT_COMPAT_STATE_MACHINE_HPP

#include "astrabot/world/world_snapshot.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace astrabot
{
namespace compat
{
enum class CompatibilityStateId : std::uint8_t
{
	None,
	Idle,
	AttackOverlay,
	Buy,
	DefuseBomb,
	EscapeFromBomb,
	FetchBomb,
	Follow,
	Hide,
	Hunt,
	InvestigateNoise,
	MoveTo,
	PlantBomb,
	UseEntity
};

enum class CompatibilityTaskId : std::uint8_t
{
	None,
	SeekAndDestroy,
	Buy,
	DefuseBomb,
	EscapeFromBomb,
	FetchBomb,
	Follow,
	InvestigateNoise,
	MoveTo,
	PlantBomb,
	GuardTickingBomb,
	MoveToLastKnownEnemyPosition
};

enum class ObservationAvailability : std::uint8_t
{
	Available,
	Unavailable
};

enum class StateTransitionId : std::uint8_t
{
	None,
	InitialState,
	ExplicitStateChange,
	IdleToMoveTo,
	MoveToHunt,
	AttackOverlayStart,
	AttackOverlayStop,
	LifecycleDeath,
	LifecycleRespawn,
	LifecycleRoundReset,
	RngDependent
};

enum class StateTransitionReason : std::uint8_t
{
	None,
	Initial,
	ExplicitRequest,
	EnemyVisible,
	EnemyLost,
	ObjectiveChanged,
	PathFailure,
	LeaderInvalid,
	DeathObserved,
	RespawnObserved,
	RoundReset,
	AttackStarted,
	AttackEnded,
	ObservationUnavailable,
	RngUnavailable
};

enum class StateResult : std::uint8_t
{
	Transitioned,
	Updated,
	NoOp,
	SkippedNotFullUpdate,
	BlockedByObservation,
	BlockedByRng,
	InvalidContext,
	InvalidArgument,
	NotInitialized,
	DuplicateUpdate
};

enum class StateTraceEvent : std::uint8_t
{
	OnExit,
	OnEnter,
	StatePublished,
	TimestampUpdated,
	OnUpdate,
	SideEffect,
	AttackOnEnter,
	AttackOnUpdate,
	AttackOnExit
};

enum class StateSideEffect : std::uint8_t
{
	None,
	ResetPath,
	ClearLookTarget,
	ClearTarget,
	SetTask,
	ClearMovement
};

struct StateUpdateContext
{
	world::ActorKey actor;
	world::FrameIdentity frame;
	float timestamp;
	std::uint32_t fullUpdateSequence;
	bool fullUpdate;
	ObservationAvailability lifeAvailability;
	bool alive;
	ObservationAvailability roundAvailability;
	bool roundActive;
	bool requestTransition;
	CompatibilityStateId requestedState;
	StateTransitionId transitionId;
	StateTransitionReason transitionReason;
	ObservationAvailability transitionRequest;
	ObservationAvailability attackObservation;
	ObservationAvailability rngAvailability;

	bool isValid() const;
};

struct StateTraceRecord
{
	std::uint64_t sequence;
	std::uint64_t stateSequence;
	StateTraceEvent event;
	world::ActorKey actor;
	world::FrameIdentity frame;
	CompatibilityStateId previousState;
	CompatibilityStateId currentState;
	CompatibilityStateId callbackState;
	CompatibilityStateId machineState;
	StateTransitionId transitionId;
	StateTransitionReason reason;
	float timestamp;
	std::uint32_t fullUpdateSequence;
	bool attackOverlayActive;
	CompatibilityTaskId task;
	StateSideEffect sideEffect;
};

class IStateTraceSink
{
public:
	virtual ~IStateTraceSink() { }
	virtual void record(const StateTraceRecord &record) = 0;
};

const char *toString(CompatibilityStateId state);
const char *toString(CompatibilityTaskId task);
const char *toString(StateTransitionId transition);
const char *toString(StateTransitionReason reason);
const char *toString(StateResult result);

class CompatibilityStateMachine
{
public:
	static constexpr std::size_t kNormalStateCount = 12U;

	CompatibilityStateMachine();
	explicit CompatibilityStateMachine(const world::ActorKey &actor);

	void setTraceSink(IStateTraceSink *sink);
	StateResult initialize(const StateUpdateContext &context);
	StateResult update(const StateUpdateContext &context);
	StateResult transition(
		CompatibilityStateId nextState,
		StateTransitionReason reason,
		StateTransitionId transition,
		const StateUpdateContext &context);
	StateResult beginAttack(const StateUpdateContext &context);
	StateResult stopAttack(const StateUpdateContext &context);
	StateResult onSpawn(const StateUpdateContext &context);
	StateResult onDeath(const StateUpdateContext &context);
	StateResult onRespawn(const StateUpdateContext &context);
	StateResult onRoundReset(const StateUpdateContext &context);

	CompatibilityStateId state() const;
	CompatibilityTaskId task() const;
	const world::ActorKey &actor() const;
	float stateTimestamp() const;
	bool attackOverlayActive() const;
	bool isInitialized() const;

private:
	struct StateInstance
	{
		CompatibilityStateId id;
		std::uint32_t enterCount;
		std::uint32_t updateCount;
		std::uint32_t exitCount;
	};

	static bool isNormalState(CompatibilityStateId state);
	static bool isValidTransition(CompatibilityStateId state);
	static bool isAvailable(ObservationAvailability availability);
	static std::size_t stateIndex(CompatibilityStateId state);
	static StateUpdateContext resetContextFor(
		const StateUpdateContext &context,
		StateTransitionReason reason,
		StateTransitionId transition);

	bool validateContext(const StateUpdateContext &context) const;
	StateInstance *instanceFor(CompatibilityStateId state);
	StateResult transitionInternal(
		CompatibilityStateId nextState,
		StateTransitionReason reason,
		StateTransitionId transition,
		const StateUpdateContext &context,
		bool forceLifecycle);
	StateResult stopAttackInternal(const StateUpdateContext &context);
	void invokeEnter(
		CompatibilityStateId state,
		CompatibilityStateId previousState,
		StateTransitionReason reason,
		StateTransitionId transition,
		const StateUpdateContext &context);
	void invokeUpdate(const StateUpdateContext &context);
	void invokeExit(
		CompatibilityStateId state,
		CompatibilityStateId nextState,
		StateTransitionReason reason,
		StateTransitionId transition,
		const StateUpdateContext &context);
	void emit(
		StateTraceEvent event,
		CompatibilityStateId callbackState,
		CompatibilityStateId previousState,
		CompatibilityStateId currentState,
		StateTransitionReason reason,
		StateTransitionId transition,
		const StateUpdateContext &context,
		StateSideEffect sideEffect = StateSideEffect::None);
	void applyEnterEffects(
		CompatibilityStateId state,
		CompatibilityStateId previousState,
		StateTransitionReason reason,
		StateTransitionId transition,
		const StateUpdateContext &context);
	void applyExitEffects(
		CompatibilityStateId state,
		CompatibilityStateId nextState,
		StateTransitionReason reason,
		StateTransitionId transition,
		const StateUpdateContext &context);
	void emitSideEffect(
		CompatibilityStateId callbackState,
		CompatibilityStateId previousState,
		CompatibilityStateId currentState,
		StateTransitionReason reason,
		StateTransitionId transition,
		const StateUpdateContext &context,
		StateSideEffect sideEffect);

	std::array<StateInstance, kNormalStateCount> stateInstances_;
	world::ActorKey actor_;
	CompatibilityStateId state_;
	CompatibilityTaskId task_;
	float stateTimestamp_;
	std::uint32_t lastFullUpdateSequence_;
	std::uint64_t traceSequence_;
	std::uint64_t stateSequence_;
	bool hasLastFullUpdate_;
	bool attackOverlayActive_;
	bool initialized_;
	IStateTraceSink *traceSink_;
};
}
}

#endif
