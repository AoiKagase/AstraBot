#ifndef ASTRABOT_METAMOD_COMPAT_SURFACE_HPP
#define ASTRABOT_METAMOD_COMPAT_SURFACE_HPP

#include "astrabot/compat/bot_configuration.hpp"
#include "astrabot/compat/command_registry.hpp"
#include "astrabot/compat/cvar_state.hpp"
#include "astrabot/metamod/profile_loader.hpp"
#include "astrabot/metamod/abi_contract.hpp"

#include <cstddef>

namespace astrabot
{
namespace metamod
{
	class CompatibilitySurface
	{
	public:
		bool registerCommands(enginefuncs_t *engineFunctions, void (*callback)(void)) const;
		compat::CommandResult resolve(
			const compat::CommandRequest &request,
			compat::CommandAction *action) const;
	compat::CvarUpdateResult setFloat(const char *name, float value);
	compat::CvarUpdateResult setString(const char *name, const char *value);
	compat::CvarSnapshot configuration() const;
	compat::BotActionResult authorize(
		const compat::CommandAction &action,
		std::size_t managedActorCount,
		bool nativeGuardAllowsCreation) const;
	ProfileLoadResult loadProfiles(const char *path);
	compat::ProfileSelectionResult selectProfile(
		const compat::CommandAction &action,
		int difficulty,
		std::size_t selectionIndex,
		compat::ProfileRecord *profile) const;

	private:
		static const char *const *commandNames(std::size_t *count);
	compat::CommandRegistry commandRegistry_;
	compat::BotConfiguration botConfiguration_;
	compat::ProfileCatalog profileCatalog_;
	ProfileLoader profileLoader_;
};
}
}

#endif
