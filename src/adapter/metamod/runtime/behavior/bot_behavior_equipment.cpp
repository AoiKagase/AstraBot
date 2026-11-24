#include "runtime/behavior/bot_behavior_runtime.hpp"
#include "runtime/shared/bot_runtime_support.hpp"
#include "astrabot/compat/weapon_pickup.hpp"
namespace astrabot
{
namespace metamod
{
bool BotBehaviorRuntime::updateUnderwaterEquipment(std::size_t index)
{
	const auto &config = services_->service_compatibilitySurface().configuration();
	auto &actor = actors_[index];
	const auto &handle = services_->service_managedBotHandles()[index];
	constexpr float kMinimumSkill = .4f;
	const float skill = static_cast<float>(actor.managedProfiles.skill) / 100.0f;
	if (config.mode != compat::RuntimeMode::Compatibility || config.botEnable <= 0 || config.botStop != 0 ||
		!handle.entity || handle.entity->v.waterlevel != 3 || !(skill > kMinimumSkill))
		return true;
	const auto receipt = reactionContext(index);
	const auto revision = actor.executionRevision;
	const auto full = services_->service_managedBotFullUpdateSequences()[index];
	const auto normal = actor.managedBotStateMachines.normalStateEpoch();
	const auto attack = actor.managedBotStateMachines.attackEntryEpoch();
	const bool attacking = actor.managedBotStateMachines.attackOverlayActive();
	const auto *entity = handle.entity;
	const auto serial = entity->serialnumber;
	const auto inventory = entity->v.weapons;
	const auto current = [&] {
		return executionCurrent(index, receipt, revision) && handle.entity == entity && !entity->free &&
			entity->serialnumber == serial && entity->v.waterlevel == 3 && entity->v.weapons == inventory &&
			services_->service_managedBotFullUpdateSequences()[index] == full &&
			actor.managedBotStateMachines.normalStateEpoch() == normal &&
			actor.managedBotStateMachines.attackEntryEpoch() == attack &&
			actor.managedBotStateMachines.attackOverlayActive() == attacking;
	};
	int weapon = 0;
	const bool predicted = services_->observation_activeWeaponIdentity(handle.entity, weapon);
	if (!current())
		return false;
	const auto &store = services_->observation_publicObservations();
	if (!predicted)
	{
		compat::PublicActorState observed{};
		if (!store.read(receipt.actor, receipt.frame, &observed) || !observed.weaponKnown)
			return true;
		weapon = observed.weaponId;
	}
	const auto definition = store.definition(weapon);
	const bool forbidden = definition.known ? (definition.flags & (1 << 5)) != 0
		: weapon == 5 || weapon == 14 || weapon == 15 || weapon == 21;
	// CBot treats a known absence of an active item as unable to fire, too.
	if (weapon == 0 || forbidden)
		(void)equipBestWeapon(index, true);
	return current();
}

bool BotBehaviorRuntime::updateUnsafeEquipment(std::size_t index)
{
	const auto &config = services_->service_compatibilitySurface().configuration();
	auto &actor = actors_[index];
	const auto &handle = services_->service_managedBotHandles()[index];
	if (config.mode != compat::RuntimeMode::Compatibility || config.botEnable <= 0 || config.botStop != 0 ||
		!handle.entity)
		return true;
	const auto receipt = reactionContext(index);
	const float start = services_->service_roundFreezeGeneration() == receipt.frame.roundGeneration
		? services_->service_roundFreezeStartTime() : 0.0f;
	const auto unsafe = [&] {
		return !actor.enemyReaction.isSafe(receipt.now - start,
			static_cast<float>(actor.managedProfiles.aggression) / 100.0f);
	};
	if (!unsafe())
		return true;
	const auto revision = actor.executionRevision;
	const auto full = services_->service_managedBotFullUpdateSequences()[index];
	const auto normal = actor.managedBotStateMachines.normalStateEpoch();
	const auto attack = actor.managedBotStateMachines.attackEntryEpoch();
	const bool attacking = actor.managedBotStateMachines.attackOverlayActive();
	const auto *entity = handle.entity;
	const auto serial = entity->serialnumber;
	const auto inventory = entity->v.weapons;
	const auto water = entity->v.waterlevel;
	const auto current = [&] {
		return executionCurrent(index, receipt, revision) && handle.entity == entity && !entity->free &&
			entity->serialnumber == serial && entity->v.weapons == inventory && entity->v.waterlevel == water &&
			services_->service_managedBotFullUpdateSequences()[index] == full &&
			actor.managedBotStateMachines.normalStateEpoch() == normal &&
			actor.managedBotStateMachines.attackEntryEpoch() == attack &&
			actor.managedBotStateMachines.attackOverlayActive() == attacking && unsafe();
	};
	const auto &store = services_->observation_publicObservations();
	compat::PublicActorState observed{};
	const bool publicKnown = store.read(receipt.actor, receipt.frame, &observed);
	int weapon = 0;
	const bool predicted = services_->observation_activeWeaponIdentity(handle.entity, weapon);
	if (!current())
		return false;
	if (!predicted)
	{
		if (!publicKnown || !observed.weaponKnown)
			return true;
		weapon = observed.weaponId;
	}
	if (weapon == 4 || weapon == 9 || weapon == 25)
		return true;
	if (weapon == 29 && actor.managedBotStateMachines.state() == compat::CompatibilityStateId::EscapeFromBomb)
		return true;
	bool rearm = weapon == 0 || weapon == 29;
	if (!rearm && weapon > 0 && static_cast<std::size_t>(weapon) < compat::PublicActorState::kWeaponStateCapacity)
	{
		std::array<compat::EquipmentAmmo, compat::PublicActorState::kWeaponStateCapacity> ammo{};
		const bool ammoPredicted = services_->observation_equipmentAmmo(handle.entity, ammo);
		if (!current())
			return false;
		auto activeAmmo = ammoPredicted ? ammo[static_cast<std::size_t>(weapon)] : compat::EquipmentAmmo::Unknown;
		if (activeAmmo == compat::EquipmentAmmo::Unknown && publicKnown)
		{
			const auto definition = store.definition(weapon);
			const bool reserveKnown = definition.known && definition.ammoIndex >= 0 &&
				static_cast<std::size_t>(definition.ammoIndex) < observed.ammoKnown.size() &&
				observed.ammoKnown[static_cast<std::size_t>(definition.ammoIndex)];
			activeAmmo = compat::nativeEquipmentAmmo(observed.weaponClipKnown[weapon], observed.weaponClips[weapon],
				reserveKnown, reserveKnown && observed.ammo[static_cast<std::size_t>(definition.ammoIndex)] > 0 ? 1U : 0U);
		}
		// A prediction callback can select another item without changing the inventory mask.
		int active = 0;
		const bool activeKnown = services_->observation_activeWeaponIdentity(handle.entity, active);
		if (!current() || (activeKnown && active != weapon))
			return false;
		rearm = activeAmmo == compat::EquipmentAmmo::Empty;
	}
	bool admitted = true;
	if (rearm)
		(void)equipBestWeapon(index, false, [&](int active) {
			admitted = admitted && current() && active == weapon;
			return admitted;
		});
	return current() && admitted;
}

bool BotBehaviorRuntime::equipBestWeapon(std::size_t index, bool mustEquip,
	const std::function<bool(int)> &admission)
{
	if (index >= actors_.size() || !services_->service_globals() ||
		services_->service_compatibilitySurface().configuration().mode != compat::RuntimeMode::Compatibility)
		return false;
	auto &actor = actors_[index];
	const auto receipt = reactionContext(index);
	if (!actor.equipment.canEquip(receipt.actor, receipt.frame, receipt.now, mustEquip))
		return false;
	const auto &handle = services_->service_managedBotHandles()[index];
	if (!handle.entity || !receipt.alive)
		return false;
	const auto execution = actor.executionRevision;
	const auto epoch = actor.managedBotStateMachines.normalStateEpoch();
	const auto attack = actor.managedBotStateMachines.attackEntryEpoch();
	const auto attacking = actor.managedBotStateMachines.attackOverlayActive();
	const auto *entity = handle.entity;
	const auto serial = entity->serialnumber;
	const auto inventory = static_cast<std::uint32_t>(handle.entity->v.weapons);
	const auto current = [&] {
		return executionCurrent(index, receipt, execution) &&
			handle.entity == entity && !entity->free && entity->serialnumber == serial &&
			actor.managedBotStateMachines.normalStateEpoch() == epoch &&
			actor.managedBotStateMachines.attackEntryEpoch() == attack &&
			actor.managedBotStateMachines.attackOverlayActive() == attacking &&
			static_cast<std::uint32_t>(handle.entity->v.weapons) == inventory;
	};
	const auto &store = services_->observation_publicObservations();
	compat::PublicActorState known{};
	(void)store.read(receipt.actor, receipt.frame, &known);
	std::array<compat::EquipmentAmmo, compat::PublicActorState::kWeaponStateCapacity> ammo{};
	const bool predicted = services_->observation_equipmentAmmo(handle.entity, ammo);
	if (!current())
		return false;
	int activeWeapon = 0;
	const bool activeKnown = services_->observation_activeWeaponIdentity(handle.entity, activeWeapon);
	if (!current())
		return false;
	auto *engine = services_->service_engineFunctions();
	const auto allowed = [&](const char *name) {
		if (!engine || !engine->pfnCVarGetPointer || !engine->pfnCVarGetPointer(name))
			return true;
		const auto value = runtime_detail::readOptionalCvarFloat(engine, name);
		return !std::isfinite(value) || value != 0;
	};
	const auto makeCandidate = [&](unsigned id, const char *permission) {
		compat::EquipmentCandidate candidate{};
		candidate.weapon = static_cast<std::uint16_t>(id);
		candidate.allowed = allowed(permission);
		const auto definition = store.definition(static_cast<int>(id));
		candidate.noFireUnderwater = definition.known ? (definition.flags & (1 << 5)) != 0
			: id == 5 || id == 14 || id == 15 || id == 21;
		if (predicted && ammo[id] != compat::EquipmentAmmo::Unknown)
			candidate.ammo = ammo[id];
		else
		{
			const bool reserveKnown = definition.known && definition.ammoIndex >= 0 &&
				static_cast<std::size_t>(definition.ammoIndex) < known.ammoKnown.size() &&
				known.ammoKnown[static_cast<std::size_t>(definition.ammoIndex)];
			candidate.ammo = compat::nativeEquipmentAmmo(known.weaponClipKnown[id], known.weaponClips[id],
				reserveKnown, reserveKnown && known.ammo[static_cast<std::size_t>(definition.ammoIndex)] > 0 ? 1U : 0U);
		}
		return candidate;
	};
	compat::BestEquipmentInput input{};
	input.actor = receipt.actor;
	input.frame = receipt.frame;
	input.now = receipt.now;
	input.mustEquip = mustEquip;
	input.skill = static_cast<float>(actor.managedProfiles.skill) / 100.0f;
	input.fullySubmerged = handle.entity->v.waterlevel == 3;
	input.currentWeapon = static_cast<std::uint16_t>(activeKnown ? activeWeapon : known.weaponKnown ? known.weaponId : 0);
	if (admission && !admission(input.currentWeapon))
		return false;
	input.hasKnife = (inventory & (1U << 29)) != 0;
	const auto *primary = compat::carriedPrimaryWeapon(inventory);
	if (primary)
	{
		const char *permission = "bot_allow_rifles";
		switch (primary->id)
		{
		case 3: case 13: case 18: case 24: permission = "bot_allow_snipers"; break;
		case 5: case 21: permission = "bot_allow_shotguns"; break;
		case 7: case 12: case 19: case 23: case 30: permission = "bot_allow_sub_machine_guns"; break;
		case 20: permission = "bot_allow_machine_guns"; break;
		default: break;
		}
		input.primary = makeCandidate(primary->id, permission);
	}
	else
		for (const auto &weapon : compat::kPickupPrimaryWeapons)
			if (inventory & (1U << weapon.id))
				return false; // Public masks cannot establish a multi-primary linked-list head.
	constexpr unsigned pistolIds[] = {1, 10, 11, 16, 17, 26};
	constexpr const char *pistolNames[] = {
		"weapon_p228", "weapon_elite", "weapon_fiveseven", "weapon_usp", "weapon_glock18", "weapon_deagle"};
	const char *pistolName = nullptr;
	for (unsigned i = 0; i < 6; ++i)
		if (inventory & (1U << pistolIds[i]))
		{
			if (pistolName)
				return false;
			input.pistol = makeCandidate(pistolIds[i], "bot_allow_pistols");
			pistolName = pistolNames[i];
		}
	if (!current())
		return false;
	if (admission)
	{
		// Caller conditions must survive prediction and permission callbacks, too.
		int active = 0;
		const bool fresh = services_->observation_activeWeaponIdentity(handle.entity, active);
		if (!current())
			return false;
		if (!fresh)
		{
			compat::PublicActorState observed{};
			if (!store.read(receipt.actor, receipt.frame, &observed) || !observed.weaponKnown)
				return false;
			active = observed.weaponId;
		}
		if (!admission(active))
			return false;
	}
	const auto decision = actor.equipment.best(input);
	if (!decision.selectWeapon)
		return false;
	const auto *command = decision.selectWeapon == 29 ? "weapon_knife"
		: primary && decision.selectWeapon == primary->id ? primary->classname : pistolName;
	if (!command)
		return false;
	const bool sent = services_->command_dispatchClientCommand(handle.entity, command, "");
	if (!current() || (admission && !admission(input.currentWeapon)))
		return false;
	actor.equipment.equipped(receipt.actor, receipt.frame, receipt.now, decision.selectWeapon, sent);
	if (services_->service_runtimeProfiler().enabled())
		runtime_detail::profiledLogConsole(services_->service_runtimeProfiler(), services_->service_pluginId(),
			"profile equipBest slot=%u now=%.6f old=%u selected=%u sent=%d primary_ammo=%u pistol_ammo=%u",
			receipt.actor.slot, receipt.now, input.currentWeapon, decision.selectWeapon, sent,
			static_cast<unsigned>(input.primary.ammo), static_cast<unsigned>(input.pistol.ammo));
	return sent;
}

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
