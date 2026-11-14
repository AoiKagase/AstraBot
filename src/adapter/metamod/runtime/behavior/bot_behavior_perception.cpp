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
		}
		else
		{
			actors_[index].recognizedTargets = {};
		}
	}
	if (actors_[index].recognizedTargets.isValid())
	{
		*targetActor = actors_[index].recognizedTargets;
		*targetPosition = actors_[index].recognizedPoints;
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
		actor.enemyCountChatter.reset();
		actor.killPraise.reset();
		actor.knifeRun.reset();
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

void BotBehaviorRuntime::observePublicDeath(std::size_t index)
{
	for (auto &actor : actors_)
	{
		if (actor.enemyWeaponSource.id == index + 1U)
		{
			actor.enemyWeaponSource = {};
		}
	}
	stopVoiceFeedback(index);
	actors_[index].outgoingRadioSchedulers.discardNoiseReport(actors_[index].noiseReport);
	actors_[index].noiseInvestigation.reset();
	actors_[index].noiseReportPolicy = {};
	actors_[index].enemyCountChatter.reset();
	actors_[index].killPraise.reset();
	actors_[index].knifeRun.reset();
	actors_[index].enemyWeaponSource = {};
	actors_[index].noiseReport = {};
	actors_[index].investigationWalking = false;
	actors_[index].noiseLook.reset();
	actors_[index].closeFootstep = {};
	actors_[index].noiseBend = {};
	actors_[index].noiseLookUpdate = 0U;
	actors_[index].reactionQueues.reset();
}
} // namespace metamod
} // namespace astrabot
