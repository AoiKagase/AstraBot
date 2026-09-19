#include "astrabot/metamod/profile_loader.hpp"

#include <cstdio>
#include <cstring>

namespace
{
	const char *const kFixturePath = "astrabot_profile_loader_fixture.db";

	std::FILE *openFixture(const char *mode)
	{
		std::FILE *file = nullptr;
#ifdef _WIN32
		if (fopen_s(&file, kFixturePath, mode) != 0)
		{
			return nullptr;
		}
#else
		file = std::fopen(kFixturePath, mode);
#endif
		return file;
	}

	bool check(bool condition, const char *description)
	{
		if (condition)
		{
			return true;
		}

		std::fprintf(stderr, "check failed: %s\n", description);
		return false;
	}

	bool writeFixture(const char *contents)
	{
		std::FILE *file = openFixture("wb");
		if (file == nullptr)
		{
			return false;
		}

		const std::size_t length = std::strlen(contents);
		const std::size_t written = std::fwrite(contents, 1U, length, file);
		const int closeResult = std::fclose(file);
		return written == length && closeResult == 0;
	}

	bool readFixture(char *buffer, std::size_t capacity, std::size_t *length)
	{
		if (buffer == nullptr || capacity == 0U || length == nullptr)
		{
			return false;
		}

		std::FILE *file = openFixture("rb");
		if (file == nullptr)
		{
			return false;
		}

		const std::size_t bytesRead = std::fread(buffer, 1U, capacity - 1U, file);
		const int closeResult = std::fclose(file);
		buffer[bytesRead] = '\0';
		*length = bytesRead;
		return closeResult == 0;
	}
} // namespace

int main()
{
	using astrabot::compat::ProfileCatalog;
	using astrabot::compat::ProfileSelectionResult;
	using astrabot::compat::ProfileTeam;
	using astrabot::metamod::ProfileLoader;
	using astrabot::metamod::ProfileLoadLimits;
	using astrabot::metamod::ProfileLoadResult;

	const char validText[] = "Default\n"
							 "{\n"
							 " Skill = 60\n"
							 " Aggression = 40\n"
							 " Difficulty = 2\n"
							 " Team = any\n"
							 "}\n"
							 "Bot\n"
							 "{\n"
							 " Name = \"Quoted Alpha\"\n"
							 " Skill = 90\n"
							 " Aggression = 80\n"
							 " Difficulty = 3\n"
							 " Team = CT\n"
							 "}\n";

	ProfileCatalog catalog;
	ProfileLoader loader;
	if (!check(loader.loadText(validText, std::strlen(validText), &catalog) ==
				   ProfileLoadResult::Loaded,
			   "valid profile text loads"))
	{
		return 1;
	}
	if (!check(catalog.size() == 2U, "valid profile count is published"))
	{
		return 1;
	}
	astrabot::compat::ProfileRecord selected = {};
	if (!check(catalog.selectNamed("Quoted Alpha", ProfileTeam::CounterTerrorist, &selected) ==
					   ProfileSelectionResult::Selected &&
				   selected.skill == 90 && selected.aggression == 80,
			   "quoted profile fields are preserved"))
	{
		return 1;
	}

	const char malformedText[] = "Broken { Skill = 50\n";
	if (!check(loader.loadText(malformedText, std::strlen(malformedText), &catalog) ==
					   ProfileLoadResult::Malformed &&
				   catalog.size() == 2U,
			   "malformed input does not publish a partial catalog"))
	{
		return 1;
	}

	const char duplicateText[] = "One { Skill = 10 }\n"
								 "one { Skill = 20 }\n";
	if (!check(loader.loadText(duplicateText, std::strlen(duplicateText), &catalog) ==
					   ProfileLoadResult::DuplicateProfile &&
				   catalog.size() == 2U,
			   "duplicate profiles are rejected transactionally"))
	{
		return 1;
	}

	const char csbotText[] = "Default\n"
							 "Skill = 50\n"
							 "Aggression = 50\n"
							 "Difficulty = NORMAL\n"
							 "End\n"
							 "Template Elite\n"
							 "Skill = 100\n"
							 "Aggression = 100\n"
							 "Difficulty = EXPERT\n"
							 "End\n"
							 "Template Normal\n"
							 "Skill = 50\n"
							 "Aggression = 50\n"
							 "Difficulty = NORMAL\n"
							 "End\n"
							 "Elite BotOne\n"
							 "End\n"
							 "Normal BotTwo\n"
							 "Skill = 45\n"
							 "End\n";
	if (!check(loader.loadText(csbotText, std::strlen(csbotText), &catalog) ==
					   ProfileLoadResult::Loaded &&
				   catalog.size() == 2U,
			   "CSBot profile database syntax loads"))
	{
		return 1;
	}
	astrabot::compat::ProfileRecord csbotProfile = {};
	if (!check(catalog.selectNamed("BotOne", ProfileTeam::Any, &csbotProfile) ==
					   ProfileSelectionResult::Selected &&
				   csbotProfile.skill == 100 && csbotProfile.difficulty == 4,
			   "CSBot profile inherits skill template"))
	{
		return 1;
	}

	const char oversizedText[] = "One { Skill = 10 }\n";
	ProfileLoadLimits smallLimits = {sizeof(oversizedText) - 1U, 1U};
	ProfileLoader boundedLoader(smallLimits);
	const char tooLargeText[] = "One { Skill = 10 }\nTwo { Skill = 20 }\n";
	if (!check(boundedLoader.loadText(tooLargeText, std::strlen(tooLargeText), &catalog) ==
					   ProfileLoadResult::TooLarge &&
				   catalog.size() == 2U,
			   "oversized input is rejected before publication"))
	{
		return 1;
	}

	const char boundedProfilesText[] = "One { Skill = 10 }\n"
									   "Two { Skill = 20 }\n";
	const ProfileLoadLimits profileLimits = {sizeof(boundedProfilesText), 1U};
	ProfileLoader profileCountLoader(profileLimits);
	if (!check(profileCountLoader.loadText(boundedProfilesText, std::strlen(boundedProfilesText),
										   &catalog) == ProfileLoadResult::CatalogFull &&
				   catalog.size() == 2U,
			   "profile count bound rejects extra records transactionally"))
	{
		return 1;
	}

	if (!check(writeFixture(validText), "read-only fixture is written"))
	{
		return 1;
	}
	char before[512] = {};
	char after[512] = {};
	std::size_t beforeLength = 0U;
	std::size_t afterLength = 0U;
	if (!check(readFixture(before, sizeof(before), &beforeLength),
			   "fixture bytes are readable before load"))
	{
		std::remove(kFixturePath);
		return 1;
	}
	if (!check(loader.loadFile(kFixturePath, &catalog) == ProfileLoadResult::Loaded,
			   "profile file loads through adapter boundary"))
	{
		std::remove(kFixturePath);
		return 1;
	}
	if (!check(readFixture(after, sizeof(after), &afterLength) && beforeLength == afterLength &&
				   std::memcmp(before, after, beforeLength) == 0,
			   "profile source bytes remain unchanged"))
	{
		std::remove(kFixturePath);
		return 1;
	}
	std::remove(kFixturePath);

	if (!check(loader.loadFile("missing-profile.db", &catalog) == ProfileLoadResult::MissingFile,
			   "missing profile file is explicit"))
	{
		return 1;
	}

	return 0;
}
