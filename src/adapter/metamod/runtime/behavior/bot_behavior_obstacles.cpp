#include "runtime/behavior/bot_behavior_runtime.hpp"
#include "astrabot/compat/obstacle_interaction.hpp"
#include "runtime/shared/bot_runtime_support.hpp"
#include <cstdlib>
#include <cstring>
namespace astrabot
{
namespace metamod
{
namespace
{
constexpr int kUnbreakableGlass = 7;
constexpr float kHalfHumanHeight = 36.0f;
constexpr float kLookDuration = 0.2f;
constexpr float kLookTolerance = 5.0f;
constexpr float kEquipInterval = 5.0f;
constexpr float kDegreesPerRadian = 57.29577951308232f;

const char *weaponAllowance(unsigned id)
{
	switch (id)
	{
	case 3:
	case 13:
	case 18:
	case 24:
		return "bot_allow_snipers";
	case 5:
	case 21:
		return "bot_allow_shotguns";
	case 19:
	case 7:
	case 12:
	case 23:
	case 30:
		return "bot_allow_sub_machine_guns";
	case 20:
		return "bot_allow_machine_guns";
	default:
		return "bot_allow_rifles";
	}
}

const char *obstacleWeapon(enginefuncs_t *engine, std::uint32_t inventory, const compat::PublicObservationStore &store,
	const compat::PublicActorState &weapon)
{
	const auto allowed = [engine](const char *name)
	{
		if (!engine || !engine->pfnCVarGetPointer || !engine->pfnCVarGetPointer(name))
			return true;
		const float value = runtime_detail::readOptionalCvarFloat(engine, name);
		return !std::isfinite(value) || value != 0.0f;
	};
	const auto usable = [&store, &weapon](unsigned id)
	{
		const auto definition = store.definition(static_cast<int>(id));
		const bool reserveKnown = definition.known && definition.ammoIndex >= 0 &&
			static_cast<std::size_t>(definition.ammoIndex) < weapon.ammoKnown.size() &&
			weapon.ammoKnown[static_cast<std::size_t>(definition.ammoIndex)];
		// Unknown ammo cannot establish HasAnyAmmo=false; never synthesize a loaded weapon.
		return !(weapon.weaponClipKnown[id] && weapon.weaponClips[id] == 0 && reserveKnown &&
			weapon.ammo[static_cast<std::size_t>(definition.ammoIndex)] == 0);
	};
	for (const unsigned id : {3U, 5U, 7U, 8U, 12U, 13U, 14U, 15U, 18U, 19U, 20U, 21U, 22U, 23U, 24U, 27U, 28U, 30U})
		if ((inventory & (1U << id)) && usable(id) && allowed(weaponAllowance(id)))
			return "slot1";
	if (allowed("bot_allow_pistols"))
		for (const unsigned id : {1U, 10U, 11U, 16U, 17U, 26U})
			if ((inventory & (1U << id)) && usable(id))
				return "slot2";
	return "weapon_knife";
}
}

void BotBehaviorRuntime::observeObstacleKeyValue(edict_t *entity, const KeyValueData *data)
{
	if (!services_ || !entity || !data || !data->szKeyName || !data->szValue)
		return;
	const auto engine = services_->service_engineFunctions();
	if (!engine || !engine->pfnIndexOfEdict)
		return;
	const int slot = engine->pfnIndexOfEdict(entity);
	if (slot < 0 || static_cast<std::size_t>(slot) >= breakableMaterials_.size())
		return;
	auto &record = breakableMaterials_[static_cast<std::size_t>(slot)];
	if (std::strcmp(data->szKeyName, "classname") == 0)
	{
		record = {};
		if (std::strcmp(data->szValue, "func_breakable") == 0)
			record = {entity, entity->serialnumber, 0};
		return;
	}
	if (!data->szClassName || std::strcmp(data->szClassName, "func_breakable") != 0 ||
		std::strcmp(data->szKeyName, "material") != 0)
		return;
	char *end = nullptr;
	const long material = std::strtol(data->szValue, &end, 10);
	record = {entity, entity->serialnumber,
		end != data->szValue && *end == '\0' && material >= 0 && material <= 9 ? static_cast<int>(material) : -1};
}

void BotBehaviorRuntime::resetObstacleMaterials()
{
	breakableMaterials_ = {};
	for (auto &actor : actors_)
	{
		actor.obstacleLook = {};
		actor.obstacleNextEquipAt = 0.0f;
		actor.obstaclePathActive = false;
	}
}

void BotBehaviorRuntime::observeObstacleTouch(edict_t *entity, edict_t *other)
{
	if (!services_ || !entity || !other || entity == other || entity->free || other->free)
		return;
	const auto engine = services_->service_engineFunctions();
	const auto globals = services_->service_globals();
	const auto config = services_->service_compatibilitySurface().configuration();
	if (!engine || !engine->pfnSzFromIndex || !engine->pfnIndexOfEdict || !globals || !std::isfinite(globals->time) ||
		config.mode != compat::RuntimeMode::Compatibility || config.botEnable <= 0 || config.botStop > 0)
		return;
	const auto observe = [&](edict_t *self, edict_t *contact)
	{
		if (!(self->v.flags & FL_CLIENT) || (contact->v.flags & FL_CLIENT) || self->v.deadflag != DEAD_NO ||
			self->v.health <= 0 || (self->v.flags & FL_FROZEN) || contact->v.takedamage != DAMAGE_YES ||
			contact->v.health <= 0)
			return;
		const char *name = engine->pfnSzFromIndex(contact->v.classname);
		if (!name || std::strcmp(name, "func_breakable") != 0)
			return;
		const int entityIndex = engine->pfnIndexOfEdict(contact);
		if (entityIndex < 0 || static_cast<std::size_t>(entityIndex) >= breakableMaterials_.size())
			return;
		const auto &material = breakableMaterials_[static_cast<std::size_t>(entityIndex)];
		if (material.entity != contact || material.serial != contact->serialnumber || material.material < 0 ||
			material.material == kUnbreakableGlass)
			return;
		for (std::size_t i = 0; i < actors_.size(); ++i)
		{
			const auto &handle = services_->service_managedBotHandles()[i];
			if (!services_->service_managedBotSlots()[i] || handle.entity != self)
				continue;
			auto &actor = actors_[i];
			if (actor.managedBotStateMachines.attackOverlayActive())
				return;
			const auto &lo = contact->v.absmin, &hi = contact->v.absmax;
			const auto &goal = actor.obstaclePathGoal;
			if (services_->navigation_hasActivePath(i) && actor.obstaclePathActive &&
				!compat::breakableBlocksPath({self->v.origin.x, self->v.origin.y, self->v.origin.z},
					{goal.x, goal.y, goal.z + kHalfHumanHeight}, {lo.x, lo.y, lo.z}, {hi.x, hi.y, hi.z}))
				return;
			const nav::NavVector point = {(lo.x + hi.x) * 0.5f, (lo.y + hi.y) * 0.5f, (lo.z + hi.z) * 0.5f};
			if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z))
				return;
			auto &look = actor.obstacleLook;
			if (look.entity != contact || look.serial != contact->serialnumber ||
				!services_->service_lifecycle().isCurrent(look.lifecycle))
			{
				look = {};
				look.entity = contact;
				look.serial = contact->serialnumber;
				look.lifecycle = services_->service_lifecycle().tokenForSlot(handle.actor.slot);
				look.angles = {self->v.v_angle.x, self->v.v_angle.y, self->v.v_angle.z};
			}
			if (look.alignedAt >= 0 && globals->time - look.alignedAt > kLookDuration)
				look.alignedAt = -1.0f;
			look.point = point;
			look.touchedAt = globals->time;
			look.pendingAttack = true;
			look.attackUpdate = services_->service_managedBotFullUpdateSequences()[i];
			return;
		}
	};
	observe(entity, other);
	observe(other, entity);
}

