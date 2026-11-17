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
	std::puts("runtime command orchestration scenarios: PASS");
	return 0;
}
