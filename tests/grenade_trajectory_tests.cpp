#include "astrabot/compat/grenade_trajectory.hpp"
#include <cmath>
#include <cstdio>
#include <limits>

int main()
{
	using namespace astrabot::compat;
	const auto launch = solveGrenadeLaunch({0.0f, 0.0f, 64.0f}, {}, {500.0f, 0.0f, 0.0f}, 800.0f, 16.0f);
	if (!launch.valid || launch.flightSeconds > 1.5f || launch.gravity != 400.0f)
	{
		std::fprintf(stderr, "public grenade formula produces a timed gravity-scaled launch\n");
		return 1;
	}
	const auto impact = grenadePosition(launch, launch.flightSeconds);
	if (std::hypot(impact.x - 500.0f, impact.y) > 16.0f || std::fabs(impact.z) > 16.0f)
	{
		std::fprintf(stderr, "solved trajectory reaches requested public target within caller tolerance\n");
		return 1;
	}
	if (solveGrenadeLaunch({0.0f, 0.0f, 64.0f}, {}, {10000.0f, 0.0f, 0.0f}, 800.0f, 16.0f).valid ||
		solveGrenadeLaunch({0.0f, 0.0f, 64.0f}, {}, {500.0f, 0.0f, 0.0f}, 0.0f, 16.0f).valid ||
		solveGrenadeLaunch(
			{0.0f, 0.0f, 64.0f}, {}, {500.0f, 0.0f, 0.0f}, std::numeric_limits<float>::quiet_NaN(), 16.0f)
			.valid)
	{
		std::fprintf(stderr, "unreachable targets and unavailable gravity never yield a throw\n");
		return 1;
	}
	return 0;
}
