#include "runtime/behavior/bot_look_geometry.hpp"
#include <algorithm>
#include <cmath>
namespace astrabot
{
namespace metamod
{
namespace
{
constexpr int kIgnoreMonstersTrace = 1;
constexpr int kApproachChestHeight = 36;
constexpr int kMinimumApproachLength = 1;
constexpr int kMaximumApproachLength = 8192;
constexpr int kBendAngleStepDegrees = 10;
constexpr int kMaximumBendAngleDegrees = 135;
constexpr int kBendSides = 2;
constexpr int kRaySampleStep = 50;
constexpr int kGroundProbeDepth = 1000;
#define ASTRABOT_LOOK_GEOMETRY_DEGREES_TO_RADIANS .01745329252f
bool clearRay(
	enginefuncs_t &engine, edict_t *self, const nav::NavVector &from, const nav::NavVector &to, TraceResult *result)
{
	const float a[] = {from.x, from.y, from.z}, b[] = {to.x, to.y, to.z};
	*result = {};
	engine.pfnTraceLine(a, b, kIgnoreMonstersTrace, self, result);
	return result->flFraction >= 1 && !result->fStartSolid && !result->fAllSolid;
}

}
bool bendLookApproach(
	enginefuncs_t &engine, edict_t *self, const nav::NavVector &eye, const nav::NavVector &point, nav::NavVector *bend,
	const std::function<bool()> *current)
{
	const auto live = [&] { return current == nullptr || (*current)(); };
	const nav::NavVector chest = {point.x, point.y, point.z + kApproachChestHeight};
	TraceResult trace{};
	const bool direct = clearRay(engine, self, eye, chest, &trace);
	if (!live())
		return false;
	if (direct)
	{
		*bend = point;
		return true;
	}
	const float length = std::hypot(point.x - eye.x, point.y - eye.y);
	if (!std::isfinite(length) || length < kMinimumApproachLength || length > kMaximumApproachLength)
		return false;
	const float start = std::atan2(point.y - eye.y, point.x - eye.x);
	for (int angle = kBendAngleStepDegrees; angle <= kMaximumBendAngleDegrees; angle += kBendAngleStepDegrees)
		for (int side = 0; side < kBendSides; ++side)
		{
			const float yaw = start + (side ? angle : -angle) * ASTRABOT_LOOK_GEOMETRY_DEGREES_TO_RADIANS;
			const nav::NavVector rotated = {eye.x + length * std::cos(yaw), eye.y + length * std::sin(yaw), point.z};
			const nav::NavVector rotatedChest = {rotated.x, rotated.y, rotated.z + kApproachChestHeight};
			clearRay(engine, self, eye, rotatedChest, &trace);
			if (!live())
				return false;
			if (trace.fStartSolid || trace.fAllSolid)
				continue;
			const float dz = rotated.z - eye.z, rayLength = std::sqrt(length * length + dz * dz);
			const float visible = rayLength * std::max(0.0f, std::min(1.0f, trace.flFraction));
			for (float distance = kRaySampleStep; distance <= visible; distance += kRaySampleStep)
			{
				const float t = distance / rayLength;
				nav::NavVector ray = {eye.x + t * (rotated.x - eye.x), eye.y + t * (rotated.y - eye.y), eye.z + t * dz};
				const bool canSee = clearRay(engine, self, ray, chest, &trace);
				if (!live())
					return false;
				if (!canSee)
					continue;
				TraceResult ground{};
				const bool noGround = clearRay(engine, self, {ray.x, ray.y, ray.z + kApproachChestHeight},
					{ray.x, ray.y, ray.z - kGroundProbeDepth}, &ground);
				if (!live())
					return false;
				if (!noGround &&
					!ground.fStartSolid && !ground.fAllSolid)
					ray.z = ground.vecEndPos.z;
				else
					ray.z = point.z;
				*bend = ray;
				return true;
			}
		}
	return false;
}
}
}
#undef ASTRABOT_LOOK_GEOMETRY_DEGREES_TO_RADIANS
