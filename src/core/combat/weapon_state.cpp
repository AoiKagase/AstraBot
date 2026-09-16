#include "astrabot/combat/weapon_state.hpp"

#include <cmath>

namespace astrabot
{
namespace combat
{
bool WeaponId::isValid() const
{
	return value > 0U && value <= WeaponLimits::kMaximumWeaponId;
}

bool WeaponId::operator==(const WeaponId &other) const
{
	return value == other.value;
}

bool AmmoState::isValid() const
{
	return clipCapacity > 0U && clipCapacity <= WeaponLimits::kMaximumAmmo &&
		clip <= clipCapacity && reserve <= WeaponLimits::kMaximumAmmo;
}

bool AmmoState::hasClipAmmo() const
{
	return clip > 0U;
}

bool AmmoState::hasReserveAmmo() const
{
	return reserve > 0U;
}

bool WeaponRecord::isValid() const
{
	return weapon.isValid() &&
		slot < WeaponLimits::kMaximumInventorySlots &&
		kind != WeaponKind::Unknown &&
		(kind == WeaponKind::Knife || kind == WeaponKind::Pistol ||
		 kind == WeaponKind::Shotgun || kind == WeaponKind::Smg ||
		 kind == WeaponKind::Rifle || kind == WeaponKind::Sniper ||
		 kind == WeaponKind::Heavy || kind == WeaponKind::Grenade) &&
		(availability == WeaponAvailability::Unknown ||
		 availability == WeaponAvailability::Unavailable ||
		 availability == WeaponAvailability::Available) &&
		ammo.isValid() &&
		(reloadState == ReloadState::Unknown ||
		 reloadState == ReloadState::NotReloading ||
		 reloadState == ReloadState::Reloading ||
		 reloadState == ReloadState::Complete);
}

bool WeaponRecord::canFire(const world::FrameIdentity &frame) const
{
	return frame.isValid() && isValid() &&
		availability == WeaponAvailability::Available &&
		reloadState != ReloadState::Reloading && ammo.hasClipAmmo() &&
		cooldownUntilTick <= frame.tick;
}

bool WeaponRecord::canReload(const world::FrameIdentity &frame) const
{
	return frame.isValid() && isValid() &&
		availability == WeaponAvailability::Available &&
		reloadState == ReloadState::NotReloading &&
		!ammo.hasClipAmmo() && ammo.hasReserveAmmo() &&
		reloadUntilTick <= frame.tick;
}

WeaponInventory::WeaponInventory()
	: weapons_(),
	  weaponCount_(0U)
{
}

WeaponInventoryResult WeaponInventory::add(const WeaponRecord &weapon)
{
	if (!weapon.isValid())
	{
		return WeaponInventoryResult::InvalidWeapon;
	}

	for (std::size_t index = 0U; index < weaponCount_; ++index)
	{
		if (weapons_[index].slot == weapon.slot ||
				weapons_[index].weapon == weapon.weapon)
		{
			return WeaponInventoryResult::DuplicateWeapon;
		}
	}

	if (weaponCount_ >= weapons_.size())
	{
		return WeaponInventoryResult::ResourceLimit;
	}

	weapons_[weaponCount_] = weapon;
	++weaponCount_;
	return WeaponInventoryResult::Accepted;
}

WeaponInventoryResult WeaponInventory::selectReady(
	const world::FrameIdentity &frame,
	const WeaponRecord **weapon) const
{
	if (weapon == nullptr)
	{
		return WeaponInventoryResult::InvalidArgument;
	}
	*weapon = nullptr;
	if (!frame.isValid())
	{
		return WeaponInventoryResult::InvalidFrame;
	}

	for (std::size_t index = 0U; index < weaponCount_; ++index)
	{
		if (!weapons_[index].canFire(frame))
		{
			continue;
		}
		if (*weapon == nullptr || isBetter(weapons_[index], **weapon))
		{
			*weapon = &weapons_[index];
		}
	}

	return *weapon == nullptr ? WeaponInventoryResult::NoAvailableWeapon :
		WeaponInventoryResult::Selected;
}

WeaponInventoryResult WeaponInventory::selectReloadable(
	const world::FrameIdentity &frame,
	const WeaponRecord **weapon) const
{
	if (weapon == nullptr)
	{
		return WeaponInventoryResult::InvalidArgument;
	}
	*weapon = nullptr;
	if (!frame.isValid())
	{
		return WeaponInventoryResult::InvalidFrame;
	}

	for (std::size_t index = 0U; index < weaponCount_; ++index)
	{
		if (!weapons_[index].canReload(frame))
		{
			continue;
		}
		if (*weapon == nullptr || isBetter(weapons_[index], **weapon))
		{
			*weapon = &weapons_[index];
		}
	}

	return *weapon == nullptr ? WeaponInventoryResult::NoAvailableWeapon :
		WeaponInventoryResult::Reloadable;
}

void WeaponInventory::clear()
{
	weaponCount_ = 0U;
}

std::size_t WeaponInventory::size() const
{
	return weaponCount_;
}

const WeaponRecord *WeaponInventory::weaponAt(std::size_t index) const
{
	return index < weaponCount_ ? &weapons_[index] : nullptr;
}

const WeaponRecord *WeaponInventory::find(WeaponId weapon) const
{
	if (!weapon.isValid())
	{
		return nullptr;
	}
	for (std::size_t index = 0U; index < weaponCount_; ++index)
	{
		if (weapons_[index].weapon == weapon)
		{
			return &weapons_[index];
		}
	}
	return nullptr;
}

bool WeaponInventory::isValidWeaponKind(WeaponKind kind)
{
	return kind != WeaponKind::Unknown;
}

bool WeaponInventory::isValidAvailability(WeaponAvailability availability)
{
	return availability == WeaponAvailability::Unknown ||
		availability == WeaponAvailability::Unavailable ||
		availability == WeaponAvailability::Available;
}

bool WeaponInventory::isValidReloadState(ReloadState state)
{
	return state == ReloadState::Unknown ||
		state == ReloadState::NotReloading || state == ReloadState::Reloading ||
		state == ReloadState::Complete;
}

bool WeaponInventory::isBetter(
	const WeaponRecord &candidate,
	const WeaponRecord &current)
{
	if (candidate.priority != current.priority)
	{
		return candidate.priority > current.priority;
	}
	if (candidate.slot != current.slot)
	{
		return candidate.slot < current.slot;
	}
	return candidate.weapon.value < current.weapon.value;
}
}
}
