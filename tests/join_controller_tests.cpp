#include "astrabot/metamod/join_controller.hpp"

#include <cstdio>

namespace
{
bool check(bool condition, const char *description)
{
	if (condition)
	{
		return true;
	}
	std::fprintf(stderr, "check failed: %s\n", description);
	return false;
}
}

int main()
{
	using astrabot::compat::CommandTeam;
	using astrabot::metamod::JoinActionKind;
	using astrabot::metamod::JoinCommandKind;
	using astrabot::metamod::JoinController;
	using astrabot::runtime::ActorId;

	JoinController controller;
	const ActorId actor{1U, 1U};
	if (!check(controller.begin(actor, CommandTeam::Terrorist, 10U).kind ==
				  JoinActionKind::None,
			  "join starts while waiting for the fake-client menu"))
	{
		return 1;
	}
	if (!check(controller.onFrame(nullptr, 11U).kind == JoinActionKind::None,
			  "first frame does not dispatch before fake-client setup settles"))
	{
		return 1;
	}
	if (!check(controller.onFrame(nullptr, 12U).kind == JoinActionKind::None,
			  "second frame still waits when no menu notification was delivered"))
	{
		return 1;
	}
	const auto teamAction = controller.onFrame(nullptr, 13U);
	if (!check(teamAction.kind == JoinActionKind::SendMenuSelect &&
				 teamAction.command == JoinCommandKind::JoinTeam && teamAction.selection == 1U,
			  "bounded fallback dispatches the terrorist team selection"))
	{
		return 1;
	}
	if (!check(controller.commandCompleted(13U).kind == JoinActionKind::None,
			  "team command completion advances to class selection"))
	{
		return 1;
	}
	if (!check(controller.onFrame(nullptr, 13U).kind == JoinActionKind::None,
			  "class selection is deferred until the next server frame"))
	{
		return 1;
	}
	(void)controller.onMenu(true, 14U);
	const auto classAction = controller.onFrame(nullptr, 14U);
	if (!check(classAction.kind == JoinActionKind::SendMenuSelect &&
				 classAction.command == JoinCommandKind::MenuSelect && classAction.selection == 1U,
			  "class menu notification drives the deferred class selection"))
	{
		return 1;
	}
	if (!check(controller.commandCompleted(13U).kind == JoinActionKind::None,
			  "class command completion enters confirmation"))
	{
		return 1;
	}
	edict_t entity{};
	const auto teamInfo = controller.onTeamInfo("TERRORIST");
	if (!check(teamInfo.kind == JoinActionKind::None,
			  "expected TeamInfo is accepted while confirming the join"))
	{
		return 1;
	}
	if (!check(controller.onFrame(&entity, 14U).kind == JoinActionKind::Joined,
			  "TeamInfo plus a non-spectator entity completes participation"))
	{
		return 1;
	}

	controller.reset();
	if (!check(controller.begin(actor, CommandTeam::Terrorist, 10U).kind ==
				  JoinActionKind::None,
			  "join can be restarted for a fresh actor lifecycle"))
	{
		return 1;
	}
	const auto fallbackTeamAction = controller.onFrame(nullptr, 13U);
	if (!check(fallbackTeamAction.command == JoinCommandKind::JoinTeam,
				 "missing team menu falls back to jointeam"))
	{
		return 1;
	}
	(void)controller.commandCompleted(13U);
	const auto fallbackClassAction = controller.onFrame(nullptr, 16U);
	if (!check(fallbackClassAction.command == JoinCommandKind::JoinClass,
				 "missing class menu falls back to joinclass"))
	{
		return 1;
	}
	(void)controller.commandCompleted(16U);
	return check(controller.onFrame(&entity, 17U).kind == JoinActionKind::Joined,
				 "a non-spectator entity completes participation when TeamInfo is unavailable")
				 ? 0
				 : 1;
}
