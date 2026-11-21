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

	services.surface.setFloat("bot_enable", 1.0f);
	command.resetManagedBotCommandTemplate(0U);
	handle.entity->v.v_angle = Vector(0, 35, 0);
	endpoint.locomotionIntent = {};
	endpoint.roamDecision.huntWaitingForFriend = true;
	services.globals.time += runtime::BotTimingScheduler::kCommandInterval;
	updateHunt();
	if (!check(services.lastYaw == 35.0f && services.lastForward == 0.0f && services.lastSide == 0.0f,
			"the first waiting command retains the observed view and has no movement"))
		return 1;
	services.globals.time += runtime::BotTimingScheduler::kCommandInterval;
	command.executeManagedBotCommand(0U, handle);
	if (!check(services.lastYaw < 35.0f && services.lastForward == 0.0f && services.lastSide == 0.0f,
			"the next upkeep turns toward the waiting Hunt route without moving"))
		return 1;
	command.resetManagedBotCommandTemplate(0U);
	handle.entity->v.v_angle = Vector(0, 35, 0);
	const auto waitOrigin = handle.entity->v.origin;
	endpoint.roamDecision.huntWaitingForFriend = false;
	endpoint.huntApproachLook.update = HuntApproachLookUpdate::Track;
	endpoint.huntApproachLook.spot = nav::NavVector{waitOrigin.x, waitOrigin.y + 100.0f, waitOrigin.z};
	services.globals.time += runtime::BotTimingScheduler::kCommandInterval;
	updateHunt();
	if (!check(services.lastYaw == 35.0f && services.lastForward == 0.0f && services.lastSide == 0.0f,
			"the new approach spot takes effect at the subsequent upkeep"))
		return 1;
	for (int tick = 0; tick < 12; ++tick)
	{
		handle.entity->v.origin = waitOrigin;
		services.globals.time += runtime::BotTimingScheduler::kCommandInterval;
		command.executeManagedBotCommand(0U, handle);
	}
	if (!check(services.lastYaw > 70.0f && services.lastForward == 0.0f && services.lastSide == 0.0f,
			"ordinary stationary Hunt uses its approach spot without a blocker"))
		return 1;
	endpoint.huntApproachLook.update = HuntApproachLookUpdate::Unchanged;
	endpoint.roamDecision.huntWaitingForFriend = false;
	endpoint.locomotionIntent.direction = {1.0f, 0.0f, 0.0f};
	endpoint.locomotionIntent.speed = 240.0f;
	handle.entity->v.origin = waitOrigin;
	services.globals.time += runtime::BotTimingScheduler::kCommandInterval;
	updateHunt();
	if (!check(services.lastYaw > 70.0f && services.lastForward == 0.0f && services.lastSide == 240.0f,
			"the first resumed move projects into the already advanced approach-spot view"))
		return 1;
	for (int tick = 0; tick < 8; ++tick)
	{
		services.globals.time += runtime::BotTimingScheduler::kCommandInterval;
		handle.entity->v.origin = waitOrigin;
		command.executeManagedBotCommand(0U, handle);
	}
	if (!check(services.lastYaw > 75.0f && services.lastYaw < 105.0f && services.lastSide == 240.0f,
			"continued walking keeps the approach spot and the cached local input"))
		return 1;
	endpoint.huntApproachLook.update = HuntApproachLookUpdate::Clear;
	services.globals.time += runtime::BotTimingScheduler::kCommandInterval;
	updateHunt();
	if (!check(!command.hasHuntApproachLook(0U), "clearing the approach spot restores path look on the next upkeep"))
		return 1;
	endpoint.huntApproachLook.update = HuntApproachLookUpdate::Unchanged;

	endpoint.roamDecision.huntWaitingForFriend = true;
	endpoint.locomotionIntent = {};
	services.globals.time += runtime::BotTimingScheduler::kCommandInterval;
	updateHunt();
	const float templateWaitYaw = services.lastYaw;
	services.globals.time += runtime::BotTimingScheduler::kCommandInterval;
	command.executeManagedBotCommand(0U, handle);
	const float cancelledWaitYaw = services.lastYaw;
	if (!check(cancelledWaitYaw != templateWaitYaw, "the waiting view has advanced since the full update"))
		return 1;
	services.surface.setFloat("bot_stop", 1.0f);
	for (int tick = 0; tick < 2; ++tick)
	{
		services.globals.time += runtime::BotTimingScheduler::kCommandInterval;
		command.executeManagedBotCommand(0U, handle);
		if (!check(services.lastYaw == cancelledWaitYaw && services.lastForward == 0.0f && services.lastSide == 0.0f,
				"bot_stop cancels a cached wait look before the next full update"))
			return 1;
	}
	services.surface.setFloat("bot_stop", 0.0f);
	endpoint.roamDecision.huntWaitingForFriend = false;
	endpoint.locomotionIntent.direction = {0.0f, -1.0f, 0.0f};
	endpoint.locomotionIntent.speed = 240.0f;
	endpoint.locomotionIntent.walkingToHuntEndpoint = true;

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
	handle.entity->v.movetype = MOVETYPE_FLY;
	handle.entity->v.flags &= ~FL_ONGROUND;
	handle.entity->v.v_angle = Vector(60.0f, 90.0f, 0.0f);
	before = services.observation_captureMovementPhysicsState(handle.entity, true, true);
	const auto descentDraft = command.beginMovementCommand(0U, handle, token, before, ladder);
	const ActionProposal descentAction{ActionKind::None, descentDraft.command.viewAngles, 0U};
	command.finishMovementCommand(0U, handle, token, before, ladder, descentDraft, descentAction);
	services.descentDestinationLook = true;
	command.advanceUpkeepLook(0U, handle);
	command.executeManagedBotCommand(0U, handle);
	if (!check(std::fabs(handle.entity->v.v_angle.x - 56.6667f) < .001f &&
			services.lastForward == 96.0f && services.lastSide == -96.0f,
			"descending upkeep turns the view while retaining ladder-local movement"))
		return 1;
	services.globals.time += runtime::BotTimingScheduler::kCommandInterval;
	command.advanceUpkeepLook(0U, handle);
	const auto haltedDescent = command.beginNeutralCommand(0U, handle, before);
	const ActionProposal haltAction{ActionKind::None, haltedDescent.command.viewAngles, 0U};
	command.finishNeutralCommand(0U, handle, before, haltedDescent, haltAction);
	command.executeManagedBotCommand(0U, handle);
	if (!check(std::fabs(handle.entity->v.v_angle.x - 50.0f) < .001f && services.lastForward == 0.0f &&
			services.lastSide == 0.0f && services.descentFallbackLooks == 0,
			"a suppressed Hunt move keeps exactly one descent look step and emits zero movement"))
		return 1;
	services.globals.time += runtime::BotTimingScheduler::kCommandInterval;
	command.advanceUpkeepLook(0U, handle);
	const auto floorYaw = handle.entity->v.v_angle.y;
	services.descentDestinationLook = false;
	updateHunt();
	if (!check(services.lastYaw == floorYaw && std::fabs(handle.entity->v.v_angle.x - 40.0f) < .001f &&
			services.lastForward == -96.0f && services.lastSide == 0.0f,
			"the first floor decision projects movement into the view already updated by upkeep"))
		return 1;
	handle.entity->v.movetype = MOVETYPE_WALK;
	services.globals.time += runtime::BotTimingScheduler::kCommandInterval;
	command.advanceUpkeepLook(0U, handle);
	command.executeManagedBotCommand(0U, handle);
	if (!check(std::fabs(handle.entity->v.v_angle.x - 26.6667f) < .001f && services.lastForward == -96.0f &&
			services.lastSide == 0.0f, "the first detached upkeep keeps the spring and last full update's axes"))
		return 1;
	handle.entity->v.flags |= FL_ONGROUND;
	services.globals.time += runtime::BotTimingScheduler::kCommandInterval;
	command.advanceUpkeepLook(0U, handle);
	updateHunt();
	const auto takeoffView = handle.entity->v.v_angle;
	handle.entity->v.flags &= ~FL_ONGROUND;
	services.globals.time += runtime::BotTimingScheduler::kCommandInterval;
	command.advanceUpkeepLook(0U, handle);
	command.executeManagedBotCommand(0U, handle);
	if (!check(handle.entity->v.v_angle.x == takeoffView.x && services.lastYaw == takeoffView.y,
			"ordinary airborne Hunt holds takeoff view after the ladder handoff ends"))
		return 1;
	command.resetManagedBotCommandTemplate(0U);
	handle.entity->v.movetype = MOVETYPE_FLY;
	handle.entity->v.v_angle = Vector(60.0f, 90.0f, 0.0f);
	before = services.observation_captureMovementPhysicsState(handle.entity, true, true);
	const auto stoppedLadderDraft = command.beginMovementCommand(0U, handle, token, before, ladder);
	const ActionProposal descentMove{ActionKind::None, stoppedLadderDraft.command.viewAngles, 0U};
	command.finishMovementCommand(0U, handle, token, before, ladder, stoppedLadderDraft, descentMove);
	services.stoppedHuntLookPhase = nav::NativeLadderPhase::Descend;
	services.globals.time += runtime::BotTimingScheduler::kCommandInterval;
	command.advanceUpkeepLook(0U, handle);
	services.stoppedHuntLookPhase = nav::NativeLadderPhase::StoppedHuntLook;
	const auto stopped = command.beginNeutralCommand(0U, handle, before);
	const ActionProposal stoppedAction{ActionKind::None, stopped.command.viewAngles, 0U};
	command.finishNeutralCommand(0U, handle, before, stopped, stoppedAction);
	command.executeManagedBotCommand(0U, handle);
	if (!check(handle.entity->v.v_angle.x == 60.0f && services.lastButtons == 0U &&
			services.lastForward == 0.0f && services.lastSide == 0.0f,
			"the successful replan keeps the view from upkeep and sends a neutral command"))
		return 1;
	for (const float pitch : {56.6667f, 50.0f})
	{
		services.globals.time += runtime::BotTimingScheduler::kCommandInterval;
		command.advanceUpkeepLook(0U, handle);
		command.executeManagedBotCommand(0U, handle);
		if (!check(std::fabs(handle.entity->v.v_angle.x - pitch) < .001f && services.lastButtons == 0U &&
				services.lastForward == 0.0f && services.lastSide == 0.0f,
				"neutral light commands resume ordinary pitch with the same spring on the ladder"))
			return 1;
	}
	handle.entity->v.movetype = MOVETYPE_WALK;
	handle.entity->v.flags |= FL_ONGROUND;
	before = services.observation_captureMovementPhysicsState(handle.entity, true, true);
	context.frame.tick++;
	context.fullUpdateSequence++;
	context.timestamp = services.globals.time;
	if (!check(services.stateMachine.transition(compat::CompatibilityStateId::Hide,
			compat::StateTransitionReason::ExplicitRequest, compat::StateTransitionId::ExplicitStateChange, context) ==
			compat::StateResult::Transitioned, "enter Hide with the view still facing the threat"))
		return 1;
	services.globals.time += runtime::BotTimingScheduler::kCommandInterval;
	command.advanceUpkeepLook(0U, handle);
	if (!check(handle.entity->v.v_angle.x == 50.0f, "Hide does not consume the stopped Hunt look"))
		return 1;
	services.stoppedHuntLookPhase = nav::NativeLadderPhase::None;
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
	if (!check(services.stateMachine.transition(compat::CompatibilityStateId::InvestigateNoise,
			compat::StateTransitionReason::ExplicitRequest, compat::StateTransitionId::ExplicitStateChange, context) ==
			compat::StateResult::Transitioned, "enter InvestigateNoise with an active movement intent"))
		return 1;
	command.resetManagedBotCommandTemplate(0U);
	handle.entity->v.v_angle = Vector(4.0f, 0.0f, 0.0f);
	NavigationUpdate investigation{};
	investigation.locomotionIntent.direction = {-1.0f, 0.0f, 0.0f};
	investigation.locomotionIntent.speed = 240.0f;
	const auto updateInvestigation = [&]()
	{
		const auto draft = command.beginMovementCommand(0U, handle, token, before, investigation);
		const ActionProposal action{ActionKind::None, draft.command.viewAngles, 0U};
		command.finishMovementCommand(0U, handle, token, before, investigation, draft, action);
	};
	updateInvestigation();
	if (!check(services.lastYaw == 0.0f && handle.entity->v.v_angle.x == 4.0f && services.lastForward < 0,
			"investigation movement starts from the current view"))
		return 1;
	services.globals.time += runtime::BotTimingScheduler::kCommandInterval;
	command.executeManagedBotCommand(0U, handle);
	const float investigationYaw = services.lastYaw;
	if (!check(std::fabs(investigationYaw) > 2.0f && std::fabs(investigationYaw) < 5.0f &&
			services.lastForward < 0, "investigation upkeep turns without changing the last movement axes"))
		return 1;
	investigation.locomotionIntent.direction.x = 1.0f;
	services.globals.time += runtime::BotTimingScheduler::kCommandInterval;
	updateInvestigation();
	if (!check(std::fabs(services.lastYaw - investigationYaw) < 10.0f && services.lastForward > 0,
			"an investigation route reversal changes the movement command without snapping the view"))
		return 1;
	context.frame.tick++;
	context.fullUpdateSequence++;
	context.timestamp = services.globals.time;
	if (!check(services.stateMachine.transition(compat::CompatibilityStateId::Hunt,
			compat::StateTransitionReason::ExplicitRequest, compat::StateTransitionId::ExplicitStateChange, context) ==
			compat::StateResult::Transitioned, "return to Hunt with an ordinary corridor"))
		return 1;
	command.resetManagedBotCommandTemplate(0U);
	handle.entity->v.v_angle = Vector(4.0f, 166.0f, 0.0f);
	endpoint.roamDecision.huntEndpointSteering = false;
	endpoint.locomotionIntent.walkingToHuntEndpoint = false;
	updateHunt();
	if (!check(services.lastYaw == 166.0f && handle.entity->v.v_angle.x == 4.0f &&
			services.lastSide < 0, "an ordinary Hunt corridor preserves the current view for its first movement command"))
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
	command.advanceUpkeepLook(0U, handle);
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
