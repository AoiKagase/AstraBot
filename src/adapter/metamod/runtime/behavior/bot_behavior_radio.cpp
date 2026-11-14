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
constexpr int kMaximumChatterFileBytes = 1024 * 1024;
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
	if (outgoingRadioMap_ != services_->service_lifecycle().mapGeneration())
	{
		outgoingRadioMap_ = services_->service_lifecycle().mapGeneration();
		outgoingChatterCatalog_ = {};
		outgoingVoiceBanks_.clear();
		outgoingMissingPhraseLogged_.fill(false);
		if (services_->service_engineFunctions()->pfnLoadFileForMe != nullptr &&
			services_->service_engineFunctions()->pfnFreeFile != nullptr)
		{
			int length = 0;
			char filename[] = "BotChatter.db";
			unsigned char *bytes = services_->service_engineFunctions()->pfnLoadFileForMe(filename, &length);
			if (bytes != nullptr)
			{
				if (length > 0 && length <= kMaximumChatterFileBytes)
				{
					(void)outgoingChatterCatalog_.loadText(
						reinterpret_cast<const char *>(bytes), static_cast<std::size_t>(length));
					outgoingVoiceBanks_.push_back({"BotChatter.db", {}, 0U});
					(void)outgoingVoiceBanks_.back().catalog.loadText(
						reinterpret_cast<const char *>(bytes), static_cast<std::size_t>(length));
				}
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
	input.reactionSeconds = actors_[index].managedProfiles.reactionTimeSeconds;
	input.inCombat = actors_[index].managedBotStateMachines.attackOverlayActive();
	for (int slot = 1; slot <= (std::min)(kMaximumClientSlots, services_->service_globals()->maxClients); ++slot)
	{
		edict_t *player = services_->service_engineFunctions()->pfnPEntityOfEntIndex(slot);
		if (player == nullptr || player == handle.entity || player->free || (player->v.flags & FL_CLIENT) == 0 ||
			player->v.deadflag != DEAD_NO || player->v.health <= 0)
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
	auto output = actors_[index].outgoingRadioSchedulers.update(input, outgoingChatterCatalog_, voicePhrases);
	if (verbosity != nullptr && std::strcmp(verbosity, "off") == 0 && output.phrase != compat::ChatterPhrase::None)
	{
		actors_[index].outgoingRadioSchedulers.discardPending();
		return;
	}
	if (verbosity != nullptr && std::strcmp(verbosity, "minimal") == 0 &&
		output.phrase != compat::ChatterPhrase::None && !outgoingChatterCatalog_.important(output.phrase))
	{
		actors_[index].outgoingRadioSchedulers.discardPending();
		return;
	}
	if (output.status == compat::OutgoingRadioStatus::Ready && voice && output.phrase != compat::ChatterPhrase::None)
	{
		float duration = 0.0f;
		if (sendChatterVoice(index, output.phrase, input.visibleEnemyCount, *voice, &duration))
		{
			actors_[index].outgoingRadioSchedulers.confirmSent(input, output);
			actors_[index].outgoingRadioAttempts = 0U;
			actors_[index].outgoingRadioAttemptChoices = {};
			outgoingTeamLastSpeech_[input.team] = input.now;
			outgoingTeamBusyUntil_[input.team] = input.now + duration + 0.1f;
			outgoingTeamCurrentPhrase_[input.team] = output.phrase;
			return;
		}
	}
	if (output.status == compat::OutgoingRadioStatus::Ready && output.menu == 0)
	{
		output.status = compat::OutgoingRadioStatus::MissingPhrase;
		actors_[index].outgoingRadioSchedulers.discardPending();
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
		actors_[index].outgoingRadioSchedulers.discardPending();
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
		actors_[index].outgoingRadioSchedulers.confirmSent(input, output);
		actors_[index].outgoingRadioAttempts = 0U;
		actors_[index].outgoingRadioAttemptChoices = {};
		outgoingTeamLastSpeech_[input.team] = services_->service_globals()->time;
		outgoingTeamBusyUntil_[input.team] =
			services_->service_globals()->time + ASTRABOT_BOT_BEHAVIOR_RADIO_TEAM_SPEECH_BUSY_SECONDS;
		outgoingTeamCurrentPhrase_[input.team] = output.phrase;
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
