#include "astrabot/compat/knife_run.hpp"
#include <cstdio>
#include <limits>
int main()
{
	using namespace astrabot;
	compat::KnifeRunTimer timer;
	const world::ActorKey actor{1, 2};
	world::FrameIdentity frame{3, 4, 5};
	int failed = 0;
	const auto check = [&](bool ok, const char *name)
	{
		if (!ok)
		{
			std::printf("FAIL: %s\n", name);
			++failed;
		}
	};
	check(!timer.active(actor, frame, 7, 10, true), "inactive before receipt");
	timer.start(actor, frame, 7, 10);
	check(timer.active(actor, frame, 7, 15, true), "native deadline inclusive");
	check(!timer.active(actor, frame, 7, 15.001f, true), "expires strictly after five seconds");
	timer.start(actor, frame, 7, 20);
	timer.start(actor, frame, 7, 24);
	check(timer.active(actor, frame, 7, 29, true), "later event restarts timer");
	check(!timer.active(actor, frame, 7, 29.01f, true), "refreshed timer expires");
	timer.start(actor, frame, 7, 30);
	check(!timer.active(actor, frame, 7, 29, true) && !timer.active(actor, frame, 7, 31, true),
		"rollback cancels permanently");
	timer.start(actor, frame, 7, 40);
	check(!timer.active(actor, frame, 8, 41, true), "observer serial mismatch");
	timer.start(actor, frame, 7, 40);
	check(!timer.active({1, 3}, frame, 7, 41, true), "observer generation mismatch");
	timer.start(actor, frame, 7, 40);
	check(!timer.active(actor, {4, 4, 6}, 7, 41, true), "map mismatch");
	timer.start(actor, frame, 7, 40);
	check(!timer.active(actor, {3, 5, 6}, 7, 41, true), "round mismatch");
	timer.start(actor, frame, 7, 40);
	check(!timer.active(actor, frame, 7, 41, false), "death cancels");
	check(!timer.active(actor, frame, 7, 41, true), "revival does not restore timer");
	timer.start(actor, frame, 7, std::numeric_limits<float>::quiet_NaN());
	check(!timer.active(actor, frame, 7, 41, true), "nonfinite receipt rejected");
	timer.start(actor, frame, 7, 40);
	check(!timer.active(actor, {3, 4, 4}, 7, 41, true), "frame regression cancels");
	check(compat::isStandardDryFireSample("weapons/dryfire_pistol.wav") &&
			compat::isStandardDryFireSample("weapons/dryfire_rifle.wav"),
		"standard dry fire");
	check(!compat::isStandardDryFireSample("weapons/ak47_clipin.wav") && !compat::isStandardDryFireSample(nullptr),
		"unknown and reload rejected");
	return failed == 0 ? 0 : 1;
}
