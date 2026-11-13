#include "astrabot/compat/public_bomb_clock.hpp"
#include "astrabot/compat/public_bomb_radar.hpp"
#include <cstdio>
#include <limits>

int main()
{
	astrabot::compat::PublicBombClock clock;
	float remaining = 99.0f;
	int failures = 0;
	const auto check = [&failures](bool value, const char *message)
	{
		if (!value)
		{
			++failures;
			std::fprintf(stderr, "FAIL: %s\n", message);
		}
	};
	check(!clock.estimate({1, 1, 1}, 10, 35, &remaining) && remaining == 99, "no notice means unknown timer");
	check(clock.observe({1, 1, 1}, 10, true), "public planted notice accepted");
	check(clock.estimate({1, 1, 2}, 20, 35, &remaining) && remaining == 25, "public fuse estimate uses elapsed time");
	check(clock.observe({1, 1, 3}, 21, true) && clock.estimate({1, 1, 3}, 21, 35, &remaining) && remaining == 24,
		"duplicate notice does not reset plant clock");
	check(!clock.observe({1, 1, 2}, 22, true) && !clock.estimate({1, 1, 3}, 20, 35, &remaining),
		"time and frame reversals rejected");
	check(clock.estimate({1, 1, 4}, 50, 35, &remaining) && remaining == 0, "elapsed fuse clamps at zero");
	check(!clock.estimate({1, 2, 4}, 50, 35, &remaining), "round rollover invalidates old estimate");
	check(clock.observe({1, 1, 5}, 51, false) && !clock.estimate({1, 1, 6}, 52, 35, &remaining),
		"defused notice clears clock");
	check(clock.observe({2, 1, 1}, 1, true) && clock.estimate({2, 1, 2}, 2, 35, &remaining) && remaining == 34,
		"map generation allows restarted timestamp");
	const float nan = std::numeric_limits<float>::quiet_NaN();
	check(!clock.estimate({2, 1, 2}, 2, nan, &remaining) && !clock.estimate({2, 1, 2}, 2, -1, &remaining) &&
			!clock.observe({2, 1, 2}, nan, true),
		"invalid inputs never manufacture a timer");
	clock.reset();
	check(!clock.estimate({2, 1, 2}, 2, 35, &remaining), "slot reset discards previous notice");
	astrabot::compat::PublicBombRadar radar;
	astrabot::world::WorldVector position{};
	check(!radar.read({1, 1, 1}, &position), "missing radar update remains unknown");
	radar.drop({1, 1, 2}, {120, 240, 8});
	check(radar.read({1, 1, 3}, &position) && position.x == 120, "T radar retains the received drop coordinates");
	check(!radar.read({1, 2, 3}, &position) && !radar.read({2, 1, 3}, &position),
		"radar never crosses round or map generations");
	check(!radar.read({1, 1, 1}, &position), "future radar coordinates are unavailable");
	radar.reset();
	check(!radar.read({1, 1, 4}, &position), "pickup and slot reset discard dropped bomb knowledge");
	return failures == 0 ? 0 : 1;
}
