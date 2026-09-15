#ifndef ASTRABOT_COMPAT_BOT_CONFIGURATION_HPP
#define ASTRABOT_COMPAT_BOT_CONFIGURATION_HPP

#include "astrabot/compat/command_registry.hpp"
#include "astrabot/compat/cvar_state.hpp"

#include <cstddef>

namespace astrabot
{
namespace compat
{
enum class BotActionResult
{
	Allowed,
	InvalidRequest,
	Disabled,
	Stopped,
	QuotaReached,
	NoTarget,
	NativeGuardDenied
};

class BotConfiguration
{
public:
	BotConfiguration();

	CvarUpdateResult setFloat(const char *name, float value);
	CvarUpdateResult setString(const char *name, const char *value);
	CvarSnapshot snapshot() const;
	BotActionResult authorize(
		const CommandAction &action,
		std::size_t managedActorCount,
		bool nativeGuardAllowsCreation) const;

private:
	static bool isValidTeam(CommandTeam team);

	CvarState cvarState_;
};
}
}

#endif
