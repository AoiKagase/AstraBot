#include "runtime_command_scenario_fixture.hpp"

int main()
{
	using namespace astrabot;
	using namespace astrabot::metamod;
	using runtime::DispatchResult;
	ScenarioServices services;
	BotCommandRuntime command;
	command.bindServices(services);
	auto &handle = services.handles[0];
	auto before = services.observation_captureMovementPhysicsState(handle.entity, true, true);
	const ActionProposal none{ActionKind::None, {0.0f, 0.0f, 0.0f}, 0U};

	// This is the boundary at which Root invokes behavior: a template draft has
	// not consumed a sequence, while the direct neutral path already has.
	const auto normal = command.beginNeutralCommand(0U, handle, before);
	if (!check(
			normal.ready && command.managedBotCommandSequences(0U) == 0U, "normal decision sees the previous sequence"))
	{
		return 1;
	}
	command.finishNeutralCommand(0U, handle, before, normal, none);
	const auto sent = command.executeManagedBotCommand(0U, handle);
	if (!check(sent.result == DispatchResult::Dispatched && sent.sequence == 1U,
			"normal sequence advances after decision"))
	{
		return 1;
	}
	const auto repeat = command.executeManagedBotCommand(0U, handle);
	if (!check(repeat.result == DispatchResult::Dispatched && repeat.sequence == 2U,
			"template reuse advances once without another decision"))
	{
		return 1;
	}
	const auto direct = command.beginDirectNeutralCommand(0U, handle, 10U);
	if (!check(direct.ready && direct.command.sequence == 3U && command.managedBotCommandSequences(0U) == 3U,
			"direct neutral decision sees the advanced sequence"))
	{
		return 1;
	}
	if (!check(command.finishDirectNeutralCommand(0U, handle, 10U, direct, none).result == DispatchResult::Dispatched,
			"direct neutral prepared input is sent once"))
	{
		return 1;
	}
	handle.entity->v.deadflag = DEAD_DEAD;
	const auto dead = command.beginDirectNeutralCommand(0U, handle, 10U);
	if (!check(!dead.ready && command.managedBotCommandSequences(0U) == 4U && services.moves == 3U,
			"dead direct neutral consumes sequence without dispatch or decision"))
	{
		return 1;
	}
	handle.entity->v.deadflag = DEAD_NO;
	const auto respawn = command.beginDirectNeutralCommand(0U, handle, 10U);
	if (!check(respawn.ready && respawn.command.sequence == 5U, "respawn preserves the next sequence"))
	{
		return 1;
	}
	services.engine.pfnRunPlayerMove = nullptr;
	const auto failed = command.finishDirectNeutralCommand(0U, handle, 10U, respawn, none);
	if (!check(failed.result == DispatchResult::EngineUnavailable && services.moves == 3U,
			"failed dispatch does not retry or notify a successful move"))
	{
		return 1;
	}
	services.engine.pfnRunPlayerMove = &scenarioMove;
	services.changeFreezeDuringMove = true;
	services.freeze = false;
	const ActionProposal attack{ActionKind::Fire, {0.0f, 0.0f, 0.0f}, 0U};
	const auto resumed = command.beginNeutralCommand(0U, handle, before);
	command.finishNeutralCommand(0U, handle, before, resumed, attack);
	services.attackMovesBefore = services.moves;
	services.attackOriginBefore = handle.entity->v.origin.x;
	services.behavior_observeC4AttackDispatched(0U, handle, IN_ATTACK);
	if (!check(!services.attackNoticeSawMovedOrigin, "notification before this dispatch fails the order guard"))
	{
		return 1;
	}
	const auto moved = command.executeManagedBotCommand(0U, handle);
	if (!check(moved.result == DispatchResult::Dispatched && services.attackNoticeSawMovedOrigin &&
				services.freezeReadAfterMove,
			"successful dispatch notifies and re-observes after the engine callback"))
	{
		return 1;
	}
	services.changeFreezeDuringMove = false;
	const auto frozenMoves = services.moves;
	handle.entity->v.flags |= FL_FROZEN;
	command.executeManagedBotCommand(0U, handle);
	if (!check(services.moves == frozenMoves + 1U && services.lastButtons == 0U &&
				!command.managedBotCommandTemplateValid(0U),
			"frozen dispatch clears cached input and buttons"))
	{
		return 1;
	}
	services.freeze = false;
	handle.entity->v.flags &= ~(FL_FROZEN | FL_ONGROUND);
	handle.entity->v.movetype = MOVETYPE_FLY;
	before = services.observation_captureMovementPhysicsState(handle.entity, true, true);
	NavigationUpdate ladder{};
	ladder.locomotionIntent.nativeLadderControl = true;
	ladder.locomotionIntent.nativeMustJump = true;
	ladder.locomotionIntent.nativeForward = 1;
	ladder.locomotionIntent.nativeSide = -1;
	ladder.locomotionIntent.nativeWalking = true;
	ladder.locomotionIntent.speed = 100;
	const auto ladderDraft =
		command.beginMovementCommand(0, handle, services.lifecycle.tokenForSlot(1), before, ladder);
	ActionProposal ladderAction{ActionKind::None, ladderDraft.command.viewAngles, ladderDraft.command.movement.buttons};
	command.finishMovementCommand(
		0, handle, services.lifecycle.tokenForSlot(1), before, ladder, ladderDraft, ladderAction);
	if (!check((services.lastButtons & IN_JUMP) && services.lastForward == 96 && services.lastSide == -96,
			"native ladder escape jump reaches RunPlayerMove while airborne, with native per-axis walking input"))
		return 1;
	CombatUpkeepUpdate ladderLook{};
	ladderLook.hasAim = true;
	ladderLook.angles = {60, 90, 0};
	command.applyCombatUpkeep(0, ladderLook);
	command.executeManagedBotCommand(0, handle);
	if (!check(services.lastForward == 96 && services.lastSide == -96,
			"combat upkeep changes ladder view without rotating native local movement fractions"))
		return 1;

	const auto token = services.lifecycle.tokenForSlot(1);
	compat::StateUpdateContext context{};
	context.actor = {handle.actor.slot, handle.actor.actorGeneration};
	context.frame = {1U, 1U, 1U};
	context.timestamp = services.globals.time;
	context.fullUpdateSequence = 1U;
	context.fullUpdate = true;
	services.stateMachine = compat::CompatibilityStateMachine(context.actor);
	if (!check(services.stateMachine.initialize(context) == compat::StateResult::Transitioned &&
			services.stateMachine.transition(compat::CompatibilityStateId::Hunt, compat::StateTransitionReason::ExplicitRequest,
				compat::StateTransitionId::ExplicitStateChange, context) == compat::StateResult::Transitioned,
			"set up the Hunt command scenario"))
		return 1;
	handle.entity->v.flags |= FL_ONGROUND;
	handle.entity->v.movetype = MOVETYPE_WALK;
	handle.entity->v.v_angle = Vector(0, 0, 0);
	NavigationUpdate endpoint{};
	endpoint.roamDecision.huntEndpointSteering = true;
	endpoint.roamDecision.corridorAreaCount = 1;
	endpoint.roamDecision.huntEndpoint = {handle.entity->v.origin.x, -100.0f, 0.0f};
	endpoint.locomotionIntent.direction = {0.0f, -1.0f, 0.0f};
	endpoint.locomotionIntent.speed = 240.0f;
	endpoint.locomotionIntent.walkingToHuntEndpoint = true;
	const auto updateHunt = [&]()
	{
		command.resetManagedBotCommandInputs(0U);
		before = services.observation_captureMovementPhysicsState(handle.entity, true, true);
		const auto draft = command.beginMovementCommand(0U, handle, token, before, endpoint);
		const ActionProposal action{ActionKind::None, draft.command.viewAngles, 0U};
		command.finishMovementCommand(0U, handle, token, before, endpoint, draft, action);
	};
	updateHunt();
	if (!check(services.lastYaw == 0.0f && services.lastForward == 0.0f && services.lastSide == 96.0f,
			"the first Hunt command steps right at walk speed using the observed view"))
		return 1;
	services.globals.time += runtime::BotTimingScheduler::kCommandInterval;
	command.executeManagedBotCommand(0U, handle);
	const float upkeepYaw = services.lastYaw;
	if (!check(upkeepYaw < 0.0f && services.lastForward == 0.0f && services.lastSide == 96.0f,
			"Hunt upkeep turns while holding both movement axes"))
		return 1;
	services.globals.time += runtime::BotTimingScheduler::kCommandInterval;
	updateHunt();
	if (!check(services.lastYaw < upkeepYaw * 2.0f,
			"the next full Hunt decision preserves angular velocity and uses its updated view"))
		return 1;

	command.resetManagedBotCommandTemplate(0U);
	handle.entity->v.v_angle = Vector(0, 0, 0);
	endpoint.roamDecision.corridorAreaCount = 2;
	endpoint.roamDecision.corridorIndex = 0;
	endpoint.locomotionIntent.walkingToHuntEndpoint = false;
	updateHunt();
	if (!check(services.lastForward == 0.0f && services.lastSide == 240.0f,
			"fallback runs while the full update starts on the portal leg inside the endpoint walk radius"))
		return 1;
	endpoint.roamDecision.corridorIndex = 1;
	endpoint.locomotionIntent.walkingToHuntEndpoint = true;
	updateHunt();
	if (!check(services.lastForward == 0.0f && services.lastSide == 96.0f,
			"fallback first slows on the update that starts on the endpoint leg"))
		return 1;
	context.frame.tick++;
	context.fullUpdateSequence++;
	context.timestamp = services.globals.time;
	if (!check(services.stateMachine.transition(compat::CompatibilityStateId::Hide,
			compat::StateTransitionReason::ExplicitRequest, compat::StateTransitionId::ExplicitStateChange, context) ==
			compat::StateResult::Transitioned, "enter Hide with the view still facing the threat"))
		return 1;
	command.resetManagedBotCommandTemplate(0U);
	handle.entity->v.v_angle = Vector(-4.0f, -170.0f, 0.0f);
	NavigationUpdate cover{};
	cover.locomotionIntent.direction = {0.0f, -1.0f, 0.0f};
	cover.locomotionIntent.speed = 240.0f;
	const auto coverDraft = command.beginMovementCommand(0U, handle, token, before, cover);
	const ActionProposal coverAction{ActionKind::None, coverDraft.command.viewAngles, 0U};
	command.finishMovementCommand(0U, handle, token, before, cover, coverDraft, coverAction);
	if (!check(services.lastYaw == -170.0f && handle.entity->v.v_angle.x == -4.0f && services.lastSide < 0,
			"Hide starts moving toward cover without snapping to the path yaw"))
		return 1;
	services.globals.time += runtime::BotTimingScheduler::kCommandInterval;
	command.executeManagedBotCommand(0U, handle);
	if (!check(services.lastYaw > -170.0f && services.lastYaw < -160.0f && services.lastSide < 0,
			"Hide upkeep turns toward the cover route while retaining the movement command"))
		return 1;
	context.frame.tick++;
	context.fullUpdateSequence++;
	context.timestamp = services.globals.time;
	if (!check(services.stateMachine.transition(compat::CompatibilityStateId::UseEntity,
			compat::StateTransitionReason::ExplicitRequest, compat::StateTransitionId::ExplicitStateChange, context) ==
			compat::StateResult::Transitioned, "enter the interaction state"))
		return 1;
	handle.entity->v.v_angle = Vector(0, 0, 0);
	const auto facingDraft = command.beginNeutralCommand(0U, handle, before);
	ActionProposal facing{ActionKind::None, {}, 0U};
	facing.stopForWeapon = true;
	facing.hasUseEntityLook = true;
	facing.useEntityLookTarget = {handle.entity->v.origin.x, -32.0f, 32.0f};
	command.finishNeutralCommand(0U, handle, before, facingDraft, facing);
	command.executeManagedBotCommand(0U, handle);
	if (!check(services.lastYaw == 0.0f && (services.lastButtons & IN_USE) == 0U,
			"interaction registers the aim goal without snapping or using"))
		return 1;
	services.globals.time += runtime::BotTimingScheduler::kCommandInterval;
	command.advanceUseEntityLook(0U, handle);
	command.executeManagedBotCommand(0U, handle);
	if (!check(services.lastYaw < 0.0f && services.lastYaw > -20.0f &&
			handle.entity->v.v_angle.x < 0.0f && (services.lastButtons & IN_USE) == 0U,
			"cached interaction upkeep moves both axes without sending USE"))
		return 1;
	const auto useDraft = command.beginNeutralCommand(0U, handle, before);
	ActionProposal useHostage{ActionKind::UseEntity, useDraft.command.viewAngles, 0U};
	useHostage.hostageUseTarget = {42U, 1U};
	command.finishNeutralCommand(0U, handle, before, useDraft, useHostage);
	services.engine.pfnRunPlayerMove = nullptr;
	command.executeManagedBotCommand(0U, handle);
	if (!check(services.hostageUseReceipts == 0U, "a failed interaction dispatch consumes no attempt"))
		return 1;
	services.engine.pfnRunPlayerMove = &scenarioMove;
	command.finishNeutralCommand(0U, handle, before, useDraft, useHostage);
	command.executeManagedBotCommand(0U, handle);
	command.executeManagedBotCommand(0U, handle);
	if (!check(services.hostageUseReceipts == 1U && (services.lastButtons & IN_USE) != 0U,
			"the accepted USE is counted once while the engine retains the held button"))
		return 1;
	std::puts("runtime command orchestration scenarios: PASS");
	return 0;
}
