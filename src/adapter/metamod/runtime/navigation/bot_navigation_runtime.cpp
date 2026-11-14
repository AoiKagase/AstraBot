#include "runtime/navigation/bot_navigation_runtime.hpp"
#include "runtime/shared/bot_runtime_support.hpp"
namespace astrabot
{
namespace metamod
{
using namespace runtime_detail;
namespace
{
constexpr float kAreaVerticalTolerance = 64.0f;
constexpr float kDescendingEdgeDropThreshold = 8.0f;
constexpr auto kMovementWarmupFrameCount = 2U;
constexpr float kStandingClearance = 72.0f;
constexpr float kCrouchingClearance = 36.0f;
constexpr float kFarGroundProbeDistance = 80.0f;
constexpr float kNearGroundProbeDistance = 30.0f;
// Keep literal evaluation precision on x87; each macro is undefined below.
#define ASTRABOT_BOT_NAVIGATION_RUNTIME_MINIMUM_UNIT_DIRECTION_LENGTH 0.9f
#define ASTRABOT_BOT_NAVIGATION_RUNTIME_MAXIMUM_UNIT_DIRECTION_LENGTH 1.1f
constexpr float kMinimumRunningSpeed = 200.0f;
constexpr auto kMovementPhysicsDiagnosticInterval = 16U;
constexpr float kProfilePercentageScale = 100.0f;
constexpr float kGapGroundProbeDistance = 10.0f;
constexpr float kHorizontalGapThreshold = 1.0f;
}

void BotNavigationRuntime::recordMovementPhysicsSample(
	std::size_t index, const runtime::MovementPhysicsState &before, const runtime::CommandReceipt &receipt)
{
	if (index >= NativeBotObservation::kClientSlotCount || !services_->service_managedBotSlots()[index])
	{
		return;
	}
	const FakeClientHandle &handle = services_->service_managedBotHandles()[index];
	runtime::MovementPhysicsSample sample{};
	sample.actor = handle.actor;
	sample.frame = {services_->service_lifecycle().mapGeneration(), services_->service_lifecycle().roundGeneration(),
		services_->service_adapterFrameCount()};
	sample.commandSequence = receipt.sequence;
	sample.before = before;
	sample.after = services_->observation_captureMovementPhysicsState(
		handle.entity, services_->service_joinControllers()[index].teamConfirmed(), true);
	sample.dispatched = receipt.result == runtime::DispatchResult::Dispatched;
	if (sample.dispatched)
	{
		services_->service_runtimeProfiler().recordRunPlayerMove();
	}
	sample.readiness = runtime::spawnReadiness(sample.after);
	actors_[index].movementPhysicsSamples = sample;
	if (sample.after.dead || sample.after.spectator)
	{
		actors_[index].movementReadyLogged = false;
	}
	else if (sample.readiness == runtime::SpawnReadiness::Ready && !actors_[index].movementReadyLogged)
	{
		actors_[index].movementDiagnosticSamples = 0U;
		actors_[index].movementReadyLogged = true;
	}

	const float maxSpeed = handle.entity != nullptr ? handle.entity->v.maxspeed : 0.0f;
	if (sample.dispatched)
	{
		if (actors_[index].movementDispatchFrames != (std::numeric_limits<std::uint32_t>::max)() &&
			actors_[index].movementDispatchFrames == services_->service_adapterFrameCount() &&
			gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogError != nullptr &&
			services_->service_pluginId() != nullptr)
		{
			gpMetaUtilFuncs->pfnLogError(services_->service_pluginId(), "movement duplicate dispatch slot=%u frame=%u",
				static_cast<unsigned int>(index + 1U),
				static_cast<unsigned int>(services_->service_adapterFrameCount()));
		}
		actors_[index].movementDispatchFrames = services_->service_adapterFrameCount();
	}

	if (gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr &&
		services_->service_pluginId() != nullptr && services_->service_runtimeProfiler().enabled() &&
		(actors_[index].movementDiagnosticSamples < kMovementPhysicsLogLimit ||
			(receipt.sequence != 0U && (receipt.sequence % kMovementPhysicsDiagnosticInterval) == 0U)))
	{
		profiledLogConsole(services_->service_runtimeProfiler(), services_->service_pluginId(),
			"movement physics actor=%u generation=%u frame=%u sequence=%u dispatched=%d "
			"before=(%.1f %.1f %.1f) after=(%.1f %.1f %.1f) velocity=(%.1f %.1f %.1f) "
			"grounded=%d ducked=%d ladder=%d groundentity=%u flags=%d deadflag=%d "
			"team=%d teamConfirmed=%d solid=%d movetype=%d maxspeed=%.1f health=%.1f ready=%d "
			"result=%d map=%u round=%u now=%.3f",
			static_cast<unsigned int>(sample.actor.slot), static_cast<unsigned int>(sample.actor.actorGeneration),
			static_cast<unsigned int>(services_->service_adapterFrameCount()),
			static_cast<unsigned int>(sample.commandSequence), sample.dispatched ? 1 : 0, sample.before.origin.x,
			sample.before.origin.y, sample.before.origin.z, sample.after.origin.x, sample.after.origin.y,
			sample.after.origin.z, sample.after.velocity.x, sample.after.velocity.y, sample.after.velocity.z,
			sample.after.grounded ? 1 : 0, sample.after.ducked ? 1 : 0, sample.after.onLadder ? 1 : 0,
			static_cast<unsigned int>(sample.after.groundEntityIndex), sample.after.flags, sample.after.deadflag,
			sample.after.team, sample.after.teamConfirmed ? 1 : 0, sample.after.solid, sample.after.movetype, maxSpeed,
			sample.after.health, sample.readiness == runtime::SpawnReadiness::Ready ? 1 : 0,
			static_cast<int>(receipt.result), sample.frame.mapGeneration, sample.frame.roundGeneration,
			services_->service_globals() != nullptr ? services_->service_globals()->time : 0.0f);
		if (actors_[index].movementDiagnosticSamples < kMovementPhysicsLogLimit)
			++actors_[index].movementDiagnosticSamples;
	}
	const bool jumpCommand = index < NativeBotObservation::kClientSlotCount &&
		services_->command_managedBotCommandTemplateValid(index) &&
		(services_->command_managedBotCommandTemplates(index).movement.buttons & static_cast<std::uint16_t>(IN_JUMP)) !=
			0U;
	if (jumpCommand && services_->service_runtimeProfiler().enabled() && gpMetaUtilFuncs != nullptr &&
		gpMetaUtilFuncs->pfnLogConsole != nullptr && services_->service_pluginId() != nullptr)
	{
		profiledLogConsole(services_->service_runtimeProfiler(), services_->service_pluginId(),
			"profile jumpFeedback bot_id=%u command_sequence=%u dispatched=%d "
			"before_origin=(%.1f %.1f %.1f) after_origin=(%.1f %.1f %.1f) "
			"after_velocity=(%.1f %.1f %.1f) after_grounded=%d",
			static_cast<unsigned int>(sample.actor.slot), static_cast<unsigned int>(sample.commandSequence),
			sample.dispatched ? 1 : 0, sample.before.origin.x, sample.before.origin.y, sample.before.origin.z,
			sample.after.origin.x, sample.after.origin.y, sample.after.origin.z, sample.after.velocity.x,
			sample.after.velocity.y, sample.after.velocity.z, sample.after.grounded ? 1 : 0);
	}
}

nav::NavPathCostContext BotNavigationRuntime::managedPathCostContext(std::size_t index)
{
	RuntimeProfilerScope costScope(services_->service_runtimeProfiler(), RuntimeProfilerStage::PathCostContext);
	nav::NavPathCostContext cost{};
	if (index >= services_->service_managedBotHandles().size() ||
		services_->service_managedBotHandles()[index].entity == nullptr)
	{
		return cost;
	}
	const auto &handle = services_->service_managedBotHandles()[index];
	cost.health = std::isfinite(handle.entity->v.health) ? (std::max)(0.0f, handle.entity->v.health) : 0.0f;
	cost.aggression =
		static_cast<float>(services_->behavior_managedProfiles(index).aggression) / kProfilePercentageScale;
	cost.attacking = services_->behavior_managedBotStateMachines(index).attackOverlayActive();
	const auto navigation = services_->service_navPublisher().snapshot();
	const auto *document = navigation.document();
	if (document == nullptr || services_->service_globals() == nullptr)
	{
		return cost;
	}
	for (const auto &area : document->areas())
	{
		float danger = 0.0f;
		if (dangerMemory_.mapGeneration() == services_->service_lifecycle().mapGeneration() &&
			dangerMemory_.query(services_->service_globals()->time,
				static_cast<std::uint8_t>(services_->observation_resolvePlayerTeam(handle.entity)), area.id, &danger) &&
			danger > 0.0f)
		{
			cost.areas.push_back({area.id, danger, 0U});
		}
	}
	nav::NavQuery query(navigation);
	for (const auto &position : services_->observation_liveFriendlyPositions(index, true))
	{
		nav::NavAreaMatch area{};
		if (query.findContaining(position, kAreaVerticalTolerance, &area) != nav::NavQueryResult::Found)
		{
			continue;
		}
		auto found = std::find_if(cost.areas.begin(), cost.areas.end(),
			[&area](const nav::NavAreaCostState &entry)
			{
				return entry.area == area.area;
			});
		if (found == cost.areas.end())
		{
			cost.areas.push_back({area.area, 0.0f, 1U});
		}
		else
		{
			++found->teammates;
		}
	}
	return cost;
}

void BotNavigationRuntime::logMovementDiagnostic(
	std::size_t index, const char *reason, const runtime::NavRoamDecision *decision)
{
	if (index >= NativeBotObservation::kClientSlotCount || actors_[index].movementDiagnosticAttempts ||
		gpMetaUtilFuncs == nullptr || gpMetaUtilFuncs->pfnLogConsole == nullptr ||
		services_->service_pluginId() == nullptr)
	{
		return;
	}
	const bool important = decision == nullptr || decision->failureReason != runtime::NavFailureReason::None;
	if (!important && !services_->service_runtimeProfiler().enabled())
		return;

	const FakeClientHandle &handle = services_->service_managedBotHandles()[index];
	const bool fakeClient = handle.entity != nullptr && (handle.entity->v.flags & FL_FAKECLIENT) != 0;
	const bool spectator = handle.entity != nullptr && (handle.entity->v.flags & FL_SPECTATOR) != 0;
	const nav::NavSnapshot navigation = services_->service_navPublisher().snapshot();
	nav::NavAreaMatch area = {};
	float feetOffset = handle.entity != nullptr ? handle.entity->v.mins.z : 0.0f;
	if (!std::isfinite(feetOffset) || feetOffset >= 0.0f)
	{
		feetOffset = -kStandingHalfHumanHeight;
	}
	const nav::NavVector feetPosition = handle.entity != nullptr
		? nav::NavVector{handle.entity->v.origin.x, handle.entity->v.origin.y, handle.entity->v.origin.z + feetOffset}
		: nav::NavVector{0.0f, 0.0f, 0.0f};
	const nav::NavQueryResult areaResult =
		nav::NavQuery(navigation).findContaining(feetPosition, kAreaVerticalTolerance, &area);
	importantLogConsole(services_->service_runtimeProfiler(), services_->service_pluginId(),
		"movement diagnostic actor=%u generation=%u frame=%u reason=%s fake=%d spectator=%d "
		"team=%d deadflag=%d health=%.1f origin=(%.1f %.1f %.1f) solid=%d movetype=%d effects=%d "
		"nav=%d navAreaResult=%d area=%u runmove=%d stage=%d currentResult=%d nearestResult=%d "
		"currentArea=%u recoveryArea=%u targetArea=%u target=(%.1f %.1f %.1f) intent=(%.2f %.2f "
		"%.2f) observedVelocity=(%.1f %.1f %.1f) corridorAreas=%u corridorIndex=%u link=(%u->%u "
		"how=%u dir=%u) linkResult=%d corridorResult=%d locomotionResult=%d "
		"nearestDistanceSquared=%.1f goalPresent=%d goalKind=%d goalArea=%u pathRequested=%d "
		"pathResult=%d failureReason=%d fullUpdate=%u recomputeReason=%d searchCalls=%u "
		"expanded=%u enqueues=%u reopens=%u staleQueue=%u equalCostRepl=%u searchUsec=%llu "
		"searchMaxUsec=%llu searchFirstId=%llu searchLastId=%llu routeType=%d",
		static_cast<unsigned int>(handle.actor.slot), static_cast<unsigned int>(handle.actor.actorGeneration),
		static_cast<unsigned int>(services_->service_adapterFrameCount()), reason != nullptr ? reason : "unknown",
		fakeClient ? 1 : 0, spectator ? 1 : 0, handle.entity != nullptr ? handle.entity->v.team : -1,
		handle.entity != nullptr ? handle.entity->v.deadflag : -1,
		handle.entity != nullptr ? handle.entity->v.health : 0.0f,
		handle.entity != nullptr ? handle.entity->v.origin.x : 0.0f,
		handle.entity != nullptr ? handle.entity->v.origin.y : 0.0f,
		handle.entity != nullptr ? handle.entity->v.origin.z : 0.0f,
		handle.entity != nullptr ? handle.entity->v.solid : -1,
		handle.entity != nullptr ? handle.entity->v.movetype : -1,
		handle.entity != nullptr ? handle.entity->v.effects : -1, navigation.isValid() ? 1 : 0,
		static_cast<int>(areaResult), static_cast<unsigned int>(area.area),
		services_->service_engineFunctions() != nullptr &&
				services_->service_engineFunctions()->pfnRunPlayerMove != nullptr
			? 1
			: 0,
		decision != nullptr ? static_cast<int>(decision->stage) : -1,
		decision != nullptr ? static_cast<int>(decision->currentAreaResult) : -1,
		decision != nullptr ? static_cast<int>(decision->nearestAreaResult) : -1,
		decision != nullptr ? static_cast<unsigned int>(decision->currentArea) : 0U,
		decision != nullptr ? static_cast<unsigned int>(decision->recoveryArea) : 0U,
		decision != nullptr ? static_cast<unsigned int>(decision->targetArea) : 0U,
		decision != nullptr ? decision->targetPosition.x : 0.0f,
		decision != nullptr ? decision->targetPosition.y : 0.0f,
		decision != nullptr ? decision->targetPosition.z : 0.0f,
		decision != nullptr ? decision->intentDirection.x : 0.0f,
		decision != nullptr ? decision->intentDirection.y : 0.0f,
		decision != nullptr ? decision->intentDirection.z : 0.0f,
		decision != nullptr ? decision->observationVelocity.x : 0.0f,
		decision != nullptr ? decision->observationVelocity.y : 0.0f,
		decision != nullptr ? decision->observationVelocity.z : 0.0f,
		decision != nullptr ? static_cast<unsigned int>(decision->corridorAreaCount) : 0U,
		decision != nullptr ? static_cast<unsigned int>(decision->corridorIndex) : 0U,
		decision != nullptr ? static_cast<unsigned int>(decision->linkFromArea) : 0U,
		decision != nullptr ? static_cast<unsigned int>(decision->linkToArea) : 0U,
		decision != nullptr ? static_cast<unsigned int>(decision->linkHow) : 0U,
		decision != nullptr ? static_cast<unsigned int>(decision->linkDirection) : 0U,
		decision != nullptr ? static_cast<int>(decision->linkResult) : -1,
		decision != nullptr ? static_cast<int>(decision->corridorResult) : -1,
		decision != nullptr ? static_cast<int>(decision->locomotionResult) : -1,
		decision != nullptr ? decision->nearestDistanceSquared : 0.0f,
		decision != nullptr && decision->goalPresent ? 1 : 0,
		decision != nullptr ? static_cast<int>(decision->goalKind) : -1,
		decision != nullptr ? static_cast<unsigned int>(decision->goalArea) : 0U,
		decision != nullptr && decision->pathRequested ? 1 : 0,
		decision != nullptr ? static_cast<int>(decision->pathResult) : -1,
		decision != nullptr ? static_cast<int>(decision->failureReason) : -1,
		decision != nullptr ? decision->fullUpdateSequence : 0U,
		decision != nullptr ? static_cast<int>(decision->recomputeReason) : -1,
		decision != nullptr ? decision->pathSearchStats.searchCalls : 0U,
		decision != nullptr ? decision->pathSearchStats.expandedUniqueAreas : 0U,
		decision != nullptr ? decision->pathSearchStats.enqueueCount : 0U,
		decision != nullptr ? decision->pathSearchStats.reopenCount : 0U,
		decision != nullptr ? decision->pathSearchStats.staleQueueEntries : 0U,
		decision != nullptr ? decision->pathSearchStats.equalCostReplacements : 0U,
		decision != nullptr ? static_cast<unsigned long long>(decision->pathSearchStats.totalUsec) : 0ULL,
		decision != nullptr ? static_cast<unsigned long long>(decision->pathSearchStats.maxUsec) : 0ULL,
		decision != nullptr ? static_cast<unsigned long long>(decision->pathSearchStats.firstSearchId) : 0ULL,
		decision != nullptr ? static_cast<unsigned long long>(decision->pathSearchStats.lastSearchId) : 0ULL,
		decision != nullptr ? static_cast<int>(decision->routeType) : -1);
	actors_[index].movementDiagnosticAttempts = true;
}

void BotNavigationRuntime::logGoalAssignmentDiagnostic(std::size_t index, const runtime::MovementPhysicsState &before,
	const runtime::NavRoamDecision &decision, const nav::LocomotionIntent &intent, runtime::NavRoamResult roamResult,
	const nav::GroundLookaheadObservation &ground, const runtime::GroundLookaheadRoute &probeRoute)
{
	if (!services_->service_runtimeProfiler().enabled() || index >= NativeBotObservation::kClientSlotCount ||
		services_->service_globals() == nullptr || gpMetaUtilFuncs == nullptr ||
		gpMetaUtilFuncs->pfnLogConsole == nullptr || services_->service_pluginId() == nullptr)
	{
		return;
	}
	const double now = std::isfinite(static_cast<double>(services_->service_globals()->time))
		? static_cast<double>(services_->service_globals()->time)
		: 0.0;
	const std::uint32_t second = now <= 0.0 ? 0U : static_cast<std::uint32_t>(now);
	if (actors_[index].movementGoalDiagnosticSeconds == second)
	{
		return;
	}
	actors_[index].movementGoalDiagnosticSeconds = second;
	const ManagedObjectiveTargetCache &cache = services_->behavior_managedBotObjectiveTargets(index);
	const auto *entity = services_->service_managedBotHandles()[index].entity;
	const int rawTeam = entity != nullptr ? entity->v.team : 0;
	const int effectiveTeam = services_->observation_resolvePlayerTeam(entity);
	const bool teamInfoKnown = services_->observation_publicTeamKnown(index);
	const char *goalSource = decision.goalKind == runtime::NavGoalKind::Objective ? "objective"
		: decision.goalKind == runtime::NavGoalKind::Roam						  ? "roam"
																				  : "none";
	profiledLogConsole(services_->service_runtimeProfiler(), services_->service_pluginId(),
		"profile goalAssignment bot_id=%u raw_team=%d effective_team=%d "
		"team_info=%d team_fresh=%d carrying_c4=%d state=%s task=%s "
		"disposition=UNAVAILABLE goal_present=%d goal_kind=%d goal_area=%u "
		"goal_position=(%.1f %.1f %.1f) goal_source=%s objective_target_id=%u "
		"goal_generation=%u cache_hit=%d cache_scope=actor corridor_goal_area=%u "
		"next_area=%u movement_target=(%.1f %.1f %.1f) current_area=%u",
		static_cast<unsigned int>(services_->service_managedBotHandles()[index].actor.slot), rawTeam, effectiveTeam,
		teamInfoKnown ? effectiveTeam : 0, teamInfoKnown ? 1 : 0, cache.carryingBomb ? 1 : 0,
		compat::toString(services_->behavior_managedBotStateMachines(index).state()),
		compat::toString(services_->behavior_managedBotStateMachines(index).task()), decision.goalPresent ? 1 : 0,
		static_cast<int>(decision.goalKind), static_cast<unsigned int>(decision.goalArea), decision.goalPosition.x,
		decision.goalPosition.y, decision.goalPosition.z, goalSource,
		static_cast<unsigned int>(cache.selectedSiteIdentity), static_cast<unsigned int>(cache.generation),
		cache.lastCacheHit ? 1 : 0, static_cast<unsigned int>(decision.goalArea),
		static_cast<unsigned int>(intent.targetArea), decision.targetPosition.x, decision.targetPosition.y,
		decision.targetPosition.z, static_cast<unsigned int>(decision.currentArea));
	profiledLogConsole(services_->service_runtimeProfiler(), services_->service_pluginId(),
		"profile goalAssignmentCorrelation bot_id=%u actor_generation=%u "
		"frame=%u nav_update=%u goal_generation=%u reselection_reason=%s "
		"selection_strategy=%s objective_generation=%u selected_site_id=%u "
		"cache_hit=%d nav_result=%d stage=%d recompute_reason=%d path_result=%d "
		"failure_reason=%d path_search_calls=%u path_search_expanded=%u path_search_enqueues=%u "
		"path_search_failures=%u locomotion_result=%d path_sequence=%u "
		"route_type=%d path_cost=%.1f goal_kind=%d goal_area=%u current_area=%u "
		"reservation_fallback=%d fallback_reason=%u "
		"path_link=(%u->%u how=%u dir=%u) input=(%.2f %.2f %.2f) "
		"speed=%.1f observed_origin=(%.1f %.1f %.1f) grounded=%d",
		static_cast<unsigned int>(services_->service_managedBotHandles()[index].actor.slot),
		static_cast<unsigned int>(services_->service_managedBotHandles()[index].actor.actorGeneration),
		static_cast<unsigned int>(services_->service_adapterFrameCount()), decision.fullUpdateSequence,
		decision.goalGeneration, goalSelectionReasonName(decision.goalSelectionReason),
		goalSelectionStrategyName(decision.goalSelectionStrategy), static_cast<unsigned int>(cache.generation),
		static_cast<unsigned int>(cache.selectedSiteIdentity), cache.lastCacheHit ? 1 : 0, static_cast<int>(roamResult),
		static_cast<int>(decision.stage), static_cast<int>(decision.recomputeReason),
		static_cast<int>(decision.pathResult), static_cast<int>(decision.failureReason),
		static_cast<unsigned int>(decision.pathSearchStats.searchCalls),
		static_cast<unsigned int>(decision.pathSearchStats.expandedUniqueAreas),
		static_cast<unsigned int>(decision.pathSearchStats.enqueueCount),
		static_cast<unsigned int>(decision.pathSearchStats.failureCount), static_cast<int>(decision.locomotionResult),
		decision.pathSequence, static_cast<int>(decision.routeType), decision.pathCost,
		static_cast<int>(decision.goalKind), static_cast<unsigned int>(decision.goalArea),
		static_cast<unsigned int>(decision.currentArea), decision.reservationFallback ? 1 : 0,
		static_cast<unsigned int>(decision.reservationFallbackReason), static_cast<unsigned int>(decision.linkFromArea),
		static_cast<unsigned int>(decision.linkToArea), static_cast<unsigned int>(decision.linkHow),
		static_cast<unsigned int>(decision.linkDirection), intent.direction.x, intent.direction.y, intent.direction.z,
		intent.speed, before.origin.x, before.origin.y, before.origin.z, before.grounded ? 1 : 0);
	profiledLogConsole(services_->service_runtimeProfiler(), services_->service_pluginId(),
		"profile groundRecovery bot_id=%u frame=%u now=%.3f "
		"full_update=%u route_frame=%u route_update=%u route_age=%.3f "
		"ground_valid=%d sampled_target=%u intent_target=%u "
		"sample_direction=(%.3f %.3f %.3f) feet_z=%.1f "
		"far80=(%d %d %.1f %.3f) near30=(%d %d %.1f %.3f) gap10=(%d %d %.1f %.3f) "
		"recovery_area=%u recovery_target=(%.1f %.1f %.1f) "
		"recovery_no_progress_updates=%u geometry_only=%d candidate_failed=%d failure_reason=%d",
		static_cast<unsigned int>(services_->service_managedBotHandles()[index].actor.slot),
		static_cast<unsigned int>(services_->service_adapterFrameCount()), services_->service_globals()->time,
		services_->service_managedBotFullUpdateSequences()[index], probeRoute.frame.tick, probeRoute.fullUpdateSequence,
		services_->service_globals()->time - probeRoute.createdAt, ground.valid ? 1 : 0, ground.targetArea,
		intent.targetArea, ground.direction.x, ground.direction.y, ground.direction.z, decision.observationPosition.z,
		ground.far80.sampled ? 1 : 0, ground.far80.hasGround ? 1 : 0, ground.far80.floorZ, ground.far80.normalZ,
		ground.near30.sampled ? 1 : 0, ground.near30.hasGround ? 1 : 0, ground.near30.floorZ, ground.near30.normalZ,
		ground.gap10.sampled ? 1 : 0, ground.gap10.hasGround ? 1 : 0, ground.gap10.floorZ, ground.gap10.normalZ,
		decision.recoveryArea, decision.targetPosition.x, decision.targetPosition.y, decision.targetPosition.z,
		decision.recoveryNoProgressUpdates, decision.recoveryGeometryOnly ? 1 : 0,
		decision.recoveryCandidateFailed ? 1 : 0, static_cast<int>(decision.failureReason));
}

void BotNavigationRuntime::logTraversalDiagnostic(std::size_t index, const runtime::MovementPhysicsState &before,
	const runtime::NavRoamDecision &decision, const nav::LocomotionIntent &intent, std::uint16_t buttons,
	const nav::LocomotionObservation &locomotion)
{
	if (!services_->service_runtimeProfiler().enabled() || index >= NativeBotObservation::kClientSlotCount ||
		services_->service_globals() == nullptr || gpMetaUtilFuncs == nullptr ||
		gpMetaUtilFuncs->pfnLogConsole == nullptr || services_->service_pluginId() == nullptr)
	{
		return;
	}
	const double now = std::isfinite(static_cast<double>(services_->service_globals()->time))
		? static_cast<double>(services_->service_globals()->time)
		: 0.0;
	const std::uint32_t second = now <= 0.0 ? 0U : static_cast<std::uint32_t>(now);
	std::uint8_t currentAttributes = 0U;
	std::uint8_t nextAttributes = 0U;
	const nav::NavSnapshot navigation = services_->service_navPublisher().snapshot();
	const nav::NavDocument *document = navigation.document();
	const nav::NavArea *sourceArea = document == nullptr ? nullptr : document->findArea(intent.currentArea);
	const nav::NavArea *targetArea = document == nullptr ? nullptr : document->findArea(intent.targetArea);
	float sourceFloorZ = std::numeric_limits<float>::quiet_NaN();
	float targetFloorZ = intent.targetPosition.z;
	float horizontalGap = std::numeric_limits<float>::quiet_NaN();
	float dropHeight = std::numeric_limits<float>::quiet_NaN();
	if (sourceArea != nullptr && targetArea != nullptr)
	{
		sourceFloorZ = nav::surfaceZAt(*sourceArea, intent.targetPosition.x, intent.targetPosition.y);
		dropHeight = sourceFloorZ - targetFloorZ;
		const auto axisGap = [](float firstLo, float firstHi, float secondLo, float secondHi) noexcept
		{
			if (firstHi < secondLo)
			{
				return secondLo - firstHi;
			}
			if (secondHi < firstLo)
			{
				return firstLo - secondHi;
			}
			return 0.0f;
		};
		const float gapX =
			axisGap(sourceArea->extent.lo.x, sourceArea->extent.hi.x, targetArea->extent.lo.x, targetArea->extent.hi.x);
		const float gapY =
			axisGap(sourceArea->extent.lo.y, sourceArea->extent.hi.y, targetArea->extent.lo.y, targetArea->extent.hi.y);
		horizontalGap = std::hypot(gapX, gapY);
	}
	const bool descendingOrGapEdge = std::isfinite(dropHeight) && dropHeight > kDescendingEdgeDropThreshold;
	const bool horizontalGapEdge = std::isfinite(horizontalGap) && horizontalGap > kHorizontalGapThreshold;
	if (intent.traversal == nav::TraversalAction::Walk && !descendingOrGapEdge && !horizontalGapEdge)
	{
		return;
	}
	if (actors_[index].movementTraversalDiagnosticSeconds == second)
	{
		return;
	}
	actors_[index].movementTraversalDiagnosticSeconds = second;
	if (document != nullptr)
	{
		const nav::NavArea *current = document->findArea(decision.currentArea);
		const nav::NavArea *next = document->findArea(intent.targetArea);
		currentAttributes = current == nullptr ? 0U : current->attributes;
		nextAttributes = next == nullptr ? 0U : next->attributes;
	}
	const float distanceToTransition =
		std::hypot(decision.targetPosition.x - before.origin.x, decision.targetPosition.y - before.origin.y);
	profiledLogConsole(services_->service_runtimeProfiler(), services_->service_pluginId(),
		"profile traversal bot_id=%u map=%u round=%u actor_generation=%u "
		"current_area=%u next_area=%u "
		"connection_direction=%u connection_how=%u current_attributes=%u "
		"next_attributes=%u traversal_intent=%d jump_required=%d "
		"source_floor_z=%.1f target_floor_z=%.1f drop_height=%.1f "
		"horizontal_gap=%.1f safe_drop_available=%d max_safe_drop=%.1f "
		"target_point=(%.1f %.1f %.1f) distance_to_transition=%.1f "
		"before_grounded=%d before_ducked=%d before_velocity_z=%.1f "
		"buttons=%u IN_JUMP=%d "
		"stuck_time=%d recovery_state=%d corridor_index=%u",
		static_cast<unsigned int>(services_->service_managedBotHandles()[index].actor.slot),
		static_cast<unsigned int>(services_->service_lifecycle().mapGeneration()),
		static_cast<unsigned int>(services_->service_lifecycle().roundGeneration()),
		static_cast<unsigned int>(services_->service_managedBotHandles()[index].actor.actorGeneration),
		static_cast<unsigned int>(decision.currentArea), static_cast<unsigned int>(intent.targetArea),
		static_cast<unsigned int>(decision.linkDirection), static_cast<unsigned int>(decision.linkHow),
		static_cast<unsigned int>(currentAttributes), static_cast<unsigned int>(nextAttributes),
		static_cast<int>(intent.traversal), intent.traversal == nav::TraversalAction::Jump ? 1 : 0, sourceFloorZ,
		targetFloorZ, dropHeight, horizontalGap, locomotion.safeDropHeightAvailable ? 1 : 0,
		locomotion.maximumSafeDropHeight, decision.targetPosition.x, decision.targetPosition.y,
		decision.targetPosition.z, distanceToTransition, before.grounded ? 1 : 0, before.ducked ? 1 : 0,
		before.velocity.z, static_cast<unsigned int>(buttons),
		(buttons & static_cast<std::uint16_t>(IN_JUMP)) != 0U ? 1 : 0,
		decision.locomotionResult == nav::LocomotionResult::Stuck ? 1 : 0, static_cast<int>(decision.failureReason),
		static_cast<unsigned int>(decision.corridorIndex));
	profiledLogConsole(services_->service_runtimeProfiler(), services_->service_pluginId(),
		"profile traversalCorrelation bot_id=%u actor_generation=%u frame=%u "
		"map=%u round=%u "
		"nav_update=%u goal_generation=%u reselection_reason=%s "
		"selection_strategy=%s current_area=%u path_link=(%u->%u how=%u dir=%u) "
		"target_area=%u input=(%.2f %.2f %.2f) speed=%.1f buttons=%u "
		"observed_origin=(%.1f %.1f %.1f) grounded=%d",
		static_cast<unsigned int>(services_->service_managedBotHandles()[index].actor.slot),
		static_cast<unsigned int>(services_->service_managedBotHandles()[index].actor.actorGeneration),
		static_cast<unsigned int>(services_->service_adapterFrameCount()),
		static_cast<unsigned int>(services_->service_lifecycle().mapGeneration()),
		static_cast<unsigned int>(services_->service_lifecycle().roundGeneration()), decision.fullUpdateSequence,
		decision.goalGeneration, goalSelectionReasonName(decision.goalSelectionReason),
		goalSelectionStrategyName(decision.goalSelectionStrategy), static_cast<unsigned int>(decision.currentArea),
		static_cast<unsigned int>(decision.linkFromArea), static_cast<unsigned int>(decision.linkToArea),
		static_cast<unsigned int>(decision.linkHow), static_cast<unsigned int>(decision.linkDirection),
		static_cast<unsigned int>(intent.targetArea), intent.direction.x, intent.direction.y, intent.direction.z,
		intent.speed, static_cast<unsigned int>(buttons), before.origin.x, before.origin.y, before.origin.z,
		before.grounded ? 1 : 0);
	PendingTraversalDiagnostic &pending = actors_[index].movementTraversalPending;
	pending = {};
	pending.active = true;
	pending.actorGeneration = services_->service_managedBotHandles()[index].actor.actorGeneration;
	pending.commandFrame = services_->service_adapterFrameCount();
	pending.navUpdateSequence = decision.fullUpdateSequence;
	pending.goalGeneration = decision.goalGeneration;
	pending.mapGeneration = services_->service_lifecycle().mapGeneration();
	pending.roundGeneration = services_->service_lifecycle().roundGeneration();
	pending.corridorIndex = decision.corridorIndex;
	pending.goalSelectionReason = decision.goalSelectionReason;
	pending.goalSelectionStrategy = decision.goalSelectionStrategy;
	pending.currentArea = decision.currentArea;
	pending.targetArea = intent.targetArea;
	pending.targetPosition = decision.targetPosition;
	pending.intentDirection = intent.direction;
	pending.intentSpeed = intent.speed;
	pending.sourceFloorZ = sourceFloorZ;
	pending.targetFloorZ = targetFloorZ;
	pending.dropHeight = dropHeight;
	pending.horizontalGap = horizontalGap;
	pending.maximumSafeDropHeight = locomotion.maximumSafeDropHeight;
	pending.safeDropHeightAvailable = locomotion.safeDropHeightAvailable;
	pending.linkDirection = decision.linkDirection;
	pending.linkHow = decision.linkHow;
	pending.action = intent.traversal;
	pending.buttons = buttons;
	pending.startPosition = before.origin;
	pending.startVelocityZ = before.velocity.z;
	pending.startGrounded = before.grounded;
	pending.startHealth = before.health;
}

void BotNavigationRuntime::logTraversalOutcome(std::size_t index, const runtime::MovementPhysicsState &after,
	const runtime::NavRoamDecision &decision, const runtime::LandingDamageObservation &landingFeedback)
{
	if (!services_->service_runtimeProfiler().enabled() || index >= NativeBotObservation::kClientSlotCount)
	{
		return;
	}
	PendingTraversalDiagnostic &pending = actors_[index].movementTraversalPending;
	if (!pending.active)
	{
		return;
	}
	const FakeClientHandle &handle = services_->service_managedBotHandles()[index];
	if (handle.actor.actorGeneration != pending.actorGeneration)
	{
		pending.active = false;
		return;
	}
	if (gpMetaUtilFuncs == nullptr || gpMetaUtilFuncs->pfnLogConsole == nullptr ||
		services_->service_pluginId() == nullptr)
	{
		return;
	}
	const float healthDelta = pending.startHealth - after.health;
	const std::uint32_t corridorAdvance =
		decision.corridorIndex >= pending.corridorIndex ? decision.corridorIndex - pending.corridorIndex : 0U;
	profiledLogConsole(services_->service_runtimeProfiler(), services_->service_pluginId(),
		"profile traversalResult bot_id=%u actor_generation=%u map=%u round=%u command_frame=%u "
		"observed_frame=%u nav_update=%u goal_generation=%u reselection_reason=%s "
		"selection_strategy=%s from_area=%u target_area=%u observed_area=%u "
		"path_link=(%u->%u how=%u dir=%u) action=%d buttons=%u IN_JUMP=%d "
		"source_floor_z=%.1f target_floor_z=%.1f drop_height=%.1f "
		"horizontal_gap=%.1f safe_drop_available=%d max_safe_drop=%.1f "
		"input=(%.2f %.2f %.2f) speed=%.1f "
		"start=(%.1f %.1f %.1f) after_move=(%.1f %.1f %.1f) "
		"start_grounded=%d grounded_after=%d velocity_z_before=%.1f velocity_z_after=%.1f "
		"health_before=%.1f health_after=%.1f health_delta=%.1f "
		"landing_damage_available=%d landing_damage=%.1f "
		"corridor_before=%u corridor_after=%u corridor_advance=%u",
		static_cast<unsigned int>(handle.actor.slot), static_cast<unsigned int>(handle.actor.actorGeneration),
		pending.mapGeneration, pending.roundGeneration, pending.commandFrame,
		static_cast<unsigned int>(services_->service_adapterFrameCount()), pending.navUpdateSequence,
		pending.goalGeneration, goalSelectionReasonName(pending.goalSelectionReason),
		goalSelectionStrategyName(pending.goalSelectionStrategy), static_cast<unsigned int>(pending.currentArea),
		static_cast<unsigned int>(pending.targetArea), static_cast<unsigned int>(decision.currentArea),
		static_cast<unsigned int>(pending.currentArea), static_cast<unsigned int>(pending.targetArea),
		static_cast<unsigned int>(pending.linkHow), static_cast<unsigned int>(pending.linkDirection),
		static_cast<int>(pending.action), static_cast<unsigned int>(pending.buttons),
		(pending.buttons & static_cast<std::uint16_t>(IN_JUMP)) != 0U ? 1 : 0, pending.sourceFloorZ,
		pending.targetFloorZ, pending.dropHeight, pending.horizontalGap, pending.safeDropHeightAvailable ? 1 : 0,
		pending.maximumSafeDropHeight, pending.intentDirection.x, pending.intentDirection.y, pending.intentDirection.z,
		pending.intentSpeed, pending.startPosition.x, pending.startPosition.y, pending.startPosition.z, after.origin.x,
		after.origin.y, after.origin.z, pending.startGrounded ? 1 : 0, after.grounded ? 1 : 0, pending.startVelocityZ,
		after.velocity.z, pending.startHealth, after.health, healthDelta, landingFeedback.hasLandingDamage ? 1 : 0,
		landingFeedback.landingDamage, static_cast<unsigned int>(pending.corridorIndex),
		static_cast<unsigned int>(decision.corridorIndex), corridorAdvance);
	pending.active = false;
}

void BotNavigationRuntime::logMovementReversalDiagnostic(std::size_t index, const runtime::MovementPhysicsState &before,
	const runtime::NavRoamDecision &decision, const nav::LocomotionIntent &intent)
{
	if (!services_->service_runtimeProfiler().enabled() || index >= NativeBotObservation::kClientSlotCount)
	{
		return;
	}
	MovementReversalDiagnosticState &state = actors_[index].movementReversalDiagnostics;
	const runtime::PhysicsVector currentDirection = {intent.direction.x, intent.direction.y, intent.direction.z};
	const bool reversed = state.valid && runtime::isMovementDirectionReversal(state.direction, currentDirection);
	if (reversed)
	{
		if (state.reversalCount != (std::numeric_limits<std::uint32_t>::max)())
		{
			++state.reversalCount;
		}
		if (gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr &&
			services_->service_pluginId() != nullptr)
		{
			profiledLogConsole(services_->service_runtimeProfiler(), services_->service_pluginId(),
				"profile movementReversal bot_id=%u frame=%u reversal_count=%u "
				"previous_feet=(%.1f %.1f %.1f) current_feet=(%.1f %.1f %.1f) "
				"previous_local_target=(%.1f %.1f %.1f) "
				"current_local_target=(%.1f %.1f %.1f) "
				"previous_direction=(%.3f %.3f %.3f) current_direction=(%.3f %.3f %.3f) "
				"previous_segment=(%u->%u) current_segment=(%u->%u) "
				"previous_path_sequence=%u path_sequence=%u corridor_index=%u "
				"link=(%u->%u how=%u dir=%u) recompute_reason=%d traversal=%d "
				"grounded=%d velocity=(%.1f %.1f %.1f)",
				static_cast<unsigned int>(services_->service_managedBotHandles()[index].actor.slot),
				static_cast<unsigned int>(services_->service_adapterFrameCount()),
				static_cast<unsigned int>(state.reversalCount), state.feetPosition.x, state.feetPosition.y,
				state.feetPosition.z, before.origin.x, before.origin.y, before.origin.z, state.localTarget.x,
				state.localTarget.y, state.localTarget.z, intent.targetPosition.x, intent.targetPosition.y,
				intent.targetPosition.z, state.direction.x, state.direction.y, state.direction.z, currentDirection.x,
				currentDirection.y, currentDirection.z, static_cast<unsigned int>(state.currentArea),
				static_cast<unsigned int>(state.targetArea), static_cast<unsigned int>(decision.currentArea),
				static_cast<unsigned int>(intent.targetArea), static_cast<unsigned int>(state.pathSequence),
				static_cast<unsigned int>(decision.pathSequence), static_cast<unsigned int>(decision.corridorIndex),
				static_cast<unsigned int>(decision.linkFromArea), static_cast<unsigned int>(decision.linkToArea),
				static_cast<unsigned int>(decision.linkHow), static_cast<unsigned int>(decision.linkDirection),
				static_cast<int>(decision.recomputeReason), static_cast<int>(intent.traversal), before.grounded ? 1 : 0,
				before.velocity.x, before.velocity.y, before.velocity.z);
		}
	}
	else if (state.valid)
	{
		state.reversalCount = 0U;
	}
	state.valid = true;
	state.feetPosition = before.origin;
	state.direction = currentDirection;
	state.localTarget = intent.targetPosition;
	state.currentArea = decision.currentArea;
	state.targetArea = intent.targetArea;
	state.pathSequence = decision.pathSequence;
}

void BotNavigationRuntime::onActorRemoved(std::size_t index)
{
	actors_[index].managedNavigationFailures = {};
	actors_[index].managedNavigationSteps = {};
	actors_[index].managedBotMovement.reset();
	actors_[index].movementFallLandingTrackers.reset();
	actors_[index].movementDiagnosticSamples = 0U;
	actors_[index].movementDiagnosticAttempts = false;
	actors_[index].movementDiagnosticUnavailable = false;
	actors_[index].movementResumeFrames = 0U;
	actors_[index].movementLastDeadFrames = 0U;
	actors_[index].movementSettledDeadFrames = 0U;
	actors_[index].movementWarmupFrames = 0U;
	actors_[index].movementPhysicsSamples = {};
	actors_[index].movementReadyLogged = false;
	actors_[index].movementGoalDiagnosticSeconds = (std::numeric_limits<std::uint32_t>::max)();
	actors_[index].movementTraversalDiagnosticSeconds = (std::numeric_limits<std::uint32_t>::max)();
	actors_[index].movementTraversalPending = {};
	actors_[index].movementDispatchFrames = (std::numeric_limits<std::uint32_t>::max)();
	actors_[index].movementWasAirborne = false;
	actors_[index].movementReversalDiagnostics = {};
}

void BotNavigationRuntime::resetSharedExecutionState()
{
	reset_managedNavigationFailures();
	reset_managedNavigationSteps();
	managedBotRoamReservationBoard_.reset(
		services_->service_lifecycle().mapGeneration(), services_->service_lifecycle().roundGeneration());
	fill_movementDiagnosticSamples(0U);
	fill_movementDiagnosticAttempts(false);
	fill_movementDiagnosticUnavailable(false);
	fill_movementResumeFrames(0U);
	fill_movementLastDeadFrames(0U);
	fill_movementSettledDeadFrames(0U);
	fill_movementWarmupFrames(0U);
	fill_movementPhysicsSamples({});
	fill_movementReadyLogged(false);
	fill_movementGoalDiagnosticSeconds((std::numeric_limits<std::uint32_t>::max)());
	fill_movementTraversalDiagnosticSeconds((std::numeric_limits<std::uint32_t>::max)());
	fill_movementTraversalPending({});
	fill_movementDispatchFrames((std::numeric_limits<std::uint32_t>::max)());
	fill_movementWasAirborne(false);
	fill_movementReversalDiagnostics({});
	movementDiagnosticRound_ = 0U;
}

void BotNavigationRuntime::resetActorExecutionState(std::size_t index)
{
	actors_[index].managedBotMovement.reset();
	actors_[index].movementFallLandingTrackers.reset();
}

void BotNavigationRuntime::beginMovementUpdate(
	const compat::CvarSnapshot &configuration, const nav::NavSnapshot &navigation, bool movementUnavailable)
{
	if (movementUnavailable)
	{
		fill_movementGroundLookaheadRoutes(runtime::GroundLookaheadRoute{});
	}
	if (movementDiagnosticRound_ != services_->service_lifecycle().roundGeneration())
	{
		movementDiagnosticRound_ = services_->service_lifecycle().roundGeneration();
		fill_movementDiagnosticSamples(0U);
		fill_movementDiagnosticAttempts(false);
		fill_movementReadyLogged(false);
		services_->command_resetGateDiagnostics();
		fill_movementGoalDiagnosticSeconds((std::numeric_limits<std::uint32_t>::max)());
		fill_movementTraversalDiagnosticSeconds((std::numeric_limits<std::uint32_t>::max)());
		fill_movementTraversalPending({});
		fill_movementReversalDiagnostics({});
		fill_movementGroundLookaheadRoutes(runtime::GroundLookaheadRoute{});
	}
	if (movementUnavailable)
	{
		if (!movementDiagnosticGlobal_ && gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr &&
			services_->service_pluginId() != nullptr)
		{
			importantLogConsole(services_->service_runtimeProfiler(), services_->service_pluginId(),
				"movement unavailable frame=%u enable=%.1f stop=%.1f nav=%d",
				static_cast<unsigned int>(services_->service_adapterFrameCount()), configuration.botEnable,
				configuration.botStop, navigation.isValid() ? 1 : 0);
			movementDiagnosticGlobal_ = true;
		}
	}
}

void BotNavigationRuntime::observeFullUpdateMotion(std::size_t index, const FakeClientHandle &handle,
	const runtime::MovementPhysicsState &before, const compat::CvarSnapshot &configuration)
{
	actors_[index].managedBotMovement.setRuntimeMode(configuration.mode);
	if (!before.dead && !services_->roundFreezeActive() && (handle.entity->v.flags & FL_FROZEN) == 0)
	{
		nav::LocomotionObservation motion{};
		motion.position = {before.origin.x, before.origin.y, before.origin.z};
		motion.grounded = before.grounded;
		motion.onLadder = before.onLadder;
		motion.ducked = before.ducked;
		motion.timingAvailable = true;
		motion.observedAt = services_->service_globals()->time;
		actors_[index].managedBotMovement.observeMotion(handle.actor,
			{services_->service_lifecycle().mapGeneration(), services_->service_lifecycle().roundGeneration(),
				services_->service_adapterFrameCount()},
			motion);
	}
	else
		actors_[index].managedBotMovement.resetProgressMonitor();
}

MovementPreparation BotNavigationRuntime::prepareMovement(
	std::size_t index, FakeClientHandle &handle, const runtime::MovementPhysicsState &before, bool movementUnavailable)
{
	if (before.dead)
	{
		if (actors_[index].movementLastDeadFrames == 0U)
		{
			actors_[index].movementLastDeadFrames = services_->service_adapterFrameCount();
			actors_[index].movementResumeFrames = services_->service_adapterFrameCount() >
					(std::numeric_limits<std::uint32_t>::max)() - kRespawnSettleFrames
				? (std::numeric_limits<std::uint32_t>::max)()
				: services_->service_adapterFrameCount() + kRespawnSettleFrames;
			actors_[index].movementWarmupFrames = 0U;
			services_->behavior_resetBombApproach(index);
			actors_[index].managedBotMovement.reset();
			actors_[index].movementFallLandingTrackers.reset();
		}
		return {MovementPreparationKind::Neutral, MovementPreparationCompletion::None};
	}
	if (actors_[index].movementLastDeadFrames != 0U)
	{
		if (services_->service_adapterFrameCount() < actors_[index].movementResumeFrames)
		{
			return {MovementPreparationKind::Neutral, MovementPreparationCompletion::None};
		}
		actors_[index].movementLastDeadFrames = 0U;
		actors_[index].movementResumeFrames = 0U;
		actors_[index].movementSettledDeadFrames = 0U;
	}
	const bool movementEligible = runtime::movementReadiness(before) == runtime::SpawnReadiness::Ready;
	const bool initialSpawnPending = actors_[index].movementWarmupFrames < kMovementWarmupFrameCount &&
		runtime::spawnReadiness(before) == runtime::SpawnReadiness::NotReady;
	if (movementUnavailable || !movementEligible || initialSpawnPending)
	{
		return {MovementPreparationKind::Neutral,
			movementEligible ? MovementPreparationCompletion::ResetRoute
							 : MovementPreparationCompletion::ResetRouteAndWarmup};
	}
	if (handle.entity == nullptr || handle.entity->v.deadflag != DEAD_NO || handle.entity->v.health <= 0.0f)
	{
		if (!actors_[index].movementDiagnosticUnavailable)
		{
			logMovementDiagnostic(index, "entity_unavailable");
			actors_[index].movementDiagnosticUnavailable = true;
		}
		actors_[index].movementResumeFrames =
			services_->service_adapterFrameCount() > (std::numeric_limits<std::uint32_t>::max)() - kRespawnSettleFrames
			? (std::numeric_limits<std::uint32_t>::max)()
			: services_->service_adapterFrameCount() + kRespawnSettleFrames;
		actors_[index].movementLastDeadFrames = services_->service_adapterFrameCount();
		actors_[index].movementWarmupFrames = 0U;
		services_->behavior_resetBombApproach(index);
		actors_[index].managedBotMovement.reset();
		actors_[index].movementFallLandingTrackers.reset();
		return {MovementPreparationKind::Skip, MovementPreparationCompletion::None};
	}
	if (actors_[index].movementLastDeadFrames != 0U &&
		actors_[index].movementLastDeadFrames != actors_[index].movementSettledDeadFrames)
	{
		actors_[index].movementSettledDeadFrames = actors_[index].movementLastDeadFrames;
		actors_[index].movementResumeFrames =
			services_->service_adapterFrameCount() > (std::numeric_limits<std::uint32_t>::max)() - kRespawnSettleFrames
			? (std::numeric_limits<std::uint32_t>::max)()
			: services_->service_adapterFrameCount() + kRespawnSettleFrames;
	}
	if (actors_[index].movementResumeFrames != 0U &&
		services_->service_adapterFrameCount() < actors_[index].movementResumeFrames)
	{
		return {MovementPreparationKind::Skip, MovementPreparationCompletion::None};
	}
	if (actors_[index].movementDiagnosticUnavailable)
	{
		if (services_->service_adapterFrameCount() < actors_[index].movementResumeFrames)
		{
			return {MovementPreparationKind::Skip, MovementPreparationCompletion::None};
		}
		actors_[index].movementDiagnosticAttempts = false;
		actors_[index].movementDiagnosticUnavailable = false;
	}
	if (actors_[index].movementWarmupFrames < kMovementWarmupFrameCount)
	{
		return {MovementPreparationKind::Neutral, MovementPreparationCompletion::AdvanceWarmup};
	}
	handle.entity->v.flags |= (FL_CLIENT | FL_FAKECLIENT);

	return {MovementPreparationKind::Ready, MovementPreparationCompletion::None};
}

void BotNavigationRuntime::completePreparation(std::size_t index, MovementPreparationCompletion completion)
{
	if (completion == MovementPreparationCompletion::ResetRoute ||
		completion == MovementPreparationCompletion::ResetRouteAndWarmup)
	{
		actors_[index].managedBotMovement.reset();
		actors_[index].movementFallLandingTrackers.reset();
	}
	if (completion == MovementPreparationCompletion::ResetRouteAndWarmup)
	{
		actors_[index].movementWarmupFrames = 0U;
	}
	if (completion == MovementPreparationCompletion::AdvanceWarmup)
	{
		++actors_[index].movementWarmupFrames;
	}
}

NavigationUpdate BotNavigationRuntime::updateNavigation(std::size_t index, const FakeClientHandle &handle,
	const runtime::MovementPhysicsState &before, const nav::NavSnapshot &navigation, bool performanceDisablePathSearch_)
{
	NavigationUpdate output{};
	auto &observation = output.observation;
	observation.actor = handle.actor;
	observation.frame = {services_->service_lifecycle().mapGeneration(),
		services_->service_lifecycle().roundGeneration(), services_->service_adapterFrameCount()};
	float feetOffset = handle.entity->v.mins.z;
	if (!std::isfinite(feetOffset) || feetOffset >= 0.0f)
	{
		feetOffset = -kStandingHalfHumanHeight;
	}
	observation.locomotion.position = {
		handle.entity->v.origin.x, handle.entity->v.origin.y, handle.entity->v.origin.z + feetOffset};
	observation.locomotion.velocity = {
		handle.entity->v.velocity.x, handle.entity->v.velocity.y, handle.entity->v.velocity.z};
	observation.locomotion.standingClearance = kStandingClearance;
	observation.locomotion.crouchingClearance = kCrouchingClearance;
	observation.locomotion.grounded = before.grounded;
	observation.locomotion.ducked = before.ducked;
	observation.locomotion.onLadder = before.onLadder;
	observation.locomotion.timingAvailable = std::isfinite(services_->service_globals()->time);
	observation.locomotion.observedAt = services_->service_globals()->time;
	observation.locomotion.viewYawAvailable = std::isfinite(handle.entity->v.v_angle.y);
	observation.locomotion.viewYaw = handle.entity->v.v_angle.y;
	if (services_->service_compatibilitySurface().configuration().mode == compat::RuntimeMode::Compatibility)
	{
		actors_[index].managedBotMovement.setPathCostContext(managedPathCostContext(index));
	}
	if (services_->service_engineFunctions() != nullptr &&
		services_->service_engineFunctions()->pfnCVarGetFloat != nullptr)
	{
		const float serverGravity = services_->service_engineFunctions()->pfnCVarGetFloat("sv_gravity");
		const float entityGravity = handle.entity->v.gravity;
		const float downwardSpeed = (std::max)(0.0f, -before.velocity.z);
		const float safeSpeedSquared =
			nav::LocomotionConfig::kMaximumSafeFallSpeed * nav::LocomotionConfig::kMaximumSafeFallSpeed -
			downwardSpeed * downwardSpeed;
		if (std::isfinite(serverGravity) && serverGravity > 0.0f && std::isfinite(entityGravity) &&
			entityGravity > 0.0f && std::isfinite(safeSpeedSquared) && safeSpeedSquared > 0.0f)
		{
			observation.locomotion.maximumSafeDropHeight = safeSpeedSquared / (2.0f * serverGravity * entityGravity);
			observation.locomotion.safeDropHeightAvailable = true;
		}
	}
	observation.airborne = !before.grounded;
	const runtime::LandingDamageObservation &landingFeedback = output.landingFeedback =
		actors_[index].movementFallLandingTrackers.observe(handle.actor.actorGeneration, before);
	observation.landingConfirmed = landingFeedback.landingConfirmed;
	observation.hasLandingDamage = landingFeedback.hasLandingDamage;
	observation.landingDamage = landingFeedback.landingDamage;
	if (services_->service_runtimeProfiler().enabled() && landingFeedback.landingConfirmed &&
		gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr &&
		services_->service_pluginId() != nullptr)
	{
		profiledLogConsole(services_->service_runtimeProfiler(), services_->service_pluginId(),
			"profile landingFeedback bot_id=%u actor_generation=%u map=%u round=%u frame=%u "
			"landing_damage_available=%d landing_damage=%.1f",
			static_cast<unsigned int>(handle.actor.slot), static_cast<unsigned int>(handle.actor.actorGeneration),
			services_->service_lifecycle().mapGeneration(), services_->service_lifecycle().roundGeneration(),
			static_cast<unsigned int>(services_->service_adapterFrameCount()), landingFeedback.hasLandingDamage ? 1 : 0,
			landingFeedback.landingDamage);
	}
	observation.ladderContact = before.onLadder;
	observation.entryConfirmed = true;
	observation.exitConfirmed = before.grounded;
	observation.collectPathStats = services_->service_runtimeProfiler().enabled();
	nav::NavSearchStats objectiveSearchStats = {};
	RuntimeObjectiveSelectionStats objectiveSelectionStats = {};
	{
		RuntimeProfilerScope objectiveScope(
			services_->service_runtimeProfiler(), RuntimeProfilerStage::FullUpdateObjective);
		RuntimeProfilerScope candidateNavScope(
			services_->service_runtimeProfiler(), RuntimeProfilerStage::ObjectiveCandidateNavEvaluate);
		observation.hasObjectiveTarget =
			services_->behavior_buildManagedObjectiveTarget(index, &observation.objectiveTarget,
				services_->service_runtimeProfiler().enabled() ? &objectiveSearchStats : nullptr,
				services_->service_runtimeProfiler().enabled() ? &objectiveSelectionStats : nullptr);
	}
	const world::ActorKey objectiveActor = {handle.actor.slot, handle.actor.actorGeneration};
	const team::TeamObjectiveAssignment *objectiveAssignment =
		services_->behavior_findManagedTeamObjectiveAssignment(objectiveActor);
	if (observation.hasObjectiveTarget && objectiveAssignment != nullptr)
	{
		observation.objectiveTargetEntity = objectiveAssignment->targetEntity;
	}
	observation.team = static_cast<std::uint8_t>(services_->observation_resolvePlayerTeam(handle.entity));
	if (services_->behavior_managedBotStateMachines(index).state() == compat::CompatibilityStateId::InvestigateNoise)
		observation.objectiveRouteType = nav::NavRouteType::Safest;
	observation.movementSuppressed = services_->roundFreezeActive();
	const auto &objectiveAction = services_->behavior_managedBotObjectiveActions(index);
	observation.movementSuppressed = observation.movementSuppressed ||
		(objectiveAction.active && objectiveAction.kind == team::TeamObjectiveKind::PlantC4 &&
			objectiveAction.actor.slot == handle.actor.slot &&
			objectiveAction.actor.generation == handle.actor.actorGeneration &&
			objectiveAction.mapGeneration == services_->service_lifecycle().mapGeneration() &&
			objectiveAction.roundGeneration == services_->service_lifecycle().roundGeneration() &&
			objectiveAction.plantAttempt.active());
	// Navigation-only teammate occupancy follows CSBot's live player scan.
	// Enemy perception and objective knowledge retain their existing gates.
	observation.friendRayOriginAvailable = true;
	observation.friendRayOrigin = {before.origin.x, before.origin.y, before.origin.z};
	observation.nearbyFriends = services_->observation_liveFriendlyPositions(index, false);
	runtime::GroundLookaheadRoute &groundLookaheadRoute = actors_[index].movementGroundLookaheadRoutes;
	output.probeRoute = groundLookaheadRoute;
	if (observation.movementSuppressed || !before.grounded || before.onLadder ||
		!groundLookaheadRoute.matchesNextUpdate(handle.actor, observation.frame,
			services_->service_managedBotFullUpdateSequences()[index], services_->service_globals()->time) ||
		services_->service_engineFunctions() == nullptr ||
		services_->service_engineFunctions()->pfnTraceLine == nullptr)
	{
		groundLookaheadRoute.reset();
	}
	else
	{
		const runtime::GroundLookaheadRoute previousRoute = groundLookaheadRoute;
		nav::GroundLookaheadObservation &lookahead = observation.locomotion.groundLookahead;
		lookahead.targetArea = previousRoute.targetArea;
		lookahead.direction = previousRoute.direction;
		lookahead.running = previousRoute.running;

		const auto traceGroundAt = [&](float distance) -> nav::GroundProbeObservation
		{
			nav::GroundProbeObservation probe = {};
			const float x = observation.locomotion.position.x + previousRoute.direction.x * distance;
			const float y = observation.locomotion.position.y + previousRoute.direction.y * distance;
			const float start[3] = {x, y, handle.entity->v.origin.z + kStandingHalfHumanHeight};
			const float end[3] = {x, y, observation.locomotion.position.z - kGroundProbeDepth};
			if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(start[2]) || !std::isfinite(end[2]))
			{
				return probe;
			}

			TraceResult trace = {};
			RuntimeProfilerScope traceScope(services_->service_runtimeProfiler(), RuntimeProfilerStage::TraceLine);
			if (services_->service_runtimeProfiler().enabled())
			{
				services_->service_runtimeProfiler().recordTraceLine(1U, 0U, 0U);
			}
			constexpr int kGroundTraceIgnoringActors = 1;
			services_->service_engineFunctions()->pfnTraceLine(
				start, end, kGroundTraceIgnoringActors, handle.entity, &trace);
			probe.sampled = true;
			const float floorZ = trace.vecEndPos[2];
			const float normalZ = trace.vecPlaneNormal[2];
			if (!trace.fStartSolid && !trace.fAllSolid && std::isfinite(trace.flFraction) && trace.flFraction >= 0.0f &&
				trace.flFraction < 1.0f && std::isfinite(floorZ) && std::isfinite(normalZ) && normalZ >= -1.0f &&
				normalZ <= 1.0f && normalZ > 0.0f)
			{
				probe.hasGround = true;
				probe.floorZ = floorZ;
				probe.normalZ = normalZ;
			}
			return probe;
		};

		if (previousRoute.running)
		{
			lookahead.far80 = traceGroundAt(kFarGroundProbeDistance);
		}
		lookahead.near30 = traceGroundAt(kNearGroundProbeDistance);
		lookahead.gap10 = traceGroundAt(kGapGroundProbeDistance);
		// Near-ground fallback remains usable when the optional far probe misses
		// or finds a slope (CSBot b0889847 cs_bot_nav.cpp:229-238).
		lookahead.valid = lookahead.near30.sampled && lookahead.gap10.sampled;
	}
	services_->service_runtimeProfiler().recordObjectiveSelection(objectiveSelectionStats.bombSites,
		objectiveSelectionStats.funcBombTargetSites, objectiveSelectionStats.infoBombTargetSites,
		objectiveSelectionStats.candidateAreas, objectiveSelectionStats.uniqueCandidateAreas,
		objectiveSelectionStats.candidateQueries, objectiveSelectionStats.duplicateCandidateAreas,
		objectiveSelectionStats.selectedGoalArea, objectiveSelectionStats.registeredSites,
		objectiveSelectionStats.registeredFuncBombTargetSites, objectiveSelectionStats.registeredInfoBombTargetSites,
		objectiveSelectionStats.registeredSiteCountsAvailable, objectiveSelectionStats.evaluatedSites,
		objectiveSelectionStats.cacheHits, objectiveSelectionStats.selectedSiteIdentity);
	services_->service_runtimeProfiler().recordPathSearchResults(RuntimePathSearchCaller::ObjectiveCandidateEvaluation,
		services_->service_managedBotHandles()[index].actor.slot,
		services_->service_managedBotFullUpdateSequences()[index],
		services_->behavior_managedBotObjectiveTargets(index).generation, objectiveSearchStats.firstStartArea,
		objectiveSearchStats.firstGoalArea, objectiveSearchStats.firstRouteType, objectiveSearchStats.searchCalls != 0U,
		objectiveSearchStats.searchCalls, objectiveSearchStats.successCount, objectiveSearchStats.failureCount,
		objectiveSearchStats.expandedUniqueAreas, objectiveSearchStats.enqueueCount, objectiveSearchStats.reopenCount,
		objectiveSearchStats.staleQueueEntries, objectiveSearchStats.equalCostReplacements,
		objectiveSearchStats.totalUsec, objectiveSearchStats.maxUsec, objectiveSearchStats.firstSearchId,
		objectiveSearchStats.lastSearchId);
	actors_[index].movementWasAirborne = !before.grounded;

