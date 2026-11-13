#ifndef ASTRABOT_COMPAT_PUBLIC_OBSERVATION_HPP
#define ASTRABOT_COMPAT_PUBLIC_OBSERVATION_HPP

#include "astrabot/compat/observation.hpp"
#include "astrabot/compat/navigation_step.hpp"

#include <array>
#include <cstring>
#include <cmath>

namespace astrabot
{
namespace compat
{
struct ObservationCapabilities
{
	bool weaponMessages = false;
	bool economyMessages = false;
	bool statusMessages = false;
	bool sameTickWeaponTimers = false;
	bool privateAccuracy = false;
};

class IObservationProvider
{
public:
	virtual ~IObservationProvider() = default;
	virtual ObservationCapabilities capabilities() const = 0;
	virtual void supplement(CompatibilityObservation *observation) const = 0;
};

struct PublicWeaponDefinition
{
	bool known = false;
	int ammoIndex = -1;
	int maxAmmo = 0;
	int slot = 0;
};

struct PublicActorState
{
	world::ActorKey actor{};
	world::FrameIdentity frame{};
	bool bound = false;
	bool weaponKnown = false;
	int weaponId = 0;
	int clip = 0;
	std::uint32_t weaponTick = 0U;
	std::array<int, 31U> weaponClips{};
	std::array<bool, 31U> weaponClipKnown{};
	std::array<std::uint32_t, 31U> weaponClipTicks{};
	std::array<int, 32U> ammo{};
	std::array<bool, 32U> ammoKnown{};
	std::array<std::uint32_t, 32U> ammoTicks{};
	std::array<int, 32U> lastAmmoPickup{};
	std::array<std::uint32_t, 32U> ammoPickupTicks{};
	std::uint32_t pickupWeaponMask = 0U;
	bool moneyKnown = false;
	int money = 0;
	bool helmetKnown = false;
	bool hasHelmet = false;
	bool buyZoneKnown = false;
	bool inBuyZone = false;
	bool bombZoneKnown = false;
	bool inBombZone = false;
	bool defuseKitKnown = false;
	bool hasDefuseKit = false;
	bool rescueZoneKnown = false;
	bool inRescueZone = false;
	bool vipZoneKnown = false;
	bool inVipZone = false;
	bool escapeZoneKnown = false;
	bool inEscapeZone = false;
	std::uint32_t bombZoneTick = 0U;
	std::uint32_t defuseKitTick = 0U;
	std::uint32_t rescueZoneTick = 0U;
	bool progressKnown = false;
	bool progressActive = false;
	std::uint32_t progressTick = 0U;
	float progressObservedAt = 0.0f;
	float progressExpiresAt = 0.0f;
	float blindUntil = 0.0f;
	bool reloadRequested = false;
	float reloadRequestedAt = 0.0f;
};

// Message-derived state is event cached, never a same-tick private weapon read.
class PublicObservationStore final : public IObservationProvider
{
public:
	void bind(world::ActorKey actor, world::FrameIdentity frame)
	{
		if (!actor.isValid() || !frame.isValid() || actor.slot > actors_.size())
			return;
		PublicActorState &state = actors_[actor.slot - 1U];
		if (!state.bound || !(state.actor == actor) ||
			state.frame.mapGeneration != frame.mapGeneration ||
			state.frame.roundGeneration != frame.roundGeneration)
		{
			state = {};
			state.actor = actor;
			state.frame = frame;
			state.bound = true;
		}
	}

	void resetSlot(std::uint32_t slot)
	{
		if (slot > 0U && slot <= actors_.size())
			actors_[slot - 1U] = {};
	}

	void reset()
	{
		actors_ = {};
		definitions_ = {};
	}

	bool read(world::ActorKey actor, world::FrameIdentity frame, PublicActorState *out) const
	{
		if (out == nullptr || !actor.isValid() || actor.slot > actors_.size())
			return false;
		const PublicActorState &state = actors_[actor.slot - 1U];
		if (!state.bound || !(state.actor == actor) ||
			state.frame.mapGeneration != frame.mapGeneration ||
			state.frame.roundGeneration != frame.roundGeneration || state.frame.tick > frame.tick)
			return false;
		*out = state;
		return true;
	}

	bool currentWeapon(world::ActorKey actor, world::FrameIdentity frame, int active, int id, int clip)
	{
		if ((active != 0 && active != 1) || id <= 0 || id > 30 || clip < -1 || clip > 255)
			return false;
		PublicActorState *state = edit(actor, frame);
		if (state == nullptr)
			return false;
		state->weaponClips[static_cast<std::size_t>(id)] = clip;
		state->weaponClipKnown[static_cast<std::size_t>(id)] = clip >= 0;
		state->weaponClipTicks[static_cast<std::size_t>(id)] = frame.tick;
		if (active != 0)
		{
			if (state->weaponId != id || clip > state->clip)
				state->reloadRequested = false;
			state->weaponId = id;
			state->clip = clip;
			state->weaponKnown = true;
			state->weaponTick = frame.tick;
		}
		return true;
	}

