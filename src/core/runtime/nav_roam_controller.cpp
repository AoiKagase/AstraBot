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
// Timed Compatibility drops use sixty30Hz command ticks (two seconds).
// Other traversals retain their existing raw-frame budget.
constexpr std::uint32_t kCompatibilityTraversalFrameBudget = 60U;
// CSBot b0889847 cs_bot_pathfind.cpp:1550-1577 gives up after five
// seconds without path-area progress, even when local movement avoids Stuck.
constexpr float kCompatibilityCorridorProgressTimeout = 5.0f;

nav::LocomotionConfig roamLocomotionConfig(compat::RuntimeMode mode)
{
	return {32.0f, 20.0f, 64.0f, 16.0f, 240.0f, 8U,
		mode == compat::RuntimeMode::Compatibility};
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
	  locomotion_(roamLocomotionConfig(mode)),
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
	roamGoalState().lastVisitedArea = 0U;
	roamGoalState().lastVisitedMapGeneration = 0U;
	roamGoalState().lastVisitedTeam = 0U;
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

void NavRoamController::setPathCostContext(const nav::NavPathCostContext &context)
{
	routeState().pathCostContext = context;
	routeState().hasPathCostContext = true;
}

void NavRoamController::setRuntimeMode(compat::RuntimeMode mode)
{
	if (mode == modePolicy_.mode()) return;
	if (reservationBoard_ != nullptr) reservationBoard_->release(actor_);
	modePolicy_ = compat::RuntimeModePolicy(mode);
	timedCompatibility_=false;
	resetRoute();
	recoveryState().nextPathAttemptAt = 0.0f;
	traversalState().jumpDrop = nav::JumpDropController(roamJumpDropConfig(mode));
}

bool NavRoamController::isPathFailureBackedOff(
	std::uint32_t mapGeneration,
	nav::AreaId startArea,
	nav::AreaId goalArea,
	nav::NavRouteType routeType,
	std::uint32_t frame) const
{
	return !timedCompatibility_ && recoveryState().hasPathFailure &&
		recoveryState().pathFailureKey.mapGeneration == mapGeneration &&
		recoveryState().pathFailureKey.startArea == startArea &&
		recoveryState().pathFailureKey.goalArea == goalArea &&
		recoveryState().pathFailureKey.routeType == routeType &&
		frame < recoveryState().pathFailureRetryFrame;
}

void NavRoamController::rememberPathFailure(
	std::uint32_t mapGeneration,
	nav::AreaId startArea,
	nav::AreaId goalArea,
	nav::NavRouteType routeType,
	std::uint32_t frame)
{
	const PathFailureKey key = {mapGeneration, startArea, goalArea, routeType};
	if (!recoveryState().hasPathFailure || recoveryState().pathFailureKey.mapGeneration != key.mapGeneration ||
		recoveryState().pathFailureKey.startArea != key.startArea ||
		recoveryState().pathFailureKey.goalArea != key.goalArea ||
		recoveryState().pathFailureKey.routeType != key.routeType)
	{
		recoveryState().pathFailureBackoffFrames = kInitialPathFailureBackoffFrames;
	}
	else
	{
		recoveryState().pathFailureBackoffFrames = (std::min)(
			kMaximumPathFailureBackoffFrames,
			recoveryState().pathFailureBackoffFrames * 2U);
	}
	recoveryState().pathFailureKey = key;
	recoveryState().hasPathFailure = true;
	recoveryState().pathFailureRetryFrame = frame + recoveryState().pathFailureBackoffFrames;
}

void NavRoamController::rememberUnsafeDrop(
	const NavRoamObservation &observation, nav::AreaId goalArea,
	const nav::NavDirectedLink &link)
{
	routeState().hasAvoidedLink = true;
	routeState().avoidedLink = link;
	if (!observation.hasObjectiveTarget) return;
	if (recoveryState().unsafeDropFailure.valid &&
		recoveryState().unsafeDropFailure.link.fromArea == link.fromArea &&
		recoveryState().unsafeDropFailure.link.toArea == link.toArea)
	{
		delayUnsafeDropRetry();
		// A discarded bypass can return the actor to the same unsafe edge.
		// Each new physical rejection must permit another directed bypass;
		// otherwise timed retries repeatedly choose only the rejected edge.
		if (timedCompatibility_) recoveryState().unsafeDropFailure.bypassPending = true;
		return;
	}
	recoveryState().unsafeDropFailure = {true, true, observation.objectiveTargetEntity,
		observation.locomotion.position, goalArea, link, 0U,
		kInitialUnsafeDropBackoffFrames};
	// Allow one immediate attempt through the existing directed bypass flow.
}

void NavRoamController::delayUnsafeDropRetry()
{
	recoveryState().unsafeDropFailure.retryFramesRemaining = recoveryState().unsafeDropFailure.nextBackoffFrames;
	recoveryState().unsafeDropFailure.nextBackoffFrames = (std::min)(
		kMaximumUnsafeDropBackoffFrames, recoveryState().unsafeDropFailure.nextBackoffFrames * 2U);
}

void NavRoamController::clearPathFailure()
{
	recoveryState().hasPathFailure = false;
	recoveryState().pathFailureKey = {0U, 0U, 0U, nav::NavRouteType::Fastest};
	recoveryState().pathFailureRetryFrame = 0U;
	recoveryState().pathFailureBackoffFrames = kInitialPathFailureBackoffFrames;
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
	const NavRoamResult result = updateInternal(snapshot, observation, intent, decision);
	if (friendWaitState().friendRepathPending)
	{
		friendWaitState().friendRepathPending = false;
		// CSBot destroys its path on patience expiry but still moves toward the
		// saved goal this update. Preserve the safety-checked intent, then replan.
		resetRoute(true);
		if (decision != nullptr && result == NavRoamResult::IntentReady)
			decision->recomputeReason = NavRecomputeReason::PathInvalidated;
	}
	if (result == NavRoamResult::IntentReady && intent != nullptr &&
		intent->traversal == nav::TraversalAction::Jump &&
		observation.locomotion.timingAvailable)
		recoveryState().lastWiggleJumpAt = observation.locomotion.observedAt;
	return result;
}

NavRoamResult NavRoamController::updateInternal(const nav::NavSnapshot &snapshot,
												const NavRoamObservation &observation,
												nav::LocomotionIntent *intent,
												NavRoamDecision *decision)
{
	{
		const auto phase = validateNavigationUpdate(snapshot, observation, intent, decision);
		if (phase.complete)
		{
			return phase.result;
		}
	}
	{
		const auto phase = updateFriendWaiting(snapshot, observation, decision);
		if (phase.complete)
		{
			return phase.result;
		}
	}
	nav::NavQuery query(snapshot);
	NavigationUpdateContext ctx{snapshot, observation, intent, decision, query};
	{
		const auto phase = locateUpdateAreas(ctx);
		if (phase.complete)
		{
			return phase.result;
		}
	}
	{
		const auto phase = recoverOffMeshUpdate(ctx);
		if (phase.complete)
		{
			return phase.result;
		}
	}
	{
		const auto phase = recoverStuckUpdate(ctx);
		if (phase.complete)
		{
			return phase.result;
		}
	}
	{
		const auto phase = selectUpdateRoute(ctx);
		if (phase.complete)
		{
			return phase.result;
		}
	}
	{
		const auto phase = startUpdateTraversal(ctx);
		if (phase.complete)
		{
			return phase.result;
		}
	}
	{
		const auto phase = updateJumpDropTraversal(ctx);
		if (phase.complete)
		{
			return phase.result;
		}
	}
	{
		const auto phase = updateSpecialTraversal(ctx);
		if (phase.complete)
		{
			return phase.result;
		}
	}
	return updateGroundLocomotion(ctx).result;
}

bool NavRoamController::expireGroundRoute(const NavRoamObservation &observation,
										  nav::LocomotionIntent *intent, NavRoamDecision *decision)
{
	if (!routeState().hasActiveRoute || !timedCompatibility_ ||
		friendWaitState().friendRepathPending || !observation.locomotion.grounded ||
		observation.airborne || observation.locomotion.onLadder ||
		traversalState().jumpDrop.isActive() || traversalState().specialTraversal.isActive() ||
		observedAt_ - corridorProgressAt_ <= kCompatibilityCorridorProgressTimeout)
		return false;
	// CSBot's five-second progress timeout destroys the corridor without
	// blacklisting its first edge. Otherwise the sole legal B-site approach
	// 157->823->643 becomes a permanent NoRoute on every subsequent retry.
	// Physical traversal failures and unsafe-drop exclusions are separate.
	routeState().hasAvoidedLink = false;
	routeState().avoidedLink = {};
	++routeState().nextLinkIndex;
	if (!observation.hasObjectiveTarget && roamGoalState().hasRoamGoal)
		roamGoalState().failedRoamGoalArea = roamGoalState().roamGoalArea;
	resetRoute();
	roamGoalState().goalSelectionReason = GoalSelectionPathFailed;
	roamGoalState().goalSelectionStrategy = GoalSelectionStrategyNone;
	if (decision != nullptr)
	{
		decision->locomotionResult = nav::LocomotionResult::Stuck;
		decision->failureReason = NavFailureReason::MovementNotProduced;
		decision->goalSelectionReason = roamGoalState().goalSelectionReason;
		decision->goalSelectionStrategy = roamGoalState().goalSelectionStrategy;
		decision->stage = NavRoamStage::Failed;
	}
	*intent = {};
	return true;
}

NavRoamController::NavigationPhaseResult NavRoamController::validateNavigationUpdate(
	const nav::NavSnapshot &snapshot, const NavRoamObservation &observation,
	nav::LocomotionIntent *intent, NavRoamDecision *decision)
{
	initializeDecision(decision);
	if (intent == nullptr)
	{
		if (decision != nullptr)
		{
			decision->stage = NavRoamStage::Failed;
		}
		return {true, NavRoamResult::InvalidArgument};
	}
	*intent = {};
	if (!snapshot.isValid())
	{
		return {true, NavRoamResult::InvalidSnapshot};
	}
	if (!isValidObservation(observation))
	{
		return {true, NavRoamResult::InvalidObservation};
	}
	collectPathStats_ = observation.collectPathStats;
	if (observation.frame.mapGeneration != snapshot.mapGeneration())
	{
		return {true, NavRoamResult::InvalidSnapshot};
	}
	if (decision != nullptr)
	{
		decision->fullUpdateSequence = observation.frame.tick;
	}

	if (!initialized_)
	{
		actor_ = observation.actor;
		routeState().nextLinkIndex =
			static_cast<std::size_t>(observation.actor.slot) +
			static_cast<std::size_t>(observation.actor.actorGeneration - 1U) * 17U;
		initialized_ = true;
	}
	else if (!sameActor(actor_, observation.actor))
	{
		if (actor_.slot != observation.actor.slot ||
			observation.actor.actorGeneration <= actor_.actorGeneration)
		{
			return {true, NavRoamResult::StaleActor};
		}
		reset();
		actor_ = observation.actor;
		routeState().nextLinkIndex =
			static_cast<std::size_t>(observation.actor.slot) +
			static_cast<std::size_t>(observation.actor.actorGeneration - 1U) * 17U;
		initialized_ = true;
	}

	if (lastFrame_.isValid())
	{
		if (sameFrame(observation.frame, lastFrame_))
		{
			return {true, NavRoamResult::DuplicateFrame};
		}
		if (!isFrameAfter(observation.frame, lastFrame_))
		{
			return {true, NavRoamResult::StaleFrame};
		}
		if (observation.frame.mapGeneration != lastFrame_.mapGeneration ||
			observation.frame.roundGeneration != lastFrame_.roundGeneration)
		{
			traversalState().locomotion.resetProgressMonitor();
			recoveryState().offMeshRecovery = {};
			recoveryState().unsafeDropFailure = {};
			clearPathFailure();
			recoveryState().nextPathAttemptAt = 0.0f;
			recoveryState().lastWiggleJumpAt = -10000.0f;
			recoveryState().stuckJumpScheduled = false;
			routeState().hasAvoidedLink = false;
			routeState().avoidedLink = {0U, 0U, 0U, 0U};
			roamGoalState().failedRoamGoalArea = 0U;
			if (decision != nullptr)
			{
				decision->recomputeReason = NavRecomputeReason::MapOrRoundChanged;
			}
			resetRoute();
		}
	}
	timedCompatibility_ = modePolicy_.mode() == compat::RuntimeMode::Compatibility &&
						  observation.locomotion.timingAvailable;
	observedAt_ = observation.locomotion.observedAt;
	viewYawAvailable_ = observation.locomotion.viewYawAvailable;
	viewYaw_ = observation.locomotion.viewYaw;
	latestLocomotion_ = observation.locomotion;
	if (recoveryState().unsafeDropFailure.valid)
	{
		const float dx =
			observation.locomotion.position.x - recoveryState().unsafeDropFailure.anchor.x;
		const float dy =
			observation.locomotion.position.y - recoveryState().unsafeDropFailure.anchor.y;
		const bool newBomb =
			observation.hasObjectiveTarget && observation.objectiveTargetEntity.isValid() &&
			!(observation.objectiveTargetEntity == recoveryState().unsafeDropFailure.entity);
		const bool madeProgress =
			observation.locomotion.grounded &&
			dx * dx + dy * dy >= kUnsafeDropProgressDistance * kUnsafeDropProgressDistance;
		if (newBomb || madeProgress)
		{
			if (newBomb)
				resetRoute();
			recoveryState().unsafeDropFailure = {};
			routeState().hasAvoidedLink = false;
			routeState().avoidedLink = {};
			roamGoalState().failedRoamGoalArea = 0U;
			clearPathFailure();
		}
		else if (lastFrame_.isValid() && !observation.movementSuppressed &&
				 !previousMovementSuppressed_)
		{
			const std::uint32_t elapsed = observation.frame.tick - lastFrame_.tick;
			recoveryState().unsafeDropFailure.retryFramesRemaining -=
				(std::min)(elapsed, recoveryState().unsafeDropFailure.retryFramesRemaining);
		}
	}
	previousMovementSuppressed_ = observation.movementSuppressed;
	const bool objectiveTargetChanged =
		roamGoalState().hasObjectiveTarget != observation.hasObjectiveTarget ||
		(observation.hasObjectiveTarget &&
		 (std::fabs(roamGoalState().objectiveTarget.x - observation.objectiveTarget.x) > 0.5f ||
		  std::fabs(roamGoalState().objectiveTarget.y - observation.objectiveTarget.y) > 0.5f ||
		  std::fabs(roamGoalState().objectiveTarget.z - observation.objectiveTarget.z) > 0.5f));
	if (objectiveTargetChanged)
	{
		// Goal reassignment cannot make a failed recovery point traversable.
		// Retain candidate exclusions until movement/lifecycle provides new evidence.
		if (!recoveryState().unsafeDropFailure.valid)
		{
			clearPathFailure();
			routeState().hasAvoidedLink = false;
			routeState().avoidedLink = {0U, 0U, 0U, 0U};
			roamGoalState().failedRoamGoalArea = 0U;
		}
		if (decision != nullptr)
		{
			decision->recomputeReason = lastFrame_.isValid() ? NavRecomputeReason::GoalChanged
															 : NavRecomputeReason::InitialGoal;
		}
		resetRoute();
	}
	roamGoalState().hasObjectiveTarget = observation.hasObjectiveTarget;
	roamGoalState().objectiveTarget = observation.objectiveTarget;
	lastFrame_ = observation.frame;
	if (decision != nullptr)
	{
		decision->observationPosition = observation.locomotion.position;
		decision->observationVelocity = observation.locomotion.velocity;
		decision->intentDirection = routeState().lastIntentDirection;
		populateRouteDecision(decision);
	}

	if (observation.movementSuppressed)
	{
		friendWaitState().waitingBehindFriend = false;
		friendWaitState().friendInTheWay = false;
		friendWaitState().friendCheckAt = -1.0f;
		traversalState().locomotion.resetProgressMonitor();
		corridorProgressAt_ = observedAt_;
		if (decision != nullptr)
		{
			decision->stage = NavRoamStage::MovementSuppressed;
			decision->locomotionResult = nav::LocomotionResult::Inactive;
			decision->failureReason = NavFailureReason::None;
		}
		return {true, NavRoamResult::MovementSuppressed};
	}
	return {false, NavRoamResult::IntentReady};
}

NavRoamController::NavigationPhaseResult
NavRoamController::updateFriendWaiting(const nav::NavSnapshot &snapshot,
									   const NavRoamObservation &observation,
									   NavRoamDecision *decision)
{
	// CSBot polite waiting precedes ground stuck/Wiggle processing. Only current
	// perceived friends are supplied by the adapter; no route reservations are used.
	const bool canWait =
		timedCompatibility_ && routeState().hasActiveRoute && observation.locomotion.grounded &&
		!observation.airborne && !observation.locomotion.onLadder &&
		!traversalState().jumpDrop.isActive() && !traversalState().specialTraversal.isActive() &&
		!routeState().pathCostContext.attacking;
	if (canWait)
	{
		if (friendWaitState().friendCheckAt < 0.0f ||
			observedAt_ < friendWaitState().friendCheckAt ||
			observedAt_ - friendWaitState().friendCheckAt >= 0.5f)
		{
			friendWaitState().friendCheckAt = observedAt_;
			friendWaitState().friendInTheWay = false;
			nav::NavVector currentTarget{};
			const bool previewReady = traversalState().locomotion.previewTarget(
				snapshot, observation.locomotion, &currentTarget);
			if (previewReady)
				friendWaitState().friendPathTarget = currentTarget;
			const auto &self = observation.friendRayOriginAvailable
								   ? observation.friendRayOrigin
								   : observation.locomotion.position;
			const float dx = friendWaitState().friendPathTarget.x - self.x;
			const float dy = friendWaitState().friendPathTarget.y - self.y;
			const float dz = friendWaitState().friendPathTarget.z - self.z;
			const float length = previewReady || recoveryState().stuckRecoveryActive
									 ? std::sqrt(dx * dx + dy * dy + dz * dz)
									 : 0.0f;
			if (std::isfinite(length) && length > 0.0f)
			{
				for (const auto &other : observation.nearbyFriends)
				{
					const float x = other.x - self.x, y = other.y - self.y, z = other.z - self.z;
					if (x * x + y * y + z * z > 10000.0f)
						continue;
					const float along = (x * dx + y * dy + z * dz) / length;
					if (along <= 0.0f)
						continue;
					const float t = (std::min)(along / length, 1.0f);
					const float ax = x - t * dx, ay = y - t * dy, az = z - t * dz;
					if (ax * ax + ay * ay + az * az < 900.0f)
					{
						friendWaitState().friendInTheWay = true;
						break;
					}
				}
			}
		}
		if (friendWaitState().friendInTheWay)
		{
			if (!friendWaitState().waitingBehindFriend)
			{
				friendWaitState().waitingBehindFriend = true;
				friendWaitState().friendWaitUntil =
					observedAt_ + 5.0f - 3.0f * routeState().pathCostContext.aggression;
			}
			traversalState().locomotion.resetProgressMonitor();
			if (observedAt_ >= friendWaitState().friendWaitUntil)
			{
				friendWaitState().friendRepathPending = true;
				friendWaitState().waitingBehindFriend = false;
				if (recoveryState().stuckRecoveryActive)
				{
					traversalState().locomotion.start(routeState().activeCorridor);
					recoveryState().stuckRecoveryActive = false;
				}
			}
			else
			{
				if (decision != nullptr)
					decision->stage = NavRoamStage::MovementSuppressed;
				return {true, NavRoamResult::MovementSuppressed};
			}
		}
	}
	if (friendWaitState().waitingBehindFriend)
	{
		traversalState().locomotion.resetProgressMonitor();
		if (recoveryState().stuckRecoveryActive)
		{
			// The follower had stopped before the friend arrived. Its old Wiggle
			// direction/jump deadline is no longer a valid movement decision.
			resetRoute(true);
			if (decision != nullptr)
				decision->recomputeReason = NavRecomputeReason::PathInvalidated;
			return {true, NavRoamResult::ReplanRequired};
		}
	}
	friendWaitState().waitingBehindFriend = false;
	if (!canWait)
	{
		friendWaitState().friendInTheWay = false;
		friendWaitState().friendCheckAt = -1.0f;
	}
	return {false, NavRoamResult::IntentReady};
}

NavRoamController::NavigationPhaseResult
NavRoamController::locateUpdateAreas(NavigationUpdateContext &ctx)
{
	const auto &observation = ctx.observation;
	auto *intent = ctx.intent;
	auto *decision = ctx.decision;
	auto &query = ctx.query;
	auto &currentArea = ctx.currentArea;
	auto &currentAreaResult = ctx.currentAreaResult;
	auto &objectiveArea = ctx.objectiveArea;
	if (routeState().hasPathCostContext && !modePolicy_.allowsAdaptiveRouteWeighting())
	{
		query.setPathCostContext(routeState().pathCostContext);
	}
	currentArea = {};
	currentAreaResult =
		query.findContaining({observation.locomotion.position.x, observation.locomotion.position.y,
							  observation.locomotion.position.z},
							 64.0f, &currentArea);
	if (decision != nullptr)
	{
		decision->currentAreaResult = currentAreaResult;
	}

	if (routeState().hasActiveRoute && timedCompatibility_)
	{
		if (observation.locomotion.onLadder || traversalState().jumpDrop.isActive() ||
			traversalState().specialTraversal.isActive() || observedAt_ < corridorProgressAt_)
		{
			// Ground-route recovery must not interrupt a jump, drop or ladder.
			corridorProgressAt_ = observedAt_;
		}
		else if (recoveryState().stuckRecoveryActive &&
				 currentArea.area != routeState().activeLink.toArea &&
				 expireGroundRoute(observation, intent, decision))
		{
			return {true, NavRoamResult::ReplanRequired};
		}
	}
	traversalState().safeDropHeightAvailable =
		observation.locomotion.safeDropHeightAvailable &&
		std::isfinite(observation.locomotion.maximumSafeDropHeight) &&
		observation.locomotion.maximumSafeDropHeight > 0.0f;
	traversalState().maximumSafeDropHeight = traversalState().safeDropHeightAvailable
												 ? observation.locomotion.maximumSafeDropHeight
												 : 0.0f;
	if (currentAreaResult == nav::NavQueryResult::Found && currentArea.area != 0U &&
		areaVisitHistory_ != nullptr &&
		(roamGoalState().lastVisitedArea != currentArea.area ||
		 roamGoalState().lastVisitedMapGeneration != observation.frame.mapGeneration ||
		 roamGoalState().lastVisitedTeam != observation.team))
	{
		areaVisitHistory_->record(observation.frame.mapGeneration, observation.team,
								  currentArea.area, observation.frame.tick);
		roamGoalState().lastVisitedArea = currentArea.area;
		roamGoalState().lastVisitedMapGeneration = observation.frame.mapGeneration;
		roamGoalState().lastVisitedTeam = observation.team;
	}
	objectiveArea = 0U;
	if (observation.hasObjectiveTarget)
	{
		nav::NavAreaMatch objectiveMatch = {};
		nav::NavQueryResult objectiveResult =
			query.findContaining(observation.objectiveTarget, 64.0f, &objectiveMatch);
		if (objectiveResult == nav::NavQueryResult::NoAreaContaining)
		{
			objectiveResult = query.findNearest(observation.objectiveTarget,
												kMaximumRecoveryDistance, &objectiveMatch);
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
	return {false, NavRoamResult::IntentReady};
}

NavRoamController::NavigationPhaseResult
NavRoamController::recoverOffMeshUpdate(NavigationUpdateContext &ctx)
{
	const auto &snapshot = ctx.snapshot;
	const auto &observation = ctx.observation;
	auto *intent = ctx.intent;
	auto *decision = ctx.decision;
	auto &query = ctx.query;
	auto &currentArea = ctx.currentArea;
	auto &currentAreaResult = ctx.currentAreaResult;
	auto &objectiveArea = ctx.objectiveArea;
	bool plannedLinkContinuation = false;
	if (currentAreaResult == nav::NavQueryResult::NoAreaContaining &&
		!traversalState().jumpDrop.isActive() && !traversalState().specialTraversal.isActive())
	{
		const nav::NavQueryResult nearestResult =
			query.findNearest({observation.locomotion.position.x, observation.locomotion.position.y,
							   observation.locomotion.position.z},
							  kMaximumRecoveryDistance, &currentArea);
		plannedLinkContinuation =
			routeState().hasActiveRoute && traversalState().locomotion.isActive() &&
			nearestResult == nav::NavQueryResult::Found &&
			routeState().activeLink.fromArea == currentArea.area &&
			routeState().activeLink.toArea != routeState().activeLink.fromArea;
		// CSBot retains its path/last known area across a ledge's NAV gap.
		// A neighboring polygon can become nearest while the same safe descent
		// is in progress. Returning to its inset would reverse the fall run-up.
		if (!plannedLinkContinuation && routeState().hasActiveRoute &&
			traversalState().locomotion.isActive() && nearestResult == nav::NavQueryResult::Found &&
			currentArea.distanceSquared <= 80.0f * 80.0f &&
			traversalState().safeDropHeightAvailable)
		{
			const auto *document = snapshot.document();
			const auto *nearby = document->findArea(currentArea.area);
			const auto *landing = document->findArea(routeState().activeLink.toArea);
			bool connected = currentArea.area == routeState().activeLink.toArea;
			if (nearby != nullptr)
				for (const auto &direction : nearby->connections)
					connected =
						connected || std::find(direction.begin(), direction.end(),
											   routeState().activeLink.toArea) != direction.end();
			if (connected && landing != nullptr)
			{
				const float drop = observation.locomotion.position.z -
								   nav::surfaceZAt(*landing, observation.locomotion.position.x,
												   observation.locomotion.position.y);
				plannedLinkContinuation = drop > nav::LocomotionConfig::kMaximumJumpHeight &&
										  drop <= traversalState().maximumSafeDropHeight;
			}
		}
		if (decision != nullptr)
		{
			decision->nearestAreaResult = nearestResult;
			decision->recoveryArea =
				nearestResult == nav::NavQueryResult::Found ? currentArea.area : 0U;
			decision->nearestDistanceSquared = currentArea.distanceSquared;
		}
		if (!plannedLinkContinuation)
		{
			if (timedCompatibility_ && recoveryState().offMeshRecovery.blocked)
			{
				// Candidate failures remain bounded, but Compatibility must keep
				// trying movement/Wiggle rather than latch neutral until death.
				recoveryState().offMeshRecovery = {};
			}
			const nav::NavVector &position = observation.locomotion.position;
			const nav::NavVector &blocked = recoveryState().offMeshRecovery.blockedPosition;
			const float externalDisplacement = std::hypot(
				std::hypot(position.x - blocked.x, position.y - blocked.y), position.z - blocked.z);
			if (recoveryState().offMeshRecovery.failedAreaCount != 0U &&
				externalDisplacement >= kExternalRecoveryResetDistance)
			{
				recoveryState().offMeshRecovery = {};
			}
			if (!recoveryState().offMeshRecovery.blocked &&
				selectRecoveryArea(snapshot, observation.locomotion,
								   recoveryState().offMeshRecovery.failedAreas.data(),
								   recoveryState().offMeshRecovery.failedAreaCount, &currentArea) &&
				buildRecoveryIntent(currentArea, observation.locomotion, intent))
			{
				const float distance = std::sqrt(currentArea.distanceSquared);
				if (!recoveryState().offMeshRecovery.active)
				{
					recoveryState().offMeshRecovery.active = true;
					recoveryState().offMeshRecovery.noProgressUpdates = 0U;
					recoveryState().offMeshRecovery.bestDistance = distance;
				}
				else if (currentArea.area == recoveryState().offMeshRecovery.area &&
						 distance + 1.0f < recoveryState().offMeshRecovery.bestDistance)
				{
					recoveryState().offMeshRecovery.bestDistance = distance;
					recoveryState().offMeshRecovery.noProgressUpdates = 0U;
				}
				else
				{
					++recoveryState().offMeshRecovery.noProgressUpdates;
					if (currentArea.area != recoveryState().offMeshRecovery.area)
					{
						recoveryState().offMeshRecovery.bestDistance = distance;
					}
				}
				recoveryState().offMeshRecovery.area = currentArea.area;
				recoveryState().offMeshRecovery.target = currentArea.closestPoint;
				if (timedCompatibility_ && observation.locomotion.grounded &&
					!observation.airborne && !observation.locomotion.onLadder &&
					recoveryState().offMeshRecovery.noProgressUpdates >= kOffMeshNoProgressLimit)
				{
					if (!recoveryState().stuckRecoveryActive ||
						observedAt_ < recoveryState().stuckRecoveryEndsAt)
					{
						routeState().lastIntentDirection = intent->direction;
						if (buildStuckRecoveryIntent(snapshot, currentArea, intent))
						{
							recoveryState().stuckRecoveryActive = true;
							intent->targetArea = currentArea.area;
							intent->targetPosition = currentArea.closestPoint;
							if (decision != nullptr)
							{
								decision->stage = NavRoamStage::OffMeshRecovery;
								decision->failureReason = NavFailureReason::RecoveryNoProgress;
								decision->recoveryArea = currentArea.area;
								decision->targetArea = currentArea.area;
								decision->targetPosition = currentArea.closestPoint;
								decision->recoveryNoProgressUpdates =
									recoveryState().offMeshRecovery.noProgressUpdates;
								decision->recoveryGeometryOnly = true;
							}
							return {true, NavRoamResult::IntentReady};
						}
					}
					recoveryState().stuckRecoveryActive = false;
				}
				if (recoveryState().offMeshRecovery.noProgressUpdates >= kOffMeshNoProgressLimit)
				{
					if (recoveryState().offMeshRecovery.failedAreaCount == 0U)
					{
						recoveryState().offMeshRecovery.blockedPosition = position;
					}
					recoveryState()
						.offMeshRecovery
						.failedAreas[recoveryState().offMeshRecovery.failedAreaCount++] =
						currentArea.area;
					recoveryState().offMeshRecovery.blocked =
						recoveryState().offMeshRecovery.failedAreaCount ==
						recoveryState().offMeshRecovery.failedAreas.size();
					recoveryState().offMeshRecovery.active = false;
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
						decision->recoveryNoProgressUpdates =
							recoveryState().offMeshRecovery.noProgressUpdates;
						decision->recoveryGeometryOnly = true;
					}
					return {true, NavRoamResult::NoRoute};
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
						decision->recoveryNoProgressUpdates =
							recoveryState().offMeshRecovery.noProgressUpdates;
						decision->recoveryGeometryOnly = true;
					}
					return {true, NavRoamResult::IntentReady};
				}
			}
			// No eligible geometry candidates remain, or the attempt cap was reached.
			if (recoveryState().offMeshRecovery.failedAreaCount != 0U)
			{
				recoveryState().offMeshRecovery.blocked = true;
			}
			*intent = {};
			if (decision != nullptr)
			{
				decision->failureReason = recoveryState().offMeshRecovery.blocked
											  ? NavFailureReason::RecoveryNoProgress
											  : NavFailureReason::CurrentAreaMissing;
				decision->stage = NavRoamStage::Failed;
				decision->recoveryArea = recoveryState().offMeshRecovery.blocked
											 ? recoveryState().offMeshRecovery.area
											 : 0U;
				decision->targetPosition = recoveryState().offMeshRecovery.blocked
											   ? recoveryState().offMeshRecovery.target
											   : nav::NavVector{};
				decision->recoveryNoProgressUpdates =
					recoveryState().offMeshRecovery.noProgressUpdates;
				decision->recoveryGeometryOnly = true;
			}
			return {true, NavRoamResult::NoRoute};
		}
	}
	if (currentAreaResult != nav::NavQueryResult::Found && !plannedLinkContinuation &&
		!traversalState().jumpDrop.isActive() && !traversalState().specialTraversal.isActive())
	{
		if (!routeState().hasActiveRoute)
		{
			resetRoute();
		}
		if (decision != nullptr)
		{
			decision->failureReason = NavFailureReason::CurrentAreaMissing;
			decision->stage = NavRoamStage::Failed;
		}
		return {true, NavRoamResult::NoRoute};
	}
	if (decision != nullptr)
	{
		decision->stage = NavRoamStage::ExactArea;
		decision->currentArea = currentArea.area;
	}
	if (currentAreaResult == nav::NavQueryResult::Found)
	{
		if (recoveryState().offMeshRecovery.active)
			recoveryState().stuckRecoveryActive = false;
		recoveryState().offMeshRecovery = {};
	}
	if (observation.hasObjectiveTarget && objectiveArea == 0U)
	{
		if (!routeState().hasActiveRoute)
		{
			resetRoute();
		}
		if (decision != nullptr)
		{
			decision->failureReason = NavFailureReason::GoalAreaMissing;
			decision->stage = NavRoamStage::Failed;
		}
		return {true, NavRoamResult::NoRoute};
	}
	return {false, NavRoamResult::IntentReady};
}

NavRoamController::NavigationPhaseResult
NavRoamController::recoverStuckUpdate(NavigationUpdateContext &ctx)
{
	const auto &snapshot = ctx.snapshot;
	const auto &observation = ctx.observation;
	auto *intent = ctx.intent;
	auto *decision = ctx.decision;
	auto &currentArea = ctx.currentArea;
	auto &currentAreaResult = ctx.currentAreaResult;
	if (recoveryState().stuckRecoveryActive)
	{
		const float stuckDx = observation.locomotion.position.x - recoveryState().stuckOrigin.x;
		const float stuckDy = observation.locomotion.position.y - recoveryState().stuckOrigin.y;
		const float stuckDz = observation.locomotion.position.z - recoveryState().stuckOrigin.z;
		const bool enteredNextArea = observation.locomotion.grounded && !observation.airborne &&
									 !observation.locomotion.onLadder &&
									 currentAreaResult == nav::NavQueryResult::Found &&
									 currentArea.area == routeState().activeLink.toArea;
		if (enteredNextArea)
			corridorProgressAt_ = observedAt_;
		const bool movedOutOfStuck =
			timedCompatibility_ &&
			(stuckDx * stuckDx + stuckDy * stuckDy + stuckDz * stuckDz > 75.0f * 75.0f ||
			 enteredNextArea);
		if (timedCompatibility_ && !movedOutOfStuck &&
			observedAt_ >= recoveryState().stuckRecoveryEndsAt)
		{
			recoveryState().stuckRecoveryActive = false;
			if (!buildStuckRecoveryIntent(snapshot, currentArea, intent))
			{
				resetRoute();
				return {true, NavRoamResult::ReplanRequired};
			}
			recoveryState().stuckRecoveryActive = true;
		}
		if (timedCompatibility_
				? !movedOutOfStuck && observedAt_ < recoveryState().stuckRecoveryEndsAt
				: recoveryState().stuckRecoveryFramesRemaining != 0U)
		{
			if (!buildStuckRecoveryIntent(snapshot, currentArea, intent))
			{
				resetRoute();
				roamGoalState().goalSelectionReason = GoalSelectionPathFailed;
				roamGoalState().goalSelectionStrategy = GoalSelectionStrategyNone;
				if (decision != nullptr)
				{
					decision->locomotionResult = nav::LocomotionResult::Stuck;
					decision->failureReason = NavFailureReason::MovementNotProduced;
					decision->goalSelectionReason = roamGoalState().goalSelectionReason;
					decision->goalSelectionStrategy = roamGoalState().goalSelectionStrategy;
					decision->stage = NavRoamStage::Failed;
				}
				return {true, NavRoamResult::ReplanRequired};
			}
			if (!timedCompatibility_)
				--recoveryState().stuckRecoveryFramesRemaining;
			if (decision != nullptr)
			{
				decision->locomotionResult = nav::LocomotionResult::Stuck;
				decision->stage = NavRoamStage::LocomotionReady;
				decision->intentDirection = intent->direction;
			}
			return {true, NavRoamResult::IntentReady};
		}
		// Preserve the established narrow-portal recovery exit when a grounded
		// actor actually enters the next area; ordinary repaths are not progress.
		if (movedOutOfStuck)
			traversalState().locomotion.resetProgressMonitor();
		const nav::LocomotionResult startResult =
			traversalState().locomotion.start(routeState().activeCorridor);
		if (startResult != nav::LocomotionResult::Started)
		{
			resetRoute();
			roamGoalState().goalSelectionReason = GoalSelectionPathFailed;
			roamGoalState().goalSelectionStrategy = GoalSelectionStrategyNone;
			if (decision != nullptr)
			{
				decision->locomotionResult = startResult;
				decision->failureReason = NavFailureReason::NavApplyRejected;
				decision->goalSelectionReason = roamGoalState().goalSelectionReason;
				decision->goalSelectionStrategy = roamGoalState().goalSelectionStrategy;
				decision->stage = NavRoamStage::Failed;
			}
			return {true, NavRoamResult::ReplanRequired};
		}
		recoveryState().stuckRecoveryActive = false;
	}
	return {false, NavRoamResult::IntentReady};
}

NavRoamController::NavigationPhaseResult
NavRoamController::selectUpdateRoute(NavigationUpdateContext &ctx)
{
	const auto &snapshot = ctx.snapshot;
	const auto &observation = ctx.observation;
	auto *intent = ctx.intent;
	auto *decision = ctx.decision;
	auto &currentArea = ctx.currentArea;
	auto &objectiveArea = ctx.objectiveArea;
	if (!traversalState().locomotion.isActive() && !traversalState().jumpDrop.isActive() &&
		!traversalState().specialTraversal.isActive())
	{
		if (decision != nullptr)
		{
			decision->stage = NavRoamStage::LinkSelection;
		}
		if (objectiveArea == currentArea.area && observation.hasObjectiveTarget)
		{
			const float deltaX = observation.objectiveTarget.x - observation.locomotion.position.x;
			const float deltaY = observation.objectiveTarget.y - observation.locomotion.position.y;
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
				routeState().lastIntentDirection = intent->direction;
				if (decision != nullptr)
				{
					decision->stage = NavRoamStage::LocomotionReady;
					decision->targetArea = currentArea.area;
					decision->targetPosition = observation.objectiveTarget;
					decision->intentDirection = intent->direction;
				}
				return {true, NavRoamResult::IntentReady};
			}
		}
		if (recoveryState().unsafeDropFailure.valid)
		{
			routeState().hasAvoidedLink =
				objectiveArea == 0U || recoveryState().unsafeDropFailure.bypassPending;
			routeState().avoidedLink = recoveryState().unsafeDropFailure.link;
			if (objectiveArea == 0U)
			{
				roamGoalState().failedRoamGoalArea = recoveryState().unsafeDropFailure.goalArea;
				roamGoalState().goalSelectionReason = GoalSelectionPathFailed;
			}
			else if (!timedCompatibility_ &&
					 recoveryState().unsafeDropFailure.retryFramesRemaining > 0U)
			{
				if (decision != nullptr)
				{
					decision->goalPresent = true;
					decision->goalKind = NavGoalKind::Objective;
					decision->goalArea = objectiveArea;
					decision->goalPosition = roamGoalState().objectiveTarget;
					decision->linkFromArea = recoveryState().unsafeDropFailure.link.fromArea;
					decision->linkToArea = recoveryState().unsafeDropFailure.link.toArea;
					decision->linkDirection = recoveryState().unsafeDropFailure.link.direction;
					decision->linkHow = recoveryState().unsafeDropFailure.link.how;
					decision->pathResult = nav::NavQueryResult::NoRoute;
					decision->failureReason = NavFailureReason::UnsafeDrop;
					decision->stage = NavRoamStage::Failed;
				}
				return {true, NavRoamResult::NoRoute};
			}
		}
		const nav::NavRouteType routeType = nav::NavRouteType::Fastest;
		if (objectiveArea != 0U &&
			isPathFailureBackedOff(observation.frame.mapGeneration, currentArea.area, objectiveArea,
								   routeType, observation.frame.tick))
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
			return {true, NavRoamResult::NoRoute};
		}
		if (!routeState().hasActiveRoute && decision != nullptr &&
			decision->recomputeReason == NavRecomputeReason::None)
		{
			decision->recomputeReason = NavRecomputeReason::InitialGoal;
		}
		if (timedCompatibility_)
		{
			if (observedAt_ < recoveryState().nextPathAttemptAt || randomSource_ == nullptr)
			{
				if (decision != nullptr)
				{
					decision->stage = NavRoamStage::Failed;
					decision->failureReason = NavFailureReason::PathSearchFailed;
				}
				return {true, NavRoamResult::NoRoute};
			}
			const auto request = compat::RandomRequest::floatRequest(
				"CSBOT-PATH-RETRY", {actor_.slot, actor_.actorGeneration},
				{0U, 0U, observation.frame.tick}, 0.4f, 0.6f);
			const auto interval = randomSource_->nextFloat(request);
			if (interval.status != compat::RandomStatus::Ok || !std::isfinite(interval.value) ||
				interval.value < 0.4f || interval.value > 0.6f)
				return {true, NavRoamResult::NoRoute};
			recoveryState().nextPathAttemptAt = observedAt_ + interval.value;
		}
		// Consume the bypass only when a route search is actually attempted.
		// Full updates during the retry cooldown must retain it.
		if (objectiveArea != 0U && recoveryState().unsafeDropFailure.valid)
			recoveryState().unsafeDropFailure.bypassPending = false;
		if (!selectRoute(snapshot, currentArea, objectiveArea, decision))
		{
			if (objectiveArea != 0U && recoveryState().unsafeDropFailure.valid)
			{
				delayUnsafeDropRetry();
				clearPathFailure();
			}
			if (objectiveArea != 0U && !recoveryState().unsafeDropFailure.valid &&
				decision != nullptr &&
				(decision->pathResult == nav::NavQueryResult::ResourceLimit ||
				 decision->pathResult == nav::NavQueryResult::NoRoute))
			{
				rememberPathFailure(observation.frame.mapGeneration, currentArea.area,
									objectiveArea, routeType, observation.frame.tick);
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
			return {true, NavRoamResult::NoRoute};
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
	return {false, NavRoamResult::IntentReady};
}

NavRoamController::NavigationPhaseResult
NavRoamController::startUpdateTraversal(NavigationUpdateContext &ctx)
{
	const auto &snapshot = ctx.snapshot;
	const auto &observation = ctx.observation;
	auto *decision = ctx.decision;
	auto &currentArea = ctx.currentArea;
	auto &objectiveArea = ctx.objectiveArea;
	if (routeState().hasActiveRoute && !routeState().activeCorridor.links.empty() &&
		!traversalState().jumpDrop.isActive() && !traversalState().specialTraversal.isActive())
	{
		const std::size_t firstLinkIndex =
			routeState().activeCorridorIndex > 0U ? routeState().activeCorridorIndex - 1U : 0U;
		for (std::size_t index = firstLinkIndex; index < routeState().activeCorridor.links.size();
			 ++index)
		{
			const nav::NavDirectedLink candidate = routeState().activeCorridor.links[index];
			if (candidate.fromArea != currentArea.area ||
				traversalActionFor(snapshot, candidate) == nav::TraversalAction::Walk)
			{
				continue;
			}
			routeState().activeLink = candidate;
			traversalState().activeTraversal = nav::TraversalAction::Walk;
			const nav::NavDocument *document = snapshot.document();
			const nav::NavArea *fromArea =
				document != nullptr ? document->findArea(candidate.fromArea) : nullptr;
			const nav::NavArea *toArea =
				document != nullptr ? document->findArea(candidate.toArea) : nullptr;
			if (fromArea == nullptr || toArea == nullptr)
			{
				resetRoute();
				if (decision != nullptr)
				{
					decision->locomotionResult = nav::LocomotionResult::InvalidCorridor;
					decision->failureReason = NavFailureReason::NavApplyRejected;
					decision->stage = NavRoamStage::Failed;
				}
				return {true, NavRoamResult::ReplanRequired};
			}
			const std::uint8_t reverseDirection =
				candidate.direction < nav::NavArea::kDirectionCount
					? static_cast<std::uint8_t>((candidate.direction + 2U) %
												nav::NavArea::kDirectionCount)
					: nav::NavArea::kDirectionCount;
			const nav::NavVector landingPortal = nav::portalSteeringPointForLink(
				*fromArea, *toArea, observation.locomotion.position, candidate.direction);
			const nav::NavVector launchPortal = nav::portalSteeringPointForLink(
				*toArea, *fromArea, landingPortal, reverseDirection);
			if (!isWithinTraversalLaunchTolerance(observation.locomotion.position, launchPortal))
			{
				break;
			}
			nav::LocomotionResult traversalStartResult = nav::LocomotionResult::InvalidArgument;
			if (!startTraversal(snapshot, routeState().activeCorridor, candidate,
								&traversalStartResult, &observation.locomotion.position,
								&landingPortal))
			{
				if (traversalStartResult == nav::LocomotionResult::UnsafeDrop)
				{
					rememberUnsafeDrop(observation, objectiveArea, candidate);
				}
				routeState().hasAvoidedLink = true;
				routeState().avoidedLink = candidate;
				++routeState().nextLinkIndex;
				if (objectiveArea == 0U && roamGoalState().hasRoamGoal)
				{
					roamGoalState().failedRoamGoalArea = roamGoalState().roamGoalArea;
				}
				resetRoute();
				roamGoalState().goalSelectionReason = GoalSelectionPathFailed;
				roamGoalState().goalSelectionStrategy = GoalSelectionStrategyNone;
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
					decision->goalSelectionReason = roamGoalState().goalSelectionReason;
					decision->goalSelectionStrategy = roamGoalState().goalSelectionStrategy;
					decision->stage = NavRoamStage::Failed;
				}
				return {true, NavRoamResult::ReplanRequired};
			}
			break;
		}
	}
	return {false, NavRoamResult::IntentReady};
}

NavRoamController::NavigationPhaseResult
NavRoamController::updateJumpDropTraversal(NavigationUpdateContext &ctx)
{
	const auto &snapshot = ctx.snapshot;
	const auto &observation = ctx.observation;
	auto *intent = ctx.intent;
	auto *decision = ctx.decision;
	auto &currentArea = ctx.currentArea;
	auto &objectiveArea = ctx.objectiveArea;
	if (traversalState().jumpDrop.isActive())
	{
		nav::JumpDropObservation traversalObservation = {};
		traversalObservation.position = observation.locomotion.position;
		traversalObservation.frame = observation.frame.tick;
		if (timedCompatibility_ && traversalState().activeTraversal == nav::TraversalAction::Drop)
			traversalObservation.frame = static_cast<std::uint32_t>(std::fmod(
				static_cast<double>(observedAt_) * 30.0,
				static_cast<double>((std::numeric_limits<std::uint32_t>::max)()) + 1.0));
		traversalObservation.actorGeneration = observation.actor.actorGeneration;
		traversalObservation.airborne = observation.airborne;
		traversalObservation.landingConfirmed = observation.landingConfirmed;
		traversalObservation.hasLandingDamage = observation.hasLandingDamage;
		traversalObservation.landingDamage = observation.landingDamage;
		nav::JumpDropIntent traversalIntent = {};
		nav::JumpDropResult traversalResult =
			traversalState().jumpDrop.update(snapshot, traversalObservation, &traversalIntent);
		if (timedCompatibility_ && traversalState().activeTraversal == nav::TraversalAction::Drop &&
			traversalResult == nav::JumpDropResult::InvalidObservation &&
			observation.landingConfirmed && !observation.airborne &&
			observation.hasLandingDamage && observation.landingDamage == 0.0f)
		{
			// Engine momentum can carry a safe fall past its nominal landing point.
			// Accept confirmed, damage-free ground contact only on the landing
			// area or its forward corridor suffix; never infer a landing receipt.
			nav::NavAreaMatch landing{};
			const auto &areas = routeState().activeCorridor.areas;
			const auto destination = std::find(areas.begin(), areas.end(), routeState().activeLink.toArea);
			if (ctx.query.findContaining(observation.locomotion.position,
					nav::JumpDropConfig::kDefaultLandingTolerance, &landing) == nav::NavQueryResult::Found &&
				destination != areas.end() && std::find(destination, areas.end(), landing.area) != areas.end())
			{
				currentArea = landing;
				if (decision != nullptr) decision->currentArea = landing.area;
				traversalResult = nav::JumpDropResult::Landed;
			}
		}
		if (traversalResult == nav::JumpDropResult::Emitted)
		{
			intent->direction = traversalIntent.direction;
			intent->speed = traversalIntent.speed;
			intent->posture = nav::LocomotionPosture::Standing;
			intent->traversal = traversalState().activeTraversal;
			intent->stepUp = false;
			intent->currentArea = traversalIntent.launchArea;
			intent->targetArea = traversalIntent.landingArea;
			routeState().lastIntentDirection = intent->direction;
			if (decision != nullptr)
			{
				decision->locomotionResult = nav::LocomotionResult::IntentReady;
				decision->stage = NavRoamStage::LocomotionReady;
				decision->targetArea = traversalIntent.landingArea;
			}
			return {true, NavRoamResult::IntentReady};
		}
		if (traversalResult == nav::JumpDropResult::Ready)
		{
			intent->direction = routeState().lastIntentDirection;
			intent->speed = kRoamSpeed;
			intent->posture = nav::LocomotionPosture::Standing;
			intent->traversal = nav::TraversalAction::Walk;
			intent->stepUp = false;
			intent->currentArea = routeState().activeLink.fromArea;
			intent->targetArea = routeState().activeLink.toArea;
			if (decision != nullptr)
			{
				decision->locomotionResult = nav::LocomotionResult::IntentReady;
				decision->stage = NavRoamStage::LocomotionReady;
				decision->targetArea = routeState().activeLink.toArea;
				decision->intentDirection = intent->direction;
			}
			return {true, NavRoamResult::IntentReady};
		}
		if (traversalResult == nav::JumpDropResult::Landed)
		{
			const bool goalReached =
				(decision != nullptr && decision->goalPresent &&
				 decision->goalArea == currentArea.area) ||
				(roamGoalState().hasRoamGoal && roamGoalState().roamGoalArea == currentArea.area);
			if (goalReached)
			{
				roamGoalState().goalSelectionReason = GoalSelectionReached;
			}
			resetRoute(!goalReached);
			if (decision != nullptr)
			{
				decision->locomotionResult = nav::LocomotionResult::TargetReached;
				decision->stage =
					goalReached ? NavRoamStage::TargetReached : NavRoamStage::LinkSelection;
				if (goalReached)
				{
					decision->goalSelectionReason = GoalSelectionReached;
				}
			}
			return {true,
					goalReached ? NavRoamResult::TargetReached : NavRoamResult::ReplanRequired};
		}
		nav::LocomotionResult traversalFailure = nav::LocomotionResult::InvalidObservation;
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
		routeState().hasAvoidedLink = true;
		routeState().avoidedLink = routeState().activeLink;
		if (traversalFailure == nav::LocomotionResult::UnsafeDrop)
		{
			rememberUnsafeDrop(observation, objectiveArea, routeState().activeLink);
		}
		++routeState().nextLinkIndex;
		if (objectiveArea == 0U && roamGoalState().hasRoamGoal)
		{
			roamGoalState().failedRoamGoalArea = roamGoalState().roamGoalArea;
		}
		roamGoalState().goalSelectionReason = GoalSelectionPathFailed;
		roamGoalState().goalSelectionStrategy = GoalSelectionStrategyNone;
		resetRoute();
		if (decision != nullptr)
		{
			decision->locomotionResult = traversalFailure;
			decision->failureReason = traversalFailure == nav::LocomotionResult::UnsafeDrop
										  ? NavFailureReason::UnsafeDrop
										  : NavFailureReason::NavApplyRejected;
			decision->goalSelectionReason = roamGoalState().goalSelectionReason;
			decision->goalSelectionStrategy = roamGoalState().goalSelectionStrategy;
			decision->stage = NavRoamStage::Failed;
		}
		return {true, NavRoamResult::ReplanRequired};
	}
	return {false, NavRoamResult::IntentReady};
}

NavRoamController::NavigationPhaseResult
NavRoamController::updateSpecialTraversal(NavigationUpdateContext &ctx)
{
	const auto &snapshot = ctx.snapshot;
	const auto &observation = ctx.observation;
	auto *intent = ctx.intent;
	auto *decision = ctx.decision;
	auto &currentArea = ctx.currentArea;
	if (traversalState().specialTraversal.isActive())
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
			traversalState().specialTraversal.update(snapshot, traversalObservation,
													 &traversalIntent);
		if (traversalResult == nav::SpecialTraversalResult::EnterIntent ||
			traversalResult == nav::SpecialTraversalResult::MaintainIntent ||
			traversalResult == nav::SpecialTraversalResult::ExitIntent)
		{
			intent->direction = traversalIntent.direction;
			intent->speed = traversalIntent.speed;
			intent->posture = traversalIntent.posture == nav::SpecialTraversalPosture::Crouching
								  ? nav::LocomotionPosture::Crouching
								  : nav::LocomotionPosture::Standing;
			intent->traversal = traversalState().activeTraversal;
			intent->stepUp = false;
			intent->currentArea = traversalIntent.entryArea;
			intent->targetArea = traversalIntent.exitArea;
			if (decision != nullptr)
			{
				decision->locomotionResult = nav::LocomotionResult::IntentReady;
				decision->stage = NavRoamStage::LocomotionReady;
				decision->targetArea = traversalIntent.exitArea;
			}
			return {true, NavRoamResult::IntentReady};
		}
		if (traversalResult == nav::SpecialTraversalResult::Completed)
		{
			const bool goalReached =
				(decision != nullptr && decision->goalPresent &&
				 decision->goalArea == currentArea.area) ||
				(roamGoalState().hasRoamGoal && roamGoalState().roamGoalArea == currentArea.area);
			if (goalReached)
			{
				roamGoalState().goalSelectionReason = GoalSelectionReached;
			}
			resetRoute(!goalReached);
			if (decision != nullptr)
			{
				decision->locomotionResult = nav::LocomotionResult::TargetReached;
				decision->stage =
					goalReached ? NavRoamStage::TargetReached : NavRoamStage::LinkSelection;
				if (goalReached)
				{
					decision->goalSelectionReason = GoalSelectionReached;
				}
			}
			return {true,
					goalReached ? NavRoamResult::TargetReached : NavRoamResult::ReplanRequired};
		}
		resetRoute();
		if (decision != nullptr)
		{
			decision->stage = NavRoamStage::Failed;
		}
		return {true, NavRoamResult::ReplanRequired};
	}
	return {false, NavRoamResult::IntentReady};
}

