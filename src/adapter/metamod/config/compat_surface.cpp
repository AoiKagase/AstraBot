#include "astrabot/metamod/compat_surface.hpp"

namespace astrabot
{
namespace metamod
{
bool CompatibilitySurface::registerCommands(enginefuncs_t *engineFunctions, void (*callback)(void)) const
{
	if (engineFunctions == nullptr || engineFunctions->pfnAddServerCommand == nullptr || callback == nullptr)
	{
		return false;
	}

	std::size_t count = 0U;
	const char *const *names = commandNames(&count);
	for (std::size_t index = 0U; index < count; ++index)
	{
		engineFunctions->pfnAddServerCommand(const_cast<char *>(names[index]), callback);
	}
	return true;
}

compat::CommandResult CompatibilitySurface::resolve(
	const compat::CommandRequest &request, compat::CommandAction *action) const
{
	return commandRegistry_.resolve(request, action);
}

compat::CvarUpdateResult CompatibilitySurface::setFloat(const char *name, float value)
{
	return botConfiguration_.setFloat(name, value);
}

compat::CvarUpdateResult CompatibilitySurface::setString(const char *name, const char *value)
{
	return botConfiguration_.setString(name, value);
}

compat::CvarSnapshot CompatibilitySurface::configuration() const
{
	return botConfiguration_.snapshot();
}

compat::BotActionResult CompatibilitySurface::authorize(
	const compat::CommandAction &action, std::size_t managedActorCount, bool nativeGuardAllowsCreation) const
{
	return botConfiguration_.authorize(action, managedActorCount, nativeGuardAllowsCreation);
}

ProfileLoadResult CompatibilitySurface::loadProfiles(const char *path)
{
	return profileLoader_.loadFile(path, &profileCatalog_);
}

compat::ProfileSelectionResult CompatibilitySurface::selectProfile(const compat::CommandAction &action, int difficulty,
	std::size_t selectionIndex, compat::ProfileRecord *profile) const
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
	else
	{
		const compat::CvarSnapshot state = botConfiguration_.snapshot();
		if (state.botJoinTeam == compat::JoinTeam::Terrorist)
		{
			team = compat::ProfileTeam::Terrorist;
		}
		else if (state.botJoinTeam == compat::JoinTeam::CounterTerrorist)
		{
			team = compat::ProfileTeam::CounterTerrorist;
		}
	}
	if (action.target != nullptr)
	{
		return profileCatalog_.selectNamed(action.target, team, profile);
	}
	return profileCatalog_.select(team, difficulty, selectionIndex, profile);
}

compat::ProfileSelectionResult CompatibilitySurface::selectUnusedProfile(const compat::CommandAction &action,
	int difficulty, const char *const *usedNames, std::size_t usedCount, compat::ICompatibilityRandomSource &random,
	compat::RandomActor actor, compat::RandomTimingContext timing, compat::ProfileRecord *profile) const
{
	if (usedCount != 0U && usedNames == nullptr)
		return compat::ProfileSelectionResult::InvalidRequest;
	compat::ProfileTeam team = compat::ProfileTeam::Any;
	const auto configured = botConfiguration_.snapshot().botJoinTeam;
	if (action.team == compat::CommandTeam::Terrorist ||
		(action.team == compat::CommandTeam::Any && configured == compat::JoinTeam::Terrorist))
		team = compat::ProfileTeam::Terrorist;
	else if (action.team == compat::CommandTeam::CounterTerrorist ||
		(action.team == compat::CommandTeam::Any && configured == compat::JoinTeam::CounterTerrorist))
		team = compat::ProfileTeam::CounterTerrorist;
	if (action.target != nullptr)
	{
		const auto result = profileCatalog_.selectNamed(action.target, team, profile);
		if (result != compat::ProfileSelectionResult::Selected)
			return result;
		for (std::size_t i = 0; i < usedCount; ++i)
		{
			if (usedNames[i] == nullptr)
				continue;
			std::size_t j = 0;
			for (; j <= compat::ProfileRecord::kNameCapacity; ++j)
			{
				const char a = profile->name[j], b = usedNames[i][j];
				const char lowerA = a >= 'A' && a <= 'Z' ? static_cast<char>(a - 'A' + 'a') : a;
				const char lowerB = b >= 'A' && b <= 'Z' ? static_cast<char>(b - 'A' + 'a') : b;
				if (lowerA != lowerB)
					break;
				if (a == '\0')
					return compat::ProfileSelectionResult::NoMatch;
			}
		}
		return result;
	}
	return profileCatalog_.selectUnused(team, difficulty, usedNames, usedCount, random, actor, timing, profile);
}

const char *const *CompatibilitySurface::commandNames(std::size_t *count)
{
	static const char *const names[] = {"bot_about", "bot_add", "bot_add_t", "bot_add_ct", "bot_kick", "bot_kill",
		"bot_knives_only", "bot_pistols_only", "bot_snipers_only", "bot_all_weapons"};
	if (count != nullptr)
	{
		*count = sizeof(names) / sizeof(names[0]);
	}
	return names;
}
}
}
