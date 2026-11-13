#include "runtime/behavior/bot_behavior_runtime.hpp"
#include "runtime/shared/bot_runtime_support.hpp"
namespace astrabot
{
namespace metamod
{
bool BotBehaviorRuntime::decideEquipmentAction(
	ManagedActionContext &ctx, const compat::PublicActorState &weapon, bool attacking)
{
	constexpr int kPrimaryWeaponSlot = 0;
	constexpr int kProfilePercentageScale = 100;
	constexpr int kAmmoTypeCount = 32;
	constexpr int kKnifeWeaponId = 29;
	constexpr unsigned kPistolWeaponIds[] = {1U, 10U, 11U, 16U, 17U, 26U};
	const auto &store = services_->observation_publicObservations();
	const auto definition = store.definition(weapon.weaponId);
	if (!weapon.weaponKnown || !definition.known || definition.slot != kPrimaryWeaponSlot || weapon.clip < 0 ||
		weapon.clip != 0)
		return false;
	compat::EquipmentInput in{};
	in.actor = ctx.actor;
	in.frame = ctx.frame;
	in.now = services_->service_globals()->time;
	in.skill = static_cast<float>(actors_[ctx.index].managedProfiles.skill) / kProfilePercentageScale;
	in.primary = true;
	in.currentWeapon = static_cast<std::uint16_t>(weapon.weaponId);
	in.weaponObservationTick = weapon.weaponTick;
	in.clipKnown = true;
	in.clip = static_cast<unsigned>(weapon.clip);
	in.attacking = attacking;
	if (definition.ammoIndex >= 0 && definition.ammoIndex < kAmmoTypeCount)
	{
		in.reserveKnown = weapon.ammoKnown[definition.ammoIndex];
		in.reserve = static_cast<unsigned>(weapon.ammo[definition.ammoIndex]);
	}
	in.defusing = actors_[ctx.index].managedBotStateMachines.state() == compat::CompatibilityStateId::DefuseBomb;
	in.reloading = (ctx.observation.weapon.reloadState.isAvailable() &&
					   ctx.observation.weapon.reloadState.value == combat::ReloadState::Reloading) ||
		store.pendingReloadRequest(ctx.actor, ctx.frame, in.now);
	const auto *engine = services_->service_engineFunctions();
	if (engine && engine->pfnCVarGetPointer && engine->pfnCVarGetPointer("bot_allow_pistols"))
	{
		const float allow =
			runtime_detail::readOptionalCvarFloat(services_->service_engineFunctions(), "bot_allow_pistols");
		in.allowPistols = !std::isfinite(allow) || allow != 0;
	}
	const auto inventory = static_cast<std::uint32_t>(ctx.handle->entity->v.weapons);
	in.hasKnife = (inventory & (1U << kKnifeWeaponId)) != 0;
	for (const unsigned id : kPistolWeaponIds)
	{
		if ((inventory & (1U << id)) == 0)
			continue;
		const auto pistol = store.definition(id);
		const bool reserveKnown = pistol.known && pistol.ammoIndex >= 0 && pistol.ammoIndex < kAmmoTypeCount &&
			weapon.ammoKnown[pistol.ammoIndex];
		const bool clipKnown = weapon.weaponClipKnown[id] && weapon.weaponClipTicks[id] <= ctx.frame.tick;
		const auto ammo =
			(clipKnown && weapon.weaponClips[id] > 0) || (reserveKnown && weapon.ammo[pistol.ammoIndex] > 0)
			? compat::EquipmentAmmo::Usable
			: clipKnown && weapon.weaponClips[id] == 0 && reserveKnown ? compat::EquipmentAmmo::Empty
																	   : compat::EquipmentAmmo::Unknown;
		if (in.pistolWeapon == 0 || ammo == compat::EquipmentAmmo::Usable ||
			(in.pistolAmmo == compat::EquipmentAmmo::Empty && ammo == compat::EquipmentAmmo::Unknown))
		{
			in.pistolWeapon = static_cast<std::uint16_t>(id);
			in.pistolAmmo = ammo;
		}
		if (ammo == compat::EquipmentAmmo::Usable)
			break;
	}
	const auto decision = actors_[ctx.index].equipment.update(in);
	if (!decision.ownsAction)
		return false;
	ctx.proposal.kind = decision.selectWeapon == kKnifeWeaponId ? ActionKind::SelectKnife
		: decision.selectWeapon != 0							? ActionKind::SelectPistol
																: ActionKind::None;
	ctx.proposal.equipmentWeaponId = decision.selectWeapon;
	ctx.proposal.secondaryAttack = false;
	ctx.proposal.movementButtons =
		static_cast<std::uint16_t>(ctx.proposal.movementButtons & ~(IN_ATTACK | IN_ATTACK2 | IN_RELOAD));
	actors_[ctx.index].weaponActions.stopRapidFire();
	return true;
}

void BotBehaviorRuntime::observeEquipmentDispatched(std::size_t index, std::uint16_t weapon, bool success)
{
	if (index >= actors_.size() || !services_->service_globals())
		return;
	const auto &handle = services_->service_managedBotHandles()[index];
	actors_[index].equipment.dispatched({handle.actor.slot, handle.actor.actorGeneration},
		{services_->service_lifecycle().mapGeneration(), services_->service_lifecycle().roundGeneration(),
			services_->service_adapterFrameCount()},
		services_->service_globals()->time, weapon, success);
}
}
}
