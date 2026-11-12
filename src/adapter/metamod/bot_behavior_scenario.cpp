#include "bot_behavior_runtime.hpp"
#include "bot_runtime_support.hpp"
namespace astrabot
{
namespace metamod
{
using namespace runtime_detail;

compat::StateUpdateContext
BotBehaviorRuntime::makeManagedBotStateContext(std::size_t index,
											   const runtime::MovementPhysicsState &before,
											   compat::StateBehaviorInput *behavior) const
{
	compat::StateUpdateContext context = {};
	if (index >= services_->service_managedBotHandles().size())
	{
		return context;
	}
	const FakeClientHandle &handle = services_->service_managedBotHandles()[index];
	context.actor = {handle.actor.slot, handle.actor.actorGeneration};
	context.frame = {services_->service_lifecycle().mapGeneration(),
					 services_->service_lifecycle().roundGeneration(),
					 services_->service_adapterFrameCount()};
	context.timestamp =
		services_->service_globals() != nullptr ? services_->service_globals()->time : 0.0f;
	context.fullUpdateSequence = services_->service_managedBotFullUpdateSequences()[index];
	context.fullUpdate = true;
	context.lifeAvailability = handle.entity != nullptr
								   ? compat::ObservationAvailability::Available
								   : compat::ObservationAvailability::Unavailable;
	context.alive = !before.dead;
	context.roundAvailability = services_->service_lifecycle().isMapActive()
									? compat::ObservationAvailability::Available
									: compat::ObservationAvailability::Unavailable;
	context.roundActive = services_->service_lifecycle().isMapActive();
	context.requestTransition = false;
	context.requestedState = compat::CompatibilityStateId::None;
	context.transitionId = compat::StateTransitionId::None;
	context.transitionReason = compat::StateTransitionReason::None;
	context.transitionRequest = compat::ObservationAvailability::Available;
	context.attackObservation = compat::ObservationAvailability::Available;
	context.rngAvailability = compat::ObservationAvailability::Available;
	if (behavior != nullptr)
	{
		*behavior = {};
		behavior->navigationOwned =
			services_->service_compatibilitySurface().configuration().mode ==
			compat::RuntimeMode::Compatibility;
		behavior->navigationReady = services_->service_navPublisher().snapshot().isValid();
		const ManagedObjectiveTargetCache &target = actors_[index].managedBotObjectiveTargets;
		behavior->goalAvailable =
			target.valid &&
			target.mapGeneration == services_->service_lifecycle().mapGeneration() &&
			target.roundGeneration == services_->service_lifecycle().roundGeneration();
		if (behavior->goalAvailable)
		{
			const float dx = before.origin.x - target.target.x;
			const float dy = before.origin.y - target.target.y;
			const float dz = before.origin.z - target.target.z;
			behavior->goalReached = dx * dx + dy * dy + dz * dz <= 64.0f * 64.0f;
		}
		const auto &failure = services_->navigation_managedNavigationFailures(index);
		const bool ownPathFailed =
			failure.actor == context.actor && failure.frame.isValid() &&
			failure.frame.mapGeneration == context.frame.mapGeneration &&
			failure.frame.roundGeneration == context.frame.roundGeneration &&
			failure.frame.tick < context.frame.tick &&
			failure.goalGeneration == target.generation &&
			actors_[index].managedBotStateMachines.state() == compat::CompatibilityStateId::MoveTo;
		behavior->pathFailed =
			ownPathFailed || (managedTeamObjectivePathFailurePending_ &&
							  managedTeamObjectivePathFailureActor_ == context.actor);
		behavior->objectiveActive = behavior->goalAvailable;
		if (behavior->navigationOwned && actors_[index].managedBotStateMachines.state() ==
											 compat::CompatibilityStateId::DefuseBomb)
		{
			behavior->objectiveActive = services_->observation_bombNoticeKnown(index) &&
										services_->observation_bombNoticePlanted(index);
			behavior->defuseReceipt = services_->observation_publicObservations().progressReceipt(
				context.actor, context.frame, context.timestamp,
				actors_[index].managedBotStateMachines.stateTimestamp());
		}
		context.behavior = behavior;
	}
	return context;
}

int BotBehaviorRuntime::resolveManagedBotTeam(
	std::size_t index, const compat::CompatibilityObservation &observation) const
{
	if (index < NativeBotObservation::kClientSlotCount &&
		services_->service_joinControllers()[index].teamConfirmed() &&
		services_->observation_managedBotTeamNumbers(index) >= 1U &&
		services_->observation_managedBotTeamNumbers(index) <= 2U)
	{
		return static_cast<int>(services_->observation_managedBotTeamNumbers(index));
	}
	if (observation.player.team.isAvailable() && observation.player.team.value >= 1 &&
		observation.player.team.value <= 2)
	{
		return observation.player.team.value;
	}
	return 0;
}

BotBehaviorRuntime::BehaviorPhaseResult
BotBehaviorRuntime::updateBehaviorLifecycle(std::size_t index,
											const runtime::MovementPhysicsState &before,
											compat::StateUpdateContext &context)
{
	auto &machine = actors_[index].managedBotStateMachines;
	actors_[index].scenarioDecisions = {};
	if (actors_[index].managedBotStateRounds != 0U &&
		actors_[index].managedBotStateRounds != services_->service_lifecycle().roundGeneration() &&
		machine.isInitialized())
	{
		actors_[index].scenarioPlanners.reset();
		(void)machine.onRoundReset(context);
	}
	actors_[index].managedBotStateRounds = services_->service_lifecycle().roundGeneration();
	if (!machine.isInitialized())
	{
		actors_[index].scenarioPlanners.reset();
		(void)machine.initialize(context);
		if (before.dead)
		{
			(void)machine.onDeath(context);
			actors_[index].managedBotWasDead = true;
		}
		return BehaviorPhaseResult::Complete;
	}
	if (before.dead)
	{
		actors_[index].scenarioPlanners.reset();
		services_->applyNavigationRequest({index, NavigationRequestKind::ClearFailure});
		if (!actors_[index].managedBotWasDead)
		{
			(void)machine.onDeath(context);
		}
		actors_[index].managedBotWasDead = true;
		return BehaviorPhaseResult::Complete;
	}
	if (actors_[index].managedBotWasDead)
	{
		actors_[index].scenarioPlanners.reset();
		(void)machine.onRespawn(context);
		actors_[index].managedBotWasDead = false;
		return BehaviorPhaseResult::Complete;
	}

	return BehaviorPhaseResult::Continue;
}

void BotBehaviorRuntime::updateScenarioState(std::size_t index,
											 const runtime::MovementPhysicsState &before,
											 compat::StateUpdateContext &context,
											 compat::StateBehaviorInput &stateBehavior)
{
	const compat::RuntimeMode mode = services_->service_compatibilitySurface().configuration().mode;
	if (mode == compat::RuntimeMode::Compatibility)
	{
		const compat::ObservationTimingContext timing = {
			services_->command_managedBotCommandSequences(index), 0U,
			services_->service_managedBotFullUpdateSequences()[index]};
		compat::CompatibilityObservation observation = {};
		const ObservationAdapterResult observationResult =
			services_->observation_observationAdapter().collectActor(
				services_->service_managedBotHandles()[index].entity, context.actor, context.frame,
				timing, &observation);
		if (observationResult == ObservationAdapterResult::Accepted)
		{
			compat::ScenarioBehaviorInput scenario = {};
			ScenarioUpdateData data{timing};
			populateScenarioPublicState(index, before, context, stateBehavior, observation,
										scenario, data);
			observeScenarioEntities(index, before, context, stateBehavior, observation, scenario,
									data);
			updateScenarioNavigation(index, before, context, stateBehavior, observation, scenario,
									 data);
			updateScenarioTeamwork(index, before, context, stateBehavior, observation, scenario,
								   data);
			applyScenarioDecision(index, before, context, stateBehavior, observation, scenario,
								  data);
		}
	}
}

void BotBehaviorRuntime::populateScenarioPublicState(
	std::size_t index, const runtime::MovementPhysicsState &before,
	compat::StateUpdateContext &context, compat::StateBehaviorInput &stateBehavior,
	const compat::CompatibilityObservation &observation, compat::ScenarioBehaviorInput &scenario,
	ScenarioUpdateData &data)
{
	(void)stateBehavior;
	auto &machine = actors_[index].managedBotStateMachines;
	scenario.actor = context.actor;
	scenario.frame = context.frame;
	scenario.timestamp = context.timestamp;
	scenario.selfPositionKnown = std::isfinite(before.origin.x) && std::isfinite(before.origin.y) &&
								 std::isfinite(before.origin.z);
	scenario.selfPosition = {before.origin.x, before.origin.y, before.origin.z};
	scenario.lifeKnown = context.lifeAvailability == compat::ObservationAvailability::Available;
	scenario.alive = context.alive;
	scenario.reactionSeconds = actors_[index].managedProfiles.reactionTimeSeconds;
	scenario.aggression = static_cast<float>(actors_[index].managedProfiles.aggression) / 100.0f;
	scenario.attacking = machine.attackOverlayActive();
	scenario.currentDefusing = machine.state() == compat::CompatibilityStateId::DefuseBomb;
	scenario.alreadyEscapingFromBomb =
		machine.state() == compat::CompatibilityStateId::EscapeFromBomb;
	data.observedTeam = resolveManagedBotTeam(index, observation);
	const int observedTeam = data.observedTeam;
	scenario.team = observedTeam == 1	? objectives::TeamRole::Terrorist
					: observedTeam == 2 ? objectives::TeamRole::CounterTerrorist
										: objectives::TeamRole::Unknown;
	compat::PublicActorState publicState = {};
	scenario.blind = services_->observation_publicObservations().read(context.actor, context.frame,
																	  &publicState) &&
					 context.timestamp < publicState.blindUntil;
	scenario.isVipKnown = observation.objective.vip.isAvailable();
	scenario.isVip = scenario.isVipKnown && observation.objective.vip.value;
	scenario.bombPlantedKnown = observation.objective.bombPlanted.isAvailable();
	scenario.bombPlanted = scenario.bombPlantedKnown && observation.objective.bombPlanted.value;
	if (observation.objective.bombPosition.isAvailable())
	{
		const auto &position = observation.objective.bombPosition.value;
		scenario.bomb.knowledge = compat::ScenarioKnowledge::Remembered;
		scenario.bomb.frame = context.frame;
		scenario.bomb.position = {position.x, position.y, position.z};
		const float dx = position.x - before.origin.x, dy = position.y - before.origin.y,
					dz = position.z - before.origin.z;
		scenario.bomb.pathCost = std::sqrt(dx * dx + dy * dy + dz * dz);
	}
	if (services_->observation_bombNoticeKnown(index))
	{
		scenario.bombPlantedKnown = true;
		scenario.bombPlanted = services_->observation_bombNoticePlanted(index);
	}
	if (scenario.bombPlantedKnown && scenario.bombPlanted)
	{
		const float publicFuse =
			readOptionalCvarFloat(services_->service_engineFunctions(), "mp_c4timer");
		scenario.timerEstimated = services_->observation_publicBombClocks(index).estimate(
			context.frame, context.timestamp, publicFuse, &scenario.bombSecondsRemaining);
	}
	const auto &score = services_->observation_publicScoreStatuses(index)[index];
	if (score.known &&
		score.serial == static_cast<std::uint32_t>(
							services_->service_managedBotHandles()[index].entity->serialnumber) &&
		score.frame.mapGeneration == context.frame.mapGeneration &&
		score.frame.roundGeneration == context.frame.roundGeneration)
	{
		scenario.isVipKnown = true;
		scenario.isVip = (score.flags & 4) != 0;
	}
	scenario.defuseKitKnown = observation.objective.hasDefuseKit.isAvailable();
	scenario.defuseKit = scenario.defuseKitKnown && observation.objective.hasDefuseKit.value;
	scenario.radio = &services_->observation_scenarioRadio(index);
}

void BotBehaviorRuntime::observeScenarioEntities(
	std::size_t index, const runtime::MovementPhysicsState &before,
	compat::StateUpdateContext &context, compat::StateBehaviorInput &stateBehavior,
	const compat::CompatibilityObservation &observation, compat::ScenarioBehaviorInput &scenario,
	ScenarioUpdateData &data)
{
	(void)stateBehavior;
	(void)observation;
	const int observedTeam = data.observedTeam;
	auto &automaticLeaderRange = data.automaticLeaderRange;

	if (services_->service_engineFunctions() != nullptr &&
		services_->service_globals() != nullptr &&
		services_->service_engineFunctions()->pfnSzFromIndex != nullptr &&
		services_->service_engineFunctions()->pfnPEntityOfEntIndex != nullptr)
	{
		const char *map = services_->service_engineFunctions()->pfnSzFromIndex(
			services_->service_globals()->mapname);
		scenario.scenario = map != nullptr && std::strncmp(map, "cs_", 3U) == 0
								? compat::PublicScenarioKind::Hostage
							: map != nullptr && std::strncmp(map, "as_", 3U) == 0
								? compat::PublicScenarioKind::Vip
							: map != nullptr && std::strncmp(map, "es_", 3U) == 0
								? compat::PublicScenarioKind::Escape
							: map != nullptr && std::strncmp(map, "de_", 3U) == 0
								? compat::PublicScenarioKind::Bomb
								: compat::PublicScenarioKind::Unknown;
		const float eye[] = {
			before.origin.x + services_->service_managedBotHandles()[index].entity->v.view_ofs.x,
			before.origin.y + services_->service_managedBotHandles()[index].entity->v.view_ofs.y,
			before.origin.z + services_->service_managedBotHandles()[index].entity->v.view_ofs.z};
		const auto project = [&](edict_t *entity, int entityIndex,
								 bool mapKnown) -> compat::ScenarioTarget {
			compat::ScenarioTarget target = {};
			if (entity == nullptr || entity->free != 0)
			{
				return target;
			}
			const bool brush = entity->v.solid == SOLID_BSP || entity->v.solid == SOLID_TRIGGER;
			const float point[] = {
				brush ? (entity->v.absmin.x + entity->v.absmax.x) * 0.5f : entity->v.origin.x,
				brush ? (entity->v.absmin.y + entity->v.absmax.y) * 0.5f : entity->v.origin.y,
				brush ? (entity->v.absmin.z + entity->v.absmax.z) * 0.5f : entity->v.origin.z};
			const float dx = point[0] - eye[0], dy = point[1] - eye[1], dz = point[2] - eye[2];
			const float planar = std::hypot(dx, dy);
			const float yaw = services_->service_managedBotHandles()[index].entity->v.v_angle.y *
							  3.14159265358979323846f / 180.0f;
			bool visible = false;
			if (!mapKnown && !scenario.blind &&
				services_->service_engineFunctions()->pfnTraceLine != nullptr &&
				!services_->observation_smokeObservations().blocks(
					{eye[0], eye[1], eye[2]}, {point[0], point[1], point[2]}, context.timestamp) &&
				(planar < 0.001f || (dx * std::cos(yaw) + dy * std::sin(yaw)) / planar > 0.5f))
			{
				TraceResult trace = {};
				services_->service_engineFunctions()->pfnTraceLine(
					eye, point, 1, services_->service_managedBotHandles()[index].entity, &trace);
				visible = trace.flFraction == 1.0f || trace.pHit == entity;
			}
			if (!mapKnown && !visible)
			{
				return target;
			}
			target.knowledge = mapKnown ? compat::ScenarioKnowledge::Remembered
										: compat::ScenarioKnowledge::Visible;
			target.frame = context.frame;
			// Core generations reserve zero; map entities may publicly have serial zero.
			target.entity = {static_cast<std::uint32_t>(entityIndex),
							 static_cast<std::uint32_t>(entity->serialnumber) + 1U};
			target.position = {point[0], point[1], point[2]};
			target.pathCost = std::sqrt(dx * dx + dy * dy + dz * dz);
			target.inUseRange = target.pathCost <= 64.0f;
			return target;
		};
		const auto nearer = [](const compat::ScenarioTarget &candidate,
							   const compat::ScenarioTarget &current) {
			return candidate.knowledge != compat::ScenarioKnowledge::Unknown &&
				   (current.knowledge == compat::ScenarioKnowledge::Unknown ||
					candidate.pathCost < current.pathCost);
		};
		const int maxEntities =
			(std::max)(0, (std::min)(services_->service_globals()->maxEntities, 2048));
		for (int entityIndex = 1; entityIndex < maxEntities; ++entityIndex)
		{
			edict_t *entity =
				services_->service_engineFunctions()->pfnPEntityOfEntIndex(entityIndex);
			if (entity == nullptr || entity->free != 0)
			{
				continue;
			}
			const char *name =
				services_->service_engineFunctions()->pfnSzFromIndex(entity->v.classname);
			if (name == nullptr)
			{
				continue;
			}
			if (entityIndex <= 32)
			{
				const auto &vipScore = services_->observation_publicScoreStatuses(
					index)[static_cast<std::size_t>(entityIndex - 1)];
				if (vipScore.known &&
					vipScore.serial == static_cast<std::uint32_t>(entity->serialnumber) &&
					vipScore.frame.mapGeneration == context.frame.mapGeneration &&
					vipScore.frame.roundGeneration == context.frame.roundGeneration &&
					(vipScore.flags & 4) != 0 && entity->v.deadflag == DEAD_NO)
				{
					scenario.vip = project(entity, entityIndex, false);
					scenario.vip.actor = {static_cast<std::uint32_t>(entityIndex),
										  static_cast<std::uint32_t>(entity->serialnumber) + 1U};
				}
			}
			if (entityIndex <= services_->service_globals()->maxClients &&
				(entity->v.flags & FL_FAKECLIENT) == 0 && entity->v.team == observedTeam &&
				entity->v.deadflag == DEAD_NO && entity->v.health > 0.0f)
			{
				const auto location = project(entity, entityIndex, false);
				if (location.knowledge == compat::ScenarioKnowledge::Visible)
				{
					const float dx = location.position.x - before.origin.x,
								dy = location.position.y - before.origin.y,
								dz = location.position.z - before.origin.z;
					const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
					if (distance < automaticLeaderRange)
					{
						automaticLeaderRange = distance;
						scenario.automaticLeader = {
							{static_cast<std::uint32_t>(entityIndex),
							 static_cast<std::uint32_t>(entity->serialnumber) + 1U},
							scenario.team,
							true,
							true,
							true,
							location};
						scenario.automaticLeader.location.actor = scenario.automaticLeader.actor;
					}
				}
			}
			if (std::strcmp(name, "grenade") == 0)
			{
				compat::ObjectiveObservation planted{};
				world::WorldVector knownPosition{};
				bool bombVisible = false;
				if (services_->observation_observationAdapter().collectPlantedBomb(
						entity, name,
						services_->service_engineFunctions()->pfnSzFromIndex(entity->v.model),
						context.timestamp, context.actor, context.frame, data.timing,
						&planted) == ObservationAdapterResult::Accepted &&
					services_->observation_observeObjective(index, entity, true, &knownPosition,
															&bombVisible))
				{
					scenario.bombPlantedKnown = true;
					scenario.bombPlanted = true;
					scenario.bomb.knowledge = bombVisible ? compat::ScenarioKnowledge::Visible
														  : compat::ScenarioKnowledge::Remembered;
					scenario.bomb.frame = context.frame;
					scenario.bomb.entity = {static_cast<std::uint32_t>(entityIndex),
											static_cast<std::uint32_t>(entity->serialnumber) + 1U};
					scenario.bomb.position = {knownPosition.x, knownPosition.y, knownPosition.z};
					const float dx = knownPosition.x - before.origin.x,
								dy = knownPosition.y - before.origin.y,
								dz = knownPosition.z - before.origin.z;
					scenario.bomb.pathCost = std::sqrt(dx * dx + dy * dy + dz * dz);
					scenario.bomb.inUseRange = scenario.bomb.pathCost <= 64.0f;
					scenario.humanDefusingKnown = true;
					scenario.humanDefusing =
						services_->observation_externalDefuserEntityIndex() != 0U &&
						services_->observation_externalDefuserLeaseUntil() >= context.timestamp &&
						services_->observation_externalDefuserMapGeneration() ==
							context.frame.mapGeneration &&
						services_->observation_externalDefuserRoundGeneration() ==
							context.frame.roundGeneration;
				}
			}
			compat::ScenarioTarget *destination = nullptr;
			bool mapKnown = true;
			if (std::strcmp(name, "hostage_entity") == 0 && entity->v.health > 0.0f)
			{
				destination = &scenario.hostage;
				mapKnown = false;
			}
			else if (std::strcmp(name, "func_hostage_rescue") == 0 ||
					 std::strcmp(name, "info_hostage_rescue") == 0)
			{
				destination = &scenario.rescueZone;
			}
			else if (std::strcmp(name, "func_vip_safetyzone") == 0)
			{
				destination = &scenario.vipZone;
			}
			else if (std::strcmp(name, "func_escapezone") == 0)
			{
				destination = &scenario.escapeZone;
			}
			if (destination != nullptr)
			{
				const auto target = project(entity, entityIndex, mapKnown);
				if (nearer(target, *destination))
				{
					*destination = target;
				}
			}
		}
		// Nearby CTs veto motion-based ownership inference. Their public
		// positions never establish escort ownership or provide a target.
		if (scenario.hostage.entity.isValid())
		{
			for (int player = 1; player <= services_->service_globals()->maxClients; ++player)
			{
				edict_t *other = services_->service_engineFunctions()->pfnPEntityOfEntIndex(player);
				if (other == nullptr ||
					other == services_->service_managedBotHandles()[index].entity ||
					other->free != 0 || other->v.team != 2 || other->v.deadflag != DEAD_NO ||
					other->v.health <= 0.0f)
					continue;
				const float dx = other->v.origin.x - scenario.hostage.position.x;
				const float dy = other->v.origin.y - scenario.hostage.position.y;
				const float dz = other->v.origin.z - scenario.hostage.position.z;
				if (dx * dx + dy * dy + dz * dz <= 256.0f * 256.0f)
				{
					scenario.hostageCompetitorNearby = true;
					break;
				}
			}
		}
		const auto tracked = actors_[index].scenarioPlanners.followLeader();
		if (actors_[index].scenarioPlanners.isFollowing() && tracked.isValid() &&
			tracked.slot <= static_cast<std::uint32_t>(services_->service_globals()->maxClients))
		{
			edict_t *human = services_->service_engineFunctions()->pfnPEntityOfEntIndex(
				static_cast<int>(tracked.slot));
			if (human != nullptr && !human->free &&
				static_cast<std::uint32_t>(human->serialnumber) + 1U == tracked.generation)
			{
				scenario.automaticLeader = {tracked,
											human->v.team == 1 ? objectives::TeamRole::Terrorist
											: human->v.team == 2
												? objectives::TeamRole::CounterTerrorist
												: objectives::TeamRole::Unknown,
											(human->v.flags & FL_FAKECLIENT) == 0,
											true,
											human->v.deadflag == DEAD_NO && human->v.health > 0.0f,
											project(human, static_cast<int>(tracked.slot), false)};
				scenario.automaticLeader.location.actor = tracked;
			}
		}
		const auto sender = services_->observation_scenarioRadio(index).sender;
		if (sender.isValid() &&
			sender.slot <= static_cast<std::uint32_t>(services_->service_globals()->maxClients))
		{
			edict_t *leader = services_->service_engineFunctions()->pfnPEntityOfEntIndex(
				static_cast<int>(sender.slot));
			if (leader != nullptr && leader->free == 0 &&
				static_cast<std::uint32_t>(leader->serialnumber) == sender.generation)
			{
				scenario.leader.actor = sender;
				scenario.leader.team = leader->v.team == 1 ? objectives::TeamRole::Terrorist
									   : leader->v.team == 2
										   ? objectives::TeamRole::CounterTerrorist
										   : objectives::TeamRole::Unknown;
				scenario.leader.human = (leader->v.flags & FL_FAKECLIENT) == 0;
				scenario.leader.lifeKnown = true;
				scenario.leader.alive = leader->v.deadflag == DEAD_NO && leader->v.health > 0.0f;
				scenario.leader.location = project(leader, static_cast<int>(sender.slot), false);
				scenario.leader.location.actor = sender;
				if (scenario.leader.location.knowledge == compat::ScenarioKnowledge::Visible)
				{
					services_->observation_observeRadioLeaderPosition(index,
																	  scenario.leader.location);
				}
			}
		}
	}
}

void BotBehaviorRuntime::updateScenarioNavigation(
	std::size_t index, const runtime::MovementPhysicsState &before,
	compat::StateUpdateContext &context, compat::StateBehaviorInput &stateBehavior,
	const compat::CompatibilityObservation &observation, compat::ScenarioBehaviorInput &scenario,
	ScenarioUpdateData &data)
{
	(void)stateBehavior;
	(void)observation;
	const int observedTeam = data.observedTeam;
	data.navigation = services_->service_navPublisher().snapshot();
	const auto &scenarioNavigation = data.navigation;
	const nav::NavDocument *scenarioDocument = scenarioNavigation.document();
	if (scenario.scenario == compat::PublicScenarioKind::Bomb &&
		scenario.team == objectives::TeamRole::CounterTerrorist && scenario.bombPlantedKnown &&
		scenario.bombPlanted && scenario.bomb.knowledge == compat::ScenarioKnowledge::Unknown &&
		scenarioNavigation.isValid() &&
		scenarioNavigation.mapGeneration() == context.frame.mapGeneration &&
		scenarioDocument != nullptr && services_->service_globals() != nullptr &&
		services_->service_engineFunctions() != nullptr &&
		services_->service_engineFunctions()->pfnPEntityOfEntIndex != nullptr &&
		services_->service_engineFunctions()->pfnSzFromIndex != nullptr)
	{
		compat::ScenarioTarget &search = actors_[index].bombSearchTargets;
		if (search.knowledge != compat::ScenarioKnowledge::Unknown)
		{
			const float dx = search.position.x - before.origin.x,
						dy = search.position.y - before.origin.y,
						dz = search.position.z - before.origin.z;
			if (dx * dx + dy * dy + dz * dz <= 64.0f * 64.0f)
			{
				actors_[index].bombSearchMemory.clear(search.entity);
				search = {};
			}
		}
		if (search.knowledge == compat::ScenarioKnowledge::Unknown)
		{
			std::vector<compat::BombSearchSite> sites;
			nav::NavQuery query(scenarioNavigation);
			nav::NavAreaMatch start{};
			const nav::NavVector self = {before.origin.x, before.origin.y, before.origin.z};
			const bool haveStart = query.findNearest3D(self, kMaximumObjectiveDistance, &start) ==
								   nav::NavQueryResult::Found;
			const int limit = (std::min)(services_->service_globals()->maxEntities, 2048);
			for (int entityIndex = 1; entityIndex < limit; ++entityIndex)
			{
				edict_t *entity =
					services_->service_engineFunctions()->pfnPEntityOfEntIndex(entityIndex);
				if (entity == nullptr || entity->free)
					continue;
				const char *name =
					services_->service_engineFunctions()->pfnSzFromIndex(entity->v.classname);
				if (name == nullptr || (std::strcmp(name, "func_bomb_target") != 0 &&
										std::strcmp(name, "info_bomb_target") != 0))
					continue;
				compat::BombSearchSite site{};
				site.entity = {static_cast<std::uint32_t>(entityIndex),
							   static_cast<std::uint32_t>(entity->serialnumber) + 1U};
				site.legacy = std::strcmp(name, "info_bomb_target") == 0;
				if (site.legacy)
				{
					site.center = {entity->v.origin.x, entity->v.origin.y, entity->v.origin.z};
					site.extent = {
						{site.center.x - 256.0f, site.center.y - 256.0f, site.center.z - 256.0f},
						{site.center.x + 256.0f, site.center.y + 256.0f, site.center.z + 256.0f}};
				}
				else
				{
					site.extent = {{entity->v.absmin.x, entity->v.absmin.y, entity->v.absmin.z},
								   {entity->v.absmax.x, entity->v.absmax.y, entity->v.absmax.z}};
					site.center = {(site.extent.lo.x + site.extent.hi.x) * 0.5f,
								   (site.extent.lo.y + site.extent.hi.y) * 0.5f,
								   (site.extent.lo.z + site.extent.hi.z) * 0.5f};
				}
				site.extent.lo.z -= 50.0f;
				site.extent.hi.z += 50.0f;
				nav::NavAreaMatch finish{};
				nav::NavCorridor route{};
				if (haveStart &&
					query.findNearest3D(site.center, kMaximumObjectiveDistance, &finish) ==
						nav::NavQueryResult::Found &&
					query.buildCorridor(start.area, finish.area, nav::NavRouteType::Fastest,
										&route) == nav::NavQueryResult::Found)
					site.travel = route.cost;
				sites.push_back(site);
			}
			const compat::RandomActor randomActor = {context.actor.slot, context.actor.generation};
			const compat::RandomTimingContext randomTiming = {
				services_->command_managedBotCommandSequences(index), 0U,
				services_->service_managedBotFullUpdateSequences()[index]};
			std::size_t selected = 0U;
			if (actors_[index].bombSearchMemory.choose(
					context.frame, sites, services_->service_compatibilityRandomSource(),
					randomActor, randomTiming, &selected))
			{
				nav::NavVector point{};
				nav::NavAreaMatch chosenArea{};
				nav::NavCorridor chosenRoute{};
				if (compat::bombSitePoint(*scenarioDocument, sites[selected], 16U,
										  services_->service_compatibilityRandomSource(),
										  randomActor, randomTiming, &point) &&
					haveStart &&
					query.findNearest3D(point, kMaximumObjectiveDistance, &chosenArea) ==
						nav::NavQueryResult::Found &&
					query.buildCorridor(start.area, chosenArea.area, nav::NavRouteType::Fastest,
										&chosenRoute) == nav::NavQueryResult::Found)
				{
					search.knowledge = compat::ScenarioKnowledge::Remembered;
					search.frame = context.frame;
					search.entity = sites[selected].entity;
					search.position = {point.x, point.y, point.z};
					search.pathCost = chosenRoute.cost;
				}
				else
					actors_[index].bombSearchMemory.clear(sites[selected].entity);
			}
		}
		if (search.knowledge != compat::ScenarioKnowledge::Unknown)
		{
			search.frame = context.frame;
			scenario.bombSearch = search;
		}
	}
	if (scenario.scenario == compat::PublicScenarioKind::Bomb && scenario.bombPlantedKnown &&
		scenario.bombPlanted && scenario.bomb.knowledge != compat::ScenarioKnowledge::Unknown &&
		scenarioNavigation.isValid() &&
		scenarioNavigation.mapGeneration() == context.frame.mapGeneration &&
		scenarioDocument != nullptr)
	{
		const nav::NavVector self = {before.origin.x, before.origin.y, before.origin.z};
		const nav::NavVector bomb = {scenario.bomb.position.x, scenario.bomb.position.y,
									 scenario.bomb.position.z};
		scenario.travelTimeEstimated =
			scenario.team == objectives::TeamRole::CounterTerrorist &&
			!scenario.alreadyEscapingFromBomb && !scenario.currentDefusing &&
			compat::estimateBombTravel(
				scenarioNavigation, self, bomb,
				services_->service_managedBotHandles()[index].entity->v.maxspeed,
				kMaximumObjectiveDistance, &scenario.travelSecondsToBomb);
		const bool lateDefuse =
			scenario.team == objectives::TeamRole::CounterTerrorist && scenario.timerEstimated &&
			scenario.travelTimeEstimated && scenario.defuseKitKnown &&
			scenario.bombSecondsRemaining <=
				scenario.travelSecondsToBomb + (scenario.defuseKit ? 5.0f : 10.0f);
		const bool requestedEscape =
			compat::ScenarioBehaviorPlanner::autoEscapeRequested(scenario) ||
			(lateDefuse && !scenario.attacking && !scenario.currentDefusing) ||
			scenario.alreadyEscapingFromBomb ||
			(scenario.radio != nullptr &&
			 scenario.radio->command == compat::ScenarioRadioCommand::GetOut);
		if (requestedEscape)
		{
			nav::AreaId &goal = actors_[index].scenarioEscapeAreas;
			const nav::NavArea *area = scenarioDocument->findArea(goal);
			if (area == nullptr)
			{
				const compat::RandomActor randomActor = {context.actor.slot,
														 context.actor.generation};
				const compat::RandomTimingContext randomTiming = {
					services_->command_managedBotCommandSequences(index), 0U,
					services_->service_managedBotFullUpdateSequences()[index]};
				if (compat::selectBombEscapeArea(*scenarioDocument, bomb,
												 services_->service_compatibilityRandomSource(),
												 randomActor, randomTiming, &goal))
					area = scenarioDocument->findArea(goal);
			}
			if (area != nullptr)
			{
				const auto point = compat::bombAreaCenter(*area);
				scenario.escapeZone.knowledge = compat::ScenarioKnowledge::Remembered;
				scenario.escapeZone.frame = context.frame;
				scenario.escapeZone.position = {point.x, point.y, point.z};
				const float dx = point.x - self.x, dy = point.y - self.y, dz = point.z - self.z;
				// A lower-bound ordering value; actual reachability belongs
				// to the state-owned navigation step, not a preflight search.
				scenario.escapeZone.pathCost = std::sqrt(dx * dx + dy * dy + dz * dz);
			}
		}
	}
	const auto validateRadioSender =
		[&](world::ActorKey sender) -> compat::ScenarioSenderValidation {
		compat::ScenarioSenderValidation validation{sender, false, false};
		if (!sender.isValid() || services_->service_globals() == nullptr ||
			services_->service_engineFunctions() == nullptr ||
			services_->service_engineFunctions()->pfnPEntityOfEntIndex == nullptr)
			return validation;
		validation.known = true;
		if (sender.slot > static_cast<std::uint32_t>(services_->service_globals()->maxClients))
			return validation;
		edict_t *human = services_->service_engineFunctions()->pfnPEntityOfEntIndex(
			static_cast<int>(sender.slot));
		validation.valid = (observedTeam == 1 || observedTeam == 2) && human != nullptr &&
						   !human->free && (human->v.flags & FL_CLIENT) != 0 &&
						   (human->v.flags & FL_FAKECLIENT) == 0 && human->v.team == observedTeam &&
						   static_cast<std::uint32_t>(human->serialnumber) == sender.generation;
		return validation;
	};
	if (scenario.radio != nullptr)
		scenario.radioSenderValidation = validateRadioSender(scenario.radio->sender);
	scenario.pendingRadioSenderValidation =
		validateRadioSender(actors_[index].scenarioPlanners.pendingRadioSender());
}

void BotBehaviorRuntime::updateScenarioTeamwork(std::size_t index,
												const runtime::MovementPhysicsState &before,
												compat::StateUpdateContext &context,
												compat::StateBehaviorInput &stateBehavior,
												const compat::CompatibilityObservation &observation,
												compat::ScenarioBehaviorInput &scenario,
												ScenarioUpdateData &data)
{
	(void)stateBehavior;
	auto &machine = actors_[index].managedBotStateMachines;
	const int observedTeam = data.observedTeam;
	auto &automaticLeaderRange = data.automaticLeaderRange;
	const auto &scenarioNavigation = data.navigation;
	const auto *scenarioDocument = data.navigation.document();
	data.wasFollowing = actors_[index].scenarioPlanners.isFollowing();
	const bool wasFollowing = data.wasFollowing;
	compat::TeamworkInput teamwork{};
	teamwork.actor = context.actor;
	teamwork.frame = context.frame;
	teamwork.now = context.timestamp;
	teamwork.teamwork = static_cast<float>(actors_[index].managedProfiles.teamwork) / 100.0f;
	teamwork.roundTimeKnown = services_->service_roundFreezeGeneration() ==
								  services_->service_lifecycle().roundGeneration() &&
							  std::isfinite(services_->service_roundFreezeStartTime()) &&
							  context.timestamp >= services_->service_roundFreezeStartTime();
	teamwork.elapsedRound = context.timestamp - services_->service_roundFreezeStartTime();
	teamwork.allowRoguesKnown =
		services_->service_engineFunctions() != nullptr &&
		services_->service_engineFunctions()->pfnCVarGetFloat != nullptr &&
		services_->service_engineFunctions()->pfnCVarGetPointer != nullptr &&
		services_->service_engineFunctions()->pfnCVarGetPointer("bot_allow_rogues") != nullptr;
	const float allowRogues =
		readOptionalCvarFloat(services_->service_engineFunctions(), "bot_allow_rogues");
	teamwork.allowRoguesKnown = teamwork.allowRoguesKnown && std::isfinite(allowRogues);
	teamwork.allowRogues = allowRogues != 0.0f;
	const auto currentState = machine.state();
	const bool sniper = observation.weapon.activeWeaponId.isAvailable() &&
						(observation.weapon.activeWeaponId.value == 3U ||
						 observation.weapon.activeWeaponId.value == 18U ||
						 observation.weapon.activeWeaponId.value == 13U ||
						 observation.weapon.activeWeaponId.value == 24U);
	scenario.busy = scenario.attacking || currentState == compat::CompatibilityStateId::Buy ||
					currentState == compat::CompatibilityStateId::DefuseBomb ||
					currentState == compat::CompatibilityStateId::PlantBomb ||
					actors_[index].scenarioPlanners.awaitingHostageEscort() ||
					(currentState == compat::CompatibilityStateId::Hide && sniper);
	teamwork.busy = scenario.busy;
	teamwork.following = wasFollowing;
	teamwork.followDuration =
		wasFollowing ? context.timestamp - actors_[index].scenarioPlanners.followSince() : 0.0f;
	teamwork.candidate = scenario.automaticLeader.actor;
	teamwork.candidateVisibleHumanFriend =
		scenario.automaticLeader.human && scenario.automaticLeader.lifeKnown &&
		scenario.automaticLeader.alive && scenario.automaticLeader.team == scenario.team &&
		scenario.automaticLeader.location.knowledge == compat::ScenarioKnowledge::Visible;
	teamwork.candidateRange = automaticLeaderRange;
	if (teamwork.candidateVisibleHumanFriend && scenarioNavigation.isValid() &&
		scenarioDocument != nullptr &&
		scenarioNavigation.mapGeneration() == context.frame.mapGeneration)
	{
		nav::NavQuery query(scenarioNavigation);
		nav::NavAreaMatch selfArea{}, humanArea{};
		nav::NavCorridor route{};
		const auto &position = scenario.automaticLeader.location.position;
		if (query.findNearest3D({before.origin.x, before.origin.y, before.origin.z},
								kMaximumObjectiveDistance,
								&selfArea) == nav::NavQueryResult::Found &&
			query.findContaining({position.x, position.y, position.z}, 64.0f, &humanArea) ==
				nav::NavQueryResult::Found &&
			query.buildCorridor(selfArea.area, humanArea.area, nav::NavRouteType::Fastest,
								&route) == nav::NavQueryResult::Found)
		{
			teamwork.candidateReachable = true;
			teamwork.candidateTravel = route.cost;
		}
		for (std::size_t bot = 0U; bot < NativeBotObservation::kClientSlotCount; ++bot)
			if (actors_[bot].scenarioPlanners.isFollowing() &&
				actors_[bot].scenarioPlanners.followLeader().slot == teamwork.candidate.slot)
				++teamwork.followerCount;
	}
	if (scenario.bombPlantedKnown && scenario.bombPlanted &&
		scenario.bomb.knowledge != compat::ScenarioKnowledge::Unknown &&
		services_->service_engineFunctions() != nullptr &&
		services_->service_engineFunctions()->pfnPEntityOfEntIndex != nullptr &&
		services_->service_engineFunctions()->pfnSzFromIndex != nullptr &&
		services_->service_globals() != nullptr)
	{
		for (int entityIndex = 1;
			 entityIndex < (std::min)(services_->service_globals()->maxEntities, 2048);
			 ++entityIndex)
		{
			edict_t *zone = services_->service_engineFunctions()->pfnPEntityOfEntIndex(entityIndex);
			if (zone == nullptr || zone->free)
				continue;
			const char *name =
				services_->service_engineFunctions()->pfnSzFromIndex(zone->v.classname);
			if (name == nullptr || (std::strcmp(name, "func_bomb_target") != 0 &&
									std::strcmp(name, "info_bomb_target") != 0))
				continue;
			const bool legacy = std::strcmp(name, "info_bomb_target") == 0;
			const float loX = legacy ? zone->v.origin.x - 256.0f : zone->v.absmin.x;
			const float loY = legacy ? zone->v.origin.y - 256.0f : zone->v.absmin.y;
			const float loZ = (legacy ? zone->v.origin.z - 256.0f : zone->v.absmin.z) - 50.0f;
			const float hiX = legacy ? zone->v.origin.x + 256.0f : zone->v.absmax.x;
			const float hiY = legacy ? zone->v.origin.y + 256.0f : zone->v.absmax.y;
			const float hiZ = (legacy ? zone->v.origin.z + 256.0f : zone->v.absmax.z) + 50.0f;
			const auto contains = [&](float x, float y, float z) {
				return x >= loX && x <= hiX && y >= loY && y <= hiY && z >= loZ && z <= hiZ;
			};
			if (contains(before.origin.x, before.origin.y, before.origin.z) &&
				contains(scenario.bomb.position.x, scenario.bomb.position.y,
						 scenario.bomb.position.z))
			{
				teamwork.atPlantedBombsite = true;
				break;
			}
		}
	}
	compat::TeamworkDecision cooperative{};
	if (scenario.lifeKnown && scenario.alive)
		cooperative = actors_[index].teamworkPolicies.update(
			teamwork, services_->service_compatibilityRandomSource(),
			{context.actor.slot, context.actor.generation},
			{services_->command_managedBotCommandSequences(index), 0U,
			 services_->service_managedBotFullUpdateSequences()[index]});
	scenario.rogue = cooperative.rogue;
	scenario.automaticFollowRequested = cooperative.available && cooperative.startFollowing;
	scenario.stopFollowingRequested = cooperative.available && cooperative.stopFollowing;
	scenario.autoFollowPermissionInferred = cooperative.humanPermissionInferred;
	if (!cooperative.rogueKnown && !wasFollowing)
		scenario.radio = nullptr;
	actors_[index].scenarioDecisions = actors_[index].scenarioPlanners.update(scenario);
	updateBombGuardTarget(index, scenario, &actors_[index].scenarioDecisions);
	if (wasFollowing && !actors_[index].scenarioPlanners.isFollowing())
		actors_[index].teamworkPolicies.followingStopped(context.timestamp);
	if (scenario.radio != nullptr &&
		scenario.radio->sequence > actors_[index].teamworkRadioApplied &&
		scenario.radio->senderHuman && scenario.radio->team == scenario.team && !scenario.busy &&
		(!scenario.rogue || wasFollowing) &&
		context.timestamp - scenario.radio->receivedTime >=
			1.0f + 2.0f * scenario.reactionSeconds + (scenario.rogue ? 2.0f : 0.0f))
	{
		const auto &decision = actors_[index].scenarioDecisions;
		const auto command = scenario.radio->command;
		const bool allow = decision.action == compat::ScenarioAction::Follow &&
						   actors_[index].scenarioPlanners.followLeader() == scenario.radio->sender;
		const bool inhibit =
			(command == compat::ScenarioRadioCommand::HoldPosition &&
			 decision.action == compat::ScenarioAction::Guard &&
			 scenario.radio->position.knowledge != compat::ScenarioKnowledge::Unknown) ||
			(command == compat::ScenarioRadioCommand::GoGoGo &&
			 decision.action == compat::ScenarioAction::Hunt) ||
			(command == compat::ScenarioRadioCommand::GetOut &&
			 decision.action == compat::ScenarioAction::Escape && scenario.bombPlantedKnown &&
			 scenario.bombPlanted);
		if ((allow || inhibit) && scenario.radioSenderValidation.known &&
			scenario.radioSenderValidation.valid &&
			scenario.radioSenderValidation.actor == scenario.radio->sender)
		{
			world::ActorKey human = scenario.radio->sender;
			edict_t *entity = services_->service_engineFunctions()->pfnPEntityOfEntIndex(
				static_cast<int>(human.slot));
			if (entity != nullptr && !entity->free && (entity->v.flags & FL_CLIENT) != 0 &&
				(entity->v.flags & FL_FAKECLIENT) == 0 && entity->v.team == observedTeam &&
				static_cast<std::uint32_t>(entity->serialnumber) == human.generation)
			{
				human.generation = static_cast<std::uint32_t>(entity->serialnumber) + 1U;
				actors_[index].teamworkPolicies.observeHumanPolicy(human, context.timestamp, allow);
				actors_[index].teamworkRadioApplied = scenario.radio->sequence;
			}
		}
	}
}

void BotBehaviorRuntime::applyScenarioDecision(std::size_t index,
											   const runtime::MovementPhysicsState &before,
											   compat::StateUpdateContext &context,
											   compat::StateBehaviorInput &stateBehavior,
											   const compat::CompatibilityObservation &observation,
											   compat::ScenarioBehaviorInput &scenario,
											   ScenarioUpdateData &data)
{
	(void)before;
	(void)data;
	auto &machine = actors_[index].managedBotStateMachines;
	const compat::ScenarioBehaviorDecision &scenarioDecision = actors_[index].scenarioDecisions;
	stateBehavior.goalAvailable = scenarioDecision.hasTarget || stateBehavior.goalAvailable;
	stateBehavior.leaderAvailable = actors_[index].scenarioPlanners.hasFollowingLeader(scenario);
	stateBehavior.objectiveActive = scenarioDecision.hasTarget || stateBehavior.objectiveActive;
	if (machine.state() == compat::CompatibilityStateId::DefuseBomb ||
		machine.state() == compat::CompatibilityStateId::EscapeFromBomb)
		stateBehavior.objectiveActive = scenario.bombPlantedKnown && scenario.bombPlanted;
	stateBehavior.inInteractionRange =
		scenarioDecision.hasTarget && scenarioDecision.target.inUseRange;
	stateBehavior.guardObjectiveEnded =
		machine.state() == compat::CompatibilityStateId::Hide &&
		scenario.team == objectives::TeamRole::CounterTerrorist && scenario.humanDefusingKnown &&
		!scenario.humanDefusing && scenarioDecision.state == compat::CompatibilityStateId::MoveTo;
	if (machine.state() == compat::CompatibilityStateId::DefuseBomb && scenario.humanDefusing &&
		scenario.bomb.knowledge == compat::ScenarioKnowledge::Visible)
		stateBehavior.defuseReceipt = compat::DefuseReceipt::Other;
	const bool ownsCurrentTask = machine.state() != compat::CompatibilityStateId::Idle &&
								 machine.state() != compat::CompatibilityStateId::Hunt &&
								 machine.state() != compat::CompatibilityStateId::MoveTo;
	if (scenarioDecision.action != compat::ScenarioAction::None &&
		scenarioDecision.state != compat::CompatibilityStateId::None &&
		!(ownsCurrentTask && scenarioDecision.state == compat::CompatibilityStateId::MoveTo) &&
		!(machine.state() == compat::CompatibilityStateId::DefuseBomb &&
		  scenarioDecision.state != compat::CompatibilityStateId::DefuseBomb))
	{
		context.requestedState = scenarioDecision.state;
		context.requestTransition = machine.state() != context.requestedState;
		context.transitionId = compat::StateTransitionId::ExplicitStateChange;
		context.transitionReason = compat::StateTransitionReason::ObjectiveChanged;
	}
	const bool carryingC4 =
		observation.objective.carryingC4.isAvailable() && observation.objective.carryingC4.value;
	const int team = resolveManagedBotTeam(index, observation);
	if (carryingC4 && team == 1)
	{
		const bool atBombsite = managedActorAtBombSite(index);
		const bool plantPhase =
			actors_[index].managedBombApproaches.initialized &&
			actors_[index].managedBombApproaches.mapGeneration == context.frame.mapGeneration &&
			actors_[index].managedBombApproaches.roundGeneration == context.frame.roundGeneration &&
			actors_[index].managedBombApproaches.finished;
		stateBehavior.objectiveActive = true;
		stateBehavior.inInteractionRange = plantPhase && atBombsite;
		context.requestedState = plantPhase && atBombsite ? compat::CompatibilityStateId::PlantBomb
								 : plantPhase && stateBehavior.goalAvailable
									 ? compat::CompatibilityStateId::MoveTo
									 : compat::CompatibilityStateId::Hunt;
		context.requestTransition = machine.state() != context.requestedState;
		context.transitionId = compat::StateTransitionId::ExplicitStateChange;
		context.transitionReason = compat::StateTransitionReason::ObjectiveChanged;
	}
}

void BotBehaviorRuntime::commitBehaviorState(std::size_t index, compat::StateUpdateContext &context,
											 compat::StateBehaviorInput &stateBehavior)
{
	auto &machine = actors_[index].managedBotStateMachines;
	const auto previousState = machine.state();
	const float previousStateTimestamp = machine.stateTimestamp();
	stateBehavior.navigation.actor = context.actor;
	stateBehavior.navigation.frame = context.frame;
	stateBehavior.navigation.fullUpdateSequence = context.fullUpdateSequence;
	actors_[index].managedStateInputs = stateBehavior;
	(void)machine.update(context);
	if (previousState != machine.state())
	{
		services_->applyNavigationRequest({index, NavigationRequestKind::ClearStep});
		// Reference state entry computes or abandons its own path. Never carry
		// Idle's roam corridor into a newly assigned MoveTo task.
		services_->applyNavigationRequest({index, NavigationRequestKind::DestroyPath});
		if (previousState == compat::CompatibilityStateId::DefuseBomb ||
			previousState == compat::CompatibilityStateId::PlantBomb ||
			previousState == compat::CompatibilityStateId::Hide)
			services_->applyNavigationRequest({index, NavigationRequestKind::ResetProgress});
		if (machine.state() == compat::CompatibilityStateId::Idle &&
			(previousState == compat::CompatibilityStateId::DefuseBomb ||
			 previousState == compat::CompatibilityStateId::PlantBomb))
		{
			actors_[index].managedBotObjectiveActions = {};
			actors_[index].scenarioDecisions = {};
			if (services_->service_runtimeProfiler().enabled())
				profiledLogConsole(
					services_->service_runtimeProfiler(), services_->service_pluginId(),
					"profile objectiveExit bot_id=%u frame=%u state_from=%s receipt=%u "
					"elapsed=%.3f",
					context.actor.slot, context.frame.tick, compat::toString(previousState),
					static_cast<unsigned>(stateBehavior.defuseReceipt),
					context.timestamp - previousStateTimestamp);
		}
	}
	if (stateBehavior.pathFailed && previousState == compat::CompatibilityStateId::MoveTo &&
		machine.state() == compat::CompatibilityStateId::Idle)
	{
		// The reference state's failure exit abandons this path/goal. Do not
		// let the outer objective loop immediately reuse its old MoveTo intent.
		services_->applyNavigationRequest({index, NavigationRequestKind::DestroyPath});
		actors_[index].managedBotObjectiveTargets = {};
		actors_[index].scenarioDecisions = {};
		if (services_->service_runtimeProfiler().enabled())
			profiledLogConsole(
				services_->service_runtimeProfiler(), services_->service_pluginId(),
				"profile navigationFailure bot_id=%u frame=%u state_from=%s state_to=%s",
				context.actor.slot, context.frame.tick, compat::toString(previousState),
				compat::toString(machine.state()));
	}
	if (!stateBehavior.pathFailed || !machine.attackOverlayActive())
		services_->applyNavigationRequest({index, NavigationRequestKind::ClearFailure});
}

} // namespace metamod
} // namespace astrabot
