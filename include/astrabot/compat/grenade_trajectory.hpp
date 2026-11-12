#ifndef ASTRABOT_COMPAT_GRENADE_TRAJECTORY_HPP
#define ASTRABOT_COMPAT_GRENADE_TRAJECTORY_HPP
#include "astrabot/world/world_snapshot.hpp"
#include <cmath>
namespace astrabot
{
namespace compat
{
struct GrenadeLaunch
{
	world::WorldPosition origin;
	world::WorldVector velocity;
	float pitch;
	float yaw;
	float flightSeconds;
	float gravity;
	bool valid;
};
inline world::WorldPosition grenadePosition(const GrenadeLaunch &launch, float seconds)
{
	return {launch.origin.x + launch.velocity.x * seconds,
			launch.origin.y + launch.velocity.y * seconds,
			launch.origin.z + launch.velocity.z * seconds -
				0.5f * launch.gravity * seconds * seconds};
}
// Public CS grenade launch formula. No private weapon timers or layout reads.
inline GrenadeLaunch solveGrenadeLaunch(world::WorldPosition eye, world::WorldVector playerVelocity,
										world::WorldPosition target, float serverGravity,
										float maximumError)
{
	GrenadeLaunch best = {};
	if (!std::isfinite(serverGravity) || serverGravity <= 0.0f || !std::isfinite(maximumError) ||
		maximumError <= 0.0f || !std::isfinite(eye.x) || !std::isfinite(eye.y) ||
		!std::isfinite(eye.z) || !std::isfinite(target.x) || !std::isfinite(target.y) ||
		!std::isfinite(target.z) || !std::isfinite(playerVelocity.x) ||
		!std::isfinite(playerVelocity.y) || !std::isfinite(playerVelocity.z))
	{
		return best;
	}
	const float radians = 3.14159265358979323846f / 180.0f;
	const float yaw = std::atan2(target.y - eye.y, target.x - eye.x);
	float bestError = maximumError * maximumError;
	for (int sample = -356; sample <= 356; ++sample)
	{
		const float pitch = static_cast<float>(sample) * 0.25f;
		const float adjusted = -10.0f + pitch * (pitch < 0.0f ? 80.0f / 90.0f : 100.0f / 90.0f);
		const float angle = adjusted * radians;
		const float speed = std::fmin(750.0f, (90.0f - adjusted) * 6.0f);
		const world::WorldVector forward = {std::cos(angle) * std::cos(yaw),
											std::cos(angle) * std::sin(yaw), -std::sin(angle)};
		GrenadeLaunch candidate = {};
		candidate.origin = {eye.x + forward.x * 16.0f, eye.y + forward.y * 16.0f,
							eye.z + forward.z * 16.0f};
		candidate.velocity = {forward.x * speed + playerVelocity.x,
							  forward.y * speed + playerVelocity.y,
							  forward.z * speed + playerVelocity.z};
		const float horizontalSquared = candidate.velocity.x * candidate.velocity.x +
										candidate.velocity.y * candidate.velocity.y;
		if (horizontalSquared <= 0.001f)
		{
			continue;
		}
		const float time = ((target.x - candidate.origin.x) * candidate.velocity.x +
							(target.y - candidate.origin.y) * candidate.velocity.y) /
						   horizontalSquared;
		if (time <= 0.0f || time > 1.5f)
		{
			continue;
		}
		candidate.gravity = serverGravity * 0.5f;
		const auto position = grenadePosition(candidate, time);
		const float dx = position.x - target.x, dy = position.y - target.y,
					dz = position.z - target.z;
		const float error = dx * dx + dy * dy + dz * dz;
		if (error < bestError)
		{
			bestError = error;
			candidate.pitch = pitch;
			candidate.yaw = yaw / radians;
			candidate.flightSeconds = time;
			candidate.valid = true;
			best = candidate;
		}
	}
	return best;
}
} // namespace compat
} // namespace astrabot
#endif
