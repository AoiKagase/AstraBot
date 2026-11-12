#ifndef ASTRABOT_COMPAT_WEAPON_ACTIONS_HPP
#define ASTRABOT_COMPAT_WEAPON_ACTIONS_HPP

#include "astrabot/world/world_snapshot.hpp"
#include "astrabot/compat/random_source.hpp"
#include <cmath>
#include <cstdint>

namespace astrabot
{
namespace compat
{
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
	float secondaryRetrySeconds = 0.5f;
	std::uint8_t maximumSecondaryAttempts = 3U;
	bool grenadeAimKnown = false;
	float grenadePitch = 0.0f;
	float grenadeYaw = 0.0f;
	bool reserveKnown = false;
	std::uint16_t reserve = 0U;
	std::uint32_t reserveObservationTick = 0U;
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
	WeaponActionDecision updateInternal(const WeaponActionInput &input, const FireCadenceContext *cadence)
	{
		WeaponActionDecision result = {};
		if (!input.actor.isValid() || !input.frame.isValid() || !std::isfinite(input.timestamp) ||
			input.timestamp < 0.0f || !std::isfinite(input.targetDistance) ||
			input.targetDistance < 0.0f || !std::isfinite(input.reactionSeconds) ||
			input.reactionSeconds < 0.0f)
		{
			return result;
		}
		if (!(input.actor == actor_) || input.frame.mapGeneration != frame_.mapGeneration ||
			input.frame.roundGeneration != frame_.roundGeneration || input.weaponId != weaponId_ ||
			input.timestamp < timestamp_)
		{
			reset();
		}
		else if (frame_.isValid() && input.frame.tick <= frame_.tick)
		{
			return result;
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
			heldPrimary_ = false;
			result.stopMovement = isSniper(input.weaponId) && input.targetVisible;
			return result;
		}
		if (isSniper(input.weaponId))
		{
			result.stopMovement = true;
			if (input.targetDistance < 310.0f)
			{
				result.switchToPistol = true;
				return result;
			}
			if (!input.fovKnown || !std::isfinite(input.fieldOfView) || input.fieldOfView <= 0.0f ||
				input.fieldOfView > 180.0f)
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
			if (input.fieldOfView > 60.0f)
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
				input.timestamp - scopeTimestamp_ < 0.2f + input.reactionSeconds)
			{
				return result;
			}
		}
		if (!input.aimAligned)
		{
			return result;
		}
		if (input.weaponId == 29U)
		{
			if (input.targetDistance >= 75.0f)
			{
				return result;
			}
			if (input.enemyFacingObserver.known && !input.enemyFacingObserver.value)
			{
				result.secondary = true;
			}
			else if (input.knifeRollKnown && std::isfinite(input.knifeRoll) &&
					 input.knifeRoll >= 0.0f && input.knifeRoll <= 100.0f)
			{
				result.secondary = input.knifeRoll < 33.3f;
				result.primary = !result.secondary;
			}
			else
			{
				result.unavailableMode = true;
			}
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
		if (cadence && (!std::isfinite(cadence->skill) || cadence->skill < 0.0f || cadence->skill > 1.0f ||
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
		if (isPistol(input.weaponId) && heldPrimary_ && clipKnown_ && input.clip < clip_)
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
			const bool spray = !pistol && (cadence->skill < 0.5f ||
				cadence->targetRange < 400.0f || input.weaponId == 20U);
			const bool rapidPistol = pistol && cadence->skill > 0.75f && cadence->targetRange < 999999.9f;
			if (!spray && !rapidPistol)
			{
				const bool distant = !pistol && !isSniper(input.weaponId) && cadence->targetRange > 800.0f;
				const float lower = distant ? 0.3f : 0.15f;
				const float upper = pistol ? 0.4f : distant ? 0.7f : 0.5f;
				if (!cadence->randomSource)
				{
					result.primary = false;
					result.unavailableMode = true;
					heldPrimary_ = false;
					return result;
				}
				const auto draw = cadence->randomSource->nextFloat(RandomRequest::floatRequest(
					"Combat.FireInterval", {input.actor.slot, input.actor.generation},
					cadence->randomTiming, lower, upper));
				if (draw.status != RandomStatus::Ok || !std::isfinite(draw.value) ||
					draw.value < lower || draw.value > upper)
				{
					result.primary = false;
					result.unavailableMode = true;
					heldPrimary_ = false;
					return result;
				}
				delay = draw.value;
			}
			// CSBot subtracts the fixed full-think latency, not the observed frame delta.
			fireReadyTimestamp_ = input.timestamp + (delay - 0.1f);
		}
		return result;
	}

  public:
	static bool isSniper(std::uint16_t id)
	{
		return id == 3U || id == 13U || id == 18U || id == 24U;
	}
	static bool isPistol(std::uint16_t id)
	{
		return id == 1U || id == 10U || id == 11U || id == 16U || id == 17U || id == 26U;
	}
	static bool isGrenade(std::uint16_t id)
	{
		return id == 4U || id == 9U || id == 25U;
	}
	static bool isFirearm(std::uint16_t id)
	{
		return id >= 1U && id <= 30U && id != 2U && id != 4U && id != 6U && id != 9U && id != 25U &&
			   id != 29U;
	}

  private:
	WeaponActionDecision modeAction(const WeaponActionInput &input)
	{
		WeaponActionDecision result = {};
		const bool silencer =
			input.requestSilencer && (input.weaponId == 16U || input.weaponId == 22U);
		const bool burst = input.requestBurst && (input.weaponId == 15U || input.weaponId == 17U);
		const KnownWeaponFlag current = silencer ? input.silenced : input.burstMode;
		const bool desired = silencer ? input.desiredSilencer : input.desiredBurst;
		if ((!silencer && !burst) || !current.known || !input.reloading.known ||
			(silencer && !input.shield.known))
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
			(silencer && (!std::isfinite(input.secondsSinceEnemy) ||
						  input.secondsSinceEnemy < 3.5f || input.shield.value)))
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
		const float deadline = input.secondaryRetrySeconds *
							   (static_cast<float>(input.maximumSecondaryAttempts) + 1.0f);
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
		WeaponActionDecision result = {};
		if (grenadeReleased_)
		{
			if (grenadeReserveKnown_ && input.reserveKnown && input.reserve > 0U &&
				input.reserve < grenadeReserve_ &&
				input.reserveObservationTick >= grenadeReleaseTick_ &&
				input.reserveObservationTick >= grenadeReserveTick_ &&
				input.reserveObservationTick <= input.frame.tick)
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
		const bool clear = input.trajectory.known && input.trajectory.lineClear &&
						   input.trajectory.hullClear && input.trajectory.landingSafe;
		const bool validAim = input.grenadeAimKnown && std::isfinite(input.grenadePitch) &&
							  std::isfinite(input.grenadeYaw);
		if (clear && input.requestGrenade && validAim)
		{
			grenadePitch_ = input.grenadePitch;
			grenadeYaw_ = input.grenadeYaw;
			grenadeAimKnown_ = true;
		}
		if (!clear || !input.requestGrenade || input.defusing ||
			(input.reloading.known && input.reloading.value))
		{
			result.waitingForObservation = input.requestGrenade;
			if (grenadeHeld_)
			{
				result.hasRetainedGrenadeAim = grenadeAimKnown_;
				result.grenadePitch = grenadePitch_;
				result.grenadeYaw = grenadeYaw_;
				result.stopMovement = true;
				if (input.timestamp - grenadeTimestamp_ >= 3.0f)
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
			grenadeReserveKnown_ =
				input.reserveKnown && input.reserveObservationTick <= input.frame.tick;
			grenadeReserve_ = input.reserve;
			grenadeReserveTick_ = input.reserveObservationTick;
			result.primary = true;
		}
		else if (input.aimAligned || input.timestamp - grenadeTimestamp_ >= 3.0f)
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