	bool weaponDefinition(int id, int ammoIndex, int maxAmmo, int slot)
	{
		if (id <= 0 || id > 30 || ammoIndex < -1 || ammoIndex >= 32 ||
			maxAmmo < -1 || maxAmmo > 255 || slot < 0 || slot > 5)
			return false;
		definitions_[static_cast<std::size_t>(id)] = {true, ammoIndex, maxAmmo, slot};
		return true;
	}

	PublicWeaponDefinition definition(int id) const
	{
		return id > 0 && id < static_cast<int>(definitions_.size())
			? definitions_[static_cast<std::size_t>(id)] : PublicWeaponDefinition{};
	}

	bool ammo(world::ActorKey actor, world::FrameIdentity frame, int index, int amount)
	{
		if (index < 0 || index >= 32 || amount < 0 || amount > 255)
			return false;
		PublicActorState *state = edit(actor, frame);
		if (state == nullptr)
			return false;
		state->ammo[static_cast<std::size_t>(index)] = amount;
		state->ammoKnown[static_cast<std::size_t>(index)] = true;
		state->ammoTicks[static_cast<std::size_t>(index)] = frame.tick;
		return true;
	}

	bool ammoPickup(world::ActorKey actor, world::FrameIdentity frame, int index, int amount)
	{
		if (index < 0 || index >= 32 || amount < 0 || amount > 255)
		{
			return false;
		}
		PublicActorState *state = edit(actor, frame);
		if (state == nullptr)
		{
			return false;
		}
		// AmmoPickup is a HUD notification, not an authoritative total. AmmoX
		// can precede or follow it, so adding this delta would double count it.
		state->lastAmmoPickup[static_cast<std::size_t>(index)] = amount;
		state->ammoPickupTicks[static_cast<std::size_t>(index)] = frame.tick;
		return true;
	}

	bool weaponPickup(world::ActorKey actor, world::FrameIdentity frame, int id)
	{
		if (id <= 0 || id > 30)
		{
			return false;
		}
		PublicActorState *state = edit(actor, frame);
		if (state == nullptr)
		{
			return false;
		}
		state->pickupWeaponMask |= std::uint32_t(1U) << id;
		return true;
	}

	bool money(world::ActorKey actor, world::FrameIdentity frame, int amount)
	{
		if (amount < 0 || amount > 1000000)
			return false;
		PublicActorState *state = edit(actor, frame);
		if (state == nullptr)
			return false;
		state->moneyKnown = true;
		state->money = amount;
		return true;
	}

	bool armorType(world::ActorKey actor, world::FrameIdentity frame, int type)
	{
		if (type != 0 && type != 1)
			return false;
		PublicActorState *state = edit(actor, frame);
		if (state == nullptr)
			return false;
		state->helmetKnown = true;
		state->hasHelmet = type == 1;
		return true;
	}

	bool statusIcon(world::ActorKey actor, world::FrameIdentity frame, const char *name, int enabled)
	{
		if (name == nullptr || enabled < 0 || enabled > 2)
			return false;
		PublicActorState *state = edit(actor, frame);
		if (state == nullptr)
			return false;
		if (std::strcmp(name, "buyzone") == 0)
		{
			state->buyZoneKnown = true;
			state->inBuyZone = enabled != 0;
		}
		else if (std::strcmp(name, "c4") == 0)
		{
			state->bombZoneKnown = true;
			state->inBombZone = enabled == 2;
			state->bombZoneTick = frame.tick;
		}
		else if (std::strcmp(name, "defuser") == 0)
		{
			state->defuseKitKnown = true;
			state->hasDefuseKit = enabled != 0;
			state->defuseKitTick = frame.tick;
		}
		else if (std::strcmp(name, "rescue") == 0)
		{
			state->rescueZoneKnown = true;
			state->inRescueZone = enabled != 0;
			state->rescueZoneTick = frame.tick;
		}
		else if (std::strcmp(name, "vipsafety") == 0)
		{
			state->vipZoneKnown = true;
			state->inVipZone = enabled != 0;
		}
		else if (std::strcmp(name, "escape") == 0)
		{
			state->escapeZoneKnown = true;
			state->inEscapeZone = enabled != 0;
		}
		return true;
	}

