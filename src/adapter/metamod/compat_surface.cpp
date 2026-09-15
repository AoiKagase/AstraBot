#include "astrabot/metamod/compat_surface.hpp"

namespace astrabot
{
namespace metamod
{
	bool CompatibilitySurface::registerCommands(
		enginefuncs_t *engineFunctions,
		void (*callback)(void)) const
	{
		if (engineFunctions == nullptr || engineFunctions->pfnAddServerCommand == nullptr ||
				callback == nullptr)
		{
			return false;
		}

		std::size_t count = 0U;
		const char *const *names = commandNames(&count);
		for (std::size_t index = 0U; index < count; ++index)
		{
			engineFunctions->pfnAddServerCommand(
				const_cast<char *>(names[index]), callback);
		}
		return true;
	}

	compat::CommandResult CompatibilitySurface::resolve(
		const compat::CommandRequest &request,
		compat::CommandAction *action) const
	{
		return commandRegistry_.resolve(request, action);
	}

	compat::CvarUpdateResult CompatibilitySurface::setFloat(const char *name, float value)
	{
		return cvarState_.setFloat(name, value);
	}

	compat::CvarUpdateResult CompatibilitySurface::setString(
		const char *name,
		const char *value)
	{
		return cvarState_.setString(name, value);
	}

	ProfileLoadResult CompatibilitySurface::loadProfiles(const char *path)
	{
		return profileLoader_.loadFile(path, &profileCatalog_);
	}

	compat::ProfileSelectionResult CompatibilitySurface::selectProfile(
		const compat::CommandAction &action,
		int difficulty,
		std::size_t selectionIndex,
		compat::ProfileRecord *profile) const
	{
		compat::ProfileTeam team = compat::ProfileTeam::Any;
		if (action.team == compat::CommandTeam::Terrorist)
		{
			team = compat::ProfileTeam::Terrorist;
		}
		else if (action.team == compat::CommandTeam::CounterTerrorist)
		{
			team = compat::ProfileTeam::CounterTerrorist;
		}
		if (action.target != nullptr)
		{
			return profileCatalog_.selectNamed(action.target, team, profile);
		}
		return profileCatalog_.select(team, difficulty, selectionIndex, profile);
	}

	const char *const *CompatibilitySurface::commandNames(std::size_t *count)
	{
		static const char *const names[] = {
			"bot_about",
			"bot_add",
			"bot_add_t",
			"bot_add_ct",
			"bot_kick",
			"bot_kill",
			"bot_knives_only",
			"bot_pistols_only",
			"bot_snipers_only",
			"bot_all_weapons"
		};
		if (count != nullptr)
		{
			*count = sizeof(names) / sizeof(names[0]);
		}
		return names;
	}
}
}
