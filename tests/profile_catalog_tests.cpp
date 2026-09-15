#include "astrabot/compat/profile_catalog.hpp"

#include <cstdio>
#include <cstring>

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

astrabot::compat::ProfileRecord profile(
	const char *name,
	astrabot::compat::ProfileTeam team,
	int difficulty,
	int skill,
	int aggression)
{
	astrabot::compat::ProfileRecord result = {};
	const std::size_t length = std::strlen(name);
	const std::size_t copyLength = length < astrabot::compat::ProfileRecord::kNameCapacity ?
		length : astrabot::compat::ProfileRecord::kNameCapacity;
	for (std::size_t index = 0U; index < copyLength; ++index)
	{
		result.name[index] = name[index];
	}
	result.name[copyLength] = '\0';
	result.team = team;
	result.difficulty = difficulty;
	result.skill = skill;
	result.aggression = aggression;
	return result;
}
}

int main()
{
	using astrabot::compat::ProfileAddResult;
	using astrabot::compat::ProfileCatalog;
	using astrabot::compat::ProfileSelectionResult;
	using astrabot::compat::ProfileTeam;

	ProfileCatalog catalog;
	if (!check(catalog.add(profile("Alpha", ProfileTeam::Terrorist, 2, 80, 40)) ==
			ProfileAddResult::Added, "valid profile is added"))
	{
		return 1;
	}
	if (!check(catalog.add(profile("Bravo", ProfileTeam::CounterTerrorist, 2, 70, 60)) ==
			ProfileAddResult::Added, "second valid profile is added"))
	{
		return 1;
	}
	if (!check(catalog.add(profile("alpha", ProfileTeam::Any, 2, 10, 10)) ==
			ProfileAddResult::DuplicateName, "duplicate names are rejected case-insensitively"))
	{
		return 1;
	}
	if (!check(catalog.add(profile("", ProfileTeam::Any, 2, 10, 10)) ==
			ProfileAddResult::InvalidRecord, "empty names are rejected"))
	{
		return 1;
	}

	astrabot::compat::ProfileRecord selected = {};
	if (!check(catalog.select(ProfileTeam::Terrorist, 2, 0U, &selected) ==
			ProfileSelectionResult::Selected && std::strcmp(selected.name, "Alpha") == 0,
			"team and difficulty select the matching profile"))
	{
		return 1;
	}
	if (!check(catalog.select(ProfileTeam::CounterTerrorist, 2, 0U, &selected) ==
			ProfileSelectionResult::Selected && std::strcmp(selected.name, "Bravo") == 0,
			"counter-terrorist selection is isolated"))
	{
		return 1;
	}
	if (!check(catalog.select(ProfileTeam::Any, 4, 0U, &selected) ==
			ProfileSelectionResult::Fallback, "missing difficulty has explicit fallback"))
	{
		return 1;
	}
	if (!check(catalog.selectNamed("missing", ProfileTeam::Any, &selected) ==
			ProfileSelectionResult::UnknownProfile, "unknown names are explicit"))
	{
		return 1;
	}
	if (!check(catalog.select(ProfileTeam::Any, 2, 0U, nullptr) ==
			ProfileSelectionResult::InvalidOutput, "null selection output is rejected"))
	{
		return 1;
	}

	ProfileCatalog deterministic;
	deterministic.add(profile("One", ProfileTeam::Any, 1, 20, 20));
	deterministic.add(profile("Two", ProfileTeam::Any, 1, 30, 30));
	if (!check(deterministic.select(ProfileTeam::Any, 1, 1U, &selected) ==
			ProfileSelectionResult::Selected && std::strcmp(selected.name, "Two") == 0,
			"selection index is deterministic"))
	{
		return 1;
	}
	if (!check(deterministic.select(ProfileTeam::Any, 1, 3U, &selected) ==
			ProfileSelectionResult::Selected && std::strcmp(selected.name, "Two") == 0,
			"selection index wraps deterministically"))
	{
		return 1;
	}

	ProfileCatalog full;
	for (std::size_t index = 0U; index < ProfileCatalog::kMaximumProfiles; ++index)
	{
		char name[astrabot::compat::ProfileRecord::kNameCapacity + 1U] = {};
		std::snprintf(name, sizeof(name), "P%zu", index);
		if (!check(full.add(profile(name, ProfileTeam::Any, 0, 50, 50)) ==
				ProfileAddResult::Added, "catalog accepts bounded profile count"))
		{
			return 1;
		}
	}
	if (!check(full.add(profile("Overflow", ProfileTeam::Any, 0, 50, 50)) ==
			ProfileAddResult::Full, "catalog rejects profiles over its bound"))
	{
		return 1;
	}

	return 0;
}
