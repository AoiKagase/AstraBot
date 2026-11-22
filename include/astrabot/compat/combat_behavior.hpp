#ifndef ASTRABOT_COMPAT_COMBAT_BEHAVIOR_HPP
#define ASTRABOT_COMPAT_COMBAT_BEHAVIOR_HPP

#include "astrabot/combat/weapon_state.hpp"
#include "astrabot/compat/profile_catalog.hpp"
#include <array>
#include <cmath>
#include <functional>
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
	world::EntityKey sourceEntity{};
	float speedSquared = 0.0f;
	std::uint8_t visibleParts = world::VisibleNone;
	bool crouching = false;
	bool reloading = false;
};

struct NativeThreatAttention
{
	world::ActorKey previousActor{};
	world::EntityKey previousSource{};
	float elapsed = 99999.9f;

	bool isPrevious(const ThreatCandidate &candidate) const
	{
		return previousActor.isValid() && previousActor == candidate.actor && previousSource == candidate.sourceEntity;
	}

	bool skipsDraw(const ThreatCandidate &candidate) const
	{
		return isPrevious(candidate) || candidate.speedSquared > 200.0f * 200.0f;
	}
};

inline float nativeThreatNoticeChance(const ThreatCandidate &candidate, float elapsed, float skill)
{
	float coverage = 0.0f;
	if ((candidate.visibleParts & world::VisibleChest) != 0U)
		coverage += 40.0f;
	if ((candidate.visibleParts & world::VisibleHead) != 0U)
		coverage += 10.0f;
	if ((candidate.visibleParts & world::VisibleLeftSide) != 0U)
		coverage += 20.0f;
	if ((candidate.visibleParts & world::VisibleRightSide) != 0U)
		coverage += 20.0f;
	if ((candidate.visibleParts & world::VisibleFeet) != 0U)
		coverage += 10.0f;

	const float rangeModifier = std::fmax(0.0f, std::fmin(1.0f, (candidate.distance - 300.0f) / 700.0f));
	const bool walking = candidate.speedSquared > 30.0f * 30.0f;
	const float closeChance = candidate.crouching ? (walking ? 90.0f : 80.0f) : 100.0f;
	const float farChance = walking ? (candidate.crouching ? 60.0f : 75.0f) : (candidate.crouching ? 5.0f : 10.0f);
	const float disposition = closeChance + (farChance - closeChance) * rangeModifier;
	float chance = disposition * coverage / 100.0f;
	chance *= 0.5f + 0.5f * skill;
	chance *= elapsed / 0.25f;
	return std::fmax(0.1f, chance);
}

class ReactionQueue
{
public:
	ReactionQueue() : samples_(), observer_(), frame_(), head_(0U), count_(0U), lastTime_(0.0f), nativeTiming_(false)
	{
	}

	void reset()
	{
		*this = ReactionQueue();
	}

	void resetForLife()
	{
		// CSBot resets the queue on spawn while the bot's attention interval keeps running.
		const auto attention = attentionTime_;
		reset();
		attentionTime_ = attention;
	}

	NativeThreatAttention nativeAttention(world::ActorKey observer, world::FrameIdentity frame, float now) const
	{
		NativeThreatAttention result{};
		if (observer_.isValid() && (!(observer == observer_) || frame.mapGeneration != frame_.mapGeneration))
			return result;
		if (attentionTime_ > 0.0f)
			result.elapsed = now - attentionTime_;
		if (nativeTiming_ && count_ && frame.roundGeneration == frame_.roundGeneration && now >= lastTime_)
		{
			const auto &previous = samples_[(head_ + kNativeReactionHistoryCapacity - 1U) % kNativeReactionHistoryCapacity];
			if (previous.hasThreat)
			{
				result.previousActor = previous.threat.actor;
				result.previousSource = previous.threat.sourceEntity;
			}
		}
		return result;
	}

	void forget(world::EntityKey source)
	{
		if (!source.isValid())
			return;
		for (auto &sample : samples_)
			if (sample.hasThreat && sample.threat.sourceEntity == source)
				sample.hasThreat = false;
	}

