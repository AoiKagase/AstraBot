// Read-only diagnostic harness for the uploaded AstraBot sources.
// It is not a GoldSrc physics or x86 ABI acceptance test.
#include "astrabot/runtime/nav_roam_controller.hpp"
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

using namespace astrabot;

nav::NavArea makeArea(nav::AreaId id, float x0, float y0, float x1, float y1, float z = 0.0f)
{
	nav::NavArea a = {};
	a.id = id;
	a.extent.lo = {x0, y0, z};
	a.extent.hi = {x1, y1, z};
	a.northEastZ = z;
	a.southWestZ = z;
	return a;
}

nav::NavSnapshot makeSnapshot(const std::vector<nav::NavArea> &areas)
{
	nav::NavDocument d;
	d.setSourceIdentity({5U, 100U, 200U});
	for (const auto &a : areas)
	{
		if (d.addArea(a) != nav::NavModelResult::Accepted)
		{
			throw std::runtime_error("invalid fixture area");
		}
	}
	nav::NavSnapshotPublisher p;
	if (p.publish(&d, 1U) != nav::NavSnapshotResult::Published)
	{
		throw std::runtime_error("invalid fixture snapshot");
	}
	return p.snapshot();
}

runtime::NavRoamObservation observation(std::uint8_t slot, nav::NavVector pos)
{
	runtime::NavRoamObservation o = {};
	o.actor = {slot, 1U};
	o.frame = {1U, 1U, 1U};
	o.locomotion.position = pos;
	o.locomotion.standingClearance = 72.0f;
	o.locomotion.crouchingClearance = 36.0f;
	o.locomotion.grounded = true;
	return o;
}

bool insideXY(const nav::NavArea &a, const nav::NavVector &p)
{
	return p.x >= a.extent.lo.x && p.x <= a.extent.hi.x &&
		p.y >= a.extent.lo.y && p.y <= a.extent.hi.y;
}

