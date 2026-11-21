#include "runtime/command/bot_command_runtime.hpp"
#include "runtime/shared/bot_runtime_support.hpp"
namespace astrabot
{
namespace metamod
{
namespace
{
// Keep literal evaluation precision on x87; each macro is undefined below.
#define ASTRABOT_BOT_COMMAND_RUNTIME_DEGREES_TO_RADIANS 0.017453292519943295f
constexpr float kModelPitchDivisor = 3.0f;
constexpr float kMovementGateLogIntervalSeconds = 1.0f;
#define ASTRABOT_BOT_COMMAND_RUNTIME_FREEZE_MOVEMENT_INPUT_TOLERANCE 0.01f
constexpr float kMinimumUsableMaxSpeed = 1.0f;
constexpr auto kFreezeDiagnosticFrameInterval = 6U;
constexpr auto kMaximumMovementDiagnosticSamples = 4U;

bool usesApproachLook(compat::CompatibilityStateId state)
{
	return state == compat::CompatibilityStateId::Hunt || state == compat::CompatibilityStateId::FetchBomb;
}

bool usesNavigationLook(compat::CompatibilityStateId state)
{
	return usesApproachLook(state) || state == compat::CompatibilityStateId::Hide ||
		state == compat::CompatibilityStateId::InvestigateNoise;
}

bool isDefuseApproach(const compat::CompatibilityStateMachine &machine)
{
	return machine.state() == compat::CompatibilityStateId::MoveTo &&
		machine.task() == compat::CompatibilityTaskId::DefuseBomb;
}

bool canAdvanceNavigationLook(compat::CompatibilityStateId state, bool grounded)
{
	// An airborne Hunt command retains its takeoff view while the route continues.
	return state != compat::CompatibilityStateId::Hunt || grounded;
}

bool clearsHuntApproachLook(compat::CompatibilityStateId state)
{
	return state == compat::CompatibilityStateId::PlantBomb || state == compat::CompatibilityStateId::DefuseBomb ||
		state == compat::CompatibilityStateId::UseEntity;
}

float nativeLookAheadYaw(const nav::NavVector &direction)
{
	if (direction.x == 0.0f && direction.y == 0.0f)
		return 0.0f;
	const double yaw = std::floor(std::atan2(static_cast<double>(direction.y), static_cast<double>(direction.x)) *
		180.0 / 3.14159265358979323846);
	return static_cast<float>(yaw < 0.0 ? yaw + 360.0 : yaw);
}
}

runtime::ViewAngles BotCommandRuntime::stepIdleLook(std::size_t index, const edict_t &entity, float desiredYaw,
	float desiredPitch, compat::AimSpringState &spring)
{
	const auto &angles = entity.v.v_angle;
	const float now = services_->service_globals()->time;
	const auto &weapon = services_->observation_managedBotCurrentWeapons(index);
	const auto &handle = services_->service_managedBotHandles()[index];
	const bool sniper = weapon.available && weapon.active && weapon.weaponId < 32U &&
		weapon.actorGeneration == handle.actor.actorGeneration &&
		weapon.mapGeneration == services_->service_lifecycle().mapGeneration() &&
		weapon.roundGeneration == services_->service_lifecycle().roundGeneration() &&
		(compat::kSniperRifleInventoryMask & (1U << weapon.weaponId)) != 0U;
	const float drift = services_->service_compatibilitySurface().configuration().mode == compat::RuntimeMode::Compatibility &&
		sniper && entity.v.fov > 0.0f && entity.v.fov < 90.0f ? 0.5f : 2.0f;
	desiredYaw += drift * ActionAdapter::csBotCosine(33.0f * now);
	services_->behavior_recordLookGoal(index, desiredYaw);
	const auto look = compat::trackAim(angles.y, angles.x, desiredYaw,
		desiredPitch + drift * ActionAdapter::csBotCosine(13.0f * now - 90.0f),
		runtime::BotTimingScheduler::kCommandInterval, 1.0f,
		&spring, false);
	actors_[index].lastLookSpring = spring;
	return {look.pitch, look.yaw, 0.0f};
}

void BotCommandRuntime::applyCombatUpkeep(std::size_t index, const CombatUpkeepUpdate &update)
{
	if (index >= actors_.size() || !actors_[index].managedBotCommandTemplateValid)
		return;
	auto &command = actors_[index].managedBotCommandTemplates;
	if (update.hasAim)
	{
		actors_[index].resetNavigationLook();
		actors_[index].useEntityLookActive = false;
		actors_[index].resumedDefaultLook = false;
		if (!update.movementRelativeToAim && !actors_[index].nativeLadderControl && !actors_[index].blindCommand)
		{
			const auto movement = ActionAdapter::reprojectMovement(
				command.movement.forward, command.movement.side, command.viewAngles.yaw, update.angles.yaw);
			command.movement.forward = movement.forward;
			command.movement.side = movement.side;
			command.movement.buttons = static_cast<std::uint16_t>(
				command.movement.buttons & ~(IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT));
			command.movement.buttons = static_cast<std::uint16_t>(
				command.movement.buttons | ActionAdapter::movementButtons(movement.forward, movement.side));
		}
		command.viewAngles = update.angles;
		command.synchronizeCombatView = true;
	}
	if (update.setPrimary)
	{
		if (!update.hasAim)
			command.synchronizeCombatView = false;
		command.movement.buttons = static_cast<std::uint16_t>(command.movement.buttons & ~IN_ATTACK);
		if (update.primary)
			command.movement.buttons = static_cast<std::uint16_t>(command.movement.buttons | IN_ATTACK);
	}
}

using namespace runtime_detail;

void BotCommandRuntime::onTouch(edict_t *entity, edict_t *other)
{
	if (!services_->runtimeIsActive() || services_->service_globals() == nullptr ||
		!std::isfinite(services_->service_globals()->time) ||
		services_->service_compatibilitySurface().configuration().mode != compat::RuntimeMode::Compatibility)
		return;
	const auto observe = [&](edict_t *self, edict_t *contact)
	{
		if (self == nullptr || contact == nullptr || self == contact || self->free || contact->free ||
			(self->v.flags & FL_CLIENT) == 0 || (contact->v.flags & FL_CLIENT) == 0 || self->v.deadflag != DEAD_NO ||
			contact->v.deadflag != DEAD_NO || self->v.health <= 0.0f || contact->v.health <= 0.0f)
			return;
		const auto actor = services_->observation_publicPlayerIdentity(self),
				   touched = services_->observation_publicPlayerIdentity(contact);
		if (actor.slot == 0U || touched.slot == 0U)
			return;
		const auto index = static_cast<std::size_t>(actor.slot - 1U);
		if (!services_->service_managedBotSlots()[index] ||
			services_->service_managedBotHandles()[index].entity != self ||
			!services_->service_lifecycle().isCurrent(services_->service_lifecycle().tokenForSlot(actor.slot)))
			return;
		const world::FrameIdentity frame = {services_->service_lifecycle().mapGeneration(),
			services_->service_lifecycle().roundGeneration(), services_->service_adapterFrameCount()};
		runtime::PlayerAvoidanceTouch touch = {actor, frame, touched, services_->publicPlayerPriority(self),
			services_->publicPlayerPriority(contact), services_->service_globals()->time,
			services_->behavior_managedBotStateMachines(index).state() == compat::CompatibilityStateId::DefuseBomb};
		runtime::ActorId previous{};
		if (actors_[index].playerAvoidance.target(actor, frame, services_->service_globals()->time, &previous) &&
			services_->service_engineFunctions() != nullptr &&
			services_->service_engineFunctions()->pfnPEntityOfEntIndex != nullptr)
		{
			const auto *incumbent =
				services_->service_engineFunctions()->pfnPEntityOfEntIndex(static_cast<int>(previous.slot));
			const auto current = services_->observation_publicPlayerIdentity(incumbent);
			if (current.slot == previous.slot && current.actorGeneration == previous.actorGeneration)
			{
				touch.currentTargetPriority = services_->publicPlayerPriority(incumbent);
				touch.currentTargetPriorityKnown = true;
			}
			else
				actors_[index].playerAvoidance.reset();
		}
		actors_[index].playerAvoidance.touch(touch);
	};
	observe(entity, other);
	observe(other, entity);
}

void BotCommandRuntime::applyPlayerAvoidance(std::size_t index, runtime::BotCommand *command, float desiredSpeed)
{
	if (command == nullptr || index >= services_->service_managedBotHandles().size() ||
		services_->service_globals() == nullptr ||
		services_->service_compatibilitySurface().configuration().mode != compat::RuntimeMode::Compatibility ||
		services_->service_engineFunctions() == nullptr ||
		services_->service_engineFunctions()->pfnPEntityOfEntIndex == nullptr)
		return;
	const auto &handle = services_->service_managedBotHandles()[index];
	runtime::ActorId target{};
	const world::FrameIdentity frame = {services_->service_lifecycle().mapGeneration(),
		services_->service_lifecycle().roundGeneration(), services_->service_adapterFrameCount()};
	if (!actors_[index].playerAvoidance.target(handle.actor, frame, services_->service_globals()->time, &target))
		return;
	const auto *other = services_->service_engineFunctions()->pfnPEntityOfEntIndex(static_cast<int>(target.slot));
	const auto identity = services_->observation_publicPlayerIdentity(other);
	const auto *self = handle.entity;
	if (identity.slot != target.slot || identity.actorGeneration != target.actorGeneration || other == nullptr ||
		other->v.deadflag != DEAD_NO || other->v.health <= 0.0f || self == nullptr || self->v.deadflag != DEAD_NO ||
		self->v.health <= 0.0f)
	{
		actors_[index].playerAvoidance.reset();
		return;
	}
	if ((self->v.flags & FL_ONGROUND) == 0 || self->v.movetype == MOVETYPE_FLY ||
		services_->service_engineFunctions()->pfnTraceLine == nullptr)
		return;
	const float yaw = command->viewAngles.yaw * ASTRABOT_BOT_COMMAND_RUNTIME_DEGREES_TO_RADIANS;
	const float dx = other->v.origin.x - self->v.origin.x, dy = other->v.origin.y - self->v.origin.y;
	const float length = std::hypot(dx, dy);
	if (!std::isfinite(length) || length <= 0.0f || !std::isfinite(yaw))
		return;
	const auto move =
		runtime::playerAvoidanceMove(command->viewAngles.yaw, dx, dy, command->movement.forward, command->movement.side,
			desiredSpeed);
	if (!move.valid)
		return;
	const float worldX = move.forward * std::cos(yaw) + move.side * std::sin(yaw);
	const float worldY = move.forward * std::sin(yaw) - move.side * std::cos(yaw);
	const float speed = std::hypot(worldX, worldY);
	const float feetZ = self->v.origin.z + self->v.mins.z;
	constexpr float kAvoidanceGroundProbeDistance = 30.0f;
	constexpr float kAvoidanceGroundProbeHeight = 36.0f;
	const float start[3] = {self->v.origin.x + kAvoidanceGroundProbeDistance * worldX / speed,
		self->v.origin.y + kAvoidanceGroundProbeDistance * worldY / speed, feetZ + kAvoidanceGroundProbeHeight};
	const float end[3] = {start[0], start[1], feetZ - nav::LocomotionConfig::kMaximumStepHeight};
	TraceResult ground{};
	constexpr int kGroundTraceIgnoringActors = 1;
	services_->service_engineFunctions()->pfnTraceLine(start, end, kGroundTraceIgnoringActors, handle.entity, &ground);
	const bool safe = !ground.fStartSolid && !ground.fAllSolid && std::isfinite(ground.flFraction) &&
		ground.flFraction < 1.0f && std::isfinite(ground.vecEndPos[2]) &&
		std::fabs(ground.vecEndPos[2] - feetZ) <= nav::LocomotionConfig::kMaximumStepHeight;
	if (safe)
	{
		command->movement.forward = move.forward;
		command->movement.side = move.side;
		command->movement.buttons = static_cast<std::uint16_t>(
			(command->movement.buttons & ~(IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT)) |
			ActionAdapter::movementButtons(move.forward, move.side));
	}
	if (services_->service_runtimeProfiler().enabled() && (services_->service_adapterFrameCount() % 16U) == 0U &&
		gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr &&
		services_->service_pluginId() != nullptr)
		profiledLogConsole(services_->service_runtimeProfiler(), services_->service_pluginId(),
			"profile playerAvoidance bot_id=%u target=%u frame=%u applied=%d "
			"side=%.1f ground_safe=%d forward=%.1f",
			handle.actor.slot, target.slot, services_->service_adapterFrameCount(), safe ? 1 : 0, move.side,
			safe ? 1 : 0, move.forward);
}

void BotCommandRuntime::observeReloadInput(const FakeClientHandle &handle, const runtime::BotCommand &command,
	const runtime::CommandReceipt &receipt)
{
	if (receipt.result != runtime::DispatchResult::Dispatched || (command.movement.buttons & IN_RELOAD) == 0U ||
		services_->service_globals() == nullptr || !services_->service_lifecycle().isCurrent(command.lifecycle))
		return;
	services_->observation_observeReloadDispatched({handle.actor.slot, handle.actor.actorGeneration},
		{command.lifecycle.mapGeneration, command.lifecycle.roundGeneration, services_->service_adapterFrameCount()},
		services_->service_globals()->time);
}

bool BotCommandRuntime::dispatchManagedActionCommand(
	std::size_t index, FakeClientHandle &handle, const ActionProposal &proposal, const ActionDispatch &dispatch)
{
	if (dispatch.clientCommand == nullptr)
	{
		return true;
	}
	const char *clientCommand = dispatch.clientCommand;
	constexpr auto kKnifeWeaponId = 29U;
	std::uint16_t selectedEquipment = proposal.kind == ActionKind::SelectKnife ? kKnifeWeaponId : 0U;
	if (proposal.kind == ActionKind::SelectPrimary)
	{
		const unsigned ids[] = {3, 5, 7, 8, 12, 13, 14, 15, 18, 19, 20, 21, 22, 23, 24, 27, 28, 30};
		const char *names[] = {"weapon_scout", "weapon_xm1014", "weapon_mac10", "weapon_aug", "weapon_ump45",
			"weapon_sg550", "weapon_galil", "weapon_famas", "weapon_awp", "weapon_mp5navy", "weapon_m249", "weapon_m3",
			"weapon_m4a1", "weapon_tmp", "weapon_g3sg1", "weapon_sg552", "weapon_ak47", "weapon_p90"};
		clientCommand = nullptr;
		const auto mask = static_cast<std::uint32_t>(handle.entity->v.weapons);
		for (std::size_t i = 0; i < sizeof(ids) / sizeof(ids[0]); ++i)
			if ((mask & (1U << ids[i])) != 0U)
			{
				clientCommand = names[i];
				selectedEquipment = static_cast<std::uint16_t>(ids[i]);
				break;
			}
		if (!clientCommand)
			return false;
	}
	if (proposal.kind == ActionKind::SelectPistol)
	{
		const unsigned ids[] = {1, 10, 11, 16, 17, 26};
		const char *names[] = {
			"weapon_p228", "weapon_elite", "weapon_fiveseven", "weapon_usp", "weapon_glock18", "weapon_deagle"};
		clientCommand = nullptr;
		const auto mask = static_cast<std::uint32_t>(handle.entity->v.weapons);
		constexpr int kPistolWeaponCount = 6;
		for (std::size_t i = 0; i < kPistolWeaponCount; ++i)
			if ((mask & (1U << ids[i])) != 0U &&
				(proposal.equipmentWeaponId == 0 || proposal.equipmentWeaponId == ids[i]))
			{
				clientCommand = names[i];
				selectedEquipment = static_cast<std::uint16_t>(ids[i]);
				break;
			}
		if (!clientCommand)
			return false;
	}
	const bool commandDispatched = services_->dispatchClientCommand(handle.entity, clientCommand, "");
	if (selectedEquipment != 0)
		services_->behavior_observeEquipmentDispatched(index, selectedEquipment, commandDispatched);
	const bool c4SelectionRequest = proposal.kind == ActionKind::SelectC4 || proposal.kind == ActionKind::Plant ||
		proposal.kind == ActionKind::PlantContinue;
	if (commandDispatched && c4SelectionRequest && index < NativeBotObservation::kClientSlotCount)
	{
		services_->behavior_observeC4SelectionDispatched(index, handle);
	}
	if (c4SelectionRequest && services_->service_runtimeProfiler().enabled() &&
		(services_->service_adapterFrameCount() % 16U) == 0U && gpMetaUtilFuncs != nullptr &&
		gpMetaUtilFuncs->pfnLogConsole != nullptr && services_->service_pluginId() != nullptr)
	{
		const ManagedCurrentWeaponObservation &weapon = services_->observation_managedBotCurrentWeapons(index);
		profiledLogConsole(services_->service_runtimeProfiler(), services_->service_pluginId(),
			"profile plantCommand actor=%u actor_generation=%u map=%u round=%u "
			"frame=%u action=%s command=%s dispatched=%d current_weapon_known=%d "
			"current_weapon_active=%d current_weapon_id=%u active_c4=%d",
			static_cast<unsigned int>(handle.actor.slot), static_cast<unsigned int>(handle.actor.actorGeneration),
			static_cast<unsigned int>(services_->service_lifecycle().mapGeneration()),
			static_cast<unsigned int>(services_->service_lifecycle().roundGeneration()),
			static_cast<unsigned int>(services_->service_adapterFrameCount()), actionKindName(proposal.kind),
			dispatch.clientCommand, commandDispatched ? 1 : 0, weapon.available ? 1 : 0, weapon.active ? 1 : 0,
			static_cast<unsigned int>(weapon.weaponId),
			weapon.available && weapon.active && weapon.weaponId == ActionAdapter::kC4WeaponId ? 1 : 0);
	}
	return commandDispatched;
}

void BotCommandRuntime::resetManagedBotCommandTemplate(std::size_t index)
{
	resetManagedBotCommandInputs(index);
	if (index < NativeBotObservation::kClientSlotCount)
	{
		actors_[index].resetNavigationLook();
		actors_[index].useEntityLookActive = false;
		actors_[index].lastLookSpring = {};
		actors_[index].resumedDefaultLook = false;
		actors_[index].resumedLookAt = std::numeric_limits<float>::quiet_NaN();
	}
}

void BotCommandRuntime::advanceLadderDescentLook(std::size_t index, FakeClientHandle &handle)
{
	auto &actor = actors_[index];
	const auto &machine = services_->behavior_managedBotStateMachines(index);
	const bool ladderLookPending = actor.nativeLadderControl || actor.descentLookPhase != nav::NativeLadderPhase::None;
	if ((!ladderLookPending && !actor.descentFloorLook) ||
		machine.state() != compat::CompatibilityStateId::Hunt || machine.attackOverlayActive())
		return;
	const auto config = services_->service_compatibilitySurface().configuration();
	auto *entity = handle.entity;
	const float now = services_->service_globals()->time;
	if (config.mode != compat::RuntimeMode::Compatibility || config.botEnable <= 0.0f || config.botStop > 0.0f ||
		!entity || entity->free || entity->v.health <= 0 || entity->v.deadflag != DEAD_NO ||
		(entity->v.flags & (FL_FROZEN | FL_SPECTATOR)) || services_->roundFreezeActive() ||
		!std::isfinite(now) || actor.descentLookAt == now)
		return;
	float yaw = actor.navigationLookYaw;
	float pitch = 0.0f;
	nav::NativeLadderLook ladder{};
	if (ladderLookPending && !actor.descentFloorLook)
	{
		const int serial = entity->serialnumber;
		const auto token = services_->service_lifecycle().tokenForSlot(handle.actor.slot);
		const auto epoch = machine.normalStateEpoch();
		if (!services_->navigation_nativeLadderLook(index, &ladder) ||
			handle.entity != entity || entity->free || entity->serialnumber != serial ||
			machine.normalStateEpoch() != epoch || machine.attackOverlayActive() ||
			!services_->service_lifecycle().isCurrent(token))
			return;
		if (ladder.phase < nav::NativeLadderPhase::ApproachDown)
			return;
		yaw = ladder.yaw;
		pitch = ladder.pitchOverride ? ladder.pitch : 0.0f;
	}
	if (!std::isfinite(yaw))
		return;
	const float drift = entity->v.fov > 0.0f && entity->v.fov < 90.0f ? 0.5f : 2.0f;
	if (!ladder.active)
	{
		yaw += drift * ActionAdapter::csBotCosine(33.0f * now);
		services_->behavior_recordLookGoal(index, yaw);
	}
	if (!ladder.pitchOverride)
		pitch += drift * ActionAdapter::csBotCosine(13.0f * now - 90.0f);
	const auto before = entity->v.v_angle;
	const auto look = compat::trackAim(before.y, before.x, yaw, pitch, runtime::BotTimingScheduler::kCommandInterval,
		1.0f, &actor.navigationLookSpring, false);
	actor.lastLookSpring = actor.navigationLookSpring;
	actor.descentLookAt = now;
	actor.descentLookPhase = ladder.phase;
	entity->v.v_angle = Vector(look.pitch, look.yaw, before.z);
	if (services_->service_runtimeProfiler().enabled())
		profiledLogConsole(services_->service_runtimeProfiler(), services_->service_pluginId(),
			"profile ladderUpkeep slot=%u time=%.6f phase=%u move=%d flags=%d target=(%.3f %.3f) "
			"view_before=(%.3f %.3f) view_after=(%.3f %.3f) spring=(%.3f %.3f)",
			handle.actor.slot, now, static_cast<unsigned>(ladder.phase), entity->v.movetype, entity->v.flags, pitch, yaw,
			before.x, before.y, look.pitch, look.yaw,
			actor.navigationLookSpring.pitchVelocity, actor.navigationLookSpring.yawVelocity);
}

void BotCommandRuntime::advanceHuntEncounterLook(std::size_t index, edict_t &entity)
{
	auto &actor = actors_[index];
	const auto config = services_->service_compatibilitySurface().configuration();
	const float now = services_->service_globals()->time;
	if (!actor.huntApproachLookActive || !actor.huntEncounterLook || actor.nativeLadderControl ||
		config.mode != compat::RuntimeMode::Compatibility || config.botEnable <= 0.0f ||
		actor.descentLookAt == now || actor.huntEncounterUpkeepAt == now ||
		services_->behavior_managedBotStateMachines(index).attackOverlayActive() ||
		entity.free || entity.v.deadflag != DEAD_NO || entity.v.health <= 0.0f ||
		(entity.v.flags & (FL_FROZEN | FL_SPECTATOR)) || services_->roundFreezeActive())
		return;

	const auto &spot = actor.huntApproachLookSpot;
	const nav::NavVector origin{entity.v.origin.x, entity.v.origin.y, entity.v.origin.z};
	const bool close = nav::NavPathFollower::nearHuntCorner(origin, spot);
	if (close)
		actor.huntApproachLookActive = false;
	else if (actor.huntEncounterAlignedAt < 0.0f)
	{
		const auto eye = entity.v.origin + entity.v.view_ofs;
		const auto goal = ActionAdapter::useEntityView(spot.x - eye.x, spot.y - eye.y, spot.z - eye.z);
		if (std::abs(compat::normalizedAimAngle(goal.yaw - entity.v.v_angle.y)) < 10.0f &&
			std::abs(compat::normalizedAimAngle(goal.pitch - entity.v.v_angle.x)) < 10.0f)
			actor.huntEncounterAlignedAt = now;
	}

	if (!close || std::isfinite(actor.navigationLookYaw))
	{
		const auto view = stepNavigationLook(index, entity, true);
		entity.v.v_angle = Vector(view.pitch, view.yaw, view.roll);
	}
	actor.huntEncounterUpkeepAt = now;
	if (actor.huntEncounterAlignedAt >= 0.0f && now > actor.huntEncounterAlignedAt)
		actor.huntApproachLookActive = false;
}

void BotCommandRuntime::advanceUpkeepLook(std::size_t index, FakeClientHandle &handle)
{
	auto &actor = actors_[index];
	const auto &machine = services_->behavior_managedBotStateMachines(index);
	if (actor.resumedDefaultLook)
	{
		const auto config = services_->service_compatibilitySurface().configuration();
		auto *entity = handle.entity;
		if (!entity || entity->free || entity->serialnumber != actor.resumedLookSerial ||
			entity->v.deadflag != DEAD_NO || entity->v.health <= 0.0f || machine.attackOverlayActive() ||
			actor.resumedLookToken.slot != handle.actor.slot ||
			!services_->service_lifecycle().isCurrent(actor.resumedLookToken) ||
			config.mode != compat::RuntimeMode::Compatibility)
			actor.resumedDefaultLook = false;
		else
		{
			const float now = services_->service_globals()->time;
			if (config.botEnable > 0.0f && config.botStop == 0.0f &&
				!(entity->v.flags & (FL_FROZEN | FL_SPECTATOR)) && !services_->roundFreezeActive() &&
				actor.resumedLookAt != now)
			{
				const auto look = stepIdleLook(index, *entity, actor.defaultLookAheadYaw, 0.0f, actor.lastLookSpring);
				entity->v.v_angle = Vector(look.pitch, look.yaw, look.roll);
				actor.resumedLookAt = now;
			}
			return;
		}
	}
	if (usesApproachLook(machine.state()) && !actor.useEntityLookActive && !actor.defusePathLook)
	{
		auto *entity = handle.entity;
		const auto serial = entity ? entity->serialnumber : -1;
		const auto token = services_->service_lifecycle().tokenForSlot(handle.actor.slot);
		if (machine.state() == compat::CompatibilityStateId::Hunt)
			advanceLadderDescentLook(index, handle);
		if (entity && handle.entity == entity && entity->serialnumber == serial &&
			services_->service_lifecycle().isCurrent(token) && usesApproachLook(machine.state()))
			advanceHuntEncounterLook(index, *entity);
		return;
	}
	if (!actor.useEntityLookActive)
	{
		if (actor.defusePathLook && !actor.nativeLadderControl && !machine.attackOverlayActive() &&
			(isDefuseApproach(machine) || machine.state() == compat::CompatibilityStateId::DefuseBomb) &&
			handle.entity->v.deadflag == DEAD_NO && handle.entity->v.health > 0.0f &&
			!(handle.entity->v.flags & FL_FROZEN) && !services_->roundFreezeActive())
		{
			const auto look = stepIdleLook(index, *handle.entity, actor.navigationLookYaw, 0.0f,
				actor.navigationLookSpring);
			handle.entity->v.v_angle = Vector(look.pitch, look.yaw, look.roll);
		}
		return;
	}
	const bool bombInteraction = machine.state() == compat::CompatibilityStateId::DefuseBomb ||
		machine.state() == compat::CompatibilityStateId::PlantBomb;
	if ((machine.state() != compat::CompatibilityStateId::UseEntity && !bombInteraction) || machine.attackOverlayActive() ||
		handle.entity->v.deadflag != DEAD_NO || handle.entity->v.health <= 0.0f)
	{
		actor.useEntityLookActive = false;
		return;
	}
	if ((handle.entity->v.flags & FL_FROZEN) != 0 || services_->roundFreezeActive())
		return;
	auto *entity = handle.entity;
	const auto serial = entity->serialnumber;
	const auto epoch = machine.normalStateEpoch();
	const auto token = services_->service_lifecycle().tokenForSlot(handle.actor.slot);
	world::WorldPosition enemyPoint{};
	const bool aimingAtEnemy = bombInteraction &&
		services_->behavior_bombObjectiveEnemyAim(index, actor.useEntityLookSpring, &enemyPoint);
	if (handle.entity != entity || entity->serialnumber != serial || !services_->service_lifecycle().isCurrent(token) ||
		machine.normalStateEpoch() != epoch || machine.attackOverlayActive())
		return;
	const auto eye = entity->v.origin + entity->v.view_ofs;
	if (aimingAtEnemy)
	{
		const auto &origin = entity->v.origin;
		const auto goal = ActionAdapter::useEntityView(
			enemyPoint.x - origin.x, enemyPoint.y - origin.y, enemyPoint.z - origin.z);
		services_->behavior_recordLookGoal(index, goal.yaw);
		const auto look = compat::trackAim(entity->v.v_angle.y, entity->v.v_angle.x, goal.yaw, goal.pitch,
			runtime::BotTimingScheduler::kCommandInterval, 1.0f, &actor.useEntityLookSpring, false);
		actor.lastLookSpring = actor.useEntityLookSpring;
		entity->v.v_angle = Vector(look.pitch, look.yaw, 0.0f);
		return;
	}
	const auto &point = actor.useEntityLookTarget;
	const auto desired = ActionAdapter::useEntityView(point.x - eye.x, point.y - eye.y, point.z - eye.z);
	const auto look = stepIdleLook(index, *handle.entity, desired.yaw, desired.pitch, actor.useEntityLookSpring);
	handle.entity->v.v_angle = Vector(look.pitch, look.yaw, look.roll);
}

void BotCommandRuntime::applyUseEntityLook(std::size_t index, const edict_t &entity, const ActionProposal &action,
	runtime::BotCommand &command)
{
	auto &actor = actors_[index];
	if (action.hasCombatAim || action.hasGuardLook ||
		services_->behavior_managedBotStateMachines(index).attackOverlayActive())
		actor.resumedDefaultLook = false;
	if (action.hasUseEntityLook)
	{
		if (!actor.useEntityLookActive)
			actor.useEntityLookSpring =
				services_->service_compatibilitySurface().configuration().mode == compat::RuntimeMode::Compatibility
				? actor.lastLookSpring : actor.defusePathLook ? actor.navigationLookSpring : compat::AimSpringState{};
		actor.resumedDefaultLook = false;
		actor.resetNavigationLook();
		actor.useEntityLookTarget = action.useEntityLookTarget;
		command.viewAngles = {entity.v.v_angle.x, entity.v.v_angle.y, entity.v.v_angle.z};
		command.synchronizeCombatView = true;
	}
	else if (actor.useEntityLookActive && !action.hasCombatAim && !action.hasGuardLook &&
		!services_->behavior_managedBotStateMachines(index).attackOverlayActive() &&
		services_->service_compatibilitySurface().configuration().mode == compat::RuntimeMode::Compatibility)
	{
		// The terminal full command already consumed Upkeep. Resume on the next command tick.
		actor.lastLookSpring = actor.useEntityLookSpring;
		actor.resumedDefaultLook = true;
		actor.resumedLookSerial = entity.serialnumber;
		actor.resumedLookToken = services_->service_lifecycle().tokenForSlot(command.actor.slot);
		actor.resumedLookAt = services_->service_globals()->time;
		actor.resetNavigationLook();
	}
	actor.useEntityLookActive = action.hasUseEntityLook;
	actor.hostageUseTarget = action.hostageUseTarget;
}

void BotCommandRuntime::observeHostageUse(std::size_t index, const runtime::BotCommand &command,
	const runtime::CommandReceipt &receipt, world::EntityKey target, const Vector &origin)
{
	if (!target.isValid() || receipt.result != runtime::DispatchResult::Dispatched ||
		receipt.sequence != command.sequence || (command.movement.buttons & IN_USE) == 0 ||
		!services_->service_lifecycle().isCurrent(command.lifecycle) || !(actors_[index].hostageUseTarget == target))
		return;
	actors_[index].hostageUseTarget = {};
	services_->behavior_observeHostageUseDispatched(index, target, {origin.x, origin.y, origin.z});
}

void BotCommandRuntime::resetManagedBotCommandInputs(std::size_t index)
{
	if (index >= NativeBotObservation::kClientSlotCount)
	{
		return;
	}
	actors_[index].managedBotCommandTemplates = {};
	actors_[index].managedBotCommandTemplateValid = false;
	actors_[index].nativeLadderControl = false;
	actors_[index].nativeLadderJump = false;
	actors_[index].managedBotActionStopsMovement = false;
	actors_[index].blindCommand = false;
	actors_[index].hostageUseTarget = {};
}

void BotCommandRuntime::prepareBlindCommand(std::size_t index, FakeClientHandle &handle,
	const runtime::MovementPhysicsState &before, BlindCommand response, bool crouching)
{
	auto draft = beginNeutralCommand(index, handle, before);
	if (!draft.ready)
		return;
	auto &actor = actors_[index];
	auto &movement = draft.command.movement;
	crouching = crouching || response.move == BlindMove::Crouch;
	if (crouching)
		movement.buttons |= IN_DUCK;
	if (response.fire)
		movement.buttons |= IN_ATTACK;

	const bool walking = !crouching && !services_->behavior_forcedRunning(index) &&
		(actor.navigationWalking || services_->behavior_noiseInvestigationWalking(index) ||
			services_->behavior_followWalking(index));
	const float maxSpeed = handle.entity->v.maxspeed;
	const float speed = (std::isfinite(maxSpeed) && maxSpeed > 0.0f) ? maxSpeed * (walking ? 0.4f : 1.0f) : 0.0f;
	switch (response.move)
	{
	case BlindMove::Right: movement.side = speed; movement.buttons |= IN_MOVERIGHT; break;
	case BlindMove::Backward: movement.forward = -speed; movement.buttons |= IN_BACK; break;
	case BlindMove::Left: movement.side = -speed; movement.buttons |= IN_MOVELEFT; break;
	default: break;
	}
	actor.managedBotJumpCrouch.reset();
	actor.managedBotCommandTemplates = draft.command;
	actor.managedBotCommandTemplateValid = true;
	actor.blindCommand = true;
}

MovementCommandDraft BotCommandRuntime::beginNeutralCommand(
	std::size_t index, FakeClientHandle &handle, const runtime::MovementPhysicsState &)
{
	MovementCommandDraft draft{};
	if (index >= NativeBotObservation::kClientSlotCount || handle.entity == nullptr)
	{
		return draft;
	}
	const runtime::LifecycleToken token = services_->service_lifecycle().tokenForSlot(handle.actor.slot);
	if (!services_->service_lifecycle().isCurrent(token))
	{
		resetManagedBotCommandTemplate(index);
		return draft;
	}
	runtime::BotCommand &command = draft.command;
	command.actor = handle.actor;
	command.lifecycle = token;
	command.viewAngles = {handle.entity->v.v_angle[0], handle.entity->v.v_angle[1], handle.entity->v.v_angle[2]};
	command.movement = {0.0f, 0.0f, 0.0f, 0U, 0U, 1U};
	draft.ready = true;
	return draft;
}

void BotCommandRuntime::finishNeutralCommand(std::size_t index, FakeClientHandle &handle,
	const runtime::MovementPhysicsState &before, MovementCommandDraft draft, const ActionProposal &requestedAction)
{
	runtime::BotCommand command = draft.command;
	auto &actor = actors_[index];
	actor.nativeLadderControl = false;
	actor.nativeLadderJump = false;
	const auto &machine = services_->behavior_managedBotStateMachines(index);
	const bool defuseView = actor.defusePathLook && machine.state() == compat::CompatibilityStateId::DefuseBomb;
	// A throttled Hunt replan can temporarily stop movement while the ladder is retained.
	const bool descentView = machine.state() == compat::CompatibilityStateId::Hunt && before.onLadder &&
		actor.descentLookPhase != nav::NativeLadderPhase::None && actor.descentLookAt == services_->service_globals()->time;
	const bool priorityLook = machine.attackOverlayActive() || requestedAction.hasCombatAim || requestedAction.hasGuardLook;
	if (before.dead || priorityLook || clearsHuntApproachLook(machine.state()))
		actor.huntApproachLookActive = false;
	if ((!defuseView && !descentView) || priorityLook)
		actor.resetPathLook();
	const ActionProposal actionProposal = ActionAdapter::forLiveDispatch(
		requestedAction, services_->observation_hasObservedActiveWeapon(index), command.viewAngles);
	const ActionDispatch actionDispatch = ActionAdapter::translate(actionProposal);
	command.viewAngles = actionDispatch.viewAngles;
	command.synchronizeCombatView = actionProposal.hasCombatAim || actionProposal.hasGuardLook || actor.defusePathLook ||
		actionProposal.preserveUpkeepView;
	applyUseEntityLook(index, *handle.entity, actionProposal, command);
	command.movement.buttons = actionDispatch.buttons;
	ActionAdapter::applyCombatMovement(command, actionProposal, handle.entity->v.maxspeed);
	if (actionDispatch.clientCommand != nullptr)
	{
		(void)dispatchManagedActionCommand(index, handle, actionProposal, actionDispatch);
	}
	actors_[index].managedBotCommandTemplates = command;
	actors_[index].managedBotCommandTemplateValid = true;
	actors_[index].managedBotActionStopsMovement = actionDispatch.stopMovement;
}

runtime::ViewAngles BotCommandRuntime::stepNavigationLook(
	std::size_t index, const edict_t &entity, bool useHuntApproachLook)
{
	auto &actor = actors_[index];
	float yaw = actor.navigationLookYaw, pitch = 0.0f;
	if (useHuntApproachLook && actor.huntApproachLookActive)
	{
		const auto eye = entity.v.origin + entity.v.view_ofs;
		const auto &spot = actor.huntApproachLookSpot;
		const auto angles = ActionAdapter::useEntityView(spot.x - eye.x, spot.y - eye.y, spot.z - eye.z);
		yaw = angles.yaw;
		pitch = angles.pitch;
	}
	return stepIdleLook(index, entity, yaw, pitch, actor.navigationLookSpring);
}

runtime::CommandReceipt BotCommandRuntime::executeManagedBotCommand(
	std::size_t index, FakeClientHandle &handle, bool navigationLookAdvanced)
{
	RuntimeProfilerScope profilerScope(services_->service_runtimeProfiler(), RuntimeProfilerStage::MovementDispatch);
	runtime::CommandReceipt receipt = {
		handle.actor, 0U, services_->service_adapterFrameCount(), runtime::DispatchResult::NoCommand};
	if (index >= NativeBotObservation::kClientSlotCount || handle.entity == nullptr ||
		actors_[index].managedBotCommandSequences == (std::numeric_limits<std::uint32_t>::max)())
	{
		return receipt;
	}
	if (!actors_[index].managedBotCommandTemplateValid)
	{
		const runtime::LifecycleToken neutralToken = services_->service_lifecycle().tokenForSlot(handle.actor.slot);
		if (!services_->service_lifecycle().isCurrent(neutralToken))
		{
			receipt.result = runtime::DispatchResult::StaleActor;
			return receipt;
		}
		runtime::BotCommand neutralCommand = {};
		neutralCommand.actor = handle.actor;
		neutralCommand.lifecycle = neutralToken;
		neutralCommand.viewAngles = {
			handle.entity->v.v_angle[0], handle.entity->v.v_angle[1], handle.entity->v.v_angle[2]};
		neutralCommand.movement = {0.0f, 0.0f, 0.0f, 0U, 0U, 1U};
		actors_[index].managedBotCommandTemplates = neutralCommand;
		actors_[index].managedBotCommandTemplateValid = true;
	}
	if (!actors_[index].managedBotCommandTemplateValid)
	{
		return receipt;
	}
	const runtime::LifecycleToken token = services_->service_lifecycle().tokenForSlot(handle.actor.slot);
	if (!services_->service_lifecycle().isCurrent(token))
	{
		receipt.result = runtime::DispatchResult::StaleActor;
		return receipt;
	}
	runtime::BotCommand command = actors_[index].managedBotCommandTemplates;
	command.sequence = ++actors_[index].managedBotCommandSequences;
	command.issueFrame = services_->service_adapterFrameCount();
	if (services_->service_globals() == nullptr)
	{
		receipt.result = runtime::DispatchResult::EngineUnavailable;
		return receipt;
	}
	command.movement.msec = services_->consumeCommandMsec(index, services_->service_globals()->time);
	auto &actor = actors_[index];
	if (actor.huntWaitNavigationLook || actor.huntApproachLookActive)
	{
		const auto config = services_->service_compatibilitySurface().configuration();
		if (config.mode != compat::RuntimeMode::Compatibility || config.botEnable <= 0.0f || config.botStop != 0.0f)
		{
			const auto &view = handle.entity->v.v_angle;
			command.viewAngles = {view.x, view.y, view.z};
			actor.managedBotCommandTemplates.viewAngles = command.viewAngles;
			actor.resetNavigationLook();
		}
	}
	const bool explicitFrozen = (handle.entity->v.flags & FL_FROZEN) != 0;
	const bool roundFreeze = services_->roundFreezeActive();
	const auto &machine = services_->behavior_managedBotStateMachines(index);
	const bool huntApproach = usesApproachLook(machine.state()) && actor.huntApproachLookActive &&
		!actor.nativeLadderControl;
	const bool descentLookAdvanced = actor.descentLookAt == services_->service_globals()->time &&
		machine.state() == compat::CompatibilityStateId::Hunt && !machine.attackOverlayActive();
	const bool encounterLookAdvanced = actor.huntEncounterUpkeepAt == services_->service_globals()->time &&
		usesApproachLook(machine.state()) && !machine.attackOverlayActive();
	const bool defaultLookAdvanced = actor.resumedDefaultLook &&
		actor.resumedLookAt == services_->service_globals()->time && !machine.attackOverlayActive();
	if (descentLookAdvanced || encounterLookAdvanced || defaultLookAdvanced)
	{
		const auto &view = handle.entity->v.v_angle;
		command.viewAngles = {view.x, view.y, view.z};
		command.synchronizeCombatView = true;
	}
	if (!navigationLookAdvanced && !descentLookAdvanced && !encounterLookAdvanced && !defaultLookAdvanced && !explicitFrozen && !roundFreeze &&
		(std::isfinite(actor.navigationLookYaw) || huntApproach) &&
		usesNavigationLook(machine.state()) && !machine.attackOverlayActive() &&
		(huntApproach || canAdvanceNavigationLook(machine.state(), (handle.entity->v.flags & FL_ONGROUND) != 0)))
	{
		// CSBot's upkeep turns the view while keeping the axes from the last full decision.
		command.viewAngles = stepNavigationLook(index, *handle.entity, usesApproachLook(machine.state()));
		command.synchronizeCombatView = true;
	}
	if (actor.useEntityLookActive || (actor.defusePathLook && !machine.attackOverlayActive() &&
		(isDefuseApproach(machine) || machine.state() == compat::CompatibilityStateId::DefuseBomb)))
		command.viewAngles = {handle.entity->v.v_angle.x, handle.entity->v.v_angle.y, handle.entity->v.v_angle.z};
	if (!actor.useEntityLookActive && !actor.nativeLadderControl && (!actor.blindCommand || !machine.attackOverlayActive()))
		services_->behavior_applyNoiseLookCommand(index, command, actor.blindCommand);
	if (!descentLookAdvanced)
		services_->behavior_applyNativeLadderLookCommand(index, command, actor.nativeLadderControl || actor.blindCommand);
	if (!actor.blindCommand && !actor.useEntityLookActive)
		services_->behavior_applyObstacleCommand(index, command);
	const float maxSpeed = handle.entity->v.maxspeed;
	const bool canWalk = !actor.blindCommand && !actors_[index].nativeLadderControl && !services_->behavior_forcedRunning(index) &&
		!(command.movement.buttons & IN_DUCK);
	const bool walking = canWalk && services_->behavior_noiseInvestigationWalking(index) &&
		(handle.entity->v.flags & FL_ONGROUND) && handle.entity->v.movetype != MOVETYPE_FLY;
	const bool followWalking = canWalk && services_->behavior_followWalking(index);
	const float movementSpeed = actors_[index].managedMoveSpeeds > 0.0f
		? actors_[index].managedMoveSpeeds : maxSpeed;
	constexpr float kWalkingSpeedFraction = .4f;
	const float walkingLimit = kWalkingSpeedFraction * maxSpeed;
	if (followWalking)
	{
		// Follow keeps the native per-axis input and can be forced to run between decisions.
		command.movement.forward *= kWalkingSpeedFraction;
		command.movement.side *= kWalkingSpeedFraction;
	}
	if (walking)
	{
		const float speed = std::hypot(command.movement.forward, command.movement.side);
		if (std::isfinite(speed) && std::isfinite(walkingLimit) && walkingLimit > 0 && speed > walkingLimit)
		{
			command.movement.forward *= walkingLimit / speed;
			command.movement.side *= walkingLimit / speed;
		}
	}
	if (!actor.blindCommand && !explicitFrozen && !roundFreeze && !actors_[index].managedBotActionStopsMovement &&
		!actors_[index].nativeLadderControl)
	{
		applyPlayerAvoidance(index, &command,
			walking || followWalking ? (std::min)(movementSpeed, walkingLimit) : movementSpeed);
	}
	if (walking)
	{
		const float speed = std::hypot(command.movement.forward, command.movement.side);
		const float limit = walkingLimit;
		if (std::isfinite(speed) && std::isfinite(limit) && limit > 0 && speed > limit)
		{
			command.movement.forward *= limit / speed;
			command.movement.side *= limit / speed;
		}
	}
	if (actors_[index].managedBotActionStopsMovement)
	{
		// Do not let a delayed jump-crouch sequence survive an objective hold.
		actors_[index].managedBotJumpCrouch.reset();
	}
	const std::uint16_t buttonsBeforeJumpCrouch = command.movement.buttons;
	const runtime::JumpCrouchObservation jumpCrouchObservation = {services_->service_globals()->time, true,
		(handle.entity->v.flags & FL_ONGROUND) != 0,
		handle.entity->v.deadflag != DEAD_NO || handle.entity->v.health <= 0.0f,
		handle.entity->v.movetype == MOVETYPE_FLY, explicitFrozen || roundFreeze,
		(command.movement.buttons & static_cast<std::uint16_t>(IN_JUMP)) != 0U};
	const auto jumpCrouchDecision = actor.blindCommand ? runtime::JumpCrouchCommandDecision{} :
		actors_[index].managedBotJumpCrouch.updateCommand(
			jumpCrouchObservation, command.movement.buttons, static_cast<std::uint16_t>(IN_DUCK));
	if (!actor.blindCommand)
		command.movement.buttons = jumpCrouchDecision.buttons;
	if (!jumpCrouchObservation.grounded && !actors_[index].nativeLadderJump)
	{
		command.movement.buttons =
			static_cast<std::uint16_t>(command.movement.buttons & ~static_cast<std::uint16_t>(IN_JUMP));
	}
	if (services_->service_runtimeProfiler().enabled() &&
		jumpCrouchDecision.sequencing.transition != runtime::JumpCrouchTransition::None && gpMetaUtilFuncs != nullptr &&
		gpMetaUtilFuncs->pfnLogConsole != nullptr && services_->service_pluginId() != nullptr)
	{
		profiledLogConsole(services_->service_runtimeProfiler(), services_->service_pluginId(),
			"profile jumpCrouch bot_id=%u command_sequence=%u transition=%s phase=%d "
			"elapsed=%.3f grounded=%d before_buttons=%u after_buttons=%u",
			static_cast<unsigned int>(handle.actor.slot), static_cast<unsigned int>(command.sequence),
			jumpCrouchTransitionName(jumpCrouchDecision.sequencing.transition),
			static_cast<int>(jumpCrouchDecision.sequencing.phase), jumpCrouchDecision.sequencing.elapsed,
			jumpCrouchObservation.grounded ? 1 : 0, static_cast<unsigned int>(buttonsBeforeJumpCrouch),
			static_cast<unsigned int>(command.movement.buttons));
	}
	MovementExecutionObservation gateObservation = {};
	gateObservation.explicitFrozen = explicitFrozen;
	gateObservation.roundFreezeActive = roundFreeze;
	gateObservation.maxSpeedAvailable = std::isfinite(maxSpeed) && maxSpeed > 0.0f;
	gateObservation.maxSpeed = maxSpeed;
	gateObservation.forward = command.movement.forward;
	gateObservation.side = command.movement.side;
	gateObservation.up = command.movement.up;
	gateObservation.buttons = command.movement.buttons;
	gateObservation.msec = command.movement.msec;
	gateObservation.freezetimeDuck = readOptionalCvarFloat(services_->service_engineFunctions(), "freezetime_duck");
	gateObservation.freezetimeJump = readOptionalCvarFloat(services_->service_engineFunctions(), "freezetime_jump");
	const MovementExecutionDecision gateDecision = MovementExecutionGate::evaluate(gateObservation);
	bool speedRestored = false;
	if (gateDecision.phase == MovementExecutionPhase::Live && !actors_[index].managedBotActionStopsMovement &&
		(!std::isfinite(maxSpeed) || maxSpeed <= kMinimumUsableMaxSpeed))
	{
		handle.entity->v.maxspeed = kDefaultManagedBotMaxSpeed;
		speedRestored = true;
		if (services_->service_engineFunctions() != nullptr &&
			services_->service_engineFunctions()->pfnSetClientMaxspeed != nullptr)
		{
			services_->service_engineFunctions()->pfnSetClientMaxspeed(handle.entity, kDefaultManagedBotMaxSpeed);
		}
	}
	if (gateDecision.phase == MovementExecutionPhase::ControlFrozen)
	{
		if (services_->service_engineFunctions() == nullptr ||
			services_->service_engineFunctions()->pfnRunPlayerMove == nullptr)
		{
			receipt.result = runtime::DispatchResult::EngineUnavailable;
			resetManagedBotCommandTemplate(index);
			return receipt;
		}
		const float viewAngles[3] = {command.viewAngles.pitch, command.viewAngles.yaw, command.viewAngles.roll};
		handle.entity->v.button = 0U;
		handle.entity->v.impulse = 0U;
		{
			RuntimeProfilerScope moveScope(services_->service_runtimeProfiler(), RuntimeProfilerStage::RunPlayerMove);
			services_->service_engineFunctions()->pfnRunPlayerMove(
				handle.entity, viewAngles, 0.0f, 0.0f, 0.0f, 0U, 0U, 0U);
		}
		receipt.sequence = command.sequence;
		receipt.result = runtime::DispatchResult::Dispatched;
		resetManagedBotCommandTemplate(index);
		return receipt;
	}
	command.movement.forward = gateDecision.forward;
	command.movement.side = gateDecision.side;
	command.movement.up = gateDecision.up;
	command.movement.buttons = gateDecision.buttons;
	command.movement.msec = gateDecision.msec;
	const runtime::QueueResult enqueueResult = services_->service_inputDispatcher().enqueue(command);
	if (enqueueResult != runtime::QueueResult::Accepted)
	{
		if (gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr &&
			services_->service_pluginId() != nullptr)
		{
			const runtime::LifecycleToken currentToken = services_->service_lifecycle().tokenForSlot(handle.actor.slot);
			importantLogConsole(services_->service_runtimeProfiler(), services_->service_pluginId(),
				"profile movementReject bot_id=%u actor_generation=%u frame=%u "
				"queue_result=%d command_valid=%d sequence=%u msec=%u "
				"command_lifecycle=(%u,%u,%u) current_lifecycle=(%u,%u,%u) "
				"view=(%.2f %.2f %.2f) move=(%.2f %.2f %.2f) buttons=%u",
				static_cast<unsigned int>(handle.actor.slot), static_cast<unsigned int>(handle.actor.actorGeneration),
				static_cast<unsigned int>(services_->service_adapterFrameCount()), static_cast<int>(enqueueResult),
				command.isValid() ? 1 : 0, static_cast<unsigned int>(command.sequence),
				static_cast<unsigned int>(command.movement.msec),
				static_cast<unsigned int>(command.lifecycle.mapGeneration),
				static_cast<unsigned int>(command.lifecycle.roundGeneration),
				static_cast<unsigned int>(command.lifecycle.slotGeneration),
				static_cast<unsigned int>(currentToken.mapGeneration),
				static_cast<unsigned int>(currentToken.roundGeneration),
				static_cast<unsigned int>(currentToken.slotGeneration), command.viewAngles.pitch,
				command.viewAngles.yaw, command.viewAngles.roll, command.movement.forward, command.movement.side,
				command.movement.up, static_cast<unsigned int>(command.movement.buttons));
		}
		receipt.sequence = command.sequence;
		receipt.result = enqueueResult == runtime::QueueResult::StaleActor ? runtime::DispatchResult::StaleActor
																		   : runtime::DispatchResult::InvalidCommand;
		// Never retry a rejected cached command unchanged; rebuild with the current lifecycle.
		resetManagedBotCommandTemplate(index);
		return receipt;
	}
	const Vector commandStart = handle.entity->v.origin;
	const auto hostageUseTarget = actor.hostageUseTarget;
	const runtime::CommandReceipt dispatchReceipt =
		services_->service_inputDispatcher().dispatchNext(handle.actor, services_->service_adapterFrameCount());
	if (services_->service_runtimeProfiler().enabled() &&
		actors_[index].managedCommandDiagnosticUpdates != services_->service_managedBotFullUpdateSequences()[index] &&
		handle.entity != nullptr && !handle.entity->free)
	{
		actors_[index].managedCommandDiagnosticUpdates = services_->service_managedBotFullUpdateSequences()[index];
		const auto &step = services_->navigation_managedNavigationSteps(index);
		profiledLogConsole(services_->service_runtimeProfiler(), services_->service_pluginId(),
			"profile motionCommand bot_id=%u actor_generation=%u map=%u round=%u frame=%u "
			"update=%u now=%.3f state=%s outcome=%u path=%u input=(%.1f %.1f %.1f) buttons=%u "
			"msec=%u dispatched=%d start=(%.1f %.1f %.1f) after=(%.1f %.1f %.1f)",
			handle.actor.slot, handle.actor.actorGeneration, services_->service_lifecycle().mapGeneration(),
			services_->service_lifecycle().roundGeneration(), services_->service_adapterFrameCount(),
			services_->service_managedBotFullUpdateSequences()[index], services_->service_globals()->time,
			compat::toString(services_->behavior_managedBotStateMachines(index).state()),
			static_cast<unsigned>(step.outcome), step.pathSequence, command.movement.forward, command.movement.side,
			command.movement.up, static_cast<unsigned>(command.movement.buttons),
			static_cast<unsigned>(command.movement.msec),
			dispatchReceipt.result == runtime::DispatchResult::Dispatched ? 1 : 0, commandStart.x, commandStart.y,
			commandStart.z, handle.entity->v.origin.x, handle.entity->v.origin.y, handle.entity->v.origin.z);
	}
	observeReloadInput(handle, command, dispatchReceipt);
	observeHostageUse(index, command, dispatchReceipt, hostageUseTarget, commandStart);
	if (!actor.blindCommand && dispatchReceipt.result == runtime::DispatchResult::Dispatched &&
		(command.movement.buttons & ActionAdapter::kAttackButton) != 0U)
	{
		services_->behavior_observeC4AttackDispatched(index, handle, command.movement.buttons);
	}
	if (services_->service_runtimeProfiler().enabled() && gpMetaUtilFuncs != nullptr &&
		gpMetaUtilFuncs->pfnLogConsole != nullptr && services_->service_pluginId() != nullptr &&
		services_->service_globals() != nullptr && std::isfinite(services_->service_globals()->time) &&
		(!std::isfinite(actors_[index].movementGateDiagnosticTimes) ||
			services_->service_globals()->time < actors_[index].movementGateDiagnosticTimes ||
			services_->service_globals()->time - actors_[index].movementGateDiagnosticTimes >=
				kMovementGateLogIntervalSeconds))
	{
		actors_[index].movementGateDiagnosticTimes = services_->service_globals()->time;
		profiledLogConsole(services_->service_runtimeProfiler(), services_->service_pluginId(),
			"profile movementGate actor=%u generation=%u map=%u round=%u frame=%u "
			"freeze_active=%d freeze_until=%.3f explicit_frozen=%d phase=%d "
			"maxspeed_before=%.1f maxspeed_after=%.1f speed_restored=%d action_stop=%d "
			"input=(%.1f %.1f %.1f) buttons=%u dispatch=%d",
			static_cast<unsigned int>(handle.actor.slot), static_cast<unsigned int>(handle.actor.actorGeneration),
			static_cast<unsigned int>(services_->service_lifecycle().mapGeneration()),
			static_cast<unsigned int>(services_->service_lifecycle().roundGeneration()),
			static_cast<unsigned int>(services_->service_adapterFrameCount()), roundFreeze ? 1 : 0,
			services_->service_roundFreezeScheduled() ? services_->service_roundFreezeUntil() : 0.0f,
			explicitFrozen ? 1 : 0, static_cast<int>(gateDecision.phase), maxSpeed, handle.entity->v.maxspeed,
			speedRestored ? 1 : 0, actors_[index].managedBotActionStopsMovement ? 1 : 0, gateDecision.forward,
			gateDecision.side, gateDecision.up, static_cast<unsigned int>(gateDecision.buttons),
			static_cast<int>(dispatchReceipt.result));
	}
	const bool freezeActiveAfterDispatch = services_->roundFreezeActive();
	const bool freezeMovementInput =
		std::fabs(gateDecision.forward) > ASTRABOT_BOT_COMMAND_RUNTIME_FREEZE_MOVEMENT_INPUT_TOLERANCE ||
		std::fabs(gateDecision.side) > ASTRABOT_BOT_COMMAND_RUNTIME_FREEZE_MOVEMENT_INPUT_TOLERANCE ||
		std::fabs(gateDecision.up) > ASTRABOT_BOT_COMMAND_RUNTIME_FREEZE_MOVEMENT_INPUT_TOLERANCE;
	if (services_->service_runtimeProfiler().enabled() && (roundFreeze || freezeActiveAfterDispatch) &&
		((services_->service_adapterFrameCount() % kFreezeDiagnosticFrameInterval) == 0U ||
			roundFreeze != freezeActiveAfterDispatch || freezeMovementInput) &&
		gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr &&
		services_->service_pluginId() != nullptr)
	{
		profiledLogConsole(services_->service_runtimeProfiler(), services_->service_pluginId(),
			"profile freezeMotion actor=%u generation=%u map=%u round=%u frame=%u "
			"freeze_gate=%d freeze_after=%d origin=(%.1f %.1f %.1f) "
			"velocity=(%.1f %.1f %.1f) input=(%.1f %.1f %.1f) buttons=%u",
			static_cast<unsigned int>(handle.actor.slot), static_cast<unsigned int>(handle.actor.actorGeneration),
			static_cast<unsigned int>(services_->service_lifecycle().mapGeneration()),
			static_cast<unsigned int>(services_->service_lifecycle().roundGeneration()),
			static_cast<unsigned int>(services_->service_adapterFrameCount()), roundFreeze ? 1 : 0,
			freezeActiveAfterDispatch ? 1 : 0, handle.entity->v.origin.x, handle.entity->v.origin.y,
			handle.entity->v.origin.z, handle.entity->v.velocity.x, handle.entity->v.velocity.y,
			handle.entity->v.velocity.z, gateDecision.forward, gateDecision.side, gateDecision.up,
			static_cast<unsigned int>(gateDecision.buttons));
	}
	if (gateDecision.invalidateTemplate)
	{
		resetManagedBotCommandTemplate(index);
	}
	return dispatchReceipt;
}

MovementCommandDraft BotCommandRuntime::beginDirectNeutralCommand(
	std::size_t index, FakeClientHandle &handle, std::uint8_t milliseconds)
{
	MovementCommandDraft draft{};
	runtime::CommandReceipt receipt = {
		handle.actor, 0U, services_->service_adapterFrameCount(), runtime::DispatchResult::NoCommand};
	if (index >= NativeBotObservation::kClientSlotCount || handle.entity == nullptr ||
		actors_[index].managedBotCommandSequences == (std::numeric_limits<std::uint32_t>::max)())
	{
		draft.receipt = receipt;
		return draft;
	}

	const runtime::LifecycleToken token = services_->service_lifecycle().tokenForSlot(handle.actor.slot);
	if (!services_->service_lifecycle().isCurrent(token))
	{
		receipt.result = runtime::DispatchResult::StaleActor;
		draft.receipt = receipt;
		return draft;
	}

	runtime::BotCommand &command = draft.command;
	command.actor = handle.actor;
	command.lifecycle = token;
	command.sequence = ++actors_[index].managedBotCommandSequences;
	command.issueFrame = services_->service_adapterFrameCount();
	command.viewAngles = {handle.entity->v.v_angle[0], handle.entity->v.v_angle[1], handle.entity->v.v_angle[2]};
	command.movement = {0.0f, 0.0f, 0.0f, 0U, 0U, milliseconds};
	runtime::MovementPhysicsState &before = draft.before;
	before = services_->observation_captureMovementPhysicsState(
		handle.entity, services_->service_joinControllers()[index].teamConfirmed(), true);
	if (before.dead)
	{
		receipt.result = runtime::DispatchResult::NoCommand;
		draft.receipt = receipt;
		return draft;
	}
	draft.receipt = receipt;
	draft.ready = true;
	return draft;
}

runtime::CommandReceipt BotCommandRuntime::finishDirectNeutralCommand(std::size_t index, FakeClientHandle &handle,
	std::uint8_t, MovementCommandDraft draft, const ActionProposal &requestedAction)
{
	runtime::BotCommand command = draft.command;
	runtime::CommandReceipt receipt = draft.receipt;
	const ActionProposal actionProposal = ActionAdapter::forLiveDispatch(
		requestedAction, services_->observation_hasObservedActiveWeapon(index), command.viewAngles);
	const ActionDispatch actionDispatch = ActionAdapter::translate(actionProposal);
	command.viewAngles = actionDispatch.viewAngles;
	command.synchronizeCombatView = actionProposal.hasCombatAim || actionProposal.hasGuardLook || actionProposal.preserveUpkeepView;
	applyUseEntityLook(index, *handle.entity, actionProposal, command);
	command.movement.buttons = actionDispatch.buttons;
	ActionAdapter::applyCombatMovement(command, actionProposal, handle.entity->v.maxspeed);
	if (actionProposal.kind != ActionKind::None && gpMetaUtilFuncs != nullptr &&
		gpMetaUtilFuncs->pfnLogConsole != nullptr && services_->service_pluginId() != nullptr &&
		(services_->service_adapterFrameCount() % 16U) == 0U)
	{
		profiledLogConsole(services_->service_runtimeProfiler(), services_->service_pluginId(),
			"bot action=%s slot=%u generation=%u frame=%u buttons=%u", actionKindName(actionProposal.kind),
			static_cast<unsigned int>(handle.actor.slot), static_cast<unsigned int>(handle.actor.actorGeneration),
			static_cast<unsigned int>(services_->service_adapterFrameCount()),
			static_cast<unsigned int>(actionDispatch.buttons));
	}
	if (actionDispatch.clientCommand != nullptr)
	{
		(void)dispatchManagedActionCommand(index, handle, actionProposal, actionDispatch);
	}
	if (services_->service_inputDispatcher().enqueue(command) != runtime::QueueResult::Accepted)
	{
		receipt.result = runtime::DispatchResult::InvalidCommand;
		return receipt;
	}
	const Vector commandStart = handle.entity->v.origin;
	const auto hostageUseTarget = actors_[index].hostageUseTarget;
	const runtime::CommandReceipt dispatchReceipt =
		services_->service_inputDispatcher().dispatchNext(handle.actor, services_->service_adapterFrameCount());
	observeReloadInput(handle, command, dispatchReceipt);
	observeHostageUse(index, command, dispatchReceipt, hostageUseTarget, commandStart);
	if (dispatchReceipt.result == runtime::DispatchResult::Dispatched &&
		(actionDispatch.buttons & ActionAdapter::kAttackButton) != 0U)
	{
		services_->behavior_observeNeutralC4AttackDispatched(index, handle);
	}
	return dispatchReceipt;
}

runtime::CommandReceipt BotCommandRuntime::dispatchJoinHeartbeat(
	std::size_t index, FakeClientHandle &handle, std::uint8_t milliseconds)
{
	runtime::CommandReceipt receipt = {
		handle.actor, 0U, services_->service_adapterFrameCount(), runtime::DispatchResult::NoCommand};
	if (index >= NativeBotObservation::kClientSlotCount || handle.entity == nullptr ||
		actors_[index].managedBotCommandSequences == (std::numeric_limits<std::uint32_t>::max)())
	{
		return receipt;
	}
	const runtime::LifecycleToken token = services_->service_lifecycle().tokenForSlot(handle.actor.slot);
	if (!services_->service_lifecycle().isCurrent(token) ||
		!services_->service_joinControllers()[index].isCurrent(handle.actor))
	{
		receipt.result = runtime::DispatchResult::StaleActor;
		return receipt;
	}
	if (services_->service_engineFunctions() == nullptr ||
		services_->service_engineFunctions()->pfnRunPlayerMove == nullptr)
	{
		receipt.result = runtime::DispatchResult::EngineUnavailable;
		return receipt;
	}
	const float viewAngles[3] = {handle.entity->v.v_angle[0], handle.entity->v.v_angle[1], handle.entity->v.v_angle[2]};
	receipt.sequence = ++actors_[index].managedBotCommandSequences;
	const unsigned short buttons = services_->service_joinControllers()[index].phase() == JoinPhase::WaitingTeamMenu
		? static_cast<unsigned short>(IN_ATTACK)
		: 0U;
	{
		RuntimeProfilerScope moveScope(services_->service_runtimeProfiler(), RuntimeProfilerStage::RunPlayerMove);
		services_->service_engineFunctions()->pfnRunPlayerMove(
			handle.entity, viewAngles, 0.0f, 0.0f, 0.0f, buttons, 0U, milliseconds);
	}
	receipt.result = runtime::DispatchResult::Dispatched;
	return receipt;
}

void BotCommandRuntime::onActorRemoved(std::size_t index)
{
	actors_[index].defaultLookAheadYaw = 0.0f;
	actors_[index].playerAvoidance.reset();
	actors_[index].navigationWalking = false;
	actors_[index].managedMoveSpeeds = 0.0f;
	actors_[index].managedCommandDiagnosticUpdates = 0U;
	actors_[index].managedBotJumpCrouch.reset();
	resetManagedBotCommandTemplate(index);
	actors_[index].managedBotCommandSequences = 0U;
	actors_[index].movementGateDiagnosticTimes = std::numeric_limits<float>::quiet_NaN();
}

void BotCommandRuntime::onActorRemembered(std::size_t index)
{
	actors_[index].playerAvoidance.reset();
}

void BotCommandRuntime::resetSharedExecutionState()
{
	reset_managedMoveSpeeds();
	reset_managedCommandDiagnosticUpdates();
	fill_managedBotCommandSequences(0U);
	fill_movementGateDiagnosticTimes(std::numeric_limits<float>::quiet_NaN());
}

void BotCommandRuntime::resetActorExecutionState(std::size_t index)
{
	actors_[index].managedBotJumpCrouch.reset();
	actors_[index].navigationWalking = false;
	resetManagedBotCommandTemplate(index);
}

MovementCommandDraft BotCommandRuntime::beginMovementCommand(std::size_t index, FakeClientHandle &handle,
	const runtime::LifecycleToken &token, const runtime::MovementPhysicsState &before, NavigationUpdate &movement)
{
	MovementCommandDraft draft{};
	const auto &locomotionIntent = movement.locomotionIntent;
	runtime::BotCommand &command = draft.command;
	command.actor = handle.actor;
	command.lifecycle = token;
	command.sequence = 0U;
	command.issueFrame = 0U;
	const auto &machine = services_->behavior_managedBotStateMachines(index);
	auto &actor = actors_[index];
	const bool huntApproach = usesApproachLook(machine.state()) && actor.huntApproachLookActive;
	const bool navigationLook =
		services_->service_compatibilitySurface().configuration().mode == compat::RuntimeMode::Compatibility &&
		!machine.attackOverlayActive() && !locomotionIntent.nativeLadderControl &&
		usesNavigationLook(machine.state()) &&
		(locomotionIntent.speed > 0.0f || huntApproach || movement.huntApproachLook.tracking() ||
			actor.huntEncounterUpkeepAt == services_->service_globals()->time ||
			(machine.state() == compat::CompatibilityStateId::Hunt && movement.roamDecision.huntEndpointSteering));
	const bool defuseLook = movement.observation.defuseApproach && isDefuseApproach(machine) &&
		!machine.attackOverlayActive() && !locomotionIntent.nativeLadderControl && locomotionIntent.speed > 0.0f;
	command.synchronizeCombatView = navigationLook || defuseLook;
	command.viewAngles = (locomotionIntent.nativeLadderControl || command.synchronizeCombatView)
		? runtime::ViewAngles{handle.entity->v.v_angle.x, handle.entity->v.v_angle.y, 0.0f}
		: runtime::ViewAngles{0.0f, movementYaw(locomotionIntent.direction), 0.0f};
	if (navigationLook && (std::isfinite(actor.navigationLookYaw) || huntApproach) &&
		actor.descentLookAt != services_->service_globals()->time &&
		actor.huntEncounterUpkeepAt != services_->service_globals()->time &&
		!(handle.entity->v.flags & FL_FROZEN) && !services_->roundFreezeActive() &&
		(huntApproach || canAdvanceNavigationLook(machine.state(), before.grounded)))
	{
		command.viewAngles = stepNavigationLook(index, *handle.entity, machine.state() == compat::CompatibilityStateId::Hunt);
	}
	std::uint16_t movementButtons = locomotionIntent.posture == nav::LocomotionPosture::Crouching
		? static_cast<std::uint16_t>(IN_DUCK)
		: static_cast<std::uint16_t>(0U);
	if (locomotionIntent.traversal == nav::TraversalAction::Jump || locomotionIntent.nativeMustJump)
	{
		movementButtons = static_cast<std::uint16_t>(movementButtons | IN_JUMP);
	}
	if (locomotionIntent.traversal == nav::TraversalAction::Crouch ||
		locomotionIntent.traversal == nav::TraversalAction::NarrowPassage)
	{
		movementButtons = static_cast<std::uint16_t>(movementButtons | IN_DUCK);
	}
	command.movement = {0.0f, 0.0f, 0.0f, movementButtons, 0U, 1U};
	draft.ready = true;
	return draft;
}

void BotCommandRuntime::finishMovementCommand(std::size_t index, FakeClientHandle &handle,
	const runtime::LifecycleToken &, const runtime::MovementPhysicsState &before, NavigationUpdate &movement,
	MovementCommandDraft draft, const ActionProposal &requestedAction)
{
	runtime::BotCommand command = draft.command;
	auto &roamDecision = movement.roamDecision;
	const auto &locomotionIntent = movement.locomotionIntent;
	const auto &observation = movement.observation;
	const auto &landingFeedback = movement.landingFeedback;
	const bool compatibilityMovement =
		services_->service_compatibilitySurface().configuration().mode == compat::RuntimeMode::Compatibility;
	const auto &machine = services_->behavior_managedBotStateMachines(index);
	const bool priorityLook = machine.attackOverlayActive() || requestedAction.hasCombatAim || requestedAction.hasGuardLook;
	const bool canUseNavigationLook = compatibilityMovement && !priorityLook && !locomotionIntent.nativeLadderControl;
	const bool huntEndpoint = canUseNavigationLook && roamDecision.huntEndpointSteering &&
		machine.state() == compat::CompatibilityStateId::Hunt;
	const bool defuseLook = canUseNavigationLook && observation.defuseApproach && isDefuseApproach(machine) &&
		locomotionIntent.speed > 0.0f;
	auto &actor = actors_[index];
	const bool huntApproach = canUseNavigationLook && usesApproachLook(machine.state()) &&
		(actor.huntApproachLookActive || movement.huntApproachLook.tracking() ||
			actor.huntEncounterUpkeepAt == services_->service_globals()->time);
	const bool navigationLook = huntEndpoint || defuseLook || huntApproach ||
		(canUseNavigationLook && usesNavigationLook(machine.state()) && locomotionIntent.speed > 0.0f);
	const bool descentLookAdvanced = actor.descentLookAt == services_->service_globals()->time &&
		machine.state() == compat::CompatibilityStateId::Hunt && !machine.attackOverlayActive();
	if (!compatibilityMovement || before.dead || priorityLook || clearsHuntApproachLook(machine.state()))
		actor.huntApproachLookActive = false;
	if (!navigationLook)
	{
		if (locomotionIntent.nativeLadderControl && descentLookAdvanced)
		{
			actor.navigationLookYaw = std::numeric_limits<float>::quiet_NaN();
			actor.defusePathLook = false;
			actor.descentFloorLook = false;
		}
		else
			actor.resetPathLook();
	}
	else if (!requestedAction.preserveUpkeepView &&
		(locomotionIntent.speed > 0.0f || (huntEndpoint && roamDecision.huntWaitingForFriend)))
	{
		// Set the next upkeep's look target after taking this decision's view.
		actor.huntWaitNavigationLook = huntEndpoint && roamDecision.huntWaitingForFriend;
		actor.descentFloorLook = descentLookAdvanced &&
			(actor.descentLookPhase == nav::NativeLadderPhase::MoveToDestination ||
				(actor.descentLookPhase == nav::NativeLadderPhase::StoppedHuntLook && before.onLadder) ||
				(actor.descentFloorLook && !before.grounded));
		actor.defusePathLook = defuseLook;
		if (huntEndpoint || defuseLook)
		{
			const auto &look = defuseLook ? roamDecision.defuseLookPoint : roamDecision.huntEndpoint;
			actor.navigationLookYaw = movementYaw({look.x - before.origin.x, look.y - before.origin.y, 0.0f});
			actor.defaultLookAheadYaw = nativeLookAheadYaw({look.x - before.origin.x, look.y - before.origin.y, 0.0f});
			if (defuseLook)
			{
				// CSBot uses the engine's integer, nonnegative VecToYaw for path look.
				const float yaw = actor.navigationLookYaw;
				actor.navigationLookYaw = std::trunc(yaw < 0.0f ? yaw + 360.0f : yaw);
			}
		}
		else
		{
			actor.navigationLookYaw = movementYaw(locomotionIntent.direction);
			actor.defaultLookAheadYaw = nativeLookAheadYaw(locomotionIntent.direction);
		}
		if (actor.resumedDefaultLook)
		{
			actor.navigationLookSpring = actor.lastLookSpring;
			actor.resumedDefaultLook = false;
		}
	}
	if (compatibilityMovement && !priorityLook && !before.dead && usesApproachLook(machine.state()) &&
		movement.huntApproachLook.update != HuntApproachLookUpdate::Unchanged)
	{
		actor.huntApproachLookActive = movement.huntApproachLook.tracking();
		actor.huntEncounterLook = movement.huntApproachLook.update == HuntApproachLookUpdate::TrackEncounter;
		actor.huntEncounterAlignedAt = -1.0f;
		actor.huntApproachLookSpot = movement.huntApproachLook.spot;
		if (actor.huntApproachLookActive && actor.resumedDefaultLook)
		{
			actor.navigationLookSpring = actor.lastLookSpring;
			actor.resumedDefaultLook = false;
		}
	}
	const ActionProposal actionProposal = ActionAdapter::forLiveDispatch(
		requestedAction, services_->observation_hasObservedActiveWeapon(index), draft.command.viewAngles);
	const ActionDispatch actionDispatch = ActionAdapter::translate(actionProposal);
	command.viewAngles = actionDispatch.viewAngles;
	if (navigationLook && !actionProposal.preserveUpkeepView)
	{
		const auto &view = handle.entity->v.v_angle;
		command.viewAngles = draft.command.synchronizeCombatView ? draft.command.viewAngles
															  : runtime::ViewAngles{view.x, view.y, view.z};
	}
	command.synchronizeCombatView =
		actionProposal.hasCombatAim || actionProposal.hasGuardLook || locomotionIntent.nativeLadderControl || navigationLook ||
		actionProposal.preserveUpkeepView;
	applyUseEntityLook(index, *handle.entity, actionProposal, command);
	command.movement.buttons = actionDispatch.buttons;
	actors_[index].nativeLadderControl = locomotionIntent.nativeLadderControl;
	actors_[index].nativeLadderJump = locomotionIntent.nativeMustJump;
	float commandSpeed = locomotionIntent.speed;
	actor.navigationWalking = false;
	if (compatibilityMovement && commandSpeed > 0.0f && std::isfinite(handle.entity->v.maxspeed) &&
		handle.entity->v.maxspeed > 0.0f)
	{
		commandSpeed = handle.entity->v.maxspeed;
		const auto &goal = services_->behavior_managedBotObjectiveTargets(index);
		const bool finalArea = roamDecision.currentArea != 0U && roamDecision.currentArea == roamDecision.goalArea;
		const bool approachingDefuseEndpoint = machine.state() == compat::CompatibilityStateId::MoveTo &&
			machine.task() == compat::CompatibilityTaskId::DefuseBomb && roamDecision.goalPresent &&
			roamDecision.corridorAreaCount != 0U && roamDecision.corridorIndex + 1U >= roamDecision.corridorAreaCount;
		const auto &target = approachingDefuseEndpoint ? locomotionIntent.targetPosition : goal.target;
		const float x = target.x - before.origin.x;
		const float y = target.y - before.origin.y;
		const float footOffset = handle.entity->v.mins.z < 0.0f ? handle.entity->v.mins.z
			: before.ducked ? -18.0f : -36.0f;
		const float z = approachingDefuseEndpoint ? target.z - (before.origin.z + footOffset) : 0.0f;
		constexpr float kGoalSlowdownDistance = 200.0f;
#define ASTRABOT_BOT_COMMAND_RUNTIME_GOAL_SLOWDOWN_FACTOR 0.4f
		const bool following = machine.state() == compat::CompatibilityStateId::Follow && !machine.attackOverlayActive();
		const bool shouldWalk = !actionProposal.runAfterPlantTimeout && !following && (huntEndpoint ? locomotionIntent.walkingToHuntEndpoint
			: (approachingDefuseEndpoint || (goal.valid && finalArea)) &&
				x * x + y * y + z * z < kGoalSlowdownDistance * kGoalSlowdownDistance);
		actor.navigationWalking = !locomotionIntent.nativeLadderControl && shouldWalk &&
			locomotionIntent.posture != nav::LocomotionPosture::Crouching;
		if (actor.navigationWalking)
			commandSpeed *= ASTRABOT_BOT_COMMAND_RUNTIME_GOAL_SLOWDOWN_FACTOR;
	}
	// Retain movement before walking reduction: events between full updates
	// must be able to restore running on the next cached command.
	// executeManagedBotCommand applies the current walk/run decision.
	if (locomotionIntent.nativeLadderControl && locomotionIntent.nativeWalking)
	{
		actor.navigationWalking = true;
		commandSpeed *= .4f;
	}
	if (!roamDecision.huntWaitingForFriend)
		actor.managedMoveSpeeds = commandSpeed;
	const MovementProjection movementProjection = ActionAdapter::projectMovement(locomotionIntent.direction.x,
		locomotionIntent.direction.y, commandSpeed, command.viewAngles.yaw, compatibilityMovement);
	command.movement.forward = actionDispatch.stopMovement ? 0.0f
		: locomotionIntent.nativeLadderControl			   ? locomotionIntent.nativeForward * commandSpeed
														   : movementProjection.forward;
	command.movement.side = actionDispatch.stopMovement ? 0.0f
		: locomotionIntent.nativeLadderControl			? locomotionIntent.nativeSide * commandSpeed
														: movementProjection.side;
	ActionAdapter::applyCombatMovement(command, actionProposal, handle.entity->v.maxspeed);
	if (!actionDispatch.stopMovement)
	{
		command.movement.buttons = static_cast<std::uint16_t>(
			command.movement.buttons | ActionAdapter::movementButtons(command.movement.forward, command.movement.side));
	}
	services_->navigation_logTraversalOutcome(index, before, roamDecision, landingFeedback);
	services_->navigation_logTraversalDiagnostic(
		index, before, roamDecision, locomotionIntent, command.movement.buttons, observation.locomotion);
	services_->navigation_logMovementReversalDiagnostic(index, before, roamDecision, locomotionIntent);
	if (services_->navigation_movementDiagnosticSamples(index) < kMovementPhysicsLogLimit &&
		gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr &&
		services_->service_pluginId() != nullptr)
	{
		profiledLogConsole(services_->service_runtimeProfiler(), services_->service_pluginId(),
			"movement command actor=%u generation=%u frame=%u forward=%.1f side=%.1f up=%.1f "
			"yaw=%.1f buttons=%u msec=%u targetArea=%u nav=(%.2f %.2f) action=%d stop=%d",
			static_cast<unsigned int>(handle.actor.slot), static_cast<unsigned int>(handle.actor.actorGeneration),
			static_cast<unsigned int>(services_->service_adapterFrameCount()), command.movement.forward,
			command.movement.side, command.movement.up, command.viewAngles.yaw,
			static_cast<unsigned int>(command.movement.buttons), static_cast<unsigned int>(command.movement.msec),
			static_cast<unsigned int>(locomotionIntent.targetArea), locomotionIntent.direction.x,
			locomotionIntent.direction.y, static_cast<int>(actionProposal.kind), actionDispatch.stopMovement ? 1 : 0);
	}
	if (actionProposal.kind != ActionKind::None && gpMetaUtilFuncs != nullptr &&
		gpMetaUtilFuncs->pfnLogConsole != nullptr && services_->service_pluginId() != nullptr &&
		(services_->service_adapterFrameCount() % 16U) == 0U)
	{
		profiledLogConsole(services_->service_runtimeProfiler(), services_->service_pluginId(),
			"bot action=%s slot=%u generation=%u frame=%u buttons=%u", actionKindName(actionProposal.kind),
			static_cast<unsigned int>(handle.actor.slot), static_cast<unsigned int>(handle.actor.actorGeneration),
			static_cast<unsigned int>(services_->service_adapterFrameCount()),
			static_cast<unsigned int>(actionDispatch.buttons));
	}
	if (actionDispatch.clientCommand != nullptr)
	{
		(void)dispatchManagedActionCommand(index, handle, actionProposal, actionDispatch);
	}
	if (!command.synchronizeCombatView)
	{
		handle.entity->v.v_angle[0] = command.viewAngles.pitch;
		handle.entity->v.v_angle[1] = command.viewAngles.yaw;
		handle.entity->v.v_angle[2] = command.viewAngles.roll;
		handle.entity->v.angles[0] = -command.viewAngles.pitch / kModelPitchDivisor;
		handle.entity->v.angles[1] = command.viewAngles.yaw;
		handle.entity->v.angles[2] = command.viewAngles.roll;
	}
	actors_[index].managedBotCommandTemplates = command;
	actors_[index].managedBotCommandTemplateValid = true;
	actors_[index].managedBotActionStopsMovement = actionDispatch.stopMovement;
	const runtime::CommandReceipt receipt = executeManagedBotCommand(index, handle, true);
	if (receipt.result != runtime::DispatchResult::Dispatched)
	{
		roamDecision.failureReason = runtime::NavFailureReason::NavApplyRejected;
		services_->navigation_logMovementDiagnostic(index, "nav_apply_rejected", &roamDecision);
	}
	services_->navigation_recordMovementPhysicsSample(index, before, receipt);
	if (receipt.result == runtime::DispatchResult::Dispatched &&
		services_->navigation_movementDiagnosticSamples(index) < kMaximumMovementDiagnosticSamples &&
		gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr &&
		services_->service_pluginId() != nullptr)
	{
		profiledLogConsole(services_->service_runtimeProfiler(), services_->service_pluginId(),
			"movement actor=%u generation=%u frame=%u origin=(%.1f %.1f %.1f) targetArea=%u "
			"sequence=%u stage=%d currentArea=%u recoveryArea=%u",
			static_cast<unsigned int>(handle.actor.slot), static_cast<unsigned int>(handle.actor.actorGeneration),
			static_cast<unsigned int>(services_->service_adapterFrameCount()), handle.entity->v.origin.x,
			handle.entity->v.origin.y, handle.entity->v.origin.z,
			static_cast<unsigned int>(locomotionIntent.targetArea), static_cast<unsigned int>(command.sequence),
			static_cast<int>(roamDecision.stage), static_cast<unsigned int>(roamDecision.currentArea),
			static_cast<unsigned int>(roamDecision.recoveryArea));
		services_->navigation_noteMovementDiagnosticSample(index);
	}
}
#undef ASTRABOT_BOT_COMMAND_RUNTIME_GOAL_SLOWDOWN_FACTOR

void BotCommandRuntime::resetCreatedActorCommand(std::size_t slotIndex)
{
	actors_[slotIndex].defaultLookAheadYaw = 0.0f;
	resetManagedBotCommandTemplate(slotIndex);
	actors_[slotIndex].managedBotCommandSequences = 0U;
}

void BotCommandRuntime::resetCreatedActorGateDiagnostic(std::size_t slotIndex)
{
	actors_[slotIndex].movementGateDiagnosticTimes = std::numeric_limits<float>::quiet_NaN();
}

void BotCommandRuntime::resetJumpCrouch(std::size_t index)
{
	actors_[index].managedBotJumpCrouch.reset();
}

bool BotCommandRuntime::dispatchClientCommand(edict_t *entity, const char *name, const char *argument)
{
	return services_->dispatchClientCommand(entity, name, argument);
}

void BotCommandRuntime::resetGateDiagnostics()
{
	fill_movementGateDiagnosticTimes(std::numeric_limits<float>::quiet_NaN());
}

void BotCommandRuntime::stabilizeFrozenActors()
{
	if (services_->roundFreezeActive())
	{
		for (std::size_t index = 0U; index < services_->service_managedBotHandles().size(); ++index)
		{
			if (!services_->service_managedBotSlots()[index])
			{
				continue;
			}
			edict_t *entity = services_->service_managedBotHandles()[index].entity;
			if (entity == nullptr || entity->free != 0)
			{
				continue;
			}
			const MovementVelocity velocity =
				MovementExecutionGate::stabilizeVelocity(MovementExecutionPhase::RoundFreeze,
					{entity->v.velocity.x, entity->v.velocity.y, entity->v.velocity.z});
			entity->v.velocity.x = velocity.x;
			entity->v.velocity.y = velocity.y;
			entity->v.velocity.z = velocity.z;
		}
	}
}

runtime::QueueResult BotCommandRuntime::enqueue(const runtime::BotCommand &command)
{
	return services_->service_inputDispatcher().enqueue(command);
}

runtime::CommandReceipt BotCommandRuntime::dispatchInput(const runtime::ActorId &actor, std::uint32_t dispatchFrame)
{
	return services_->service_inputDispatcher().dispatchNext(actor, dispatchFrame);
}

} // namespace metamod
} // namespace astrabot
#undef ASTRABOT_BOT_COMMAND_RUNTIME_DEGREES_TO_RADIANS
#undef ASTRABOT_BOT_COMMAND_RUNTIME_FREEZE_MOVEMENT_INPUT_TOLERANCE
