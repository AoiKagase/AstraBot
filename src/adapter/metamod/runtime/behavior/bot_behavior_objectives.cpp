#include "runtime/behavior/bot_behavior_runtime.hpp"
#include "runtime/shared/bot_runtime_support.hpp"
namespace astrabot
{
namespace metamod
{
namespace
{
constexpr int kTerroristTeam = 1;
constexpr int kCounterTerroristTeam = 2;
constexpr int kScoutWeaponId = 3;
constexpr int kSg550WeaponId = 13;
constexpr int kAwpWeaponId = 18;
constexpr int kG3sg1WeaponId = 24;
constexpr int kMaximumEntityIndex = 2048;
constexpr int kMaximumClientSlots = 32;
constexpr float kDefuseRange = 100.0f;
// Keep literal evaluation precision on x87; each macro is undefined below.
#define ASTRABOT_BOT_BEHAVIOR_OBJECTIVES_DEGREES_TO_RADIANS 0.017453292519943295f
constexpr float kNearbyGuardRadius = 75.0f;
constexpr float kGuardFacingCosine = 0.5f;
constexpr float kGuardArrivalRange = 20.0f;
constexpr float kNavContainingTolerance = 64.0f;
constexpr float kSecondsPerMinute = 60.0f;
constexpr float kRecentEnemySeconds = 3.0f;
constexpr float kFallbackTravelSpeed = 240.0f;
constexpr float kPlantCompletionBudgetSeconds = 5.0f;
constexpr unsigned kObjectiveLogIntervalFrames = 16U;
constexpr unsigned kEntityScanLogIntervalFrames = 64U;
constexpr float kPlantPitchDegrees = 90.0f;
constexpr unsigned kKnifeWeaponId = 29U;
constexpr float kObjectiveInteractionRange = 96.0f;

ActionKind bestOwnedWeapon(const edict_t &entity)
{
	const auto inventory = static_cast<std::uint32_t>(entity.v.weapons);
	for (const unsigned id : {3U, 5U, 7U, 8U, 12U, 13U, 14U, 15U, 18U, 19U, 20U, 21U, 22U, 23U, 24U, 27U, 28U, 30U})
		if ((inventory & (1U << id)) != 0U)
			return ActionKind::SelectPrimary;
	for (const unsigned id : {1U, 10U, 11U, 16U, 17U, 26U})
		if ((inventory & (1U << id)) != 0U)
			return ActionKind::SelectPistol;
	return ActionKind::None;
}
}
using namespace runtime_detail;

bool BotBehaviorRuntime::finishManagedBotNavigation(std::size_t index, const runtime::MovementPhysicsState &before,
	runtime::NavRoamResult result, const runtime::NavRoamDecision &decision, nav::LocomotionIntent *intent)
{
	if (services_->service_compatibilitySurface().configuration().mode != compat::RuntimeMode::Compatibility ||
		index >= services_->service_managedBotHandles().size() || services_->service_globals() == nullptr)
		return true;
	actors_[index].obstaclePathActive = decision.pathSequence != 0U && decision.goalPresent;
	actors_[index].obstaclePathGoal = decision.targetPosition;
	compat::StateBehaviorInput input{};
	auto context = makeManagedBotStateContext(index, before, &input);
	const auto &prepared = actors_[index].managedStateInputs;
	if (!(prepared.navigation.actor == context.actor) || prepared.navigation.frame.tick != context.frame.tick ||
		prepared.navigation.frame.mapGeneration != context.frame.mapGeneration ||
		prepared.navigation.frame.roundGeneration != context.frame.roundGeneration)
		return true;
	input = prepared;
	compat::NavigationStep step{};
	step.actor = context.actor;
	step.frame = context.frame;
	step.goalGeneration = actors_[index].managedBotObjectiveTargets.generation;
	step.pathSequence = decision.pathSequence;
	step.fullUpdateSequence = context.fullUpdateSequence;
	using Outcome = compat::NavigationStepOutcome;
	if (result == runtime::NavRoamResult::IntentReady || result == runtime::NavRoamResult::MovementSuppressed ||
		(result == runtime::NavRoamResult::ReplanRequired && decision.failureReason == runtime::NavFailureReason::None))
		step.outcome = Outcome::Progressing;
	else if (result == runtime::NavRoamResult::TargetReached)
		step.outcome = Outcome::EndOfPath;
	else if (result == runtime::NavRoamResult::NoRoute || result == runtime::NavRoamResult::ReplanRequired)
		step.outcome = Outcome::PathFailure;
	else
		step.outcome = Outcome::ObservationUnavailable;
	const auto currentState = actors_[index].managedBotStateMachines.state();
	if (currentState == compat::CompatibilityStateId::DefuseBomb ||
		currentState == compat::CompatibilityStateId::PlantBomb || currentState == compat::CompatibilityStateId::Buy)
		step.outcome = Outcome::NotEvaluated;
	step.pathDestroyedAfterMovement = step.outcome == Outcome::Progressing &&
		decision.recomputeReason == runtime::NavRecomputeReason::PathInvalidated;
	input.navigation = step;
	if (currentState == compat::CompatibilityStateId::EscapeFromBomb &&
		(step.outcome == Outcome::EndOfPath || step.outcome == Outcome::PathFailure) &&
		actors_[index].scenarioEscapeAreas != 0U)
	{
		if (services_->service_runtimeProfiler().enabled())
			profiledLogConsole(services_->service_runtimeProfiler(), services_->service_pluginId(),
				"profile bombEscapeRetry bot_id=%u frame=%u previous_area=%u result=%d reason=%d", context.actor.slot,
				context.frame.tick, actors_[index].scenarioEscapeAreas, static_cast<int>(result),
				static_cast<int>(decision.recomputeReason));
		actors_[index].scenarioEscapeAreas = 0U;
	}
	const auto &target = actors_[index].scenarioDecisions.target;
	const auto &bomb = services_->observation_objectiveKnowledge(index)[1U];
	input.defuseGoalKnown = services_->observation_resolvePlayerTeam(
								services_->service_managedBotHandles()[index].entity) == kCounterTerroristTeam &&
		actors_[index].managedBotStateMachines.task() == compat::CompatibilityTaskId::DefuseBomb &&
		bomb.entity.id != 0U &&
		(helpGoalCurrent(index) || (actors_[index].scenarioDecisions.hasTarget && target.entity.id == bomb.entity.id));
	if (input.defuseGoalKnown)
	{
		const auto *entity = services_->service_managedBotHandles()[index].entity;
		const float footOffset = entity->v.mins.z < 0.0f ? entity->v.mins.z : -kStandingHalfHumanHeight;
		const float x = bomb.position.x - before.origin.x, y = bomb.position.y - before.origin.y,
					z = bomb.position.z - before.origin.z - footOffset;
		input.withinDefuseRange = x * x + y * y + z * z < kDefuseRange * kDefuseRange;
	}
	compat::CompatibilityObservation observation{};
	const compat::ObservationTimingContext timing = {
		services_->command_managedBotCommandSequences(index), 0U, context.fullUpdateSequence};
	auto reload = combat::ReloadState::Unknown;
	if (services_->observation_observationAdapter().collectActor(services_->service_managedBotHandles()[index].entity,
			context.actor, context.frame, timing, &observation) == ObservationAdapterResult::Accepted &&
		observation.weapon.reloadState.isAvailable())
		reload = observation.weapon.reloadState.value;
	if (reload == combat::ReloadState::Unknown && currentState == compat::CompatibilityStateId::MoveTo &&
		input.defuseGoalKnown && input.withinDefuseRange && input.navigationOwned &&
		!actors_[index].managedBotStateMachines.attackOverlayActive() &&
		(step.outcome == Outcome::EndOfPath || step.outcome == Outcome::PathFailure))
	{
		// CSBot checks its active weapon before entering DefuseBomb from MoveTo.
		const auto receipt = reactionContext(index);
		const auto revision = actors_[index].executionRevision;
		const auto epoch = actors_[index].managedBotStateMachines.normalStateEpoch();
		if (!(receipt.actor == context.actor && receipt.frame == context.frame))
			return false;
		reload = services_->observation_activeWeaponReloadState(services_->service_managedBotHandles()[index].entity);
		if (!executionCurrent(index, receipt, revision) ||
			actors_[index].managedBotStateMachines.normalStateEpoch() != epoch ||
			actors_[index].managedBotObjectiveTargets.generation != step.goalGeneration ||
			services_->service_managedBotFullUpdateSequences()[index] != step.fullUpdateSequence)
			return false;
	}
	if (reload != combat::ReloadState::Unknown)
	{
		input.reloadKnown = true;
		input.reloadInferred = false;
		input.reloading = reload == combat::ReloadState::Reloading;
	}
	else
	{
		// Fall back to the bot's delivered reload requests when prediction is unavailable.
		input.reloadInferred = true;
		input.reloading = services_->observation_publicObservations().pendingReloadRequest(
			context.actor, context.frame, context.timestamp);
	}
	auto &actor = actors_[index];
	auto &machine = actor.managedBotStateMachines;
	const auto previous = machine.state();
	const bool pursuitPathEnded = previous == compat::CompatibilityStateId::MoveTo &&
		machine.task() == compat::CompatibilityTaskId::MoveToLastKnownEnemyPosition &&
		(step.outcome == Outcome::EndOfPath || step.outcome == Outcome::PathFailure);
	const auto pursuitTarget = pursuitPathEnded ? actor.managedTaskEntity : world::EntityKey{};
	const auto receipt = pursuitTarget.isValid() ? reactionContext(index) : compat::ReactionContext{};
	const auto revision = actor.executionRevision;
	const auto transition = machine.finishNavigation(context);
	const auto resultingEpoch = machine.normalStateEpoch();
	if (actors_[index].helpGoal.active && !helpGoalCurrent(index))
	{
		actors_[index].helpGoal = {};
		actors_[index].managedBotObjectiveTargets = {};
	}
	if (!machine.attackOverlayActive())
		services_->applyNavigationRequest({index, NavigationRequestKind::PublishStep, step});
	if (machine.state() != previous)
	{
		actors_[index].enemyReaction.onStateChange(reactionContext(index), previous, machine.state());
		if (previous == compat::CompatibilityStateId::InvestigateNoise)
		{
			actors_[index].noiseInvestigation.cancel();
			actors_[index].investigationWalking = false;
		}
		services_->applyNavigationRequest({index, NavigationRequestKind::DestroyPath});
		if (machine.state() == compat::CompatibilityStateId::Idle)
		{
			actors_[index].managedBotObjectiveTargets = {};
			actors_[index].scenarioDecisions = {};
			if (services_->service_runtimeProfiler().enabled())
				profiledLogConsole(services_->service_runtimeProfiler(), services_->service_pluginId(),
					"profile %s bot_id=%u frame=%u state_from=%s state_to=%s", navigationTransitionEvent(step.outcome),
					context.actor.slot, context.frame.tick, compat::toString(previous),
					compat::toString(machine.state()));
		}
	}
	if (machine.state() == compat::CompatibilityStateId::DefuseBomb && intent != nullptr)
		*intent = {};
	if (services_->service_runtimeProfiler().enabled())
		profiledLogConsole(services_->service_runtimeProfiler(), services_->service_pluginId(),
			"profile navigationStep bot_id=%u frame=%u update=%u goal_generation=%u path=%u "
			"outcome=%u destroyed=%d state_from=%s state_to=%s reload_known=%d reload_inferred=%d "
			"goal_position=(%.1f %.1f %.1f) failure_reason=%d current_area=%u goal_area=%u "
			"path_link=(%u->%u)",
			context.actor.slot, context.frame.tick, context.fullUpdateSequence, step.goalGeneration, step.pathSequence,
			static_cast<unsigned>(step.outcome), step.pathDestroyedAfterMovement ? 1 : 0, compat::toString(previous),
			compat::toString(machine.state()), input.reloadKnown ? 1 : 0, input.reloadInferred ? 1 : 0,
			decision.goalPosition.x, decision.goalPosition.y, decision.goalPosition.z,
			static_cast<int>(decision.failureReason), decision.currentArea, decision.goalArea, decision.linkFromArea,
			decision.linkToArea);
	if (pursuitTarget.isValid() && transition == compat::StateResult::Transitioned &&
		machine.state() == compat::CompatibilityStateId::Idle)
	{
		const auto current = [&] {
			return executionCurrent(index, receipt, revision) && machine.normalStateEpoch() == resultingEpoch;
		};
		if (current())
			reportLostEnemy(index, pursuitTarget, current);
	}
	return true;
}

void BotBehaviorRuntime::refreshManagedObjectiveSiteRegistry()
{
	const runtime::LifecycleGeneration mapGeneration = services_->service_lifecycle().mapGeneration();
	const runtime::LifecycleGeneration roundGeneration = services_->service_lifecycle().roundGeneration();
	if (managedObjectiveSiteRegistry_.initialized && managedObjectiveSiteRegistry_.mapGeneration == mapGeneration &&
		managedObjectiveSiteRegistry_.roundGeneration == roundGeneration)
	{
		return;
	}
	managedObjectiveSiteRegistry_ = {};
	managedObjectiveSiteRegistry_.initialized = true;
	managedObjectiveSiteRegistry_.mapGeneration = mapGeneration;
	managedObjectiveSiteRegistry_.roundGeneration = roundGeneration;
	if (services_->service_engineFunctions() == nullptr || services_->service_globals() == nullptr ||
		services_->service_engineFunctions()->pfnPEntityOfEntIndex == nullptr ||
		services_->service_engineFunctions()->pfnSzFromIndex == nullptr)
	{
		return;
	}
	const int maxEntities = (std::max)(0, (std::min)(services_->service_globals()->maxEntities, kMaximumEntityIndex));
	for (int entityIndex = 1; entityIndex <= maxEntities; ++entityIndex)
	{
		if (managedObjectiveSiteRegistry_.registeredCount >= ManagedObjectiveSiteRegistry::kMaximumSites)
		{
			break;
		}
		edict_t *entity = services_->service_engineFunctions()->pfnPEntityOfEntIndex(entityIndex);
		if (entity == nullptr || entity->free)
		{
			continue;
		}
		const char *classname = services_->service_engineFunctions()->pfnSzFromIndex(entity->v.classname);
		if (!isBombTargetClassname(classname))
		{
			continue;
		}
		if (equalsIgnoreCase(classname, "func_bomb_target"))
		{
			++managedObjectiveSiteRegistry_.registeredFuncBombTargetCount;
		}
		else if (equalsIgnoreCase(classname, "info_bomb_target"))
		{
			++managedObjectiveSiteRegistry_.registeredInfoBombTargetCount;
		}
		ManagedObjectiveSiteRegistry::Entry &entry =
			managedObjectiveSiteRegistry_.entries[managedObjectiveSiteRegistry_.registeredCount];
		entry.registered = true;
		entry.targetId = managedObjectiveSiteRegistry_.registeredCount + 1U;
		entry.entityIndex = entityIndex;
		entry.siteIdentity = entry.targetId;
		entry.zone = publicBombSite(*entity, entityIndex, ActionAdapter::isLegacyInfoBombTargetClassname(classname));
		++managedObjectiveSiteRegistry_.registeredCount;
	}
}

void BotBehaviorRuntime::updateBombGuardTarget(
	std::size_t index, const compat::ScenarioBehaviorInput &input, compat::ScenarioBehaviorDecision *decision)
{
	RuntimeProfilerScope guardScope(services_->service_runtimeProfiler(), RuntimeProfilerStage::BombGuardSelection);
	if (index >= NativeBotObservation::kClientSlotCount || decision == nullptr)
		return;
	if (!input.bombPlantedKnown || !input.bombPlanted || !decision->hasTarget ||
		decision->action != compat::ScenarioAction::Guard || !(decision->target.entity == input.bomb.entity))
	{
		actors_[index].bombGuardPlanners.reset();
		return;
	}
	const auto &handle = services_->service_managedBotHandles()[index];
	if (handle.entity == nullptr || services_->service_engineFunctions() == nullptr ||
		services_->service_globals() == nullptr)
		return;
	const nav::NavVector self = {
		handle.entity->v.origin.x, handle.entity->v.origin.y, handle.entity->v.origin.z + handle.entity->v.mins.z};
	std::vector<nav::NavVector> occupied;
	compat::PublicActorState weapon{};
	const bool publicStateKnown = services_->observation_publicObservations().read(input.actor, input.frame, &weapon);
	const bool blind = publicStateKnown && input.timestamp < weapon.blindUntil;
	if (!blind && services_->service_engineFunctions()->pfnPEntityOfEntIndex != nullptr)
	{
		for (int slot = 1; slot <= (std::min)(kMaximumClientSlots, services_->service_globals()->maxClients); ++slot)
		{
			edict_t *other = services_->service_engineFunctions()->pfnPEntityOfEntIndex(slot);
			if (other == nullptr || other == handle.entity || other->free || (other->v.flags & FL_CLIENT) == 0 ||
				other->v.deadflag != DEAD_NO || other->v.health <= 0.0f)
				continue;
			const float eye[] = {handle.entity->v.origin.x + handle.entity->v.view_ofs.x,
				handle.entity->v.origin.y + handle.entity->v.view_ofs.y,
				handle.entity->v.origin.z + handle.entity->v.view_ofs.z};
			const float point[] = {other->v.origin.x, other->v.origin.y, other->v.origin.z};
			const float dx = point[0] - eye[0], dy = point[1] - eye[1];
			const float length = std::hypot(dx, dy);
			const float yaw = handle.entity->v.v_angle.y * ASTRABOT_BOT_BEHAVIOR_OBJECTIVES_DEGREES_TO_RADIANS;
			if (length > kNearbyGuardRadius && (dx * std::cos(yaw) + dy * std::sin(yaw)) < kGuardFacingCosine * length)
				continue;
			if (services_->service_engineFunctions()->pfnTraceLine == nullptr)
				continue;
			TraceResult trace{};
			services_->service_engineFunctions()->pfnTraceLine(eye, point, 1, handle.entity, &trace);
			if (trace.flFraction != 1.0f && trace.pHit != other)
				continue;
			occupied.push_back({point[0], point[1], point[2] + other->v.mins.z});
		}
	}
	const bool sniper = publicStateKnown && weapon.weaponKnown &&
		(weapon.weaponId == kScoutWeaponId || weapon.weaponId == kSg550WeaponId || weapon.weaponId == kAwpWeaponId ||
			weapon.weaponId == kG3sg1WeaponId);
	nav::NavVector spot{};
	const auto navigation = services_->service_navPublisher().snapshot();
	const nav::NavVector bombPosition = {input.bomb.position.x, input.bomb.position.y, input.bomb.position.z};
	// The active guard target is checked before constructing the danger/team
	// path-cost context. Retained choices need neither a new route nor new costs.
	nav::NavPathCostContext cost{};
	if (actors_[index].bombGuardPlanners.needsSelection(
			navigation, input.frame, input.bomb.entity, bombPosition, self, occupied, input.timestamp))
		cost = services_->navigation_managedPathCostContext(index);
	if (!actors_[index].bombGuardPlanners.update(navigation, input.frame, input.bomb.entity,
			{input.bomb.position.x, input.bomb.position.y, input.bomb.position.z}, self, sniper, occupied,
			input.timestamp, services_->service_compatibilityRandomSource(), {input.actor.slot, input.actor.generation},
			{services_->command_managedBotCommandSequences(index), 0U,
				services_->service_managedBotFullUpdateSequences()[index]},
			&spot, cost))
	{
		// Unknown NAV/RNG does not authorize converging on the C4 as cover.
		spot = self;
	}
	decision->target.position = {spot.x, spot.y, spot.z};
	decision->target.frame = input.frame;
	const float dx = spot.x - self.x, dy = spot.y - self.y, dz = spot.z - self.z;
	decision->target.pathCost = std::sqrt(dx * dx + dy * dy + dz * dz);
	decision->target.inUseRange = decision->target.pathCost <= kGuardArrivalRange;
}

void BotBehaviorRuntime::refreshManagedTeamObjectiveAssignments(std::size_t contextIndex)
{
	const bool compatibility =
		services_->service_compatibilitySurface().configuration().mode == compat::RuntimeMode::Compatibility;
	const std::uint32_t mapGeneration = services_->service_lifecycle().mapGeneration();
	const std::uint32_t roundGeneration = services_->service_lifecycle().roundGeneration();
	const world::FrameIdentity frame = {mapGeneration, roundGeneration, services_->service_adapterFrameCount()};
	if (managedTeamObjectiveAssignmentsValid_ &&
		(!compatibility || managedTeamObjectiveAssignmentContext_ == contextIndex) &&
		managedTeamObjectiveAssignmentMapGeneration_ == mapGeneration &&
		managedTeamObjectiveAssignmentRoundGeneration_ == roundGeneration &&
		managedTeamObjectiveAssignmentFrame_ == services_->service_adapterFrameCount())
	{
		return;
	}

	managedTeamObjectiveAssignments_ = {};
	managedTeamObjectiveAssignmentsValid_ = false;
	if (contextIndex >= services_->service_managedBotHandles().size() ||
		services_->service_engineFunctions() == nullptr ||
		services_->service_engineFunctions()->pfnPEntityOfEntIndex == nullptr ||
		services_->service_engineFunctions()->pfnSzFromIndex == nullptr || services_->service_globals() == nullptr ||
		!frame.isValid())
	{
		return;
	}

	const FakeClientHandle &contextHandle = services_->service_managedBotHandles()[contextIndex];
	managedTeamObjectiveAssignmentContext_ = contextIndex;
	team::RoundObjectiveCoordinator &coordinator =
		compatibility ? actors_[contextIndex].compatibilityObjectiveCoordinators : managedTeamObjectiveCoordinator_;
	const world::ActorKey contextActor = {contextHandle.actor.slot, contextHandle.actor.actorGeneration};
	const compat::ObservationTimingContext timing = {
		services_->command_managedBotCommandSequences(contextIndex), 0U, 0U};
	team::TeamObjectiveInput input = {};
	input.mapGeneration = mapGeneration;
	input.roundGeneration = roundGeneration;
	input.frameSequence = services_->service_adapterFrameCount();
	const nav::NavSnapshot navigation = services_->service_navPublisher().snapshot();
	const nav::NavDocument *document = navigation.document();
	nav::NavQuery query(navigation);
	if (compatibility)
	{
		query.setPathCostContext(services_->navigation_managedPathCostContext(contextIndex));
	}

	for (std::size_t index = 0U;
		 index < services_->service_managedBotHandles().size() && input.actorCount < team::kMaximumTeamObjectiveActors;
		 ++index)
	{
		if (!services_->service_managedBotSlots()[index] ||
			services_->service_managedBotHandles()[index].entity == nullptr ||
			services_->service_managedBotHandles()[index].entity->free != 0 || (compatibility && index != contextIndex))
		{
			continue;
		}
		const FakeClientHandle &handle = services_->service_managedBotHandles()[index];
		compat::CompatibilityObservation observation = {};
		const compat::ObservationTimingContext actorTiming = {
			services_->command_managedBotCommandSequences(index), 0U, 0U};
		if (services_->observation_observationAdapter().collectActor(handle.entity,
				{handle.actor.slot, handle.actor.actorGeneration}, frame, actorTiming,
				&observation) != ObservationAdapterResult::Accepted)
		{
			continue;
		}

		team::TeamObjectiveActorObservation &actor = input.actors[input.actorCount];
		actor = {};
		actor.actor = {handle.actor.slot, handle.actor.actorGeneration};
		const int observedTeam = services_->observation_resolvePlayerTeam(handle.entity);
		const bool teamConfirmed = services_->service_joinControllers()[index].teamConfirmed() &&
			observedTeam >= kTerroristTeam && observedTeam <= kCounterTerroristTeam;
		actor.team = !teamConfirmed			 ? objectives::TeamRole::Unknown
			: observedTeam == kTerroristTeam ? objectives::TeamRole::Terrorist
											 : objectives::TeamRole::CounterTerrorist;
		actor.alive = observation.player.deadflag.isAvailable() && observation.player.deadflag.value == DEAD_NO;
		actor.carryingC4 = observation.objective.carryingC4.isAvailable() && observation.objective.carryingC4.value;
		actor.positionAvailable = observation.player.origin.isAvailable();
		if (actor.positionAvailable)
		{
			actor.position = observation.player.origin.value;
			if (navigation.isValid() && document != nullptr)
			{
				nav::NavAreaMatch area = {};
				const nav::NavVector position = {actor.position.x, actor.position.y, actor.position.z};
				if (query.findContaining(position, kNavContainingTolerance, &area) != nav::NavQueryResult::Found &&
					query.findNearest(position, kMaximumObjectiveDistance, &area) != nav::NavQueryResult::Found)
				{
					area.area = 0U;
				}
				actor.currentArea = area.area;
			}
		}
		++input.actorCount;
	}

	const int maximumEntities =
		(std::max)(0, (std::min)(services_->service_globals()->maxEntities, kMaximumEntityIndex));
	compat::DroppedC4Observation droppedObservation = {};
	compat::ObjectiveObservation plantedObservation = {};
	for (int entityIndex = 1; entityIndex <= maximumEntities; ++entityIndex)
	{
		edict_t *entity = services_->service_engineFunctions()->pfnPEntityOfEntIndex(entityIndex);
		if (entity == nullptr || entity->free != 0)
		{
			continue;
		}
		const char *classname = services_->service_engineFunctions()->pfnSzFromIndex(entity->v.classname);
		const char *model = services_->service_engineFunctions()->pfnSzFromIndex(entity->v.model);
		if (!input.droppedC4.available)
		{
			compat::DroppedC4Observation candidate = {};
			if (services_->observation_observationAdapter().collectDroppedC4(entity, classname, model, contextActor,
					frame, timing, &candidate) == ObservationAdapterResult::Accepted &&
				candidate.isCurrent(frame))
			{
				droppedObservation = candidate;
				input.droppedC4.available = true;
				input.droppedC4.entity = candidate.entity;
				world::WorldVector knownPosition{};
				if (!services_->observation_observeObjective(contextIndex, entity, false, &knownPosition))
				{
					input.droppedC4 = {};
					continue;
				}
				input.droppedC4.position = knownPosition;
			}
		}
		if (!input.plantedC4.available &&
			services_->observation_observationAdapter().collectPlantedBomb(entity, classname, model,
				services_->service_globals()->time, contextActor, frame, timing,
				&plantedObservation) == ObservationAdapterResult::Accepted &&
			plantedObservation.bombPosition.isCurrent(frame) &&
			services_->service_engineFunctions()->pfnIndexOfEdict != nullptr && entity->serialnumber != 0)
		{
			const int plantedIndex = services_->service_engineFunctions()->pfnIndexOfEdict(entity);
			if (plantedIndex > 0)
			{
				input.plantedC4.available = true;
				input.plantedC4.entity = {
					static_cast<std::uint32_t>(plantedIndex), static_cast<std::uint32_t>(entity->serialnumber)};
				world::WorldVector knownPosition{};
				if (!services_->observation_observeObjective(contextIndex, entity, true, &knownPosition))
				{
					input.plantedC4 = {};
					continue;
				}
				input.plantedC4.position = knownPosition;
			}
		}
	}

	if (navigation.isValid() && document != nullptr)
	{
		for (team::TeamBombTargetObservation *target : {&input.droppedC4, &input.plantedC4})
		{
			if (!target->available)
			{
				continue;
			}
			nav::NavAreaMatch area = {};
			if (query.findNearest({target->position.x, target->position.y, target->position.z},
					kMaximumObjectiveDistance, &area) == nav::NavQueryResult::Found)
			{
				target->area = area.area;
			}
		}
	}

	if (managedTeamObjectivePathFailurePending_)
	{
		coordinator.reportPathFailure(managedTeamObjectivePathFailureActor_, managedTeamObjectivePathFailureGeneration_,
			managedTeamObjectivePathFailureStartArea_, managedTeamObjectivePathFailureFrame_);
		managedTeamObjectivePathFailurePending_ = false;
		managedTeamObjectivePathFailureActor_ = {};
		managedTeamObjectivePathFailureGeneration_ = 0U;
		managedTeamObjectivePathFailureStartArea_ = 0U;
		managedTeamObjectivePathFailureFrame_ = 0U;
	}
	const bool externalDefuserActive = services_->observation_externalDefuserEntityIndex() != 0U &&
		services_->observation_externalDefuserMapGeneration() == mapGeneration &&
		services_->observation_externalDefuserRoundGeneration() == roundGeneration &&
		std::isfinite(services_->service_globals()->time) &&
		std::isfinite(services_->observation_externalDefuserLeaseUntil()) &&
		services_->service_globals()->time <= services_->observation_externalDefuserLeaseUntil();
	input.externalDefuserActive = externalDefuserActive;
	(void)coordinator.assign(input, navigation, &managedTeamObjectiveAssignments_);
	if (services_->service_runtimeProfiler().enabled() && gpMetaUtilFuncs != nullptr &&
		gpMetaUtilFuncs->pfnLogConsole != nullptr && services_->service_pluginId() != nullptr)
	{
		for (std::size_t index = 0U; index < managedTeamObjectiveAssignments_.count; ++index)
		{
			const team::TeamObjectiveAssignment &assignment = managedTeamObjectiveAssignments_.assignments[index];
			profiledLogConsole(services_->service_runtimeProfiler(), services_->service_pluginId(),
				"team_objective map=%u round=%u frame=%u actor=%u:%u kind=%u target=%u:%u area=%u "
				"generation=%u cost=%.2f cost_fallback=%u dropped=%u planted=%u",
				static_cast<unsigned int>(mapGeneration), static_cast<unsigned int>(roundGeneration),
				static_cast<unsigned int>(services_->service_adapterFrameCount()),
				static_cast<unsigned int>(assignment.actor.slot),
				static_cast<unsigned int>(assignment.actor.generation), static_cast<unsigned int>(assignment.kind),
				static_cast<unsigned int>(assignment.targetEntity.id),
				static_cast<unsigned int>(assignment.targetEntity.generation),
				static_cast<unsigned int>(assignment.targetArea), static_cast<unsigned int>(assignment.generation),
				assignment.routeCostAvailable ? assignment.routeCost : 0.0f,
				assignment.routeCostGeometricFallback ? 1U : 0U, input.droppedC4.available ? 1U : 0U,
				input.plantedC4.available ? 1U : 0U);
		}
	}
	managedTeamObjectiveAssignmentMapGeneration_ = mapGeneration;
	managedTeamObjectiveAssignmentRoundGeneration_ = roundGeneration;
	managedTeamObjectiveAssignmentFrame_ = services_->service_adapterFrameCount();
	managedTeamObjectiveAssignmentsValid_ = true;
}

const team::TeamObjectiveAssignment *BotBehaviorRuntime::findManagedTeamObjectiveAssignment(
	const world::ActorKey &actor) const
{
	if (!managedTeamObjectiveAssignmentsValid_)
	{
		return nullptr;
	}
	for (std::size_t index = 0U; index < managedTeamObjectiveAssignments_.count; ++index)
	{
		const team::TeamObjectiveAssignment &assignment = managedTeamObjectiveAssignments_.assignments[index];
		if (assignment.actor == actor)
		{
			return &assignment;
		}
	}
	return nullptr;
}

float BotBehaviorRuntime::managedRoundRemainingSeconds() const
{
	const float minutes = readOptionalCvarFloat(services_->service_engineFunctions(), "mp_roundtime");
	if (services_->service_globals() == nullptr || !std::isfinite(services_->service_globals()->time) ||
		!std::isfinite(minutes) || minutes <= 0.0f ||
		services_->service_roundFreezeGeneration() != services_->service_lifecycle().roundGeneration())
	{
		return std::numeric_limits<float>::quiet_NaN();
	}
	return (std::max)(0.0f,
		minutes * kSecondsPerMinute -
			(std::max)(0.0f, services_->service_globals()->time - services_->service_roundFreezeUntil()));
}

bool BotBehaviorRuntime::managedActorAtBombSite(std::size_t index) const
{
	if (index >= services_->service_managedBotHandles().size() ||
		services_->service_managedBotHandles()[index].entity == nullptr ||
		services_->service_engineFunctions() == nullptr ||
		services_->service_engineFunctions()->pfnPEntityOfEntIndex == nullptr ||
		services_->service_engineFunctions()->pfnSzFromIndex == nullptr)
	{
		return false;
	}
	const edict_t *actor = services_->service_managedBotHandles()[index].entity;
	const PlantTargetBounds actorBounds = {actor->v.absmin.x, actor->v.absmin.y, actor->v.absmin.z, actor->v.absmax.x,
		actor->v.absmax.y, actor->v.absmax.z};
	for (std::size_t siteIndex = 0U; siteIndex < managedObjectiveSiteRegistry_.registeredCount; ++siteIndex)
	{
		const auto &entry = managedObjectiveSiteRegistry_.entries[siteIndex];
		edict_t *site = services_->service_engineFunctions()->pfnPEntityOfEntIndex(entry.entityIndex);
		if (site == nullptr || site->free)
			continue;
		const char *classname = services_->service_engineFunctions()->pfnSzFromIndex(site->v.classname);
		if (ActionAdapter::isLegacyInfoBombTargetClassname(classname))
		{
			const auto center = entityObjectiveCenter(site);
			if (ActionAdapter::withinLegacyBombTargetRadius(
					{center.x, center.y, center.z}, {actor->v.origin.x, actor->v.origin.y, actor->v.origin.z}))
				return true;
		}
		else if (isBombTargetClassname(classname))
		{
			const PlantTargetBounds siteBounds = {site->v.absmin.x, site->v.absmin.y, site->v.absmin.z,
				site->v.absmax.x, site->v.absmax.y, site->v.absmax.z};
			if (siteBounds.maximumX > siteBounds.minimumX && siteBounds.maximumY > siteBounds.minimumY &&
				ActionAdapter::overlapsPlantTarget(siteBounds, actorBounds))
				return true;
		}
	}
	return false;
}

bool BotBehaviorRuntime::buildManagedObjectiveTarget(std::size_t index, nav::NavVector *target,
	nav::NavSearchStats *searchStats, RuntimeObjectiveSelectionStats *objectiveStats, nav::AreaId *goalArea)
{
	if (goalArea != nullptr)
		*goalArea = 0U;
	if (objectiveStats != nullptr)
	{
		*objectiveStats = {};
	}
	if (index >= services_->service_managedBotHandles().size() || target == nullptr ||
		services_->service_engineFunctions() == nullptr ||
		services_->service_engineFunctions()->pfnPEntityOfEntIndex == nullptr ||
		services_->service_engineFunctions()->pfnSzFromIndex == nullptr || services_->service_globals() == nullptr)
	{
		return false;
	}
	const FakeClientHandle &handle = services_->service_managedBotHandles()[index];
	if (handle.entity == nullptr)
	{
		return false;
	}
	if (services_->service_compatibilitySurface().configuration().mode == compat::RuntimeMode::Compatibility)
	{
		if (helpGoalCurrent(index))
		{
			*target = actors_[index].helpGoal.position;
			return true;
		}
		if (reactionPursuing(index) &&
			actors_[index].managedBotStateMachines.state() == compat::CompatibilityStateId::MoveTo)
		{
			const auto context = reactionContext(index);
			const auto point = actors_[index].enemyReaction.pursuitPosition(context);
			*target = {point.x, point.y, point.z};
			auto &cache = actors_[index].managedBotObjectiveTargets;
			cache.valid = true;
			cache.mapGeneration = context.frame.mapGeneration;
			cache.roundGeneration = context.frame.roundGeneration;
			cache.generation = actors_[index].enemyReaction.pursuitGeneration();
			cache.target = *target;
			cache.selectedEntityIndex = static_cast<int>(actors_[index].enemyReaction.pursuitSource(context).id);
			return true;
		}
		if (actors_[index].managedBotStateMachines.state() == compat::CompatibilityStateId::InvestigateNoise &&
			noiseInvestigationCurrent(index))
		{
			const auto &investigation = actors_[index].noiseInvestigation;
			const auto position = investigation.target();
			*target = {position.x, position.y, position.z};
			if (goalArea != nullptr)
				*goalArea = investigation.area();
			return true;
		}
		const auto &actor = actors_[index];
		const auto &machine = actor.managedBotStateMachines;
		if (machine.state() == compat::CompatibilityStateId::Follow && !machine.attackOverlayActive())
		{
			const auto &motion = actor.followMotion;
			const auto snapshot = services_->service_navPublisher().snapshot();
			const auto *document = snapshot.document();
			if (!document || motion.epoch != machine.normalStateEpoch() ||
				!(motion.leader == actor.scenarioPlanners.followLeader()) || motion.goalNavRevision != snapshot.revision() ||
				(motion.goalUpdate != services_->service_managedBotFullUpdateSequences()[index] &&
					!services_->navigation_hasActivePath(index)))
				return false;
			const auto *area = document->findArea(motion.goalArea);
			if (!area)
				return false;
			*target = nav::extentCenterOf(*area);
			if (goalArea)
				*goalArea = area->id;
			return true;
		}
		const auto &scenario = actors_[index].scenarioDecisions;
		if (scenario.hasTarget && !scenario.unavailable &&
			scenario.target.frame.mapGeneration == services_->service_lifecycle().mapGeneration() &&
			scenario.target.frame.roundGeneration == services_->service_lifecycle().roundGeneration() &&
			scenario.target.frame.tick <= services_->service_adapterFrameCount())
		{
			*target = {scenario.target.position.x, scenario.target.position.y, scenario.target.position.z};
			return true;
		}
	}
	const world::FrameIdentity frame = {services_->service_lifecycle().mapGeneration(),
		services_->service_lifecycle().roundGeneration(), services_->service_adapterFrameCount()};
	const world::ActorKey actor = {handle.actor.slot, handle.actor.actorGeneration};
	const compat::ObservationTimingContext observationTiming = {
		services_->command_managedBotCommandSequences(index), 0U, 0U};
	compat::CompatibilityObservation compatObservation = {};
	if (services_->observation_observationAdapter().collectActor(
			handle.entity, actor, frame, observationTiming, &compatObservation) != ObservationAdapterResult::Accepted)
	{
		return false;
	}
	const int team = services_->observation_resolvePlayerTeam(handle.entity);
	ManagedObjectiveTargetCache &cache = actors_[index].managedBotObjectiveTargets;
	if (team != kTerroristTeam && team != kCounterTerroristTeam)
	{
		return false;
	}
	const bool carryingBomb =
		compatObservation.objective.carryingC4.isAvailable() && compatObservation.objective.carryingC4.value;
	refreshManagedTeamObjectiveAssignments(index);
	const world::ActorKey actorKey = {handle.actor.slot, handle.actor.actorGeneration};
	const team::TeamObjectiveAssignment *assignment = findManagedTeamObjectiveAssignment(actorKey);
	const team::TeamObjectiveKind assignmentKind =
		assignment == nullptr ? team::TeamObjectiveKind::None : assignment->kind;
	const std::uint32_t assignmentGeneration = assignment == nullptr ? 0U : assignment->generation;
	const bool needsDroppedC4 = assignmentKind == team::TeamObjectiveKind::RetrieveDroppedC4;
	const bool needsBombSite =
		team == kTerroristTeam && carryingBomb && assignmentKind == team::TeamObjectiveKind::PlantC4;
	const bool needsPlantedBomb = team == kCounterTerroristTeam && assignmentKind == team::TeamObjectiveKind::DefuseC4;
	objectives::BombApproachState &approach = actors_[index].managedBombApproaches;
	if (needsBombSite &&
		services_->service_compatibilitySurface().configuration().mode == compat::RuntimeMode::Compatibility)
	{
		approach.mapGeneration = frame.mapGeneration;
		approach.roundGeneration = frame.roundGeneration;
		approach.actorSlot = actor.slot;
		approach.actorGeneration = actor.generation;
		approach.initialized = true;
		approach.plantAt = roundPlantDecision_.plantAt;
		approach.finished = (roundPlantDecision_.known && services_->service_globals()->time >= approach.plantAt) ||
			(managedActorAtBombSite(index) &&
				services_->service_globals()->time - actors_[index].lastVisibleEnemyTimes > kRecentEnemySeconds);
		if (!approach.finished)
		{
			cache.valid = false;
			return false;
		}
	}
	if (needsBombSite)
	{
		refreshManagedObjectiveSiteRegistry();
		if (objectiveStats != nullptr)
		{
			objectiveStats->registeredSites = managedObjectiveSiteRegistry_.registeredCount;
			objectiveStats->registeredFuncBombTargetSites = managedObjectiveSiteRegistry_.registeredFuncBombTargetCount;
			objectiveStats->registeredInfoBombTargetSites = managedObjectiveSiteRegistry_.registeredInfoBombTargetCount;
			objectiveStats->registeredSiteCountsAvailable = true;
		}
	}
	if (!needsBombSite)
	{
		approach = {};
	}
	else if (approach.initialized && !approach.finished &&
		objectives::bombApproachPhase(&approach, services_->service_globals()->time, managedRoundRemainingSeconds(),
			cache.approachRouteSeconds, carryingBomb, false,
			managedActorAtBombSite(index)) == objectives::BombApproachPhase::Plant)
	{
		// One phase transition permits a fresh nearest route from the moved position.
		cache.valid = false;
		cache.generation = cache.generation == (std::numeric_limits<std::uint32_t>::max)() ? 1U : cache.generation + 1U;
	}
	const bool sameObjectiveState = cache.mapGeneration == services_->service_lifecycle().mapGeneration() &&
		cache.roundGeneration == services_->service_lifecycle().roundGeneration() &&
		cache.team == static_cast<std::uint8_t>(team) && cache.carryingBomb == carryingBomb &&
		cache.teamObjectiveKind == assignmentKind && cache.teamObjectiveAssignmentGeneration == assignmentGeneration;
	const bool diagnosticCacheRefresh =
		sameObjectiveState && needsBombSite && cache.valid && objectiveStats != nullptr && !cache.diagnosticsEmitted;
	const char *objectiveCacheState = diagnosticCacheRefresh ? "profile_refresh" : "miss";
	if (sameObjectiveState && needsBombSite && cache.valid && !diagnosticCacheRefresh)
	{
		edict_t *cachedSite = cache.selectedEntityIndex > 0
			? services_->service_engineFunctions()->pfnPEntityOfEntIndex(cache.selectedEntityIndex)
			: nullptr;
		const char *cachedClassname = cachedSite != nullptr && !cachedSite->free
			? services_->service_engineFunctions()->pfnSzFromIndex(cachedSite->v.classname)
			: nullptr;
		if (cachedClassname != nullptr && isBombTargetClassname(cachedClassname))
		{
			*target = cache.target;
			if (objectiveStats != nullptr)
			{
				++objectiveStats->cacheHits;
				objectiveStats->selectedSiteIdentity = cache.selectedSiteIdentity;
			}
			cache.lastCacheHit = true;
			return std::isfinite(target->x) && std::isfinite(target->y) && std::isfinite(target->z);
		}
		cache.valid = false;
		cache.diagnosticsEmitted = false;
		cache.lastCacheHit = false;
		cache.generation = cache.generation == (std::numeric_limits<std::uint32_t>::max)() ? 1U : cache.generation + 1U;
	}
	if (!sameObjectiveState)
	{
		cache.generation = cache.generation == (std::numeric_limits<std::uint32_t>::max)() ? 1U : cache.generation + 1U;
	}
	cache.valid = false;
	cache.selectedEntityIndex = 0;
	cache.selectedSiteIdentity = 0U;
	cache.mapGeneration = services_->service_lifecycle().mapGeneration();
	cache.roundGeneration = services_->service_lifecycle().roundGeneration();
	cache.team = static_cast<std::uint8_t>(team);
	cache.carryingBomb = carryingBomb;
	cache.teamObjectiveKind = assignmentKind;
	cache.teamObjectiveAssignmentGeneration = assignmentGeneration;
	cache.diagnosticsEmitted = false;
	cache.lastCacheHit = false;
	if (needsDroppedC4 && assignment != nullptr)
	{
		*target = {assignment->targetPosition.x, assignment->targetPosition.y, assignment->targetPosition.z};
		return std::isfinite(target->x) && std::isfinite(target->y) && std::isfinite(target->z);
	}
	if (assignmentKind == team::TeamObjectiveKind::GuardBombDefuser && assignment != nullptr)
	{
		*target = {assignment->targetPosition.x, assignment->targetPosition.y, assignment->targetPosition.z};
		return std::isfinite(target->x) && std::isfinite(target->y) && std::isfinite(target->z);
	}
	if (!needsBombSite && !needsPlantedBomb)
	{
		return false;
	}

	edict_t *plantedBomb = nullptr;
	compat::ObjectiveObservation plantedObservation = {};
	nav::NavVector selectedBombSite = {0.0f, 0.0f, 0.0f};
	float selectedPathCost = (std::numeric_limits<float>::max)();
	int selectedBombSiteEntityIndex = 0;
	std::uint32_t selectedSiteIdentity = 0U;
	struct ReachableBombSite
	{
		std::uint32_t identity;
		int entityIndex;
		float cost;
		nav::NavVector point;
	};
	std::vector<ReachableBombSite> reachableBombSites;
	const nav::NavSnapshot navigation = services_->service_navPublisher().snapshot();
	const nav::NavDocument *document = navigation.document();
	nav::NavAreaMatch currentMatch = {};
	bool haveCurrentArea = false;
	std::vector<ObjectiveCandidateKey> evaluatedCandidateKeys;
	if (navigation.isValid() && document != nullptr)
	{
		nav::NavQuery query(navigation);
		const nav::NavVector position = {
			handle.entity->v.origin.x, handle.entity->v.origin.y, handle.entity->v.origin.z};
		haveCurrentArea =
			query.findContaining(position, kNavContainingTolerance, &currentMatch) == nav::NavQueryResult::Found;
		if (!haveCurrentArea)
		{
			haveCurrentArea =
				query.findNearest(position, kMaximumObjectiveDistance, &currentMatch) == nav::NavQueryResult::Found;
		}
	}
	const int maxEntities = (std::max)(0, (std::min)(services_->service_globals()->maxEntities, kMaximumEntityIndex));
	std::uint32_t targetId = 0U;
	for (int entityIndex = 1; entityIndex <= maxEntities; ++entityIndex)
	{
		edict_t *entity = services_->service_engineFunctions()->pfnPEntityOfEntIndex(entityIndex);
		if (entity == nullptr || entity->free)
		{
			continue;
		}
		const char *classname = services_->service_engineFunctions()->pfnSzFromIndex(entity->v.classname);
		if (classname == nullptr)
		{
			continue;
		}
		if (needsBombSite && isBombTargetClassname(classname))
		{
			const std::uint32_t siteIdentity = ++targetId;
			if (objectiveStats != nullptr)
			{
				++objectiveStats->bombSites;
				++objectiveStats->evaluatedSites;
				if (equalsIgnoreCase(classname, "func_bomb_target"))
				{
					++objectiveStats->funcBombTargetSites;
				}
				else if (equalsIgnoreCase(classname, "info_bomb_target"))
				{
					++objectiveStats->infoBombTargetSites;
				}
			}
			const nav::NavVector candidate = entityObjectiveCenter(entity);
			nav::NavExtent siteExtent = {};
			const bool hasSiteExtent = objectiveSiteExtent(entity, classname, &siteExtent);
			float pathCost = (std::numeric_limits<float>::max)();
			nav::NavVector reachablePoint = candidate;
			nav::AreaId nearestNavArea = 0U;
			nav::AreaId candidateNavArea = 0U;
			bool validCandidate = false;
			std::uint32_t siteCandidateAreaSamples = 0U;
			std::uint32_t siteUniqueCandidateAreas = 0U;
			std::uint32_t siteReachableCandidateAreas = 0U;
			const char *rejectionReason = "no_candidate_nav_area";
			if (haveCurrentArea && document != nullptr && hasSiteExtent)
			{
				nav::NavQuery query(navigation);
				const bool compatibility = services_->service_compatibilitySurface().configuration().mode ==
					compat::RuntimeMode::Compatibility;
				if (compatibility)
				{
					query.setPathCostContext(services_->navigation_managedPathCostContext(index));
				}
				const nav::NavVector samplePoints[] = {candidate,
					{siteExtent.lo.x, (siteExtent.lo.y + siteExtent.hi.y) * 0.5f, candidate.z},
					{siteExtent.hi.x, (siteExtent.lo.y + siteExtent.hi.y) * 0.5f, candidate.z},
					{(siteExtent.lo.x + siteExtent.hi.x) * 0.5f, siteExtent.lo.y, candidate.z},
					{(siteExtent.lo.x + siteExtent.hi.x) * 0.5f, siteExtent.hi.y, candidate.z}};
				for (const nav::NavVector &samplePoint : samplePoints)
				{
					nav::NavAreaMatch siteMatch = {};
					if (query.findNearest(samplePoint, kMaximumObjectiveDistance, &siteMatch) !=
						nav::NavQueryResult::Found)
					{
						continue;
					}
					if (nearestNavArea == 0U)
					{
						nearestNavArea = siteMatch.area;
					}
					const nav::NavArea *siteArea = document->findArea(siteMatch.area);
					if (siteArea == nullptr || siteMatch.closestPoint.x < siteExtent.lo.x ||
						siteMatch.closestPoint.x > siteExtent.hi.x || siteMatch.closestPoint.y < siteExtent.lo.y ||
						siteMatch.closestPoint.y > siteExtent.hi.y)
					{
						if (!validCandidate)
						{
							rejectionReason = "candidate_outside_site_extent";
						}
						continue;
					}
					if (equalsIgnoreCase(classname, "info_bomb_target") &&
						!ActionAdapter::withinLegacyBombTargetRadius({candidate.x, candidate.y, candidate.z},
							{siteMatch.closestPoint.x, siteMatch.closestPoint.y, siteMatch.closestPoint.z}))
					{
						rejectionReason = "outside_legacy_info_radius";
						continue;
					}
					if (objectiveStats != nullptr)
					{
						++objectiveStats->candidateAreas;
					}
					++siteCandidateAreaSamples;
					const ObjectiveCandidateKey candidateKey = {entityIndex, siteIdentity, siteMatch.area};
					if (std::find(evaluatedCandidateKeys.begin(), evaluatedCandidateKeys.end(), candidateKey) !=
						evaluatedCandidateKeys.end())
					{
						if (objectiveStats != nullptr)
						{
							++objectiveStats->duplicateCandidateAreas;
						}
						if (!validCandidate)
						{
							rejectionReason = "duplicate_candidate_identity";
						}
						continue;
					}
					evaluatedCandidateKeys.push_back(candidateKey);
					++siteUniqueCandidateAreas;
					if (objectiveStats != nullptr)
					{
						++objectiveStats->uniqueCandidateAreas;
						++objectiveStats->candidateQueries;
					}
					const nav::NavArea &area = *siteArea;
					nav::NavCorridor corridor = {};
					nav::NavSearchStats sample = {};
					if (query.buildCorridor(currentMatch.area, area.id,
							compatibility ? nav::NavRouteType::Safest : nav::NavRouteType::Fastest, &corridor,
							searchStats == nullptr ? nullptr : &sample) == nav::NavQueryResult::Found &&
						std::isfinite(corridor.cost))
					{
						++siteReachableCandidateAreas;
						if (corridor.cost < pathCost)
						{
							pathCost = corridor.cost;
							reachablePoint = siteMatch.closestPoint;
							candidateNavArea = siteMatch.area;
						}
						validCandidate = true;
						rejectionReason = "none";
					}
					else
					{
						if (!validCandidate)
						{
							rejectionReason = "no_route";
						}
					}
					addNavSearchStats(searchStats, sample);
				}
			}
			else
			{
				pathCost = haveCurrentArea ? 1.0f : 0.0f;
				validCandidate = true;
				rejectionReason = "nav_unavailable_fallback";
			}
			if (objectiveStats != nullptr && !cache.diagnosticsEmitted)
			{
				logObjectiveTargetDiagnostic(services_->service_runtimeProfiler(), services_->service_pluginId(),
					targetId, entityIndex, classname, managedObjectiveSiteRegistry_.registeredCount,
					managedObjectiveSiteRegistry_.registeredFuncBombTargetCount,
					managedObjectiveSiteRegistry_.registeredInfoBombTargetCount, entity, candidate, nearestNavArea,
					candidateNavArea, siteCandidateAreaSamples, siteUniqueCandidateAreas, siteReachableCandidateAreas,
					pathCost == (std::numeric_limits<float>::max)() ? -1.0f : pathCost, objectiveCacheState,
					validCandidate, rejectionReason);
			}
			if (validCandidate)
			{
				reachableBombSites.push_back({siteIdentity, entityIndex, pathCost, reachablePoint});
			}
			if (validCandidate && objectives::isBetterBombSiteRoute(pathCost, selectedPathCost))
			{
				selectedPathCost = pathCost;
				selectedBombSite = reachablePoint;
				selectedBombSiteEntityIndex = entityIndex;
				selectedSiteIdentity = siteIdentity;
			}
		}
		const char *model = services_->service_engineFunctions()->pfnSzFromIndex(entity->v.model);
		if (needsPlantedBomb && plantedBomb == nullptr &&
			services_->observation_observationAdapter().collectPlantedBomb(entity, classname, model,
				services_->service_globals()->time, actor, frame, observationTiming,
				&plantedObservation) == ObservationAdapterResult::Accepted)
		{
			world::WorldVector knownPosition{};
			if (services_->observation_observeObjective(index, entity, true, &knownPosition))
			{
				plantedBomb = entity;
			}
		}
	}
	if (objectiveStats != nullptr)
	{
		cache.diagnosticsEmitted = true;
	}
	if (team == kTerroristTeam && carryingBomb && selectedPathCost != (std::numeric_limits<float>::max)())
	{
		const float speed = std::isfinite(handle.entity->v.maxspeed) && handle.entity->v.maxspeed > 0.0f
			? handle.entity->v.maxspeed
			: kFallbackTravelSpeed;
		const float nearestTravel = selectedPathCost / speed;
		const float roundRemaining = managedRoundRemainingSeconds();
		const compat::RandomActor randomActor = {handle.actor.slot, handle.actor.actorGeneration};
		const compat::RandomTimingContext randomTiming = {services_->command_managedBotCommandSequences(index),
			services_->service_adapterFrameCount(), services_->service_managedBotFullUpdateSequences()[index]};
		const float decisionStart =
			services_->service_roundFreezeGeneration() == services_->service_lifecycle().roundGeneration()
			? services_->service_roundFreezeStartTime()
			: services_->service_globals()->time;
		objectives::beginBombApproach(&approach, frame.mapGeneration, frame.roundGeneration, randomActor,
			services_->service_globals()->time, decisionStart, roundRemaining, nearestTravel,
			haveCurrentArea && document != nullptr ? &services_->service_compatibilityRandomSource() : nullptr,
			randomTiming);
		const auto phase = objectives::bombApproachPhase(&approach, services_->service_globals()->time, roundRemaining,
			nearestTravel, carryingBomb, false, managedActorAtBombSite(index));
		if (phase == objectives::BombApproachPhase::Approach)
		{
			if (approach.preferredSiteIdentity == 0U)
			{
				std::vector<float> costs;
				const float delayLeft = (std::max)(0.0f, approach.plantAt - services_->service_globals()->time);
				for (const auto &site : reachableBombSites)
				{
					costs.push_back(roundRemaining > delayLeft + site.cost / speed + kPlantCompletionBudgetSeconds
							? site.cost
							: -1.0f);
				}
				const auto choice = objectives::chooseBombApproachSite(costs.data(), costs.size(),
					&services_->service_compatibilityRandomSource(), randomActor, randomTiming);
				if (choice < reachableBombSites.size())
					approach.preferredSiteIdentity = reachableBombSites[choice].identity;
			}
			const auto preferred = std::find_if(reachableBombSites.begin(), reachableBombSites.end(),
				[&approach](const ReachableBombSite &site)
				{
					return site.identity == approach.preferredSiteIdentity;
				});
			if (preferred != reachableBombSites.end())
			{
				selectedBombSite = preferred->point;
				selectedBombSiteEntityIndex = preferred->entityIndex;
				selectedSiteIdentity = preferred->identity;
				selectedPathCost = preferred->cost;
			}
			else
			{
				approach.finished = true;
			}
		}
		cache.approachRouteSeconds = selectedPathCost / speed;
		*target = selectedBombSite;
		cache.target = *target;
		cache.selectedEntityIndex = selectedBombSiteEntityIndex;
		cache.selectedSiteIdentity = selectedSiteIdentity;
		cache.valid = true;
		if (objectiveStats != nullptr)
		{
			objectiveStats->selectedSiteIdentity = selectedSiteIdentity;
		}
	}
	else if (team == kCounterTerroristTeam && plantedBomb != nullptr)
	{
		world::WorldVector knownPosition{};
		if (!services_->observation_observeObjective(index, plantedBomb, true, &knownPosition))
		{
			return false;
		}
		*target = {knownPosition.x, knownPosition.y, knownPosition.z};
	}
	else
	{
		return false;
	}
	cache.diagnosticsEmitted = objectiveStats != nullptr;
	return std::isfinite(target->x) && std::isfinite(target->y) && std::isfinite(target->z);
}

void BotBehaviorRuntime::noteNavigationFailure(
	const FakeClientHandle &handle, runtime::NavRoamResult roamResult, const runtime::NavRoamDecision &roamDecision)
{
	const bool objectivePathFailed =
		(roamResult == runtime::NavRoamResult::NoRoute || roamResult == runtime::NavRoamResult::ReplanRequired) &&
		(roamDecision.failureReason == runtime::NavFailureReason::PathSearchFailed ||
			roamDecision.failureReason == runtime::NavFailureReason::UnsafeDrop ||
			roamDecision.failureReason == runtime::NavFailureReason::NavApplyRejected ||
			roamDecision.failureReason == runtime::NavFailureReason::MovementNotProduced ||
			(roamDecision.failureReason == runtime::NavFailureReason::RecoveryNoProgress &&
				roamDecision.recoveryCandidateFailed));
	if (objectivePathFailed && roamDecision.goalKind == runtime::NavGoalKind::Objective)
	{
		const world::ActorKey actor = {handle.actor.slot, handle.actor.actorGeneration};
		const team::TeamObjectiveAssignment *assignment = findManagedTeamObjectiveAssignment(actor);
		if (assignment != nullptr)
		{
			managedTeamObjectivePathFailurePending_ = true;
			managedTeamObjectivePathFailureActor_ = actor;
			managedTeamObjectivePathFailureGeneration_ = assignment->generation;
			// A portal-area switch alone does not clear a physical drop veto.
			// Zero keeps the coordinator's bounded retry lease across that jitter.
			managedTeamObjectivePathFailureStartArea_ =
				roamDecision.failureReason == runtime::NavFailureReason::UnsafeDrop
				? 0U
				: (roamDecision.currentArea != 0U ? roamDecision.currentArea : roamDecision.recoveryArea);
			managedTeamObjectivePathFailureFrame_ = services_->service_adapterFrameCount();
		}
	}
}

void BotBehaviorRuntime::beginRoundPlantDecision()
{
	reset_buyControllers();
	if (services_->service_globals() != nullptr &&
		services_->service_compatibilitySurface().configuration().mode == compat::RuntimeMode::Compatibility)
	{
		roundPlantDecision_.begin(services_->service_lifecycle().mapGeneration(),
			services_->service_lifecycle().roundGeneration(), services_->service_globals()->time,
			services_->service_compatibilityRandomSource());
	}
}

void BotBehaviorRuntime::beginMapPlantDecision()
{
	hostageRescueResult_ = {};
	moraleResultRound_ = 0;
	roundPlantDecision_ = {};
	if (services_->service_globals() != nullptr &&
		services_->service_compatibilitySurface().configuration().mode == compat::RuntimeMode::Compatibility)
	{
		roundPlantDecision_.begin(services_->service_lifecycle().mapGeneration(),
			services_->service_lifecycle().roundGeneration(), services_->service_globals()->time,
			services_->service_compatibilityRandomSource());
	}
}

void BotBehaviorRuntime::invalidateObjectiveAssignments()
{
	managedTeamObjectiveAssignmentsValid_ = false;
}

void BotBehaviorRuntime::resetBombApproach(std::size_t index)
{
	actors_[index].managedBombApproaches = {};
}

void BotBehaviorRuntime::observeC4SelectionDispatched(std::size_t index, const FakeClientHandle &handle)
{
	ManagedObjectiveActionState &state = actors_[index].managedBotObjectiveActions;
	if (state.active && state.actor.slot == handle.actor.slot &&
		state.actor.generation == handle.actor.actorGeneration && state.kind == team::TeamObjectiveKind::PlantC4)
	{
		state.c4SelectionCommandDispatched = true;
		state.plantAttempt.recordDispatched(services_->service_globals() != nullptr
				? services_->service_globals()->time
				: std::numeric_limits<float>::quiet_NaN());
	}
}

void BotBehaviorRuntime::observeC4AttackDispatched(
	std::size_t index, const FakeClientHandle &handle, std::uint16_t buttons)
{
	ManagedObjectiveActionState &actionState = actors_[index].managedBotObjectiveActions;
	if (actionState.active && actionState.actor.slot == handle.actor.slot &&
		actionState.actor.generation == handle.actor.actorGeneration &&
		actionState.kind == team::TeamObjectiveKind::PlantC4 &&
		actionState.mapGeneration == services_->service_lifecycle().mapGeneration() &&
		actionState.roundGeneration == services_->service_lifecycle().roundGeneration())
	{
		actionState.c4AttackInputDispatched = true;
		actionState.plantAttempt.recordDispatched(services_->service_globals() != nullptr
				? services_->service_globals()->time
				: std::numeric_limits<float>::quiet_NaN());
		if (services_->service_runtimeProfiler().enabled() &&
			(services_->service_adapterFrameCount() % kObjectiveLogIntervalFrames) == 0U &&
			gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr &&
			services_->service_pluginId() != nullptr)
		{
			profiledLogConsole(services_->service_runtimeProfiler(), services_->service_pluginId(),
				"profile plantInput actor=%u actor_generation=%u map=%u round=%u "
				"frame=%u stage=attack_dispatched buttons=%u",
				static_cast<unsigned int>(handle.actor.slot), static_cast<unsigned int>(handle.actor.actorGeneration),
				static_cast<unsigned int>(services_->service_lifecycle().mapGeneration()),
				static_cast<unsigned int>(services_->service_lifecycle().roundGeneration()),
				static_cast<unsigned int>(services_->service_adapterFrameCount()), static_cast<unsigned int>(buttons));
		}
	}
}

void BotBehaviorRuntime::observeNeutralC4AttackDispatched(std::size_t index, const FakeClientHandle &handle)
{
	ManagedObjectiveActionState &actionState = actors_[index].managedBotObjectiveActions;
	if (actionState.active && actionState.actor.slot == handle.actor.slot &&
		actionState.actor.generation == handle.actor.actorGeneration &&
		actionState.kind == team::TeamObjectiveKind::PlantC4)
	{
		actionState.c4AttackInputDispatched = true;
		actionState.plantAttempt.recordDispatched(services_->service_globals() != nullptr
				? services_->service_globals()->time
				: std::numeric_limits<float>::quiet_NaN());
	}
}

BotBehaviorRuntime::BehaviorPhaseResult BotBehaviorRuntime::prepareManagedAction(ManagedActionContext &ctx)
{
	const std::size_t index = ctx.index;
	const auto &before = ctx.before;
	const auto &movementAngles = ctx.movementAngles;
	auto &proposal = ctx.proposal;
	auto &compatObservation = ctx.observation;

	if (index < NativeBotObservation::kClientSlotCount &&
		services_->service_compatibilitySurface().configuration().mode == compat::RuntimeMode::Compatibility &&
		!actors_[index].managedBotStateMachines.attackOverlayActive() &&
		(actors_[index].managedBotStateMachines.state() == compat::CompatibilityStateId::PlantBomb ||
			actors_[index].managedBotStateMachines.state() == compat::CompatibilityStateId::DefuseBomb))
	{
		proposal.stopForWeapon = true;
		proposal.movementButtons = static_cast<std::uint16_t>(
			(proposal.movementButtons & ~(IN_JUMP | IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT)) | IN_DUCK);
	}

	if (index >= services_->service_managedBotHandles().size())
	{
		return BehaviorPhaseResult::Complete;
	}
	if (before.dead || services_->service_managedBotHandles()[index].entity == nullptr)
	{
		actors_[index].equipment.reset();
		actors_[index].managedBotObjectiveActions.active = false;
		actors_[index].weaponActions.reset();
		actors_[index].escapingWithKnife = false;
		return BehaviorPhaseResult::Complete;
	}
	ctx.handle = &services_->service_managedBotHandles()[index];
	const FakeClientHandle &handle = *ctx.handle;
	ctx.frame = {services_->service_lifecycle().mapGeneration(), services_->service_lifecycle().roundGeneration(),
		services_->service_adapterFrameCount()};
	ctx.actor = {handle.actor.slot, handle.actor.actorGeneration};
	ctx.observationTiming = {services_->command_managedBotCommandSequences(index), 0U, 0U};

	if (services_->observation_observationAdapter().collectActor(handle.entity, ctx.actor, ctx.frame,
			ctx.observationTiming, &compatObservation) != ObservationAdapterResult::Accepted)
	{
		actors_[index].managedBotObjectiveActions.active = false;
		(void)cancelHeldGrenade(index, movementAngles, proposal);
		return BehaviorPhaseResult::Complete;
	}
	if (services_->service_compatibilitySurface().configuration().mode == compat::RuntimeMode::Compatibility &&
		actors_[index].managedBotStateMachines.state() == compat::CompatibilityStateId::PlantBomb &&
		!actors_[index].managedBotStateMachines.attackOverlayActive() &&
		compatObservation.objective.carryingC4.isAvailable() && compatObservation.objective.carryingC4.value &&
		managedActorAtBombSite(index))
	{
		// Plant owns the interaction after arrival. A cleared corridor or an old
		// destination farther than96 must not suppress its C4 selection/attack.
		if (cancelHeldGrenade(index, movementAngles, proposal))
			return BehaviorPhaseResult::Complete;
		auto &action = actors_[index].managedBotObjectiveActions;
		action.active = true;
		action.actor = ctx.actor;
		action.kind = team::TeamObjectiveKind::PlantC4;
		action.mapGeneration = ctx.frame.mapGeneration;
		action.roundGeneration = ctx.frame.roundGeneration;
		const auto &weapon = services_->observation_managedBotCurrentWeapons(index);
		const bool current = weapon.available && weapon.actorGeneration == ctx.actor.generation &&
			weapon.mapGeneration == ctx.frame.mapGeneration && weapon.roundGeneration == ctx.frame.roundGeneration;
		proposal.kind = ActionAdapter::plantActionForWeaponObservation(
			current, current && weapon.active, weapon.weaponId, action.c4AttackInputDispatched);
		proposal.stopForWeapon = true;
		proposal.movementButtons = IN_DUCK;
		proposal.viewAngles = {kPlantPitchDegrees, movementAngles.yaw, movementAngles.roll};
		return BehaviorPhaseResult::Complete;
	}
	if (services_->service_compatibilitySurface().configuration().mode == compat::RuntimeMode::Compatibility &&
		actors_[index].scenarioDecisions.action == compat::ScenarioAction::Escape &&
		actors_[index].managedBotStateMachines.state() == compat::CompatibilityStateId::EscapeFromBomb)
	{
		if (cancelHeldGrenade(index, movementAngles, proposal))
			return BehaviorPhaseResult::Complete;
		actors_[index].escapingWithKnife = true;
		if (!compatObservation.weapon.activeWeaponId.isAvailable() ||
			compatObservation.weapon.activeWeaponId.value != kKnifeWeaponId)
			proposal.kind = ActionKind::SelectKnife;
		return BehaviorPhaseResult::Complete;
	}
	auto &actorState = actors_[index];
	auto &equipEpoch = actorState.combatRetreat.fallBackEquipEpoch;
	if (equipEpoch != 0)
	{
		const bool current = equipEpoch == actorState.managedBotStateMachines.normalStateEpoch();
		equipEpoch = 0;
		const auto &active = compatObservation.weapon.activeWeaponId;
		if (current && active.isAvailable() && (active.value == 4 || active.value == 9 || active.value == 25))
		{
			if (cancelHeldGrenade(index, movementAngles, proposal))
				return BehaviorPhaseResult::Complete;
			proposal.kind = bestOwnedWeapon(*handle.entity);
			if (proposal.kind == ActionKind::None &&
				(static_cast<std::uint32_t>(handle.entity->v.weapons) & (1U << kKnifeWeaponId)) != 0)
				proposal.kind = ActionKind::SelectKnife;
			if (proposal.kind != ActionKind::None)
				return BehaviorPhaseResult::Complete;
		}
	}
	if (actorState.escapingWithKnife)
	{
		actorState.escapingWithKnife = false;
		const auto choice = bestOwnedWeapon(*handle.entity);
		if (choice != ActionKind::None)
		{
			proposal.kind = choice;
			return BehaviorPhaseResult::Complete;
		}
	}

	return BehaviorPhaseResult::Continue;
}

BotBehaviorRuntime::BehaviorPhaseResult BotBehaviorRuntime::decideObjectiveAction(ManagedActionContext &ctx)
{
	bool objectiveActionDispatched = false;
	bool preserveObjectiveActionState = false;
	const std::size_t index = ctx.index;
	const auto &movementAngles = ctx.movementAngles;
	auto &proposal = ctx.proposal;
	auto &compatObservation = ctx.observation;
	const auto &handle = *ctx.handle;
	const auto &frame = ctx.frame;
	const auto &actor = ctx.actor;
	auto &snapshot = ctx.snapshot;
	auto &targetActor = ctx.targetActor;
	auto &targetPosition = ctx.targetPosition;
	refreshManagedTeamObjectiveAssignments(index);
	const team::TeamObjectiveAssignment *objectiveAssignment = findManagedTeamObjectiveAssignment(actor);

	const bool compatibility =
		services_->service_compatibilitySurface().configuration().mode == compat::RuntimeMode::Compatibility;
	const auto &machine = actors_[index].managedBotStateMachines;
	if (compatibility && machine.state() == compat::CompatibilityStateId::DefuseBomb &&
		!machine.attackOverlayActive())
	{
		// DefuseBomb owns its look and use commands even if the route and
		// team assignment have been cleared on arrival.
		proposal.stopForWeapon = true;
		proposal.movementButtons = static_cast<std::uint16_t>(
			(proposal.movementButtons & ~(IN_JUMP | IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT)) | IN_DUCK);
		actors_[index].managedBotObjectiveActions.active = false;
		const auto &bomb = services_->observation_objectiveKnowledge(index)[1U];
		const auto &scenario = actors_[index].scenarioDecisions;
		if (bomb.entity.id != 0U && scenario.action != compat::ScenarioAction::Guard &&
			(!objectiveAssignment || objectiveAssignment->kind != team::TeamObjectiveKind::GuardBombDefuser))
		{
			proposal.hasUseEntityLook = true;
			proposal.useEntityLookTarget = {bomb.position.x, bomb.position.y, bomb.position.z};
			if (machine.stateTimestamp() != services_->service_globals()->time)
				proposal.kind = ActionKind::Defuse;
		}
		return BehaviorPhaseResult::Complete;
	}

	if (!prepareManagedPerception(index, &snapshot, &targetActor, &targetPosition))
	{
		actors_[index].managedBotObjectiveActions.active = false;
		(void)cancelHeldGrenade(index, movementAngles, proposal);
		if ((services_->service_adapterFrameCount() % kObjectiveLogIntervalFrames) == 0U &&
			gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr &&
			services_->service_pluginId() != nullptr)
		{
			profiledLogConsole(services_->service_runtimeProfiler(), services_->service_pluginId(),
				"action sensor slot=%u frame=%u team=%d teamInfo=%u target=0",
				static_cast<unsigned int>(handle.actor.slot),
				static_cast<unsigned int>(services_->service_adapterFrameCount()),
				static_cast<int>(handle.entity->v.team),
				static_cast<unsigned int>(services_->observation_teamInfoTeams(index)));
		}
		return BehaviorPhaseResult::Complete;
	}
	if (targetActor.isValid())
		actors_[index].guardLook.reset();
	ctx.team = services_->observation_resolvePlayerTeam(handle.entity);
	const int team = ctx.team;
	const objectives::TeamRole teamRole = team == kTerroristTeam ? objectives::TeamRole::Terrorist
		: team == kCounterTerroristTeam							 ? objectives::TeamRole::CounterTerrorist
																 : objectives::TeamRole::Unknown;
	if (reactionPursuing(index) || helpGoalCurrent(index))
	{
		actors_[index].managedBotObjectiveActions.active = false;
		return cancelHeldGrenade(index, movementAngles, proposal) ? BehaviorPhaseResult::Complete
			: BehaviorPhaseResult::Continue;
	}
	const world::ActorObservation *actionTarget = nullptr;
	const bool targetCurrentlyVisible = targetActor.isValid() &&
		snapshot.findActor(targetActor, &actionTarget) == world::ContactLookupResult::Found && actionTarget && actionTarget->visible;
	if ((!targetActor.isValid() || !targetCurrentlyVisible) && compatibility)
	{
		if (cancelHeldGrenade(index, movementAngles, proposal))
			return BehaviorPhaseResult::Complete;
		const auto &scenario = actors_[index].scenarioDecisions;
		if (!scenario.unavailable)
		{
			if (scenario.action == compat::ScenarioAction::Stop ||
				(scenario.action == compat::ScenarioAction::Guard && scenario.hasTarget && scenario.target.inUseRange))
			{
				proposal.stopForWeapon = true;
				if (scenario.action == compat::ScenarioAction::Guard)
					updateGuardLook(ctx);
				return BehaviorPhaseResult::Complete;
			}
			if (scenario.action == compat::ScenarioAction::Use &&
				scenario.state != compat::CompatibilityStateId::DefuseBomb && scenario.hasTarget &&
				scenario.target.entity.isValid() && services_->service_engineFunctions() != nullptr &&
				services_->service_engineFunctions()->pfnPEntityOfEntIndex != nullptr &&
				services_->service_globals() != nullptr)
			{
				edict_t *useEntity = services_->service_engineFunctions()->pfnPEntityOfEntIndex(
					static_cast<int>(scenario.target.entity.id));
				if (useEntity != nullptr && !useEntity->free &&
					static_cast<std::uint32_t>(useEntity->serialnumber) + 1U == scenario.target.entity.generation)
				{
					const auto &self = handle.entity->v;
					if (scenario.state != compat::CompatibilityStateId::UseEntity)
					{
						const float dx = scenario.target.position.x - self.origin.x;
						const float dy = scenario.target.position.y - self.origin.y;
						proposal.kind = ActionKind::UseEntity;
						proposal.viewAngles.yaw = std::atan2(dy, dx) * 180.0f / 3.14159265358979323846f;
						return BehaviorPhaseResult::Complete;
					}
					const float now = services_->service_globals()->time;
					proposal.stopForWeapon = true;
					if (machine.state() != compat::CompatibilityStateId::UseEntity || machine.attackOverlayActive() ||
						machine.stateTimestamp() == now || ctx.before.dead || useEntity->v.health <= 0.0f)
						return BehaviorPhaseResult::Complete;
					const auto eye = self.origin + self.view_ofs;
					constexpr float halfHumanHeight = 36.0f;
					const auto point = useEntity->v.origin + Vector(0.0f, 0.0f, halfHumanHeight);
					const auto desired = ActionAdapter::useEntityView(point.x - eye.x, point.y - eye.y, point.z - eye.z);
					const runtime::ViewAngles current{self.v_angle.x, self.v_angle.y, self.v_angle.z};
					if (ActionAdapter::isFacingUseEntity(current, desired))
					{
						proposal.viewAngles = current;
						const bool frozen = (self.flags & FL_FROZEN) != 0 ||
							(services_->service_roundFreezeGeneration() == frame.roundGeneration &&
								now < services_->service_roundFreezeUntil());
						if (!frozen)
						{
							proposal.kind = ActionKind::UseEntity;
							proposal.hostageUseTarget = scenario.target.entity;
						}
					}
					else
					{
						proposal.viewAngles = current;
						proposal.hasUseEntityLook = true;
						proposal.useEntityLookTarget = {point.x, point.y, point.z};
					}
					return BehaviorPhaseResult::Complete;
				}
			}
		}
	}
	if (teamRole != objectives::TeamRole::Unknown && services_->service_engineFunctions() != nullptr &&
		services_->service_engineFunctions()->pfnPEntityOfEntIndex != nullptr &&
		services_->service_engineFunctions()->pfnSzFromIndex != nullptr && services_->service_globals() != nullptr)
	{
		const auto findEntityByClassname = [this](const char *wantedClassname) -> edict_t *
		{
			if (wantedClassname == nullptr || services_->service_engineFunctions() == nullptr ||
				services_->service_engineFunctions()->pfnPEntityOfEntIndex == nullptr ||
				services_->service_engineFunctions()->pfnSzFromIndex == nullptr ||
				services_->service_globals() == nullptr)
			{
				return nullptr;
			}
			const int maxEntities =
				(std::max)(0, (std::min)(services_->service_globals()->maxEntities, kMaximumEntityIndex));
			for (int entityIndex = 1; entityIndex <= maxEntities; ++entityIndex)
			{
				edict_t *entity = services_->service_engineFunctions()->pfnPEntityOfEntIndex(entityIndex);
				if (entity == nullptr || entity->free)
				{
					continue;
				}
				const char *classname = services_->service_engineFunctions()->pfnSzFromIndex(entity->v.classname);
				if (classname != nullptr && std::strcmp(classname, wantedClassname) == 0)
				{
					return entity;
				}
			}
			return nullptr;
		};

		edict_t *bombSite = findEntityByClassname("func_bomb_target");
		edict_t *plantedBomb = nullptr;
		compat::ObjectiveObservation plantedObservation = {};
		std::uint32_t grenadeEntityCount = 0U;
		std::uint32_t c4ModelEntityCount = 0U;
		int firstGrenadeEntityIndex = 0;
		int firstC4ModelEntityIndex = 0;
		const char *firstGrenadeClassname = "(none)";
		const char *firstGrenadeModel = "(none)";
		const char *firstC4ModelClassname = "(none)";
		const char *firstC4ModelName = "(none)";
		float firstGrenadeDamageTime = 0.0f;
		float firstC4ModelDamageTime = 0.0f;
		int firstGrenadeObservationResult = -1;
		int firstC4ModelObservationResult = -1;
		const int maxEntities =
			(std::max)(0, (std::min)(services_->service_globals()->maxEntities, kMaximumEntityIndex));
		for (int entityIndex = 1; entityIndex <= maxEntities; ++entityIndex)
		{
			edict_t *entity = services_->service_engineFunctions()->pfnPEntityOfEntIndex(entityIndex);
			if (entity == nullptr || entity->free)
			{
				continue;
			}
			const char *classname = services_->service_engineFunctions()->pfnSzFromIndex(entity->v.classname);
			const char *model = services_->service_engineFunctions()->pfnSzFromIndex(entity->v.model);
			const bool grenadeEntity = equalsIgnoreCase(classname, "grenade");
			const bool c4Model = model != nullptr && std::strstr(model, "w_c4.mdl") != nullptr;
			if (grenadeEntity)
			{
				++grenadeEntityCount;
			}
			if (c4Model)
			{
				++c4ModelEntityCount;
			}
			ObservationAdapterResult plantedResult = ObservationAdapterResult::InvalidEntity;
			if (plantedBomb == nullptr)
			{
				plantedResult = services_->observation_observationAdapter().collectPlantedBomb(entity, classname, model,
					services_->service_globals()->time, actor, frame, ctx.observationTiming, &plantedObservation);
			}
			if (grenadeEntity && firstGrenadeEntityIndex == 0)
			{
				firstGrenadeEntityIndex = entityIndex;
				firstGrenadeClassname = classname != nullptr ? classname : "(null)";
				firstGrenadeModel = model != nullptr ? model : "(null)";
				firstGrenadeDamageTime = entity->v.dmgtime;
				firstGrenadeObservationResult = static_cast<int>(plantedResult);
			}
			if (c4Model && firstC4ModelEntityIndex == 0)
			{
				firstC4ModelEntityIndex = entityIndex;
				firstC4ModelClassname = classname != nullptr ? classname : "(null)";
				firstC4ModelName = model != nullptr ? model : "(null)";
				firstC4ModelDamageTime = entity->v.dmgtime;
				firstC4ModelObservationResult = static_cast<int>(plantedResult);
			}
			if (plantedResult == ObservationAdapterResult::Accepted)
			{
				world::WorldVector knownPosition{};
				if (services_->observation_observeObjective(index, entity, true, &knownPosition))
				{
					plantedBomb = entity;
					break;
				}
			}
		}
		if (plantedBomb == nullptr && teamRole == objectives::TeamRole::CounterTerrorist &&
			services_->service_runtimeProfiler().enabled() &&
			(services_->service_adapterFrameCount() % kEntityScanLogIntervalFrames) == 0U &&
			gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr &&
			services_->service_pluginId() != nullptr)
		{
			profiledLogConsole(services_->service_runtimeProfiler(), services_->service_pluginId(),
				"profile plantedBombScan actor=%u actor_generation=%u map=%u round=%u "
				"frame=%u now=%.2f entities=%d grenade_count=%u "
				"first_grenade=(%d %s %s dmgtime=%.2f result=%d) "
				"c4_model_count=%u first_c4_model=(%d %s %s dmgtime=%.2f result=%d)",
				static_cast<unsigned int>(handle.actor.slot), static_cast<unsigned int>(handle.actor.actorGeneration),
				static_cast<unsigned int>(services_->service_lifecycle().mapGeneration()),
				static_cast<unsigned int>(services_->service_lifecycle().roundGeneration()),
				static_cast<unsigned int>(services_->service_adapterFrameCount()), services_->service_globals()->time,
				maxEntities, grenadeEntityCount, firstGrenadeEntityIndex, firstGrenadeClassname, firstGrenadeModel,
				firstGrenadeDamageTime, firstGrenadeObservationResult, c4ModelEntityCount, firstC4ModelEntityIndex,
				firstC4ModelClassname, firstC4ModelName, firstC4ModelDamageTime, firstC4ModelObservationResult);
		}
		const bool carryingBomb =
			compatObservation.objective.carryingC4.isAvailable() && compatObservation.objective.carryingC4.value;
		ManagedObjectiveActionState &plantState = actors_[index].managedBotObjectiveActions;
		if (plantedBomb != nullptr && plantState.active && plantState.actor == actor &&
			plantState.kind == team::TeamObjectiveKind::PlantC4)
		{
			(void)plantState.plantAttempt.evaluate(false, true,
				services_->service_globals() != nullptr ? services_->service_globals()->time
														: std::numeric_limits<float>::quiet_NaN());
			if (services_->service_runtimeProfiler().enabled() && gpMetaUtilFuncs != nullptr &&
				gpMetaUtilFuncs->pfnLogConsole != nullptr && services_->service_pluginId() != nullptr)
			{
				const int plantedEntityIndex = services_->service_engineFunctions() != nullptr &&
						services_->service_engineFunctions()->pfnIndexOfEdict != nullptr
					? services_->service_engineFunctions()->pfnIndexOfEdict(plantedBomb)
					: 0;
				profiledLogConsole(services_->service_runtimeProfiler(), services_->service_pluginId(),
					"profile plantConfirm actor=%u actor_generation=%u map=%u round=%u "
					"frame=%u assignment=%u site=%d site_id=%u planted_entity=%d "
					"source=public_planted_c4_observation",
					static_cast<unsigned int>(handle.actor.slot),
					static_cast<unsigned int>(handle.actor.actorGeneration),
					static_cast<unsigned int>(services_->service_lifecycle().mapGeneration()),
					static_cast<unsigned int>(services_->service_lifecycle().roundGeneration()),
					static_cast<unsigned int>(services_->service_adapterFrameCount()), plantState.assignmentGeneration,
					plantState.targetEntityIndex, plantState.targetSiteIdentity, plantedEntityIndex);
			}
			plantState = {};
		}
		if ((services_->service_adapterFrameCount() % kObjectiveLogIntervalFrames) == 0U &&
			gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr &&
			services_->service_pluginId() != nullptr)
		{
			profiledLogConsole(services_->service_runtimeProfiler(), services_->service_pluginId(),
				"objective sensor slot=%u frame=%u team=%d weapons=%d carryingC4=%d "
				"bombSite=%d plantedC4=%d",
				static_cast<unsigned int>(handle.actor.slot),
				static_cast<unsigned int>(services_->service_adapterFrameCount()), team, handle.entity->v.weapons,
				carryingBomb ? 1 : 0, bombSite != nullptr ? 1 : 0, plantedBomb != nullptr ? 1 : 0);
		}
		if (plantedBomb != nullptr || carryingBomb)
		{
			objectives::ScenarioObservation scenario = {};
			scenario.scenario.frame = snapshot.identity().frame;
			scenario.scenario.scenarioGeneration = 1U;
			scenario.scenario.kind = objectives::ScenarioKind::Bomb;
			scenario.scenario.team = teamRole;
			scenario.phase = objectives::RoundPhase::Live;
			scenario.buyAvailability = objectives::Availability::Unavailable;
			if (plantedBomb != nullptr)
			{
				scenario.eventCount = 1U;
				scenario.events[0].id = 1U;
				scenario.events[0].frame = snapshot.identity().frame;
				scenario.events[0].kind = objectives::ScenarioEventKind::BombPlanted;
				scenario.events[0].state = objectives::EventState::Observed;
				scenario.events[0].actor = {handle.actor.slot, handle.actor.actorGeneration};
				scenario.events[0].team = objectives::TeamRole::Terrorist;
			}
			else if (carryingBomb)
			{
				scenario.eventCount = 1U;
				scenario.events[0].id = 1U;
				scenario.events[0].frame = snapshot.identity().frame;
				scenario.events[0].kind = objectives::ScenarioEventKind::BombCarried;
				scenario.events[0].state = objectives::EventState::Observed;
				scenario.events[0].actor = {handle.actor.slot, handle.actor.actorGeneration};
				scenario.events[0].team = objectives::TeamRole::Terrorist;
			}
			objectives::ObjectiveProposal objective = {};
			const behavior::BehaviorState compatibilityBehaviorState =
				actors_[index].managedBotStateMachines.attackOverlayActive() ? behavior::BehaviorState::Engage
																			 : behavior::BehaviorState::Roam;
			const objectives::ObjectiveResult objectiveResult = actors_[index].managedBotObjectives.plan(
				snapshot, compatibilityBehaviorState, scenario, nullptr, &objective);
			const bool hasPlantAssignment =
				objectiveAssignment != nullptr && objectiveAssignment->kind == team::TeamObjectiveKind::PlantC4;
			nav::NavVector selectedPlantTarget{};
			const bool haveSelectedPlantTarget = hasPlantAssignment && carryingBomb &&
				teamRole == objectives::TeamRole::Terrorist &&
				buildManagedObjectiveTarget(index, &selectedPlantTarget, nullptr);
			objectives::ObjectiveKind objectiveKind = objectives::ObjectiveKind::None;
			if (objectiveResult == objectives::ObjectiveResult::Proposed &&
				objective.objective.kind != objectives::ObjectiveKind::Plant &&
				objective.objective.kind != objectives::ObjectiveKind::Defuse)
			{
				objectiveKind = objective.objective.kind;
			}
			if (objectives::PlantAttemptController::canBeginObjective(hasPlantAssignment, carryingBomb,
					teamRole == objectives::TeamRole::Terrorist,
					haveSelectedPlantTarget && actors_[index].managedBombApproaches.finished))
			{
				objectiveKind = objectives::ObjectiveKind::Plant;
			}
			else if (!compatibility &&
				objectiveAssignment != nullptr && objectiveAssignment->kind == team::TeamObjectiveKind::DefuseC4 &&
				plantedBomb != nullptr && teamRole == objectives::TeamRole::CounterTerrorist)
			{
				objectiveKind = objectives::ObjectiveKind::Defuse;
			}
			else if (objectiveAssignment == nullptr ||
				(objectiveAssignment->kind != team::TeamObjectiveKind::PlantC4 &&
					objectiveAssignment->kind != team::TeamObjectiveKind::DefuseC4))
			{
				objectiveKind = objectives::ObjectiveKind::None;
			}
			if (objectiveKind == objectives::ObjectiveKind::Plant || objectiveKind == objectives::ObjectiveKind::Defuse)
			{
				nav::NavVector objectivePosition = {0.0f, 0.0f, 0.0f};
				bool haveObjectivePosition = false;
				if (objectiveKind == objectives::ObjectiveKind::Defuse)
				{
					if (plantedBomb != nullptr)
					{
						objectivePosition = {plantedBomb->v.origin.x, plantedBomb->v.origin.y, plantedBomb->v.origin.z};
						haveObjectivePosition = true;
					}
				}
				else
				{
					objectivePosition = selectedPlantTarget;
					haveObjectivePosition = haveSelectedPlantTarget;
				}
				if (haveObjectivePosition)
				{
					const float dx = objectivePosition.x - handle.entity->v.origin.x;
					const float dy = objectivePosition.y - handle.entity->v.origin.y;
					const float dz = objectivePosition.z - handle.entity->v.origin.z;
					if (dx * dx + dy * dy + dz * dz <= kObjectiveInteractionRange * kObjectiveInteractionRange)
					{
						ManagedObjectiveActionState &actionState = actors_[index].managedBotObjectiveActions;
						const bool planting = objectiveAssignment->kind == team::TeamObjectiveKind::PlantC4;
						const int targetEntityIndex = planting
							? actors_[index].managedBotObjectiveTargets.selectedEntityIndex
							: static_cast<int>(objectiveAssignment->targetEntity.id);
						const std::uint32_t targetIdentity = planting
							? actors_[index].managedBotObjectiveTargets.selectedSiteIdentity
							: objectiveAssignment->targetEntity.generation;
						const bool sameAction = actionState.active && actionState.actor == actor &&
							actionState.kind == objectiveAssignment->kind &&
							actionState.mapGeneration == services_->service_lifecycle().mapGeneration() &&
							actionState.roundGeneration == services_->service_lifecycle().roundGeneration() &&
							actionState.assignmentGeneration == objectiveAssignment->generation &&
							actionState.targetEntityIndex == targetEntityIndex &&
							actionState.targetSiteIdentity == targetIdentity;
						if (!sameAction)
						{
							actionState = {};
							actionState.active = true;
							actionState.actor = actor;
							actionState.kind = objectiveAssignment->kind;
							actionState.mapGeneration = services_->service_lifecycle().mapGeneration();
							actionState.roundGeneration = services_->service_lifecycle().roundGeneration();
							actionState.assignmentGeneration = objectiveAssignment->generation;
							actionState.targetEntityIndex = targetEntityIndex;
							actionState.targetSiteIdentity = targetIdentity;
						}
						if (planting)
						{
							preserveObjectiveActionState = true;
							PlantTargetBounds siteBounds{};
							PlantTargetBounds actorBounds{};
							bool siteBoundsAvailable = false;
							bool legacyInfoTarget = false;
							PlantTargetPoint legacyInfoTargetCenter{};
							if (targetEntityIndex > 0 && services_->service_engineFunctions() != nullptr &&
								services_->service_engineFunctions()->pfnPEntityOfEntIndex != nullptr &&
								services_->service_engineFunctions()->pfnSzFromIndex != nullptr)
							{
								edict_t *site =
									services_->service_engineFunctions()->pfnPEntityOfEntIndex(targetEntityIndex);
								const char *siteClassname = site != nullptr && !site->free
									? services_->service_engineFunctions()->pfnSzFromIndex(site->v.classname)
									: nullptr;
								if (site != nullptr && !site->free && isBombTargetClassname(siteClassname))
								{
									legacyInfoTarget = equalsIgnoreCase(siteClassname, "info_bomb_target");
									if (legacyInfoTarget)
									{
										const nav::NavVector center = entityObjectiveCenter(site);
										legacyInfoTargetCenter = {center.x, center.y, center.z};
										const float radius = ActionAdapter::kLegacyBombTargetRadius;
										siteBounds = {center.x - radius, center.y - radius, center.z - radius,
											center.x + radius, center.y + radius, center.z + radius};
										siteBoundsAvailable = ActionAdapter::validPlantTargetBounds(siteBounds);
									}
									else
									{
										siteBounds = {site->v.absmin.x, site->v.absmin.y, site->v.absmin.z,
											site->v.absmax.x, site->v.absmax.y, site->v.absmax.z};
										siteBoundsAvailable = ActionAdapter::validPlantTargetBounds(siteBounds) &&
											siteBounds.maximumX > siteBounds.minimumX &&
											siteBounds.maximumY > siteBounds.minimumY;
									}
								}
							}
							actorBounds = {handle.entity->v.absmin.x, handle.entity->v.absmin.y,
								handle.entity->v.absmin.z, handle.entity->v.absmax.x, handle.entity->v.absmax.y,
								handle.entity->v.absmax.z};
							const PlantTargetPoint actorPosition = {
								handle.entity->v.origin.x, handle.entity->v.origin.y, handle.entity->v.origin.z};
							const bool actorInSite = siteBoundsAvailable &&
								(legacyInfoTarget ? ActionAdapter::withinLegacyBombTargetRadius(
														legacyInfoTargetCenter, actorPosition)
												  : ActionAdapter::overlapsPlantTarget(siteBounds, actorBounds));
							const float now = services_->service_globals() != nullptr
								? services_->service_globals()->time
								: std::numeric_limits<float>::quiet_NaN();
							const objectives::PlantAttemptResult attempt =
								actionState.plantAttempt.evaluate(actorInSite, plantedBomb != nullptr, now);
							if (attempt == objectives::PlantAttemptResult::TimedOut)
							{
								actionState.c4SelectionCommandDispatched = false;
								actionState.c4AttackInputDispatched = false;
								actors_[index].managedBotObjectiveTargets.valid = false;
								actors_[index].managedBotObjectiveTargets.diagnosticsEmitted = false;
								services_->applyNavigationRequest({index, NavigationRequestKind::ResetMovement});
							}
							const ManagedCurrentWeaponObservation &weapon =
								services_->observation_managedBotCurrentWeapons(index);
							const bool weaponObservationCurrent = weapon.available &&
								weapon.actorGeneration == handle.actor.actorGeneration &&
								weapon.mapGeneration == services_->service_lifecycle().mapGeneration() &&
								weapon.roundGeneration == services_->service_lifecycle().roundGeneration();
							const bool canContinuePlant = attempt == objectives::PlantAttemptResult::Ready ||
								attempt == objectives::PlantAttemptResult::Continuing;
							if (services_->service_runtimeProfiler().enabled() &&
								(services_->service_adapterFrameCount() % kObjectiveLogIntervalFrames) == 0U &&
								gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr &&
								services_->service_pluginId() != nullptr)
							{
								profiledLogConsole(services_->service_runtimeProfiler(), services_->service_pluginId(),
									"profile plantGate actor=%u actor_generation=%u map=%u "
									"round=%u "
									"frame=%u assignment=%u site=%d site_id=%u bounds_available=%d "
									"zone_mode=%s "
									"actor_in_site=%d gate=%d site_bounds=(%.1f %.1f %.1f %.1f "
									"%.1f %.1f) "
									"actor_bounds=(%.1f %.1f %.1f %.1f %.1f %.1f) "
									"nav_target=(%.1f %.1f %.1f) actor_origin=(%.1f %.1f %.1f) "
									"weapon_known=%d weapon_active=%d weapon_id=%u",
									static_cast<unsigned int>(handle.actor.slot),
									static_cast<unsigned int>(handle.actor.actorGeneration),
									static_cast<unsigned int>(services_->service_lifecycle().mapGeneration()),
									static_cast<unsigned int>(services_->service_lifecycle().roundGeneration()),
									static_cast<unsigned int>(services_->service_adapterFrameCount()),
									objectiveAssignment->generation, targetEntityIndex, targetIdentity,
									siteBoundsAvailable ? 1 : 0,
									legacyInfoTarget ? "legacy_info_radius" : "func_bounds", actorInSite ? 1 : 0,
									static_cast<int>(attempt), siteBounds.minimumX, siteBounds.minimumY,
									siteBounds.minimumZ, siteBounds.maximumX, siteBounds.maximumY, siteBounds.maximumZ,
									actorBounds.minimumX, actorBounds.minimumY, actorBounds.minimumZ,
									actorBounds.maximumX, actorBounds.maximumY, actorBounds.maximumZ,
									objectivePosition.x, objectivePosition.y, objectivePosition.z,
									handle.entity->v.origin.x, handle.entity->v.origin.y, handle.entity->v.origin.z,
									weaponObservationCurrent ? 1 : 0, weaponObservationCurrent && weapon.active ? 1 : 0,
									static_cast<unsigned int>(weapon.weaponId));
							}
							if (!canContinuePlant)
							{
								return BehaviorPhaseResult::Complete;
							}
							proposal.kind = ActionAdapter::plantActionForWeaponObservation(weaponObservationCurrent,
								weaponObservationCurrent && weapon.active, weapon.weaponId,
								actionState.c4AttackInputDispatched);
						}
						else
						{
							proposal.kind = ActionKind::Defuse;
						}
						proposal.viewAngles = {-kRadiansToDegrees * std::atan2(dz, std::sqrt(dx * dx + dy * dy)),
							kRadiansToDegrees * std::atan2(dy, dx), movementAngles.roll};
						objectiveActionDispatched = true;
						return BehaviorPhaseResult::Complete;
					}
				}
			}
		}
	}
	if (!objectiveActionDispatched)
	{
		if (!preserveObjectiveActionState)
		{
			actors_[index].managedBotObjectiveActions.active = false;
		}
	}

	return BehaviorPhaseResult::Continue;
}

} // namespace metamod
} // namespace astrabot
#undef ASTRABOT_BOT_BEHAVIOR_OBJECTIVES_DEGREES_TO_RADIANS
