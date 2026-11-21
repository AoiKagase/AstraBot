#include "runtime/behavior/bot_behavior_runtime.hpp"
#include "runtime/shared/bot_runtime_support.hpp"
namespace astrabot
{
namespace metamod
{
using namespace runtime_detail;

ActionProposal BotBehaviorRuntime::decideManagedBotAction(std::size_t index,
	const runtime::MovementPhysicsState &before, const runtime::ViewAngles &movementAngles,
	std::uint16_t movementButtons)
{
	compat::FootstepEquipmentReceipt footstep{};
	if (index < actors_.size())
	{
		footstep = actors_[index].closeFootstep;
		actors_[index].closeFootstep = {};
		actors_[index].managedBotCombat.clearAim();
		actors_[index].weaponActions.stopRapidFire();
		const auto &scenario = actors_[index].scenarioDecisions;
		if (scenario.action != compat::ScenarioAction::Guard || !scenario.hasTarget || !scenario.target.inUseRange ||
			scenario.state != compat::CompatibilityStateId::Hide)
			actors_[index].guardLook.reset();
	}
	ManagedActionContext ctx{index, before, movementAngles};
	ctx.proposal = {ActionKind::None, movementAngles, movementButtons};
	const auto finish = [&](ActionProposal proposal) {
		applyAttackPosture(index, proposal);
		return proposal;
	};
	if (prepareManagedAction(ctx) == BehaviorPhaseResult::Complete)
	{
		actors_[index].managedBotCombat.stopMovement();
		return finish(ctx.proposal);
	}
	ctx.closeFootstepDistance = footstep.current(ctx.actor, ctx.frame, services_->service_globals()->time)
		? footstep.distance : -1;
	if (decideReloadAction(ctx))
		return finish(ctx.proposal);
	if (decideObjectiveAction(ctx) == BehaviorPhaseResult::Complete)
	{
		actors_[index].managedBotCombat.stopMovement();
		return finish(ctx.proposal);
	}
	return finish(decideCombatAction(ctx));
}

bool BotBehaviorRuntime::executionCurrent(
	std::size_t index, const compat::ReactionContext &receipt, std::uint64_t revision) const
{
	const auto now = reactionContext(index);
	return actors_[index].executionRevision == revision && receipt.actor == now.actor && receipt.frame == now.frame &&
		receipt.entityGeneration == now.entityGeneration && receipt.now == now.now && receipt.alive == now.alive;
}

bool BotBehaviorRuntime::plantCommandCurrent(std::size_t index) const
{
	const auto &actor = actors_[index];
	const auto &command = actor.plantCommand;
	return command.active && command.receipt.alive && executionCurrent(index, command.receipt, command.revision) &&
		services_->service_managedBotFullUpdateSequences()[index] == command.fullUpdate &&
		actor.managedBotStateMachines.normalStateEpoch() == command.stateEpoch &&
		actor.managedBotStateMachines.attackEntryEpoch() == command.attackEpoch &&
		!actor.managedBotStateMachines.attackOverlayActive();
}

BlindCommand BotBehaviorRuntime::blindCommand(std::size_t index, bool retainUntilFullUpdate) const
{
	const auto &bot = actors_[index];
	if (!bot.flashResponse.active ||
		services_->service_compatibilitySurface().configuration().mode != compat::RuntimeMode::Compatibility)
		return {};
	const auto now = reactionContext(index);
	const auto &earlier = bot.blindReceipt;
	const auto &observations = services_->observation_publicObservations();
	if (!now.alive || !(now.actor == earlier.actor) || now.entityGeneration != earlier.entityGeneration ||
		now.frame.mapGeneration != earlier.frame.mapGeneration || now.frame.roundGeneration != earlier.frame.roundGeneration ||
		!(now.now >= earlier.now) || observations.lifeRevision(now.actor) != bot.blindLifeRevision ||
		(!retainUntilFullUpdate && !observations.isGameDllBlinded(now.actor, now.frame, now.now)))
		return {};

	BlindCommand response = bot.flashResponse;
	if (bot.managedBotStateMachines.state() == compat::CompatibilityStateId::Hide && bot.scenarioPlanners.atDefenseSpot())
		response.move = BlindMove::None;
	return response;
}

bool BotBehaviorRuntime::nativePlantEntryStopsMovement(std::size_t index) const
{
	if (index >= actors_.size() || services_->service_compatibilitySurface().configuration().mode != compat::RuntimeMode::Compatibility)
		return false;
	const auto &actor = actors_[index];
	const auto &entry = actor.plantEntry;
	const auto &machine = actor.managedBotStateMachines;
	return entry.active && entry.fullUpdate == services_->service_managedBotFullUpdateSequences()[index] &&
		machine.state() == entry.state && machine.normalStateEpoch() == entry.epoch &&
		machine.attackEntryEpoch() == entry.attackEpoch && !machine.attackOverlayActive() &&
		executionCurrent(index, entry.receipt, entry.revision);
}

void BotBehaviorRuntime::updateManagedBotCompatibilityState(
	std::size_t index, const runtime::MovementPhysicsState &before, bool recoveringFromBlind,
	PlantMoveChatter *plantMove)
{
	if (plantMove)
		*plantMove = {};
	RuntimeProfilerScope scenarioScope(services_->service_runtimeProfiler(), RuntimeProfilerStage::ScenarioUpdate);
	if (index >= NativeBotObservation::kClientSlotCount || !services_->service_managedBotSlots()[index] ||
		services_->service_managedBotHandles()[index].entity == nullptr)
	{
		return;
	}
	if (actors_[index].noiseInvestigation.active() && !noiseInvestigationCurrent(index))
	{
		actors_[index].noiseInvestigation.reset();
	actors_[index].noiseReportPolicy = {};
	actors_[index].noiseReport = {};
		actors_[index].investigationWalking = false;
	}
	actors_[index].plantCommand = {};
	actors_[index].plantEntry = {};
	compat::StateBehaviorInput stateBehavior = {};
	compat::StateUpdateContext context = makeManagedBotStateContext(index, before, &stateBehavior);

	const auto lifecycleResult = updateBehaviorLifecycle(index, before, context);
	if (applyBombPlantNotice(index, context))
		context = makeManagedBotStateContext(index, before, &stateBehavior);
	if (lifecycleResult == BehaviorPhaseResult::Complete)
	{
		return;
	}
	if (services_->service_compatibilitySurface().configuration().mode == compat::RuntimeMode::Compatibility &&
		actors_[index].enemyReaction.eventActive(reactionContext(index)))
	{
		updateEnemyReaction(index, before);
		context = makeManagedBotStateContext(index, before, &stateBehavior);
	}
	if (recoveringFromBlind && !refreshRecoveredPerception(index))
		return;
	const auto receipt = reactionContext(index);
	const auto revision = actors_[index].executionRevision;
	const auto epoch = actors_[index].managedBotStateMachines.normalStateEpoch();
	const bool wasAtDefenseSpot = actors_[index].scenarioPlanners.atDefenseSpot();
	PlantMoveChatter currentPlantMove{};
	updateScenarioState(index, before, context, stateBehavior, plantMove ? &currentPlantMove : nullptr);
	if (!executionCurrent(index, receipt, revision) ||
		actors_[index].managedBotStateMachines.normalStateEpoch() != epoch)
		return;
	updateBehaviorPriorities(index, before, context, stateBehavior, wasAtDefenseSpot);
	if (!executionCurrent(index, receipt, revision) ||
		actors_[index].managedBotStateMachines.normalStateEpoch() != epoch)
		return;
	commitBehaviorState(index, context, stateBehavior, &currentPlantMove);
	if (plantMove && executionCurrent(index, receipt, revision) &&
		!actors_[index].managedBotStateMachines.attackOverlayActive())
		*plantMove = currentPlantMove;
}

void BotBehaviorRuntime::invalidateHelpExecution(std::size_t index)
{
	auto &actor = actors_[index];
	if (++actor.executionRevision == 0)
		++actor.executionRevision;
	actor.helpGoal = {};
	actor.teammateFollow = {};
	actor.followMotion = {};
	actor.managedTaskEntity = {};
}

void BotBehaviorRuntime::onActorRemoved(std::size_t index)
{
	actors_[index].morale = {};
	invalidateHelpExecution(index);
	actors_[index].flashResponse = {};
	actors_[index].noiseLook.reset();
	actors_[index].closeFootstep = {};
	actors_[index].knifeRun.reset();
	actors_[index].enemyReaction.reset();
	actors_[index].noiseEnemyContact = {};
	actors_[index].combatRetreat = {};
	actors_[index].enemyWeaponSource = {};
	actors_[index].noiseBend = {};
	actors_[index].noiseLookUpdate = 0U;
	stopVoiceFeedback(index);
	actors_[index].obstacleLook = {};
	actors_[index].obstacleNextEquipAt = 0.0f;
	actors_[index].obstaclePathActive = false;
	actors_[index].reactionQueues.reset();
	actors_[index].recognizedTargets = {};
	actors_[index].recognizedReloading = false;
	actors_[index].bombGuardPlanners.reset();
	actors_[index].guardLook.reset();
	actors_[index].huntApproaches = {};
	actors_[index].huntEncounter = {};
	actors_[index].equipment.reset();
	actors_[index].scenarioEscapeAreas = 0U;
	actors_[index].bombSearchMemory.reset();
	actors_[index].bombStatus = {};
	actors_[index].someHostagesTaken = false;
	actors_[index].sniperWarning = {};
	actors_[index].hostagePositions = {};
	actors_[index].looseBombObservation = {};
	actors_[index].managedStateInputs = {};
	actors_[index].bombSearchTargets = {};
	actors_[index].clearedBombsite = {};
	actors_[index].plantedBombsite = {};
	actors_[index].discoveredBombsiteReport = {};
	actors_[index].teamworkPolicies.reset();
	actors_[index].teamworkRadioApplied = 0U;
	actors_[index].noiseInvestigation.reset();
	actors_[index].noiseReportPolicy = {};
	actors_[index].outgoingRadioSchedulers.discardPending();
	actors_[index].confirmedSpeech.reset();
	actors_[index].noiseReport = {};
	actors_[index].investigationWalking = false;
	actors_[index].weaponActions.reset();
	actors_[index].buyControllers.reset();
	actors_[index].managedBotCombat = combat::CombatController();
	actors_[index].desiredLookYaw = 0.0f;
	actors_[index].managedBotObjectives = objectives::RoundObjectivePlanner();
	actors_[index].managedBotObjectiveTargets = {};
	actors_[index].managedBombApproaches = {};
	actors_[index].managedBotStateMachines = compat::CompatibilityStateMachine();
	actors_[index].handledBombPlant = {};
	actors_[index].managedBotStateRounds = 0U;
	actors_[index].managedBotWasDead = false;
}

void BotBehaviorRuntime::onActorRemembered(std::size_t index, const FakeClientHandle &handle)
{
	resetEncouragement();
	actors_[index].morale = {};
	invalidateHelpExecution(index);
	actors_[index].flashResponse = {};
	actors_[index].bombStatus = {};
	actors_[index].someHostagesTaken = false;
	actors_[index].sniperWarning = {};
	actors_[index].hostagePositions = {};
	actors_[index].looseBombObservation = {};
	actors_[index].teamworkPolicies.reset();
	actors_[index].noiseLook.reset();
	actors_[index].closeFootstep = {};
	actors_[index].knifeRun.reset();
	actors_[index].enemyReaction.reset();
	actors_[index].noiseEnemyContact = {};
	actors_[index].combatRetreat = {};
	actors_[index].enemyWeaponSource = {};
	actors_[index].noiseBend = {};
	actors_[index].noiseLookUpdate = 0U;
	actors_[index].reactionQueues.reset();
	actors_[index].recognizedTargets = {};
	actors_[index].recognizedReloading = false;
	actors_[index].recognitionSequences = 0U;
	const world::ActorKey actor = {handle.actor.slot, handle.actor.actorGeneration};
	actors_[index].guardLook.reset();
	actors_[index].huntApproaches = {};
	actors_[index].huntEncounter = {};
	actors_[index].equipment.reset();
	actors_[index].managedBotCombat = combat::CombatController(actor);
	actors_[index].managedBotCombat.initializeMovementRange(
		actor, services_->service_lifecycle().mapGeneration(), handle.combatRange);
	actors_[index].desiredLookYaw = 0.0f;
	actors_[index].managedBotObjectives = objectives::RoundObjectivePlanner(actor);
	actors_[index].managedBotStateMachines = compat::CompatibilityStateMachine(actor);
	actors_[index].managedBotStateRounds = services_->service_lifecycle().roundGeneration();
	actors_[index].managedBotWasDead = false;
	actors_[index].lastCombatTimes = 0.0f;
	actors_[index].lastVisibleEnemyTimes = 0.0f;
	actors_[index].noiseReportPolicy = {};
	actors_[index].outgoingRadioSchedulers.discardPending();
	actors_[index].confirmedSpeech.reset();
	actors_[index].noiseReport = {};
	actors_[index].buyControllers.reset();
	actors_[index].buyingComplete = false;
	actors_[index].escapingWithKnife = false;
	actors_[index].buyReceiptTimeoutLogged = false;
}

void BotBehaviorRuntime::resetSharedExecutionState()
{
	resetEncouragement();
	for (auto &actor : actors_)
	{
		if (++actor.executionRevision == 0)
			++actor.executionRevision;
		actor.helpGoal = {};
		actor.teammateFollow = {};
		actor.followMotion = {};
		actor.managedTaskEntity = {};
		actor.teamworkPolicies.resetValues();
		actor.outgoingRadioSchedulers.discardPending();
		actor.confirmedSpeech.reset();
		actor.knifeRun.reset();
		actor.enemyReaction.resetForLife();
		actor.noiseEnemyContact = {};
		actor.enemyWeaponSource = {};
	}
	updateVoiceFeedback(true);
	reset_managedStateInputs();
	managedTeamObjectiveCoordinator_.reset(
		services_->service_lifecycle().mapGeneration(), services_->service_lifecycle().roundGeneration());
	managedTeamObjectiveAssignments_ = {};
	managedTeamObjectiveAssignmentMapGeneration_ = services_->service_lifecycle().mapGeneration();
	managedTeamObjectiveAssignmentRoundGeneration_ = services_->service_lifecycle().roundGeneration();
	managedTeamObjectiveAssignmentFrame_ = 0U;
	managedTeamObjectiveAssignmentsValid_ = false;
	managedTeamObjectivePathFailurePending_ = false;
	managedTeamObjectivePathFailureActor_ = {};
	managedTeamObjectivePathFailureGeneration_ = 0U;
	managedTeamObjectivePathFailureStartArea_ = 0U;
	managedTeamObjectivePathFailureFrame_ = 0U;
	reset_managedBotObjectiveActions();
	fill_managedBotStateRounds(0U);
	fill_managedBotWasDead(false);
}

void BotBehaviorRuntime::resetActorExecutionState(std::size_t index)
{
	invalidateHelpExecution(index);
	actors_[index].flashResponse = {};
	actors_[index].teamworkPolicies.resetValues();
	actors_[index].noiseInvestigation.reset();
	actors_[index].noiseReportPolicy = {};
	actors_[index].outgoingRadioSchedulers.discardPending();
	actors_[index].confirmedSpeech.reset();
	actors_[index].noiseReport = {};
	actors_[index].investigationWalking = false;
	actors_[index].noiseLook.reset();
	actors_[index].closeFootstep = {};
	actors_[index].knifeRun.reset();
	actors_[index].enemyReaction.resetForLife();
		actors_[index].noiseEnemyContact = {};
	actors_[index].enemyWeaponSource = {};
	actors_[index].noiseBend = {};
	actors_[index].noiseLookUpdate = 0U;
	actors_[index].obstacleLook = {};
	actors_[index].obstacleNextEquipAt = 0.0f;
	actors_[index].obstaclePathActive = false;
	actors_[index].managedBotCombat = combat::CombatController();
	const auto &handle = services_->service_managedBotHandles()[index];
	actors_[index].managedBotCombat.initializeMovementRange({handle.actor.slot, handle.actor.actorGeneration},
		services_->service_lifecycle().mapGeneration(), handle.combatRange);
	actors_[index].desiredLookYaw = 0.0f;
	actors_[index].managedBotObjectives = objectives::RoundObjectivePlanner();
	actors_[index].managedBotObjectiveTargets = {};
	actors_[index].managedBombApproaches = {};
	actors_[index].managedBotObjectiveActions = {};
	actors_[index].managedBotStateMachines = compat::CompatibilityStateMachine();
}

void BotBehaviorRuntime::resetRoundTactics()
{
	for (auto &actor : actors_)
	{
		if (++actor.executionRevision == 0)
			++actor.executionRevision;
		actor.helpGoal = {};
		actor.teammateFollow = {};
		actor.followMotion = {};
		actor.obstacleLook = {};
		actor.obstaclePathActive = false;
		actor.desiredLookYaw = 0.0f;
	}

	reset_scenarioEscapeAreas();
	reset_bombSearchMemory();
	reset_bombSearchTargets();
	reset_teamworkPolicies();
	reset_teamworkRadioApplied();
}

void BotBehaviorRuntime::resetRoundNoiseInvestigation()
{
	reset_noiseInvestigation();
}

void BotBehaviorRuntime::resetRoundObjectiveCoordination()
{
	fill_managedBotObjectiveActions(ManagedObjectiveActionState{});
	managedTeamObjectiveAssignmentsValid_ = false;
	managedTeamObjectivePathFailurePending_ = false;
	managedTeamObjectivePathFailureActor_ = {};
	managedTeamObjectivePathFailureGeneration_ = 0U;
	managedTeamObjectivePathFailureStartArea_ = 0U;
	managedTeamObjectivePathFailureFrame_ = 0U;
}

void BotBehaviorRuntime::assignProfile(std::size_t index, const compat::ProfileRecord &profile)
{
	actors_[index].managedProfiles = profile;
}

void BotBehaviorRuntime::observeRecognizedTargetChange(std::size_t index, float now)
{
	auto &actor = actors_[index];
	actor.attackReadyTimes = now + actor.managedProfiles.attackDelaySeconds;
	actor.enemyIdleEntries = actor.managedBotStateMachines.idleEntryCount();
}

} // namespace metamod
} // namespace astrabot
