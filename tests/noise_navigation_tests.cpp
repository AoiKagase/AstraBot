#include "astrabot/compat/noise_navigation.hpp"
#include <cstdio>
#include <cmath>
#include <limits>
using namespace astrabot;
bool check(bool ok, const char *m)
{
	if (!ok)
		std::fprintf(stderr, "FAIL: %s\n", m);
	return ok;
}
struct Random : compat::ICompatibilityRandomSource
{
	int draws = 0;
	float lower = 0, upper = 0;
	bool fail = false;
	compat::RandomFloatResult nextFloat(const compat::RandomRequest &r) override
	{
		++draws;
		lower = r.floatLower;
		upper = r.floatUpper;
		return {fail ? compat::RandomStatus::Unavailable : compat::RandomStatus::Ok, r.floatUpper};
	}
	compat::RandomLongResult nextLong(const compat::RandomRequest &) override
	{
		return {compat::RandomStatus::Unavailable, 0};
	}
};
struct Probe : compat::INoiseNavProbe
{
	bool clear = true;
	bool ground = true;
	bool groundHeight(const nav::NavVector &, float *z) override
	{
		*z = 0;
		return ground;
	}
	bool clearLine(const nav::NavVector &, const nav::NavVector &) override
	{
		return clear;
	}
};
nav::NavArea area(unsigned id, float floor)
{
	nav::NavArea a{};
	a.id = id;
	a.extent.lo = {0, 0, floor};
	a.extent.hi = {100, 100, floor};
	a.northEastZ = floor;
	a.southWestZ = floor;
	return a;
}
int main()
{
	nav::NavDocument doc;
	doc.setSourceIdentity({5, 100, 200});
	auto low = area(1, 0);
	low.northEastZ = 20;
	low.extent.hi.z = 20;
	doc.addArea(low);
	doc.addArea(area(2, 80));
	nav::NavSnapshotPublisher pub;
	pub.publish(&doc, 1);
	Random rng;
	Probe probe;
	compat::NavNoisePositionResolver resolver(pub.snapshot(), rng, {1, 11}, {}, &probe);
	world::AudibleEvent sound{};
	sound.id = 1;
	sound.kind = world::SoundKind::WeaponFire;
	sound.origin = {90, 90, 46};
	sound.distance = 500;
	sound.timestampSeconds = 10;
	sound.loudness = 1;
	sound.priority = world::NoisePriority::High;
	sound.confidence = {1, 0};
	world::WorldPosition pos{};
	unsigned areaId = 0;
	std::uint64_t revision = 0;
	if (!check(resolver.resolve(sound, &pos, &areaId, &revision) && areaId == 1 && pos.x == 100 && pos.y == 100 &&
				pos.z == 20 && rng.draws == 2 && rng.lower == -100 && rng.upper == 100,
			"offset scales with distance, stays in original area, projects sloped Z and excludes ceiling"))
		return 1;
	sound.origin = {150, 50, 36};
	if (!check(resolver.resolve(sound, &pos, &areaId, &revision) && areaId == 1 && pos.x == 100,
			"offmesh source uses visible nearest ground area"))
		return 1;
	probe.clear = false;
	int before = rng.draws;
	if (!check(!resolver.resolve(sound, &pos, &areaId, &revision) && rng.draws == before,
			"blocked fallback never consumes error RNG"))
		return 1;
	compat::NavNoisePositionResolver absent(nav::NavSnapshot{}, rng, {1, 11}, {});
	if (!check(!absent.resolve(sound, &pos, &areaId, &revision), "missing NAV rejects enemy sound position"))
		return 1;
	probe.clear = true;
	rng.fail = true;
	if (!check(!resolver.resolve(sound, &pos, &areaId, &revision),
			"missing random source cannot fabricate noise position"))
		return 1;
	rng.fail = false;
	perception::PerceptionAssembler assembler;
	world::WorldSnapshot belief;
	auto publish = [&](unsigned tick, const world::AudibleEvent *event)
	{
		perception::PerceptionInput in({{1, 1, tick}, {1, 11}, 0});
		in.setHearingTime(10 + tick * .1f, .125f);
		in.setNoisePositionResolver(&resolver);
		if (event && in.addSound(*event) != perception::PerceptionInputResult::Accepted)
			return false;
		return assembler.publish(in, &belief) == perception::PerceptionResult::Published;
	};
	sound.origin = {50, 50, 36};
	sound.distance = 500;
	sound.timestampSeconds = 10.1f;
	sound.timestampTick = 1;
	if (!check(publish(1, &sound) && belief.noise().navArea == 1 &&
				belief.noise().navRevision == pub.snapshot().revision(),
			"accepted noise retains area/revision"))
		return 1;
	before = rng.draws;
	sound.id = 2;
	sound.timestampSeconds = 10.2f;
	sound.timestampTick = 2;
	sound.distance = 600;
	if (!check(publish(2, &sound) && rng.draws == before, "rejected farther receipt does not redraw localization"))
		return 1;
	auto accepted = belief.noise();
	auto stale = accepted;
	stale.timestampSeconds -= 1;
	if (!check(!assembler.consumeNoise(stale) && belief.noise().active, "stale consume cannot erase newer memory"))
		return 1;
	if (!check(assembler.consumeNoise(accepted) && !assembler.snapshot().noise().active,
			"attending consumes accepted noise without inventing identity"))
		return 1;
	nav::NavDocument routeDoc;
	routeDoc.setSourceIdentity({1, 0, 0});
	auto start = area(1, 0);
	auto goal = area(2, 200);
	goal.extent.lo.x = 1000;
	goal.extent.hi.x = 1100;
	start.connections[1].push_back(2);
	routeDoc.addArea(start);
	routeDoc.addArea(goal);
	nav::NavSnapshotPublisher routePub;
	routePub.publish(&routeDoc, 1);
	nav::NavPathCostContext cost{};
	cost.areas.push_back({2, 10000, 0});
	const float travel = compat::noiseTravelDistance(routePub.snapshot(), 1, 2, cost);
	if (!check(std::fabs(travel - std::sqrt(1000.0f * 1000.0f + 200.0f * 200.0f)) < .01f &&
				compat::noiseTravelDistance(routePub.snapshot(), 2, 1, cost) < 0 &&
				compat::noiseTravelDistance(routePub.snapshot(), 1, 1, cost) == 0,
			"noise range uses physical area-center distance and directed reachability"))
		return 1;
	return 0;
}
