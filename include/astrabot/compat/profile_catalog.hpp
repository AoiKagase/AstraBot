#ifndef ASTRABOT_COMPAT_PROFILE_CATALOG_HPP
#define ASTRABOT_COMPAT_PROFILE_CATALOG_HPP

#include <array>
#include <cstddef>
#include "astrabot/compat/random_source.hpp"

namespace astrabot
{
namespace compat
{
enum class ProfileTeam
{
	Any,
	Terrorist,
	CounterTerrorist
};

enum class ProfileAddResult
{
	Added,
	DuplicateName,
	InvalidRecord,
	Full
};

enum class ProfileSelectionResult
{
	Selected,
	Fallback,
	UnknownProfile,
	NoMatch,
	InvalidRequest,
	InvalidOutput
};

struct ProfileRecord
{
	static constexpr float kDefaultReactionSeconds = 0.3f;
	static constexpr int kDefaultTeamwork = 75;
	static constexpr int kWeaponPreferenceCapacity = 16;
	static constexpr int kDefaultVoicePitch = 100;
	static constexpr std::size_t kNameCapacity = 31U;

	char name[kNameCapacity + 1U];
	ProfileTeam team;
	int difficulty;
	int skill;
	int aggression;
	float reactionTimeSeconds = kDefaultReactionSeconds;
	float attackDelaySeconds = 0.0f;
	int teamwork = kDefaultTeamwork;
	int weaponPreferences[kWeaponPreferenceCapacity] = {};
	std::size_t weaponPreferenceCount = 0U;
	int cost = 0;
	int voicePitch = kDefaultVoicePitch;
	char voiceBank[128] = {}; // Empty selects the default BotChatter.db bank.
	unsigned difficultyMask = 0;
};

class ProfileCatalog
{
public:
	static constexpr std::size_t kMaximumProfiles = 256U;
	static constexpr int kMinimumDifficulty = 0;
	static constexpr int kMaximumDifficulty = 4;
	static constexpr int kMinimumAttribute = 0;
	static constexpr int kMaximumAttribute = 100;

	ProfileCatalog();

	ProfileAddResult add(const ProfileRecord &profile);
	ProfileSelectionResult select(
		ProfileTeam team, int difficulty, std::size_t selectionIndex, ProfileRecord *profile) const;
	ProfileSelectionResult selectNamed(const char *name, ProfileTeam team, ProfileRecord *profile) const;
	ProfileSelectionResult selectUnused(ProfileTeam team, int difficulty, const char *const *usedNames,
		std::size_t usedCount, ICompatibilityRandomSource &random, RandomActor actor, RandomTimingContext timing,
		ProfileRecord *profile) const;

	std::size_t size() const;
	void clear();

private:
	static bool isValid(const ProfileRecord &profile);
	static bool isValidTeam(ProfileTeam team);
	static bool isCompatible(ProfileTeam profileTeam, ProfileTeam requestedTeam);
	static bool namesEqual(const char *left, const char *right);
	static bool isInputName(const char *name);

	std::array<ProfileRecord, kMaximumProfiles> profiles_;
	std::size_t size_;
};
} // namespace compat
} // namespace astrabot

#endif
