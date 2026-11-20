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
constexpr float kGoalArrivalRange = 64.0f;
constexpr float kProfilePercentScale = 100.0f;
constexpr int kVipScoreFlag = 4;
constexpr unsigned kScenarioMapPrefixLength = 3U;
// Keep literal evaluation precision on x87; each macro is undefined below.
#define ASTRABOT_BOT_BEHAVIOR_SCENARIO_MINIMUM_PLANAR_SIGHT_RANGE 0.001f
constexpr float kVisibleFacingCosine = 0.5f;
constexpr float kEntityUseRange = 64.0f;
constexpr int kMaximumEntityIndex = 2048;
constexpr int kMaximumClientSlots = 32;
constexpr float kHostageCompetitorRange = 256.0f;
constexpr float kDefuseWithKitSeconds = 5.0f;
constexpr float kDefuseWithoutKitSeconds = 10.0f;
constexpr float kHumanNavTolerance = 64.0f;
constexpr float kRadioResponseBaseSeconds = 1.0f;
constexpr float kRadioReactionScale = 2.0f;
constexpr float kRogueRadioDelaySeconds = 2.0f;
}
using namespace runtime_detail;

void BotBehaviorRuntime::resetMatchMorale()
{
	for (auto &actor : actors_)
		actor.morale.newMatch();
}

bool BotBehaviorRuntime::allHostagesRescued(world::FrameIdentity frame) const
{
	if (!hostageRescueResult_.isValid() || hostageRescueResult_.mapGeneration != frame.mapGeneration)
		return false;
	// Round_End allocates the next generation before Round_Start opens it.
	return hostageRescueResult_.roundGeneration == frame.roundGeneration ||
		(services_->service_roundOver() && frame.roundGeneration > hostageRescueResult_.roundGeneration &&
			frame.roundGeneration - hostageRescueResult_.roundGeneration == 1U);
}

void BotBehaviorRuntime::observeRoundWin(int team, world::FrameIdentity frame, bool rescuedHostages,
	const LateDefuseRecipients &lateDefuse)
{
	const auto current = [&] {
		const auto &life = services_->service_lifecycle();
		return life.mapGeneration() == frame.mapGeneration && life.roundGeneration() == frame.roundGeneration &&
			frame.tick == services_->service_adapterFrameCount() && !services_->service_roundOver();
	};
	if (!current() ||
		services_->service_compatibilitySurface().configuration().mode != compat::RuntimeMode::Compatibility)
		return;
	if (rescuedHostages && team == kCounterTerroristTeam)
		hostageRescueResult_ = frame;
	if (moraleResultRound_ == frame.roundGeneration)
		return;
	moraleResultRound_ = frame.roundGeneration;
	const auto handles = services_->service_managedBotHandles();
	for (std::size_t i = 0; i < handles.size(); ++i)
	{
		const auto &handle = handles[i];
		if (!services_->service_managedBotSlots()[i] || !handle.entity || handle.entity->free)
			continue;
		const int serial = handle.entity->serialnumber;
		const int ownerTeam = services_->observation_resolvePlayerTeam(handle.entity);
		if (!current())
			return;
		const auto &now = services_->service_managedBotHandles()[i];
		if (services_->service_managedBotSlots()[i] && now.entity == handle.entity &&
			now.actor.actorGeneration == handle.actor.actorGeneration && !handle.entity->free &&
			handle.entity->serialnumber == serial && (ownerTeam == kCounterTerroristTeam || ownerTeam == kTerroristTeam))
		{
			actors_[i].morale.roundResult(ownerTeam == team);
			if (ownerTeam == team)
				reportRoundVictory(i, ownerTeam, lateDefuse.test(i), current);
		}
	}
}

compat::StateUpdateContext BotBehaviorRuntime::makeManagedBotStateContext(
	std::size_t index, const runtime::MovementPhysicsState &before, compat::StateBehaviorInput *behavior) const
{
	compat::StateUpdateContext context = {};
	if (index >= services_->service_managedBotHandles().size())
	{
		return context;
	}
	const FakeClientHandle &handle = services_->service_managedBotHandles()[index];
	context.actor = {handle.actor.slot, handle.actor.actorGeneration};
	context.frame = {services_->service_lifecycle().mapGeneration(), services_->service_lifecycle().roundGeneration(),
		services_->service_adapterFrameCount()};
	context.timestamp = services_->service_globals() != nullptr ? services_->service_globals()->time : 0.0f;
	context.fullUpdateSequence = services_->service_managedBotFullUpdateSequences()[index];
	context.fullUpdate = true;
	context.lifeAvailability = handle.entity != nullptr ? compat::ObservationAvailability::Available
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
			services_->service_compatibilitySurface().configuration().mode == compat::RuntimeMode::Compatibility;
		behavior->navigationReady = services_->service_navPublisher().snapshot().isValid();
		const ManagedObjectiveTargetCache &target = actors_[index].managedBotObjectiveTargets;
		behavior->goalAvailable = target.valid &&
			target.mapGeneration == services_->service_lifecycle().mapGeneration() &&
			target.roundGeneration == services_->service_lifecycle().roundGeneration();
		if (behavior->goalAvailable)
		{
			const float dx = before.origin.x - target.target.x;
			const float dy = before.origin.y - target.target.y;
			const float dz = before.origin.z - target.target.z;
			behavior->goalReached = dx * dx + dy * dy + dz * dz <= kGoalArrivalRange * kGoalArrivalRange;
		}
		const auto &failure = services_->navigation_managedNavigationFailures(index);
		const bool ownPathFailed = failure.actor == context.actor && failure.frame.isValid() &&
			failure.frame.mapGeneration == context.frame.mapGeneration &&
			failure.frame.roundGeneration == context.frame.roundGeneration && failure.frame.tick < context.frame.tick &&
			failure.goalGeneration == target.generation &&
			actors_[index].managedBotStateMachines.state() == compat::CompatibilityStateId::MoveTo;
		behavior->pathFailed = ownPathFailed ||
			(managedTeamObjectivePathFailurePending_ && managedTeamObjectivePathFailureActor_ == context.actor);
		behavior->objectiveActive = behavior->goalAvailable;
		if (behavior->navigationOwned &&
			actors_[index].managedBotStateMachines.state() == compat::CompatibilityStateId::DefuseBomb)
		{
			behavior->objectiveActive =
				services_->observation_bombNoticeKnown(index) && services_->observation_bombNoticePlanted(index);
			behavior->defuseReceipt = services_->observation_publicObservations().progressReceipt(context.actor,
				context.frame, context.timestamp, actors_[index].managedBotStateMachines.stateTimestamp());
		}
		context.behavior = behavior;
	}
	return context;
}

BotBehaviorRuntime::BehaviorPhaseResult BotBehaviorRuntime::updateBehaviorLifecycle(
	std::size_t index, const runtime::MovementPhysicsState &before, compat::StateUpdateContext &context)
{
	auto &machine = actors_[index].managedBotStateMachines;
	const bool native =
		services_->service_compatibilitySurface().configuration().mode == compat::RuntimeMode::Compatibility;
	if (native)
		actors_[index].morale.observeLife(!before.dead);
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
		if (native && !actors_[index].buyingComplete)
			(void)machine.transition(compat::CompatibilityStateId::Buy, compat::StateTransitionReason::RespawnObserved,
				compat::StateTransitionId::LifecycleRespawn, context);
		if (before.dead)
		{
			if (!native)
				(void)machine.onDeath(context);
			actors_[index].managedBotWasDead = true;
		}
		return BehaviorPhaseResult::Complete;
	}
	if (before.dead)
	{
		actors_[index].noiseInvestigation.reset();
		actors_[index].investigationWalking = false;
		actors_[index].knifeRun.reset();
		actors_[index].enemyReaction.resetForLife();
		actors_[index].enemyWeaponSource = {};
		actors_[index].equipment.reset();
		actors_[index].scenarioPlanners.reset();
		services_->applyNavigationRequest({index, NavigationRequestKind::ClearFailure});
		if (!actors_[index].managedBotWasDead && !native)
		{
			(void)machine.onDeath(context);
		}
		actors_[index].managedBotWasDead = true;
		return BehaviorPhaseResult::Complete;
	}
	if (actors_[index].managedBotWasDead)
	{
		actors_[index].bombStatus = {};
		actors_[index].someHostagesTaken = false;
		actors_[index].sniperWarning = {};
		actors_[index].hostagePositions = {};
		actors_[index].looseBombObservation = {};
		actors_[index].equipment.reset();
		actors_[index].scenarioPlanners.reset();
		if (native)
		{
			invalidateHelpExecution(index);
			actors_[index].teamworkPolicies.resetValues();
			actors_[index].buyControllers.reset();
			actors_[index].buyingComplete = false;
			actors_[index].managedBotObjectiveTargets = {};
			actors_[index].managedBotObjectiveActions = {};
		}
		(void)machine.onRespawn(context);
		if (native)
			(void)machine.transition(compat::CompatibilityStateId::Buy, compat::StateTransitionReason::RespawnObserved,
				compat::StateTransitionId::LifecycleRespawn, context);
		actors_[index].managedBotWasDead = false;
		return BehaviorPhaseResult::Complete;
	}

	return BehaviorPhaseResult::Continue;
}

