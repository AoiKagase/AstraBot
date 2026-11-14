#ifndef ASTRABOT_COMPAT_WEAPON_PICKUP_HPP
#define ASTRABOT_COMPAT_WEAPON_PICKUP_HPP
#include "astrabot/compat/profile_catalog.hpp"
#include <cmath>
#include <cstdint>
#include <cstring>
namespace astrabot
{
namespace compat
{
struct PrimaryWeaponInfo
{
	unsigned id;
	const char *classname;
	const char *worldModel;
};
// Public vanilla identities only. A custom box model does not establish its private contents.
constexpr PrimaryWeaponInfo kPickupPrimaryWeapons[] = {{3, "weapon_scout", "models/w_scout.mdl"},
	{5, "weapon_xm1014", "models/w_xm1014.mdl"}, {7, "weapon_mac10", "models/w_mac10.mdl"},
	{8, "weapon_aug", "models/w_aug.mdl"}, {12, "weapon_ump45", "models/w_ump45.mdl"},
	{13, "weapon_sg550", "models/w_sg550.mdl"}, {14, "weapon_galil", "models/w_galil.mdl"},
	{15, "weapon_famas", "models/w_famas.mdl"}, {18, "weapon_awp", "models/w_awp.mdl"},
	{19, "weapon_mp5navy", "models/w_mp5.mdl"}, {20, "weapon_m249", "models/w_m249.mdl"},
	{21, "weapon_m3", "models/w_m3.mdl"}, {22, "weapon_m4a1", "models/w_m4a1.mdl"},
	{23, "weapon_tmp", "models/w_tmp.mdl"}, {24, "weapon_g3sg1", "models/w_g3sg1.mdl"},
	{27, "weapon_sg552", "models/w_sg552.mdl"}, {28, "weapon_ak47", "models/w_ak47.mdl"},
	{30, "weapon_p90", "models/w_p90.mdl"}};
inline const PrimaryWeaponInfo *pickupPrimaryWeapon(int id)
{
	for (const auto &weapon : kPickupPrimaryWeapons)
		if (static_cast<int>(weapon.id) == id)
			return &weapon;
	return nullptr;
}
inline const PrimaryWeaponInfo *primaryWeaponFromWorldModel(const char *model)
{
	if (model)
		for (const auto &weapon : kPickupPrimaryWeapons)
			if (std::strcmp(model, weapon.worldModel) == 0)
				return &weapon;
	return nullptr;
}
inline const PrimaryWeaponInfo *carriedPrimaryWeapon(std::uint32_t inventory)
{
	const PrimaryWeaponInfo *found = nullptr;
	for (const auto &weapon : kPickupPrimaryWeapons)
		if (inventory & (1U << weapon.id))
		{
			if (found)
				return nullptr;
			found = &weapon;
		}
	return found;
}
inline bool shouldExchangePrimary(const ProfileRecord &profile, int current, int dropped, float secondsSinceEnemy)
{
	constexpr float kSafeSeconds = 2.5f;
	if (current == dropped || !pickupPrimaryWeapon(current) || !pickupPrimaryWeapon(dropped) ||
		!std::isfinite(secondsSinceEnemy) || secondsSinceEnemy < kSafeSeconds)
		return false;
	for (std::size_t i = 0; i < profile.weaponPreferenceCount && i < ProfileRecord::kWeaponPreferenceCapacity; ++i)
	{
		const int preference = profile.weaponPreferences[i];
		if (!pickupPrimaryWeapon(preference))
			continue;
		if (preference == current)
			return false;
		if (preference == dropped)
			return true;
	}
	return false;
}
} // namespace compat
} // namespace astrabot
#endif
