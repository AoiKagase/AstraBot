#ifndef ASTRABOT_COMPAT_NOISE_LOOK_HPP
#define ASTRABOT_COMPAT_NOISE_LOOK_HPP
#include "astrabot/compat/combat_behavior.hpp"
#include "astrabot/compat/random_source.hpp"
#include "astrabot/world/world_snapshot.hpp"
#include "astrabot/runtime/bot_timing_scheduler.hpp"
#include <cstring>
namespace astrabot
{
namespace compat
{
enum class NoiseLookKind
{
	None,
	Ordinary,
	Startle
};
struct NoiseLookInput
{
	world::ActorKey actor{};
	world::FrameIdentity frame{};
	float now = 0, noiseTime = -1, reactionSeconds = 0, distanceSquared = 0;
	bool hasNoise = false, highPriority = false, visibleEnemy = false, hiding = false, clearLine = false;
	int enemiesRemaining = 0;
};
inline bool isNearbyEnemyGunfire(const NoiseLookInput &input)
{
	constexpr float kStartleRange = 1000.0f;
	return input.highPriority && input.distanceSquared >= 0.0f &&
		input.distanceSquared <= kStartleRange * kStartleRange;
}
inline NoiseLookKind noiseLookKind(const NoiseLookInput &input, float inhibitedUntil)
{
	if (!input.hasNoise || input.visibleEnemy || !std::isfinite(input.now) || !std::isfinite(input.noiseTime) ||
		input.noiseTime < 0 || input.now < input.noiseTime || input.now - input.noiseTime > 0.5f ||
		input.now < inhibitedUntil)
		return NoiseLookKind::None;
	if (input.clearLine && isNearbyEnemyGunfire(input))
		return NoiseLookKind::Startle;
	if (!std::isfinite(input.reactionSeconds) || input.reactionSeconds < 0 ||
		input.now - input.noiseTime < input.reactionSeconds)
		return NoiseLookKind::None;
	return NoiseLookKind::Ordinary;
}
inline bool isStandardShotSample(const char *sample)
{
	if (!sample)
		return false;
	// Public standard sample identities; unknown/custom names do not establish a shot.
	const char *names[] = {"weapons/ak47-1.wav", "weapons/ak47-2.wav", "weapons/aug-1.wav", "weapons/awp1.wav",
		"weapons/deagle-1.wav", "weapons/deagle-2.wav", "weapons/elite_fire.wav", "weapons/famas-1.wav",
		"weapons/famas-2.wav", "weapons/fiveseven-1.wav", "weapons/g3sg1-1.wav", "weapons/galil-1.wav",
		"weapons/galil-2.wav", "weapons/glock18-1.wav", "weapons/glock18-2.wav", "weapons/m249-1.wav",
		"weapons/m249-2.wav", "weapons/m3-1.wav", "weapons/m4a1-1.wav", "weapons/m4a1_unsil-1.wav",
		"weapons/m4a1_unsil-2.wav", "weapons/mac10-1.wav", "weapons/mp5-1.wav", "weapons/mp5-2.wav",
		"weapons/p228-1.wav", "weapons/p90-1.wav", "weapons/scout_fire-1.wav", "weapons/sg550-1.wav",
		"weapons/sg552-1.wav", "weapons/sg552-2.wav", "weapons/tmp-1.wav", "weapons/tmp-2.wav", "weapons/ump45-1.wav",
		"weapons/usp1.wav", "weapons/usp2.wav", "weapons/usp_unsil-1.wav", "weapons/xm1014-1.wav"};
	for (const auto *name : names)
		if (std::strcmp(sample, name) == 0)
			return true;
	return false;
}
class NoiseLookPlanner
{
public:
	void reset()
	{
		*this = NoiseLookPlanner{};
	}
	void clearLook()
	{
		active_ = false;
		damagePanic_ = false;
		alignedAt_ = -1;
		spring_ = {};
	}
	void synchronize(world::ActorKey actor, world::FrameIdentity frame, float now)
	{
		if (!(actor == actor_) || frame.mapGeneration != frame_.mapGeneration ||
			frame.roundGeneration != frame_.roundGeneration || !std::isfinite(now) || now < lastTime_)
			reset();
		actor_ = actor;
		frame_ = frame;
		lastTime_ = now;
	}
	void lookAtThreat(world::ActorKey actor, const world::FrameIdentity &frame, float now,
		const world::WorldPosition &point)
	{
		synchronize(actor, frame, now);
		clearLook();
		point_ = point;
		active_ = true;
		highPriority_ = true;
		duration_ = 0.0f;
		clearIfClose_ = false;
	}
	void lookAtDamagePanic(world::ActorKey actor, const world::FrameIdentity &frame, float now,
		const world::WorldPosition &point)
	{
		synchronize(actor, frame, now);
		if (!active_ || std::fabs(point.x - point_.x) > 10.0f || std::fabs(point.y - point_.y) > 10.0f ||
			std::fabs(point.z - point_.z) > 10.0f)
			lookAtThreat(actor, frame, now, point);
		damagePanic_ = true;
		highPriority_ = true;
		duration_ = 0.0f;
		clearIfClose_ = false;
	}
	bool damagePanic() const { return damagePanic_; }
	bool active() const
	{
		return active_;
	}
	bool inhibited(float now) const
	{
		return now < inhibitedUntil_;
	}
	float inhibitedUntil() const
	{
		return inhibitedUntil_;
	}
	bool request(const NoiseLookInput &input, NoiseLookKind kind, const world::WorldPosition &point,
		ICompatibilityRandomSource &random, RandomTimingContext timing)
	{
		if (kind == NoiseLookKind::None || !input.actor.isValid() || !input.frame.isValid() ||
			!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z))
			return false;
		const float lower = kind == NoiseLookKind::Startle ? 2.0f
			: input.hiding								   ? (input.enemiesRemaining == 1 ? 5.0f : 2.0f)
														   : 1.0f;
		const float upper = kind == NoiseLookKind::Startle ? 4.0f
			: input.hiding								   ? (input.enemiesRemaining == 1 ? 15.0f : 5.0f)
														   : 2.0f;
		const auto draw = random.nextFloat(RandomRequest::floatRequest(
			kind == NoiseLookKind::Startle ? "NoiseLook.StartleInhibit" : "NoiseLook.Duration",
			{input.actor.slot, input.actor.generation}, timing, lower, upper));
		if (draw.status != RandomStatus::Ok || !std::isfinite(draw.value) || draw.value < lower || draw.value > upper)
			return false;
		const bool high = kind == NoiseLookKind::Startle || input.highPriority;
		if (active_ && highPriority_ && !high)
			return false;
		if (!active_ || std::fabs(point.x - point_.x) > 10.0f || std::fabs(point.y - point_.y) > 10.0f ||
			std::fabs(point.z - point_.z) > 10.0f)
		{
			alignedAt_ = -1;
			spring_ = {};
		}
		active_ = true;
		damagePanic_ = false;
		point_ = point;
		highPriority_ = high;
		duration_ = kind == NoiseLookKind::Startle ? 0.25f : draw.value;
		clearIfClose_ = kind != NoiseLookKind::Startle;
		if (kind == NoiseLookKind::Startle)
			inhibitedUntil_ = input.now + draw.value;
		return true;
	}
	bool update(float now, const world::WorldPosition &origin, const world::WorldPosition &eye, float pitch, float yaw,
		float *outPitch, float *outYaw)
	{
		if (!active_ || !outPitch || !outYaw || !std::isfinite(now) || now < lastTime_)
			return false;
		lastTime_ = now;
		const float ox = point_.x - origin.x, oy = point_.y - origin.y, oz = point_.z - origin.z;
		if ((clearIfClose_ && ox * ox + oy * oy + oz * oz < 100.0f * 100.0f) ||
			(alignedAt_ >= 0 && now - alignedAt_ > duration_))
		{
			clearLook();
			return false;
		}
		constexpr float kDegreesPerRadian = 57.29577951308232f;
		const float dx = point_.x - eye.x, dy = point_.y - eye.y, dz = point_.z - eye.z;
		const float desiredYaw = kDegreesPerRadian * std::atan2(dy, dx);
		const float desiredPitch = -kDegreesPerRadian * std::atan2(dz, std::hypot(dx, dy));
		const auto aim = trackAim(
			yaw, pitch, desiredYaw, desiredPitch, runtime::BotTimingScheduler::kCommandInterval, 1.0f, &spring_, false);
		*outPitch = aim.pitch;
		*outYaw = aim.yaw;
		if (alignedAt_ < 0 && std::fabs(normalizedAimAngle(desiredYaw - aim.yaw)) <= 5.0f &&
			std::fabs(normalizedAimAngle(desiredPitch - aim.pitch)) <= 5.0f)
			alignedAt_ = now;
		return true;
	}

private:
	world::ActorKey actor_{};
	world::FrameIdentity frame_{};
	world::WorldPosition point_{};
	AimSpringState spring_{};
	float lastTime_ = 0, alignedAt_ = -1, duration_ = 0, inhibitedUntil_ = 0;
	bool active_ = false, highPriority_ = false, clearIfClose_ = true, damagePanic_ = false;
};
} // namespace compat
} // namespace astrabot
#endif