void BotBehaviorRuntime::updateScenarioState(std::size_t index, const runtime::MovementPhysicsState &before,
	compat::StateUpdateContext &context, compat::StateBehaviorInput &stateBehavior, PlantMoveChatter *plantMove)
{
	const auto receipt = reactionContext(index);
	const auto revision = actors_[index].executionRevision;
	const auto epoch = actors_[index].managedBotStateMachines.normalStateEpoch();
	const auto current = [&]
	{
		return executionCurrent(index, receipt, revision) &&
			actors_[index].managedBotStateMachines.normalStateEpoch() == epoch;
	};
	const compat::RuntimeMode mode = services_->service_compatibilitySurface().configuration().mode;
	if (mode == compat::RuntimeMode::Compatibility)
	{
		const compat::ObservationTimingContext timing = {services_->command_managedBotCommandSequences(index), 0U,
			services_->service_managedBotFullUpdateSequences()[index]};
		compat::CompatibilityObservation observation = {};
		const ObservationAdapterResult observationResult = services_->observation_observationAdapter().collectActor(
			services_->service_managedBotHandles()[index].entity, context.actor, context.frame, timing, &observation);
		if (observationResult == ObservationAdapterResult::Accepted && current())
		{
			compat::ScenarioBehaviorInput scenario = {};
			ScenarioUpdateData data{timing};
			populateScenarioPublicState(index, before, context, stateBehavior, observation, scenario, data);
			if (!current())
				return;
			observeScenarioEntities(index, before, context, stateBehavior, observation, scenario, data);
			if (!current())
				return;
			if (scenario.scenario == compat::PublicScenarioKind::Hostage && data.observedTeam == kTerroristTeam)
				updateHostagePositions(index, context, data.initialHostages);
			if (!current())
				return;
			actors_[index].looseBombObservation = data.looseBomb;
			if (data.looseBomb.entity.isValid())
				rememberLooseBombDrop(data.looseBomb.entity, context.frame, data.looseBombOrigin);
			else if (data.looseBomb.frame == context.frame)
				looseBombDrop_ = {};
			updateScenarioNavigation(index, before, context, stateBehavior, observation, scenario, data);
			if (!current())
				return;
			updateScenarioTeamwork(index, before, context, stateBehavior, observation, scenario, data);
			if (!current())
				return;
			applyScenarioDecision(index, before, context, stateBehavior, observation, scenario, data, plantMove);
		}
	}
}

void BotBehaviorRuntime::populateScenarioPublicState(std::size_t index, const runtime::MovementPhysicsState &before,
	compat::StateUpdateContext &context, compat::StateBehaviorInput &stateBehavior,
	const compat::CompatibilityObservation &observation, compat::ScenarioBehaviorInput &scenario,
	ScenarioUpdateData &data)
{
	(void)stateBehavior;
	(void)observation;
	auto &machine = actors_[index].managedBotStateMachines;
	scenario.actor = context.actor;
	scenario.frame = context.frame;
	scenario.timestamp = context.timestamp;
	scenario.selfPositionKnown =
		std::isfinite(before.origin.x) && std::isfinite(before.origin.y) && std::isfinite(before.origin.z);
	scenario.selfPosition = {before.origin.x, before.origin.y, before.origin.z};
	scenario.lifeKnown = context.lifeAvailability == compat::ObservationAvailability::Available;
	scenario.alive = context.alive;
	scenario.reactionSeconds = actors_[index].managedProfiles.reactionTimeSeconds;
	scenario.aggression = static_cast<float>(actors_[index].managedProfiles.aggression) / kProfilePercentScale;
	scenario.attacking = machine.attackOverlayActive();
	scenario.currentDefusing = machine.state() == compat::CompatibilityStateId::DefuseBomb;
	scenario.alreadyEscapingFromBomb = machine.state() == compat::CompatibilityStateId::EscapeFromBomb;
	data.observedTeam = services_->observation_resolvePlayerTeam(services_->service_managedBotHandles()[index].entity);
	const int observedTeam = data.observedTeam;
	scenario.team = observedTeam == kTerroristTeam ? objectives::TeamRole::Terrorist
		: observedTeam == kCounterTerroristTeam	   ? objectives::TeamRole::CounterTerrorist
												   : objectives::TeamRole::Unknown;
	compat::PublicActorState publicState = {};
	scenario.blind = services_->observation_publicObservations().read(context.actor, context.frame, &publicState) &&
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
		const float publicFuse = readOptionalCvarFloat(services_->service_engineFunctions(), "mp_c4timer");
		scenario.timerEstimated = services_->observation_publicBombClocks(index).estimate(
			context.frame, context.timestamp, publicFuse, &scenario.bombSecondsRemaining);
	}
	const auto &score = services_->observation_publicScoreStatuses(index)[index];
	if (score.known &&
		score.serial ==
			static_cast<std::uint32_t>(services_->service_managedBotHandles()[index].entity->serialnumber) &&
		score.frame.mapGeneration == context.frame.mapGeneration &&
		score.frame.roundGeneration == context.frame.roundGeneration)
	{
		scenario.isVipKnown = true;
		scenario.isVip = (score.flags & kVipScoreFlag) != 0;
	}
	scenario.defuseKitKnown = observation.objective.hasDefuseKit.isAvailable();
	scenario.defuseKit = scenario.defuseKitKnown && observation.objective.hasDefuseKit.value;
	scenario.radio = &services_->observation_scenarioRadio(index);
}

