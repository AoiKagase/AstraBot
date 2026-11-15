#include "runtime/behavior/bot_behavior_runtime.hpp"
#include <cstring>
#include <limits>
namespace astrabot
{
namespace metamod
{
namespace
{
int voiceMessageId(plid_t plugin, const char *name)
{
	int size = 0;
	return gpMetaUtilFuncs && gpMetaUtilFuncs->pfnGetUserMsgID && plugin
		? gpMetaUtilFuncs->pfnGetUserMsgID(plugin, name, &size)
		: 0;
}
}
void BotBehaviorRuntime::ensureOutgoingChatterCatalog()
{
	constexpr int kMaximumChatterFileBytes = 1024 * 1024;
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
}

compat::ChatterVoiceCatalog *BotBehaviorRuntime::chatterBank(std::size_t index)
{
	if (!services_ || index >= actors_.size())
		return nullptr;
	const auto engine = services_->service_engineFunctions();
	const char *configured = actors_[index].managedProfiles.voiceBank;
	std::string name = *configured ? configured : "BotChatter.db";
	for (auto &c : name)
		if (c == '\\')
			c = '/';
	if (!compat::ChatterVoiceCatalog::validPath(name))
		return nullptr;
	ChatterBank *found = nullptr;
	for (auto &bank : outgoingVoiceBanks_)
		if (bank.name == name)
		{
			found = &bank;
			break;
		}
	if (!found)
	{
		if (outgoingVoiceBanks_.size() >= 16U)
			return nullptr;
		outgoingVoiceBanks_.push_back({name, {}, 0U});
		found = &outgoingVoiceBanks_.back(); // Negative loads are cached for this map too.
		if (engine && engine->pfnLoadFileForMe && engine->pfnFreeFile)
		{
			int length = 0;
			auto *data = engine->pfnLoadFileForMe(&name[0], &length);
			if (data)
			{
				if (length > 0 && length <= static_cast<int>(compat::ChatterVoiceCatalog::kMaximumFileBytes))
					(void)found->catalog.loadText(
						reinterpret_cast<const char *>(data), static_cast<std::size_t>(length));
				engine->pfnFreeFile(data);
			}
		}
	}
	if (!found->filesChecked)
	{
		if (engine && engine->pfnGetFileSize)
			found->catalog.retainAvailable(
				[engine](const std::string &sample)
				{
					std::string path = "sound/" + sample;
					return engine->pfnGetFileSize(&path[0]) > 0;
				});
		found->filesChecked = true;
	}
	const auto round = services_->service_lifecycle().roundGeneration();
	if (found->shuffledRound != round)
	{
		const auto &handle = services_->service_managedBotHandles()[index];
		const compat::RandomTimingContext timing = {services_->command_managedBotCommandSequences(index), 0U,
			services_->service_managedBotFullUpdateSequences()[index]};
		if (!found->catalog.shuffle(services_->service_compatibilityRandomSource(),
				{handle.actor.slot, handle.actor.actorGeneration}, timing))
			return nullptr;
		found->shuffledRound = round;
	}
	return &found->catalog;
}
std::uint32_t BotBehaviorRuntime::allowedFixedChatterPhrases(
	const char *verbosity, compat::ChatterPhrase first, compat::ChatterPhrase last)
{
	// The native global phrase definitions/flags belong to the default catalog.
	// Profile banks provide speakables only; do not load/probe them in a death event.
	if (verbosity && std::strcmp(verbosity, "off") == 0)
		return 0;
	ensureOutgoingChatterCatalog();
	const bool minimal = verbosity && std::strcmp(verbosity, "minimal") == 0;
	std::uint32_t mask = 0;
	for (unsigned i = static_cast<unsigned>(first); i <= static_cast<unsigned>(last); ++i)
	{
		const auto phrase = static_cast<compat::ChatterPhrase>(i);
		if (outgoingChatterCatalog_.defined(phrase) && (!minimal || outgoingChatterCatalog_.important(phrase)))
			mask |= 1U << i;
	}
	return mask;
}

void BotBehaviorRuntime::observeRadioPreference(edict_t *recipient, bool ignored)
{
	if (!services_ || !recipient || recipient->free || !(recipient->v.flags & FL_CLIENT))
		return;
	const auto engine = services_->service_engineFunctions();
	if (!engine || !engine->pfnIndexOfEdict)
		return;
	const int slot = engine->pfnIndexOfEdict(recipient);
	if (slot < 1 || slot > static_cast<int>(outgoingRadioPreferences_.size()))
		return;
	outgoingRadioPreferences_[static_cast<std::size_t>(slot - 1)] = {
		recipient->serialnumber, services_->service_lifecycle().mapGeneration(), ignored};
}
void BotBehaviorRuntime::stopVoiceFeedback(std::size_t index)
{
	if (index >= actors_.size() || !(outgoingVoiceMask_ & (1U << index)))
		return;
	const auto stopContext = reactionContext(index);
	const auto stopRevision = actors_[index].confirmedSpeech.revision();
	const auto stopConnection =
		services_ ? services_->service_lifecycle().slotGeneration(static_cast<std::uint32_t>(index + 1)) : 0;
	outgoingVoiceMask_ &= ~(1U << index);
	actors_[index].outgoingVoiceFeedback = {};
	const auto engine = services_ ? services_->service_engineFunctions() : nullptr;
	const int message = services_ ? voiceMessageId(services_->service_pluginId(), "BotVoice") : 0;
	const auto currentContext = reactionContext(index);
	if ((outgoingVoiceMask_ & (1U << index)) || stopRevision != actors_[index].confirmedSpeech.revision() ||
		!(stopContext.actor == currentContext.actor) ||
		stopContext.entityGeneration != currentContext.entityGeneration ||
		!(stopContext.frame == currentContext.frame) || stopContext.alive != currentContext.alive ||
		std::memcmp(&stopContext.now, &currentContext.now, sizeof(stopContext.now)) != 0 ||
		(services_ &&
			stopConnection != services_->service_lifecycle().slotGeneration(static_cast<std::uint32_t>(index + 1))))
	{
		return;
	}
	if (message > 0 && engine && engine->pfnMessageBegin && engine->pfnWriteByte && engine->pfnMessageEnd)
	{
		engine->pfnMessageBegin(MSG_ALL, message, nullptr, nullptr);
		engine->pfnWriteByte(0);
		engine->pfnWriteByte(static_cast<int>(index + 1U));
		engine->pfnMessageEnd();
	}
}
void BotBehaviorRuntime::updateVoiceFeedback(bool force)
{
	if (!outgoingVoiceMask_ || !services_)
		return;
	const auto globals = services_->service_globals();
	for (std::size_t i = 0; i < actors_.size(); ++i)
	{
		if (!(outgoingVoiceMask_ & (1U << i)))
			continue;
		const auto self = services_->service_managedBotHandles()[i].entity;
		const auto &feedback = actors_[i].outgoingVoiceFeedback;
		if (force || !globals || !std::isfinite(globals->time) || !self || self->free ||
			self->serialnumber != feedback.serial || self->v.deadflag != DEAD_NO || self->v.health <= 0 ||
			globals->time >= feedback.until)
			stopVoiceFeedback(i);
	}
}
void BotBehaviorRuntime::finishDeathChatter(
	std::size_t index, const compat::ConfirmedChatterSpeech &previous, const world::FrameIdentity &deathFrame)
{
	const auto receipt = reactionContext(index);
	if (!previous.wasTalking(receipt) || (deathFrame.isValid() && !(deathFrame == receipt.frame)))
	{
		stopVoiceFeedback(index);
		return;
	}
	const auto revision = actors_[index].confirmedSpeech.revision();
	const world::ActorKey connection{
		receipt.actor.slot, services_->service_lifecycle().slotGeneration(receipt.actor.slot)};
	const auto current = [&]()
	{
		return actors_[index].confirmedSpeech.revision() == revision &&
			compat::sameChatterUpdateReceipt(receipt, reactionContext(index)) && receiptConnectionCurrent(connection);
	};
	const auto engine = services_->service_engineFunctions();
	const auto *self = services_->service_managedBotHandles()[index].entity;
	const int team = services_->observation_resolvePlayerTeam(self);
	if (!current())
	{
		return;
	}
	constexpr int kTerroristTeam = 1;
	constexpr int kCounterTerroristTeam = 2;
	if (team == kTerroristTeam || team == kCounterTerroristTeam)
	{
		if (previous.ownsSpeech(receipt, outgoingTeamCurrentPhrase_[team], outgoingTeamBusyUntil_[team]))
		{
			outgoingTeamBusyUntil_[team] = 0;
			outgoingTeamCurrentPhrase_[team] = compat::ChatterPhrase::None;
			outgoingTeamCurrentPlace_[team] = 0;
		}
		const bool emitPain = deathFrame.isValid();
		const char *verbosity = emitPain && engine && engine->pfnCVarGetString ? engine->pfnCVarGetString("bot_chatter") : nullptr;
		if (!current())
		{
			return;
		}
		if (emitPain && compat::deathInterruptVoiceEnabled(verbosity))
		{
			ensureOutgoingChatterCatalog();
			if (!current())
			{
				return;
			}
			if (outgoingChatterCatalog_.defined(compat::ChatterPhrase::Pain))
			{
				auto *bank = chatterBank(index);
				if (!current())
				{
					return;
				}
				if (bank)
				{
					float unusedDuration = 0;
					(void)sendChatterVoice(index, compat::ChatterPhrase::Pain, -1, *bank, &unusedDuration, 0,
						ChatterVoicePurpose::DeathInterrupt);
					if (!current())
					{
						return;
					}
				}
				// OnDeath resets silence for a defined pain phrase even without a speakable.
				outgoingTeamLastSpeech_[team] = receipt.now;
			}
		}
	}
	stopVoiceFeedback(index);
}

bool BotBehaviorRuntime::sendChatterVoice(std::size_t index, compat::ChatterPhrase phrase, int count,
	compat::ChatterVoiceCatalog &bank, float *duration, std::uint16_t reportPlace, ChatterVoicePurpose purpose)
{
	if (!services_ || index >= actors_.size() || !duration)
		return false;
	const bool deathInterrupt = purpose == ChatterVoicePurpose::DeathInterrupt;
	const auto receiptContext = reactionContext(index);
	const world::ActorKey connection{
		receiptContext.actor.slot, services_->service_lifecycle().slotGeneration(receiptContext.actor.slot)};
	const auto speechRevision = actors_[index].confirmedSpeech.revision();
	const auto current = [&]()
	{
		return deathInterrupt
			? actors_[index].confirmedSpeech.revision() == speechRevision &&
				compat::sameChatterUpdateReceipt(receiptContext, reactionContext(index)) &&
				receiptConnectionCurrent(connection)
			: actors_[index].confirmedSpeech.canComplete(receiptContext, reactionContext(index), speechRevision);
	};
	if (!current())
		return false;
	const auto engine = services_->service_engineFunctions();
	const auto globals = services_->service_globals();
	const auto &handle = services_->service_managedBotHandles()[index];
	auto *self = handle.entity;
	const int message = voiceMessageId(services_->service_pluginId(), "SendAudio");
	if (!current())
		return false;
	if (message <= 0 || !engine || !globals || !self || self->free ||
		(!deathInterrupt && (self->v.deadflag != DEAD_NO || self->v.health <= 0)) || !engine->pfnMessageBegin ||
		!engine->pfnWriteByte || !engine->pfnWriteString || !engine->pfnWriteShort || !engine->pfnMessageEnd ||
		!engine->pfnPEntityOfEntIndex)
		return false;
	std::string place;
	const auto navigation = services_->service_navPublisher().snapshot();
	if (!deathInterrupt && !compat::usesUndefinedChatterPlace(phrase) && navigation.isValid() &&
		navigation.mapGeneration() == services_->service_lifecycle().mapGeneration())
	{
		nav::NavAreaMatch match{};
		if (phrase == compat::ChatterPhrase::HeardNoise)
		{
			for (const auto &known : navigation.document()->places())
				if (known.id == reportPlace)
				{
					place = known.name;
					break;
				}
		}
		else if (nav::NavQuery(navigation)
					 .findContaining({self->v.origin.x, self->v.origin.y, self->v.origin.z}, 64.0f, &match) ==
			nav::NavQueryResult::Found)
		{
			const auto *area = navigation.document()->findArea(match.area);
			if (area)
				for (const auto &known : navigation.document()->places())
					if (known.id == area->placeId)
					{
						place = known.name;
						break;
					}
		}
	}
	auto *speakable = bank.select(phrase, count, deathInterrupt ? "ANY" : place.c_str());
	if (!speakable)
		return false;
	const std::string sample = speakable->sample;
	const int pitch = actors_[index].managedProfiles.voicePitch;
	if (!deathInterrupt && speakable->duration < 0.0f)
	{
		std::string filepath = "sound/" + sample;
		const auto milliseconds =
			engine->pfnGetApproxWavePlayLen ? engine->pfnGetApproxWavePlayLen(filepath.c_str()) : 0U;
		if (!current())
			return false;
		speakable->duration = milliseconds ? static_cast<float>(milliseconds) / 1000.0f : 1.0f;
	}
	const float clipDuration = deathInterrupt ? 0.0f : speakable->duration;
	if (!deathInterrupt && clipDuration <= 0.0f)
		return false;
	const int team = services_->observation_resolvePlayerTeam(self);
	if (!current())
		return false;
	if (team != 1 && team != 2)
		return false;
	const int feedbackMessage = deathInterrupt ? 0 : voiceMessageId(services_->service_pluginId(), "BotVoice");
	if (!current())
		return false;
	// Native marks the statement speaking before Radio, including reentrant death.
	constexpr float kStatementGapSeconds = .1f;
	if (!deathInterrupt &&
		!actors_[index].confirmedSpeech.beginSpeaking(
			receiptContext, phrase, receiptContext.now + clipDuration + kStatementGapSeconds, speechRevision))
	{
		return false;
	}
	int radioIconModel = 0;
	if (deathInterrupt && engine->pfnModelIndex && engine->pfnWriteCoord)
	{
		// Ordinary statements pass showIcon=false; native OnDeath uses Radio's true default.
		const auto *showIcon = engine->pfnCVarGetPointer ? engine->pfnCVarGetPointer("mp_show_radioicon") : nullptr;
		if (!current())
		{
			return false;
		}
		if (!showIcon || showIcon->value != 0.0f)
		{
			const int model = engine->pfnModelIndex("sprites/radio.spr");
			if (!current())
			{
				return false;
			}
			constexpr int kMaximumModelIndex = (std::numeric_limits<short>::max)();
			if (model > 0 && model <= kMaximumModelIndex)
			{
				radioIconModel = model;
			}
		}
	}
	unsigned eligible = 0;
	for (int slot = 1; slot <= (std::min)(32, globals->maxClients); ++slot)
	{
		auto *recipient = engine->pfnPEntityOfEntIndex(slot);
		if (!current())
			return false;
		if (!recipient || recipient->free || !(recipient->v.flags & FL_CLIENT) || (recipient->v.flags & FL_DORMANT))
			continue;
		const int recipientSerial = recipient->serialnumber;
		const auto recipientConnection =
			services_->service_lifecycle().slotGeneration(static_cast<std::uint32_t>(slot));
		const auto recipientCurrent = [&]()
		{
			return !recipient->free && (recipient->v.flags & FL_CLIENT) != 0 &&
				(recipient->v.flags & FL_DORMANT) == 0 && recipient->serialnumber == recipientSerial &&
				services_->service_lifecycle().slotGeneration(static_cast<std::uint32_t>(slot)) == recipientConnection;
		};
		const int recipientTeam = services_->observation_resolvePlayerTeam(recipient);
		if (!current())
			return false;
		if (!recipientCurrent())
		{
			continue;
		}
		bool receives = !(recipient->v.flags & FL_SPECTATOR) && recipientTeam == team;
		if ((recipient->v.flags & FL_SPECTATOR) || recipientTeam == 0)
		{
			const int mode = recipient->v.iuser1, targetSlot = recipient->v.iuser2;
			if ((mode == 1 || mode == 2 || mode == 4) && targetSlot >= 1 && targetSlot <= globals->maxClients)
			{
				const auto *target = engine->pfnPEntityOfEntIndex(targetSlot);
				if (!current())
					return false;
				receives = target && !target->free && !(target->v.flags & FL_DORMANT) &&
					services_->observation_resolvePlayerTeam(target) == team;
			}
			else if (recipient->v.flags & FL_SPECTATOR)
				receives = false;
		}
		if (!current())
			return false;
		if (!recipientCurrent() || !receives)
			continue;
		++eligible;
		const auto &preference = outgoingRadioPreferences_[static_cast<std::size_t>(slot - 1)];
		const bool ignored = preference.map == services_->service_lifecycle().mapGeneration() &&
			preference.serial == recipient->serialnumber && preference.ignored;
		if (!ignored)
		{
			engine->pfnMessageBegin(MSG_ONE, message, nullptr, recipient);
			engine->pfnWriteByte(static_cast<int>(receiptContext.actor.slot));
			engine->pfnWriteString(sample.c_str());
			engine->pfnWriteShort(pitch);
			engine->pfnMessageEnd();
			if (!current())
				return false;
			if (!recipientCurrent())
			{
				continue;
			}
			if (radioIconModel > 0)
			{
				// Public GoldSrc SVC_TEMPENTITY and native Radio attachment payload.
				constexpr int kTemporaryEntityService = 23;
				constexpr float kRadioIconHeight = 35.0f;
				constexpr int kRadioIconLifetimeTenths = 15;
				engine->pfnMessageBegin(MSG_ONE, kTemporaryEntityService, nullptr, recipient);
				engine->pfnWriteByte(TE_PLAYERATTACHMENT);
				engine->pfnWriteByte(static_cast<int>(receiptContext.actor.slot));
				engine->pfnWriteCoord(kRadioIconHeight);
				engine->pfnWriteShort(radioIconModel);
				engine->pfnWriteShort(kRadioIconLifetimeTenths);
				engine->pfnMessageEnd();
				if (!current())
				{
					return false;
				}
			}
		}
		if (feedbackMessage > 0)
		{
			// Publish ownership before the first externally observable feedback start.
			outgoingVoiceMask_ |= 1U << index;
			actors_[index].outgoingVoiceFeedback = {self->serialnumber, receiptContext.now + clipDuration + 1.0f};
			engine->pfnMessageBegin(MSG_ONE, feedbackMessage, nullptr, recipient);
			engine->pfnWriteByte(1);
			engine->pfnWriteByte(static_cast<int>(receiptContext.actor.slot));
			engine->pfnMessageEnd();
			if (!current())
				return false;
		}
	}
	// Radio() advances the statement even when all eligible listeners ignore audio.
	// BotVoice feedback must still receive its own tracked stop.
	if (!eligible)
		return false;
	*duration = clipDuration;
	return true;
}
} // namespace metamod
} // namespace astrabot
