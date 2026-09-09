// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "core/combat.hpp"
#include <extdll.h>
// CSSDK normally gets BIT from engine/maintypes.h, which also declares
// engine ABI types. Keep the pinned Metamod-P types and scope this prerequisite.
#pragma push_macro("BIT")
#ifndef BIT
#define BIT(n) (1 << (n))
#endif
#include <cssdk/dlls/weapontype.h>
#pragma push_macro("MAX_WEAPON_SLOTS")
#undef MAX_WEAPON_SLOTS
#pragma push_macro("MAX_ITEM_TYPES")
#undef MAX_ITEM_TYPES
#pragma push_macro("MAX_AMMO_SLOTS")
#undef MAX_AMMO_SLOTS
#pragma push_macro("MAX_ITEMS")
#undef MAX_ITEMS
#include <cssdk/dlls/cdll_dll.h>
#pragma pop_macro("MAX_ITEMS")
#pragma pop_macro("MAX_AMMO_SLOTS")
#pragma pop_macro("MAX_ITEM_TYPES")
#pragma pop_macro("MAX_WEAPON_SLOTS")
#include <cstddef>

namespace astrabot::adapter::cstrike::protocol {
// CS-specific SDK declarations; engine ABI types still come from Metamod-P.
// GameDLL implementation and private C++ object layouts are not imported.
inline constexpr std::size_t kWeaponDataSlots = 32;
inline constexpr int kNoClip = -1;
inline constexpr int kCanShoot = PLAYER_CAN_SHOOT;
// Despite the upstream name PLAYER_FREEZE_TIME_OVER, the prediction producer
// sets this bit DURING freeze time. Preserve the established consumer polarity.
inline constexpr int kFreezePeriod = PLAYER_FREEZE_TIME_OVER;
using WeaponClass = core::combat::WeaponSnapshot::WeaponClass;
struct WeaponDescriptor {
    int id;
    const char* command;
    WeaponClass weaponClass;
};
// Classification is AstraBot policy; identifiers and commands are CS protocol.
inline constexpr WeaponDescriptor kWeapons[] = {
    {WEAPON_P228, "weapon_p228", WeaponClass::Pistol},
    {WEAPON_SCOUT, "weapon_scout", WeaponClass::Sniper},
    {WEAPON_HEGRENADE, "weapon_hegrenade", WeaponClass::Unknown},
    {WEAPON_XM1014, "weapon_xm1014", WeaponClass::Shotgun},
    {WEAPON_C4, "weapon_c4", WeaponClass::Unknown},
    {WEAPON_MAC10, "weapon_mac10", WeaponClass::SMG},
    {WEAPON_AUG, "weapon_aug", WeaponClass::Rifle},
    {WEAPON_SMOKEGRENADE, "weapon_smokegrenade", WeaponClass::Unknown},
    {WEAPON_ELITE, "weapon_elite", WeaponClass::Pistol},
    {WEAPON_FIVESEVEN, "weapon_fiveseven", WeaponClass::Pistol},
    {WEAPON_UMP45, "weapon_ump45", WeaponClass::SMG},
    {WEAPON_SG550, "weapon_sg550", WeaponClass::Sniper},
    {WEAPON_GALIL, "weapon_galil", WeaponClass::Rifle},
    {WEAPON_FAMAS, "weapon_famas", WeaponClass::Rifle},
    {WEAPON_USP, "weapon_usp", WeaponClass::Pistol},
    {WEAPON_GLOCK18, "weapon_glock18", WeaponClass::Pistol},
    {WEAPON_AWP, "weapon_awp", WeaponClass::Sniper},
    {WEAPON_MP5N, "weapon_mp5navy", WeaponClass::SMG},
    {WEAPON_M249, "weapon_m249", WeaponClass::MachineGun},
    {WEAPON_M3, "weapon_m3", WeaponClass::Shotgun},
    {WEAPON_M4A1, "weapon_m4a1", WeaponClass::Rifle},
    {WEAPON_TMP, "weapon_tmp", WeaponClass::SMG},
    {WEAPON_G3SG1, "weapon_g3sg1", WeaponClass::Sniper},
    {WEAPON_FLASHBANG, "weapon_flashbang", WeaponClass::Unknown},
    {WEAPON_DEAGLE, "weapon_deagle", WeaponClass::Pistol},
    {WEAPON_SG552, "weapon_sg552", WeaponClass::Rifle},
    {WEAPON_AK47, "weapon_ak47", WeaponClass::Rifle},
    {WEAPON_KNIFE, "weapon_knife", WeaponClass::Melee},
    {WEAPON_P90, "weapon_p90", WeaponClass::SMG},
};
inline constexpr const WeaponDescriptor* weapon(int id) noexcept {
    for (const auto& entry : kWeapons)
        if (entry.id == id) return &entry;
    return nullptr;
}
inline constexpr WeaponClass weaponClass(int id) noexcept {
    const auto* entry = weapon(id);
    return entry ? entry->weaponClass : WeaponClass::Unknown;
}
inline constexpr bool isSwitchableWeapon(int id) noexcept {
    return weaponClass(id) != WeaponClass::Unknown;
}
inline constexpr const char* weaponCommand(int id) noexcept {
    const auto* entry = weapon(id);
    return entry ? entry->command : nullptr;
}
} // namespace astrabot::adapter::cstrike::protocol
#pragma pop_macro("BIT")
