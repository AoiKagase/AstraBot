#include "astrabot/compat/chatter_place_history.hpp"
#include <cstdio>
#include <limits>
namespace
{
bool check(bool ok, const char *message)
{
	if (!ok)
		std::fprintf(stderr, "check failed: %s\n", message);
	return ok;
}
}
int main()
{
	using namespace astrabot::compat;
	ChatterPlaceHistory history;
	if (!check(history.allow(7, 100) && !history.allow(7, 105) && !history.allow(7, 119.999f) && history.allow(7, 120),
			"first Place is allowed; denied attempts do not delay exact twenty-second release"))
		return 1;
	if (!check(history.allow(8, 120) && !history.allow(7, 120.125f),
			"distinct places are independent while all speakers share the same place history"))
		return 1;
	history.reset();
	if (!check(history.allow(7, 120.25f), "explicit map/round reset permits a place immediately"))
		return 1;
	if (!check(!history.allow(7, 110) && !history.allow(7, 140.249f) && history.allow(7, 140.25f),
			"clock rewind preserves native negative elapsed time rather than clearing history"))
		return 1;
	history.reset();
	for (unsigned id = 1; id <= 64; ++id)
		if (!check(history.allow(id, 200), "each of the first sixty-four native history slots is usable"))
			return 1;
	if (!check(!history.allow(64, 200.125f) && history.allow(65, 200.125f) && history.allow(65, 200.25f),
			"full history still tracks slot64 while later untracked places remain repeatable"))
		return 1;
	history.reset();
	for (float invalid : {-1.0f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()})
		if (!check(!history.allow(1, invalid), "invalid time cannot allocate or reset a place interval"))
			return 1;
	if (!check(!history.allow(0, 300) && history.allow(1, 300) && !history.allow(1, 300.125f),
			"undefined ID is rejected without disturbing a valid interval"))
		return 1;
	return 0;
}
