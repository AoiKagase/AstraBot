#include "astrabot/compat/command_registry.hpp"

#include <cstring>

namespace astrabot
{
namespace compat
{
	CommandResult CommandRegistry::resolve(
		const CommandRequest &request,
		CommandAction *action) const
	{
		if (request.name == nullptr || action == nullptr || request.argumentCount > 2U)
		{
			return request.name == nullptr || action == nullptr ?
				CommandResult::InvalidOutput : CommandResult::InvalidArguments;
		}

		CommandId id = CommandId::About;
		CommandTeam team = CommandTeam::Any;
		if (!isKnownCommand(request.name, &id, &team))
		{
			return CommandResult::Unknown;
		}

		const bool targetCommand = isTargetCommand(id);
		const bool acceptsOptionalTarget = id == CommandId::Add;
		const std::size_t maximumArguments = acceptsOptionalTarget || targetCommand ? 1U : 0U;
		if (request.argumentCount > maximumArguments)
		{
			return CommandResult::InvalidArguments;
		}

		action->id = id;
		action->team = team;
		action->allTargets = false;
		action->target = nullptr;
		if (request.argumentCount == 1U)
		{
			if (request.arguments[0] == nullptr || request.arguments[0][0] == '\0')
			{
				return CommandResult::InvalidArguments;
			}
			if (targetCommand && std::strcmp(request.arguments[0], "all") == 0)
			{
				action->allTargets = true;
			}
			else
			{
				action->target = request.arguments[0];
			}
		}
		return CommandResult::Handled;
	}

	bool CommandRegistry::isKnownCommand(
		const char *name,
		CommandId *id,
		CommandTeam *team)
	{
		if (name == nullptr || id == nullptr || team == nullptr)
		{
			return false;
		}
		struct CommandName
		{
			const char *name;
			CommandId id;
			CommandTeam team;
		};
		static const CommandName kCommands[] = {
			{"bot_about", CommandId::About, CommandTeam::Any},
			{"bot_add", CommandId::Add, CommandTeam::Any},
			{"bot_add_t", CommandId::Add, CommandTeam::Terrorist},
			{"bot_add_ct", CommandId::Add, CommandTeam::CounterTerrorist},
			{"bot_kick", CommandId::Kick, CommandTeam::Any},
			{"bot_kill", CommandId::Kill, CommandTeam::Any},
			{"bot_knives_only", CommandId::KnivesOnly, CommandTeam::Any},
			{"bot_pistols_only", CommandId::PistolsOnly, CommandTeam::Any},
			{"bot_snipers_only", CommandId::SnipersOnly, CommandTeam::Any},
			{"bot_all_weapons", CommandId::AllWeapons, CommandTeam::Any}
		};
		for (const CommandName &command : kCommands)
		{
			if (std::strcmp(name, command.name) == 0)
			{
				*id = command.id;
				*team = command.team;
				return true;
			}
		}
		return false;
	}

	bool CommandRegistry::isTargetCommand(CommandId id)
	{
		return id == CommandId::Kick || id == CommandId::Kill;
	}
}
}
