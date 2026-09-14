// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "nav/local/primitive.hpp"
#include "nav/runtime/movement_snapshot.hpp"
#include <cmath>

namespace astrabot::nav::local
{
enum class LocomotionPhase : std::uint8_t
{
	Ground,
	Takeoff,
	Airborne,
	Drop,
	Hold
};
// This is a bounded identity/spatial permission to RECHECK motion, never a
// replacement for the current hull sweep/support check at dispatch.
struct MotionEnvelope
{
	Binding binding{};
	core::TickId tick{};
	runtime::HullDimensions hull{};
	model::NavVector3 origin{}, target{};
	model::NavAreaId supportArea{};
	LocomotionPhase phase{LocomotionPhase::Hold};
	std::uint64_t validForUs{120000};
	double maximumDisplacement{64};
	bool matches(const runtime::MovementSnapshot& s, std::uint64_t age) const noexcept
	{
		if (s.agent != binding.agent || s.actor != binding.actor || s.map != binding.map || !binding.routeGeneration ||
			!tick.isValid() || !s.tick.isAfter(tick) || s.kind != runtime::ActorKind::ManagedBot ||
			s.connected != true || s.alive != true || s.joined != true || !s.position || !s.position->isFinite() ||
			!s.hull || s.hull->minimum != hull.minimum || s.hull->maximum != hull.maximum || !origin.isFinite() ||
			!target.isFinite() || !validForUs || validForUs > 120000 || age > validForUs ||
			!std::isfinite(maximumDisplacement) || maximumDisplacement <= 0)
			return false;
		return std::hypot(double(s.position->x) - origin.x, double(s.position->y) - origin.y) <= maximumDisplacement;
	}
};
struct MovementFeedback
{
	Binding binding{};
	core::TickId commandTick{}, dispatchTick{};
	bool dispatched{}, physicalValid{};
	std::uint64_t durationUs{};
	model::NavVector3 before{}, after{}, velocityBefore{}, velocityAfter{};
	bool groundedBefore{}, groundedAfter{};
};
} // namespace astrabot::nav::local