NavRoamController::NavigationPhaseResult
NavRoamController::updateGroundLocomotion(NavigationUpdateContext &ctx)
{
	const auto &snapshot = ctx.snapshot;
	const auto &observation = ctx.observation;
	auto *intent = ctx.intent;
	auto *decision = ctx.decision;
	auto &currentArea = ctx.currentArea;
	auto &objectiveArea = ctx.objectiveArea;
	nav::LocomotionResult locomotionResult =
		traversalState().locomotion.update(snapshot, observation.locomotion, intent);
	if (routeState().hasActiveRoute)
	{
		if (traversalState().locomotion.currentCorridorIndex() > routeState().activeCorridorIndex)
			corridorProgressAt_ = observedAt_;
		routeState().activeCorridorIndex = traversalState().locomotion.currentCorridorIndex();
		if (!routeState().activeCorridor.areas.empty() &&
			routeState().activeCorridorIndex >= routeState().activeCorridor.areas.size())
		{
			routeState().activeCorridorIndex = routeState().activeCorridor.areas.size() - 1U;
		}
		if (routeState().activeCorridorIndex > 0U &&
			routeState().activeCorridorIndex - 1U < routeState().activeCorridor.links.size())
		{
			const nav::NavDirectedLink currentLink =
				routeState().activeCorridor.links[routeState().activeCorridorIndex - 1U];
			if (traversalActionFor(snapshot, currentLink) == nav::TraversalAction::Walk)
			{
				routeState().activeLink = currentLink;
			}
		}
	}
	if (locomotionResult == nav::LocomotionResult::IntentReady &&
		expireGroundRoute(observation, intent, decision))
		return {true, NavRoamResult::ReplanRequired};
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
		routeState().lastIntentDirection = intent->direction;
		friendWaitState().friendPathTarget = intent->targetPosition;
	}
	if (locomotionResult == nav::LocomotionResult::IntentReady &&
		traversalState().activeTraversal != nav::TraversalAction::Walk)
	{
		intent->traversal = traversalState().activeTraversal;
	}
	switch (locomotionResult)
	{
	case nav::LocomotionResult::Started:
		resetRoute();
		if (decision != nullptr)
		{
			decision->stage = NavRoamStage::Failed;
		}
		return {true, NavRoamResult::ReplanRequired};
	case nav::LocomotionResult::IntentReady:
		if (decision != nullptr)
		{
			decision->stage = NavRoamStage::LocomotionReady;
		}
		return {true, NavRoamResult::IntentReady};
	case nav::LocomotionResult::TargetReached:
	{
		const bool goalReached =
			routeState().activeCorridor.hasEndpoint ||
			(decision != nullptr && decision->goalPresent &&
			 decision->goalArea == currentArea.area) ||
			(roamGoalState().hasRoamGoal && roamGoalState().roamGoalArea == currentArea.area);
		if (goalReached)
		{
			roamGoalState().goalSelectionReason = GoalSelectionReached;
		}
		resetRoute(!goalReached);
		if (decision != nullptr)
		{
			decision->stage =
				goalReached ? NavRoamStage::TargetReached : NavRoamStage::LinkSelection;
			if (goalReached)
			{
				decision->goalSelectionReason = GoalSelectionReached;
			}
		}
		return {true, goalReached ? NavRoamResult::TargetReached : NavRoamResult::ReplanRequired};
	}
	case nav::LocomotionResult::UnsafeDrop:
		// The follower may have advanced past a safe prefix before the veto.
		// Capture its rejected transition even when it is a Drop, not a Walk.
		if (routeState().activeCorridorIndex > 0U &&
			routeState().activeCorridorIndex - 1U < routeState().activeCorridor.links.size())
		{
			routeState().activeLink =
				routeState().activeCorridor.links[routeState().activeCorridorIndex - 1U];
		}
		if (decision != nullptr)
		{
			decision->linkFromArea = routeState().activeLink.fromArea;
			decision->linkToArea = routeState().activeLink.toArea;
			decision->linkDirection = routeState().activeLink.direction;
			decision->linkHow = routeState().activeLink.how;
		}
		rememberUnsafeDrop(observation, objectiveArea, routeState().activeLink);
		resetRoute();
		if (decision != nullptr)
		{
			decision->locomotionResult = nav::LocomotionResult::UnsafeDrop;
			decision->failureReason = NavFailureReason::UnsafeDrop;
			decision->stage = NavRoamStage::Failed;
		}
		return {true, NavRoamResult::ReplanRequired};
	case nav::LocomotionResult::NeedsRecovery:
	case nav::LocomotionResult::StepTooHigh:
	case nav::LocomotionResult::Stuck:
	case nav::LocomotionResult::Inactive:
		if (locomotionResult == nav::LocomotionResult::Stuck && routeState().hasActiveRoute)
		{
			if (!recoveryState().stuckRecoveryActive &&
				(timedCompatibility_ ||
				 recoveryState().stuckRecoveryCount < kMaximumStuckRecoveryAttempts))
			{
				recoveryState().stuckRecoveryActive = false;
				recoveryState().stuckRecoveryFramesRemaining = kStuckRecoveryFrameLimit;
				recoveryState().stuckOrigin = observation.locomotion.position;
				recoveryState().stuckJumpScheduled = false;
				++recoveryState().stuckRecoveryCount;
				if (buildStuckRecoveryIntent(snapshot, currentArea, intent))
				{
					recoveryState().stuckRecoveryActive = true;
					--recoveryState().stuckRecoveryFramesRemaining;
					if (decision != nullptr)
					{
						decision->locomotionResult = nav::LocomotionResult::Stuck;
						decision->stage = NavRoamStage::LocomotionReady;
						decision->intentDirection = intent->direction;
					}
					return {true, NavRoamResult::IntentReady};
				}
				recoveryState().stuckRecoveryActive = false;
				recoveryState().stuckRecoveryFramesRemaining = 0U;
			}
			routeState().hasAvoidedLink = true;
			routeState().avoidedLink = routeState().activeLink;
			++routeState().nextLinkIndex;
			if (objectiveArea == 0U && roamGoalState().hasRoamGoal)
			{
				roamGoalState().failedRoamGoalArea = roamGoalState().roamGoalArea;
			}
			resetRoute();
			if (decision != nullptr)
			{
				decision->failureReason = NavFailureReason::MovementNotProduced;
				decision->stage = NavRoamStage::Failed;
			}
			return {true, NavRoamResult::ReplanRequired};
		}
		resetRoute();
		if (decision != nullptr)
		{
			decision->stage = NavRoamStage::Failed;
		}
		return {true, NavRoamResult::ReplanRequired};
	case nav::LocomotionResult::InvalidSnapshot:
	case nav::LocomotionResult::StaleSnapshot:
		resetRoute();
		if (decision != nullptr)
		{
			decision->stage = NavRoamStage::Failed;
		}
		return {true, NavRoamResult::InvalidSnapshot};
	case nav::LocomotionResult::InvalidCorridor:
		if (routeState().activeLink.fromArea == currentArea.area &&
			routeState().activeLink.toArea != currentArea.area)
		{
			routeState().hasAvoidedLink = true;
			routeState().avoidedLink = routeState().activeLink;
			++routeState().nextLinkIndex;
		}
		if (objectiveArea == 0U && roamGoalState().hasRoamGoal)
		{
			roamGoalState().failedRoamGoalArea = roamGoalState().roamGoalArea;
		}
		resetRoute();
		if (decision != nullptr)
		{
			decision->recomputeReason = NavRecomputeReason::PathInvalidated;
			decision->failureReason = NavFailureReason::NavApplyRejected;
			decision->stage = NavRoamStage::Failed;
		}
		return {true, NavRoamResult::ReplanRequired};
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
		return {true, NavRoamResult::InvalidObservation};
	}

	resetRoute();
	if (decision != nullptr)
	{
		decision->stage = NavRoamStage::Failed;
	}
	return {true, NavRoamResult::InvalidObservation};
}

