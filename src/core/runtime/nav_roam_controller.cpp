#include "astrabot/runtime/nav_roam_controller.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

namespace astrabot
{
namespace runtime
{
namespace
{
	constexpr float kMaximumRecoveryDistance = 10000.0f;
constexpr float kRoamSpeed = 240.0f;
constexpr float kRecoveryInset = 5.0f;
constexpr float kRecoveryLayerTolerance = 64.0f;
constexpr float kMaximumGroundRouteAge = 0.25f;
constexpr std::uint32_t kOffMeshNoProgressLimit = 20U;
constexpr float kExternalRecoveryResetDistance = 8.0f;
	constexpr std::uint32_t kStuckRecoveryFrameLimit = 8U;
constexpr std::uint32_t kMaximumStuckRecoveryAttempts = 2U;
// Server-frame budget, independent of full-update cadence and assignment generation.
constexpr std::uint32_t kInitialUnsafeDropBackoffFrames = 128U;
constexpr std::uint32_t kMaximumUnsafeDropBackoffFrames = 512U;
constexpr float kUnsafeDropProgressDistance = 64.0f;
constexpr std::uint32_t kInitialPathFailureBackoffFrames = 15U;
constexpr std::uint32_t kMaximumPathFailureBackoffFrames = 120U;
// Keep safe falls and their run-up alive long enough to confirm landing.
constexpr std::uint32_t kCompatibilityTraversalFrameBudget = 60U;

nav::LocomotionConfig roamLocomotionConfig()
{
	return {32.0f, 20.0f, 64.0f, 16.0f, 240.0f, 8U};
}

nav::JumpDropConfig roamJumpDropConfig(compat::RuntimeMode mode)
{
	const float intentSpeed = mode == compat::RuntimeMode::Compatibility
		? kRoamSpeed
		: nav::JumpDropConfig::kDefaultIntentSpeed;
	const std::uint32_t traversalFrames =
		mode == compat::RuntimeMode::Compatibility
			? kCompatibilityTraversalFrameBudget
			: nav::JumpDropConfig::kDefaultTraversalFrames;
	return {
		nav::JumpDropConfig::kDefaultLaunchHorizontalTolerance,
		nav::JumpDropConfig::kDefaultLaunchVerticalTolerance,
		nav::JumpDropConfig::kDefaultLandingTolerance,
		intentSpeed,
		traversalFrames};
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
	decision->goalSelectionReason = GoalSelectionNone;
	decision->goalSelectionStrategy = GoalSelectionStrategyNone;
	decision->goalPresent = false;
		decision->pathRequested = false;
		decision->pathResult = nav::NavQueryResult::InvalidArgument;
	decision->goalArea = 0U;
	decision->goalPosition = {0.0f, 0.0f, 0.0f};
	decision->goalGeneration = 0U;
		decision->currentAreaResult = nav::NavQueryResult::InvalidArgument;
		decision->nearestAreaResult = nav::NavQueryResult::InvalidArgument;
		decision->linkResult = nav::NavQueryResult::InvalidArgument;
		decision->corridorResult = nav::NavQueryResult::InvalidArgument;
		decision->locomotionResult = nav::LocomotionResult::InvalidArgument;
	}

	// Geometry-only candidate selection. Public NAV cannot prove collision clearance.
	bool selectRecoveryArea(const nav::NavSnapshot &snapshot,
		const nav::LocomotionObservation &observation,
		const nav::AreaId *excludedAreas, std::size_t excludedCount, nav::NavAreaMatch *match)
	{
		const nav::NavDocument *document = snapshot.document();
		float bestScore = std::numeric_limits<float>::max();
		bool found = false;
		for (const nav::NavArea &candidate : document->areas())
		{
			bool excluded = false;
			for (std::size_t index = 0U; index < excludedCount; ++index)
			{
				excluded = excluded || excludedAreas[index] == candidate.id;
			}
			if (excluded) continue;
			const float insetX = (std::min)(kRecoveryInset,
				(candidate.extent.hi.x - candidate.extent.lo.x) * 0.5f);
			const float insetY = (std::min)(kRecoveryInset,
				(candidate.extent.hi.y - candidate.extent.lo.y) * 0.5f);
			nav::NavVector point = {
				(std::max)(candidate.extent.lo.x + insetX,
					(std::min)(observation.position.x, candidate.extent.hi.x - insetX)),
				(std::max)(candidate.extent.lo.y + insetY,
					(std::min)(observation.position.y, candidate.extent.hi.y - insetY)), 0.0f};
			point.z = nav::surfaceZAt(candidate, point.x, point.y);
			const float dx = point.x - observation.position.x;
			const float dy = point.y - observation.position.y;
			const float dz = point.z - observation.position.z;
			const float horizontalSquared = dx * dx + dy * dy;
			const float score = horizontalSquared + dz * dz;
			// Retain the existing 64-unit NAV vertical tolerance. Height participates
			// in ranking so coincident layers do not reduce to the smallest area ID.
			if (!std::isfinite(score) || std::fabs(dz) > kRecoveryLayerTolerance ||
				horizontalSquared <= 0.00000001f ||
				horizontalSquared > kMaximumRecoveryDistance * kMaximumRecoveryDistance ||
				(found && (score > bestScore ||
					(score == bestScore && candidate.id >= match->area))))
			{
				continue;
			}
			found = true;
			bestScore = score;
			*match = {candidate.id, horizontalSquared, point};
		}
		return found;
	}

