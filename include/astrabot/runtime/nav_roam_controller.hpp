#ifndef ASTRABOT_RUNTIME_NAV_ROAM_CONTROLLER_HPP
#define ASTRABOT_RUNTIME_NAV_ROAM_CONTROLLER_HPP

#include "astrabot/nav/jump_drop.hpp"
#include "astrabot/nav/locomotion.hpp"
#include "astrabot/nav/special_traversal.hpp"
#include "astrabot/compat/runtime_mode_policy.hpp"
#include "astrabot/world/world_snapshot.hpp"
#include "astrabot/runtime/actor_registry.hpp"

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
			StaleFrame
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
	Failed
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
	MovementNotProduced
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
	bool goalPresent;
	bool pathRequested;
	nav::NavQueryResult pathResult;
	nav::AreaId goalArea;
	nav::NavVector goalPosition;
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

		class NavRoamController
		{
		  public:
	NavRoamController();
	explicit NavRoamController(compat::RuntimeMode mode);
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
					const nav::NavDirectedLink &link);
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
			void rememberRoute(
					const nav::NavSnapshot &snapshot,
					const nav::NavCorridor &corridor,
					const nav::NavDirectedLink &link);
			void populateRouteDecision(NavRoamDecision *decision) const;
			void resetRoute();

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
			bool initialized_;
		};
	}
}

#endif
