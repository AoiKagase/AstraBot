#include "astrabot/nav/danger_memory.hpp"

#include <cmath>
#include <cstdio>
#include <limits>

namespace
{
astrabot::nav::NavArea makeArea(astrabot::nav::AreaId id, float x, float z = 0.0f)
{
	astrabot::nav::NavArea area{};
	area.id = id;
	area.extent = {{x - 1.0f, -1.0f, z}, {x + 1.0f, 1.0f, z}};
	area.northEastZ = area.southWestZ = z;
	return area;
}
}

int main()
{
	using namespace astrabot::nav;
	int failures = 0;
	const auto check = [&failures](bool condition, const char *message)
	{
		if (!condition) { ++failures; std::fprintf(stderr, "FAIL: %s\n", message); }
	};
	NavDocument document;
	NavArea start = makeArea(1U, 0.0f);
	NavArea half = makeArea(2U, 250.0f);
	NavArea edge = makeArea(3U, 500.0f);
	NavArea vertical = makeArea(4U, 300.0f, 400.0f);
	NavArea outside = makeArea(5U, 501.0f);
	NavArea disconnected = makeArea(6U, 10.0f);
	NavArea behindOutside = makeArea(7U, 20.0f);
	start.connections[0] = {2U, 4U, 5U};
	// NavDocument rejects duplicate IDs within one directional list. Repeat
	// the edge across directions instead to exercise BFS visit suppression
	// while keeping the fixture valid.
	half.connections[0] = {1U, 3U};
	half.connections[1] = {3U};
	outside.connections[0] = {7U};
	for (const NavArea &area : {start, half, edge, vertical, outside, disconnected, behindOutside})
		check(document.addArea(area) == NavModelResult::Accepted, "test area accepted");
	DangerMemory memory;
	float value = 42.0f;
	check(!memory.query(0.0, 1U, 1U, &value) && value == 42.0f, "unbound memory rejects query without changing output");
	check(memory.recordDeath(document, 10U, 1U, 1U, {0, 0, 0}, 0.0), "self death recorded");
	const auto expect = [&](double now, std::uint8_t team, AreaId area, float expected, const char *message)
	{
		float actual = -1.0f;
		check(memory.query(now, team, area, &actual) && std::fabs(actual - expected) < 0.0001f, message);
	};
	expect(0, 1, 1, 1.0f, "start area receives full danger once despite cycle");
	expect(0, 1, 2, 0.5f, "adjacent danger uses distance divided by radius");
	expect(0, 1, 3, 1.0f, "radius boundary included and duplicate links counted once");
	expect(0, 1, 4, 1.0f, "radius uses 3D distance");
	expect(0, 1, 5, 0.0f, "outside radius excluded");
	expect(0, 1, 6, 0.0f, "disconnected close area excluded");
	expect(0, 1, 7, 0.0f, "search does not cross outside-radius area");
	expect(0, 2, 1, 0.0f, "other team does not inherit death danger");
	expect(60, 1, 1, 0.5f, "danger decays linearly by elapsed seconds divided by 120");
	expect(60, 1, 2, 0.0f, "fractional danger decays to zero sooner");
	expect(120, 1, 1, 0.0f, "danger reaches zero after 120 seconds");
	check(memory.recordDeath(document, 10U, 1U, 1U, {0, 0, 0}, 120.0), "second death recorded after decay");
	check(memory.recordDeath(document, 10U, 1U, 1U, {0, 0, 0}, 180.0), "another death adds to decayed value");
	expect(180, 1, 1, 1.5f, "danger accumulation preserves decay before new observation");
	check(memory.recordDeath(document, 10U, 2U, 1U, {0, 0, 0}, 180.0), "second team death recorded independently");
	expect(180, 2, 1, 1.0f, "second team receives only its own observation");
	expect(180, 1, 1, 1.5f, "second team event does not change first team");
	value = 42;
	check(!memory.query(179, 1, 1, &value) && value == 42, "query time reversal rejected");
	check(!memory.query(-1, 1, 1, &value) && !memory.query(180, 0, 1, &value) &&
		!memory.query(180, 1, 0, &value) && !memory.query(180, 1, 1, nullptr), "invalid query arguments rejected");
	const float nan = std::numeric_limits<float>::quiet_NaN();
	check(!memory.recordDeath(document, 10, 1, 1, {0, 0, 0}, 179), "death time reversal rejected");
	check(!memory.recordDeath(document, 11, 1, 1, {nan, 0, 0}, 0) &&
		!memory.recordDeath(document, 11, 1, 1, {0, 0, 0}, -1) &&
		!memory.recordDeath(document, 11, 1, 1, {0, 0, 0}, 0, -1) &&
		!memory.recordDeath(document, 11, 1, 1, {0, 0, 0}, 0, nan) &&
		!memory.recordDeath(document, 11, 3, 1, {0, 0, 0}, 0) &&
		!memory.recordDeath(document, 11, 1, 999, {0, 0, 0}, 0), "invalid death input rejected transactionally");
	check(memory.mapGeneration() == 10, "invalid new-map event does not clear previous map");
	expect(180, 1, 1, 1.5f, "invalid observations preserve existing memory");
	check(memory.recordDeath(document, 11, 2, 1, {0, 0, 0}, 0), "new map resets memory and accepts restarted clock");
	expect(0, 1, 1, 0.0f, "new map clears previous team knowledge");
	expect(0, 2, 1, 1.0f, "new map retains only new observation");
	memory.reset(12U);
	expect(0, 2, 1, 0.0f, "explicit map reset clears knowledge");
	check(!memory.query(std::numeric_limits<double>::infinity(), 1, 1, &value), "nonfinite query time rejected");
	check(!memory.recordDeath(document, 12, 1, 1, {0, 0, 0}, std::numeric_limits<double>::infinity()), "nonfinite death time rejected");
	return failures == 0 ? 0 : 1;
}