	bool buildRecoveryIntent(const nav::NavAreaMatch &area,
		const nav::LocomotionObservation &observation, nav::LocomotionIntent *intent)
	{
		const float dx = area.closestPoint.x - observation.position.x;
		const float dy = area.closestPoint.y - observation.position.y;
		const float length = std::hypot(dx, dy);
		if (!std::isfinite(length) || length <= 0.0001f)
		{
			return false;
		}
		intent->direction = {dx / length, dy / length, 0.0f};
		intent->targetPosition = area.closestPoint;
		intent->speed = kRoamSpeed;
		intent->posture = nav::LocomotionPosture::Standing;
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

bool isWithinTraversalLaunchTolerance(
	const nav::NavVector &position,
	const nav::NavVector &launchPosition)
{
	const float deltaX = position.x - launchPosition.x;
	const float deltaY = position.y - launchPosition.y;
	const float horizontalTolerance = (std::max)(
		32.0f, nav::JumpDropConfig::kDefaultLaunchHorizontalTolerance);
	return std::isfinite(deltaX) && std::isfinite(deltaY) &&
		std::isfinite(position.z) && std::isfinite(launchPosition.z) &&
		deltaX * deltaX + deltaY * deltaY <=
			horizontalTolerance * horizontalTolerance &&
		std::fabs(position.z - launchPosition.z) <=
			nav::JumpDropConfig::kDefaultLaunchVerticalTolerance;
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
	const nav::NavDocument *document = snapshot.document();
	if (document != nullptr)
	{
		const nav::NavArea *from = document->findArea(link.fromArea);
		const nav::NavArea *to = document->findArea(link.toArea);
		if (from != nullptr && to != nullptr &&
			floorHeight(*from) - floorHeight(*to) >
				nav::LocomotionConfig::kMaximumJumpHeight)
		{
			return nav::TraversalAction::Drop;
		}
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

void NavAreaVisitHistory::record(
	std::uint32_t mapGeneration,
	std::uint8_t team,
	nav::AreaId area,
	std::uint32_t frame)
{
	if (team > 2U || area == 0U)
	{
		return;
	}
	if (!initialized_ || mapGeneration_ != mapGeneration)
	{
		reset(mapGeneration);
	}
	auto entry = std::lower_bound(
		entries_.begin(), entries_.end(), area,
		[team](const Entry &candidate, nav::AreaId candidateArea) {
			return candidate.team < team ||
				(candidate.team == team && candidate.area < candidateArea);
		});
	if (entry == entries_.end() || entry->team != team || entry->area != area)
	{
		entries_.insert(entry, {team, area, frame});
		return;
	}
	if (frame > entry->frame)
	{
		entry->frame = frame;
	}
}

std::uint32_t NavAreaVisitHistory::lastVisited(
	std::uint32_t mapGeneration,
	std::uint8_t team,
	nav::AreaId area) const
{
	if (!initialized_ || mapGeneration_ != mapGeneration || team > 2U || area == 0U)
	{
		return 0U;
	}
	const auto entry = std::lower_bound(
		entries_.begin(), entries_.end(), area,
		[team](const Entry &candidate, nav::AreaId candidateArea) {
			return candidate.team < team ||
				(candidate.team == team && candidate.area < candidateArea);
		});
	return entry != entries_.end() && entry->team == team && entry->area == area
		? entry->frame
		: 0U;
}

void NavAreaVisitHistory::reset(std::uint32_t mapGeneration)
{
	mapGeneration_ = mapGeneration;
	initialized_ = true;
	entries_.clear();
}

NavRoamReservationBoard::NavRoamReservationBoard()
	: claims_(), mapGeneration_(0U), roundGeneration_(0U), initialized_(false)
{
}

void NavRoamReservationBoard::reset(
	std::uint32_t mapGeneration, std::uint32_t roundGeneration)
{
	if (initialized_ && mapGeneration_ == mapGeneration &&
		roundGeneration_ == roundGeneration)
	{
		return;
	}
	for (Claim &claim : claims_)
	{
		claim = {};
	}
	mapGeneration_ = mapGeneration;
	roundGeneration_ = roundGeneration;
	initialized_ = true;
}

bool NavRoamReservationBoard::goalReservedByOther(
	std::uint8_t team, ActorId actor, nav::AreaId goalArea) const
{
	if (!initialized_ || team == 0U || goalArea == 0U)
	{
		return false;
	}
	for (const Claim &claim : claims_)
	{
		if (claim.valid && claim.team == team && claim.goalArea == goalArea &&
			(claim.actor.slot != actor.slot ||
				claim.actor.actorGeneration != actor.actorGeneration))
		{
			return true;
		}
	}
	return false;
}

bool NavRoamReservationBoard::firstLinkReservedByOther(
	std::uint8_t team,
	ActorId actor,
	nav::AreaId fromArea,
	nav::AreaId toArea) const
{
	if (!initialized_ || team == 0U || fromArea == 0U || toArea == 0U)
	{
		return false;
	}
	for (const Claim &claim : claims_)
	{
		if (claim.valid && claim.team == team &&
			(claim.actor.slot != actor.slot ||
				claim.actor.actorGeneration != actor.actorGeneration) &&
			claim.fromArea == fromArea && claim.toArea == toArea)
		{
			return true;
		}
	}
	return false;
}

bool NavRoamReservationBoard::reserve(
	std::uint8_t team,
	ActorId actor,
	nav::AreaId goalArea,
	nav::AreaId fromArea,
	nav::AreaId toArea)
{
	if (!initialized_ || team == 0U || actor.slot < LifecycleSession::kFirstClientSlot ||
		actor.slot > LifecycleSession::kLastClientSlot || actor.actorGeneration == 0U ||
		goalArea == 0U || fromArea == 0U || toArea == 0U)
	{
		return false;
	}
	Claim &claim = claims_[static_cast<std::size_t>(actor.slot - 1U)];
	claim.valid = true;
	claim.team = team;
	claim.actor = actor;
	claim.goalArea = goalArea;
	claim.fromArea = fromArea;
	claim.toArea = toArea;
	return true;
}

void NavRoamReservationBoard::release(ActorId actor)
{
	if (actor.slot < LifecycleSession::kFirstClientSlot ||
		actor.slot > LifecycleSession::kLastClientSlot)
	{
		return;
	}
	Claim &claim = claims_[static_cast<std::size_t>(actor.slot - 1U)];
	if (claim.valid && claim.actor.slot == actor.slot &&
		claim.actor.actorGeneration == actor.actorGeneration)
	{
		claim = {};
	}
}

void GroundLookaheadRoute::reset()
{
	actor = {};
	frame = {};
	targetArea = 0U;
	direction = {};
	running = false;
	active = false;
	fullUpdateSequence = 0U;
	createdAt = 0.0f;
}

bool GroundLookaheadRoute::matchesNextUpdate(
	const ActorId &currentActor,
	const world::FrameIdentity &currentFrame,
	std::uint32_t currentFullUpdate, float now) const
{
	const float directionLength = std::hypot(direction.x, direction.y);
	return active && targetArea != 0U && actor.actorGeneration != 0U &&
		actor.slot == currentActor.slot &&
		actor.actorGeneration == currentActor.actorGeneration &&
		frame.mapGeneration == currentFrame.mapGeneration &&
		frame.roundGeneration == currentFrame.roundGeneration &&
		currentFrame.tick > frame.tick &&
		static_cast<std::uint32_t>(currentFullUpdate - fullUpdateSequence) == 1U &&
		std::isfinite(createdAt) && std::isfinite(now) &&
		now >= createdAt && now - createdAt <= kMaximumGroundRouteAge &&
		std::isfinite(direction.x) && std::isfinite(direction.y) &&
		std::isfinite(direction.z) && std::isfinite(directionLength) &&
		directionLength >= 0.9f && directionLength <= 1.1f;
}

NavRoamController::NavRoamController()
	: NavRoamController(compat::RuntimeMode::Compatibility)
{
}

NavRoamController::NavRoamController(compat::RuntimeMode mode)
	: offMeshRecovery_{},
	  unsafeDropFailure_{},
	  previousMovementSuppressed_(false),
	  locomotion_(roamLocomotionConfig()),
	  activeTraversal_(nav::TraversalAction::Walk),
	jumpDrop_(roamJumpDropConfig(mode)),
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
	initialized_(false),
	randomSource_(nullptr),
	localAreaVisitHistory_(),
	areaVisitHistory_(&localAreaVisitHistory_),
	reservationBoard_(nullptr),
	lastVisitedArea_(0U),
	lastVisitedMapGeneration_(0U),
	lastVisitedTeam_(0U),
	roamGoalGeneration_(0U),
	goalSelectionReason_(GoalSelectionNone),
	goalSelectionStrategy_(GoalSelectionStrategyNone),
	maximumSafeDropHeight_(0.0f),
	safeDropHeightAvailable_(false),
	unsafeDropRejected_(false),
	hasRoamGoal_(false),
	roamGoalArea_(0U),
	failedRoamGoalArea_(0U),
	roamGoalPosition_{0.0f, 0.0f, 0.0f}
{
}

void NavRoamController::setRandomSource(
	compat::ICompatibilityRandomSource *source)
{
	randomSource_ = source;
}

void NavRoamController::setAreaVisitHistory(NavAreaVisitHistory *history)
{
	areaVisitHistory_ = history != nullptr ? history : &localAreaVisitHistory_;
	lastVisitedArea_ = 0U;
	lastVisitedMapGeneration_ = 0U;
	lastVisitedTeam_ = 0U;
}

void NavRoamController::setReservationBoard(NavRoamReservationBoard *board)
{
	if (reservationBoard_ == board)
	{
		return;
	}
	if (reservationBoard_ != nullptr)
	{
		reservationBoard_->release(actor_);
	}
	reservationBoard_ = board;
}

void NavRoamController::setRuntimeMode(compat::RuntimeMode mode)
{
	modePolicy_ = compat::RuntimeModePolicy(mode);
	jumpDrop_ = nav::JumpDropController(roamJumpDropConfig(mode));
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

void NavRoamController::rememberUnsafeDrop(
	const NavRoamObservation &observation, nav::AreaId goalArea,
	const nav::NavDirectedLink &link)
{
	hasAvoidedLink_ = true;
	avoidedLink_ = link;
	if (!observation.hasObjectiveTarget) return;
	if (unsafeDropFailure_.valid &&
		unsafeDropFailure_.link.fromArea == link.fromArea &&
		unsafeDropFailure_.link.toArea == link.toArea)
	{
		delayUnsafeDropRetry();
		return;
	}
	unsafeDropFailure_ = {true, true, observation.objectiveTargetEntity,
		observation.locomotion.position, goalArea, link, 0U,
		kInitialUnsafeDropBackoffFrames};
	// Allow one immediate attempt through the existing directed bypass flow.
}

void NavRoamController::delayUnsafeDropRetry()
{
	unsafeDropFailure_.retryFramesRemaining = unsafeDropFailure_.nextBackoffFrames;
	unsafeDropFailure_.nextBackoffFrames = (std::min)(
		kMaximumUnsafeDropBackoffFrames, unsafeDropFailure_.nextBackoffFrames * 2U);
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
		offMeshRecovery_ = {};
		unsafeDropFailure_ = {};
		clearPathFailure();
		hasAvoidedLink_ = false;
		avoidedLink_ = {0U, 0U, 0U, 0U};
		failedRoamGoalArea_ = 0U;
			if (decision != nullptr)
		{
			decision->recomputeReason = NavRecomputeReason::MapOrRoundChanged;
		}
		resetRoute();
		}
	}
	if (unsafeDropFailure_.valid)
	{
		const float dx = observation.locomotion.position.x - unsafeDropFailure_.anchor.x;
		const float dy = observation.locomotion.position.y - unsafeDropFailure_.anchor.y;
		const bool newBomb = observation.hasObjectiveTarget &&
			observation.objectiveTargetEntity.isValid() &&
			!(observation.objectiveTargetEntity == unsafeDropFailure_.entity);
		const bool madeProgress = observation.locomotion.grounded &&
			dx * dx + dy * dy >= kUnsafeDropProgressDistance * kUnsafeDropProgressDistance;
		if (newBomb || madeProgress)
		{
			if (newBomb) resetRoute();
			unsafeDropFailure_ = {};
			hasAvoidedLink_ = false;
			avoidedLink_ = {};
			failedRoamGoalArea_ = 0U;
			clearPathFailure();
		}
		else if (lastFrame_.isValid() && !observation.movementSuppressed &&
			!previousMovementSuppressed_)
		{
			const std::uint32_t elapsed = observation.frame.tick - lastFrame_.tick;
			unsafeDropFailure_.retryFramesRemaining -= (std::min)(
				elapsed, unsafeDropFailure_.retryFramesRemaining);
		}
	}
	previousMovementSuppressed_ = observation.movementSuppressed;
	const bool objectiveTargetChanged =
		hasObjectiveTarget_ != observation.hasObjectiveTarget ||
		(observation.hasObjectiveTarget &&
			(std::fabs(objectiveTarget_.x - observation.objectiveTarget.x) > 0.5f ||
			 std::fabs(objectiveTarget_.y - observation.objectiveTarget.y) > 0.5f ||
			 std::fabs(objectiveTarget_.z - observation.objectiveTarget.z) > 0.5f));
	if (objectiveTargetChanged)
	{
		// Goal reassignment cannot make a failed recovery point traversable.
		// Retain candidate exclusions until movement/lifecycle provides new evidence.
		if (!unsafeDropFailure_.valid)
		{
			clearPathFailure();
			hasAvoidedLink_ = false;
			avoidedLink_ = {0U, 0U, 0U, 0U};
			failedRoamGoalArea_ = 0U;
		}
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

	if (observation.movementSuppressed)
	{
		if (decision != nullptr)
		{
			decision->stage = NavRoamStage::MovementSuppressed;
			decision->locomotionResult = nav::LocomotionResult::Inactive;
			decision->failureReason = NavFailureReason::None;
		}
		return NavRoamResult::MovementSuppressed;
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
	safeDropHeightAvailable_ =
		observation.locomotion.safeDropHeightAvailable &&
		std::isfinite(observation.locomotion.maximumSafeDropHeight) &&
		observation.locomotion.maximumSafeDropHeight > 0.0f;
	maximumSafeDropHeight_ = safeDropHeightAvailable_
		? observation.locomotion.maximumSafeDropHeight
		: 0.0f;
	if (currentAreaResult == nav::NavQueryResult::Found &&
		currentArea.area != 0U && areaVisitHistory_ != nullptr &&
		(lastVisitedArea_ != currentArea.area ||
			lastVisitedMapGeneration_ != observation.frame.mapGeneration ||
			lastVisitedTeam_ != observation.team))
	{
		areaVisitHistory_->record(
			observation.frame.mapGeneration,
			observation.team,
			currentArea.area,
			observation.frame.tick);
		lastVisitedArea_ = currentArea.area;
		lastVisitedMapGeneration_ = observation.frame.mapGeneration;
		lastVisitedTeam_ = observation.team;
	}
	nav::AreaId objectiveArea = 0U;
	if (observation.hasObjectiveTarget)
	{
		nav::NavAreaMatch objectiveMatch = {};
		nav::NavQueryResult objectiveResult = query.findContaining(
				observation.objectiveTarget,
				64.0f,
				&objectiveMatch);
		if (objectiveResult == nav::NavQueryResult::NoAreaContaining)
		{
			objectiveResult = query.findNearest(
					observation.objectiveTarget,
					kMaximumRecoveryDistance,
					&objectiveMatch);
		}
		if (objectiveResult == nav::NavQueryResult::Found)
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
	bool plannedLinkContinuation = false;
	if (currentAreaResult == nav::NavQueryResult::NoAreaContaining &&
			!jumpDrop_.isActive() && !specialTraversal_.isActive())
	{
		const nav::NavQueryResult nearestResult = query.findNearest(
				{observation.locomotion.position.x,
				 observation.locomotion.position.y,
				 observation.locomotion.position.z},
			kMaximumRecoveryDistance,
			&currentArea);
		plannedLinkContinuation =
			hasActiveRoute_ && locomotion_.isActive() &&
			nearestResult == nav::NavQueryResult::Found &&
			activeLink_.fromArea == currentArea.area &&
			activeLink_.toArea != activeLink_.fromArea;
		if (decision != nullptr)
		{
			decision->nearestAreaResult = nearestResult;
			decision->recoveryArea = nearestResult == nav::NavQueryResult::Found ?
				currentArea.area : 0U;
			decision->nearestDistanceSquared = currentArea.distanceSquared;
		}
		if (!plannedLinkContinuation)
		{
			const nav::NavVector &position = observation.locomotion.position;
			const nav::NavVector &blocked = offMeshRecovery_.blockedPosition;
			const float externalDisplacement = std::hypot(
				std::hypot(position.x - blocked.x, position.y - blocked.y),
				position.z - blocked.z);
			if (offMeshRecovery_.failedAreaCount != 0U &&
				externalDisplacement >= kExternalRecoveryResetDistance)
			{
				offMeshRecovery_ = {};
			}
			if (!offMeshRecovery_.blocked &&
				selectRecoveryArea(snapshot, observation.locomotion,
					offMeshRecovery_.failedAreas.data(), offMeshRecovery_.failedAreaCount, &currentArea) &&
				buildRecoveryIntent(currentArea, observation.locomotion, intent))
			{
				const float distance = std::sqrt(currentArea.distanceSquared);
				if (!offMeshRecovery_.active)
				{
					offMeshRecovery_.active = true;
					offMeshRecovery_.noProgressUpdates = 0U;
					offMeshRecovery_.bestDistance = distance;
				}
				else if (currentArea.area == offMeshRecovery_.area &&
					distance + 1.0f < offMeshRecovery_.bestDistance)
				{
					offMeshRecovery_.bestDistance = distance;
					offMeshRecovery_.noProgressUpdates = 0U;
				}
				else
				{
					++offMeshRecovery_.noProgressUpdates;
					if (currentArea.area != offMeshRecovery_.area)
					{
						offMeshRecovery_.bestDistance = distance;
					}
				}
				offMeshRecovery_.area = currentArea.area;
				offMeshRecovery_.target = currentArea.closestPoint;
				if (offMeshRecovery_.noProgressUpdates >= kOffMeshNoProgressLimit)
				{
					if (offMeshRecovery_.failedAreaCount == 0U)
					{
						offMeshRecovery_.blockedPosition = position;
					}
					offMeshRecovery_.failedAreas[offMeshRecovery_.failedAreaCount++] = currentArea.area;
					offMeshRecovery_.blocked = offMeshRecovery_.failedAreaCount ==
						offMeshRecovery_.failedAreas.size();
					offMeshRecovery_.active = false;
					resetRoute();
					// Report this failed attempt for existing objective reassignment.
					// The next update selects an untried candidate if budget remains.
					*intent = {};
					if (decision != nullptr)
					{
						decision->stage = NavRoamStage::Failed;
						decision->failureReason = NavFailureReason::RecoveryNoProgress;
						decision->recoveryCandidateFailed = true;
						decision->recoveryArea = currentArea.area;
						decision->targetPosition = currentArea.closestPoint;
						decision->recoveryNoProgressUpdates = offMeshRecovery_.noProgressUpdates;
						decision->recoveryGeometryOnly = true;
					}
					return NavRoamResult::NoRoute;
				}
				else
				{
					if (decision != nullptr)
					{
						decision->stage = NavRoamStage::OffMeshRecovery;
						decision->failureReason = NavFailureReason::CurrentAreaMissing;
						decision->recoveryArea = currentArea.area;
						decision->targetArea = currentArea.area;
						decision->targetPosition = currentArea.closestPoint;
						decision->nearestDistanceSquared = currentArea.distanceSquared;
						decision->recoveryNoProgressUpdates = offMeshRecovery_.noProgressUpdates;
						decision->recoveryGeometryOnly = true;
					}
					return NavRoamResult::IntentReady;
				}
			}
			// No eligible geometry candidates remain, or the attempt cap was reached.
			if (offMeshRecovery_.failedAreaCount != 0U)
			{
				offMeshRecovery_.blocked = true;
			}
			*intent = {};
			if (decision != nullptr)
			{
				decision->failureReason = offMeshRecovery_.blocked
					? NavFailureReason::RecoveryNoProgress : NavFailureReason::CurrentAreaMissing;
				decision->stage = NavRoamStage::Failed;
				decision->recoveryArea = offMeshRecovery_.blocked ? offMeshRecovery_.area : 0U;
				decision->targetPosition = offMeshRecovery_.blocked ? offMeshRecovery_.target : nav::NavVector{};
				decision->recoveryNoProgressUpdates = offMeshRecovery_.noProgressUpdates;
				decision->recoveryGeometryOnly = true;
			}
			return NavRoamResult::NoRoute;
		}
	}
	if (currentAreaResult != nav::NavQueryResult::Found &&
			!plannedLinkContinuation &&
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
	if (currentAreaResult == nav::NavQueryResult::Found)
	{
		offMeshRecovery_ = {};
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
				goalSelectionReason_ = GoalSelectionPathFailed;
				goalSelectionStrategy_ = GoalSelectionStrategyNone;
				if (decision != nullptr)
				{
					decision->locomotionResult = nav::LocomotionResult::Stuck;
					decision->failureReason = NavFailureReason::MovementNotProduced;
					decision->goalSelectionReason = goalSelectionReason_;
					decision->goalSelectionStrategy = goalSelectionStrategy_;
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
		const nav::LocomotionResult startResult =
			locomotion_.start(activeCorridor_);
		if (startResult != nav::LocomotionResult::Started)
		{
			resetRoute();
			goalSelectionReason_ = GoalSelectionPathFailed;
			goalSelectionStrategy_ = GoalSelectionStrategyNone;
			if (decision != nullptr)
			{
				decision->locomotionResult = startResult;
				decision->failureReason = NavFailureReason::NavApplyRejected;
				decision->goalSelectionReason = goalSelectionReason_;
				decision->goalSelectionStrategy = goalSelectionStrategy_;
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
		if (unsafeDropFailure_.valid)
		{
			hasAvoidedLink_ = objectiveArea == 0U || unsafeDropFailure_.bypassPending;
			avoidedLink_ = unsafeDropFailure_.link;
			if (objectiveArea == 0U)
			{
				failedRoamGoalArea_ = unsafeDropFailure_.goalArea;
				goalSelectionReason_ = GoalSelectionPathFailed;
			}
			else if (unsafeDropFailure_.retryFramesRemaining > 0U)
			{
				if (decision != nullptr)
				{
					decision->goalPresent = true;
					decision->goalKind = NavGoalKind::Objective;
					decision->goalArea = objectiveArea;
					decision->goalPosition = objectiveTarget_;
					decision->linkFromArea = unsafeDropFailure_.link.fromArea;
					decision->linkToArea = unsafeDropFailure_.link.toArea;
					decision->linkDirection = unsafeDropFailure_.link.direction;
					decision->linkHow = unsafeDropFailure_.link.how;
					decision->pathResult = nav::NavQueryResult::NoRoute;
					decision->failureReason = NavFailureReason::UnsafeDrop;
					decision->stage = NavRoamStage::Failed;
				}
				return NavRoamResult::NoRoute;
			}
		}
		if (objectiveArea != 0U && unsafeDropFailure_.valid)
		{
			unsafeDropFailure_.bypassPending = false;
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
			if (objectiveArea != 0U && unsafeDropFailure_.valid)
			{
				delayUnsafeDropRetry();
				clearPathFailure();
			}
		if (objectiveArea != 0U && !unsafeDropFailure_.valid && decision != nullptr &&
				(decision->pathResult == nav::NavQueryResult::ResourceLimit ||
					decision->pathResult == nav::NavQueryResult::NoRoute))
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

	if (hasActiveRoute_ && !activeCorridor_.links.empty() &&
			!jumpDrop_.isActive() && !specialTraversal_.isActive())
	{
		const std::size_t firstLinkIndex = activeCorridorIndex_ > 0U
			? activeCorridorIndex_ - 1U
			: 0U;
		for (std::size_t index = firstLinkIndex;
			index < activeCorridor_.links.size(); ++index)
		{
			const nav::NavDirectedLink candidate = activeCorridor_.links[index];
			if (candidate.fromArea != currentArea.area ||
				traversalActionFor(snapshot, candidate) == nav::TraversalAction::Walk)
			{
				continue;
			}
		activeLink_ = candidate;
		activeTraversal_ = nav::TraversalAction::Walk;
		const nav::NavDocument *document = snapshot.document();
		const nav::NavArea *fromArea = document != nullptr
			? document->findArea(candidate.fromArea)
			: nullptr;
		const nav::NavArea *toArea = document != nullptr
			? document->findArea(candidate.toArea)
			: nullptr;
		if (fromArea == nullptr || toArea == nullptr)
		{
			resetRoute();
			if (decision != nullptr)
			{
				decision->locomotionResult = nav::LocomotionResult::InvalidCorridor;
				decision->failureReason = NavFailureReason::NavApplyRejected;
				decision->stage = NavRoamStage::Failed;
			}
			return NavRoamResult::ReplanRequired;
		}
		const std::uint8_t reverseDirection =
			candidate.direction < nav::NavArea::kDirectionCount
				? static_cast<std::uint8_t>(
					(candidate.direction + 2U) % nav::NavArea::kDirectionCount)
				: nav::NavArea::kDirectionCount;
		const nav::NavVector landingPortal = nav::portalSteeringPointForLink(
			*fromArea, *toArea, observation.locomotion.position,
			candidate.direction);
		const nav::NavVector launchPortal = nav::portalSteeringPointForLink(
			*toArea, *fromArea, landingPortal, reverseDirection);
		if (!isWithinTraversalLaunchTolerance(
				observation.locomotion.position, launchPortal))
		{
			break;
		}
		nav::LocomotionResult traversalStartResult =
				nav::LocomotionResult::InvalidArgument;
		if (!startTraversal(
				snapshot, activeCorridor_, candidate, &traversalStartResult,
				&observation.locomotion.position, &landingPortal))
		{
			if (traversalStartResult == nav::LocomotionResult::UnsafeDrop)
			{
				rememberUnsafeDrop(observation, objectiveArea, candidate);
			}
			hasAvoidedLink_ = true;
			avoidedLink_ = candidate;
			++nextLinkIndex_;
			if (objectiveArea == 0U && hasRoamGoal_)
			{
				failedRoamGoalArea_ = roamGoalArea_;
			}
			resetRoute();
				goalSelectionReason_ = GoalSelectionPathFailed;
				goalSelectionStrategy_ = GoalSelectionStrategyNone;
				if (decision != nullptr)
				{
					decision->linkFromArea = candidate.fromArea;
					decision->linkToArea = candidate.toArea;
					decision->linkDirection = candidate.direction;
					decision->linkHow = candidate.how;
					decision->locomotionResult = traversalStartResult;
					decision->failureReason =
						traversalStartResult == nav::LocomotionResult::UnsafeDrop
							? NavFailureReason::UnsafeDrop
							: NavFailureReason::NavApplyRejected;
					decision->goalSelectionReason = goalSelectionReason_;
					decision->goalSelectionStrategy = goalSelectionStrategy_;
					decision->stage = NavRoamStage::Failed;
				}
				return NavRoamResult::ReplanRequired;
			}
			break;
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
			lastIntentDirection_ = intent->direction;
			if (decision != nullptr)
			{
				decision->locomotionResult = nav::LocomotionResult::IntentReady;
				decision->stage = NavRoamStage::LocomotionReady;
				decision->targetArea = traversalIntent.landingArea;
			}
			return NavRoamResult::IntentReady;
		}
		if (traversalResult == nav::JumpDropResult::Ready)
		{
			intent->direction = lastIntentDirection_;
			intent->speed = kRoamSpeed;
			intent->posture = nav::LocomotionPosture::Standing;
			intent->traversal = nav::TraversalAction::Walk;
			intent->stepUp = false;
			intent->currentArea = activeLink_.fromArea;
			intent->targetArea = activeLink_.toArea;
			if (decision != nullptr)
			{
				decision->locomotionResult = nav::LocomotionResult::IntentReady;
				decision->stage = NavRoamStage::LocomotionReady;
				decision->targetArea = activeLink_.toArea;
				decision->intentDirection = intent->direction;
			}
			return NavRoamResult::IntentReady;
		}
		if (traversalResult == nav::JumpDropResult::Landed)
		{
			const bool goalReached =
				(decision != nullptr && decision->goalPresent &&
					decision->goalArea == currentArea.area) ||
				(hasRoamGoal_ && roamGoalArea_ == currentArea.area);
			if (goalReached)
			{
				goalSelectionReason_ = GoalSelectionReached;
			}
			resetRoute(!goalReached);
			if (decision != nullptr)
			{
				decision->locomotionResult = nav::LocomotionResult::TargetReached;
				decision->stage = goalReached
					? NavRoamStage::TargetReached
					: NavRoamStage::LinkSelection;
				if (goalReached)
				{
					decision->goalSelectionReason = GoalSelectionReached;
				}
			}
			return goalReached
				? NavRoamResult::TargetReached
				: NavRoamResult::ReplanRequired;
		}
		nav::LocomotionResult traversalFailure =
			nav::LocomotionResult::InvalidObservation;
		switch (traversalResult)
		{
		case nav::JumpDropResult::Unsafe:
			traversalFailure = nav::LocomotionResult::UnsafeDrop;
			break;
		case nav::JumpDropResult::InvalidSnapshot:
			traversalFailure = nav::LocomotionResult::InvalidSnapshot;
			break;
		case nav::JumpDropResult::InvalidCorridor:
		case nav::JumpDropResult::InvalidEnvelope:
			traversalFailure = nav::LocomotionResult::InvalidCorridor;
			break;
		case nav::JumpDropResult::Invalidated:
			traversalFailure = nav::LocomotionResult::StaleSnapshot;
			break;
		case nav::JumpDropResult::TimedOut:
		case nav::JumpDropResult::Inactive:
			traversalFailure = nav::LocomotionResult::Inactive;
			break;
		case nav::JumpDropResult::InvalidArgument:
			traversalFailure = nav::LocomotionResult::InvalidArgument;
			break;
		case nav::JumpDropResult::InvalidConfig:
			traversalFailure = nav::LocomotionResult::InvalidConfig;
			break;
		case nav::JumpDropResult::InvalidObservation:
		case nav::JumpDropResult::Ready:
		case nav::JumpDropResult::Emitted:
		case nav::JumpDropResult::Landed:
		default:
			break;
		}
		hasAvoidedLink_ = true;
		avoidedLink_ = activeLink_;
		if (traversalFailure == nav::LocomotionResult::UnsafeDrop)
		{
			rememberUnsafeDrop(observation, objectiveArea, activeLink_);
		}
		++nextLinkIndex_;
		if (objectiveArea == 0U && hasRoamGoal_)
		{
			failedRoamGoalArea_ = roamGoalArea_;
		}
		goalSelectionReason_ = GoalSelectionPathFailed;
		goalSelectionStrategy_ = GoalSelectionStrategyNone;
		resetRoute();
		if (decision != nullptr)
		{
			decision->locomotionResult = traversalFailure;
			decision->failureReason =
				traversalFailure == nav::LocomotionResult::UnsafeDrop
					? NavFailureReason::UnsafeDrop
					: NavFailureReason::NavApplyRejected;
			decision->goalSelectionReason = goalSelectionReason_;
			decision->goalSelectionStrategy = goalSelectionStrategy_;
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
			const bool goalReached =
				(decision != nullptr && decision->goalPresent &&
					decision->goalArea == currentArea.area) ||
				(hasRoamGoal_ && roamGoalArea_ == currentArea.area);
			if (goalReached)
			{
				goalSelectionReason_ = GoalSelectionReached;
			}
			resetRoute(!goalReached);
			if (decision != nullptr)
			{
				decision->locomotionResult = nav::LocomotionResult::TargetReached;
				decision->stage = goalReached
					? NavRoamStage::TargetReached
					: NavRoamStage::LinkSelection;
				if (goalReached)
				{
					decision->goalSelectionReason = GoalSelectionReached;
				}
			}
			return goalReached
				? NavRoamResult::TargetReached
				: NavRoamResult::ReplanRequired;
		}
		resetRoute();
		if (decision != nullptr)
		{
			decision->stage = NavRoamStage::Failed;
		}
		return NavRoamResult::ReplanRequired;
	}

	nav::LocomotionResult locomotionResult = locomotion_.update(
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
		if (activeCorridorIndex_ > 0U &&
			activeCorridorIndex_ - 1U < activeCorridor_.links.size())
		{
			const nav::NavDirectedLink currentLink =
				activeCorridor_.links[activeCorridorIndex_ - 1U];
			if (traversalActionFor(snapshot, currentLink) ==
				nav::TraversalAction::Walk)
			{
				activeLink_ = currentLink;
			}
		}
	}
	if (decision != nullptr)
	{
		decision->locomotionResult = locomotionResult;
		if (intent->targetArea != 0U)
		{
			decision->targetArea = intent->targetArea;
			decision->targetPosition = intent->targetPosition;
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
	{
		const bool goalReached =
			(decision != nullptr && decision->goalPresent &&
				decision->goalArea == currentArea.area) ||
			(hasRoamGoal_ && roamGoalArea_ == currentArea.area);
		if (goalReached)
		{
			goalSelectionReason_ = GoalSelectionReached;
		}
		resetRoute(!goalReached);
		if (decision != nullptr)
		{
			decision->stage = goalReached
				? NavRoamStage::TargetReached
				: NavRoamStage::LinkSelection;
			if (goalReached)
			{
				decision->goalSelectionReason = GoalSelectionReached;
			}
		}
		return goalReached
			? NavRoamResult::TargetReached
			: NavRoamResult::ReplanRequired;
	}
	case nav::LocomotionResult::UnsafeDrop:
		// The follower may have advanced past a safe prefix before the veto.
		// Capture its rejected transition even when it is a Drop, not a Walk.
		if (activeCorridorIndex_ > 0U &&
			activeCorridorIndex_ - 1U < activeCorridor_.links.size())
		{
			activeLink_ = activeCorridor_.links[activeCorridorIndex_ - 1U];
		}
		if (decision != nullptr)
		{
			decision->linkFromArea = activeLink_.fromArea;
			decision->linkToArea = activeLink_.toArea;
			decision->linkDirection = activeLink_.direction;
			decision->linkHow = activeLink_.how;
		}
		rememberUnsafeDrop(observation, objectiveArea, activeLink_);
		resetRoute();
		if (decision != nullptr)
		{
			decision->locomotionResult = nav::LocomotionResult::UnsafeDrop;
			decision->failureReason = NavFailureReason::UnsafeDrop;
			decision->stage = NavRoamStage::Failed;
		}
		return NavRoamResult::ReplanRequired;
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
				if (objectiveArea == 0U && hasRoamGoal_)
				{
					failedRoamGoalArea_ = roamGoalArea_;
				}
				resetRoute();
				if (decision != nullptr)
				{
					decision->failureReason = NavFailureReason::MovementNotProduced;
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
	case nav::LocomotionResult::InvalidCorridor:
		if (activeLink_.fromArea == currentArea.area &&
				activeLink_.toArea != currentArea.area)
		{
			hasAvoidedLink_ = true;
			avoidedLink_ = activeLink_;
			++nextLinkIndex_;
		}
		if (objectiveArea == 0U && hasRoamGoal_)
		{
			failedRoamGoalArea_ = roamGoalArea_;
		}
		resetRoute();
		if (decision != nullptr)
		{
			decision->recomputeReason = NavRecomputeReason::PathInvalidated;
			decision->failureReason = NavFailureReason::NavApplyRejected;
			decision->stage = NavRoamStage::Failed;
		}
		return NavRoamResult::ReplanRequired;
	case nav::LocomotionResult::InvalidArgument:
	case nav::LocomotionResult::InvalidConfig:
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
	if (reservationBoard_ != nullptr)
	{
		reservationBoard_->release(actor_);
	}
	offMeshRecovery_ = {};
	unsafeDropFailure_ = {};
	previousMovementSuppressed_ = false;
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
	lastVisitedArea_ = 0U;
	lastVisitedMapGeneration_ = 0U;
	lastVisitedTeam_ = 0U;
	roamGoalGeneration_ = 0U;
	goalSelectionReason_ = GoalSelectionNone;
	goalSelectionStrategy_ = GoalSelectionStrategyNone;
	maximumSafeDropHeight_ = 0.0f;
	safeDropHeightAvailable_ = false;
	unsafeDropRejected_ = false;
	hasRoamGoal_ = false;
	roamGoalArea_ = 0U;
	failedRoamGoalArea_ = 0U;
	roamGoalPosition_ = {0.0f, 0.0f, 0.0f};
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
	const nav::NavDirectedLink &link,
	nav::LocomotionResult *startResult,
	const nav::NavVector *launchPositionOverride,
	const nav::NavVector *landingPositionOverride)
{
	if (startResult != nullptr)
	{
		*startResult = nav::LocomotionResult::InvalidArgument;
	}
	unsafeDropRejected_ = false;
	const nav::NavDocument *document = snapshot.document();
	if (document == nullptr || corridor.areas.size() < 2U)
	{
		if (startResult != nullptr)
		{
			*startResult = document == nullptr
				? nav::LocomotionResult::InvalidSnapshot
				: nav::LocomotionResult::InvalidCorridor;
		}
		return false;
	}
	const nav::NavArea *fromArea = document->findArea(link.fromArea);
	const nav::NavArea *toArea = document->findArea(link.toArea);
	if (fromArea == nullptr || toArea == nullptr)
	{
		if (startResult != nullptr)
		{
			*startResult = nav::LocomotionResult::InvalidCorridor;
		}
		return false;
	}
	activeTraversal_ = traversalActionFor(snapshot, link);
	const nav::NavVector launchPosition = launchPositionOverride != nullptr
		? *launchPositionOverride
		: centerOf(*fromArea);
	const nav::NavVector landingPosition = landingPositionOverride != nullptr
		? *landingPositionOverride
		: centerOf(*toArea);
	const float verticalDelta = floorHeight(*toArea) - floorHeight(*fromArea);
	const float horizontalDeltaX = landingPosition.x - launchPosition.x;
	const float horizontalDeltaY = landingPosition.y - launchPosition.y;
	const float horizontalReach = std::sqrt(
		horizontalDeltaX * horizontalDeltaX + horizontalDeltaY * horizontalDeltaY);

	if (activeTraversal_ == nav::TraversalAction::Jump ||
		activeTraversal_ == nav::TraversalAction::Drop)
	{
		nav::NavCorridor traversalCorridor = corridor;
		traversalCorridor.areas = {link.fromArea, link.toArea};
		traversalCorridor.links = {link};
		nav::JumpDropEnvelope envelope = {};
		envelope.kind = activeTraversal_ == nav::TraversalAction::Jump
				? nav::JumpDropKind::Jump
				: nav::JumpDropKind::Drop;
		envelope.launch = {launchPosition, link.fromArea};
		envelope.landing = {landingPosition, link.toArea};
		envelope.maximumRise = (std::max)(16.0f, verticalDelta + 16.0f);
		envelope.maximumDrop = (std::max)(16.0f, -verticalDelta + 16.0f);
		envelope.horizontalReach = (std::max)(16.0f, horizontalReach + 16.0f);
		envelope.damageRisk = activeTraversal_ == nav::TraversalAction::Drop
			? nav::JumpDropDamageRisk{maximumSafeDropHeight_, 0.0f}
			: nav::JumpDropDamageRisk{4096.0f, 100.0f};
		const nav::JumpDropResult result =
			jumpDrop_.start(traversalCorridor, envelope, actor_.actorGeneration);
		if (startResult != nullptr)
		{
			switch (result)
			{
			case nav::JumpDropResult::Ready:
				*startResult = nav::LocomotionResult::Started;
				break;
			case nav::JumpDropResult::Unsafe:
				*startResult = nav::LocomotionResult::UnsafeDrop;
				break;
			case nav::JumpDropResult::InvalidObservation:
				*startResult = nav::LocomotionResult::InvalidObservation;
				break;
			case nav::JumpDropResult::InvalidSnapshot:
				*startResult = nav::LocomotionResult::InvalidSnapshot;
				break;
			case nav::JumpDropResult::InvalidConfig:
				*startResult = nav::LocomotionResult::InvalidConfig;
				break;
			case nav::JumpDropResult::InvalidArgument:
				*startResult = nav::LocomotionResult::InvalidArgument;
				break;
			case nav::JumpDropResult::Invalidated:
			case nav::JumpDropResult::TimedOut:
			case nav::JumpDropResult::Inactive:
				*startResult = nav::LocomotionResult::Inactive;
				break;
			default:
				*startResult = nav::LocomotionResult::InvalidCorridor;
				break;
			}
		}
		if (activeTraversal_ == nav::TraversalAction::Drop &&
			result != nav::JumpDropResult::Ready)
		{
			unsafeDropRejected_ = true;
		}
		return result == nav::JumpDropResult::Ready;
	}

	if (activeTraversal_ == nav::TraversalAction::Ladder)
	{
		nav::NavCorridor traversalCorridor = corridor;
		traversalCorridor.areas = {link.fromArea, link.toArea};
		traversalCorridor.links = {link};
		nav::SpecialTraversalCapability capability = {};
		capability.kind = nav::SpecialTraversalKind::Ladder;
		capability.entry = {launchPosition, link.fromArea};
		capability.exit = {landingPosition, link.toArea};
		capability.sourceDirection = link.direction;
		capability.requiredClearance = 36.0f;
		capability.minimumPosture = nav::SpecialTraversalPosture::Standing;
		const nav::SpecialTraversalResult result = specialTraversal_.start(
			traversalCorridor, capability, actor_.actorGeneration);
		if (startResult != nullptr)
		{
			*startResult = result == nav::SpecialTraversalResult::Ready
				? nav::LocomotionResult::Started
				: nav::LocomotionResult::InvalidCorridor;
		}
		return result == nav::SpecialTraversalResult::Ready;
	}

	const nav::LocomotionResult result = locomotion_.start(corridor);
	if (startResult != nullptr)
	{
		*startResult = result;
	}
	return result == nav::LocomotionResult::Started;
}

bool NavRoamController::startRoute(
	const nav::NavSnapshot &snapshot,
	const nav::NavCorridor &corridor,
	const nav::NavDirectedLink &link,
	nav::LocomotionResult *startResult)
{
	if (traversalActionFor(snapshot, link) == nav::TraversalAction::Walk)
	{
		return startTraversal(snapshot, corridor, link, startResult);
	}
	activeTraversal_ = nav::TraversalAction::Walk;
	const nav::LocomotionResult result = locomotion_.start(corridor);
	if (startResult != nullptr)
	{
		*startResult = result;
	}
	return result == nav::LocomotionResult::Started;
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
		if (hasAvoidedLink_ && link.fromArea == avoidedLink_.fromArea &&
				link.toArea == avoidedLink_.toArea)
		{
			nav::NavCorridor alternativeCorridor = {};
			const nav::NavQueryResult alternativeResult =
				query.buildAlternativeCorridor(
					corridor, avoidedLink_, &alternativeCorridor,
					decision == nullptr || !collectPathStats_
						? nullptr
						: &decision->pathSearchStats);
			if (decision != nullptr)
			{
				decision->corridorResult = alternativeResult;
				decision->pathResult = alternativeResult;
			}
			if (alternativeResult != nav::NavQueryResult::Found ||
					alternativeCorridor.links.empty())
			{
				if (decision != nullptr)
				{
					decision->linkFromArea = avoidedLink_.fromArea;
					decision->linkToArea = avoidedLink_.toArea;
					decision->linkDirection = avoidedLink_.direction;
					decision->linkHow = avoidedLink_.how;
					decision->linkResult = nav::NavQueryResult::Found;
					decision->failureReason = NavFailureReason::PathSearchFailed;
					decision->stage = NavRoamStage::Failed;
				}
				return false;
			}
			corridor = std::move(alternativeCorridor);
			link = corridor.links.front();
		}
		if (decision != nullptr)
		{
			decision->linkFromArea = link.fromArea;
			decision->linkToArea = link.toArea;
			decision->linkDirection = link.direction;
			decision->linkHow = link.how;
			decision->linkResult = nav::NavQueryResult::Found;
		}
		nav::LocomotionResult startResult =
			nav::LocomotionResult::InvalidArgument;
		if (startRoute(snapshot, corridor, link, &startResult))
		{
			hasAvoidedLink_ = false;
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
		hasAvoidedLink_ = true;
		avoidedLink_ = link;
		++nextLinkIndex_;
		if (decision != nullptr)
		{
			decision->linkFromArea = link.fromArea;
			decision->linkToArea = link.toArea;
			decision->linkDirection = link.direction;
			decision->linkHow = link.how;
			decision->linkResult = nav::NavQueryResult::Found;
			decision->locomotionResult = startResult;
			decision->failureReason =
				startResult == nav::LocomotionResult::UnsafeDrop
					? NavFailureReason::UnsafeDrop
					: NavFailureReason::NavApplyRejected;
			decision->stage = NavRoamStage::Failed;
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
			if (!startRoute(snapshot, corridor, links[linkIndex]))
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
				!startRoute(snapshot, corridor, {currentArea.area, candidate.id, 0U, 0U}))
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

bool NavRoamController::selectCompatibilityGoal(
	const nav::NavSnapshot &snapshot,
	const nav::NavAreaMatch &currentArea,
	const std::vector<nav::NavDirectedLink> &links,
	NavRoamDecision *decision)
{
	const nav::NavDocument *document = snapshot.document();
	if (document == nullptr)
	{
		return false;
	}
	const bool goalReached = hasRoamGoal_ && roamGoalArea_ == currentArea.area;
	nav::NavCorridor selectedCorridor = {};
	bool hasSelectedCorridor = false;
	bool reservationFallback = false;
	bool reservationBudgetExhausted = false;
	NavRoamReservationFallbackReason reservationFallbackReason =
		NavRoamReservationFallbackReason::None;
	if (!hasRoamGoal_ || goalReached)
	{
		if (goalReached && reservationBoard_ != nullptr)
		{
			reservationBoard_->release(actor_);
		}
		NavGoalSelectionReason reason = goalReached
			? GoalSelectionReached
			: goalSelectionReason_ == GoalSelectionPathFailed
				? GoalSelectionPathFailed
				: goalSelectionReason_ == GoalSelectionReached
					? GoalSelectionReached
				: GoalSelectionInitial;
		NavGoalSelectionStrategy strategy =
			GoalSelectionStrategyOldestVisitedArea;
		nav::AreaId selectedGoal = 0U;
		std::uint32_t oldestVisit = (std::numeric_limits<std::uint32_t>::max)();
		for (const nav::NavArea &candidate : document->areas())
		{
			if (candidate.id == currentArea.area ||
					(goalSelectionReason_ == GoalSelectionPathFailed &&
						failedRoamGoalArea_ != 0U &&
						candidate.id == failedRoamGoalArea_))
			{
				continue;
			}
			if (candidate.extent.hi.x - candidate.extent.lo.x < 150.0f ||
				candidate.extent.hi.y - candidate.extent.lo.y < 150.0f)
			{
				continue;
			}
			const std::uint32_t lastVisited = areaVisitHistory_ == nullptr
				? 0U
				: areaVisitHistory_->lastVisited(
					lastVisitedMapGeneration_, lastVisitedTeam_, candidate.id);
			if (selectedGoal == 0U || lastVisited < oldestVisit)
			{
				selectedGoal = candidate.id;
			oldestVisit = lastVisited;
		}
	}
	if (reservationBoard_ != nullptr)
	{
		selectedGoal = 0U;
		struct GoalCandidate
		{
			nav::AreaId area;
			std::uint32_t lastVisited;
		};
		std::vector<GoalCandidate> candidates;
		candidates.reserve(document->areas().size());
		for (const nav::NavArea &candidate : document->areas())
		{
			if (candidate.id == currentArea.area ||
				(goalSelectionReason_ == GoalSelectionPathFailed &&
					failedRoamGoalArea_ != 0U &&
					candidate.id == failedRoamGoalArea_) ||
				candidate.extent.hi.x - candidate.extent.lo.x < 150.0f ||
				candidate.extent.hi.y - candidate.extent.lo.y < 150.0f)
			{
				continue;
			}
			candidates.push_back({
				candidate.id,
				areaVisitHistory_ == nullptr
					? 0U
					: areaVisitHistory_->lastVisited(
						lastVisitedMapGeneration_, lastVisitedTeam_, candidate.id)});
		}
		std::stable_sort(candidates.begin(), candidates.end(),
			[](const GoalCandidate &left, const GoalCandidate &right) {
				return left.lastVisited < right.lastVisited;
			});

		nav::NavQuery query(snapshot);
		nav::AreaId fallbackGoal = 0U;
		std::uint32_t fallbackVisit =
			(std::numeric_limits<std::uint32_t>::max)();
		int fallbackConflictCost = 3;
		nav::NavCorridor fallbackCorridor = {};
		std::size_t evaluatedCandidatePaths = 0U;
		constexpr std::size_t kMaximumCandidatePaths = 8U;
		for (const GoalCandidate &candidate : candidates)
		{
			if (reservationBoard_->goalReservedByOther(
					lastVisitedTeam_, actor_, candidate.area))
			{
				continue;
			}
		if (evaluatedCandidatePaths >= kMaximumCandidatePaths)
		{
			reservationBudgetExhausted = true;
			break;
			}
			++evaluatedCandidatePaths;

			nav::NavCorridor candidateCorridor = {};
			const nav::NavQueryResult candidateResult = buildCorridor(
				query, currentArea.area, candidate.area, nav::NavRouteType::Safest,
				&candidateCorridor,
				decision == nullptr || !collectPathStats_
					? nullptr
					: &decision->pathSearchStats);
			if (candidateResult != nav::NavQueryResult::Found ||
				candidateCorridor.areas.size() < 2U ||
				candidateCorridor.links.empty())
			{
				continue;
			}

			const nav::NavDirectedLink firstLink = candidateCorridor.links.front();
			const bool firstLinkReserved =
				reservationBoard_->firstLinkReservedByOther(
					lastVisitedTeam_, actor_, firstLink.fromArea, firstLink.toArea);
			if (!firstLinkReserved)
			{
				selectedGoal = candidate.area;
				oldestVisit = candidate.lastVisited;
				selectedCorridor = std::move(candidateCorridor);
				hasSelectedCorridor = true;
				break;
			}
			if (fallbackConflictCost > 1)
			{
				fallbackGoal = candidate.area;
				fallbackVisit = candidate.lastVisited;
				fallbackConflictCost = 1;
				fallbackCorridor = std::move(candidateCorridor);
			}
		}

		if (selectedGoal == 0U && fallbackGoal != 0U)
		{
			selectedGoal = fallbackGoal;
			oldestVisit = fallbackVisit;
			selectedCorridor = std::move(fallbackCorridor);
			hasSelectedCorridor = true;
			reservationFallback = true;
			reservationFallbackReason = reservationBudgetExhausted
				? NavRoamReservationFallbackReason::CandidateBudgetExhausted
				: NavRoamReservationFallbackReason::NoDistinctRouteFound;
		}

		if (selectedGoal == 0U)
		{
			// If every goal has an owner, allow one oldest reachable shared route.
			for (const GoalCandidate &candidate : candidates)
			{
				if (evaluatedCandidatePaths >= kMaximumCandidatePaths)
				{
					break;
				}
				if (!reservationBoard_->goalReservedByOther(
						lastVisitedTeam_, actor_, candidate.area))
				{
					continue;
				}
				++evaluatedCandidatePaths;
				nav::NavCorridor candidateCorridor = {};
				const nav::NavQueryResult candidateResult = buildCorridor(
					query, currentArea.area, candidate.area, nav::NavRouteType::Safest,
					&candidateCorridor,
					decision == nullptr || !collectPathStats_
						? nullptr
						: &decision->pathSearchStats);
				if (candidateResult == nav::NavQueryResult::Found &&
					candidateCorridor.areas.size() >= 2U &&
					!candidateCorridor.links.empty())
				{
					selectedGoal = candidate.area;
					oldestVisit = candidate.lastVisited;
					selectedCorridor = std::move(candidateCorridor);
					hasSelectedCorridor = true;
					reservationFallback = true;
					reservationFallbackReason = reservationBudgetExhausted
						? NavRoamReservationFallbackReason::CandidateBudgetExhausted
						: NavRoamReservationFallbackReason::NoDistinctRouteFound;
					break;
				}
			}
		}
	}
	if (selectedGoal == 0U)
	{
			strategy = GoalSelectionStrategyRandomFallback;
			if (document->areas().empty() || randomSource_ == nullptr)
			{
				hasRoamGoal_ = false;
				goalSelectionReason_ = document->areas().empty()
					? GoalSelectionNoEligibleArea
					: reason;
				goalSelectionStrategy_ = document->areas().empty()
					? GoalSelectionStrategyNoEligibleArea
					: strategy;
				if (decision != nullptr)
				{
					decision->goalSelectionReason = goalSelectionReason_;
					decision->goalSelectionStrategy = goalSelectionStrategy_;
					decision->failureReason = NavFailureReason::NoGoal;
				}
				return false;
			}
			const compat::RandomRequest request = compat::RandomRequest::longRequest(
				"CSBOT-HUNT-GOAL",
				{actor_.slot, actor_.actorGeneration},
				{0U, 0U, decision == nullptr ? 0U : decision->fullUpdateSequence},
				0, static_cast<std::int32_t>(document->areas().size() - 1U));
			const compat::RandomLongResult result = randomSource_->nextLong(request);
			if (result.status != compat::RandomStatus::Ok || result.value < 0 ||
				static_cast<std::size_t>(result.value) >= document->areas().size())
			{
				hasRoamGoal_ = false;
				goalSelectionReason_ = GoalSelectionNoEligibleArea;
				goalSelectionStrategy_ = GoalSelectionStrategyRandomFallback;
				if (decision != nullptr)
				{
					decision->goalSelectionReason = goalSelectionReason_;
					decision->goalSelectionStrategy = goalSelectionStrategy_;
					decision->failureReason = NavFailureReason::NoGoal;
				}
				return false;
			}
			selectedGoal = document->areas()[static_cast<std::size_t>(result.value)].id;
		}
		roamGoalArea_ = selectedGoal;
		const nav::NavArea *goal = document->findArea(roamGoalArea_);
		roamGoalPosition_ = goal == nullptr
			? nav::NavVector{0.0f, 0.0f, 0.0f}
			: centerOf(*goal);
		hasRoamGoal_ = goal != nullptr;
		if (!hasRoamGoal_)
		{
			goalSelectionReason_ = GoalSelectionNoEligibleArea;
			goalSelectionStrategy_ = GoalSelectionStrategyNoEligibleArea;
			if (decision != nullptr)
			{
				decision->goalSelectionReason = goalSelectionReason_;
				decision->goalSelectionStrategy = goalSelectionStrategy_;
				decision->failureReason = NavFailureReason::NoGoal;
			}
			return false;
		}
		++roamGoalGeneration_;
		if (roamGoalGeneration_ == 0U)
		{
			++roamGoalGeneration_;
		}
		goalSelectionReason_ = reason;
		goalSelectionStrategy_ = strategy;
	}

	if (!hasRoamGoal_ || roamGoalArea_ == currentArea.area)
	{
		hasRoamGoal_ = false;
		return false;
	}
	if (decision != nullptr)
	{
		decision->goalSelectionReason = goalSelectionReason_;
		decision->goalSelectionStrategy = goalSelectionStrategy_;
		decision->goalGeneration = roamGoalGeneration_;
	}

	nav::NavQuery query(snapshot);
	if (decision != nullptr)
	{
		decision->goalPresent = true;
		decision->goalKind = NavGoalKind::Roam;
		decision->goalArea = roamGoalArea_;
		decision->goalPosition = roamGoalPosition_;
		decision->pathRequested = true;
	}
	nav::NavCorridor corridor = std::move(selectedCorridor);
	const nav::NavQueryResult corridorResult = hasSelectedCorridor
		? nav::NavQueryResult::Found
		: buildCorridor(
			query, currentArea.area, roamGoalArea_, nav::NavRouteType::Safest,
			&corridor,
			decision == nullptr || !collectPathStats_
				? nullptr
				: &decision->pathSearchStats);
	if (decision != nullptr)
	{
		decision->corridorResult = corridorResult;
		decision->pathResult = corridorResult;
	}
	if (corridorResult != nav::NavQueryResult::Found || corridor.areas.size() < 2U)
	{
		failedRoamGoalArea_ = roamGoalArea_;
		hasRoamGoal_ = false;
		goalSelectionReason_ = GoalSelectionPathFailed;
		goalSelectionStrategy_ = GoalSelectionStrategyNone;
		if (decision != nullptr)
		{
			decision->goalSelectionReason = goalSelectionReason_;
			decision->goalSelectionStrategy = goalSelectionStrategy_;
			decision->failureReason = NavFailureReason::PathSearchFailed;
		}
		return false;
	}
	nav::AreaId nextArea = corridor.areas[1U];
	nav::NavDirectedLink link = {currentArea.area, nextArea, 0U, 0U};
	for (const nav::NavDirectedLink &candidate : links)
	{
		if (candidate.toArea == nextArea)
		{
			link = candidate;
			break;
		}
	}
	if (hasAvoidedLink_ && avoidedLink_.fromArea == currentArea.area &&
			link.fromArea == avoidedLink_.fromArea &&
			link.toArea == avoidedLink_.toArea)
	{
		nav::NavCorridor alternativeCorridor = {};
		const nav::NavQueryResult alternativeResult =
			query.buildAlternativeCorridor(
				corridor, avoidedLink_, &alternativeCorridor,
				decision == nullptr || !collectPathStats_
					? nullptr
					: &decision->pathSearchStats);
		if (alternativeResult != nav::NavQueryResult::Found ||
				alternativeCorridor.links.empty() ||
				(alternativeCorridor.links.front().fromArea == avoidedLink_.fromArea &&
				 alternativeCorridor.links.front().toArea == avoidedLink_.toArea))
		{
			failedRoamGoalArea_ = roamGoalArea_;
			hasRoamGoal_ = false;
			goalSelectionReason_ = GoalSelectionPathFailed;
			goalSelectionStrategy_ = GoalSelectionStrategyNone;
			if (decision != nullptr)
			{
				decision->corridorResult = alternativeResult;
				decision->pathResult = alternativeResult;
				decision->linkFromArea = avoidedLink_.fromArea;
				decision->linkToArea = avoidedLink_.toArea;
				decision->linkDirection = avoidedLink_.direction;
				decision->linkHow = avoidedLink_.how;
				decision->linkResult = nav::NavQueryResult::Found;
				decision->goalSelectionReason = goalSelectionReason_;
				decision->goalSelectionStrategy = goalSelectionStrategy_;
				decision->failureReason = NavFailureReason::PathSearchFailed;
			}
			return false;
		}
		corridor = std::move(alternativeCorridor);
		link = corridor.links.front();
		nextArea = link.toArea;
	}
	if (decision != nullptr)
	{
		decision->linkFromArea = link.fromArea;
		decision->linkToArea = link.toArea;
		decision->linkDirection = link.direction;
		decision->linkHow = link.how;
		decision->linkResult = nav::NavQueryResult::Found;
	}
	if (!startRoute(snapshot, corridor, link))
	{
		failedRoamGoalArea_ = roamGoalArea_;
		hasRoamGoal_ = false;
		goalSelectionReason_ = GoalSelectionPathFailed;
		goalSelectionStrategy_ = GoalSelectionStrategyNone;
		if (decision != nullptr)
		{
			decision->linkFromArea = link.fromArea;
			decision->linkToArea = link.toArea;
			decision->linkDirection = link.direction;
			decision->linkHow = link.how;
			decision->linkResult = nav::NavQueryResult::Found;
			decision->goalSelectionReason = goalSelectionReason_;
			decision->goalSelectionStrategy = goalSelectionStrategy_;
			decision->failureReason = unsafeDropRejected_
				? NavFailureReason::UnsafeDrop
				: NavFailureReason::NavApplyRejected;
		}
		return false;
	}
	if (reservationBoard_ != nullptr)
	{
		reservationFallback =
			reservationBoard_->goalReservedByOther(
				lastVisitedTeam_, actor_, roamGoalArea_) ||
			reservationBoard_->firstLinkReservedByOther(
				lastVisitedTeam_, actor_, link.fromArea, link.toArea);
		if (reservationFallback &&
			reservationFallbackReason == NavRoamReservationFallbackReason::None)
		{
			reservationFallbackReason = reservationBudgetExhausted
				? NavRoamReservationFallbackReason::CandidateBudgetExhausted
				: NavRoamReservationFallbackReason::NoDistinctRouteFound;
		}
		reservationBoard_->reserve(
			lastVisitedTeam_, actor_, roamGoalArea_, link.fromArea, link.toArea);
		if (decision != nullptr)
		{
			decision->reservationFallback = reservationFallback;
			decision->reservationFallbackReason =
				reservationFallbackReason;
		}
	}
	rememberRoute(snapshot, corridor, link);
	failedRoamGoalArea_ = 0U;
	hasAvoidedLink_ = false;
	if (decision != nullptr)
	{
		decision->targetArea = nextArea;
		populateRouteDecision(decision);
	}
	return true;
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
		return selectCompatibilityGoal(snapshot, currentArea, links, decision);
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
					!startRoute(snapshot, corridor, link))
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
		if (!startRoute(snapshot, corridor, link))
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
	const bool hadGoal = decision->goalPresent;
	decision->goalPresent = true;
	if (!hadGoal)
	{
		decision->goalKind = hasObjectiveTarget_ ? NavGoalKind::Objective : NavGoalKind::Roam;
		decision->goalArea = activeLink_.toArea;
		decision->goalPosition = activeTargetPosition_;
	}
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

void NavRoamController::resetRoute(bool preserveRoamGoal)
{
	if (!preserveRoamGoal && reservationBoard_ != nullptr)
	{
		reservationBoard_->release(actor_);
	}
	locomotion_ = nav::LocomotionController(roamLocomotionConfig());
	activeTraversal_ = nav::TraversalAction::Walk;
	jumpDrop_ = nav::JumpDropController(roamJumpDropConfig(
			modePolicy_.allowsAdaptiveRouteWeighting()
				? compat::RuntimeMode::Enhanced
				: compat::RuntimeMode::Compatibility));
	specialTraversal_ = nav::SpecialTraversalController();
	activeCorridor_ = {};
	activeLink_ = {};
	activeTargetPosition_ = {0.0f, 0.0f, 0.0f};
	if (!preserveRoamGoal)
	{
		hasRoamGoal_ = false;
		roamGoalArea_ = 0U;
		roamGoalPosition_ = {0.0f, 0.0f, 0.0f};
	}
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
