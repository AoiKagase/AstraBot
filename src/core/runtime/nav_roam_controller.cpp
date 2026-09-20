#include "astrabot/runtime/nav_roam_controller.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace astrabot
{
namespace runtime
{
namespace
{
	constexpr float kMaximumRecoveryDistance = 10000.0f;
	constexpr float kRoamSpeed = 32.0f;
	constexpr std::uint32_t kStuckRecoveryFrameLimit = 8U;
constexpr std::uint32_t kMaximumStuckRecoveryAttempts = 2U;
constexpr std::uint32_t kInitialPathFailureBackoffFrames = 15U;
constexpr std::uint32_t kMaximumPathFailureBackoffFrames = 120U;

	nav::LocomotionConfig roamLocomotionConfig()
	{
		return {32.0f, 20.0f, 64.0f, 16.0f, 100.0f, 8U};
	}

void initializeDecision(NavRoamDecision *decision)
	{
		if (decision == nullptr)
		{
			return;
		}
		*decision = {};
		decision->stage = NavRoamStage::None;
		decision->failureReason = NavFailureReason::None;
		decision->goalKind = NavGoalKind::None;
		decision->goalPresent = false;
		decision->pathRequested = false;
		decision->pathResult = nav::NavQueryResult::InvalidArgument;
		decision->goalArea = 0U;
		decision->goalPosition = {0.0f, 0.0f, 0.0f};
		decision->currentAreaResult = nav::NavQueryResult::InvalidArgument;
		decision->nearestAreaResult = nav::NavQueryResult::InvalidArgument;
		decision->linkResult = nav::NavQueryResult::InvalidArgument;
		decision->corridorResult = nav::NavQueryResult::InvalidArgument;
		decision->locomotionResult = nav::LocomotionResult::InvalidArgument;
	}

	bool buildRecoveryIntent(
		const nav::NavAreaMatch &area,
		const nav::LocomotionObservation &observation,
		nav::LocomotionIntent *intent)
	{
		if (intent == nullptr)
		{
			return false;
		}
		const nav::NavVector delta = {
			area.closestPoint.x - observation.position.x,
			area.closestPoint.y - observation.position.y,
			area.closestPoint.z - observation.position.z};
		const float length = std::sqrt(
			delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
		if (!std::isfinite(length) || length <= 0.0f)
		{
			return false;
		}
		intent->direction = {
			delta.x / length,
			delta.y / length,
			delta.z / length};
		intent->speed = kRoamSpeed;
		intent->posture = nav::LocomotionPosture::Standing;
		intent->stepUp = false;
		intent->currentArea = area.area;
		intent->targetArea = area.area;
		return true;
	}

	float floorHeight(const nav::NavArea &area)
	{
		return area.northEastZ * 0.5f + area.southWestZ * 0.5f;
	}

	nav::NavVector centerOf(const nav::NavArea &area)
	{
		return {
			area.extent.lo.x * 0.5f + area.extent.hi.x * 0.5f,
			area.extent.lo.y * 0.5f + area.extent.hi.y * 0.5f,
			floorHeight(area)};
	}

	nav::TraversalAction traversalActionFor(
		const nav::NavSnapshot &snapshot,
		const nav::NavDirectedLink &link)
	{
		if (link.how == 4U || link.how == 5U)
		{
			return nav::TraversalAction::Ladder;
		}
		if (link.how == 6U)
		{
			const nav::NavDocument *document = snapshot.document();
			if (document != nullptr)
			{
				const nav::NavArea *from = document->findArea(link.fromArea);
				const nav::NavArea *to = document->findArea(link.toArea);
				if (from != nullptr && to != nullptr && floorHeight(*to) < floorHeight(*from))
				{
					return nav::TraversalAction::Drop;
				}
			}
			return nav::TraversalAction::Jump;
		}
		return nav::TraversalAction::Walk;
	}
}

void addSearchStats(nav::NavSearchStats *total, const nav::NavSearchStats &sample)
{
	if (total == nullptr)
	{
		return;
	}
	total->expandedUniqueAreas += sample.expandedUniqueAreas;
	total->enqueueCount += sample.enqueueCount;
	total->reopenCount += sample.reopenCount;
	total->staleQueueEntries += sample.staleQueueEntries;
	total->equalCostReplacements += sample.equalCostReplacements;
	total->searchCalls += sample.searchCalls;
	total->successCount += sample.successCount;
	total->failureCount += sample.failureCount;
	total->totalUsec += sample.totalUsec;
	if (sample.maxUsec > total->maxUsec)
	{
		total->maxUsec = sample.maxUsec;
	}
	if (sample.firstSearchId != 0U)
	{
		if (total->firstSearchId == 0U)
		{
			total->firstSearchId = sample.firstSearchId;
			total->firstStartArea = sample.firstStartArea;
			total->firstGoalArea = sample.firstGoalArea;
			total->firstRouteType = sample.firstRouteType;
		}
		total->lastSearchId = sample.lastSearchId;
		total->lastStartArea = sample.lastStartArea;
		total->lastGoalArea = sample.lastGoalArea;
		total->lastRouteType = sample.lastRouteType;
	}
}

nav::NavQueryResult buildCorridor(
	const nav::NavQuery &query,
	nav::AreaId start,
	nav::AreaId goal,
	nav::NavCorridor *corridor,
	nav::NavSearchStats *stats)
{
	nav::NavSearchStats sample = {};
	const nav::NavQueryResult result = query.buildCorridor(
		start, goal, corridor, stats == nullptr ? nullptr : &sample);
	if (stats != nullptr)
	{
		if (result == nav::NavQueryResult::Found)
		{
			++sample.successCount;
		}
		else
		{
			++sample.failureCount;
		}
	}
	addSearchStats(stats, sample);
	return result;
}

nav::NavQueryResult buildCorridor(
	const nav::NavQuery &query,
	nav::AreaId start,
	nav::AreaId goal,
	nav::NavRouteType routeType,
	nav::NavCorridor *corridor,
	nav::NavSearchStats *stats)
{
	nav::NavSearchStats sample = {};
	const nav::NavQueryResult result = query.buildCorridor(
		start, goal, routeType, corridor, stats == nullptr ? nullptr : &sample);
	if (stats != nullptr)
	{
		if (result == nav::NavQueryResult::Found)
		{
			++sample.successCount;
		}
		else
		{
			++sample.failureCount;
		}
	}
	addSearchStats(stats, sample);
	return result;
}

NavRoamController::NavRoamController()
	: NavRoamController(compat::RuntimeMode::Compatibility)
{
}

NavRoamController::NavRoamController(compat::RuntimeMode mode)
	: locomotion_(roamLocomotionConfig()),
	  activeTraversal_(nav::TraversalAction::Walk),
	  jumpDrop_(),
	  specialTraversal_(),
	  modePolicy_(mode),
	  actor_{0U, LifecycleSession::kInvalidGeneration},
	  lastFrame_(),
	  nextLinkIndex_(0U),
	  activeCorridor_(),
	  activeLink_(),
	  activeTargetPosition_{0.0f, 0.0f, 0.0f},
	  lastIntentDirection_{0.0f, 0.0f, 0.0f},
	  stuckRecoveryDirection_{0.0f, 0.0f, 0.0f},
	  activeCorridorIndex_(0U),
	  pathSequence_(0U),
	  stuckRecoveryCount_(0U),
	  stuckRecoveryFramesRemaining_(0U),
	  stuckRecoveryActive_(false),
	  hasActiveRoute_(false),
	  hasAvoidedLink_(false),
	  avoidedLink_{0U, 0U, 0U, 0U},
	  hasObjectiveTarget_(false),
	  objectiveTarget_{0.0f, 0.0f, 0.0f},
	  hasPathFailure_(false),
	  pathFailureKey_{0U, 0U, 0U, nav::NavRouteType::Fastest},
	  pathFailureRetryFrame_(0U),
	  pathFailureBackoffFrames_(kInitialPathFailureBackoffFrames),
	  collectPathStats_(false),
	  initialized_(false)
{
}

void NavRoamController::setRuntimeMode(compat::RuntimeMode mode)
{
	modePolicy_ = compat::RuntimeModePolicy(mode);
}

bool NavRoamController::isPathFailureBackedOff(
	std::uint32_t mapGeneration,
	nav::AreaId startArea,
	nav::AreaId goalArea,
	nav::NavRouteType routeType,
	std::uint32_t frame) const
{
	return hasPathFailure_ &&
		pathFailureKey_.mapGeneration == mapGeneration &&
		pathFailureKey_.startArea == startArea &&
		pathFailureKey_.goalArea == goalArea &&
		pathFailureKey_.routeType == routeType &&
		frame < pathFailureRetryFrame_;
}

void NavRoamController::rememberPathFailure(
	std::uint32_t mapGeneration,
	nav::AreaId startArea,
	nav::AreaId goalArea,
	nav::NavRouteType routeType,
	std::uint32_t frame)
{
	const PathFailureKey key = {mapGeneration, startArea, goalArea, routeType};
	if (!hasPathFailure_ || pathFailureKey_.mapGeneration != key.mapGeneration ||
		pathFailureKey_.startArea != key.startArea ||
		pathFailureKey_.goalArea != key.goalArea ||
		pathFailureKey_.routeType != key.routeType)
	{
		pathFailureBackoffFrames_ = kInitialPathFailureBackoffFrames;
	}
	else
	{
		pathFailureBackoffFrames_ = (std::min)(
			kMaximumPathFailureBackoffFrames,
			pathFailureBackoffFrames_ * 2U);
	}
	pathFailureKey_ = key;
	hasPathFailure_ = true;
	pathFailureRetryFrame_ = frame + pathFailureBackoffFrames_;
}

void NavRoamController::clearPathFailure()
{
	hasPathFailure_ = false;
	pathFailureKey_ = {0U, 0U, 0U, nav::NavRouteType::Fastest};
	pathFailureRetryFrame_ = 0U;
	pathFailureBackoffFrames_ = kInitialPathFailureBackoffFrames;
}

NavRoamResult NavRoamController::update(
	const nav::NavSnapshot &snapshot,
	const NavRoamObservation &observation,
	nav::LocomotionIntent *intent)

{
	return update(snapshot, observation, intent, nullptr);
}

NavRoamResult NavRoamController::update(
	const nav::NavSnapshot &snapshot,
	const NavRoamObservation &observation,
	nav::LocomotionIntent *intent,
	NavRoamDecision *decision)
{
	initializeDecision(decision);
	if (intent == nullptr)
	{
		if (decision != nullptr)
		{
			decision->stage = NavRoamStage::Failed;
		}
		return NavRoamResult::InvalidArgument;
	}
	*intent = {};
	if (!snapshot.isValid())
	{
		return NavRoamResult::InvalidSnapshot;
	}
	if (!isValidObservation(observation))
	{
		return NavRoamResult::InvalidObservation;
	}
	collectPathStats_ = observation.collectPathStats;
	if (observation.frame.mapGeneration != snapshot.mapGeneration())
	{
		return NavRoamResult::InvalidSnapshot;
	}
	if (decision != nullptr)
	{
		decision->fullUpdateSequence = observation.frame.tick;
	}

	if (!initialized_)
	{
		actor_ = observation.actor;
		nextLinkIndex_ = static_cast<std::size_t>(observation.actor.slot) +
			static_cast<std::size_t>(observation.actor.actorGeneration - 1U) * 17U;
		initialized_ = true;
	}
	else if (!sameActor(actor_, observation.actor))
	{
		if (actor_.slot != observation.actor.slot ||
				observation.actor.actorGeneration <= actor_.actorGeneration)
		{
			return NavRoamResult::StaleActor;
		}
		reset();
		actor_ = observation.actor;
		nextLinkIndex_ = static_cast<std::size_t>(observation.actor.slot) +
			static_cast<std::size_t>(observation.actor.actorGeneration - 1U) * 17U;
		initialized_ = true;
	}

	if (lastFrame_.isValid())
	{
		if (sameFrame(observation.frame, lastFrame_))
		{
			return NavRoamResult::DuplicateFrame;
		}
		if (!isFrameAfter(observation.frame, lastFrame_))
		{
			return NavRoamResult::StaleFrame;
		}
		if (observation.frame.mapGeneration != lastFrame_.mapGeneration ||
				observation.frame.roundGeneration != lastFrame_.roundGeneration)
		{
			clearPathFailure();
			if (decision != nullptr)
		{
			decision->recomputeReason = NavRecomputeReason::MapOrRoundChanged;
		}
		resetRoute();
		}
	}
	const bool objectiveTargetChanged =
		hasObjectiveTarget_ != observation.hasObjectiveTarget ||
		(observation.hasObjectiveTarget &&
			(std::fabs(objectiveTarget_.x - observation.objectiveTarget.x) > 0.5f ||
			 std::fabs(objectiveTarget_.y - observation.objectiveTarget.y) > 0.5f ||
			 std::fabs(objectiveTarget_.z - observation.objectiveTarget.z) > 0.5f));
	if (objectiveTargetChanged)
	{
		clearPathFailure();
		if (decision != nullptr)
		{
			decision->recomputeReason = lastFrame_.isValid()
					? NavRecomputeReason::GoalChanged
					: NavRecomputeReason::InitialGoal;
		}
		resetRoute();
	}
	hasObjectiveTarget_ = observation.hasObjectiveTarget;
	objectiveTarget_ = observation.objectiveTarget;
	lastFrame_ = observation.frame;
	if (decision != nullptr)
	{
		decision->observationPosition = observation.locomotion.position;
		decision->observationVelocity = observation.locomotion.velocity;
		decision->intentDirection = lastIntentDirection_;
		populateRouteDecision(decision);
	}

	nav::NavQuery query(snapshot);
	nav::NavAreaMatch currentArea = {};
	nav::NavQueryResult currentAreaResult = query.findContaining(
			{observation.locomotion.position.x,
			 observation.locomotion.position.y,
			 observation.locomotion.position.z},
			64.0f,
			&currentArea);
	if (decision != nullptr)
	{
		decision->currentAreaResult = currentAreaResult;
	}
	nav::AreaId objectiveArea = 0U;
	if (observation.hasObjectiveTarget)
	{
		nav::NavAreaMatch objectiveMatch = {};
		if (query.findNearest(
				observation.objectiveTarget,
				kMaximumRecoveryDistance,
				&objectiveMatch) == nav::NavQueryResult::Found)
		{
			objectiveArea = objectiveMatch.area;
			if (decision != nullptr)
			{
				decision->goalPresent = true;
				decision->goalKind = NavGoalKind::Objective;
				decision->goalArea = objectiveArea;
				decision->goalPosition = observation.objectiveTarget;
			}
		}
		else if (decision != nullptr)
		{
			decision->failureReason = NavFailureReason::GoalAreaMissing;
		}
	}
	if (currentAreaResult == nav::NavQueryResult::NoAreaContaining &&
			!jumpDrop_.isActive() && !specialTraversal_.isActive())
	{
		const nav::NavQueryResult nearestResult = query.findNearest(
				{observation.locomotion.position.x,
				 observation.locomotion.position.y,
				 observation.locomotion.position.z},
			kMaximumRecoveryDistance,
			&currentArea);
		if (decision != nullptr)
		{
			decision->nearestAreaResult = nearestResult;
			decision->recoveryArea = nearestResult == nav::NavQueryResult::Found ?
				currentArea.area : 0U;
			decision->nearestDistanceSquared = currentArea.distanceSquared;
		}
		if (nearestResult != nav::NavQueryResult::Found ||
			!buildRecoveryIntent(currentArea, observation.locomotion, intent))
		{
			if (!hasActiveRoute_)
			{
				resetRoute();
			}
			if (decision != nullptr)
			{
				decision->failureReason = NavFailureReason::CurrentAreaMissing;
				decision->stage = NavRoamStage::Failed;
			}
			return NavRoamResult::NoRoute;
		}
		if (decision != nullptr)
		{
			decision->failureReason = NavFailureReason::CurrentAreaMissing;
			decision->stage = NavRoamStage::OffMeshRecovery;
			decision->targetArea = currentArea.area;
		}
		return NavRoamResult::IntentReady;
	}
	if (currentAreaResult != nav::NavQueryResult::Found &&
			!jumpDrop_.isActive() && !specialTraversal_.isActive())
	{
		if (!hasActiveRoute_)
		{
			resetRoute();
		}
		if (decision != nullptr)
		{
			decision->failureReason = NavFailureReason::CurrentAreaMissing;
			decision->stage = NavRoamStage::Failed;
		}
		return NavRoamResult::NoRoute;
	}
	if (decision != nullptr)
	{
		decision->stage = NavRoamStage::ExactArea;
		decision->currentArea = currentArea.area;
	}
	if (observation.hasObjectiveTarget && objectiveArea == 0U)
	{
		if (!hasActiveRoute_)
		{
			resetRoute();
		}
		if (decision != nullptr)
		{
			decision->failureReason = NavFailureReason::GoalAreaMissing;
			decision->stage = NavRoamStage::Failed;
		}
		return NavRoamResult::NoRoute;
	}

	if (stuckRecoveryActive_)
	{
		if (stuckRecoveryFramesRemaining_ != 0U)
		{
			if (!buildStuckRecoveryIntent(currentArea, intent))
			{
				resetRoute();
				if (decision != nullptr)
				{
					decision->stage = NavRoamStage::Failed;
				}
				return NavRoamResult::ReplanRequired;
			}
			--stuckRecoveryFramesRemaining_;
			if (decision != nullptr)
			{
				decision->locomotionResult = nav::LocomotionResult::Stuck;
				decision->stage = NavRoamStage::LocomotionReady;
				decision->intentDirection = intent->direction;
			}
			return NavRoamResult::IntentReady;
		}
		if (locomotion_.start(activeCorridor_) != nav::LocomotionResult::Started)
		{
			resetRoute();
			if (decision != nullptr)
			{
				decision->stage = NavRoamStage::Failed;
			}
			return NavRoamResult::ReplanRequired;
		}
		stuckRecoveryActive_ = false;
	}

	if (!locomotion_.isActive() && !jumpDrop_.isActive() && !specialTraversal_.isActive())
	{
		if (decision != nullptr)
		{
			decision->stage = NavRoamStage::LinkSelection;
		}
		if (objectiveArea == currentArea.area && observation.hasObjectiveTarget)
		{
			const float deltaX = observation.objectiveTarget.x -
				observation.locomotion.position.x;
			const float deltaY = observation.objectiveTarget.y -
				observation.locomotion.position.y;
			const float distance = std::hypot(deltaX, deltaY);
			if (std::isfinite(distance) && distance > 8.0f)
			{
				intent->direction = {deltaX / distance, deltaY / distance, 0.0f};
				intent->speed = kRoamSpeed;
				intent->posture = nav::LocomotionPosture::Standing;
				intent->traversal = nav::TraversalAction::Walk;
				intent->stepUp = false;
				intent->currentArea = currentArea.area;
				intent->targetArea = currentArea.area;
				lastIntentDirection_ = intent->direction;
				if (decision != nullptr)
				{
					decision->stage = NavRoamStage::LocomotionReady;
					decision->targetArea = currentArea.area;
					decision->targetPosition = observation.objectiveTarget;
					decision->intentDirection = intent->direction;
				}
				return NavRoamResult::IntentReady;
			}
		}
		const nav::NavRouteType routeType = nav::NavRouteType::Fastest;
		if (objectiveArea != 0U && isPathFailureBackedOff(
				observation.frame.mapGeneration,
				currentArea.area,
				objectiveArea,
				routeType,
				observation.frame.tick))
		{
			if (decision != nullptr)
			{
				decision->goalPresent = true;
				decision->goalKind = NavGoalKind::Objective;
				decision->goalArea = objectiveArea;
				decision->pathRequested = true;
				decision->pathResult = nav::NavQueryResult::ResourceLimit;
				decision->failureReason = NavFailureReason::PathSearchFailed;
				decision->stage = NavRoamStage::Failed;
			}
			return NavRoamResult::NoRoute;
		}
	if (!hasActiveRoute_ && decision != nullptr &&
			decision->recomputeReason == NavRecomputeReason::None)
	{
		decision->recomputeReason = NavRecomputeReason::InitialGoal;
	}
	if (!selectRoute(snapshot, currentArea, objectiveArea, decision))
		{
			if (objectiveArea != 0U && decision != nullptr &&
				decision->pathResult == nav::NavQueryResult::ResourceLimit)
			{
				rememberPathFailure(
					observation.frame.mapGeneration,
					currentArea.area,
					objectiveArea,
					routeType,
					observation.frame.tick);
			}
			if (decision != nullptr && decision->failureReason == NavFailureReason::None)
			{
				decision->failureReason = decision->pathRequested
					? NavFailureReason::PathSearchFailed
					: NavFailureReason::NoGoal;
			}
			resetRoute();
			if (decision != nullptr)
			{
				decision->stage = NavRoamStage::Failed;
			}
			return NavRoamResult::NoRoute;
		}
		if (decision != nullptr)
		{
			decision->stage = NavRoamStage::CorridorReady;
		}
		if (objectiveArea != 0U)
		{
			clearPathFailure();
		}
	}

	if (jumpDrop_.isActive())
	{
		nav::JumpDropObservation traversalObservation = {};
		traversalObservation.position = observation.locomotion.position;
		traversalObservation.frame = observation.frame.tick;
		traversalObservation.actorGeneration = observation.actor.actorGeneration;
		traversalObservation.airborne = observation.airborne;
		traversalObservation.landingConfirmed = observation.landingConfirmed;
		traversalObservation.hasLandingDamage = observation.hasLandingDamage;
		traversalObservation.landingDamage = observation.landingDamage;
		nav::JumpDropIntent traversalIntent = {};
		const nav::JumpDropResult traversalResult =
				jumpDrop_.update(snapshot, traversalObservation, &traversalIntent);
		if (traversalResult == nav::JumpDropResult::Emitted)
		{
			intent->direction = traversalIntent.direction;
			intent->speed = traversalIntent.speed;
			intent->posture = nav::LocomotionPosture::Standing;
			intent->traversal = activeTraversal_;
			intent->stepUp = false;
			intent->currentArea = traversalIntent.launchArea;
			intent->targetArea = traversalIntent.landingArea;
			if (decision != nullptr)
			{
				decision->locomotionResult = nav::LocomotionResult::IntentReady;
				decision->stage = NavRoamStage::LocomotionReady;
				decision->targetArea = traversalIntent.landingArea;
			}
			return NavRoamResult::IntentReady;
		}
		if (traversalResult == nav::JumpDropResult::Landed)
		{
			resetRoute();
			if (decision != nullptr)
			{
				decision->locomotionResult = nav::LocomotionResult::TargetReached;
				decision->stage = NavRoamStage::TargetReached;
			}
			return NavRoamResult::TargetReached;
		}
		resetRoute();
		if (decision != nullptr)
		{
			decision->stage = NavRoamStage::Failed;
		}
		return NavRoamResult::ReplanRequired;
	}

	if (specialTraversal_.isActive())
	{
		nav::SpecialTraversalObservation traversalObservation = {};
		traversalObservation.position = observation.locomotion.position;
		traversalObservation.frame = observation.frame.tick;
		traversalObservation.actorGeneration = observation.actor.actorGeneration;
		traversalObservation.standingClearance = observation.locomotion.standingClearance;
		traversalObservation.crouchingClearance = observation.locomotion.crouchingClearance;
		traversalObservation.availability = nav::SpecialTraversalAvailability::Open;
		traversalObservation.ladderContact = observation.ladderContact;
		traversalObservation.entryConfirmed = observation.entryConfirmed;
		traversalObservation.exitConfirmed = observation.exitConfirmed;
		nav::SpecialTraversalIntent traversalIntent = {};
		const nav::SpecialTraversalResult traversalResult =
				specialTraversal_.update(snapshot, traversalObservation, &traversalIntent);
		if (traversalResult == nav::SpecialTraversalResult::EnterIntent ||
				traversalResult == nav::SpecialTraversalResult::MaintainIntent ||
				traversalResult == nav::SpecialTraversalResult::ExitIntent)
		{
			intent->direction = traversalIntent.direction;
			intent->speed = traversalIntent.speed;
			intent->posture = traversalIntent.posture == nav::SpecialTraversalPosture::Crouching
					? nav::LocomotionPosture::Crouching
					: nav::LocomotionPosture::Standing;
			intent->traversal = activeTraversal_;
			intent->stepUp = false;
			intent->currentArea = traversalIntent.entryArea;
			intent->targetArea = traversalIntent.exitArea;
			if (decision != nullptr)
			{
				decision->locomotionResult = nav::LocomotionResult::IntentReady;
				decision->stage = NavRoamStage::LocomotionReady;
				decision->targetArea = traversalIntent.exitArea;
			}
			return NavRoamResult::IntentReady;
		}
		if (traversalResult == nav::SpecialTraversalResult::Completed)
		{
			resetRoute();
			if (decision != nullptr)
			{
				decision->locomotionResult = nav::LocomotionResult::TargetReached;
				decision->stage = NavRoamStage::TargetReached;
			}
			return NavRoamResult::TargetReached;
		}
		resetRoute();
		if (decision != nullptr)
		{
			decision->stage = NavRoamStage::Failed;
		}
		return NavRoamResult::ReplanRequired;
	}

	const nav::LocomotionResult locomotionResult = locomotion_.update(
		snapshot,
		observation.locomotion,
		intent);
	if (hasActiveRoute_)
	{
		activeCorridorIndex_ = locomotion_.currentCorridorIndex();
		if (!activeCorridor_.areas.empty() &&
				activeCorridorIndex_ >= activeCorridor_.areas.size())
		{
			activeCorridorIndex_ = activeCorridor_.areas.size() - 1U;
		}
	}
	if (decision != nullptr)
	{
		decision->locomotionResult = locomotionResult;
		if (intent->targetArea != 0U)
		{
			decision->targetArea = intent->targetArea;
		}
		if (locomotionResult == nav::LocomotionResult::IntentReady)
		{
			decision->intentDirection = intent->direction;
		}
	}
	if (locomotionResult == nav::LocomotionResult::IntentReady)
	{
		lastIntentDirection_ = intent->direction;
	}
	if (locomotionResult == nav::LocomotionResult::IntentReady &&
			activeTraversal_ != nav::TraversalAction::Walk)
	{
		intent->traversal = activeTraversal_;
	}
	switch (locomotionResult)
	{
	case nav::LocomotionResult::Started:
		resetRoute();
		if (decision != nullptr)
		{
			decision->stage = NavRoamStage::Failed;
		}
		return NavRoamResult::ReplanRequired;
	case nav::LocomotionResult::IntentReady:
		if (decision != nullptr)
		{
			decision->stage = NavRoamStage::LocomotionReady;
		}
		return NavRoamResult::IntentReady;
	case nav::LocomotionResult::TargetReached:
		resetRoute();
		if (decision != nullptr)
		{
			decision->stage = NavRoamStage::TargetReached;
		}
		return NavRoamResult::TargetReached;
	case nav::LocomotionResult::NeedsRecovery:
	case nav::LocomotionResult::StepTooHigh:
	case nav::LocomotionResult::Stuck:
	case nav::LocomotionResult::Inactive:
		if (locomotionResult == nav::LocomotionResult::Stuck && hasActiveRoute_)
		{
			if (!stuckRecoveryActive_ &&
					stuckRecoveryCount_ < kMaximumStuckRecoveryAttempts)
			{
				stuckRecoveryActive_ = false;
				stuckRecoveryFramesRemaining_ = kStuckRecoveryFrameLimit;
				++stuckRecoveryCount_;
				if (buildStuckRecoveryIntent(currentArea, intent))
				{
					stuckRecoveryActive_ = true;
					--stuckRecoveryFramesRemaining_;
					if (decision != nullptr)
					{
						decision->locomotionResult = nav::LocomotionResult::Stuck;
						decision->stage = NavRoamStage::LocomotionReady;
						decision->intentDirection = intent->direction;
					}
					return NavRoamResult::IntentReady;
				}
				stuckRecoveryActive_ = false;
				stuckRecoveryFramesRemaining_ = 0U;
			}
			hasAvoidedLink_ = true;
			avoidedLink_ = activeLink_;
			++nextLinkIndex_;
			resetRoute();
			if (decision != nullptr)
			{
				decision->stage = NavRoamStage::Failed;
			}
			return NavRoamResult::ReplanRequired;
		}
		resetRoute();
		if (decision != nullptr)
		{
			decision->stage = NavRoamStage::Failed;
		}
		return NavRoamResult::ReplanRequired;
	case nav::LocomotionResult::InvalidSnapshot:
	case nav::LocomotionResult::StaleSnapshot:
		resetRoute();
		if (decision != nullptr)
		{
			decision->stage = NavRoamStage::Failed;
		}
		return NavRoamResult::InvalidSnapshot;
	case nav::LocomotionResult::InvalidArgument:
	case nav::LocomotionResult::InvalidConfig:
	case nav::LocomotionResult::InvalidCorridor:
	case nav::LocomotionResult::InvalidObservation:
	case nav::LocomotionResult::InvalidClearance:
	case nav::LocomotionResult::ResourceLimit:
		resetRoute();
		if (decision != nullptr)
		{
			decision->stage = NavRoamStage::Failed;
		}
		return NavRoamResult::InvalidObservation;
	}

	resetRoute();
	if (decision != nullptr)
	{
		decision->stage = NavRoamStage::Failed;
	}
	return NavRoamResult::InvalidObservation;
}

void NavRoamController::reset()
{
	resetRoute();
	actor_ = {0U, LifecycleSession::kInvalidGeneration};
	lastFrame_ = {};
	nextLinkIndex_ = 0U;
	pathSequence_ = 0U;
	hasAvoidedLink_ = false;
	avoidedLink_ = {0U, 0U, 0U, 0U};
	hasObjectiveTarget_ = false;
	objectiveTarget_ = {0.0f, 0.0f, 0.0f};
	clearPathFailure();
	initialized_ = false;
}

bool NavRoamController::isActive() const
{
	return locomotion_.isActive() || jumpDrop_.isActive() ||
		specialTraversal_.isActive() || stuckRecoveryActive_;
}

bool NavRoamController::sameFrame(
	const world::FrameIdentity &left,
	const world::FrameIdentity &right)
{
	return left == right;
}

bool NavRoamController::sameActor(const ActorId &left, const ActorId &right)
{
	return left.slot == right.slot &&
		left.actorGeneration == right.actorGeneration;
}

bool NavRoamController::isFrameAfter(
	const world::FrameIdentity &candidate,
	const world::FrameIdentity &current)
{
	if (candidate.mapGeneration != current.mapGeneration)
	{
		return candidate.mapGeneration > current.mapGeneration;
	}
	if (candidate.roundGeneration != current.roundGeneration)
	{
		return candidate.roundGeneration > current.roundGeneration;
	}
	return candidate.tick > current.tick;
}

bool NavRoamController::isValidObservation(
	const NavRoamObservation &observation)
{
	return observation.actor.slot >= LifecycleSession::kFirstClientSlot &&
		observation.actor.slot <= LifecycleSession::kLastClientSlot &&
		observation.actor.actorGeneration != LifecycleSession::kInvalidGeneration &&
		observation.frame.isValid() &&
		std::isfinite(observation.locomotion.position.x) &&
		std::isfinite(observation.locomotion.position.y) &&
		std::isfinite(observation.locomotion.position.z) &&
		std::isfinite(observation.locomotion.velocity.x) &&
		std::isfinite(observation.locomotion.velocity.y) &&
		std::isfinite(observation.locomotion.velocity.z) &&
		std::isfinite(observation.landingDamage) &&
		(!observation.hasObjectiveTarget ||
			(std::isfinite(observation.objectiveTarget.x) &&
			 std::isfinite(observation.objectiveTarget.y) &&
			 std::isfinite(observation.objectiveTarget.z)));
}

bool NavRoamController::startTraversal(
	const nav::NavSnapshot &snapshot,
	const nav::NavCorridor &corridor,
	const nav::NavDirectedLink &link)
{
	const nav::NavDocument *document = snapshot.document();
	if (document == nullptr || corridor.areas.size() < 2U)
	{
		return false;
	}
	const nav::NavArea *fromArea = document->findArea(link.fromArea);
	const nav::NavArea *toArea = document->findArea(link.toArea);
	if (fromArea == nullptr || toArea == nullptr)
	{
		return false;
	}
	activeTraversal_ = traversalActionFor(snapshot, link);
	const nav::NavVector launchPosition = centerOf(*fromArea);
	const nav::NavVector landingPosition = centerOf(*toArea);
	const float verticalDelta = floorHeight(*toArea) - floorHeight(*fromArea);
	const float horizontalDeltaX = landingPosition.x - launchPosition.x;
	const float horizontalDeltaY = landingPosition.y - launchPosition.y;
	const float horizontalReach = std::sqrt(
		horizontalDeltaX * horizontalDeltaX + horizontalDeltaY * horizontalDeltaY);

	if (activeTraversal_ == nav::TraversalAction::Jump ||
			activeTraversal_ == nav::TraversalAction::Drop)
	{
		nav::JumpDropEnvelope envelope = {};
		envelope.kind = activeTraversal_ == nav::TraversalAction::Jump
				? nav::JumpDropKind::Jump
				: nav::JumpDropKind::Drop;
		envelope.launch = {launchPosition, link.fromArea};
		envelope.landing = {landingPosition, link.toArea};
		envelope.maximumRise = (std::max)(16.0f, verticalDelta + 16.0f);
		envelope.maximumDrop = (std::max)(16.0f, -verticalDelta + 16.0f);
		envelope.horizontalReach = (std::max)(16.0f, horizontalReach + 16.0f);
		envelope.damageRisk = {4096.0f, 100.0f};
		return jumpDrop_.start(corridor, envelope, actor_.actorGeneration) ==
				nav::JumpDropResult::Ready;
	}

	if (activeTraversal_ == nav::TraversalAction::Ladder)
	{
		nav::SpecialTraversalCapability capability = {};
		capability.kind = nav::SpecialTraversalKind::Ladder;
		capability.entry = {launchPosition, link.fromArea};
		capability.exit = {landingPosition, link.toArea};
		capability.sourceDirection = link.direction;
		capability.requiredClearance = 36.0f;
		capability.minimumPosture = nav::SpecialTraversalPosture::Standing;
		return specialTraversal_.start(corridor, capability, actor_.actorGeneration) ==
				nav::SpecialTraversalResult::Ready;
	}

	return locomotion_.start(corridor) == nav::LocomotionResult::Started;
}

bool NavRoamController::buildStuckRecoveryIntent(
	const nav::NavAreaMatch &currentArea,
	nav::LocomotionIntent *intent)
{
	if (intent == nullptr || !hasActiveRoute_)
	{
		return false;
	}

	float baseX = lastIntentDirection_.x;
	float baseY = lastIntentDirection_.y;
	const float baseLength = std::hypot(baseX, baseY);
	if (!std::isfinite(baseLength) || baseLength <= 0.0f)
	{
		baseX = activeTargetPosition_.x - currentArea.closestPoint.x;
		baseY = activeTargetPosition_.y - currentArea.closestPoint.y;
	}
	const float length = std::hypot(baseX, baseY);
	if (!std::isfinite(length) || length <= 0.0f)
	{
		return false;
	}

	if (!stuckRecoveryActive_)
	{
		const float sign = (stuckRecoveryCount_ % 2U) == 0U ? 1.0f : -1.0f;
		stuckRecoveryDirection_ = {
			-baseY / length * sign,
			baseX / length * sign,
			0.0f};
	}
	intent->direction = stuckRecoveryDirection_;
	intent->speed = kRoamSpeed;
	intent->posture = nav::LocomotionPosture::Standing;
	intent->traversal = nav::TraversalAction::Walk;
	intent->stepUp = false;
	intent->currentArea = currentArea.area;
	intent->targetArea = activeLink_.toArea;
	return true;
}

bool NavRoamController::selectRoute(
	const nav::NavSnapshot &snapshot,
	const nav::NavAreaMatch &currentArea,
	nav::AreaId objectiveArea,
	NavRoamDecision *decision)
{
	if (objectiveArea == 0U)
	{
		return selectRoamRoute(snapshot, currentArea, decision);
	}
	nav::NavQuery query(snapshot);
	std::vector<nav::NavDirectedLink> links;
	const nav::NavQueryResult linksResult = query.outgoingLinks(currentArea.area, &links);
	if (decision != nullptr)
	{
		decision->linkResult = linksResult;
	}
	if (linksResult != nav::NavQueryResult::Found)
	{
		return false;
	}

	if (objectiveArea != 0U && objectiveArea != currentArea.area)
	{
		if (decision != nullptr)
		{
			decision->goalPresent = true;
			decision->goalKind = NavGoalKind::Objective;
			decision->goalArea = objectiveArea;
			decision->goalPosition = objectiveTarget_;
			decision->pathRequested = true;
		}
		nav::NavCorridor corridor = {};
		const nav::NavQueryResult corridorResult = buildCorridor(
			query, currentArea.area, objectiveArea, &corridor,
			decision == nullptr || !collectPathStats_ ? nullptr : &decision->pathSearchStats);
		if (decision != nullptr)
		{
			decision->corridorResult = corridorResult;
			decision->pathResult = corridorResult;
		}
		if (corridorResult == nav::NavQueryResult::Found && corridor.areas.size() >= 2U)
		{
			const nav::AreaId nextArea = corridor.areas[1U];
			nav::NavDirectedLink link = {currentArea.area, nextArea, 0U, 0U};
			for (const nav::NavDirectedLink &candidate : links)
			{
				if (candidate.toArea == nextArea)
				{
					link = candidate;
					break;
				}
			}
			if (startTraversal(snapshot, corridor, link))
			{
				nextLinkIndex_ = 0U;
				rememberRoute(snapshot, corridor, link);
				if (decision != nullptr)
				{
					populateRouteDecision(decision);
					decision->targetArea = objectiveArea;
					decision->targetPosition = objectiveTarget_;
				}
				return true;
			}
		}
		return false;
	}

	if (!links.empty())
	{
		const std::size_t firstIndex = nextLinkIndex_ % links.size();
		for (std::size_t offset = 0U; offset < links.size(); ++offset)
		{
			const std::size_t linkIndex = (firstIndex + offset) % links.size();
			if (hasAvoidedLink_ &&
					links[linkIndex].fromArea == avoidedLink_.fromArea &&
					links[linkIndex].toArea == avoidedLink_.toArea)
			{
				continue;
			}
			nav::NavCorridor corridor = {};
			const nav::NavQueryResult corridorResult = buildCorridor(
				query,
				currentArea.area,
				links[linkIndex].toArea,
				&corridor,
				decision == nullptr || !collectPathStats_ ? nullptr : &decision->pathSearchStats);
			if (decision != nullptr)
			{
				decision->corridorResult = corridorResult;
			}
			if (links[linkIndex].toArea == currentArea.area ||
					corridorResult != nav::NavQueryResult::Found)
			{
				continue;
			}
		if (!startTraversal(snapshot, corridor, links[linkIndex]))
		{
			continue;
		}
		activeTraversal_ = traversalActionFor(snapshot, links[linkIndex]);
		nextLinkIndex_ = linkIndex + 1U;
		rememberRoute(snapshot, corridor, links[linkIndex]);
			if (decision != nullptr)
			{
				decision->targetArea = links[linkIndex].toArea;
				populateRouteDecision(decision);
			}
			hasAvoidedLink_ = false;
			return true;
		}
	}

	const nav::NavDocument *document = snapshot.document();
	if (document == nullptr)
	{
		return false;
	}
	std::size_t candidateCount = 0U;
	for (const nav::NavArea &candidate : document->areas())
	{
		if (candidate.id == currentArea.area || candidateCount >= 64U)
		{
			continue;
		}
		if (hasAvoidedLink_ && candidate.id == avoidedLink_.toArea)
		{
			continue;
		}
		++candidateCount;
		nav::NavCorridor corridor = {};
		const nav::NavQueryResult corridorResult = buildCorridor(
			query,
			currentArea.area,
			candidate.id,
			&corridor,
			decision == nullptr || !collectPathStats_ ? nullptr : &decision->pathSearchStats);
		if (decision != nullptr)
		{
			decision->corridorResult = corridorResult;
		}
	if (corridorResult != nav::NavQueryResult::Found || corridor.areas.size() < 2U ||
			!startTraversal(snapshot, corridor, {currentArea.area, candidate.id, 0U, 0U}))
		{
			continue;
		}
		const nav::NavDirectedLink fallbackLink = {
			currentArea.area, candidate.id, 0U, 0U};
		rememberRoute(snapshot, corridor, fallbackLink);
		if (decision != nullptr)
		{
			decision->targetArea = candidate.id;
			populateRouteDecision(decision);
		}
		hasAvoidedLink_ = false;
		return true;
	}

	if (hasAvoidedLink_)
	{
		hasAvoidedLink_ = false;
		return selectRoute(snapshot, currentArea, objectiveArea, decision);
	}
	return false;
}

bool NavRoamController::selectRoamRoute(
	const nav::NavSnapshot &snapshot,
	const nav::NavAreaMatch &currentArea,
	NavRoamDecision *decision)
{
	const nav::NavDocument *document = snapshot.document();
	if (document == nullptr || document->areas().empty())
	{
		return false;
	}
	nav::NavQuery query(snapshot);
	std::vector<nav::NavDirectedLink> links;
	if (query.outgoingLinks(currentArea.area, &links) != nav::NavQueryResult::Found)
	{
		return false;
	}
	if (!modePolicy_.allowsAdaptiveRouteWeighting())
	{
		for (const nav::NavDirectedLink &link : links)
		{
			if (decision != nullptr)
			{
				const nav::NavArea *target = document->findArea(link.toArea);
				decision->goalPresent = target != nullptr;
				decision->goalKind = NavGoalKind::Roam;
				decision->goalArea = link.toArea;
				decision->goalPosition = target != nullptr
					? centerOf(*target) : nav::NavVector{0.0f, 0.0f, 0.0f};
				decision->pathRequested = true;
			}
			nav::NavCorridor corridor = {};
			const nav::NavQueryResult corridorResult = buildCorridor(
				query, currentArea.area, link.toArea, nav::NavRouteType::Fastest,
				&corridor, decision == nullptr || !collectPathStats_ ? nullptr : &decision->pathSearchStats);
			if (decision != nullptr)
			{
				decision->corridorResult = corridorResult;
				decision->pathResult = corridorResult;
			}
			if (link.toArea == currentArea.area ||
					corridorResult != nav::NavQueryResult::Found ||
					corridor.areas.size() < 2U ||
					!startTraversal(snapshot, corridor, link))
			{
				continue;
			}
			activeTraversal_ = traversalActionFor(snapshot, link);
			rememberRoute(snapshot, corridor, link);
			if (decision != nullptr)
			{
				decision->targetArea = link.toArea;
				populateRouteDecision(decision);
			}
			return true;
		}
		return false;
	}
	const std::size_t firstCandidate = nextLinkIndex_ % document->areas().size();
	std::size_t attempts = 0U;
	for (std::size_t offset = 0U;
			offset < document->areas().size() && attempts < 64U; ++offset)
	{
		const std::size_t candidateIndex =
				(firstCandidate + offset) % document->areas().size();
		const nav::NavArea &candidate = document->areas()[candidateIndex];
		if (candidate.id == currentArea.area)
		{
			continue;
		}
		++attempts;
		if (decision != nullptr)
		{
			decision->goalPresent = true;
			decision->goalKind = NavGoalKind::Roam;
			decision->goalArea = candidate.id;
			decision->goalPosition = centerOf(candidate);
			decision->pathRequested = true;
		}
		nav::NavCorridor corridor = {};
		const nav::NavQueryResult corridorResult = buildCorridor(
			query, currentArea.area, candidate.id, &corridor,
			decision == nullptr || !collectPathStats_ ? nullptr : &decision->pathSearchStats);
		if (decision != nullptr)
		{
			decision->pathResult = corridorResult;
		}
		if (corridorResult != nav::NavQueryResult::Found || corridor.areas.size() < 2U)
		{
			continue;
		}
		const nav::AreaId nextArea = corridor.areas[1U];
		nav::NavDirectedLink link = {currentArea.area, nextArea, 0U, 0U};
		for (const nav::NavDirectedLink &candidateLink : links)
		{
			if (candidateLink.toArea == nextArea)
			{
				link = candidateLink;
				break;
			}
		}
		if (hasAvoidedLink_ && link.fromArea == avoidedLink_.fromArea &&
				link.toArea == avoidedLink_.toArea)
		{
			continue;
		}
		if (!startTraversal(snapshot, corridor, link))
		{
			continue;
		}
		nextLinkIndex_ = candidateIndex + 1U;
		rememberRoute(snapshot, corridor, link);
		if (decision != nullptr)
		{
			populateRouteDecision(decision);
			decision->targetArea = candidate.id;
			decision->targetPosition = centerOf(candidate);
		}
		hasAvoidedLink_ = false;
		return true;
	}
	if (hasAvoidedLink_)
	{
		hasAvoidedLink_ = false;
		return selectRoamRoute(snapshot, currentArea, decision);
	}
	return false;
}

void NavRoamController::rememberRoute(
	const nav::NavSnapshot &snapshot,
	const nav::NavCorridor &corridor,
	const nav::NavDirectedLink &link)
{
	activeCorridor_ = corridor;
	activeLink_ = link;
	activeCorridorIndex_ = 0U;
	hasActiveRoute_ = true;
	if (pathSequence_ == (std::numeric_limits<std::uint32_t>::max)())
	{
		pathSequence_ = 1U;
	}
	else
	{
		++pathSequence_;
	}
	const nav::NavDocument *document = snapshot.document();
	const nav::NavArea *targetArea = document != nullptr
			? document->findArea(link.toArea)
			: nullptr;
	activeTargetPosition_ = targetArea != nullptr
			? centerOf(*targetArea)
			: nav::NavVector{0.0f, 0.0f, 0.0f};
}

void NavRoamController::populateRouteDecision(NavRoamDecision *decision) const
{
	if (decision == nullptr || !hasActiveRoute_)
	{
		return;
	}
	decision->goalPresent = true;
	decision->goalKind = hasObjectiveTarget_ ? NavGoalKind::Objective : NavGoalKind::Roam;
	decision->goalArea = activeLink_.toArea;
	decision->goalPosition = activeTargetPosition_;
	decision->targetArea = activeLink_.toArea;
	decision->targetPosition = activeTargetPosition_;
	decision->corridorAreaCount = activeCorridor_.areas.size();
	decision->corridorIndex = activeCorridorIndex_;
	decision->linkFromArea = activeLink_.fromArea;
	decision->linkToArea = activeLink_.toArea;
	decision->linkDirection = activeLink_.direction;
	decision->linkHow = activeLink_.how;
	decision->linkResult = nav::NavQueryResult::Found;
	decision->corridorResult = nav::NavQueryResult::Found;
	decision->pathSequence = pathSequence_;
	decision->routeType = activeCorridor_.routeType;
	decision->pathCost = activeCorridor_.cost;
	decision->selectedPath = activeCorridor_.areas;
}

void NavRoamController::resetRoute()
{
	locomotion_ = nav::LocomotionController(roamLocomotionConfig());
	activeTraversal_ = nav::TraversalAction::Walk;
	jumpDrop_ = nav::JumpDropController();
	specialTraversal_ = nav::SpecialTraversalController();
	activeCorridor_ = {};
	activeLink_ = {};
	activeTargetPosition_ = {0.0f, 0.0f, 0.0f};
	lastIntentDirection_ = {0.0f, 0.0f, 0.0f};
	stuckRecoveryDirection_ = {0.0f, 0.0f, 0.0f};
	activeCorridorIndex_ = 0U;
	stuckRecoveryCount_ = 0U;
	stuckRecoveryFramesRemaining_ = 0U;
	stuckRecoveryActive_ = false;
	hasActiveRoute_ = false;
}
}
}
