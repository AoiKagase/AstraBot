#include "runtime/behavior/bot_behavior_runtime.hpp"
#include "runtime/shared/bot_runtime_support.hpp"
namespace astrabot
{
namespace metamod
{
namespace
{
constexpr int kTerroristTeam = 1;
constexpr int kCounterTerroristTeam = 2;
constexpr int kCommandRadioMenu = 1;
constexpr int kGroupRadioMenu = 2;
constexpr unsigned kVipMapPrefixLength = 3U;
constexpr int kMaximumClientSlots = 32;
constexpr unsigned kMaximumRadioAttempts = 3U;
// Keep literal evaluation precision on x87; each macro is undefined below.
#define ASTRABOT_BOT_BEHAVIOR_RADIO_RADIO_RECEIPT_RETRY_SECONDS 0.2f
#define ASTRABOT_BOT_BEHAVIOR_RADIO_TEAM_SPEECH_BUSY_SECONDS 2.1f
}
using namespace runtime_detail;

void BotBehaviorRuntime::updateOutgoingPublicRadio(std::size_t index)
{
	if (index >= services_->service_managedBotHandles().size() || services_->service_globals() == nullptr ||
		services_->service_engineFunctions() == nullptr ||
		services_->service_engineFunctions()->pfnPEntityOfEntIndex == nullptr)
		return;
	const auto &handle = services_->service_managedBotHandles()[index];
	if (handle.entity == nullptr || handle.entity->free)
		return;
	const char *verbosity = services_->service_engineFunctions()->pfnCVarGetString != nullptr
		? services_->service_engineFunctions()->pfnCVarGetString("bot_chatter")
		: nullptr;
	ensureOutgoingChatterCatalog();
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
		outgoingTeamCurrentPlace_.fill(0);
	}
	compat::OutgoingRadioInput input{};
	input.actor = {handle.actor.slot, handle.actor.actorGeneration};
	input.frame = {services_->service_lifecycle().mapGeneration(), services_->service_lifecycle().roundGeneration(),
		services_->service_adapterFrameCount()};
	input.now = services_->service_globals()->time;
	input.alive = handle.entity->v.deadflag == DEAD_NO && handle.entity->v.health > 0.0f;
	input.team = services_->observation_resolvePlayerTeam(handle.entity);
	if (input.team != kTerroristTeam && input.team != kCounterTerroristTeam)
		return;
	input.teamLastSpeech = outgoingTeamLastSpeech_[input.team];
	input.teamBusyUntil = outgoingTeamBusyUntil_[input.team];
	input.teamCurrentPhrase = outgoingTeamCurrentPhrase_[input.team];
	input.teamCurrentPlace = outgoingTeamCurrentPlace_[input.team];
	const auto reportNav = services_->service_navPublisher().snapshot();
	input.navRevision = reportNav.isValid() ? reportNav.revision() : 0;
	input.roundOver = services_->service_roundOver();
	input.noiseReport = actors_[index].noiseReport;
	input.reactionSeconds = actors_[index].managedProfiles.reactionTimeSeconds;
	input.inCombat = actors_[index].managedBotStateMachines.attackOverlayActive();
	for (int slot = 1; slot <= (std::min)(kMaximumClientSlots, services_->service_globals()->maxClients); ++slot)
	{
		edict_t *player = services_->service_engineFunctions()->pfnPEntityOfEntIndex(slot);
		if (player == nullptr || player == handle.entity || player->free || (player->v.flags & FL_CLIENT) == 0 ||
			player->serialnumber < 0 || (player->v.flags & (FL_SPECTATOR | FL_DORMANT)) != 0 ||
			player->v.deadflag != DEAD_NO || !(player->v.health > 0))
			continue;
		if (services_->observation_resolvePlayerTeam(player) == input.team)
		{
			++input.friendCount;
		}
		else if (services_->observation_resolvePlayerTeam(player) == kTerroristTeam ||
			services_->observation_resolvePlayerTeam(player) == kCounterTerroristTeam)
			++input.enemiesRemaining;
	}
	// Count the observer's current public FOV/LOS/smoke-filtered contacts. A
	// selected combat target is only one contact and cannot express outnumbering.
	world::WorldSnapshot belief;
	world::ActorKey selected{};
	world::WorldPosition selectedPosition{};
	compat::PublicActorState ownObservation{};
	const bool blind = services_->observation_publicObservations().read(input.actor, input.frame, &ownObservation) &&
		services_->service_globals()->time < ownObservation.blindUntil;
	const bool haveBelief = !blind && prepareManagedPerception(index, &belief, &selected, &selectedPosition);
	const auto contacts = actors_[index].outgoingRadioContactMemory.update(input.actor, input.frame, input.now,
		haveBelief ? &belief : nullptr, blind, input.friendCount, input.enemiesRemaining);
	input.visibleEnemyCount = contacts.enemies;
	input.nearbyFriends = contacts.friends;
	const auto &order = services_->observation_scenarioRadio(index);
	const auto &decision = actors_[index].scenarioDecisions;
	if (order.senderHuman && static_cast<int>(order.team) == input.team)
	{
		input.humanOrderSequence = order.sequence;
		input.humanOrderReceived = order.receivedTime;
		input.humanOrderAccepted = decision.action == compat::ScenarioAction::Follow ||
			decision.action == compat::ScenarioAction::Guard || decision.action == compat::ScenarioAction::Hunt ||
			decision.action == compat::ScenarioAction::Escape;
		input.humanOrderHelp = order.command == compat::ScenarioRadioCommand::NeedBackup;
		outgoingTeamLastSpeech_[input.team] = (std::max)(outgoingTeamLastSpeech_[input.team], order.receivedTime);
	}
	const char *mapName = services_->service_engineFunctions()->pfnSzFromIndex != nullptr
		? services_->service_engineFunctions()->pfnSzFromIndex(services_->service_globals()->mapname)
		: nullptr;
	input.vipEscaping = mapName != nullptr && std::strncmp(mapName, "as_", kVipMapPrefixLength) == 0 &&
		input.team == kCounterTerroristTeam && decision.action == compat::ScenarioAction::Escape;
	input.plantingBomb = actors_[index].managedBotStateMachines.state() == compat::CompatibilityStateId::PlantBomb;
	input.escortingHostages = decision.escortInferred;
	input.bombLocationUnknown = input.team == kCounterTerroristTeam && services_->observation_bombNoticeKnown(index) &&
		services_->observation_bombNoticePlanted(index) && decision.action == compat::ScenarioAction::Move &&
		decision.hasTarget && actors_[index].bombSearchTargets.knowledge != compat::ScenarioKnowledge::Unknown &&
		actors_[index].bombSearchTargets.frame.mapGeneration == input.frame.mapGeneration &&
		actors_[index].bombSearchTargets.frame.roundGeneration == input.frame.roundGeneration &&
		decision.target.entity == actors_[index].bombSearchTargets.entity;
	const bool voiceMode = !verbosity || (std::strcmp(verbosity, "off") != 0 && std::strcmp(verbosity, "radio") != 0);
	auto *voice = voiceMode ? chatterBank(index) : nullptr;
	std::uint32_t voicePhrases = 0U;
	if (voice)
		for (unsigned phrase = 1U; phrase < static_cast<unsigned>(compat::ChatterPhrase::Count); ++phrase)
			if (voice->has(static_cast<compat::ChatterPhrase>(phrase)))
				voicePhrases |= 1U << phrase;
	compat::EnemyCountSpeechInput countInput{};
	countInput.actor = input.actor;
	countInput.frame = input.frame;
	countInput.entityGeneration =
		handle.entity->serialnumber >= 0 ? static_cast<std::uint32_t>(handle.entity->serialnumber) + 1U : 0;
	countInput.now = input.now;
	countInput.alive = input.alive;
	countInput.roundOver = input.roundOver;
	countInput.friendsRemaining = input.friendCount;
	countInput.enemiesRemaining = input.enemiesRemaining;
	countInput.teamBusyUntil = input.teamBusyUntil;
	countInput.teamReportingRemaining = compat::isRemainingEnemyPhrase(input.teamCurrentPhrase);
	// REMAINING_ENEMY_COUNT is a context statement, not an Important phrase.
	countInput.enabled = !verbosity || (std::strcmp(verbosity, "off") != 0 && std::strcmp(verbosity, "minimal") != 0);
	auto output = actors_[index].outgoingRadioSchedulers.update(input, outgoingChatterCatalog_, voicePhrases);
	// All topics maintain lifetime before arbitration; only the selected topic
	// evaluates speech context, including when an older topic is still delayed.
	countInput.canStart = false;
	actors_[index].enemyCountChatter.update(countInput);
	compat::KillPraiseSpeechInput praiseInput{};
	praiseInput.actor = input.actor;
	praiseInput.frame = input.frame;
	praiseInput.entityGeneration = countInput.entityGeneration;
	praiseInput.now = input.now;
	praiseInput.alive = input.alive;
	praiseInput.roundOver = input.roundOver;
	praiseInput.friendsRemaining = input.friendCount;
	praiseInput.teamBusyUntil = input.teamBusyUntil;
	praiseInput.teamPraising = compat::isKillPraisePhrase(input.teamCurrentPhrase);
	praiseInput.allowedPhrases = actors_[index].killPraise.active() ? allowedKillPraisePhrases(verbosity) : 0;
	praiseInput.canStart = false;
	actors_[index].killPraise.update(praiseInput);
	enum class SpeechOwner
	{
		Generic,
		EnemyCount,
		KillPraise
	};
	SpeechOwner owner = SpeechOwner::Generic;
	bool havePending = actors_[index].outgoingRadioSchedulers.hasPending();
	float earliest = actors_[index].outgoingRadioSchedulers.scheduledStart();
	if (output.status != compat::OutgoingRadioStatus::MissingPhrase)
	{
		if (actors_[index].enemyCountChatter.active() &&
			(!havePending || actors_[index].enemyCountChatter.scheduledStart() < earliest))
		{
			owner = SpeechOwner::EnemyCount;
			havePending = true;
			earliest = actors_[index].enemyCountChatter.scheduledStart();
		}
		if (actors_[index].killPraise.active() &&
			(!havePending || actors_[index].killPraise.scheduledStart() < earliest))
			owner = SpeechOwner::KillPraise;
	}
	const bool countChosen = owner == SpeechOwner::EnemyCount;
	const bool praiseChosen = owner == SpeechOwner::KillPraise;
	countInput.canStart = countChosen;
	praiseInput.canStart = praiseChosen;
	const auto countOutput = actors_[index].enemyCountChatter.update(countInput);
	const auto praiseOutput = actors_[index].killPraise.update(praiseInput);
	if (countChosen || praiseChosen)
	{
		output = {};
		const auto phrase = countChosen && countOutput.ready
			? compat::remainingEnemyPhrase(countOutput.enemiesRemaining)
			: praiseChosen && praiseOutput.ready ? praiseOutput.phrase
												 : compat::ChatterPhrase::None;
		if (phrase != compat::ChatterPhrase::None)
		{
			const auto choice = outgoingChatterCatalog_.lookup(phrase);
			const bool available = choice.menu != 0 || (voicePhrases & (1U << static_cast<unsigned>(phrase))) != 0;
			output = {available ? compat::OutgoingRadioStatus::Ready : compat::OutgoingRadioStatus::MissingPhrase,
				phrase, choice.menu, choice.slot, 0};
		}
	}
	const auto discardOutput = [&]()
	{
		if (praiseChosen)
			actors_[index].killPraise.discard();
		else if (countChosen)
			actors_[index].enemyCountChatter.discard();
		else
			actors_[index].outgoingRadioSchedulers.discardPending();
	};
	const auto confirmOutput = [&]()
	{
		if (praiseChosen)
			actors_[index].killPraise.discard();
		else if (countChosen)
			actors_[index].enemyCountChatter.discard();
		else
			actors_[index].outgoingRadioSchedulers.confirmSent(input, output);
	};
	const bool suppressNoise = verbosity &&
		(std::strcmp(verbosity, "off") == 0 ||
			(std::strcmp(verbosity, "minimal") == 0 &&
				!outgoingChatterCatalog_.important(compat::ChatterPhrase::HeardNoise)));
	if (suppressNoise)
	{
		actors_[index].outgoingRadioSchedulers.discardNoiseReport(input.noiseReport);
	}
	if (verbosity != nullptr && std::strcmp(verbosity, "off") == 0 && output.phrase != compat::ChatterPhrase::None)
	{
		discardOutput();
		return;
	}
	if (verbosity != nullptr && std::strcmp(verbosity, "minimal") == 0 &&
		output.phrase != compat::ChatterPhrase::None && !outgoingChatterCatalog_.important(output.phrase))
	{
		discardOutput();
		return;
	}
	if (output.status == compat::OutgoingRadioStatus::Ready && voice && output.phrase != compat::ChatterPhrase::None)
	{
		float duration = 0.0f;
		if (sendChatterVoice(index, output.phrase, countChosen || praiseChosen ? -1 : input.visibleEnemyCount, *voice,
				&duration, output.place))
		{
			confirmOutput();
			actors_[index].outgoingRadioAttempts = 0U;
			actors_[index].outgoingRadioAttemptChoices = {};
			outgoingTeamLastSpeech_[input.team] = input.now;
			outgoingTeamBusyUntil_[input.team] = input.now + duration + 0.1f;
			outgoingTeamCurrentPhrase_[input.team] = output.phrase;
			outgoingTeamCurrentPlace_[input.team] = output.place;
			return;
		}
	}
	if (output.status == compat::OutgoingRadioStatus::Ready && output.menu == 0)
	{
		output.status = compat::OutgoingRadioStatus::MissingPhrase;
		discardOutput();
	}
	if (output.status == compat::OutgoingRadioStatus::MissingPhrase)
	{
		if (countChosen || praiseChosen)
			discardOutput();
		const auto phrase = static_cast<unsigned>(output.phrase);
		if (phrase < outgoingMissingPhraseLogged_.size() && !outgoingMissingPhraseLogged_[phrase])
		{
			outgoingMissingPhraseLogged_[phrase] = true;
			if (gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr &&
				services_->service_pluginId() != nullptr)
				gpMetaUtilFuncs->pfnLogConsole(services_->service_pluginId(),
					"outgoing chatter unavailable phrase=%s "
					"host_radio_mapping=%d voice_bank=unavailable",
					compat::ChatterRadioCatalog::name(output.phrase), outgoingChatterCatalog_.loaded() ? 1 : 0);
		}
		return;
	}
	if (output.status != compat::OutgoingRadioStatus::Ready)
		return;
	const char *menu = output.menu == kCommandRadioMenu ? "radio1"
		: output.menu == kGroupRadioMenu				? "radio2"
														: "radio3";
	if (actors_[index].outgoingRadioAttemptGenerations != handle.actor.actorGeneration ||
		actors_[index].outgoingRadioAttemptChoices.menu != output.menu ||
		actors_[index].outgoingRadioAttemptChoices.slot != output.slot)
	{
		actors_[index].outgoingRadioAttemptGenerations = handle.actor.actorGeneration;
		actors_[index].outgoingRadioAttemptChoices = {output.menu, output.slot};
		actors_[index].outgoingRadioAttempts = 0U;
		actors_[index].outgoingRadioAttemptTimes = -1.0f;
	}
	if (actors_[index].outgoingRadioAttempts >= kMaximumRadioAttempts)
	{
		discardOutput();
		actors_[index].outgoingRadioAttemptChoices = {};
		if (gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr &&
			services_->service_pluginId() != nullptr)
			gpMetaUtilFuncs->pfnLogConsole(services_->service_pluginId(),
				"outgoing radio receipt unavailable bot=%u menu=%d slot=%d attempts=3",
				static_cast<unsigned>(handle.actor.slot), output.menu, output.slot);
		return;
	}
	if (actors_[index].outgoingRadioAttemptTimes >= 0.0f &&
		services_->service_globals()->time - actors_[index].outgoingRadioAttemptTimes <
			ASTRABOT_BOT_BEHAVIOR_RADIO_RADIO_RECEIPT_RETRY_SECONDS)
		return;
	actors_[index].outgoingRadioAttemptTimes = services_->service_globals()->time;
	++actors_[index].outgoingRadioAttempts;
	const std::uint32_t receiptBefore = actors_[index].outgoingRadioReceipts;
	char slot[] = {static_cast<char>('0' + output.slot), '\0'};
	const bool opened = services_->command_dispatchClientCommand(handle.entity, menu, nullptr);
	const bool menuSelected = opened && services_->command_dispatchClientCommand(handle.entity, "menuselect", slot);
	if (opened)
		(void)services_->command_dispatchClientCommand(handle.entity, "menuselect", "10");
	// A client command reaching the GameDLL does not prove that its radio
	// quota/cooldown accepted it. Only public SendAudio confirms a sent phrase.
	if (menuSelected && actors_[index].outgoingRadioReceipts != receiptBefore)
	{
		confirmOutput();
		actors_[index].outgoingRadioAttempts = 0U;
		actors_[index].outgoingRadioAttemptChoices = {};
		outgoingTeamLastSpeech_[input.team] = services_->service_globals()->time;
		outgoingTeamBusyUntil_[input.team] =
			services_->service_globals()->time + ASTRABOT_BOT_BEHAVIOR_RADIO_TEAM_SPEECH_BUSY_SECONDS;
		outgoingTeamCurrentPhrase_[input.team] = output.phrase;
		outgoingTeamCurrentPlace_[input.team] = output.place;
	}
}

void BotBehaviorRuntime::observeRadioReceipt(std::size_t index)
{
	++actors_[index].outgoingRadioReceipts;
}

} // namespace metamod
} // namespace astrabot
#undef ASTRABOT_BOT_BEHAVIOR_RADIO_RADIO_RECEIPT_RETRY_SECONDS
#undef ASTRABOT_BOT_BEHAVIOR_RADIO_TEAM_SPEECH_BUSY_SECONDS
