#include "plugin_runtime.hpp"
#include "bot_runtime_support.hpp"
namespace astrabot
{
namespace metamod
{
using namespace runtime_detail;

void BotBehaviorRuntime::updateOutgoingPublicRadio(std::size_t index)
{
	if (index >= services_->service_managedBotHandles().size() ||
		services_->service_globals() == nullptr ||
		services_->service_engineFunctions() == nullptr ||
		services_->service_engineFunctions()->pfnPEntityOfEntIndex == nullptr)
		return;
	const auto &handle = services_->service_managedBotHandles()[index];
	if (handle.entity == nullptr || handle.entity->free)
		return;
	const char *verbosity =
		services_->service_engineFunctions()->pfnCVarGetString != nullptr
			? services_->service_engineFunctions()->pfnCVarGetString("bot_chatter")
			: nullptr;
	if (outgoingRadioMap_ != services_->service_lifecycle().mapGeneration())
	{
		outgoingRadioMap_ = services_->service_lifecycle().mapGeneration();
		outgoingChatterCatalog_ = {};
		outgoingMissingPhraseLogged_.fill(false);
		if (services_->service_engineFunctions()->pfnLoadFileForMe != nullptr &&
			services_->service_engineFunctions()->pfnFreeFile != nullptr)
		{
			int length = 0;
			char filename[] = "BotChatter.db";
			unsigned char *bytes =
				services_->service_engineFunctions()->pfnLoadFileForMe(filename, &length);
			if (bytes != nullptr)
			{
				if (length > 0 && length <= 1024 * 1024)
					(void)outgoingChatterCatalog_.loadText(reinterpret_cast<const char *>(bytes),
														   static_cast<std::size_t>(length));
				services_->service_engineFunctions()->pfnFreeFile(bytes);
			}
		}
	}
	if (outgoingRadioRound_ != services_->service_lifecycle().roundGeneration())
	{
		outgoingRadioRound_ = services_->service_lifecycle().roundGeneration();
		fill_outgoingRadioSchedulers(compat::OutgoingRadioScheduler{});
		fill_outgoingRadioAttempts(0U);
		fill_outgoingRadioAttemptTimes(-1.0f);
		fill_outgoingRadioAttemptChoices(compat::RadioMenuChoice{});
		outgoingTeamLastSpeech_.fill(-1.0f);
		outgoingTeamBusyUntil_.fill(0.0f);
		outgoingTeamCurrentPhrase_.fill(compat::ChatterPhrase::None);
	}
	compat::OutgoingRadioInput input{};
	input.actor = {handle.actor.slot, handle.actor.actorGeneration};
	input.frame = {services_->service_lifecycle().mapGeneration(),
				   services_->service_lifecycle().roundGeneration(),
				   services_->service_adapterFrameCount()};
	input.now = services_->service_globals()->time;
	input.alive = handle.entity->v.deadflag == DEAD_NO && handle.entity->v.health > 0.0f;
	input.team = services_->observation().managedBotTeamNumbers(index);
	if (input.team != 1 && input.team != 2)
		return;
	input.teamLastSpeech = outgoingTeamLastSpeech_[input.team];
	input.teamBusyUntil = outgoingTeamBusyUntil_[input.team];
	input.teamCurrentPhrase = outgoingTeamCurrentPhrase_[input.team];
	input.reactionSeconds = actors_[index].managedProfiles.reactionTimeSeconds;
	input.inCombat = actors_[index].managedBotStateMachines.attackOverlayActive();
	for (int slot = 1; slot <= (std::min)(32, services_->service_globals()->maxClients); ++slot)
	{
		edict_t *player = services_->service_engineFunctions()->pfnPEntityOfEntIndex(slot);
		if (player == nullptr || player == handle.entity || player->free ||
			(player->v.flags & FL_CLIENT) == 0 || player->v.deadflag != DEAD_NO ||
			player->v.health <= 0)
			continue;
		if (player->v.team == input.team)
		{
			++input.friendCount;
		}
		else if (player->v.team == 1 || player->v.team == 2)
			++input.enemiesRemaining;
	}
	// Count the observer's current public FOV/LOS/smoke-filtered contacts. A
	// selected combat target is only one contact and cannot express outnumbering.
	world::WorldSnapshot belief;
	world::ActorKey selected{};
	world::WorldPosition selectedPosition{};
	compat::PublicActorState ownObservation{};
	const bool blind = services_->observation().publicObservations().read(input.actor, input.frame,
																		  &ownObservation) &&
					   services_->service_globals()->time < ownObservation.blindUntil;
	const bool haveBelief = !blind && services_->observation().buildManagedWorldSnapshot(
										  index, &belief, &selected, &selectedPosition);
	const auto contacts = actors_[index].outgoingRadioContactMemory.update(
		input.actor, input.frame, input.now, haveBelief ? &belief : nullptr, blind,
		input.friendCount, input.enemiesRemaining);
	input.visibleEnemyCount = contacts.enemies;
	input.nearbyFriends = contacts.friends;
	const auto &order = services_->observation().scenarioRadio(index);
	const auto &decision = actors_[index].scenarioDecisions;
	if (order.senderHuman && static_cast<int>(order.team) == input.team)
	{
		input.humanOrderSequence = order.sequence;
		input.humanOrderReceived = order.receivedTime;
		input.humanOrderAccepted = decision.action == compat::ScenarioAction::Follow ||
								   decision.action == compat::ScenarioAction::Guard ||
								   decision.action == compat::ScenarioAction::Hunt ||
								   decision.action == compat::ScenarioAction::Escape;
		input.humanOrderHelp = order.command == compat::ScenarioRadioCommand::NeedBackup;
		outgoingTeamLastSpeech_[input.team] =
			(std::max)(outgoingTeamLastSpeech_[input.team], order.receivedTime);
	}
	const char *mapName = services_->service_engineFunctions()->pfnSzFromIndex != nullptr
							  ? services_->service_engineFunctions()->pfnSzFromIndex(
									services_->service_globals()->mapname)
							  : nullptr;
	input.vipEscaping = mapName != nullptr && std::strncmp(mapName, "as_", 3U) == 0 &&
						input.team == 2 && decision.action == compat::ScenarioAction::Escape;
	input.plantingBomb =
		actors_[index].managedBotStateMachines.state() == compat::CompatibilityStateId::PlantBomb;
	input.escortingHostages = decision.escortInferred;
	input.bombLocationUnknown =
		input.team == 2 && services_->observation().bombNoticeKnown(index) &&
		services_->observation().bombNoticePlanted(index) &&
		decision.action == compat::ScenarioAction::Move && decision.hasTarget &&
		actors_[index].bombSearchTargets.knowledge != compat::ScenarioKnowledge::Unknown &&
		actors_[index].bombSearchTargets.frame.mapGeneration == input.frame.mapGeneration &&
		actors_[index].bombSearchTargets.frame.roundGeneration == input.frame.roundGeneration &&
		decision.target.entity == actors_[index].bombSearchTargets.entity;
	const auto output =
		actors_[index].outgoingRadioSchedulers.update(input, outgoingChatterCatalog_);
	if (verbosity != nullptr && std::strcmp(verbosity, "off") == 0 &&
		output.phrase != compat::ChatterPhrase::None)
	{
		actors_[index].outgoingRadioSchedulers.discardPending();
		return;
	}
	if (output.status == compat::OutgoingRadioStatus::MissingPhrase)
	{
		const auto phrase = static_cast<unsigned>(output.phrase);
		if (phrase < outgoingMissingPhraseLogged_.size() && !outgoingMissingPhraseLogged_[phrase])
		{
			outgoingMissingPhraseLogged_[phrase] = true;
			if (gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr &&
				services_->service_pluginId() != nullptr)
				gpMetaUtilFuncs->pfnLogConsole(services_->service_pluginId(),
											   "outgoing chatter unavailable phrase=%s "
											   "host_radio_mapping=%d voice_bank=unavailable",
											   compat::ChatterRadioCatalog::name(output.phrase),
											   outgoingChatterCatalog_.loaded() ? 1 : 0);
		}
		return;
	}
	if (output.status != compat::OutgoingRadioStatus::Ready)
		return;
	if (verbosity != nullptr && std::strcmp(verbosity, "minimal") == 0 &&
		output.phrase != compat::ChatterPhrase::None &&
		!outgoingChatterCatalog_.important(output.phrase))
	{
		actors_[index].outgoingRadioSchedulers.discardPending();
		return;
	}
	const char *menu = output.menu == 1 ? "radio1" : output.menu == 2 ? "radio2" : "radio3";
	if (actors_[index].outgoingRadioAttemptGenerations != handle.actor.actorGeneration ||
		actors_[index].outgoingRadioAttemptChoices.menu != output.menu ||
		actors_[index].outgoingRadioAttemptChoices.slot != output.slot)
	{
		actors_[index].outgoingRadioAttemptGenerations = handle.actor.actorGeneration;
		actors_[index].outgoingRadioAttemptChoices = {output.menu, output.slot};
		actors_[index].outgoingRadioAttempts = 0U;
		actors_[index].outgoingRadioAttemptTimes = -1.0f;
	}
	if (actors_[index].outgoingRadioAttempts >= 3U)
	{
		actors_[index].outgoingRadioSchedulers.discardPending();
		actors_[index].outgoingRadioAttemptChoices = {};
		if (gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr &&
			services_->service_pluginId() != nullptr)
			gpMetaUtilFuncs->pfnLogConsole(
				services_->service_pluginId(),
				"outgoing radio receipt unavailable bot=%u menu=%d slot=%d attempts=3",
				static_cast<unsigned>(handle.actor.slot), output.menu, output.slot);
		return;
	}
	if (actors_[index].outgoingRadioAttemptTimes >= 0.0f &&
		services_->service_globals()->time - actors_[index].outgoingRadioAttemptTimes < 0.2f)
		return;
	actors_[index].outgoingRadioAttemptTimes = services_->service_globals()->time;
	++actors_[index].outgoingRadioAttempts;
	const std::uint32_t receiptBefore = actors_[index].outgoingRadioReceipts;
	char slot[] = {static_cast<char>('0' + output.slot), '\0'};
	const bool opened = services_->command().dispatchClientCommand(handle.entity, menu, nullptr);
	const bool menuSelected =
		opened && services_->command().dispatchClientCommand(handle.entity, "menuselect", slot);
	if (opened)
		(void)services_->command().dispatchClientCommand(handle.entity, "menuselect", "10");
	// A client command reaching the GameDLL does not prove that its radio
	// quota/cooldown accepted it. Only public SendAudio confirms a sent phrase.
	if (menuSelected && actors_[index].outgoingRadioReceipts != receiptBefore)
	{
		actors_[index].outgoingRadioSchedulers.confirmSent(input, output);
		actors_[index].outgoingRadioAttempts = 0U;
		actors_[index].outgoingRadioAttemptChoices = {};
		outgoingTeamLastSpeech_[input.team] = services_->service_globals()->time;
		outgoingTeamBusyUntil_[input.team] = services_->service_globals()->time + 2.1f;
		outgoingTeamCurrentPhrase_[input.team] = output.phrase;
	}
}

void BotBehaviorRuntime::updatePublicBuying(std::size_t index)
{
	if (index >= services_->service_managedBotHandles().size() ||
		services_->service_globals() == nullptr)
		return;
	const FakeClientHandle &handle = services_->service_managedBotHandles()[index];
	if (handle.entity == nullptr || handle.entity->free || handle.entity->v.deadflag != DEAD_NO)
		return;
	compat::PublicActorState observation{};
	if (!services_->observation().publicObservations().read(
			{handle.actor.slot, handle.actor.actorGeneration},
			{services_->service_lifecycle().mapGeneration(),
			 services_->service_lifecycle().roundGeneration(),
			 services_->service_adapterFrameCount()},
			&observation))
		return;
	compat::BuyContext context{};
	context.nowSeconds = services_->service_globals()->time;
	context.stateEnteredSeconds = services_->service_roundFreezeStartTime();
	context.generation = handle.actor.actorGeneration;
	context.round = services_->service_lifecycle().roundGeneration();
	context.alive = true;
	context.inBuyZone = observation.buyZoneKnown && observation.inBuyZone;
	const float buyTime = readOptionalCvarFloat(services_->service_engineFunctions(), "mp_buytime");
	context.buyTimeOpen =
		std::isfinite(buyTime) && services_->service_globals()->time <=
									  services_->service_roundFreezeStartTime() + buyTime * 60.0f;
	context.team = services_->observation().managedBotTeamNumbers(index) == 1U
					   ? compat::ProfileTeam::Terrorist
				   : services_->observation().managedBotTeamNumbers(index) == 2U
					   ? compat::ProfileTeam::CounterTerrorist
					   : compat::ProfileTeam::Any;
	context.moneyKnown = observation.moneyKnown;
	context.money = observation.money;
	context.weaponMaskKnown = true;
	context.weaponMask = static_cast<std::uint32_t>(handle.entity->v.weapons);
	const auto allowed = [this](const char *name) {
		if (services_->service_engineFunctions() == nullptr ||
			services_->service_engineFunctions()->pfnCVarGetPointer == nullptr ||
			services_->service_engineFunctions()->pfnCVarGetPointer(name) == nullptr)
			return true;
		const float value = readOptionalCvarFloat(services_->service_engineFunctions(), name);
		return !std::isfinite(value) || value != 0.0f;
	};
	context.allowRifles = allowed("bot_allow_rifles");
	context.allowSnipers = allowed("bot_allow_snipers");
	context.allowShotguns = allowed("bot_allow_shotguns");
	context.allowMachineGuns = allowed("bot_allow_machine_guns");
	context.allowSubMachineGuns = allowed("bot_allow_sub_machine_guns");
	context.allowPistols = allowed("bot_allow_pistols");
	context.allowGrenades = allowed("bot_allow_grenades");
	context.allowArmor = true;
	context.armorKnown = std::isfinite(handle.entity->v.armorvalue);
	context.armor = static_cast<int>(handle.entity->v.armorvalue);
	context.helmetKnown = observation.helmetKnown;
	context.hasHelmet = observation.hasHelmet;
	context.allowDefuseKit = true;
	context.defuseKitKnown = observation.defuseKitKnown;
	context.hasDefuseKit = observation.hasDefuseKit;
	context.random = &services_->service_compatibilityRandomSource();
	context.randomActor = {handle.actor.slot, handle.actor.actorGeneration};
	context.randomTiming = {services_->command().managedBotCommandSequences(index), 0U,
							services_->service_managedBotFullUpdateSequences()[index]};
	context.bombScenario = managedObjectiveSiteRegistry_.registeredCount != 0U;
	const compat::PublicWeaponDefinition weapon =
		services_->observation().publicObservations().definition(observation.weaponId);
	context.ammoKnown = observation.weaponKnown && weapon.known && weapon.ammoIndex >= 0 &&
						observation.ammoKnown[static_cast<std::size_t>(weapon.ammoIndex)];
	if (context.ammoKnown)
	{
		const bool needsAmmo =
			observation.ammo[static_cast<std::size_t>(weapon.ammoIndex)] < weapon.maxAmmo;
		context.needsPrimaryAmmo = weapon.slot == 0 && needsAmmo;
		context.needsSecondaryAmmo = weapon.slot == 1 && needsAmmo;
	}
	const compat::BuyDecision decision =
		actors_[index].buyControllers.update(context, actors_[index].managedProfiles);
	actors_[index].buyingComplete = decision.status == compat::BuyStatus::Complete;
	if (decision.status != compat::BuyStatus::ReceiptTimeout)
		actors_[index].buyReceiptTimeoutLogged = false;
	if (decision.status == compat::BuyStatus::ReceiptTimeout &&
		!actors_[index].buyReceiptTimeoutLogged && gpMetaUtilFuncs != nullptr &&
		gpMetaUtilFuncs->pfnLogConsole != nullptr && services_->service_pluginId() != nullptr)
	{
		actors_[index].buyReceiptTimeoutLogged = true;
		profiledLogConsole(
			services_->service_runtimeProfiler(), services_->service_pluginId(),
			"buy receipt unconfirmed actor=%u map=%u round=%u; equipment remains unknown",
			static_cast<unsigned int>(handle.actor.slot),
			services_->service_lifecycle().mapGeneration(),
			services_->service_lifecycle().roundGeneration());
	}
	if (decision.status == compat::BuyStatus::Command && decision.command != nullptr)
		(void)services_->command().dispatchClientCommand(handle.entity, decision.command, "");
}

void BotBehaviorRuntime::finishManagedBotNavigation(std::size_t index,
													const runtime::MovementPhysicsState &before,
													runtime::NavRoamResult result,
													const runtime::NavRoamDecision &decision,
													nav::LocomotionIntent *intent)
{
	if (services_->service_compatibilitySurface().configuration().mode !=
			compat::RuntimeMode::Compatibility ||
		index >= services_->service_managedBotHandles().size() ||
		services_->service_globals() == nullptr)
		return;
	compat::StateBehaviorInput input{};
	auto context = makeManagedBotStateContext(index, before, &input);
	const auto &prepared = actors_[index].managedStateInputs;
	if (!(prepared.navigation.actor == context.actor) ||
		prepared.navigation.frame.tick != context.frame.tick ||
		prepared.navigation.frame.mapGeneration != context.frame.mapGeneration ||
		prepared.navigation.frame.roundGeneration != context.frame.roundGeneration)
		return;
	input = prepared;
	compat::NavigationStep step{};
	step.actor = context.actor;
	step.frame = context.frame;
	step.goalGeneration = actors_[index].managedBotObjectiveTargets.generation;
	step.pathSequence = decision.pathSequence;
	step.fullUpdateSequence = context.fullUpdateSequence;
	using Outcome = compat::NavigationStepOutcome;
	if (result == runtime::NavRoamResult::IntentReady ||
		result == runtime::NavRoamResult::MovementSuppressed ||
		(result == runtime::NavRoamResult::ReplanRequired &&
		 decision.failureReason == runtime::NavFailureReason::None))
		step.outcome = Outcome::Progressing;
	else if (result == runtime::NavRoamResult::TargetReached)
		step.outcome = Outcome::EndOfPath;
	else if (result == runtime::NavRoamResult::NoRoute ||
			 result == runtime::NavRoamResult::ReplanRequired)
		step.outcome = Outcome::PathFailure;
	else
		step.outcome = Outcome::ObservationUnavailable;
	const auto currentState = actors_[index].managedBotStateMachines.state();
	if (currentState == compat::CompatibilityStateId::DefuseBomb ||
		currentState == compat::CompatibilityStateId::PlantBomb ||
		currentState == compat::CompatibilityStateId::Buy)
		step.outcome = Outcome::NotEvaluated;
	step.pathDestroyedAfterMovement =
		step.outcome == Outcome::Progressing &&
		decision.recomputeReason == runtime::NavRecomputeReason::PathInvalidated;
	input.navigation = step;
	if (currentState == compat::CompatibilityStateId::EscapeFromBomb &&
		(step.outcome == Outcome::EndOfPath || step.outcome == Outcome::PathFailure) &&
		actors_[index].scenarioEscapeAreas != 0U)
	{
		if (services_->service_runtimeProfiler().enabled())
			profiledLogConsole(
				services_->service_runtimeProfiler(), services_->service_pluginId(),
				"profile bombEscapeRetry bot_id=%u frame=%u previous_area=%u result=%d reason=%d",
				context.actor.slot, context.frame.tick, actors_[index].scenarioEscapeAreas,
				static_cast<int>(result), static_cast<int>(decision.recomputeReason));
		actors_[index].scenarioEscapeAreas = 0U;
	}
	const auto &target = actors_[index].scenarioDecisions.target;
	const auto &bomb = services_->observation().objectiveKnowledge(index)[1U];
	input.defuseGoalKnown = services_->observation().publicPlayerTeam(
								services_->service_managedBotHandles()[index].entity) == 2 &&
							actors_[index].scenarioDecisions.hasTarget && bomb.entity.id != 0U &&
							target.entity.id == bomb.entity.id;
	if (input.defuseGoalKnown)
	{
		const auto *entity = services_->service_managedBotHandles()[index].entity;
		const float footOffset =
			entity->v.mins.z < 0.0f ? entity->v.mins.z : -kStandingHalfHumanHeight;
		const float x = bomb.position.x - before.origin.x, y = bomb.position.y - before.origin.y,
					z = bomb.position.z - before.origin.z - footOffset;
		input.withinDefuseRange = x * x + y * y + z * z < 100.0f * 100.0f;
	}
	compat::CompatibilityObservation observation{};
	const compat::ObservationTimingContext timing = {
		services_->command().managedBotCommandSequences(index), 0U, context.fullUpdateSequence};
	if (services_->observation().observationAdapter().collectActor(
			services_->service_managedBotHandles()[index].entity, context.actor, context.frame,
			timing, &observation) == ObservationAdapterResult::Accepted &&
		observation.weapon.reloadState.isAvailable() &&
		observation.weapon.reloadState.value != combat::ReloadState::Unknown)
	{
		input.reloadKnown = true;
		input.reloading = observation.weapon.reloadState.value == combat::ReloadState::Reloading;
	}
	else
	{
		// Vanilla cannot expose the internal reload flag. Own delivered reload
		// requests conservatively block use; no pending request is an inference.
		input.reloadInferred = true;
		input.reloading = services_->observation().publicObservations().pendingReloadRequest(
			context.actor, context.frame, context.timestamp);
	}
	auto &machine = actors_[index].managedBotStateMachines;
	const auto previous = machine.state();
	(void)machine.finishNavigation(context);
	if (!machine.attackOverlayActive())
		services_->applyNavigationRequest({index, NavigationRequestKind::PublishStep, step});
	if (machine.state() != previous)
	{
		services_->applyNavigationRequest({index, NavigationRequestKind::DestroyPath});
		if (machine.state() == compat::CompatibilityStateId::Idle)
		{
			actors_[index].managedBotObjectiveTargets = {};
			actors_[index].scenarioDecisions = {};
			if (services_->service_runtimeProfiler().enabled())
				profiledLogConsole(
					services_->service_runtimeProfiler(), services_->service_pluginId(),
					"profile navigationFailure bot_id=%u frame=%u state_from=%s state_to=%s",
					context.actor.slot, context.frame.tick, compat::toString(previous),
					compat::toString(machine.state()));
		}
	}
	if (machine.state() == compat::CompatibilityStateId::DefuseBomb && intent != nullptr)
		*intent = {};
	if (services_->service_runtimeProfiler().enabled())
		profiledLogConsole(
			services_->service_runtimeProfiler(), services_->service_pluginId(),
			"profile navigationStep bot_id=%u frame=%u update=%u goal_generation=%u path=%u "
			"outcome=%u destroyed=%d state_from=%s state_to=%s reload_known=%d reload_inferred=%d "
			"goal_position=(%.1f %.1f %.1f) failure_reason=%d current_area=%u goal_area=%u "
			"path_link=(%u->%u)",
			context.actor.slot, context.frame.tick, context.fullUpdateSequence, step.goalGeneration,
			step.pathSequence, static_cast<unsigned>(step.outcome),
			step.pathDestroyedAfterMovement ? 1 : 0, compat::toString(previous),
			compat::toString(machine.state()), input.reloadKnown ? 1 : 0,
			input.reloadInferred ? 1 : 0, decision.goalPosition.x, decision.goalPosition.y,
			decision.goalPosition.z, static_cast<int>(decision.failureReason), decision.currentArea,
			decision.goalArea, decision.linkFromArea, decision.linkToArea);
}

void BotBehaviorRuntime::refreshManagedObjectiveSiteRegistry()
{
	const runtime::LifecycleGeneration mapGeneration =
		services_->service_lifecycle().mapGeneration();
	const runtime::LifecycleGeneration roundGeneration =
		services_->service_lifecycle().roundGeneration();
	if (managedObjectiveSiteRegistry_.initialized &&
		managedObjectiveSiteRegistry_.mapGeneration == mapGeneration &&
		managedObjectiveSiteRegistry_.roundGeneration == roundGeneration)
	{
		return;
	}
	managedObjectiveSiteRegistry_ = {};
	managedObjectiveSiteRegistry_.initialized = true;
	managedObjectiveSiteRegistry_.mapGeneration = mapGeneration;
	managedObjectiveSiteRegistry_.roundGeneration = roundGeneration;
	if (services_->service_engineFunctions() == nullptr ||
		services_->service_globals() == nullptr ||
		services_->service_engineFunctions()->pfnPEntityOfEntIndex == nullptr ||
		services_->service_engineFunctions()->pfnSzFromIndex == nullptr)
	{
		return;
	}
	const int maxEntities =
		(std::max)(0, (std::min)(services_->service_globals()->maxEntities, 2048));
	for (int entityIndex = 1; entityIndex <= maxEntities; ++entityIndex)
	{
		if (managedObjectiveSiteRegistry_.registeredCount >=
			ManagedObjectiveSiteRegistry::kMaximumSites)
		{
			break;
		}
		edict_t *entity = services_->service_engineFunctions()->pfnPEntityOfEntIndex(entityIndex);
		if (entity == nullptr || entity->free)
		{
			continue;
		}
		const char *classname =
			services_->service_engineFunctions()->pfnSzFromIndex(entity->v.classname);
		if (!isBombTargetClassname(classname))
		{
			continue;
		}
		if (equalsIgnoreCase(classname, "func_bomb_target"))
		{
			++managedObjectiveSiteRegistry_.registeredFuncBombTargetCount;
		}
		else if (equalsIgnoreCase(classname, "info_bomb_target"))
		{
			++managedObjectiveSiteRegistry_.registeredInfoBombTargetCount;
		}
		ManagedObjectiveSiteRegistry::Entry &entry =
			managedObjectiveSiteRegistry_.entries[managedObjectiveSiteRegistry_.registeredCount];
		entry.registered = true;
		entry.targetId = managedObjectiveSiteRegistry_.registeredCount + 1U;
		entry.entityIndex = entityIndex;
		entry.siteIdentity = entry.targetId;
		entry.center = entityObjectiveCenter(entity);
		entry.hasExtent = objectiveSiteExtent(entity, classname, &entry.extent);
		++managedObjectiveSiteRegistry_.registeredCount;
	}
}

void BotBehaviorRuntime::updateBombGuardTarget(std::size_t index,
											   const compat::ScenarioBehaviorInput &input,
											   compat::ScenarioBehaviorDecision *decision)
{
	RuntimeProfilerScope guardScope(services_->service_runtimeProfiler(),
									RuntimeProfilerStage::BombGuardSelection);
	if (index >= NativeBotObservation::kClientSlotCount || decision == nullptr)
		return;
	if (!input.bombPlantedKnown || !input.bombPlanted || !decision->hasTarget ||
		decision->action != compat::ScenarioAction::Guard ||
		!(decision->target.entity == input.bomb.entity))
	{
		actors_[index].bombGuardPlanners.reset();
		return;
	}
	const auto &handle = services_->service_managedBotHandles()[index];
	if (handle.entity == nullptr || services_->service_engineFunctions() == nullptr ||
		services_->service_globals() == nullptr)
		return;
	const nav::NavVector self = {handle.entity->v.origin.x, handle.entity->v.origin.y,
								 handle.entity->v.origin.z + handle.entity->v.mins.z};
	std::vector<nav::NavVector> occupied;
	compat::PublicActorState weapon{};
	const bool publicStateKnown =
		services_->observation().publicObservations().read(input.actor, input.frame, &weapon);
	const bool blind = publicStateKnown && input.timestamp < weapon.blindUntil;
	if (!blind && services_->service_engineFunctions()->pfnPEntityOfEntIndex != nullptr)
	{
		for (int slot = 1; slot <= (std::min)(32, services_->service_globals()->maxClients); ++slot)
		{
			edict_t *other = services_->service_engineFunctions()->pfnPEntityOfEntIndex(slot);
			if (other == nullptr || other == handle.entity || other->free ||
				(other->v.flags & FL_CLIENT) == 0 || other->v.deadflag != DEAD_NO ||
				other->v.health <= 0.0f)
				continue;
			const float eye[] = {handle.entity->v.origin.x + handle.entity->v.view_ofs.x,
								 handle.entity->v.origin.y + handle.entity->v.view_ofs.y,
								 handle.entity->v.origin.z + handle.entity->v.view_ofs.z};
			const float point[] = {other->v.origin.x, other->v.origin.y, other->v.origin.z};
			const float dx = point[0] - eye[0], dy = point[1] - eye[1];
			const float length = std::hypot(dx, dy);
			const float yaw = handle.entity->v.v_angle.y * 0.017453292519943295f;
			if (length > 75.0f && (dx * std::cos(yaw) + dy * std::sin(yaw)) < 0.5f * length)
				continue;
			if (services_->service_engineFunctions()->pfnTraceLine == nullptr)
				continue;
			TraceResult trace{};
			services_->service_engineFunctions()->pfnTraceLine(eye, point, 1, handle.entity,
															   &trace);
			if (trace.flFraction != 1.0f && trace.pHit != other)
				continue;
			occupied.push_back({point[0], point[1], point[2] + other->v.mins.z});
		}
	}
	const bool sniper = publicStateKnown && weapon.weaponKnown &&
						(weapon.weaponId == 3 || weapon.weaponId == 13 || weapon.weaponId == 18 ||
						 weapon.weaponId == 24);
	nav::NavVector spot{};
	const auto navigation = services_->service_navPublisher().snapshot();
	const nav::NavVector bombPosition = {input.bomb.position.x, input.bomb.position.y,
										 input.bomb.position.z};
	// The active guard target is checked before constructing the danger/team
	// path-cost context. Retained choices need neither a new route nor new costs.
	nav::NavPathCostContext cost{};
	if (actors_[index].bombGuardPlanners.needsSelection(navigation, input.frame, input.bomb.entity,
														bombPosition, self, occupied,
														input.timestamp))
		cost = services_->navigation().managedPathCostContext(index);
	if (!actors_[index].bombGuardPlanners.update(
			navigation, input.frame, input.bomb.entity,
			{input.bomb.position.x, input.bomb.position.y, input.bomb.position.z}, self, sniper,
			occupied, input.timestamp, services_->service_compatibilityRandomSource(),
			{input.actor.slot, input.actor.generation},
			{services_->command().managedBotCommandSequences(index), 0U,
			 services_->service_managedBotFullUpdateSequences()[index]},
			&spot, cost))
	{
		// Unknown NAV/RNG does not authorize converging on the C4 as cover.
		spot = self;
	}
	decision->target.position = {spot.x, spot.y, spot.z};
	decision->target.frame = input.frame;
	const float dx = spot.x - self.x, dy = spot.y - self.y, dz = spot.z - self.z;
	decision->target.pathCost = std::sqrt(dx * dx + dy * dy + dz * dz);
	decision->target.inUseRange = decision->target.pathCost <= 20.0f;
}

void BotBehaviorRuntime::refreshManagedTeamObjectiveAssignments(std::size_t contextIndex)
{
	const bool compatibility = services_->service_compatibilitySurface().configuration().mode ==
							   compat::RuntimeMode::Compatibility;
	const std::uint32_t mapGeneration = services_->service_lifecycle().mapGeneration();
	const std::uint32_t roundGeneration = services_->service_lifecycle().roundGeneration();
	const world::FrameIdentity frame = {mapGeneration, roundGeneration,
										services_->service_adapterFrameCount()};
	if (managedTeamObjectiveAssignmentsValid_ &&
		(!compatibility || managedTeamObjectiveAssignmentContext_ == contextIndex) &&
		managedTeamObjectiveAssignmentMapGeneration_ == mapGeneration &&
		managedTeamObjectiveAssignmentRoundGeneration_ == roundGeneration &&
		managedTeamObjectiveAssignmentFrame_ == services_->service_adapterFrameCount())
	{
		return;
	}

	managedTeamObjectiveAssignments_ = {};
	managedTeamObjectiveAssignmentsValid_ = false;
	if (contextIndex >= services_->service_managedBotHandles().size() ||
		services_->service_engineFunctions() == nullptr ||
		services_->service_engineFunctions()->pfnPEntityOfEntIndex == nullptr ||
		services_->service_engineFunctions()->pfnSzFromIndex == nullptr ||
		services_->service_globals() == nullptr || !frame.isValid())
	{
		return;
	}

	const FakeClientHandle &contextHandle = services_->service_managedBotHandles()[contextIndex];
	managedTeamObjectiveAssignmentContext_ = contextIndex;
	team::RoundObjectiveCoordinator &coordinator =
		compatibility ? actors_[contextIndex].compatibilityObjectiveCoordinators
					  : managedTeamObjectiveCoordinator_;
	const world::ActorKey contextActor = {contextHandle.actor.slot,
										  contextHandle.actor.actorGeneration};
	const compat::ObservationTimingContext timing = {
		services_->command().managedBotCommandSequences(contextIndex), 0U, 0U};
	team::TeamObjectiveInput input = {};
	input.mapGeneration = mapGeneration;
	input.roundGeneration = roundGeneration;
	input.frameSequence = services_->service_adapterFrameCount();
	const nav::NavSnapshot navigation = services_->service_navPublisher().snapshot();
	const nav::NavDocument *document = navigation.document();
	nav::NavQuery query(navigation);
	if (compatibility)
	{
		query.setPathCostContext(services_->navigation().managedPathCostContext(contextIndex));
	}

	for (std::size_t index = 0U; index < services_->service_managedBotHandles().size() &&
								 input.actorCount < team::kMaximumTeamObjectiveActors;
		 ++index)
	{
		if (!services_->service_managedBotSlots()[index] ||
			services_->service_managedBotHandles()[index].entity == nullptr ||
			services_->service_managedBotHandles()[index].entity->free != 0 ||
			(compatibility && index != contextIndex))
		{
			continue;
		}
		const FakeClientHandle &handle = services_->service_managedBotHandles()[index];
		compat::CompatibilityObservation observation = {};
		const compat::ObservationTimingContext actorTiming = {
			services_->command().managedBotCommandSequences(index), 0U, 0U};
		if (services_->observation().observationAdapter().collectActor(
				handle.entity, {handle.actor.slot, handle.actor.actorGeneration}, frame,
				actorTiming, &observation) != ObservationAdapterResult::Accepted)
		{
			continue;
		}

		team::TeamObjectiveActorObservation &actor = input.actors[input.actorCount];
		actor = {};
		actor.actor = {handle.actor.slot, handle.actor.actorGeneration};
		const bool teamInfoConfirmed =
			services_->service_joinControllers()[index].teamConfirmed() &&
			services_->observation().managedBotTeamNumbers(index) >= 1U &&
			services_->observation().managedBotTeamNumbers(index) <= 2U;
		actor.team = !teamInfoConfirmed ? objectives::TeamRole::Unknown
					 : services_->observation().managedBotTeamNumbers(index) == 1U
						 ? objectives::TeamRole::Terrorist
						 : objectives::TeamRole::CounterTerrorist;
		actor.alive = observation.player.deadflag.isAvailable() &&
					  observation.player.deadflag.value == DEAD_NO;
		actor.carryingC4 = observation.objective.carryingC4.isAvailable() &&
						   observation.objective.carryingC4.value;
		actor.positionAvailable = observation.player.origin.isAvailable();
		if (actor.positionAvailable)
		{
			actor.position = observation.player.origin.value;
			if (navigation.isValid() && document != nullptr)
			{
				nav::NavAreaMatch area = {};
				const nav::NavVector position = {actor.position.x, actor.position.y,
												 actor.position.z};
				if (query.findContaining(position, 64.0f, &area) != nav::NavQueryResult::Found &&
					query.findNearest(position, kMaximumObjectiveDistance, &area) !=
						nav::NavQueryResult::Found)
				{
					area.area = 0U;
				}
				actor.currentArea = area.area;
			}
		}
		++input.actorCount;
	}

	const int maximumEntities =
		(std::max)(0, (std::min)(services_->service_globals()->maxEntities, 2048));
	compat::DroppedC4Observation droppedObservation = {};
	compat::ObjectiveObservation plantedObservation = {};
	for (int entityIndex = 1; entityIndex <= maximumEntities; ++entityIndex)
	{
		edict_t *entity = services_->service_engineFunctions()->pfnPEntityOfEntIndex(entityIndex);
		if (entity == nullptr || entity->free != 0)
		{
			continue;
		}
		const char *classname =
			services_->service_engineFunctions()->pfnSzFromIndex(entity->v.classname);
		const char *model = services_->service_engineFunctions()->pfnSzFromIndex(entity->v.model);
		if (!input.droppedC4.available)
		{
			compat::DroppedC4Observation candidate = {};
			if (services_->observation().observationAdapter().collectDroppedC4(
					entity, classname, model, contextActor, frame, timing, &candidate) ==
					ObservationAdapterResult::Accepted &&
				candidate.isCurrent(frame))
			{
				droppedObservation = candidate;
				input.droppedC4.available = true;
				input.droppedC4.entity = candidate.entity;
				world::WorldVector knownPosition{};
				if (!services_->observation().observeObjective(contextIndex, entity, false,
															   &knownPosition))
				{
					input.droppedC4 = {};
					continue;
				}
				input.droppedC4.position = knownPosition;
			}
		}
		if (!input.plantedC4.available &&
			services_->observation().observationAdapter().collectPlantedBomb(
				entity, classname, model, services_->service_globals()->time, contextActor, frame,
				timing, &plantedObservation) == ObservationAdapterResult::Accepted &&
			plantedObservation.bombPosition.isCurrent(frame) &&
			services_->service_engineFunctions()->pfnIndexOfEdict != nullptr &&
			entity->serialnumber != 0)
		{
			const int plantedIndex = services_->service_engineFunctions()->pfnIndexOfEdict(entity);
			if (plantedIndex > 0)
			{
				input.plantedC4.available = true;
				input.plantedC4.entity = {static_cast<std::uint32_t>(plantedIndex),
										  static_cast<std::uint32_t>(entity->serialnumber)};
				world::WorldVector knownPosition{};
				if (!services_->observation().observeObjective(contextIndex, entity, true,
															   &knownPosition))
				{
					input.plantedC4 = {};
					continue;
				}
				input.plantedC4.position = knownPosition;
			}
		}
	}

	if (navigation.isValid() && document != nullptr)
	{
		for (team::TeamBombTargetObservation *target : {&input.droppedC4, &input.plantedC4})
		{
			if (!target->available)
			{
				continue;
			}
			nav::NavAreaMatch area = {};
			if (query.findNearest({target->position.x, target->position.y, target->position.z},
								  kMaximumObjectiveDistance, &area) == nav::NavQueryResult::Found)
			{
				target->area = area.area;
			}
		}
	}

	if (managedTeamObjectivePathFailurePending_)
	{
		coordinator.reportPathFailure(
			managedTeamObjectivePathFailureActor_, managedTeamObjectivePathFailureGeneration_,
			managedTeamObjectivePathFailureStartArea_, managedTeamObjectivePathFailureFrame_);
		managedTeamObjectivePathFailurePending_ = false;
		managedTeamObjectivePathFailureActor_ = {};
		managedTeamObjectivePathFailureGeneration_ = 0U;
		managedTeamObjectivePathFailureStartArea_ = 0U;
		managedTeamObjectivePathFailureFrame_ = 0U;
	}
	const bool externalDefuserActive =
		services_->observation().externalDefuserEntityIndex() != 0U &&
		services_->observation().externalDefuserMapGeneration() == mapGeneration &&
		services_->observation().externalDefuserRoundGeneration() == roundGeneration &&
		std::isfinite(services_->service_globals()->time) &&
		std::isfinite(services_->observation().externalDefuserLeaseUntil()) &&
		services_->service_globals()->time <= services_->observation().externalDefuserLeaseUntil();
	input.externalDefuserActive = externalDefuserActive;
	(void)coordinator.assign(input, navigation, &managedTeamObjectiveAssignments_);
	if (services_->service_runtimeProfiler().enabled() && gpMetaUtilFuncs != nullptr &&
		gpMetaUtilFuncs->pfnLogConsole != nullptr && services_->service_pluginId() != nullptr)
	{
		for (std::size_t index = 0U; index < managedTeamObjectiveAssignments_.count; ++index)
		{
			const team::TeamObjectiveAssignment &assignment =
				managedTeamObjectiveAssignments_.assignments[index];
			profiledLogConsole(
				services_->service_runtimeProfiler(), services_->service_pluginId(),
				"team_objective map=%u round=%u frame=%u actor=%u:%u kind=%u target=%u:%u area=%u "
				"generation=%u cost=%.2f cost_fallback=%u dropped=%u planted=%u",
				static_cast<unsigned int>(mapGeneration),
				static_cast<unsigned int>(roundGeneration),
				static_cast<unsigned int>(services_->service_adapterFrameCount()),
				static_cast<unsigned int>(assignment.actor.slot),
				static_cast<unsigned int>(assignment.actor.generation),
				static_cast<unsigned int>(assignment.kind),
				static_cast<unsigned int>(assignment.targetEntity.id),
				static_cast<unsigned int>(assignment.targetEntity.generation),
				static_cast<unsigned int>(assignment.targetArea),
				static_cast<unsigned int>(assignment.generation),
				assignment.routeCostAvailable ? assignment.routeCost : 0.0f,
				assignment.routeCostGeometricFallback ? 1U : 0U,
				input.droppedC4.available ? 1U : 0U, input.plantedC4.available ? 1U : 0U);
		}
	}
	managedTeamObjectiveAssignmentMapGeneration_ = mapGeneration;
	managedTeamObjectiveAssignmentRoundGeneration_ = roundGeneration;
	managedTeamObjectiveAssignmentFrame_ = services_->service_adapterFrameCount();
	managedTeamObjectiveAssignmentsValid_ = true;
}

const team::TeamObjectiveAssignment *BotBehaviorRuntime::findManagedTeamObjectiveAssignment(
	const world::ActorKey &actor) const
{
	if (!managedTeamObjectiveAssignmentsValid_)
	{
		return nullptr;
	}
	for (std::size_t index = 0U; index < managedTeamObjectiveAssignments_.count; ++index)
	{
		const team::TeamObjectiveAssignment &assignment =
			managedTeamObjectiveAssignments_.assignments[index];
		if (assignment.actor == actor)
		{
			return &assignment;
		}
	}
	return nullptr;
}

float BotBehaviorRuntime::managedRoundRemainingSeconds() const
{
	const float minutes =
		readOptionalCvarFloat(services_->service_engineFunctions(), "mp_roundtime");
	if (services_->service_globals() == nullptr ||
		!std::isfinite(services_->service_globals()->time) || !std::isfinite(minutes) ||
		minutes <= 0.0f ||
		services_->service_roundFreezeGeneration() !=
			services_->service_lifecycle().roundGeneration())
	{
		return std::numeric_limits<float>::quiet_NaN();
	}
	return (std::max)(0.0f, minutes * 60.0f -
								(std::max)(0.0f, services_->service_globals()->time -
													 services_->service_roundFreezeUntil()));
}

bool BotBehaviorRuntime::managedActorAtBombSite(std::size_t index) const
{
	if (index >= services_->service_managedBotHandles().size() ||
		services_->service_managedBotHandles()[index].entity == nullptr ||
		services_->service_engineFunctions() == nullptr ||
		services_->service_engineFunctions()->pfnPEntityOfEntIndex == nullptr ||
		services_->service_engineFunctions()->pfnSzFromIndex == nullptr)
	{
		return false;
	}
	const edict_t *actor = services_->service_managedBotHandles()[index].entity;
	const PlantTargetBounds actorBounds = {actor->v.absmin.x, actor->v.absmin.y, actor->v.absmin.z,
										   actor->v.absmax.x, actor->v.absmax.y, actor->v.absmax.z};
	for (std::size_t siteIndex = 0U; siteIndex < managedObjectiveSiteRegistry_.registeredCount;
		 ++siteIndex)
	{
		const auto &entry = managedObjectiveSiteRegistry_.entries[siteIndex];
		edict_t *site =
			services_->service_engineFunctions()->pfnPEntityOfEntIndex(entry.entityIndex);
		if (site == nullptr || site->free)
			continue;
		const char *classname =
			services_->service_engineFunctions()->pfnSzFromIndex(site->v.classname);
		if (ActionAdapter::isLegacyInfoBombTargetClassname(classname))
		{
			const auto center = entityObjectiveCenter(site);
			if (ActionAdapter::withinLegacyBombTargetRadius(
					{center.x, center.y, center.z},
					{actor->v.origin.x, actor->v.origin.y, actor->v.origin.z}))
				return true;
		}
		else if (isBombTargetClassname(classname))
		{
			const PlantTargetBounds siteBounds = {site->v.absmin.x, site->v.absmin.y,
												  site->v.absmin.z, site->v.absmax.x,
												  site->v.absmax.y, site->v.absmax.z};
			if (siteBounds.maximumX > siteBounds.minimumX &&
				siteBounds.maximumY > siteBounds.minimumY &&
				ActionAdapter::overlapsPlantTarget(siteBounds, actorBounds))
				return true;
		}
	}
	return false;
}

bool BotBehaviorRuntime::buildManagedObjectiveTarget(std::size_t index, nav::NavVector *target,
													 nav::NavSearchStats *searchStats,
													 RuntimeObjectiveSelectionStats *objectiveStats)
{
	if (objectiveStats != nullptr)
	{
		*objectiveStats = {};
	}
	if (index >= services_->service_managedBotHandles().size() || target == nullptr ||
		services_->service_engineFunctions() == nullptr ||
		services_->service_engineFunctions()->pfnPEntityOfEntIndex == nullptr ||
		services_->service_engineFunctions()->pfnSzFromIndex == nullptr ||
		services_->service_globals() == nullptr)
	{
		return false;
	}
	const FakeClientHandle &handle = services_->service_managedBotHandles()[index];
	if (handle.entity == nullptr)
	{
		return false;
	}
	if (services_->service_compatibilitySurface().configuration().mode ==
		compat::RuntimeMode::Compatibility)
	{
		const auto &belief = services_->observation().managedBotPerception(index).snapshot();
		if (actors_[index].managedBotStateMachines.state() ==
				compat::CompatibilityStateId::InvestigateNoise &&
			belief.isValid() &&
			belief.identity().frame.mapGeneration ==
				services_->service_lifecycle().mapGeneration() &&
			belief.identity().frame.roundGeneration ==
				services_->service_lifecycle().roundGeneration() &&
			belief.noise().active && belief.noise().ready)
		{
			const auto &position = belief.noise().position;
			*target = {position.x, position.y, position.z};
			return true;
		}
		const auto &scenario = actors_[index].scenarioDecisions;
		if (scenario.hasTarget && !scenario.unavailable &&
			scenario.target.frame.mapGeneration == services_->service_lifecycle().mapGeneration() &&
			scenario.target.frame.roundGeneration ==
				services_->service_lifecycle().roundGeneration() &&
			scenario.target.frame.tick <= services_->service_adapterFrameCount())
		{
			*target = {scenario.target.position.x, scenario.target.position.y,
					   scenario.target.position.z};
			return true;
		}
	}
	const world::FrameIdentity frame = {services_->service_lifecycle().mapGeneration(),
										services_->service_lifecycle().roundGeneration(),
										services_->service_adapterFrameCount()};
	const world::ActorKey actor = {handle.actor.slot, handle.actor.actorGeneration};
	const compat::ObservationTimingContext observationTiming = {
		services_->command().managedBotCommandSequences(index), 0U, 0U};
	compat::CompatibilityObservation compatObservation = {};
	if (services_->observation().observationAdapter().collectActor(
			handle.entity, actor, frame, observationTiming, &compatObservation) !=
		ObservationAdapterResult::Accepted)
	{
		return false;
	}
	const int rawTeam = compatObservation.player.team.isAvailable()
							? compatObservation.player.team.value
							: static_cast<int>(handle.entity->v.team);
	const int team = resolveManagedBotTeam(index, compatObservation);
	ManagedObjectiveTargetCache &cache = actors_[index].managedBotObjectiveTargets;
	cache.rawTeam = rawTeam;
	cache.effectiveTeam = team;
	cache.teamInfoFresh = services_->service_joinControllers()[index].teamConfirmed() &&
						  services_->observation().managedBotTeamNumbers(index) >= 1U &&
						  services_->observation().managedBotTeamNumbers(index) <= 2U;
	if (team != 1 && team != 2)
	{
		return false;
	}
	const bool carryingBomb = compatObservation.objective.carryingC4.isAvailable() &&
							  compatObservation.objective.carryingC4.value;
	refreshManagedTeamObjectiveAssignments(index);
	const world::ActorKey actorKey = {handle.actor.slot, handle.actor.actorGeneration};
	const team::TeamObjectiveAssignment *assignment = findManagedTeamObjectiveAssignment(actorKey);
	const team::TeamObjectiveKind assignmentKind =
		assignment == nullptr ? team::TeamObjectiveKind::None : assignment->kind;
	const std::uint32_t assignmentGeneration = assignment == nullptr ? 0U : assignment->generation;
	const bool needsDroppedC4 = assignmentKind == team::TeamObjectiveKind::RetrieveDroppedC4;
	const bool needsBombSite =
		team == 1 && carryingBomb && assignmentKind == team::TeamObjectiveKind::PlantC4;
	const bool needsPlantedBomb = team == 2 && assignmentKind == team::TeamObjectiveKind::DefuseC4;
	objectives::BombApproachState &approach = actors_[index].managedBombApproaches;
	if (needsBombSite && services_->service_compatibilitySurface().configuration().mode ==
							 compat::RuntimeMode::Compatibility)
	{
		approach.mapGeneration = frame.mapGeneration;
		approach.roundGeneration = frame.roundGeneration;
		approach.actorSlot = actor.slot;
		approach.actorGeneration = actor.generation;
		approach.initialized = true;
		approach.plantAt = roundPlantDecision_.plantAt;
		approach.finished =
			(roundPlantDecision_.known && services_->service_globals()->time >= approach.plantAt) ||
			(managedActorAtBombSite(index) &&
			 services_->service_globals()->time -
					 services_->observation().lastVisibleEnemyTimes(index) >
				 3.0f);
		if (!approach.finished)
		{
			cache.valid = false;
			return false;
		}
	}
	if (needsBombSite)
	{
		refreshManagedObjectiveSiteRegistry();
		if (objectiveStats != nullptr)
		{
			objectiveStats->registeredSites = managedObjectiveSiteRegistry_.registeredCount;
			objectiveStats->registeredFuncBombTargetSites =
				managedObjectiveSiteRegistry_.registeredFuncBombTargetCount;
			objectiveStats->registeredInfoBombTargetSites =
				managedObjectiveSiteRegistry_.registeredInfoBombTargetCount;
			objectiveStats->registeredSiteCountsAvailable = true;
		}
	}
	if (!needsBombSite)
	{
		approach = {};
	}
	else if (approach.initialized && !approach.finished &&
			 objectives::bombApproachPhase(
				 &approach, services_->service_globals()->time, managedRoundRemainingSeconds(),
				 cache.approachRouteSeconds, carryingBomb, false,
				 managedActorAtBombSite(index)) == objectives::BombApproachPhase::Plant)
	{
		// One phase transition permits a fresh nearest route from the moved position.
		cache.valid = false;
		cache.generation = cache.generation == (std::numeric_limits<std::uint32_t>::max)()
							   ? 1U
							   : cache.generation + 1U;
	}
	const bool sameObjectiveState =
		cache.mapGeneration == services_->service_lifecycle().mapGeneration() &&
		cache.roundGeneration == services_->service_lifecycle().roundGeneration() &&
		cache.team == static_cast<std::uint8_t>(team) && cache.carryingBomb == carryingBomb &&
		cache.teamObjectiveKind == assignmentKind &&
		cache.teamObjectiveAssignmentGeneration == assignmentGeneration;
	const bool diagnosticCacheRefresh = sameObjectiveState && needsBombSite && cache.valid &&
										objectiveStats != nullptr && !cache.diagnosticsEmitted;
	const char *objectiveCacheState = diagnosticCacheRefresh ? "profile_refresh" : "miss";
	if (sameObjectiveState && needsBombSite && cache.valid && !diagnosticCacheRefresh)
	{
		edict_t *cachedSite = cache.selectedEntityIndex > 0
								  ? services_->service_engineFunctions()->pfnPEntityOfEntIndex(
										cache.selectedEntityIndex)
								  : nullptr;
		const char *cachedClassname =
			cachedSite != nullptr && !cachedSite->free
				? services_->service_engineFunctions()->pfnSzFromIndex(cachedSite->v.classname)
				: nullptr;
		if (cachedClassname != nullptr && isBombTargetClassname(cachedClassname))
		{
			*target = cache.target;
			if (objectiveStats != nullptr)
			{
				++objectiveStats->cacheHits;
				objectiveStats->selectedSiteIdentity = cache.selectedSiteIdentity;
			}
			cache.lastCacheHit = true;
			return std::isfinite(target->x) && std::isfinite(target->y) && std::isfinite(target->z);
		}
		cache.valid = false;
		cache.diagnosticsEmitted = false;
		cache.lastCacheHit = false;
		cache.generation = cache.generation == (std::numeric_limits<std::uint32_t>::max)()
							   ? 1U
							   : cache.generation + 1U;
	}
	if (!sameObjectiveState)
	{
		cache.generation = cache.generation == (std::numeric_limits<std::uint32_t>::max)()
							   ? 1U
							   : cache.generation + 1U;
	}
	cache.valid = false;
	cache.selectedEntityIndex = 0;
	cache.selectedSiteIdentity = 0U;
	cache.mapGeneration = services_->service_lifecycle().mapGeneration();
	cache.roundGeneration = services_->service_lifecycle().roundGeneration();
	cache.team = static_cast<std::uint8_t>(team);
	cache.carryingBomb = carryingBomb;
	cache.teamObjectiveKind = assignmentKind;
	cache.teamObjectiveAssignmentGeneration = assignmentGeneration;
	cache.diagnosticsEmitted = false;
	cache.lastCacheHit = false;
	if (needsDroppedC4 && assignment != nullptr)
	{
		*target = {assignment->targetPosition.x, assignment->targetPosition.y,
				   assignment->targetPosition.z};
		return std::isfinite(target->x) && std::isfinite(target->y) && std::isfinite(target->z);
	}
	if (assignmentKind == team::TeamObjectiveKind::GuardBombDefuser && assignment != nullptr)
	{
		*target = {assignment->targetPosition.x, assignment->targetPosition.y,
				   assignment->targetPosition.z};
		return std::isfinite(target->x) && std::isfinite(target->y) && std::isfinite(target->z);
	}
	if (!needsBombSite && !needsPlantedBomb)
	{
		return false;
	}

	edict_t *plantedBomb = nullptr;
	compat::ObjectiveObservation plantedObservation = {};
	nav::NavVector selectedBombSite = {0.0f, 0.0f, 0.0f};
	float selectedPathCost = (std::numeric_limits<float>::max)();
	int selectedBombSiteEntityIndex = 0;
	std::uint32_t selectedSiteIdentity = 0U;
	struct ReachableBombSite
	{
		std::uint32_t identity;
		int entityIndex;
		float cost;
		nav::NavVector point;
	};
	std::vector<ReachableBombSite> reachableBombSites;
	const nav::NavSnapshot navigation = services_->service_navPublisher().snapshot();
	const nav::NavDocument *document = navigation.document();
	nav::NavAreaMatch currentMatch = {};
	bool haveCurrentArea = false;
	std::vector<ObjectiveCandidateKey> evaluatedCandidateKeys;
	if (navigation.isValid() && document != nullptr)
	{
		nav::NavQuery query(navigation);
		const nav::NavVector position = {handle.entity->v.origin.x, handle.entity->v.origin.y,
										 handle.entity->v.origin.z};
		haveCurrentArea =
			query.findContaining(position, 64.0f, &currentMatch) == nav::NavQueryResult::Found;
		if (!haveCurrentArea)
		{
			haveCurrentArea = query.findNearest(position, kMaximumObjectiveDistance,
												&currentMatch) == nav::NavQueryResult::Found;
		}
	}
	const int maxEntities =
		(std::max)(0, (std::min)(services_->service_globals()->maxEntities, 2048));
	std::uint32_t targetId = 0U;
	for (int entityIndex = 1; entityIndex <= maxEntities; ++entityIndex)
	{
		edict_t *entity = services_->service_engineFunctions()->pfnPEntityOfEntIndex(entityIndex);
		if (entity == nullptr || entity->free)
		{
			continue;
		}
		const char *classname =
			services_->service_engineFunctions()->pfnSzFromIndex(entity->v.classname);
		if (classname == nullptr)
		{
			continue;
		}
		if (needsBombSite && isBombTargetClassname(classname))
		{
			const std::uint32_t siteIdentity = ++targetId;
			if (objectiveStats != nullptr)
			{
				++objectiveStats->bombSites;
				++objectiveStats->evaluatedSites;
				if (equalsIgnoreCase(classname, "func_bomb_target"))
				{
					++objectiveStats->funcBombTargetSites;
				}
				else if (equalsIgnoreCase(classname, "info_bomb_target"))
				{
					++objectiveStats->infoBombTargetSites;
				}
			}
			const nav::NavVector candidate = entityObjectiveCenter(entity);
			nav::NavExtent siteExtent = {};
			const bool hasSiteExtent = objectiveSiteExtent(entity, classname, &siteExtent);
			float pathCost = (std::numeric_limits<float>::max)();
			nav::NavVector reachablePoint = candidate;
			nav::AreaId nearestNavArea = 0U;
			nav::AreaId candidateNavArea = 0U;
			bool validCandidate = false;
			std::uint32_t siteCandidateAreaSamples = 0U;
			std::uint32_t siteUniqueCandidateAreas = 0U;
			std::uint32_t siteReachableCandidateAreas = 0U;
			const char *rejectionReason = "no_candidate_nav_area";
			if (haveCurrentArea && document != nullptr && hasSiteExtent)
			{
				nav::NavQuery query(navigation);
				const bool compatibility =
					services_->service_compatibilitySurface().configuration().mode ==
					compat::RuntimeMode::Compatibility;
				if (compatibility)
				{
					query.setPathCostContext(services_->navigation().managedPathCostContext(index));
				}
				const nav::NavVector samplePoints[] = {
					candidate,
					{siteExtent.lo.x, (siteExtent.lo.y + siteExtent.hi.y) * 0.5f, candidate.z},
					{siteExtent.hi.x, (siteExtent.lo.y + siteExtent.hi.y) * 0.5f, candidate.z},
					{(siteExtent.lo.x + siteExtent.hi.x) * 0.5f, siteExtent.lo.y, candidate.z},
					{(siteExtent.lo.x + siteExtent.hi.x) * 0.5f, siteExtent.hi.y, candidate.z}};
				for (const nav::NavVector &samplePoint : samplePoints)
				{
					nav::NavAreaMatch siteMatch = {};
					if (query.findNearest(samplePoint, kMaximumObjectiveDistance, &siteMatch) !=
						nav::NavQueryResult::Found)
					{
						continue;
					}
					if (nearestNavArea == 0U)
					{
						nearestNavArea = siteMatch.area;
					}
					const nav::NavArea *siteArea = document->findArea(siteMatch.area);
					if (siteArea == nullptr || siteMatch.closestPoint.x < siteExtent.lo.x ||
						siteMatch.closestPoint.x > siteExtent.hi.x ||
						siteMatch.closestPoint.y < siteExtent.lo.y ||
						siteMatch.closestPoint.y > siteExtent.hi.y)
					{
						if (!validCandidate)
						{
							rejectionReason = "candidate_outside_site_extent";
						}
						continue;
					}
					if (equalsIgnoreCase(classname, "info_bomb_target") &&
						!ActionAdapter::withinLegacyBombTargetRadius(
							{candidate.x, candidate.y, candidate.z},
							{siteMatch.closestPoint.x, siteMatch.closestPoint.y,
							 siteMatch.closestPoint.z}))
					{
						rejectionReason = "outside_legacy_info_radius";
						continue;
					}
					if (objectiveStats != nullptr)
					{
						++objectiveStats->candidateAreas;
					}
					++siteCandidateAreaSamples;
					const ObjectiveCandidateKey candidateKey = {entityIndex, siteIdentity,
																siteMatch.area};
					if (std::find(evaluatedCandidateKeys.begin(), evaluatedCandidateKeys.end(),
								  candidateKey) != evaluatedCandidateKeys.end())
					{
						if (objectiveStats != nullptr)
						{
							++objectiveStats->duplicateCandidateAreas;
						}
						if (!validCandidate)
						{
							rejectionReason = "duplicate_candidate_identity";
						}
						continue;
					}
					evaluatedCandidateKeys.push_back(candidateKey);
					++siteUniqueCandidateAreas;
					if (objectiveStats != nullptr)
					{
						++objectiveStats->uniqueCandidateAreas;
						++objectiveStats->candidateQueries;
					}
					const nav::NavArea &area = *siteArea;
					nav::NavCorridor corridor = {};
					nav::NavSearchStats sample = {};
					if (query.buildCorridor(
							currentMatch.area, area.id,
							compatibility ? nav::NavRouteType::Safest : nav::NavRouteType::Fastest,
							&corridor, searchStats == nullptr ? nullptr : &sample) ==
							nav::NavQueryResult::Found &&
						std::isfinite(corridor.cost))
					{
						++siteReachableCandidateAreas;
						if (corridor.cost < pathCost)
						{
							pathCost = corridor.cost;
							reachablePoint = siteMatch.closestPoint;
							candidateNavArea = siteMatch.area;
						}
						validCandidate = true;
						rejectionReason = "none";
					}
					else
					{
						if (!validCandidate)
						{
							rejectionReason = "no_route";
						}
					}
					addNavSearchStats(searchStats, sample);
				}
			}
			else
			{
				pathCost = haveCurrentArea ? 1.0f : 0.0f;
				validCandidate = true;
				rejectionReason = "nav_unavailable_fallback";
			}
			if (objectiveStats != nullptr && !cache.diagnosticsEmitted)
			{
				logObjectiveTargetDiagnostic(
					services_->service_runtimeProfiler(), services_->service_pluginId(), targetId,
					entityIndex, classname, managedObjectiveSiteRegistry_.registeredCount,
					managedObjectiveSiteRegistry_.registeredFuncBombTargetCount,
					managedObjectiveSiteRegistry_.registeredInfoBombTargetCount, entity, candidate,
					nearestNavArea, candidateNavArea, siteCandidateAreaSamples,
					siteUniqueCandidateAreas, siteReachableCandidateAreas,
					pathCost == (std::numeric_limits<float>::max)() ? -1.0f : pathCost,
					objectiveCacheState, validCandidate, rejectionReason);
			}
			if (validCandidate)
			{
				reachableBombSites.push_back({siteIdentity, entityIndex, pathCost, reachablePoint});
			}
			if (validCandidate && objectives::isBetterBombSiteRoute(pathCost, selectedPathCost))
			{
				selectedPathCost = pathCost;
				selectedBombSite = reachablePoint;
				selectedBombSiteEntityIndex = entityIndex;
				selectedSiteIdentity = siteIdentity;
			}
		}
		const char *model = services_->service_engineFunctions()->pfnSzFromIndex(entity->v.model);
		if (needsPlantedBomb && plantedBomb == nullptr &&
			services_->observation().observationAdapter().collectPlantedBomb(
				entity, classname, model, services_->service_globals()->time, actor, frame,
				observationTiming, &plantedObservation) == ObservationAdapterResult::Accepted)
		{
			world::WorldVector knownPosition{};
			if (services_->observation().observeObjective(index, entity, true, &knownPosition))
			{
				plantedBomb = entity;
			}
		}
	}
	if (objectiveStats != nullptr)
	{
		cache.diagnosticsEmitted = true;
	}
	if (team == 1 && carryingBomb && selectedPathCost != (std::numeric_limits<float>::max)())
	{
		const float speed =
			std::isfinite(handle.entity->v.maxspeed) && handle.entity->v.maxspeed > 0.0f
				? handle.entity->v.maxspeed
				: 240.0f;
		const float nearestTravel = selectedPathCost / speed;
		const float roundRemaining = managedRoundRemainingSeconds();
		const compat::RandomActor randomActor = {handle.actor.slot, handle.actor.actorGeneration};
		const compat::RandomTimingContext randomTiming = {
			services_->command().managedBotCommandSequences(index),
			services_->service_adapterFrameCount(),
			services_->service_managedBotFullUpdateSequences()[index]};
		const float decisionStart = services_->service_roundFreezeGeneration() ==
											services_->service_lifecycle().roundGeneration()
										? services_->service_roundFreezeStartTime()
										: services_->service_globals()->time;
		objectives::beginBombApproach(
			&approach, frame.mapGeneration, frame.roundGeneration, randomActor,
			services_->service_globals()->time, decisionStart, roundRemaining, nearestTravel,
			haveCurrentArea && document != nullptr ? &services_->service_compatibilityRandomSource()
												   : nullptr,
			randomTiming);
		const auto phase = objectives::bombApproachPhase(
			&approach, services_->service_globals()->time, roundRemaining, nearestTravel,
			carryingBomb, false, managedActorAtBombSite(index));
		if (phase == objectives::BombApproachPhase::Approach)
		{
			if (approach.preferredSiteIdentity == 0U)
			{
				std::vector<float> costs;
				const float delayLeft =
					(std::max)(0.0f, approach.plantAt - services_->service_globals()->time);
				for (const auto &site : reachableBombSites)
				{
					costs.push_back(
						roundRemaining > delayLeft + site.cost / speed + 5.0f ? site.cost : -1.0f);
				}
				const auto choice = objectives::chooseBombApproachSite(
					costs.data(), costs.size(), &services_->service_compatibilityRandomSource(),
					randomActor, randomTiming);
				if (choice < reachableBombSites.size())
					approach.preferredSiteIdentity = reachableBombSites[choice].identity;
			}
			const auto preferred =
				std::find_if(reachableBombSites.begin(), reachableBombSites.end(),
							 [&approach](const ReachableBombSite &site) {
								 return site.identity == approach.preferredSiteIdentity;
							 });
			if (preferred != reachableBombSites.end())
			{
				selectedBombSite = preferred->point;
				selectedBombSiteEntityIndex = preferred->entityIndex;
				selectedSiteIdentity = preferred->identity;
				selectedPathCost = preferred->cost;
			}
			else
			{
				approach.finished = true;
			}
		}
		cache.approachRouteSeconds = selectedPathCost / speed;
		*target = selectedBombSite;
		cache.target = *target;
		cache.selectedEntityIndex = selectedBombSiteEntityIndex;
		cache.selectedSiteIdentity = selectedSiteIdentity;
		cache.valid = true;
		if (objectiveStats != nullptr)
		{
			objectiveStats->selectedSiteIdentity = selectedSiteIdentity;
		}
	}
	else if (team == 2 && plantedBomb != nullptr)
	{
		world::WorldVector knownPosition{};
		if (!services_->observation().observeObjective(index, plantedBomb, true, &knownPosition))
		{
			return false;
		}
		*target = {knownPosition.x, knownPosition.y, knownPosition.z};
	}
	else
	{
		return false;
	}
	cache.diagnosticsEmitted = objectiveStats != nullptr;
	return std::isfinite(target->x) && std::isfinite(target->y) && std::isfinite(target->z);
}

ActionProposal BotBehaviorRuntime::decideManagedBotAction(
	std::size_t index, const runtime::MovementPhysicsState &before,
	const runtime::ViewAngles &movementAngles, std::uint16_t movementButtons)
{
	ActionProposal proposal = {ActionKind::None, movementAngles, movementButtons};
	if (index < NativeBotObservation::kClientSlotCount &&
		services_->service_compatibilitySurface().configuration().mode ==
			compat::RuntimeMode::Compatibility &&
		!actors_[index].managedBotStateMachines.attackOverlayActive() &&
		(actors_[index].managedBotStateMachines.state() ==
			 compat::CompatibilityStateId::PlantBomb ||
		 actors_[index].managedBotStateMachines.state() ==
			 compat::CompatibilityStateId::DefuseBomb))
	{
		proposal.stopForWeapon = true;
		proposal.movementButtons = static_cast<std::uint16_t>(
			(proposal.movementButtons &
			 ~(IN_JUMP | IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT)) |
			IN_DUCK);
	}
	bool objectiveActionDispatched = false;
	bool preserveObjectiveActionState = false;
	const auto cancelHeldGrenade = [&]() -> bool {
		if (index >= services_->service_managedBotHandles().size() ||
			!actors_[index].weaponActions.grenadeInProgress())
			return false;
		const FakeClientHandle &cancelHandle = services_->service_managedBotHandles()[index];
		compat::WeaponActionInput cancel = {};
		cancel.actor = {cancelHandle.actor.slot, cancelHandle.actor.actorGeneration};
		cancel.frame = {services_->service_lifecycle().mapGeneration(),
						services_->service_lifecycle().roundGeneration(),
						services_->service_adapterFrameCount()};
		cancel.timestamp =
			services_->service_globals() != nullptr ? services_->service_globals()->time : 0.0f;
		cancel.weaponId = actors_[index].weaponActions.currentWeaponId();
		compat::PublicActorState activeWeapon = {};
		if (services_->observation().publicObservations().read(cancel.actor, cancel.frame,
															   &activeWeapon) &&
			activeWeapon.weaponKnown && activeWeapon.weaponId != static_cast<int>(cancel.weaponId))
		{
			actors_[index].weaponActions.reset();
			return false;
		}
		const auto ammoDefinition =
			services_->observation().publicObservations().definition(cancel.weaponId);
		if (activeWeapon.bound && ammoDefinition.known && ammoDefinition.ammoIndex >= 0 &&
			static_cast<std::size_t>(ammoDefinition.ammoIndex) < activeWeapon.ammo.size() &&
			activeWeapon.ammoKnown[static_cast<std::size_t>(ammoDefinition.ammoIndex)])
		{
			const auto ammoIndex = static_cast<std::size_t>(ammoDefinition.ammoIndex);
			cancel.reserveKnown = true;
			cancel.reserve = static_cast<std::uint16_t>(activeWeapon.ammo[ammoIndex]);
			cancel.reserveObservationTick = activeWeapon.ammoTicks[ammoIndex];
		}
		const auto canceled = actors_[index].weaponActions.update(cancel);
		proposal.kind = canceled.primary ? ActionKind::Fire : ActionKind::None;
		proposal.stopForWeapon = canceled.stopMovement;
		if (canceled.hasRetainedGrenadeAim)
			proposal.viewAngles = {canceled.grenadePitch, canceled.grenadeYaw, movementAngles.roll};
		return true;
	};
	if (index >= services_->service_managedBotHandles().size())
	{
		return proposal;
	}
	if (before.dead || services_->service_managedBotHandles()[index].entity == nullptr)
	{
		actors_[index].managedBotObjectiveActions.active = false;
		actors_[index].weaponActions.reset();
		actors_[index].escapingWithKnife = false;
		return proposal;
	}
	const FakeClientHandle &handle = services_->service_managedBotHandles()[index];
	const world::FrameIdentity frame = {services_->service_lifecycle().mapGeneration(),
										services_->service_lifecycle().roundGeneration(),
										services_->service_adapterFrameCount()};
	const world::ActorKey actor = {handle.actor.slot, handle.actor.actorGeneration};
	const compat::ObservationTimingContext observationTiming = {
		services_->command().managedBotCommandSequences(index), 0U, 0U};
	compat::CompatibilityObservation compatObservation = {};
	if (services_->observation().observationAdapter().collectActor(
			handle.entity, actor, frame, observationTiming, &compatObservation) !=
		ObservationAdapterResult::Accepted)
	{
		actors_[index].managedBotObjectiveActions.active = false;
		(void)cancelHeldGrenade();
		return proposal;
	}
	if (services_->service_compatibilitySurface().configuration().mode ==
			compat::RuntimeMode::Compatibility &&
		actors_[index].managedBotStateMachines.state() == compat::CompatibilityStateId::PlantBomb &&
		!actors_[index].managedBotStateMachines.attackOverlayActive() &&
		compatObservation.objective.carryingC4.isAvailable() &&
		compatObservation.objective.carryingC4.value && managedActorAtBombSite(index))
	{
		// Plant owns the interaction after arrival. A cleared corridor or an old
		// destination farther than96 must not suppress its C4 selection/attack.
		if (cancelHeldGrenade())
			return proposal;
		auto &action = actors_[index].managedBotObjectiveActions;
		action.active = true;
		action.actor = actor;
		action.kind = team::TeamObjectiveKind::PlantC4;
		action.mapGeneration = frame.mapGeneration;
		action.roundGeneration = frame.roundGeneration;
		const auto &weapon = services_->observation().managedBotCurrentWeapons(index);
		const bool current = weapon.available && weapon.actorGeneration == actor.generation &&
							 weapon.mapGeneration == frame.mapGeneration &&
							 weapon.roundGeneration == frame.roundGeneration;
		proposal.kind = ActionAdapter::plantActionForWeaponObservation(
			current, current && weapon.active, weapon.weaponId, action.c4AttackInputDispatched);
		proposal.stopForWeapon = true;
		proposal.movementButtons = IN_DUCK;
		proposal.viewAngles = {90.0f, movementAngles.yaw, movementAngles.roll};
		return proposal;
	}
	if (services_->service_compatibilitySurface().configuration().mode ==
			compat::RuntimeMode::Compatibility &&
		actors_[index].scenarioDecisions.action == compat::ScenarioAction::Escape &&
		actors_[index].managedBotStateMachines.state() ==
			compat::CompatibilityStateId::EscapeFromBomb)
	{
		if (cancelHeldGrenade())
			return proposal;
		actors_[index].escapingWithKnife = true;
		if (!compatObservation.weapon.activeWeaponId.isAvailable() ||
			compatObservation.weapon.activeWeaponId.value != 29U)
			proposal.kind = ActionKind::SelectKnife;
		return proposal;
	}
	if (actors_[index].escapingWithKnife)
	{
		actors_[index].escapingWithKnife = false;
		const auto inventory = static_cast<std::uint32_t>(handle.entity->v.weapons);
		for (const unsigned id :
			 {3U, 5U, 7U, 8U, 12U, 13U, 14U, 15U, 18U, 19U, 20U, 21U, 22U, 23U, 24U, 27U, 28U, 30U})
			if ((inventory & (1U << id)) != 0U)
			{
				proposal.kind = ActionKind::SelectPrimary;
				return proposal;
			}
		for (const unsigned id : {1U, 10U, 11U, 16U, 17U, 26U})
			if ((inventory & (1U << id)) != 0U)
			{
				proposal.kind = ActionKind::SelectPistol;
				return proposal;
			}
	}
	refreshManagedTeamObjectiveAssignments(index);
	const team::TeamObjectiveAssignment *objectiveAssignment =
		findManagedTeamObjectiveAssignment(actor);
	world::WorldSnapshot snapshot;
	world::ActorKey targetActor = {};
	world::WorldPosition targetPosition = {};
	if (!services_->observation().buildManagedWorldSnapshot(index, &snapshot, &targetActor,
															&targetPosition))
	{
		actors_[index].managedBotObjectiveActions.active = false;
		(void)cancelHeldGrenade();
		if ((services_->service_adapterFrameCount() % 16U) == 0U && gpMetaUtilFuncs != nullptr &&
			gpMetaUtilFuncs->pfnLogConsole != nullptr && services_->service_pluginId() != nullptr)
		{
			profiledLogConsole(
				services_->service_runtimeProfiler(), services_->service_pluginId(),
				"action sensor slot=%u frame=%u team=%d teamInfo=%u target=0",
				static_cast<unsigned int>(handle.actor.slot),
				static_cast<unsigned int>(services_->service_adapterFrameCount()),
				static_cast<int>(handle.entity->v.team),
				static_cast<unsigned int>(services_->observation().managedBotTeamNumbers(index)));
		}
		return proposal;
	}
	const int team = resolveManagedBotTeam(index, compatObservation);
	const objectives::TeamRole teamRole = team == 1	  ? objectives::TeamRole::Terrorist
										  : team == 2 ? objectives::TeamRole::CounterTerrorist
													  : objectives::TeamRole::Unknown;
	if (!targetActor.isValid() && services_->service_compatibilitySurface().configuration().mode ==
									  compat::RuntimeMode::Compatibility)
	{
		if (cancelHeldGrenade())
			return proposal;
		const auto &scenario = actors_[index].scenarioDecisions;
		if (!scenario.unavailable)
		{
			if (scenario.action == compat::ScenarioAction::Stop ||
				(scenario.action == compat::ScenarioAction::Guard && scenario.hasTarget &&
				 scenario.target.inUseRange))
			{
				proposal.stopForWeapon = true;
				return proposal;
			}
			if (scenario.action == compat::ScenarioAction::Use && scenario.hasTarget &&
				scenario.target.entity.isValid() &&
				services_->service_engineFunctions() != nullptr &&
				services_->service_engineFunctions()->pfnPEntityOfEntIndex != nullptr)
			{
				edict_t *useEntity = services_->service_engineFunctions()->pfnPEntityOfEntIndex(
					static_cast<int>(scenario.target.entity.id));
				if (useEntity != nullptr && !useEntity->free &&
					static_cast<std::uint32_t>(useEntity->serialnumber) + 1U ==
						scenario.target.entity.generation)
				{
					const float dx = scenario.target.position.x - handle.entity->v.origin.x;
					const float dy = scenario.target.position.y - handle.entity->v.origin.y;
					proposal.kind = ActionKind::UseEntity;
					proposal.viewAngles.yaw = std::atan2(dy, dx) * 180.0f / 3.14159265358979323846f;
					return proposal;
				}
			}
		}
	}
	if (teamRole != objectives::TeamRole::Unknown &&
		services_->service_engineFunctions() != nullptr &&
		services_->service_engineFunctions()->pfnPEntityOfEntIndex != nullptr &&
		services_->service_engineFunctions()->pfnSzFromIndex != nullptr &&
		services_->service_globals() != nullptr)
	{
		const auto findEntityByClassname = [this](const char *wantedClassname) -> edict_t * {
			if (wantedClassname == nullptr || services_->service_engineFunctions() == nullptr ||
				services_->service_engineFunctions()->pfnPEntityOfEntIndex == nullptr ||
				services_->service_engineFunctions()->pfnSzFromIndex == nullptr ||
				services_->service_globals() == nullptr)
			{
				return nullptr;
			}
			const int maxEntities =
				(std::max)(0, (std::min)(services_->service_globals()->maxEntities, 2048));
			for (int entityIndex = 1; entityIndex <= maxEntities; ++entityIndex)
			{
				edict_t *entity =
					services_->service_engineFunctions()->pfnPEntityOfEntIndex(entityIndex);
				if (entity == nullptr || entity->free)
				{
					continue;
				}
				const char *classname =
					services_->service_engineFunctions()->pfnSzFromIndex(entity->v.classname);
				if (classname != nullptr && std::strcmp(classname, wantedClassname) == 0)
				{
					return entity;
				}
			}
			return nullptr;
		};

		edict_t *bombSite = findEntityByClassname("func_bomb_target");
		edict_t *plantedBomb = nullptr;
		compat::ObjectiveObservation plantedObservation = {};
		std::uint32_t grenadeEntityCount = 0U;
		std::uint32_t c4ModelEntityCount = 0U;
		int firstGrenadeEntityIndex = 0;
		int firstC4ModelEntityIndex = 0;
		const char *firstGrenadeClassname = "(none)";
		const char *firstGrenadeModel = "(none)";
		const char *firstC4ModelClassname = "(none)";
		const char *firstC4ModelName = "(none)";
		float firstGrenadeDamageTime = 0.0f;
		float firstC4ModelDamageTime = 0.0f;
		int firstGrenadeObservationResult = -1;
		int firstC4ModelObservationResult = -1;
		const int maxEntities =
			(std::max)(0, (std::min)(services_->service_globals()->maxEntities, 2048));
		for (int entityIndex = 1; entityIndex <= maxEntities; ++entityIndex)
		{
			edict_t *entity =
				services_->service_engineFunctions()->pfnPEntityOfEntIndex(entityIndex);
			if (entity == nullptr || entity->free)
			{
				continue;
			}
			const char *classname =
				services_->service_engineFunctions()->pfnSzFromIndex(entity->v.classname);
			const char *model =
				services_->service_engineFunctions()->pfnSzFromIndex(entity->v.model);
			const bool grenadeEntity = equalsIgnoreCase(classname, "grenade");
			const bool c4Model = model != nullptr && std::strstr(model, "w_c4.mdl") != nullptr;
			if (grenadeEntity)
			{
				++grenadeEntityCount;
			}
			if (c4Model)
			{
				++c4ModelEntityCount;
			}
			ObservationAdapterResult plantedResult = ObservationAdapterResult::InvalidEntity;
			if (plantedBomb == nullptr)
			{
				plantedResult = services_->observation().observationAdapter().collectPlantedBomb(
					entity, classname, model, services_->service_globals()->time, actor, frame,
					observationTiming, &plantedObservation);
			}
			if (grenadeEntity && firstGrenadeEntityIndex == 0)
			{
				firstGrenadeEntityIndex = entityIndex;
				firstGrenadeClassname = classname != nullptr ? classname : "(null)";
				firstGrenadeModel = model != nullptr ? model : "(null)";
				firstGrenadeDamageTime = entity->v.dmgtime;
				firstGrenadeObservationResult = static_cast<int>(plantedResult);
			}
			if (c4Model && firstC4ModelEntityIndex == 0)
			{
				firstC4ModelEntityIndex = entityIndex;
				firstC4ModelClassname = classname != nullptr ? classname : "(null)";
				firstC4ModelName = model != nullptr ? model : "(null)";
				firstC4ModelDamageTime = entity->v.dmgtime;
				firstC4ModelObservationResult = static_cast<int>(plantedResult);
			}
			if (plantedResult == ObservationAdapterResult::Accepted)
			{
				world::WorldVector knownPosition{};
				if (services_->observation().observeObjective(index, entity, true, &knownPosition))
				{
					plantedBomb = entity;
					break;
				}
			}
		}
		if (plantedBomb == nullptr && teamRole == objectives::TeamRole::CounterTerrorist &&
			services_->service_runtimeProfiler().enabled() &&
			(services_->service_adapterFrameCount() % 64U) == 0U && gpMetaUtilFuncs != nullptr &&
			gpMetaUtilFuncs->pfnLogConsole != nullptr && services_->service_pluginId() != nullptr)
		{
			profiledLogConsole(
				services_->service_runtimeProfiler(), services_->service_pluginId(),
				"profile plantedBombScan actor=%u actor_generation=%u map=%u round=%u "
				"frame=%u now=%.2f entities=%d grenade_count=%u "
				"first_grenade=(%d %s %s dmgtime=%.2f result=%d) "
				"c4_model_count=%u first_c4_model=(%d %s %s dmgtime=%.2f result=%d)",
				static_cast<unsigned int>(handle.actor.slot),
				static_cast<unsigned int>(handle.actor.actorGeneration),
				static_cast<unsigned int>(services_->service_lifecycle().mapGeneration()),
				static_cast<unsigned int>(services_->service_lifecycle().roundGeneration()),
				static_cast<unsigned int>(services_->service_adapterFrameCount()),
				services_->service_globals()->time, maxEntities, grenadeEntityCount,
				firstGrenadeEntityIndex, firstGrenadeClassname, firstGrenadeModel,
				firstGrenadeDamageTime, firstGrenadeObservationResult, c4ModelEntityCount,
				firstC4ModelEntityIndex, firstC4ModelClassname, firstC4ModelName,
				firstC4ModelDamageTime, firstC4ModelObservationResult);
		}
		const bool carryingBomb = compatObservation.objective.carryingC4.isAvailable() &&
								  compatObservation.objective.carryingC4.value;
		ManagedObjectiveActionState &plantState = actors_[index].managedBotObjectiveActions;
		if (plantedBomb != nullptr && plantState.active && plantState.actor == actor &&
			plantState.kind == team::TeamObjectiveKind::PlantC4)
		{
			(void)ActionAdapter::evaluatePlantAttempt(
				plantState.plantAttempt, false, true,
				services_->service_globals() != nullptr ? services_->service_globals()->time
														: std::numeric_limits<float>::quiet_NaN());
			if (services_->service_runtimeProfiler().enabled() && gpMetaUtilFuncs != nullptr &&
				gpMetaUtilFuncs->pfnLogConsole != nullptr &&
				services_->service_pluginId() != nullptr)
			{
				const int plantedEntityIndex =
					services_->service_engineFunctions() != nullptr &&
							services_->service_engineFunctions()->pfnIndexOfEdict != nullptr
						? services_->service_engineFunctions()->pfnIndexOfEdict(plantedBomb)
						: 0;
				profiledLogConsole(
					services_->service_runtimeProfiler(), services_->service_pluginId(),
					"profile plantConfirm actor=%u actor_generation=%u map=%u round=%u "
					"frame=%u assignment=%u site=%d site_id=%u planted_entity=%d "
					"source=public_planted_c4_observation",
					static_cast<unsigned int>(handle.actor.slot),
					static_cast<unsigned int>(handle.actor.actorGeneration),
					static_cast<unsigned int>(services_->service_lifecycle().mapGeneration()),
					static_cast<unsigned int>(services_->service_lifecycle().roundGeneration()),
					static_cast<unsigned int>(services_->service_adapterFrameCount()),
					plantState.assignmentGeneration, plantState.targetEntityIndex,
					plantState.targetSiteIdentity, plantedEntityIndex);
			}
			plantState = {};
		}
		if ((services_->service_adapterFrameCount() % 16U) == 0U && gpMetaUtilFuncs != nullptr &&
			gpMetaUtilFuncs->pfnLogConsole != nullptr && services_->service_pluginId() != nullptr)
		{
			profiledLogConsole(services_->service_runtimeProfiler(), services_->service_pluginId(),
							   "objective sensor slot=%u frame=%u team=%d weapons=%d carryingC4=%d "
							   "bombSite=%d plantedC4=%d",
							   static_cast<unsigned int>(handle.actor.slot),
							   static_cast<unsigned int>(services_->service_adapterFrameCount()),
							   team, handle.entity->v.weapons, carryingBomb ? 1 : 0,
							   bombSite != nullptr ? 1 : 0, plantedBomb != nullptr ? 1 : 0);
		}
		if (plantedBomb != nullptr || carryingBomb)
		{
			objectives::ScenarioObservation scenario = {};
			scenario.scenario.frame = snapshot.identity().frame;
			scenario.scenario.scenarioGeneration = 1U;
			scenario.scenario.kind = objectives::ScenarioKind::Bomb;
			scenario.scenario.team = teamRole;
			scenario.phase = objectives::RoundPhase::Live;
			scenario.buyAvailability = objectives::Availability::Unavailable;
			if (plantedBomb != nullptr)
			{
				scenario.eventCount = 1U;
				scenario.events[0].id = 1U;
				scenario.events[0].frame = snapshot.identity().frame;
				scenario.events[0].kind = objectives::ScenarioEventKind::BombPlanted;
				scenario.events[0].state = objectives::EventState::Observed;
				scenario.events[0].actor = {handle.actor.slot, handle.actor.actorGeneration};
				scenario.events[0].team = objectives::TeamRole::Terrorist;
			}
			else if (carryingBomb)
			{
				scenario.eventCount = 1U;
				scenario.events[0].id = 1U;
				scenario.events[0].frame = snapshot.identity().frame;
				scenario.events[0].kind = objectives::ScenarioEventKind::BombCarried;
				scenario.events[0].state = objectives::EventState::Observed;
				scenario.events[0].actor = {handle.actor.slot, handle.actor.actorGeneration};
				scenario.events[0].team = objectives::TeamRole::Terrorist;
			}
			objectives::ObjectiveProposal objective = {};
			const behavior::BehaviorState compatibilityBehaviorState =
				actors_[index].managedBotStateMachines.attackOverlayActive()
					? behavior::BehaviorState::Engage
					: behavior::BehaviorState::Roam;
			const objectives::ObjectiveResult objectiveResult =
				actors_[index].managedBotObjectives.plan(snapshot, compatibilityBehaviorState,
														 scenario, nullptr, &objective);
			const bool hasPlantAssignment =
				objectiveAssignment != nullptr &&
				objectiveAssignment->kind == team::TeamObjectiveKind::PlantC4;
			nav::NavVector selectedPlantTarget{};
			const bool haveSelectedPlantTarget =
				hasPlantAssignment && carryingBomb && teamRole == objectives::TeamRole::Terrorist &&
				buildManagedObjectiveTarget(index, &selectedPlantTarget, nullptr);
			objectives::ObjectiveKind objectiveKind = objectives::ObjectiveKind::None;
			if (objectiveResult == objectives::ObjectiveResult::Proposed &&
				objective.objective.kind != objectives::ObjectiveKind::Plant &&
				objective.objective.kind != objectives::ObjectiveKind::Defuse)
			{
				objectiveKind = objective.objective.kind;
			}
			if (ActionAdapter::canBeginPlantObjective(
					hasPlantAssignment, carryingBomb, teamRole == objectives::TeamRole::Terrorist,
					haveSelectedPlantTarget && actors_[index].managedBombApproaches.finished))
			{
				objectiveKind = objectives::ObjectiveKind::Plant;
			}
			else if (objectiveAssignment != nullptr &&
					 objectiveAssignment->kind == team::TeamObjectiveKind::DefuseC4 &&
					 plantedBomb != nullptr && teamRole == objectives::TeamRole::CounterTerrorist)
			{
				objectiveKind = objectives::ObjectiveKind::Defuse;
			}
			else if (objectiveAssignment == nullptr ||
					 (objectiveAssignment->kind != team::TeamObjectiveKind::PlantC4 &&
					  objectiveAssignment->kind != team::TeamObjectiveKind::DefuseC4))
			{
				objectiveKind = objectives::ObjectiveKind::None;
			}
			// MoveTo owns the approach. A legacy wider action radius must not stop
			// an actor before its observed scenario has entered the defuse state.
			if (objectiveKind == objectives::ObjectiveKind::Defuse &&
				services_->service_compatibilitySurface().configuration().mode ==
					compat::RuntimeMode::Compatibility &&
				actors_[index].managedBotStateMachines.state() !=
					compat::CompatibilityStateId::DefuseBomb)
				objectiveKind = objectives::ObjectiveKind::None;
			if (objectiveKind == objectives::ObjectiveKind::Plant ||
				objectiveKind == objectives::ObjectiveKind::Defuse)
			{
				nav::NavVector objectivePosition = {0.0f, 0.0f, 0.0f};
				bool haveObjectivePosition = false;
				if (objectiveKind == objectives::ObjectiveKind::Defuse)
				{
					if (plantedBomb != nullptr)
					{
						objectivePosition = {plantedBomb->v.origin.x, plantedBomb->v.origin.y,
											 plantedBomb->v.origin.z};
						haveObjectivePosition = true;
					}
				}
				else
				{
					objectivePosition = selectedPlantTarget;
					haveObjectivePosition = haveSelectedPlantTarget;
				}
				if (haveObjectivePosition)
				{
					const float dx = objectivePosition.x - handle.entity->v.origin.x;
					const float dy = objectivePosition.y - handle.entity->v.origin.y;
					const float dz = objectivePosition.z - handle.entity->v.origin.z;
					if ((objectiveKind == objectives::ObjectiveKind::Defuse &&
						 services_->service_compatibilitySurface().configuration().mode ==
							 compat::RuntimeMode::Compatibility &&
						 actors_[index].managedBotStateMachines.state() ==
							 compat::CompatibilityStateId::DefuseBomb) ||
						dx * dx + dy * dy + dz * dz <= 96.0f * 96.0f)
					{
						ManagedObjectiveActionState &actionState =
							actors_[index].managedBotObjectiveActions;
						const bool planting =
							objectiveAssignment->kind == team::TeamObjectiveKind::PlantC4;
						const int targetEntityIndex =
							planting ? actors_[index].managedBotObjectiveTargets.selectedEntityIndex
									 : static_cast<int>(objectiveAssignment->targetEntity.id);
						const std::uint32_t targetIdentity =
							planting
								? actors_[index].managedBotObjectiveTargets.selectedSiteIdentity
								: objectiveAssignment->targetEntity.generation;
						const bool sameAction =
							actionState.active && actionState.actor == actor &&
							actionState.kind == objectiveAssignment->kind &&
							actionState.mapGeneration ==
								services_->service_lifecycle().mapGeneration() &&
							actionState.roundGeneration ==
								services_->service_lifecycle().roundGeneration() &&
							actionState.assignmentGeneration == objectiveAssignment->generation &&
							actionState.targetEntityIndex == targetEntityIndex &&
							actionState.targetSiteIdentity == targetIdentity;
						if (!sameAction)
						{
							actionState = {};
							actionState.active = true;
							actionState.actor = actor;
							actionState.kind = objectiveAssignment->kind;
							actionState.mapGeneration =
								services_->service_lifecycle().mapGeneration();
							actionState.roundGeneration =
								services_->service_lifecycle().roundGeneration();
							actionState.assignmentGeneration = objectiveAssignment->generation;
							actionState.targetEntityIndex = targetEntityIndex;
							actionState.targetSiteIdentity = targetIdentity;
						}
						if (planting)
						{
							preserveObjectiveActionState = true;
							PlantTargetBounds siteBounds{};
							PlantTargetBounds actorBounds{};
							bool siteBoundsAvailable = false;
							bool legacyInfoTarget = false;
							PlantTargetPoint legacyInfoTargetCenter{};
							if (targetEntityIndex > 0 &&
								services_->service_engineFunctions() != nullptr &&
								services_->service_engineFunctions()->pfnPEntityOfEntIndex !=
									nullptr &&
								services_->service_engineFunctions()->pfnSzFromIndex != nullptr)
							{
								edict_t *site =
									services_->service_engineFunctions()->pfnPEntityOfEntIndex(
										targetEntityIndex);
								const char *siteClassname =
									site != nullptr && !site->free
										? services_->service_engineFunctions()->pfnSzFromIndex(
											  site->v.classname)
										: nullptr;
								if (site != nullptr && !site->free &&
									isBombTargetClassname(siteClassname))
								{
									legacyInfoTarget =
										equalsIgnoreCase(siteClassname, "info_bomb_target");
									if (legacyInfoTarget)
									{
										const nav::NavVector center = entityObjectiveCenter(site);
										legacyInfoTargetCenter = {center.x, center.y, center.z};
										const float radius = ActionAdapter::kLegacyBombTargetRadius;
										siteBounds = {center.x - radius, center.y - radius,
													  center.z - radius, center.x + radius,
													  center.y + radius, center.z + radius};
										siteBoundsAvailable =
											ActionAdapter::validPlantTargetBounds(siteBounds);
									}
									else
									{
										siteBounds = {site->v.absmin.x, site->v.absmin.y,
													  site->v.absmin.z, site->v.absmax.x,
													  site->v.absmax.y, site->v.absmax.z};
										siteBoundsAvailable =
											ActionAdapter::validPlantTargetBounds(siteBounds) &&
											siteBounds.maximumX > siteBounds.minimumX &&
											siteBounds.maximumY > siteBounds.minimumY;
									}
								}
							}
							actorBounds = {handle.entity->v.absmin.x, handle.entity->v.absmin.y,
										   handle.entity->v.absmin.z, handle.entity->v.absmax.x,
										   handle.entity->v.absmax.y, handle.entity->v.absmax.z};
							const PlantTargetPoint actorPosition = {handle.entity->v.origin.x,
																	handle.entity->v.origin.y,
																	handle.entity->v.origin.z};
							const bool actorInSite =
								siteBoundsAvailable &&
								(legacyInfoTarget
									 ? ActionAdapter::withinLegacyBombTargetRadius(
										   legacyInfoTargetCenter, actorPosition)
									 : ActionAdapter::overlapsPlantTarget(siteBounds, actorBounds));
							const float now = services_->service_globals() != nullptr
												  ? services_->service_globals()->time
												  : std::numeric_limits<float>::quiet_NaN();
							const PlantAttemptResult attempt = ActionAdapter::evaluatePlantAttempt(
								actionState.plantAttempt, actorInSite, plantedBomb != nullptr, now);
							if (attempt == PlantAttemptResult::TimedOut)
							{
								actionState.c4SelectionCommandDispatched = false;
								actionState.c4AttackInputDispatched = false;
								actors_[index].managedBotObjectiveTargets.valid = false;
								actors_[index].managedBotObjectiveTargets.diagnosticsEmitted =
									false;
								services_->applyNavigationRequest(
									{index, NavigationRequestKind::ResetMovement});
							}
							const ManagedCurrentWeaponObservation &weapon =
								services_->observation().managedBotCurrentWeapons(index);
							const bool weaponObservationCurrent =
								weapon.available &&
								weapon.actorGeneration == handle.actor.actorGeneration &&
								weapon.mapGeneration ==
									services_->service_lifecycle().mapGeneration() &&
								weapon.roundGeneration ==
									services_->service_lifecycle().roundGeneration();
							const bool canContinuePlant = attempt == PlantAttemptResult::Ready ||
														  attempt == PlantAttemptResult::Continuing;
							if (services_->service_runtimeProfiler().enabled() &&
								(services_->service_adapterFrameCount() % 16U) == 0U &&
								gpMetaUtilFuncs != nullptr &&
								gpMetaUtilFuncs->pfnLogConsole != nullptr &&
								services_->service_pluginId() != nullptr)
							{
								profiledLogConsole(
									services_->service_runtimeProfiler(),
									services_->service_pluginId(),
									"profile plantGate actor=%u actor_generation=%u map=%u "
									"round=%u "
									"frame=%u assignment=%u site=%d site_id=%u bounds_available=%d "
									"zone_mode=%s "
									"actor_in_site=%d gate=%d site_bounds=(%.1f %.1f %.1f %.1f "
									"%.1f %.1f) "
									"actor_bounds=(%.1f %.1f %.1f %.1f %.1f %.1f) "
									"nav_target=(%.1f %.1f %.1f) actor_origin=(%.1f %.1f %.1f) "
									"weapon_known=%d weapon_active=%d weapon_id=%u",
									static_cast<unsigned int>(handle.actor.slot),
									static_cast<unsigned int>(handle.actor.actorGeneration),
									static_cast<unsigned int>(
										services_->service_lifecycle().mapGeneration()),
									static_cast<unsigned int>(
										services_->service_lifecycle().roundGeneration()),
									static_cast<unsigned int>(
										services_->service_adapterFrameCount()),
									objectiveAssignment->generation, targetEntityIndex,
									targetIdentity, siteBoundsAvailable ? 1 : 0,
									legacyInfoTarget ? "legacy_info_radius" : "func_bounds",
									actorInSite ? 1 : 0, static_cast<int>(attempt),
									siteBounds.minimumX, siteBounds.minimumY, siteBounds.minimumZ,
									siteBounds.maximumX, siteBounds.maximumY, siteBounds.maximumZ,
									actorBounds.minimumX, actorBounds.minimumY,
									actorBounds.minimumZ, actorBounds.maximumX,
									actorBounds.maximumY, actorBounds.maximumZ, objectivePosition.x,
									objectivePosition.y, objectivePosition.z,
									handle.entity->v.origin.x, handle.entity->v.origin.y,
									handle.entity->v.origin.z, weaponObservationCurrent ? 1 : 0,
									weaponObservationCurrent && weapon.active ? 1 : 0,
									static_cast<unsigned int>(weapon.weaponId));
							}
							if (!canContinuePlant)
							{
								return proposal;
							}
							proposal.kind = ActionAdapter::plantActionForWeaponObservation(
								weaponObservationCurrent, weaponObservationCurrent && weapon.active,
								weapon.weaponId, actionState.c4AttackInputDispatched);
						}
						else
						{
							proposal.kind = ActionKind::Defuse;
							if (services_->service_compatibilitySurface().configuration().mode ==
								compat::RuntimeMode::Compatibility)
								proposal.movementButtons = static_cast<std::uint16_t>(
									(proposal.movementButtons & ~(IN_JUMP | IN_FORWARD | IN_BACK |
																  IN_MOVELEFT | IN_MOVERIGHT)) |
									IN_DUCK);
						}
						proposal.viewAngles = {
							-kRadiansToDegrees * std::atan2(dz, std::sqrt(dx * dx + dy * dy)),
							kRadiansToDegrees * std::atan2(dy, dx), movementAngles.roll};
						objectiveActionDispatched = true;
						return proposal;
					}
				}
			}
		}
	}
	if (!objectiveActionDispatched)
	{
		if (!preserveObjectiveActionState)
		{
			actors_[index].managedBotObjectiveActions.active = false;
		}
	}
	combat::WeaponInventory inventory;
	combat::WeaponRecord weapon = {};
	compat::PublicActorState observedWeapon{};
	if (!services_->observation().publicObservations().read(actor, frame, &observedWeapon) ||
		!observedWeapon.weaponKnown)
		return proposal;
	const int weaponValue = observedWeapon.weaponId;
	weapon.weapon = {static_cast<std::uint16_t>(weaponValue)};
	const compat::PublicWeaponDefinition definition =
		services_->observation().publicObservations().definition(weaponValue);
	weapon.slot = static_cast<std::uint8_t>(definition.known ? definition.slot : 0);
	switch (weaponValue)
	{
	case 1:
	case 10:
	case 11:
	case 16:
	case 17:
	case 26:
		weapon.kind = combat::WeaponKind::Pistol;
		break;
	case 5:
	case 21:
		weapon.kind = combat::WeaponKind::Shotgun;
		break;
	case 7:
	case 12:
	case 19:
	case 23:
	case 30:
		weapon.kind = combat::WeaponKind::Smg;
		break;
	case 3:
	case 13:
	case 18:
	case 24:
		weapon.kind = combat::WeaponKind::Sniper;
		break;
	case 20:
		weapon.kind = combat::WeaponKind::Heavy;
		break;
	case 29:
		weapon.kind = combat::WeaponKind::Knife;
		break;
	case 4:
	case 9:
	case 25:
		weapon.kind = combat::WeaponKind::Grenade;
		break;
	case 8:
	case 14:
	case 15:
	case 22:
	case 27:
	case 28:
		weapon.kind = combat::WeaponKind::Rifle;
		break;
	default:
		return proposal;
	}
	weapon.availability = combat::WeaponAvailability::Available;
	weapon.priority = 1U;
	const bool knife = weapon.kind == combat::WeaponKind::Knife;
	const bool reserveKnown =
		definition.known && definition.ammoIndex >= 0 &&
		observedWeapon.ammoKnown[static_cast<std::size_t>(definition.ammoIndex)];
	const std::uint16_t reserve =
		reserveKnown ? static_cast<std::uint16_t>(
						   observedWeapon.ammo[static_cast<std::size_t>(definition.ammoIndex)])
					 : 0U;
	weapon.ammo = {static_cast<std::uint16_t>(knife ? 1 : (std::max)(0, observedWeapon.clip)),
				   reserve, 255U, knife || observedWeapon.clip >= 0, reserveKnown};
	weapon.reloadState = compatObservation.weapon.reloadState.isAvailable()
							 ? compatObservation.weapon.reloadState.value
							 : combat::ReloadState::Unknown;
	if (inventory.add(weapon) != combat::WeaponInventoryResult::Accepted)
	{
		return proposal;
	}
	combat::TargetBelief target = {};
	target.target = targetActor;
	target.state = combat::TargetBeliefState::Visible;
	target.position = targetPosition;
	target.confidence = 1.0f;
	target.ageTicks = 0U;
	target.friendlyFireRisk = false;
	if (services_->service_engineFunctions()->pfnTraceLine != nullptr)
	{
		const float eye[] = {handle.entity->v.origin.x + handle.entity->v.view_ofs.x,
							 handle.entity->v.origin.y + handle.entity->v.view_ofs.y,
							 handle.entity->v.origin.z + handle.entity->v.view_ofs.z};
		const float aim[] = {targetPosition.x, targetPosition.y, targetPosition.z};
		TraceResult trace{};
		services_->service_engineFunctions()->pfnTraceLine(eye, aim, 0, handle.entity, &trace);
		if (trace.pHit != nullptr && trace.pHit != handle.entity &&
			trace.pHit->v.deadflag == DEAD_NO)
		{
			int hitTeam = static_cast<int>(trace.pHit->v.team);
			for (std::size_t slot = 0U; slot < services_->service_managedBotHandles().size();
				 ++slot)
				if (services_->service_managedBotHandles()[slot].entity == trace.pHit &&
					services_->observation().managedBotTeamNumbers(slot) != 0U)
					hitTeam = services_->observation().managedBotTeamNumbers(slot);
			target.friendlyFireRisk = hitTeam == team && hitTeam != 0;
		}
	}
	combat::CombatDecisionContext context = {};
	context.actor = {handle.actor.slot, handle.actor.actorGeneration};
	context.frame = snapshot.identity().frame;
	context.origin = {handle.entity->v.origin.x + handle.entity->v.view_ofs.x,
					  handle.entity->v.origin.y + handle.entity->v.view_ofs.y,
					  handle.entity->v.origin.z + handle.entity->v.view_ofs.z};
	context.aimTrackingEnabled = true;
	context.currentPitch = handle.entity->v.v_angle.x;
	context.currentYaw = handle.entity->v.v_angle.y;
	context.elapsedSeconds =
		actors_[index].lastCombatTimes > 0.0f
			? (std::max)(0.0f, (std::min)(0.1f, services_->service_globals()->time -
													actors_[index].lastCombatTimes))
			: 1.0f / 30.0f;
	context.timestamp = services_->service_globals()->time;
	context.skill = static_cast<float>(actors_[index].managedProfiles.skill) / 100.0f;
	context.fieldOfView = handle.entity->v.fov > 0.0f ? handle.entity->v.fov : 90.0f;
	context.randomSource = &services_->service_compatibilityRandomSource();
	context.randomTiming = {services_->command().managedBotCommandSequences(index), 0U,
							services_->service_managedBotFullUpdateSequences()[index]};
	actors_[index].lastCombatTimes = services_->service_globals()->time;
	combat::CombatDecision decision = {};
	const combat::CombatResult result =
		actors_[index].managedBotCombat.decide(snapshot, inventory, target, context, &decision);
	const bool fireIntentReady =
		result == combat::CombatResult::FireIntentReady && decision.hasFire && decision.hasAim &&
		services_->service_globals()->time >= actors_[index].attackReadyTimes;
	const compat::StateUpdateContext stateContext = makeManagedBotStateContext(index, before);
	if (fireIntentReady)
	{
		(void)actors_[index].managedBotStateMachines.beginAttack(stateContext);
	}
	else if (actors_[index].managedBotStateMachines.attackOverlayActive())
	{
		(void)actors_[index].managedBotStateMachines.stopAttack(stateContext);
	}
	if ((services_->service_adapterFrameCount() % 16U) == 0U && gpMetaUtilFuncs != nullptr &&
		gpMetaUtilFuncs->pfnLogConsole != nullptr && services_->service_pluginId() != nullptr)
	{
		profiledLogConsole(
			services_->service_runtimeProfiler(), services_->service_pluginId(),
			"action sensor slot=%u frame=%u team=%d teamInfo=%u target=1 combatResult=%d "
			"hasAim=%d hasFire=%d",
			static_cast<unsigned int>(handle.actor.slot),
			static_cast<unsigned int>(services_->service_adapterFrameCount()),
			static_cast<int>(handle.entity->v.team),
			static_cast<unsigned int>(services_->observation().managedBotTeamNumbers(index)),
			static_cast<int>(result), decision.hasAim ? 1 : 0, decision.hasFire ? 1 : 0);
	}
	if (fireIntentReady)
	{
		proposal.kind = ActionKind::Fire;
		proposal.viewAngles = {decision.aim.pitch, decision.aim.yaw, movementAngles.roll};
	}
	else if (result == combat::CombatResult::ReloadIntentReady && decision.hasReload)
	{
		proposal.kind = ActionKind::Reload;
	}
	if (decision.hasAim)
	{
		proposal.viewAngles = {decision.aim.pitch, decision.aim.yaw, movementAngles.roll};
	}
	compat::WeaponActionInput weaponInput{};
	weaponInput.actor = actor;
	weaponInput.frame = frame;
	weaponInput.timestamp = services_->service_globals()->time;
	weaponInput.weaponId = static_cast<std::uint16_t>(weaponValue);
	weaponInput.clipKnown = observedWeapon.clip >= 0;
	weaponInput.clip = static_cast<std::uint16_t>((std::max)(0, observedWeapon.clip));
	weaponInput.fovKnown = std::isfinite(handle.entity->v.fov);
	weaponInput.fieldOfView = handle.entity->v.fov > 0.0f ? handle.entity->v.fov : 90.0f;
	weaponInput.targetDistance =
		std::sqrt((targetPosition.x - context.origin.x) * (targetPosition.x - context.origin.x) +
				  (targetPosition.y - context.origin.y) * (targetPosition.y - context.origin.y) +
				  (targetPosition.z - context.origin.z) * (targetPosition.z - context.origin.z));
	weaponInput.reactionSeconds = actors_[index].managedProfiles.reactionTimeSeconds;
	weaponInput.targetVisible = targetActor.isValid();
	const float weaponDx = targetPosition.x - context.origin.x;
	const float weaponDy = targetPosition.y - context.origin.y;
	const float weaponDz = targetPosition.z - context.origin.z;
	const float desiredYaw = std::atan2(weaponDy, weaponDx) * 180.0f / 3.14159265358979323846f;
	const float desiredPitch =
		-std::atan2(weaponDz, std::hypot(weaponDx, weaponDy)) * 180.0f / 3.14159265358979323846f;
	weaponInput.aimAligned =
		fireIntentReady ||
		(decision.hasAim &&
		 std::fabs(compat::normalizedAimAngle(desiredYaw - handle.entity->v.v_angle.y)) <= 1.0f &&
		 std::fabs(compat::normalizedAimAngle(desiredPitch - handle.entity->v.v_angle.x)) <= 1.0f);
	weaponInput.stationary =
		std::hypot(handle.entity->v.velocity.x, handle.entity->v.velocity.y) < 10.0f;
	weaponInput.requestFire = weaponInput.targetVisible && weaponInput.aimAligned &&
							  !target.friendlyFireRisk &&
							  services_->service_globals()->time >= actors_[index].attackReadyTimes;
	weaponInput.attacking = actors_[index].managedBotStateMachines.attackOverlayActive();
	compat::CompatibilityObservation publicWeapon = {};
	const compat::ObservationTimingContext weaponTiming = {
		services_->command().managedBotCommandSequences(index), 0U,
		services_->service_managedBotFullUpdateSequences()[index]};
	if (services_->observation().observationAdapter().collectActor(handle.entity, actor, frame,
																   weaponTiming, &publicWeapon) ==
		ObservationAdapterResult::Accepted)
	{
		if (publicWeapon.weapon.silencer.isAvailable())
			weaponInput.silenced = {true, publicWeapon.weapon.silencer.value};
		if (publicWeapon.weapon.reserve.isAvailable() &&
			publicWeapon.weapon.reserve.context.delayTicks <= frame.tick)
		{
			weaponInput.reserveKnown = true;
			weaponInput.reserve = publicWeapon.weapon.reserve.value;
			weaponInput.reserveObservationTick =
				frame.tick - publicWeapon.weapon.reserve.context.delayTicks;
		}
		if (publicWeapon.weapon.burst.isAvailable())
			weaponInput.burstMode = {true, publicWeapon.weapon.burst.value};
		if (publicWeapon.weapon.reloadState.isAvailable() &&
			publicWeapon.weapon.reloadState.value != combat::ReloadState::Unknown)
			weaponInput.reloading = {true, publicWeapon.weapon.reloadState.value ==
											   combat::ReloadState::Reloading};
		if (publicWeapon.weapon.nextPrimaryAttack.isAvailable() &&
			publicWeapon.weapon.nextPrimaryAttack.value > services_->service_globals()->time)
			weaponInput.requestFire = false;
		if (publicWeapon.weapon.nextSecondaryAttack.isAvailable())
			weaponInput.secondaryReady = {true, publicWeapon.weapon.nextSecondaryAttack.value <=
													services_->service_globals()->time};
	}
	const std::uint32_t inventoryMask = static_cast<std::uint32_t>(handle.entity->v.weapons);
	bool havePistol = false;
	for (const unsigned id : {1U, 10U, 11U, 16U, 17U, 26U})
		havePistol = havePistol || (inventoryMask & (1U << id)) != 0U;
	if (weaponValue == 29 && weaponInput.requestFire && weaponInput.targetDistance < 75.0f)
	{
		edict_t *enemy = services_->service_engineFunctions()->pfnPEntityOfEntIndex != nullptr
							 ? services_->service_engineFunctions()->pfnPEntityOfEntIndex(
								   static_cast<int>(targetActor.slot))
							 : nullptr;
		if (enemy != nullptr && enemy->free == 0)
		{
			const float toUsX = handle.entity->v.origin.x - enemy->v.origin.x;
			const float toUsY = handle.entity->v.origin.y - enemy->v.origin.y;
			const float pitchDegrees = enemy->v.v_angle.x + enemy->v.punchangle.x;
			const float yawDegrees = enemy->v.v_angle.y + enemy->v.punchangle.y;
			const float rollDegrees = enemy->v.v_angle.z + enemy->v.punchangle.z;
			if (std::isfinite(pitchDegrees) && std::isfinite(yawDegrees) &&
				std::isfinite(rollDegrees) && std::isfinite(toUsX) && std::isfinite(toUsY))
			{
				const float yaw = yawDegrees * 3.14159265358979323846f / 180.0f;
				const float pitch = pitchDegrees * 3.14159265358979323846f / 180.0f;
				weaponInput.enemyFacingObserver = {
					true, std::cos(pitch) * (toUsX * std::cos(yaw) + toUsY * std::sin(yaw)) > 0.0f};
			}
		}
		if (!weaponInput.enemyFacingObserver.known || weaponInput.enemyFacingObserver.value)
		{
			const auto choice = services_->service_compatibilityRandomSource().nextFloat(
				compat::RandomRequest::floatRequest(
					"Knife.AttackChoice", {actor.slot, actor.generation},
					{services_->command().managedBotCommandSequences(index),
					 services_->service_adapterFrameCount(),
					 services_->service_managedBotFullUpdateSequences()[index]},
					0.0f, 100.0f));
			weaponInput.knifeRollKnown = choice.status == compat::RandomStatus::Ok;
			weaponInput.knifeRoll = choice.value;
		}
	}
	if (compat::WeaponActionPlanner::isGrenade(static_cast<std::uint16_t>(weaponValue)) &&
		weaponInput.targetVisible && !target.friendlyFireRisk &&
		services_->service_engineFunctions()->pfnCVarGetFloat != nullptr &&
		services_->service_engineFunctions()->pfnTraceLine != nullptr &&
		services_->service_engineFunctions()->pfnTraceHull != nullptr)
	{
		const auto launch = compat::solveGrenadeLaunch(
			context.origin,
			{handle.entity->v.velocity.x, handle.entity->v.velocity.y, handle.entity->v.velocity.z},
			targetPosition, services_->service_engineFunctions()->pfnCVarGetFloat("sv_gravity"),
			16.0f);
		if (launch.valid)
		{
			edict_t *intendedTarget =
				services_->service_engineFunctions()->pfnPEntityOfEntIndex != nullptr
					? services_->service_engineFunctions()->pfnPEntityOfEntIndex(
						  static_cast<int>(targetActor.slot))
					: nullptr;
			bool lineClear = true, hullClear = true;
			world::WorldPosition previous = context.origin;
			const unsigned steps = static_cast<unsigned>(std::ceil(launch.flightSeconds / 0.05f));
			for (unsigned step = 0U; step <= steps && lineClear && hullClear; ++step)
			{
				const auto next =
					step == 0U ? launch.origin
							   : compat::grenadePosition(launch, launch.flightSeconds *
																	 static_cast<float>(step) /
																	 static_cast<float>(steps));
				const float start[] = {previous.x, previous.y, previous.z};
				const float end[] = {next.x, next.y, next.z};
				TraceResult line = {}, hull = {};
				services_->service_engineFunctions()->pfnTraceLine(start, end, 0, handle.entity,
																   &line);
				services_->service_engineFunctions()->pfnTraceHull(start, end, 0, 3, handle.entity,
																   &hull);
				lineClear = (line.flFraction >= 1.0f ||
							 (intendedTarget != nullptr && line.pHit == intendedTarget)) &&
							line.fStartSolid == 0 && line.fAllSolid == 0;
				hullClear = (hull.flFraction >= 1.0f ||
							 (intendedTarget != nullptr && hull.pHit == intendedTarget)) &&
							hull.fStartSolid == 0 && hull.fAllSolid == 0;
				previous = next;
				if (lineClear && hullClear && intendedTarget != nullptr &&
					(line.pHit == intendedTarget || hull.pHit == intendedTarget))
					break;
			}
			const auto landing = compat::grenadePosition(launch, launch.flightSeconds);
			const float groundStart[] = {landing.x, landing.y, landing.z};
			const float groundEnd[] = {landing.x, landing.y, landing.z - 72.0f};
			TraceResult ground = {};
			services_->service_engineFunctions()->pfnTraceHull(
				groundStart, groundEnd, 0, 3,
				intendedTarget != nullptr ? intendedTarget : handle.entity, &ground);
			const bool landingSafe = ground.flFraction < 1.0f && ground.fStartSolid == 0 &&
									 ground.fAllSolid == 0 && ground.vecPlaneNormal.z >= 0.7f;
			weaponInput.trajectory = {true, lineClear, hullClear, landingSafe};
			weaponInput.grenadeAimKnown = lineClear && hullClear && landingSafe;
			weaponInput.grenadePitch = launch.pitch - handle.entity->v.punchangle.x;
			weaponInput.grenadeYaw = launch.yaw - handle.entity->v.punchangle.y;
			weaponInput.requestGrenade =
				lineClear && hullClear && landingSafe &&
				services_->service_globals()->time >= actors_[index].attackReadyTimes;
			weaponInput.aimAligned =
				std::fabs(compat::normalizedAimAngle(launch.yaw - handle.entity->v.v_angle.y -
													 handle.entity->v.punchangle.y)) <= 1.0f &&
				std::fabs(compat::normalizedAimAngle(launch.pitch - handle.entity->v.v_angle.x -
													 handle.entity->v.punchangle.x)) <= 1.0f;
			if (weaponInput.requestGrenade)
				proposal.viewAngles = {launch.pitch - handle.entity->v.punchangle.x,
									   launch.yaw - handle.entity->v.punchangle.y,
									   movementAngles.roll};
		}
	}
	const compat::WeaponActionDecision weaponDecision =
		actors_[index].weaponActions.update(weaponInput);
	if (proposal.kind == ActionKind::Fire && !weaponDecision.primary)
	{
		proposal.kind = ActionKind::None;
	}
	if (weaponDecision.primary)
		proposal.kind = ActionKind::Fire;
	if (weaponDecision.switchToPistol && havePistol)
		proposal.kind = ActionKind::SelectPistol;
	proposal.secondaryAttack = weaponDecision.secondary;
	proposal.stopForWeapon = weaponDecision.stopMovement;
	if (weaponDecision.hasRetainedGrenadeAim)
		proposal.viewAngles = {weaponDecision.grenadePitch, weaponDecision.grenadeYaw,
							   movementAngles.roll};
	return proposal;
}

compat::StateUpdateContext BotBehaviorRuntime::makeManagedBotStateContext(
	std::size_t index, const runtime::MovementPhysicsState &before,
	compat::StateBehaviorInput *behavior) const
{
	compat::StateUpdateContext context = {};
	if (index >= services_->service_managedBotHandles().size())
	{
		return context;
	}
	const FakeClientHandle &handle = services_->service_managedBotHandles()[index];
	context.actor = {handle.actor.slot, handle.actor.actorGeneration};
	context.frame = {services_->service_lifecycle().mapGeneration(),
					 services_->service_lifecycle().roundGeneration(),
					 services_->service_adapterFrameCount()};
	context.timestamp =
		services_->service_globals() != nullptr ? services_->service_globals()->time : 0.0f;
	context.fullUpdateSequence = services_->service_managedBotFullUpdateSequences()[index];
	context.fullUpdate = true;
	context.lifeAvailability = handle.entity != nullptr
								   ? compat::ObservationAvailability::Available
								   : compat::ObservationAvailability::Unavailable;
	context.alive = !before.dead;
	context.roundAvailability = services_->service_lifecycle().isMapActive()
									? compat::ObservationAvailability::Available
									: compat::ObservationAvailability::Unavailable;
	context.roundActive = services_->service_lifecycle().isMapActive();
	context.requestTransition = false;
	context.requestedState = compat::CompatibilityStateId::None;
	context.transitionId = compat::StateTransitionId::None;
	context.transitionReason = compat::StateTransitionReason::None;
	context.transitionRequest = compat::ObservationAvailability::Available;
	context.attackObservation = compat::ObservationAvailability::Available;
	context.rngAvailability = compat::ObservationAvailability::Available;
	if (behavior != nullptr)
	{
		*behavior = {};
		behavior->navigationOwned =
			services_->service_compatibilitySurface().configuration().mode ==
			compat::RuntimeMode::Compatibility;
		behavior->navigationReady = services_->service_navPublisher().snapshot().isValid();
		const ManagedObjectiveTargetCache &target = actors_[index].managedBotObjectiveTargets;
		behavior->goalAvailable =
			target.valid &&
			target.mapGeneration == services_->service_lifecycle().mapGeneration() &&
			target.roundGeneration == services_->service_lifecycle().roundGeneration();
		if (behavior->goalAvailable)
		{
			const float dx = before.origin.x - target.target.x;
			const float dy = before.origin.y - target.target.y;
			const float dz = before.origin.z - target.target.z;
			behavior->goalReached = dx * dx + dy * dy + dz * dz <= 64.0f * 64.0f;
		}
		const auto &failure = services_->navigation().managedNavigationFailures(index);
		const bool ownPathFailed =
			failure.actor == context.actor && failure.frame.isValid() &&
			failure.frame.mapGeneration == context.frame.mapGeneration &&
			failure.frame.roundGeneration == context.frame.roundGeneration &&
			failure.frame.tick < context.frame.tick &&
			failure.goalGeneration == target.generation &&
			actors_[index].managedBotStateMachines.state() == compat::CompatibilityStateId::MoveTo;
		behavior->pathFailed =
			ownPathFailed || (managedTeamObjectivePathFailurePending_ &&
							  managedTeamObjectivePathFailureActor_ == context.actor);
		behavior->objectiveActive = behavior->goalAvailable;
		if (behavior->navigationOwned && actors_[index].managedBotStateMachines.state() ==
											 compat::CompatibilityStateId::DefuseBomb)
		{
			behavior->objectiveActive = services_->observation().bombNoticeKnown(index) &&
										services_->observation().bombNoticePlanted(index);
			behavior->defuseReceipt = services_->observation().publicObservations().progressReceipt(
				context.actor, context.frame, context.timestamp,
				actors_[index].managedBotStateMachines.stateTimestamp());
		}
		context.behavior = behavior;
	}
	return context;
}

int BotBehaviorRuntime::resolveManagedBotTeam(
	std::size_t index, const compat::CompatibilityObservation &observation) const
{
	if (index < NativeBotObservation::kClientSlotCount &&
		services_->service_joinControllers()[index].teamConfirmed() &&
		services_->observation().managedBotTeamNumbers(index) >= 1U &&
		services_->observation().managedBotTeamNumbers(index) <= 2U)
	{
		return static_cast<int>(services_->observation().managedBotTeamNumbers(index));
	}
	if (observation.player.team.isAvailable() && observation.player.team.value >= 1 &&
		observation.player.team.value <= 2)
	{
		return observation.player.team.value;
	}
	return 0;
}

void BotBehaviorRuntime::updateManagedBotCompatibilityState(
	std::size_t index, const runtime::MovementPhysicsState &before)
{
	RuntimeProfilerScope scenarioScope(services_->service_runtimeProfiler(),
									   RuntimeProfilerStage::ScenarioUpdate);
	if (index >= NativeBotObservation::kClientSlotCount ||
		!services_->service_managedBotSlots()[index] ||
		services_->service_managedBotHandles()[index].entity == nullptr)
	{
		return;
	}
	compat::StateBehaviorInput stateBehavior = {};
	compat::StateUpdateContext context = makeManagedBotStateContext(index, before, &stateBehavior);
	compat::CompatibilityStateMachine &machine = actors_[index].managedBotStateMachines;
	actors_[index].scenarioDecisions = {};
	if (actors_[index].managedBotStateRounds != 0U &&
		actors_[index].managedBotStateRounds != services_->service_lifecycle().roundGeneration() &&
		machine.isInitialized())
	{
		actors_[index].scenarioPlanners.reset();
		(void)machine.onRoundReset(context);
	}
	actors_[index].managedBotStateRounds = services_->service_lifecycle().roundGeneration();
	if (!machine.isInitialized())
	{
		actors_[index].scenarioPlanners.reset();
		(void)machine.initialize(context);
		if (before.dead)
		{
			(void)machine.onDeath(context);
			actors_[index].managedBotWasDead = true;
		}
		return;
	}
	if (before.dead)
	{
		actors_[index].scenarioPlanners.reset();
		services_->applyNavigationRequest({index, NavigationRequestKind::ClearFailure});
		if (!actors_[index].managedBotWasDead)
		{
			(void)machine.onDeath(context);
		}
		actors_[index].managedBotWasDead = true;
		return;
	}
	if (actors_[index].managedBotWasDead)
	{
		actors_[index].scenarioPlanners.reset();
		(void)machine.onRespawn(context);
		actors_[index].managedBotWasDead = false;
		return;
	}

	const compat::RuntimeMode mode = services_->service_compatibilitySurface().configuration().mode;
	if (mode == compat::RuntimeMode::Compatibility)
	{
		const compat::ObservationTimingContext timing = {
			services_->command().managedBotCommandSequences(index), 0U,
			services_->service_managedBotFullUpdateSequences()[index]};
		compat::CompatibilityObservation observation = {};
		const ObservationAdapterResult observationResult =
			services_->observation().observationAdapter().collectActor(
				services_->service_managedBotHandles()[index].entity, context.actor, context.frame,
				timing, &observation);
		if (observationResult == ObservationAdapterResult::Accepted)
		{
			compat::ScenarioBehaviorInput scenario = {};
			scenario.actor = context.actor;
			scenario.frame = context.frame;
			scenario.timestamp = context.timestamp;
			scenario.selfPositionKnown = std::isfinite(before.origin.x) &&
										 std::isfinite(before.origin.y) &&
										 std::isfinite(before.origin.z);
			scenario.selfPosition = {before.origin.x, before.origin.y, before.origin.z};
			scenario.lifeKnown =
				context.lifeAvailability == compat::ObservationAvailability::Available;
			scenario.alive = context.alive;
			scenario.reactionSeconds = actors_[index].managedProfiles.reactionTimeSeconds;
			scenario.aggression =
				static_cast<float>(actors_[index].managedProfiles.aggression) / 100.0f;
			scenario.attacking = machine.attackOverlayActive();
			scenario.currentDefusing = machine.state() == compat::CompatibilityStateId::DefuseBomb;
			scenario.alreadyEscapingFromBomb =
				machine.state() == compat::CompatibilityStateId::EscapeFromBomb;
			const int observedTeam = resolveManagedBotTeam(index, observation);
			scenario.team = observedTeam == 1	? objectives::TeamRole::Terrorist
							: observedTeam == 2 ? objectives::TeamRole::CounterTerrorist
												: objectives::TeamRole::Unknown;
			compat::PublicActorState publicState = {};
			scenario.blind = services_->observation().publicObservations().read(
								 context.actor, context.frame, &publicState) &&
							 context.timestamp < publicState.blindUntil;
			scenario.isVipKnown = observation.objective.vip.isAvailable();
			scenario.isVip = scenario.isVipKnown && observation.objective.vip.value;
			scenario.bombPlantedKnown = observation.objective.bombPlanted.isAvailable();
			scenario.bombPlanted =
				scenario.bombPlantedKnown && observation.objective.bombPlanted.value;
			if (observation.objective.bombPosition.isAvailable())
			{
				const auto &position = observation.objective.bombPosition.value;
				scenario.bomb.knowledge = compat::ScenarioKnowledge::Remembered;
				scenario.bomb.frame = context.frame;
				scenario.bomb.position = {position.x, position.y, position.z};
				const float dx = position.x - before.origin.x, dy = position.y - before.origin.y,
							dz = position.z - before.origin.z;
				scenario.bomb.pathCost = std::sqrt(dx * dx + dy * dy + dz * dz);
			}
			if (services_->observation().bombNoticeKnown(index))
			{
				scenario.bombPlantedKnown = true;
				scenario.bombPlanted = services_->observation().bombNoticePlanted(index);
			}
			if (scenario.bombPlantedKnown && scenario.bombPlanted)
			{
				const float publicFuse =
					readOptionalCvarFloat(services_->service_engineFunctions(), "mp_c4timer");
				scenario.timerEstimated = services_->observation().publicBombClocks(index).estimate(
					context.frame, context.timestamp, publicFuse, &scenario.bombSecondsRemaining);
			}
			const auto &score = services_->observation().publicScoreStatuses(index)[index];
			if (score.known &&
				score.serial ==
					static_cast<std::uint32_t>(
						services_->service_managedBotHandles()[index].entity->serialnumber) &&
				score.frame.mapGeneration == context.frame.mapGeneration &&
				score.frame.roundGeneration == context.frame.roundGeneration)
			{
				scenario.isVipKnown = true;
				scenario.isVip = (score.flags & 4) != 0;
			}
			scenario.defuseKitKnown = observation.objective.hasDefuseKit.isAvailable();
			scenario.defuseKit =
				scenario.defuseKitKnown && observation.objective.hasDefuseKit.value;
			scenario.radio = &services_->observation().scenarioRadio(index);
			float automaticLeaderRange = (std::numeric_limits<float>::max)();
			if (services_->service_engineFunctions() != nullptr &&
				services_->service_globals() != nullptr &&
				services_->service_engineFunctions()->pfnSzFromIndex != nullptr &&
				services_->service_engineFunctions()->pfnPEntityOfEntIndex != nullptr)
			{
				const char *map = services_->service_engineFunctions()->pfnSzFromIndex(
					services_->service_globals()->mapname);
				scenario.scenario = map != nullptr && std::strncmp(map, "cs_", 3U) == 0
										? compat::PublicScenarioKind::Hostage
									: map != nullptr && std::strncmp(map, "as_", 3U) == 0
										? compat::PublicScenarioKind::Vip
									: map != nullptr && std::strncmp(map, "es_", 3U) == 0
										? compat::PublicScenarioKind::Escape
									: map != nullptr && std::strncmp(map, "de_", 3U) == 0
										? compat::PublicScenarioKind::Bomb
										: compat::PublicScenarioKind::Unknown;
				const float eye[] = {
					before.origin.x +
						services_->service_managedBotHandles()[index].entity->v.view_ofs.x,
					before.origin.y +
						services_->service_managedBotHandles()[index].entity->v.view_ofs.y,
					before.origin.z +
						services_->service_managedBotHandles()[index].entity->v.view_ofs.z};
				const auto project = [&](edict_t *entity, int entityIndex,
										 bool mapKnown) -> compat::ScenarioTarget {
					compat::ScenarioTarget target = {};
					if (entity == nullptr || entity->free != 0)
					{
						return target;
					}
					const bool brush =
						entity->v.solid == SOLID_BSP || entity->v.solid == SOLID_TRIGGER;
					const float point[] = {brush ? (entity->v.absmin.x + entity->v.absmax.x) * 0.5f
												 : entity->v.origin.x,
										   brush ? (entity->v.absmin.y + entity->v.absmax.y) * 0.5f
												 : entity->v.origin.y,
										   brush ? (entity->v.absmin.z + entity->v.absmax.z) * 0.5f
												 : entity->v.origin.z};
					const float dx = point[0] - eye[0], dy = point[1] - eye[1],
								dz = point[2] - eye[2];
					const float planar = std::hypot(dx, dy);
					const float yaw =
						services_->service_managedBotHandles()[index].entity->v.v_angle.y *
						3.14159265358979323846f / 180.0f;
					bool visible = false;
					if (!mapKnown && !scenario.blind &&
						services_->service_engineFunctions()->pfnTraceLine != nullptr &&
						!services_->observation().smokeObservations().blocks(
							{eye[0], eye[1], eye[2]}, {point[0], point[1], point[2]},
							context.timestamp) &&
						(planar < 0.001f ||
						 (dx * std::cos(yaw) + dy * std::sin(yaw)) / planar > 0.5f))
					{
						TraceResult trace = {};
						services_->service_engineFunctions()->pfnTraceLine(
							eye, point, 1, services_->service_managedBotHandles()[index].entity,
							&trace);
						visible = trace.flFraction == 1.0f || trace.pHit == entity;
					}
					if (!mapKnown && !visible)
					{
						return target;
					}
					target.knowledge = mapKnown ? compat::ScenarioKnowledge::Remembered
												: compat::ScenarioKnowledge::Visible;
					target.frame = context.frame;
					// Core generations reserve zero; map entities may publicly have serial zero.
					target.entity = {static_cast<std::uint32_t>(entityIndex),
									 static_cast<std::uint32_t>(entity->serialnumber) + 1U};
					target.position = {point[0], point[1], point[2]};
					target.pathCost = std::sqrt(dx * dx + dy * dy + dz * dz);
					target.inUseRange = target.pathCost <= 64.0f;
					return target;
				};
				const auto nearer = [](const compat::ScenarioTarget &candidate,
									   const compat::ScenarioTarget &current) {
					return candidate.knowledge != compat::ScenarioKnowledge::Unknown &&
						   (current.knowledge == compat::ScenarioKnowledge::Unknown ||
							candidate.pathCost < current.pathCost);
				};
				const int maxEntities =
					(std::max)(0, (std::min)(services_->service_globals()->maxEntities, 2048));
				for (int entityIndex = 1; entityIndex < maxEntities; ++entityIndex)
				{
					edict_t *entity =
						services_->service_engineFunctions()->pfnPEntityOfEntIndex(entityIndex);
					if (entity == nullptr || entity->free != 0)
					{
						continue;
					}
					const char *name =
						services_->service_engineFunctions()->pfnSzFromIndex(entity->v.classname);
					if (name == nullptr)
					{
						continue;
					}
					if (entityIndex <= 32)
					{
						const auto &vipScore = services_->observation().publicScoreStatuses(
							index)[static_cast<std::size_t>(entityIndex - 1)];
						if (vipScore.known &&
							vipScore.serial == static_cast<std::uint32_t>(entity->serialnumber) &&
							vipScore.frame.mapGeneration == context.frame.mapGeneration &&
							vipScore.frame.roundGeneration == context.frame.roundGeneration &&
							(vipScore.flags & 4) != 0 && entity->v.deadflag == DEAD_NO)
						{
							scenario.vip = project(entity, entityIndex, false);
							scenario.vip.actor = {static_cast<std::uint32_t>(entityIndex),
												  static_cast<std::uint32_t>(entity->serialnumber) +
													  1U};
						}
					}
					if (entityIndex <= services_->service_globals()->maxClients &&
						(entity->v.flags & FL_FAKECLIENT) == 0 && entity->v.team == observedTeam &&
						entity->v.deadflag == DEAD_NO && entity->v.health > 0.0f)
					{
						const auto location = project(entity, entityIndex, false);
						if (location.knowledge == compat::ScenarioKnowledge::Visible)
						{
							const float dx = location.position.x - before.origin.x,
										dy = location.position.y - before.origin.y,
										dz = location.position.z - before.origin.z;
							const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
							if (distance < automaticLeaderRange)
							{
								automaticLeaderRange = distance;
								scenario.automaticLeader = {
									{static_cast<std::uint32_t>(entityIndex),
									 static_cast<std::uint32_t>(entity->serialnumber) + 1U},
									scenario.team,
									true,
									true,
									true,
									location};
								scenario.automaticLeader.location.actor =
									scenario.automaticLeader.actor;
							}
						}
					}
					if (std::strcmp(name, "grenade") == 0)
					{
						compat::ObjectiveObservation planted{};
						world::WorldVector knownPosition{};
						bool bombVisible = false;
						if (services_->observation().observationAdapter().collectPlantedBomb(
								entity, name,
								services_->service_engineFunctions()->pfnSzFromIndex(
									entity->v.model),
								context.timestamp, context.actor, context.frame, timing,
								&planted) == ObservationAdapterResult::Accepted &&
							services_->observation().observeObjective(index, entity, true,
																	  &knownPosition, &bombVisible))
						{
							scenario.bombPlantedKnown = true;
							scenario.bombPlanted = true;
							scenario.bomb.knowledge = bombVisible
														  ? compat::ScenarioKnowledge::Visible
														  : compat::ScenarioKnowledge::Remembered;
							scenario.bomb.frame = context.frame;
							scenario.bomb.entity = {
								static_cast<std::uint32_t>(entityIndex),
								static_cast<std::uint32_t>(entity->serialnumber) + 1U};
							scenario.bomb.position = {knownPosition.x, knownPosition.y,
													  knownPosition.z};
							const float dx = knownPosition.x - before.origin.x,
										dy = knownPosition.y - before.origin.y,
										dz = knownPosition.z - before.origin.z;
							scenario.bomb.pathCost = std::sqrt(dx * dx + dy * dy + dz * dz);
							scenario.bomb.inUseRange = scenario.bomb.pathCost <= 64.0f;
							scenario.humanDefusingKnown = true;
							scenario.humanDefusing =
								services_->observation().externalDefuserEntityIndex() != 0U &&
								services_->observation().externalDefuserLeaseUntil() >=
									context.timestamp &&
								services_->observation().externalDefuserMapGeneration() ==
									context.frame.mapGeneration &&
								services_->observation().externalDefuserRoundGeneration() ==
									context.frame.roundGeneration;
						}
					}
					compat::ScenarioTarget *destination = nullptr;
					bool mapKnown = true;
					if (std::strcmp(name, "hostage_entity") == 0 && entity->v.health > 0.0f)
					{
						destination = &scenario.hostage;
						mapKnown = false;
					}
					else if (std::strcmp(name, "func_hostage_rescue") == 0 ||
							 std::strcmp(name, "info_hostage_rescue") == 0)
					{
						destination = &scenario.rescueZone;
					}
					else if (std::strcmp(name, "func_vip_safetyzone") == 0)
					{
						destination = &scenario.vipZone;
					}
					else if (std::strcmp(name, "func_escapezone") == 0)
					{
						destination = &scenario.escapeZone;
					}
					if (destination != nullptr)
					{
						const auto target = project(entity, entityIndex, mapKnown);
						if (nearer(target, *destination))
						{
							*destination = target;
						}
					}
				}
				// Nearby CTs veto motion-based ownership inference. Their public
				// positions never establish escort ownership or provide a target.
				if (scenario.hostage.entity.isValid())
				{
					for (int player = 1; player <= services_->service_globals()->maxClients;
						 ++player)
					{
						edict_t *other =
							services_->service_engineFunctions()->pfnPEntityOfEntIndex(player);
						if (other == nullptr ||
							other == services_->service_managedBotHandles()[index].entity ||
							other->free != 0 || other->v.team != 2 ||
							other->v.deadflag != DEAD_NO || other->v.health <= 0.0f)
							continue;
						const float dx = other->v.origin.x - scenario.hostage.position.x;
						const float dy = other->v.origin.y - scenario.hostage.position.y;
						const float dz = other->v.origin.z - scenario.hostage.position.z;
						if (dx * dx + dy * dy + dz * dz <= 256.0f * 256.0f)
						{
							scenario.hostageCompetitorNearby = true;
							break;
						}
					}
				}
				const auto tracked = actors_[index].scenarioPlanners.followLeader();
				if (actors_[index].scenarioPlanners.isFollowing() && tracked.isValid() &&
					tracked.slot <=
						static_cast<std::uint32_t>(services_->service_globals()->maxClients))
				{
					edict_t *human = services_->service_engineFunctions()->pfnPEntityOfEntIndex(
						static_cast<int>(tracked.slot));
					if (human != nullptr && !human->free &&
						static_cast<std::uint32_t>(human->serialnumber) + 1U == tracked.generation)
					{
						scenario.automaticLeader = {
							tracked,
							human->v.team == 1	 ? objectives::TeamRole::Terrorist
							: human->v.team == 2 ? objectives::TeamRole::CounterTerrorist
												 : objectives::TeamRole::Unknown,
							(human->v.flags & FL_FAKECLIENT) == 0,
							true,
							human->v.deadflag == DEAD_NO && human->v.health > 0.0f,
							project(human, static_cast<int>(tracked.slot), false)};
						scenario.automaticLeader.location.actor = tracked;
					}
				}
				const auto sender = services_->observation().scenarioRadio(index).sender;
				if (sender.isValid() &&
					sender.slot <=
						static_cast<std::uint32_t>(services_->service_globals()->maxClients))
				{
					edict_t *leader = services_->service_engineFunctions()->pfnPEntityOfEntIndex(
						static_cast<int>(sender.slot));
					if (leader != nullptr && leader->free == 0 &&
						static_cast<std::uint32_t>(leader->serialnumber) == sender.generation)
					{
						scenario.leader.actor = sender;
						scenario.leader.team = leader->v.team == 1 ? objectives::TeamRole::Terrorist
											   : leader->v.team == 2
												   ? objectives::TeamRole::CounterTerrorist
												   : objectives::TeamRole::Unknown;
						scenario.leader.human = (leader->v.flags & FL_FAKECLIENT) == 0;
						scenario.leader.lifeKnown = true;
						scenario.leader.alive =
							leader->v.deadflag == DEAD_NO && leader->v.health > 0.0f;
						scenario.leader.location =
							project(leader, static_cast<int>(sender.slot), false);
						scenario.leader.location.actor = sender;
						if (scenario.leader.location.knowledge ==
							compat::ScenarioKnowledge::Visible)
						{
							services_->observation().observeRadioLeaderPosition(
								index, scenario.leader.location);
						}
					}
				}
			}
			const nav::NavSnapshot scenarioNavigation =
				services_->service_navPublisher().snapshot();
			const nav::NavDocument *scenarioDocument = scenarioNavigation.document();
			if (scenario.scenario == compat::PublicScenarioKind::Bomb &&
				scenario.team == objectives::TeamRole::CounterTerrorist &&
				scenario.bombPlantedKnown && scenario.bombPlanted &&
				scenario.bomb.knowledge == compat::ScenarioKnowledge::Unknown &&
				scenarioNavigation.isValid() &&
				scenarioNavigation.mapGeneration() == context.frame.mapGeneration &&
				scenarioDocument != nullptr && services_->service_globals() != nullptr &&
				services_->service_engineFunctions() != nullptr &&
				services_->service_engineFunctions()->pfnPEntityOfEntIndex != nullptr &&
				services_->service_engineFunctions()->pfnSzFromIndex != nullptr)
			{
				compat::ScenarioTarget &search = actors_[index].bombSearchTargets;
				if (search.knowledge != compat::ScenarioKnowledge::Unknown)
				{
					const float dx = search.position.x - before.origin.x,
								dy = search.position.y - before.origin.y,
								dz = search.position.z - before.origin.z;
					if (dx * dx + dy * dy + dz * dz <= 64.0f * 64.0f)
					{
						actors_[index].bombSearchMemory.clear(search.entity);
						search = {};
					}
				}
				if (search.knowledge == compat::ScenarioKnowledge::Unknown)
				{
					std::vector<compat::BombSearchSite> sites;
					nav::NavQuery query(scenarioNavigation);
					nav::NavAreaMatch start{};
					const nav::NavVector self = {before.origin.x, before.origin.y, before.origin.z};
					const bool haveStart =
						query.findNearest3D(self, kMaximumObjectiveDistance, &start) ==
						nav::NavQueryResult::Found;
					const int limit = (std::min)(services_->service_globals()->maxEntities, 2048);
					for (int entityIndex = 1; entityIndex < limit; ++entityIndex)
					{
						edict_t *entity =
							services_->service_engineFunctions()->pfnPEntityOfEntIndex(entityIndex);
						if (entity == nullptr || entity->free)
							continue;
						const char *name = services_->service_engineFunctions()->pfnSzFromIndex(
							entity->v.classname);
						if (name == nullptr || (std::strcmp(name, "func_bomb_target") != 0 &&
												std::strcmp(name, "info_bomb_target") != 0))
							continue;
						compat::BombSearchSite site{};
						site.entity = {static_cast<std::uint32_t>(entityIndex),
									   static_cast<std::uint32_t>(entity->serialnumber) + 1U};
						site.legacy = std::strcmp(name, "info_bomb_target") == 0;
						if (site.legacy)
						{
							site.center = {entity->v.origin.x, entity->v.origin.y,
										   entity->v.origin.z};
							site.extent = {{site.center.x - 256.0f, site.center.y - 256.0f,
											site.center.z - 256.0f},
										   {site.center.x + 256.0f, site.center.y + 256.0f,
											site.center.z + 256.0f}};
						}
						else
						{
							site.extent = {
								{entity->v.absmin.x, entity->v.absmin.y, entity->v.absmin.z},
								{entity->v.absmax.x, entity->v.absmax.y, entity->v.absmax.z}};
							site.center = {(site.extent.lo.x + site.extent.hi.x) * 0.5f,
										   (site.extent.lo.y + site.extent.hi.y) * 0.5f,
										   (site.extent.lo.z + site.extent.hi.z) * 0.5f};
						}
						site.extent.lo.z -= 50.0f;
						site.extent.hi.z += 50.0f;
						nav::NavAreaMatch finish{};
						nav::NavCorridor route{};
						if (haveStart &&
							query.findNearest3D(site.center, kMaximumObjectiveDistance, &finish) ==
								nav::NavQueryResult::Found &&
							query.buildCorridor(start.area, finish.area, nav::NavRouteType::Fastest,
												&route) == nav::NavQueryResult::Found)
							site.travel = route.cost;
						sites.push_back(site);
					}
					const compat::RandomActor randomActor = {context.actor.slot,
															 context.actor.generation};
					const compat::RandomTimingContext randomTiming = {
						services_->command().managedBotCommandSequences(index), 0U,
						services_->service_managedBotFullUpdateSequences()[index]};
					std::size_t selected = 0U;
					if (actors_[index].bombSearchMemory.choose(
							context.frame, sites, services_->service_compatibilityRandomSource(),
							randomActor, randomTiming, &selected))
					{
						nav::NavVector point{};
						nav::NavAreaMatch chosenArea{};
						nav::NavCorridor chosenRoute{};
						if (compat::bombSitePoint(*scenarioDocument, sites[selected], 16U,
												  services_->service_compatibilityRandomSource(),
												  randomActor, randomTiming, &point) &&
							haveStart &&
							query.findNearest3D(point, kMaximumObjectiveDistance, &chosenArea) ==
								nav::NavQueryResult::Found &&
							query.buildCorridor(start.area, chosenArea.area,
												nav::NavRouteType::Fastest,
												&chosenRoute) == nav::NavQueryResult::Found)
						{
							search.knowledge = compat::ScenarioKnowledge::Remembered;
							search.frame = context.frame;
							search.entity = sites[selected].entity;
							search.position = {point.x, point.y, point.z};
							search.pathCost = chosenRoute.cost;
						}
						else
							actors_[index].bombSearchMemory.clear(sites[selected].entity);
					}
				}
				if (search.knowledge != compat::ScenarioKnowledge::Unknown)
				{
					search.frame = context.frame;
					scenario.bombSearch = search;
				}
			}
			if (scenario.scenario == compat::PublicScenarioKind::Bomb &&
				scenario.bombPlantedKnown && scenario.bombPlanted &&
				scenario.bomb.knowledge != compat::ScenarioKnowledge::Unknown &&
				scenarioNavigation.isValid() &&
				scenarioNavigation.mapGeneration() == context.frame.mapGeneration &&
				scenarioDocument != nullptr)
			{
				const nav::NavVector self = {before.origin.x, before.origin.y, before.origin.z};
				const nav::NavVector bomb = {scenario.bomb.position.x, scenario.bomb.position.y,
											 scenario.bomb.position.z};
				scenario.travelTimeEstimated =
					scenario.team == objectives::TeamRole::CounterTerrorist &&
					!scenario.alreadyEscapingFromBomb && !scenario.currentDefusing &&
					compat::estimateBombTravel(
						scenarioNavigation, self, bomb,
						services_->service_managedBotHandles()[index].entity->v.maxspeed,
						kMaximumObjectiveDistance, &scenario.travelSecondsToBomb);
				const bool lateDefuse =
					scenario.team == objectives::TeamRole::CounterTerrorist &&
					scenario.timerEstimated && scenario.travelTimeEstimated &&
					scenario.defuseKitKnown &&
					scenario.bombSecondsRemaining <=
						scenario.travelSecondsToBomb + (scenario.defuseKit ? 5.0f : 10.0f);
				const bool requestedEscape =
					compat::ScenarioBehaviorPlanner::autoEscapeRequested(scenario) ||
					(lateDefuse && !scenario.attacking && !scenario.currentDefusing) ||
					scenario.alreadyEscapingFromBomb ||
					(scenario.radio != nullptr &&
					 scenario.radio->command == compat::ScenarioRadioCommand::GetOut);
				if (requestedEscape)
				{
					nav::AreaId &goal = actors_[index].scenarioEscapeAreas;
					const nav::NavArea *area = scenarioDocument->findArea(goal);
					if (area == nullptr)
					{
						const compat::RandomActor randomActor = {context.actor.slot,
																 context.actor.generation};
						const compat::RandomTimingContext randomTiming = {
							services_->command().managedBotCommandSequences(index), 0U,
							services_->service_managedBotFullUpdateSequences()[index]};
						if (compat::selectBombEscapeArea(
								*scenarioDocument, bomb,
								services_->service_compatibilityRandomSource(), randomActor,
								randomTiming, &goal))
							area = scenarioDocument->findArea(goal);
					}
					if (area != nullptr)
					{
						const auto point = compat::bombAreaCenter(*area);
						scenario.escapeZone.knowledge = compat::ScenarioKnowledge::Remembered;
						scenario.escapeZone.frame = context.frame;
						scenario.escapeZone.position = {point.x, point.y, point.z};
						const float dx = point.x - self.x, dy = point.y - self.y,
									dz = point.z - self.z;
						// A lower-bound ordering value; actual reachability belongs
						// to the state-owned navigation step, not a preflight search.
						scenario.escapeZone.pathCost = std::sqrt(dx * dx + dy * dy + dz * dz);
					}
				}
			}
			const auto validateRadioSender =
				[&](world::ActorKey sender) -> compat::ScenarioSenderValidation {
				compat::ScenarioSenderValidation validation{sender, false, false};
				if (!sender.isValid() || services_->service_globals() == nullptr ||
					services_->service_engineFunctions() == nullptr ||
					services_->service_engineFunctions()->pfnPEntityOfEntIndex == nullptr)
					return validation;
				validation.known = true;
				if (sender.slot >
					static_cast<std::uint32_t>(services_->service_globals()->maxClients))
					return validation;
				edict_t *human = services_->service_engineFunctions()->pfnPEntityOfEntIndex(
					static_cast<int>(sender.slot));
				validation.valid =
					(observedTeam == 1 || observedTeam == 2) && human != nullptr && !human->free &&
					(human->v.flags & FL_CLIENT) != 0 && (human->v.flags & FL_FAKECLIENT) == 0 &&
					human->v.team == observedTeam &&
					static_cast<std::uint32_t>(human->serialnumber) == sender.generation;
				return validation;
			};
			if (scenario.radio != nullptr)
				scenario.radioSenderValidation = validateRadioSender(scenario.radio->sender);
			scenario.pendingRadioSenderValidation =
				validateRadioSender(actors_[index].scenarioPlanners.pendingRadioSender());
			const bool wasFollowing = actors_[index].scenarioPlanners.isFollowing();
			compat::TeamworkInput teamwork{};
			teamwork.actor = context.actor;
			teamwork.frame = context.frame;
			teamwork.now = context.timestamp;
			teamwork.teamwork =
				static_cast<float>(actors_[index].managedProfiles.teamwork) / 100.0f;
			teamwork.roundTimeKnown =
				services_->service_roundFreezeGeneration() ==
					services_->service_lifecycle().roundGeneration() &&
				std::isfinite(services_->service_roundFreezeStartTime()) &&
				context.timestamp >= services_->service_roundFreezeStartTime();
			teamwork.elapsedRound = context.timestamp - services_->service_roundFreezeStartTime();
			teamwork.allowRoguesKnown =
				services_->service_engineFunctions() != nullptr &&
				services_->service_engineFunctions()->pfnCVarGetFloat != nullptr &&
				services_->service_engineFunctions()->pfnCVarGetPointer != nullptr &&
				services_->service_engineFunctions()->pfnCVarGetPointer("bot_allow_rogues") !=
					nullptr;
			const float allowRogues =
				readOptionalCvarFloat(services_->service_engineFunctions(), "bot_allow_rogues");
			teamwork.allowRoguesKnown = teamwork.allowRoguesKnown && std::isfinite(allowRogues);
			teamwork.allowRogues = allowRogues != 0.0f;
			const auto currentState = machine.state();
			const bool sniper = observation.weapon.activeWeaponId.isAvailable() &&
								(observation.weapon.activeWeaponId.value == 3U ||
								 observation.weapon.activeWeaponId.value == 18U ||
								 observation.weapon.activeWeaponId.value == 13U ||
								 observation.weapon.activeWeaponId.value == 24U);
			scenario.busy = scenario.attacking ||
							currentState == compat::CompatibilityStateId::Buy ||
							currentState == compat::CompatibilityStateId::DefuseBomb ||
							currentState == compat::CompatibilityStateId::PlantBomb ||
							actors_[index].scenarioPlanners.awaitingHostageEscort() ||
							(currentState == compat::CompatibilityStateId::Hide && sniper);
			teamwork.busy = scenario.busy;
			teamwork.following = wasFollowing;
			teamwork.followDuration =
				wasFollowing ? context.timestamp - actors_[index].scenarioPlanners.followSince()
							 : 0.0f;
			teamwork.candidate = scenario.automaticLeader.actor;
			teamwork.candidateVisibleHumanFriend =
				scenario.automaticLeader.human && scenario.automaticLeader.lifeKnown &&
				scenario.automaticLeader.alive && scenario.automaticLeader.team == scenario.team &&
				scenario.automaticLeader.location.knowledge == compat::ScenarioKnowledge::Visible;
			teamwork.candidateRange = automaticLeaderRange;
			if (teamwork.candidateVisibleHumanFriend && scenarioNavigation.isValid() &&
				scenarioDocument != nullptr &&
				scenarioNavigation.mapGeneration() == context.frame.mapGeneration)
			{
				nav::NavQuery query(scenarioNavigation);
				nav::NavAreaMatch selfArea{}, humanArea{};
				nav::NavCorridor route{};
				const auto &position = scenario.automaticLeader.location.position;
				if (query.findNearest3D({before.origin.x, before.origin.y, before.origin.z},
										kMaximumObjectiveDistance,
										&selfArea) == nav::NavQueryResult::Found &&
					query.findContaining({position.x, position.y, position.z}, 64.0f, &humanArea) ==
						nav::NavQueryResult::Found &&
					query.buildCorridor(selfArea.area, humanArea.area, nav::NavRouteType::Fastest,
										&route) == nav::NavQueryResult::Found)
				{
					teamwork.candidateReachable = true;
					teamwork.candidateTravel = route.cost;
				}
				for (std::size_t bot = 0U; bot < NativeBotObservation::kClientSlotCount; ++bot)
					if (actors_[bot].scenarioPlanners.isFollowing() &&
						actors_[bot].scenarioPlanners.followLeader().slot ==
							teamwork.candidate.slot)
						++teamwork.followerCount;
			}
			if (scenario.bombPlantedKnown && scenario.bombPlanted &&
				scenario.bomb.knowledge != compat::ScenarioKnowledge::Unknown &&
				services_->service_engineFunctions() != nullptr &&
				services_->service_engineFunctions()->pfnPEntityOfEntIndex != nullptr &&
				services_->service_engineFunctions()->pfnSzFromIndex != nullptr &&
				services_->service_globals() != nullptr)
			{
				for (int entityIndex = 1;
					 entityIndex < (std::min)(services_->service_globals()->maxEntities, 2048);
					 ++entityIndex)
				{
					edict_t *zone =
						services_->service_engineFunctions()->pfnPEntityOfEntIndex(entityIndex);
					if (zone == nullptr || zone->free)
						continue;
					const char *name =
						services_->service_engineFunctions()->pfnSzFromIndex(zone->v.classname);
					if (name == nullptr || (std::strcmp(name, "func_bomb_target") != 0 &&
											std::strcmp(name, "info_bomb_target") != 0))
						continue;
					const bool legacy = std::strcmp(name, "info_bomb_target") == 0;
					const float loX = legacy ? zone->v.origin.x - 256.0f : zone->v.absmin.x;
					const float loY = legacy ? zone->v.origin.y - 256.0f : zone->v.absmin.y;
					const float loZ =
						(legacy ? zone->v.origin.z - 256.0f : zone->v.absmin.z) - 50.0f;
					const float hiX = legacy ? zone->v.origin.x + 256.0f : zone->v.absmax.x;
					const float hiY = legacy ? zone->v.origin.y + 256.0f : zone->v.absmax.y;
					const float hiZ =
						(legacy ? zone->v.origin.z + 256.0f : zone->v.absmax.z) + 50.0f;
					const auto contains = [&](float x, float y, float z) {
						return x >= loX && x <= hiX && y >= loY && y <= hiY && z >= loZ && z <= hiZ;
					};
					if (contains(before.origin.x, before.origin.y, before.origin.z) &&
						contains(scenario.bomb.position.x, scenario.bomb.position.y,
								 scenario.bomb.position.z))
					{
						teamwork.atPlantedBombsite = true;
						break;
					}
				}
			}
			compat::TeamworkDecision cooperative{};
			if (scenario.lifeKnown && scenario.alive)
				cooperative = actors_[index].teamworkPolicies.update(
					teamwork, services_->service_compatibilityRandomSource(),
					{context.actor.slot, context.actor.generation},
					{services_->command().managedBotCommandSequences(index), 0U,
					 services_->service_managedBotFullUpdateSequences()[index]});
			scenario.rogue = cooperative.rogue;
			scenario.automaticFollowRequested = cooperative.available && cooperative.startFollowing;
			scenario.stopFollowingRequested = cooperative.available && cooperative.stopFollowing;
			scenario.autoFollowPermissionInferred = cooperative.humanPermissionInferred;
			if (!cooperative.rogueKnown && !wasFollowing)
				scenario.radio = nullptr;
			actors_[index].scenarioDecisions = actors_[index].scenarioPlanners.update(scenario);
			updateBombGuardTarget(index, scenario, &actors_[index].scenarioDecisions);
			if (wasFollowing && !actors_[index].scenarioPlanners.isFollowing())
				actors_[index].teamworkPolicies.followingStopped(context.timestamp);
			if (scenario.radio != nullptr &&
				scenario.radio->sequence > actors_[index].teamworkRadioApplied &&
				scenario.radio->senderHuman && scenario.radio->team == scenario.team &&
				!scenario.busy && (!scenario.rogue || wasFollowing) &&
				context.timestamp - scenario.radio->receivedTime >=
					1.0f + 2.0f * scenario.reactionSeconds + (scenario.rogue ? 2.0f : 0.0f))
			{
				const auto &decision = actors_[index].scenarioDecisions;
				const auto command = scenario.radio->command;
				const bool allow =
					decision.action == compat::ScenarioAction::Follow &&
					actors_[index].scenarioPlanners.followLeader() == scenario.radio->sender;
				const bool inhibit =
					(command == compat::ScenarioRadioCommand::HoldPosition &&
					 decision.action == compat::ScenarioAction::Guard &&
					 scenario.radio->position.knowledge != compat::ScenarioKnowledge::Unknown) ||
					(command == compat::ScenarioRadioCommand::GoGoGo &&
					 decision.action == compat::ScenarioAction::Hunt) ||
					(command == compat::ScenarioRadioCommand::GetOut &&
					 decision.action == compat::ScenarioAction::Escape &&
					 scenario.bombPlantedKnown && scenario.bombPlanted);
				if ((allow || inhibit) && scenario.radioSenderValidation.known &&
					scenario.radioSenderValidation.valid &&
					scenario.radioSenderValidation.actor == scenario.radio->sender)
				{
					world::ActorKey human = scenario.radio->sender;
					edict_t *entity = services_->service_engineFunctions()->pfnPEntityOfEntIndex(
						static_cast<int>(human.slot));
					if (entity != nullptr && !entity->free && (entity->v.flags & FL_CLIENT) != 0 &&
						(entity->v.flags & FL_FAKECLIENT) == 0 && entity->v.team == observedTeam &&
						static_cast<std::uint32_t>(entity->serialnumber) == human.generation)
					{
						human.generation = static_cast<std::uint32_t>(entity->serialnumber) + 1U;
						actors_[index].teamworkPolicies.observeHumanPolicy(human, context.timestamp,
																		   allow);
						actors_[index].teamworkRadioApplied = scenario.radio->sequence;
					}
				}
			}
			const compat::ScenarioBehaviorDecision &scenarioDecision =
				actors_[index].scenarioDecisions;
			stateBehavior.goalAvailable = scenarioDecision.hasTarget || stateBehavior.goalAvailable;
			stateBehavior.leaderAvailable =
				actors_[index].scenarioPlanners.hasFollowingLeader(scenario);
			stateBehavior.objectiveActive =
				scenarioDecision.hasTarget || stateBehavior.objectiveActive;
			if (machine.state() == compat::CompatibilityStateId::DefuseBomb ||
				machine.state() == compat::CompatibilityStateId::EscapeFromBomb)
				stateBehavior.objectiveActive = scenario.bombPlantedKnown && scenario.bombPlanted;
			stateBehavior.inInteractionRange =
				scenarioDecision.hasTarget && scenarioDecision.target.inUseRange;
			stateBehavior.guardObjectiveEnded =
				machine.state() == compat::CompatibilityStateId::Hide &&
				scenario.team == objectives::TeamRole::CounterTerrorist &&
				scenario.humanDefusingKnown && !scenario.humanDefusing &&
				scenarioDecision.state == compat::CompatibilityStateId::MoveTo;
			if (machine.state() == compat::CompatibilityStateId::DefuseBomb &&
				scenario.humanDefusing &&
				scenario.bomb.knowledge == compat::ScenarioKnowledge::Visible)
				stateBehavior.defuseReceipt = compat::DefuseReceipt::Other;
			const bool ownsCurrentTask = machine.state() != compat::CompatibilityStateId::Idle &&
										 machine.state() != compat::CompatibilityStateId::Hunt &&
										 machine.state() != compat::CompatibilityStateId::MoveTo;
			if (scenarioDecision.action != compat::ScenarioAction::None &&
				scenarioDecision.state != compat::CompatibilityStateId::None &&
				!(ownsCurrentTask &&
				  scenarioDecision.state == compat::CompatibilityStateId::MoveTo) &&
				!(machine.state() == compat::CompatibilityStateId::DefuseBomb &&
				  scenarioDecision.state != compat::CompatibilityStateId::DefuseBomb))
			{
				context.requestedState = scenarioDecision.state;
				context.requestTransition = machine.state() != context.requestedState;
				context.transitionId = compat::StateTransitionId::ExplicitStateChange;
				context.transitionReason = compat::StateTransitionReason::ObjectiveChanged;
			}
			const bool carryingC4 = observation.objective.carryingC4.isAvailable() &&
									observation.objective.carryingC4.value;
			const int team = resolveManagedBotTeam(index, observation);
			if (carryingC4 && team == 1)
			{
				const bool atBombsite = managedActorAtBombSite(index);
				const bool plantPhase = actors_[index].managedBombApproaches.initialized &&
										actors_[index].managedBombApproaches.mapGeneration ==
											context.frame.mapGeneration &&
										actors_[index].managedBombApproaches.roundGeneration ==
											context.frame.roundGeneration &&
										actors_[index].managedBombApproaches.finished;
				stateBehavior.objectiveActive = true;
				stateBehavior.inInteractionRange = plantPhase && atBombsite;
				context.requestedState = plantPhase && atBombsite
											 ? compat::CompatibilityStateId::PlantBomb
										 : plantPhase && stateBehavior.goalAvailable
											 ? compat::CompatibilityStateId::MoveTo
											 : compat::CompatibilityStateId::Hunt;
				context.requestTransition = machine.state() != context.requestedState;
				context.transitionId = compat::StateTransitionId::ExplicitStateChange;
				context.transitionReason = compat::StateTransitionReason::ObjectiveChanged;
			}
		}
	}
	compat::PublicActorState buyingObservation{};
	const float buyMinutes =
		readOptionalCvarFloat(services_->service_engineFunctions(), "mp_buytime");
	const bool shouldBuy =
		!actors_[index].buyingComplete && context.alive &&
		services_->service_globals() != nullptr && std::isfinite(buyMinutes) && buyMinutes > 0.0f &&
		services_->service_globals()->time <=
			services_->service_roundFreezeStartTime() + buyMinutes * 60.0f &&
		services_->observation().publicObservations().read(context.actor, context.frame,
														   &buyingObservation) &&
		buyingObservation.buyZoneKnown && buyingObservation.inBuyZone;
	if (shouldBuy || machine.state() == compat::CompatibilityStateId::Buy)
	{
		context.requestedState =
			shouldBuy ? compat::CompatibilityStateId::Buy : compat::CompatibilityStateId::Idle;
		context.requestTransition = machine.state() != context.requestedState;
		context.transitionId = compat::StateTransitionId::ExplicitStateChange;
		context.transitionReason = compat::StateTransitionReason::ObjectiveChanged;
	}
	else if (!context.requestTransition && !machine.attackOverlayActive())
	{
		const auto &belief = services_->observation().managedBotPerception(index).snapshot();
		const auto &noise = belief.noise();
		stateBehavior.noiseAvailable =
			belief.isValid() &&
			belief.identity().frame.mapGeneration == context.frame.mapGeneration &&
			belief.identity().frame.roundGeneration == context.frame.roundGeneration &&
			noise.active && noise.ready &&
			noise.timestampTick > actors_[index].investigatedNoiseTicks;
		if (stateBehavior.noiseAvailable)
		{
			const float dx = before.origin.x - noise.position.x;
			const float dy = before.origin.y - noise.position.y;
			const float dz = before.origin.z - noise.position.z;
			if (dx * dx + dy * dy + dz * dz <= 50.0f * 50.0f)
			{
				actors_[index].investigatedNoiseTicks = noise.timestampTick;
				stateBehavior.noiseAvailable = false;
				stateBehavior.goalReached = true;
			}
			else if (machine.state() == compat::CompatibilityStateId::Hunt ||
					 machine.state() == compat::CompatibilityStateId::Idle)
			{
				context.requestedState = compat::CompatibilityStateId::InvestigateNoise;
				context.requestTransition = true;
				context.transitionId = compat::StateTransitionId::ExplicitStateChange;
				context.transitionReason = compat::StateTransitionReason::ObjectiveChanged;
			}
		}
	}
	const auto previousState = machine.state();
	const float previousStateTimestamp = machine.stateTimestamp();
	stateBehavior.navigation.actor = context.actor;
	stateBehavior.navigation.frame = context.frame;
	stateBehavior.navigation.fullUpdateSequence = context.fullUpdateSequence;
	actors_[index].managedStateInputs = stateBehavior;
	(void)machine.update(context);
	if (previousState != machine.state())
	{
		services_->applyNavigationRequest({index, NavigationRequestKind::ClearStep});
		// Reference state entry computes or abandons its own path. Never carry
		// Idle's roam corridor into a newly assigned MoveTo task.
		services_->applyNavigationRequest({index, NavigationRequestKind::DestroyPath});
		if (previousState == compat::CompatibilityStateId::DefuseBomb ||
			previousState == compat::CompatibilityStateId::PlantBomb ||
			previousState == compat::CompatibilityStateId::Hide)
			services_->applyNavigationRequest({index, NavigationRequestKind::ResetProgress});
		if (machine.state() == compat::CompatibilityStateId::Idle &&
			(previousState == compat::CompatibilityStateId::DefuseBomb ||
			 previousState == compat::CompatibilityStateId::PlantBomb))
		{
			actors_[index].managedBotObjectiveActions = {};
			actors_[index].scenarioDecisions = {};
			if (services_->service_runtimeProfiler().enabled())
				profiledLogConsole(
					services_->service_runtimeProfiler(), services_->service_pluginId(),
					"profile objectiveExit bot_id=%u frame=%u state_from=%s receipt=%u "
					"elapsed=%.3f",
					context.actor.slot, context.frame.tick, compat::toString(previousState),
					static_cast<unsigned>(stateBehavior.defuseReceipt),
					context.timestamp - previousStateTimestamp);
		}
	}
	if (stateBehavior.pathFailed && previousState == compat::CompatibilityStateId::MoveTo &&
		machine.state() == compat::CompatibilityStateId::Idle)
	{
		// The reference state's failure exit abandons this path/goal. Do not
		// let the outer objective loop immediately reuse its old MoveTo intent.
		services_->applyNavigationRequest({index, NavigationRequestKind::DestroyPath});
		actors_[index].managedBotObjectiveTargets = {};
		actors_[index].scenarioDecisions = {};
		if (services_->service_runtimeProfiler().enabled())
			profiledLogConsole(
				services_->service_runtimeProfiler(), services_->service_pluginId(),
				"profile navigationFailure bot_id=%u frame=%u state_from=%s state_to=%s",
				context.actor.slot, context.frame.tick, compat::toString(previousState),
				compat::toString(machine.state()));
	}
	if (!stateBehavior.pathFailed || !machine.attackOverlayActive())
		services_->applyNavigationRequest({index, NavigationRequestKind::ClearFailure});
}

void BotBehaviorRuntime::onActorRemoved(std::size_t index)
{
	actors_[index].bombGuardPlanners.reset();
	actors_[index].scenarioEscapeAreas = 0U;
	actors_[index].bombSearchMemory.reset();
	actors_[index].managedStateInputs = {};
	actors_[index].bombSearchTargets = {};
	actors_[index].teamworkPolicies.reset();
	actors_[index].teamworkRadioApplied = 0U;
	actors_[index].investigatedNoiseTicks = 0U;
	actors_[index].weaponActions.reset();
	actors_[index].buyControllers.reset();
	actors_[index].managedBotCombat = combat::CombatController();
	actors_[index].managedBotObjectives = objectives::RoundObjectivePlanner();
	actors_[index].managedBotObjectiveTargets = {};
	actors_[index].managedBombApproaches = {};
	actors_[index].managedBotStateMachines = compat::CompatibilityStateMachine();
	actors_[index].managedBotStateRounds = 0U;
	actors_[index].managedBotWasDead = false;
}

void BotBehaviorRuntime::onActorRemembered(std::size_t index, const FakeClientHandle &handle)
{
	const world::ActorKey actor = {handle.actor.slot, handle.actor.actorGeneration};
	actors_[index].managedBotCombat = combat::CombatController(actor);
	actors_[index].managedBotObjectives = objectives::RoundObjectivePlanner(actor);
	actors_[index].managedBotStateMachines = compat::CompatibilityStateMachine(actor);
	actors_[index].managedBotStateRounds = services_->service_lifecycle().roundGeneration();
	actors_[index].managedBotWasDead = false;
	actors_[index].lastCombatTimes = 0.0f;
	actors_[index].buyControllers.reset();
	actors_[index].buyingComplete = false;
	actors_[index].escapingWithKnife = false;
	actors_[index].buyReceiptTimeoutLogged = false;
}

void BotBehaviorRuntime::onMovementResetShared()
{
	reset_managedStateInputs();
	managedTeamObjectiveCoordinator_.reset(services_->service_lifecycle().mapGeneration(),
										   services_->service_lifecycle().roundGeneration());
	managedTeamObjectiveAssignments_ = {};
	managedTeamObjectiveAssignmentMapGeneration_ = services_->service_lifecycle().mapGeneration();
	managedTeamObjectiveAssignmentRoundGeneration_ =
		services_->service_lifecycle().roundGeneration();
	managedTeamObjectiveAssignmentFrame_ = 0U;
	managedTeamObjectiveAssignmentsValid_ = false;
	managedTeamObjectivePathFailurePending_ = false;
	managedTeamObjectivePathFailureActor_ = {};
	managedTeamObjectivePathFailureGeneration_ = 0U;
	managedTeamObjectivePathFailureStartArea_ = 0U;
	managedTeamObjectivePathFailureFrame_ = 0U;
	reset_managedBotObjectiveActions();
	fill_managedBotStateRounds(0U);
	fill_managedBotWasDead(false);
}

void BotBehaviorRuntime::onMovementResetActor(std::size_t index)
{
	actors_[index].managedBotCombat = combat::CombatController();
	actors_[index].managedBotObjectives = objectives::RoundObjectivePlanner();
	actors_[index].managedBotObjectiveTargets = {};
	actors_[index].managedBombApproaches = {};
	actors_[index].managedBotObjectiveActions = {};
	actors_[index].managedBotStateMachines = compat::CompatibilityStateMachine();
}

void BotBehaviorRuntime::noteNavigationFailure(const FakeClientHandle &handle,
											   runtime::NavRoamResult roamResult,
											   const runtime::NavRoamDecision &roamDecision)
{
	const bool objectivePathFailed =
		(roamResult == runtime::NavRoamResult::NoRoute ||
		 roamResult == runtime::NavRoamResult::ReplanRequired) &&
		(roamDecision.failureReason == runtime::NavFailureReason::PathSearchFailed ||
		 roamDecision.failureReason == runtime::NavFailureReason::UnsafeDrop ||
		 roamDecision.failureReason == runtime::NavFailureReason::NavApplyRejected ||
		 roamDecision.failureReason == runtime::NavFailureReason::MovementNotProduced ||
		 (roamDecision.failureReason == runtime::NavFailureReason::RecoveryNoProgress &&
		  roamDecision.recoveryCandidateFailed));
	if (objectivePathFailed && roamDecision.goalKind == runtime::NavGoalKind::Objective)
	{
		const world::ActorKey actor = {handle.actor.slot, handle.actor.actorGeneration};
		const team::TeamObjectiveAssignment *assignment = findManagedTeamObjectiveAssignment(actor);
		if (assignment != nullptr)
		{
			managedTeamObjectivePathFailurePending_ = true;
			managedTeamObjectivePathFailureActor_ = actor;
			managedTeamObjectivePathFailureGeneration_ = assignment->generation;
			// A portal-area switch alone does not clear a physical drop veto.
			// Zero keeps the coordinator's bounded retry lease across that jitter.
			managedTeamObjectivePathFailureStartArea_ =
				roamDecision.failureReason == runtime::NavFailureReason::UnsafeDrop
					? 0U
					: (roamDecision.currentArea != 0U ? roamDecision.currentArea
													  : roamDecision.recoveryArea);
			managedTeamObjectivePathFailureFrame_ = services_->service_adapterFrameCount();
		}
	}
}

void BotBehaviorRuntime::resetRoundBuyingState()
{
	reset_buyingComplete();
	reset_escapingWithKnife();
	reset_buyReceiptTimeoutLogged();
}

void BotBehaviorRuntime::beginRoundPlantDecision()
{
	reset_buyControllers();
	if (services_->service_globals() != nullptr &&
		services_->service_compatibilitySurface().configuration().mode ==
			compat::RuntimeMode::Compatibility)
	{
		roundPlantDecision_.begin(services_->service_lifecycle().mapGeneration(),
								  services_->service_lifecycle().roundGeneration(),
								  services_->service_globals()->time,
								  services_->service_compatibilityRandomSource());
	}
}

void BotBehaviorRuntime::resetRoundTactics()
{
	reset_scenarioEscapeAreas();
	reset_bombSearchMemory();
	reset_bombSearchTargets();
	reset_teamworkPolicies();
	reset_teamworkRadioApplied();
}

void BotBehaviorRuntime::resetRoundNoiseInvestigation()
{
	reset_investigatedNoiseTicks();
}

void BotBehaviorRuntime::resetRoundObjectiveCoordination()
{
	fill_managedBotObjectiveActions(ManagedObjectiveActionState{});
	managedTeamObjectiveAssignmentsValid_ = false;
	managedTeamObjectivePathFailurePending_ = false;
	managedTeamObjectivePathFailureActor_ = {};
	managedTeamObjectivePathFailureGeneration_ = 0U;
	managedTeamObjectivePathFailureStartArea_ = 0U;
	managedTeamObjectivePathFailureFrame_ = 0U;
}

void BotBehaviorRuntime::beginMapPlantDecision()
{
	roundPlantDecision_ = {};
	if (services_->service_globals() != nullptr &&
		services_->service_compatibilitySurface().configuration().mode ==
			compat::RuntimeMode::Compatibility)
	{
		roundPlantDecision_.begin(services_->service_lifecycle().mapGeneration(),
								  services_->service_lifecycle().roundGeneration(),
								  services_->service_globals()->time,
								  services_->service_compatibilityRandomSource());
	}
}

void BotBehaviorRuntime::invalidateObjectiveAssignments()
{
	managedTeamObjectiveAssignmentsValid_ = false;
}

void BotBehaviorRuntime::assignProfile(std::size_t index, const compat::ProfileRecord &profile)
{
	actors_[index].managedProfiles = profile;
}

void BotBehaviorRuntime::observeRadioReceipt(std::size_t index)
{
	++actors_[index].outgoingRadioReceipts;
}

void BotBehaviorRuntime::observeRecognizedTargetChange(std::size_t index, float now)
{
	actors_[index].attackReadyTimes = now + actors_[index].managedProfiles.attackDelaySeconds;
}

void BotBehaviorRuntime::resetBombApproach(std::size_t index)
{
	actors_[index].managedBombApproaches = {};
}

void BotBehaviorRuntime::observeC4SelectionDispatched(std::size_t index,
													  const FakeClientHandle &handle)
{
	ManagedObjectiveActionState &state = actors_[index].managedBotObjectiveActions;
	if (state.active && state.actor.slot == handle.actor.slot &&
		state.actor.generation == handle.actor.actorGeneration &&
		state.kind == team::TeamObjectiveKind::PlantC4)
	{
		state.c4SelectionCommandDispatched = true;
		ActionAdapter::recordPlantAttemptDispatched(state.plantAttempt,
													services_->service_globals() != nullptr
														? services_->service_globals()->time
														: std::numeric_limits<float>::quiet_NaN());
	}
}

void BotBehaviorRuntime::observeC4AttackDispatched(std::size_t index,
												   const FakeClientHandle &handle,
												   std::uint16_t buttons)
{
	ManagedObjectiveActionState &actionState = actors_[index].managedBotObjectiveActions;
	if (actionState.active && actionState.actor.slot == handle.actor.slot &&
		actionState.actor.generation == handle.actor.actorGeneration &&
		actionState.kind == team::TeamObjectiveKind::PlantC4 &&
		actionState.mapGeneration == services_->service_lifecycle().mapGeneration() &&
		actionState.roundGeneration == services_->service_lifecycle().roundGeneration())
	{
		actionState.c4AttackInputDispatched = true;
		ActionAdapter::recordPlantAttemptDispatched(actionState.plantAttempt,
													services_->service_globals() != nullptr
														? services_->service_globals()->time
														: std::numeric_limits<float>::quiet_NaN());
		if (services_->service_runtimeProfiler().enabled() &&
			(services_->service_adapterFrameCount() % 16U) == 0U && gpMetaUtilFuncs != nullptr &&
			gpMetaUtilFuncs->pfnLogConsole != nullptr && services_->service_pluginId() != nullptr)
		{
			profiledLogConsole(
				services_->service_runtimeProfiler(), services_->service_pluginId(),
				"profile plantInput actor=%u actor_generation=%u map=%u round=%u "
				"frame=%u stage=attack_dispatched buttons=%u",
				static_cast<unsigned int>(handle.actor.slot),
				static_cast<unsigned int>(handle.actor.actorGeneration),
				static_cast<unsigned int>(services_->service_lifecycle().mapGeneration()),
				static_cast<unsigned int>(services_->service_lifecycle().roundGeneration()),
				static_cast<unsigned int>(services_->service_adapterFrameCount()),
				static_cast<unsigned int>(buttons));
		}
	}
}

void BotBehaviorRuntime::observeNeutralC4AttackDispatched(std::size_t index,
														  const FakeClientHandle &handle)
{
	ManagedObjectiveActionState &actionState = actors_[index].managedBotObjectiveActions;
	if (actionState.active && actionState.actor.slot == handle.actor.slot &&
		actionState.actor.generation == handle.actor.actorGeneration &&
		actionState.kind == team::TeamObjectiveKind::PlantC4)
	{
		actionState.c4AttackInputDispatched = true;
		ActionAdapter::recordPlantAttemptDispatched(actionState.plantAttempt,
													services_->service_globals() != nullptr
														? services_->service_globals()->time
														: std::numeric_limits<float>::quiet_NaN());
	}
}

} // namespace metamod
} // namespace astrabot
