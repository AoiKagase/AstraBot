#include "bot_command_runtime.hpp"
#include "bot_runtime_support.hpp"
namespace astrabot
{
namespace metamod
{
using namespace runtime_detail;

void BotCommandRuntime::onTouch(edict_t *entity, edict_t *other)
{
	if (!services_->runtimeIsActive() || services_->service_globals() == nullptr ||
		!std::isfinite(services_->service_globals()->time) ||
		services_->service_compatibilitySurface().configuration().mode !=
			compat::RuntimeMode::Compatibility)
		return;
	const auto observe = [&](edict_t *self, edict_t *contact) {
		if (self == nullptr || contact == nullptr || self == contact || self->free ||
			contact->free || (self->v.flags & FL_CLIENT) == 0 ||
			(contact->v.flags & FL_CLIENT) == 0 || self->v.deadflag != DEAD_NO ||
			contact->v.deadflag != DEAD_NO || self->v.health <= 0.0f || contact->v.health <= 0.0f)
			return;
		const auto actor = services_->observation_publicPlayerIdentity(self),
				   touched = services_->observation_publicPlayerIdentity(contact);
		if (actor.slot == 0U || touched.slot == 0U)
			return;
		const auto index = static_cast<std::size_t>(actor.slot - 1U);
		if (!services_->service_managedBotSlots()[index] ||
			services_->service_managedBotHandles()[index].entity != self ||
			!services_->service_lifecycle().isCurrent(
				services_->service_lifecycle().tokenForSlot(actor.slot)))
			return;
		const world::FrameIdentity frame = {services_->service_lifecycle().mapGeneration(),
											services_->service_lifecycle().roundGeneration(),
											services_->service_adapterFrameCount()};
		runtime::PlayerAvoidanceTouch touch = {
			actor,
			frame,
			touched,
			services_->publicPlayerPriority(self),
			services_->publicPlayerPriority(contact),
			services_->service_globals()->time,
			services_->behavior_managedBotStateMachines(index).state() ==
				compat::CompatibilityStateId::DefuseBomb};
		runtime::ActorId previous{};
		if (actors_[index].playerAvoidance.target(actor, frame, services_->service_globals()->time,
												  &previous) &&
			services_->service_engineFunctions() != nullptr &&
			services_->service_engineFunctions()->pfnPEntityOfEntIndex != nullptr)
		{
			const auto *incumbent = services_->service_engineFunctions()->pfnPEntityOfEntIndex(
				static_cast<int>(previous.slot));
			const auto current = services_->observation_publicPlayerIdentity(incumbent);
			if (current.slot == previous.slot &&
				current.actorGeneration == previous.actorGeneration)
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

void BotCommandRuntime::applyPlayerAvoidance(std::size_t index, runtime::BotCommand *command)
{
	if (command == nullptr || index >= services_->service_managedBotHandles().size() ||
		services_->service_globals() == nullptr ||
		services_->service_compatibilitySurface().configuration().mode !=
			compat::RuntimeMode::Compatibility ||
		services_->service_engineFunctions() == nullptr ||
		services_->service_engineFunctions()->pfnPEntityOfEntIndex == nullptr)
		return;
	const auto &handle = services_->service_managedBotHandles()[index];
	runtime::ActorId target{};
	const world::FrameIdentity frame = {services_->service_lifecycle().mapGeneration(),
										services_->service_lifecycle().roundGeneration(),
										services_->service_adapterFrameCount()};
	if (!actors_[index].playerAvoidance.target(handle.actor, frame,
											   services_->service_globals()->time, &target))
		return;
	const auto *other =
		services_->service_engineFunctions()->pfnPEntityOfEntIndex(static_cast<int>(target.slot));
	const auto identity = services_->observation_publicPlayerIdentity(other);
	const auto *self = handle.entity;
	if (identity.slot != target.slot || identity.actorGeneration != target.actorGeneration ||
		other == nullptr || other->v.deadflag != DEAD_NO || other->v.health <= 0.0f ||
		self == nullptr || self->v.deadflag != DEAD_NO || self->v.health <= 0.0f)
	{
		actors_[index].playerAvoidance.reset();
		return;
	}
	if ((self->v.flags & FL_ONGROUND) == 0 || self->v.movetype == MOVETYPE_FLY ||
		services_->service_engineFunctions()->pfnTraceLine == nullptr)
		return;
	const float yaw = command->viewAngles.yaw * 0.017453292519943295f;
	const float dx = other->v.origin.x - self->v.origin.x,
				dy = other->v.origin.y - self->v.origin.y;
	const float length = std::hypot(dx, dy);
	if (!std::isfinite(length) || length <= 0.0f || !std::isfinite(yaw))
		return;
	const auto move = runtime::playerAvoidanceMove(
		command->viewAngles.yaw, dx, dy, command->movement.forward, command->movement.side,
		actors_[index].managedMoveSpeeds > 0.0f ? actors_[index].managedMoveSpeeds
												: self->v.maxspeed);
	if (!move.valid)
		return;
	const float worldX = move.forward * std::cos(yaw) + move.side * std::sin(yaw);
	const float worldY = move.forward * std::sin(yaw) - move.side * std::cos(yaw);
	const float speed = std::hypot(worldX, worldY);
	const float feetZ = self->v.origin.z + self->v.mins.z;
	const float start[3] = {self->v.origin.x + 30.0f * worldX / speed,
							self->v.origin.y + 30.0f * worldY / speed, feetZ + 36.0f};
	const float end[3] = {start[0], start[1], feetZ - nav::LocomotionConfig::kMaximumStepHeight};
	TraceResult ground{};
	services_->service_engineFunctions()->pfnTraceLine(start, end, 1, handle.entity, &ground);
	const bool safe =
		!ground.fStartSolid && !ground.fAllSolid && std::isfinite(ground.flFraction) &&
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
	if (services_->service_runtimeProfiler().enabled() &&
		(services_->service_adapterFrameCount() % 16U) == 0U && gpMetaUtilFuncs != nullptr &&
		gpMetaUtilFuncs->pfnLogConsole != nullptr && services_->service_pluginId() != nullptr)
		profiledLogConsole(services_->service_runtimeProfiler(), services_->service_pluginId(),
						   "profile playerAvoidance bot_id=%u target=%u frame=%u applied=%d "
						   "side=%.1f ground_safe=%d forward=%.1f",
						   handle.actor.slot, target.slot, services_->service_adapterFrameCount(),
						   safe ? 1 : 0, move.side, safe ? 1 : 0, move.forward);
}

bool BotCommandRuntime::dispatchManagedActionCommand(std::size_t index, FakeClientHandle &handle,
													 const ActionProposal &proposal,
													 const ActionDispatch &dispatch)
{
	if (dispatch.clientCommand == nullptr)
	{
		return true;
	}
	const bool commandDispatched =
		services_->dispatchClientCommand(handle.entity, dispatch.clientCommand, "");
	if (commandDispatched && proposal.kind == ActionKind::Reload &&
		services_->service_globals() != nullptr)
	{
		services_->observation_observeReloadDispatched(
			{handle.actor.slot, handle.actor.actorGeneration},
			{services_->service_lifecycle().mapGeneration(),
			 services_->service_lifecycle().roundGeneration(),
			 services_->service_adapterFrameCount()},
			services_->service_globals()->time);
	}
	const bool c4SelectionRequest = proposal.kind == ActionKind::SelectC4 ||
									proposal.kind == ActionKind::Plant ||
									proposal.kind == ActionKind::PlantContinue;
	if (commandDispatched && c4SelectionRequest && index < NativeBotObservation::kClientSlotCount)
	{
		services_->behavior_observeC4SelectionDispatched(index, handle);
	}
	if (c4SelectionRequest && services_->service_runtimeProfiler().enabled() &&
		(services_->service_adapterFrameCount() % 16U) == 0U && gpMetaUtilFuncs != nullptr &&
		gpMetaUtilFuncs->pfnLogConsole != nullptr && services_->service_pluginId() != nullptr)
	{
		const ManagedCurrentWeaponObservation &weapon =
			services_->observation_managedBotCurrentWeapons(index);
		profiledLogConsole(
			services_->service_runtimeProfiler(), services_->service_pluginId(),
			"profile plantCommand actor=%u actor_generation=%u map=%u round=%u "
			"frame=%u action=%s command=%s dispatched=%d current_weapon_known=%d "
			"current_weapon_active=%d current_weapon_id=%u active_c4=%d",
			static_cast<unsigned int>(handle.actor.slot),
			static_cast<unsigned int>(handle.actor.actorGeneration),
			static_cast<unsigned int>(services_->service_lifecycle().mapGeneration()),
			static_cast<unsigned int>(services_->service_lifecycle().roundGeneration()),
			static_cast<unsigned int>(services_->service_adapterFrameCount()),
			actionKindName(proposal.kind), dispatch.clientCommand, commandDispatched ? 1 : 0,
			weapon.available ? 1 : 0, weapon.active ? 1 : 0,
			static_cast<unsigned int>(weapon.weaponId),
			weapon.available && weapon.active && weapon.weaponId == ActionAdapter::kC4WeaponId ? 1
																							   : 0);
	}
	return commandDispatched;
}

void BotCommandRuntime::resetManagedBotCommandTemplate(std::size_t index)
{
	if (index >= NativeBotObservation::kClientSlotCount)
	{
		return;
	}
	actors_[index].managedBotCommandTemplates = {};
	actors_[index].managedBotCommandTemplateValid = false;
	actors_[index].managedBotActionStopsMovement = false;
}

MovementCommandDraft BotCommandRuntime::beginNeutralCommand(std::size_t index, FakeClientHandle &handle,
															const runtime::MovementPhysicsState &)
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
											 const runtime::MovementPhysicsState &, MovementCommandDraft draft,
											 const ActionProposal &requestedAction)
{
	runtime::BotCommand command = draft.command;
	const ActionProposal actionProposal = ActionAdapter::forLiveDispatch(
		requestedAction, services_->observation_hasObservedActiveWeapon(index), command.viewAngles);
	const ActionDispatch actionDispatch = ActionAdapter::translate(actionProposal);
	command.viewAngles = actionDispatch.viewAngles;
	command.movement.buttons = actionDispatch.buttons;
	if (actionDispatch.clientCommand != nullptr)
	{
		(void)dispatchManagedActionCommand(index, handle, actionProposal, actionDispatch);
	}
	actors_[index].managedBotCommandTemplates = command;
	actors_[index].managedBotCommandTemplateValid = true;
	actors_[index].managedBotActionStopsMovement = actionDispatch.stopMovement;
}

runtime::CommandReceipt BotCommandRuntime::executeManagedBotCommand(std::size_t index,
																	FakeClientHandle &handle)
{
	RuntimeProfilerScope profilerScope(services_->service_runtimeProfiler(),
									   RuntimeProfilerStage::MovementDispatch);
	runtime::CommandReceipt receipt = {handle.actor, 0U, services_->service_adapterFrameCount(),
									   runtime::DispatchResult::NoCommand};
	if (index >= NativeBotObservation::kClientSlotCount || handle.entity == nullptr ||
		actors_[index].managedBotCommandSequences == (std::numeric_limits<std::uint32_t>::max)())
	{
		return receipt;
	}
	if (!actors_[index].managedBotCommandTemplateValid)
	{
		const runtime::LifecycleToken neutralToken =
			services_->service_lifecycle().tokenForSlot(handle.actor.slot);
		if (!services_->service_lifecycle().isCurrent(neutralToken))
		{
			receipt.result = runtime::DispatchResult::StaleActor;
			return receipt;
		}
		runtime::BotCommand neutralCommand = {};
		neutralCommand.actor = handle.actor;
		neutralCommand.lifecycle = neutralToken;
		neutralCommand.viewAngles = {handle.entity->v.v_angle[0], handle.entity->v.v_angle[1],
									 handle.entity->v.v_angle[2]};
		neutralCommand.movement = {0.0f, 0.0f, 0.0f, 0U, 0U, 1U};
		actors_[index].managedBotCommandTemplates = neutralCommand;
		actors_[index].managedBotCommandTemplateValid = true;
	}
	if (!actors_[index].managedBotCommandTemplateValid)
	{
		return receipt;
	}
	const runtime::LifecycleToken token =
		services_->service_lifecycle().tokenForSlot(handle.actor.slot);
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
	command.movement.msec =
		services_->consumeCommandMsec(index, services_->service_globals()->time);
	const float maxSpeed = handle.entity->v.maxspeed;
	const bool explicitFrozen = (handle.entity->v.flags & FL_FROZEN) != 0;
	const bool roundFreeze = services_->roundFreezeActive();
	if (!explicitFrozen && !roundFreeze && !actors_[index].managedBotActionStopsMovement)
		applyPlayerAvoidance(index, &command);
	if (actors_[index].managedBotActionStopsMovement)
	{
		// Do not let a delayed jump-crouch sequence survive an objective hold.
		actors_[index].managedBotJumpCrouch.reset();
	}
	const std::uint16_t buttonsBeforeJumpCrouch = command.movement.buttons;
	const runtime::JumpCrouchObservation jumpCrouchObservation = {
		services_->service_globals()->time,
		true,
		(handle.entity->v.flags & FL_ONGROUND) != 0,
		handle.entity->v.deadflag != DEAD_NO || handle.entity->v.health <= 0.0f,
		handle.entity->v.movetype == MOVETYPE_FLY,
		explicitFrozen || roundFreeze,
		(command.movement.buttons & static_cast<std::uint16_t>(IN_JUMP)) != 0U};
	const runtime::JumpCrouchCommandDecision jumpCrouchDecision =
		actors_[index].managedBotJumpCrouch.updateCommand(
			jumpCrouchObservation, command.movement.buttons, static_cast<std::uint16_t>(IN_DUCK));
	command.movement.buttons = jumpCrouchDecision.buttons;
	if (!jumpCrouchObservation.grounded)
	{
		command.movement.buttons = static_cast<std::uint16_t>(command.movement.buttons &
															  ~static_cast<std::uint16_t>(IN_JUMP));
	}
	if (services_->service_runtimeProfiler().enabled() &&
		jumpCrouchDecision.sequencing.transition != runtime::JumpCrouchTransition::None &&
		gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr &&
		services_->service_pluginId() != nullptr)
	{
		profiledLogConsole(
			services_->service_runtimeProfiler(), services_->service_pluginId(),
			"profile jumpCrouch bot_id=%u command_sequence=%u transition=%s phase=%d "
			"elapsed=%.3f grounded=%d before_buttons=%u after_buttons=%u",
			static_cast<unsigned int>(handle.actor.slot),
			static_cast<unsigned int>(command.sequence),
			jumpCrouchTransitionName(jumpCrouchDecision.sequencing.transition),
			static_cast<int>(jumpCrouchDecision.sequencing.phase),
			jumpCrouchDecision.sequencing.elapsed, jumpCrouchObservation.grounded ? 1 : 0,
			static_cast<unsigned int>(buttonsBeforeJumpCrouch),
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
	gateObservation.freezetimeDuck =
		readOptionalCvarFloat(services_->service_engineFunctions(), "freezetime_duck");
	gateObservation.freezetimeJump =
		readOptionalCvarFloat(services_->service_engineFunctions(), "freezetime_jump");
	const MovementExecutionDecision gateDecision = MovementExecutionGate::evaluate(gateObservation);
	bool speedRestored = false;
	if (gateDecision.phase == MovementExecutionPhase::Live &&
		!actors_[index].managedBotActionStopsMovement &&
		(!std::isfinite(maxSpeed) || maxSpeed <= 1.0f))
	{
		handle.entity->v.maxspeed = kDefaultManagedBotMaxSpeed;
		speedRestored = true;
		if (services_->service_engineFunctions() != nullptr &&
			services_->service_engineFunctions()->pfnSetClientMaxspeed != nullptr)
		{
			services_->service_engineFunctions()->pfnSetClientMaxspeed(handle.entity,
																	   kDefaultManagedBotMaxSpeed);
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
		const float viewAngles[3] = {command.viewAngles.pitch, command.viewAngles.yaw,
									 command.viewAngles.roll};
		handle.entity->v.button = 0U;
		handle.entity->v.impulse = 0U;
		{
			RuntimeProfilerScope moveScope(services_->service_runtimeProfiler(),
										   RuntimeProfilerStage::RunPlayerMove);
			services_->service_engineFunctions()->pfnRunPlayerMove(handle.entity, viewAngles, 0.0f,
																   0.0f, 0.0f, 0U, 0U, 0U);
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
	const runtime::QueueResult enqueueResult =
		services_->service_inputDispatcher().enqueue(command);
	if (enqueueResult != runtime::QueueResult::Accepted)
	{
		if (gpMetaUtilFuncs != nullptr &&
			gpMetaUtilFuncs->pfnLogConsole != nullptr && services_->service_pluginId() != nullptr)
		{
			const runtime::LifecycleToken currentToken =
				services_->service_lifecycle().tokenForSlot(handle.actor.slot);
			importantLogConsole(services_->service_runtimeProfiler(), services_->service_pluginId(),
							   "profile movementReject bot_id=%u actor_generation=%u frame=%u "
							   "queue_result=%d command_valid=%d sequence=%u msec=%u "
							   "command_lifecycle=(%u,%u,%u) current_lifecycle=(%u,%u,%u) "
							   "view=(%.2f %.2f %.2f) move=(%.2f %.2f %.2f) buttons=%u",
							   static_cast<unsigned int>(handle.actor.slot),
							   static_cast<unsigned int>(handle.actor.actorGeneration),
							   static_cast<unsigned int>(services_->service_adapterFrameCount()),
							   static_cast<int>(enqueueResult), command.isValid() ? 1 : 0,
							   static_cast<unsigned int>(command.sequence),
							   static_cast<unsigned int>(command.movement.msec),
							   static_cast<unsigned int>(command.lifecycle.mapGeneration),
							   static_cast<unsigned int>(command.lifecycle.roundGeneration),
							   static_cast<unsigned int>(command.lifecycle.slotGeneration),
							   static_cast<unsigned int>(currentToken.mapGeneration),
							   static_cast<unsigned int>(currentToken.roundGeneration),
							   static_cast<unsigned int>(currentToken.slotGeneration),
							   command.viewAngles.pitch, command.viewAngles.yaw,
							   command.viewAngles.roll, command.movement.forward,
							   command.movement.side, command.movement.up,
							   static_cast<unsigned int>(command.movement.buttons));
		}
		receipt.sequence = command.sequence;
		receipt.result = enqueueResult == runtime::QueueResult::StaleActor
							 ? runtime::DispatchResult::StaleActor
							 : runtime::DispatchResult::InvalidCommand;
		// Never retry a rejected cached command unchanged; rebuild with the current lifecycle.
		resetManagedBotCommandTemplate(index);
		return receipt;
	}
	const Vector commandStart = handle.entity->v.origin;
	const runtime::CommandReceipt dispatchReceipt =
		services_->service_inputDispatcher().dispatchNext(handle.actor,
														  services_->service_adapterFrameCount());
	if (services_->service_runtimeProfiler().enabled() &&
		actors_[index].managedCommandDiagnosticUpdates !=
			services_->service_managedBotFullUpdateSequences()[index] &&
		handle.entity != nullptr && !handle.entity->free)
	{
		actors_[index].managedCommandDiagnosticUpdates =
			services_->service_managedBotFullUpdateSequences()[index];
		const auto &step = services_->navigation_managedNavigationSteps(index);
		profiledLogConsole(
			services_->service_runtimeProfiler(), services_->service_pluginId(),
			"profile motionCommand bot_id=%u actor_generation=%u map=%u round=%u frame=%u "
			"update=%u now=%.3f state=%s outcome=%u path=%u input=(%.1f %.1f %.1f) buttons=%u "
			"msec=%u dispatched=%d start=(%.1f %.1f %.1f) after=(%.1f %.1f %.1f)",
			handle.actor.slot, handle.actor.actorGeneration,
			services_->service_lifecycle().mapGeneration(),
			services_->service_lifecycle().roundGeneration(),
			services_->service_adapterFrameCount(),
			services_->service_managedBotFullUpdateSequences()[index],
			services_->service_globals()->time,
			compat::toString(services_->behavior_managedBotStateMachines(index).state()),
			static_cast<unsigned>(step.outcome), step.pathSequence, command.movement.forward,
			command.movement.side, command.movement.up,
			static_cast<unsigned>(command.movement.buttons),
			static_cast<unsigned>(command.movement.msec),
			dispatchReceipt.result == runtime::DispatchResult::Dispatched ? 1 : 0, commandStart.x,
			commandStart.y, commandStart.z, handle.entity->v.origin.x, handle.entity->v.origin.y,
			handle.entity->v.origin.z);
	}
	if (dispatchReceipt.result == runtime::DispatchResult::Dispatched &&
		(command.movement.buttons & IN_RELOAD) != 0U && services_->service_globals() != nullptr)
	{
		services_->observation_observeReloadDispatched(
			{handle.actor.slot, handle.actor.actorGeneration},
			{services_->service_lifecycle().mapGeneration(),
			 services_->service_lifecycle().roundGeneration(),
			 services_->service_adapterFrameCount()},
			services_->service_globals()->time);
	}
	if (dispatchReceipt.result == runtime::DispatchResult::Dispatched &&
		(command.movement.buttons & ActionAdapter::kAttackButton) != 0U)
	{
		services_->behavior_observeC4AttackDispatched(index, handle, command.movement.buttons);
	}
	if (services_->service_runtimeProfiler().enabled() && gpMetaUtilFuncs != nullptr &&
		gpMetaUtilFuncs->pfnLogConsole != nullptr && services_->service_pluginId() != nullptr &&
		services_->service_globals() != nullptr &&
		std::isfinite(services_->service_globals()->time) &&
		(!std::isfinite(actors_[index].movementGateDiagnosticTimes) ||
		 services_->service_globals()->time < actors_[index].movementGateDiagnosticTimes ||
		 services_->service_globals()->time - actors_[index].movementGateDiagnosticTimes >= 1.0f))
	{
		actors_[index].movementGateDiagnosticTimes = services_->service_globals()->time;
		profiledLogConsole(
			services_->service_runtimeProfiler(), services_->service_pluginId(),
			"profile movementGate actor=%u generation=%u map=%u round=%u frame=%u "
			"freeze_active=%d freeze_until=%.3f explicit_frozen=%d phase=%d "
			"maxspeed_before=%.1f maxspeed_after=%.1f speed_restored=%d action_stop=%d "
			"input=(%.1f %.1f %.1f) buttons=%u dispatch=%d",
			static_cast<unsigned int>(handle.actor.slot),
			static_cast<unsigned int>(handle.actor.actorGeneration),
			static_cast<unsigned int>(services_->service_lifecycle().mapGeneration()),
			static_cast<unsigned int>(services_->service_lifecycle().roundGeneration()),
			static_cast<unsigned int>(services_->service_adapterFrameCount()), roundFreeze ? 1 : 0,
			services_->service_roundFreezeScheduled() ? services_->service_roundFreezeUntil()
													  : 0.0f,
			explicitFrozen ? 1 : 0, static_cast<int>(gateDecision.phase), maxSpeed,
			handle.entity->v.maxspeed, speedRestored ? 1 : 0,
			actors_[index].managedBotActionStopsMovement ? 1 : 0, gateDecision.forward,
			gateDecision.side, gateDecision.up, static_cast<unsigned int>(gateDecision.buttons),
			static_cast<int>(dispatchReceipt.result));
	}
	const bool freezeActiveAfterDispatch = services_->roundFreezeActive();
	const bool freezeMovementInput = std::fabs(gateDecision.forward) > 0.01f ||
									 std::fabs(gateDecision.side) > 0.01f ||
									 std::fabs(gateDecision.up) > 0.01f;
	if (services_->service_runtimeProfiler().enabled() &&
		(roundFreeze || freezeActiveAfterDispatch) &&
		((services_->service_adapterFrameCount() % 6U) == 0U ||
		 roundFreeze != freezeActiveAfterDispatch || freezeMovementInput) &&
		gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr &&
		services_->service_pluginId() != nullptr)
	{
		profiledLogConsole(
			services_->service_runtimeProfiler(), services_->service_pluginId(),
			"profile freezeMotion actor=%u generation=%u map=%u round=%u frame=%u "
			"freeze_gate=%d freeze_after=%d origin=(%.1f %.1f %.1f) "
			"velocity=(%.1f %.1f %.1f) input=(%.1f %.1f %.1f) buttons=%u",
			static_cast<unsigned int>(handle.actor.slot),
			static_cast<unsigned int>(handle.actor.actorGeneration),
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

MovementCommandDraft BotCommandRuntime::beginDirectNeutralCommand(std::size_t index, FakeClientHandle &handle,
																  std::uint8_t milliseconds)
{
	MovementCommandDraft draft{};
	runtime::CommandReceipt receipt = {handle.actor, 0U, services_->service_adapterFrameCount(),
									   runtime::DispatchResult::NoCommand};
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
																	  std::uint8_t, MovementCommandDraft draft,
																	  const ActionProposal &requestedAction)
{
	runtime::BotCommand command = draft.command;
	runtime::CommandReceipt receipt = draft.receipt;
	const ActionProposal actionProposal = ActionAdapter::forLiveDispatch(
		requestedAction, services_->observation_hasObservedActiveWeapon(index), command.viewAngles);
	const ActionDispatch actionDispatch = ActionAdapter::translate(actionProposal);
	command.viewAngles = actionDispatch.viewAngles;
	command.movement.buttons = actionDispatch.buttons;
	if (actionProposal.kind != ActionKind::None && gpMetaUtilFuncs != nullptr &&
		gpMetaUtilFuncs->pfnLogConsole != nullptr && services_->service_pluginId() != nullptr &&
		(services_->service_adapterFrameCount() % 16U) == 0U)
	{
		profiledLogConsole(services_->service_runtimeProfiler(), services_->service_pluginId(),
						   "bot action=%s slot=%u generation=%u frame=%u buttons=%u",
						   actionKindName(actionProposal.kind), static_cast<unsigned int>(handle.actor.slot),
						   static_cast<unsigned int>(handle.actor.actorGeneration),
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
	if (dispatchReceipt.result == runtime::DispatchResult::Dispatched &&
		(actionDispatch.buttons & ActionAdapter::kAttackButton) != 0U)
	{
		services_->behavior_observeNeutralC4AttackDispatched(index, handle);
	}
	return dispatchReceipt;
}

runtime::CommandReceipt BotCommandRuntime::dispatchJoinHeartbeat(std::size_t index,
																 FakeClientHandle &handle,
																 std::uint8_t milliseconds)
{
	runtime::CommandReceipt receipt = {handle.actor, 0U, services_->service_adapterFrameCount(),
									   runtime::DispatchResult::NoCommand};
	if (index >= NativeBotObservation::kClientSlotCount || handle.entity == nullptr ||
		actors_[index].managedBotCommandSequences == (std::numeric_limits<std::uint32_t>::max)())
	{
		return receipt;
	}
	const runtime::LifecycleToken token =
		services_->service_lifecycle().tokenForSlot(handle.actor.slot);
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
	const float viewAngles[3] = {handle.entity->v.v_angle[0], handle.entity->v.v_angle[1],
								 handle.entity->v.v_angle[2]};
	receipt.sequence = ++actors_[index].managedBotCommandSequences;
	const unsigned short buttons =
		services_->service_joinControllers()[index].phase() == JoinPhase::WaitingTeamMenu
			? static_cast<unsigned short>(IN_ATTACK)
			: 0U;
	{
		RuntimeProfilerScope moveScope(services_->service_runtimeProfiler(),
									   RuntimeProfilerStage::RunPlayerMove);
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

void BotCommandRuntime::onMovementResetShared()
{
	reset_managedMoveSpeeds();
	reset_managedCommandDiagnosticUpdates();
	fill_managedBotCommandSequences(0U);
	fill_movementGateDiagnosticTimes(std::numeric_limits<float>::quiet_NaN());
}

void BotCommandRuntime::onMovementResetActor(std::size_t index)
{
	actors_[index].managedBotJumpCrouch.reset();
	resetManagedBotCommandTemplate(index);
}

MovementCommandDraft BotCommandRuntime::beginMovementCommand(std::size_t, FakeClientHandle &handle,
															 const runtime::LifecycleToken &token,
															 const runtime::MovementPhysicsState &,
															 NavigationUpdate &movement)
{
	MovementCommandDraft draft{};
	const auto &locomotionIntent = movement.locomotionIntent;
	runtime::BotCommand &command = draft.command;
	command.actor = handle.actor;
	command.lifecycle = token;
	command.sequence = 0U;
	command.issueFrame = 0U;
	const runtime::ViewAngles movementViewAngles = {0.0f, movementYaw(locomotionIntent.direction), 0.0f};
	command.viewAngles = movementViewAngles;
	std::uint16_t movementButtons = locomotionIntent.posture == nav::LocomotionPosture::Crouching
										? static_cast<std::uint16_t>(IN_DUCK)
										: static_cast<std::uint16_t>(0U);
	if (locomotionIntent.traversal == nav::TraversalAction::Jump)
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
											  const runtime::LifecycleToken &,
											  const runtime::MovementPhysicsState &before, NavigationUpdate &movement,
											  MovementCommandDraft draft, const ActionProposal &requestedAction)
{
	runtime::BotCommand command = draft.command;
	auto &roamDecision = movement.roamDecision;
	const auto &locomotionIntent = movement.locomotionIntent;
	const auto &observation = movement.observation;
	const auto &landingFeedback = movement.landingFeedback;
	const runtime::ViewAngles movementViewAngles = {0.0f, movementYaw(locomotionIntent.direction), 0.0f};
	const ActionProposal actionProposal = ActionAdapter::forLiveDispatch(
		requestedAction, services_->observation_hasObservedActiveWeapon(index), movementViewAngles);
	const ActionDispatch actionDispatch = ActionAdapter::translate(actionProposal);
	command.viewAngles = actionDispatch.viewAngles;
	command.movement.buttons = actionDispatch.buttons;
	const bool compatibilityMovement =
		services_->service_compatibilitySurface().configuration().mode == compat::RuntimeMode::Compatibility;
	float commandSpeed = locomotionIntent.speed;
	if (compatibilityMovement && commandSpeed > 0.0f && std::isfinite(handle.entity->v.maxspeed) &&
		handle.entity->v.maxspeed > 0.0f)
	{
		commandSpeed = handle.entity->v.maxspeed;
		const auto &goal = services_->behavior_managedBotObjectiveTargets(index);
		const float x = goal.target.x - before.origin.x, y = goal.target.y - before.origin.y;
		const bool finalArea = roamDecision.currentArea != 0U && roamDecision.currentArea == roamDecision.goalArea;
		if (goal.valid && finalArea && x * x + y * y < 200.0f * 200.0f &&
			locomotionIntent.posture != nav::LocomotionPosture::Crouching)
			commandSpeed *= 0.4f;
	}
	actors_[index].managedMoveSpeeds = commandSpeed;
	const MovementProjection movementProjection =
		ActionAdapter::projectMovement(locomotionIntent.direction.x, locomotionIntent.direction.y, commandSpeed,
									   command.viewAngles.yaw, compatibilityMovement);
	command.movement.forward = actionDispatch.stopMovement ? 0.0f : movementProjection.forward;
	command.movement.side = actionDispatch.stopMovement ? 0.0f : movementProjection.side;
	if (!actionDispatch.stopMovement)
	{
		command.movement.buttons = static_cast<std::uint16_t>(
			command.movement.buttons | ActionAdapter::movementButtons(command.movement.forward, command.movement.side));
	}
	services_->navigation_logTraversalOutcome(index, before, roamDecision, landingFeedback);
	services_->navigation_logTraversalDiagnostic(index, before, roamDecision, locomotionIntent,
												 command.movement.buttons, observation.locomotion);
	services_->navigation_logMovementReversalDiagnostic(index, before, roamDecision, locomotionIntent);
	if (services_->navigation_movementDiagnosticSamples(index) < kMovementPhysicsLogLimit &&
		gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr &&
		services_->service_pluginId() != nullptr)
	{
		profiledLogConsole(
			services_->service_runtimeProfiler(), services_->service_pluginId(),
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
						   "bot action=%s slot=%u generation=%u frame=%u buttons=%u",
						   actionKindName(actionProposal.kind), static_cast<unsigned int>(handle.actor.slot),
						   static_cast<unsigned int>(handle.actor.actorGeneration),
						   static_cast<unsigned int>(services_->service_adapterFrameCount()),
						   static_cast<unsigned int>(actionDispatch.buttons));
	}
	if (actionDispatch.clientCommand != nullptr)
	{
		(void)dispatchManagedActionCommand(index, handle, actionProposal, actionDispatch);
	}
	handle.entity->v.v_angle[0] = command.viewAngles.pitch;
	handle.entity->v.v_angle[1] = command.viewAngles.yaw;
	handle.entity->v.v_angle[2] = command.viewAngles.roll;
	handle.entity->v.angles[0] = -command.viewAngles.pitch / 3.0f;
	handle.entity->v.angles[1] = command.viewAngles.yaw;
	handle.entity->v.angles[2] = command.viewAngles.roll;
	actors_[index].managedBotCommandTemplates = command;
	actors_[index].managedBotCommandTemplateValid = true;
	actors_[index].managedBotActionStopsMovement = actionDispatch.stopMovement;
	const runtime::CommandReceipt receipt = executeManagedBotCommand(index, handle);
	if (receipt.result != runtime::DispatchResult::Dispatched)
	{
		roamDecision.failureReason = runtime::NavFailureReason::NavApplyRejected;
		services_->navigation_logMovementDiagnostic(index, "nav_apply_rejected", &roamDecision);
	}
	services_->navigation_recordMovementPhysicsSample(index, before, receipt);
	if (receipt.result == runtime::DispatchResult::Dispatched &&
		services_->navigation_movementDiagnosticSamples(index) < 4U && gpMetaUtilFuncs != nullptr &&
		gpMetaUtilFuncs->pfnLogConsole != nullptr && services_->service_pluginId() != nullptr)
	{
		profiledLogConsole(
			services_->service_runtimeProfiler(), services_->service_pluginId(),
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

bool BotCommandRuntime::dispatchClientCommand(edict_t *entity, const char *name,
											  const char *argument)
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
			const MovementVelocity velocity = MovementExecutionGate::stabilizeVelocity(
				MovementExecutionPhase::RoundFreeze,
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

runtime::CommandReceipt BotCommandRuntime::dispatchInput(const runtime::ActorId &actor,
														 std::uint32_t dispatchFrame)
{
	return services_->service_inputDispatcher().dispatchNext(actor, dispatchFrame);
}

} // namespace metamod
} // namespace astrabot
