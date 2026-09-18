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
	if (!check(teamAction.kind == JoinActionKind::SendMenuSelect &&
				  teamAction.command == JoinCommandKind::JoinTeam &&
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
	if (!check(controller.onMenu(JoinMenuKind::TerroristClass, 1U, 14U,
					  astrabot::metamod::JoinMenuSource::LegacyShowMenu).kind ==
				  JoinActionKind::None,
			  "matching terrorist class menu is accepted"))
	{
		return false;
	}
	const auto classAction = controller.onFrame(14U);
	if (!check(classAction.kind == JoinActionKind::SendMenuSelect &&
				  classAction.command == JoinCommandKind::MenuSelect &&
				  classAction.selection == 1U,
			  "class menu emits selection through menuselect"))
	{
		return false;
	}
	return check(controller.commandCompleted(14U).kind == JoinActionKind::None,
				 "class command completion enters confirmation");
}
}

int main()
{
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
	if (!check(controller.begin(actor, CommandTeam::Terrorist, 10U).kind ==
				   JoinActionKind::None,
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
	if (!completeTeamSelection(controller) ||
		controller.onTeamInfo("TERRORIST").kind != JoinActionKind::None ||
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
	if (!check(controller.begin(actor, CommandTeam::Terrorist, 10U).kind ==
				   JoinActionKind::None,
			  "join can restart for a fresh actor lifecycle"))
	{
		return 1;
	}
	if (!completeTeamSelection(controller) ||
		controller.onTeamInfo("TERRORIST").kind != JoinActionKind::None ||
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
	if (!check(controller.begin(actor, CommandTeam::Terrorist, 10U).kind ==
				   JoinActionKind::None,
			  "TeamInfo gate scenario starts"))
	{
		return 1;
	}
	const auto directTeamAction = controller.onFrame(13U);
	if (!check(directTeamAction.command == JoinCommandKind::JoinTeam,
			  "missing team menu uses public jointeam command"))
	{
		return 1;
	}
	controller.commandCompleted(13U);
	if (!check(controller.onFrame(16U).kind == JoinActionKind::None,
			  "class command waits for matching TeamInfo"))
	{
		return 1;
	}
	if (!check(controller.onTeamInfo("TERRORIST").kind == JoinActionKind::None,
			  "matching TeamInfo releases class command gate"))
	{
		return 1;
	}
	const auto directClassAction = controller.onFrame(17U);
	if (!check(directClassAction.command == JoinCommandKind::JoinClass &&
				  directClassAction.selection == 1U,
				"missing class menu uses public joinclass command"))
	{
		return 1;
	}

	controller.reset();
	if (!check(controller.begin(actor, CommandTeam::Terrorist, 20U).kind ==
				   JoinActionKind::None,
				"same-dispatch class menu scenario starts"))
	{
		return 1;
	}
	controller.onMenu(JoinMenuKind::Team, 1U, 21U, JoinMenuSource::LegacyShowMenu);
	const auto menuTeamAction = controller.onFrame(21U);
	if (!check(menuTeamAction.command == JoinCommandKind::MenuSelect,
				"legacy team menu uses menuselect"))
	{
		return 1;
	}
	controller.onMenu(JoinMenuKind::TerroristClass, 1U, 21U,
						 JoinMenuSource::LegacyShowMenu);
	controller.onTeamInfo("TERRORIST");
	controller.commandCompleted(21U);
	const auto sameDispatchClassAction = controller.onFrame(24U);
	if (!check(sameDispatchClassAction.command == JoinCommandKind::MenuSelect,
				"class menu delivered during team dispatch is preserved"))
	{
		return 1;
	}

	controller.reset();
	if (!check(controller.begin(actor, CommandTeam::Terrorist, 20U).kind ==
				   JoinActionKind::None,
			  "VGUI menu scenario starts"))
	{
		return 1;
	}
	if (!check(controller.onMenu(JoinMenuKind::Team, 1U, 21U, JoinMenuSource::Vgui).kind ==
				   JoinActionKind::None,
			  "VGUI team menu is recorded"))
	{
		return 1;
	}
	const auto vguiTeamAction = controller.onFrame(21U);
	if (!check(vguiTeamAction.command == JoinCommandKind::JoinTeam,
			  "VGUI team menu uses public jointeam command"))
	{
		return 1;
	}
	controller.commandCompleted(21U);
	controller.onTeamInfo("TERRORIST");
	if (!check(controller.onMenu(JoinMenuKind::TerroristClass, 1U, 22U,
					  JoinMenuSource::Vgui).kind == JoinActionKind::None,
			  "VGUI terrorist class menu is recorded"))
	{
		return 1;
	}
	const auto vguiClassAction = controller.onFrame(22U);
	if (!check(vguiClassAction.command == JoinCommandKind::JoinClass,
			  "VGUI class menu uses public joinclass command"))
	{
		return 1;
	}

	controller.reset();
	if (!check(controller.begin(actor, CommandTeam::Terrorist, 10U).kind ==
				   JoinActionKind::None,
			  "wrong class menu scenario starts"))
	{
		return 1;
	}
	if (!check(controller.onMenu(JoinMenuKind::CounterTerroristClass, 1U, 11U).kind ==
				   JoinActionKind::None &&
				  controller.phase() == astrabot::metamod::JoinPhase::WaitingTeamMenu,
			  "counter-terrorist class menu is ignored for terrorist request"))
	{
		return 1;
	}

	controller.reset();
	if (!check(controller.begin(actor, CommandTeam::CounterTerrorist, 10U).kind ==
				   JoinActionKind::None,
			  "counter-terrorist wrong class menu scenario starts"))
	{
		return 1;
	}
	if (!check(controller.onMenu(JoinMenuKind::TerroristClass, 1U, 11U).kind ==
				   JoinActionKind::None &&
				  controller.phase() == astrabot::metamod::JoinPhase::WaitingTeamMenu,
			  "terrorist class menu is ignored for counter-terrorist request"))
	{
		return 1;
	}

	return check(controller.onFrame(138U).kind == JoinActionKind::Failed,
				 "join timeout terminates instead of retrying forever")
			   ? 0
			   : 1;
}
