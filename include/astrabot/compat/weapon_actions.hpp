#ifndef ASTRABOT_COMPAT_WEAPON_ACTIONS_HPP
#define ASTRABOT_COMPAT_WEAPON_ACTIONS_HPP

#include "astrabot/world/world_snapshot.hpp"
#include "astrabot/compat/random_source.hpp"
#include "astrabot/compat/profile_catalog.hpp"
#include <cmath>
#include <cstdint>

namespace astrabot
{
namespace compat
{
inline bool nativeMagazineNeedsReload(int clip, unsigned capacity, float timeSinceEnemy,
	bool skilledAttack, bool awp)
{
	if (capacity == 0 || clip < 0)
		return false;
	if (clip == 0)
		return true;
	return !awp && !skilledAttack && timeSinceEnemy > 3.0f &&
		static_cast<float>(clip) / static_cast<float>(capacity) <= 0.6f;
}

struct KnownWeaponFlag
{
	bool known;
	bool value;
};

// Supplied by the adapter after sampling the ballistic arc with public traces.
// These booleans are receipts, never values inferred from sending an attack command.
struct GrenadeTrajectory
{
	bool known;
	bool lineClear;
	bool hullClear;
	bool landingSafe;
};

struct WeaponActionInput
{
	static constexpr float kDefaultSecondaryRetrySeconds = 0.5f;
	static constexpr unsigned kDefaultMaximumSecondaryAttempts = 3U;
	world::ActorKey actor;
	world::FrameIdentity frame;
	float timestamp;
	std::uint16_t weaponId;
	bool clipKnown;
	std::uint16_t clip;
	bool fovKnown;
	float fieldOfView;
	float targetDistance;
	float reactionSeconds;
	bool targetVisible;
	bool aimAligned;
	bool stationary;
	bool requestFire;
	bool attacking;
	bool defusing;
	KnownWeaponFlag reloading;
	KnownWeaponFlag resumeZoom;
	KnownWeaponFlag silenced;
	KnownWeaponFlag burstMode;
	KnownWeaponFlag shield;
	bool requestSilencer;
	bool desiredSilencer;
	bool requestBurst;
	bool desiredBurst;
	float secondsSinceEnemy;
	bool noNearbyEnemies;
	KnownWeaponFlag enemyFacingObserver;
	bool knifeRollKnown;
	float knifeRoll;
	GrenadeTrajectory trajectory;
	bool requestGrenade;
	KnownWeaponFlag secondaryReady = {};
	// Transport retry policy, not an inferred GameDLL weapon cooldown.
	float secondaryRetrySeconds = kDefaultSecondaryRetrySeconds;
	std::uint8_t maximumSecondaryAttempts = kDefaultMaximumSecondaryAttempts;
	bool grenadeAimKnown = false;
	float grenadePitch = 0.0f;
	float grenadeYaw = 0.0f;
	bool reserveKnown = false;
	std::uint16_t reserve = 0U;
	std::uint32_t reserveObservationTick = 0U;
	bool pistolAvailable = true;
};

struct WeaponActionDecision
{
	bool primary;
	bool secondary;
	bool releasePrimary;
	bool stopMovement;
	bool switchToPistol;
	bool waitingForObservation;
	bool unavailableMode;
	bool secondaryTimedOut = false;
	bool hasRetainedGrenadeAim = false;
	float grenadePitch = 0.0f;
	float grenadeYaw = 0.0f;
	bool waitingForCadence = false;
};

struct FireCadenceContext
{
	float skill;
	ICompatibilityRandomSource *randomSource;
	RandomTimingContext randomTiming;
	float targetRange;
};

class WeaponActionPlanner
{
public:
	WeaponActionPlanner() = default;
	bool grenadeInProgress() const
	{
		return grenadeHeld_;
	}

	std::uint16_t currentWeaponId() const
	{
		return weaponId_;
	}

	float fireReadyTimestamp() const
	{
		return fireReadyTimestamp_;
	}

