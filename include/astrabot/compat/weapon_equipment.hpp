#ifndef ASTRABOT_COMPAT_WEAPON_EQUIPMENT_HPP
#define ASTRABOT_COMPAT_WEAPON_EQUIPMENT_HPP
#include "astrabot/world/world_snapshot.hpp"
#include "astrabot/compat/footstep_equipment.hpp"
#include <cmath>
#include <cstdint>
namespace astrabot
{
namespace compat
{
enum class EquipmentAmmo
{
	Unknown,
	Empty,
	Usable
};
struct EquipmentInput
{
	world::ActorKey actor{};
	world::FrameIdentity frame{};
	float now = 0, skill = 0;
	std::uint16_t currentWeapon = 0, pistolWeapon = 0;
	std::uint32_t weaponObservationTick = 0;
	bool primary = false, clipKnown = false, reserveKnown = false;
	unsigned clip = 0, reserve = 0;
	bool attacking = false, reloading = false, defusing = false, allowPistols = true, hasKnife = false;
	EquipmentAmmo pistolAmmo = EquipmentAmmo::Unknown;
	float closeFootstepDistance = -1;
};
struct EquipmentDecision
{
	bool ownsAction = false;
	std::uint16_t selectWeapon = 0;
};
class WeaponEquipmentPlanner
{
public:
	void reset()
	{
		*this = WeaponEquipmentPlanner{};
	}

	EquipmentDecision update(const EquipmentInput &in)
	{
		constexpr float kTacticalPistolMinimumSkill = .5f;
		constexpr int kKnifeWeaponId = 29;
		if (!in.actor.isValid() || !in.frame.isValid() || !std::isfinite(in.now) || in.now < 0 ||
			!std::isfinite(in.skill) || in.skill < 0 || in.skill > 1)
			return {};
		bind(in.actor, in.frame, in.now);
		if (lastTick_ != 0 && in.frame.tick <= lastTick_)
			return {};
		lastTick_ = in.frame.tick;
		lastTime_ = in.now;
		if (pending_ != 0)
		{
			if (in.currentWeapon == pending_ && in.weaponObservationTick >= pendingTick_)
				pending_ = 0;
			else if (in.now < retryAt_)
				return {true, 0};
			else
				pending_ = 0;
		}
		if (in.defusing || in.reloading || !in.primary)
			return {};
		const bool usablePistol = in.allowPistols && in.pistolWeapon != 0 && in.pistolAmmo == EquipmentAmmo::Usable;
		constexpr float kCloseFootstepRange = 300.0f;
		const bool closeFootstep = isFootstepSniper(in.currentWeapon) && std::isfinite(in.closeFootstepDistance) &&
			in.closeFootstepDistance >= 0 && in.closeFootstepDistance < kCloseFootstepRange && usablePistol;
		if (!closeFootstep && (!in.clipKnown || in.clip != 0))
		{
			return {};
		}
		const bool tactical = in.skill > kTacticalPistolMinimumSkill && in.attacking && usablePistol;
		const bool exhausted = in.reserveKnown && in.reserve == 0;
		if (!closeFootstep && !tactical && !exhausted)
			return {};
		std::uint16_t target = 0;
		if (usablePistol)
			target = in.pistolWeapon;
		else if (exhausted && (!in.allowPistols || in.pistolWeapon == 0 || in.pistolAmmo == EquipmentAmmo::Empty) &&
			in.hasKnife)
			target = kKnifeWeaponId;
		if (target == 0)
			return {};
		if (in.now < retryAt_)
			return {true, 0};
		return {true, target};
	}

	void dispatched(world::ActorKey actor, world::FrameIdentity frame, float now, std::uint16_t weapon, bool success)
	{
		constexpr float kEquipmentRetrySeconds = 5.0f;
		if (!actor.isValid() || !frame.isValid() || !std::isfinite(now) || now < 0 || weapon == 0)
			return;
		bind(actor, frame, now);
		lastTime_ = now;
		retryAt_ = now + kEquipmentRetrySeconds;
		if (success)
		{
			pending_ = weapon;
			pendingTick_ = frame.tick;
		}
	}

private:
	void bind(world::ActorKey actor, world::FrameIdentity frame, float now)
	{
		if (!(actor_ == actor) || map_ != frame.mapGeneration || round_ != frame.roundGeneration || now < lastTime_)
		{
			reset();
			actor_ = actor;
			map_ = frame.mapGeneration;
			round_ = frame.roundGeneration;
		}
	}

	world::ActorKey actor_{};
	std::uint32_t map_ = 0, round_ = 0, lastTick_ = 0, pendingTick_ = 0;
	std::uint16_t pending_ = 0;
	float lastTime_ = 0, retryAt_ = 0;
};
}
}
#endif