	auto &locomotionIntent = output.locomotionIntent;
	auto &roamDecision = output.roamDecision;
	auto &roamResult = output.roamResult;
	groundLookaheadRoute.reset();
	const auto &previousStep = actors_[index].managedNavigationSteps;
	const bool destroyedPath = previousStep.pathDestroyedAfterMovement && previousStep.actor == objectiveActor &&
		previousStep.frame.mapGeneration == observation.frame.mapGeneration &&
		previousStep.frame.roundGeneration == observation.frame.roundGeneration &&
		previousStep.frame.tick < observation.frame.tick &&
		previousStep.goalGeneration == services_->behavior_managedBotObjectiveTargets(index).generation &&
		(services_->behavior_managedBotStateMachines(index).state() == compat::CompatibilityStateId::MoveTo ||
			services_->behavior_managedBotStateMachines(index).state() ==
				compat::CompatibilityStateId::EscapeFromBomb) &&
		!services_->behavior_managedBotStateMachines(index).attackOverlayActive();
	const auto navigationState = services_->behavior_managedBotStateMachines(index).state();
	const bool stateHold =
		services_->service_compatibilitySurface().configuration().mode == compat::RuntimeMode::Compatibility &&
		(navigationState == compat::CompatibilityStateId::DefuseBomb ||
			navigationState == compat::CompatibilityStateId::PlantBomb ||
			navigationState == compat::CompatibilityStateId::Buy ||
			(navigationState == compat::CompatibilityStateId::Hide &&
				services_->behavior_scenarioDecisions(index).action == compat::ScenarioAction::Guard &&
				services_->behavior_scenarioDecisions(index).hasTarget &&
				services_->behavior_scenarioDecisions(index).target.inUseRange));
	if (stateHold)
	{
		roamResult = runtime::NavRoamResult::MovementSuppressed;
		roamDecision.stage = runtime::NavRoamStage::MovementSuppressed;
	}
	else if (performanceDisablePathSearch_)
	{
		roamDecision.failureReason = runtime::NavFailureReason::PathSearchFailed;
		roamDecision.stage = runtime::NavRoamStage::Failed;
	}
	else if (destroyedPath)
	{
		// The preceding update moved before DestroyPath. This update must
		// report the missing path to MoveTo instead of silently rebuilding it.
		roamDecision.failureReason = runtime::NavFailureReason::MovementNotProduced;
		roamDecision.stage = runtime::NavRoamStage::Failed;
	}
	else
	{
		actors_[index].managedBotMovement.setRandomSource(&services_->service_compatibilityRandomSource());
		actors_[index].managedBotMovement.setAreaVisitHistory(&managedBotAreaVisitHistory_);
		managedBotRoamReservationBoard_.reset(
			services_->service_lifecycle().mapGeneration(), services_->service_lifecycle().roundGeneration());
		actors_[index].managedBotMovement.setReservationBoard(&managedBotRoamReservationBoard_);
		{
			RuntimeProfilerScope navMovementScope(
				services_->service_runtimeProfiler(), RuntimeProfilerStage::NavMovement);
			roamResult =
				actors_[index].managedBotMovement.update(navigation, observation, &locomotionIntent, &roamDecision);
		}
		if (services_->behavior_noiseInvestigationWalking(index) &&
			services_->behavior_managedBotStateMachines(index).state() ==
				compat::CompatibilityStateId::InvestigateNoise &&
			locomotionIntent.posture == nav::LocomotionPosture::Standing && before.grounded && !before.onLadder)
			locomotionIntent.speed = (std::min)(locomotionIntent.speed, .4f * handle.entity->v.maxspeed);
		const float routeDirectionLength = std::hypot(locomotionIntent.direction.x, locomotionIntent.direction.y);
		if (roamResult == runtime::NavRoamResult::IntentReady && !observation.movementSuppressed && before.grounded &&
			!before.onLadder && locomotionIntent.targetArea != 0U && std::isfinite(locomotionIntent.direction.x) &&
			std::isfinite(locomotionIntent.direction.y) && std::isfinite(locomotionIntent.direction.z) &&
			std::isfinite(routeDirectionLength) &&
			routeDirectionLength >= ASTRABOT_BOT_NAVIGATION_RUNTIME_MINIMUM_UNIT_DIRECTION_LENGTH &&
			routeDirectionLength <= ASTRABOT_BOT_NAVIGATION_RUNTIME_MAXIMUM_UNIT_DIRECTION_LENGTH &&
			std::isfinite(locomotionIntent.speed) && locomotionIntent.speed > 0.0f)
		{
			groundLookaheadRoute.actor = handle.actor;
			groundLookaheadRoute.frame = observation.frame;
			groundLookaheadRoute.fullUpdateSequence = services_->service_managedBotFullUpdateSequences()[index];
			groundLookaheadRoute.createdAt = services_->service_globals()->time;
			groundLookaheadRoute.targetArea = locomotionIntent.targetArea;
			groundLookaheadRoute.direction = locomotionIntent.direction;
			groundLookaheadRoute.running = locomotionIntent.posture == nav::LocomotionPosture::Standing &&
				locomotionIntent.speed >= kMinimumRunningSpeed;
			groundLookaheadRoute.active = true;
		}
		services_->behavior_noteNavigationFailure(handle, roamResult, roamDecision);
	}

