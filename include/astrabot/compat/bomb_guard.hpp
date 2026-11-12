#ifndef ASTRABOT_COMPAT_BOMB_GUARD_HPP
#define ASTRABOT_COMPAT_BOMB_GUARD_HPP

#include "astrabot/compat/random_source.hpp"
#include "astrabot/nav/nav_query.hpp"
#include "astrabot/world/world_snapshot.hpp"
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>

namespace astrabot { namespace compat {
struct BombGuardWorkStats
{
	std::uint64_t selectionCalls = 0U, areaLookups = 0U, candidateAreas = 0U;
	std::uint64_t pathSearches = 0U, nearestQueries = 0U, indexEntries = 0U;
};
class BombGuardPlanner
{
public:
	void reset() { *this = BombGuardPlanner{}; }
	const BombGuardWorkStats &workStats() const { return workStats_; }
	bool needsSelection(const nav::NavSnapshot &snapshot, const world::FrameIdentity &frame,
		const world::EntityKey &bomb, const nav::NavVector &bombPosition,
		const nav::NavVector &self, const std::vector<nav::NavVector> &occupied, float now) const
	{
		if (!frame.isValid() || !bomb.isValid() || !snapshot.isValid() ||
			snapshot.mapGeneration() != frame.mapGeneration || !std::isfinite(now) ||
			!finite(self) || !finite(bombPosition)) return true;
		return !(active_ && map_ == frame.mapGeneration && round_ == frame.roundGeneration &&
			revision_ == snapshot.revision() && indexedSnapshot_.document() == snapshot.document() &&
			bomb_ == bomb && now >= selectedAt_ && now < expiresAt_ && !isTaken(spot_, occupied));
	}
	bool update(const nav::NavSnapshot &snapshot, const world::FrameIdentity &frame,
		const world::EntityKey &bomb, const nav::NavVector &bombPosition,
		const nav::NavVector &self, bool sniper, const std::vector<nav::NavVector> &occupied,
		float now, ICompatibilityRandomSource &random, const RandomActor &actor,
		const RandomTimingContext &timing, nav::NavVector *target,
		const nav::NavPathCostContext &cost = {})
	{
		if (target == nullptr || !frame.isValid() || !bomb.isValid() || !snapshot.isValid() ||
			snapshot.mapGeneration() != frame.mapGeneration || !std::isfinite(now) ||
			!finite(self) || !finite(bombPosition)) return false;
		const auto taken = [&](const nav::NavVector &point) {
			return isTaken(point, occupied);
		};
		if (!needsSelection(snapshot, frame, bomb, bombPosition, self, occupied, now))
		{
			*target = spot_; return true;
		}
		active_ = false;
		++workStats_.selectionCalls;
		if (indexedSnapshot_.document() != snapshot.document() ||
			indexedSnapshot_.revision() != snapshot.revision() ||
			indexedSnapshot_.mapGeneration() != snapshot.mapGeneration())
		{
			areaIndex_.clear();
			indexedSnapshot_ = snapshot; // Own the immutable document behind indexed pointers.
			areaIndex_.reserve(snapshot.document()->areaCount());
			for (const auto &area : snapshot.document()->areas())
			{
				areaIndex_.emplace(area.id, &area);
				++workStats_.indexEntries;
			}
			failedGoals_.clear();
		}
		nav::NavQuery query(snapshot);
		nav::NavAreaMatch anchor{}, start{};
		++workStats_.nearestQueries;
		if (query.findNearest3D(bombPosition, 2048.0f, &anchor) != nav::NavQueryResult::Found) return false;
		++workStats_.nearestQueries;
		if (query.findNearest3D(self, 2048.0f, &start) != nav::NavQueryResult::Found) return false;
		if (failedStart_ != start.area || failedAnchor_ != anchor.area || !(failedBomb_ == bomb) ||
			failedRound_ != frame.roundGeneration || !sameCost(failedCost_, cost))
		{
			failedGoals_.clear(); failedCost_ = cost;
			failedStart_ = start.area; failedAnchor_ = anchor.area;
			failedBomb_ = bomb; failedRound_ = frame.roundGeneration;
		}
		const auto *anchorArea = findArea(anchor.area);
		if (anchorArea == nullptr) return false;
		const nav::NavVector center = {(anchorArea->extent.lo.x + anchorArea->extent.hi.x) * 0.5f,
			(anchorArea->extent.lo.y + anchorArea->extent.hi.y) * 0.5f,
			(anchorArea->extent.lo.z + anchorArea->extent.hi.z) * 0.5f};
		struct Candidate { nav::NavVector position; nav::AreaId area; std::uint8_t flags; };
		std::vector<Candidate> candidates;
		std::vector<nav::AreaId> pending{anchor.area};
		std::unordered_set<nav::AreaId> visited{anchor.area};
		for (std::size_t cursor = 0U; cursor < pending.size() && candidates.size() < 256U; ++cursor)
		{
			++workStats_.candidateAreas;
			const auto *area = findArea(pending[cursor]);
			if (area == nullptr) continue;
			for (const auto &spot : area->hidingSpots)
			{
				if (candidates.size() >= 256U) break;
				if (distance(center, spot.position) <= 750.0f && !taken(spot.position))
					candidates.push_back({spot.position, area->id, spot.flags});
			}
			for (const auto &links : area->connections) for (auto next : links)
			{
				if (visited.count(next) != 0U) continue;
				const auto *neighbor = findArea(next);
				if (neighbor == nullptr) continue;
				const nav::NavVector closest = {std::max(neighbor->extent.lo.x, std::min(center.x, neighbor->extent.hi.x)),
					std::max(neighbor->extent.lo.y, std::min(center.y, neighbor->extent.hi.y)), center.z};
				if (distance(center, closest) <= 750.0f) { visited.insert(next); pending.push_back(next); }
			}
		}
		// Ideal sniper, good sniper, then ordinary cover; do not invent cover
		// at the bomb when the NAV has no usable hiding spot.
		nav::NavVector chosen = self;
		for (unsigned flag : {sniper ? 4U : 1U, sniper ? 2U : 1U, 1U})
		{
			std::vector<Candidate> eligible;
			for (const auto &candidate : candidates)
			{
				if ((candidate.flags & flag) == 0U) continue;
				eligible.push_back(candidate);
			}
			if (eligible.empty()) continue;
			const auto choice = random.nextLong(RandomRequest::longRequest("Hide.Spot", actor, timing,
				0, static_cast<std::int32_t>(eligible.size() - 1U)));
			if (choice.status != RandomStatus::Ok || choice.value < 0 ||
				static_cast<std::size_t>(choice.value) >= eligible.size()) return false;
			const auto &selected = eligible[static_cast<std::size_t>(choice.value)];
			// Preserve the random draw above, while reusing only an identical,
			// definitive failed search. Resource limits are never cached.
			if (failedGoals_.count(selected.area) != 0U) return false;
			nav::NavCorridor route{};
			query.setPathCostContext(cost);
			++workStats_.pathSearches;
			const auto routeResult = query.buildCorridor(start.area, selected.area, nav::NavRouteType::Fastest, &route);
			if (routeResult != nav::NavQueryResult::Found)
			{
				if (routeResult == nav::NavQueryResult::NoRoute) failedGoals_.insert(selected.area);
				return false;
			}
			chosen = selected.position;
			break;
		}
		const auto duration = random.nextFloat(RandomRequest::floatRequest("Hide.Duration", actor, timing, 30.0f, 60.0f));
		if (duration.status != RandomStatus::Ok || !std::isfinite(duration.value) ||
			duration.value < 30.0f || duration.value > 60.0f) return false;
		spot_ = chosen; bomb_ = bomb; map_ = frame.mapGeneration; round_ = frame.roundGeneration;
		revision_ = snapshot.revision();
		selectedAt_ = now; expiresAt_ = now + duration.value; active_ = true;
		*target = spot_; return true;
	}
private:
	const nav::NavArea *findArea(nav::AreaId id)
	{
		++workStats_.areaLookups;
		const auto found = areaIndex_.find(id);
		return found == areaIndex_.end() ? nullptr : found->second;
	}
	static bool sameCost(const nav::NavPathCostContext &a, const nav::NavPathCostContext &b)
	{
		if (a.aggression != b.aggression || a.health != b.health ||
			a.hostageEscortCount != b.hostageEscortCount || a.attacking != b.attacking ||
			a.zombie != b.zombie || a.areas.size() != b.areas.size()) return false;
		for (std::size_t i = 0U; i < a.areas.size(); ++i)
			if (a.areas[i].area != b.areas[i].area || a.areas[i].danger != b.areas[i].danger ||
				a.areas[i].teammates != b.areas[i].teammates) return false;
		return true;
	}
	static bool isTaken(const nav::NavVector &point, const std::vector<nav::NavVector> &occupied)
	{
		for (const auto &other : occupied) if (distance(point, other) < 75.0f) return true;
		return false;
	}
	static bool finite(const nav::NavVector &point)
	{
		return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
	}
	static float distance(const nav::NavVector &a, const nav::NavVector &b)
	{
		const float x = a.x - b.x, y = a.y - b.y, z = a.z - b.z;
		return std::sqrt(x*x + y*y + z*z);
	}
	bool active_ = false;
	BombGuardWorkStats workStats_{};
	nav::NavSnapshot indexedSnapshot_{};
	std::unordered_map<nav::AreaId, const nav::NavArea *> areaIndex_;
	std::unordered_set<nav::AreaId> failedGoals_;
	nav::NavPathCostContext failedCost_{};
	nav::AreaId failedStart_ = 0U, failedAnchor_ = 0U;
	world::EntityKey failedBomb_{};
	std::uint32_t failedRound_ = 0U;
	std::uint32_t map_ = 0U, round_ = 0U;
	std::uint64_t revision_ = 0U;
	world::EntityKey bomb_{};
	nav::NavVector spot_{};
	float selectedAt_ = 0.0f, expiresAt_ = 0.0f;
};
} }
#endif
