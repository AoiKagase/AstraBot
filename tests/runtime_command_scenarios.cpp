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

	command.resetManagedBotCommandTemplate(0U);
	handle.entity->v.origin = waitOrigin;
	handle.entity->v.v_angle = Vector(0, 90, 0);
	const auto eye = waitOrigin + handle.entity->v.view_ofs;
	endpoint.huntApproachLook = {HuntApproachLookUpdate::TrackEncounter, {eye.x, eye.y + 200.0f, eye.z}};
	services.globals.time += runtime::BotTimingScheduler::kCommandInterval;
	updateHunt();
	const float glanceForward = services.lastForward, glanceSide = services.lastSide;
	if (!check(command.hasHuntApproachLook(0U) && services.lastYaw == 90.0f &&
			glanceForward == 0.0f && glanceSide == 240.0f,
			"the selecting update keeps the current view and local movement"))
		return 1;
	for (int tick = 0; tick < 2; ++tick)
	{
		handle.entity->v.origin = waitOrigin;
		services.globals.time += runtime::BotTimingScheduler::kCommandInterval;
		command.advanceUpkeepLook(0U, handle);
		command.executeManagedBotCommand(0U, handle);
		if (!check(command.hasHuntApproachLook(0U) == (tick == 0) &&
				services.lastForward == glanceForward && services.lastSide == glanceSide &&
				std::fabs(services.lastYaw - 90.0f) < 10.0f,
				"encounter holds through the first aligned upkeep and releases after the next while keeping the axes"))
			return 1;
	}

	handle.entity->v.origin = waitOrigin;
	endpoint.huntApproachLook.spot = {waitOrigin.x, waitOrigin.y + 90.0f, waitOrigin.z};
	services.globals.time += runtime::BotTimingScheduler::kCommandInterval;
	updateHunt();
	services.globals.time += runtime::BotTimingScheduler::kCommandInterval;
	command.advanceUpkeepLook(0U, handle);
	if (!check(!command.hasHuntApproachLook(0U), "an encounter target within 100 units clears before turning"))
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
	// A new injected pose belongs to a new command timestamp. Retain the
	// actor's velocity instead of assuming ResetPath reset a private spring.
	services.globals.time += runtime::BotTimingScheduler::kCommandInterval;
	handle.entity->v.movetype = MOVETYPE_FLY;
	handle.entity->v.flags &= ~FL_ONGROUND;
	handle.entity->v.v_angle = Vector(60.0f, 90.0f, 0.0f);
	before = services.observation_captureMovementPhysicsState(handle.entity, true, true);
	const auto descentDraft = command.beginMovementCommand(0U, handle, token, before, ladder);
	const ActionProposal descentAction{ActionKind::None, descentDraft.command.viewAngles, 0U};
	command.finishMovementCommand(0U, handle, token, before, ladder, descentDraft, descentAction);
	auto referenceSpring = command.lookSpring(0U);
	runtime::ViewAngles referenceLook{60.0f, 90.0f, 0.0f};
	const auto nativeStep = [&](float yaw, float pitch = 0.0f, bool ladderOverride = false) {
		const float now = services.globals.time;
		if (!ladderOverride)
		{
			yaw += 2.0f * ActionAdapter::csBotCosine(33.0f * now);
			pitch += 2.0f * ActionAdapter::csBotCosine(13.0f * now - 90.0f);
		}
		const auto next = compat::trackAim(referenceLook.yaw, referenceLook.pitch, yaw, pitch,
			runtime::BotTimingScheduler::kCommandInterval, 1.0f, &referenceSpring, false);
		referenceLook = {next.pitch, next.yaw, 0.0f};
	};
	const auto matchesNative = [&]() {
		const auto spring = command.lookSpring(0U);
		const bool match = std::fabs(handle.entity->v.v_angle.x - referenceLook.pitch) < .0001f &&
			std::fabs(services.lastYaw - referenceLook.yaw) < .0001f &&
			std::fabs(spring.yawVelocity - referenceSpring.yawVelocity) < .0001f &&
			std::fabs(spring.pitchVelocity - referenceSpring.pitchVelocity) < .0001f;
		if (!match)
			std::fprintf(stderr, "native command look expected=(%.9f %.9f) actual=(%.9f %.9f) spring_expected=(%.9f %.9f) spring_actual=(%.9f %.9f)\n",
				referenceLook.yaw, referenceLook.pitch, services.lastYaw, handle.entity->v.v_angle.x,
				referenceSpring.yawVelocity, referenceSpring.pitchVelocity, spring.yawVelocity, spring.pitchVelocity);
		return match;
	};
	services.descentDestinationLook = true;
	nativeStep(90.0f);
	command.advanceUpkeepLook(0U, handle);
	command.executeManagedBotCommand(0U, handle);
	if (!check(matchesNative() &&
			services.lastForward == 96.0f && services.lastSide == -96.0f,
			"descending upkeep turns the view while retaining ladder-local movement"))
		return 1;
	services.globals.time += runtime::BotTimingScheduler::kCommandInterval;
	nativeStep(90.0f);
	command.advanceUpkeepLook(0U, handle);
	const auto haltedDescent = command.beginNeutralCommand(0U, handle, before);
	const ActionProposal haltAction{ActionKind::None, haltedDescent.command.viewAngles, 0U};
	command.finishNeutralCommand(0U, handle, before, haltedDescent, haltAction);
	command.executeManagedBotCommand(0U, handle);
	if (!check(matchesNative() && services.lastForward == 0.0f &&
			services.lastSide == 0.0f && services.descentFallbackLooks == 0,
			"a suppressed Hunt move keeps exactly one descent look step and emits zero movement"))
		return 1;
	services.globals.time += runtime::BotTimingScheduler::kCommandInterval;
	nativeStep(90.0f);
	command.advanceUpkeepLook(0U, handle);
	const auto floorYaw = handle.entity->v.v_angle.y;
	services.descentDestinationLook = false;
	updateHunt();
	const auto floorMove = ActionAdapter::projectMovement(0, -1, 96, referenceLook.yaw, true);
	if (!check(matchesNative() && services.lastYaw == floorYaw &&
			services.lastForward == floorMove.forward && services.lastSide == floorMove.side,
			"the first floor decision projects movement into the view already updated by upkeep"))
		return 1;
	handle.entity->v.movetype = MOVETYPE_WALK;
	services.globals.time += runtime::BotTimingScheduler::kCommandInterval;
	nativeStep(-90.0f);
	command.advanceUpkeepLook(0U, handle);
	command.executeManagedBotCommand(0U, handle);
	if (!check(matchesNative() && services.lastForward == floorMove.forward &&
			services.lastSide == floorMove.side, "the first detached upkeep keeps the spring and last full update's axes"))
		return 1;
	handle.entity->v.flags |= FL_ONGROUND;
	services.globals.time += runtime::BotTimingScheduler::kCommandInterval;
	nativeStep(-90.0f);
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
	services.globals.time += runtime::BotTimingScheduler::kCommandInterval;
	handle.entity->v.movetype = MOVETYPE_FLY;
	handle.entity->v.v_angle = Vector(60.0f, 90.0f, 0.0f);
	referenceLook = {60.0f, 90.0f, 0.0f};
	referenceSpring = command.lookSpring(0U);
	before = services.observation_captureMovementPhysicsState(handle.entity, true, true);
	const auto stoppedLadderDraft = command.beginMovementCommand(0U, handle, token, before, ladder);
	const ActionProposal descentMove{ActionKind::None, stoppedLadderDraft.command.viewAngles, 0U};
	command.finishMovementCommand(0U, handle, token, before, ladder, stoppedLadderDraft, descentMove);
	services.stoppedHuntLookPhase = nav::NativeLadderPhase::Descend;
	services.globals.time += runtime::BotTimingScheduler::kCommandInterval;
	nativeStep(90.0f, 60.0f, true);
	command.advanceUpkeepLook(0U, handle);
	services.stoppedHuntLookPhase = nav::NativeLadderPhase::StoppedHuntLook;
	const auto stopped = command.beginNeutralCommand(0U, handle, before);
	const ActionProposal stoppedAction{ActionKind::None, stopped.command.viewAngles, 0U};
	command.finishNeutralCommand(0U, handle, before, stopped, stoppedAction);
	command.executeManagedBotCommand(0U, handle);
	if (!check(matchesNative() && services.lastButtons == 0U &&
			services.lastForward == 0.0f && services.lastSide == 0.0f,
			"the successful replan keeps the view from upkeep and sends a neutral command"))
		return 1;
	for (unsigned tick = 0; tick < 2; ++tick)
	{
		services.globals.time += runtime::BotTimingScheduler::kCommandInterval;
		nativeStep(90.0f);
		command.advanceUpkeepLook(0U, handle);
		command.executeManagedBotCommand(0U, handle);
		if (!check(matchesNative() && services.lastButtons == 0U &&
				services.lastForward == 0.0f && services.lastSide == 0.0f,
				"neutral light commands resume ordinary pitch with the same spring on the ladder"))
			return 1;
	}
	handle.entity->v.movetype = MOVETYPE_WALK;
	handle.entity->v.flags |= FL_ONGROUND;
	const auto stoppedLook = handle.entity->v.v_angle;
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
	if (!check(handle.entity->v.v_angle.x == stoppedLook.x && handle.entity->v.v_angle.y == stoppedLook.y,
			"Hide does not consume the stopped Hunt look"))
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
	referenceLook = {-4.0f, -170.0f, 0.0f};
	referenceSpring = command.lookSpring(0U);
	nativeStep(-90.0f);
	command.executeManagedBotCommand(0U, handle);
	if (!check(matchesNative() && services.lastSide < 0,
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
	referenceLook = {4.0f, 0.0f, 0.0f};
	referenceSpring = command.lookSpring(0U);
	nativeStep(180.0f);
	command.executeManagedBotCommand(0U, handle);
	if (!check(matchesNative() &&
			services.lastForward < 0, "investigation upkeep turns without changing the last movement axes"))
		return 1;
	investigation.locomotionIntent.direction.x = 1.0f;
	services.globals.time += runtime::BotTimingScheduler::kCommandInterval;
	nativeStep(180.0f);
	updateInvestigation();
	if (!check(matchesNative() && services.lastForward > 0,
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
	referenceLook = {0.0f, 0.0f, 0.0f};
	referenceSpring = command.lookSpring(0U);
	const auto useEye = handle.entity->v.origin + handle.entity->v.view_ofs;
	const auto useGoal = ActionAdapter::useEntityView(facing.useEntityLookTarget.x - useEye.x,
		facing.useEntityLookTarget.y - useEye.y, facing.useEntityLookTarget.z - useEye.z);
	nativeStep(useGoal.yaw, useGoal.pitch);
	command.advanceUpkeepLook(0U, handle);
	command.executeManagedBotCommand(0U, handle);
	if (!check(matchesNative() && (services.lastButtons & IN_USE) == 0U,
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

	command.resetActorExecutionState(0U);
	handle.entity->v.maxspeed = 250.0f;
	services.followWalking = true;
	command.prepareBlindCommand(0U, handle, before, {BlindMove::Backward, true, true}, false);
	command.executeManagedBotCommand(0U, handle);
	if (!check(services.lastForward == -100.0f && services.lastSide == 0.0f &&
			services.lastButtons == (IN_BACK | IN_ATTACK), "blind standing walk dispatches 40 percent exactly once"))
		return 1;
	command.prepareBlindCommand(0U, handle, before, {BlindMove::Left, false, true}, true);
	command.executeManagedBotCommand(0U, handle);
	if (!check(services.lastForward == 0.0f && services.lastSide == -250.0f &&
			services.lastButtons == (IN_MOVELEFT | IN_DUCK), "blind retreat at the existing crouch retains max input"))
		return 1;
	{
		// Native Upkeep owns the view already published this tick. A subsequent
		// MoveTo full update may choose a route goal, but must not snap the view
		// to that route or erase pitch before the next Upkeep.
		ScenarioServices publication;
		publication.surface.setFloat("bot_enable", 1.0f);
		BotCommandRuntime publisher;
		publisher.bindServices(publication);
		auto &actor = publication.handles[0];
		compat::StateUpdateContext frame{};
		frame.actor = {actor.actor.slot, actor.actor.actorGeneration};
		frame.frame = {1U, 1U, 1U};
		frame.timestamp = publication.globals.time;
		frame.fullUpdateSequence = 1U;
		frame.fullUpdate = true;
		publication.stateMachine = compat::CompatibilityStateMachine(frame.actor);
		if (!check(publication.stateMachine.initialize(frame) == compat::StateResult::Transitioned,
				"C16 initialize a live Idle actor"))
			return 1;
		actor.entity->v.v_angle = Vector(-4.0f, 35.0f, 0.0f);
		auto physics = publication.observation_captureMovementPhysicsState(actor.entity, true, true);
		const auto idleDraft = publisher.beginNeutralCommand(0U, actor, physics);
		const ActionProposal idleAction{ActionKind::None, idleDraft.command.viewAngles, 0U};
		publisher.finishNeutralCommand(0U, actor, physics, idleDraft, idleAction);
		publication.globals.time += runtime::BotTimingScheduler::kCommandInterval;
		publisher.advanceUpkeepLook(0U, actor);
		const auto heldView = actor.entity->v.v_angle;
		const auto heldSpring = publisher.lookSpring(0U);
		if (!check(heldSpring.yawVelocity != 0.0f && heldView.x != 0.0f && heldView.y != 90.0f,
				"C16 control: preceding Upkeep has nonzero velocity and a view distinct from the route"))
			return 1;
		frame.frame.tick++;
		frame.fullUpdateSequence++;
		frame.timestamp = publication.globals.time;
		if (!check(publication.stateMachine.transition(compat::CompatibilityStateId::MoveTo,
				compat::StateTransitionReason::ExplicitRequest, compat::StateTransitionId::ExplicitStateChange, frame) ==
				compat::StateResult::Transitioned, "C16 enter ordinary MoveTo after this tick's Upkeep"))
			return 1;
		NavigationUpdate route{};
		route.locomotionIntent.direction = {0.0f, 1.0f, 0.0f};
		route.locomotionIntent.speed = 240.0f;
		physics = publication.observation_captureMovementPhysicsState(actor.entity, true, true);
		const auto routeDraft = publisher.beginMovementCommand(0U, actor,
			publication.lifecycle.tokenForSlot(1U), physics, route);
		const ActionProposal routeAction{ActionKind::None, routeDraft.command.viewAngles, 0U};
		const auto movesBefore = publication.moves;
		publisher.finishMovementCommand(0U, actor, publication.lifecycle.tokenForSlot(1U), physics,
			route, routeDraft, routeAction);
		const auto afterSpring = publisher.lookSpring(0U);
		if (!check(publication.moves == movesBefore + 1U &&
				afterSpring.yawVelocity == heldSpring.yawVelocity && afterSpring.pitchVelocity == heldSpring.pitchVelocity,
				"C16 control: real movement command dispatched without reintegrating the shared spring"))
			return 1;
		if (!check(publication.lastYaw == heldView.y && actor.entity->v.v_angle.x == heldView.x &&
				actor.entity->v.v_angle.y == heldView.y,
				"C16 MoveTo full command preserves the preceding Upkeep view at RunPlayerMove"))
			return 1;
		const float heldForward = publication.lastForward, heldSide = publication.lastSide;
		publication.globals.time += runtime::BotTimingScheduler::kCommandInterval;
		auto expectedSpring = heldSpring;
		const float now = publication.globals.time;
		const auto expected = compat::trackAim(heldView.y, heldView.x,
			90.0f + 2.0f * ActionAdapter::csBotCosine(33.0f * now),
			2.0f * ActionAdapter::csBotCosine(13.0f * now - 90.0f),
			runtime::BotTimingScheduler::kCommandInterval, 1.0f, &expectedSpring, false);
		publisher.advanceUpkeepLook(0U, actor);
		publisher.executeManagedBotCommand(0U, actor);
		if (!check(std::fabs(publication.lastYaw - expected.yaw) < .0001f &&
				std::fabs(actor.entity->v.v_angle.x - expected.pitch) < .0001f &&
				publication.lastForward == heldForward && publication.lastSide == heldSide,
				"C16 next cached Upkeep turns toward the new Ahead with retained full-decision axes"))
			return 1;
		const auto stoppedView = actor.entity->v.v_angle;
		route.locomotionIntent.speed = 0.0f;
		route.locomotionIntent.direction = {};
		physics = publication.observation_captureMovementPhysicsState(actor.entity, true, true);
		const auto stoppedDraft = publisher.beginMovementCommand(0U, actor,
			publication.lifecycle.tokenForSlot(1U), physics, route);
		const ActionProposal publicationStoppedAction{ActionKind::None, stoppedDraft.command.viewAngles, 0U};
		publisher.finishMovementCommand(0U, actor, publication.lifecycle.tokenForSlot(1U), physics,
			route, stoppedDraft, publicationStoppedAction);
		if (!check(publication.lastYaw == stoppedView.y && actor.entity->v.v_angle.x == stoppedView.x &&
				publication.lastForward == 0.0f && publication.lastSide == 0.0f,
				"C16 stopped MoveTo full command retains this Upkeep view"))
			return 1;
		const auto stoppedSpring = publisher.lookSpring(0U);
		publication.globals.time += runtime::BotTimingScheduler::kCommandInterval;
		publisher.advanceUpkeepLook(0U, actor);
		publisher.executeManagedBotCommand(0U, actor);
		if (!check(publication.lastYaw != stoppedView.y &&
				publisher.lookSpring(0U).yawVelocity != stoppedSpring.yawVelocity &&
				publication.lastForward == 0.0f && publication.lastSide == 0.0f,
				"C16 stopped cached Upkeep continues tracking the retained Ahead"))
			return 1;
		const auto followView = actor.entity->v.v_angle;
		frame.frame.tick++;
		frame.fullUpdateSequence++;
		frame.timestamp = publication.globals.time;
		if (!check(publication.stateMachine.transition(compat::CompatibilityStateId::Follow,
				compat::StateTransitionReason::ExplicitRequest, compat::StateTransitionId::ExplicitStateChange, frame) ==
				compat::StateResult::Transitioned, "C16 enter Follow"))
			return 1;
		route.locomotionIntent.speed = 240.0f;
		route.locomotionIntent.direction = {-1.0f, 0.0f, 0.0f};
		const auto followDraft = publisher.beginMovementCommand(0U, actor,
			publication.lifecycle.tokenForSlot(1U), physics, route);
		const ActionProposal followAction{ActionKind::None, followDraft.command.viewAngles, 0U};
		publisher.finishMovementCommand(0U, actor, publication.lifecycle.tokenForSlot(1U), physics,
			route, followDraft, followAction);
		if (!check(publication.lastYaw == followView.y && actor.entity->v.v_angle.x == followView.x,
				"C16 Follow full command also retains the preceding Upkeep view"))
			return 1;
		frame.frame.tick++;
		frame.fullUpdateSequence++;
		if (!check(publication.stateMachine.transition(compat::CompatibilityStateId::EscapeFromBomb,
				compat::StateTransitionReason::ExplicitRequest, compat::StateTransitionId::ExplicitStateChange, frame) ==
				compat::StateResult::Transitioned, "C16 enter EscapeFromBomb"))
			return 1;
		route.locomotionIntent.direction = {1.0f, 0.0f, 0.0f};
		const auto escapeDraft = publisher.beginMovementCommand(0U, actor,
			publication.lifecycle.tokenForSlot(1U), physics, route);
		const ActionProposal escapeAction{ActionKind::None, escapeDraft.command.viewAngles, 0U};
		publisher.finishMovementCommand(0U, actor, publication.lifecycle.tokenForSlot(1U), physics,
			route, escapeDraft, escapeAction);
		const auto escapeView = actor.entity->v.v_angle;
		auto escapeSpring = publisher.lookSpring(0U);
		publication.globals.time += runtime::BotTimingScheduler::kCommandInterval;
		const float escapeAt = publication.globals.time;
		const auto expectedEscape = compat::trackAim(escapeView.y, escapeView.x,
			2.0f * ActionAdapter::csBotCosine(33.0f * escapeAt),
			2.0f * ActionAdapter::csBotCosine(13.0f * escapeAt - 90.0f),
			runtime::BotTimingScheduler::kCommandInterval, 1.0f, &escapeSpring, false);
		publisher.advanceUpkeepLook(0U, actor);
		publisher.executeManagedBotCommand(0U, actor);
		if (!check(std::fabs(publication.lastYaw - expectedEscape.yaw) < .0001f &&
				std::fabs(actor.entity->v.v_angle.x - expectedEscape.pitch) < .0001f &&
				publisher.lookSpring(0U).yawVelocity == escapeSpring.yawVelocity,
				"C16 EscapeFromBomb publishes a route goal for the next cached Upkeep"))
			return 1;
		publication.surface.setString("astrabot_mode", "enhanced");
		route.locomotionIntent.direction = {-1.0f, 0.0f, 0.0f};
		const auto enhancedSpring = publisher.lookSpring(0U);
		const auto enhancedDraft = publisher.beginMovementCommand(0U, actor,
			publication.lifecycle.tokenForSlot(1U), physics, route);
		const ActionProposal enhancedAction{ActionKind::None, enhancedDraft.command.viewAngles, 0U};
		publisher.finishMovementCommand(0U, actor, publication.lifecycle.tokenForSlot(1U), physics,
			route, enhancedDraft, enhancedAction);
		if (!check(std::fabs(std::fabs(publication.lastYaw) - 180.0f) < .0001f &&
				actor.entity->v.v_angle.x == 0.0f &&
				publisher.lookSpring(0U).yawVelocity == enhancedSpring.yawVelocity,
				"C16 Enhanced retains its existing direct movement view without stepping the shared spring"))
			return 1;
	}
	std::puts("runtime command orchestration scenarios: PASS");
	return 0;
}
