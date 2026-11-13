#ifndef ASTRABOT_COMPAT_GUARD_LOOK_HPP
#define ASTRABOT_COMPAT_GUARD_LOOK_HPP
#include "astrabot/compat/random_source.hpp"
#include "astrabot/nav/nav_query.hpp"
#include "astrabot/world/world_snapshot.hpp"
#include <algorithm>
#include <cmath>
#include <vector>
namespace astrabot
{
namespace compat
{
inline nav::NavVector guardApproachPoint(const nav::NavArea &previous, const nav::NavArea &here, unsigned how)
{
	constexpr int kLowYTransition = 0;
	constexpr int kHighXTransition = 1;
	constexpr int kHighYTransition = 2;
	constexpr int kLowXTransition = 3;
	nav::NavVector point = {
		(here.extent.lo.x + here.extent.hi.x) * .5f, (here.extent.lo.y + here.extent.hi.y) * .5f, 0};
	const auto clamp = [](float value, float lo, float hi)
	{
		return std::max(lo, std::min(hi, value));
	};
	if (how == kLowYTransition || how == kHighYTransition)
	{
		point.y = how == kLowYTransition ? previous.extent.lo.y : previous.extent.hi.y;
		point.x = .5f *
			(clamp(std::max(previous.extent.lo.x, here.extent.lo.x), previous.extent.lo.x, previous.extent.hi.x) +
				clamp(std::min(previous.extent.hi.x, here.extent.hi.x), previous.extent.lo.x, previous.extent.hi.x));
	}
	else if (how == kHighXTransition || how == kLowXTransition)
	{
		point.x = how == kLowXTransition ? previous.extent.lo.x : previous.extent.hi.x;
		point.y = .5f *
			(clamp(std::max(previous.extent.lo.y, here.extent.lo.y), previous.extent.lo.y, previous.extent.hi.y) +
				clamp(std::min(previous.extent.hi.y, here.extent.hi.y), previous.extent.lo.y, previous.extent.hi.y));
	}
	point.z = nav::surfaceZAt(here, point.x, point.y);
	return point;
}

// Private behavior state: geometry is supplied by the adapter, input by its owner.
class GuardLookPlanner
{
public:
	void reset()
	{
		*this = GuardLookPlanner{};
	}

	bool current(world::ActorKey actor, world::FrameIdentity frame, world::EntityKey bomb, const nav::NavVector &spot,
		std::uint64_t revision, float now) const
	{
		constexpr int kSpotTolerance = 1;
		return active_ && actor == actor_ && frame.mapGeneration == frame_.mapGeneration &&
			frame.roundGeneration == frame_.roundGeneration && bomb == bomb_ && revision == revision_ &&
			std::hypot(spot.x - spot_.x, spot.y - spot_.y) < kSpotTolerance &&
			std::fabs(spot.z - spot_.z) < kSpotTolerance && now >= lastTime_;
	}

	void begin(world::ActorKey actor, world::FrameIdentity frame, world::EntityKey bomb, const nav::NavVector &spot,
		std::uint64_t revision, float now, float openYaw, const std::vector<nav::NavVector> &approaches)
	{
		reset();
		active_ = true;
		actor_ = actor;
		frame_ = frame;
		bomb_ = bomb;
		spot_ = spot;
		revision_ = revision;
		lastTime_ = now;
		nextTime_ = now;
		desiredYaw_ = openYaw;
		approaches_ = approaches;
	}

	bool update(float now, bool sniper, const nav::NavVector &eye, float currentPitch, float currentYaw,
		ICompatibilityRandomSource &random, RandomTimingContext timing, float *pitch, float *yaw)
	{
		constexpr float kSniperMinimumIntervalSeconds = 5.0f;
		constexpr float kSniperMaximumIntervalSeconds = 10.0f;
		constexpr float kMinimumIntervalSeconds = 1.0f;
		constexpr float kMaximumIntervalSeconds = 2.0f;
		constexpr int kMinimumApproachRange = 1;
// Keep literal evaluation precision on x87; each macro is undefined below.
#define ASTRABOT_GUARD_LOOK_RADIANS_TO_DEGREES 57.295779513f
		constexpr int kApproachAimHeight = 36;
		constexpr float kTurnDegreesPerSecond = 240.0f;
#define ASTRABOT_GUARD_LOOK_MAXIMUM_UPDATE_SECONDS .1f
		if (!active_ || !std::isfinite(now) || now < lastTime_ || !pitch || !yaw)
			return false;
		if (now >= nextTime_)
		{
			const auto interval = random.nextFloat(
				RandomRequest::floatRequest("LookAround.StateInterval", {actor_.slot, actor_.generation}, timing,
					sniper ? kSniperMinimumIntervalSeconds : kMinimumIntervalSeconds,
					sniper ? kSniperMaximumIntervalSeconds : kMaximumIntervalSeconds));
			if (interval.status != RandomStatus::Ok || !std::isfinite(interval.value) ||
				interval.value < (sniper ? kSniperMinimumIntervalSeconds : kMinimumIntervalSeconds) ||
				interval.value > (sniper ? kSniperMaximumIntervalSeconds : kMaximumIntervalSeconds))
				return false;
			nextTime_ = now + interval.value;
			if (!approaches_.empty())
			{
				const auto which = random.nextLong(RandomRequest::longRequest("LookAround.ApproachPoint",
					{actor_.slot, actor_.generation}, timing, 0, static_cast<std::int32_t>(approaches_.size() - 1)));
				if (which.status != RandomStatus::Ok || which.value < 0 ||
					static_cast<std::size_t>(which.value) >= approaches_.size())
					return false;
				const auto &p = approaches_[which.value];
				const float dx = p.x - eye.x, dy = p.y - eye.y;
				const float range = std::hypot(dx, dy);
				if (range >= kMinimumApproachRange)
				{
					desiredYaw_ = std::atan2(dy, dx) * ASTRABOT_GUARD_LOOK_RADIANS_TO_DEGREES;
					desiredPitch_ =
						-std::atan2(p.z + kApproachAimHeight - eye.z, range) * ASTRABOT_GUARD_LOOK_RADIANS_TO_DEGREES;
				}
			}
		}
		const float dt = std::max(0.0f, std::min(ASTRABOT_GUARD_LOOK_MAXIMUM_UPDATE_SECONDS, now - lastTime_));
		lastTime_ = now;
		const float step = kTurnDegreesPerSecond * dt;
		*pitch = currentPitch + std::max(-step, std::min(step, normalize(desiredPitch_ - currentPitch)));
		*yaw = normalize(currentYaw + std::max(-step, std::min(step, normalize(desiredYaw_ - currentYaw))));
		return true;
	}
#undef ASTRABOT_GUARD_LOOK_RADIANS_TO_DEGREES
#undef ASTRABOT_GUARD_LOOK_MAXIMUM_UPDATE_SECONDS

private:
	static float normalize(float value)
	{
		constexpr float kFullTurnDegrees = 360.0f;
		return std::remainder(value, kFullTurnDegrees);
	}

	bool active_ = false;
	world::ActorKey actor_{};
	world::FrameIdentity frame_{};
	world::EntityKey bomb_{};
	nav::NavVector spot_{};
	std::uint64_t revision_ = 0;
	float nextTime_ = 0, lastTime_ = 0, desiredYaw_ = 0, desiredPitch_ = 0;
	std::vector<nav::NavVector> approaches_;
};
}
}
#endif