void BotBehaviorRuntime::applyObstacleCommand(std::size_t index, runtime::BotCommand &command)
{
	if (index >= actors_.size() || !services_ || !actors_[index].obstacleLook.entity)
		return;
	auto &actor = actors_[index];
	auto &look = actor.obstacleLook;
	const auto &handle = services_->service_managedBotHandles()[index];
	auto *self = handle.entity;
	auto *target = look.entity;
	const auto globals = services_->service_globals();
	const auto config = services_->service_compatibilitySurface().configuration();
	if (!self || !globals || !services_->service_lifecycle().isCurrent(look.lifecycle) || target->free ||
		target->serialnumber != look.serial || target->v.takedamage != DAMAGE_YES || target->v.health <= 0 ||
		self->v.deadflag != DEAD_NO || self->v.health <= 0 || (self->v.flags & FL_FROZEN) ||
		actor.managedBotStateMachines.attackOverlayActive() || config.mode != compat::RuntimeMode::Compatibility ||
		config.botEnable <= 0 || config.botStop > 0 || !std::isfinite(globals->time) ||
		globals->time < look.touchedAt ||
		(!look.pendingAttack && look.alignedAt >= 0 && globals->time - look.alignedAt > kLookDuration))
	{
		look = {};
		return;
	}
	const auto engine = services_->service_engineFunctions();
	const int materialIndex = engine && engine->pfnIndexOfEdict ? engine->pfnIndexOfEdict(target) : -1;
	if (materialIndex < 0 || static_cast<std::size_t>(materialIndex) >= breakableMaterials_.size())
	{
		look = {};
		return;
	}
	const auto &material = breakableMaterials_[static_cast<std::size_t>(materialIndex)];
	if (material.entity != target || material.serial != look.serial || material.material < 0 ||
		material.material == kUnbreakableGlass)
	{
		look = {};
		return;
	}
	// Public objective interaction keeps ownership of USE and C4 attack.
	if (command.movement.buttons & IN_USE)
		return;
	const world::FrameIdentity frame{services_->service_lifecycle().mapGeneration(),
		services_->service_lifecycle().roundGeneration(), services_->service_adapterFrameCount()};
	compat::PublicActorState weapon{};
	const bool knownWeapon = services_->observation_publicObservations().read(
								 {handle.actor.slot, handle.actor.actorGeneration}, frame, &weapon) &&
		weapon.weaponKnown;
	if (knownWeapon && weapon.weaponId == 6)
		return;
	// A new contact attack survives a look timer crossing between Touch and this command.
	if (look.pendingAttack && look.alignedAt >= 0 && globals->time - look.alignedAt > kLookDuration)
		look.alignedAt = -1.0f;
	const float dx = look.point.x - self->v.origin.x - self->v.view_ofs.x;
	const float dy = look.point.y - self->v.origin.y - self->v.view_ofs.y;
	const float dz = look.point.z - self->v.origin.z - self->v.view_ofs.z;
	const float yaw = kDegreesPerRadian * std::atan2(dy, dx);
	const float pitch = -kDegreesPerRadian * std::atan2(dz, std::hypot(dx, dy));
	recordLookGoal(index, yaw);
	const auto aim = services_->service_compatibilitySurface().configuration().mode == compat::RuntimeMode::Compatibility
		? services_->command_stepLook(index, globals->time, self->v.v_angle.y, self->v.v_angle.x, yaw, pitch, false, 7U)
		: compat::trackAim(look.angles.yaw, look.angles.pitch, yaw, pitch,
		runtime::BotTimingScheduler::kCommandInterval, 1.0f, &look.spring, false);
	look.angles = {aim.pitch, aim.yaw, 0};
	if (look.alignedAt < 0 && std::fabs(compat::normalizedAimAngle(yaw - aim.yaw)) <= kLookTolerance &&
		std::fabs(compat::normalizedAimAngle(pitch - aim.pitch)) <= kLookTolerance)
		look.alignedAt = globals->time;
	const auto move = ActionAdapter::reprojectMovement(
		command.movement.forward, command.movement.side, command.viewAngles.yaw, aim.yaw);
	command.movement.forward = move.forward;
	command.movement.side = move.side;
	command.movement.buttons =
		static_cast<std::uint16_t>((command.movement.buttons & ~(IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT)) |
			ActionAdapter::movementButtons(move.forward, move.side));
	command.viewAngles = look.angles;
	command.synchronizeCombatView = true;
	const bool attack =
		look.pendingAttack || look.attackUpdate == services_->service_managedBotFullUpdateSequences()[index];
	look.pendingAttack = false;
	if (!knownWeapon || !attack)
		return;
	if (compat::WeaponActionPlanner::isGrenade(static_cast<std::uint16_t>(weapon.weaponId)))
	{
		command.movement.buttons = static_cast<std::uint16_t>(command.movement.buttons & ~(IN_ATTACK | IN_ATTACK2));
		if (globals->time >= actor.obstacleNextEquipAt)
		{
			actor.obstacleNextEquipAt = globals->time + kEquipInterval;
			services_->command_dispatchClientCommand(self,
				obstacleWeapon(services_->service_engineFunctions(), static_cast<std::uint32_t>(self->v.weapons),
					services_->observation_publicObservations(), weapon),
				"");
		}
		return;
	}
	command.movement.buttons = static_cast<std::uint16_t>(command.movement.buttons | IN_ATTACK);
}
}
}
