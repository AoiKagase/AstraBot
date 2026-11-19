#ifndef ASTRABOT_COMPAT_NOISE_INVESTIGATION_HPP
#define ASTRABOT_COMPAT_NOISE_INVESTIGATION_HPP
#include "astrabot/world/world_snapshot.hpp"
#include "astrabot/compat/random_source.hpp"
#include <algorithm>
#include <cmath>
namespace astrabot
{
namespace compat
{
struct NoiseInvestigationInput
{
	world::ActorKey actor{};
	world::FrameIdentity frame{};
	RandomTimingContext timing{};
	float now = 0, elapsedRound = 0, aggression = .5f;
	float reactionSeconds = 0.0f;
	float seenEnemyElapsed = -1.0f;
	std::uint64_t navRevision = 0;
	world::WorldPosition origin{};
	world::NoiseMemory noise{};
	bool allowed = false, visibleEnemy = false, alive = false, hiding = false, travelKnown = false;
	float travelDistance = -1;
	unsigned friends = 0;
	bool defenseAmbushExpired = false;
};
enum class NoiseInvestigationResult
{
	Ignore,
	NeedTravel,
	Attend
};
inline bool investigationShouldWalk(float distance, unsigned friends, bool knife, bool hurry)
{
	return !hurry && (knife || (distance < 1500.0f && friends <= 2));
}
class NoiseInvestigationPlanner
{
public:
	void reset()
	{
		*this = NoiseInvestigationPlanner{};
	}
	void cancel()
	{
		active_ = false;
	}
	bool current(world::ActorKey actor, const world::FrameIdentity &frame, float now, std::uint64_t revision) const
	{
		return initialized_ && actor_ == actor && map_ == frame.mapGeneration && round_ == frame.roundGeneration &&
			revision != 0 && navRevision_ == revision && std::isfinite(now) && now >= lastNow_;
	}
	bool active() const
	{
		return active_;
	}
	world::WorldPosition target() const
	{
		return target_;
	}
	std::uint32_t area() const
	{
		return area_;
	}
	NoiseInvestigationResult evaluate(const NoiseInvestigationInput &in, ICompatibilityRandomSource &rng)
	{
		using Result = NoiseInvestigationResult;
		if (!in.actor.isValid() || !in.frame.isValid() || !std::isfinite(in.now) || !std::isfinite(in.elapsedRound) ||
			!std::isfinite(in.reactionSeconds) || in.reactionSeconds < 0 || !std::isfinite(in.aggression) ||
			in.aggression < 0 || in.aggression > 1 || !in.alive || !std::isfinite(in.origin.x) ||
			!std::isfinite(in.origin.y) || !std::isfinite(in.origin.z))
		{
			reset();
			return Result::Ignore;
		}
		if (!initialized_ || !(actor_ == in.actor) || map_ != in.frame.mapGeneration ||
			round_ != in.frame.roundGeneration || in.now < lastNow_ || navRevision_ != in.navRevision)
		{
			reset();
			initialized_ = true;
			actor_ = in.actor;
			map_ = in.frame.mapGeneration;
			round_ = in.frame.roundGeneration;
			navRevision_ = in.navRevision;
			safeTime_ = 15.0f + 5.0f * in.aggression;
		}
		lastNow_ = in.now;
		if (in.seenEnemyElapsed >= 0 && std::isfinite(in.seenEnemyElapsed) && safeTime_ > in.seenEnemyElapsed)
			safeTime_ = in.seenEnemyElapsed - 2.0f;
		if (in.visibleEnemy && safeTime_ > in.elapsedRound)
			safeTime_ = in.elapsedRound - 2.0f;
		const auto &noise = in.noise;
		if (!noise.active || noise.kind == world::SoundKind::Objective || noise.navArea == 0 || in.navRevision == 0 ||
			noise.navRevision != in.navRevision || !std::isfinite(noise.timestampSeconds) ||
			noise.timestampSeconds <= 0 || noise.timestampSeconds > in.now ||
			in.now - noise.timestampSeconds < in.reactionSeconds ||
			!in.allowed || in.visibleEnemy || in.elapsedRound <= 1.25f * safeTime_)
			return Result::Ignore;
		if (in.hiding && in.defenseAmbushExpired)
		{
			active_ = true;
			target_ = noise.position;
			area_ = noise.navArea;
			return Result::Attend;
		}
		if (noise.timestampSeconds != candidate_.timestampSeconds || noise.timestampTick != candidate_.timestampTick ||
			noise.position.x != candidate_.position.x || noise.position.y != candidate_.position.y ||
			noise.position.z != candidate_.position.z || noise.navArea != candidate_.navArea)
		{
			candidate_ = noise;
			travelChecked_ = false;
		}
		if (travelChecked_ || in.now - lastCheck_ < .25f)
			return Result::Ignore;
		const float dx = noise.position.x - in.origin.x, dy = noise.position.y - in.origin.y,
					dz = noise.position.z - in.origin.z;
		float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
		if (!std::isfinite(distance))
			return Result::Ignore;
		if (std::fabs(dz) > 120.0f)
		{
			if (!in.travelKnown)
				return Result::NeedTravel;
			travelChecked_ = true;
			lastCheck_ = in.now;
			if (!std::isfinite(in.travelDistance) || in.travelDistance < 0 || in.travelDistance > 1500.0f)
				return Result::Ignore;
			distance = (std::max)(distance, in.travelDistance);
		}
		lastCheck_ = in.now;
		if (in.hiding && distance > 100.0f + 400.0f * in.aggression)
			return Result::Ignore;
		float chance = 1.0f - distance / 2000.0f;
		if (in.friends >= 3)
			chance -= (std::min)(.5f, .05f * in.friends);
		const auto draw = rng.nextFloat(
			RandomRequest::floatRequest("noise.investigate", {in.actor.slot, in.actor.generation}, in.timing, 0, 1));
		if (draw.status != RandomStatus::Ok || !std::isfinite(draw.value) || draw.value < 0 || draw.value > 1 ||
			draw.value > chance)
			return Result::Ignore;
		if (active_)
		{
			const float ox = target_.x - in.origin.x, oy = target_.y - in.origin.y, oz = target_.z - in.origin.z;
			if (std::sqrt(ox * ox + oy * oy + oz * oz) <= distance + 100.0f)
				return Result::Ignore;
		}
		active_ = true;
		target_ = noise.position;
		area_ = noise.navArea;
		return Result::Attend;
	}

private:
	bool initialized_ = false, active_ = false, travelChecked_ = false;
	world::ActorKey actor_{};
	std::uint32_t map_ = 0, round_ = 0, area_ = 0;
	std::uint64_t navRevision_ = 0;
	float lastNow_ = 0, lastCheck_ = 0, safeTime_ = 0;
	world::NoiseMemory candidate_{};
	world::WorldPosition target_{};
};
}
}
#endif
