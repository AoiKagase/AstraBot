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
#define ASTRABOT_BOT_BEHAVIOR_RADIO_NEXT_PHRASE_GAP_SECONDS 0.1f
}
using namespace runtime_detail;

compat::ChatterScheduleAdmission BotBehaviorRuntime::chatterAdmission(std::size_t index)
{
	const auto context = reactionContext(index);
	if (!context.valid())
		return {};
	auto &actor = actors_[index];
	actor.confirmedSpeech.synchronizeOwner(context);
	return {context, services_->service_lifecycle().slotGeneration(context.actor.slot),
		actor.confirmedSpeech.lifetimeRevision(), &actor.chatterAdmissions};
}

compat::ChatterScheduleEntry BotBehaviorRuntime::activeTeamChatter(std::size_t index, int team, bool *current)
{
	*current = false;
	const auto engine = services_->service_engineFunctions();
	const auto &handles = services_->service_managedBotHandles();
	constexpr int kEligibilityFlags = FL_CLIENT | FL_DORMANT | FL_SPECTATOR;
	struct PeerReceipt
	{
		bool managed = false;
		edict_t *entity = nullptr;
		world::ActorKey actor{};
		compat::ReactionContext context{};
		int serial = 0;
		bool free = false;
		int flags = 0;
		std::uint32_t connection = 0;
		std::uint64_t revision = 0;
		bool teamKnown = false;
		std::uint8_t publicTeam = 0;
		int entityTeam = 0;
		string_t name = 0;
		std::uint64_t queueRevision = 0;
		bool hasPending = false;
		compat::ChatterScheduleEntry speaking{};
	};
	const auto snapshotPeer = [&](std::size_t peerIndex)
	{
		PeerReceipt receipt{};
		receipt.managed = services_->service_managedBotSlots()[peerIndex];
		if (!receipt.managed)
		{
			return receipt;
		}
		const auto &handle = handles[peerIndex];
		receipt.entity = handle.entity;
		receipt.actor = {handle.actor.slot, handle.actor.actorGeneration};
		receipt.context = reactionContext(peerIndex);
		receipt.connection = services_->service_lifecycle().slotGeneration(handle.actor.slot);
		receipt.revision = actors_[peerIndex].confirmedSpeech.revision();
		receipt.teamKnown = services_->observation_publicTeamKnown(peerIndex);
		receipt.publicTeam = services_->observation_teamInfoTeams(peerIndex);
		if (receipt.entity)
		{
			receipt.serial = receipt.entity->serialnumber;
			receipt.free = receipt.entity->free != 0;
			receipt.flags = receipt.entity->v.flags & kEligibilityFlags;
			receipt.entityTeam = receipt.entity->v.team;
			receipt.name = receipt.entity->v.netname;
		}
		receipt.queueRevision = actors_[peerIndex].outgoingRadioSchedulers.statements().revision();
		receipt.hasPending = !actors_[peerIndex].outgoingRadioSchedulers.statements().empty();
		receipt.speaking = actors_[peerIndex].confirmedSpeech.scheduledStatement();
		return receipt;
	};
	const auto sameStatement = [](const compat::ChatterScheduleEntry &a, const compat::ChatterScheduleEntry &b)
	{
		return a.owner.actor == b.owner.actor && a.owner.frame == b.owner.frame &&
			a.owner.entityGeneration == b.owner.entityGeneration && a.owner.now == b.owner.now &&
			a.owner.alive == b.owner.alive && a.connection == b.connection && a.ownerRevision == b.ownerRevision &&
			a.source == b.source && a.topic == b.topic && a.created == b.created && a.start == b.start &&
			a.order == b.order && a.place == b.place && a.subject == b.subject && a.speaking == b.speaking;
	};
	const auto samePeer = [&](const PeerReceipt &a, const PeerReceipt &b)
	{
		if (a.managed != b.managed)
		{
			return false;
		}
		if (!a.managed)
		{
			return true;
		}
		if (!(a.entity == b.entity && a.actor == b.actor && a.context.actor == b.context.actor &&
				a.context.frame == b.context.frame && a.context.entityGeneration == b.context.entityGeneration &&
				a.context.now == b.context.now && a.context.alive == b.context.alive && a.serial == b.serial &&
				a.free == b.free && a.flags == b.flags && a.connection == b.connection && a.revision == b.revision &&
				a.teamKnown == b.teamKnown && a.publicTeam == b.publicTeam && a.entityTeam == b.entityTeam &&
				a.name == b.name))
			return false;
		return a.queueRevision == b.queueRevision && a.hasPending == b.hasPending &&
			sameStatement(a.speaking, b.speaking);
	};
	std::array<PeerReceipt, kMaximumClientSlots> peers{};
	for (std::size_t i = 0; i < peers.size(); ++i)
	{
		peers[i] = snapshotPeer(i);
	}
	const auto ownCurrent = [&]()
	{
		return samePeer(peers[index], snapshotPeer(index));
	};
	compat::ChatterTeamSelection selection;
	for (std::size_t peerIndex = 0; peerIndex < peers.size(); ++peerIndex)
	{
		const auto &peer = peers[peerIndex];
		const auto &context = peer.context;
		if (!peer.managed || !context.valid() || !peer.entity || !(peer.flags & FL_CLIENT) ||
			(peer.flags & FL_DORMANT) || peer.name == 0)
		{
			continue;
		}
		if (!peer.hasPending && !compat::validChatterScheduleEntry(peer.speaking))
			continue;
		const int peerTeam = services_->observation_resolvePlayerTeam(peer.entity);
		if (!ownCurrent() || !samePeer(peer, snapshotPeer(peerIndex)))
		{
			return {};
		}
		bool named = true;
		if (engine->pfnSzFromIndex)
		{
			const char *name = engine->pfnSzFromIndex(peer.name);
			if (!ownCurrent() || !samePeer(peer, snapshotPeer(peerIndex)))
			{
				return {};
			}
			named = name && name[0] != '\0';
		}
		if (peerTeam != team || !named)
		{
			continue;
		}
		const auto append = [&](const compat::ChatterScheduleEntry &entry)
		{
			if (!compat::validChatterScheduleEntry(entry))
			{
				return true;
			}
			if (!(entry.owner.actor == context.actor) || entry.owner.entityGeneration != context.entityGeneration ||
				entry.owner.frame.mapGeneration != context.frame.mapGeneration ||
				entry.owner.frame.roundGeneration != context.frame.roundGeneration ||
				entry.owner.frame.tick > context.frame.tick || entry.owner.now > context.now ||
				entry.connection != peer.connection ||
				entry.ownerRevision != actors_[peerIndex].confirmedSpeech.lifetimeRevision())
			{
				return true;
			}
			return selection.consider(entry);
		};
		// Retained speech retains its native insertion position among pending nodes.
		bool visitedSpeaker = false;
		actors_[peerIndex].outgoingRadioSchedulers.statements().visitSchedules(
			[&](const compat::ChatterScheduleEntry &entry)
			{
				if (!visitedSpeaker && compat::validChatterScheduleEntry(peer.speaking) &&
					(peer.speaking.start < entry.start ||
						(peer.speaking.start == entry.start && peer.speaking.order < entry.order)))
				{
					visitedSpeaker = true;
					if (!append(peer.speaking))
						return false;
				}
				if (compat::sameChatterStatement(entry, peer.speaking))
				{
					visitedSpeaker = true;
					return append(peer.speaking);
				}
				return append(entry);
			});
		if (!visitedSpeaker)
			append(peer.speaking);
	}
	// Exclusion is part of the receipt: later callbacks may requalify an earlier peer.
	if (!ownCurrent())
	{
		return {};
	}
	for (std::size_t i = 0; i < peers.size(); ++i)
	{
		if (!samePeer(peers[i], snapshotPeer(i)))
		{
			return {};
		}
	}
	*current = true;
	return selection.finish(peers[index].context.now);
}

