#ifndef ASTRABOT_COMPAT_COMMAND_REGISTRY_HPP
#define ASTRABOT_COMPAT_COMMAND_REGISTRY_HPP

#include <cstddef>

namespace astrabot
{
namespace compat
{
	enum class CommandId
	{
		About,
		Add,
		Kick,
		Kill,
		KnivesOnly,
		PistolsOnly,
		SnipersOnly,
		AllWeapons
	};

	enum class CommandTeam
	{
		Any,
		Terrorist,
		CounterTerrorist
	};

	enum class CommandResult
	{
		Handled,
		Unknown,
		InvalidArguments,
		InvalidOutput
	};

	struct CommandRequest
	{
		const char *name;
		std::size_t argumentCount;
		const char *arguments[2];
	};

	struct CommandAction
	{
		CommandId id;
		CommandTeam team;
		bool allTargets;
		const char *target;
	};

	class CommandRegistry
	{
	public:
		CommandResult resolve(
			const CommandRequest &request,
			CommandAction *action) const;

	private:
		static bool isKnownCommand(const char *name, CommandId *id, CommandTeam *team);
		static bool isTargetCommand(CommandId id);
	};
}
}

#endif
