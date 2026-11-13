#include "astrabot/metamod/join_controller.hpp"

#include <cstdio>

namespace
{
edict_t makeReadyEntity()
{
	edict_t entity{};
	entity.v.flags = FL_CLIENT | FL_FAKECLIENT;
	entity.v.deadflag = DEAD_NO;
	entity.v.health = 100.0f;
	entity.v.team = 0;
	entity.v.solid = SOLID_SLIDEBOX;
	entity.v.movetype = MOVETYPE_WALK;
	return entity;
}

bool check(bool condition, const char *description)
{
	if (condition)
	{
		return true;
	}
	std::fprintf(stderr, "check failed: %s\n", description);
	return false;
}

bool completeTeamSelection(astrabot::metamod::JoinController &controller)
{
	using astrabot::metamod::JoinActionKind;
	using astrabot::metamod::JoinCommandKind;
	const auto teamAction = controller.onFrame(13U);
	if (!check(teamAction.kind == JoinActionKind::SendMenuSelect && teamAction.command == JoinCommandKind::JoinTeam &&
				teamAction.selection == 1U,
			"fallback emits direct terrorist team selection"))
	{
		return false;
	}
	return check(controller.commandCompleted(13U).kind == JoinActionKind::None,
		"team command completion advances class selection");
}

bool completeClassSelection(astrabot::metamod::JoinController &controller)
{
	using astrabot::metamod::JoinActionKind;
	using astrabot::metamod::JoinCommandKind;
	using astrabot::metamod::JoinMenuKind;
	if (!check(
			controller.onMenu(JoinMenuKind::TerroristClass, 1U, 14U, astrabot::metamod::JoinMenuSource::LegacyShowMenu)
					.kind == JoinActionKind::None,
			"matching terrorist class menu is accepted"))
	{
		return false;
	}
	const auto classAction = controller.onFrame(14U);
	if (!check(classAction.kind == JoinActionKind::SendMenuSelect &&
				classAction.command == JoinCommandKind::MenuSelect && classAction.selection == 1U,
			"class menu emits selection through menuselect"))
	{
		return false;
	}
	return check(
		controller.commandCompleted(14U).kind == JoinActionKind::None, "class command completion enters confirmation");
}

bool testDelayedClassRejection()
{
	using namespace astrabot::metamod;
	JoinController controller;
	controller.begin({3U, 2U}, astrabot::compat::CommandTeam::Terrorist, 10U);
	if (!completeTeamSelection(controller))
		return false;
	controller.onTeamInfo("TERRORIST");
	if (!completeClassSelection(controller))
		return false;
	controller.onMenu(JoinMenuKind::TerroristClass, 1U, 15U, JoinMenuSource::LegacyShowMenu);
	auto ready = makeReadyEntity();
	if (!check(controller.phase() == JoinPhase::WaitingClassMenu && !controller.classSelectionCompleted() &&
				controller.onFrame(&ready, 45U).kind == JoinActionKind::None &&
				controller.onFrame(46U).kind == JoinActionKind::SendMenuSelect,
			"delayed class rejection reopens admission without accepting readiness or bypassing cooldown"))
		return false;
	controller.commandCompleted(46U);
	controller.onMenu(JoinMenuKind::TerroristClass, 1U, 47U, JoinMenuSource::LegacyShowMenu);
	for (unsigned frame = 48U; frame < 78U; ++frame)
	{
		controller.onTeamInfo("TERRORIST");
		controller.onMenu(JoinMenuKind::TerroristClass, 1U, frame, JoinMenuSource::LegacyShowMenu);
		if (!check(controller.onFrame(frame).kind == JoinActionKind::None && controller.attempts() == 2U,
				"duplicate delayed class menus retain cooldown and finite budget"))
			return false;
	}
	if (!check(controller.onFrame(78U).kind == JoinActionKind::SendMenuSelect,
			"third delayed class attempt respects cooldown"))
		return false;
	controller.commandCompleted(78U);
	controller.onMenu(JoinMenuKind::TerroristClass, 1U, 79U, JoinMenuSource::LegacyShowMenu);
	if (!check(controller.onFrame(110U).error == JoinError::CommandAttemptsExhausted && !controller.active(),
			"persistent delayed class rejection terminates instead of waiting forever"))
		return false;

	controller.reset();
	controller.begin({3U, 3U}, astrabot::compat::CommandTeam::Terrorist, 10U);
	if (!completeTeamSelection(controller))
		return false;
	controller.onTeamInfo("TERRORIST");
	if (!completeClassSelection(controller))
		return false;
	if (!check(controller.onFrame(1000U).kind == JoinActionKind::None &&
				controller.phase() == JoinPhase::WaitingConfirmation,
			"silent respawn confirmation may still wait beyond the admission deadline"))
		return false;
	controller.onMenu(JoinMenuKind::TerroristClass, 1U, 1001U, JoinMenuSource::Vgui);
	const auto late = controller.onFrame(1001U);
	if (!check(late.kind == JoinActionKind::SendMenuSelect && late.command == JoinCommandKind::JoinClass,
			"explicit rejection after a long spawn wait has a bounded chance to retry"))
		return false;
	controller.commandCompleted(1001U);
	return check(controller.onFrame(&ready, 1002U).kind == JoinActionKind::Joined,
		"delayed class retry can finish with the same actor");
}

bool testDelayedTeamRejection()
{
	using namespace astrabot::metamod;
	JoinController controller;
	controller.begin({3U, 4U}, astrabot::compat::CommandTeam::Terrorist, 10U);
	if (!completeTeamSelection(controller))
		return false;
	controller.onMenu(JoinMenuKind::Team, 0xFFFFU, 14U, JoinMenuSource::LegacyShowMenu);
	if (!check(controller.phase() == JoinPhase::WaitingTeamMenu &&
				controller.onFrame(44U).kind == JoinActionKind::None &&
				controller.onFrame(45U).kind == JoinActionKind::SendMenuSelect,
			"delayed team rejection retries at the original cooldown instead of timing out"))
		return false;
	controller.commandCompleted(45U);
	controller.onMenu(JoinMenuKind::Team, 0xFFFFU, 46U, JoinMenuSource::Vgui);
	controller.onTeamInfo("TERRORIST");
	controller.onMenu(JoinMenuKind::Team, 0xFFFFU, 47U, JoinMenuSource::LegacyShowMenu);
	controller.onMenu(JoinMenuKind::TerroristClass, 1U, 47U, JoinMenuSource::LegacyShowMenu);
	const auto klass = controller.onFrame(47U);
	if (!check(klass.kind == JoinActionKind::SendMenuSelect && controller.phase() == JoinPhase::ClassCommandPending,
			"positive TeamInfo cancels a delayed team retry and stale team menus cannot undo progress"))
		return false;
	controller.commandCompleted(47U);
	auto ready = makeReadyEntity();
	if (!check(controller.onFrame(&ready, 48U).kind == JoinActionKind::Joined,
			"delayed team retry finishes without recreating the actor"))
		return false;
	controller.reset();
	controller.begin({3U, 5U}, astrabot::compat::CommandTeam::CounterTerrorist, 10U);
	controller.onFrame(13U);
	controller.commandCompleted(13U);
	controller.onMenu(JoinMenuKind::Team, 0xFFFFU, 14U, JoinMenuSource::Vgui);
	controller.cancel(JoinError::Disconnected);
	if (!check(controller.onFrame(45U).kind == JoinActionKind::None, "disconnect cancels a pending delayed retry"))
		return false;
	controller.reset();
	controller.begin({3U, 6U}, astrabot::compat::CommandTeam::CounterTerrorist, 20U);
	controller.onMenu(JoinMenuKind::Team, 0xFFFFU, 21U, JoinMenuSource::Vgui);
	return check(controller.onFrame(21U).kind == JoinActionKind::SendMenuSelect && controller.attempts() == 1U,
		"slot reuse starts with a fresh cooldown and retry budget");
}

bool testRejectedTeamRetry()
{
	using namespace astrabot::metamod;
	using astrabot::compat::CommandTeam;
	JoinController controller;
	controller.begin({3U, 2U}, CommandTeam::Terrorist, 100U);
	controller.onMenu(JoinMenuKind::Team, 0xFFFFU, 101U, JoinMenuSource::LegacyShowMenu);
	if (!check(controller.onFrame(101U).kind == JoinActionKind::SendMenuSelect,
			"initial team menu dispatches immediately"))
		return false;
	// ReGameDLL rejects a stacked team with #Too_Many_Terrorists and
	// synchronously displays the same menu. Repeated prompts are not progress.
	controller.onMenu(JoinMenuKind::Team, 0xFFFFU, 101U, JoinMenuSource::LegacyShowMenu);
	controller.commandCompleted(101U);
	for (unsigned frame = 102U; frame < 133U; ++frame)
	{
		controller.onMenu(JoinMenuKind::Team, 0xFFFFU, frame, JoinMenuSource::LegacyShowMenu);
		if (!check(controller.onFrame(frame).kind == JoinActionKind::None && controller.attempts() == 1U,
				"rejected team and duplicate prompts cannot consume retries before cooldown"))
			return false;
	}
	if (!check(controller.onFrame(133U).kind == JoinActionKind::SendMenuSelect,
			"team retry starts exactly at its bounded cooldown"))
		return false;
	controller.onMenu(JoinMenuKind::Team, 0xFFFFU, 133U, JoinMenuSource::LegacyShowMenu);
	controller.commandCompleted(133U);
	if (!check(controller.onFrame(164U).kind == JoinActionKind::None &&
				controller.onFrame(165U).kind == JoinActionKind::SendMenuSelect,
			"third team attempt remains spaced"))
		return false;
	controller.onTeamInfo("TERRORIST");
	controller.commandCompleted(165U);
	controller.onMenu(JoinMenuKind::TerroristClass, 1U, 166U, JoinMenuSource::LegacyShowMenu);
	if (!check(controller.onFrame(166U).kind == JoinActionKind::SendMenuSelect,
			"successful third team attempt leaves a fresh class selection budget"))
		return false;
	controller.onMenu(JoinMenuKind::TerroristClass, 1U, 166U, JoinMenuSource::LegacyShowMenu);
	controller.commandCompleted(166U);
	for (unsigned frame = 167U; frame < 198U; ++frame)
	{
		controller.onTeamInfo("TERRORIST");
		controller.onMenu(JoinMenuKind::TerroristClass, 1U, frame, JoinMenuSource::LegacyShowMenu);
		if (!check(controller.onFrame(frame).kind == JoinActionKind::None && controller.attempts() == 1U,
				"duplicate TeamInfo and class menus do not refill or bypass retry budget"))
			return false;
	}
	if (!check(controller.onFrame(198U).kind == JoinActionKind::SendMenuSelect, "class retry also respects cooldown"))
		return false;
	controller.commandCompleted(198U);
	auto ready = makeReadyEntity();
	if (!check(controller.onFrame(&ready, 199U).kind == JoinActionKind::Joined,
			"temporarily rejected bot eventually joins without recreating its actor"))
		return false;

	controller.reset();
	controller.begin({3U, 3U}, CommandTeam::CounterTerrorist, 300U);
	for (unsigned attempt = 0; attempt < 3; ++attempt)
	{
		const unsigned frame = 301U + 32U * attempt;
		controller.onMenu(JoinMenuKind::Team, 0xFFFFU, frame, JoinMenuSource::LegacyShowMenu);
		if (!check(controller.onFrame(frame).kind == JoinActionKind::SendMenuSelect,
				"persistent rejection has a bounded number of paced sends"))
			return false;
		controller.onMenu(JoinMenuKind::Team, 0xFFFFU, frame, JoinMenuSource::LegacyShowMenu);
		controller.commandCompleted(frame);
	}
	if (!check(controller.onFrame(396U).kind == JoinActionKind::None &&
				controller.onFrame(397U).error == JoinError::CommandAttemptsExhausted,
			"persistent team rejection exhausts only after paced attempts"))
		return false;
	controller.reset();
	controller.begin({3U, 4U}, CommandTeam::Terrorist, 500U);
	controller.onMenu(JoinMenuKind::Team, 0xFFFFU, 501U);
	controller.onFrame(501U);
	controller.onMenu(JoinMenuKind::Team, 0xFFFFU, 501U);
	controller.commandCompleted(501U);
	controller.onTeamInfo("TERRORIST");
	controller.onMenu(JoinMenuKind::TerroristClass, 1U, 502U);
	const auto late = controller.onFrame(502U);
	if (!check(late.kind == JoinActionKind::SendMenuSelect && late.command == JoinCommandKind::JoinClass,
			"delayed positive TeamInfo cancels the pending team retry and advances class selection"))
		return false;
	controller.reset();
	controller.begin({3U, 5U}, CommandTeam::Terrorist, 600U);
	controller.onMenu(JoinMenuKind::Team, 0xFFFFU, 601U, JoinMenuSource::LegacyShowMenu);
	controller.onFrame(601U);
	controller.onMenu(JoinMenuKind::Team, 0xFFFFU, 601U, JoinMenuSource::LegacyShowMenu);
	controller.onTeamInfo("TERRORIST");
	controller.onMenu(JoinMenuKind::TerroristClass, 1U, 601U, JoinMenuSource::LegacyShowMenu);
	controller.commandCompleted(601U);
	const auto synchronous = controller.onFrame(604U);
	return check(synchronous.kind == JoinActionKind::SendMenuSelect &&
			synchronous.command == JoinCommandKind::MenuSelect && controller.phase() == JoinPhase::ClassCommandPending,
		"matching TeamInfo takes priority over an earlier repeated team prompt in the same dispatch");
}
}

