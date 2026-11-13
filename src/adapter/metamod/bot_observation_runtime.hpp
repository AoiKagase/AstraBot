#ifndef ASTRABOT_ADAPTER_BOT_OBSERVATION_RUNTIME_HPP
#define ASTRABOT_ADAPTER_BOT_OBSERVATION_RUNTIME_HPP
#include "bot_runtime_services.hpp"
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
	bool collectManagedPerception(std::size_t index, world::WorldSnapshot *snapshot,
		ThreatObservation *threats);
	bool readManagedWorldSnapshot(std::size_t index, world::WorldSnapshot *snapshot) const;
	void captureManagedCurrentWeapon();
	runtime::MovementPhysicsState captureMovementPhysicsState(const edict_t *entity,
															  bool teamConfirmed,
															  bool managedFakeClient) const;
	void capturePublicMessage();
	bool hasObservedActiveWeapon(std::size_t index) const;
	std::vector<nav::NavVector> liveFriendlyPositions(std::size_t index, bool feet) const;
	void notifyTeamInfo(std::uint8_t slot, const char *teamName);
	bool observeObjective(std::size_t index, edict_t *entity, bool planted,
						  world::WorldVector *position, bool *currentlyVisible = nullptr);
	void onEmitSound(edict_t *entity, const char *sample, float volume, float attenuation);
	void onMessageBegin(int messageDestination, int messageType, const float *origin,
						edict_t *entity);
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
	using View_teamScores = std::array<int, 3U>;
	const View_teamScores &teamScores() const
	{
		return teamScores_;
	}
	using View_teamScoresKnown = std::array<bool, 3U>;
	const View_teamScoresKnown &teamScoresKnown() const
	{
		return teamScoresKnown_;
	}
	const std::array<PublicScoreStatus, NativeBotObservation::kClientSlotCount>
		&publicScoreStatuses(std::size_t index) const
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
	const std::array<ObjectiveKnowledge, 2U> &objectiveKnowledge(std::size_t index) const
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

	void onMovementResetShared();

	void onMovementResetActor(std::size_t index);

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
	void reset_scenarioRadio()
	{
		for (auto &actor : actors_)
			actor.scenarioRadio = {};
	}
	struct ActorState
	{
		compat::PublicBombRadar publicBombRadars{};
		compat::ScenarioRadioObservation scenarioRadio{};
		std::array<PublicScoreStatus, NativeBotObservation::kClientSlotCount> publicScoreStatuses{};
		bool bombNoticeKnown{};
		bool bombNoticePlanted{};
		compat::PublicBombClock publicBombClocks{};
		std::uint32_t deliveredSoundSequences{};
		std::uint8_t teamInfoTeams{};
		bool publicTeamKnown{};
		std::array<ObjectiveKnowledge, 2U> objectiveKnowledge{};
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
	world::ActorKey publicMessageActor_{};
	world::FrameIdentity publicMessageFrame_{};
	std::array<int, 12U> publicMessageValues_{};
	std::array<float, 3U> publicMessageCoords_{};
	std::uint16_t publicMessageCoordMask_ = 0U;
	std::uint16_t publicMessageStringMask_ = 0U;
	std::size_t publicMessageFieldCount_ = 0U;
	bool publicMessageMalformed_ = false;
	std::array<char, 96U> publicMessageString_{};
	std::array<std::array<char, 96U>, 12U> publicMessageStrings_{};
	std::uint32_t radioSequence_ = 0U;
	std::array<int, 3U> teamScores_{};
	std::array<bool, 3U> teamScoresKnown_{};
	compat::SmokeObservations smokeObservations_{};
	ReGameDllObservationProvider optionalObservations_{};
	std::array<world::AudibleEvent, 64U> soundEvents_{};
	std::array<int, 64U> soundTeams_{};
	std::uint32_t soundSequence_ = 0U;
	std::array<PublicSmoke, 32U> publicSmokeEntities_{};
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
	std::array<char, 257U> userMessageText_{};
	std::uint16_t userMessageTextLength_{};
	std::uint32_t externalDefuserEntityIndex_ = 0U;
	std::uint32_t externalDefuserEntitySerial_ = 0U;
	float externalDefuserLeaseUntil_ = 0.0f;
	std::uint32_t externalDefuserMapGeneration_ = 0U;
	std::uint32_t externalDefuserRoundGeneration_ = 0U;
};
} // namespace metamod
} // namespace astrabot
#endif
