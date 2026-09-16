#include "astrabot/team/radio_intent.hpp"

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

	astrabot::world::FrameIdentity frame(std::uint32_t tick)
	{
		return {3U, 4U, tick};
	}

	astrabot::team::TeamReport report()
	{
	astrabot::team::TeamReport value = {};
	value.id = 10U;
		value.source.actor = {2U, 6U};
		value.source.team = astrabot::team::TeamRole::CounterTerrorist;
		value.source.frame = frame(10U);
		value.kind = astrabot::team::ReportKind::NeedBackup;
		value.subject = {3U, 7U};
		value.state = astrabot::team::ReportState::Observed;
		value.position = {128.0f, 64.0f, 16.0f};
		value.confidence = 0.9f;
		value.ageTicks = 0U;
		value.expiresAtTick = 40U;
		return value;
	}
}

bool testRadioIntentAndCooldown()
{
	using astrabot::team::RadioController;
	using astrabot::team::RadioIntent;
	using astrabot::team::RadioResult;

	RadioController controller({1U, 5U}, astrabot::team::TeamRole::CounterTerrorist);
	RadioIntent intent = {};
	if (!check(controller.compose(report(), frame(10U), &intent) ==
			RadioResult::IntentReady && intent.isIntent() &&
			!intent.isDelivered() && intent.recipientCount == 0U,
			"valid report produces undelivered radio intent"))
	{
		return false;
	}

	if (!check(controller.compose(report(), frame(11U), &intent) ==
			RadioResult::Cooldown,
			"radio cooldown suppresses repeated intent"))
	{
		return false;
	}

	return check(controller.compose(report(), frame(14U), &intent) ==
			RadioResult::IntentReady && intent.frame.tick == 14U,
		"radio intent becomes available after cooldown");
}

bool testUnknownInformationAndGeneration()
{
	using astrabot::team::RadioController;
	using astrabot::team::RadioIntent;
	using astrabot::team::RadioResult;

	RadioController controller({1U, 5U}, astrabot::team::TeamRole::CounterTerrorist);
	astrabot::team::TeamReport unknown = report();
	unknown.state = astrabot::team::ReportState::Unknown;
	RadioIntent intent = {};
	if (!check(controller.compose(unknown, frame(10U), &intent) ==
			RadioResult::UnknownInformation && !intent.isIntent(),
			"unknown report does not create radio certainty"))
	{
		return false;
	}

	astrabot::team::TeamReport stale = report();
	stale.source.actor.generation = 4U;
	if (!check(controller.compose(stale, frame(10U), &intent) ==
			RadioResult::StaleGeneration,
			"stale reporter generation is rejected"))
	{
		return false;
	}

	astrabot::team::TeamReport wrongTeam = report();
	wrongTeam.source.team = astrabot::team::TeamRole::Terrorist;
	return check(controller.compose(wrongTeam, frame(10U), &intent) ==
			RadioResult::WrongTeam,
		"cross-team radio source is rejected");
}

int main()
{
	if (!testRadioIntentAndCooldown() ||
			!testUnknownInformationAndGeneration())
	{
		return 1;
	}

	return 0;
}
