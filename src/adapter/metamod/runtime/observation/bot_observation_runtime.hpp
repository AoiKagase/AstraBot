#ifndef ASTRABOT_ADAPTER_BOT_OBSERVATION_RUNTIME_HPP
#define ASTRABOT_ADAPTER_BOT_OBSERVATION_RUNTIME_HPP
#include "runtime/shared/bot_runtime_services.hpp"
#include "astrabot/compat/gunshot_events.hpp"
#include <functional>
#include <limits>
namespace astrabot
{
namespace metamod
{
class BotObservationRuntime
{
public:
	void bindServices(BotObservationServices &services)
	{
		services_ = &services;
	}

	void appendPublicMessageInteger(int value);
	bool collectManagedPerception(std::size_t index, world::WorldSnapshot *snapshot, ThreatObservation *threats);
	bool readManagedWorldSnapshot(std::size_t index, world::WorldSnapshot *snapshot) const;
	bool consumeNoise(std::size_t index, const world::NoiseMemory &expected)
	{
		return index < actors_.size() && actors_[index].managedBotPerception.consumeNoise(expected);
	}
	void captureManagedCurrentWeapon();
	std::function<void()> prepareGameDllBlind(const void *player, float duration,
		const std::function<const void *(const char *)> &findClientObject);
	void observeGameDllDamage(const void *player, const entvars_t *inflictor,
		const std::function<const void *(const char *)> &findClientObject);
	void observeGameDllBotEvent(NativeBotEvent kind, const void *player);
	bool nativeBotEventsConnected() const;
	unsigned nativeDefuserSlot() const;
	bool dispatchingGameDllEvents() const { return optionalObservations_.dispatchingEvents(); }
	runtime::MovementPhysicsState captureMovementPhysicsState(
		const edict_t *entity, bool teamConfirmed, bool managedFakeClient) const;
	void capturePublicMessage();
	bool hasObservedActiveWeapon(std::size_t index) const;
	std::vector<nav::NavVector> liveFriendlyPositions(std::size_t index, bool feet) const;
	void notifyTeamInfo(std::uint8_t slot, const char *teamName);
	bool hasLooseBombRadar(std::size_t index, const world::FrameIdentity &frame) const;
	bool observeObjective(std::size_t index, edict_t *entity, bool planted, world::WorldVector *position,
		bool *currentlyVisible = nullptr);
	void onEmitSound(edict_t *entity, const char *sample, float volume, float attenuation);
	void onEmitSoundAt(edict_t *entity, const char *sample, float volume, float attenuation,
		const world::WorldVector &origin);
	void onPrecacheEvent(int type, const char *script, unsigned short index);
	void onPlaybackEvent(
		int flags, const edict_t *invoker, unsigned short index, float delay, int bparam1, int bparam2);
	void resetGunshotEvents()
	{
		gunshotEvents_.reset();
	}
	void onMessageBegin(int messageDestination, int messageType, const float *origin, edict_t *entity);
	void onMessageEnd();
	void onWriteByte(int value);
	void onWriteChar(int value);
	void onWriteCoord(float value);
	void onWriteLong(int value);
	void onWriteShort(int value);
	void onWriteString(const char *value);
	runtime::ActorId publicPlayerIdentity(const edict_t *entity) const;
	int resolvePlayerTeam(const edict_t *entity) const;
	using View_observationAdapter = ObservationAdapter;
	const View_observationAdapter &observationAdapter() const
	{
		return observationAdapter_;
	}

	using View_publicObservations = compat::PublicObservationStore;
	const View_publicObservations &publicObservations() const
	{
		return publicObservations_;
	}

	const compat::ScenarioRadioObservation &scenarioRadio(std::size_t index) const
	{
		return actors_[index].scenarioRadio;
	}

	using View_teamScores = std::array<int, kTeamScoreEntryCount>;
	const View_teamScores &teamScores() const
	{
		return teamScores_;
	}

	using View_teamScoresKnown = std::array<bool, kTeamScoreEntryCount>;
	const View_teamScoresKnown &teamScoresKnown() const
	{
		return teamScoresKnown_;
	}

	const std::array<PublicScoreStatus, NativeBotObservation::kClientSlotCount> &publicScoreStatuses(
		std::size_t index) const
	{
		return actors_[index].publicScoreStatuses;
	}

	const bool &bombNoticeKnown(std::size_t index) const
	{
		return actors_[index].bombNoticeKnown;
	}

	const bool &bombNoticePlanted(std::size_t index) const
	{
		return actors_[index].bombNoticePlanted;
	}

	const compat::PublicBombClock &publicBombClocks(std::size_t index) const
	{
		return actors_[index].publicBombClocks;
	}

	using View_smokeObservations = compat::SmokeObservations;
	const View_smokeObservations &smokeObservations() const
	{
		return smokeObservations_;
	}

	const std::uint8_t &teamInfoTeams(std::size_t index) const
	{
		return actors_[index].teamInfoTeams;
	}

	const bool &publicTeamKnown(std::size_t index) const
	{
		return actors_[index].publicTeamKnown;
	}