	bool rapidFiring() const
	{
		return rapidFiring_;
	}

	void stopRapidFire()
	{
		rapidFiring_ = false;
	}

	void reset()
	{
		*this = WeaponActionPlanner();
	}

	WeaponActionDecision update(const WeaponActionInput &input)
	{
		return updateInternal(input, nullptr);
	}

	WeaponActionDecision update(const WeaponActionInput &input, const FireCadenceContext &cadence)
	{
		return updateInternal(input, &cadence);
	}

private:
	static constexpr unsigned kP228 = 1U;
	static constexpr unsigned kHeGrenade = 4U;
	static constexpr unsigned kSmokeGrenade = 9U;
	static constexpr unsigned kUsp = 16U;
	static constexpr unsigned kGlock18 = 17U;
	static constexpr unsigned kFlashbang = 25U;
	static constexpr unsigned kKnife = 29U;
	WeaponActionDecision updateInternal(const WeaponActionInput &input, const FireCadenceContext *cadence)
	{
		constexpr unsigned kM249 = 20U;
		constexpr float kSniperPistolSwitchRange = 310.0f;
		constexpr float kMaximumFieldOfView = 180.0f;
		constexpr float kUnscopedFieldOfViewThreshold = 60.0f;
		constexpr float kMinimumScopeRangeWithoutPistol = 300.0f;
// Keep literal evaluation precision on x87; each macro is undefined below.
#define ASTRABOT_WEAPON_ACTIONS_SCOPE_SETTLE_SECONDS 0.2f
		constexpr float kKnifeAttackRange = 75.0f;
		constexpr float kMaximumKnifeRoll = 100.0f;
#define ASTRABOT_WEAPON_ACTIONS_KNIFE_SECONDARY_CHANCE 33.3f
#define ASTRABOT_WEAPON_ACTIONS_THINK_LATENCY_SECONDS .1f
		constexpr float kRapidPistolMinimumSkill = 0.75f;
#define ASTRABOT_WEAPON_ACTIONS_RAPID_PISTOL_MAXIMUM_RANGE 999999.9f
		constexpr float kSprayMaximumSkill = 0.5f;
		constexpr float kSprayRange = 400.0f;
		constexpr float kDistantFireRange = 800.0f;
#define ASTRABOT_WEAPON_ACTIONS_DISTANT_MINIMUM_FIRE_INTERVAL_SECONDS 0.3f
#define ASTRABOT_WEAPON_ACTIONS_MINIMUM_FIRE_INTERVAL_SECONDS 0.15f
#define ASTRABOT_WEAPON_ACTIONS_PISTOL_MAXIMUM_FIRE_INTERVAL_SECONDS 0.4f
#define ASTRABOT_WEAPON_ACTIONS_DISTANT_MAXIMUM_FIRE_INTERVAL_SECONDS 0.7f
		constexpr float kMaximumFireIntervalSeconds = 0.5f;
		WeaponActionDecision result = {};
		if (!input.actor.isValid() || !input.frame.isValid() || !std::isfinite(input.timestamp) ||
			input.timestamp < 0.0f || !std::isfinite(input.targetDistance) || input.targetDistance < 0.0f ||
			!std::isfinite(input.reactionSeconds) || input.reactionSeconds < ProfileRecord::kMinimumReactionSeconds)
		{
			return result;
		}
		if (!(input.actor == actor_) || input.frame.mapGeneration != frame_.mapGeneration ||
			input.frame.roundGeneration != frame_.roundGeneration || input.timestamp < timestamp_)
		{
			reset();
		}
		else if (frame_.isValid() && input.frame.tick <= frame_.tick)
		{
			return result;
		}
		else if (input.weaponId != weaponId_)
		{
			const float fireDeadline = fireReadyTimestamp_;
			reset();
			fireReadyTimestamp_ = fireDeadline;
		}
		actor_ = input.actor;
		frame_ = input.frame;
		weaponId_ = input.weaponId;
		timestamp_ = input.timestamp;

		if (isGrenade(input.weaponId))
		{
			return grenadeAction(input);
		}
		if (input.defusing || (input.reloading.known && input.reloading.value))
		{
			heldPrimary_ = false;
			return result;
		}
		if (!input.attacking && (input.requestSilencer || input.requestBurst))
		{
			return modeAction(input);
		}
		if (!input.requestFire || !input.targetVisible)
		{
			rapidFiring_ = false;
			heldPrimary_ = false;
			result.stopMovement = isSniper(input.weaponId) && input.targetVisible;
			return result;
		}
		if (isSniper(input.weaponId))
		{
			result.stopMovement = true;
			if (input.targetDistance < kSniperPistolSwitchRange && input.pistolAvailable)
			{
				result.switchToPistol = true;
				return result;
			}
			if (!input.fovKnown || !std::isfinite(input.fieldOfView) || input.fieldOfView <= 0.0f ||
				input.fieldOfView > kMaximumFieldOfView)
			{
				result.waitingForObservation = true;
				return result;
			}
			if (input.resumeZoom.known && input.resumeZoom.value)
			{
				scopeTimestamp_ = input.timestamp;
				result.waitingForObservation = true;
				return result;
			}
			// AttackState calls AdjustZoom only at NO_ZOOM, even beyond 1500 units.
			if (input.fieldOfView > kUnscopedFieldOfViewThreshold &&
				(input.pistolAvailable || input.targetDistance > kMinimumScopeRangeWithoutPistol))
			{
				requestSecondary(input, &result);
				if (result.secondary)
					scopeTimestamp_ = input.timestamp;
				if (result.secondaryTimedOut)
					result.switchToPistol = true;
				return result;
			}
			pendingSecondary_ = false;
			secondaryAttempts_ = 0U;
			if (!input.stationary ||
				input.timestamp - scopeTimestamp_ <
					ASTRABOT_WEAPON_ACTIONS_SCOPE_SETTLE_SECONDS + input.reactionSeconds)
			{
				return result;
			}
		}
		if (!input.aimAligned)
		{
			return result;
		}
		if (cadence &&
			(!std::isfinite(cadence->skill) || cadence->skill < 0.0f || cadence->skill > 1.0f ||
				!std::isfinite(cadence->targetRange) || cadence->targetRange < 0.0f))
		{
			result.unavailableMode = true;
			return result;
		}
		if (cadence && input.timestamp <= fireReadyTimestamp_)
		{
			result.waitingForCadence = true;
			heldPrimary_ = false;
			return result;
		}
		if (input.weaponId == kKnife)
		{
			if (input.targetDistance >= kKnifeAttackRange)
			{
				return result;
			}
			if (input.enemyFacingObserver.known && !input.enemyFacingObserver.value)
			{
				result.secondary = true;
			}
			else if (input.knifeRollKnown && std::isfinite(input.knifeRoll) && input.knifeRoll >= 0.0f &&
				input.knifeRoll <= kMaximumKnifeRoll)
			{
				result.secondary = input.knifeRoll < ASTRABOT_WEAPON_ACTIONS_KNIFE_SECONDARY_CHANCE;
				result.primary = !result.secondary;
			}
			else
			{
				result.unavailableMode = true;
			}
			if (cadence && (result.primary || result.secondary))
				fireReadyTimestamp_ = input.timestamp - ASTRABOT_WEAPON_ACTIONS_THINK_LATENCY_SECONDS;
			return result;
		}
		if (!isFirearm(input.weaponId))
		{
			result.unavailableMode = true;
			return result;
		}
		if (!input.clipKnown || input.clip == 0U)
		{
			result.waitingForObservation = !input.clipKnown;
			return result;
		}
		if (isPistol(input.weaponId) && heldPrimary_ && clipKnown_ && input.clip < clip_ &&
			!(cadence && cadence->skill > kRapidPistolMinimumSkill &&
				cadence->targetRange < ASTRABOT_WEAPON_ACTIONS_RAPID_PISTOL_MAXIMUM_RANGE))
		{
			result.releasePrimary = true;
			heldPrimary_ = false;
		}
		else
		{
			result.primary = true;
			heldPrimary_ = true;
		}
		clipKnown_ = input.clipKnown;
		clip_ = input.clip;
		if (cadence && result.primary)
		{
			float delay = 0.0f;
			const bool pistol = isPistol(input.weaponId);
			const bool spray = !pistol &&
				(cadence->skill < kSprayMaximumSkill || cadence->targetRange < kSprayRange || input.weaponId == kM249);
			const bool rapidPistol = pistol && cadence->skill > kRapidPistolMinimumSkill &&
				cadence->targetRange < ASTRABOT_WEAPON_ACTIONS_RAPID_PISTOL_MAXIMUM_RANGE;
			rapidFiring_ = rapidPistol;
			if (!spray && !rapidPistol)
			{
				const bool distant = !pistol && !isSniper(input.weaponId) && cadence->targetRange > kDistantFireRange;
				const float lower = distant ? ASTRABOT_WEAPON_ACTIONS_DISTANT_MINIMUM_FIRE_INTERVAL_SECONDS
											: ASTRABOT_WEAPON_ACTIONS_MINIMUM_FIRE_INTERVAL_SECONDS;
				const float upper = pistol ? ASTRABOT_WEAPON_ACTIONS_PISTOL_MAXIMUM_FIRE_INTERVAL_SECONDS
					: distant			   ? ASTRABOT_WEAPON_ACTIONS_DISTANT_MAXIMUM_FIRE_INTERVAL_SECONDS
										   : kMaximumFireIntervalSeconds;
				if (!cadence->randomSource)
				{
					result.primary = false;
					result.unavailableMode = true;
					heldPrimary_ = false;
					return result;
				}
				const auto draw = cadence->randomSource->nextFloat(RandomRequest::floatRequest("Combat.FireInterval",
					{input.actor.slot, input.actor.generation}, cadence->randomTiming, lower, upper));
				if (draw.status != RandomStatus::Ok || !std::isfinite(draw.value) || draw.value < lower ||
					draw.value > upper)
				{
					result.primary = false;
					result.unavailableMode = true;
					heldPrimary_ = false;
					return result;
				}
				delay = draw.value;
			}
			// CSBot subtracts the fixed full-think latency, not the observed frame delta.
			fireReadyTimestamp_ = input.timestamp + (delay - ASTRABOT_WEAPON_ACTIONS_THINK_LATENCY_SECONDS);
		}
		return result;
	}
#undef ASTRABOT_WEAPON_ACTIONS_SCOPE_SETTLE_SECONDS
#undef ASTRABOT_WEAPON_ACTIONS_KNIFE_SECONDARY_CHANCE
#undef ASTRABOT_WEAPON_ACTIONS_THINK_LATENCY_SECONDS
#undef ASTRABOT_WEAPON_ACTIONS_RAPID_PISTOL_MAXIMUM_RANGE
#undef ASTRABOT_WEAPON_ACTIONS_DISTANT_MINIMUM_FIRE_INTERVAL_SECONDS
#undef ASTRABOT_WEAPON_ACTIONS_MINIMUM_FIRE_INTERVAL_SECONDS
#undef ASTRABOT_WEAPON_ACTIONS_PISTOL_MAXIMUM_FIRE_INTERVAL_SECONDS
#undef ASTRABOT_WEAPON_ACTIONS_DISTANT_MAXIMUM_FIRE_INTERVAL_SECONDS

public:
	static bool isSniper(std::uint16_t id)
	{
		constexpr unsigned kScout = 3U;
		constexpr unsigned kSg550 = 13U;
		constexpr unsigned kAwp = 18U;
		constexpr unsigned kG3sg1 = 24U;
		return id == kScout || id == kSg550 || id == kAwp || id == kG3sg1;
	}