	bool update(world::ActorKey observer, world::FrameIdentity frame, float now, float reactionSeconds,
		const ThreatCandidate *candidates, std::size_t count, bool blind, ThreatCandidate *selected,
		bool nativeTiming = false, bool (*completeSample)(void *, ThreatCandidate &) = nullptr, void *context = nullptr)
	{
// Keep literal evaluation precision on x87; each macro is undefined below.
#define ASTRABOT_COMBAT_BEHAVIOR_REACTION_TIME_TOLERANCE_SECONDS 0.00001f
		if (selected == nullptr)
			return false;
		*selected = {};
		const float minimumReaction = nativeTiming ? ProfileRecord::kMinimumReactionSeconds : 0.0f;
		if (!observer.isValid() || !frame.isValid() || !std::isfinite(now) || now < 0.0f ||
			!std::isfinite(reactionSeconds) || reactionSeconds < minimumReaction || (count && !candidates))
			return false;
		if (observer_.isValid() && (!(observer == observer_) || frame.mapGeneration != frame_.mapGeneration))
			reset();
		else if (!(observer == observer_) ||
			frame.roundGeneration != frame_.roundGeneration || now < lastTime_ || nativeTiming != nativeTiming_)
			resetForLife();
		else if (count_ && frame.tick <= frame_.tick)
			return false;
		observer_ = observer;
		frame_ = frame;
		lastTime_ = now;
		nativeTiming_ = nativeTiming;
		// The native full update returns before touching the ring while blinded.
		if (nativeTiming && blind)
			return false;
		Sample sample = {};
		sample.time = now;
		if (!blind)
		{
			const auto attention = nativeTiming ? nativeAttention(observer, frame, now) : NativeThreatAttention{};
			const ThreatCandidate *previous = nullptr;
			const ThreatCandidate *nearest = nullptr;
			for (std::size_t i = 0; i < count; ++i)
			{
				const ThreatCandidate &c = candidates[i];
				if (!c.actor.isValid() || c.actor == observer || !std::isfinite(c.distance) || c.distance < 0.0f ||
					!std::isfinite(c.visiblePoint.x) || !std::isfinite(c.visiblePoint.y) ||
					!std::isfinite(c.visiblePoint.z))
					continue;
				if (nativeTiming)
				{
					if (attention.isPrevious(c))
						previous = &c;
					if (!nearest || c.distance < nearest->distance)
						nearest = &c;
					continue;
				}
				if (!sample.hasThreat || (c.attackingObserver && !sample.threat.attackingObserver) ||
					(c.attackingObserver == sample.threat.attackingObserver && c.distance < sample.threat.distance))
				{
					sample.threat = c;
					sample.hasThreat = true;
				}
			}
			// The current threat keeps the next draw exemption unless another noticed enemy is significantly closer.
			if (nearest)
			{
				sample.hasThreat = true;
				sample.threat = previous && !(nearest->distance < previous->distance * 0.7f) ? *previous : *nearest;
			}
		}
		if (sample.hasThreat && completeSample && !completeSample(context, sample.threat))
			return false;
		const std::size_t capacity = nativeTiming ? kNativeReactionHistoryCapacity : samples_.size();
		samples_[head_] = sample;
		head_ = (head_ + 1U) % capacity;
		if (count_ < capacity)
			++count_;
		if (blind)
			return false;
		if (nativeTiming)
		{
			attentionTime_ = now;
			constexpr float interval = runtime::BotTimingScheduler::kFullUpdateInterval;
			constexpr float maxDelay = interval * kNativeReactionHistoryCapacity - 0.01f;
			const int steps = static_cast<int>(std::fmin(reactionSeconds, maxDelay) / interval + 0.5f);
			// The native clamp can round to a full turn and select this update's slot.
			const auto age = static_cast<std::size_t>(steps) % capacity;
			if (age >= count_)
				return false;
			const auto &historical = samples_[(head_ + capacity - 1U - age) % capacity];
			if (!historical.hasThreat)
				return false;
			// The runtime verifies entity identity before using a delayed contact.
			*selected = historical.threat;
			return true;
		}
		// Select the most recent complete recognition snapshot old enough for the profile.
		for (std::size_t age = 0; age < count_; ++age)
		{
			const Sample &historical = samples_[(head_ + samples_.size() - 1U - age) % samples_.size()];
			if (historical.time <= now - reactionSeconds + ASTRABOT_COMBAT_BEHAVIOR_REACTION_TIME_TOLERANCE_SECONDS)
			{
				if (!historical.hasThreat)
					return false;
				for (std::size_t i = 0; i < count; ++i)
					if (candidates[i].actor == historical.threat.actor)
					{
						*selected = historical.threat;
						selected->sourceEntity = candidates[i].sourceEntity;
						return true;
					}
				return false;
			}
		}
		return false;
	}
#undef ASTRABOT_COMBAT_BEHAVIOR_REACTION_TIME_TOLERANCE_SECONDS

private:
	static constexpr unsigned kReactionHistoryCapacity = 64U;
	static constexpr unsigned kNativeReactionHistoryCapacity = 20U;
	struct Sample
	{
		float time;
		bool hasThreat;
		ThreatCandidate threat;
	};
	std::array<Sample, kReactionHistoryCapacity> samples_;
	world::ActorKey observer_;
	world::FrameIdentity frame_;
	std::size_t head_, count_;
	float lastTime_;
	bool nativeTiming_;
	float attentionTime_ = -1.0f;
};