void NavRoamController::reset()
{
	motionActor_={};motionFrame_={};
	friendWaitState().friendRepathPending = false;
	if (reservationBoard_ != nullptr)
	{
		reservationBoard_->release(actor_);
	}
	recoveryState().offMeshRecovery = {};
	recoveryState().unsafeDropFailure = {};
	previousMovementSuppressed_ = false;
	recoveryState().nextPathAttemptAt = 0.0f;
	timedCompatibility_ = false;
	observedAt_ = 0.0f;
	recoveryState().stuckRecoveryEndsAt = 0.0f;
	recoveryState().stuckJumpScheduled = false;
	recoveryState().lastWiggleJumpAt = -10000.0f;
	resetRoute();
	actor_ = {0U, LifecycleSession::kInvalidGeneration};
	lastFrame_ = {};
	routeState().nextLinkIndex = 0U;
	routeState().pathSequence = 0U;
	routeState().hasAvoidedLink = false;
	routeState().avoidedLink = {0U, 0U, 0U, 0U};
	roamGoalState().hasObjectiveTarget = false;
	roamGoalState().objectiveTarget = {0.0f, 0.0f, 0.0f};
	clearPathFailure();
	roamGoalState().lastVisitedArea = 0U;
	roamGoalState().lastVisitedMapGeneration = 0U;
	roamGoalState().lastVisitedTeam = 0U;
	roamGoalState().roamGoalGeneration = 0U;
	roamGoalState().goalSelectionReason = GoalSelectionNone;
	roamGoalState().goalSelectionStrategy = GoalSelectionStrategyNone;
	traversalState().maximumSafeDropHeight = 0.0f;
	traversalState().safeDropHeightAvailable = false;
	traversalState().unsafeDropRejected = false;
	roamGoalState().hasRoamGoal = false;
	roamGoalState().roamGoalArea = 0U;
	roamGoalState().failedRoamGoalArea = 0U;
	roamGoalState().roamGoalPosition = {0.0f, 0.0f, 0.0f};
	initialized_ = false;
}

