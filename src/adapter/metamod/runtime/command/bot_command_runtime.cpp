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

runtime::ViewAngles stepHuntLook(const edict_t &entity, float desiredYaw, float now, compat::AimSpringState &spring)
{
	const auto &angles = entity.v.v_angle;
	const auto look = compat::trackAim(angles.y, angles.x, desiredYaw + 2.0f * ActionAdapter::csBotCosine(33.0f * now),
		2.0f * ActionAdapter::csBotCosine(13.0f * now - 90.0f), runtime::BotTimingScheduler::kCommandInterval, 1.0f,
		&spring, false);
	return {look.pitch, look.yaw, 0.0f};
}
}

void BotCommandRuntime::applyCombatUpkeep(std::size_t index, const CombatUpkeepUpdate &update)
{
	if (index >= actors_.size() || !actors_[index].managedBotCommandTemplateValid)
		return;
	auto &command = actors_[index].managedBotCommandTemplates;
	if (update.hasAim)
	{
		actors_[index].resetHuntLook();
		if (!update.movementRelativeToAim && !actors_[index].nativeLadderControl)
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
		actors_[index].resetHuntLook();
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
	const runtime::MovementPhysicsState &, MovementCommandDraft draft, const ActionProposal &requestedAction)
{
	runtime::BotCommand command = draft.command;
	actors_[index].nativeLadderControl = false;
	actors_[index].nativeLadderJump = false;
	actors_[index].resetHuntLook();
	const ActionProposal actionProposal = ActionAdapter::forLiveDispatch(
		requestedAction, services_->observation_hasObservedActiveWeapon(index), command.viewAngles);
	const ActionDispatch actionDispatch = ActionAdapter::translate(actionProposal);
	command.viewAngles = actionDispatch.viewAngles;
	command.synchronizeCombatView = actionProposal.hasCombatAim || actionProposal.hasGuardLook;
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

runtime::CommandReceipt BotCommandRuntime::executeManagedBotCommand(
	std::size_t index, FakeClientHandle &handle, bool huntLookAdvanced)
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
	const bool explicitFrozen = (handle.entity->v.flags & FL_FROZEN) != 0;
	const bool roundFreeze = services_->roundFreezeActive();
	const auto &machine = services_->behavior_managedBotStateMachines(index);
	if (!huntLookAdvanced && !explicitFrozen && !roundFreeze && std::isfinite(actor.huntLookYaw) &&
		machine.state() == compat::CompatibilityStateId::Hunt && !machine.attackOverlayActive())
	{
		// CSBot's upkeep turns the view while keeping the axes from the last full decision.
		command.viewAngles = stepHuntLook(
			*handle.entity, actor.huntLookYaw, services_->service_globals()->time, actor.huntLookSpring);
		command.synchronizeCombatView = true;
	}
	if (!actors_[index].nativeLadderControl)
		services_->behavior_applyNoiseLookCommand(index, command);
	services_->behavior_applyNativeLadderLookCommand(index, command, actors_[index].nativeLadderControl);
	services_->behavior_applyObstacleCommand(index, command);
	const float maxSpeed = handle.entity->v.maxspeed;
	const bool walking = !actors_[index].nativeLadderControl && !services_->behavior_forcedRunning(index) &&
		services_->behavior_noiseInvestigationWalking(index) && (handle.entity->v.flags & FL_ONGROUND) &&
		handle.entity->v.movetype != MOVETYPE_FLY && !(command.movement.buttons & IN_DUCK);
	const float movementSpeed = actors_[index].managedMoveSpeeds > 0.0f
		? actors_[index].managedMoveSpeeds : maxSpeed;
	constexpr float kWalkingSpeedFraction = .4f;
	const float walkingLimit = kWalkingSpeedFraction * maxSpeed;
	if (walking)
	{
		const float speed = std::hypot(command.movement.forward, command.movement.side);
		if (std::isfinite(speed) && std::isfinite(walkingLimit) && walkingLimit > 0 && speed > walkingLimit)
		{
			command.movement.forward *= walkingLimit / speed;
			command.movement.side *= walkingLimit / speed;
		}
	}
	if (!explicitFrozen && !roundFreeze && !actors_[index].managedBotActionStopsMovement &&
		!actors_[index].nativeLadderControl)
	{
		applyPlayerAvoidance(index, &command, walking ? (std::min)(movementSpeed, walkingLimit) : movementSpeed);
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
	const runtime::JumpCrouchCommandDecision jumpCrouchDecision = actors_[index].managedBotJumpCrouch.updateCommand(
		jumpCrouchObservation, command.movement.buttons, static_cast<std::uint16_t>(IN_DUCK));
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
	if (dispatchReceipt.result == runtime::DispatchResult::Dispatched &&
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
	command.synchronizeCombatView = actionProposal.hasCombatAim || actionProposal.hasGuardLook;
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
	const runtime::CommandReceipt dispatchReceipt =
		services_->service_inputDispatcher().dispatchNext(handle.actor, services_->service_adapterFrameCount());
	observeReloadInput(handle, command, dispatchReceipt);
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
	actors_[index].playerAvoidance.reset();
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
	resetManagedBotCommandTemplate(index);
}

MovementCommandDraft BotCommandRuntime::beginMovementCommand(std::size_t index, FakeClientHandle &handle,
	const runtime::LifecycleToken &token, const runtime::MovementPhysicsState &, NavigationUpdate &movement)
{
	MovementCommandDraft draft{};
	const auto &locomotionIntent = movement.locomotionIntent;
	runtime::BotCommand &command = draft.command;
	command.actor = handle.actor;
	command.lifecycle = token;
	command.sequence = 0U;
	command.issueFrame = 0U;
	const auto &machine = services_->behavior_managedBotStateMachines(index);
	const bool huntEndpoint = movement.roamDecision.huntEndpointSteering &&
		services_->service_compatibilitySurface().configuration().mode == compat::RuntimeMode::Compatibility &&
		machine.state() == compat::CompatibilityStateId::Hunt && !machine.attackOverlayActive() &&
		!locomotionIntent.nativeLadderControl;
	command.viewAngles = (locomotionIntent.nativeLadderControl || huntEndpoint)
		? runtime::ViewAngles{handle.entity->v.v_angle.x, handle.entity->v.v_angle.y, 0.0f}
		: runtime::ViewAngles{0.0f, movementYaw(locomotionIntent.direction), 0.0f};
	auto &actor = actors_[index];
	if (huntEndpoint && std::isfinite(actor.huntLookYaw) &&
		!(handle.entity->v.flags & FL_FROZEN) && !services_->roundFreezeActive())
	{
		command.viewAngles =
			stepHuntLook(*handle.entity, actor.huntLookYaw, services_->service_globals()->time, actor.huntLookSpring);
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
	const bool huntEndpoint = compatibilityMovement && roamDecision.huntEndpointSteering &&
		machine.state() == compat::CompatibilityStateId::Hunt && !machine.attackOverlayActive() &&
		!locomotionIntent.nativeLadderControl && !requestedAction.hasCombatAim && !requestedAction.hasGuardLook;
	auto &actor = actors_[index];
	if (!huntEndpoint)
	{
		actor.resetHuntLook();
	}
	else if (locomotionIntent.speed > 0.0f)
	{
		// Set the next upkeep's look target after taking this decision's view.
		actor.huntLookYaw = movementYaw({
			roamDecision.huntEndpoint.x - before.origin.x, roamDecision.huntEndpoint.y - before.origin.y, 0.0f});
	}
	const ActionProposal actionProposal = ActionAdapter::forLiveDispatch(
		requestedAction, services_->observation_hasObservedActiveWeapon(index), draft.command.viewAngles);
	const ActionDispatch actionDispatch = ActionAdapter::translate(actionProposal);
	command.viewAngles = huntEndpoint ? draft.command.viewAngles : actionDispatch.viewAngles;
	command.synchronizeCombatView =
		actionProposal.hasCombatAim || actionProposal.hasGuardLook || locomotionIntent.nativeLadderControl || huntEndpoint;
	command.movement.buttons = actionDispatch.buttons;
	actors_[index].nativeLadderControl = locomotionIntent.nativeLadderControl;
	actors_[index].nativeLadderJump = locomotionIntent.nativeMustJump;
	float commandSpeed = locomotionIntent.speed;
	if (compatibilityMovement && commandSpeed > 0.0f && std::isfinite(handle.entity->v.maxspeed) &&
		handle.entity->v.maxspeed > 0.0f)
	{
		commandSpeed = handle.entity->v.maxspeed;
		const auto &goal = services_->behavior_managedBotObjectiveTargets(index);
		const float x = goal.target.x - before.origin.x, y = goal.target.y - before.origin.y;
		const bool finalArea = roamDecision.currentArea != 0U && roamDecision.currentArea == roamDecision.goalArea;
		constexpr float kGoalSlowdownDistance = 200.0f;
#define ASTRABOT_BOT_COMMAND_RUNTIME_GOAL_SLOWDOWN_FACTOR 0.4f
		const auto &feet = observation.locomotion.position;
		const float huntX = roamDecision.huntEndpoint.x - feet.x, huntY = roamDecision.huntEndpoint.y - feet.y,
					huntZ = roamDecision.huntEndpoint.z - feet.z;
		const bool nearHuntEnd = huntEndpoint && !observation.locomotion.ducked &&
			roamDecision.corridorIndex + 1U == roamDecision.corridorAreaCount &&
			huntX * huntX + huntY * huntY + huntZ * huntZ < kGoalSlowdownDistance * kGoalSlowdownDistance;
		const bool slowForEndpoint = huntEndpoint ? nearHuntEnd
			: goal.valid && finalArea && x * x + y * y < kGoalSlowdownDistance * kGoalSlowdownDistance;
		if (!locomotionIntent.nativeLadderControl && slowForEndpoint &&
			locomotionIntent.posture != nav::LocomotionPosture::Crouching)
			commandSpeed *= ASTRABOT_BOT_COMMAND_RUNTIME_GOAL_SLOWDOWN_FACTOR;
	}
	// Retain movement before walking reduction: events between full updates
	// must be able to restore running on the next cached command.
	// executeManagedBotCommand applies the current walk/run decision.
	if (locomotionIntent.nativeLadderControl && locomotionIntent.nativeWalking)
		commandSpeed *= .4f;
	actors_[index].managedMoveSpeeds = commandSpeed;
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
