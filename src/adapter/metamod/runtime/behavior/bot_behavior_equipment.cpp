#include "runtime/behavior/bot_behavior_runtime.hpp"
#include "runtime/shared/bot_runtime_support.hpp"
namespace astrabot
{
namespace metamod
{
bool BotBehaviorRuntime::decideReloadAction(ManagedActionContext &ctx)
{
	if (services_->service_compatibilitySurface().configuration().mode != compat::RuntimeMode::Compatibility)
		return false;
	const auto &actor = actors_[ctx.index];
	const auto &machine = actor.managedBotStateMachines;
	if (machine.state() == compat::CompatibilityStateId::DefuseBomb ||
		(ctx.observation.objective.defusing.isAvailable() && ctx.observation.objective.defusing.value))
		return false;
	const auto &store = services_->observation_publicObservations();
	compat::PublicActorState weapon{};
	if (!store.read(ctx.actor, ctx.frame, &weapon) || !weapon.weaponKnown)
		return false;

	// Counter-Strike's ItemInfo capacities by weapon ID. Non-magazine items use zero.
	static constexpr unsigned capacities[] = {
		0, 13, 0, 10, 0, 7, 0, 30, 30, 0, 30, 20, 25, 30, 35, 25,
		12, 20, 10, 30, 100, 8, 30, 30, 20, 0, 7, 30, 30, 0, 50,
	};
	if (weapon.weaponId <= 0 || static_cast<unsigned>(weapon.weaponId) >= sizeof(capacities) / sizeof(*capacities))
		return false;
	const auto receipt = reactionContext(ctx.index);
	const auto execution = actor.executionRevision;
	const auto epoch = machine.normalStateEpoch();
	const auto current = [&] {
		return executionCurrent(ctx.index, receipt, execution) && machine.normalStateEpoch() == epoch;
	};
	const float timeSinceEnemy = receipt.now - actor.lastVisibleEnemyTimes;
	const bool attacking = machine.attackOverlayActive();
	// Native footstep equipment requests precede the ordinary ReloadCheck.
	if (ctx.closeFootstepDistance >= 0 && decideEquipmentAction(ctx, weapon, attacking))
		return true;
	if (!current())
		return false;
	const bool skilledAttack = attacking && actor.managedProfiles.skill > 50;
	if (!compat::nativeMagazineNeedsReload(weapon.clip, capacities[weapon.weaponId], timeSinceEnemy,
			skilledAttack, weapon.weaponId == 18))
		return false;
	const auto &reload = ctx.observation.weapon.reloadState;
	if ((reload.isAvailable() && reload.value == combat::ReloadState::Reloading) ||
		store.pendingReloadRequest(ctx.actor, ctx.frame, receipt.now))
		return false;
	const auto *engine = services_->service_engineFunctions();
	if (!engine || !engine->pfnPEntityOfEntIndex)
		return false;
	constexpr int kTerroristTeam = 1, kCounterTerroristTeam = 2;
	const int team = services_->observation_resolvePlayerTeam(ctx.handle->entity);
	if (!current() || (team != kTerroristTeam && team != kCounterTerroristTeam))
		return false;
	const auto living = livingRadioPeers(ctx.handle->entity, team);
	if (!current() || !living.enemies)
		return false;
	if (weapon.clip == 0 && decideEquipmentAction(ctx, weapon, attacking))
		return true;
	if (!current())
		return false;
	const bool movingToCover = tryReloadCover(ctx.index, team, living.enemies, timeSinceEnemy);
	if (!movingToCover && !current())
		return false;
	ctx.proposal.kind = ActionKind::Reload;
	ctx.proposal.secondaryAttack = false;
	ctx.proposal.movementButtons = static_cast<std::uint16_t>(
		ctx.proposal.movementButtons & ~(IN_ATTACK | IN_ATTACK2));
	if (movingToCover)
		ctx.proposal.movementButtons = static_cast<std::uint16_t>(ctx.proposal.movementButtons & ~IN_DUCK);
	actors_[ctx.index].weaponActions.stopRapidFire();
	return true;
}

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
	if (ctx.proposal.kind == ActionKind::SelectPistol && ctx.proposal.equipmentWeaponId != 0)
	{
		return true;
	}
	const bool closeFootstep = ctx.closeFootstepDistance >= 0;
	if (!weapon.weaponKnown || !definition.known || definition.slot != kPrimaryWeaponSlot ||
		(!closeFootstep && (weapon.clip < 0 || weapon.clip != 0)))
		return false;
	compat::EquipmentInput in{};
	in.actor = ctx.actor;
	in.frame = ctx.frame;
	in.now = services_->service_globals()->time;
	in.skill = static_cast<float>(actors_[ctx.index].managedProfiles.skill) / kProfilePercentageScale;
	in.primary = true;
	in.currentWeapon = static_cast<std::uint16_t>(weapon.weaponId);
	in.weaponObservationTick = weapon.weaponTick;
	in.closeFootstepDistance = ctx.closeFootstepDistance;
	in.clipKnown = weapon.clip >= 0;
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

void BotBehaviorRuntime::observeCloseFootstep(std::size_t index, const compat::FootstepEquipmentReceipt &receipt)
{
	if (!services_ || !services_->service_globals() || index >= actors_.size())
	{
		return;
	}
	const auto config = services_->service_compatibilitySurface().configuration();
	const auto &handle = services_->service_managedBotHandles()[index];
	const world::ActorKey actor{handle.actor.slot, handle.actor.actorGeneration};
	const world::FrameIdentity frame{services_->service_lifecycle().mapGeneration(),
		services_->service_lifecycle().roundGeneration(), services_->service_adapterFrameCount()};
	const float now = services_->service_globals()->time;
	if (config.mode != compat::RuntimeMode::Compatibility || config.botEnable <= 0 || config.botStop > 0 ||
		!handle.entity || handle.entity->free || handle.entity->v.deadflag != DEAD_NO || handle.entity->v.health <= 0 ||
		(handle.entity->v.flags & (FL_FROZEN | FL_SPECTATOR)) != 0 || !receipt.current(actor, frame, now) ||
		(services_->service_roundFreezeGeneration() == frame.roundGeneration && now < services_->service_roundFreezeUntil()))
	{
		return;
	}
	actors_[index].closeFootstep = receipt;
}

void BotBehaviorRuntime::observeEquipmentDispatched(std::size_t index, std::uint16_t weapon, bool success)
{
	if (index >= actors_.size() || !services_->service_globals())
		return;
	const auto &handle = services_->service_managedBotHandles()[index];
	actors_[index].closeFootstep = {};
	actors_[index].equipment.dispatched({handle.actor.slot, handle.actor.actorGeneration},
		{services_->service_lifecycle().mapGeneration(), services_->service_lifecycle().roundGeneration(),
			services_->service_adapterFrameCount()},
		services_->service_globals()->time, weapon, success);
}
}
}