	return output;
}

void BotNavigationRuntime::recordNavigationUpdate(
	std::size_t index, const runtime::MovementPhysicsState &before, NavigationUpdate &movement)
{
	auto &roamDecision = movement.roamDecision;
	const auto &locomotionIntent = movement.locomotionIntent;
	const auto roamResult = movement.roamResult;
	const auto &observation = movement.observation;
	const auto &probeRoute = movement.probeRoute;
	services_->service_runtimeProfiler().recordPathSearchResults(navSearchCaller(roamDecision),
		services_->service_managedBotHandles()[index].actor.slot,
		services_->service_managedBotFullUpdateSequences()[index],
		services_->behavior_managedBotObjectiveTargets(index).generation, roamDecision.pathSearchStats.firstStartArea,
		roamDecision.pathSearchStats.firstGoalArea, roamDecision.pathSearchStats.firstRouteType,
		roamDecision.pathRequested, roamDecision.pathSearchStats.searchCalls, roamDecision.pathSearchStats.successCount,
		roamDecision.pathSearchStats.failureCount, roamDecision.pathSearchStats.expandedUniqueAreas,
		roamDecision.pathSearchStats.enqueueCount, roamDecision.pathSearchStats.reopenCount,
		roamDecision.pathSearchStats.staleQueueEntries, roamDecision.pathSearchStats.equalCostReplacements,
		roamDecision.pathSearchStats.totalUsec, roamDecision.pathSearchStats.maxUsec,
		roamDecision.pathSearchStats.firstSearchId, roamDecision.pathSearchStats.lastSearchId);
	logGoalAssignmentDiagnostic(
		index, before, roamDecision, locomotionIntent, roamResult, observation.locomotion.groundLookahead, probeRoute);
	if (roamDecision.recomputeReason != runtime::NavRecomputeReason::None)
	{
		services_->service_runtimeProfiler().recordPathRecompute();
	}
}

void BotNavigationRuntime::resetRoundDeathReceipts()
{
	reset_deathDangerRecorded();
}

void BotNavigationRuntime::resetMapDanger()
{
	dangerMemory_.reset(services_->service_lifecycle().mapGeneration());
}

void BotNavigationRuntime::resetAvailabilityDiagnostic()
{
	movementDiagnosticGlobal_ = false;
}

void BotNavigationRuntime::restartLoggingDiagnostics()
{
	// Only diagnostic state is reset; movement, lifecycle and route state survive.
	movementDiagnosticGlobal_ = false;
	fill_movementDiagnosticAttempts(false);
	fill_movementDiagnosticSamples(0U);
	fill_movementGoalDiagnosticSeconds((std::numeric_limits<std::uint32_t>::max)());
	fill_movementTraversalDiagnosticSeconds((std::numeric_limits<std::uint32_t>::max)());
}

void BotNavigationRuntime::resetCreatedActorRoute(std::size_t slotIndex)
{
	actors_[slotIndex].managedBotMovement.reset();
}

void BotNavigationRuntime::resetCreatedActorSampling(std::size_t slotIndex)
{
	actors_[slotIndex].movementDiagnosticSamples = 0U;
	actors_[slotIndex].movementDiagnosticAttempts = false;
	actors_[slotIndex].movementReadyLogged = false;
}

void BotNavigationRuntime::resetCreatedActorReadiness(std::size_t slotIndex)
{
	actors_[slotIndex].movementGoalDiagnosticSeconds = (std::numeric_limits<std::uint32_t>::max)();
	actors_[slotIndex].movementTraversalDiagnosticSeconds = (std::numeric_limits<std::uint32_t>::max)();
	actors_[slotIndex].movementTraversalPending = {};
	actors_[slotIndex].movementDiagnosticUnavailable = false;
	actors_[slotIndex].movementResumeFrames = 0U;
	actors_[slotIndex].movementLastDeadFrames = 0U;
	actors_[slotIndex].movementSettledDeadFrames = 0U;
	actors_[slotIndex].movementWarmupFrames = 0U;
}

void BotNavigationRuntime::observePublicDeath(std::size_t victim)
{
	if (services_->service_managedBotSlots()[victim] && !actors_[victim].deathDangerRecorded &&
		services_->service_globals() != nullptr && services_->service_managedBotHandles()[victim].entity != nullptr)
	{
		const auto originalHandle = services_->service_managedBotHandles()[victim];
		const int serial = originalHandle.entity->serialnumber;
		const auto map = services_->service_lifecycle().mapGeneration();
		const auto round = services_->service_lifecycle().roundGeneration();
		const auto frame = services_->service_adapterFrameCount();
		const auto deathGlobals = services_->service_globals();
		const float time = deathGlobals->time;
		const auto connection = services_->service_lifecycle().slotGeneration(static_cast<std::uint32_t>(victim + 1));
		const auto revision = services_->behavior_chatterRevision(victim);
		const auto current = [&]()
		{
			const auto &handle = services_->service_managedBotHandles()[victim];
			return services_->service_globals() == deathGlobals && services_->service_managedBotSlots()[victim] &&
				handle.entity == originalHandle.entity && handle.actor.slot == originalHandle.actor.slot &&
				handle.actor.actorGeneration == originalHandle.actor.actorGeneration && !originalHandle.entity->free &&
				originalHandle.entity->serialnumber == serial &&
				services_->service_lifecycle().mapGeneration() == map &&
				services_->service_lifecycle().roundGeneration() == round &&
				services_->service_adapterFrameCount() == frame && services_->service_globals()->time == time &&
				services_->service_lifecycle().slotGeneration(static_cast<std::uint32_t>(victim + 1)) == connection &&
				services_->behavior_chatterRevision(victim) == revision;
		};
		const auto navigation = services_->service_navPublisher().snapshot();
		const auto *document = navigation.document();
		const auto *entity = services_->service_managedBotHandles()[victim].entity;
		nav::NavAreaMatch area{};
		nav::NavQuery query(navigation);
		const nav::NavVector position{entity->v.origin.x, entity->v.origin.y, entity->v.origin.z};
		if (document != nullptr &&
			query.findContaining(position, kAreaVerticalTolerance, &area) == nav::NavQueryResult::Found)
		{
			const auto team = services_->observation_resolvePlayerTeam(entity);
			if (!current())
			{
				return;
			}
			(void)dangerMemory_.recordDeath(*document, map, static_cast<std::uint8_t>(team), area.area, position, time);
		}
		actors_[victim].deathDangerRecorded = true;
	}
}

void BotNavigationRuntime::observeResetHud(std::size_t index)
{
	actors_[index].deathDangerRecorded = false;
}

void BotNavigationRuntime::applyBehaviorRequest(const NavigationRequest &request)
{
	auto &actor = actors_[request.index];
	switch (request.kind)
	{
	case NavigationRequestKind::PublishStep:
		actor.managedNavigationSteps = request.step;
		break;
	case NavigationRequestKind::ClearStep:
		actor.managedNavigationSteps = {};
		break;
	case NavigationRequestKind::ClearFailure:
		actor.managedNavigationFailures = {};
		break;
	case NavigationRequestKind::ResetMovement:
		actor.managedBotMovement.reset();
		break;
	case NavigationRequestKind::DestroyPath:
		actor.managedBotMovement.destroyPath();
		break;
	case NavigationRequestKind::ResetProgress:
		actor.managedBotMovement.resetProgressMonitor();
		break;
	}
}

void BotNavigationRuntime::noteMovementDiagnosticSample(std::size_t index)
{
	++actors_[index].movementDiagnosticSamples;
}

} // namespace metamod
} // namespace astrabot
#undef ASTRABOT_BOT_NAVIGATION_RUNTIME_MINIMUM_UNIT_DIRECTION_LENGTH
#undef ASTRABOT_BOT_NAVIGATION_RUNTIME_MAXIMUM_UNIT_DIRECTION_LENGTH
