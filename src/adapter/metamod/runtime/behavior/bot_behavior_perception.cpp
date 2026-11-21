#include "runtime/behavior/bot_behavior_runtime.hpp"
#include <algorithm>
#include <cmath>

namespace astrabot
{
namespace metamod
{
bool BotBehaviorRuntime::isBlinded(world::ActorKey actor, world::FrameIdentity frame, float now) const
{
	const bool compatibility =
		services_->service_compatibilitySurface().configuration().mode == compat::RuntimeMode::Compatibility;
	return services_->observation_publicObservations().isBlinded(actor, frame, now, compatibility);
}

bool BotBehaviorRuntime::canSeeEntityOrigin(
	std::size_t index, const edict_t *entity, const world::FrameIdentity &frame, bool checkFov) const
{
	return entity && canSeePoint(index, {entity->v.origin.x, entity->v.origin.y, entity->v.origin.z}, frame, checkFov);
}

bool BotBehaviorRuntime::canSeePoint(std::size_t index, world::WorldPosition point,
	const world::FrameIdentity &frame, bool checkFov) const
{
	if (!services_ || index >= actors_.size())
		return false;
	const auto engine = services_->service_engineFunctions();
	const auto globals = services_->service_globals();
	const auto &handle = services_->service_managedBotHandles()[index];
	auto *self = handle.entity;
	if (!engine || !engine->pfnTraceLine || !globals || !self)
		return false;
	if (isBlinded({handle.actor.slot, handle.actor.actorGeneration}, frame, globals->time))
		return false;
	const world::WorldPosition eye{self->v.origin.x + self->v.view_ofs.x, self->v.origin.y + self->v.view_ofs.y,
		self->v.origin.z + self->v.view_ofs.z};
	constexpr float kMaximumCoordinate = world::WorldLimits::kMaximumCoordinate;
	for (float value : {eye.x, eye.y, eye.z, point.x, point.y, point.z, self->v.origin.x, self->v.origin.y})
		if (!std::isfinite(value) || std::fabs(value) > kMaximumCoordinate)
			return false;
	if ((checkFov && !compat::killPraiseViewCone(
			point.x - self->v.origin.x, point.y - self->v.origin.y, self->v.angles.x, self->v.angles.y)) ||
		services_->observation_smokeObservations().blocks(eye, point, globals->time))
		return false;
	const float from[]{eye.x, eye.y, eye.z};
	const float to[]{point.x, point.y, point.z};
	TraceResult result{};
	constexpr int kIgnoreActorsAndGlass = 0x101;
	engine->pfnTraceLine(from, to, kIgnoreActorsAndGlass, self, &result);
	return result.flFraction == 1.0f;
}

bool BotBehaviorRuntime::refreshRecoveredPerception(std::size_t index)
{
	const auto receipt = reactionContext(index);
	const auto revision = actors_[index].executionRevision;
	const auto epoch = actors_[index].managedBotStateMachines.normalStateEpoch();
	// Blind updates skip perception. Publish pending sounds before the
	// recovered state's investigation reads the shared snapshot.
	world::WorldSnapshot belief;
	world::ActorKey target{};
	world::WorldPosition point{};
	(void)prepareManagedPerception(index, &belief, &target, &point);
	return executionCurrent(index, receipt, revision) &&
		actors_[index].managedBotStateMachines.normalStateEpoch() == epoch;
}

bool BotBehaviorRuntime::prepareManagedPerception(std::size_t index, world::WorldSnapshot *snapshot,
	world::ActorKey *targetActor, world::WorldPosition *targetPosition)
{
	auto &actor = actors_[index];
	const bool compatibility =
		services_->service_compatibilitySurface().configuration().mode == compat::RuntimeMode::Compatibility;
	const auto idleEntries = actor.managedBotStateMachines.idleEntryCount();
	if (compatibility && actor.enemyIdleEntries != idleEntries)
	{
		// CSBot clears the acquired enemy when entering Idle, including after Follow's Attack ends.
		actor.enemyIdleEntries = idleEntries;
		actor.enemyReaction.clearEnemy();
		actor.enemyWeaponSource = {};
		actor.recognizedTargets = {};
		actor.occludedRecognition = {};
		actor.noiseEnemyContact.forgetEnemy();
	}
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
	auto &candidates = threats.candidates;
	auto candidateCount = threats.count;
	const bool newSample = actor.recognitionSequences != services_->service_managedBotFullUpdateSequences()[index];
	if (newSample)
	{
		actor.occludedRecognition = {};
		actors_[index].recognitionSequences = services_->service_managedBotFullUpdateSequences()[index];
		compat::ThreatCandidate threat{};
		const bool blinded = services_->service_globals() != nullptr &&
			isBlinded(observer, belief.identity().frame, services_->service_globals()->time);
		const auto findThreatEntity = [&](world::ActorKey key) -> edict_t *
		{
			const auto *engine = services_->service_engineFunctions();
			return engine && engine->pfnPEntityOfEntIndex ? engine->pfnPEntityOfEntIndex(static_cast<int>(key.slot))
													 : nullptr;
		};
		const auto isLiveThreat = [&](const compat::ThreatCandidate &candidate, const edict_t *enemy)
		{
			return enemy && !enemy->free && enemy->serialnumber >= 0 && enemy->v.deadflag == DEAD_NO &&
				enemy->v.health > 0 && candidate.sourceEntity == world::EntityKey{candidate.actor.slot,
					static_cast<std::uint32_t>(enemy->serialnumber) + 1U} && reactionEnemyCurrent(candidate.actor);
		};
		if (compatibility && !blinded && candidateCount != 0U && services_->service_globals())
		{
			const auto attention = actor.reactionQueues.nativeAttention(observer, belief.identity().frame, reaction.now);
			const auto execution = actor.executionRevision;
			const auto update = services_->service_managedBotFullUpdateSequences()[index];
			const auto request = compat::RandomRequest::floatRequest("Vision.Notice", {observer.slot, observer.generation},
				{services_->command_managedBotCommandSequences(index), 0U, update}, 0.0f, 100.0f);
			const float skill = static_cast<float>(actor.managedProfiles.skill) / 100.0f;
			std::sort(candidates.begin(), candidates.begin() + candidateCount,
				[](const compat::ThreatCandidate &a, const compat::ThreatCandidate &b) { return a.actor.slot < b.actor.slot; });
			std::size_t noticed = 0U;
			for (std::size_t entry = 0U; entry < candidateCount; ++entry)
			{
				const auto &candidate = candidates[entry];
				if (!attention.skipsDraw(candidate))
				{
					const float chance = compat::nativeThreatNoticeChance(candidate, attention.elapsed, skill);
					const auto roll = services_->service_compatibilityRandomSource().nextFloat(request);
					if (!executionCurrent(index, reaction, execution) ||
						services_->service_managedBotFullUpdateSequences()[index] != update)
						return false;
					if (roll.status != compat::RandomStatus::Ok || !(roll.value < chance))
						continue;
				}
				candidates[noticed++] = candidate;
			}
			// Finish engine callbacks before checking the live identities that will enter the ring.
			std::array<const edict_t *, ThreatObservation::kMaximumThreatCandidates> entities{};
			for (std::size_t entry = 0U; entry < noticed; ++entry)
				entities[entry] = findThreatEntity(candidates[entry].actor);
			if (!executionCurrent(index, reaction, execution) ||
				services_->service_managedBotFullUpdateSequences()[index] != update)
				return false;
			candidateCount = 0U;
			for (std::size_t entry = 0U; entry < noticed; ++entry)
				if (isLiveThreat(candidates[entry], entities[entry]))
					candidates[candidateCount++] = candidates[entry];
		}
		if (!blinded && candidateCount != 0U && services_->service_globals() != nullptr)
			actors_[index].lastVisibleEnemyTimes = services_->service_globals()->time;
		const float delay = actors_[index].managedProfiles.effectiveReactionSeconds(compatibility);
		const auto sampleExecution = actor.executionRevision;
		const auto sampleUpdate = services_->service_managedBotFullUpdateSequences()[index];
		auto completeSample = [&](compat::ThreatCandidate &candidate) {
			if (!compatibility)
				return true;
			auto *entity = findThreatEntity(candidate.actor);
			if (!executionCurrent(index, reaction, sampleExecution) || !isLiveThreat(candidate, entity))
				return false;
			bool ownsShield = false, shieldDrawn = false;
			auto reload = combat::ReloadState::Unknown;
			(void)services_->observation_shieldState(entity, ownsShield, shieldDrawn, reload);
			candidate.reloading = reload == combat::ReloadState::Reloading;
			return executionCurrent(index, reaction, sampleExecution) &&
				services_->service_managedBotFullUpdateSequences()[index] == sampleUpdate && isLiveThreat(candidate, entity);
		};
		const bool sampled = services_->service_globals() != nullptr &&
			actors_[index].reactionQueues.update(observer, belief.identity().frame, services_->service_globals()->time,
				delay, candidates.data(), candidateCount, blinded,
				&threat, compatibility,
				[](void *context, compat::ThreatCandidate &candidate) {
					return (*static_cast<decltype(completeSample) *>(context))(candidate);
				}, &completeSample);
		if (!executionCurrent(index, reaction, sampleExecution) ||
			services_->service_managedBotFullUpdateSequences()[index] != sampleUpdate)
			return false;
		const world::ActorObservation *seen = nullptr;
		actor.recognizedReloading = sampled && threat.reloading;
		bool recognized = sampled;
		if (sampled)
		{
			(void)belief.findActor(threat.actor, &seen);
			if (compatibility)
			{
				const auto execution = actors_[index].executionRevision;
				const auto *enemy = findThreatEntity(threat.actor);
				if (!executionCurrent(index, reaction, execution))
					return false;
				recognized = seen && seen->relation == world::TeamRelation::Hostile && isLiveThreat(threat, enemy);
				if (recognized && !seen->visible && !actor.managedBotStateMachines.attackOverlayActive())
					actor.occludedRecognition = {enemy, threat.sourceEntity, reaction.frame.tick};
			}
		}
		if (recognized)
		{
			// CSBot can retain a delayed threat without entering the attack overlay.
			if (compatibility && reactionIgnoring(index))
			{
				actor.noiseEnemyContact.enemy = threat.actor;
				actor.noiseEnemyContact.source = threat.sourceEntity;
			}
			const auto recentAttacker = memory.recentAttacker(reaction, 3.0f);
			const bool alreadyRetaliating = recentAttacker.isValid() && memory.source(reaction) == recentAttacker &&
				memory.enemy(reaction) == threat.actor;
			// A temporary gap in delayed recognition does not replace the retained enemy.
			const bool sameEnemy = !memory.enemyDead(reaction) && memory.enemy(reaction) == threat.actor &&
				memory.source(reaction) == threat.sourceEntity &&
				actors_[index].enemyWeaponSource == threat.sourceEntity;
			const bool newAcquisition = compatibility ? !sameEnemy
				: !(actors_[index].recognizedTargets == threat.actor) && !alreadyRetaliating;
			if (newAcquisition)
			{
				observeRecognizedTargetChange(index, services_->service_globals()->time);
				if (compatibility && memory.enemyDead(reaction))
					memory.clearEnemy();
			}
			actors_[index].recognizedTargets = threat.actor;
			actors_[index].recognizedPoints = threat.visiblePoint;
			actors_[index].enemyWeaponSource = threat.sourceEntity;
			const bool currentlyVisible = seen && seen->visible;
			if (currentlyVisible || (compatibility && newAcquisition))
			{
				// Seed an occluded contact once. Only new sight refreshes its timestamp.
				memory.rememberVisible(reaction, threat.actor, threat.sourceEntity,
					currentlyVisible ? seen->position : threat.visiblePoint);
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
	const bool blinded = isBlinded(observer, belief.identity().frame, reaction.now);
	const world::ActorObservation *currentEnemy = nullptr;
	if (!blinded && memory.enemy(reaction).isValid() &&
		belief.findActor(memory.enemy(reaction), &currentEnemy) == world::ContactLookupResult::Found && currentEnemy &&
		currentEnemy->visible)
	{
		memory.rememberVisible(reaction, currentEnemy->actor, memory.source(reaction), currentEnemy->position);
		auto &contact = actor.noiseEnemyContact;
		if (compatibility && belief.identity().frame == reaction.frame && contact.enemy == currentEnemy->actor &&
			contact.source == memory.source(reaction) && !memory.enemyDead(reaction))
		{
			contact.position = currentEnemy->position;
			contact.seenAt = reaction.now;
		}
	}
	if (compatibility && newSample && !blinded &&
		(actor.managedBotStateMachines.state() == compat::CompatibilityStateId::DefuseBomb ||
			actor.managedBotStateMachines.state() == compat::CompatibilityStateId::PlantBomb))
	{
		// Upkeep uses this decision until the next full perception update, even across the three-second deadline.
		actor.bombObjectiveAimDecision = {memory.enemy(reaction),
			memory.shouldAim(reaction) ? currentEnemySource(index) : world::EntityKey{},
			actor.managedBotStateMachines.normalStateEpoch()};
	}
	if (actors_[index].recognizedTargets.isValid() && !blinded)
	{
		*targetActor = actors_[index].recognizedTargets;
		*targetPosition = actors_[index].recognizedPoints;
		if (compatibility && memory.enemy(reaction) == *targetActor && memory.source(reaction) == actor.enemyWeaponSource &&
			!memory.enemyDead(reaction) && currentEnemy && !currentEnemy->visible)
			*targetPosition = memory.position(reaction);
	}
	else if (!blinded && compatibility && memory.shouldAim(reaction))
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

void BotBehaviorRuntime::resetRoundRecognition(bool endingRound)
{
	if (!endingRound)
	{
		resetEncouragement();
		if (!services_->service_roundOver())
			shuffleReadyChatterBanks();
	}
	looseBombDrop_ = {};
	updateVoiceFeedback(true);
	for (std::size_t index = 0; index < actors_.size(); ++index)
	{
		auto &actor = actors_[index];
		actor.bombStatus = {};
		actor.someHostagesTaken = false;
		actor.sniperWarning = {};
		actor.hostagePositions = {};
		actor.looseBombObservation = {};
		actor.noiseInvestigation.reset();
		actor.noiseReportPolicy = {};
		actor.confirmedSpeech.reset();
		actor.outgoingRadioSchedulers.discardPending(chatterAdmission(index), endingRound);
		actor.knifeRun.reset();
		actor.enemyReaction.resetForLife();
		actor.noiseEnemyContact = {};
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
		actor.reactionQueues.resetForLife();
	}
}

void BotBehaviorRuntime::observePublicDeath(std::size_t index, const world::FrameIdentity &deathFrame)
{
	const auto *self = services_->service_managedBotHandles()[index].entity;
	if (deathFrame.isValid() && self && !self->free && (self->v.deadflag != DEAD_NO || self->v.health <= 0.0f))
	{
		actors_[index].morale.observeLife(false);
		resetEncouragement();
	}
	const auto previousSpeech = actors_[index].confirmedSpeech;
	const bool compatibility =
		services_->service_compatibilitySurface().configuration().mode == compat::RuntimeMode::Compatibility;
	for (std::size_t observer = 0; observer < actors_.size(); ++observer)
	{
		auto &actor = actors_[observer];
		const auto context = reactionContext(observer);
		if (actor.enemyReaction.source(context).id == index + 1U)
		{
			actor.enemyReaction.markDead(context);
		}
		if (compatibility && actor.recognizedTargets.slot == index + 1U)
		{
			actor.recognizedTargets = {};
		}
		if (actor.enemyWeaponSource.id == index + 1U)
		{
			actor.enemyWeaponSource = {};
		}
		if (actor.noiseEnemyContact.source.id == index + 1U)
			actor.noiseEnemyContact.forgetEnemy();
	}
	actors_[index].outgoingRadioSchedulers.discardNoiseReport(actors_[index].noiseReport);
	actors_[index].confirmedSpeech.reset();
	actors_[index].outgoingRadioSchedulers.cancelOnDeath(
		services_->observation_scenarioRadio(index).sequence, chatterAdmission(index));
	actors_[index].noiseInvestigation.reset();
	actors_[index].noiseReportPolicy = {};
	actors_[index].knifeRun.reset();
	actors_[index].enemyReaction.resetForLife();
	actors_[index].noiseEnemyContact = {};
	actors_[index].enemyWeaponSource = {};
	actors_[index].noiseReport = {};
	actors_[index].investigationWalking = false;
	actors_[index].noiseLook.reset();
	actors_[index].closeFootstep = {};
	actors_[index].noiseBend = {};
	actors_[index].noiseLookUpdate = 0U;
	actors_[index].reactionQueues.resetForLife();
	finishDeathChatter(index, previousSpeech, deathFrame);
}
} // namespace metamod
} // namespace astrabot
