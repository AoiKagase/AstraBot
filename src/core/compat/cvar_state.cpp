#include "astrabot/compat/cvar_state.hpp"

#include <cmath>
#include <cstring>

namespace astrabot
{
namespace compat
{
	namespace
	{
		constexpr int kMinimumDifficulty = 0;
		constexpr int kMaximumDifficulty = 4;
		constexpr int kMinimumQuota = 0;
		constexpr int kMaximumQuota = 32;

		bool isInteger(float value)
		{
			return std::isfinite(value) && std::floor(value) == value;
		}

		bool equalsIgnoreCase(const char *left, const char *right)
		{
			if (left == nullptr || right == nullptr)
			{
				return false;
			}
			while (*left != '\0' && *right != '\0')
			{
				const char leftCharacter = *left >= 'a' && *left <= 'z' ?
					static_cast<char>(*left - 'a' + 'A') : *left;
				const char rightCharacter = *right >= 'a' && *right <= 'z' ?
					static_cast<char>(*right - 'a' + 'A') : *right;
				if (leftCharacter != rightCharacter)
				{
					return false;
				}
				++left;
				++right;
			}
			return *left == '\0' && *right == '\0';
		}
	}

	CvarState::CvarState()
		: state_{0.0f, 0.0f, 1, 0, JoinTeam::Any}
	{
	}

	CvarUpdateResult CvarState::setFloat(const char *name, float value)
	{
		if (name == nullptr || !std::isfinite(value))
		{
			return name == nullptr ? CvarUpdateResult::Unknown : CvarUpdateResult::InvalidValue;
		}
		if (std::strcmp(name, "bot_enable") == 0 || std::strcmp(name, "bot_stop") == 0)
		{
			if (value != 0.0f && value != 1.0f)
			{
				return CvarUpdateResult::InvalidValue;
			}
			float &target = std::strcmp(name, "bot_enable") == 0 ?
				state_.botEnable : state_.botStop;
			if (target == value)
			{
				return CvarUpdateResult::NoChange;
			}
			target = value;
			return CvarUpdateResult::Updated;
		}
		if (std::strcmp(name, "bot_difficulty") == 0)
		{
			if (!isInteger(value) || value < kMinimumDifficulty || value > kMaximumDifficulty)
			{
				return CvarUpdateResult::InvalidValue;
			}
			const int difficulty = static_cast<int>(value);
			if (state_.botDifficulty == difficulty)
			{
				return CvarUpdateResult::NoChange;
			}
			state_.botDifficulty = difficulty;
			return CvarUpdateResult::Updated;
		}
		if (std::strcmp(name, "bot_quota") == 0)
		{
			if (!isInteger(value) || value < kMinimumQuota || value > kMaximumQuota)
			{
				return CvarUpdateResult::InvalidValue;
			}
			const int quota = static_cast<int>(value);
			if (state_.botQuota == quota)
			{
				return CvarUpdateResult::NoChange;
			}
			state_.botQuota = quota;
			return CvarUpdateResult::Updated;
		}
		return CvarUpdateResult::Unknown;
	}

	CvarUpdateResult CvarState::setString(const char *name, const char *value)
	{
		if (name == nullptr || std::strcmp(name, "bot_join_team") != 0)
		{
			return CvarUpdateResult::Unknown;
		}
		JoinTeam joinTeam = JoinTeam::Any;
		if (equalsIgnoreCase(value, "T"))
		{
			joinTeam = JoinTeam::Terrorist;
		}
		else if (equalsIgnoreCase(value, "CT"))
		{
			joinTeam = JoinTeam::CounterTerrorist;
		}
		else if (!equalsIgnoreCase(value, "any"))
		{
			return CvarUpdateResult::InvalidValue;
		}
		if (state_.botJoinTeam == joinTeam)
		{
			return CvarUpdateResult::NoChange;
		}
		state_.botJoinTeam = joinTeam;
		return CvarUpdateResult::Updated;
	}

	CvarSnapshot CvarState::snapshot() const
	{
		return state_;
	}
}
}
