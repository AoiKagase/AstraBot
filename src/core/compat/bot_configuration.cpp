#include "astrabot/compat/bot_configuration.hpp"
#include <algorithm>

namespace astrabot
{
namespace compat
{
int calculateDesiredBotCount(const CvarSnapshot &settings, const QuotaPopulation &population)
{
	if (settings.botEnable == 0.0f) return 0;
	if (population.elapsedMapSeconds < settings.botJoinDelay) return 0;
	if ((settings.botJoinAfterPlayer == 1 && population.activeHumanClients <= 0) ||
		(settings.botJoinAfterPlayer == 2 && population.humanClients <= 0)) return 0;
	int desired = settings.botQuota;
	if (settings.botQuotaMode == QuotaMode::Fill) desired -= population.activeHumanClients;
	else if (settings.botQuotaMode == QuotaMode::Match) desired *= population.activeHumanClients;
	if (population.lateJoinBlocked && settings.botQuotaMode != QuotaMode::Normal) desired = population.managedBotClients;
	const int capacity = population.maxClients - population.humanClients - (settings.botAutoVacate ? 1 : 0);
	return std::max(0, std::min(desired, capacity));
}
BotConfiguration::BotConfiguration() : cvarState_()
{
}

CvarUpdateResult BotConfiguration::setFloat(const char *name, float value)
{
	return cvarState_.setFloat(name, value);
}

CvarUpdateResult BotConfiguration::setString(const char *name, const char *value)
{
	return cvarState_.setString(name, value);
}

CvarSnapshot BotConfiguration::snapshot() const
{
	return cvarState_.snapshot();
}

BotActionResult BotConfiguration::authorize(
	const CommandAction &action,
	std::size_t managedActorCount,
	bool nativeGuardAllowsCreation) const
{
	if (!isValidTeam(action.team))
	{
		return BotActionResult::InvalidRequest;
	}

	switch (action.id)
	{
	case CommandId::Add:
	{
		if (!nativeGuardAllowsCreation)
		{
			return BotActionResult::NativeGuardDenied;
		}
		const CvarSnapshot state = cvarState_.snapshot();
		if (state.botEnable == 0.0f)
		{
			return BotActionResult::Disabled;
		}
		if (state.botStop != 0.0f)
		{
			return BotActionResult::Stopped;
		}
		return BotActionResult::Allowed;
	}
	case CommandId::Kick:
	case CommandId::Kill:
		return managedActorCount == 0U ? BotActionResult::NoTarget :
			BotActionResult::Allowed;
	case CommandId::About:
	case CommandId::KnivesOnly:
	case CommandId::PistolsOnly:
	case CommandId::SnipersOnly:
	case CommandId::AllWeapons:
		return BotActionResult::Allowed;
	default:
		return BotActionResult::InvalidRequest;
	}
}

bool BotConfiguration::isValidTeam(CommandTeam team)
{
	return team == CommandTeam::Any || team == CommandTeam::Terrorist ||
			team == CommandTeam::CounterTerrorist;
}
}
}
