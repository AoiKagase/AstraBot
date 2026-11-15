#include "astrabot/compat/nav_area_lookup.hpp"
#include "astrabot/compat/chatter_enemy_place.hpp"
#include <cstdio>
#include <vector>
#include <limits>
using namespace astrabot;
namespace
{
bool check(bool ok, const char *text)
{
	if (!ok)
		std::fprintf(stderr, "check failed: %s\n", text);
	return ok;
}
nav::NavArea area(unsigned id, float floor, float loX = 0, float hiX = 100)
{
	nav::NavArea a{};
	a.id = id;
	a.extent.lo = {loX, 0, floor};
	a.extent.hi = {hiX, 100, floor};
	a.northEastZ = a.southWestZ = floor;
	return a;
}
struct Probe final : compat::INoiseNavProbe
{
	float floor = 0;
	bool available = true, blockLow = false;
	unsigned grounds = 0, lines = 0;
	bool groundHeight(const nav::NavVector &, float *z) override
	{
		++grounds;
		*z = floor;
		return available;
	}
	bool clearLine(const nav::NavVector &, const nav::NavVector &to) override
	{
		++lines;
		return !blockLow || to.z > 36;
	}
};
}
int main()
{
	nav::NavDocument doc;
	doc.setSourceIdentity({5, 100, 200});
	doc.addArea(area(1, 0));
	doc.addArea(area(2, 80));
	nav::NavSnapshotPublisher pub;
	pub.publish(&doc, 1);
	Probe probe;
	if (!check(compat::findBotNavArea(pub.snapshot(), {50, 50, 40}, &probe) == 1 && !probe.grounds && !probe.lines,
			"native overlapping floor excludes ceiling and takes quick path without trace"))
		return 1;
	if (!check(compat::findBotNavArea(pub.snapshot(), {50, 50, 77}, &probe) == 2 && !probe.grounds,
			"overlapping floor may be up to five units above origin"))
		return 1;
	if (!check(compat::findBotNavArea(pub.snapshot(), {50, 50, 200}, &probe) == 2 && !probe.grounds,
			"exact120-unit beneath limit remains a quick floor match"))
		return 1;
	if (!check(compat::botNavAreaContains(pub.snapshot(), 1, {50, 50, 77}) &&
				!compat::botNavAreaContains(pub.snapshot(), 1, {50, 50, 80}) &&
				compat::botNavAreaContains(pub.snapshot(), 2, {50, 50, 500}),
			"cached current area keeps an overhead floor until crossed and is not limited to120 below"))
		return 1;
	nav::NavDocument adjacent;
	adjacent.setSourceIdentity({5, 100, 200});
	adjacent.addArea(area(1, 0, 0, 100));
	adjacent.addArea(area(2, 40, 100, 200));
	nav::NavSnapshotPublisher adjacentPub;
	adjacentPub.publish(&adjacent, 1);
	if (!check(compat::botNavAreaContains(adjacentPub.snapshot(), 1, {100, 50, 50}) &&
				!compat::botNavAreaContains(adjacentPub.snapshot(), 1, {101, 50, 50}),
			"touching neighboring area is not a positive-overlap ceiling at shared boundary"))
		return 1;
	compat::ChatterKnownArea tracked;
	world::ActorKey bot{2, 3};
	world::FrameIdentity botFrame{1, 2, 3};
	tracked.observe(bot, botFrame, pub.snapshot().revision(), 1);
	tracked.update(pub.snapshot(), bot, botFrame, {50, 50, 77});
	if (!check(tracked.current(bot, botFrame, pub.snapshot().revision()) == 1,
			"managed update retains containing current area instead of selecting overhead floor"))
		return 1;
	++botFrame.tick;
	tracked.update(pub.snapshot(), bot, botFrame, {50, 50, 80});
	if (!check(tracked.read(bot, botFrame, pub.snapshot().revision()) == 2 &&
				tracked.current(bot, botFrame, pub.snapshot().revision()) == 2,
			"crossing higher floor updates current and last known area"))
		return 1;
	++botFrame.tick;
	tracked.update(pub.snapshot(), bot, botFrame, {150, 50, 36});
	if (!check(tracked.read(bot, botFrame, pub.snapshot().revision()) == 2 &&
				!tracked.current(bot, botFrame, pub.snapshot().revision()),
			"leaving mesh clears current but does not replace last known area with nearest recovery target"))
		return 1;
	probe.blockLow = true;
	if (!check(compat::findBotNavArea(pub.snapshot(), {150, 50, 36}, &probe) == 2 && probe.grounds == 1 &&
				probe.lines == 2,
			"off-mesh nearest uses ground correction plus3D distance and skips blocked closest area"))
		return 1;
	probe.grounds = probe.lines = 0;
	if (!check(compat::findBotNavArea(pub.snapshot(), {150, 50, 36}, &probe, true) == 1 && probe.grounds == 1 &&
				probe.lines == 0,
			"any-Z mode still adjusts ground but does not require line of sight"))
		return 1;
	nav::NavDocument ties;
	ties.setSourceIdentity({5, 100, 200});
	ties.addArea(area(9, 0, -20, 0));
	ties.addArea(area(2, 0, 20, 40));
	nav::NavSnapshotPublisher tiePub;
	tiePub.publish(&ties, 1);
	probe.blockLow = false;
	if (!check(compat::findBotNavArea(tiePub.snapshot(), {10, 50, 36}, &probe) == 9,
			"equal3D distances retain first loaded area rather than lowest ID"))
		return 1;
	nav::NavDocument limited;
	limited.setSourceIdentity({5, 100, 200});
	limited.addArea(area(1, 0, 0, 10));
	nav::NavSnapshotPublisher limitPub;
	limitPub.publish(&limited, 1);
	probe.floor = -36;
	if (!check(!compat::findBotNavArea(limitPub.snapshot(), {10010, 50, 36}, &probe) &&
				compat::findBotNavArea(limitPub.snapshot(), {10009, 50, 36}, &probe) == 1,
			"native nearest cutoff is strict squared100000000"))
		return 1;
	probe.available = false;
	if (!check(!compat::findBotNavArea(pub.snapshot(), {150, 50, 36}, &probe) &&
				!compat::findBotNavArea({}, {0, 0, 0}, &probe),
			"missing ground or NAV cannot fabricate a place"))
		return 1;
	float height = -999;
	unsigned calls = 0;
	std::vector<float> offsets;
	std::vector<std::uintptr_t> ignored;
	const auto trace = [&](float offset, std::uintptr_t skip, compat::NavGroundTrace *out)
	{
		++calls;
		offsets.push_back(offset);
		ignored.push_back(skip);
		*out = {0, false, false, 0};
		if (calls == 1)
			*out = {90, false, true, 17};
		else if (calls == 2)
			*out = {0, true, false, 0};
		else if (calls == 3)
			*out = {0, false, false, 0};
		else if (calls == 4)
			*out = {10, false, false, 0};
		else if (calls == 5)
			*out = {20, false, false, 0};
		else
			*out = {60, false, false, 0};
		return true;
	};
	if (!check(compat::findBotGroundHeight(trace, &height) && height == 20 && calls == 10 && offsets.front() == 1 &&
				offsets.back() == 91 && ignored[1] == 17 && ignored.back() == 17,
			"ground layers skip walkable brush, ignore start-solid and select first36-unit headroom gap"))
		return 1;
	const auto solid = [](float, std::uintptr_t, compat::NavGroundTrace *out)
	{
		*out = {0, true, false, 0};
		return true;
	};
	if (!check(!compat::findBotGroundHeight(solid, &height), "all start-solid layers provide no ground"))
		return 1;
	const auto stale = [](float, std::uintptr_t, compat::NavGroundTrace *)
	{
		return false;
	};
	if (!check(!compat::findBotGroundHeight(stale, &height), "invalidated public trace receipt aborts ground query"))
		return 1;
	return 0;
}