int main(int argc, char **argv)
{
	int reproduced = 0;
	int safetyViolations = 0;
	const bool assertCoreSafety = argc > 1 && std::string(argv[1]) == "--assert-core-safety";
	if (argc > 2 || (argc > 1 && !assertCoreSafety))
	{
		std::fprintf(stderr, "usage: audit_repro [--assert-core-safety]\n");
		return 3;
	}
	try
	{
		// R1: actor-local objects still choose the first outgoing edge.
		auto a = makeArea(1U, 0, 0, 64, 64);
		auto b = makeArea(2U, 64, 0, 128, 64);
		auto c = makeArea(3U, 0, 64, 64, 128);
		a.connections[1].push_back(2U);
		a.connections[2].push_back(3U);
		b.connections[3].push_back(1U);
		auto s = makeSnapshot({a, b, c});
		unsigned same = 0;
		std::printf("R1 initial goals:");
		for (std::uint8_t slot = 1; slot <= 4; ++slot)
		{
			runtime::NavRoamController ctl;
			auto o = observation(slot, {32, 32, 0});
			nav::LocomotionIntent intent = {};
			runtime::NavRoamDecision decision = {};
			ctl.update(s, o, &intent, &decision);
			std::printf(" bot%u=%u", slot, decision.goalArea);
			same += decision.goalArea == 2U ? 1 : 0;
		}
		std::printf("\n");
		reproduced += same == 4 ? 1 : 0;

		// R2: exact area-centre observations expose repeated A-B selection.
		runtime::NavRoamController loop;
		auto o = observation(1U, {32, 32, 0});
		nav::LocomotionIntent intent = {};
		runtime::NavRoamDecision decision = {};
		std::vector<nav::AreaId> goals;
		for (unsigned i = 0; i < 12; ++i)
		{
			o.frame.tick = i + 1;
			auto r = loop.update(s, o, &intent, &decision);
			if (r == runtime::NavRoamResult::IntentReady)
			{
				goals.push_back(intent.targetArea);
				o.locomotion.position = intent.targetArea == 2U ? nav::NavVector{96, 32, 0} : nav::NavVector{32, 32, 0};
			}
		}
		std::printf("R2 roam goals:");
		for (auto g : goals)
		{
			std::printf(" %u", g);
		}
		std::printf(" (available branch=3)\n");
		bool cycle = goals.size() >= 4;
		for (std::size_t i = 0; i < goals.size(); ++i)
		{
			cycle = cycle && goals[i] == (i % 2 == 0 ? 2U : 1U);
		}
		reproduced += cycle ? 1 : 0;

		// R3: the follower advances before entering the narrow connecting area.
		a = makeArea(1U, 0, 0, 100, 100);
		b = makeArea(2U, 100, 0, 140, 200);
		c = makeArea(3U, 140, 100, 240, 200);
		a.connections[1].push_back(2U);
		b.connections[1].push_back(3U);
		s = makeSnapshot({a, b, c});
		nav::NavQuery query(s);
		nav::NavCorridor corridor = {};
		if (query.buildCorridor(1, 3, &corridor) != nav::NavQueryResult::Found)
		{
			throw std::runtime_error("R3 path not found");
		}
		nav::NavPathFollower follower;
		follower.start(corridor);
		nav::NavVector pos = {90, 20, 0};
		nav::NavVector target = {};
		nav::AreaId ta = 0;
		follower.update(s, pos, 20, 64, &target, &ta);
		std::printf("R3 first target: area=%u pos=(%.1f,%.1f) index=%zu\n", ta, target.x, target.y, follower.currentIndex());
		follower.update(s, pos, 20, 64, &target, &ta);
		bool outside = false;
		nav::NavVector bad = {};
		for (int i = 1; i < 100; ++i)
		{
			const float t = static_cast<float>(i) / 100.0f;
			nav::NavVector p = {pos.x + t * (target.x - pos.x), pos.y + t * (target.y - pos.y), 0};
			if (!insideXY(a, p) && !insideXY(b, p) && !insideXY(c, p))
			{
				outside = true;
				bad = p;
				break;
			}
		}
		std::printf("R3 unchanged-position target: area=%u pos=(%.1f,%.1f) index=%zu entered_B=%d leaves_walkable_union=%d first_outside=(%.2f,%.2f)\n", ta, target.x, target.y, follower.currentIndex(), insideXY(b, pos), outside, bad.x, bad.y);
		const bool unsafeCorner = ta == 3U && !insideXY(b, pos) && outside;
		reproduced += unsafeCorner ? 1 : 0;
		safetyViolations += unsafeCorner ? 1 : 0;

		// R4: a continuous slope is rejected using differences of area averages.
		a = makeArea(1U, 0, 0, 100, 64);
		a.extent.hi.z = 60;
		a.northEastZ = 60;
		b = makeArea(2U, 100, 0, 200, 64, 60);
		a.connections[1].push_back(2);
		s = makeSnapshot({a, b});
		runtime::NavRoamController slope;
		o = observation(1U, {80, 32, 48});
		auto r = slope.update(s, o, &intent, &decision);
		const bool slopeFail = decision.locomotionResult == nav::LocomotionResult::StepTooHigh;
		std::printf("R4 continuous ramp: result=%d locomotion=%d StepTooHigh=%d shared_edge_height_delta=0\n", static_cast<int>(r), static_cast<int>(decision.locomotionResult), slopeFail);
		reproduced += slopeFail ? 1 : 0;
		safetyViolations += slopeFail ? 1 : 0;

		// R5: NAV_JUMP handling is ordered after the ordinary step-height rejection.
		a = makeArea(1U, 0, 0, 64, 64);
		b = makeArea(2U, 64, 0, 128, 64, 32);
		b.attributes = nav::NavArea::kJump;
		a.connections[1].push_back(2);
		s = makeSnapshot({a, b});
		runtime::NavRoamController jump;
		o = observation(1U, {32, 32, 0});
		r = jump.update(s, o, &intent, &decision);
		const bool jumpFail = decision.locomotionResult == nav::LocomotionResult::StepTooHigh;
		std::printf("R5 NAV_JUMP rise=32: result=%d locomotion=%d StepTooHigh=%d traversal=%d\n", static_cast<int>(r), static_cast<int>(decision.locomotionResult), jumpFail, static_cast<int>(intent.traversal));
		reproduced += jumpFail ? 1 : 0;
		safetyViolations += jumpFail ? 1 : 0;

		// R6: a later special link isn't handed to the traversal controller.
		a = makeArea(1U, 0, 0, 64, 64);
		b = makeArea(2U, 64, 0, 128, 64);
		c = makeArea(3U, 128, 0, 192, 64);
		a.connections[1].push_back(2);
		b.connections[1].push_back(3);
		b.approaches.push_back({2U, 0U, 3U, 0U, 6U});
		s = makeSnapshot({a, b, c});
		runtime::NavRoamController multi;
		o = observation(1U, {32, 32, 0});
		o.hasObjectiveTarget = true;
		o.objectiveTarget = {160, 32, 0};
		multi.update(s, o, &intent, &decision);
		o.frame.tick = 2;
		o.locomotion.position = {96, 32, 0};
		r = multi.update(s, o, &intent, &decision);
		const auto laterAction = intent.traversal;
		const auto laterArea = intent.targetArea;
		runtime::NavRoamController direct;
		o.frame.tick = 1;
		direct.update(s, o, &intent, &decision);
		const bool skipped = laterArea == 3 && laterAction != nav::TraversalAction::Jump && intent.traversal == nav::TraversalAction::Jump;
		std::printf("R6 same B->C link: via_A action=%d target=%u; starting_at_B action=%d skipped_jump=%d\n", static_cast<int>(laterAction), laterArea, static_cast<int>(intent.traversal), skipped);
		reproduced += skipped ? 1 : 0;
		safetyViolations += skipped ? 1 : 0;

		// R7: closest area is selected entirely in XY, even on another floor.
		a = makeArea(1U, 0, 0, 64, 64, 200);
		b = makeArea(2U, 0, 0, 64, 64, 0);
		s = makeSnapshot({a, b});
		nav::NavAreaMatch match = {};
		nav::NavQuery nearest(s);
		nearest.findNearest({32, 32, 0}, 16.0f, &match);
		const bool wrongFloor = match.area == 1 && match.closestPoint.z == 200;
		std::printf("R7 nearest at z=0 maxDistance=16: area=%u selected_z=%.1f distanceSquared=%.1f wrong_floor=%d\n", match.area, match.closestPoint.z, match.distanceSquared, wrongFloor);
		reproduced += wrongFloor ? 1 : 0;

		// R8: arbitrarily tiny lateral creep resets stuck detection indefinitely.
		a = makeArea(1U, 0, 0, 64, 64);
		b = makeArea(2U, 64, 0, 128, 64);
		a.connections[1].push_back(2);
		s = makeSnapshot({a, b});
		runtime::NavRoamController creep;
		o = observation(1U, {32, 20, 0});
		unsigned stuck = 0;
		for (unsigned i = 0; i < 500; ++i)
		{
			o.frame.tick = i + 1;
			o.locomotion.position.y = 20.0f + static_cast<float>(i) * 0.02f;
			creep.update(s, o, &intent, &decision);
			stuck += decision.locomotionResult == nav::LocomotionResult::Stuck ? 1 : 0;
		}
		std::printf("R8 500 updates, x progress=0, y creep=0.02/update: stuck_events=%u still_active=%d\n", stuck, creep.isActive());
		reproduced += stuck == 0 && creep.isActive() ? 1 : 0;

		std::printf("SUMMARY: reproduced %d / 8 diagnostic observations\n", reproduced);
		std::printf("CORE SAFETY: %d violations among R3-R6\n", safetyViolations);
		// R1/R2/R7/R8 are observations, not a demand for forced route diversity.
		// Passing this small check is not complete CSBot parity acceptance.
		return assertCoreSafety && safetyViolations != 0 ? 1 : 0;
	}
	catch (const std::exception &e)
	{
		std::fprintf(stderr, "fixture error: %s\n", e.what());
		return 3;
	}
}
