#include "astrabot/compat/public_observation.hpp"
#include "astrabot/compat/smoke_observation.hpp"
#include "astrabot/objectives/bomb_site_route_selection.hpp"

#include <cstdio>

using namespace astrabot;

int main()
{
	compat::ScriptedRandomSource tape(
		{compat::RandomTapeEntry::floatEntry("Bomb.RoundPlantDelay", {}, {0U, 0U, 3U}, 10.0f, 30.0f, 20.0f)});
	objectives::RoundPlantDecision roundPlant;
	roundPlant.begin(2U, 3U, 100.0f, tape);
	roundPlant.begin(2U, 3U, 110.0f, tape);
	if (!roundPlant.known || roundPlant.plantAt != 120.0f || !tape.verifyComplete())
	{
		return 15;
	}
	compat::SmokeObservations smoke;
	smoke.observe({50U, 1U}, {0.0f, 0.0f, 0.0f}, 8.0f);
	if (!smoke.blocks({-200.0f, 0.0f, 0.0f}, {200.0f, 0.0f, 0.0f}, 1.0f) ||
		smoke.blocks({-200.0f, 200.0f, 0.0f}, {200.0f, 200.0f, 0.0f}, 1.0f) ||
		smoke.blocks({-200.0f, 0.0f, 0.0f}, {200.0f, 0.0f, 0.0f}, 9.0f))
	{
		return 11;
	}
	compat::PublicObservationStore store;
	const world::ActorKey actor{1U, 7U};
	const world::FrameIdentity frame{2U, 3U, 10U};
	store.bind(actor, frame);
	compat::PublicActorState state{};
	if (!store.read(actor, frame, &state) || state.helmetKnown)
		return 16;
	if (!store.armorType(actor, frame, 0) || !store.read(actor, frame, &state) || !state.helmetKnown || state.hasHelmet)
		return 17;
	if (!store.armorType(actor, frame, 1) || !store.read(actor, frame, &state) || !state.helmetKnown ||
		!state.hasHelmet)
		return 18;
	if (store.armorType(actor, frame, 2) || store.armorType({1U, 8U}, frame, 0) || !store.read(actor, frame, &state) ||
		!state.hasHelmet)
		return 19;
	if (store.read(actor, frame, &state) && state.weaponKnown)
		return 1;
	if (!store.currentWeapon(actor, frame, 1, 22, 0))
		return 2;
	if (!store.ammo(actor, frame, 4, 17))
		return 3;
	if (!store.read(actor, {2U, 3U, 11U}, &state) || !state.weaponKnown || state.clip != 0)
		return 4;
	if (store.currentWeapon(actor, frame, 1, 22, 999))
		return 5;
	if (store.read({1U, 8U}, frame, &state))
		return 6;
	if (store.read(actor, {2U, 4U, 11U}, &state))
		return 7;
	if (!store.money(actor, frame, 2500) || !store.statusIcon(actor, frame, "buyzone", 1))
		return 8;
	if (!store.read(actor, frame, &state) || !state.moneyKnown || state.money != 2500 || !state.inBuyZone)
		return 9;
	if (!store.ammoPickup(actor, {2U, 3U, 11U}, 4, 10) || !store.ammoPickup(actor, {2U, 3U, 11U}, 5, 20) ||
		!store.weaponPickup(actor, {2U, 3U, 11U}, 1))
	{
		return 12;
	}
	if (!store.read(actor, {2U, 3U, 11U}, &state) || state.ammo[4] != 17 || state.ammoKnown[5] ||
		state.weaponId != 22 || state.pickupWeaponMask != (1U << 1U))
	{
		return 13;
	}
	store.weaponDefinition(22, 4, 90, 0);
	compat::CompatibilityObservation observation{};
	observation.actor = actor;
	observation.frame = {2U, 3U, 12U};
	store.supplement(&observation);
	if (observation.weapon.clip.context.delayTicks != 2U || observation.weapon.reserve.context.delayTicks != 2U)
	{
		return 14;
	}
	if (!store.progress(actor, {2U, 3U, 13U}, 5, 10.0f) ||
		store.progressReceipt(actor, {2U, 3U, 14U}, 10.5f, 10.0f) != compat::DefuseReceipt::Self ||
		store.progressReceipt(actor, {2U, 3U, 14U}, 15.0f, 10.0f) != compat::DefuseReceipt::Unknown ||
		store.progressReceipt(actor, {2U, 4U, 14U}, 10.5f, 10.0f) != compat::DefuseReceipt::Unknown ||
		store.progressReceipt(actor, {2U, 3U, 14U}, 10.5f, 10.6f) != compat::DefuseReceipt::Unknown)
	{
		std::puts("FAIL: progress receipt must preserve lifetime, attempt and round ownership");
		return 21;
	}
	if (!store.progress(actor, {2U, 3U, 15U}, 0, 11.0f) ||
		store.progressReceipt(actor, {2U, 3U, 16U}, 11.1f, 10.0f) != compat::DefuseReceipt::None)
		return 22;
	store.requestReload(actor, {2U, 3U, 17U}, 12.0f);
	if (!store.pendingReloadRequest(actor, {2U, 3U, 18U}, 12.1f) ||
		store.pendingReloadRequest(actor, {2U, 3U, 18U}, 17.0f))
	{
		std::puts("FAIL: rejected reload request must not permanently block defuse");
		return 23;
	}
	store.resetSlot(1U);
	if (store.read(actor, frame, &state))
		return 10;
	store.bind({1U, 8U}, {2U, 4U, 1U});
	if (!store.read({1U, 8U}, {2U, 4U, 1U}, &state) || state.helmetKnown)
		return 20;
	std::puts("public observation isolation and unknown/zero distinction: PASS");
	return 0;
}
