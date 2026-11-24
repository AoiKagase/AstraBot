#include "runtime/behavior/bot_behavior_runtime.hpp"
#include "runtime/behavior/bot_look_geometry.hpp"
#include "runtime/shared/bot_runtime_support.hpp"
#include <cmath>
namespace astrabot
{
namespace metamod
{
namespace
{
bool advanceNoiseLook(compat::NoiseLookPlanner &look, const edict_t &self, float now, runtime::BotCommand &command,
	bool localMovement, float *goalYaw, const compat::SharedLookStep &sharedLookStep)
{
	float pitch = 0, yaw = 0;
	const auto eye = self.v.origin + self.v.view_ofs;
	if (!look.update(now, {self.v.origin.x, self.v.origin.y, self.v.origin.z}, {eye.x, eye.y, eye.z},
			self.v.v_angle.x, self.v.v_angle.y, &pitch, &yaw, goalYaw, sharedLookStep))
		return false;
	if (!localMovement)
	{
		const auto move =
			ActionAdapter::reprojectMovement(command.movement.forward, command.movement.side, command.viewAngles.yaw, yaw);
		command.movement.forward = move.forward;
		command.movement.side = move.side;
		command.movement.buttons =
			static_cast<std::uint16_t>((command.movement.buttons & ~(IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT)) |
				ActionAdapter::movementButtons(move.forward, move.side));
	}
	command.viewAngles = {pitch, yaw, 0};
	command.synchronizeCombatView = true;
	return true;
}
}

void BotBehaviorRuntime::applyNoiseLookCommand(std::size_t index, runtime::BotCommand &command, bool upkeepOnly)
{
	(void)processNoiseLook(index, command, upkeepOnly, false);
}

BotBehaviorRuntime::NoiseLookResult BotBehaviorRuntime::prepareNoiseLook(std::size_t index)
{
	runtime::BotCommand unused{};
	return processNoiseLook(index, unused, false, true);
}

BotBehaviorRuntime::NoiseLookResult BotBehaviorRuntime::processNoiseLook(
	std::size_t index, runtime::BotCommand &command, bool upkeepOnly, bool decisionOnly)
{
	if (!services_ || index >= actors_.size())
		return NoiseLookResult::NotHandled;
	auto &actor = actors_[index];
	auto &look = actor.noiseLook;
	const auto &belief = services_->observation_managedBotPerception(index).snapshot();
	const auto &noise = belief.noise();
	const bool enemySound = compat::hasNoiseLookPosition(noise);
	if (!decisionOnly && !look.active() && !enemySound)
		return NoiseLookResult::NotHandled;
	const auto globals = services_->service_globals();
	const auto &handle = services_->service_managedBotHandles()[index];
	auto *self = handle.entity;
	const auto &lifecycle = services_->service_lifecycle();
	const auto config = services_->service_compatibilitySurface().configuration();
	if (!globals || !std::isfinite(globals->time) || !self || self->free || self->v.health <= 0 ||
		self->v.deadflag != DEAD_NO || (self->v.flags & (FL_FROZEN | FL_SPECTATOR)) ||
		self->v.movetype == MOVETYPE_FLY || !lifecycle.isCurrent(lifecycle.tokenForSlot(handle.actor.slot)) ||
		config.mode != compat::RuntimeMode::Compatibility || config.botEnable <= 0 || (!upkeepOnly && config.botStop > 0) ||
		(services_->service_roundFreezeGeneration() == lifecycle.roundGeneration() &&
			globals->time < services_->service_roundFreezeUntil()))
	{
		look.reset();
		actor.noiseLookUpdate = 0;
		actor.noiseLookDecision = {};
		return NoiseLookResult::NotHandled;
	}
	const world::FrameIdentity frame{
		lifecycle.mapGeneration(), lifecycle.roundGeneration(), services_->service_adapterFrameCount()};
	look.synchronize({handle.actor.slot, handle.actor.actorGeneration}, frame, globals->time);
	syncNativeGrenadeLook(index);
	const bool grenadeSpot = actor.nativeGrenade.looking();
	if (grenadeSpot)
		look.clearLook();
	if (upkeepOnly)
	{
		if (grenadeSpot)
			return NoiseLookResult::NotHandled;
		float goalYaw = 0.0f;
		if (advanceNoiseLook(look, *self, globals->time, command, true, &goalYaw, [this, index](float time, float yaw, float pitch, float goalYaw, float goalPitch, bool attack, unsigned priority) { return services_->command_stepLook(index, time, yaw, pitch, goalYaw, goalPitch, attack, priority); }))
			recordLookGoal(index, goalYaw);
		return NoiseLookResult::NotHandled;
	}
	bool visibleEnemy = false;
	for (std::size_t i = 0; i < belief.actorCount(); ++i)
	{
		const auto *contact = belief.actorAt(i);
		if (contact && contact->visible && contact->relation == world::TeamRelation::Hostile)
		{
			visibleEnemy = true;
			break;
		}
	}
	compat::PublicActorState weapon{};
	const bool known = services_->observation_publicObservations().read(
		{handle.actor.slot, handle.actor.actorGeneration}, frame, &weapon);
	const bool panicLook = look.damagePanic();
	if ((panicLook ? actor.enemyReaction.shouldAim(reactionContext(index))
				   : visibleEnemy || actor.managedBotStateMachines.attackOverlayActive()) ||
		actor.weaponActions.grenadeInProgress() || (command.movement.buttons & (IN_USE | IN_ATTACK2)) ||
		((command.movement.buttons & IN_ATTACK) && !actor.grenadeCommand.active) ||
		(known && weapon.weaponKnown && weapon.weaponId == 6) ||
		isBlinded({handle.actor.slot, handle.actor.actorGeneration}, frame, globals->time) ||
		actor.obstacleLook.entity)
	{
		if (!panicLook)
			look.clearLook();
		return NoiseLookResult::NotHandled;
	}
	const auto fullUpdate = services_->service_managedBotFullUpdateSequences()[index];
	const auto receipt = reactionContext(index);
	const auto revision = actor.executionRevision;
	const auto normalEpoch = actor.managedBotStateMachines.normalStateEpoch();
	const auto attackEpoch = actor.managedBotStateMachines.attackEntryEpoch();
	const auto clearSequence = actor.managedBotStateMachines.lookClearSequence();
	const auto current = [&] {
		return executionCurrent(index, receipt, revision) &&
			actor.managedBotStateMachines.normalStateEpoch() == normalEpoch &&
			actor.managedBotStateMachines.attackEntryEpoch() == attackEpoch &&
			actor.managedBotStateMachines.lookClearSequence() == clearSequence &&
			services_->service_managedBotFullUpdateSequences()[index] == fullUpdate;
	};
	auto &decision = actor.noiseLookDecision;
	const bool cached = decision.valid && decision.full == fullUpdate &&
		executionCurrent(index, decision.receipt, decision.revision) &&
		decision.normalEpoch == normalEpoch && decision.attackEpoch == attackEpoch &&
		decision.clearSequence == clearSequence;
	// A callback cannot turn an in-flight or invalidated decision into a
	// fresh "not handled" result. A completed decision may outlive its tick:
	// upkeep and independent spot expiry must not retry it or be aborted just
	// because time advanced without another full decision.
	const bool decisionIdentityChanged = !(decision.receipt.actor == receipt.actor) ||
		decision.receipt.entityGeneration != receipt.entityGeneration ||
		decision.receipt.alive != receipt.alive || receipt.now < decision.receipt.now ||
		receipt.frame.tick < decision.receipt.frame.tick ||
		decision.receipt.frame.mapGeneration != receipt.frame.mapGeneration ||
		decision.receipt.frame.roundGeneration != receipt.frame.roundGeneration ||
		decision.revision != revision || decision.normalEpoch != normalEpoch ||
		decision.attackEpoch != attackEpoch || decision.clearSequence != clearSequence;
	if (decision.valid && decision.full == fullUpdate && (decision.preparing || decisionIdentityChanged))
		return NoiseLookResult::Aborted;
	bool handled = cached && decision.handled;
	if (actor.noiseLookUpdate != fullUpdate)
	{
		actor.noiseLookUpdate = fullUpdate;
		decision = {receipt, revision, normalEpoch, attackEpoch, clearSequence, fullUpdate, true, true, false};
		compat::NoiseLookInput input{};
		input.actor = {handle.actor.slot, handle.actor.actorGeneration};
		input.frame = frame;
		input.now = globals->time;
		input.hasNoise = enemySound && belief.isValid() &&
			belief.identity().frame.mapGeneration == frame.mapGeneration &&
			belief.identity().frame.roundGeneration == frame.roundGeneration;
		input.noiseTime = noise.timestampSeconds;
		input.reactionSeconds = actor.managedProfiles.effectiveReactionSeconds(true);
		input.highPriority = noise.priority == world::NoisePriority::High;
		input.hiding = actor.managedBotStateMachines.state() == compat::CompatibilityStateId::Hide &&
			std::hypot(self->v.velocity.x, self->v.velocity.y) < 10.0f;
		const float dx = noise.position.x - self->v.origin.x, dy = noise.position.y - self->v.origin.y,
					dz = noise.position.z - self->v.origin.z;
		input.distanceSquared = dx * dx + dy * dy + dz * dz;
		const auto engine = services_->service_engineFunctions();
		const float age = input.now - input.noiseTime;
		const bool fresh =
			input.hasNoise && std::isfinite(age) && age >= 0 && age <= 0.5f && !look.inhibited(input.now);
		if (fresh && (compat::isNearbyEnemyGunfire(input) || age >= input.reactionSeconds) && engine &&
			engine->pfnTraceLine)
		{
			const world::WorldPosition target{noise.position.x, noise.position.y, noise.position.z + 36.0f};
			const float from[] = {self->v.origin.x + self->v.view_ofs.x, self->v.origin.y + self->v.view_ofs.y,
				self->v.origin.z + self->v.view_ofs.z};
			const float to[] = {target.x, target.y, target.z};
			TraceResult trace{};
			engine->pfnTraceLine(from, to, 0x101, self, &trace);
			if (!current())
				return NoiseLookResult::Aborted;
			input.clearLine = trace.flFraction >= 1.0f && !trace.fStartSolid && !trace.fAllSolid;
			const auto kind = compat::noiseLookKind(input, look.inhibitedUntil());
			world::WorldPosition point = target;
			const bool atHidingSpot = actor.managedBotStateMachines.state() == compat::CompatibilityStateId::Hide &&
				actor.scenarioPlanners.atDefenseSpot();
			bool havePoint = actor.noiseEnemyContact.allowsDirectLook(input.clearLine, atHidingSpot, input.now,
				{self->v.origin.x, self->v.origin.y, self->v.origin.z});
			const auto nav = services_->service_navPublisher().snapshot();
			if (!havePoint && kind != compat::NoiseLookKind::None)
			{
				auto &cache = actor.noiseBend;
				const auto &origin = self->v.origin;
				if (cache.valid && cache.noiseTime == noise.timestampSeconds && cache.navRevision == nav.revision() &&
					cache.noiseOrigin.x == noise.position.x && cache.noiseOrigin.y == noise.position.y &&
					cache.noiseOrigin.z == noise.position.z && std::fabs(cache.origin.x - origin.x) <= 10.0f &&
					std::fabs(cache.origin.y - origin.y) <= 10.0f && std::fabs(cache.origin.z - origin.z) <= 10.0f)
				{
					point = cache.point;
					havePoint = cache.hasPoint;
				}
				else
				{
					if (!havePoint && kind != compat::NoiseLookKind::None && nav.isValid())
					{
						nav::NavAreaMatch match{};
						if (nav::NavQuery(nav).findContaining({self->v.origin.x, self->v.origin.y, self->v.origin.z},
								120.0f, &match) == nav::NavQueryResult::Found)
						{
							const auto *area = nav.document()->findArea(match.area);
							float nearest = (std::numeric_limits<float>::max)();
							nav::NavVector approachPoint{};
							bool found = false;
							if (area)
								for (std::size_t n = 0; n < area->approaches.size() && n < 16U; ++n)
								{
									const auto &approach = area->approaches[n];
									const auto *here = nav.document()->findArea(approach.here);
									const auto *previous = nav.document()->findArea(approach.previous);
									if (!here || !previous)
										continue;
									const auto candidate =
										compat::guardApproachPoint(*previous, *here, approach.previousToHereHow);
									const float x = candidate.x - noise.position.x, y = candidate.y - noise.position.y,
												z = candidate.z - noise.position.z;
									const float distance = x * x + y * y + z * z;
									if (distance < nearest)
									{
										nearest = distance;
										approachPoint = candidate;
										found = true;
									}
								}
							nav::NavVector bend{};
							if (found &&
								bendLookApproach(*engine, self, {self->v.origin.x, self->v.origin.y, self->v.origin.z},
									approachPoint, &bend))
							{
								point = {bend.x, bend.y, bend.z + 36.0f};
								havePoint = true;
							}
						}
					}
					if (!current())
						return NoiseLookResult::Aborted;
					cache = {true, havePoint, noise.timestampSeconds, nav.revision(), {origin.x, origin.y, origin.z},
						noise.position, point};
				}
			}
			if (havePoint && kind != compat::NoiseLookKind::None)
			{
				for (int slot = 1; slot <= (std::min)(32, globals->maxClients); ++slot)
				{
					const auto *player = engine->pfnPEntityOfEntIndex ? engine->pfnPEntityOfEntIndex(slot) : nullptr;
					if (!current())
						return NoiseLookResult::Aborted;
					if (player && !player->free && (player->v.flags & FL_CLIENT) && player->v.deadflag == DEAD_NO &&
						player->v.health > 0)
					{
						const int team = services_->observation_resolvePlayerTeam(player);
						if ((team == 1 || team == 2) && team != services_->observation_resolvePlayerTeam(self))
							++input.enemiesRemaining;
					}
				}
				(void)look.request(input, kind, point, services_->service_compatibilityRandomSource(),
					{services_->command_managedBotCommandSequences(index), 0U, fullUpdate}, !grenadeSpot, current, &handled);
				if (!current())
					return NoiseLookResult::Aborted;
			}
		}
		if (!current())
			return NoiseLookResult::Aborted;
		decision.preparing = false;
		decision.handled = handled;
	}
	if (decisionOnly)
		return handled ? NoiseLookResult::Handled : NoiseLookResult::NotHandled;
	float goalYaw = 0.0f;
	if (advanceNoiseLook(look, *self, globals->time, command, false, &goalYaw, [this, index](float time, float yaw, float pitch, float goalYaw, float goalPitch, bool attack, unsigned priority) { return services_->command_stepLook(index, time, yaw, pitch, goalYaw, goalPitch, attack, priority); }))
		recordLookGoal(index, goalYaw);
	return handled ? NoiseLookResult::Handled : NoiseLookResult::NotHandled;
}
} // namespace metamod
} // namespace astrabot
