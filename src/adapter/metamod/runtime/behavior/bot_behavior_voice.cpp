#include "runtime/behavior/bot_behavior_runtime.hpp"
#include <cstring>
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
	outgoingVoiceMask_ &= ~(1U << index);
	actors_[index].outgoingVoiceFeedback = {};
	const auto engine = services_ ? services_->service_engineFunctions() : nullptr;
	const int message = services_ ? voiceMessageId(services_->service_pluginId(), "BotVoice") : 0;
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
bool BotBehaviorRuntime::sendChatterVoice(
	std::size_t index, compat::ChatterPhrase phrase, int count, compat::ChatterVoiceCatalog &bank, float *duration)
{
	if (!services_ || index >= actors_.size() || !duration)
		return false;
	const auto engine = services_->service_engineFunctions();
	const auto globals = services_->service_globals();
	const auto &handle = services_->service_managedBotHandles()[index];
	auto *self = handle.entity;
	const int message = voiceMessageId(services_->service_pluginId(), "SendAudio");
	if (message <= 0 || !engine || !globals || !self || self->free || self->v.deadflag != DEAD_NO ||
		self->v.health <= 0 || !engine->pfnMessageBegin || !engine->pfnWriteByte || !engine->pfnWriteString ||
		!engine->pfnWriteShort || !engine->pfnMessageEnd || !engine->pfnPEntityOfEntIndex)
		return false;
	std::string place;
	const auto navigation = services_->service_navPublisher().snapshot();
	if (navigation.isValid() && navigation.mapGeneration() == services_->service_lifecycle().mapGeneration())
	{
		nav::NavAreaMatch match{};
		if (nav::NavQuery(navigation)
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
	auto *speakable = bank.select(phrase, count, place.c_str());
	if (!speakable)
		return false;
	if (speakable->duration < 0.0f)
	{
		std::string filepath = "sound/" + speakable->sample;
		const auto milliseconds =
			engine->pfnGetApproxWavePlayLen ? engine->pfnGetApproxWavePlayLen(filepath.c_str()) : 0U;
		speakable->duration = milliseconds ? static_cast<float>(milliseconds) / 1000.0f : 1.0f;
	}
	if (speakable->duration <= 0.0f)
		return false;
	const int team = services_->observation_resolvePlayerTeam(self);
	if (team != 1 && team != 2)
		return false;
	const int feedbackMessage = voiceMessageId(services_->service_pluginId(), "BotVoice");
	unsigned eligible = 0;
	for (int slot = 1; slot <= (std::min)(32, globals->maxClients); ++slot)
	{
		auto *recipient = engine->pfnPEntityOfEntIndex(slot);
		if (!recipient || recipient->free || !(recipient->v.flags & FL_CLIENT) || (recipient->v.flags & FL_DORMANT))
			continue;
		const int recipientTeam = services_->observation_resolvePlayerTeam(recipient);
		bool receives = !(recipient->v.flags & FL_SPECTATOR) && recipientTeam == team;
		if ((recipient->v.flags & FL_SPECTATOR) || recipientTeam == 0)
		{
			const int mode = recipient->v.iuser1, targetSlot = recipient->v.iuser2;
			if ((mode == 1 || mode == 2 || mode == 4) && targetSlot >= 1 && targetSlot <= globals->maxClients)
			{
				const auto *target = engine->pfnPEntityOfEntIndex(targetSlot);
				receives = target && !target->free && !(target->v.flags & FL_DORMANT) &&
					services_->observation_resolvePlayerTeam(target) == team;
			}
			else if (recipient->v.flags & FL_SPECTATOR)
				receives = false;
		}
		if (!receives)
			continue;
		++eligible;
		const auto &preference = outgoingRadioPreferences_[static_cast<std::size_t>(slot - 1)];
		const bool ignored = preference.map == services_->service_lifecycle().mapGeneration() &&
			preference.serial == recipient->serialnumber && preference.ignored;
		if (!ignored)
		{
			engine->pfnMessageBegin(MSG_ONE, message, nullptr, recipient);
			engine->pfnWriteByte(static_cast<int>(handle.actor.slot));
			engine->pfnWriteString(speakable->sample.c_str());
			engine->pfnWriteShort(actors_[index].managedProfiles.voicePitch);
			engine->pfnMessageEnd();
		}
		if (feedbackMessage > 0)
		{
			engine->pfnMessageBegin(MSG_ONE, feedbackMessage, nullptr, recipient);
			engine->pfnWriteByte(1);
			engine->pfnWriteByte(static_cast<int>(handle.actor.slot));
			engine->pfnMessageEnd();
		}
	}
	// Radio() advances the statement even when all eligible listeners ignore audio.
	// BotVoice feedback must still receive its own tracked stop.
	if (!eligible)
		return false;
	if (feedbackMessage > 0)
	{
		outgoingVoiceMask_ |= 1U << index;
		actors_[index].outgoingVoiceFeedback = {self->serialnumber, globals->time + speakable->duration + 1.0f};
	}
	*duration = speakable->duration;
	return true;
}
} // namespace metamod
} // namespace astrabot