struct AimStep
{
	float yaw;
	float pitch;
	bool aligned;
};

inline world::WorldPosition combatAimPoint(world::WorldPosition enemy, world::WorldPosition observer,
	std::uint8_t visibleParts, std::uint16_t weapon, float skill, float feetOffset, bool recoilKnown, float accuracy)
{
	constexpr int kP228 = 1;
	constexpr int kElite = 10;
	constexpr int kFiveSeven = 11;
	constexpr int kUsp = 16;
	constexpr int kGlock18 = 17;
	constexpr int kDeagle = 26;
	constexpr int kScout = 3;
	constexpr int kSg550 = 13;
	constexpr int kAwp = 18;
	constexpr int kG3sg1 = 24;
	constexpr int kXm1014 = 5;
	constexpr int kM3 = 21;
	constexpr int kM249 = 20;
	constexpr float kHeadAimMinimumSkill = .8f;
	constexpr float kChestAimAccuracyThreshold = .4f;
	constexpr float kChestAimFeetScale = .25f;
	constexpr float kHeadOccludedFeetScale = .5f;
#define ASTRABOT_COMBAT_BEHAVIOR_MINIMUM_SIDE_AIM_RANGE .001f
	constexpr int kSideAimOffset = 16;
	constexpr float kFeetAimScale = 2.0f;
	const bool pistol = weapon == kP228 || weapon == kElite || weapon == kFiveSeven || weapon == kUsp ||
		weapon == kGlock18 || weapon == kDeagle;
	const bool sniper = weapon == kScout || weapon == kSg550 || weapon == kAwp || weapon == kG3sg1;
	const bool chest = (visibleParts & world::VisibleChest) != 0U;
	if (chest &&
		(weapon == kAwp || weapon == kXm1014 || weapon == kM3 || weapon == kM249 ||
			skill < kHeadAimMinimumSkill ||
			(recoilKnown && accuracy > kChestAimAccuracyThreshold && !pistol && !sniper)))
		enemy.z -= feetOffset * kChestAimFeetScale;
	else if ((visibleParts & world::VisibleHead) == 0U)
	{
		if (chest)
			enemy.z -= feetOffset * kHeadOccludedFeetScale;
		else if ((visibleParts & (world::VisibleLeftSide | world::VisibleRightSide)) != 0U)
		{
			const float dx = enemy.x - observer.x, dy = enemy.y - observer.y;
			const float length = std::hypot(dx, dy);
			const float side = (visibleParts & world::VisibleLeftSide) != 0U ? 1.0f : -1.0f;
			if (length > ASTRABOT_COMBAT_BEHAVIOR_MINIMUM_SIDE_AIM_RANGE)
			{
				enemy.x -= side * dy / length * kSideAimOffset;
				enemy.y += side * dx / length * kSideAimOffset;
			}
			enemy.z -= feetOffset * kHeadOccludedFeetScale;
		}
		else
			enemy.z -= feetOffset * kFeetAimScale;
	}
	return enemy;
}
#undef ASTRABOT_COMBAT_BEHAVIOR_MINIMUM_SIDE_AIM_RANGE

inline bool combatAimOnTarget(world::WorldPosition point, world::WorldPosition origin, float yaw, bool sniper)
{
#define ASTRABOT_COMBAT_BEHAVIOR_MINIMUM_AIM_RANGE .001f
#define ASTRABOT_COMBAT_BEHAVIOR_DEGREES_TO_RADIANS .01745329251994329577f
	constexpr float kSniperAimRadius = 16.0f;
	constexpr float kFirearmAimRadius = 32.0f;
	const float dx = point.x - origin.x, dy = point.y - origin.y;
	const float range = std::hypot(dx, dy);
	if (!std::isfinite(range) || range <= ASTRABOT_COMBAT_BEHAVIOR_MINIMUM_AIM_RANGE || !std::isfinite(yaw))
		return false;
	const float radians = yaw * ASTRABOT_COMBAT_BEHAVIOR_DEGREES_TO_RADIANS;
	const float dot = (dx * std::cos(radians) + dy * std::sin(radians)) / range;
	return dot > std::cos(std::atan((sniper ? kSniperAimRadius : kFirearmAimRadius) / range));
}
#undef ASTRABOT_COMBAT_BEHAVIOR_MINIMUM_AIM_RANGE
#undef ASTRABOT_COMBAT_BEHAVIOR_DEGREES_TO_RADIANS

struct AimSpringState
{
	float yawVelocity;
	float pitchVelocity;
};
inline float normalizedAimAngle(float angle)
{
	constexpr float kHalfTurnDegrees = 180.0f;
	constexpr float kFullTurnDegrees = 360.0f;
	angle = std::fmod(angle + kHalfTurnDegrees, kFullTurnDegrees);
	if (angle < 0.0f)
		angle += kFullTurnDegrees;
	return angle - kHalfTurnDegrees;
}

