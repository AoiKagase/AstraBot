#ifndef ASTRABOT_COMPAT_COMBAT_BEHAVIOR_HPP
#define ASTRABOT_COMPAT_COMBAT_BEHAVIOR_HPP

#include "astrabot/combat/weapon_state.hpp"
#include <array>
#include <cmath>
#include <cstddef>

namespace astrabot
{
namespace compat
{

// Only already-visible, non-smoke-obscured candidates belong at this boundary.
struct ThreatCandidate
{
	world::ActorKey actor;
	world::WorldPosition visiblePoint;
	float distance;
	bool attackingObserver;
};

class ReactionQueue
{
  public:
	ReactionQueue() : samples_(), observer_(), frame_(), head_(0U), count_(0U), lastTime_(0.0f)
	{
	}
	void reset()
	{
		*this = ReactionQueue();
	}
	bool update(world::ActorKey observer, world::FrameIdentity frame, float now,
				float reactionSeconds, const ThreatCandidate *candidates, std::size_t count,
				bool blind, ThreatCandidate *selected)
	{
		if (selected == nullptr)
			return false;
		*selected = {};
		if (!observer.isValid() || !frame.isValid() || !std::isfinite(now) || now < 0.0f ||
			!std::isfinite(reactionSeconds) || reactionSeconds < 0.0f || (count && !candidates))
			return false;
		if (!(observer == observer_) || frame.mapGeneration != frame_.mapGeneration ||
			frame.roundGeneration != frame_.roundGeneration || now < lastTime_)
			reset();
		else if (count_ && frame.tick <= frame_.tick)
			return false;
		observer_ = observer;
		frame_ = frame;
		lastTime_ = now;
		Sample sample = {};
		sample.time = now;
		if (!blind)
		{
			for (std::size_t i = 0; i < count; ++i)
			{
				const ThreatCandidate &c = candidates[i];
				if (!c.actor.isValid() || c.actor == observer || !std::isfinite(c.distance) ||
					c.distance < 0.0f || !std::isfinite(c.visiblePoint.x) ||
					!std::isfinite(c.visiblePoint.y) || !std::isfinite(c.visiblePoint.z))
					continue;
				if (!sample.hasThreat ||
					(c.attackingObserver && !sample.threat.attackingObserver) ||
					(c.attackingObserver == sample.threat.attackingObserver &&
					 c.distance < sample.threat.distance))
				{
					sample.threat = c;
					sample.hasThreat = true;
				}
			}
		}
		samples_[head_] = sample;
		head_ = (head_ + 1U) % samples_.size();
		if (count_ < samples_.size())
			++count_;
		if (blind)
			return false;
		// Select the most recent complete recognition snapshot old enough for the profile.
		for (std::size_t age = 0; age < count_; ++age)
		{
			const Sample &historical =
				samples_[(head_ + samples_.size() - 1U - age) % samples_.size()];
			if (historical.time <= now - reactionSeconds + 0.00001f)
			{
				if (!historical.hasThreat)
					return false;
				// Never resurrect a disconnected/reused actor slot from a delayed sample.
				for (std::size_t i = 0; i < count; ++i)
					if (candidates[i].actor == historical.threat.actor)
					{
						*selected = historical.threat;
						return true;
					}
				return false;
			}
		}
		return false;
	}

