#include "runtime/behavior/bot_behavior_runtime.hpp"
#include "runtime/behavior/bot_look_geometry.hpp"
#include "runtime/shared/bot_runtime_support.hpp"
#include <cmath>
namespace astrabot
{
namespace metamod
{
void BotBehaviorRuntime::applyNoiseLookCommand(std::size_t index, runtime::BotCommand &command)
{
	if (!services_ || index >= actors_.size())
		return;
	auto &actor = actors_[index];
	auto &look = actor.noiseLook;
	const auto &belief = services_->observation_managedBotPerception(index).snapshot();
	const auto &noise = belief.noise();
	const bool enemySound = compat::hasNoiseLookPosition(noise);
	if (!look.active() && !enemySound)
		return;
	const auto globals = services_->service_globals();
	const auto &handle = services_->service_managedBotHandles()[index];
	auto *self = handle.entity;
	const auto &lifecycle = services_->service_lifecycle();
	const auto config = services_->service_compatibilitySurface().configuration();
	if (!globals || !std::isfinite(globals->time) || !self || self->free || self->v.health <= 0 ||
		self->v.deadflag != DEAD_NO || (self->v.flags & (FL_FROZEN | FL_SPECTATOR)) ||
		self->v.movetype == MOVETYPE_FLY || !lifecycle.isCurrent(lifecycle.tokenForSlot(handle.actor.slot)) ||
		config.mode != compat::RuntimeMode::Compatibility || config.botEnable <= 0 || config.botStop > 0 ||
		(services_->service_roundFreezeGeneration() == lifecycle.roundGeneration() &&
			globals->time < services_->service_roundFreezeUntil()))
	{
		look.reset();
		actor.noiseLookUpdate = 0;
		return;
	}
	const world::FrameIdentity frame{
		lifecycle.mapGeneration(), lifecycle.roundGeneration(), services_->service_adapterFrameCount()};
	look.synchronize({handle.actor.slot, handle.actor.actorGeneration}, frame, globals->time);
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
		actor.weaponActions.grenadeInProgress() || (command.movement.buttons & (IN_USE | IN_ATTACK | IN_ATTACK2)) ||
		(known && weapon.weaponKnown && weapon.weaponId == 6) ||
		isBlinded({handle.actor.slot, handle.actor.actorGeneration}, frame, globals->time) ||
		actor.obstacleLook.entity)
	{
		if (!panicLook)
			look.clearLook();
		return;
	}
	const auto fullUpdate = services_->service_managedBotFullUpdateSequences()[index];
	if (actor.noiseLookUpdate != fullUpdate)
	{
		actor.noiseLookUpdate = fullUpdate;
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
			input.clearLine = trace.flFraction >= 1.0f && !trace.fStartSolid && !trace.fAllSolid;
			const auto kind = compat::noiseLookKind(input, look.inhibitedUntil());
			world::WorldPosition point = target;
			const auto &last = actor.lastVisibleEnemyPositions;
			const float ex = last.x - self->v.origin.x, ey = last.y - self->v.origin.y, ez = last.z - self->v.origin.z;
			const bool nearbyThreat = !input.hiding && actor.lastVisibleEnemyTimes > 0 &&
				input.now - actor.lastVisibleEnemyTimes < 5.0f && ex * ex + ey * ey + ez * ez < 750.0f * 750.0f;
			bool havePoint = input.clearLine || nearbyThreat;
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
					cache = {true, havePoint, noise.timestampSeconds, nav.revision(), {origin.x, origin.y, origin.z},
						noise.position, point};
				}
			}
			if (havePoint && kind != compat::NoiseLookKind::None)
			{
				for (int slot = 1; slot <= (std::min)(32, globals->maxClients); ++slot)
				{
					const auto *player = engine->pfnPEntityOfEntIndex ? engine->pfnPEntityOfEntIndex(slot) : nullptr;
					if (player && !player->free && (player->v.flags & FL_CLIENT) && player->v.deadflag == DEAD_NO &&
						player->v.health > 0)
					{
						const int team = services_->observation_resolvePlayerTeam(player);
						if ((team == 1 || team == 2) && team != services_->observation_resolvePlayerTeam(self))
							++input.enemiesRemaining;
					}
				}
				(void)look.request(input, kind, point, services_->service_compatibilityRandomSource(),
					{services_->command_managedBotCommandSequences(index), 0U, fullUpdate});
			}
		}
	}
	float pitch = 0, yaw = 0;
	if (!look.update(globals->time, {self->v.origin.x, self->v.origin.y, self->v.origin.z},
			{self->v.origin.x + self->v.view_ofs.x, self->v.origin.y + self->v.view_ofs.y,
				self->v.origin.z + self->v.view_ofs.z},
			self->v.v_angle.x, self->v.v_angle.y, &pitch, &yaw))
		return;
	const auto move =
		ActionAdapter::reprojectMovement(command.movement.forward, command.movement.side, command.viewAngles.yaw, yaw);
	command.movement.forward = move.forward;
	command.movement.side = move.side;
	command.movement.buttons =
		static_cast<std::uint16_t>((command.movement.buttons & ~(IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT)) |
			ActionAdapter::movementButtons(move.forward, move.side));
	command.viewAngles = {pitch, yaw, 0};
	command.synchronizeCombatView = true;
}
} // namespace metamod
} // namespace astrabot
