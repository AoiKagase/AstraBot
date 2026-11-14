#include "astrabot/compat/enemy_count_chatter.hpp"
#include "astrabot/compat/outgoing_radio.hpp"
#include <cstdio>
#include <cstring>
#include <limits>

namespace
{
class CountRandom : public astrabot::compat::ICompatibilityRandomSource
{
public:
	astrabot::compat::RandomFloatResult nextFloat(const astrabot::compat::RandomRequest &r) override
	{
		++draws;
		valid = valid && r.type == astrabot::compat::RandomType::Float && r.floatLower == 2 && r.floatUpper == 4 &&
			r.actor.slot == 1 && r.actor.generation == 2 &&
			std::strcmp(r.semanticId, "RNG-CSBOT-ENEMIES-REMAINING-DELAY") == 0;
		return {status, delay};
	}
	astrabot::compat::RandomLongResult nextLong(const astrabot::compat::RandomRequest &) override
	{
		return {astrabot::compat::RandomStatus::Unavailable, 0};
	}
	unsigned draws = 0;
	bool valid = true;
	float delay = 2;
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
	EnemyCountDeathInput event{};
	event.actor = {1, 2};
	event.frame = {1, 1, 1};
	event.now = 100;
	event.alive = true;
	event.victimEnemy = true;
	event.killerFriendly = true;
	event.enabled = true;
	EnemyCountSpeechInput speech{};
	speech.actor = event.actor;
	speech.frame = event.frame;
	speech.now = 100;
	speech.alive = true;
	speech.enabled = true;
	speech.friendsRemaining = 1;
	speech.enemiesRemaining = 3;
	EnemyCountChatterPolicy policy;
	CountRandom rng;
	for (unsigned gate = 0; gate < 4; ++gate)
	{
		auto rejected = event;
		if (gate == 0)
			rejected.alive = false;
		if (gate == 1)
			rejected.victimEnemy = false;
		if (gate == 2)
			rejected.killerFriendly = false;
		if (gate == 3)
			rejected.nearbyEnemies = 2;
		check(!policy.observe(rejected, rng) && rng.draws == 0, "ineligible death draws nothing");
	}
	event.enabled = false;
	check(!policy.observe(event, rng) && rng.draws == 1 && !policy.active(),
		"muted context still draws before AddStatement suppression");
	event.enabled = true;
	event.nearbyEnemies = 1;
	check(policy.observe(event, rng) && rng.draws == 2 && rng.valid, "one nearby enemy queues named shared delay");
	speech.now = 101.99f;
	check(!policy.update(speech).ready, "two-second start delay is retained");
	event.now = 101.99f;
	rng.delay = 4;
	check(!policy.observe(event, rng) && rng.draws == 3, "redundant event draws but keeps earliest statement");
	speech.now = 102;
	auto out = policy.update(speech);
	check(out.ready && out.enemiesRemaining == 3 && out.startTime == 102,
		"earliest start is not shifted by redundant death");
	speech.enemiesRemaining = 1;
	check(policy.update(speech).enemiesRemaining == 1, "count is evaluated at speaking time");
	for (int n = 0; n <= 3; ++n)
	{
		speech.enemiesRemaining = n;
		check(policy.update(speech).ready, "all0..3 counts are speakable");
	}
	speech.now = 105;
	speech.enemiesRemaining = 0;
	check(policy.update(speech).ready, "strict five-second lifetime includes boundary");
	speech.now = 105.01f;
	check(!policy.update(speech).ready && !policy.active(), "strictly later than five seconds expires");
	for (unsigned gate = 0; gate < 10; ++gate)
	{
		policy.reset();
		event.now = 100;
		rng.delay = 2;
		policy.observe(event, rng);
		auto rejected = speech;
		rejected.now = 102;
		rejected.enemiesRemaining = 1;
		if (gate == 0)
			rejected.alive = false;
		if (gate == 1)
			rejected.roundOver = true;
		if (gate == 2)
			rejected.frame.mapGeneration++;
		if (gate == 3)
			rejected.frame.roundGeneration++;
		if (gate == 4)
			rejected.actor.generation++;
		if (gate == 5)
			rejected.enabled = false;
		if (gate == 6)
			rejected.friendsRemaining = 0;
		if (gate == 7)
			rejected.enemiesRemaining = 4;
		if (gate == 8)
			rejected.now = 99;
		if (gate == 9)
			rejected.entityGeneration++;
		check(!policy.update(rejected).ready && !policy.active(),
			"lifetime/mode/friends/range invalidation consumes statement");
		rejected = speech;
		rejected.now = 102;
		rejected.enemiesRemaining = 1;
		check(!policy.update(rejected).ready, "invalidated statement cannot replay on catch-up/revival");
	}
	policy.reset();
	policy.observe(event, rng);
	speech.now = 100.5f;
	speech.enemiesRemaining = 1;
	speech.teamBusyUntil = 104;
	speech.teamReportingRemaining = true;
	check(!policy.update(speech).ready && !policy.active(), "same team topic suppresses even before start time");
	policy.reset();
	policy.observe(event, rng);
	speech.teamReportingRemaining = false;
	speech.now = 102;
	check(!policy.update(speech).ready && policy.active(), "unrelated team speech waits without destroying report");
	speech.now = 104;
	speech.teamBusyUntil = 0;
	check(policy.update(speech).ready, "queued report resumes after unrelated speech");
	policy.discard();
	check(!policy.update(speech).ready, "sent or unavailable statement cannot replay");
	policy.reset();
	rng.delay = 4;
	policy.observe(event, rng);
	speech.now = 103.99f;
	check(!policy.update(speech).ready, "four-second endpoint delay stays exact");
	speech.now = 104;
	check(policy.update(speech).ready, "four-second endpoint speaks");
	policy.reset();
	rng.status = RandomStatus::Unavailable;
	check(!policy.observe(event, rng) && !policy.active(), "unavailable shared RNG creates no report");
	rng.status = RandomStatus::Ok;
	rng.delay = std::numeric_limits<float>::quiet_NaN();
	check(!policy.observe(event, rng), "nonfinite random delay is rejected");
	rng.delay = 4.1f;
	check(!policy.observe(event, rng), "out-of-range delay is rejected");
	policy.reset();
	event.now = 100;
	rng.delay = 2;
	policy.observe(event, rng);
	speech.now = 102;
	speech.enemiesRemaining = 4;
	OutgoingRadioScheduler older;
	OutgoingRadioInput olderInput{};
	olderInput.actor = event.actor;
	olderInput.frame = event.frame;
	olderInput.now = 100;
	olderInput.alive = true;
	olderInput.team = 1;
	olderInput.friendCount = 1;
	olderInput.plantingBomb = true;
	olderInput.teamBusyUntil = 101;
	ChatterRadioCatalog emptyCatalog;
	const auto olderVoice = 1U << static_cast<unsigned>(ChatterPhrase::PlantingBomb);
	older.update(olderInput, emptyCatalog, olderVoice);
	olderInput.now = 102;
	olderInput.teamBusyUntil = 0;
	const auto olderOutput = older.update(olderInput, emptyCatalog, olderVoice);
	check(olderOutput.status == OutgoingRadioStatus::Ready, "earlier generic statement owns the speech slot");
	speech.canStart = !older.hasPending() || policy.scheduledStart() < older.scheduledStart();
	check(!policy.update(speech).ready && policy.active(),
		"older statement retains count context until actual speech slot");
	older.confirmSent(olderInput, olderOutput);
	speech.now = 102.4f;
	speech.enemiesRemaining = 3;
	speech.canStart = !older.hasPending() || policy.scheduledStart() < older.scheduledStart();
	check(policy.update(speech).ready && policy.update(speech).enemiesRemaining == 3,
		"delayed context evaluates new count when its slot starts");
	for (int n = 0; n <= 3; ++n)
	{
		const auto phrase = remainingEnemyPhrase(n);
		check(isRemainingEnemyPhrase(phrase), "count phrases share one report topic");
		check(std::strcmp(ChatterRadioCatalog::name(phrase),
				  n == 0	   ? "NoEnemiesLeft"
					  : n == 1 ? "OneEnemyLeft"
					  : n == 2 ? "TwoEnemiesLeft"
							   : "ThreeEnemiesLeft") == 0,
			"host phrase names match native contexts");
	}
	check(remainingEnemyPhrase(4) == ChatterPhrase::None && !isRemainingEnemyPhrase(ChatterPhrase::EnemySpotted),
		"other phrases do not share remaining-count topic");
	return failures ? 1 : 0;
}
