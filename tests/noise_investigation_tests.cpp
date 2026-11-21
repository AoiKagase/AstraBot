#include "astrabot/compat/noise_investigation.hpp"
#include "astrabot/compat/friend_death.hpp"
#include <cstdio>
using namespace astrabot;
bool check(bool ok, const char *m)
{
	if (!ok)
		std::fprintf(stderr, "FAIL: %s\n", m);
	return ok;
}
struct Random : compat::ICompatibilityRandomSource
{
	float value = 0;
	int draws = 0;
	bool fail = false;
	compat::RandomFloatResult nextFloat(const compat::RandomRequest &r) override
	{
		++draws;
		if (r.floatLower != 0 || r.floatUpper != 1)
			fail = true;
		return {fail ? compat::RandomStatus::Unavailable : compat::RandomStatus::Ok, value};
	}
	compat::RandomLongResult nextLong(const compat::RandomRequest &) override
	{
		return {compat::RandomStatus::Unavailable, 0};
	}
};
compat::NoiseInvestigationInput input()
{
	compat::NoiseInvestigationInput i{};
	i.actor = {1, 11};
	i.frame = {1, 1, 1};
	i.now = 100;
	i.wellPastSafe = true;
	i.aggression = .5f;
	i.allowed = true;
	i.alive = true;
	i.navRevision = 7;
	i.noise.active = true;
	i.noise.ready = true;
	i.noise.position = {500, 0, 0};
	i.noise.navArea = 1;
	i.noise.navRevision = 7;
	i.noise.timestampSeconds = 99;
	return i;
}
int main()
{
	using Result = compat::NoiseInvestigationResult;
	Random random;
	compat::NoiseInvestigationPlanner p;
	auto i = input();
	compat::EnemyReactionMemory enemy;
	i.wellPastSafe = enemy.wellPastSafe(21.875f, i.aggression);
	if (!check(p.evaluate(i, random) == Result::Ignore && random.draws == 0,
			"strict well-past-safe boundary does not roll"))
		return 1;
	i.now += .01f;
	i.wellPastSafe = enemy.wellPastSafe(21.885f, i.aggression);
	if (!check(p.evaluate(i, random) == Result::Attend && p.active(), "past safe permits investigation"))
		return 1;
	i.noise.active = false;
	i.now += .1f;
	if (!check(p.evaluate(i, random) == Result::Ignore && p.active() && p.target().x == 500,
			"target survives consumed sound"))
		return 1;
	p.reset();
	i = input();
	i.visibleEnemy = true;
	i.wellPastSafe = enemy.wellPastSafe(5.0f, i.aggression);
	p.evaluate(i, random);
	i.visibleEnemy = false;
	i.now += .25f;
	const int beforeContact = random.draws;
	if (!check(p.evaluate(i, random) == Result::Ignore && random.draws == beforeContact,
			"transient visibility without recognition does not shorten the safe period"))
		return 1;
	enemy.adjustSafeTime(6.0f, i.aggression);
	i.wellPastSafe = enemy.wellPastSafe(5.0f, i.aggression);
	if (!check(p.evaluate(i, random) == Result::Ignore && random.draws == beforeContact,
			"shared learned time still excludes the exact boundary"))
		return 1;
	i.wellPastSafe = enemy.wellPastSafe(5.001f, i.aggression);
	++i.frame.roundGeneration;
	++i.navRevision;
	i.noise.navRevision = i.navRevision;
	if (!check(p.evaluate(i, random) == Result::Attend && random.draws == beforeContact + 1,
			"a new round and NAV revision can investigate using the same learned time"))
		return 1;
	p.reset();
	i = input();
	i.allowed = false;
	int draws = random.draws;
	if (!check(p.evaluate(i, random) == Result::Ignore && random.draws == draws,
			"non-investigating disposition preserves task and RNG"))
		return 1;
	p.reset();
	i = input();
	random.value = .9f;
	p.evaluate(i, random);
	draws = random.draws;
	i.now += .249f;
	p.evaluate(i, random);
	if (!check(random.draws == draws, "quarter-second cadence bounds repeated probability checks"))
		return 1;
	i.now = 100.25f;
	p.evaluate(i, random);
	if (!check(random.draws == draws + 1, "cadence boundary permits new roll"))
		return 1;
	p.reset();
	i = input();
	random.value = .65f;
	i.friends = 2;
	if (!check(p.evaluate(i, random) == Result::Attend, "two friends do not reduce probability"))
		return 1;
	p.reset();
	i.friends = 3;
	if (!check(p.evaluate(i, random) == Result::Ignore, "three friends reduce probability"))
		return 1;
	p.reset();
	i = input();
	i.noise.position = {0, 0, 121};
	random.value = 0;
	if (!check(p.evaluate(i, random) == Result::NeedTravel, "height strictly above120 requires route distance"))
		return 1;
	i.travelKnown = true;
	i.travelDistance = 1500;
	if (!check(p.evaluate(i, random) == Result::Attend, "1500 route distance inclusive"))
		return 1;
	p.reset();
	i.travelDistance = 1500.01f;
	draws = random.draws;
	if (!check(
			p.evaluate(i, random) == Result::Ignore && random.draws == draws, "long route rejected before probability"))
		return 1;
	i.now += 1;
	i.travelDistance = 100;
	if (!check(p.evaluate(i, random) == Result::Ignore, "height travel is checked once per noise"))
		return 1;
	p.reset();
	i = input();
	i.hiding = true;
	i.noise.position = {301, 0, 0};
	i.aggression = .5f;
	if (!check(p.evaluate(i, random) == Result::Ignore, "hiding radius depends on aggression"))
		return 1;
	draws = random.draws;
	i.defenseAmbushExpired = true;
	if (!check(p.evaluate(i, random) == Result::Attend && p.target().x == 301 && random.draws == draws,
			"expired ambush attends the remembered sound without another distance or probability check"))
		return 1;
	i.defenseAmbushExpired = false;
	p.reset();
	i.noise.position = {300, 0, 0};
	if (!check(p.evaluate(i, random) == Result::Attend, "hiding radius inclusive boundary"))
		return 1;
	p.reset();
	i = input();
	i.noise.position = {1000, 0, 0};
	p.evaluate(i, random);
	i.now += .3f;
	i.noise.timestampSeconds = 100.3f;
	i.noise.position = {900, 0, 0};
	if (!check(p.evaluate(i, random) == Result::Ignore && p.target().x == 1000,
			"exact100-unit improvement does not retarget"))
		return 1;
	i.now += .3f;
	i.noise.timestampSeconds = 100.6f;
	i.noise.position = {899, 0, 0};
	if (!check(p.evaluate(i, random) == Result::Attend && p.target().x == 899, "more than100 closer retargets"))
		return 1;
	if (!check(compat::investigationShouldWalk(899, 2, false, false) &&
				!compat::investigationShouldWalk(1500, 2, false, false) &&
				compat::investigationShouldWalk(2000, 5, true, false) &&
				!compat::investigationShouldWalk(100, 0, true, true),
			"native knife/friend/hurry walk rules"))
		return 1;
	i.frame.roundGeneration = 2;
	i.noise.active = false;
	p.evaluate(i, random);
	if (!check(!p.active(), "round change clears target"))
		return 1;
	p.reset();
	i = input();
	p.evaluate(i, random);
	i.now = 90;
	i.noise.active = false;
	p.evaluate(i, random);
	if (!check(!p.active(), "clock rewind clears target"))
		return 1;
	p.reset();
	i = input();
	random.fail = true;
	if (!check(
			p.evaluate(i, random) == Result::Ignore && !p.active(), "RNG unavailable never fabricates investigation"))
		return 1;
	p.reset();
	i = input();
	i.noise.ready = false;
	random.fail = false;
	if (!check(p.evaluate(i, random) == Result::Attend,
			"current receipt age must not depend on a previous publication ready flag"))
		return 1;
	p.reset();
	i = input();
	i.noise.ready = true;
	i.noise.timestampSeconds = i.now;
	i.reactionSeconds = .125f;
	if (!check(p.evaluate(i, random) == Result::Ignore, "cached ready cannot bypass current profile reaction delay"))
		return 1;
	i.now += .125f;
	if (!check(p.evaluate(i, random) == Result::Attend, "profile reaction boundary uses decision time"))
		return 1;
	p.reset();
	i = input();
	i.noise.timestampSeconds = i.now;
	i.reactionSeconds = astrabot::compat::ProfileRecord::kMinimumReactionSeconds;
	if (!check(p.evaluate(i, random) == Result::Attend, "an adjusted zero profile may investigate immediately"))
		return 1;
	p.reset();
	i = input();
	i.noise.timestampSeconds = 79.75f;
	if (!check(p.evaluate(i, random) == Result::Attend, "the investigation planner leaves expiration to the full update"))
		return 1;
	return 0;
}
