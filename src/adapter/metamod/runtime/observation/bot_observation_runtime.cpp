#include "runtime/observation/bot_observation_runtime.hpp"
#include "runtime/shared/bot_runtime_support.hpp"
namespace astrabot
{
namespace metamod
{
using namespace runtime_detail;
namespace
{
constexpr float kSmokeObservationLifetimeSeconds = 4.0f;
constexpr float kMaximumSoundDistance = 4096.0f;
constexpr float kSoundAttenuationDistanceScale = 1000.0f;
// Keep literal evaluation precision on x87; each macro is undefined below.
#define ASTRABOT_BOT_OBSERVATION_RUNTIME_PI 3.14159265358979323846f
constexpr float kHalfTurnDegrees = 180.0f;
constexpr float kHeadProbeHeight = 25.0f;
constexpr float kAimTraceDistance = 8192.0f;
#define ASTRABOT_BOT_OBSERVATION_RUNTIME_MINIMUM_VIEW_DIRECTION_LENGTH 0.001f
constexpr float kMinimumViewConeDot = 0.5f;
constexpr float kBombBeepHearingDistance = 1000.0f;
constexpr float kBombBeepKnowledgeDistance = 1000.0f;
constexpr float kFeetTargetProbeDepth = 14.0f;
constexpr int kMaximumClientSlot = 32;
constexpr int kMaximumMessageByte = 255;
constexpr int kTerroristTeam = 1;
constexpr int kCounterTerroristTeam = 2;
constexpr auto kTerroristTeamByte = 1U;
constexpr auto kCounterTerroristTeamByte = 2U;
constexpr auto kShowMenuContinuationField = 2U;
constexpr auto kMinimumCurrentWeaponFieldCount = 2U;
constexpr auto kMaximumCurrentWeaponId = 30U;
constexpr int kInactiveWeaponState = 0;
constexpr int kActiveWeaponState = 1;
constexpr int kObservationTraceIgnoringActors = 1;
constexpr int kObservationTraceIncludingActors = 0;
constexpr auto kWeaponSoundPrefixLength = 8U;
}

void BotObservationRuntime::onMessageBegin(
	int messageDestination, int messageType, const float *origin, edict_t *entity)
{
	(void)messageDestination;
	(void)origin;
	publicMessageName_ = nullptr;
	publicMessageTarget_ = entity;
	publicMessageDestination_ = messageDestination;
	publicMessageActor_ = {};
	publicMessageValues_ = {};
	publicMessageCoords_ = {};
	publicMessageCoordMask_ = 0U;
	publicMessageString_.fill('\0');
	publicMessageStringMask_ = 0U;
	publicMessageFieldCount_ = 0U;
	publicMessageMalformed_ = false;
	publicMessageFrame_ = {services_->service_lifecycle().mapGeneration(),
		services_->service_lifecycle().roundGeneration(), services_->service_adapterFrameCount()};
	if (gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnGetUserMsgID != nullptr &&
		services_->service_pluginId() != nullptr)
	{
		const char *const names[] = {"WeaponList", "CurWeapon", "AmmoX", "AmmoPickup", "WeapPickup", "Money",
			"StatusIcon", "ScreenFade", "BarTime", "DeathMsg", "ResetHUD", "TextMsg", "TeamScore", "SendAudio",
			"ScoreAttrib", "ArmorType", "BombDrop", "BombPickup"};
		for (const char *name : names)
		{
			int size = 0;
			const int id = gpMetaUtilFuncs->pfnGetUserMsgID(services_->service_pluginId(), name, &size);
			if (id > 0 && id == messageType)
			{
				publicMessageName_ = name;
				break;
			}
		}
	}
	if (publicMessageName_ != nullptr && entity != nullptr)
	{
		for (const FakeClientHandle &handle : services_->service_managedBotHandles())
		{
			if (handle.entity == entity && handle.actor.actorGeneration != 0U)
			{
				publicMessageActor_ = {handle.actor.slot, handle.actor.actorGeneration};
				publicObservations_.bind(publicMessageActor_, publicMessageFrame_);
				break;
			}
		}
	}
	const bool continueShowMenu = userMessageShowFragmentActive_;
	userMessageKind_ = UserMessageKind::None;
	userMessageTarget_ = nullptr;
	userMessageFieldCount_ = 0U;
	userMessageWeaponState_ = 0U;
	userMessageWeaponId_ = 0U;
	userMessageWeaponMessageValid_ = false;
	userMessageMenuType_ = 0U;
	userMessageNeedMore_ = 0U;
	userMessageValidSlots_ = 0U;
	userMessageTeamSlot_ = 0U;
	if (!continueShowMenu)
	{
		userMessageText_.fill('\0');
		userMessageTextLength_ = 0U;
	}
	userMessageShowFragmentActive_ = false;

	int messageSize = 0;
	int showMenuId = kShowMenuMessageId;
	int vguiMenuId = kVguiMenuMessageId;
	int teamInfoId = kTeamInfoMessageId;
	int curWeaponId = 0;
	if (gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnGetUserMsgID != nullptr &&
		services_->service_pluginId() != nullptr)
	{
		const int resolvedShowMenuId =
			gpMetaUtilFuncs->pfnGetUserMsgID(services_->service_pluginId(), "ShowMenu", &messageSize);
		const int resolvedVguiMenuId =
			gpMetaUtilFuncs->pfnGetUserMsgID(services_->service_pluginId(), "VGUIMenu", &messageSize);
		const int resolvedTeamInfoId =
			gpMetaUtilFuncs->pfnGetUserMsgID(services_->service_pluginId(), "TeamInfo", &messageSize);
		const int resolvedCurWeaponId =
			gpMetaUtilFuncs->pfnGetUserMsgID(services_->service_pluginId(), "CurWeapon", &messageSize);
		if (resolvedShowMenuId > 0)
		{
			showMenuId = resolvedShowMenuId;
		}
		if (resolvedVguiMenuId > 0)
		{
			vguiMenuId = resolvedVguiMenuId;
		}
		if (resolvedTeamInfoId > 0)
		{
			teamInfoId = resolvedTeamInfoId;
		}
		if (resolvedCurWeaponId > 0)
		{
			curWeaponId = resolvedCurWeaponId;
		}
	}
	if (showMenuId > 0 && messageType == showMenuId)
	{
		userMessageKind_ = UserMessageKind::ShowMenu;
	}
	else if (vguiMenuId > 0 && messageType == vguiMenuId)
	{
		userMessageKind_ = UserMessageKind::VguiMenu;
	}
	else if (teamInfoId > 0 && messageType == teamInfoId)
	{
		userMessageKind_ = UserMessageKind::TeamInfo;
	}
	else if (curWeaponId > 0 && messageType == curWeaponId)
	{
		userMessageKind_ = UserMessageKind::CurWeapon;
	}
	if (userMessageKind_ == UserMessageKind::None ||
		(userMessageKind_ != UserMessageKind::TeamInfo && entity == nullptr))
	{
		return;
	}

	if (userMessageKind_ != UserMessageKind::CurWeapon && gpMetaUtilFuncs != nullptr &&
		gpMetaUtilFuncs->pfnLogConsole != nullptr && services_->service_pluginId() != nullptr)
	{
		const int slot = entity != nullptr && services_->service_engineFunctions() != nullptr &&
				services_->service_engineFunctions()->pfnIndexOfEdict != nullptr
			? services_->service_engineFunctions()->pfnIndexOfEdict(entity)
			: 0;
		profiledLogConsole(services_->service_runtimeProfiler(), services_->service_pluginId(),
			"menu message begin kind=%d type=%d slot=%d frame=%u", static_cast<int>(userMessageKind_), messageType,
			slot, static_cast<unsigned int>(services_->service_adapterFrameCount()));
	}
	userMessageTarget_ = entity;
}

void BotObservationRuntime::onMessageEnd()
{
	constexpr auto kTeamMenu = 2U;
	constexpr auto kTerroristClassMenu = 26U;
	constexpr auto kCounterTerroristClassMenu = 27U;
	capturePublicMessage();
	if (userMessageKind_ == UserMessageKind::CurWeapon)
	{
		captureManagedCurrentWeapon();
	}
	if (userMessageKind_ == UserMessageKind::VguiMenu && userMessageTarget_ != nullptr)
	{
		if (userMessageMenuType_ == kTeamMenu)
		{
			services_->notifyMenuReady(
				userMessageTarget_, JoinMenuKind::Team, userMessageValidSlots_, JoinMenuSource::Vgui);
		}
		else if (userMessageMenuType_ == kTerroristClassMenu)
		{
			services_->notifyMenuReady(
				userMessageTarget_, JoinMenuKind::TerroristClass, userMessageValidSlots_, JoinMenuSource::Vgui);
		}
		else if (userMessageMenuType_ == kCounterTerroristClassMenu)
		{
			services_->notifyMenuReady(
				userMessageTarget_, JoinMenuKind::CounterTerroristClass, userMessageValidSlots_, JoinMenuSource::Vgui);
		}
	}
	userMessageShowFragmentActive_ =
		userMessageKind_ == UserMessageKind::ShowMenu && userMessageNeedMore_ != 0U && userMessageTextLength_ != 0U;
	userMessageKind_ = UserMessageKind::None;
	userMessageTarget_ = nullptr;
	userMessageFieldCount_ = 0U;
	userMessageWeaponState_ = 0U;
	userMessageWeaponId_ = 0U;
	userMessageWeaponMessageValid_ = false;
	userMessageMenuType_ = 0U;
	userMessageNeedMore_ = 0U;
	userMessageValidSlots_ = 0U;
	userMessageTeamSlot_ = 0U;
	if (!userMessageShowFragmentActive_)
	{
		userMessageText_.fill('\0');
		userMessageTextLength_ = 0U;
	}
}

void BotObservationRuntime::onWriteByte(int value)
{
	appendPublicMessageInteger(value);
	if (userMessageKind_ == UserMessageKind::CurWeapon)
	{
		if (userMessageFieldCount_ == 0U && (value == kInactiveWeaponState || value == kActiveWeaponState))
		{
			userMessageWeaponState_ = static_cast<std::uint8_t>(value);
			userMessageWeaponMessageValid_ = true;
		}
		else if (userMessageFieldCount_ == 1U && value >= 0 && value <= kMaximumMessageByte)
		{
			userMessageWeaponId_ = static_cast<std::uint8_t>(value);
		}
	}
	else if (userMessageKind_ == UserMessageKind::TeamInfo && userMessageFieldCount_ == 0U)
	{
		if (value >= static_cast<int>(runtime::LifecycleSession::kFirstClientSlot) &&
			value <= static_cast<int>(runtime::LifecycleSession::kClientSlotCount))
		{
			userMessageTeamSlot_ = static_cast<std::uint8_t>(value);
		}
	}
	else if (userMessageKind_ == UserMessageKind::VguiMenu && userMessageFieldCount_ == 0U)
	{
		if (value >= 0 && value <= kMaximumMessageByte)
		{
			userMessageMenuType_ = static_cast<std::uint8_t>(value);
		}
		if (gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr &&
			services_->service_pluginId() != nullptr)
		{
			profiledLogConsole(services_->service_runtimeProfiler(), services_->service_pluginId(),
				"menu vgui value=%d frame=%u", value,
				static_cast<unsigned int>(services_->service_adapterFrameCount()));
		}
	}
	else if (userMessageKind_ == UserMessageKind::ShowMenu && userMessageFieldCount_ == kShowMenuContinuationField)
	{
		userMessageNeedMore_ = value == 0 ? 0U : 1U;
	}
	if (userMessageFieldCount_ < (std::numeric_limits<std::uint8_t>::max)())
	{
		++userMessageFieldCount_;
	}
}

void BotObservationRuntime::onWriteChar(int value)
{
	onWriteByte(value);
}

void BotObservationRuntime::onWriteShort(int value)
{
	constexpr int kMaximumMenuSlotMask = 65535;
	appendPublicMessageInteger(value);
	const bool showMenuSlots = userMessageKind_ == UserMessageKind::ShowMenu && userMessageFieldCount_ == 0U;
	const bool vguiMenuSlots = userMessageKind_ == UserMessageKind::VguiMenu && userMessageFieldCount_ == 1U;
	if ((showMenuSlots || vguiMenuSlots) && value >= 0 && value <= kMaximumMenuSlotMask)
	{
		userMessageValidSlots_ = static_cast<std::uint16_t>(value);
	}
	if (userMessageFieldCount_ < (std::numeric_limits<std::uint8_t>::max)())
	{
		++userMessageFieldCount_;
	}
}

void BotObservationRuntime::onWriteString(const char *value)
{
	if (publicMessageName_ != nullptr)
	{
		if (value == nullptr || publicMessageFieldCount_ >= publicMessageValues_.size() ||
			std::strlen(value) >= publicMessageString_.size())
			publicMessageMalformed_ = true;
		else
		{
			publicMessageStringMask_ |= static_cast<std::uint16_t>(1U << publicMessageFieldCount_);
			std::memcpy(publicMessageString_.data(), value, std::strlen(value) + 1U);
			std::memcpy(publicMessageStrings_[publicMessageFieldCount_].data(), value, std::strlen(value) + 1U);
			++publicMessageFieldCount_;
		}
	}
	if (value == nullptr)
	{
		return;
	}
	if (userMessageKind_ == UserMessageKind::TeamInfo)
	{
		notifyTeamInfo(userMessageTeamSlot_, value);
		return;
	}
	if (userMessageKind_ != UserMessageKind::ShowMenu)
	{
		return;
	}
	const std::size_t valueLength = std::strlen(value);
	if (valueLength > userMessageText_.size() - 1U ||
		userMessageTextLength_ > userMessageText_.size() - 1U - valueLength)
	{
		userMessageText_.fill('\0');
		userMessageTextLength_ = 0U;
		return;
	}
	for (std::size_t index = 0U; index < valueLength; ++index)
	{
		userMessageText_[userMessageTextLength_ + index] = value[index];
	}
	userMessageTextLength_ = static_cast<std::uint16_t>(userMessageTextLength_ + valueLength);
	userMessageText_[userMessageTextLength_] = '\0';
	if (userMessageNeedMore_ != 0U)
	{
		return;
	}
	const char *menuText = userMessageText_.data();
	const bool teamMenu = std::strcmp(menuText, "#Team_Select") == 0 ||
		std::strcmp(menuText, "#Team_Select_Spect") == 0 || std::strcmp(menuText, "#IG_Team_Select") == 0 ||
		std::strcmp(menuText, "#IG_Team_Select_Spect") == 0 || std::strcmp(menuText, "#IG_VIP_Team_Select") == 0 ||
		std::strcmp(menuText, "#IG_VIP_Team_Select_Spect") == 0;
	if (teamMenu)
	{
		if (gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr &&
			services_->service_pluginId() != nullptr)
		{
			profiledLogConsole(services_->service_runtimeProfiler(), services_->service_pluginId(),
				"menu show team value=%s frame=%u", value,
				static_cast<unsigned int>(services_->service_adapterFrameCount()));
		}
		services_->notifyMenuReady(
			userMessageTarget_, JoinMenuKind::Team, userMessageValidSlots_, JoinMenuSource::LegacyShowMenu);
	}
	else if (std::strcmp(menuText, "#Terrorist_Select") == 0)
	{
		if (gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr &&
			services_->service_pluginId() != nullptr)
		{
			profiledLogConsole(services_->service_runtimeProfiler(), services_->service_pluginId(),
				"menu show class value=%s frame=%u", value,
				static_cast<unsigned int>(services_->service_adapterFrameCount()));
		}
		services_->notifyMenuReady(
			userMessageTarget_, JoinMenuKind::TerroristClass, userMessageValidSlots_, JoinMenuSource::LegacyShowMenu);
	}
	else if (std::strcmp(menuText, "#CT_Select") == 0)
	{
		if (gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr &&
			services_->service_pluginId() != nullptr)
		{
			profiledLogConsole(services_->service_runtimeProfiler(), services_->service_pluginId(),
				"menu show class value=%s frame=%u", value,
				static_cast<unsigned int>(services_->service_adapterFrameCount()));
		}
		services_->notifyMenuReady(userMessageTarget_, JoinMenuKind::CounterTerroristClass, userMessageValidSlots_,
			JoinMenuSource::LegacyShowMenu);
	}
}

void BotObservationRuntime::notifyTeamInfo(std::uint8_t slot, const char *teamName)
{
	if (slot < runtime::LifecycleSession::kFirstClientSlot || slot > runtime::LifecycleSession::kClientSlotCount ||
		teamName == nullptr)
	{
		return;
	}
	const std::size_t index = static_cast<std::size_t>(slot - 1U);
	actors_[index].teamInfoTeams = equalsIgnoreCase(teamName, "TERRORIST") ? kTerroristTeamByte
		: equalsIgnoreCase(teamName, "CT") || equalsIgnoreCase(teamName, "COUNTER-TERRORIST")
		? kCounterTerroristTeamByte
		: 0U;
	actors_[index].publicTeamKnown = true;
	if (!services_->service_managedBotSlots()[index] || services_->service_managedBotHandles()[index].entity == nullptr)
	{
		return;
	}
	const FakeClientHandle &handle = services_->service_managedBotHandles()[index];
	if (gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr &&
		services_->service_pluginId() != nullptr)
	{
		profiledLogConsole(services_->service_runtimeProfiler(), services_->service_pluginId(),
			"team info slot=%u generation=%u name=%s phase=%d requested_team=%d "
			"entity_team=%d deadflag=%d spectator=%d frame=%u",
			static_cast<unsigned int>(slot), static_cast<unsigned int>(handle.actor.actorGeneration), teamName,
			static_cast<int>(services_->service_joinControllers()[index].phase()),
			static_cast<int>(services_->service_joinControllers()[index].requestedTeam()),
			static_cast<int>(handle.entity->v.team), static_cast<int>(handle.entity->v.deadflag),
			(handle.entity->v.flags & FL_SPECTATOR) != 0 ? 1 : 0,
			static_cast<unsigned int>(services_->service_adapterFrameCount()));
	}
	services_->observeJoinTeamInfo(index, teamName);
}

bool BotObservationRuntime::hasObservedActiveWeapon(std::size_t index) const
{
	if (index >= services_->service_managedBotHandles().size())
	{
		return false;
	}
	const auto &handle = services_->service_managedBotHandles()[index];
	compat::PublicActorState state{};
	return publicObservations_.read({handle.actor.slot, handle.actor.actorGeneration},
			   {services_->service_lifecycle().mapGeneration(), services_->service_lifecycle().roundGeneration(),
				   services_->service_adapterFrameCount()},
			   &state) &&
		state.weaponKnown;
}

void BotObservationRuntime::onEmitSound(edict_t *entity, const char *sample, float volume, float attenuation)
{
	if (entity == nullptr || entity->free || sample == nullptr || services_->service_globals() == nullptr ||
		services_->service_engineFunctions() == nullptr ||
		services_->service_engineFunctions()->pfnIndexOfEdict == nullptr || !std::isfinite(volume) || volume <= 0.0f ||
		volume > 1.0f || !std::isfinite(attenuation))
	{
		return;
	}
	const int entityIndex = services_->service_engineFunctions()->pfnIndexOfEdict(entity);
	const world::WorldVector origin{entity->v.origin.x, entity->v.origin.y, entity->v.origin.z};
	const bool bombBeep = std::strstr(sample, "c4_beep") != nullptr;
	if (std::strstr(sample, "sg_explode") != nullptr && entityIndex > 0 && entity->serialnumber != 0)
	{
		const world::EntityKey key{
			static_cast<std::uint32_t>(entityIndex), static_cast<std::uint32_t>(entity->serialnumber)};
		publicSmokeEntities_[nextSmokeEntity_++ % publicSmokeEntities_.size()] = {key, origin};
		smokeObservations_.observe(key, origin, services_->service_globals()->time + kSmokeObservationLifetimeSeconds);
	}
	world::AudibleEvent event{};
	event.id = ++soundSequence_;
	if (event.id == 0U)
	{
		event.id = ++soundSequence_;
		reset_deliveredSoundSequences();
		soundEvents_ = {};
		soundTeams_ = {};
	}
	event.kind = bombBeep												  ? world::SoundKind::Objective
		: std::strstr(sample, "pl_step") != nullptr						  ? world::SoundKind::Footstep
		: std::strncmp(sample, "weapons/", kWeaponSoundPrefixLength) == 0 ? world::SoundKind::WeaponFire
																		  : world::SoundKind::Other;
	event.origin = origin;
	event.loudness = volume;
	event.confidence = {1.0f, 0U};
	event.priority = bombBeep ? world::NoisePriority::High : world::NoisePriority::Medium;
	// Derived public sound attenuation, not an exact GameEvent audibility observation.
	event.distance = bombBeep ? kBombBeepHearingDistance
		: attenuation > 0.0f  ? (std::min)(kMaximumSoundDistance, kSoundAttenuationDistanceScale * volume / attenuation)
							  : kMaximumSoundDistance;
	event.timestampTick = services_->service_adapterFrameCount();
	if (entityIndex >= 1 && entityIndex <= kMaximumClientSlot && (entity->v.flags & FL_CLIENT) != 0)
	{
		event.source = {static_cast<std::uint32_t>(entityIndex), static_cast<std::uint32_t>(entity->serialnumber) + 1U};
	}
	// Multiple callbacks for one public sound in the same tick are one receipt.
	for (const auto &previous : soundEvents_)
	{
		if (previous.id != 0U && previous.timestampTick == event.timestampTick && previous.kind == event.kind &&
			previous.source == event.source && previous.origin.x == event.origin.x &&
			previous.origin.y == event.origin.y && previous.origin.z == event.origin.z)
		{
			--soundSequence_;
			return;
		}
	}
	soundEvents_[event.id % soundEvents_.size()] = event;
	soundTeams_[event.id % soundEvents_.size()] = event.source.isValid() ? resolvePlayerTeam(entity) : 0;
	if (bombBeep && entityIndex > 0)
	{
		for (std::size_t index = 0U; index < services_->service_managedBotHandles().size(); ++index)
		{
			const auto &handle = services_->service_managedBotHandles()[index];
			if (!services_->service_managedBotSlots()[index] || handle.entity == nullptr ||
				handle.entity->v.deadflag != DEAD_NO)
			{
				continue;
			}
			const float dx = handle.entity->v.origin.x - origin.x;
			const float dy = handle.entity->v.origin.y - origin.y;
			const float dz = handle.entity->v.origin.z - origin.z;
			if (dx * dx + dy * dy + dz * dz < kBombBeepKnowledgeDistance * kBombBeepKnowledgeDistance)
			{
				actors_[index].objectiveKnowledge[1U] = {
					{static_cast<std::uint32_t>(entityIndex), static_cast<std::uint32_t>(entity->serialnumber)},
					origin};
			}
		}
		services_->behavior_invalidateObjectiveAssignments();
	}
}

void BotObservationRuntime::appendPublicMessageInteger(int value)
{
	if (publicMessageName_ == nullptr)
		return;
	if (publicMessageFieldCount_ >= publicMessageValues_.size())
	{
		publicMessageMalformed_ = true;
		return;
	}
	publicMessageValues_[publicMessageFieldCount_++] = value;
}

void BotObservationRuntime::onWriteLong(int value)
{
	appendPublicMessageInteger(value);
}

void BotObservationRuntime::onWriteCoord(float value)
{
	if (publicMessageName_ == nullptr)
		return;
	if (publicMessageFieldCount_ >= publicMessageCoords_.size() || !std::isfinite(value))
	{
		publicMessageMalformed_ = true;
		return;
	}
	publicMessageCoords_[publicMessageFieldCount_] = value;
	publicMessageCoordMask_ |= static_cast<std::uint16_t>(1U << publicMessageFieldCount_);
	++publicMessageFieldCount_;
}

void BotObservationRuntime::capturePublicMessage()
{
	constexpr int kMinimumBlindFadeAlpha = 180;
	constexpr auto kBombDropFieldCount = 4U;
	constexpr int kDroppedBombState = 0;
	constexpr int kPlantedBombState = 1;
	constexpr auto kBombDropCoordinateMask = 7U;
	constexpr auto kWeaponListFieldCount = 9U;
	constexpr auto kWeaponListStringMask = 1U;
	constexpr auto kDeathMessageFieldCount = 4U;
	constexpr auto kDeathMessageStringMask = 8U;
	constexpr auto kTeamScoreFieldCount = 2U;
	constexpr auto kTeamScoreStringMask = 1U;
	constexpr auto kScoreAttributeFieldCount = 2U;
	constexpr auto kMinimumTextMessageFieldCount = 2U;
	constexpr auto kTextMessageTokenMask = 2U;
	constexpr auto kSendAudioFieldCount = 3U;
	constexpr auto kSendAudioStringMask = 2U;
	constexpr auto kRadioTokenPrefixLength = 7U;
	constexpr auto kRadioCommandCount = 8U;
	constexpr auto kCurrentWeaponFieldCount = 3U;
	constexpr auto kAmmoFieldCount = 2U;
	constexpr auto kWeaponPickupFieldCount = 1U;
	constexpr auto kMoneyBaseFieldCount = 1U;
	constexpr auto kMoneyWithFlashFieldCount = 2U;
	constexpr auto kArmorTypeFieldCount = 1U;
	constexpr auto kStatusIconBaseFieldCount = 2U;
	constexpr auto kStatusIconWithColorFieldCount = 5U;
	constexpr auto kStatusIconStringMask = 2U;
	constexpr auto kBarTimeFieldCount = 1U;
	constexpr auto kScreenFadeFieldCount = 7U;
	constexpr float kFadeDurationUnitsPerSecond = 4096.0f;
	const char *name = publicMessageName_;
	publicMessageName_ = nullptr;
	if (name == nullptr || publicMessageMalformed_ || !publicMessageFrame_.isValid())
		return;
	const std::size_t count = publicMessageFieldCount_;
	const auto &v = publicMessageValues_;
	if (std::strcmp(name, "BombDrop") == 0 || std::strcmp(name, "BombPickup") == 0)
	{
		// Vanilla sends radar updates individually to T. Never redistribute a
		// message sent to a human, CT, or an unrelated/reused client slot.
		if (!publicMessageActor_.isValid() || publicMessageTarget_ == nullptr ||
			(publicMessageDestination_ != MSG_ONE && publicMessageDestination_ != MSG_ONE_UNRELIABLE))
			return;
		const std::size_t index = static_cast<std::size_t>(publicMessageActor_.slot - 1U);
		if (index >= services_->service_managedBotSlots().size() || !services_->service_managedBotSlots()[index] ||
			services_->service_managedBotHandles()[index].entity != publicMessageTarget_ ||
			services_->service_managedBotHandles()[index].actor.actorGeneration != publicMessageActor_.generation ||
			actors_[index].teamInfoTeams != kTerroristTeamByte)
			return;
		if (std::strcmp(name, "BombPickup") == 0 && count == 0U)
			actors_[index].publicBombRadars.reset();
		else if (std::strcmp(name, "BombDrop") == 0 && count == kBombDropFieldCount &&
			publicMessageCoordMask_ == kBombDropCoordinateMask && publicMessageStringMask_ == 0U &&
			(v[3] == kDroppedBombState || v[3] == kPlantedBombState))
		{
			if (v[3] == kDroppedBombState)
				actors_[index].publicBombRadars.drop(
					publicMessageFrame_, {publicMessageCoords_[0], publicMessageCoords_[1], publicMessageCoords_[2]});
			else
				actors_[index].publicBombRadars.reset();
		}
		return;
	}
	if (publicMessageCoordMask_ != 0U)
		return;
	const auto integers = [this](std::uint16_t mask)
	{
		return publicMessageStringMask_ == mask;
	};
	if (std::strcmp(name, "WeaponList") == 0 && count == kWeaponListFieldCount && integers(kWeaponListStringMask))
	{
		(void)publicObservations_.weaponDefinition(
			v[7], v[1] == kMaximumMessageByte ? -1 : v[1], v[2] == kMaximumMessageByte ? -1 : v[2], v[5]);
		return;
	}
	if (std::strcmp(name, "DeathMsg") == 0 && count == kDeathMessageFieldCount && integers(kDeathMessageStringMask) &&
		v[1] > 0 && v[1] <= kMaximumClientSlot)
	{
		const std::size_t victim = static_cast<std::size_t>(v[1] - 1);
		services_->navigation_observePublicDeath(victim);
		services_->behavior_resetEquipment(victim);
		publicObservations_.resetSlot(static_cast<std::uint32_t>(v[1]));
		services_->behavior_observePublicDeath(victim);
		return;
	}
	if (std::strcmp(name, "TeamScore") == 0 && count == kTeamScoreFieldCount && integers(kTeamScoreStringMask))
	{
		const char *teamName = publicMessageStrings_[0U].data();
		const int team = std::strcmp(teamName, "TERRORIST") == 0 ? kTerroristTeam
			: std::strcmp(teamName, "CT") == 0					 ? kCounterTerroristTeam
																 : 0;
		if (team != 0 && v[1] >= 0)
		{
			teamScores_[team] = v[1];
			teamScoresKnown_[team] = true;
		}
		return;
	}
	if (std::strcmp(name, "ScoreAttrib") == 0 && count == kScoreAttributeFieldCount && integers(0U) && v[0] >= 1 &&
		v[0] <= kMaximumClientSlot && v[1] >= 0 && v[1] <= kMaximumMessageByte &&
		services_->service_engineFunctions() != nullptr &&
		services_->service_engineFunctions()->pfnPEntityOfEntIndex != nullptr)
	{
		edict_t *player = services_->service_engineFunctions()->pfnPEntityOfEntIndex(v[0]);
		if (player != nullptr && !player->free)
		{
			for (std::size_t i = 0U; i < NativeBotObservation::kClientSlotCount; ++i)
			{
				if ((publicMessageTarget_ == nullptr &&
						(publicMessageDestination_ == MSG_ALL || publicMessageDestination_ == MSG_BROADCAST)) ||
					(publicMessageActor_.isValid() && publicMessageActor_.slot == i + 1U))
				{
					actors_[i].publicScoreStatuses[static_cast<std::size_t>(v[0] - 1)] = {
						true, static_cast<std::uint32_t>(player->serialnumber), publicMessageFrame_, v[1]};
				}
			}
		}
		return;
	}
	if (std::strcmp(name, "TextMsg") == 0 && count >= kMinimumTextMessageFieldCount &&
		(publicMessageStringMask_ & kTextMessageTokenMask) != 0U)
	{
		const char *token = publicMessageStrings_[1U].data();
		if (publicMessageTarget_ != nullptr &&
			(publicMessageDestination_ == MSG_ONE || publicMessageDestination_ == MSG_ONE_UNRELIABLE) &&
			(std::strcmp(token, "#Ignore_Radio") == 0 || std::strcmp(token, "#Accept_Radio") == 0))
			services_->behavior_observeRadioPreference(publicMessageTarget_, std::strcmp(token, "#Ignore_Radio") == 0);
		const bool planted = std::strcmp(token, "#Bomb_Planted") == 0;
		const bool cleared = std::strcmp(token, "#Bomb_Defused") == 0 || std::strcmp(token, "#Target_Bombed") == 0;
		if (planted || cleared)
		{
			for (std::size_t i = 0U; i < NativeBotObservation::kClientSlotCount; ++i)
			{
				if ((publicMessageTarget_ == nullptr &&
						(publicMessageDestination_ == MSG_ALL || publicMessageDestination_ == MSG_BROADCAST)) ||
					(publicMessageActor_.isValid() && publicMessageActor_.slot == i + 1U))
				{
					actors_[i].bombNoticeKnown = true;
					actors_[i].bombNoticePlanted = planted;
					if (services_->service_globals() != nullptr)
						actors_[i].publicBombClocks.observe(
							publicMessageFrame_, services_->service_globals()->time, planted);
				}
			}
		}
		return;
	}
	if (std::strcmp(name, "SendAudio") == 0 && count == kSendAudioFieldCount && integers(kSendAudioStringMask) &&
		v[0] >= 1 && v[0] <= kMaximumClientSlot && services_->service_globals() != nullptr &&
		services_->service_engineFunctions() != nullptr &&
		services_->service_engineFunctions()->pfnPEntityOfEntIndex != nullptr)
	{
		edict_t *sender = services_->service_engineFunctions()->pfnPEntityOfEntIndex(v[0]);
		// Observe our own public radio receipt before excluding bot-originated
		// commands from the human-order planner below.
		if (sender != nullptr && !sender->free && (sender->v.flags & FL_FAKECLIENT) != 0 &&
			(publicMessageStringMask_ & kSendAudioStringMask) != 0U &&
			std::strncmp(publicMessageStrings_[1U].data(), "%!MRAD_", kRadioTokenPrefixLength) == 0)
		{
			const std::size_t senderIndex = static_cast<std::size_t>(v[0] - 1);
			if (services_->service_managedBotSlots()[senderIndex] &&
				services_->service_managedBotHandles()[senderIndex].entity == sender)
				services_->behavior_observeRadioReceipt(senderIndex);
		}
		if ((publicMessageTarget_ != nullptr && !publicMessageActor_.isValid()) ||
			(publicMessageTarget_ == nullptr && publicMessageDestination_ != MSG_ALL &&
				publicMessageDestination_ != MSG_BROADCAST))
		{
			return;
		}
		if (sender == nullptr || sender->free || (sender->v.flags & FL_FAKECLIENT) != 0 || sender->serialnumber <= 0)
		{
			return;
		}
		const char *token = publicMessageStrings_[1U].data();
		const char *const names[] = {"%!MRAD_FOLLOWME", "%!MRAD_COVERME", "%!MRAD_STICKTOG", "%!MRAD_REGROUP",
			"%!MRAD_BACKUP", "%!MRAD_POSITION", "%!MRAD_GO", "%!MRAD_GETOUT"};
		const compat::ScenarioRadioCommand commands[] = {compat::ScenarioRadioCommand::FollowMe,
			compat::ScenarioRadioCommand::CoverMe, compat::ScenarioRadioCommand::StickTogether,
			compat::ScenarioRadioCommand::Regroup, compat::ScenarioRadioCommand::NeedBackup,
			compat::ScenarioRadioCommand::HoldPosition, compat::ScenarioRadioCommand::GoGoGo,
			compat::ScenarioRadioCommand::GetOut};
		compat::ScenarioRadioObservation radio{};
		for (std::size_t i = 0U; i < kRadioCommandCount; ++i)
		{
			if (std::strcmp(names[i], token) == 0)
			{
				radio.command = commands[i];
				break;
			}
		}
		if (radio.command == compat::ScenarioRadioCommand::None)
		{
			return;
		}
		radio.sequence = ++radioSequence_;
		radio.sender = {static_cast<std::uint32_t>(v[0]), static_cast<std::uint32_t>(sender->serialnumber)};
		radio.senderHuman = true;
		radio.team = resolvePlayerTeam(sender) == kTerroristTeam ? objectives::TeamRole::Terrorist
			: resolvePlayerTeam(sender) == kCounterTerroristTeam ? objectives::TeamRole::CounterTerrorist
																 : objectives::TeamRole::Unknown;
		radio.receivedTime = services_->service_globals()->time;
		// A radio receipt reveals the order and sender, never the sender's coordinates.
		for (std::size_t i = 0U; i < services_->service_managedBotHandles().size(); ++i)
		{
			if (services_->service_managedBotSlots()[i] && actors_[i].teamInfoTeams == resolvePlayerTeam(sender) &&
				(!publicMessageActor_.isValid() || publicMessageActor_.slot == i + 1U))
			{
				if (actors_[i].scenarioRadio.sender == radio.sender &&
					actors_[i].scenarioRadio.command == radio.command &&
					actors_[i].scenarioRadio.receivedTime == radio.receivedTime)
				{
					continue;
				}
				actors_[i].scenarioRadio = radio;
			}
		}
		return;
	}
	if (!publicMessageActor_.isValid())
		return;
	if (std::strcmp(name, "CurWeapon") == 0 && count == kCurrentWeaponFieldCount && integers(0U))
		(void)publicObservations_.currentWeapon(
			publicMessageActor_, publicMessageFrame_, v[0], v[1], v[2] == kMaximumMessageByte ? -1 : v[2]);
	else if (std::strcmp(name, "AmmoX") == 0 && count == kAmmoFieldCount && integers(0U))
		(void)publicObservations_.ammo(publicMessageActor_, publicMessageFrame_, v[0], v[1]);
	else if (std::strcmp(name, "AmmoPickup") == 0 && count == kAmmoFieldCount && integers(0U))
		(void)publicObservations_.ammoPickup(publicMessageActor_, publicMessageFrame_, v[0], v[1]);
	else if (std::strcmp(name, "WeapPickup") == 0 && count == kWeaponPickupFieldCount && integers(0U))
		(void)publicObservations_.weaponPickup(publicMessageActor_, publicMessageFrame_, v[0]);
	else if (std::strcmp(name, "Money") == 0 && (count == kMoneyBaseFieldCount || count == kMoneyWithFlashFieldCount) &&
		integers(0U))
		(void)publicObservations_.money(publicMessageActor_, publicMessageFrame_, v[0]);
	else if (std::strcmp(name, "ArmorType") == 0 && count == kArmorTypeFieldCount && integers(0U))
		(void)publicObservations_.armorType(publicMessageActor_, publicMessageFrame_, v[0]);
	else if (std::strcmp(name, "StatusIcon") == 0 &&
		(count == kStatusIconBaseFieldCount || count == kStatusIconWithColorFieldCount) &&
		integers(kStatusIconStringMask))
		(void)publicObservations_.statusIcon(
			publicMessageActor_, publicMessageFrame_, publicMessageString_.data(), v[0]);
	else if (std::strcmp(name, "BarTime") == 0 && count == kBarTimeFieldCount && integers(0U))
		(void)publicObservations_.progress(publicMessageActor_, publicMessageFrame_, v[0],
			services_->service_globals() != nullptr ? services_->service_globals()->time : 0.0f);
	else if (std::strcmp(name, "ScreenFade") == 0 && count == kScreenFadeFieldCount && integers(0U) &&
		services_->service_globals() != nullptr && v[3] == kMaximumMessageByte && v[4] == kMaximumMessageByte &&
		v[5] == kMaximumMessageByte && v[6] > kMinimumBlindFadeAlpha)
	{
		// HUD fade duration is fixed point (1 << 12); private blindness remains unavailable.
		const float duration =
			static_cast<float>((std::max)(0, v[0]) + (std::max)(0, v[1])) / kFadeDurationUnitsPerSecond;
		(void)publicObservations_.blind(
			publicMessageActor_, publicMessageFrame_, services_->service_globals()->time + duration);
	}
	else if (std::strcmp(name, "ResetHUD") == 0)
	{
		services_->behavior_resetEquipment(publicMessageActor_.slot - 1U);
		services_->navigation_observeResetHud(publicMessageActor_.slot - 1U);
		publicObservations_.resetSlot(publicMessageActor_.slot);
		publicObservations_.bind(publicMessageActor_, publicMessageFrame_);
	}
}

void BotObservationRuntime::captureManagedCurrentWeapon()
{
	if (!userMessageWeaponMessageValid_ || userMessageFieldCount_ < kMinimumCurrentWeaponFieldCount ||
		userMessageTarget_ == nullptr || services_->service_engineFunctions() == nullptr ||
		services_->service_engineFunctions()->pfnIndexOfEdict == nullptr)
	{
		return;
	}
	const int slot = services_->service_engineFunctions()->pfnIndexOfEdict(userMessageTarget_);
	if (slot < static_cast<int>(runtime::LifecycleSession::kFirstClientSlot) ||
		slot > static_cast<int>(runtime::LifecycleSession::kClientSlotCount))
	{
		return;
	}
	const std::size_t index = static_cast<std::size_t>(slot - 1);
	if (!services_->service_managedBotSlots()[index] ||
		services_->service_managedBotHandles()[index].entity != userMessageTarget_)
	{
		return;
	}
	const FakeClientHandle &handle = services_->service_managedBotHandles()[index];
	const auto &current = actors_[index].managedBotCurrentWeapons;
	// CurWeapon also reports inactive inventory entries, sometimes after the
	// active entry. Only deactivation of the current weapon invalidates it.
	const bool inactiveOther = userMessageWeaponState_ == 0U && userMessageWeaponId_ > 0U &&
		userMessageWeaponId_ <= kMaximumCurrentWeaponId && current.available && current.active &&
		current.actorGeneration == handle.actor.actorGeneration &&
		current.mapGeneration == services_->service_lifecycle().mapGeneration() &&
		current.roundGeneration == services_->service_lifecycle().roundGeneration() &&
		current.weaponId != userMessageWeaponId_;
	if (!inactiveOther)
		actors_[index].managedBotCurrentWeapons = {true, userMessageWeaponState_ != 0U, userMessageWeaponId_,
			handle.actor.actorGeneration, services_->service_lifecycle().mapGeneration(),
			services_->service_lifecycle().roundGeneration(), services_->service_adapterFrameCount()};
	if (services_->service_runtimeProfiler().enabled() && gpMetaUtilFuncs != nullptr &&
		gpMetaUtilFuncs->pfnLogConsole != nullptr && services_->service_pluginId() != nullptr)
	{
		profiledLogConsole(services_->service_runtimeProfiler(), services_->service_pluginId(),
			"profile currentWeapon actor=%u actor_generation=%u map=%u round=%u "
			"frame=%u state=%u weapon_id=%u is_c4=%d",
			static_cast<unsigned int>(handle.actor.slot), static_cast<unsigned int>(handle.actor.actorGeneration),
			static_cast<unsigned int>(services_->service_lifecycle().mapGeneration()),
			static_cast<unsigned int>(services_->service_lifecycle().roundGeneration()),
			static_cast<unsigned int>(services_->service_adapterFrameCount()),
			static_cast<unsigned int>(userMessageWeaponState_), static_cast<unsigned int>(userMessageWeaponId_),
			userMessageWeaponState_ != 0U && userMessageWeaponId_ == ActionAdapter::kC4WeaponId ? 1 : 0);
	}
}

runtime::MovementPhysicsState BotObservationRuntime::captureMovementPhysicsState(
	const edict_t *entity, bool teamConfirmed, bool managedFakeClient) const
{
	runtime::MovementPhysicsState state{};
	if (entity == nullptr)
	{
		return state;
	}
	state.entityValid = true;
	state.fakeClient = managedFakeClient || (entity->v.flags & FL_FAKECLIENT) != 0;
	state.spectator = (entity->v.flags & FL_SPECTATOR) != 0;
	state.dead = entity->v.deadflag != DEAD_NO || entity->v.health <= 0.0f;
	state.grounded = (entity->v.flags & FL_ONGROUND) != 0;
	state.ducked = (entity->v.flags & FL_DUCKING) != 0;
	state.onLadder = entity->v.movetype == MOVETYPE_FLY;
	state.flags = entity->v.flags;
	state.deadflag = entity->v.deadflag;
	state.team = resolvePlayerTeam(entity);
	state.teamConfirmed = teamConfirmed;
	state.solid = entity->v.solid;
	state.movetype = entity->v.movetype;
	state.health = entity->v.health;
	state.origin = {entity->v.origin.x, entity->v.origin.y, entity->v.origin.z};
	state.velocity = {entity->v.velocity.x, entity->v.velocity.y, entity->v.velocity.z};
	if (entity->v.groundentity != nullptr && services_->service_engineFunctions() != nullptr &&
		services_->service_engineFunctions()->pfnIndexOfEdict != nullptr)
	{
		const int groundIndex = services_->service_engineFunctions()->pfnIndexOfEdict(entity->v.groundentity);
		if (groundIndex >= 0)
		{
			state.groundEntityIndex = static_cast<std::uint32_t>(groundIndex);
		}
	}
	return state;
}

bool BotObservationRuntime::readManagedWorldSnapshot(std::size_t index, world::WorldSnapshot *snapshot) const
{
	if (index >= actors_.size() || snapshot == nullptr || !actors_[index].managedBotPerception.snapshot().isValid())
	{
		return false;
	}
	*snapshot = actors_[index].managedBotPerception.snapshot();
	return true;
}

bool BotObservationRuntime::collectManagedPerception(
	std::size_t index, world::WorldSnapshot *snapshot, ThreatObservation *threats)
{
	RuntimeProfilerScope profilerScope(services_->service_runtimeProfiler(), RuntimeProfilerStage::Observation);
	if (index >= services_->service_managedBotHandles().size() || snapshot == nullptr || threats == nullptr ||
		services_->service_engineFunctions() == nullptr ||
		services_->service_engineFunctions()->pfnPEntityOfEntIndex == nullptr ||
		services_->service_globals() == nullptr)
	{
		return false;
	}
	const FakeClientHandle &handle = services_->service_managedBotHandles()[index];
	if (handle.entity == nullptr || handle.actor.actorGeneration == 0U)
	{
		return false;
	}
	const auto collectVisibleThreats = [this, index, threats](
										   const world::WorldSnapshot &belief, const world::ActorKey &observer) -> bool
	{
		*threats = {};
		auto &candidates = threats->candidates;
		auto &candidateCount = threats->count;
		const world::ActorObservation *observerObservation = nullptr;
		if (belief.findActor(observer, &observerObservation) != world::ContactLookupResult::Found ||
			observerObservation == nullptr || !observerObservation->hasCurrentPosition())
		{
			return false;
		}
		threats->observerAvailable = true;
		for (std::size_t actorIndex = 0U; actorIndex < belief.actorCount(); ++actorIndex)
		{
			const world::ActorObservation *candidate = belief.actorAt(actorIndex);
			if (candidate == nullptr || candidate->actor == observer ||
				candidate->relation != world::TeamRelation::Hostile || !candidate->visible ||
				!candidate->hasCurrentPosition())
			{
				continue;
			}
			const float dx = candidate->position.x - observerObservation->position.x;
			const float dy = candidate->position.y - observerObservation->position.y;
			const float dz = candidate->position.z - observerObservation->position.z;
			const float distanceSquared = dx * dx + dy * dy + dz * dz;
			if (candidateCount < candidates.size())
			{
				world::WorldPosition visiblePoint = candidate->position;
				if ((candidate->visibleParts & world::VisibleHead) != 0U)
				{
					visiblePoint.z += kHeadProbeHeight;
				}
				else if ((candidate->visibleParts & world::VisibleChest) == 0U &&
					(candidate->visibleParts & world::VisibleFeet) != 0U)
				{
					visiblePoint.z -= kFeetTargetProbeDepth;
				}
				bool attackingObserver = false;
				if (services_->service_engineFunctions() != nullptr &&
					services_->service_engineFunctions()->pfnPEntityOfEntIndex != nullptr &&
					services_->service_engineFunctions()->pfnTraceLine != nullptr)
				{
					edict_t *enemy = services_->service_engineFunctions()->pfnPEntityOfEntIndex(
						static_cast<int>(candidate->actor.slot));
					if (enemy != nullptr && !enemy->free && (enemy->v.button & IN_ATTACK) != 0)
					{
						const float yaw = enemy->v.v_angle.y * ASTRABOT_BOT_OBSERVATION_RUNTIME_PI / kHalfTurnDegrees;
						const float pitch = enemy->v.v_angle.x * ASTRABOT_BOT_OBSERVATION_RUNTIME_PI / kHalfTurnDegrees;
						const float eye[] = {enemy->v.origin.x + enemy->v.view_ofs.x,
							enemy->v.origin.y + enemy->v.view_ofs.y, enemy->v.origin.z + enemy->v.view_ofs.z};
						const float end[] = {eye[0] + kAimTraceDistance * std::cos(pitch) * std::cos(yaw),
							eye[1] + kAimTraceDistance * std::cos(pitch) * std::sin(yaw),
							eye[2] - kAimTraceDistance * std::sin(pitch)};
						TraceResult trace{};
						services_->service_engineFunctions()->pfnTraceLine(
							eye, end, kObservationTraceIncludingActors, enemy, &trace);
						attackingObserver = trace.pHit == services_->service_managedBotHandles()[index].entity;
					}
				}
				candidates[candidateCount++] = {
					candidate->actor, visiblePoint, std::sqrt(distanceSquared), attackingObserver};
			}
		}
		return true;
	};
	const world::ActorKey observer = {handle.actor.slot, handle.actor.actorGeneration};
	if (actors_[index].managedBotPerceptionFullUpdates == services_->service_managedBotFullUpdateSequences()[index] &&
		actors_[index].managedBotPerception.snapshot().isValid())
	{
		*snapshot = actors_[index].managedBotPerception.snapshot();
		(void)collectVisibleThreats(*snapshot, observer);
		return true;
	}
	const world::FrameIdentity frame = {services_->service_lifecycle().mapGeneration(),
		services_->service_lifecycle().roundGeneration(), services_->service_adapterFrameCount()};
	const world::SnapshotIdentity identity = {frame, observer, 0U};
	perception::PerceptionInput input(identity);
	const compat::ObservationTimingContext observationTiming = {
		services_->command_managedBotCommandSequences(index), 0U, 0U};
	compat::CompatibilityObservation observerObservation = {};
	if (observationAdapter_.collectActor(handle.entity, observer, frame, observationTiming, &observerObservation) !=
		ObservationAdapterResult::Accepted)
	{
		return false;
	}
	const int observerTeam = resolvePlayerTeam(handle.entity);
	const int maxClients = std::max(0, std::min(services_->service_globals()->maxClients, kMaximumClientSlot));
	for (int slot = 1; slot <= maxClients; ++slot)
	{
		edict_t *entity = services_->service_engineFunctions()->pfnPEntityOfEntIndex(slot);
		const int entityTeam = resolvePlayerTeam(entity);
		if (entity == nullptr || entity->v.deadflag != DEAD_NO || entity->v.health <= 0.0f ||
			(entity->v.flags & FL_SPECTATOR) != 0 || entityTeam < 1)
		{
			continue;
		}
		const auto token = services_->service_lifecycle().tokenForSlot(static_cast<std::uint32_t>(slot));
		// Self belongs to SnapshotIdentity's managed observer. Contacts remain
		// connection-scoped, so replacing any client invalidates old target keys.
		const world::ActorKey actor = entity == handle.entity
			? observer
			: world::ActorKey{static_cast<std::uint32_t>(slot), token.slotGeneration == 0U ? 1U : token.slotGeneration};
		compat::CompatibilityObservation compatObservation = {};
		if (observationAdapter_.collectActor(entity, actor, frame, observationTiming, &compatObservation) !=
				ObservationAdapterResult::Accepted ||
			!compatObservation.player.origin.isAvailable() || !compatObservation.player.velocity.isAvailable())
		{
			continue;
		}
		perception::VisionObservation visibility = {};
		const bool isObserver = actor == observer;
		const bool visible = isObserver ||
			(observationAdapter_.collectVisibility(handle.entity, entity, actor, frame, &visibility) ==
					ObservationAdapterResult::Accepted &&
				visibility.visible);
		world::ActorObservation observation = {};
		observation.actor = actor;
		observation.relation =
			entityTeam == observerTeam ? world::TeamRelation::Friendly : world::TeamRelation::Hostile;
		observation.state =
			visible ? world::ObservationState::ObservedPresent : world::ObservationState::ObservedAbsent;
		observation.position = visible ? compatObservation.player.origin.value : world::WorldPosition{};
		observation.velocity = visible ? compatObservation.player.velocity.value : world::WorldVelocity{};
		observation.confidence = visible ? world::ContactConfidence{1.0f, 0U} : world::ContactConfidence{0.0f, 0U};
		observation.visible = visible;
		observation.fovPassed = isObserver || visibility.fovPassed;
		observation.losPassed = isObserver || visibility.losPassed;
		observation.visibleParts = isObserver
			? static_cast<std::uint8_t>(world::VisibleChest | world::VisibleHead | world::VisibleFeet |
				  world::VisibleLeftSide | world::VisibleRightSide)
			: visibility.visibleParts;
		if (input.addActor(observation) != perception::PerceptionInputResult::Accepted)
		{
			return false;
		}
	}
	const std::uint64_t firstSound = (std::max)(std::uint64_t(actors_[index].deliveredSoundSequences) + 1U,
		soundSequence_ > soundEvents_.size() ? std::uint64_t(soundSequence_ - soundEvents_.size() + 1U)
											 : std::uint64_t(1U));
	for (std::uint64_t sequence = firstSound; sequence <= soundSequence_; ++sequence)
	{
		const auto &observed = soundEvents_[sequence % soundEvents_.size()];
		if (observed.id != sequence || observed.source.slot == handle.actor.slot ||
			(soundTeams_[sequence % soundTeams_.size()] != 0 &&
				soundTeams_[sequence % soundTeams_.size()] == actors_[index].teamInfoTeams))
		{
			continue;
		}
		const float dx = observed.origin.x - handle.entity->v.origin.x;
		const float dy = observed.origin.y - handle.entity->v.origin.y;
		const float dz = observed.origin.z - handle.entity->v.origin.z;
		const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
		if (std::isfinite(distance) && distance < observed.distance)
		{
			world::AudibleEvent audible = observed;
			audible.distance = distance;
			(void)input.addSound(audible);
		}
	}
	actors_[index].deliveredSoundSequences = soundSequence_;
	perception::PerceptionAssembler &assembler = actors_[index].managedBotPerception;
	RuntimeProfilerScope worldPublishScope(services_->service_runtimeProfiler(), RuntimeProfilerStage::WorldPublish);
	if (assembler.publish(input, snapshot) != perception::PerceptionResult::Published)
	{
		return false;
	}
	actors_[index].managedBotPerceptionFullUpdates = services_->service_managedBotFullUpdateSequences()[index];
	*snapshot = assembler.snapshot();
	(void)collectVisibleThreats(*snapshot, observer);
	return true;
}

bool BotObservationRuntime::observeObjective(
	std::size_t index, edict_t *entity, bool planted, world::WorldVector *position, bool *currentlyVisible)
{
	if (currentlyVisible != nullptr)
		*currentlyVisible = false;
	if (index >= services_->service_managedBotHandles().size() || entity == nullptr || entity->free != 0 ||
		position == nullptr || services_->service_engineFunctions() == nullptr ||
		services_->service_engineFunctions()->pfnIndexOfEdict == nullptr)
		return false;
	const FakeClientHandle &handle = services_->service_managedBotHandles()[index];
	if (handle.entity == nullptr)
		return false;
	const bool compatibility =
		services_->service_compatibilitySurface().configuration().mode == compat::RuntimeMode::Compatibility;
	ObjectiveKnowledge &knowledge = actors_[index].objectiveKnowledge[planted ? 1U : 0U];
	const world::EntityKey key{
		static_cast<std::uint32_t>(services_->service_engineFunctions()->pfnIndexOfEdict(entity)),
		static_cast<std::uint32_t>(entity->serialnumber)};
	const float eye[] = {handle.entity->v.origin.x + handle.entity->v.view_ofs.x,
		handle.entity->v.origin.y + handle.entity->v.view_ofs.y,
		handle.entity->v.origin.z + handle.entity->v.view_ofs.z};
	const float target[] = {entity->v.origin.x, entity->v.origin.y, entity->v.origin.z};
	const float dx = target[0] - eye[0];
	const float dy = target[1] - eye[1];
	const float length = std::hypot(dx, dy);
	const float angle = handle.entity->v.v_angle.y * ASTRABOT_BOT_OBSERVATION_RUNTIME_PI / kHalfTurnDegrees;
	// The reference gives T the planted location on the announced plant event,
	// not merely because the server scan found a grenade (CSGameState::OnEvent).
	bool visible = !compatibility ||
		(planted && actors_[index].teamInfoTeams == 1U && actors_[index].bombNoticeKnown &&
			actors_[index].bombNoticePlanted);
	if (!planted && compatibility && actors_[index].teamInfoTeams == 1U)
	{
		world::WorldVector radar{};
		if (actors_[index].publicBombRadars.read(
				{services_->service_lifecycle().mapGeneration(), services_->service_lifecycle().roundGeneration(),
					services_->service_adapterFrameCount()},
				&radar))
		{
			knowledge.entity = key;
			knowledge.position = radar;
			*position = radar;
			return true;
		}
	}
	compat::PublicActorState publicState{};
	const bool blind = services_->service_globals() != nullptr &&
		publicObservations_.read({handle.actor.slot, handle.actor.actorGeneration},
			{services_->service_lifecycle().mapGeneration(), services_->service_lifecycle().roundGeneration(),
				services_->service_adapterFrameCount()},
			&publicState) &&
		services_->service_globals()->time < publicState.blindUntil;
	const bool smokeBlocked = services_->service_globals() != nullptr &&
		smokeObservations_.blocks(
			{eye[0], eye[1], eye[2]}, {target[0], target[1], target[2]}, services_->service_globals()->time);
	if (!visible && !blind && !smokeBlocked && services_->service_engineFunctions()->pfnTraceLine != nullptr &&
		(length < ASTRABOT_BOT_OBSERVATION_RUNTIME_MINIMUM_VIEW_DIRECTION_LENGTH ||
			(dx * std::cos(angle) + dy * std::sin(angle)) / length > kMinimumViewConeDot))
	{
		TraceResult trace{};
		services_->service_engineFunctions()->pfnTraceLine(
			eye, target, kObservationTraceIgnoringActors, handle.entity, &trace);
		visible = trace.flFraction == 1.0f || trace.pHit == entity;
	}
	if (visible)
	{
		knowledge.entity = key;
		knowledge.position = {target[0], target[1], target[2]};
	}
	if (!visible && (!key.isValid() || knowledge.entity.id != key.id || knowledge.entity.generation != key.generation))
		return false;
	*position = knowledge.position;
	if (currentlyVisible != nullptr)
		*currentlyVisible = visible;
	return true;
}

int BotObservationRuntime::resolvePlayerTeam(const edict_t *entity) const
{
	if (entity == nullptr || entity->free || services_->service_engineFunctions() == nullptr ||
		services_->service_engineFunctions()->pfnIndexOfEdict == nullptr)
		return 0;
	const int slot = services_->service_engineFunctions()->pfnIndexOfEdict(entity);
	if (slot >= 1 && slot <= kMaximumClientSlot && actors_[static_cast<std::size_t>(slot - 1)].publicTeamKnown)
		return actors_[static_cast<std::size_t>(slot - 1)].teamInfoTeams;
	return entity->v.team >= kTerroristTeam && entity->v.team <= kCounterTerroristTeam ? entity->v.team : 0;
}

runtime::ActorId BotObservationRuntime::publicPlayerIdentity(const edict_t *entity) const
{
	if (entity == nullptr || entity->free || services_->service_engineFunctions() == nullptr ||
		services_->service_engineFunctions()->pfnIndexOfEdict == nullptr)
		return {};
	const int slot = services_->service_engineFunctions()->pfnIndexOfEdict(entity);
	if (slot < 1 || slot > kMaximumClientSlot)
		return {};
	const auto index = static_cast<std::size_t>(slot - 1);
	if (services_->service_managedBotSlots()[index] && services_->service_managedBotHandles()[index].entity == entity)
		return services_->service_managedBotHandles()[index].actor;
	return {static_cast<std::uint32_t>(slot), static_cast<std::uint32_t>(entity->serialnumber) + 1U};
}

std::vector<nav::NavVector> BotObservationRuntime::liveFriendlyPositions(std::size_t index, bool feet) const
{
	std::vector<nav::NavVector> positions;
	if (index >= services_->service_managedBotHandles().size() || services_->service_globals() == nullptr ||
		services_->service_engineFunctions() == nullptr ||
		services_->service_engineFunctions()->pfnPEntityOfEntIndex == nullptr)
		return positions;
	const auto *self = services_->service_managedBotHandles()[index].entity;
	const int team = resolvePlayerTeam(self);
	if (team == 0)
		return positions;
	for (int slot = 1; slot <= (std::min)(kMaximumClientSlot, services_->service_globals()->maxClients); ++slot)
	{
		const auto *other = services_->service_engineFunctions()->pfnPEntityOfEntIndex(slot);
		if (other == nullptr || other == self || other->free || (other->v.flags & FL_CLIENT) == 0 ||
			other->v.deadflag != DEAD_NO || other->v.health <= 0.0f || resolvePlayerTeam(other) != team)
			continue;
		const float offset = feet ? other->v.mins.z : 0.0f;
		const nav::NavVector position = {other->v.origin.x, other->v.origin.y, other->v.origin.z + offset};
		if (std::isfinite(position.x) && std::isfinite(position.y) && std::isfinite(position.z))
			positions.push_back(position);
	}
	return positions;
}

void BotObservationRuntime::onActorRemoved(std::size_t index)
{
	publicObservations_.resetSlot(static_cast<std::uint32_t>(index + 1U));
	actors_[index].publicBombRadars.reset();
	actors_[index].scenarioRadio = {};
	actors_[index].publicScoreStatuses = {};
	actors_[index].bombNoticeKnown = false;
	actors_[index].bombNoticePlanted = false;
	actors_[index].publicBombClocks.reset();
	actors_[index].deliveredSoundSequences = soundSequence_;
	actors_[index].objectiveKnowledge = {};
	actors_[index].teamInfoTeams = 0U;
	actors_[index].publicTeamKnown = false;
	actors_[index].managedBotPerception = perception::PerceptionAssembler();
	actors_[index].managedBotPerceptionFullUpdates = (std::numeric_limits<std::uint32_t>::max)();
}

void BotObservationRuntime::onActorRemembered(std::size_t index, const FakeClientHandle &handle)
{
	const world::ActorKey actor = {handle.actor.slot, handle.actor.actorGeneration};
	actors_[index].teamInfoTeams = 0U;
	actors_[index].publicTeamKnown = false;
	publicObservations_.bind(actor,
		{services_->service_lifecycle().mapGeneration(), services_->service_lifecycle().roundGeneration(),
			services_->service_adapterFrameCount()});
}

void BotObservationRuntime::resetSharedExecutionState()
{
	fill_managedBotPerceptionFullUpdates((std::numeric_limits<std::uint32_t>::max)());
}

void BotObservationRuntime::resetActorExecutionState(std::size_t index)
{
	actors_[index].managedBotCurrentWeapons = {};
	actors_[index].managedBotPerception = perception::PerceptionAssembler();
}

void BotObservationRuntime::configureEngine()
{
	observationAdapter_.configure(services_->service_engineFunctions(), services_->service_globals());
	observationAdapter_.setObservationProviders(&publicObservations_, &optionalObservations_);
	observationAdapter_.setSmokeObservations(&smokeObservations_);
	observationAdapter_.setProfiler(&services_->service_runtimeProfiler());
}

void BotObservationRuntime::resetRoundWeapons()
{
	fill_managedBotCurrentWeapons(ManagedCurrentWeaponObservation{});
}

void BotObservationRuntime::resetRoundObjectiveKnowledge()
{
	reset_objectiveKnowledge();
	reset_publicScoreStatuses();
	reset_bombNoticeKnown();
	reset_bombNoticePlanted();
	reset_publicBombClocks();
}

void BotObservationRuntime::resetRoundRadioAndSound()
{
	soundEvents_ = {};
	reset_scenarioRadio();
	radioSequence_ = 0U;
	soundSequence_ = 0U;
	reset_deliveredSoundSequences();
}

void BotObservationRuntime::rebindRoundObservations()
{
	smokeObservations_.reset();
	publicSmokeEntities_ = {};
	nextSmokeEntity_ = 0U;
	for (std::size_t i = 0U; i < services_->service_managedBotHandles().size(); ++i)
	{
		publicObservations_.resetSlot(static_cast<std::uint32_t>(i + 1U));
		const auto &handle = services_->service_managedBotHandles()[i];
		if (handle.actor.actorGeneration != 0U)
			publicObservations_.bind({handle.actor.slot, handle.actor.actorGeneration},
				{services_->service_lifecycle().mapGeneration(), services_->service_lifecycle().roundGeneration(),
					services_->service_adapterFrameCount()});
	}
}

void BotObservationRuntime::probeMapProvider()
{
	optionalObservations_.probeLoadedGameDll();
}

void BotObservationRuntime::resetMapPublicObservations()
{
	if (gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr &&
		services_->service_pluginId() != nullptr)
	{
		importantLogConsole(services_->service_runtimeProfiler(), services_->service_pluginId(),
			"observation provider: %s (API %d.%d)", optionalObservations_.diagnostic(),
			optionalObservations_.majorVersion(), optionalObservations_.minorVersion());
	}
	publicObservations_.reset();
	teamScores_ = {};
	teamScoresKnown_ = {};
	reset_scenarioRadio();
	radioSequence_ = 0U;
}

void BotObservationRuntime::resetMapSoundAndSmoke()
{
	soundEvents_ = {};
	soundSequence_ = 0U;
	reset_deliveredSoundSequences();
	smokeObservations_.reset();
	publicSmokeEntities_ = {};
	nextSmokeEntity_ = 0U;
}

void BotObservationRuntime::resetMapObjectiveKnowledge()
{
	reset_objectiveKnowledge();
}

void BotObservationRuntime::deactivateMapProvider()
{
	optionalObservations_.reset();
}

void BotObservationRuntime::configurePerformance(bool disableVision, bool disableTrace)
{
	observationAdapter_.setPerformanceToggles(disableVision, disableTrace);
}

void BotObservationRuntime::observeExternalCommand(const edict_t *entity, std::uint16_t buttons)
{

	if (!services_->runtimeIsActive() || entity == nullptr || entity->free != 0 ||
		services_->service_engineFunctions() == nullptr ||
		services_->service_engineFunctions()->pfnIndexOfEdict == nullptr || services_->service_globals() == nullptr ||
		!std::isfinite(services_->service_globals()->time))
	{
		return;
	}

	const int entityIndex = services_->service_engineFunctions()->pfnIndexOfEdict(entity);
	if (entityIndex <= 0)
	{
		return;
	}
	for (const FakeClientHandle &handle : services_->service_managedBotHandles())
	{
		if (handle.entity == entity)
		{
			return;
		}
	}

	const bool sameExternalActor = static_cast<std::uint32_t>(entityIndex) == externalDefuserEntityIndex_ &&
		static_cast<std::uint32_t>(entity->serialnumber) == externalDefuserEntitySerial_;
	const bool usingButton = (buttons & static_cast<std::uint16_t>(IN_USE)) != 0U;
	const bool livingCounterTerrorist = resolvePlayerTeam(entity) == kCounterTerroristTeam &&
		entity->v.deadflag == DEAD_NO && std::isfinite(entity->v.health) && entity->v.health > 0.0f;
	if (!usingButton || !livingCounterTerrorist || !std::isfinite(entity->v.origin.x) ||
		!std::isfinite(entity->v.origin.y) || !std::isfinite(entity->v.origin.z))
	{
		if (sameExternalActor)
		{
			externalDefuserEntityIndex_ = 0U;
			externalDefuserEntitySerial_ = 0U;
			externalDefuserLeaseUntil_ = 0.0f;
			services_->behavior_invalidateObjectiveAssignments();
		}
		return;
	}
	if (services_->service_engineFunctions()->pfnFindEntityByString == nullptr ||
		services_->service_engineFunctions()->pfnSzFromIndex == nullptr)
	{
		return;
	}

	constexpr float kDefuseUseRadius = 100.0f;
	constexpr float kDefuseUseRadiusSquared = kDefuseUseRadius * kDefuseUseRadius;
	edict_t *searchAfter = nullptr;
	bool nearPlantedBomb = false;
	constexpr int kMaximumDefuserSearchEntities = 2048;
	const int searchLimit =
		(std::max)(1, (std::min)(services_->service_globals()->maxEntities, kMaximumDefuserSearchEntities));
	for (int searchIndex = 0; searchIndex < searchLimit; ++searchIndex)
	{
		edict_t *candidate =
			services_->service_engineFunctions()->pfnFindEntityByString(searchAfter, "classname", "grenade");
		if (candidate == nullptr || candidate == searchAfter)
		{
			break;
		}
		searchAfter = candidate;
		if (candidate->free != 0)
		{
			continue;
		}
		const char *classname = services_->service_engineFunctions()->pfnSzFromIndex(candidate->v.classname);
		const char *model = services_->service_engineFunctions()->pfnSzFromIndex(candidate->v.model);
		if (classname == nullptr || std::strcmp(classname, "grenade") != 0 || model == nullptr ||
			std::strstr(model, "w_c4.mdl") == nullptr || !std::isfinite(candidate->v.origin.x) ||
			!std::isfinite(candidate->v.origin.y) || !std::isfinite(candidate->v.origin.z))
		{
			continue;
		}

		const float dx = entity->v.origin.x - candidate->v.origin.x;
		const float dy = entity->v.origin.y - candidate->v.origin.y;
		const float dz = entity->v.origin.z - candidate->v.origin.z;
		if (dx * dx + dy * dy + dz * dz <= kDefuseUseRadiusSquared)
		{
			nearPlantedBomb = true;
			break;
		}
	}
	if (!nearPlantedBomb)
	{
		if (sameExternalActor)
		{
			externalDefuserEntityIndex_ = 0U;
			externalDefuserEntitySerial_ = 0U;
			externalDefuserLeaseUntil_ = 0.0f;
			services_->behavior_invalidateObjectiveAssignments();
		}
		return;
	}

	externalDefuserEntityIndex_ = static_cast<std::uint32_t>(entityIndex);
	externalDefuserEntitySerial_ = static_cast<std::uint32_t>(entity->serialnumber);
	constexpr float kExternalDefuserLeaseSeconds = 0.25f;
	externalDefuserLeaseUntil_ = services_->service_globals()->time + kExternalDefuserLeaseSeconds;
	externalDefuserMapGeneration_ = services_->service_lifecycle().mapGeneration();
	externalDefuserRoundGeneration_ = services_->service_lifecycle().roundGeneration();
	services_->behavior_invalidateObjectiveAssignments();
}

void BotObservationRuntime::refreshPublicSmoke()
{
	if (services_->service_engineFunctions() != nullptr &&
		services_->service_engineFunctions()->pfnPEntityOfEntIndex != nullptr)
	{
		for (const PublicSmoke &smoke : publicSmokeEntities_)
		{
			if (!smoke.entity.isValid())
			{
				continue;
			}
			edict_t *entity =
				services_->service_engineFunctions()->pfnPEntityOfEntIndex(static_cast<int>(smoke.entity.id));
			if (entity != nullptr && !entity->free &&
				static_cast<std::uint32_t>(entity->serialnumber) == smoke.entity.generation)
			{
				smokeObservations_.observe(smoke.entity, smoke.position,
					services_->service_globals()->time + kSmokeObservationLifetimeSeconds);
			}
		}
	}
}

void BotObservationRuntime::observeRadioLeaderPosition(std::size_t index, const compat::ScenarioTarget &position)
{
	actors_[index].scenarioRadio.position = position;
}

void BotObservationRuntime::observeReloadDispatched(world::ActorKey actor, world::FrameIdentity frame, float now)
{
	publicObservations_.requestReload(actor, frame, now);
}

} // namespace metamod
} // namespace astrabot
#undef ASTRABOT_BOT_OBSERVATION_RUNTIME_PI
#undef ASTRABOT_BOT_OBSERVATION_RUNTIME_MINIMUM_VIEW_DIRECTION_LENGTH