	static bool isPistol(std::uint16_t id)
	{
		constexpr unsigned kElite = 10U;
		constexpr unsigned kFiveSeven = 11U;
		constexpr unsigned kDeagle = 26U;
		return id == kP228 || id == kElite || id == kFiveSeven || id == kUsp || id == kGlock18 || id == kDeagle;
	}

	static bool isGrenade(std::uint16_t id)
	{
		return id == kHeGrenade || id == kSmokeGrenade || id == kFlashbang;
	}

	static bool isFirearm(std::uint16_t id)
	{
		constexpr unsigned kShield = 2U;
		constexpr unsigned kC4 = 6U;
		constexpr unsigned kP90 = 30U;
		return id >= kP228 && id <= kP90 && id != kShield && id != kHeGrenade && id != kC4 && id != kSmokeGrenade &&
			id != kFlashbang && id != kKnife;
	}

private:
	WeaponActionDecision modeAction(const WeaponActionInput &input)
	{
		constexpr unsigned kFamas = 15U;
		constexpr unsigned kM4a1 = 22U;
		constexpr float kSilencerEnemyQuietSeconds = 3.5f;
		WeaponActionDecision result = {};
		const bool silencer = input.requestSilencer && (input.weaponId == kUsp || input.weaponId == kM4a1);
		const bool burst = input.requestBurst && (input.weaponId == kFamas || input.weaponId == kGlock18);
		const KnownWeaponFlag current = silencer ? input.silenced : input.burstMode;
		const bool desired = silencer ? input.desiredSilencer : input.desiredBurst;
		if ((!silencer && !burst) || !current.known || !input.reloading.known || (silencer && !input.shield.known))
		{
			result.unavailableMode = true;
			return result;
		}
		if (current.value == desired)
		{
			pendingSecondary_ = false;
			secondaryAttempts_ = 0U;
			return result;
		}
		if (input.reloading.value || !input.noNearbyEnemies ||
			(silencer &&
				(!std::isfinite(input.secondsSinceEnemy) || input.secondsSinceEnemy < kSilencerEnemyQuietSeconds ||
					input.shield.value)))
		{
			return result;
		}
		requestSecondary(input, &result);
		return result;
	}