void BotBehaviorRuntime::observeScenarioEntities(std::size_t index, const runtime::MovementPhysicsState &before,
	compat::StateUpdateContext &context, compat::StateBehaviorInput &stateBehavior,
	const compat::CompatibilityObservation &observation, compat::ScenarioBehaviorInput &scenario,
	ScenarioUpdateData &data)
{
	(void)stateBehavior;
	(void)observation;
	const int observedTeam = data.observedTeam;
	auto &automaticLeaderRange = data.automaticLeaderRange;

	if (services_->service_engineFunctions() != nullptr && services_->service_globals() != nullptr &&
		services_->service_engineFunctions()->pfnSzFromIndex != nullptr &&
		services_->service_engineFunctions()->pfnPEntityOfEntIndex != nullptr)
	{
		const char *map = services_->service_engineFunctions()->pfnSzFromIndex(services_->service_globals()->mapname);
		scenario.scenario = map != nullptr && std::strncmp(map, "cs_", kScenarioMapPrefixLength) == 0
			? compat::PublicScenarioKind::Hostage
			: map != nullptr && std::strncmp(map, "as_", kScenarioMapPrefixLength) == 0
			? compat::PublicScenarioKind::Vip
			: map != nullptr && std::strncmp(map, "es_", kScenarioMapPrefixLength) == 0
			? compat::PublicScenarioKind::Escape
			: map != nullptr && std::strncmp(map, "de_", kScenarioMapPrefixLength) == 0
			? compat::PublicScenarioKind::Bomb
			: compat::PublicScenarioKind::Unknown;
		const float eye[] = {before.origin.x + services_->service_managedBotHandles()[index].entity->v.view_ofs.x,
			before.origin.y + services_->service_managedBotHandles()[index].entity->v.view_ofs.y,
			before.origin.z + services_->service_managedBotHandles()[index].entity->v.view_ofs.z};
		const auto project = [&](edict_t *entity, int entityIndex, bool mapKnown) -> compat::ScenarioTarget
		{
			compat::ScenarioTarget target = {};
			if (entity == nullptr || entity->free != 0)
			{
				return target;
			}
			const bool brush = entity->v.solid == SOLID_BSP || entity->v.solid == SOLID_TRIGGER;
			const float point[] = {brush ? (entity->v.absmin.x + entity->v.absmax.x) * 0.5f : entity->v.origin.x,
				brush ? (entity->v.absmin.y + entity->v.absmax.y) * 0.5f : entity->v.origin.y,
				brush ? (entity->v.absmin.z + entity->v.absmax.z) * 0.5f : entity->v.origin.z};
			const float dx = point[0] - eye[0], dy = point[1] - eye[1], dz = point[2] - eye[2];
			const float planar = std::hypot(dx, dy);
			const float yaw =
				services_->service_managedBotHandles()[index].entity->v.v_angle.y * 3.14159265358979323846f / 180.0f;
			bool visible = false;
			if (!mapKnown && !scenario.blind && services_->service_engineFunctions()->pfnTraceLine != nullptr &&
				!services_->observation_smokeObservations().blocks(
					{eye[0], eye[1], eye[2]}, {point[0], point[1], point[2]}, context.timestamp) &&
				(planar < ASTRABOT_BOT_BEHAVIOR_SCENARIO_MINIMUM_PLANAR_SIGHT_RANGE ||
					(dx * std::cos(yaw) + dy * std::sin(yaw)) / planar > kVisibleFacingCosine))
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
			target.knowledge = mapKnown ? compat::ScenarioKnowledge::Remembered : compat::ScenarioKnowledge::Visible;
			target.frame = context.frame;
			// Core generations reserve zero; map entities may publicly have serial zero.
			target.entity = {
				static_cast<std::uint32_t>(entityIndex), static_cast<std::uint32_t>(entity->serialnumber) + 1U};
			target.position = {point[0], point[1], point[2]};
			target.pathCost = std::sqrt(dx * dx + dy * dy + dz * dz);
			target.inUseRange = target.pathCost <= kEntityUseRange;
			return target;
		};
		const auto nearer = [](const compat::ScenarioTarget &candidate, const compat::ScenarioTarget &current)
		{
			return candidate.knowledge != compat::ScenarioKnowledge::Unknown &&
				(current.knowledge == compat::ScenarioKnowledge::Unknown || candidate.pathCost < current.pathCost);
		};
		const int maxEntities =
			(std::max)(0, (std::min)(services_->service_globals()->maxEntities, kMaximumEntityIndex));
		const bool seedHostages = scenario.scenario == compat::PublicScenarioKind::Hostage &&
			observedTeam == kTerroristTeam && actors_[index].hostagePositions.nextCheck < 0.0f;
		std::size_t hostageCount = 0;
		const auto &previousScenario = actors_[index].scenarioDecisions;
		if (actors_[index].managedBotStateMachines.state() == compat::CompatibilityStateId::UseEntity &&
			previousScenario.action == compat::ScenarioAction::Use && previousScenario.hasTarget)
			scenario.boundUseEntity = previousScenario.target.entity;
		const bool checkHuman = observedTeam == kCounterTerroristTeam && services_->service_deferToHuman();
		if (scenario.scenario == compat::PublicScenarioKind::Bomb)
			data.looseBomb.frame = context.frame;
		for (int entityIndex = 1; entityIndex < maxEntities; ++entityIndex)
		{
			edict_t *entity = services_->service_engineFunctions()->pfnPEntityOfEntIndex(entityIndex);
			if (entity == nullptr || entity->free != 0)
			{
				continue;
			}
			const char *name = services_->service_engineFunctions()->pfnSzFromIndex(entity->v.classname);
			if (name == nullptr)
			{
				continue;
			}
			if (entityIndex <= kMaximumClientSlots)
			{
				const auto &vipScore =
					services_->observation_publicScoreStatuses(index)[static_cast<std::size_t>(entityIndex - 1)];
				if (vipScore.known && vipScore.serial == static_cast<std::uint32_t>(entity->serialnumber) &&
					vipScore.frame.mapGeneration == context.frame.mapGeneration &&
					vipScore.frame.roundGeneration == context.frame.roundGeneration &&
					(vipScore.flags & kVipScoreFlag) != 0 && entity->v.deadflag == DEAD_NO)
				{
					scenario.vip = project(entity, entityIndex, false);
					scenario.vip.actor = {
						static_cast<std::uint32_t>(entityIndex), static_cast<std::uint32_t>(entity->serialnumber) + 1U};
				}
			}
			if (entityIndex <= services_->service_globals()->maxClients && (entity->v.flags & FL_FAKECLIENT) == 0 &&
				services_->observation_resolvePlayerTeam(entity) == observedTeam && entity->v.deadflag == DEAD_NO &&
				entity->v.health > 0.0f)
			{
				if (checkHuman && !scenario.deferToHuman && entity->v.netname &&
					(entity->v.flags & (FL_CLIENT | FL_DORMANT)) == FL_CLIENT)
				{
					const char *playerName = services_->service_engineFunctions()->pfnSzFromIndex(entity->v.netname);
					scenario.deferToHuman = playerName && *playerName;
				}
				const auto location = project(entity, entityIndex, false);
				if (location.knowledge == compat::ScenarioKnowledge::Visible)
				{
					const float dx = location.position.x - before.origin.x, dy = location.position.y - before.origin.y,
								dz = location.position.z - before.origin.z;
					const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
					if (distance < automaticLeaderRange)
					{
						automaticLeaderRange = distance;
						scenario.automaticLeader = {{static_cast<std::uint32_t>(entityIndex),
														static_cast<std::uint32_t>(entity->serialnumber) + 1U},
							scenario.team, true, true, true, location};
						scenario.automaticLeader.location.actor = scenario.automaticLeader.actor;
					}
				}
			}
			if (scenario.scenario == compat::PublicScenarioKind::Bomb && entity->serialnumber >= 0 &&
				entity->v.model && std::strcmp(name, "weaponbox") == 0 && ObservationAdapter::isLooseBombBox(
					entity, name, services_->service_engineFunctions()->pfnSzFromIndex(entity->v.model)))
			{
				const auto serial = entity->serialnumber;
				const auto position = entity->v.origin;
				const bool visible = canSeeEntityOrigin(index, entity, context.frame);
				if (!entity->free && entity->serialnumber == serial)
				{
					auto &bomb = data.looseBomb;
					data.looseBombOrigin = {position.x, position.y, position.z};
					bomb.entity = {static_cast<unsigned>(entityIndex), static_cast<unsigned>(serial) + 1U};
					bomb.knowledge = visible ? compat::ScenarioKnowledge::Visible : compat::ScenarioKnowledge::Unknown;
					if (visible)
						bomb.position = {position.x, position.y, position.z};
				}
			}
			if (std::strcmp(name, "grenade") == 0)
			{
				compat::ObjectiveObservation planted{};
				world::WorldVector knownPosition{};
				bool bombVisible = false;
				if (services_->observation_observationAdapter().collectPlantedBomb(entity, name,
						services_->service_engineFunctions()->pfnSzFromIndex(entity->v.model), context.timestamp,
						context.actor, context.frame, data.timing, &planted) == ObservationAdapterResult::Accepted &&
					services_->observation_observeObjective(index, entity, true, &knownPosition, &bombVisible))
				{
					scenario.bombPlantedKnown = true;
					scenario.bombPlanted = true;
					scenario.bomb.knowledge =
						bombVisible ? compat::ScenarioKnowledge::Visible : compat::ScenarioKnowledge::Remembered;
					scenario.bomb.frame = context.frame;
					scenario.bomb.entity = {
						static_cast<std::uint32_t>(entityIndex), static_cast<std::uint32_t>(entity->serialnumber) + 1U};
					scenario.bomb.position = {knownPosition.x, knownPosition.y, knownPosition.z};
					const float dx = knownPosition.x - before.origin.x, dy = knownPosition.y - before.origin.y,
								dz = knownPosition.z - before.origin.z;
					scenario.bomb.pathCost = std::sqrt(dx * dx + dy * dy + dz * dz);
					scenario.bomb.inUseRange = scenario.bomb.pathCost <= kEntityUseRange;
					scenario.humanDefusingKnown = true;
					scenario.humanDefusing = services_->observation_externalDefuserEntityIndex() != 0U &&
						services_->observation_externalDefuserLeaseUntil() >= context.timestamp &&
						services_->observation_externalDefuserMapGeneration() == context.frame.mapGeneration &&
						services_->observation_externalDefuserRoundGeneration() == context.frame.roundGeneration;
				}
			}
			compat::ScenarioTarget *destination = nullptr;
			bool mapKnown = true;
			if (std::strcmp(name, "hostage_entity") == 0)
			{
				if (seedHostages && hostageCount < data.initialHostages.entries.size() &&
					entity->serialnumber >= 0 && entity->v.takedamage == DAMAGE_YES)
				{
					const auto &position = entity->v.origin;
					data.initialHostages.entries[hostageCount++] = {
						{static_cast<unsigned>(entityIndex), static_cast<unsigned>(entity->serialnumber) + 1U},
						{position.x, position.y, position.z}, true, true};
				}
				if (entity->v.health > 0.0f)
				{
					if (scenario.boundUseEntity.isValid())
					{
						if (scenario.boundUseEntity.id == static_cast<std::uint32_t>(entityIndex) &&
							scenario.boundUseEntity.generation == static_cast<std::uint32_t>(entity->serialnumber) + 1U)
							scenario.hostage = project(entity, entityIndex, true);
						continue;
					}
					destination = &scenario.hostage;
					mapKnown = false;
				}
			}
			else if (std::strcmp(name, "func_hostage_rescue") == 0 || std::strcmp(name, "info_hostage_rescue") == 0)
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
				if (other == nullptr || other == services_->service_managedBotHandles()[index].entity ||
					other->free != 0 || services_->observation_resolvePlayerTeam(other) != kCounterTerroristTeam ||
					other->v.deadflag != DEAD_NO || other->v.health <= 0.0f)
					continue;
				const float dx = other->v.origin.x - scenario.hostage.position.x;
				const float dy = other->v.origin.y - scenario.hostage.position.y;
				const float dz = other->v.origin.z - scenario.hostage.position.z;
				if (dx * dx + dy * dy + dz * dz <= kHostageCompetitorRange * kHostageCompetitorRange)
				{
					scenario.hostageCompetitorNearby = true;
					break;
				}
			}
		}
		const auto tracked = actors_[index].scenarioPlanners.followLeader();
		const auto teammate = actors_[index].teammateFollow;
		if (actors_[index].scenarioPlanners.isFollowing() && teammate.entity.isValid() && teammate.entity == tracked)
		{
			const auto slot = tracked.slot;
			if (slot <= services_->service_managedBotHandles().size() &&
				slot <= static_cast<std::uint32_t>(services_->service_globals()->maxClients))
			{
				const auto peerIndex = static_cast<std::size_t>(slot - 1);
				const auto leaderCurrent = [&]()
				{
					const auto &handle = services_->service_managedBotHandles()[peerIndex];
					return services_->service_managedBotSlots()[peerIndex] &&
						teammate.actor == world::ActorKey{handle.actor.slot, handle.actor.actorGeneration} &&
						receiptConnectionCurrent({slot, teammate.connection}) && handle.entity &&
						!handle.entity->free && handle.entity->serialnumber >= 0 &&
						static_cast<std::uint32_t>(handle.entity->serialnumber) + 1U == tracked.generation &&
						(handle.entity->v.flags & (FL_CLIENT | FL_DORMANT)) == FL_CLIENT;
				};
				if (leaderCurrent())
				{
					auto *leader = services_->service_managedBotHandles()[peerIndex].entity;
					const auto team = services_->observation_resolvePlayerTeam(leader);
					if (leaderCurrent() && services_->service_managedBotHandles()[peerIndex].entity == leader)
					{
						scenario.teammateLeader = {tracked,
							team == kTerroristTeam				? objectives::TeamRole::Terrorist
								: team == kCounterTerroristTeam ? objectives::TeamRole::CounterTerrorist
																: objectives::TeamRole::Unknown,
							false, true, leader->v.deadflag == DEAD_NO && leader->v.health > 0.0f,
							project(leader, static_cast<int>(slot), true)};
						scenario.teammateLeader.location.actor = tracked;
					}
				}
			}
		}
		else if (actors_[index].scenarioPlanners.isFollowing() && tracked.isValid() &&
			tracked.slot <= static_cast<std::uint32_t>(services_->service_globals()->maxClients))
		{
			edict_t *human = services_->service_engineFunctions()->pfnPEntityOfEntIndex(static_cast<int>(tracked.slot));
			if (human != nullptr && !human->free &&
				static_cast<std::uint32_t>(human->serialnumber) + 1U == tracked.generation)
			{
				scenario.automaticLeader = {tracked,
					services_->observation_resolvePlayerTeam(human) == kTerroristTeam ? objectives::TeamRole::Terrorist
						: services_->observation_resolvePlayerTeam(human) == kCounterTerroristTeam
						? objectives::TeamRole::CounterTerrorist
						: objectives::TeamRole::Unknown,
					(human->v.flags & FL_FAKECLIENT) == 0, true, human->v.deadflag == DEAD_NO && human->v.health > 0.0f,
					project(human, static_cast<int>(tracked.slot), false)};
				scenario.automaticLeader.location.actor = tracked;
			}
		}
		const auto sender = services_->observation_scenarioRadio(index).sender;
		if (sender.isValid() && sender.slot <= static_cast<std::uint32_t>(services_->service_globals()->maxClients))
		{
			edict_t *leader = services_->service_engineFunctions()->pfnPEntityOfEntIndex(static_cast<int>(sender.slot));
			if (leader != nullptr && leader->free == 0 &&
				static_cast<std::uint32_t>(leader->serialnumber) == sender.generation)
			{
				scenario.leader.actor = sender;
				scenario.leader.team = services_->observation_resolvePlayerTeam(leader) == kTerroristTeam
					? objectives::TeamRole::Terrorist
					: services_->observation_resolvePlayerTeam(leader) == kCounterTerroristTeam
					? objectives::TeamRole::CounterTerrorist
					: objectives::TeamRole::Unknown;
				scenario.leader.human = (leader->v.flags & FL_FAKECLIENT) == 0;
				scenario.leader.lifeKnown = true;
				scenario.leader.alive = leader->v.deadflag == DEAD_NO && leader->v.health > 0.0f;
				scenario.leader.location = project(leader, static_cast<int>(sender.slot), false);
				scenario.leader.location.actor = sender;
				if (scenario.leader.location.knowledge == compat::ScenarioKnowledge::Visible)
				{
					services_->observation_observeRadioLeaderPosition(index, scenario.leader.location);
				}
			}
		}
	}
}

