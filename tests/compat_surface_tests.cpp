#include "astrabot/metamod/compat_surface.hpp"

#include <cstdio>
#include <cstring>

namespace
{
	int gRegistrationCount = 0;
	char gRegisteredCommands[16][32]{};

	bool check(bool condition, const char *description)
	{
		if (condition)
		{
			return true;
		}

		std::fprintf(stderr, "check failed: %s\n", description);
		return false;
	}

	void recordCommand(char *name, void (*)(void))
	{
		if (gRegistrationCount < 16)
		{
			std::size_t index = 0U;
			while (name[index] != '\0' && index + 1U < sizeof(gRegisteredCommands[0]))
			{
				gRegisteredCommands[gRegistrationCount][index] = name[index];
				++index;
			}
			gRegisteredCommands[gRegistrationCount][index] = '\0';
		}
		++gRegistrationCount;
	}

void compatibilityCallback()
{
}

std::FILE *openProfileFixture(const char *path, const char *mode)
{
	std::FILE *file = nullptr;
#ifdef _WIN32
	if (fopen_s(&file, path, mode) != 0)
	{
		return nullptr;
	}
#else
	file = std::fopen(path, mode);
#endif
	return file;
}
}

int main()
{
	using astrabot::compat::CommandAction;
	using astrabot::compat::CommandRequest;
	using astrabot::compat::CommandResult;
	using astrabot::metamod::CompatibilitySurface;

	enginefuncs_t engineFunctions{};
	engineFunctions.pfnAddServerCommand = &recordCommand;
	CompatibilitySurface surface;
	if (!check(surface.registerCommands(&engineFunctions, &compatibilityCallback),
			"compatibility commands are registered"))
	{
		return 1;
	}
	if (!check(gRegistrationCount >= 6, "required compatibility command set is registered"))
	{
		return 1;
	}
	bool foundAdd = false;
	for (int index = 0; index < gRegistrationCount && index < 16; ++index)
	{
		if (std::strcmp(gRegisteredCommands[index], "bot_add") == 0)
		{
			foundAdd = true;
		}
	}
	if (!check(foundAdd, "bot_add is registered by AstraBot surface"))
	{
		return 1;
	}
	CommandAction action{};
	const CommandRequest request = {"bot_add_ct", 0U, {nullptr, nullptr}};
	if (!check(surface.resolve(request, &action) == CommandResult::Handled,
			"registered command resolves through surface"))
	{
		return 1;
	}
	if (!check(surface.setFloat("bot_quota", 2.0f) ==
			astrabot::compat::CvarUpdateResult::Updated,
			"surface updates desired CVar state"))
	{
		return 1;
	}
	if (!check(surface.setString("bot_join_team", "T") ==
			astrabot::compat::CvarUpdateResult::Updated,
			"surface updates desired team state"))
	{
		return 1;
	}
	const char *const profilePath = "astrabot_surface_profile.db";
	std::FILE *profileFile = openProfileFixture(profilePath, "wb");
	if (!check(profileFile != nullptr, "surface profile fixture opens"))
	{
		return 1;
	}
	const char profileText[] =
		"SurfaceBot { Name = \"Surface Bot\" Team = CT Difficulty = 3 "
		"Skill = 88 Aggression = 72 }\n";
	const std::size_t profileLength = std::strlen(profileText);
	const std::size_t profileWritten =
		std::fwrite(profileText, 1U, profileLength, profileFile);
	const int profileCloseResult = std::fclose(profileFile);
	if (!check(profileWritten == profileLength && profileCloseResult == 0,
			"surface profile fixture is written"))
	{
		std::remove(profilePath);
		return 1;
	}
	if (!check(surface.loadProfiles(profilePath) ==
			astrabot::metamod::ProfileLoadResult::Loaded,
			"surface loads profile catalog"))
	{
		std::remove(profilePath);
		return 1;
	}
	astrabot::compat::ProfileRecord profile = {};
	if (!check(surface.selectProfile(action, 3, 0U, &profile) ==
			astrabot::compat::ProfileSelectionResult::Selected &&
			std::strcmp(profile.name, "Surface Bot") == 0,
			"bot_add action resolves a loaded profile"))
	{
		std::remove(profilePath);
		return 1;
	}
	std::remove(profilePath);
	return 0;
}