  private:
	struct Sample
	{
		float time;
		bool hasThreat;
		ThreatCandidate threat;
	};
	std::array<Sample, 64U> samples_;
	world::ActorKey observer_;
	world::FrameIdentity frame_;
	std::size_t head_, count_;
	float lastTime_;
};

struct AimStep
{
	float yaw;
	float pitch;
	bool aligned;
};

inline world::WorldPosition combatAimPoint(world::WorldPosition enemy, world::WorldPosition observer,
	std::uint8_t visibleParts, std::uint16_t weapon, float skill, float feetOffset,
	bool recoilKnown, float accuracy)
{
	const bool pistol = weapon == 1 || weapon == 10 || weapon == 11 || weapon == 16 || weapon == 17 || weapon == 26;
	const bool sniper = weapon == 3 || weapon == 13 || weapon == 18 || weapon == 24;
	const bool chest = (visibleParts & world::VisibleChest) != 0U;
	if (chest && (weapon == 18 || weapon == 5 || weapon == 21 || weapon == 20 || skill < .8f ||
		(recoilKnown && accuracy > .4f && !pistol && !sniper))) enemy.z -= feetOffset * .25f;
	else if ((visibleParts & world::VisibleHead) == 0U)
	{
		if (chest) enemy.z -= feetOffset * .5f;
		else if ((visibleParts & (world::VisibleLeftSide | world::VisibleRightSide)) != 0U)
		{
			const float dx = enemy.x - observer.x, dy = enemy.y - observer.y;
			const float length = std::hypot(dx, dy);
			const float side = (visibleParts & world::VisibleLeftSide) != 0U ? 1.0f : -1.0f;
			if (length > .001f) { enemy.x -= side * dy / length * 16; enemy.y += side * dx / length * 16; }
			enemy.z -= feetOffset * .5f;
		}
		else enemy.z -= feetOffset * 2.0f;
	}
	return enemy;
}

inline bool combatAimOnTarget(world::WorldPosition point, world::WorldPosition origin, float yaw, bool sniper)
{
	const float dx = point.x - origin.x, dy = point.y - origin.y;
	const float range = std::hypot(dx, dy);
	if (!std::isfinite(range) || range <= .001f || !std::isfinite(yaw)) return false;
	const float radians = yaw * .01745329251994329577f;
	const float dot = (dx * std::cos(radians) + dy * std::sin(radians)) / range;
	return dot > std::cos(std::atan((sniper ? 16.0f : 32.0f) / range));
}
struct AimSpringState
{
	float yawVelocity;
	float pitchVelocity;
};
inline float normalizedAimAngle(float angle)
{
	angle = std::fmod(angle + 180.0f, 360.0f);
	if (angle < 0.0f)
		angle += 360.0f;
	return angle - 180.0f;
}
inline AimStep trackAim(float yaw, float pitch, float desiredYaw, float desiredPitch, float elapsed,
						float skill, AimSpringState *persistent = nullptr, bool attacking = true)
{
	AimStep result = {yaw, pitch, false};
	if (!std::isfinite(yaw) || !std::isfinite(pitch) || !std::isfinite(desiredYaw) ||
		!std::isfinite(desiredPitch) || !std::isfinite(elapsed) || elapsed < 0.0f ||
		!std::isfinite(skill) || skill < 0.0f || skill > 1.0f)
		return result;
	AimSpringState temporary = {};
	AimSpringState &state = persistent ? *persistent : temporary;
	const float stiffness = attacking ? 300.0f : 200.0f;
	const float damping = attacking ? 30.0f : 25.0f;
	const float dy = normalizedAimAngle(desiredYaw - yaw);
	const float dp = normalizedAimAngle(desiredPitch - pitch);
	float yawAccel = stiffness * dy - damping * state.yawVelocity;
	float pitchAccel = 2.0f * stiffness * dp - damping * state.pitchVelocity;
	yawAccel = yawAccel > 3000.0f ? 3000.0f : yawAccel < -3000.0f ? -3000.0f : yawAccel;
	pitchAccel = pitchAccel > 3000.0f ? 3000.0f : pitchAccel < -3000.0f ? -3000.0f : pitchAccel;
	if (std::fabs(dy) < 1.0f)
	{
		result.yaw = desiredYaw;
		state.yawVelocity = 0.0f;
	}
	else
	{
		state.yawVelocity += elapsed * yawAccel;
		result.yaw = normalizedAimAngle(yaw + elapsed * state.yawVelocity);
	}
	state.pitchVelocity += elapsed * pitchAccel;
	result.pitch = pitch + elapsed * state.pitchVelocity;
	if (result.pitch < -89.0f)
		result.pitch = -89.0f;
	if (result.pitch > 89.0f)
		result.pitch = 89.0f;
	result.aligned = std::fabs(normalizedAimAngle(desiredYaw - result.yaw)) <= 1.0f &&
					 std::fabs(desiredPitch - result.pitch) <= 1.0f;
	return result;
}
inline bool weaponEngagementAllowed(combat::WeaponKind kind, float distance)
{
	if (!std::isfinite(distance) || distance < 0.0f)
		return false;
	if (kind == combat::WeaponKind::Unknown || kind == combat::WeaponKind::Grenade)
		return false;
	return kind != combat::WeaponKind::Knife || distance <= 75.0f;
}
} // namespace compat
} // namespace astrabot
#endif