void BotBehaviorRuntime::updateHostagePositions(std::size_t index, const compat::StateUpdateContext &context,
	const HostagePositionMemory &initial)
{
	auto &actor = actors_[index];
	const auto engine = services_->service_engineFunctions();
	if (!context.alive || allHostagesRescued(context.frame) || !engine || !engine->pfnPEntityOfEntIndex ||
		!engine->pfnTraceLine ||
		!(context.timestamp > actor.hostagePositions.nextCheck))
		return;

	const auto receipt = reactionContext(index);
	const auto revision = actor.executionRevision;
	const auto epoch = actor.managedBotStateMachines.normalStateEpoch();
	const auto current = [&] {
		return executionCurrent(index, receipt, revision) && services_->service_engineFunctions() == engine &&
			actor.managedBotStateMachines.normalStateEpoch() == epoch;
	};
	auto memory = actor.hostagePositions.nextCheck < 0.0f ? initial : actor.hostagePositions;
	const auto countValid = [&] {
		return std::count_if(memory.entries.begin(), memory.entries.end(),
			[](const RememberedHostage &entry) { return entry.valid; });
	};
	const auto previousCount = countValid();
	bool missing = false;
	for (auto &entry : memory.entries)
	{
		if (!entry.entity.isValid())
			break;
		const auto *entity = engine->pfnPEntityOfEntIndex(static_cast<int>(entry.entity.id));
		if (!current())
			return;
		const auto sameHostage = [&] {
			return entity && !entity->free && entity->serialnumber >= 0 &&
				static_cast<unsigned>(entity->serialnumber) + 1U == entry.entity.generation;
		};
		if (!sameHostage())
			continue;
		const world::WorldPosition position{entity->v.origin.x, entity->v.origin.y, entity->v.origin.z};
		if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z))
			continue;
		const bool visible = canSeePoint(index, position, context.frame, true);
		if (!current())
			return;
		if (!sameHostage())
			continue;
		const bool oldPositionVisible = !visible && entry.valid && canSeePoint(index, entry.position, context.frame, true);
		if (!current())
			return;
		if (!sameHostage())
			continue;
		const bool alive = entity->v.takedamage == DAMAGE_YES;
		if (!alive && (visible || oldPositionVisible))
			entry.alive = false;
		if (visible)
		{
			entry.valid = alive;
			if (alive)
				entry.position = position;
		}
		else if (oldPositionVisible)
		{
			const auto dx = position.x - entry.position.x;
			const auto dy = position.y - entry.position.y;
			const auto dz = position.z - entry.position.z;
			constexpr float kMovedTolerance = 50.0f;
			const bool displaced = dx * dx + dy * dy + dz * dz > kMovedTolerance * kMovedTolerance;
			entry.valid = alive && !displaced;
			missing = missing || (alive && displaced);
		}
	}
	memory.nextCheck = context.timestamp + 0.5f;
	actor.hostagePositions = memory;
	const bool allGone = previousCount > 0 && countValid() == 0;
	if (allGone || missing)
		reactToHostageLoss(index, allGone);
}

