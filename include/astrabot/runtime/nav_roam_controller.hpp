#ifndef ASTRABOT_RUNTIME_NAV_ROAM_CONTROLLER_HPP
#define ASTRABOT_RUNTIME_NAV_ROAM_CONTROLLER_HPP

#include "astrabot/nav/jump_drop.hpp"
#include "astrabot/nav/locomotion.hpp"
#include "astrabot/nav/special_traversal.hpp"
#include "astrabot/nav/native_ladder.hpp"
#include "astrabot/compat/runtime_mode_policy.hpp"
#include "astrabot/compat/random_source.hpp"
#include "astrabot/world/world_snapshot.hpp"
#include "astrabot/runtime/actor_registry.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace astrabot
{
namespace runtime
{
enum class NoiseRouteUpdate
{
	None,
	Enter,
	Redirect,
	Follow
};

struct NavRoamObservation
{
	ActorId actor;
	world::FrameIdentity frame;
	std::uint8_t team;
	nav::LocomotionObservation locomotion;
	bool hasObjectiveTarget;
	nav::NavRouteType objectiveRouteType = nav::NavRouteType::Fastest;
	nav::NavVector objectiveTarget;
	bool airborne;
	bool landingConfirmed;
	bool hasLandingDamage;
	float landingDamage;
	bool ladderContact;
	bool entryConfirmed;
	bool exitConfirmed;
	bool collectPathStats;
	bool movementSuppressed;
	world::EntityKey objectiveTargetEntity;
	// Navigation uses feet; CSBot's friend ray uses entity origins against
	// floor-level path points. Friends share friendRayOrigin's coordinates.
	// Without a public origin, existing geometry-only callers use feet.
	bool friendRayOriginAvailable = false;
	nav::NavVector friendRayOrigin = {};
	std::vector<nav::NavVector> nearbyFriends;
	const nav::NavLadderTopology *nativeLadders = nullptr;
	nav::INativeLadderProbe *nativeLadderProbe = nullptr;
	float viewPitch = 0.0f;
	NoiseRouteUpdate noiseRoute = NoiseRouteUpdate::None;
	nav::AreaId noisePathStartArea = 0U;
	nav::AreaId noiseGoalArea = 0U;
	bool noiseTargetCleared = false;
	bool nativeDefusePath = false;
};

struct GroundLookaheadRoute
{
	ActorId actor;
	world::FrameIdentity frame;
	nav::AreaId targetArea;
	nav::NavVector direction;
	std::uint32_t fullUpdateSequence;
	float createdAt;
	bool running;
	bool active;

	void reset();
	bool matchesNextUpdate(const ActorId &currentActor, const world::FrameIdentity &currentFrame,
		std::uint32_t currentFullUpdate, float now) const;
};

enum class NavRoamResult
{
	IntentReady,
	TargetReached,
	NoRoute,
	ReplanRequired,
	InvalidArgument,
	InvalidSnapshot,
	InvalidObservation,
	StaleActor,
	DuplicateFrame,
	StaleFrame,
	MovementSuppressed
};

enum class NavRoamStage
{
	None,
	ExactArea,
	OffMeshRecovery,
	LinkSelection,
	CorridorReady,
	LocomotionReady,
	TargetReached,
	Failed,
	MovementSuppressed
};

enum class NavRecomputeReason
{
	None,
	InitialGoal,
	GoalChanged,
	MapOrRoundChanged,
	PathInvalidated,
	Stuck
};

enum NavGoalSelectionReason
{
	GoalSelectionNone,
	GoalSelectionInitial,
	GoalSelectionReached,
	GoalSelectionPathFailed,
	GoalSelectionNoEligibleArea
};

enum NavGoalSelectionStrategy
{
	GoalSelectionStrategyNone,
	GoalSelectionStrategyOldestVisitedArea,
	GoalSelectionStrategyRandomFallback,
	GoalSelectionStrategyNoEligibleArea
};

enum class NavRoamReservationFallbackReason : std::uint8_t
{
	None,
	NoDistinctRouteFound,
	CandidateBudgetExhausted
};

enum class NavGoalKind
{
	None,
	Roam,
	Objective
};

enum class NavFailureReason
{
	None,
	NoGoal,
	GoalInvalid,
	CurrentAreaMissing,
	GoalAreaMissing,
	PathSearchFailed,
	NavApplyRejected,
	MovementNotProduced,
	UnsafeDrop,
	RecoveryNoProgress
};

struct NavRoamDecision
{
	NavRoamStage stage;
	nav::NavQueryResult currentAreaResult;
	nav::NavQueryResult nearestAreaResult;
	nav::NavQueryResult linkResult;
	nav::NavQueryResult corridorResult;
	nav::LocomotionResult locomotionResult;
	NavRecomputeReason recomputeReason;
	NavFailureReason failureReason;
	NavGoalKind goalKind;
	NavGoalSelectionReason goalSelectionReason;
	NavGoalSelectionStrategy goalSelectionStrategy;
	bool reservationFallback;
	NavRoamReservationFallbackReason reservationFallbackReason;
	bool goalPresent;
	bool pathRequested;
	nav::NavQueryResult pathResult;
	nav::NavSearchStats pathSearchStats;
	nav::AreaId goalArea;
	nav::NavVector goalPosition;
	std::uint32_t goalGeneration;
	std::uint32_t pathSequence;
	std::uint32_t fullUpdateSequence;
	nav::NavRouteType routeType;
	float pathCost;
	std::vector<nav::AreaId> selectedPath;
	nav::AreaId currentArea;
	nav::AreaId recoveryArea;
	nav::AreaId targetArea;
	float nearestDistanceSquared;
	nav::NavVector targetPosition;
	std::uint32_t recoveryNoProgressUpdates;
	bool recoveryGeometryOnly;
	bool recoveryCandidateFailed;
	nav::NavVector intentDirection;
	nav::NavVector observationPosition;
	nav::NavVector observationVelocity;
	std::size_t corridorAreaCount;
	std::size_t corridorIndex;
	nav::AreaId linkFromArea;
	nav::AreaId linkToArea;
	std::uint8_t linkDirection;
	std::uint8_t linkHow;
	bool huntEndpointSteering = false;
	nav::NavVector huntEndpoint{};
};

class NavAreaVisitHistory
{
public:
	void record(std::uint32_t mapGeneration, std::uint8_t team, nav::AreaId area, std::uint32_t frame);
	std::uint32_t lastVisited(std::uint32_t mapGeneration, std::uint8_t team, nav::AreaId area) const;
	void reset(std::uint32_t mapGeneration);

private:
	struct Entry
	{
		std::uint8_t team;
		nav::AreaId area;
		std::uint32_t frame;
	};

	std::uint32_t mapGeneration_ = 0U;
	bool initialized_ = false;
	std::vector<Entry> entries_;
};

class NavRoamReservationBoard
{
public:
	NavRoamReservationBoard();
	void reset(std::uint32_t mapGeneration, std::uint32_t roundGeneration);
	bool goalReservedByOther(std::uint8_t team, ActorId actor, nav::AreaId goalArea) const;
	bool firstLinkReservedByOther(std::uint8_t team, ActorId actor, nav::AreaId fromArea, nav::AreaId toArea) const;
	bool reserve(std::uint8_t team, ActorId actor, nav::AreaId goalArea, nav::AreaId fromArea, nav::AreaId toArea);
	void release(ActorId actor);

private:
	struct Claim
	{
		bool valid;
		std::uint8_t team;
		ActorId actor;
		nav::AreaId goalArea;
		nav::AreaId fromArea;
		nav::AreaId toArea;
	};

	std::array<Claim, LifecycleSession::kClientSlotCount> claims_;
	std::uint32_t mapGeneration_;
	std::uint32_t roundGeneration_;
	bool initialized_;
};

class NavRoamController
{
public:
	NavRoamController();
	explicit NavRoamController(compat::RuntimeMode mode);
	void setRandomSource(compat::ICompatibilityRandomSource *source);
	void setPathCostContext(const nav::NavPathCostContext &context);
	void setAreaVisitHistory(NavAreaVisitHistory *history);
	void setReservationBoard(NavRoamReservationBoard *board);
	void setRuntimeMode(compat::RuntimeMode mode);
	NavRoamResult update(
		const nav::NavSnapshot &snapshot, const NavRoamObservation &observation, nav::LocomotionIntent *intent);
	NavRoamResult update(const nav::NavSnapshot &snapshot, const NavRoamObservation &observation,
		nav::LocomotionIntent *intent, NavRoamDecision *decision);
	void reset();
	void destroyPath();
	bool usingNativeLadder() const
	{
		return nativeLadder_.active();
	}
	bool nativeLadderLook(const nav::NavSnapshot &, const nav::NativeLadderObservation &, nav::NativeLadderLook *,
		nav::NavLadder * = nullptr) const;
	void resetProgressMonitor();
	void observeMotion(ActorId actor, world::FrameIdentity frame, const nav::LocomotionObservation &observation);
	bool isActive() const;
	bool hasPath() const;
	bool hasNativeLadderPath() const;
	float pathDistanceRemaining(const nav::NavSnapshot &snapshot) const;

private:
	nav::NativeLadderRecovery nativeLadderRecovery_{};
	nav::NativeLadderController nativeLadder_{};
	std::size_t nativeLadderStep_ = 0;
	std::uint64_t routeRevision_ = 1;
	ActorId motionActor_{};
	world::FrameIdentity motionFrame_{};
	NavRoamResult updateInternal(const nav::NavSnapshot &snapshot, const NavRoamObservation &observation,
		nav::LocomotionIntent *intent, NavRoamDecision *decision);

	struct NavigationPhaseResult
	{
		bool complete;
		NavRoamResult result;
	};
	struct NavigationUpdateContext
	{
		const nav::NavSnapshot &snapshot;
		const NavRoamObservation &observation;
		nav::LocomotionIntent *intent;
		NavRoamDecision *decision;
		nav::NavQuery &query;
		nav::NavAreaMatch currentArea{};
		nav::NavQueryResult currentAreaResult = nav::NavQueryResult::NoAreaContaining;
		nav::AreaId objectiveArea = 0U;
		bool areasLocated = false;
	};
	NavigationPhaseResult validateNavigationUpdate(const nav::NavSnapshot &snapshot,
		const NavRoamObservation &observation, nav::LocomotionIntent *intent, NavRoamDecision *decision);
	NavigationPhaseResult updateFriendWaiting(
		const nav::NavSnapshot &snapshot, const NavRoamObservation &observation, NavRoamDecision *decision);
	NavigationPhaseResult locateUpdateAreas(NavigationUpdateContext &ctx);
	NavigationPhaseResult prepareNoiseRoute(NavigationUpdateContext &ctx);
	bool buildNoiseRoute(NavigationUpdateContext &ctx);
	NavigationPhaseResult recoverOffMeshUpdate(NavigationUpdateContext &ctx);
	NavigationPhaseResult recoverStuckUpdate(NavigationUpdateContext &ctx);
	NavigationPhaseResult selectUpdateRoute(NavigationUpdateContext &ctx);
	NavigationPhaseResult startUpdateTraversal(NavigationUpdateContext &ctx);
	NavigationPhaseResult updateJumpDropTraversal(NavigationUpdateContext &ctx);
	NavigationPhaseResult updateNativeTraversal(NavigationUpdateContext &ctx);
	nav::NativeLadderObservation nativeObservation(const NavRoamObservation &) const;
	static bool sameNativeLadder(const nav::NavLadder &, const nav::NavLadder &);
	NavigationPhaseResult updateSpecialTraversal(NavigationUpdateContext &ctx);
	NavigationPhaseResult updateGroundLocomotion(NavigationUpdateContext &ctx);
	bool expireGroundRoute(
		const NavRoamObservation &observation, nav::LocomotionIntent *intent, NavRoamDecision *decision);
	struct OffMeshRecoveryState
	{
		nav::AreaId area;
		nav::NavVector target;
		nav::NavVector blockedPosition;
		float bestDistance;
		std::uint32_t noProgressUpdates;
		static constexpr auto kMaximumFailedAreas = 4U;
		std::array<nav::AreaId, kMaximumFailedAreas> failedAreas;
		std::size_t failedAreaCount;
		bool active;
		bool blocked;
	};
	OffMeshRecoveryState offMeshRecovery_;
	struct UnsafeDropFailure
	{
		bool valid;
		bool bypassPending;
		world::EntityKey entity;
		nav::NavVector anchor;
		nav::AreaId goalArea;
		nav::NavDirectedLink link;
		std::uint32_t retryFramesRemaining;
		std::uint32_t nextBackoffFrames;
	};
	UnsafeDropFailure unsafeDropFailure_;
	bool previousMovementSuppressed_;
	void rememberUnsafeDrop(
		const NavRoamObservation &observation, nav::AreaId goalArea, const nav::NavDirectedLink &link);
	void delayUnsafeDropRetry();
	struct PathFailureKey
	{
		std::uint32_t mapGeneration;
		nav::AreaId startArea;
		nav::AreaId goalArea;
		nav::NavRouteType routeType;
	};

	static bool sameFrame(const world::FrameIdentity &left, const world::FrameIdentity &right);
	static bool sameActor(const ActorId &left, const ActorId &right);
	static bool isFrameAfter(const world::FrameIdentity &candidate, const world::FrameIdentity &current);
	static bool isValidObservation(const NavRoamObservation &observation);
	bool startTraversal(const nav::NavSnapshot &snapshot, const nav::NavCorridor &corridor,
		const nav::NavDirectedLink &link, nav::LocomotionResult *startResult = nullptr,
		const nav::NavVector *launchPosition = nullptr, const nav::NavVector *landingPosition = nullptr);
	bool startRoute(const nav::NavSnapshot &snapshot, const nav::NavCorridor &corridor,
		const nav::NavDirectedLink &link, nav::LocomotionResult *startResult = nullptr);
	bool buildStuckRecoveryIntent(
		const nav::NavSnapshot &snapshot, const nav::NavAreaMatch &currentArea, nav::LocomotionIntent *intent);
	bool scheduleStuckJump(ActorId actor, std::uint32_t tick, float observedAt);
	struct HuntRouteSampling
	{
		const nav::NavHuntPathProbe *probe;
		bool invalidated = false;
	};
	bool selectRoute(const nav::NavSnapshot &snapshot, const nav::NavAreaMatch &currentArea, nav::AreaId objectiveArea,
		NavRoamDecision *decision, nav::NavRouteType routeType = nav::NavRouteType::Fastest,
		const nav::NavLadderTopology *nativeLadders = nullptr, HuntRouteSampling *sampling = nullptr,
		bool nativeDefusePath = false);
	bool selectRoamRoute(const nav::NavSnapshot &snapshot, const nav::NavAreaMatch &currentArea,
		NavRoamDecision *decision, HuntRouteSampling *sampling);
	bool selectCompatibilityGoal(const nav::NavSnapshot &snapshot, const nav::NavAreaMatch &currentArea,
		const std::vector<nav::NavDirectedLink> &links, NavRoamDecision *decision, HuntRouteSampling *sampling);
	void rememberRoute(
		const nav::NavSnapshot &snapshot, const nav::NavCorridor &corridor, const nav::NavDirectedLink &link);
	void populateRouteDecision(NavRoamDecision *decision) const;
	void resetRoute(bool preserveRoamGoal = false);
	bool isPathFailureBackedOff(std::uint32_t mapGeneration, nav::AreaId startArea, nav::AreaId goalArea,
		nav::NavRouteType routeType, std::uint32_t frame) const;
	void rememberPathFailure(std::uint32_t mapGeneration, nav::AreaId startArea, nav::AreaId goalArea,
		nav::NavRouteType routeType, std::uint32_t frame);
	void clearPathFailure();

	nav::LocomotionController locomotion_;
	nav::NavPathCostContext pathCostContext_;
	bool hasPathCostContext_ = false;
	bool timedCompatibility_ = false;
	float observedAt_ = 0.0f;
	float nextPathAttemptAt_ = 0.0f;
	float stuckRecoveryEndsAt_ = 0.0f;
	float corridorProgressAt_ = 0.0f;
	bool waitingBehindFriend_ = false;
	bool friendInTheWay_ = false;
	static constexpr float kFriendCheckUnscheduledTime = -1.0f;
	float friendCheckAt_ = kFriendCheckUnscheduledTime;
	float friendWaitUntil_ = 0.0f;
	bool friendRepathPending_ = false;
	nav::NavVector friendPathTarget_{};
	bool viewYawAvailable_ = false;
	float viewYaw_ = 0.0f;
	nav::NavVector stuckOrigin_ = {};
	nav::LocomotionObservation latestLocomotion_ = {};
	bool stuckJumpScheduled_ = false;
	float nextStuckJumpAt_ = 0.0f;
	static constexpr float kNoPreviousWiggleJumpTime = -10000.0f;
	float lastWiggleJumpAt_ = kNoPreviousWiggleJumpTime;
	nav::TraversalAction activeTraversal_;
	nav::JumpDropController jumpDrop_;
	nav::SpecialTraversalController specialTraversal_;
	compat::RuntimeModePolicy modePolicy_;
	ActorId actor_;
	world::FrameIdentity lastFrame_;
	std::size_t nextLinkIndex_;
	nav::NavCorridor activeCorridor_;
	nav::NavDirectedLink activeLink_;
	nav::NavVector activeTargetPosition_;
	nav::NavVector lastIntentDirection_;
	nav::NavVector stuckRecoveryDirection_;
	std::size_t activeCorridorIndex_;
	std::uint32_t pathSequence_;
	std::uint32_t stuckRecoveryCount_;
	std::uint32_t stuckRecoveryFramesRemaining_;
	bool stuckRecoveryActive_;
	bool hasActiveRoute_;
	bool hasAvoidedLink_;
	nav::NavDirectedLink avoidedLink_;
	bool hasObjectiveTarget_;
	nav::NavVector objectiveTarget_;
	bool hasPathFailure_;
	PathFailureKey pathFailureKey_;
	std::uint32_t pathFailureRetryFrame_;
	std::uint32_t pathFailureBackoffFrames_;
	bool collectPathStats_;
	bool initialized_;
	compat::ICompatibilityRandomSource *randomSource_;
	NavAreaVisitHistory localAreaVisitHistory_;
	NavAreaVisitHistory *areaVisitHistory_;
	NavRoamReservationBoard *reservationBoard_;
	nav::AreaId lastVisitedArea_;
	std::uint32_t lastVisitedMapGeneration_;
	std::uint8_t lastVisitedTeam_;
	std::uint32_t roamGoalGeneration_;
	NavGoalSelectionReason goalSelectionReason_;
	NavGoalSelectionStrategy goalSelectionStrategy_;
	float maximumSafeDropHeight_;
	bool safeDropHeightAvailable_;
	bool unsafeDropRejected_;
	bool hasRoamGoal_;
	nav::AreaId roamGoalArea_;
	nav::AreaId failedRoamGoalArea_;
	nav::NavVector roamGoalPosition_;

	// These private views name state boundaries while retaining the original
	// storage order, object layout and constructor initialization order. A view
	// never escapes the controller and owns no state or observation cache.
	struct RoamGoalState
	{
		bool &hasObjectiveTarget;
		nav::NavVector &objectiveTarget;
		nav::AreaId &lastVisitedArea;
		std::uint32_t &lastVisitedMapGeneration;
		std::uint8_t &lastVisitedTeam;
		std::uint32_t &roamGoalGeneration;
		NavGoalSelectionReason &goalSelectionReason;
		NavGoalSelectionStrategy &goalSelectionStrategy;
		bool &hasRoamGoal;
		nav::AreaId &roamGoalArea;
		nav::AreaId &failedRoamGoalArea;
		nav::NavVector &roamGoalPosition;
	};
	RoamGoalState roamGoalState()
	{
		return {hasObjectiveTarget_, objectiveTarget_, lastVisitedArea_, lastVisitedMapGeneration_, lastVisitedTeam_,
			roamGoalGeneration_, goalSelectionReason_, goalSelectionStrategy_, hasRoamGoal_, roamGoalArea_,
			failedRoamGoalArea_, roamGoalPosition_};
	}

	struct ConstRoamGoalState
	{
		const bool &hasObjectiveTarget;
		const nav::NavVector &objectiveTarget;
		const nav::AreaId &lastVisitedArea;
		const std::uint32_t &lastVisitedMapGeneration;
		const std::uint8_t &lastVisitedTeam;
		const std::uint32_t &roamGoalGeneration;
		const NavGoalSelectionReason &goalSelectionReason;
		const NavGoalSelectionStrategy &goalSelectionStrategy;
		const bool &hasRoamGoal;
		const nav::AreaId &roamGoalArea;
		const nav::AreaId &failedRoamGoalArea;
		const nav::NavVector &roamGoalPosition;
	};
	ConstRoamGoalState roamGoalState() const
	{
		return {hasObjectiveTarget_, objectiveTarget_, lastVisitedArea_, lastVisitedMapGeneration_, lastVisitedTeam_,
			roamGoalGeneration_, goalSelectionReason_, goalSelectionStrategy_, hasRoamGoal_, roamGoalArea_,
			failedRoamGoalArea_, roamGoalPosition_};
	}

	struct RouteState
	{
		nav::NavPathCostContext &pathCostContext;
		bool &hasPathCostContext;
		std::size_t &nextLinkIndex;
		nav::NavCorridor &activeCorridor;
		nav::NavDirectedLink &activeLink;
		nav::NavVector &activeTargetPosition;
		nav::NavVector &lastIntentDirection;
		std::size_t &activeCorridorIndex;
		std::uint32_t &pathSequence;
		bool &hasActiveRoute;
		bool &hasAvoidedLink;
		nav::NavDirectedLink &avoidedLink;
	};
	RouteState routeState()
	{
		return {pathCostContext_, hasPathCostContext_, nextLinkIndex_, activeCorridor_, activeLink_,
			activeTargetPosition_, lastIntentDirection_, activeCorridorIndex_, pathSequence_, hasActiveRoute_,
			hasAvoidedLink_, avoidedLink_};
	}

	struct ConstRouteState
	{
		const nav::NavPathCostContext &pathCostContext;
		const bool &hasPathCostContext;
		const std::size_t &nextLinkIndex;
		const nav::NavCorridor &activeCorridor;
		const nav::NavDirectedLink &activeLink;
		const nav::NavVector &activeTargetPosition;
		const nav::NavVector &lastIntentDirection;
		const std::size_t &activeCorridorIndex;
		const std::uint32_t &pathSequence;
		const bool &hasActiveRoute;
		const bool &hasAvoidedLink;
		const nav::NavDirectedLink &avoidedLink;
	};
	ConstRouteState routeState() const
	{
		return {pathCostContext_, hasPathCostContext_, nextLinkIndex_, activeCorridor_, activeLink_,
			activeTargetPosition_, lastIntentDirection_, activeCorridorIndex_, pathSequence_, hasActiveRoute_,
			hasAvoidedLink_, avoidedLink_};
	}

	struct TraversalState
	{
		nav::LocomotionController &locomotion;
		nav::TraversalAction &activeTraversal;
		nav::JumpDropController &jumpDrop;
		nav::SpecialTraversalController &specialTraversal;
		float &maximumSafeDropHeight;
		bool &safeDropHeightAvailable;
		bool &unsafeDropRejected;
	};
	TraversalState traversalState()
	{
		return {locomotion_, activeTraversal_, jumpDrop_, specialTraversal_, maximumSafeDropHeight_,
			safeDropHeightAvailable_, unsafeDropRejected_};
	}

	struct ConstTraversalState
	{
		const nav::LocomotionController &locomotion;
		const nav::TraversalAction &activeTraversal;
		const nav::JumpDropController &jumpDrop;
		const nav::SpecialTraversalController &specialTraversal;
		const float &maximumSafeDropHeight;
		const bool &safeDropHeightAvailable;
		const bool &unsafeDropRejected;
	};
	ConstTraversalState traversalState() const
	{
		return {locomotion_, activeTraversal_, jumpDrop_, specialTraversal_, maximumSafeDropHeight_,
			safeDropHeightAvailable_, unsafeDropRejected_};
	}

	struct FriendWaitState
	{
		bool &waitingBehindFriend;
		bool &friendInTheWay;
		float &friendCheckAt;
		float &friendWaitUntil;
		bool &friendRepathPending;
		nav::NavVector &friendPathTarget;
	};
	FriendWaitState friendWaitState()
	{
		return {waitingBehindFriend_, friendInTheWay_, friendCheckAt_, friendWaitUntil_, friendRepathPending_,
			friendPathTarget_};
	}

	struct ConstFriendWaitState
	{
		const bool &waitingBehindFriend;
		const bool &friendInTheWay;
		const float &friendCheckAt;
		const float &friendWaitUntil;
		const bool &friendRepathPending;
		const nav::NavVector &friendPathTarget;
	};
	ConstFriendWaitState friendWaitState() const
	{
		return {waitingBehindFriend_, friendInTheWay_, friendCheckAt_, friendWaitUntil_, friendRepathPending_,
			friendPathTarget_};
	}

	struct RecoveryState
	{
		OffMeshRecoveryState &offMeshRecovery;
		UnsafeDropFailure &unsafeDropFailure;
		float &nextPathAttemptAt;
		float &stuckRecoveryEndsAt;
		nav::NavVector &stuckOrigin;
		bool &stuckJumpScheduled;
		float &nextStuckJumpAt;
		float &lastWiggleJumpAt;
		nav::NavVector &stuckRecoveryDirection;
		std::uint32_t &stuckRecoveryCount;
		std::uint32_t &stuckRecoveryFramesRemaining;
		bool &stuckRecoveryActive;
		bool &hasPathFailure;
		PathFailureKey &pathFailureKey;
		std::uint32_t &pathFailureRetryFrame;
		std::uint32_t &pathFailureBackoffFrames;
	};
	RecoveryState recoveryState()
	{
		return {offMeshRecovery_, unsafeDropFailure_, nextPathAttemptAt_, stuckRecoveryEndsAt_, stuckOrigin_,
			stuckJumpScheduled_, nextStuckJumpAt_, lastWiggleJumpAt_, stuckRecoveryDirection_, stuckRecoveryCount_,
			stuckRecoveryFramesRemaining_, stuckRecoveryActive_, hasPathFailure_, pathFailureKey_,
			pathFailureRetryFrame_, pathFailureBackoffFrames_};
	}

	struct ConstRecoveryState
	{
		const OffMeshRecoveryState &offMeshRecovery;
		const UnsafeDropFailure &unsafeDropFailure;
		const float &nextPathAttemptAt;
		const float &stuckRecoveryEndsAt;
		const nav::NavVector &stuckOrigin;
		const bool &stuckJumpScheduled;
		const float &nextStuckJumpAt;
		const float &lastWiggleJumpAt;
		const nav::NavVector &stuckRecoveryDirection;
		const std::uint32_t &stuckRecoveryCount;
		const std::uint32_t &stuckRecoveryFramesRemaining;
		const bool &stuckRecoveryActive;
		const bool &hasPathFailure;
		const PathFailureKey &pathFailureKey;
		const std::uint32_t &pathFailureRetryFrame;
		const std::uint32_t &pathFailureBackoffFrames;
	};
	ConstRecoveryState recoveryState() const
	{
		return {offMeshRecovery_, unsafeDropFailure_, nextPathAttemptAt_, stuckRecoveryEndsAt_, stuckOrigin_,
			stuckJumpScheduled_, nextStuckJumpAt_, lastWiggleJumpAt_, stuckRecoveryDirection_, stuckRecoveryCount_,
			stuckRecoveryFramesRemaining_, stuckRecoveryActive_, hasPathFailure_, pathFailureKey_,
			pathFailureRetryFrame_, pathFailureBackoffFrames_};
	}
};
}
}

#endif
