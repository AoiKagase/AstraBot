#include "astrabot/compat/noise_report.hpp"
#include "astrabot/compat/outgoing_radio.hpp"
#include "astrabot/metamod/round_lifecycle_tracker.hpp"
#include <cstdio>
#include <cstring>
#include <limits>

namespace
{
class ReportRandom : public astrabot::compat::ICompatibilityRandomSource
{
public:
	astrabot::compat::RandomFloatResult nextFloat(const astrabot::compat::RandomRequest &request) override
	{
		++draws;
		validRequest = request.type == astrabot::compat::RandomType::Float && request.floatLower == 0 &&
			request.floatUpper == 100 && request.actor.slot == 1 && request.actor.generation == 2 &&
			std::strcmp(request.semanticId, "RNG-CSBOT-HEARD-NOISE-REPORT") == 0;
		return {status, value};
	}

	astrabot::compat::RandomLongResult nextLong(const astrabot::compat::RandomRequest &) override
	{
		return {astrabot::compat::RandomStatus::Unavailable, 0};
	}

	unsigned draws = 0;
	bool validRequest = false;
	float value = 32.99f;
	astrabot::compat::RandomStatus status = astrabot::compat::RandomStatus::Ok;
};
}

int main()
{
	using namespace astrabot;
	using namespace compat;
	int failures = 0;
	const auto check = [&](bool ok, const char *name)
	{
		if (!ok)
		{
			++failures;
			std::fprintf(stderr, "FAIL %s\n", name);
		}
	};
	NoiseReportInput in{};
	in.actor = {1, 2};
	in.frame = {1, 1, 1};
	in.now = 100;
	in.lastSawEnemy = 70;
	in.alive = true;
	in.wellPastSafe = true;
	in.priority = world::NoisePriority::Medium;
	NoiseReportPolicy policy;
	ReportRandom rng;
	check(!policy.attend(in, rng) && rng.draws == 0, "exactly30 enemy-free seconds draws nothing");
	in.lastSawEnemy = 69;
	in.priority = world::NoisePriority::High;
	check(!policy.attend(in, rng) && rng.draws == 0, "high priority noise draws nothing");
	in.priority = world::NoisePriority::Medium;
	in.roundOver = true;
	check(!policy.attend(in, rng) && rng.draws == 0, "round over draws nothing");
	in.roundOver = false;
	in.wellPastSafe = false;
	check(!policy.attend(in, rng) && rng.draws == 0, "safe-time gate draws nothing");
	in.wellPastSafe = true;
	rng.value = 33;
	check(!policy.attend(in, rng) && rng.draws == 1 && rng.validRequest, "33 percent boundary rejects by named draw");
	in.now = 120;
	rng.value = 0;
	check(!policy.attend(in, rng) && rng.draws == 1, "failed chance still suppresses at exactly20 seconds");
	in.now = 120.01f;
	check(policy.attend(in, rng) && rng.draws == 2, "cooldown elapsed admits low draw");
	in.frame.roundGeneration++;
	check(policy.attend(in, rng), "new round resets cooldown");
	in.actor.generation++;
	check(policy.attend(in, rng), "new actor resets cooldown");
	in.frame.mapGeneration++;
	check(policy.attend(in, rng), "new map resets cooldown");
	in.now = 100;
	check(policy.attend(in, rng), "clock rollback resets cooldown");
	in.now = 121;
	rng.status = RandomStatus::Unavailable;
	check(!policy.attend(in, rng), "unavailable RNG emits nothing");
	rng.status = RandomStatus::Ok;
	in.now = 130;
	check(!policy.attend(in, rng), "failed RNG retains suppression");
	in.now = 142;
	rng.value = std::numeric_limits<float>::quiet_NaN();
	check(!policy.attend(in, rng), "nonfinite RNG emits nothing");

	ChatterRadioCatalog catalog;
	const char definitions[] = "Chatter HeardNoise End Chatter PlantingBomb Important End";
	check(catalog.loadText(definitions, sizeof(definitions) - 1), "native default metadata defines voice-only phrases");
	OutgoingRadioScheduler scheduler;
	OutgoingRadioInput radio{};
	radio.actor = {1, 2};
	radio.frame = {1, 1, 1};
	radio.now = 100;
	radio.alive = true;
	radio.team = 1;
	radio.friendCount = 1;
	radio.navRevision = 7;
	radio.noiseReport = {true, radio.actor, radio.frame, 100, 9, 7};
	const auto voice = 1U << static_cast<unsigned>(ChatterPhrase::HeardNoise);
	radio.teamBusyUntil = 110;
	radio.teamCurrentPhrase = ChatterPhrase::HeardNoise;
	radio.teamCurrentPlace = 8;
	check(scheduler.update(radio, catalog, voice).status == OutgoingRadioStatus::WaitingTeam,
		"different report places wait instead of being discarded");
	radio.now = 105;
	radio.teamBusyUntil = 0;
	auto out = scheduler.update(radio, catalog, voice);
	check(out.status == OutgoingRadioStatus::Ready && out.phrase == ChatterPhrase::HeardNoise && out.place == 9,
		"exactly five seconds remains speakable with original noise place");
	radio.now = 105.01f;
	radio.canStart = false;
	check(scheduler.update(radio, catalog, voice).status == OutgoingRadioStatus::None,
		"unselected owner maintenance expires report strictly after five seconds");
	radio.canStart = true;
	for (unsigned place : {0U, 9U})
	{
		OutgoingRadioScheduler duplicate;
		radio.now = 100;
		radio.noiseReport.place = static_cast<std::uint16_t>(place);
		radio.teamCurrentPlace = static_cast<std::uint16_t>(place);
		radio.teamBusyUntil = 110;
		check(duplicate.update(radio, catalog, voice).status == OutgoingRadioStatus::None,
			"same defined or both undefined place suppress redundant report");
	}
	OutgoingRadioScheduler unknownPlace;
	radio.noiseReport.place = 0;
	radio.teamCurrentPlace = 9;
	check(unknownPlace.update(radio, catalog, voice).status == OutgoingRadioStatus::WaitingTeam,
		"one undefined place does not prove redundancy");
	radio.roundOver = true;
	check(unknownPlace.update(radio, catalog, voice).status == OutgoingRadioStatus::None,
		"round over cancels pending report");
	radio.roundOver = false;
	OutgoingRadioScheduler stale;
	radio.navRevision++;
	check(stale.update(radio, catalog, voice).status == OutgoingRadioStatus::None,
		"NAV replacement rejects old report");
	OutgoingRadioScheduler rewind;
	radio.navRevision = 7;
	radio.now = 100;
	radio.noiseReport = {true, radio.actor, radio.frame, 100, 9, 7};
	radio.teamCurrentPhrase = ChatterPhrase::None;
	radio.teamBusyUntil = 110;
	check(rewind.update(radio, catalog, voice).status == OutgoingRadioStatus::WaitingTeam,
		"report is pending before clock reversal");
	radio.now = 90;
	rewind.update(radio, catalog, voice);
	radio.now = 100;
	radio.teamBusyUntil = 0;
	check(rewind.update(radio, catalog, voice).status == OutgoingRadioStatus::None,
		"clock reversal discards old report even after time catches up");
	radio.noiseReport.created = 99;
	check(rewind.update(radio, catalog, voice).status == OutgoingRadioStatus::Ready,
		"fresh report from new clock interval is not blocked by former timestamp");
	OutgoingRadioScheduler dead;
	radio.alive = false;
	dead.update(radio, catalog, voice);
	radio.alive = true;
	check(dead.update(radio, catalog, voice).status == OutgoingRadioStatus::None,
		"death cannot replay a discarded report on resurrection");
	OutgoingRadioScheduler cancelled;
	radio.now = 100;
	radio.noiseReport = {true, radio.actor, radio.frame, 100, 9, 7};
	radio.teamBusyUntil = 110;
	check(cancelled.update(radio, catalog, voice).status == OutgoingRadioStatus::WaitingTeam,
		"report is queued before asynchronous death cancellation");
	radio.noiseReport = {};
	radio.now = 100.5f;
	radio.teamBusyUntil = 0;
	check(cancelled.update(radio, catalog, voice).status == OutgoingRadioStatus::None,
		"cleared death receipt cancels pending report even after revival between full updates");
	metamod::RoundLifecycleTracker late;
	check(!late.shouldBeginNewGeneration(metamod::RoundLifecycleEvent::RoundEnd),
		"late first round end preserves existing generation gate");
	check(late.isRoundOver(), "first observed round end after late attach still suppresses reports");
	late.shouldBeginNewGeneration(metamod::RoundLifecycleEvent::RoundStart);
	check(!late.isRoundOver(), "observed round start clears round-over report gate");
	OutgoingRadioScheduler offOccupied;
	radio.friendCount = 1;
	radio.noiseReport = {};
	radio.now = 100;
	radio.plantingBomb = true;
	const auto plantingVoice = 1U << static_cast<unsigned>(ChatterPhrase::PlantingBomb);
	check(offOccupied.update(radio, catalog, voice | plantingVoice).phrase == ChatterPhrase::PlantingBomb,
		"another statement occupies the outgoing slot before off-mode noise attendance");
	radio.noiseReport = {true, radio.actor, radio.frame, 100, 9, 7};
	offOccupied.update(radio, catalog, voice | plantingVoice);
	offOccupied.discardNoiseReport(radio.noiseReport);
	offOccupied.discardPending();
	radio.plantingBomb = false;
	radio.now = 100.5f;
	check(offOccupied.update(radio, catalog, voice | plantingVoice).status == OutgoingRadioStatus::None,
		"off-created report cannot survive behind another pending topic after normal mode resumes");
	return failures ? 1 : 0;
}