	const std::array<ObjectiveKnowledge, kObjectiveKnowledgeStateCount> &objectiveKnowledge(std::size_t index) const
	{
		return actors_[index].objectiveKnowledge;
	}

	using View_externalDefuserEntityIndex = std::uint32_t;
	const View_externalDefuserEntityIndex &externalDefuserEntityIndex() const
	{
		return externalDefuserEntityIndex_;
	}

	using View_externalDefuserLeaseUntil = float;
	const View_externalDefuserLeaseUntil &externalDefuserLeaseUntil() const
	{
		return externalDefuserLeaseUntil_;
	}

	using View_externalDefuserMapGeneration = std::uint32_t;
	const View_externalDefuserMapGeneration &externalDefuserMapGeneration() const
	{
		return externalDefuserMapGeneration_;
	}

	using View_externalDefuserRoundGeneration = std::uint32_t;
	const View_externalDefuserRoundGeneration &externalDefuserRoundGeneration() const
	{
		return externalDefuserRoundGeneration_;
	}

	const ManagedCurrentWeaponObservation &managedBotCurrentWeapons(std::size_t index) const
	{
		return actors_[index].managedBotCurrentWeapons;
	}

	const perception::PerceptionAssembler &managedBotPerception(std::size_t index) const
	{
		return actors_[index].managedBotPerception;
	}

	void onActorRemoved(std::size_t index);

	void onActorRemembered(std::size_t index, const FakeClientHandle &handle);

	void resetSharedExecutionState();

	void resetActorExecutionState(std::size_t index);

	void configureEngine();

	void resetRoundWeapons();

	void resetRoundObjectiveKnowledge();

	void resetRoundRadioAndSound();

	void rebindRoundObservations();

	void probeMapProvider();

	void resetMapPublicObservations();

	void resetMapSoundAndSmoke();

	void resetMapObjectiveKnowledge();

	void deactivateMapProvider();

	void configurePerformance(bool disableVision, bool disableTrace);

	void observeExternalCommand(const edict_t *entity, std::uint16_t buttons);

	void refreshPublicSmoke();

	void observeRadioLeaderPosition(std::size_t index, const compat::ScenarioTarget &position);

	void observeReloadDispatched(world::ActorKey actor, world::FrameIdentity frame, float now);

private:
	struct GameDllClientReceipt
	{
		std::size_t index = 0;
		world::ActorKey actor{};
		world::FrameIdentity frame{};
		const edict_t *entity = nullptr;
		int serial = -1;
		std::uint32_t connection = 0U;
		std::uint64_t lifeRevision = 0U;
	};
	GameDllClientReceipt resolveGameDllClient(const void *player,
		const std::function<const void *(const char *)> &findClientObject, bool useOpaqueIdentity = false) const;
	void fill_managedBotPerceptionFullUpdates(const std::uint32_t &value)
	{
		for (auto &actor : actors_)
			actor.managedBotPerceptionFullUpdates = value;
	}

	void fill_managedBotCurrentWeapons(const ManagedCurrentWeaponObservation &value)
	{
		for (auto &actor : actors_)
			actor.managedBotCurrentWeapons = value;
	}

	void reset_objectiveKnowledge()
	{
		for (auto &actor : actors_)
			actor.objectiveKnowledge = {};
	}

	void reset_deliveredSoundSequences()
	{
		for (auto &actor : actors_)
			actor.deliveredSoundSequences = {};
	}

	void reset_publicBombClocks()
	{
		for (auto &actor : actors_)
			actor.publicBombClocks = {};
	}

	void reset_bombNoticePlanted()
	{
		for (auto &actor : actors_)
			actor.bombNoticePlanted = {};
	}

	void reset_bombNoticeKnown()
	{
		for (auto &actor : actors_)
			actor.bombNoticeKnown = {};
	}

	void reset_publicScoreStatuses()
	{
		for (auto &actor : actors_)
			actor.publicScoreStatuses = {};
	}

	void invalidateRadioInput(std::size_t index)
	{
		auto &actor = actors_[index];
		actor.scenarioRadio = {};
		if (++actor.radioInputRevision == 0)
			++actor.radioInputRevision;
	}
	void reset_scenarioRadio()
	{
		for (std::size_t index = 0; index < actors_.size(); ++index)
			invalidateRadioInput(index);
	}