void BotBehaviorRuntime::updateScenarioNavigation(std::size_t index, const runtime::MovementPhysicsState &before,
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
	auto &knownSite = actors_[index].plantedBombsite;
	if (scenario.bombPlantedKnown && scenario.bombPlanted && knownSite.entity.isValid() &&
		knownSite.frame.mapGeneration == context.frame.mapGeneration &&
		knownSite.frame.roundGeneration == context.frame.roundGeneration &&
		scenario.bomb.knowledge == compat::ScenarioKnowledge::Unknown)
	{
		const auto engine = services_->service_engineFunctions();
		const auto globals = services_->service_globals();
		const auto *zone = engine && engine->pfnPEntityOfEntIndex && globals &&
			knownSite.entity.id < static_cast<unsigned>(globals->maxEntities)
			? engine->pfnPEntityOfEntIndex(static_cast<int>(knownSite.entity.id)) : nullptr;
		if (zone && !zone->free && zone->serialnumber >= 0 &&
			static_cast<unsigned>(zone->serialnumber) + 1U == knownSite.entity.generation)
		{
			scenario.plantedBombsite = knownSite;
			const float dx = knownSite.position.x - before.origin.x, dy = knownSite.position.y - before.origin.y,
				dz = knownSite.position.z - before.origin.z;
			scenario.plantedBombsite.pathCost = std::sqrt(dx * dx + dy * dy + dz * dz);
		}
		else
			knownSite = {};
	}
	if (scenario.scenario == compat::PublicScenarioKind::Bomb &&
		scenario.plantedBombsite.knowledge == compat::ScenarioKnowledge::Unknown &&
		scenario.team == objectives::TeamRole::CounterTerrorist && scenario.bombPlantedKnown && scenario.bombPlanted &&
		scenario.bomb.knowledge == compat::ScenarioKnowledge::Unknown && scenarioNavigation.isValid() &&
		scenarioNavigation.mapGeneration() == context.frame.mapGeneration && scenarioDocument != nullptr &&
		services_->service_globals() != nullptr && services_->service_engineFunctions() != nullptr &&
		services_->service_engineFunctions()->pfnPEntityOfEntIndex != nullptr &&
		services_->service_engineFunctions()->pfnSzFromIndex != nullptr)
	{
		compat::ScenarioTarget &search = actors_[index].bombSearchTargets;
		auto &actor = actors_[index];
		if (actor.managedBotStateMachines.state() == compat::CompatibilityStateId::MoveTo &&
			actor.managedBotStateMachines.task() == compat::CompatibilityTaskId::FindTickingBomb &&
			!actor.managedBotStateMachines.attackOverlayActive())
		{
			const auto *site = actor.bombSearchMemory.enteredUnclearedSite(context.frame,
				{before.origin.x, before.origin.y, before.origin.z});
			if (site)
			{
				actor.clearedBombsite.frame = context.frame;
				actor.clearedBombsite.entity = site->entity;
				actor.clearedBombsite.position = {site->center.x, site->center.y, site->center.z};
				actor.bombSearchMemory.clear(context.frame, site->entity);
				search = {};
				return;
			}
		}
		if (search.knowledge == compat::ScenarioKnowledge::Unknown)
		{
			std::vector<compat::BombSearchSite> sites;
			nav::NavQuery query(scenarioNavigation);
			nav::NavAreaMatch start{};
			const nav::NavVector self = {before.origin.x, before.origin.y, before.origin.z};
			const bool haveStart =
				query.findNearest3D(self, kMaximumObjectiveDistance, &start) == nav::NavQueryResult::Found;
			const int limit = (std::min)(services_->service_globals()->maxEntities, kMaximumEntityIndex);
			for (int entityIndex = 1; entityIndex < limit; ++entityIndex)
			{
				edict_t *entity = services_->service_engineFunctions()->pfnPEntityOfEntIndex(entityIndex);
				if (entity == nullptr || entity->free)
					continue;
				const char *name = services_->service_engineFunctions()->pfnSzFromIndex(entity->v.classname);
				if (name == nullptr ||
					(std::strcmp(name, "func_bomb_target") != 0 && std::strcmp(name, "info_bomb_target") != 0))
					continue;
				auto site = publicBombSite(*entity, entityIndex, std::strcmp(name, "info_bomb_target") == 0);
				nav::NavAreaMatch finish{};
				nav::NavCorridor route{};
				if (haveStart &&
					query.findNearest3D(site.center, kMaximumObjectiveDistance, &finish) ==
						nav::NavQueryResult::Found &&
					query.buildCorridor(start.area, finish.area, nav::NavRouteType::Fastest, &route) ==
						nav::NavQueryResult::Found)
					site.travel = route.cost;
				sites.push_back(site);
			}
			const compat::RandomActor randomActor = {context.actor.slot, context.actor.generation};
			const compat::RandomTimingContext randomTiming = {services_->command_managedBotCommandSequences(index), 0U,
				services_->service_managedBotFullUpdateSequences()[index]};
			std::size_t selected = 0U;
			if (actors_[index].bombSearchMemory.choose(context.frame, sites,
					services_->service_compatibilityRandomSource(), randomActor, randomTiming, &selected))
			{
				nav::NavVector point{};
				nav::NavAreaMatch chosenArea{};
				nav::NavCorridor chosenRoute{};
				if (compat::bombSitePoint(*scenarioDocument, sites[selected], compat::BombSearchSite::kMaximumNavAreas,
						services_->service_compatibilityRandomSource(), randomActor, randomTiming, &point) &&
					haveStart &&
					query.findNearest3D(point, kMaximumObjectiveDistance, &chosenArea) == nav::NavQueryResult::Found &&
					query.buildCorridor(start.area, chosenArea.area, nav::NavRouteType::Fastest, &chosenRoute) ==
						nav::NavQueryResult::Found)
				{
					search.knowledge = compat::ScenarioKnowledge::Remembered;
					search.frame = context.frame;
					search.entity = sites[selected].entity;
					search.position = {point.x, point.y, point.z};
					search.pathCost = chosenRoute.cost;
				}
				else
					actors_[index].bombSearchMemory.clear(context.frame, sites[selected].entity);
			}
		}
		if (search.knowledge != compat::ScenarioKnowledge::Unknown)
		{
			search.frame = context.frame;
			scenario.bombSearch = search;
		}
	}
	if (scenario.scenario == compat::PublicScenarioKind::Bomb && scenario.bombPlantedKnown && scenario.bombPlanted &&
		scenario.bomb.knowledge != compat::ScenarioKnowledge::Unknown && scenarioNavigation.isValid() &&
		scenarioNavigation.mapGeneration() == context.frame.mapGeneration && scenarioDocument != nullptr)
	{
		const nav::NavVector self = {before.origin.x, before.origin.y, before.origin.z};
		const nav::NavVector bomb = {scenario.bomb.position.x, scenario.bomb.position.y, scenario.bomb.position.z};
		scenario.travelTimeEstimated = scenario.team == objectives::TeamRole::CounterTerrorist &&
			!scenario.alreadyEscapingFromBomb && !scenario.currentDefusing &&
			compat::estimateBombTravel(scenarioNavigation, self, bomb,
				services_->service_managedBotHandles()[index].entity->v.maxspeed, kMaximumObjectiveDistance,
				&scenario.travelSecondsToBomb);
		const bool lateDefuse = scenario.team == objectives::TeamRole::CounterTerrorist && scenario.timerEstimated &&
			scenario.travelTimeEstimated && scenario.defuseKitKnown &&
			scenario.bombSecondsRemaining <=
				scenario.travelSecondsToBomb + (scenario.defuseKit ? kDefuseWithKitSeconds : kDefuseWithoutKitSeconds);
		const bool requestedEscape = compat::ScenarioBehaviorPlanner::autoEscapeRequested(scenario) ||
			(lateDefuse && !scenario.attacking && !scenario.currentDefusing) || scenario.alreadyEscapingFromBomb ||
			(scenario.radio != nullptr && scenario.radio->command == compat::ScenarioRadioCommand::GetOut);
		if (requestedEscape)
		{
			nav::AreaId &goal = actors_[index].scenarioEscapeAreas;
			const nav::NavArea *area = scenarioDocument->findArea(goal);
			if (area == nullptr)
			{
				const compat::RandomActor randomActor = {context.actor.slot, context.actor.generation};
				const compat::RandomTimingContext randomTiming = {services_->command_managedBotCommandSequences(index),
					0U, services_->service_managedBotFullUpdateSequences()[index]};
				if (compat::selectBombEscapeArea(*scenarioDocument, bomb,
						services_->service_compatibilityRandomSource(), randomActor, randomTiming, &goal))
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
	const auto validateRadioSender = [&](world::ActorKey sender) -> compat::ScenarioSenderValidation
	{
		compat::ScenarioSenderValidation validation{sender, false, false};
		if (!sender.isValid() || services_->service_globals() == nullptr ||
			services_->service_engineFunctions() == nullptr ||
			services_->service_engineFunctions()->pfnPEntityOfEntIndex == nullptr)
			return validation;
		validation.known = true;
		if (sender.slot > static_cast<std::uint32_t>(services_->service_globals()->maxClients))
			return validation;
		edict_t *human = services_->service_engineFunctions()->pfnPEntityOfEntIndex(static_cast<int>(sender.slot));
		validation.valid = (observedTeam == kTerroristTeam || observedTeam == kCounterTerroristTeam) &&
			human != nullptr && !human->free && (human->v.flags & FL_CLIENT) != 0 &&
			(human->v.flags & FL_FAKECLIENT) == 0 && services_->observation_resolvePlayerTeam(human) == observedTeam &&
			static_cast<std::uint32_t>(human->serialnumber) == sender.generation;
		return validation;
	};
	if (scenario.radio != nullptr)
		scenario.radioSenderValidation = validateRadioSender(scenario.radio->sender);
	scenario.pendingRadioSenderValidation = validateRadioSender(actors_[index].scenarioPlanners.pendingRadioSender());
}

void BotBehaviorRuntime::updateScenarioTeamwork(std::size_t index, const runtime::MovementPhysicsState &before,
	compat::StateUpdateContext &context, compat::StateBehaviorInput &stateBehavior,
	const compat::CompatibilityObservation &observation, compat::ScenarioBehaviorInput &scenario,
	ScenarioUpdateData &data)
{
	const auto receipt = reactionContext(index);
	const auto revision = actors_[index].executionRevision;
	const auto epoch = actors_[index].managedBotStateMachines.normalStateEpoch();
	const auto actorCurrent = [&] { return executionCurrent(index, receipt, revision); };
	const auto current = [&]
	{
		return actorCurrent() && actors_[index].managedBotStateMachines.normalStateEpoch() == epoch;
	};
	(void)stateBehavior;
	(void)observation;
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
	teamwork.teamwork = static_cast<float>(actors_[index].managedProfiles.teamwork) / kProfilePercentScale;
	teamwork.roundTimeKnown =
		services_->service_roundFreezeGeneration() == services_->service_lifecycle().roundGeneration() &&
		std::isfinite(services_->service_roundFreezeStartTime()) &&
		context.timestamp >= services_->service_roundFreezeStartTime();
	teamwork.elapsedRound = context.timestamp - services_->service_roundFreezeStartTime();
	teamwork.allowRoguesKnown = services_->service_engineFunctions() != nullptr &&
		services_->service_engineFunctions()->pfnCVarGetFloat != nullptr &&
		services_->service_engineFunctions()->pfnCVarGetPointer != nullptr &&
		services_->service_engineFunctions()->pfnCVarGetPointer("bot_allow_rogues") != nullptr;
	if (!current())
		return;
	const float allowRogues = readOptionalCvarFloat(services_->service_engineFunctions(), "bot_allow_rogues");
	if (!current())
		return;
	teamwork.allowRoguesKnown = teamwork.allowRoguesKnown && std::isfinite(allowRogues);
	teamwork.allowRogues = allowRogues != 0.0f;
	const auto currentState = machine.state();
	scenario.busy = compat::nativeBotBusy(currentState, machine.task(), scenario.attacking);
	teamwork.busy = scenario.busy;
	teamwork.following = wasFollowing;
	teamwork.followDuration = wasFollowing ? context.timestamp - actors_[index].scenarioPlanners.followSince() : 0.0f;
	teamwork.candidate = scenario.automaticLeader.actor;
	teamwork.candidateVisibleHumanFriend = scenario.automaticLeader.human && scenario.automaticLeader.lifeKnown &&
		scenario.automaticLeader.alive && scenario.automaticLeader.team == scenario.team &&
		scenario.automaticLeader.location.knowledge == compat::ScenarioKnowledge::Visible;
	teamwork.candidateRange = automaticLeaderRange;
	if (teamwork.candidateVisibleHumanFriend && scenarioNavigation.isValid() && scenarioDocument != nullptr &&
		scenarioNavigation.mapGeneration() == context.frame.mapGeneration)
	{
		nav::NavQuery query(scenarioNavigation);
		nav::NavAreaMatch selfArea{}, humanArea{};
		nav::NavCorridor route{};
		const auto &position = scenario.automaticLeader.location.position;
		if (query.findNearest3D({before.origin.x, before.origin.y, before.origin.z}, kMaximumObjectiveDistance,
				&selfArea) == nav::NavQueryResult::Found &&
			query.findContaining({position.x, position.y, position.z}, kHumanNavTolerance, &humanArea) ==
				nav::NavQueryResult::Found &&
			query.buildCorridor(selfArea.area, humanArea.area, nav::NavRouteType::Fastest, &route) ==
				nav::NavQueryResult::Found)
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
		services_->service_engineFunctions()->pfnSzFromIndex != nullptr && services_->service_globals() != nullptr)
	{
		float closestSiteDistanceSquared = 1.0e9f;
		const auto &previous = actors_[index].plantedBombsite;
		const auto &observationMemory = services_->observation_objectiveKnowledge(index)[1U];
		bool reportDiscovery = scenario.team == objectives::TeamRole::CounterTerrorist &&
			(!previous.entity.isValid() || previous.frame.mapGeneration != context.frame.mapGeneration ||
				previous.frame.roundGeneration != context.frame.roundGeneration) &&
			scenario.bomb.entity.isValid() && observationMemory.entity.id == scenario.bomb.entity.id &&
			observationMemory.entity.generation + 1U == scenario.bomb.entity.generation &&
			observationMemory.firstHeardAt >= 0.0f;
		for (int entityIndex = 1;
			 entityIndex < (std::min)(services_->service_globals()->maxEntities, kMaximumEntityIndex); ++entityIndex)
		{
			// The identified C4 cannot also be a zone. Once the containment check is
			// finished, only additional zone centers could improve the closest match.
			if (teamwork.atPlantedBombsite && static_cast<unsigned>(entityIndex) == scenario.bomb.entity.id)
				continue;
			edict_t *zone = services_->service_engineFunctions()->pfnPEntityOfEntIndex(entityIndex);
			if (zone == nullptr || zone->free)
				continue;
			const char *name = services_->service_engineFunctions()->pfnSzFromIndex(zone->v.classname);
			if (name == nullptr ||
				(std::strcmp(name, "func_bomb_target") != 0 && std::strcmp(name, "info_bomb_target") != 0))
				continue;
			const bool legacy = std::strcmp(name, "info_bomb_target") == 0;
			const float loX = legacy ? zone->v.origin.x - kLegacyZoneHalfExtent : zone->v.absmin.x;
			const float loY = legacy ? zone->v.origin.y - kLegacyZoneHalfExtent : zone->v.absmin.y;
			const float loZ =
				(legacy ? zone->v.origin.z - kLegacyZoneHalfExtent : zone->v.absmin.z) - kZoneVerticalPadding;
			const float hiX = legacy ? zone->v.origin.x + kLegacyZoneHalfExtent : zone->v.absmax.x;
			const float hiY = legacy ? zone->v.origin.y + kLegacyZoneHalfExtent : zone->v.absmax.y;
			const float hiZ =
				(legacy ? zone->v.origin.z + kLegacyZoneHalfExtent : zone->v.absmax.z) + kZoneVerticalPadding;
			const world::WorldPosition center{(loX + hiX) * 0.5f, (loY + hiY) * 0.5f, (loZ + hiZ) * 0.5f};
			const auto &bomb = scenario.bomb.position;
			const float dx = center.x - bomb.x, dy = center.y - bomb.y, dz = center.z - bomb.z;
			const float distanceSquared = dx * dx + dy * dy + dz * dz;
			if (zone->serialnumber >= 0 && distanceSquared < closestSiteDistanceSquared)
			{
				closestSiteDistanceSquared = distanceSquared;
				auto &known = actors_[index].plantedBombsite;
				known.knowledge = compat::ScenarioKnowledge::Remembered;
				known.frame = context.frame;
				known.entity = {static_cast<unsigned>(entityIndex), static_cast<unsigned>(zone->serialnumber) + 1U};
				known.position = center;
			}
			const auto contains = [&](float x, float y, float z)
			{
				return x >= loX && x <= hiX && y >= loY && y <= hiY && z >= loZ && z <= hiZ;
			};
			if (reportDiscovery && zone->serialnumber >= 0 && contains(bomb.x, bomb.y, bomb.z))
			{
				auto &report = actors_[index].discoveredBombsiteReport;
				report = compat::plantedBombsiteReport(observationMemory.firstHeardAt,
					{static_cast<unsigned>(entityIndex), static_cast<unsigned>(zone->serialnumber) + 1U}, 0, 0);
				report.meme.position = {center.x, center.y, center.z};
				reportDiscovery = false;
			}
			if (contains(before.origin.x, before.origin.y, before.origin.z) &&
				contains(scenario.bomb.position.x, scenario.bomb.position.y, scenario.bomb.position.z))
				teamwork.atPlantedBombsite = true;
		}
	}
	compat::TeamworkDecision cooperative{};
	if (scenario.lifeKnown && scenario.alive)
		cooperative = actors_[index].teamworkPolicies.update(teamwork, services_->service_compatibilityRandomSource(),
			{context.actor.slot, context.actor.generation},
			{services_->command_managedBotCommandSequences(index), 0U,
				services_->service_managedBotFullUpdateSequences()[index]},
			current);
	if (!current())
		return;
	scenario.rogue = cooperative.rogue;
	scenario.automaticFollowRequested = cooperative.available && cooperative.startFollowing;
	scenario.stopFollowingRequested = cooperative.available && cooperative.stopFollowing;
	scenario.autoFollowPermissionInferred = cooperative.humanPermissionInferred;
	if (!cooperative.rogueKnown && !wasFollowing)
		scenario.radio = nullptr;
	bool finishedDefense = false;
	const bool guardingLoose = machine.task() == compat::CompatibilityTaskId::GuardLooseBomb;
	const bool guardingSite = machine.task() == compat::CompatibilityTaskId::GuardBombZone;
	const bool guardingHostages = machine.task() == compat::CompatibilityTaskId::GuardHostages;
	const bool guardingRescue = machine.task() == compat::CompatibilityTaskId::GuardHostageRescueZone;
	const bool sniping = machine.task() == compat::CompatibilityTaskId::MoveToSniperSpot ||
		machine.task() == compat::CompatibilityTaskId::Sniping;
	const bool retreating = actors_[index].combatRetreat.hideEpoch == machine.normalStateEpoch() &&
		actors_[index].combatRetreat.hideEpoch != 0;
	if (machine.state() == compat::CompatibilityStateId::Hide &&
		(retreating || guardingLoose || guardingSite || guardingHostages || guardingRescue || sniping ||
			machine.task() == compat::CompatibilityTaskId::HoldPosition ||
			machine.task() == compat::CompatibilityTaskId::SeekAndDestroy) &&
		!machine.attackOverlayActive())
	{
		const float feetZ = before.origin.z - (before.ducked ? 18.0f : 36.0f);
		auto &actor = actors_[index];
		const bool wasAtDefenseSpot = actor.scenarioPlanners.atDefenseSpot();
		actor.scenarioPlanners.observeDefenderFeet(before.origin.x, before.origin.y, feetZ);
		if (retreating && !wasAtDefenseSpot && actors_[index].scenarioPlanners.atDefenseSpot())
			actors_[index].enemyReaction.cancelIgnore();
		const auto &reload = observation.weapon.reloadState;
		const bool reloading = reload.isAvailable() && reload.value != combat::ReloadState::Unknown
			? reload.value == combat::ReloadState::Reloading
			: services_->observation_publicObservations().pendingReloadRequest(context.actor, context.frame, context.timestamp);
		const float elapsed = context.timestamp - machine.stateTimestamp();
		if (guardingLoose && actors_[index].scenarioPlanners.defenseExpired(elapsed, reloading))
		{
			hideNearLooseBomb(index, false);
			return;
		}
		if (guardingSite && actors_[index].scenarioPlanners.defenseExpired(elapsed, reloading) &&
			(renewBombsiteGuard(index) || !current()))
			return;
		if (guardingRescue && actors_[index].scenarioPlanners.defenseExpired(elapsed, reloading) &&
			(renewHostageRescueGuard(index) || !current()))
			return;
		finishedDefense = actors_[index].scenarioPlanners.finishDefense(elapsed, reloading);
		if (guardingHostages && !reloading &&
			(allHostagesRescued(context.frame) || !actors_[index].hostagePositions.hasKnownPosition()))
		{
			actors_[index].scenarioPlanners.cancelHoldPosition();
			finishedDefense = true;
		}
		if (!finishedDefense && wasAtDefenseSpot && actor.enemyReaction.recentlyDamaged(receipt, 1.0f))
		{
			const auto &belief = services_->observation_managedBotPerception(index).snapshot();
			const world::ActorObservation *enemy = nullptr;
			if (belief.findActor(actor.enemyReaction.enemy(receipt), &enemy) != world::ContactLookupResult::Found ||
				!enemy || !enemy->visible)
			{
				actor.scenarioPlanners.cancelHoldPosition();
				actor.scenarioDecisions = {};
				actor.scenarioDecisions.action = compat::ScenarioAction::Stop;
				actor.scenarioDecisions.state = compat::CompatibilityStateId::Idle;
				return;
			}
		}
		if (!finishedDefense && guardingSite && observedTeam == kCounterTerroristTeam && scenario.deferToHuman &&
			scenario.bombPlanted && scenario.timerEstimated && actors_[index].scenarioPlanners.atDefenseSpot())
		{
			encourageHumanDefuse(index, scenario.bombSecondsRemaining, current);
			if (!current())
				return;
		}
		if (!finishedDefense && sniping && actors_[index].scenarioPlanners.atDefenseSpot())
			machine.reachedSniperSpot();
		const bool knownLoose = !scenario.bombPlanted && (data.looseBomb.knowledge == compat::ScenarioKnowledge::Visible ||
			actors_[index].bombStatus.status == compat::BombStatusMemory::Status::Loose);
		if (guardingSite && !reloading && (knownLoose || (scenario.bombPlanted && !scenario.deferToHuman)))
		{
			actors_[index].scenarioPlanners.cancelHoldPosition();
			finishedDefense = true;
		}
		if (guardingLoose && !reloading && data.looseBomb.frame == context.frame && !data.looseBomb.entity.isValid())
		{
			reportBombPickedUp(index);
			if (!current())
				return;
			actors_[index].scenarioPlanners.cancelHoldPosition();
			finishedDefense = true;
		}
	}
	if (finishedDefense || actors_[index].clearedBombsite.frame == context.frame)
	{
		actors_[index].scenarioDecisions = {};
		actors_[index].scenarioDecisions.action = compat::ScenarioAction::Stop;
		actors_[index].scenarioDecisions.state = compat::CompatibilityStateId::Idle;
	}
	else
	{
		compat::ScenarioFallBackResponse fallBack{};
		actors_[index].scenarioDecisions = actors_[index].scenarioPlanners.update(scenario, &fallBack);
		if (fallBack.sender.isValid() && (respondToFallBack(index, observedTeam, fallBack) || !current()))
			return;
		if (actors_[index].scenarioDecisions.startedAutomaticFollow)
		{
			reportAutomaticFollow(index, current);
			if (!current())
				return;
		}
	}
	if (!wasFollowing && !actors_[index].scenarioPlanners.isFollowing() &&
		machine.state() == compat::CompatibilityStateId::Hunt)
	{
		// The hiding operation validates and advances its own state epoch.
		if (!current() || waitAtEndOfSafeTime(index, scenario, actorCurrent) || !current())
			return;
	}
	const auto action = actors_[index].scenarioDecisions.action;
	if (machine.state() == compat::CompatibilityStateId::Idle && !machine.attackOverlayActive() &&
		action == compat::ScenarioAction::None && scenario.scenario == compat::PublicScenarioKind::Bomb &&
		scenario.team == objectives::TeamRole::CounterTerrorist && !scenario.bombPlanted && !services_->service_roundOver() &&
		(selectPreplantDefense(index, cooperative.rogueKnown && !cooperative.rogue, scenario) || !current()))
		return;
	if (machine.state() == compat::CompatibilityStateId::Idle && !machine.attackOverlayActive() &&
		action == compat::ScenarioAction::None && scenario.scenario == compat::PublicScenarioKind::Hostage &&
		scenario.team == objectives::TeamRole::Terrorist && !services_->service_roundOver() &&
		!allHostagesRescued(context.frame) && (selectHostageDefense(index, cooperative.rogue, scenario) || !current()))
		return;
	if (actors_[index].scenarioDecisions.hideTask == compat::CompatibilityTaskId::GuardBombZone)
	{
		const auto target = actors_[index].scenarioDecisions.target;
		actors_[index].scenarioDecisions = {};
		const auto state = machine.state();
		if (!machine.attackOverlayActive() && (state == compat::CompatibilityStateId::Idle ||
			state == compat::CompatibilityStateId::Hunt || state == compat::CompatibilityStateId::MoveTo))
			hideAtPlantedBombsite(index, target);
		return;
	}
	if (machine.state() == compat::CompatibilityStateId::Hunt && !machine.attackOverlayActive() &&
		(action == compat::ScenarioAction::None || action == compat::ScenarioAction::Hunt) &&
		scenario.team == objectives::TeamRole::CounterTerrorist && scenario.scenario == compat::PublicScenarioKind::Bomb &&
		cooperative.rogueKnown && !cooperative.rogue && context.timestamp - machine.stateTimestamp() <= 30.0f &&
		data.looseBomb.knowledge == compat::ScenarioKnowledge::Visible)
	{
		hideNearLooseBomb(index, true);
		return;
	}
	if (!guardingSite && !reactionPursuing(index))
	{
		updateBombGuardTarget(index, scenario, &actors_[index].scenarioDecisions);
	}
	if (wasFollowing && !actors_[index].scenarioPlanners.isFollowing())
		actors_[index].teamworkPolicies.followingStopped(context.timestamp);
	if (scenario.radio != nullptr && scenario.radio->sequence > actors_[index].teamworkRadioApplied &&
		scenario.radio->senderHuman && scenario.radio->team == scenario.team && !scenario.busy &&
		(!scenario.rogue || wasFollowing) &&
		context.timestamp - scenario.radio->receivedTime >= kRadioResponseBaseSeconds +
				kRadioReactionScale * scenario.reactionSeconds + (scenario.rogue ? kRogueRadioDelaySeconds : 0.0f))
	{
		const auto &decision = actors_[index].scenarioDecisions;
		const auto command = scenario.radio->command;
		const bool allow = decision.action == compat::ScenarioAction::Follow &&
			actors_[index].scenarioPlanners.followLeader() == scenario.radio->sender;
		const bool inhibit = (command == compat::ScenarioRadioCommand::HoldPosition &&
								 decision.action == compat::ScenarioAction::Guard &&
								 scenario.radio->position.knowledge != compat::ScenarioKnowledge::Unknown) ||
			(command == compat::ScenarioRadioCommand::GoGoGo && decision.action == compat::ScenarioAction::Hunt) ||
			(command == compat::ScenarioRadioCommand::GetOut && decision.action == compat::ScenarioAction::Escape &&
				scenario.bombPlantedKnown && scenario.bombPlanted);
		if ((allow || inhibit) && scenario.radioSenderValidation.known && scenario.radioSenderValidation.valid &&
			scenario.radioSenderValidation.actor == scenario.radio->sender)
		{
			world::ActorKey human = scenario.radio->sender;
			edict_t *entity = services_->service_engineFunctions()->pfnPEntityOfEntIndex(static_cast<int>(human.slot));
			if (entity != nullptr && !entity->free && (entity->v.flags & FL_CLIENT) != 0 &&
				(entity->v.flags & FL_FAKECLIENT) == 0 &&
				services_->observation_resolvePlayerTeam(entity) == observedTeam &&
				static_cast<std::uint32_t>(entity->serialnumber) == human.generation)
			{
				human.generation = static_cast<std::uint32_t>(entity->serialnumber) + 1U;
				actors_[index].teamworkPolicies.observeHumanPolicy(human, context.timestamp, allow);
				actors_[index].teamworkRadioApplied = scenario.radio->sequence;
			}
		}
	}
}

void BotBehaviorRuntime::applyScenarioDecision(std::size_t index, const runtime::MovementPhysicsState &before,
	compat::StateUpdateContext &context, compat::StateBehaviorInput &stateBehavior,
	const compat::CompatibilityObservation &observation, compat::ScenarioBehaviorInput &scenario,
	ScenarioUpdateData &data, PlantMoveChatter *plantMove)
{
	(void)before;
	if (helpGoalCurrent(index) && !actors_[index].managedBotStateMachines.attackOverlayActive())
	{
		auto &actor = actors_[index];
		const auto receipt = reactionContext(index);
		const auto execution = actor.executionRevision;
		const auto current = [&]
		{
			return actor.executionRevision == execution && helpGoalCurrent(index) &&
				compat::sameChatterUpdateReceipt(receipt, reactionContext(index));
		};
		compat::RetainedHelpTaskInput task{};
		task.task = actor.managedBotStateMachines.task();
		task.goal = actor.helpGoal.position;
		task.bombScenario = scenario.scenario == compat::PublicScenarioKind::Bomb;
		task.counterTerrorist = scenario.team == objectives::TeamRole::CounterTerrorist;
		task.bombPlantedKnown = scenario.bombPlantedKnown;
		task.bombPlanted = scenario.bombPlanted;
		task.bombLocated = scenario.bomb.knowledge != compat::ScenarioKnowledge::Unknown;
		task.bombVisible = scenario.bomb.knowledge == compat::ScenarioKnowledge::Visible;
		task.otherDefuser = scenario.humanDefusingKnown && scenario.humanDefusing;
		if (task.task == compat::CompatibilityTaskId::CollectHostages ||
			task.task == compat::CompatibilityTaskId::MoveToLastKnownEnemyPosition)
		{
			const auto key = actor.managedTaskEntity;
			const auto engine = services_->service_engineFunctions();
			if (!key.isValid())
			{
				task.taskLifeKnown = true;
				task.taskAlive = false;
			}
			else if (engine && engine->pfnPEntityOfEntIndex)
			{
				const auto entity = engine->pfnPEntityOfEntIndex(static_cast<int>(key.id));
				if (!current())
					return;
				task.taskLifeKnown = true;
				task.taskAlive = entity && !entity->free && entity->serialnumber >= 0 &&
					static_cast<std::uint32_t>(entity->serialnumber) + 1U == key.generation && entity->v.health > 0 &&
					entity->v.deadflag == DEAD_NO;
				if (task.taskAlive && task.task == compat::CompatibilityTaskId::CollectHostages)
				{
					task.hostagePositionKnown = true;
					task.hostagePosition = {entity->v.origin.x, entity->v.origin.y, entity->v.origin.z};
					// Vanilla public fields do not provide the hostage's authoritative follow leader.
				}
			}
		}
		const auto action = compat::updateRetainedHelpTask(task);
		if (action == compat::RetainedHelpTaskAction::Idle)
		{
			actor.helpGoal = {};
			actor.managedBotObjectiveTargets = {};
			actor.scenarioDecisions = {};
			stateBehavior.goalAvailable = false;
			context.requestTransition = true;
			context.requestedState = compat::CompatibilityStateId::Idle;
			context.transitionId = compat::StateTransitionId::ExplicitStateChange;
			context.transitionReason = compat::StateTransitionReason::ObjectiveChanged;
			return;
		}
		if (action == compat::RetainedHelpTaskAction::Repath)
		{
			actor.helpGoal.position = task.hostagePosition;
			actor.managedBotObjectiveTargets.target = task.hostagePosition;
			if (++actor.managedBotObjectiveTargets.generation == 0)
				++actor.managedBotObjectiveTargets.generation;
			services_->applyNavigationRequest({index, NavigationRequestKind::DestroyPath});
			if (!current())
				return;
			services_->applyNavigationRequest({index, NavigationRequestKind::ClearStep});
			if (!current())
				return;
		}
	}
	if (reactionPursuing(index) || helpGoalCurrent(index))
	{
		const auto &order = services_->observation_scenarioRadio(index);
		const auto &decision = actors_[index].scenarioDecisions;
		const bool movementOrder = (order.command == compat::ScenarioRadioCommand::HoldPosition &&
			decision.action == compat::ScenarioAction::Guard) ||
			(order.command == compat::ScenarioRadioCommand::GoGoGo && decision.action == compat::ScenarioAction::Hunt) ||
			(order.command == compat::ScenarioRadioCommand::GetOut && decision.action == compat::ScenarioAction::Escape) ||
			(decision.action == compat::ScenarioAction::Follow && actors_[index].scenarioPlanners.followLeader() == order.sender);
		if (order.sequence == 0 || order.receivedTime <= actors_[index].managedBotStateMachines.stateTimestamp() || !movementOrder)
		{
			actors_[index].scenarioDecisions = {};
			return;
		}
		actors_[index].helpGoal = {};
	}
	auto &machine = actors_[index].managedBotStateMachines;
	const compat::ScenarioBehaviorDecision &scenarioDecision = actors_[index].scenarioDecisions;
	const bool carryingC4 = observation.objective.carryingC4.isAvailable() && observation.objective.carryingC4.value;
	context.moveToTask = compat::nativeScenarioMoveTask(scenario, scenarioDecision, carryingC4);
	stateBehavior.goalAvailable = scenarioDecision.hasTarget || stateBehavior.goalAvailable;
	stateBehavior.leaderAvailable = actors_[index].scenarioPlanners.hasFollowingLeader(scenario);
	stateBehavior.objectiveActive = scenarioDecision.hasTarget || stateBehavior.objectiveActive;
	if (machine.state() == compat::CompatibilityStateId::UseEntity && scenario.boundUseEntity.isValid())
		stateBehavior.objectiveActive = scenarioDecision.action == compat::ScenarioAction::Use;
	if (machine.state() == compat::CompatibilityStateId::DefuseBomb ||
		machine.state() == compat::CompatibilityStateId::EscapeFromBomb)
		stateBehavior.objectiveActive = scenario.bombPlantedKnown && scenario.bombPlanted;
	stateBehavior.inInteractionRange = scenarioDecision.hasTarget && scenarioDecision.target.inUseRange;
	stateBehavior.guardObjectiveEnded = machine.state() == compat::CompatibilityStateId::Hide &&
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
		if (scenarioDecision.state == compat::CompatibilityStateId::MoveTo &&
			context.moveToTask != compat::CompatibilityTaskId::None)
			actors_[index].managedTaskEntity = scenarioDecision.target.entity;
		if (machine.state() == compat::CompatibilityStateId::PlantBomb &&
			scenarioDecision.state == compat::CompatibilityStateId::Hide &&
			scenarioDecision.action == compat::ScenarioAction::Guard && scenario.bombPlantedKnown && scenario.bombPlanted &&
			observation.objective.carryingC4.isAvailable() && !carryingC4)
		{
			context.hideTask = compat::CompatibilityTaskId::GuardTickingBomb;
		}
		context.requestedState = scenarioDecision.state;
		context.requestTransition = machine.state() != context.requestedState ||
			(context.requestedState == compat::CompatibilityStateId::MoveTo &&
				context.moveToTask != compat::CompatibilityTaskId::None && machine.task() != context.moveToTask);
		context.transitionId = compat::StateTransitionId::ExplicitStateChange;
		context.transitionReason = compat::StateTransitionReason::ObjectiveChanged;
	}
	const int team = services_->observation_resolvePlayerTeam(services_->service_managedBotHandles()[index].entity);
	if (carryingC4 && team == kTerroristTeam)
	{
		const bool atBombsite = managedActorAtBombSite(index);
		if (plantMove && machine.state() == compat::CompatibilityStateId::MoveTo &&
			machine.task() == compat::CompatibilityTaskId::PlantBomb && !machine.attackOverlayActive())
		{
			// Chatter belongs to this MoveTo update even when it exits to PlantBomb.
			plantMove->epoch = machine.normalStateEpoch();
			plantMove->age = context.timestamp - machine.stateTimestamp();
			plantMove->coverDue =
				atBombsite && services_->navigation_pathDistanceRemaining(index, data.navigation) < 50.0f;
			const auto &target = actors_[index].managedBotObjectiveTargets;
			plantMove->hasGoal = target.valid && target.mapGeneration == context.frame.mapGeneration &&
				target.roundGeneration == context.frame.roundGeneration;
			plantMove->goal = target.target;
			const auto *carrier = services_->service_managedBotHandles()[index].entity;
			if (!carrier)
				return;
			const auto &origin = carrier->v.origin;
			plantMove->reportPosition = {origin.x, origin.y, origin.z};
		}
		const auto &teammate = actors_[index].teammateFollow;
		const bool followingTeammate = teammate.entity.isValid() && stateBehavior.leaderAvailable &&
			teammate.entity == actors_[index].scenarioPlanners.followLeader() &&
			scenarioDecision.action == compat::ScenarioAction::Follow;
		if (followingTeammate && !atBombsite)
			return;
		const bool plantPhase = followingTeammate ||
			(actors_[index].managedBombApproaches.initialized &&
				actors_[index].managedBombApproaches.mapGeneration == context.frame.mapGeneration &&
				actors_[index].managedBombApproaches.roundGeneration == context.frame.roundGeneration &&
				actors_[index].managedBombApproaches.finished);
		stateBehavior.objectiveActive = true;
		stateBehavior.inInteractionRange = plantPhase && atBombsite;
		context.moveToTask = compat::CompatibilityTaskId::PlantBomb;
		context.requestedState = plantPhase && atBombsite ? compat::CompatibilityStateId::PlantBomb
			: plantPhase && stateBehavior.goalAvailable	  ? compat::CompatibilityStateId::MoveTo
														  : compat::CompatibilityStateId::Hunt;
		context.requestTransition = machine.state() != context.requestedState;
		context.transitionId = compat::StateTransitionId::ExplicitStateChange;
		context.transitionReason = compat::StateTransitionReason::ObjectiveChanged;
	}
}

void BotBehaviorRuntime::observeHostageUseDispatched(
	std::size_t index, world::EntityKey target, const world::WorldPosition &origin)
{
	auto &actor = actors_[index];
	auto &machine = actor.managedBotStateMachines;
	const auto &decision = actor.scenarioDecisions;
	if (machine.state() != compat::CompatibilityStateId::UseEntity || machine.attackOverlayActive() ||
		decision.action != compat::ScenarioAction::Use || !decision.hasTarget || !(decision.target.entity == target))
		return;
	const auto context = makeManagedBotStateContext(index, {}, nullptr);
	if (!actor.scenarioPlanners.issueHostageUse(decision.target, context.timestamp, origin))
		return;
	if (machine.transition(compat::CompatibilityStateId::Idle, compat::StateTransitionReason::ObjectiveChanged,
			compat::StateTransitionId::ExplicitStateChange, context) == compat::StateResult::Transitioned)
	{
		actor.enemyReaction.onStateChange(
			reactionContext(index), compat::CompatibilityStateId::UseEntity, compat::CompatibilityStateId::Idle);
		services_->applyNavigationRequest({index, NavigationRequestKind::ClearStep});
		services_->applyNavigationRequest({index, NavigationRequestKind::DestroyPath});
		services_->applyNavigationRequest({index, NavigationRequestKind::ResetProgress});
	}
}

void BotBehaviorRuntime::commitBehaviorState(
	std::size_t index, compat::StateUpdateContext &context, compat::StateBehaviorInput &stateBehavior)
{
	auto &machine = actors_[index].managedBotStateMachines;
	const auto previousState = machine.state();
	const float previousStateTimestamp = machine.stateTimestamp();
	stateBehavior.navigation.actor = context.actor;
	stateBehavior.navigation.frame = context.frame;
	stateBehavior.navigation.fullUpdateSequence = context.fullUpdateSequence;
	actors_[index].managedStateInputs = stateBehavior;
	(void)machine.update(context);
	if (actors_[index].helpGoal.active && !helpGoalCurrent(index))
	{
		actors_[index].helpGoal = {};
		actors_[index].managedBotObjectiveTargets = {};
	}
	if (previousState != machine.state())
	{
		actors_[index].enemyReaction.onStateChange(reactionContext(index), previousState, machine.state());
		if (previousState == compat::CompatibilityStateId::InvestigateNoise)
		{
			actors_[index].noiseInvestigation.cancel();
			actors_[index].investigationWalking = false;
		}
		services_->applyNavigationRequest({index, NavigationRequestKind::ClearStep});
		// Reference state entry computes or abandons its own path. Never carry
		// Idle's roam corridor into a newly assigned MoveTo task.
		services_->applyNavigationRequest({index, NavigationRequestKind::DestroyPath});
		if (previousState == compat::CompatibilityStateId::Buy ||
			previousState == compat::CompatibilityStateId::DefuseBomb ||
			previousState == compat::CompatibilityStateId::PlantBomb ||
			previousState == compat::CompatibilityStateId::Hide || previousState == compat::CompatibilityStateId::UseEntity)
			services_->applyNavigationRequest({index, NavigationRequestKind::ResetProgress});
		if (machine.state() == compat::CompatibilityStateId::Idle &&
			(previousState == compat::CompatibilityStateId::DefuseBomb ||
				previousState == compat::CompatibilityStateId::PlantBomb))
		{
			actors_[index].managedBotObjectiveActions = {};
			actors_[index].scenarioDecisions = {};
			if (services_->service_runtimeProfiler().enabled())
				profiledLogConsole(services_->service_runtimeProfiler(), services_->service_pluginId(),
					"profile objectiveExit bot_id=%u frame=%u state_from=%s receipt=%u "
					"elapsed=%.3f",
					context.actor.slot, context.frame.tick, compat::toString(previousState),
					static_cast<unsigned>(stateBehavior.defuseReceipt), context.timestamp - previousStateTimestamp);
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
			profiledLogConsole(services_->service_runtimeProfiler(), services_->service_pluginId(),
				"profile navigationFailure bot_id=%u frame=%u state_from=%s state_to=%s", context.actor.slot,
				context.frame.tick, compat::toString(previousState), compat::toString(machine.state()));
	}
	if (!stateBehavior.pathFailed || !machine.attackOverlayActive())
		services_->applyNavigationRequest({index, NavigationRequestKind::ClearFailure});
}

} // namespace metamod
} // namespace astrabot
#undef ASTRABOT_BOT_BEHAVIOR_SCENARIO_MINIMUM_PLANAR_SIGHT_RANGE
