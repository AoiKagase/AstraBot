#include "runtime/behavior/bot_behavior_runtime.hpp"
#include <algorithm>
#include <cmath>

namespace astrabot
{
namespace metamod
{
bool BotBehaviorRuntime::prepareManagedPerception(std::size_t index, world::WorldSnapshot *snapshot,
	world::ActorKey *targetActor, world::WorldPosition *targetPosition)
{
	ThreatObservation threats{};
	if (!services_->observation_collectManagedPerception(index, snapshot, &threats))
	{
		return false;
	}
	if (!threats.observerAvailable)
	{
		return true;
	}
	const auto &belief = *snapshot;
	const auto observer = belief.identity().observer;
	const auto reaction = reactionContext(index);
	auto &memory = actors_[index].enemyReaction;
	(void)memory.update(reaction);
	const world::ActorObservation *retained = nullptr;
	const auto retainedEnemy = memory.enemy(reaction);
	if (retainedEnemy.isValid() && (!reactionEnemyCurrent(retainedEnemy) ||
		belief.findActor(retainedEnemy, &retained) != world::ContactLookupResult::Found))
	{
		memory.clearEnemy(!memory.pursuing(reaction) || memory.pursuitEnemy(reaction) == retainedEnemy);
	}
	const auto &candidates = threats.candidates;
	const auto candidateCount = threats.count;
	if (actors_[index].recognitionSequences != services_->service_managedBotFullUpdateSequences()[index])
	{
		actors_[index].recognitionSequences = services_->service_managedBotFullUpdateSequences()[index];
		compat::ThreatCandidate threat{};
		compat::PublicActorState publicState{};
		const bool blinded =
			services_->observation_publicObservations().read(observer, belief.identity().frame, &publicState) &&
			services_->service_globals() != nullptr && services_->service_globals()->time < publicState.blindUntil;
		if (!blinded && candidateCount != 0U && services_->service_globals() != nullptr)
		{
			actors_[index].lastVisibleEnemyTimes = services_->service_globals()->time;
			actors_[index].lastVisibleEnemyPositions = candidates[0].visiblePoint;
		}
		if (services_->service_globals() != nullptr &&
			actors_[index].reactionQueues.update(observer, belief.identity().frame, services_->service_globals()->time,
				actors_[index].managedProfiles.reactionTimeSeconds, candidates.data(), candidateCount, blinded,
				&threat))
		{
			if (!(actors_[index].recognizedTargets == threat.actor))
			{
				observeRecognizedTargetChange(index, services_->service_globals()->time);
			}
			actors_[index].recognizedTargets = threat.actor;
			actors_[index].recognizedPoints = threat.visiblePoint;
			actors_[index].enemyWeaponSource = {};
			for (std::size_t candidate = 0; candidate < candidateCount; ++candidate)
			{
				if (candidates[candidate].actor == threat.actor)
				{
					actors_[index].enemyWeaponSource = threats.sourceEntities[candidate];
					break;
				}
			}
			const world::ActorObservation *seen = nullptr;
			if (belief.findActor(threat.actor, &seen) == world::ContactLookupResult::Found && seen && seen->visible)
			{
				memory.rememberVisible(reaction, threat.actor, actors_[index].enemyWeaponSource, seen->position);
				const float roundStart = services_->service_roundFreezeGeneration() == reaction.frame.roundGeneration
					? services_->service_roundFreezeUntil()
					: 0.0f;
				memory.adjustSafeTime(
					reaction.now - roundStart, static_cast<float>(actors_[index].managedProfiles.aggression) / 100.0f);
			}
		}
		else
		{
			actors_[index].recognizedTargets = {};
		}
	}
	compat::PublicActorState sight{};
	const bool blinded = services_->observation_publicObservations().read(observer, belief.identity().frame, &sight) &&
		reaction.now < sight.blindUntil;
	const world::ActorObservation *currentEnemy = nullptr;
	if (!blinded && memory.enemy(reaction).isValid() &&
		belief.findActor(memory.enemy(reaction), &currentEnemy) == world::ContactLookupResult::Found && currentEnemy &&
		currentEnemy->visible)
	{
		memory.rememberVisible(reaction, currentEnemy->actor, memory.source(reaction), currentEnemy->position);
	}
	if (actors_[index].recognizedTargets.isValid() && !blinded)
	{
		*targetActor = actors_[index].recognizedTargets;
		*targetPosition = actors_[index].recognizedPoints;
	}
	else if (!blinded &&
		services_->service_compatibilitySurface().configuration().mode == compat::RuntimeMode::Compatibility &&
		memory.shouldAim(reaction))
	{
		*targetActor = memory.enemy(reaction);
		*targetPosition = memory.position(reaction);
	}
	return true;
}

void BotBehaviorRuntime::observeEnemyWeaponFire(
	const edict_t *source, const world::FrameIdentity &receiptFrame, float now)
{
	const auto frame = receiptFrame;
	if (!services_ || !source || source->free || source->serialnumber < 0 || (source->v.flags & FL_CLIENT) == 0 ||
		(source->v.flags & (FL_SPECTATOR | FL_DORMANT)) != 0 || source->v.deadflag != DEAD_NO ||
		!(source->v.health > 0) || !std::isfinite(now) || now < 0 ||
		services_->service_compatibilitySurface().configuration().mode != compat::RuntimeMode::Compatibility)
	{
		return;
	}
	const auto engine = services_->service_engineFunctions();
	const auto globals = services_->service_globals();
	const auto &life = services_->service_lifecycle();
	if (!engine || !globals || !engine->pfnIndexOfEdict || !engine->pfnPEntityOfEntIndex || !life.isMapActive() ||
		!frame.isValid() || frame.mapGeneration != life.mapGeneration() ||
		frame.roundGeneration != life.roundGeneration() || frame.tick != services_->service_adapterFrameCount() ||
		globals->time != now)
	{
		return;
	}
	constexpr int kMaximumClients = 32;
	const int slot = engine->pfnIndexOfEdict(source);
	if (slot < 1 || slot > (std::min)(kMaximumClients, globals->maxClients) ||
		engine->pfnPEntityOfEntIndex(slot) != source)
	{
		return;
	}
	constexpr int kTerroristTeam = 1;
	constexpr int kCounterTerroristTeam = 2;
	const int sourceTeam = services_->observation_resolvePlayerTeam(source);
	if (sourceTeam != kTerroristTeam && sourceTeam != kCounterTerroristTeam)
	{
		return;
	}
	const world::EntityKey sourceKey{
		static_cast<std::uint32_t>(slot), static_cast<std::uint32_t>(source->serialnumber) + 1U};
	for (std::size_t index = 0; index < actors_.size(); ++index)
	{
		const auto &handle = services_->service_managedBotHandles()[index];
		auto *self = handle.entity;
		const auto &weapon = services_->observation_managedBotCurrentWeapons(index);
		constexpr int kKnife = 29;
		if (!services_->service_managedBotSlots()[index] || !self || self == source || self->free ||
			self->serialnumber < 0 || handle.actor.actorGeneration == 0 || (self->v.flags & FL_CLIENT) == 0 ||
			(self->v.flags & (FL_SPECTATOR | FL_DORMANT)) != 0 || self->v.deadflag != DEAD_NO ||
			!(self->v.health > 0) || engine->pfnPEntityOfEntIndex(static_cast<int>(index + 1U)) != self ||
			!(actors_[index].enemyWeaponSource == sourceKey) || !weapon.available || !weapon.active ||
			weapon.weaponId != kKnife)
		{
			continue;
		}
		const int team = services_->observation_resolvePlayerTeam(self);
		if ((team == kTerroristTeam || team == kCounterTerroristTeam) && team != sourceTeam)
		{
			actors_[index].knifeRun.start({handle.actor.slot, handle.actor.actorGeneration}, frame,
				static_cast<std::uint32_t>(self->serialnumber) + 1U, now);
		}
	}
}

bool BotBehaviorRuntime::forcedRunning(std::size_t index)
{
	if (!services_ || index >= actors_.size())
	{
		return false;
	}
	const auto &handle = services_->service_managedBotHandles()[index];
	const auto *self = handle.entity;
	const auto globals = services_->service_globals();
	const auto &life = services_->service_lifecycle();
	if (!self || self->free || self->serialnumber < 0 || !globals || !life.isMapActive() ||
		!services_->service_managedBotSlots()[index])
	{
		actors_[index].knifeRun.reset();
		actors_[index].enemyReaction.reset();
		return false;
	}
	return actors_[index].knifeRun.active({handle.actor.slot, handle.actor.actorGeneration},
		{life.mapGeneration(), life.roundGeneration(), services_->service_adapterFrameCount()},
		static_cast<std::uint32_t>(self->serialnumber) + 1U, globals->time,
		self->v.deadflag == DEAD_NO && self->v.health > 0);
}

void BotBehaviorRuntime::resetRoundRecognition()
{
	updateVoiceFeedback(true);
	for (auto &actor : actors_)
	{
		actor.noiseInvestigation.reset();
		actor.noiseReportPolicy = {};
		actor.outgoingRadioSchedulers.discardPending();
		actor.confirmedSpeech.reset();
		actor.knifeRun.reset();
		actor.enemyReaction.reset();
		actor.enemyWeaponSource = {};
		actor.noiseReport = {};
		actor.investigationWalking = false;
		actor.noiseLook.reset();
		actor.closeFootstep = {};
		actor.noiseBend = {};
		actor.noiseLookUpdate = 0U;
		actor.lastVisibleEnemyTimes = 0.0f;
		actor.recognizedTargets = {};
		actor.recognitionSequences = 0U;
		actor.reactionQueues.reset();
	}
}

void BotBehaviorRuntime::observePublicDeath(std::size_t index, const world::FrameIdentity &deathFrame)
{
	const auto previousSpeech = actors_[index].confirmedSpeech;
	for (std::size_t observer = 0; observer < actors_.size(); ++observer)
	{
		auto &actor = actors_[observer];
		if (actor.enemyReaction.source(reactionContext(observer)).id == index + 1U)
		{
			actor.enemyReaction.markDead();
		}
		if (actor.enemyWeaponSource.id == index + 1U)
		{
			actor.enemyWeaponSource = {};
		}
	}
	actors_[index].outgoingRadioSchedulers.discardNoiseReport(actors_[index].noiseReport);
	actors_[index].outgoingRadioSchedulers.cancelOnDeath(services_->observation_scenarioRadio(index).sequence);
	actors_[index].noiseInvestigation.reset();
	actors_[index].noiseReportPolicy = {};
	actors_[index].outgoingRadioSchedulers.discardPending();
	actors_[index].confirmedSpeech.reset();
	actors_[index].knifeRun.reset();
	actors_[index].enemyReaction.reset();
	actors_[index].enemyWeaponSource = {};
	actors_[index].noiseReport = {};
	actors_[index].investigationWalking = false;
	actors_[index].noiseLook.reset();
	actors_[index].closeFootstep = {};
	actors_[index].noiseBend = {};
	actors_[index].noiseLookUpdate = 0U;
	actors_[index].reactionQueues.reset();
	finishDeathChatter(index, previousSpeech, deathFrame);
}
} // namespace metamod
} // namespace astrabot