void BotBehaviorRuntime::updateOutgoingPublicRadio(std::size_t index)
{
	if (index >= services_->service_managedBotHandles().size() || services_->service_globals() == nullptr ||
		services_->service_engineFunctions() == nullptr ||
		services_->service_engineFunctions()->pfnPEntityOfEntIndex == nullptr)
		return;
	const auto &handle = services_->service_managedBotHandles()[index];
	if (handle.entity == nullptr || handle.entity->free)
		return;
	const auto receiptContext = reactionContext(index);
	actors_[index].confirmedSpeech.synchronizeOwner(receiptContext);
	const auto receiptRevision = actors_[index].confirmedSpeech.revision();
	const auto receiptConnection = services_->service_lifecycle().slotGeneration(receiptContext.actor.slot);
	const auto receiptTeamKnown = services_->observation_publicTeamKnown(index);
	const auto receiptTeam = services_->observation_teamInfoTeams(index);
	const int receiptEntityTeam = handle.entity->v.team;
	const auto receiptCurrent = [&]()
	{
		// Dead/spectating observers still maintain the existing public radio update.
		// Transport completion separately requires a valid living speech receipt.
		const auto current = reactionContext(index);
		return receiptContext.actor.isValid() && receiptContext.frame.isValid() &&
			receiptContext.entityGeneration != 0 && std::isfinite(receiptContext.now) && receiptContext.now >= 0 &&
			receiptContext.actor == current.actor && receiptContext.entityGeneration == current.entityGeneration &&
			receiptContext.frame == current.frame && receiptContext.now == current.now &&
			receiptContext.alive == current.alive && actors_[index].confirmedSpeech.revision() == receiptRevision &&
			services_->service_lifecycle().slotGeneration(receiptContext.actor.slot) == receiptConnection &&
			services_->observation_publicTeamKnown(index) == receiptTeamKnown &&
			services_->observation_teamInfoTeams(index) == receiptTeam && handle.entity->v.team == receiptEntityTeam;
	};
	if (!receiptCurrent())
		return;
	const char *verbosity = services_->service_engineFunctions()->pfnCVarGetString != nullptr
		? services_->service_engineFunctions()->pfnCVarGetString("bot_chatter")
		: nullptr;
	ensureOutgoingChatterCatalog();
	if (!receiptCurrent())
		return;
	if (outgoingRadioRound_ != services_->service_lifecycle().roundGeneration())
	{
		outgoingRadioRound_ = services_->service_lifecycle().roundGeneration();
		// Round recognition already cleared old nodes before current-round event admission.
		fill_outgoingRadioAttempts(0U);
		fill_outgoingRadioAttemptTimes(-1.0f);
		fill_outgoingRadioAttemptChoices(compat::RadioMenuChoice{});
		outgoingTeamLastSpeech_.fill(-1.0f);
		outgoingTeamBusyUntil_.fill(0.0f);
		outgoingTeamCurrentPhrase_.fill(compat::ChatterPhrase::None);
		outgoingTeamCurrentPlace_.fill(0);
	}
	compat::OutgoingRadioInput input{};
	input.actor = receiptContext.actor;
	input.frame = receiptContext.frame;
	input.now = receiptContext.now;
	input.alive = handle.entity->v.deadflag == DEAD_NO && handle.entity->v.health > 0.0f;
	input.team = services_->observation_resolvePlayerTeam(handle.entity);
	if (!receiptCurrent())
		return;
	if (input.team != kTerroristTeam && input.team != kCounterTerroristTeam)
		return;
	input.teamLastSpeech = outgoingTeamLastSpeech_[input.team];
	// Native active statements, including retained speakers, own arbitration.
	input.teamBusyUntil = 0;
	input.teamCurrentPhrase = compat::ChatterPhrase::None;
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
	if (!receiptCurrent())
		return;
	// Count the observer's current public FOV/LOS/smoke-filtered contacts. A
	// selected combat target is only one contact and cannot express outnumbering.
	world::WorldSnapshot belief;
	world::ActorKey selected{};
	world::WorldPosition selectedPosition{};
	compat::PublicActorState ownObservation{};
	const bool blind = services_->observation_publicObservations().read(input.actor, input.frame, &ownObservation) &&
		services_->service_globals()->time < ownObservation.blindUntil;
	const bool haveBelief = !blind && prepareManagedPerception(index, &belief, &selected, &selectedPosition);
	if (!receiptCurrent())
		return;
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
	if (!receiptCurrent())
		return;
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
	const auto mode = compat::parseChatterVerbosity(verbosity);
	const bool voiceMode = mode != compat::ChatterVerbosity::Radio;
	auto *voice = voiceMode ? chatterBank(index) : nullptr;
	if (!receiptCurrent())
		return;
	std::uint32_t voicePhrases = 0U;
	if (voice)
		for (unsigned phrase = 1U; phrase < static_cast<unsigned>(compat::ChatterPhrase::Count); ++phrase)
			if (voice->has(static_cast<compat::ChatterPhrase>(phrase)))
				voicePhrases |= 1U << phrase;
	input.admission = chatterAdmission(index);
	input.verbosity = mode;
	input.speaking = actors_[index].confirmedSpeech.scheduledStatement();
	actors_[index].outgoingRadioSchedulers.prepare(input, outgoingChatterCatalog_);
	compat::OutgoingRadioDecision output{};
	const bool direct = actors_[index].outgoingRadioSchedulers.hasDirect();
	bool scanCurrent = true;
	compat::ChatterScheduleEntry selectedStatement{};
	if (!direct)
	{
		selectedStatement = activeTeamChatter(index, input.team, &scanCurrent);
	}
	if (!scanCurrent || !receiptCurrent())
		return;
	if (direct)
	{
		// Escape direct radio has no dependency on another bot's statement scan.
		(void)actors_[index].confirmedSpeech.completeIfElapsed(receiptContext, input.friendCount == 0);
	}
	const auto action = [&]()
	{
		const bool ownSelected = selectedStatement.source != compat::ChatterScheduleSource::None &&
			selectedStatement.owner.actor == input.actor;
		if (ownSelected && selectedStatement.speaking)
		{
			(void)actors_[index].confirmedSpeech.completeIfElapsed(receiptContext, input.friendCount == 0);
			if (!direct)
				return;
		}
		if (!direct && !ownSelected)
			return;
		const auto selectedHandle = actors_[index].outgoingRadioSchedulers.statements().handle(selectedStatement);
		input.canStart = true;
		output =
			actors_[index].outgoingRadioSchedulers.decide(input, outgoingChatterCatalog_, voicePhrases, selectedHandle);
		const auto discardOutput = [&]()
		{
			if (direct)
				actors_[index].outgoingRadioSchedulers.discardDirect(output.directToken);
			else
				actors_[index].outgoingRadioSchedulers.statements().erase(selectedHandle);
		};
		const auto confirmOutput = [&]()
		{
			actors_[index].outgoingRadioSchedulers.confirmSent(input, output);
		};
		if (!direct)
			actors_[index].confirmedSpeech.prepareSchedule(selectedStatement);
		if (output.status == compat::OutgoingRadioStatus::Ready && voice &&
			output.phrase != compat::ChatterPhrase::None)
		{
			float duration = 0.0f;
			const auto speechRevision = actors_[index].confirmedSpeech.revision();
			const bool sent = sendChatterVoice(index, output.phrase,
				output.phrase == compat::ChatterPhrase::EnemySpotted ? input.visibleEnemyCount : -1, *voice, &duration,
				output.place);
			if (!actors_[index].confirmedSpeech.canComplete(receiptContext, reactionContext(index), speechRevision))
				return;
			if (sent)
			{
				if (!actors_[index].confirmedSpeech.record(receiptContext, output.phrase,
						input.now + duration + ASTRABOT_BOT_BEHAVIOR_RADIO_NEXT_PHRASE_GAP_SECONDS, speechRevision))
					return;
				confirmOutput();
				actors_[index].outgoingRadioAttempts = 0U;
				actors_[index].outgoingRadioAttemptChoices = {};
				outgoingTeamLastSpeech_[input.team] = input.now;
				outgoingTeamBusyUntil_[input.team] =
					input.now + duration + ASTRABOT_BOT_BEHAVIOR_RADIO_NEXT_PHRASE_GAP_SECONDS;
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
		const auto speechRevision = actors_[index].confirmedSpeech.revision();
		const std::uint32_t receiptBefore = actors_[index].outgoingRadioReceipts;
		char slot[] = {static_cast<char>('0' + output.slot), '\0'};
		if (!actors_[index].confirmedSpeech.canComplete(receiptContext, reactionContext(index), speechRevision))
			return;
		if (output.phrase != compat::ChatterPhrase::None &&
			!actors_[index].confirmedSpeech.beginSpeaking(receiptContext, output.phrase,
				input.now + ASTRABOT_BOT_BEHAVIOR_RADIO_TEAM_SPEECH_BUSY_SECONDS, speechRevision))
		{
			return;
		}
		const bool opened = services_->command_dispatchClientCommand(handle.entity, menu, nullptr);
		if (!actors_[index].confirmedSpeech.canComplete(receiptContext, reactionContext(index), speechRevision))
			return;
		const bool menuSelected = opened && services_->command_dispatchClientCommand(handle.entity, "menuselect", slot);
		if (!actors_[index].confirmedSpeech.canComplete(receiptContext, reactionContext(index), speechRevision))
			return;
		if (opened)
			(void)services_->command_dispatchClientCommand(handle.entity, "menuselect", "10");
		// A client command reaching the GameDLL does not prove that its radio
		// quota/cooldown accepted it. Only public SendAudio confirms a sent phrase.
		if (!actors_[index].confirmedSpeech.canComplete(receiptContext, reactionContext(index), speechRevision))
			return;
		if (menuSelected && actors_[index].outgoingRadioReceipts != receiptBefore)
		{
			if (output.phrase != compat::ChatterPhrase::None &&
				!actors_[index].confirmedSpeech.record(receiptContext, output.phrase,
					services_->service_globals()->time + ASTRABOT_BOT_BEHAVIOR_RADIO_TEAM_SPEECH_BUSY_SECONDS,
					speechRevision))
				return;
			confirmOutput();
			actors_[index].outgoingRadioAttempts = 0U;
			actors_[index].outgoingRadioAttemptChoices = {};
			outgoingTeamLastSpeech_[input.team] = services_->service_globals()->time;
			if (output.phrase != compat::ChatterPhrase::None)
			{
				outgoingTeamBusyUntil_[input.team] =
					services_->service_globals()->time + ASTRABOT_BOT_BEHAVIOR_RADIO_TEAM_SPEECH_BUSY_SECONDS;
				outgoingTeamCurrentPhrase_[input.team] = output.phrase;
				outgoingTeamCurrentPlace_[input.team] = output.place;
			}
		}
	};
	action();
	const auto currentContext = reactionContext(index);
	if (!compat::sameChatterReceipt(receiptContext, currentContext) ||
		services_->service_lifecycle().slotGeneration(receiptContext.actor.slot) != receiptConnection ||
		actors_[index].confirmedSpeech.lifetimeRevision() != input.admission.ownerRevision)
		return;
	compat::ChatterScheduleEntry teammate{};
	if (!direct)
	{
		teammate = activeTeamChatter(index, input.team, &scanCurrent);
		if (!scanCurrent)
			return;
		if (teammate.owner.actor == input.actor)
			teammate = {};
	}
	input.speaking = actors_[index].confirmedSpeech.scheduledStatement();
	actors_[index].outgoingRadioSchedulers.maintain(input, teammate);
}

void BotBehaviorRuntime::observeRadioReceipt(std::size_t index)
{
	++actors_[index].outgoingRadioReceipts;
}

} // namespace metamod
} // namespace astrabot
#undef ASTRABOT_BOT_BEHAVIOR_RADIO_RADIO_RECEIPT_RETRY_SECONDS
#undef ASTRABOT_BOT_BEHAVIOR_RADIO_TEAM_SPEECH_BUSY_SECONDS
#undef ASTRABOT_BOT_BEHAVIOR_RADIO_NEXT_PHRASE_GAP_SECONDS
