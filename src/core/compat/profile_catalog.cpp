#include "astrabot/compat/profile_catalog.hpp"

#include <cctype>
#include <cstring>

namespace astrabot
{
namespace compat
{
namespace
{
char lowerAscii(char value)
{
	return static_cast<char>(std::tolower(static_cast<unsigned char>(value)));
}
}

ProfileCatalog::ProfileCatalog() : profiles_(), size_(0U)
{
}

ProfileAddResult ProfileCatalog::add(const ProfileRecord &profile)
{
	if (!isValid(profile))
	{
		return ProfileAddResult::InvalidRecord;
	}
	for (std::size_t index = 0U; index < size_; ++index)
	{
		if (namesEqual(profiles_[index].name, profile.name))
		{
			return ProfileAddResult::DuplicateName;
		}
	}
	if (size_ >= profiles_.size())
	{
		return ProfileAddResult::Full;
	}
	profiles_[size_] = profile;
	++size_;
	return ProfileAddResult::Added;
}

ProfileSelectionResult ProfileCatalog::select(
	ProfileTeam team,
	int difficulty,
	std::size_t selectionIndex,
	ProfileRecord *profile) const
{
	if (profile == nullptr)
	{
		return ProfileSelectionResult::InvalidOutput;
	}
	if (!isValidTeam(team) || difficulty < kMinimumDifficulty ||
			difficulty > kMaximumDifficulty)
	{
		return ProfileSelectionResult::InvalidRequest;
	}

	std::array<std::size_t, kMaximumProfiles> matchingIndices = {};
	std::size_t matchingCount = 0U;
	for (std::size_t index = 0U; index < size_; ++index)
	{
		if (profiles_[index].difficulty == difficulty &&
				isCompatible(profiles_[index].team, team))
		{
			matchingIndices[matchingCount] = index;
			++matchingCount;
		}
	}
	if (matchingCount > 0U)
	{
		*profile = profiles_[matchingIndices[selectionIndex % matchingCount]];
		return ProfileSelectionResult::Selected;
	}

	matchingCount = 0U;
	for (std::size_t index = 0U; index < size_; ++index)
	{
		if (isCompatible(profiles_[index].team, team))
		{
			matchingIndices[matchingCount] = index;
			++matchingCount;
		}
	}
	if (matchingCount == 0U)
	{
		return ProfileSelectionResult::NoMatch;
	}
	*profile = profiles_[matchingIndices[selectionIndex % matchingCount]];
	return ProfileSelectionResult::Fallback;
}

ProfileSelectionResult ProfileCatalog::selectNamed(
	const char *name,
	ProfileTeam team,
	ProfileRecord *profile) const
{
	if (profile == nullptr)
	{
		return ProfileSelectionResult::InvalidOutput;
	}
	if (!isInputName(name) || !isValidTeam(team))
	{
		return ProfileSelectionResult::InvalidRequest;
	}
	for (std::size_t index = 0U; index < size_; ++index)
	{
		if (namesEqual(profiles_[index].name, name))
		{
			if (!isCompatible(profiles_[index].team, team))
			{
				return ProfileSelectionResult::NoMatch;
			}
			*profile = profiles_[index];
			return ProfileSelectionResult::Selected;
		}
	}
	return ProfileSelectionResult::UnknownProfile;
}

std::size_t ProfileCatalog::size() const
{
	return size_;
}

void ProfileCatalog::clear()
{
	size_ = 0U;
}

bool ProfileCatalog::isValid(const ProfileRecord &profile)
{
	return isInputName(profile.name) && isValidTeam(profile.team) &&
			profile.difficulty >= kMinimumDifficulty &&
			profile.difficulty <= kMaximumDifficulty &&
			profile.skill >= kMinimumAttribute &&
			profile.skill <= kMaximumAttribute &&
			profile.aggression >= kMinimumAttribute &&
			profile.aggression <= kMaximumAttribute;
}

bool ProfileCatalog::isValidTeam(ProfileTeam team)
{
	return team == ProfileTeam::Any || team == ProfileTeam::Terrorist ||
			team == ProfileTeam::CounterTerrorist;
}

bool ProfileCatalog::isCompatible(ProfileTeam profileTeam, ProfileTeam requestedTeam)
{
	return profileTeam == ProfileTeam::Any || requestedTeam == ProfileTeam::Any ||
			profileTeam == requestedTeam;
}

bool ProfileCatalog::namesEqual(const char *left, const char *right)
{
	if (left == nullptr || right == nullptr)
	{
		return false;
	}
	for (std::size_t index = 0U; index <= ProfileRecord::kNameCapacity; ++index)
	{
		if (lowerAscii(left[index]) != lowerAscii(right[index]))
		{
			return false;
		}
		if (left[index] == '\0')
		{
			return true;
		}
	}
	return false;
}

bool ProfileCatalog::isInputName(const char *name)
{
	if (name == nullptr || name[0] == '\0')
	{
		return false;
	}
	for (std::size_t index = 0U; index <= ProfileRecord::kNameCapacity; ++index)
	{
		if (name[index] == '\0')
		{
			return true;
		}
	}
	return false;
}
}
}