bool NavRoamController::isActive() const
{
	return traversalState().locomotion.isActive() || traversalState().jumpDrop.isActive() ||
		traversalState().specialTraversal.isActive() || recoveryState().stuckRecoveryActive;
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
		(!observation.friendRayOriginAvailable ||
			(std::isfinite(observation.friendRayOrigin.x) &&
			 std::isfinite(observation.friendRayOrigin.y) &&
			 std::isfinite(observation.friendRayOrigin.z))) &&
		(!observation.locomotion.viewYawAvailable ||
			std::isfinite(observation.locomotion.viewYaw)) &&
		(!observation.locomotion.timingAvailable ||
			(std::isfinite(observation.locomotion.observedAt) &&
			 observation.locomotion.observedAt >= 0.0f)) &&
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
	traversalState().unsafeDropRejected = false;
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
	traversalState().activeTraversal = traversalActionFor(snapshot, link);
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

	if (traversalState().activeTraversal == nav::TraversalAction::Jump ||
		traversalState().activeTraversal == nav::TraversalAction::Drop)
	{
		nav::NavCorridor traversalCorridor = corridor;
		traversalCorridor.areas = {link.fromArea, link.toArea};
		traversalCorridor.links = {link};
		nav::JumpDropEnvelope envelope = {};
		envelope.kind = traversalState().activeTraversal == nav::TraversalAction::Jump
				? nav::JumpDropKind::Jump
				: nav::JumpDropKind::Drop;
		envelope.launch = {launchPosition, link.fromArea};
		envelope.landing = {landingPosition, link.toArea};
		envelope.maximumRise = (std::max)(16.0f, verticalDelta + 16.0f);
		envelope.maximumDrop = (std::max)(16.0f, -verticalDelta + 16.0f);
		envelope.horizontalReach = (std::max)(16.0f, horizontalReach + 16.0f);
		envelope.damageRisk = traversalState().activeTraversal == nav::TraversalAction::Drop
			? nav::JumpDropDamageRisk{traversalState().maximumSafeDropHeight, 0.0f}
			: nav::JumpDropDamageRisk{4096.0f, 100.0f};
		const nav::JumpDropResult result =
			traversalState().jumpDrop.start(traversalCorridor, envelope, actor_.actorGeneration);
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
		if (traversalState().activeTraversal == nav::TraversalAction::Drop &&
			result != nav::JumpDropResult::Ready)
		{
			traversalState().unsafeDropRejected = true;
		}
		return result == nav::JumpDropResult::Ready;
	}

	if (traversalState().activeTraversal == nav::TraversalAction::Ladder)
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
		const nav::SpecialTraversalResult result = traversalState().specialTraversal.start(
			traversalCorridor, capability, actor_.actorGeneration);
		if (startResult != nullptr)
		{
			*startResult = result == nav::SpecialTraversalResult::Ready
				? nav::LocomotionResult::Started
				: nav::LocomotionResult::InvalidCorridor;
		}
		return result == nav::SpecialTraversalResult::Ready;
	}

	const nav::LocomotionResult result = traversalState().locomotion.start(corridor);
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
	traversalState().activeTraversal = nav::TraversalAction::Walk;
	const nav::LocomotionResult result = traversalState().locomotion.start(corridor);
	if (startResult != nullptr)
	{
		*startResult = result;
	}
	return result == nav::LocomotionResult::Started;
}

