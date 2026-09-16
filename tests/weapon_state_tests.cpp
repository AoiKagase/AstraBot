#include "astrabot/combat/weapon_state.hpp"

#include <cstdio>

namespace
{
	bool check(bool condition, const char *description)
	{
		if (condition)
		{
			return true;
		}

		std::fprintf(stderr, "check failed: %s\n", description);
		return false;
	}

	astrabot::world::FrameIdentity frame(std::uint32_t tick)
	{
		return {3U, 4U, tick};
	}

	astrabot::combat::WeaponRecord rifle()
	{
		astrabot::combat::WeaponRecord record = {};
		record.weapon = {7U};
		record.slot = 1U;
		record.kind = astrabot::combat::WeaponKind::Rifle;
		record.availability = astrabot::combat::WeaponAvailability::Available;
		record.priority = 10U;
		record.ammo = {30U, 90U, 30U};
		record.cooldownUntilTick = 0U;
		record.reloadUntilTick = 0U;
		record.reloadState = astrabot::combat::ReloadState::NotReloading;
		return record;
	}
}

bool testInventoryBoundsAndSelection()
{
	using astrabot::combat::WeaponInventory;
	using astrabot::combat::WeaponInventoryResult;
	using astrabot::combat::WeaponRecord;

	WeaponInventory inventory;
	if (!check(inventory.add(rifle()) == WeaponInventoryResult::Accepted,
			"valid weapon is accepted"))
	{
		return false;
	}

	if (!check(inventory.add(rifle()) == WeaponInventoryResult::DuplicateWeapon,
			"duplicate weapon identity is rejected"))
	{
		return false;
	}

	WeaponRecord invalid = rifle();
	invalid.ammo.clip = 31U;
	if (!check(inventory.add(invalid) == WeaponInventoryResult::InvalidWeapon,
			"clip above capacity is rejected"))
	{
		return false;
	}

	invalid = rifle();
	invalid.weapon = {8U};
	invalid.slot = static_cast<std::uint8_t>(
		astrabot::combat::WeaponLimits::kMaximumInventorySlots);
	if (!check(inventory.add(invalid) == WeaponInventoryResult::InvalidWeapon,
			"out-of-range inventory slot is rejected"))
	{
		return false;
	}

	const WeaponRecord *selected = nullptr;
	if (!check(inventory.selectReady(frame(5U), &selected) ==
			WeaponInventoryResult::Selected && selected != nullptr &&
			selected->weapon.value == 7U,
			"available weapon with clip ammo is selected"))
	{
		return false;
	}

	WeaponRecord reloadable = rifle();
	reloadable.weapon = {9U};
	reloadable.slot = 2U;
	reloadable.priority = 20U;
	reloadable.ammo = {0U, 30U, 30U};
	if (!check(inventory.add(reloadable) == WeaponInventoryResult::Accepted,
			"reloadable weapon is accepted"))
	{
		return false;
	}

	if (!check(inventory.selectReloadable(frame(5U), &selected) ==
			WeaponInventoryResult::Reloadable && selected != nullptr &&
			selected->weapon.value == 9U,
			"empty clip with reserve ammo selects reload"))
	{
		return false;
	}

	return check(inventory.size() == 2U,
		"inventory count remains bounded and transactional");
}

bool testUnavailableAndReloadStatesFailClosed()
{
	using astrabot::combat::WeaponInventory;
	using astrabot::combat::WeaponInventoryResult;

	WeaponInventory inventory;
	astrabot::combat::WeaponRecord unknown = rifle();
	unknown.availability = astrabot::combat::WeaponAvailability::Unknown;
	if (!check(inventory.add(unknown) == WeaponInventoryResult::Accepted,
			"unknown weapon observation is stored as unknown"))
	{
		return false;
	}

	const astrabot::combat::WeaponRecord *selected = nullptr;
	return check(inventory.selectReady(frame(5U), &selected) ==
			WeaponInventoryResult::NoAvailableWeapon && selected == nullptr,
		"unknown weapon availability cannot produce a fire selection");
}

int main()
{
	if (!testInventoryBoundsAndSelection() ||
			!testUnavailableAndReloadStatesFailClosed())
	{
		return 1;
	}

	return 0;
}
