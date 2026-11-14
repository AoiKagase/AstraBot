#include "astrabot/compat/gunshot_events.hpp"
#include <cstdio>
#include <limits>
bool check(bool ok, const char *message)
{
	if (!ok)
		std::fprintf(stderr, "FAIL: %s\n", message);
	return ok;
}
int main()
{
	astrabot::compat::GunshotEventCatalog catalog;
	if (!check(catalog.range(17, 0, 0) == 0, "unmapped event is silent"))
		return 1;
	catalog.observe(17, "events/ak47.sc");
	catalog.observe(65000, "events/awp.sc");
	catalog.observe(3, "events/m4a1.sc");
	catalog.observe(4, "events/usp.sc");
	catalog.observe(5, "events/tmp.sc");
	if (!check(catalog.range(17, 0, 0) == 2000 && catalog.range(65000, 0, 0) == 99999 && catalog.range(5, 0, 0) == 1000,
			"engine indices and native ranges"))
		return 1;
	if (!check(catalog.range(3, 1, 0) == 1000 && catalog.range(3, 0, 1) == 2000 && catalog.range(4, 0, 1) == 1000 &&
				catalog.range(4, 1, 0) == 2000,
			"weapon-specific public silencer booleans"))
		return 1;
	catalog.observe(17, "events/createexplo.sc");
	catalog.observe(18, "events/knife.sc");
	catalog.observe(19, "events/custom.sc");
	if (!check(catalog.range(17, 0, 0) == 0 && catalog.range(18, 0, 0) == 0 && catalog.range(19, 0, 0) == 0,
			"non-guns and index reassignment fail closed"))
		return 1;
	const char *guns[] = {"ak47", "aug", "awp", "deagle", "elite_left", "elite_right", "famas", "fiveseven", "g3sg1",
		"galil", "glock18", "m249", "m3", "m4a1", "mac10", "mp5n", "p228", "p90", "scout", "sg550", "sg552", "tmp",
		"ump45", "usp", "xm1014"};
	catalog.reset();
	unsigned short index = 1;
	for (auto name : guns)
	{
		char path[64];
		std::snprintf(path, sizeof(path), "events/%s.sc", name);
		catalog.observe(index, path);
		if (!check(catalog.range(index, 0, 0) > 0, "all standard gun scripts registered"))
			return 1;
		++index;
	}
	catalog.reset();
	return check(catalog.range(65000, 0, 0) == 0 && catalog.range(3, 1, 0) == 0, "map reset invalidates index mapping")
		? 0
		: 1;
}