int main()
{
	const bool delayedClass = testDelayedClassRejection();
	const bool delayedTeam = testDelayedTeamRejection();
	if (!delayedClass || !delayedTeam)
		return 1;
	if (!testRejectedTeamRetry())
		return 1;
	using astrabot::compat::CommandTeam;
	using astrabot::metamod::JoinActionKind;
	using astrabot::metamod::JoinCommandKind;
	using astrabot::metamod::JoinController;
	using astrabot::metamod::JoinMenuKind;
	using astrabot::metamod::JoinMenuSource;
	using astrabot::runtime::ActorId;

	const ActorId actor{1U, 1U};
	JoinController controller;
	edict_t readyEntity = makeReadyEntity();
	if (!check(controller.begin(actor, CommandTeam::Terrorist, 10U).kind == JoinActionKind::None,
			"join starts while waiting for fake-client menu"))
	{
		return 1;
	}
	if (!check(controller.onFrame(11U).kind == JoinActionKind::None &&
				controller.onFrame(12U).kind == JoinActionKind::None,
			"fallback waits bounded menu grace period"))
	{
		return 1;
	}
	if (!completeTeamSelection(controller) || controller.onTeamInfo("TERRORIST").kind != JoinActionKind::None ||
		!completeClassSelection(controller))
	{
		return 1;
	}
	if (!check(controller.onFrame(15U).kind == JoinActionKind::None &&
				controller.onFrame(&readyEntity, 16U).kind == JoinActionKind::Joined,
			"join completes after TeamInfo and one post-class frame"))
	{
		return 1;
	}

	controller.reset();
	if (!check(controller.begin(actor, CommandTeam::Terrorist, 10U).kind == JoinActionKind::None,
			"join can restart for a fresh actor lifecycle"))
	{
		return 1;
	}
	if (!completeTeamSelection(controller) || controller.onTeamInfo("TERRORIST").kind != JoinActionKind::None ||
		!completeClassSelection(controller))
	{
		return 1;
	}
	edict_t deadEntity = makeReadyEntity();
	deadEntity.v.flags |= FL_SPECTATOR;
	deadEntity.v.deadflag = DEAD_DEAD;
	deadEntity.v.health = 0.0f;
	deadEntity.v.team = 0;
	if (!check(controller.onFrame(&deadEntity, 15U).kind == JoinActionKind::None &&
				controller.onFrame(&deadEntity, 16U).kind == JoinActionKind::None,
			"join remains pending until physical spawn readiness"))
	{
		return 1;
	}
	if (!check(controller.onFrame(&deadEntity, 22U).kind == JoinActionKind::None &&
				controller.onFrame(&deadEntity, 138U).kind == JoinActionKind::None &&
				controller.phase() == astrabot::metamod::JoinPhase::WaitingConfirmation,
			"unspawned join waits across round transitions without rejoining"))
	{
		return 1;
	}

	controller.reset();
	if (!check(controller.begin(actor, CommandTeam::Terrorist, 10U).kind == JoinActionKind::None,
			"TeamInfo gate scenario starts"))
	{
		return 1;
	}
	const auto directTeamAction = controller.onFrame(13U);
	if (!check(directTeamAction.command == JoinCommandKind::JoinTeam, "missing team menu uses public jointeam command"))
	{
		return 1;
	}
	controller.commandCompleted(13U);
	if (!check(controller.onFrame(16U).kind == JoinActionKind::None, "class command waits for matching TeamInfo"))
	{
		return 1;
	}
	if (!check(controller.onTeamInfo("TERRORIST").kind == JoinActionKind::None,
			"matching TeamInfo releases class command gate"))
	{
		return 1;
	}
	const auto directClassAction = controller.onFrame(17U);
	if (!check(directClassAction.command == JoinCommandKind::JoinClass && directClassAction.selection == 1U,
			"missing class menu uses public joinclass command"))
	{
		return 1;
	}

	controller.reset();
	if (!check(controller.begin(actor, CommandTeam::Terrorist, 20U).kind == JoinActionKind::None,
			"same-dispatch class menu scenario starts"))
	{
		return 1;
	}
	controller.onMenu(JoinMenuKind::Team, 1U, 21U, JoinMenuSource::LegacyShowMenu);
	const auto menuTeamAction = controller.onFrame(21U);
	if (!check(menuTeamAction.command == JoinCommandKind::MenuSelect, "legacy team menu uses menuselect"))
	{
		return 1;
	}
	controller.onMenu(JoinMenuKind::TerroristClass, 1U, 21U, JoinMenuSource::LegacyShowMenu);
	controller.onTeamInfo("TERRORIST");
	controller.commandCompleted(21U);
	const auto sameDispatchClassAction = controller.onFrame(24U);
	if (!check(sameDispatchClassAction.command == JoinCommandKind::MenuSelect,
			"class menu delivered during team dispatch is preserved"))
	{
		return 1;
	}

	controller.reset();
	if (!check(controller.begin(actor, CommandTeam::Terrorist, 20U).kind == JoinActionKind::None,
			"VGUI menu scenario starts"))
	{
		return 1;
	}
	if (!check(controller.onMenu(JoinMenuKind::Team, 1U, 21U, JoinMenuSource::Vgui).kind == JoinActionKind::None,
			"VGUI team menu is recorded"))
	{
		return 1;
	}
	const auto vguiTeamAction = controller.onFrame(21U);
	if (!check(vguiTeamAction.command == JoinCommandKind::JoinTeam, "VGUI team menu uses public jointeam command"))
	{
		return 1;
	}
	controller.commandCompleted(21U);
	controller.onTeamInfo("TERRORIST");
	if (!check(
			controller.onMenu(JoinMenuKind::TerroristClass, 1U, 22U, JoinMenuSource::Vgui).kind == JoinActionKind::None,
			"VGUI terrorist class menu is recorded"))
	{
		return 1;
	}
	const auto vguiClassAction = controller.onFrame(22U);
	if (!check(vguiClassAction.command == JoinCommandKind::JoinClass, "VGUI class menu uses public joinclass command"))
	{
		return 1;
	}

	controller.reset();
	if (!check(controller.begin(actor, CommandTeam::Terrorist, 10U).kind == JoinActionKind::None,
			"wrong class menu scenario starts"))
	{
		return 1;
	}
	if (!check(controller.onMenu(JoinMenuKind::CounterTerroristClass, 1U, 11U).kind == JoinActionKind::None &&
				controller.phase() == astrabot::metamod::JoinPhase::WaitingTeamMenu,
			"counter-terrorist class menu is ignored for terrorist request"))
	{
		return 1;
	}

	controller.reset();
	if (!check(controller.begin(actor, CommandTeam::CounterTerrorist, 10U).kind == JoinActionKind::None,
			"counter-terrorist wrong class menu scenario starts"))
	{
		return 1;
	}
	if (!check(controller.onMenu(JoinMenuKind::TerroristClass, 1U, 11U).kind == JoinActionKind::None &&
				controller.phase() == astrabot::metamod::JoinPhase::WaitingTeamMenu,
			"terrorist class menu is ignored for counter-terrorist request"))
	{
		return 1;
	}

	astrabot::metamod::DeferredJoinActionQueue deferredActions;
	const astrabot::runtime::ActorId deferredActor{8U, 1U};
	const astrabot::runtime::LifecycleToken deferredToken{1U, 2U, 1U, 8U};
	const auto wrongTeam = astrabot::metamod::JoinAction::failed(astrabot::metamod::JoinError::WrongTeam);
	if (!check(deferredActions.enqueue(deferredActor, deferredToken, 11062U, wrongTeam),
			"message callback can defer wrong-team cleanup"))
	{
		return 1;
	}
	astrabot::metamod::DeferredJoinAction pending{};
	if (!check(deferredActions.size() == 1U && deferredActions.dequeue(&pending) &&
				pending.actor.slot == deferredActor.slot &&
				pending.actor.actorGeneration == deferredActor.actorGeneration &&
				pending.action.kind == JoinActionKind::Failed &&
				pending.action.error == astrabot::metamod::JoinError::WrongTeam && pending.mapGeneration == 1U &&
				pending.slotGeneration == 1U,
			"deferred join action retains actor generation and FIFO payload"))
	{
		return 1;
	}
	return check(controller.onFrame(138U).kind == JoinActionKind::Failed,
			   "join timeout terminates instead of retrying forever")
		? 0
		: 1;
}