inline AimStep trackAim(float yaw, float pitch, float desiredYaw, float desiredPitch, float elapsed, float skill,
	AimSpringState *persistent = nullptr, bool attacking = true)
{
	constexpr float kAttackingStiffness = 300.0f;
	constexpr float kIdleStiffness = 200.0f;
	constexpr float kAttackingDamping = 30.0f;
	constexpr float kIdleDamping = 25.0f;
	constexpr float kPitchStiffnessScale = 2.0f;
	constexpr float kMaximumAcceleration = 3000.0f;
	constexpr float kMinimumAcceleration = -3000.0f;
	constexpr float kYawSnapDegrees = 1.0f;
	constexpr float kMinimumPitchDegrees = -89.0f;
	constexpr float kMaximumPitchDegrees = 89.0f;
	constexpr float kAlignmentToleranceDegrees = 1.0f;
	AimStep result = {yaw, pitch, false};
	if (!std::isfinite(yaw) || !std::isfinite(pitch) || !std::isfinite(desiredYaw) || !std::isfinite(desiredPitch) ||
		!std::isfinite(elapsed) || elapsed < 0.0f || !std::isfinite(skill) || skill < 0.0f || skill > 1.0f)
		return result;
	AimSpringState temporary = {};
	AimSpringState &state = persistent ? *persistent : temporary;
	const float stiffness = attacking ? kAttackingStiffness : kIdleStiffness;
	const float damping = attacking ? kAttackingDamping : kIdleDamping;
	const float dy = normalizedAimAngle(desiredYaw - yaw);
	const float dp = normalizedAimAngle(desiredPitch - pitch);
	float yawAccel = stiffness * dy - damping * state.yawVelocity;
	float pitchAccel = kPitchStiffnessScale * stiffness * dp - damping * state.pitchVelocity;
	yawAccel = yawAccel > kMaximumAcceleration ? kMaximumAcceleration
		: yawAccel < kMinimumAcceleration	   ? kMinimumAcceleration
											   : yawAccel;
	pitchAccel = pitchAccel > kMaximumAcceleration ? kMaximumAcceleration
		: pitchAccel < kMinimumAcceleration		   ? kMinimumAcceleration
												   : pitchAccel;
	if (std::fabs(dy) < kYawSnapDegrees)
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
	if (result.pitch < kMinimumPitchDegrees)
		result.pitch = kMinimumPitchDegrees;
	if (result.pitch > kMaximumPitchDegrees)
		result.pitch = kMaximumPitchDegrees;
	result.aligned = std::fabs(normalizedAimAngle(desiredYaw - result.yaw)) <= kAlignmentToleranceDegrees &&
		std::fabs(desiredPitch - result.pitch) <= kAlignmentToleranceDegrees;
	return result;
}

// One native yaw/pitch spring per actor. A higher-priority command look can
// replace this tick's provisional goal, starting from the same pre-Upkeep state.
// It never adds another integration to the already integrated result.
using SharedLookStep = std::function<AimStep(float, float, float, float, float, bool, unsigned)>;

struct SharedLookState
{
	AimSpringState spring{};
	float timestamp = -1.0f;
	float startYaw = 0.0f, startPitch = 0.0f;
	AimSpringState startSpring{};
	AimStep result{};
	unsigned priority = 0U;

	AimStep step(float now, float yaw, float pitch, float desiredYaw, float desiredPitch,
		bool attacking, unsigned ownerPriority)
	{
		if (!std::isfinite(now) || !std::isfinite(yaw) || !std::isfinite(pitch) ||
			!std::isfinite(desiredYaw) || !std::isfinite(desiredPitch) ||
			now < 0.0f || (timestamp >= 0.0f && now < timestamp))
			return {yaw, pitch, false};
		if (timestamp != now)
		{
			timestamp = now;
			startYaw = yaw;
			startPitch = pitch;
			startSpring = spring;
		}
		else if (ownerPriority <= priority)
			return result;
		spring = startSpring;
		priority = ownerPriority;
		result = trackAim(startYaw, startPitch, desiredYaw, desiredPitch, 1.0f / 30.0f, 1.0f, &spring, attacking);
		return result;
	}
};

inline bool weaponEngagementAllowed(combat::WeaponKind kind, float distance)
{
	constexpr float kMaximumKnifeEngagementRange = 75.0f;
	if (!std::isfinite(distance) || distance < 0.0f)
		return false;
	if (kind == combat::WeaponKind::Unknown || kind == combat::WeaponKind::Grenade)
		return false;
	return kind != combat::WeaponKind::Knife || distance <= kMaximumKnifeEngagementRange;
}
} // namespace compat
} // namespace astrabot
#endif