	void requestSecondary(const WeaponActionInput &input, WeaponActionDecision *result)
	{
		if (!std::isfinite(input.secondaryRetrySeconds) || input.secondaryRetrySeconds <= 0.0f ||
			input.maximumSecondaryAttempts == 0U)
		{
			result->unavailableMode = true;
			return;
		}
		const float deadline =
			input.secondaryRetrySeconds * (static_cast<float>(input.maximumSecondaryAttempts) + 1.0f);
		if ((secondaryAttempts_ >= input.maximumSecondaryAttempts &&
				input.timestamp - secondarySentAt_ >= input.secondaryRetrySeconds) ||
			(secondaryAttempts_ != 0U && input.timestamp - secondaryStartedAt_ >= deadline))
		{
			pendingSecondary_ = false;
			result->secondaryTimedOut = true;
			return;
		}
		result->waitingForObservation = true;
		if ((input.secondaryReady.known && !input.secondaryReady.value) ||
			(pendingSecondary_ && input.timestamp - secondarySentAt_ < input.secondaryRetrySeconds))
			return;
		if (secondaryAttempts_ == 0U)
			secondaryStartedAt_ = input.timestamp;
		++secondaryAttempts_;
		pendingSecondary_ = true;
		secondarySentAt_ = input.timestamp;
		result->secondary = true;
	}

