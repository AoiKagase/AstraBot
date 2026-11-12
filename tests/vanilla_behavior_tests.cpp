#include "astrabot/compat/combat_behavior.hpp"
#include <cmath>
#include <cstdio>

namespace
{
bool check(bool v, const char *message)
{
	if (!v)
		std::fprintf(stderr, "%s\n", message);
	return v;
}
} // namespace
int main()
{
	using namespace astrabot;
	compat::ReactionQueue queue;
	compat::ThreatCandidate candidate = {{2U, 1U}, {100.0f, 0.0f, 32.0f}, 100.0f, false};
	compat::ThreatCandidate selected = {};
	const world::ActorKey observer = {1U, 1U};
	bool ok =
		check(!queue.update(observer, {1U, 1U, 1U}, 0.0f, 0.2f, &candidate, 1U, false, &selected),
			  "new contact waits for reaction");
	ok &= check(!queue.update(observer, {1U, 1U, 2U}, 0.1f, 0.2f, &candidate, 1U, false, &selected),
				"reaction has not elapsed");
	ok &=
		check(queue.update(observer, {1U, 1U, 3U}, 0.2f, 0.2f, &candidate, 1U, false, &selected) &&
				  selected.actor.slot == 2U,
			  "delayed contact becomes recognized");
	candidate.visiblePoint.x = 200.0f;
	ok &=
		check(queue.update(observer, {1U, 1U, 4U}, 0.3f, 0.2f, &candidate, 1U, false, &selected) &&
				  selected.visiblePoint.x == 100.0f,
			  "reaction uses historical position");
	ok &= check(!queue.update(observer, {1U, 1U, 5U}, 0.4f, 0.2f, &candidate, 1U, true, &selected),
				"blind observer cannot acquire contact");
	ok &= check(!queue.update(observer, {1U, 2U, 1U}, 0.5f, 0.2f, &candidate, 1U, false, &selected),
				"round generation clears delayed knowledge");
	compat::ThreatCandidate threats[2] = {candidate,
										  {{3U, 1U}, {1000.0f, 0.0f, 0.0f}, 1000.0f, true}};
	ok &= check(queue.update(observer, {1U, 2U, 2U}, 0.6f, 0.0f, threats, 2U, false, &selected) &&
					selected.actor.slot == 3U,
				"attacker threat precedes nearer passive enemy");
	ok &= check(!queue.update(observer, {1U, 2U, 3U}, 0.7f, 0.0f, nullptr, 0U, false, &selected),
				"empty visible set clears threat");
	compat::AimStep aim = compat::trackAim(179.0f, 0.0f, -179.0f, 0.0f, 0.01f, 0.5f);
	ok &= check(aim.yaw > 179.0f && aim.yaw < 180.0f,
				"aim takes shortest angular turn with finite acceleration");
	aim = compat::trackAim(0.0f, 0.0f, 90.0f, 0.0f, 0.1f, 1.0f);
	ok &= check(aim.yaw > 0.0f && aim.yaw < 90.0f && !aim.aligned,
				"aim inertia prevents instantaneous target snap");
	ok &= check(!compat::weaponEngagementAllowed(combat::WeaponKind::Knife, 200.0f),
				"knife cannot fire beyond reach");
	ok &= check(!compat::weaponEngagementAllowed(combat::WeaponKind::Grenade, 100.0f),
				"grenades require dedicated throw solution");
	ok &= check(compat::weaponEngagementAllowed(combat::WeaponKind::Rifle, 200.0f),
				"known firearm uses normal engagement");
	return ok ? 0 : 1;
}
