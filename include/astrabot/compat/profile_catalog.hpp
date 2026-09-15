#ifndef ASTRABOT_COMPAT_PROFILE_CATALOG_HPP
#define ASTRABOT_COMPAT_PROFILE_CATALOG_HPP

#include <array>
#include <cstddef>

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
	static constexpr std::size_t kNameCapacity = 31U;

	char name[kNameCapacity + 1U];
	ProfileTeam team;
	int difficulty;
	int skill;
	int aggression;
};

class ProfileCatalog
{
public:
	static constexpr std::size_t kMaximumProfiles = 64U;
	static constexpr int kMinimumDifficulty = 0;
	static constexpr int kMaximumDifficulty = 4;
	static constexpr int kMinimumAttribute = 0;
	static constexpr int kMaximumAttribute = 100;

	ProfileCatalog();

	ProfileAddResult add(const ProfileRecord &profile);
	ProfileSelectionResult select(
		ProfileTeam team,
		int difficulty,
		std::size_t selectionIndex,
		ProfileRecord *profile) const;
	ProfileSelectionResult selectNamed(
		const char *name,
		ProfileTeam team,
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
}
}

#endif
