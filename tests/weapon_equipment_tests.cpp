#include "astrabot/compat/weapon_equipment.hpp"
#include "astrabot/compat/footstep_equipment.hpp"
#include "astrabot/compat/public_observation.hpp"
#include <cstdio>
using namespace astrabot;
namespace
{
bool check(bool value, const char *message)
{
	if (!value)
		std::fprintf(stderr, "%s\n", message);
	return value;
}

compat::EquipmentInput input()
{
	compat::EquipmentInput in{};
	in.actor = {1, 1};
	in.frame = {1, 1, 1};
	in.now = 10;
	in.skill = .9f;
	in.currentWeapon = 22;
	in.weaponObservationTick = 1;
	in.primary = true;
	in.clipKnown = true;
	in.reserveKnown = true;
	in.reserve = 30;
	in.attacking = true;
	in.pistolWeapon = 17;
	in.pistolAmmo = compat::EquipmentAmmo::Usable;
	in.hasKnife = true;
	return in;
}

bool testConditions()
{
	for (unsigned variant = 0; variant < 13; ++variant)
	{
		auto in = input();
		unsigned expected = 17;
		switch (variant)
		{
		case 0:
			in.skill = .5f;
			expected = 0;
			break;
		case 1:
			in.skill = .51f;
			break;
		case 2:
			in.attacking = false;
			expected = 0;
			break;
		case 3:
			in.attacking = false;
			in.skill = .1f;
			in.reserve = 0;
			break;
		case 4:
			in.reserveKnown = false;
			break;
		case 5:
			in.allowPistols = false;
			expected = 0;
			break;
		case 6:
			in.clipKnown = false;
			expected = 0;
			break;
		case 7:
			in.reloading = true;
			expected = 0;
			break;
		case 8:
			in.defusing = true;
			expected = 0;
			break;
		case 9:
			in.pistolWeapon = 0;
			in.reserve = 0;
			expected = 29;
			break;
		case 10:
			in.pistolAmmo = compat::EquipmentAmmo::Unknown;
			in.reserve = 0;
			expected = 0;
			break;
		case 11:
			in.pistolAmmo = compat::EquipmentAmmo::Empty;
			in.reserve = 0;
			expected = 29;
			break;
		case 12:
			in.clip = 1;
			expected = 0;
			break;
		}
		compat::WeaponEquipmentPlanner planner;
		const auto d = planner.update(in);
		if (!check(d.selectWeapon == expected,
				"equipment condition matrix: skill, combat, reserve, exclusions and unknown ammo"))
			return false;
	}
	return true;
}

bool testReceiptAndClock()
{
	auto in = input();
	compat::WeaponEquipmentPlanner planner;
	if (!check(planner.update(in).selectWeapon == 17, "first eligible switch is immediate"))
		return false;
	planner.dispatched(in.actor, in.frame, in.now, 17, true);
	in.frame.tick = 2;
	in.now = 14.999f;
	if (!check(planner.update(in).ownsAction, "5-second interval suppresses reload as well as another switch"))
		return false;
	in.frame.tick = 3;
	in.now = 15;
	if (!check(planner.update(in).selectWeapon == 17, "5-second exact boundary permits retry without receipt"))
		return false;
	planner.dispatched(in.actor, in.frame, in.now, 17, true);
	in.frame.tick = 4;
	in.now = 15.1f;
	in.currentWeapon = 17;
	in.primary = false;
	in.weaponObservationTick = 2;
	if (!check(planner.update(in).ownsAction, "old weapon observation is not a new equip receipt"))
		return false;
	in.frame.tick = 5;
	in.weaponObservationTick = 4;
	if (!check(!planner.update(in).ownsAction, "fresh CurWeapon releases the pending selection"))
		return false;
	in = input();
	planner.reset();
	planner.dispatched(in.actor, in.frame, in.now, 17, false);
	in.frame.tick = 2;
	in.now = 10.1f;
	if (!check(planner.update(in).selectWeapon == 0,
			"failed transport has bounded retry and does not assert weapon success"))
		return false;
	in.frame.tick = 3;
	in.now = 15;
	if (!check(planner.update(in).selectWeapon == 17, "failed transport can retry at the deadline"))
		return false;
	for (unsigned variant = 0; variant < 4; ++variant)
	{
		in = input();
		planner.reset();
		planner.dispatched(in.actor, in.frame, in.now, 17, true);
		in.frame.tick = 2;
		in.now = 10.1f;
		if (variant == 0)
			in.actor.generation++;
		if (variant == 1)
			in.frame.mapGeneration++;
		if (variant == 2)
			in.frame.roundGeneration++;
		if (variant == 3)
			in.now = 1;
		if (!check(planner.update(in).selectWeapon == 17, "actor/map/round/time restart clears old equip state"))
			return false;
	}
	return true;
}

bool testPublicClipHistory()
{
	compat::PublicObservationStore store;
	compat::PublicActorState state{};
	world::ActorKey actor{1, 1};
	world::FrameIdentity frame{1, 1, 1};
	store.bind(actor, frame);
	store.currentWeapon(actor, frame, 1, 22, 0);
	store.currentWeapon(actor, frame, 0, 17, 20);
	if (!check(store.read(actor, frame, &state) && state.weaponId == 22 && state.clip == 0 &&
				state.weaponClipKnown[17] && state.weaponClips[17] == 20,
			"inactive CurWeapon records pistol clip without changing active primary"))
		return false;
	store.currentWeapon(actor, frame, 0, 17, -1);
	if (!check(store.read(actor, frame, &state) && !state.weaponClipKnown[17],
			"unknown clip invalidates the cached certainty"))
		return false;
	frame.roundGeneration++;
	store.bind(actor, frame);
	if (!check(store.read(actor, frame, &state) && !state.weaponClipKnown[17],
			"new round cannot reuse earlier pistol clip"))
		return false;
	store.currentWeapon(actor, frame, 0, 17, 20);
	store.resetSlot(1);
	return check(!store.read(actor, frame, &state), "removed actor cannot reuse ammo history");
}
}
bool testCloseFootsteps()
{
	for (const char *sample : {"player/pl_step1.wav", "player/pl_metal4.wav", "player/pl_tile5.wav",
		"player/pl_dirt2.wav", "player/pl_duct3.wav", "player/pl_grate1.wav", "player/pl_slosh4.wav",
		"player/pl_wade2.wav", "player/pl_ladder4.wav", "player/pl_snow1.wav"})
		if (!check(compat::isStandardFootstepSample(sample), "standard surface footsteps recognized"))
			return false;
	for (const char *sample : {"player/pl_step0.wav", "player/pl_step5.wav", "player/pl_tile6.wav",
		"player/pl_step1.wav.extra", "weapons/ak47-1.wav", "player/pl_fallpain3.wav", "custom/pl_step1.wav"})
		if (!check(!compat::isStandardFootstepSample(sample), "non-step/custom/invalid sample is not footsteps"))
			return false;
	compat::FootstepEquipmentReceipt receipt{{1, 1}, {1, 1, 1}, 10, 299};
	if (!check(receipt.current({1, 1}, {1, 1, 2}, 10.125f) &&
			!receipt.current({1, 1}, {1, 1, 3}, 10.251f) && !receipt.current({1, 1}, {1, 1, 3}, 9) &&
			!receipt.current({1, 2}, {1, 1, 3}, 10.125f) && !receipt.current({1, 1}, {1, 2, 3}, 10.125f),
			"footstep receipt is bounded to current actor/round and next full update"))
		return false;
	for (unsigned id : {3U, 13U, 18U, 24U})
	{
		for (float distance : {299.0f, 300.0f, 301.0f})
		{
			auto in = input();
			in.currentWeapon = static_cast<std::uint16_t>(id);
			in.clip = 10;
			in.clipKnown = false;
			in.skill = 0;
			in.attacking = false;
			in.closeFootstepDistance = distance;
			compat::WeaponEquipmentPlanner planner;
			if (!check(planner.update(in).selectWeapon == (distance < 300 ? 17 : 0),
					"raw close-footstep threshold selects pistol for all snipers independent of clip/skill/combat"))
				return false;
		}
	}
	for (unsigned variant = 0; variant < 6; ++variant)
	{
		auto in = input();
		in.currentWeapon = variant == 0 ? 22 : 18;
		in.clip = 10;
		in.closeFootstepDistance = 100;
		if (variant == 1)
			in.allowPistols = false;
		if (variant == 2)
			in.pistolAmmo = compat::EquipmentAmmo::Unknown;
		if (variant == 3)
			in.pistolAmmo = compat::EquipmentAmmo::Empty;
		if (variant == 4)
			in.defusing = true;
		if (variant == 5)
			in.reloading = true;
		compat::WeaponEquipmentPlanner planner;
		const auto decision = planner.update(in);
		if (!check(variant == 5 ? decision.selectWeapon == 17 : !decision.ownsAction,
				"qualified footsteps may interrupt reloading but preserve ammo, pistol permission and defuse gates"))
			return false;
	}
	return true;
}

int main()
{
	return testCloseFootsteps() && testConditions() && testReceiptAndClock() && testPublicClipHistory() ? 0 : 1;
}
