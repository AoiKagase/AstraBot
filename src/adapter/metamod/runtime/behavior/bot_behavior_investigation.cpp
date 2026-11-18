#include "runtime/behavior/bot_behavior_runtime.hpp"
#include "astrabot/compat/noise_navigation.hpp"
#include <cmath>
namespace astrabot
{
namespace metamod
{
namespace
{
float distanceTo(const world::WorldPosition &a, const world::WorldPosition &b)
{
	const float x = a.x - b.x, y = a.y - b.y, z = a.z - b.z;
	return std::sqrt(x * x + y * y + z * z);
}
}
void BotBehaviorRuntime::updateNoiseInvestigation(std::size_t index, const runtime::MovementPhysicsState &before,
	compat::StateUpdateContext &context, compat::StateBehaviorInput &state)
{
	auto &actor = actors_[index];
	auto &planner = actor.noiseInvestigation;
	auto &machine = actor.managedBotStateMachines;
	const auto config = services_->service_compatibilitySurface().configuration();
	const auto &belief = services_->observation_managedBotPerception(index).snapshot();
	const auto nav = services_->service_navPublisher().snapshot();
	auto *self = services_->service_managedBotHandles()[index].entity;
	if (config.mode != compat::RuntimeMode::Compatibility || !self)
	{
		planner.reset();
		actor.investigationWalking = false;
		return;
	}
	compat::NoiseInvestigationInput in{};
	in.actor = context.actor;
	in.frame = context.frame;
	in.now = context.timestamp;
	in.navRevision = nav.isValid() ? nav.revision() : 0;
	in.origin = {before.origin.x, before.origin.y, before.origin.z};
	in.reactionSeconds = actor.managedProfiles.reactionTimeSeconds;
	in.aggression = static_cast<float>(actor.managedProfiles.aggression) / 100.0f;
	const float roundStart = services_->service_roundFreezeGeneration() == context.frame.roundGeneration
		? services_->service_roundFreezeUntil()
		: 0;
	in.elapsedRound = (std::max)(0.0f, in.now - roundStart);
	if (actor.lastVisibleEnemyTimes > 0 && actor.lastVisibleEnemyTimes >= roundStart)
		in.seenEnemyElapsed = actor.lastVisibleEnemyTimes - roundStart;
	in.alive = !before.dead;
	in.timing = {services_->command_managedBotCommandSequences(index), 0, context.fullUpdateSequence};
	if (belief.isValid() && belief.identity().observer == in.actor &&
		belief.identity().frame.mapGeneration == in.frame.mapGeneration &&
		belief.identity().frame.roundGeneration == in.frame.roundGeneration)
		in.noise = belief.noise();
	if (in.noise.active)
		in.noise.ready = in.noise.timestampSeconds >= 0 && in.now - in.noise.timestampSeconds >= in.reactionSeconds;
	for (std::size_t i = 0; i < belief.actorCount(); ++i)
	{
		const auto *contact = belief.actorAt(i);
		if (contact->visible && contact->relation == world::TeamRelation::Hostile)
		{
			in.visibleEnemy = true;
			break;
		}
	}
	const auto current = machine.state();
	const bool defending = current == compat::CompatibilityStateId::Hide &&
		machine.task() == compat::CompatibilityTaskId::HoldPosition;
	constexpr unsigned sniperWeapons = (1U << 3) | (1U << 13) | (1U << 18) | (1U << 24);
	const bool mayInvestigateFromDefense = defending &&
		reactionDisposition(index) == compat::ReactionDisposition::Engage &&
		!(actor.scenarioPlanners.atDefenseSpot() && (static_cast<unsigned>(self->v.weapons) & sniperWeapons)) &&
		!services_->observation_publicObservations().pendingReloadRequest(in.actor, in.frame, in.now);
	in.defenseAmbushExpired = mayInvestigateFromDefense && actor.scenarioPlanners.defenseAmbushExpired(in.now);
	const bool freeTask = current == compat::CompatibilityStateId::Idle ||
		current == compat::CompatibilityStateId::Hunt || current == compat::CompatibilityStateId::InvestigateNoise;
	compat::PublicActorState publicState{};
	const bool blind = services_->observation_publicObservations().read(in.actor, in.frame, &publicState) &&
		in.now < publicState.blindUntil;
	in.allowed = (freeTask || mayInvestigateFromDefense) && !machine.attackOverlayActive() && config.botEnable > 0 && config.botStop <= 0 &&
		!(self->v.flags & (FL_FROZEN | FL_SPECTATOR)) && in.now >= roundStart && !blind;
	in.hiding = current == compat::CompatibilityStateId::Hide;
	if (current != compat::CompatibilityStateId::InvestigateNoise && planner.active())
		planner.cancel();
	if ((in.noise.active && in.noise.ready && in.noise.kind != world::SoundKind::Objective && in.noise.navArea != 0 &&
			in.noise.navRevision == in.navRevision && in.allowed) ||
		planner.active())
	{
		const auto engine = services_->service_engineFunctions();
		const auto globals = services_->service_globals();
		const int team = services_->observation_resolvePlayerTeam(self);
		if (engine && engine->pfnPEntityOfEntIndex && globals && (team == 1 || team == 2))
			for (int slot = 1; slot <= (std::min)(32, globals->maxClients); ++slot)
			{
				const auto *player = engine->pfnPEntityOfEntIndex(slot);
				if (player && player != self && !player->free && (player->v.flags & FL_CLIENT) &&
					player->v.deadflag == DEAD_NO && player->v.health > 0 &&
					services_->observation_resolvePlayerTeam(player) == team)
					++in.friends;
			}
	}
	auto result = planner.evaluate(in, services_->service_compatibilityRandomSource());
	if (result == compat::NoiseInvestigationResult::NeedTravel)
	{
		nav::NavQuery query(nav);
		nav::NavAreaMatch start{};
		nav::NavVector feet{before.origin.x, before.origin.y, before.origin.z + self->v.mins.z};
		if (query.findContaining(feet, 64, &start) != nav::NavQueryResult::Found)
			(void)query.findNearest3D(feet, 10000, &start);
		in.travelKnown = true;
		in.travelDistance = start.area != 0 ? compat::noiseTravelDistance(nav, start.area, in.noise.navArea,
												  services_->navigation_managedPathCostContext(index))
											: -1;
		result = planner.evaluate(in, services_->service_compatibilityRandomSource());
	}
	if (result == compat::NoiseInvestigationResult::Attend)
	{
		if ((defending && actor.scenarioPlanners.delayDefenseInvestigation(in.now)) ||
			!services_->observation_consumeNoise(index, in.noise))
		{
			planner.cancel();
		}
		else
		{
			if (defending)
				actor.scenarioPlanners.cancelHoldPosition();
			compat::NoiseReportInput report{};
			report.actor = in.actor;
			report.frame = in.frame;
			report.timing = in.timing;
			report.now = in.now;
			report.lastSawEnemy = actor.lastVisibleEnemyTimes;
			report.alive = in.alive;
			report.roundOver = services_->service_roundOver();
			report.wellPastSafe = true; // Attend is reachable only after the planner's strict safe-time gate.
			report.priority = in.noise.priority;
			if (actor.noiseReportPolicy.attend(report, services_->service_compatibilityRandomSource()))
			{
				const auto *area = nav.document()->findArea(in.noise.navArea);
				actor.noiseReport = {true, in.actor, in.frame, in.now, area ? area->placeId : std::uint16_t{0},
					in.navRevision};
			}
			services_->applyNavigationRequest({index, NavigationRequestKind::DestroyPath});
			context.requestedState = compat::CompatibilityStateId::InvestigateNoise;
			context.requestTransition = current != context.requestedState;
			context.transitionId = compat::StateTransitionId::ExplicitStateChange;
			context.transitionReason = compat::StateTransitionReason::ObjectiveChanged;
		}
	}
	state.noiseAvailable = planner.active();
	if (planner.active())
	{
		const auto target = planner.target();
		const float distance = distanceTo(in.origin, target);
		state.goalReached = false;
		state.pathFailed = false;
		const auto &weapon = services_->observation_managedBotCurrentWeapons(index);
		const bool knife = weapon.available && weapon.active && weapon.weaponId == 29;
		const bool hurry =
			reactionHurrying(index) ||
			(services_->observation_bombNoticeKnown(index) && services_->observation_bombNoticePlanted(index));
		actor.investigationWalking = compat::investigationShouldWalk(distance, in.friends, knife, hurry);
		if (distance < 200 && !blind && services_->service_engineFunctions() &&
			services_->service_engineFunctions()->pfnTraceLine)
		{
			const world::WorldPosition eye{before.origin.x + self->v.view_ofs.x, before.origin.y + self->v.view_ofs.y,
				before.origin.z + self->v.view_ofs.z};
			const float x = target.x - eye.x, y = target.y - eye.y, z = target.z - eye.z;
			const float yaw = std::atan2(y, x) * 57.29577951308232f,
						pitch = -std::atan2(z, std::hypot(x, y)) * 57.29577951308232f;
			if (std::fabs(compat::normalizedAimAngle(yaw - self->v.v_angle.y)) < 20 &&
				std::fabs(compat::normalizedAimAngle(pitch - self->v.v_angle.x)) < 20 &&
				!services_->observation_smokeObservations().blocks(eye, target, in.now))
			{
				const float from[] = {eye.x, eye.y, eye.z}, to[] = {target.x, target.y, target.z};
				TraceResult trace{};
				services_->service_engineFunctions()->pfnTraceLine(from, to, 0x101, self, &trace);
				if (trace.flFraction == 1 && !trace.fStartSolid && !trace.fAllSolid)
				{
					planner.cancel();
					actor.investigationWalking = false;
					state.goalReached = true;
					state.noiseAvailable = false;
				}
			}
		}
	}
	else
		actor.investigationWalking = false;
}
}
}
