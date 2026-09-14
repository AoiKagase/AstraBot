// SPDX-License-Identifier: MPL-2.0
#include "nav/local/terrain_sampler.hpp"
#include "nav/local/supported_nav.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace astrabot::nav::local
{
namespace
{
bool valid(runtime::HullDimensions h) noexcept
{
	return h.minimum.isFinite() && h.maximum.isFinite() && h.minimum.x < h.maximum.x && h.minimum.y < h.maximum.y &&
		   h.minimum.z < h.maximum.z;
}
bool representable(double value) noexcept
{
	return std::isfinite(value) && value >= std::numeric_limits<float>::lowest() &&
		   value <= (std::numeric_limits<float>::max)();
}
ProbeReason floorReason(const runtime::FloorObservation& f, double minimumNormal) noexcept
{
	switch (f.status)
	{
	case runtime::FloorObservationStatus::TraceNoHit:
		return ProbeReason::TraceNoHit;
	case runtime::FloorObservationStatus::StartSolid:
		return ProbeReason::StartSolid;
	case runtime::FloorObservationStatus::AllSolid:
		return ProbeReason::AllSolid;
	case runtime::FloorObservationStatus::InvalidTrace:
		return ProbeReason::InvalidResult;
	case runtime::FloorObservationStatus::UnsupportedNormal:
		return ProbeReason::UnsupportedFloor;
	case runtime::FloorObservationStatus::HeightMismatch:
		return ProbeReason::FloorHeightMismatch;
	case runtime::FloorObservationStatus::NavContainmentMissing:
		break; // NAV is advisory; validate physical floor below.
	case runtime::FloorObservationStatus::Unknown:
	case runtime::FloorObservationStatus::Supported:
		break;
	}
	if (!std::isfinite(f.height) || !f.normal.isFinite())
		return ProbeReason::InvalidResult;
	if (!f.supported)
		return ProbeReason::ActorNotGrounded;
	if (f.normal.z < minimumNormal)
		return ProbeReason::UnsupportedFloor;
	const double n =
		double(f.normal.x) * f.normal.x + double(f.normal.y) * f.normal.y + double(f.normal.z) * f.normal.z;
	return n >= 0.99 && n <= 1.01 ? ProbeReason::None : ProbeReason::InvalidResult;
}
ProbeResult probe(const runtime::MovementSnapshot& s, std::uint64_t generation, model::NavAreaId currentArea, float x,
				  float y, const query::NavSpatialIndex& index, core::MapGeneration indexMap,
				  runtime::IWorldQueries& port, GroundProbeLimits limits, bool locateOnly) noexcept
{
	ProbeResult result;
	result.stamp = {s.agent, s.actor, s.map, s.tick, generation, 0};
	const auto fail = [&](ProbeReason reason) {
		result.reason = reason;
		result.target.reset();
		return result;
	};
	if (!s.agent.isValid() || !s.actor.isValid() || !s.map.isValid() || !s.tick.isValid() || !generation ||
		(!locateOnly && !currentArea.isValid()) || s.kind != runtime::ActorKind::ManagedBot || s.connected != true ||
		s.alive != true || s.joined != true || !s.position || !s.position->isFinite() || !s.hull || !valid(*s.hull) ||
		!std::isfinite(x) || !std::isfinite(y))
		return fail(ProbeReason::InvalidInput);
	if (indexMap != s.map)
		return fail(ProbeReason::StaleNavigation);
	for (double v : {limits.maxDistance, limits.sampleSpacing, limits.maxStepUp, limits.maxDrop, limits.probeDepth,
					 limits.supportTolerance, limits.navTolerance, limits.minNormalZ})
		if (!std::isfinite(v) || v < 0)
			return fail(ProbeReason::InvalidInput);
	if (limits.sampleSpacing == 0 || limits.probeDepth < limits.maxDrop || limits.minNormalZ <= 0 ||
		limits.minNormalZ > 1)
		return fail(ProbeReason::InvalidInput);
	const double dx = double(x) - s.position->x, dy = double(y) - s.position->y;
	const double distance = std::hypot(dx, dy);
	const double count = locateOnly ? 0.0 : std::max(1.0, std::ceil(distance / limits.sampleSpacing));
	if (distance > limits.maxDistance || count > limits.maxSamples || limits.maxQueries < 1)
		return fail(ProbeReason::BudgetExceeded);
	const auto samples = static_cast<std::uint32_t>(count);
	const auto fetch = [&](runtime::QueryKind kind, model::NavVector3 start,
						   model::NavVector3 end) -> std::optional<runtime::WorldQueryResult> {
		if (!start.isFinite() || !end.isFinite())
		{
			result.reason = ProbeReason::InvalidInput;
			return {};
		}
		if (result.queries == limits.maxQueries)
		{
			result.reason = ProbeReason::BudgetExceeded;
			return {};
		}
		runtime::QueryRequest q{{s.agent, s.actor, s.map, s.tick, generation, ++result.queries},
								kind,
								start,
								end,
								s.hull,
								limits.navTolerance};
		try
		{
			auto reply = port.query(q);
			if (!(reply.stamp == q.stamp) || reply.kind != kind)
			{
				result.reason = ProbeReason::StaleQuery;
				return {};
			}
			else if (reply.error == runtime::QueryError::BudgetExceeded)
				result.reason = ProbeReason::BudgetExceeded;
			else if (reply.error == runtime::QueryError::Unavailable)
				result.reason = ProbeReason::QueryUnavailable;
			else if (reply.error == runtime::QueryError::InvalidResult)
				result.reason = ProbeReason::InvalidResult;
			else if (reply.error != runtime::QueryError::None)
				result.reason = ProbeReason::QueryFailed;
			else
				return reply;
		}
		catch (...)
		{
			result.reason = ProbeReason::QueryFailed;
		}
		return {};
	};

	const auto clearance = [&](model::NavVector3 a, model::NavVector3 b) {
		const auto reply = fetch(runtime::QueryKind::SweptHull, a, b);
		if (!reply)
			return false;
		if (!reply->hull)
		{
			result.reason = ProbeReason::InvalidResult;
			return false;
		}
		const auto& hit = *reply->hull;
		if (!std::isfinite(hit.fraction) || hit.fraction < 0 || hit.fraction > 1 || !hit.end.isFinite() ||
			!hit.normal.isFinite())
		{
			result.reason = ProbeReason::InvalidResult;
			return false;
		}
		if (hit.startSolid || hit.allSolid || hit.fraction < 1)
		{
			result.reason = ProbeReason::Blocked;
			return false;
		}
		if (std::abs(double(hit.end.x) - b.x) > 0.001 || std::abs(double(hit.end.y) - b.y) > 0.001 ||
			std::abs(double(hit.end.z) - b.z) > 0.001)
		{
			result.reason = ProbeReason::InvalidResult;
			return false;
		}
		result.reason = ProbeReason::None;
		return true;
	};
	// Candidate sensing never proves that the player's footprint or head fits.
	const auto support = [&](runtime::QueryKind kind, float px, float py, double top,
							 double bottom) -> std::optional<runtime::FloorObservation> {
		if (!representable(top) || !representable(bottom) || top <= bottom)
		{
			result.reason = ProbeReason::InvalidInput;
			return {};
		}
		auto reply = fetch(kind, {px, py, static_cast<float>(top)}, {px, py, static_cast<float>(bottom)});
		if (!reply)
			return {};
		if (!reply->floor)
		{
			result.reason = ProbeReason::QueryUnavailable;
			return {};
		}
		auto f = *reply->floor;
		result.reason = floorReason(f, limits.minNormalZ);
		if (result.reason == ProbeReason::None && (f.height > top || f.height < bottom))
			result.reason = ProbeReason::InvalidResult;
		return f;
	};
	const double feet = double(s.position->z) + s.hull->minimum.z;
	// Use the current origin (plus a small clearance) for source support:
	// raising a whole step here could incorrectly attach an airborne actor to a ledge.
	auto source = support(runtime::QueryKind::HullSupport, s.position->x, s.position->y, feet + limits.supportTolerance,
						  feet - limits.probeDepth);
	if (!source)
		return result;
	result.initialTrace = source->trace;
	result.initialReason = result.reason;
	if (result.reason == ProbeReason::StartSolid || result.reason == ProbeReason::AllSolid)
	{
		// The probing offset itself can hit a low ceiling. Retry from just
		// above the real feet, then still require the actual hull passage.
		result.supportFallbackAttempted = true;
		source = support(runtime::QueryKind::HullSupport, s.position->x, s.position->y, feet + 0.03125,
						 feet - limits.probeDepth);
		if (!source)
			return result;
		result.fallbackTrace = source->trace;
		if (result.reason == ProbeReason::None)
			result.supportFallbackAccepted = true;
	}
	if (result.reason != ProbeReason::None)
		return fail(result.reason);
	auto floor = *source;
	const double sourceDrop = feet - floor.height;
	if (sourceDrop < -limits.supportTolerance || sourceDrop > limits.maxDrop ||
		(s.grounded == true && std::abs(sourceDrop) > limits.supportTolerance))
		return fail(ProbeReason::FloorHeightMismatch);
	const double sourceZ = double(floor.height) - s.hull->minimum.z;
	if (!representable(sourceZ))
		return fail(ProbeReason::InvalidInput);
	model::NavVector3 position{s.position->x, s.position->y, static_cast<float>(sourceZ)};
	// A transient loss of FL_ONGROUND is admissible only with a clear, bounded
	// downward path to the measured support. Never convert a hull penetration.
	if (!clearance(*s.position, position))
		return fail(result.reason);
	// Preserve verified physical support even if the advisory NAV lookup below
	// has no candidate. Diagnostics must not describe that as an absent floor.
	result.floorEvidenceValid = true;
	result.startFloorHeight = floor.height;
	result.lastFloorHeight = floor.height;
	auto match = supportedNavArea(index, {position.x, position.y, floor.height}, *s.hull, limits,
								  locateOnly ? model::NavAreaId{} : currentArea);
	if (!match || !*match.value)
		return fail(ProbeReason::NavContainmentMissing);
	model::NavAreaId area = (**match.value).areaId;
	// A physically supported hull may straddle a narrow NAV seam. Keep the
	// measured area and let PathFollower decide whether it is reachable on
	// the active route; rejecting it here turns a continuous passage into a
	// false wall.
	for (std::uint32_t i = 0; i < samples; ++i)
	{
		const double fraction = double(i + 1) / samples;
		const float tx = static_cast<float>(s.position->x + dx * fraction);
		const float ty = static_cast<float>(s.position->y + dy * fraction);
		const double top = double(floor.height) + limits.maxStepUp + limits.supportTolerance;
		const double bottom = double(floor.height) - limits.probeDepth;
		auto candidate = support(runtime::QueryKind::FloorCandidate, tx, ty, top, bottom);
		if (!candidate)
			return fail(result.reason);
		const auto candidateReason = result.reason;
		// A point may start in the riser while the player's footprint can land
		// on a nearby tread. Only physical hull support and a full step path may
		// rescue that candidate; invalid/stale/unavailable results stay unknown.
		const bool rescue = candidateReason == ProbeReason::StartSolid || candidateReason == ProbeReason::AllSolid;
		if (rescue)
			result.supportFallbackAttempted = true;
		if (candidateReason != ProbeReason::None && !rescue)
			return fail(candidateReason);
		auto landing = support(runtime::QueryKind::HullSupport, tx, ty, top, bottom);
		if (!landing)
			return fail(result.reason);
		if (result.reason != ProbeReason::None)
			return fail(result.reason);
		const auto next = *landing;
		const double delta = double(next.height) - floor.height;
		if (delta > limits.maxStepUp)
			return fail(ProbeReason::Blocked);
		if (-delta > limits.maxDrop)
			return fail(ProbeReason::UnsafeDrop);
		const double originZ = double(next.height) - s.hull->minimum.z;
		if (!representable(originZ))
			return fail(ProbeReason::InvalidInput);
		const model::NavVector3 destination{tx, ty, static_cast<float>(originZ)};
		const auto nextMatch = supportedNavArea(index, {tx, ty, next.height}, *s.hull, limits, {});
		if (!nextMatch)
			return fail(ProbeReason::QueryUnavailable);
		const auto nextArea = nextMatch.value && *nextMatch.value ? (**nextMatch.value).areaId : area;
		if (!nextMatch.value || !*nextMatch.value)
		{
			// Physical FloorCandidate/HullSupport/SweptHull evidence remains
			// authoritative when adjacent NAV rectangles have a center gap.
			if (result.initialReason == ProbeReason::None)
				result.initialReason = ProbeReason::NavContainmentMissing;
		}
		bool direct = false;
		if (!rescue)
			direct = clearance(position, destination);
		if (!direct)
		{
			if (!rescue && result.reason != ProbeReason::Blocked)
				return fail(result.reason);
			const double liftZ = double(position.z) + limits.maxStepUp;
			if (!representable(liftZ))
				return fail(ProbeReason::InvalidInput);
			const model::NavVector3 lifted{position.x, position.y, static_cast<float>(liftZ)};
			const model::NavVector3 across{tx, ty, lifted.z};
			if (!clearance(position, lifted) || !clearance(lifted, across) || !clearance(across, destination))
				return fail(result.reason);
			result.lastStep = StepEvidence{position, lifted, across, GroundedTarget{destination, nextArea, next}};
			++result.steps;
			if (rescue)
			{
				result.supportFallbackAttempted = true;
				result.supportFallbackAccepted = true;
				result.fallbackTrace = next.trace;
			}
		}
		result.floorDelta = -delta;
		result.cumulativeDownDrop = (std::max)(0.0, result.startFloorHeight - double(next.height));
		result.maxDownStep = (std::max)(result.maxDownStep, (std::max)(0.0, -delta));
		result.lastFloorHeight = next.height;
		position = destination;
		floor = next;
		area = nextArea;
		++result.samples;
	}
	result.reason = ProbeReason::None;
	result.target = GroundedTarget{position, area, floor};
	return result;
}
} // namespace
ProbeResult TerrainSampler::locate(const runtime::MovementSnapshot& s, std::uint64_t generation,
								   const query::NavSpatialIndex& index, core::MapGeneration indexMap,
								   runtime::IWorldQueries& port, GroundProbeLimits limits) noexcept
{
	return probe(s, generation, {}, s.position ? s.position->x : 0, s.position ? s.position->y : 0, index, indexMap,
				 port, limits, true);
}
ProbeResult TerrainSampler::inspect(const runtime::MovementSnapshot& s, std::uint64_t generation,
									model::NavAreaId currentArea, float x, float y, const query::NavSpatialIndex& index,
									core::MapGeneration indexMap, runtime::IWorldQueries& port,
									GroundProbeLimits limits) noexcept
{
	return probe(s, generation, currentArea, x, y, index, indexMap, port, limits, false);
}
} // namespace astrabot::nav::local