	struct ActorState
	{
		compat::PublicBombRadar publicBombRadars{};
		compat::ScenarioRadioObservation scenarioRadio{};
		std::uint64_t radioInputRevision = 0;
		std::array<PublicScoreStatus, NativeBotObservation::kClientSlotCount> publicScoreStatuses{};
		bool bombNoticeKnown{};
		bool bombNoticePlanted{};
		compat::PublicBombClock publicBombClocks{};
		std::uint32_t deliveredSoundSequences{};
		std::uint8_t teamInfoTeams{};
		bool publicTeamKnown{};
		std::array<ObjectiveKnowledge, kObjectiveKnowledgeStateCount> objectiveKnowledge{};
		ManagedCurrentWeaponObservation managedBotCurrentWeapons{};
		perception::PerceptionAssembler managedBotPerception{};
		std::uint32_t managedBotPerceptionFullUpdates{};
	};
	std::array<ActorState, NativeBotObservation::kClientSlotCount> actors_{};
	BotObservationServices *services_ = nullptr;
	ObservationAdapter observationAdapter_{};
	compat::PublicObservationStore publicObservations_{};
	const char *publicMessageName_ = nullptr;
	edict_t *publicMessageTarget_ = nullptr;
	int publicMessageDestination_ = 0;
	int publicMessageSerial_ = -1;
	world::ActorKey publicMessageActor_{};
	world::FrameIdentity publicMessageFrame_{};
	std::array<std::uint64_t, NativeBotObservation::kClientSlotCount> publicMessageRadioRevisions_{};
	static constexpr auto kPublicMessageIntegerFieldCount = 12U;
	std::array<int, kPublicMessageIntegerFieldCount> publicMessageValues_{};
	std::array<float, 3U> publicMessageCoords_{};
	std::uint16_t publicMessageCoordMask_ = 0U;
	std::uint16_t publicMessageStringMask_ = 0U;
	std::size_t publicMessageFieldCount_ = 0U;
	bool publicMessageMalformed_ = false;
	static constexpr auto kPublicMessageStringCapacity = 96U;
	static constexpr auto kPublicMessageStringFieldCount = 12U;
	static constexpr auto kMaximumSoundEvents = 64U;
	static constexpr auto kMaximumPublicSmokeEntities = 32U;
	static constexpr auto kUserMessageTextCapacity = 257U;
	std::array<char, kPublicMessageStringCapacity> publicMessageString_{};
	std::array<std::array<char, kPublicMessageStringCapacity>, kPublicMessageStringFieldCount> publicMessageStrings_{};
	std::uint32_t radioSequence_ = 0U;
	std::array<int, kTeamScoreEntryCount> teamScores_{};
	std::array<bool, kTeamScoreEntryCount> teamScoresKnown_{};
	compat::SmokeObservations smokeObservations_{};
	ReGameDllObservationProvider optionalObservations_{};
	std::array<world::AudibleEvent, kMaximumSoundEvents> soundEvents_{};
	std::array<int, kMaximumSoundEvents> soundTeams_{};
	std::array<bool, kMaximumSoundEvents> fallbackWeaponSounds_{};
	struct FlashHearing
	{
		std::uint64_t lifeRevision = 0U;
		std::uint32_t actorGeneration = 0U;
		float distance = 0.0f;
	};
	using FlashAudience = std::array<FlashHearing, NativeBotObservation::kClientSlotCount>;
	std::array<FlashAudience, kMaximumSoundEvents> flashAudiences_{};
	std::uint32_t soundSequence_ = 0U;
	compat::GunshotEventCatalog gunshotEvents_{};
	int playerTeamAtSlot(const edict_t &entity, int slot) const;
	void recordNativeFlashSound(const edict_t &source, world::ActorKey sourceKey,
		const world::FrameIdentity &frame, float now);
	void recordSound(world::AudibleEvent event, const edict_t *source, bool fallbackWeapon = false,
		const FlashAudience *flashAudience = nullptr);
	void observePlantedBombAnnouncement(world::WorldVector origin, world::FrameIdentity frame);
	void rememberPlantedBomb(world::EntityKey bomb, world::WorldVector origin, float now, bool announcement);
	bool hasNativeWeaponEvents(const edict_t *source, int slot) const;
	struct NativeWeaponSource
	{
		int serial = -1;
		std::uint32_t connection = 0;
	};
	std::array<NativeWeaponSource, 32> nativeWeaponSources_{};
	void notifyCloseFootstep(const world::AudibleEvent &event, const edict_t *source);
	std::array<PublicSmoke, kMaximumPublicSmokeEntities> publicSmokeEntities_{};
	std::size_t nextSmokeEntity_ = 0U;
	UserMessageKind userMessageKind_{};
	edict_t *userMessageTarget_{};
	std::uint8_t userMessageFieldCount_{};
	std::uint8_t userMessageWeaponState_{};
	std::uint8_t userMessageWeaponId_{};
	bool userMessageWeaponMessageValid_{};
	std::uint8_t userMessageMenuType_{};
	std::uint8_t userMessageNeedMore_{};
	std::uint16_t userMessageValidSlots_{};
	std::uint8_t userMessageTeamSlot_{};
	bool userMessageShowFragmentActive_{};
	std::array<char, kUserMessageTextCapacity> userMessageText_{};
	std::uint16_t userMessageTextLength_{};
	std::uint32_t externalDefuserEntityIndex_ = 0U;
	std::uint32_t externalDefuserEntitySerial_ = 0U;
	float externalDefuserLeaseUntil_ = 0.0f;
	struct NativeDefuser
	{
		world::FrameIdentity frame{};
		runtime::ActorId connection{};
		int serial = -1;
	};
	NativeDefuser nativeDefuser_{};
	std::uint32_t externalDefuserMapGeneration_ = 0U;
	std::uint32_t externalDefuserRoundGeneration_ = 0U;
};
} // namespace metamod
} // namespace astrabot
#endif