	WeaponActionDecision grenadeAction(const WeaponActionInput &input)
	{
		constexpr float kMaximumGrenadeHoldSeconds = 3.0f;
		WeaponActionDecision result = {};
		if (grenadeReleased_)
		{
			if (grenadeReserveKnown_ && input.reserveKnown && input.reserve > 0U && input.reserve < grenadeReserve_ &&
				input.reserveObservationTick >= grenadeReleaseTick_ &&
				input.reserveObservationTick >= grenadeReserveTick_ && input.reserveObservationTick <= input.frame.tick)
			{
				grenadeReleased_ = false;
				grenadeAimKnown_ = false;
				grenadeReserveKnown_ = false;
			}
			else
			{
				result.waitingForObservation = true;
				return result;
			}
		}
		const bool clear = input.trajectory.known && input.trajectory.lineClear && input.trajectory.hullClear &&
			input.trajectory.landingSafe;
		const bool validAim =
			input.grenadeAimKnown && std::isfinite(input.grenadePitch) && std::isfinite(input.grenadeYaw);
		if (clear && input.requestGrenade && validAim)
		{
			grenadePitch_ = input.grenadePitch;
			grenadeYaw_ = input.grenadeYaw;
			grenadeAimKnown_ = true;
		}
		if (!clear || !input.requestGrenade || input.defusing || (input.reloading.known && input.reloading.value))
		{
			result.waitingForObservation = input.requestGrenade;
			if (grenadeHeld_)
			{
				result.hasRetainedGrenadeAim = grenadeAimKnown_;
				result.grenadePitch = grenadePitch_;
				result.grenadeYaw = grenadeYaw_;
				result.stopMovement = true;
				if (input.timestamp - grenadeTimestamp_ >= kMaximumGrenadeHoldSeconds)
				{
					result.releasePrimary = true;
					grenadeHeld_ = false;
					grenadeReleased_ = true;
					grenadeReleaseTick_ = input.frame.tick;
				}
				else
					result.primary = true;
			}
			return result;
		}
		if (!validAim && !grenadeHeld_)
		{
			result.waitingForObservation = true;
			return result;
		}
		result.stopMovement = true;
		result.hasRetainedGrenadeAim = grenadeAimKnown_;
		result.grenadePitch = grenadePitch_;
		result.grenadeYaw = grenadeYaw_;
		if (!grenadeHeld_)
		{
			grenadeHeld_ = true;
			grenadeTimestamp_ = input.timestamp;
			grenadeReserveKnown_ = input.reserveKnown && input.reserveObservationTick <= input.frame.tick;
			grenadeReserve_ = input.reserve;
			grenadeReserveTick_ = input.reserveObservationTick;
			result.primary = true;
		}
		else if (input.aimAligned || input.timestamp - grenadeTimestamp_ >= kMaximumGrenadeHoldSeconds)
		{
			result.releasePrimary = true;
			grenadeHeld_ = false;
			grenadeReleased_ = true;
			grenadeReleaseTick_ = input.frame.tick;
		}
		else
		{
			result.primary = true;
		}
		return result;
	}

	world::ActorKey actor_ = {};
	world::FrameIdentity frame_ = {};
	std::uint16_t weaponId_ = 0U;
	float timestamp_ = 0.0f;
	float scopeTimestamp_ = 0.0f;
	float fireReadyTimestamp_ = 0.0f;
	bool rapidFiring_ = false;
	float grenadeTimestamp_ = 0.0f;
	bool pendingSecondary_ = false;
	std::uint8_t secondaryAttempts_ = 0U;
	float secondaryStartedAt_ = 0.0f;
	float secondarySentAt_ = 0.0f;
	bool heldPrimary_ = false;
	bool clipKnown_ = false;
	std::uint16_t clip_ = 0U;
	bool grenadeHeld_ = false;
	bool grenadeReleased_ = false;
	bool grenadeAimKnown_ = false;
	float grenadePitch_ = 0.0f;
	float grenadeYaw_ = 0.0f;
	bool grenadeReserveKnown_ = false;
	std::uint16_t grenadeReserve_ = 0U;
	std::uint32_t grenadeReserveTick_ = 0U;
	std::uint32_t grenadeReleaseTick_ = 0U;
};
} // namespace compat
} // namespace astrabot
#endif
