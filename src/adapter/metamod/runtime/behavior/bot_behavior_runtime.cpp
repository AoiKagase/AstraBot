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
	if (prepareManagedAction(ctx) == BehaviorPhaseResult::Complete)
	{
		actors_[index].managedBotCombat.stopMovement();
		return ctx.proposal;
	}
	ctx.closeFootstepDistance = footstep.current(ctx.actor, ctx.frame, services_->service_globals()->time)
		? footstep.distance : -1;
	if (decideReloadAction(ctx))
		return ctx.proposal;
	if (decideObjectiveAction(ctx) == BehaviorPhaseResult::Complete)
	{
		actors_[index].managedBotCombat.stopMovement();
		return ctx.proposal;
	}
	return decideCombatAction(ctx);
}

bool BotBehaviorRuntime::executionCurrent(
	std::size_t index, const compat::ReactionContext &receipt, std::uint64_t revision) const
{
	const auto now = reactionContext(index);
	return actors_[index].executionRevision == revision && receipt.actor == now.actor && receipt.frame == now.frame &&
		receipt.entityGeneration == now.entityGeneration && receipt.now == now.now && receipt.alive == now.alive;
}

void BotBehaviorRuntime::updateManagedBotCompatibilityState(
	std::size_t index, const runtime::MovementPhysicsState &before, PlantMoveChatter *plantMove)
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
	commitBehaviorState(index, context, stateBehavior);
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
	actors_[index].noiseLook.reset();
	actors_[index].closeFootstep = {};
	actors_[index].knifeRun.reset();
	actors_[index].enemyReaction.reset();
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
	actors_[index].bombGuardPlanners.reset();
	actors_[index].guardLook.reset();
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
	actors_[index].combatRetreat = {};
	actors_[index].enemyWeaponSource = {};
	actors_[index].noiseBend = {};
	actors_[index].noiseLookUpdate = 0U;
	actors_[index].reactionQueues.reset();
	actors_[index].recognizedTargets = {};
	actors_[index].recognitionSequences = 0U;
	const world::ActorKey actor = {handle.actor.slot, handle.actor.actorGeneration};
	actors_[index].guardLook.reset();
	actors_[index].equipment.reset();
	actors_[index].managedBotCombat = combat::CombatController(actor);
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
	actors_[index].enemyWeaponSource = {};
	actors_[index].noiseBend = {};
	actors_[index].noiseLookUpdate = 0U;
	actors_[index].obstacleLook = {};
	actors_[index].obstacleNextEquipAt = 0.0f;
	actors_[index].obstaclePathActive = false;
	actors_[index].managedBotCombat = combat::CombatController();
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
	actors_[index].attackReadyTimes = now + actors_[index].managedProfiles.attackDelaySeconds;
}

} // namespace metamod
} // namespace astrabot
