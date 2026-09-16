#ifndef ASTRABOT_COMBAT_WEAPON_STATE_HPP
#define ASTRABOT_COMBAT_WEAPON_STATE_HPP

#include "astrabot/world/world_snapshot.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace astrabot
{
namespace combat
{
struct WeaponLimits
{
	static constexpr std::size_t kMaximumInventorySlots = 8U;
	static constexpr std::uint16_t kMaximumWeaponId = 64U;
	static constexpr std::uint16_t kMaximumAmmo = 255U;
};

struct WeaponId
{
	std::uint16_t value;

	bool isValid() const;
	bool operator==(const WeaponId &other) const;
};

enum class WeaponKind
{
	Unknown,
	Knife,
	Pistol,
	Shotgun,
	Smg,
	Rifle,
	Sniper,
	Heavy,
	Grenade
};

enum class WeaponAvailability
{
	Unknown,
	Unavailable,
	Available
};

enum class ReloadState
{
	Unknown,
	NotReloading,
	Reloading,
	Complete
};

struct AmmoState
{
	std::uint16_t clip;
	std::uint16_t reserve;
	std::uint16_t clipCapacity;

	bool isValid() const;
	bool hasClipAmmo() const;
	bool hasReserveAmmo() const;
};

struct WeaponRecord
{
	WeaponId weapon;
	std::uint8_t slot;
	WeaponKind kind;
	WeaponAvailability availability;
	std::uint8_t priority;
	AmmoState ammo;
	std::uint32_t cooldownUntilTick;
	std::uint32_t reloadUntilTick;
	ReloadState reloadState;

	bool isValid() const;
	bool canFire(const world::FrameIdentity &frame) const;
	bool canReload(const world::FrameIdentity &frame) const;
};

enum class WeaponInventoryResult
{
	Accepted,
	InvalidArgument,
	InvalidWeapon,
	DuplicateWeapon,
	ResourceLimit,
	Selected,
	Reloadable,
	NoAvailableWeapon,
	InvalidFrame
};

class WeaponInventory
{
public:
	WeaponInventory();

	WeaponInventoryResult add(const WeaponRecord &weapon);
	WeaponInventoryResult selectReady(
		const world::FrameIdentity &frame,
		const WeaponRecord **weapon) const;
	WeaponInventoryResult selectReloadable(
		const world::FrameIdentity &frame,
		const WeaponRecord **weapon) const;

	void clear();
	std::size_t size() const;
	const WeaponRecord *weaponAt(std::size_t index) const;
	const WeaponRecord *find(WeaponId weapon) const;

private:
	static bool isValidWeaponKind(WeaponKind kind);
	static bool isValidAvailability(WeaponAvailability availability);
	static bool isValidReloadState(ReloadState state);
	static bool isBetter(
		const WeaponRecord &candidate,
		const WeaponRecord &current);

	std::array<WeaponRecord, WeaponLimits::kMaximumInventorySlots> weapons_;
	std::size_t weaponCount_;
};
}
}

#endif