bool NavRoamController::buildStuckRecoveryIntent(
	const nav::NavSnapshot &snapshot,
	const nav::NavAreaMatch &currentArea,
	nav::LocomotionIntent *intent)
{
	if (intent == nullptr || (!routeState().hasActiveRoute && !recoveryState().offMeshRecovery.active))
	{
		return false;
	}

	float baseX = routeState().lastIntentDirection.x;
	float baseY = routeState().lastIntentDirection.y;
	const float baseLength = std::hypot(baseX, baseY);
	if (!std::isfinite(baseLength) || baseLength <= 0.0f)
	{
		baseX = routeState().activeTargetPosition.x - currentArea.closestPoint.x;
		baseY = routeState().activeTargetPosition.y - currentArea.closestPoint.y;
	}
	const float length = std::hypot(baseX, baseY);
	if (!std::isfinite(length) || length <= 0.0f)
	{
		return false;
	}

	if (!recoveryState().stuckRecoveryActive)
	{
		if (timedCompatibility_)
		{
			if (randomSource_ == nullptr) return false;
			const compat::RandomActor actor = {actor_.slot, actor_.actorGeneration};
			const compat::RandomTimingContext timing = {0U, 0U, lastFrame_.tick};
			if (!recoveryState().stuckJumpScheduled)
			{
				const auto delay = randomSource_->nextFloat(
					compat::RandomRequest::floatRequest("CSBOT-STUCK-JUMP-DELAY",
						actor, timing, 0.0f, 0.5f));
				if (delay.status != compat::RandomStatus::Ok || !std::isfinite(delay.value) ||
					delay.value < 0.0f || delay.value > 0.5f) return false;
				recoveryState().nextStuckJumpAt = observedAt_ + delay.value;
				recoveryState().stuckJumpScheduled = true;
			}
			const auto direction = randomSource_->nextLong(
				compat::RandomRequest::longRequest("CSBOT-WIGGLE-DIRECTION",
					actor, timing, 0, 3));
			if (direction.status != compat::RandomStatus::Ok ||
				direction.value < 0 || direction.value > 3) return false;
			const auto duration = randomSource_->nextFloat(
				compat::RandomRequest::floatRequest("CSBOT-WIGGLE-DURATION",
					actor, timing, 0.5f, 1.5f));
			if (duration.status != compat::RandomStatus::Ok ||
				!std::isfinite(duration.value) || duration.value < 0.5f || duration.value > 1.5f)
				return false;
			const float yaw = viewYaw_ * 0.017453292519943295f;
			const float x = viewYawAvailable_ ? std::cos(yaw) : baseX / length;
			const float y = viewYawAvailable_ ? std::sin(yaw) : baseY / length;
			switch (direction.value)
			{
			case 0: recoveryState().stuckRecoveryDirection = {x, y, 0.0f}; break;
			case 1: recoveryState().stuckRecoveryDirection = {y, -x, 0.0f}; break;
			case 2: recoveryState().stuckRecoveryDirection = {-x, -y, 0.0f}; break;
			default: recoveryState().stuckRecoveryDirection = {-y, x, 0.0f}; break;
			}
			recoveryState().stuckRecoveryEndsAt = observedAt_ + duration.value;
		}
		else
		{
			const float sign = (recoveryState().stuckRecoveryCount % 2U) == 0U ? 1.0f : -1.0f;
			recoveryState().stuckRecoveryDirection = {
				-baseY / length * sign, baseX / length * sign, 0.0f};
		}
	}
	intent->direction = recoveryState().stuckRecoveryDirection;
	intent->speed = kRoamSpeed;
	intent->posture = nav::LocomotionPosture::Standing;
	intent->traversal = nav::TraversalAction::Walk;
	intent->stepUp = false;
	intent->currentArea = currentArea.area;
	intent->targetArea = routeState().activeLink.toArea;
	if (timedCompatibility_ && observedAt_ >= recoveryState().nextStuckJumpAt &&
		observedAt_ - recoveryState().lastWiggleJumpAt >= 1.0f && latestLocomotion_.grounded &&
		!latestLocomotion_.ducked && !latestLocomotion_.onLadder)
	{
		const nav::NavArea *area = snapshot.document()->findArea(currentArea.area);
		const bool crouchOnly = area != nullptr &&
			(area->attributes & nav::NavArea::kCrouch) != 0U &&
			(area->attributes & nav::NavArea::kJump) == 0U;
		if (!crouchOnly)
		{
			const auto interval = randomSource_->nextFloat(
				compat::RandomRequest::floatRequest("CSBOT-WIGGLE-JUMP-INTERVAL",
					{actor_.slot, actor_.actorGeneration}, {0U, 0U, lastFrame_.tick},
					1.0f, 2.0f));
			if (interval.status != compat::RandomStatus::Ok ||
				!std::isfinite(interval.value) || interval.value < 1.0f || interval.value > 2.0f)
				return false;
			intent->traversal = nav::TraversalAction::Jump;
			recoveryState().lastWiggleJumpAt = observedAt_;
			recoveryState().nextStuckJumpAt = observedAt_ + interval.value;
		}
	}
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
	if (routeState().hasPathCostContext && !modePolicy_.allowsAdaptiveRouteWeighting())
	{
		query.setPathCostContext(routeState().pathCostContext);
	}
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
			decision->goalPosition = roamGoalState().objectiveTarget;
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
		if (routeState().hasAvoidedLink && link.fromArea == routeState().avoidedLink.fromArea &&
				link.toArea == routeState().avoidedLink.toArea)
		{
			nav::NavCorridor alternativeCorridor = {};
			const nav::NavQueryResult alternativeResult =
				query.buildAlternativeCorridor(
					corridor, routeState().avoidedLink, &alternativeCorridor,
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
					decision->linkFromArea = routeState().avoidedLink.fromArea;
					decision->linkToArea = routeState().avoidedLink.toArea;
					decision->linkDirection = routeState().avoidedLink.direction;
					decision->linkHow = routeState().avoidedLink.how;
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
		if (modePolicy_.mode() == compat::RuntimeMode::Compatibility)
		{
			corridor.hasEndpoint = true;
			corridor.endpoint = roamGoalState().objectiveTarget;
			const auto *goal = snapshot.document()->findArea(objectiveArea);
			corridor.endpoint.z = nav::surfaceZAt(*goal, roamGoalState().objectiveTarget.x, roamGoalState().objectiveTarget.y);
		}
		if (startRoute(snapshot, corridor, link, &startResult))
		{
			routeState().hasAvoidedLink = false;
			routeState().nextLinkIndex = 0U;
			rememberRoute(snapshot, corridor, link);
				if (decision != nullptr)
				{
					populateRouteDecision(decision);
					decision->targetArea = objectiveArea;
					decision->targetPosition = roamGoalState().objectiveTarget;
			}
			return true;
		}
		routeState().hasAvoidedLink = true;
		routeState().avoidedLink = link;
		++routeState().nextLinkIndex;
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
		const std::size_t firstIndex = routeState().nextLinkIndex % links.size();
		for (std::size_t offset = 0U; offset < links.size(); ++offset)
		{
			const std::size_t linkIndex = (firstIndex + offset) % links.size();
			if (routeState().hasAvoidedLink &&
					links[linkIndex].fromArea == routeState().avoidedLink.fromArea &&
					links[linkIndex].toArea == routeState().avoidedLink.toArea)
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
		traversalState().activeTraversal = traversalActionFor(snapshot, links[linkIndex]);
		routeState().nextLinkIndex = linkIndex + 1U;
		rememberRoute(snapshot, corridor, links[linkIndex]);
			if (decision != nullptr)
			{
				decision->targetArea = links[linkIndex].toArea;
				populateRouteDecision(decision);
			}
			routeState().hasAvoidedLink = false;
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
		if (routeState().hasAvoidedLink && candidate.id == routeState().avoidedLink.toArea)
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
		routeState().hasAvoidedLink = false;
		return true;
	}

	if (routeState().hasAvoidedLink)
	{
		routeState().hasAvoidedLink = false;
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
	const bool goalReached = roamGoalState().hasRoamGoal && roamGoalState().roamGoalArea == currentArea.area;
	nav::NavCorridor selectedCorridor = {};
	bool hasSelectedCorridor = false;
	bool reservationFallback = false;
	bool reservationBudgetExhausted = false;
	NavRoamReservationFallbackReason reservationFallbackReason =
		NavRoamReservationFallbackReason::None;
	if (!roamGoalState().hasRoamGoal || goalReached)
	{
		if (goalReached && reservationBoard_ != nullptr)
		{
			reservationBoard_->release(actor_);
		}
		NavGoalSelectionReason reason = goalReached
			? GoalSelectionReached
			: roamGoalState().goalSelectionReason == GoalSelectionPathFailed
				? GoalSelectionPathFailed
				: roamGoalState().goalSelectionReason == GoalSelectionReached
					? GoalSelectionReached
				: GoalSelectionInitial;
		NavGoalSelectionStrategy strategy =
			GoalSelectionStrategyOldestVisitedArea;
		nav::AreaId selectedGoal = 0U;
		std::uint32_t oldestVisit = (std::numeric_limits<std::uint32_t>::max)();
		for (const nav::NavArea &candidate : document->areas())
		{
			if (candidate.id == currentArea.area ||
					(roamGoalState().goalSelectionReason == GoalSelectionPathFailed &&
						roamGoalState().failedRoamGoalArea != 0U &&
						candidate.id == roamGoalState().failedRoamGoalArea))
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
					roamGoalState().lastVisitedMapGeneration, roamGoalState().lastVisitedTeam, candidate.id);
			if (selectedGoal == 0U || lastVisited < oldestVisit)
			{
				selectedGoal = candidate.id;
			oldestVisit = lastVisited;
		}
	}
	if (modePolicy_.allowsTacticalTeamOverrides() && reservationBoard_ != nullptr)
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
				(roamGoalState().goalSelectionReason == GoalSelectionPathFailed &&
					roamGoalState().failedRoamGoalArea != 0U &&
					candidate.id == roamGoalState().failedRoamGoalArea) ||
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
						roamGoalState().lastVisitedMapGeneration, roamGoalState().lastVisitedTeam, candidate.id)});
		}
		std::stable_sort(candidates.begin(), candidates.end(),
			[](const GoalCandidate &left, const GoalCandidate &right) {
				return left.lastVisited < right.lastVisited;
			});

		nav::NavQuery query(snapshot);
	if (routeState().hasPathCostContext && !modePolicy_.allowsAdaptiveRouteWeighting())
	{
		query.setPathCostContext(routeState().pathCostContext);
	}
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
					roamGoalState().lastVisitedTeam, actor_, candidate.area))
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
					roamGoalState().lastVisitedTeam, actor_, firstLink.fromArea, firstLink.toArea);
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
						roamGoalState().lastVisitedTeam, actor_, candidate.area))
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
	// In the pinned HuntState::OnUpdate, RANDOM_LONG and the following
	// list traversal execute unconditionally and overwrite the oldest-area
	// candidate. Preserve that draw (including current/small/failed areas)
	// in Compatibility; Enhanced retains its reservation-based selection.
	if (selectedGoal == 0U || !modePolicy_.allowsTacticalTeamOverrides())
	{
			strategy = GoalSelectionStrategyRandomFallback;
			if (document->areas().empty() || randomSource_ == nullptr)
			{
				roamGoalState().hasRoamGoal = false;
				roamGoalState().goalSelectionReason = document->areas().empty()
					? GoalSelectionNoEligibleArea
					: reason;
				roamGoalState().goalSelectionStrategy = document->areas().empty()
					? GoalSelectionStrategyNoEligibleArea
					: strategy;
				if (decision != nullptr)
				{
					decision->goalSelectionReason = roamGoalState().goalSelectionReason;
					decision->goalSelectionStrategy = roamGoalState().goalSelectionStrategy;
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
				roamGoalState().hasRoamGoal = false;
				roamGoalState().goalSelectionReason = GoalSelectionNoEligibleArea;
				roamGoalState().goalSelectionStrategy = GoalSelectionStrategyRandomFallback;
				if (decision != nullptr)
				{
					decision->goalSelectionReason = roamGoalState().goalSelectionReason;
					decision->goalSelectionStrategy = roamGoalState().goalSelectionStrategy;
					decision->failureReason = NavFailureReason::NoGoal;
				}
				return false;
			}
			selectedGoal = document->areas()[static_cast<std::size_t>(result.value)].id;
		}
		roamGoalState().roamGoalArea = selectedGoal;
		const nav::NavArea *goal = document->findArea(roamGoalState().roamGoalArea);
		roamGoalState().roamGoalPosition = goal == nullptr
			? nav::NavVector{0.0f, 0.0f, 0.0f}
			: centerOf(*goal);
		roamGoalState().hasRoamGoal = goal != nullptr;
		if (!roamGoalState().hasRoamGoal)
		{
			roamGoalState().goalSelectionReason = GoalSelectionNoEligibleArea;
			roamGoalState().goalSelectionStrategy = GoalSelectionStrategyNoEligibleArea;
			if (decision != nullptr)
			{
				decision->goalSelectionReason = roamGoalState().goalSelectionReason;
				decision->goalSelectionStrategy = roamGoalState().goalSelectionStrategy;
				decision->failureReason = NavFailureReason::NoGoal;
			}
			return false;
		}
		++roamGoalState().roamGoalGeneration;
		if (roamGoalState().roamGoalGeneration == 0U)
		{
			++roamGoalState().roamGoalGeneration;
		}
		roamGoalState().goalSelectionReason = reason;
		roamGoalState().goalSelectionStrategy = strategy;
	}

	if (!roamGoalState().hasRoamGoal || roamGoalState().roamGoalArea == currentArea.area)
	{
		roamGoalState().hasRoamGoal = false;
		return false;
	}
	if (decision != nullptr)
	{
		decision->goalSelectionReason = roamGoalState().goalSelectionReason;
		decision->goalSelectionStrategy = roamGoalState().goalSelectionStrategy;
		decision->goalGeneration = roamGoalState().roamGoalGeneration;
	}

	nav::NavQuery query(snapshot);
	if (routeState().hasPathCostContext && !modePolicy_.allowsAdaptiveRouteWeighting())
	{
		query.setPathCostContext(routeState().pathCostContext);
	}
	if (decision != nullptr)
	{
		decision->goalPresent = true;
		decision->goalKind = NavGoalKind::Roam;
		decision->goalArea = roamGoalState().roamGoalArea;
		decision->goalPosition = roamGoalState().roamGoalPosition;
		decision->pathRequested = true;
	}
	nav::NavCorridor corridor = std::move(selectedCorridor);
	const nav::NavQueryResult corridorResult = hasSelectedCorridor
		? nav::NavQueryResult::Found
		: buildCorridor(
			query, currentArea.area, roamGoalState().roamGoalArea, nav::NavRouteType::Safest,
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
		roamGoalState().failedRoamGoalArea = roamGoalState().roamGoalArea;
		roamGoalState().hasRoamGoal = false;
		roamGoalState().goalSelectionReason = GoalSelectionPathFailed;
		roamGoalState().goalSelectionStrategy = GoalSelectionStrategyNone;
		if (decision != nullptr)
		{
			decision->goalSelectionReason = roamGoalState().goalSelectionReason;
			decision->goalSelectionStrategy = roamGoalState().goalSelectionStrategy;
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
	if (routeState().hasAvoidedLink && routeState().avoidedLink.fromArea == currentArea.area &&
			link.fromArea == routeState().avoidedLink.fromArea &&
			link.toArea == routeState().avoidedLink.toArea)
	{
		nav::NavCorridor alternativeCorridor = {};
		const nav::NavQueryResult alternativeResult =
			query.buildAlternativeCorridor(
				corridor, routeState().avoidedLink, &alternativeCorridor,
				decision == nullptr || !collectPathStats_
					? nullptr
					: &decision->pathSearchStats);
		if (alternativeResult != nav::NavQueryResult::Found ||
				alternativeCorridor.links.empty() ||
				(alternativeCorridor.links.front().fromArea == routeState().avoidedLink.fromArea &&
				 alternativeCorridor.links.front().toArea == routeState().avoidedLink.toArea))
		{
			roamGoalState().failedRoamGoalArea = roamGoalState().roamGoalArea;
			roamGoalState().hasRoamGoal = false;
			roamGoalState().goalSelectionReason = GoalSelectionPathFailed;
			roamGoalState().goalSelectionStrategy = GoalSelectionStrategyNone;
			if (decision != nullptr)
			{
				decision->corridorResult = alternativeResult;
				decision->pathResult = alternativeResult;
				decision->linkFromArea = routeState().avoidedLink.fromArea;
				decision->linkToArea = routeState().avoidedLink.toArea;
				decision->linkDirection = routeState().avoidedLink.direction;
				decision->linkHow = routeState().avoidedLink.how;
				decision->linkResult = nav::NavQueryResult::Found;
				decision->goalSelectionReason = roamGoalState().goalSelectionReason;
				decision->goalSelectionStrategy = roamGoalState().goalSelectionStrategy;
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
	if (modePolicy_.mode() == compat::RuntimeMode::Compatibility)
	{
		corridor.hasEndpoint = true;
		corridor.endpoint = roamGoalState().roamGoalPosition;
		corridor.endpoint.z = nav::surfaceZAt(*document->findArea(roamGoalState().roamGoalArea),
			corridor.endpoint.x, corridor.endpoint.y);
	}
	if (!startRoute(snapshot, corridor, link))
	{
		roamGoalState().failedRoamGoalArea = roamGoalState().roamGoalArea;
		roamGoalState().hasRoamGoal = false;
		roamGoalState().goalSelectionReason = GoalSelectionPathFailed;
		roamGoalState().goalSelectionStrategy = GoalSelectionStrategyNone;
		if (decision != nullptr)
		{
			decision->linkFromArea = link.fromArea;
			decision->linkToArea = link.toArea;
			decision->linkDirection = link.direction;
			decision->linkHow = link.how;
			decision->linkResult = nav::NavQueryResult::Found;
			decision->goalSelectionReason = roamGoalState().goalSelectionReason;
			decision->goalSelectionStrategy = roamGoalState().goalSelectionStrategy;
			decision->failureReason = traversalState().unsafeDropRejected
				? NavFailureReason::UnsafeDrop
				: NavFailureReason::NavApplyRejected;
		}
		return false;
	}
	if (modePolicy_.allowsTacticalTeamOverrides() && reservationBoard_ != nullptr)
	{
		reservationFallback =
			reservationBoard_->goalReservedByOther(
				roamGoalState().lastVisitedTeam, actor_, roamGoalState().roamGoalArea) ||
			reservationBoard_->firstLinkReservedByOther(
				roamGoalState().lastVisitedTeam, actor_, link.fromArea, link.toArea);
		if (reservationFallback &&
			reservationFallbackReason == NavRoamReservationFallbackReason::None)
		{
			reservationFallbackReason = reservationBudgetExhausted
				? NavRoamReservationFallbackReason::CandidateBudgetExhausted
				: NavRoamReservationFallbackReason::NoDistinctRouteFound;
		}
		reservationBoard_->reserve(
			roamGoalState().lastVisitedTeam, actor_, roamGoalState().roamGoalArea, link.fromArea, link.toArea);
		if (decision != nullptr)
		{
			decision->reservationFallback = reservationFallback;
			decision->reservationFallbackReason =
				reservationFallbackReason;
		}
	}
	rememberRoute(snapshot, corridor, link);
	roamGoalState().failedRoamGoalArea = 0U;
	routeState().hasAvoidedLink = false;
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
	if (routeState().hasPathCostContext && !modePolicy_.allowsAdaptiveRouteWeighting())
	{
		query.setPathCostContext(routeState().pathCostContext);
	}
	std::vector<nav::NavDirectedLink> links;
	if (query.outgoingLinks(currentArea.area, &links) != nav::NavQueryResult::Found)
	{
		return false;
	}
	if (!modePolicy_.allowsAdaptiveRouteWeighting() || reservationBoard_ != nullptr)
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
			traversalState().activeTraversal = traversalActionFor(snapshot, link);
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
	const std::size_t firstCandidate = routeState().nextLinkIndex % document->areas().size();
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
		if (routeState().hasAvoidedLink && link.fromArea == routeState().avoidedLink.fromArea &&
				link.toArea == routeState().avoidedLink.toArea)
		{
			continue;
		}
		if (!startRoute(snapshot, corridor, link))
		{
			continue;
		}
		routeState().nextLinkIndex = candidateIndex + 1U;
		rememberRoute(snapshot, corridor, link);
		if (decision != nullptr)
		{
			populateRouteDecision(decision);
			decision->targetArea = candidate.id;
			decision->targetPosition = centerOf(candidate);
		}
		routeState().hasAvoidedLink = false;
		return true;
	}
	if (routeState().hasAvoidedLink)
	{
		routeState().hasAvoidedLink = false;
		return selectRoamRoute(snapshot, currentArea, decision);
	}
	return false;
}

void NavRoamController::rememberRoute(
	const nav::NavSnapshot &snapshot,
	const nav::NavCorridor &corridor,
	const nav::NavDirectedLink &link)
{
	routeState().activeCorridor = corridor;
	routeState().activeLink = link;
	routeState().activeCorridorIndex = 0U;
	routeState().hasActiveRoute = true;
	corridorProgressAt_ = observedAt_;
	if (routeState().pathSequence == (std::numeric_limits<std::uint32_t>::max)())
	{
		routeState().pathSequence = 1U;
	}
	else
	{
		++routeState().pathSequence;
	}
	const nav::NavDocument *document = snapshot.document();
	const nav::NavArea *targetArea = document != nullptr
			? document->findArea(link.toArea)
			: nullptr;
	routeState().activeTargetPosition = targetArea != nullptr
			? centerOf(*targetArea)
			: nav::NavVector{0.0f, 0.0f, 0.0f};
}

void NavRoamController::populateRouteDecision(NavRoamDecision *decision) const
{
	if (decision == nullptr || !routeState().hasActiveRoute)
	{
		return;
	}
	const bool hadGoal = decision->goalPresent;
	decision->goalPresent = true;
	if (!hadGoal)
	{
		decision->goalKind = roamGoalState().hasObjectiveTarget ? NavGoalKind::Objective : NavGoalKind::Roam;
		decision->goalArea = routeState().activeLink.toArea;
		decision->goalPosition = routeState().activeTargetPosition;
	}
	decision->targetArea = routeState().activeLink.toArea;
	decision->targetPosition = routeState().activeTargetPosition;
	decision->corridorAreaCount = routeState().activeCorridor.areas.size();
	decision->corridorIndex = routeState().activeCorridorIndex;
	decision->linkFromArea = routeState().activeLink.fromArea;
	decision->linkToArea = routeState().activeLink.toArea;
	decision->linkDirection = routeState().activeLink.direction;
	decision->linkHow = routeState().activeLink.how;
	decision->linkResult = nav::NavQueryResult::Found;
	decision->corridorResult = nav::NavQueryResult::Found;
	decision->pathSequence = routeState().pathSequence;
	decision->routeType = routeState().activeCorridor.routeType;
	decision->pathCost = routeState().activeCorridor.cost;
	decision->selectedPath = routeState().activeCorridor.areas;
}

void NavRoamController::resetRoute(bool preserveRoamGoal)
{
	friendWaitState().waitingBehindFriend = false;
	friendWaitState().friendInTheWay = false;
	friendWaitState().friendCheckAt = -1.0f;
	friendWaitState().friendWaitUntil = 0.0f;
	if (!preserveRoamGoal && reservationBoard_ != nullptr)
	{
		reservationBoard_->release(actor_);
	}
	if(timedCompatibility_)traversalState().locomotion.clearPath();
	else traversalState().locomotion = nav::LocomotionController(roamLocomotionConfig(modePolicy_.mode()));
	traversalState().activeTraversal = nav::TraversalAction::Walk;
	traversalState().jumpDrop = nav::JumpDropController(roamJumpDropConfig(
			modePolicy_.allowsAdaptiveRouteWeighting()
				? compat::RuntimeMode::Enhanced
				: compat::RuntimeMode::Compatibility));
	traversalState().specialTraversal = nav::SpecialTraversalController();
	routeState().activeCorridor = {};
	routeState().activeLink = {};
	routeState().activeTargetPosition = {0.0f, 0.0f, 0.0f};
	if (!preserveRoamGoal)
	{
		roamGoalState().hasRoamGoal = false;
		roamGoalState().roamGoalArea = 0U;
		roamGoalState().roamGoalPosition = {0.0f, 0.0f, 0.0f};
	}
	routeState().lastIntentDirection = {0.0f, 0.0f, 0.0f};
	recoveryState().stuckRecoveryDirection = {0.0f, 0.0f, 0.0f};
	routeState().activeCorridorIndex = 0U;
	recoveryState().stuckRecoveryCount = 0U;
	recoveryState().stuckRecoveryFramesRemaining = 0U;
	recoveryState().stuckRecoveryActive = false;
	routeState().hasActiveRoute = false;
}

void NavRoamController::destroyPath()
{
	resetRoute(true);
}

void NavRoamController::resetProgressMonitor()
{
	traversalState().locomotion.resetProgressMonitor();
	recoveryState().stuckRecoveryActive=false;
	recoveryState().stuckJumpScheduled=false;
}

void NavRoamController::observeMotion(ActorId actor,world::FrameIdentity frame,
	const nav::LocomotionObservation &observation)
{
	if(actor.slot==0U || actor.actorGeneration==0U || !frame.isValid())return;
	if(!sameActor(actor,motionActor_) || frame.mapGeneration!=motionFrame_.mapGeneration ||
		frame.roundGeneration!=motionFrame_.roundGeneration)resetProgressMonitor();
	else if(frame.tick<=motionFrame_.tick)return;
	motionActor_=actor;motionFrame_=frame;
	traversalState().locomotion.observeMotion(observation);
}
}
}
