#include "runtime/behavior/bot_behavior_runtime.hpp"
#include <cmath>
namespace astrabot
{
namespace metamod
{
compat::ReactionContext BotBehaviorRuntime::reactionContext(std::size_t index) const
{
	compat::ReactionContext out{};
	if (!services_ || index >= actors_.size())
	{
		return out;
	}
	const auto &handle = services_->service_managedBotHandles()[index];
	const auto *self = handle.entity;
	const auto globals = services_->service_globals();
	const auto &life = services_->service_lifecycle();
	if (!self || self->free || self->serialnumber < 0 || !globals || !life.isMapActive() ||
		!services_->service_managedBotSlots()[index])
	{
		return out;
	}
	out.actor = {handle.actor.slot, handle.actor.actorGeneration};
	out.frame = {life.mapGeneration(), life.roundGeneration(), services_->service_adapterFrameCount()};
	out.entityGeneration = static_cast<std::uint32_t>(self->serialnumber) + 1U;
	out.now = globals->time;
	out.alive = self->v.deadflag == DEAD_NO && self->v.health > 0 && (self->v.flags & (FL_SPECTATOR | FL_DORMANT)) == 0;
	return out;
}

bool BotBehaviorRuntime::receiptConnectionCurrent(world::ActorKey connection) const
{
	if (!services_)
	{
		return false;
	}
	const auto &life = services_->service_lifecycle();
	return life.slotGeneration(connection.slot) == connection.generation &&
		(connection.generation == 0 || life.isCurrent(life.tokenForSlot(connection.slot)));
}

bool BotBehaviorRuntime::reactionEnemyCurrent(world::ActorKey enemy) const
{
	if (!services_ || !enemy.isValid())
	{
		return false;
	}
	const auto &life = services_->service_lifecycle();
	const auto token = life.tokenForSlot(enemy.slot);
	return life.slotGeneration(enemy.slot) == 0 ? enemy.generation == 1
												: token.slotGeneration == enemy.generation && life.isCurrent(token);
}

world::EntityKey BotBehaviorRuntime::currentEnemySource(std::size_t index) const
{
	if (!services_ || index >= actors_.size() ||
		services_->service_compatibilitySurface().configuration().mode != compat::RuntimeMode::Compatibility)
	{
		return {};
	}
	const auto context = reactionContext(index);
	return actors_[index].enemyReaction.enemyDead(context) ||
			!reactionEnemyCurrent(actors_[index].enemyReaction.enemy(context))
		? world::EntityKey{}
		: actors_[index].enemyReaction.source(context);
}

bool BotBehaviorRuntime::reactionPursuing(std::size_t index) const
{
	return services_ &&
		services_->service_compatibilitySurface().configuration().mode == compat::RuntimeMode::Compatibility &&
		index < actors_.size() && actors_[index].enemyReaction.pursuing(reactionContext(index));
}

bool BotBehaviorRuntime::reactionIgnoring(std::size_t index) const
{
	return services_ &&
		services_->service_compatibilitySurface().configuration().mode == compat::RuntimeMode::Compatibility &&
		index < actors_.size() && actors_[index].enemyReaction.ignoring(reactionContext(index));
}

bool BotBehaviorRuntime::reactionHurrying(std::size_t index) const
{
	return services_ &&
		services_->service_compatibilitySurface().configuration().mode == compat::RuntimeMode::Compatibility &&
		index < actors_.size() && actors_[index].enemyReaction.hurrying(reactionContext(index));
}

compat::ReactionDisposition BotBehaviorRuntime::reactionDisposition(std::size_t index) const
{
	if (reactionIgnoring(index))
	{
		return compat::ReactionDisposition::Ignore;
	}
	const auto &actor = actors_[index];
	const auto state = actor.managedBotStateMachines.state();
	if (state == compat::CompatibilityStateId::DefuseBomb || state == compat::CompatibilityStateId::PlantBomb ||
		(state == compat::CompatibilityStateId::MoveTo &&
			(actor.scenarioDecisions.escortInferred ||
				actor.scenarioDecisions.action == compat::ScenarioAction::Escape)))
	{
		return compat::ReactionDisposition::SelfDefense;
	}
	if (actor.scenarioDecisions.action == compat::ScenarioAction::Guard ||
		actor.managedBotStateMachines.task() == compat::CompatibilityTaskId::GuardTickingBomb)
	{
		return compat::ReactionDisposition::Opportunity;
	}
	return compat::ReactionDisposition::Engage;
}

void BotBehaviorRuntime::changeReactionState(std::size_t index, compat::CompatibilityStateId next, bool pursue)
{
	auto &actor = actors_[index];
	auto &machine = actor.managedBotStateMachines;
	const auto from = machine.state();
	runtime::MovementPhysicsState before{};
	before.dead = !reactionContext(index).alive;
	auto state = makeManagedBotStateContext(index, before);
	if (pursue)
	{
		state.moveToTask = compat::CompatibilityTaskId::MoveToLastKnownEnemyPosition;
	}
	if (from == compat::CompatibilityStateId::Follow && machine.attackOverlayActive())
	{
		// StopAttacking calls Idle while following; Idle entry resets disposition.
		actor.enemyReaction.cancelIgnore();
	}
	actor.enemyReaction.onStateChange(reactionContext(index), from, next);
	(void)machine.transition(
		next, compat::StateTransitionReason::DeathObserved, compat::StateTransitionId::ExplicitStateChange, state);
	actor.managedBotCombat.stopMovement();
	actor.managedBotCombat.clearAim();
	actor.weaponActions.stopRapidFire();
	actor.managedBotObjectiveTargets = {};
	actor.managedBotObjectiveActions = {};
	actor.scenarioDecisions = {};
	actor.noiseInvestigation.cancel();
	actor.investigationWalking = false;
	services_->applyNavigationRequest({index, NavigationRequestKind::ClearStep});
	services_->applyNavigationRequest({index, NavigationRequestKind::DestroyPath});
	services_->applyNavigationRequest({index, NavigationRequestKind::ClearFailure});
	services_->applyNavigationRequest({index, NavigationRequestKind::ResetProgress});
}

void BotBehaviorRuntime::observeFriendDeath(std::size_t index, const edict_t *victim, const edict_t *killer,
	const world::FrameIdentity &frame, world::ActorKey victimConnection, world::ActorKey killerConnection)
{
	if (services_->service_compatibilitySurface().configuration().mode != compat::RuntimeMode::Compatibility)
	{
		return;
	}
	auto &actor = actors_[index];
	auto context = reactionContext(index);
	const auto admission = chatterAdmission(index);
	if (!actor.enemyReaction.update(context))
	{
		return;
	}
	const auto &handle = services_->service_managedBotHandles()[index];
	auto *self = handle.entity;
	const auto engine = services_->service_engineFunctions();
	const int selfSerial = self->serialnumber;
	const world::ActorKey observerConnection{
		context.actor.slot, services_->service_lifecycle().slotGeneration(context.actor.slot)};
	const int victimSerial = victim->serialnumber;
	const int killerSerial = killer ? killer->serialnumber : -1;
	const auto current = [&]()
	{
		const auto now = reactionContext(index);
		return now.valid() && admission.ownerRevision == actor.confirmedSpeech.lifetimeRevision() &&
			now.entityGeneration == context.entityGeneration && now.actor == context.actor && now.frame == frame &&
			now.now == context.now && receiptConnectionCurrent(victimConnection) &&
			receiptConnectionCurrent(killerConnection) && receiptConnectionCurrent(observerConnection) &&
			self->serialnumber == selfSerial && (self->v.flags & FL_CLIENT) != 0 && !victim->free &&
			victim->serialnumber == victimSerial && (victim->v.flags & FL_CLIENT) != 0 &&
			(!killer ||
				(!killer->free && killer->serialnumber == killerSerial && (killer->v.flags & FL_CLIENT) != 0)) &&
			services_->service_managedBotHandles()[index].entity == self;
	};
	runtime::MovementPhysicsState before{};
	before.dead = false;
	if (!actor.managedBotStateMachines.isInitialized())
	{
		(void)actor.managedBotStateMachines.initialize(makeManagedBotStateContext(index, before));
	}
	compat::FriendDeathInput in{};
	in.context = context;
	in.timing = {services_->command_managedBotCommandSequences(index), 0,
		services_->service_managedBotFullUpdateSequences()[index]};
	in.state = actor.managedBotStateMachines.state();
	in.attacking = actor.managedBotStateMachines.attackOverlayActive();
	in.victimFriendly = true;
	in.killerEnemy = killer &&
		services_->observation_resolvePlayerTeam(killer) != services_->observation_resolvePlayerTeam(self) &&
		services_->observation_resolvePlayerTeam(killer) != 0;
	in.disposition = reactionDisposition(index);
	const auto inventory = static_cast<std::uint32_t>(self->v.weapons);
	for (unsigned weapon : {3U, 13U, 18U, 24U})
	{
		in.sniperInventory = in.sniperInventory || (inventory & (1U << weapon)) != 0;
	}
	const auto &weapon = services_->observation_managedBotCurrentWeapons(index);
	constexpr unsigned kKnife = 29;
	in.usingKnife = weapon.available && weapon.active && weapon.weaponId == kKnife;
	in.enemyKnown = actor.enemyReaction.enemy(context).isValid();
	in.lastSawEnemy = actor.enemyReaction.lastSaw(context);
	if (!in.attacking && in.killerEnemy && !in.sniperInventory &&
		(in.disposition == compat::ReactionDisposition::Engage ||
			in.disposition == compat::ReactionDisposition::Opportunity))
	{
		in.victimVisible = killVictimVisible(index, victim, frame, false);
	}
	if (!current())
	{
		return;
	}
	const auto decision = compat::decideFriendDeath(in, services_->service_compatibilityRandomSource());
	if (!current())
	{
		return;
	}
	if (decision.hurrySeconds > 0)
	{
		actor.enemyReaction.hurry(context, decision.hurrySeconds);
	}
	if (decision.action == compat::FriendDeathAction::Retaliate)
	{
		compat::CompatibilityObservation observation{};
		services_->observation_observationAdapter().collectActor(
			self, context.actor, frame, {in.timing.commandSequence, 0, in.timing.fullUpdateSequence}, &observation);
		const bool reloading = observation.weapon.reloadState.isAvailable() &&
				observation.weapon.reloadState.value != combat::ReloadState::Unknown
			? observation.weapon.reloadState.value == combat::ReloadState::Reloading
			: services_->observation_publicObservations().pendingReloadRequest(context.actor, frame, context.now);
		const bool zombie = engine->pfnCVarGetFloat && engine->pfnCVarGetFloat("bot_zombie") != 0;
		const bool mimic = engine->pfnCVarGetFloat && engine->pfnCVarGetFloat("bot_mimic") != 0;
		if (reloading || zombie || mimic || !current() || !killer || !engine->pfnIndexOfEdict)
		{
			return;
		}
		const int slot = engine->pfnIndexOfEdict(killer);
		if (!current() || slot < 1 || slot > 32 || engine->pfnPEntityOfEntIndex(slot) != killer)
		{
			return;
		}
		if (!current())
		{
			return;
		}
		const auto token = services_->service_lifecycle().tokenForSlot(static_cast<std::uint32_t>(slot));
		if (services_->service_lifecycle().slotGeneration(static_cast<std::uint32_t>(slot)) != 0 &&
			!services_->service_lifecycle().isCurrent(token))
		{
			return;
		}
		const world::ActorKey target{
			static_cast<std::uint32_t>(slot), token.slotGeneration == 0 ? 1U : token.slotGeneration};
		const world::EntityKey source{static_cast<std::uint32_t>(slot), static_cast<std::uint32_t>(killerSerial) + 1U};
		const world::WorldPosition point{killer->v.origin.x, killer->v.origin.y, killer->v.origin.z};
		for (float value : {point.x, point.y, point.z})
		{
			if (!std::isfinite(value) || std::fabs(value) > world::WorldLimits::kMaximumCoordinate)
			{
				return;
			}
		}
		actor.enemyReaction.seedRetaliation(context, target, source, point);
		actor.enemyWeaponSource = source;
		observeRecognizedTargetChange(index, context.now);
		(void)actor.managedBotStateMachines.beginAttack(makeManagedBotStateContext(index, before));
		services_->applyNavigationRequest({index, NavigationRequestKind::DestroyPath});
		services_->applyNavigationRequest({index, NavigationRequestKind::ResetProgress});
		return;
	}
	compat::FriendDeathChatterEvent speech{};
	speech.context = context;
	speech.timing = in.timing;
	speech.victimFriendly = true;
	speech.victimHuman = (victim->v.flags & FL_FAKECLIENT) == 0;
	speech.careerKnown = engine->pfnIsCareerMatch != nullptr;
	speech.career = speech.victimHuman && speech.careerKnown && engine->pfnIsCareerMatch() != FALSE;
	if (!current())
	{
		return;
	}
	if (compat::commanderDownEligible(speech))
	{
		const char *verbosity = engine->pfnCVarGetString ? engine->pfnCVarGetString("bot_chatter") : nullptr;
		const auto mode = compat::parseChatterVerbosity(verbosity);
		ensureOutgoingChatterCatalog();
		if (!current())
		{
			return;
		}
		compat::ChatterStatementRequest request{};
		request.source = compat::ChatterScheduleSource::CommanderDown;
		request.topic = compat::ChatterStatementTopic::MyIntention;
		request.phrase = compat::ChatterPhrase::CommanderDown;
		request.created = context.now;
		request.start = context.now;
		request.readyAt = context.now;
		request.expires = context.now + 20.0f;
		actor.outgoingRadioSchedulers.statements().admit(
			request, admission, outgoingChatterCatalog_, mode, actor.confirmedSpeech.scheduledStatement());
	}
	speech.killerSelf = killer == self;
	speech.killerHumanFriendly = killer && (killer->v.flags & FL_FAKECLIENT) == 0 &&
		services_->observation_resolvePlayerTeam(killer) == services_->observation_resolvePlayerTeam(self);
	if (!current())
	{
		return;
	}
	const auto delay = compat::prepareKilledFriend(speech, services_->service_compatibilityRandomSource());
	if (!current())
	{
		return;
	}
	if (delay.valid)
	{
		const char *verbosity = engine->pfnCVarGetString ? engine->pfnCVarGetString("bot_chatter") : nullptr;
		const auto mode = compat::parseChatterVerbosity(verbosity);
		ensureOutgoingChatterCatalog();
		if (!current())
		{
			return;
		}
		compat::ChatterStatementRequest request{};
		request.source = compat::ChatterScheduleSource::KilledFriend;
		request.topic = compat::ChatterStatementTopic::KilledFriend;
		request.phrase = compat::ChatterPhrase::KilledFriend;
		request.created = context.now;
		request.start = context.now + delay.seconds;
		request.readyAt = request.start;
		request.expires = context.now + 2.0f;
		actor.outgoingRadioSchedulers.statements().admit(
			request, admission, outgoingChatterCatalog_, mode, actor.confirmedSpeech.scheduledStatement());
	}
	// Speech callbacks may change our state. Native evaluates these tail branches
	// after chatter admission, without repeating retaliation or its RNG.
	in.state = actor.managedBotStateMachines.state();
	in.attacking = actor.managedBotStateMachines.attackOverlayActive();
	in.enemyKnown = actor.enemyReaction.enemy(context).isValid();
	in.lastSawEnemy = actor.enemyReaction.lastSaw(context);
	const auto tail = compat::decideFriendDeathTail(in);
	if (tail == compat::FriendDeathAction::RethinkHunt)
	{
		changeReactionState(index, compat::CompatibilityStateId::Idle);
	}
	else if (tail == compat::FriendDeathAction::Reposition)
	{
		actor.enemyReaction.beginPursuit(context, true, in.state);
		changeReactionState(index, compat::CompatibilityStateId::MoveTo, true);
	}
}

void BotBehaviorRuntime::updateEnemyReaction(std::size_t index, const runtime::MovementPhysicsState &before)
{
	auto &actor = actors_[index];
	const auto context = reactionContext(index);
	if (!actor.enemyReaction.update(context) ||
		(!actor.enemyReaction.enemy(context).isValid() && !actor.enemyReaction.pursuing(context)))
	{
		return;
	}
	const auto engine = services_->service_engineFunctions();
	if (actor.enemyReaction.enemyDead(context) && actor.enemyReaction.pursuing(context) &&
		!(actor.enemyReaction.source(context) == actor.enemyReaction.pursuitSource(context)))
	{
		actor.enemyReaction.clearEnemy(false);
	}
	const auto source = actor.enemyReaction.pursuing(context) ? actor.enemyReaction.pursuitSource(context)
															  : actor.enemyReaction.source(context);
	const auto taskEnemy = actor.enemyReaction.pursuing(context) ? actor.enemyReaction.pursuitEnemy(context)
																 : actor.enemyReaction.enemy(context);
	const auto *enemy =
		engine && engine->pfnPEntityOfEntIndex ? engine->pfnPEntityOfEntIndex(static_cast<int>(source.id)) : nullptr;
	if (!reactionEnemyCurrent(taskEnemy) || actor.enemyReaction.enemyDead(context) || !enemy || enemy->free ||
		enemy->serialnumber < 0 || static_cast<std::uint32_t>(enemy->serialnumber) + 1U != source.generation ||
		enemy->v.deadflag != DEAD_NO || !(enemy->v.health > 0))
	{
		const bool pursuing = actor.enemyReaction.pursuing(context);
		if (actor.enemyReaction.source(context) == source)
		{
			actor.enemyReaction.clearEnemy();
		}
		if (pursuing)
		{
			changeReactionState(index, compat::CompatibilityStateId::Idle);
		}
		else if (actor.managedBotStateMachines.attackOverlayActive())
		{
			(void)actor.managedBotStateMachines.stopAttack(makeManagedBotStateContext(index, before));
		}
		return;
	}
	world::WorldSnapshot snapshot;
	world::ActorKey target{};
	world::WorldPosition point{};
	if (!prepareManagedPerception(index, &snapshot, &target, &point))
	{
		return;
	}
	const world::ActorObservation *contact = nullptr;
	const bool visible =
		snapshot.findActor(actor.enemyReaction.enemy(context), &contact) == world::ContactLookupResult::Found &&
		contact && contact->visible;
	if (actor.enemyReaction.initialAttack(context) && !visible)
	{
		actor.enemyReaction.beginPursuit(context, false, actor.managedBotStateMachines.state());
		changeReactionState(index, compat::CompatibilityStateId::MoveTo, true);
	}
	constexpr int kCounterTerroristTeam = 2;
	if (actor.enemyReaction.pursuing(context) &&
		services_->observation_resolvePlayerTeam(services_->service_managedBotHandles()[index].entity) ==
			kCounterTerroristTeam &&
		services_->observation_bombNoticeKnown(index) && services_->observation_bombNoticePlanted(index))
	{
		changeReactionState(index, compat::CompatibilityStateId::Idle);
	}
	(void)before;
}
}
}
