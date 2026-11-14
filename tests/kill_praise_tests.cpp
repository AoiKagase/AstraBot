#include "astrabot/compat/kill_praise.hpp"
#include "astrabot/compat/chatter_voice.hpp"
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>
namespace
{
class PraiseRandom : public astrabot::compat::ICompatibilityRandomSource
{
public:
	astrabot::compat::RandomFloatResult nextFloat(const astrabot::compat::RandomRequest &r) override
	{
		ids.push_back(r.semanticId);
		valid = valid && r.type == astrabot::compat::RandomType::Float && r.actor.slot == 1 && r.actor.generation == 2;
		if (std::strcmp(r.semanticId, "RNG-CSBOT-KILL-PRAISE-DELAY") == 0)
		{
			valid = valid && r.floatLower == 2 && r.floatUpper == 3;
			return {status, delay};
		}
		valid = valid && std::strcmp(r.semanticId, "RNG-CSBOT-KILL-PRAISE-BOT-CHANCE") == 0 && r.floatLower == 0 &&
			r.floatUpper == 100;
		return {status, chance};
	}
	astrabot::compat::RandomLongResult nextLong(const astrabot::compat::RandomRequest &) override
	{
		return {astrabot::compat::RandomStatus::Unavailable, 0};
	}
	std::vector<std::string> ids;
	bool valid = true;
	float delay = 2;
	float chance = 39;
	astrabot::compat::RandomStatus status = astrabot::compat::RandomStatus::Ok;
};
}
int main()
{
	using namespace astrabot;
	using namespace compat;
	unsigned failures = 0;
	const auto check = [&](bool ok, const char *message)
	{
		if (!ok)
		{
			++failures;
			std::fprintf(stderr, "FAIL %s\n", message);
		}
	};
	const auto bit = [](ChatterPhrase p)
	{
		return 1U << static_cast<unsigned>(p);
	};
	const auto all =
		bit(ChatterPhrase::NiceShot) | bit(ChatterPhrase::NiceShotSir) | bit(ChatterPhrase::NiceShotCommander);
	KillPraiseDeathInput event{};
	event.actor = {1, 2};
	event.frame = {1, 1, 1};
	event.now = 100;
	event.alive = true;
	event.victimEnemy = true;
	event.killerFriendly = true;
	event.victimVisible = true;
	event.careerKnown = true;
	event.allowedPhrases = all;
	KillPraiseSpeechInput speech{};
	speech.actor = event.actor;
	speech.frame = event.frame;
	speech.now = 102;
	speech.alive = true;
	speech.friendsRemaining = 1;
	speech.allowedPhrases = all;
	KillPraisePolicy policy;
	PraiseRandom rng;
	for (unsigned gate = 0; gate < 6; ++gate)
	{
		auto rejected = event;
		if (gate == 0)
			rejected.alive = false;
		if (gate == 1)
			rejected.victimEnemy = false;
		if (gate == 2)
			rejected.killerFriendly = false;
		if (gate == 3)
			rejected.victimVisible = false;
		if (gate == 4)
			rejected.killerSelf = true;
		if (gate == 5)
			rejected.nearbyEnemies = 2;
		check(!policy.observe(rejected, rng) && rng.ids.empty(), "ineligible kill performs no praise draw");
	}
	check(policy.observe(event, rng) && rng.ids.size() == 1 && rng.valid, "human kill draws only2..3-second delay");
	speech.now = 101.99f;
	check(!policy.update(speech).ready, "human praise waits its delay");
	speech.now = 102;
	check(policy.update(speech).phrase == ChatterPhrase::NiceShotSir, "ordinary human killer gets honorific");
	event.now = 102;
	event.career = true;
	check(!policy.observe(event, rng) && rng.ids.size() == 2, "duplicate intention draws but retains earliest phrase");
	check(policy.update(speech).phrase == ChatterPhrase::NiceShotSir,
		"later Career variant cannot replace earlier pending intention");
	speech.now = 103;
	check(policy.update(speech).ready, "exact3-second lifetime remains valid");
	speech.now = 103.01f;
	check(!policy.update(speech).ready && !policy.active(), "strictly beyond3 seconds expires");
	policy.reset();
	event.now = 100;
	check(policy.observe(event, rng), "Career human can queue");
	speech.now = 102;
	check(policy.update(speech).phrase == ChatterPhrase::NiceShotCommander, "public Career true selects Commander");
	policy.reset();
	event.careerKnown = false;
	const auto unknownBefore = rng.ids.size();
	check(!policy.observe(event, rng) && rng.ids.size() == unknownBefore + 1,
		"unknown public Career consumes delay without assuming human variant");
	event.killerBot = true;
	check(policy.observe(event, rng) && rng.ids[rng.ids.size() - 2] == "RNG-CSBOT-KILL-PRAISE-DELAY" &&
			rng.ids.back() == "RNG-CSBOT-KILL-PRAISE-BOT-CHANCE",
		"Bot killer draws chance after delay even without Career getter");
	check(policy.update(speech).phrase == ChatterPhrase::NiceShot, "Bot chance below40 selects NiceShot");
	policy.reset();
	rng.chance = 40;
	check(!policy.observe(event, rng) && !policy.active(), "exact40 chance fails");
	rng.chance = 0;
	event.allowedPhrases = 0;
	const auto muteBefore = rng.ids.size();
	check(!policy.observe(event, rng) && rng.ids.size() == muteBefore + 2,
		"mute/missing phrase does not skip delay or Bot chance");
	event.allowedPhrases = all;
	for (unsigned gate = 0; gate < 9; ++gate)
	{
		policy.reset();
		policy.observe(event, rng);
		auto rejected = speech;
		rejected.now = 102;
		if (gate == 0)
			rejected.alive = false;
		if (gate == 1)
			rejected.roundOver = true;
		if (gate == 2)
			rejected.actor.generation++;
		if (gate == 3)
			rejected.entityGeneration++;
		if (gate == 4)
			rejected.frame.mapGeneration++;
		if (gate == 5)
			rejected.frame.roundGeneration++;
		if (gate == 6)
			rejected.now = 99;
		if (gate == 7)
			rejected.allowedPhrases = 0;
		if (gate == 8)
			rejected.friendsRemaining = 0;
		check(!policy.update(rejected).ready && !policy.active(),
			"lifetime/admission/no-friends cancellation clears praise");
		check(!policy.update(speech).ready, "cancelled praise cannot replay after revival or catch-up");
	}
	policy.reset();
	policy.observe(event, rng);
	speech.now = 100.5f;
	speech.teamBusyUntil = 104;
	speech.teamPraising = true;
	check(!policy.update(speech).ready && !policy.active(), "same intention suppresses even before praise start");
	policy.reset();
	policy.observe(event, rng);
	speech.teamPraising = false;
	speech.now = 102;
	check(!policy.update(speech).ready && policy.active(), "unrelated team speech retains pending praise");
	speech.teamBusyUntil = 0;
	speech.canStart = false;
	speech.friendsRemaining = 0;
	check(
		!policy.update(speech).ready && policy.active(), "unselected topic cannot consume speech context prematurely");
	speech.friendsRemaining = 1;
	speech.canStart = true;
	check(policy.update(speech).ready, "selected praise speaks when friends become available");
	policy.discard();
	check(!policy.update(speech).ready, "confirmed or unavailable praise is consumed");
	policy.reset();
	rng.status = RandomStatus::Unavailable;
	check(!policy.observe(event, rng), "unavailable RNG creates no praise");
	rng.status = RandomStatus::Ok;
	rng.delay = std::numeric_limits<float>::quiet_NaN();
	check(!policy.observe(event, rng), "invalid random delay creates no praise");
	check(killPraiseViewCone(100, 0, 0, 0) && !killPraiseViewCone(-100, 0, 0, 0),
		"native2D cone includes front and rejects rear");
	check(!killPraiseViewCone(0, 0, 0, 0) && !killPraiseViewCone(100, 0, 89, 0),
		"native zero vector and pitched forward projection fail cone");
	const char db[] =
		"Chatter NiceShot Important Count 2 wrong_count.wav Count Many many.wav End Chatter NiceShotSir sir.wav End";
	ChatterRadioCatalog radio;
	ChatterVoiceCatalog voice;
	check(radio.loadText(db, sizeof(db) - 1) && voice.loadText(db, sizeof(db) - 1), "praise host metadata parses");
	check(radio.defined(ChatterPhrase::NiceShot) && radio.important(ChatterPhrase::NiceShot) &&
			voice.has(ChatterPhrase::NiceShot),
		"definition and Important metadata are explicit");
	auto *fixed = voice.select(ChatterPhrase::NiceShot, -1, "");
	check(fixed && fixed->sample == "many.wav", "native unspecified count accepts Many but rejects Count2");
	voice.retainAvailable(
		[](const std::string &)
		{
			return false;
		});
	check(!voice.has(ChatterPhrase::NiceShot) && radio.defined(ChatterPhrase::NiceShot) &&
			radio.important(ChatterPhrase::NiceShot),
		"missing waves do not erase native phrase definition");
	check(!voice.has(ChatterPhrase::NiceShotCommander) && !radio.defined(ChatterPhrase::NiceShotCommander),
		"undefined variant is not presumed available");
	return failures ? 1 : 0;
}
