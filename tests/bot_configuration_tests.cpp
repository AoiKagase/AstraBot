#include "astrabot/compat/bot_configuration.hpp"

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
	using astrabot::compat::BotActionResult;
	using astrabot::compat::BotConfiguration;
	using astrabot::compat::CommandAction;
	using astrabot::compat::CommandId;
	using astrabot::compat::CommandTeam;
	using astrabot::compat::CvarUpdateResult;

	BotConfiguration configuration;
	if (!check(configuration.setFloat("bot_enable", 1.0f) == CvarUpdateResult::Updated,
			"enable desired state is updated"))
	{
		return 1;
	}
	if (!check(configuration.setFloat("bot_stop", 0.0f) == CvarUpdateResult::NoChange,
			"stop desired state starts disabled"))
	{
		return 1;
	}
	if (!check(configuration.setFloat("bot_difficulty", 3.0f) ==
			CvarUpdateResult::Updated &&
			configuration.setFloat("bot_quota", 2.0f) == CvarUpdateResult::Updated,
			"difficulty and quota desired states are updated"))
	{
		return 1;
	}
	if (!check(configuration.setString("bot_join_team", "CT") == CvarUpdateResult::Updated,
			"team desired state is updated"))
	{
		return 1;
	}

	const astrabot::compat::CvarSnapshot beforeInvalid = configuration.snapshot();
	if (!check(configuration.setFloat("bot_quota", 33.0f) ==
			CvarUpdateResult::InvalidValue &&
			configuration.snapshot().botQuota == beforeInvalid.botQuota,
			"invalid quota leaves desired state unchanged"))
	{
		return 1;
	}
	if (!check(configuration.setString("bot_join_team", "invalid") ==
			CvarUpdateResult::InvalidValue &&
			configuration.snapshot().botJoinTeam == beforeInvalid.botJoinTeam,
			"invalid team leaves desired state unchanged"))
	{
		return 1;
	}

	const CommandAction add = {CommandId::Add, CommandTeam::Any, false, nullptr};
	if (!check(configuration.authorize(add, 0U, true) == BotActionResult::Allowed,
			"enabled add is authorized under quota"))
	{
		return 1;
	}
	if (!check(configuration.authorize(add, 2U, true) == BotActionResult::QuotaReached,
			"quota rejects an additional actor"))
	{
		return 1;
	}
	if (!check(configuration.authorize(add, 0U, false) == BotActionResult::NativeGuardDenied,
			"native guard denial is explicit"))
	{
		return 1;
	}
	if (!check(configuration.setFloat("bot_stop", 1.0f) == CvarUpdateResult::Updated &&
			configuration.authorize(add, 0U, true) == BotActionResult::Stopped,
			"stop state rejects an additional actor"))
	{
		return 1;
	}
	if (!check(configuration.setFloat("bot_stop", 0.0f) == CvarUpdateResult::Updated &&
			configuration.setFloat("bot_enable", 0.0f) == CvarUpdateResult::Updated &&
			configuration.authorize(add, 0U, true) == BotActionResult::Disabled,
			"disabled state rejects an additional actor"))
	{
		return 1;
	}

	const CommandAction remove = {CommandId::Kick, CommandTeam::Any, true, nullptr};
	if (!check(configuration.authorize(remove, 0U, true) == BotActionResult::NoTarget,
			"remove with no actors is explicit"))
	{
		return 1;
	}
	if (!check(configuration.authorize(remove, 1U, true) == BotActionResult::Allowed,
			"remove with an actor is authorized"))
	{
		return 1;
	}

	const CommandAction invalid = {
		static_cast<CommandId>(99), CommandTeam::Any, false, nullptr};
	if (!check(configuration.authorize(invalid, 0U, true) == BotActionResult::InvalidRequest,
			"unknown action is rejected without state changes"))
	{
		return 1;
	}

	return 0;
}
