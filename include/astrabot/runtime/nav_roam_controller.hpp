#ifndef ASTRABOT_RUNTIME_NAV_ROAM_CONTROLLER_HPP
#define ASTRABOT_RUNTIME_NAV_ROAM_CONTROLLER_HPP

#include "astrabot/nav/jump_drop.hpp"
#include "astrabot/nav/locomotion.hpp"
#include "astrabot/nav/special_traversal.hpp"
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
	struct NavRoamObservation
	{
			ActorId actor;
			world::FrameIdentity frame;
			std::uint8_t team;
			nav::LocomotionObservation locomotion;
			bool hasObjectiveTarget;
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
		bool matchesNextUpdate(
			const ActorId &currentActor,
			const world::FrameIdentity &currentFrame,
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
		};

		class NavAreaVisitHistory
		{
		public:
			void record(
				std::uint32_t mapGeneration,
				std::uint8_t team,
				nav::AreaId area,
				std::uint32_t frame);
			std::uint32_t lastVisited(
				std::uint32_t mapGeneration,
				std::uint8_t team,
				nav::AreaId area) const;
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
		bool firstLinkReservedByOther(
			std::uint8_t team,
			ActorId actor,
			nav::AreaId fromArea,
			nav::AreaId toArea) const;
		bool reserve(
			std::uint8_t team,
			ActorId actor,
			nav::AreaId goalArea,
			nav::AreaId fromArea,
			nav::AreaId toArea);
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
	void setAreaVisitHistory(NavAreaVisitHistory *history);
	void setReservationBoard(NavRoamReservationBoard *board);
	void setRuntimeMode(compat::RuntimeMode mode);
			NavRoamResult update(
				const nav::NavSnapshot &snapshot,
				const NavRoamObservation &observation,
				nav::LocomotionIntent *intent);
			NavRoamResult update(
				const nav::NavSnapshot &snapshot,
				const NavRoamObservation &observation,
				nav::LocomotionIntent *intent,
				NavRoamDecision *decision);
			void reset();
			bool isActive() const;

	private:
	struct OffMeshRecoveryState
	{
		nav::AreaId area;
		nav::NavVector target;
		nav::NavVector blockedPosition;
		float bestDistance;
		std::uint32_t noProgressUpdates;
		std::array<nav::AreaId, 4U> failedAreas;
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
	void rememberUnsafeDrop(const NavRoamObservation &observation,
		nav::AreaId goalArea, const nav::NavDirectedLink &link);
	void delayUnsafeDropRetry();
	struct PathFailureKey
	{
		std::uint32_t mapGeneration;
		nav::AreaId startArea;
		nav::AreaId goalArea;
		nav::NavRouteType routeType;
	};

			static bool sameFrame(
				const world::FrameIdentity &left,
				const world::FrameIdentity &right);
			static bool sameActor(const ActorId &left, const ActorId &right);
			static bool isFrameAfter(
				const world::FrameIdentity &candidate,
				const world::FrameIdentity &current);
			static bool isValidObservation(const NavRoamObservation &observation);
		bool startTraversal(
			const nav::NavSnapshot &snapshot,
			const nav::NavCorridor &corridor,
			const nav::NavDirectedLink &link,
			nav::LocomotionResult *startResult = nullptr,
			const nav::NavVector *launchPosition = nullptr,
			const nav::NavVector *landingPosition = nullptr);
		bool startRoute(
			const nav::NavSnapshot &snapshot,
			const nav::NavCorridor &corridor,
			const nav::NavDirectedLink &link,
			nav::LocomotionResult *startResult = nullptr);
			bool buildStuckRecoveryIntent(
					const nav::NavAreaMatch &currentArea,
					nav::LocomotionIntent *intent);
			bool selectRoute(
					const nav::NavSnapshot &snapshot,
					const nav::NavAreaMatch &currentArea,
					nav::AreaId objectiveArea,
					NavRoamDecision *decision);
			bool selectRoamRoute(
					const nav::NavSnapshot &snapshot,
					const nav::NavAreaMatch &currentArea,
					NavRoamDecision *decision);
			bool selectCompatibilityGoal(
					const nav::NavSnapshot &snapshot,
					const nav::NavAreaMatch &currentArea,
					const std::vector<nav::NavDirectedLink> &links,
					NavRoamDecision *decision);
			void rememberRoute(
					const nav::NavSnapshot &snapshot,
					const nav::NavCorridor &corridor,
					const nav::NavDirectedLink &link);
			void populateRouteDecision(NavRoamDecision *decision) const;
		void resetRoute(bool preserveRoamGoal = false);
		bool isPathFailureBackedOff(
			std::uint32_t mapGeneration,
			nav::AreaId startArea,
			nav::AreaId goalArea,
			nav::NavRouteType routeType,
			std::uint32_t frame) const;
		void rememberPathFailure(
			std::uint32_t mapGeneration,
			nav::AreaId startArea,
			nav::AreaId goalArea,
			nav::NavRouteType routeType,
			std::uint32_t frame);
		void clearPathFailure();

			nav::LocomotionController locomotion_;
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
};
	}
}

#endif