	DefuseReceipt progressReceipt(world::ActorKey actor,world::FrameIdentity frame,float now,float attemptAt) const
	{
		PublicActorState state{};
		if(!std::isfinite(now) || !std::isfinite(attemptAt) || now<attemptAt ||
			!read(actor,frame,&state) || !state.progressKnown || state.progressTick>frame.tick ||
			state.progressObservedAt<attemptAt || state.progressObservedAt>now)
			return DefuseReceipt::Unknown;
		if(!state.progressActive)return DefuseReceipt::None;
		return now<state.progressExpiresAt ? DefuseReceipt::Self : DefuseReceipt::Unknown;
	}
	bool progress(world::ActorKey actor, world::FrameIdentity frame, int duration,float now=0.0f)
	{
		if (duration < 0 || duration > 65535 || !std::isfinite(now) || now<0.0f)
			return false;
		PublicActorState *state = edit(actor, frame);
		if (state == nullptr)
			return false;
		state->progressKnown = true;
		state->progressActive = duration != 0;
		state->progressTick=frame.tick;
		state->progressObservedAt=now;
		state->progressExpiresAt=now+static_cast<float>(duration);
		return true;
	}

	bool blind(world::ActorKey actor, world::FrameIdentity frame, float until)
	{
		if (!std::isfinite(until) || until < 0.0f)
			return false;
		PublicActorState *state = edit(actor, frame);
		if (state == nullptr)
			return false;
		state->blindUntil = until;
		return true;
	}

	void requestReload(world::ActorKey actor, world::FrameIdentity frame, float now)
	{
		PublicActorState *state = edit(actor, frame);
		if (state != nullptr && std::isfinite(now))
		{
			state->reloadRequested = true;
			state->reloadRequestedAt = now;
		}
	}

	bool pendingReloadRequest(world::ActorKey actor,world::FrameIdentity frame,float now) const
	{
		PublicActorState state{};
		// This is an inferred command lease, not the GameDLL's reload timer.
		return read(actor,frame,&state) && state.reloadRequested && std::isfinite(now) &&
			now>=state.reloadRequestedAt && now-state.reloadRequestedAt<5.0f;
	}

	ObservationCapabilities capabilities() const override
	{
		return {true, true, true, false, false};
	}

	void supplement(CompatibilityObservation *observation) const override
	{
		if (observation == nullptr)
			return;
		PublicActorState state{};
		if (!read(observation->actor, observation->frame, &state))
			return;
		if (state.weaponKnown)
		{
			observation->weapon.activeWeaponId = value<std::uint16_t>(
				"OBS-PLAYER-ACTIVE-WEAPON", state.weaponTick, *observation, static_cast<std::uint16_t>(state.weaponId));
			if (state.clip >= 0)
				observation->weapon.clip = value<std::uint16_t>(
					"OBS-WEAPON-CLIP", state.weaponTick, *observation, static_cast<std::uint16_t>(state.clip));
			const PublicWeaponDefinition weapon = definition(state.weaponId);
			if (weapon.known && weapon.ammoIndex >= 0 &&
				state.ammoKnown[static_cast<std::size_t>(weapon.ammoIndex)])
				observation->weapon.reserve = value<std::uint16_t>("OBS-WEAPON-RESERVE-AMMO", state.ammoTicks[static_cast<std::size_t>(weapon.ammoIndex)],
					*observation, static_cast<std::uint16_t>(state.ammo[static_cast<std::size_t>(weapon.ammoIndex)]));
		}
		if (state.defuseKitKnown)
			observation->objective.hasDefuseKit = value<bool>("OBS-OBJECTIVE-DEFUSE-KIT", state.defuseKitTick,
				*observation, state.hasDefuseKit);
		if (state.bombZoneKnown)
			observation->objective.inBombZone = value<bool>("OBS-OBJECTIVE-BOMB-ZONE", state.bombZoneTick,
				*observation, state.inBombZone);
		if (state.rescueZoneKnown)
			observation->objective.inRescueZone = value<bool>("OBS-OBJECTIVE-RESCUE-ZONE", state.rescueZoneTick,
				*observation, state.inRescueZone);
	}

private:
	PublicActorState *edit(world::ActorKey actor, world::FrameIdentity frame)
	{
		PublicActorState state{};
		if (!read(actor, frame, &state))
			return nullptr;
		PublicActorState &result = actors_[actor.slot - 1U];
		result.frame = frame;
		return &result;
	}

	template <typename T>
	static ObservationValue<T> value(const char *id, std::uint32_t observedTick,
		const CompatibilityObservation &observation, T data)
	{
		const ObservationContext context{id, observation.actor, observation.frame,
			ObservationQuality::Delayed, ObservationFreshness::EventDrivenCached,
			ObservationSource::PublicGameDll, observation.frame.tick - observedTick, observation.player.origin.context.timing};
		return {data, true, context};
	}

	std::array<PublicActorState, 32U> actors_{};
	std::array<PublicWeaponDefinition, 31U> definitions_{};
};
}
}
#endif
