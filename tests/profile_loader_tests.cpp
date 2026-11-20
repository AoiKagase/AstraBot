#include "astrabot/metamod/profile_loader.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace
{
// Compare stored float32 values, not x87 excess-precision literal registers.
bool sameFloat32(float actual, float expected)
{
	return std::memcmp(&actual, &expected, sizeof(float)) == 0;
}

float adjustedReaction(float explicitValue)
{
	// The native loader stores the parsed value before subtracting the think interval.
	volatile float stored = explicitValue;
	volatile float interval = 0.1f;
	return stored - interval;
}

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
	if (!check(loader.loadText(validText, std::strlen(validText), &catalog) == ProfileLoadResult::Loaded,
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
	if (!check(loader.loadText(malformedText, std::strlen(malformedText), &catalog) == ProfileLoadResult::Malformed &&
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

	const char csbotText[] =
		"Default\n"
		"ReactionTime = 0.45\nTeamwork = 60\nCost = 1500\nVoicePitch = 95\nVoiceBank = default-voice.db\nWeaponPreference = ak47\n"
		"Skill = 50\n"
		"Aggression = 50\n"
		"Difficulty = NORMAL\n"
		"End\n"
		"Template Elite\n"
		"ReactionTime = 0.2\nAttackDelay = 0.1\nVoiceBank = elite-voice.db\nTeamwork = 90\nWeaponPreference = m4a1\nWeaponPreference = deagle\n"
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
	if (!check(loader.loadText(csbotText, std::strlen(csbotText), &catalog) == ProfileLoadResult::Loaded &&
				catalog.size() == 2U,
			"CSBot profile database syntax loads"))
	{
		return 1;
	}
	astrabot::compat::ProfileRecord csbotProfile = {};
	if (!check(catalog.selectNamed("BotOne", ProfileTeam::Any, &csbotProfile) == ProfileSelectionResult::Selected &&
				csbotProfile.skill == 100 && csbotProfile.difficulty == 4 &&
				sameFloat32(csbotProfile.effectiveReactionSeconds(true), adjustedReaction(0.2f)) &&
				sameFloat32(csbotProfile.effectiveReactionSeconds(false), 0.2f) &&
				sameFloat32(csbotProfile.attackDelaySeconds, 0.1f) && csbotProfile.teamwork == 90 &&
				csbotProfile.cost == 1500 && csbotProfile.voicePitch == 95 &&
				std::strcmp(csbotProfile.voiceBank, "elite-voice.db") == 0 && csbotProfile.weaponPreferenceCount == 2 &&
				csbotProfile.weaponPreferences[0] == 22 && csbotProfile.weaponPreferences[1] == 26 &&
				csbotProfile.difficultyMask == 8,
			"CSBot profile inherits skill template"))
	{
		return 1;
	}
	if (!check(catalog.selectNamed("BotTwo", ProfileTeam::Any, &csbotProfile) == ProfileSelectionResult::Selected &&
			sameFloat32(csbotProfile.effectiveReactionSeconds(true), adjustedReaction(0.45f)) &&
			std::strcmp(csbotProfile.voiceBank, "default-voice.db") == 0,
			"profile inherits the adjusted Default reaction and voice bank"))
		return 1;

	const char multiTemplateText[] =
		"Default\nSkill = 50\nAggression = 50\nTeam = T\nDifficulty = NORMAL\n"
		"ReactionTime = 0.4\nAttackDelay = 0.2\nTeamwork = 60\nCost = 1500\nVoicePitch = 95\n"
		"VoiceBank = default.db\nWeaponPreference = ak47\nEnd\n"
		"Template A\nSkill = 75\nAggression = 80\nTeam = CT\nDifficulty = EXPERT\n"
		"ReactionTime = 0.3\nAttackDelay = 0.1\nTeamwork = 90\nCost = 2000\nVoicePitch = 115\n"
		"VoiceBank = other.db\nWeaponPreference = m4a1\nWeaponPreference = deagle\nEnd\n"
		"Template B\nSkill = 50\nAggression = 50\nTeam = T\nDifficulty = NORMAL\n"
		"ReactionTime = 0.4\nAttackDelay = 0.2\nTeamwork = 60\nCost = 1500\nVoicePitch = 95\n"
		"VoiceBank = default.db\nWeaponPreference = awp\nEnd\n"
		"Template C\nWeaponPreference = awp\nWeaponPreference = ak47\nEnd\n"
		"A+B Inherited\nEnd\n"
		"A+C EqualToCurrentCount\nEnd\n"
		"A+B Explicit\nSkill = 50\nTeamwork = 60\nReactionTime = 0.05\nWeaponPreference = awp\nEnd\n";
	astrabot::compat::ProfileCatalog multiTemplateCatalog;
	if (!check(loader.loadText(multiTemplateText, std::strlen(multiTemplateText), &multiTemplateCatalog) ==
			ProfileLoadResult::Loaded, "CSBot profiles with multiple templates load"))
	{
		return 1;
	}
	if (!check(multiTemplateCatalog.selectNamed("Inherited", ProfileTeam::Any, &csbotProfile) ==
			ProfileSelectionResult::Selected, "multiple CSBot templates resolve a profile"))
	{
		return 1;
	}
	bool multiTemplatePassed = check(csbotProfile.skill == 75 && csbotProfile.aggression == 80 &&
		csbotProfile.team == ProfileTeam::CounterTerrorist && csbotProfile.difficultyMask == 8U &&
		sameFloat32(csbotProfile.effectiveReactionSeconds(true), adjustedReaction(0.3f)) &&
		sameFloat32(csbotProfile.effectiveReactionSeconds(false), 0.3f) &&
		sameFloat32(csbotProfile.attackDelaySeconds, 0.1f) &&
		csbotProfile.teamwork == 90 && csbotProfile.cost == 2000 && csbotProfile.voicePitch == 115 &&
		std::strcmp(csbotProfile.voiceBank, "other.db") == 0,
		"default-equivalent template values preserve preceding scalar overrides");
	multiTemplatePassed &= check(csbotProfile.weaponPreferenceCount == 2 && csbotProfile.weaponPreferences[0] == 22 &&
		csbotProfile.weaponPreferences[1] == 26, "a template with the default preference count does not inherit");
	multiTemplatePassed &= check(multiTemplateCatalog.selectNamed("EqualToCurrentCount", ProfileTeam::Any,
									&csbotProfile) == ProfileSelectionResult::Selected &&
			csbotProfile.weaponPreferenceCount == 2 && csbotProfile.weaponPreferences[0] == 18 &&
			csbotProfile.weaponPreferences[1] == 28, "preference count compares with Default, not the preceding template");
	multiTemplatePassed &= check(multiTemplateCatalog.selectNamed("Explicit", ProfileTeam::Any, &csbotProfile) ==
				ProfileSelectionResult::Selected && csbotProfile.skill == 50 && csbotProfile.teamwork == 60 &&
			sameFloat32(csbotProfile.effectiveReactionSeconds(true), adjustedReaction(0.05f)) &&
			csbotProfile.weaponPreferenceCount == 1 && csbotProfile.weaponPreferences[0] == 18,
		"profile fields override template values even when they equal Default");
	if (!multiTemplatePassed)
	{
		return 1;
	}

	const char omittedReaction[] =
		"Default\nSkill = 50\nEnd\n"
		"Template Plain\nEnd\n"
		"Template Override\nReactionTime = 0.2\nEnd\n"
		"Template EqualDefault\nReactionTime = 0.4\nEnd\n"
		"Plain Unspecified\nEnd\n"
		"EqualDefault Unchanged\nEnd\n"
		"Override+EqualDefault Kept\nEnd\n"
		"EqualDefault Immediate\nReactionTime = 0\nEnd\n"
		"EqualDefault Rounded\nReactionTime = 0.1499999985098838800\nEnd\n";
	astrabot::compat::ProfileCatalog reactionCatalog;
	if (!check(loader.loadText(omittedReaction, std::strlen(omittedReaction), &reactionCatalog) == ProfileLoadResult::Loaded &&
			reactionCatalog.selectNamed("Unspecified", ProfileTeam::Any, &csbotProfile) == ProfileSelectionResult::Selected &&
			sameFloat32(csbotProfile.effectiveReactionSeconds(true), 0.3f) &&
			sameFloat32(csbotProfile.effectiveReactionSeconds(false), 0.3f) &&
			reactionCatalog.selectNamed("Unchanged", ProfileTeam::Any, &csbotProfile) == ProfileSelectionResult::Selected &&
			sameFloat32(csbotProfile.effectiveReactionSeconds(true), 0.3f) &&
			sameFloat32(csbotProfile.effectiveReactionSeconds(false), 0.4f) &&
			reactionCatalog.selectNamed("Kept", ProfileTeam::Any, &csbotProfile) == ProfileSelectionResult::Selected &&
			sameFloat32(csbotProfile.effectiveReactionSeconds(true), adjustedReaction(0.2f)) &&
			sameFloat32(csbotProfile.effectiveReactionSeconds(false), 0.4f) &&
			reactionCatalog.selectNamed("Immediate", ProfileTeam::Any, &csbotProfile) == ProfileSelectionResult::Selected &&
			sameFloat32(csbotProfile.effectiveReactionSeconds(true), -0.1f) &&
			sameFloat32(csbotProfile.effectiveReactionSeconds(false), 0.0f),
			"native omission, adjusted Default-equivalent inheritance and explicit zero"))
		return 1;
	if (!check(reactionCatalog.selectNamed("Rounded", ProfileTeam::Any, &csbotProfile) ==
			ProfileSelectionResult::Selected &&
			sameFloat32(csbotProfile.effectiveReactionSeconds(true), adjustedReaction(0.15f)) &&
			sameFloat32(csbotProfile.effectiveReactionSeconds(false),
				std::strtof("0.1499999985098838800", nullptr)),
			"native conversion rounds through double while Enhanced keeps the configured float parser"))
		return 1;

	const char bracedReaction[] = "Explicit { Team = any ReactionTime = 0.05 }\n";
	if (!check(loader.loadText(bracedReaction, std::strlen(bracedReaction), &reactionCatalog) == ProfileLoadResult::Loaded &&
			reactionCatalog.selectNamed("Explicit", ProfileTeam::Any, &csbotProfile) == ProfileSelectionResult::Selected &&
			sameFloat32(csbotProfile.effectiveReactionSeconds(true), 0.05f) &&
			sameFloat32(csbotProfile.effectiveReactionSeconds(false), 0.05f),
			"the braced grammar keeps its specified reaction time"))
		return 1;

	const char invalidExtended[] = "Bad { ReactionTime = nan }\n";
	if (!check(
			loader.loadText(invalidExtended, std::strlen(invalidExtended), &catalog) == ProfileLoadResult::Malformed &&
				catalog.size() == 2,
			"invalid reaction time rejects transaction without replacing loaded profiles"))
		return 1;
	const char invalidAlias[] = "Default\nEnd\nTemplate Test\nWeaponPreference = unknown_weapon\nEnd\nTest Bad\nEnd\n";
	if (!check(loader.loadText(invalidAlias, std::strlen(invalidAlias), &catalog) == ProfileLoadResult::Malformed &&
				catalog.size() == 2,
			"unknown preference alias is explicit and transactional"))
		return 1;

	const char oversizedText[] = "One { Skill = 10 }\n";
	ProfileLoadLimits smallLimits = {sizeof(oversizedText) - 1U, 1U};
	ProfileLoader boundedLoader(smallLimits);
	const char tooLargeText[] = "One { Skill = 10 }\nTwo { Skill = 20 }\n";
	if (!check(
			boundedLoader.loadText(tooLargeText, std::strlen(tooLargeText), &catalog) == ProfileLoadResult::TooLarge &&
				catalog.size() == 2U,
			"oversized input is rejected before publication"))
	{
		return 1;
	}

	const char boundedProfilesText[] = "One { Skill = 10 }\n"
									   "Two { Skill = 20 }\n";
	const ProfileLoadLimits profileLimits = {sizeof(boundedProfilesText), 1U};
	ProfileLoader profileCountLoader(profileLimits);
	if (!check(profileCountLoader.loadText(boundedProfilesText, std::strlen(boundedProfilesText), &catalog) ==
					ProfileLoadResult::CatalogFull &&
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
	if (!check(readFixture(before, sizeof(before), &beforeLength), "fixture bytes are readable before load"))
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
