#include "astrabot/compat/command_registry.hpp"

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

	astrabot::compat::CommandRequest request(
		const char *name,
		std::size_t argumentCount,
		const char *firstArgument = nullptr,
		const char *secondArgument = nullptr)
	{
		return {name, argumentCount, {firstArgument, secondArgument}};
	}
}

int main()
{
	using astrabot::compat::CommandAction;
	using astrabot::compat::CommandId;
	using astrabot::compat::CommandRegistry;
	using astrabot::compat::CommandResult;
	using astrabot::compat::CommandTeam;

	const CommandRegistry registry;
	CommandAction action{};
	if (!check(registry.resolve(request("bot_add", 0U), &action) == CommandResult::Handled,
			"bot_add is handled"))
	{
		return 1;
	}
	if (!check(action.id == CommandId::Add && action.team == CommandTeam::Any,
			"bot_add selects any team"))
	{
		return 1;
	}
	if (!check(registry.resolve(request("bot_add_t", 0U), &action) == CommandResult::Handled &&
			action.team == CommandTeam::Terrorist, "bot_add_t selects terrorist team"))
	{
		return 1;
	}
	if (!check(registry.resolve(request("bot_add_ct", 0U), &action) == CommandResult::Handled &&
			action.team == CommandTeam::CounterTerrorist,
			"bot_add_ct selects counter-terrorist team"))
	{
		return 1;
	}
	if (!check(registry.resolve(request("bot_kick", 1U, "all"), &action) ==
			CommandResult::Handled && action.id == CommandId::Kick && action.allTargets,
			"bot_kick all is handled"))
	{
		return 1;
	}
	if (!check(registry.resolve(request("bot_kill", 1U, "Astra-1"), &action) ==
			CommandResult::Handled && action.id == CommandId::Kill && !action.allTargets,
			"bot_kill named target is handled"))
	{
		return 1;
	}
	if (!check(registry.resolve(request("BOT_ADD", 0U), &action) == CommandResult::Unknown,
			"command matching remains case-sensitive"))
	{
		return 1;
	}
	if (!check(registry.resolve(request("bot_add", 2U, "profile", "extra"), &action) ==
			CommandResult::InvalidArguments, "bot_add rejects extra arguments"))
	{
		return 1;
	}
	if (!check(registry.resolve(request("bot_unknown", 0U), &action) == CommandResult::Unknown,
			"unknown command is rejected"))
	{
		return 1;
	}
	return 0;
}
