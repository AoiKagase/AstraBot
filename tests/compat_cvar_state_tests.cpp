#include "astrabot/compat/cvar_state.hpp"

#include <cstdio>

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
}

int main()
{
	using astrabot::compat::CvarState;
	using astrabot::compat::CvarUpdateResult;
	using astrabot::compat::JoinTeam;

	CvarState state;
	const auto defaults = state.snapshot();
	if (!check(defaults.botEnable == 0.0f && defaults.botStop == 0.0f &&
			defaults.botDifficulty == 1 && defaults.botQuota == 0 &&
			defaults.botJoinTeam == JoinTeam::Any, "default desired CVar state"))
	{
		return 1;
	}
	if (!check(state.setFloat("bot_enable", 1.0f) == CvarUpdateResult::Updated,
			"bot_enable accepts a valid value"))
	{
		return 1;
	}
	if (!check(state.setFloat("bot_stop", 1.0f) == CvarUpdateResult::Updated,
			"bot_stop accepts a valid value"))
	{
		return 1;
	}
	if (!check(state.setFloat("bot_difficulty", 4.0f) == CvarUpdateResult::Updated,
			"difficulty accepts the upper bound"))
	{
		return 1;
	}
	if (!check(state.setFloat("bot_quota", 32.0f) == CvarUpdateResult::Updated,
			"quota accepts the client bound"))
	{
		return 1;
	}
	if (!check(state.setString("bot_join_team", "CT") == CvarUpdateResult::Updated,
			"join team accepts CT"))
	{
		return 1;
	}
	const auto changed = state.snapshot();
	if (!check(changed.botEnable == 1.0f && changed.botStop == 1.0f &&
			changed.botDifficulty == 4 && changed.botQuota == 32 &&
			changed.botJoinTeam == JoinTeam::CounterTerrorist,
			"valid CVar updates are visible"))
	{
		return 1;
	}
	if (!check(state.setFloat("bot_difficulty", 5.0f) == CvarUpdateResult::InvalidValue,
			"difficulty rejects values over the bound"))
	{
		return 1;
	}
	if (!check(state.setFloat("bot_quota", -1.0f) == CvarUpdateResult::InvalidValue,
			"quota rejects negative values"))
	{
		return 1;
	}
	if (!check(state.setString("bot_join_team", "invalid") == CvarUpdateResult::InvalidValue,
			"join team rejects unknown values"))
	{
		return 1;
	}
	if (!check(state.setFloat("unknown_cvar", 1.0f) == CvarUpdateResult::Unknown,
			"unknown CVar is rejected"))
	{
		return 1;
	}
	const auto unchanged = state.snapshot();
	if (!check(unchanged.botDifficulty == changed.botDifficulty &&
			unchanged.botQuota == changed.botQuota &&
			unchanged.botJoinTeam == changed.botJoinTeam,
			"invalid CVar updates are side-effect free"))
	{
		return 1;
	}
	return 0;
}
