#ifndef ASTRABOT_ADAPTER_BOT_BEHAVIOR_RUNTIME_HPP
#define ASTRABOT_ADAPTER_BOT_BEHAVIOR_RUNTIME_HPP
#include "runtime/shared/bot_runtime_services.hpp"
#include "astrabot/compat/enemy_count_chatter.hpp"
#include "astrabot/compat/kill_praise.hpp"
#include "astrabot/compat/friend_death_chatter.hpp"
#include "astrabot/compat/knife_run.hpp"
#include "astrabot/compat/friend_death.hpp"
#include "astrabot/compat/noise_investigation.hpp"
#include "astrabot/compat/guard_look.hpp"
#include "astrabot/compat/weapon_equipment.hpp"
#include "astrabot/compat/chatter_voice.hpp"
#include "astrabot/compat/noise_look.hpp"
#include <limits>
#include <memory>
#include "astrabot/compat/chatter_place_history.hpp"
#include "astrabot/compat/chatter_enemy_place.hpp"
namespace astrabot
{
namespace metamod
{
class BotBehaviorRuntime
{
public:
	bool noiseInvestigationCurrent(std::size_t index) const
	{
		if (index >= actors_.size() || !actors_[index].noiseInvestigation.active() || !services_)
			return false;
		const auto &handle = services_->service_managedBotHandles()[index];
		const auto globals = services_->service_globals();
		if (!handle.entity || handle.entity->free || handle.entity->v.deadflag != DEAD_NO ||
			handle.entity->v.health <= 0 || !globals)
			return false;
		const auto nav = services_->service_navPublisher().snapshot();
		const auto &lifecycle = services_->service_lifecycle();
		return nav.isValid() &&
			actors_[index].noiseInvestigation.current({handle.actor.slot, handle.actor.actorGeneration},
				{lifecycle.mapGeneration(), lifecycle.roundGeneration(), services_->service_adapterFrameCount()},
				globals->time, nav.revision());
	}
	bool noiseInvestigationWalking(std::size_t index) const
	{
		if (index < actors_.size() && reactionPursuing(index) && !actors_[index].managedBotStateMachines.attackOverlayActive())
		{
			const auto &weapon = services_->observation_managedBotCurrentWeapons(index);
			const auto context = reactionContext(index);
			const float roundStart = services_->service_roundFreezeGeneration() == context.frame.roundGeneration
				? services_->service_roundFreezeUntil() : 0.0f;
			constexpr unsigned kKnife = 29;
			return weapon.available && weapon.active && weapon.weaponId == kKnife && !reactionHurrying(index) &&
				actors_[index].enemyReaction.wellPastSafe(context.now - roundStart,
					static_cast<float>(actors_[index].managedProfiles.aggression) / 100.0f);
		}
		return actors_[index].investigationWalking &&
			actors_[index].managedBotStateMachines.state() == compat::CompatibilityStateId::InvestigateNoise &&
			!actors_[index].managedBotStateMachines.attackOverlayActive() && noiseInvestigationCurrent(index);
	}
	void updateVoiceFeedback(bool force = false);
	void observeRadioPreference(edict_t *recipient, bool ignored);
	void observeObstacleKeyValue(edict_t *entity, const KeyValueData *data);
	void observeObstacleTouch(edict_t *entity, edict_t *other);
	void observeWeaponTouch(edict_t *box, edict_t *self);
	void applyNoiseLookCommand(std::size_t index, runtime::BotCommand &command);
	void applyObstacleCommand(std::size_t index, runtime::BotCommand &command);
	void resetObstacleMaterials();
	void resetRoundRecognition();
	void observePublicDeath(std::size_t index, const world::FrameIdentity &deathFrame = {});
	std::uint64_t chatterRevision(std::size_t index) const
	{
		return actors_[index].confirmedSpeech.revision();
	}
	void observeEnemyWeaponFire(const edict_t *source, const world::FrameIdentity &frame, float now);
	bool forcedRunning(std::size_t index);
	world::EntityKey currentEnemySource(std::size_t index) const;
	void observePlayerDeath(int victimSlot, int killerSlot, edict_t *recipient, const world::FrameIdentity &frame);
	void bindServices(BotBehaviorServices &services)
	{
		services_ = &services;
	}

	bool buildManagedObjectiveTarget(std::size_t index, nav::NavVector *target, nav::NavSearchStats *searchStats,
		RuntimeObjectiveSelectionStats *objectiveStats = nullptr);
	ActionProposal decideManagedBotAction(std::size_t index, const runtime::MovementPhysicsState &before,
		const runtime::ViewAngles &movementAngles, std::uint16_t movementButtons);
	const team::TeamObjectiveAssignment *findManagedTeamObjectiveAssignment(const world::ActorKey &actor) const;
	void finishManagedBotNavigation(std::size_t index, const runtime::MovementPhysicsState &before,
		runtime::NavRoamResult result, const runtime::NavRoamDecision &decision, nav::LocomotionIntent *intent);
	compat::StateUpdateContext makeManagedBotStateContext(std::size_t index,
		const runtime::MovementPhysicsState &before, compat::StateBehaviorInput *behavior = nullptr) const;
	bool managedActorAtBombSite(std::size_t index) const;
	float managedRoundRemainingSeconds() const;
	void refreshManagedObjectiveSiteRegistry();
	void refreshManagedTeamObjectiveAssignments(std::size_t contextIndex);
	void updateBombGuardTarget(
		std::size_t index, const compat::ScenarioBehaviorInput &input, compat::ScenarioBehaviorDecision *decision);
	void updateManagedBotCompatibilityState(std::size_t index, const runtime::MovementPhysicsState &before);
	void updateOutgoingPublicRadio(std::size_t index);
	compat::ChatterScheduleAdmission chatterAdmission(std::size_t index);
	compat::ChatterScheduleEntry activeTeamChatter(std::size_t index, int team, bool *current);
	void updatePublicBuying(std::size_t index);
	CombatUpkeepUpdate upkeepCombat(
		std::size_t index, const runtime::MovementPhysicsState &before, std::uint16_t cachedButtons);
	void clearCombatUpkeep(std::size_t index)
	{
		if (index >= actors_.size())
			return;
		actors_[index].managedBotCombat.clearAim();
		actors_[index].weaponActions.stopRapidFire();
	}

	bool combatMovementActive(std::size_t index) const
	{
		return index < actors_.size() && actors_[index].managedBotCombat.movementActive();
	}

	const compat::ProfileRecord &managedProfiles(std::size_t index) const
	{
		return actors_[index].managedProfiles;
	}

	const ManagedObjectiveTargetCache &managedBotObjectiveTargets(std::size_t index) const
	{
		return actors_[index].managedBotObjectiveTargets;
	}

	const ManagedObjectiveActionState &managedBotObjectiveActions(std::size_t index) const
	{
		return actors_[index].managedBotObjectiveActions;
	}

	const compat::CompatibilityStateMachine &managedBotStateMachines(std::size_t index) const
	{
		return actors_[index].managedBotStateMachines;
	}

	const compat::ScenarioBehaviorDecision &scenarioDecisions(std::size_t index) const
	{
		return actors_[index].scenarioDecisions;
	}

	void onActorRemoved(std::size_t index);

	void onActorRemembered(std::size_t index, const FakeClientHandle &handle);

	void resetSharedExecutionState();

	void resetActorExecutionState(std::size_t index);

	void noteNavigationFailure(const FakeClientHandle &handle, runtime::NavRoamResult roamResult,
		const runtime::NavRoamDecision &roamDecision);

	void resetRoundBuyingState();

	void beginRoundPlantDecision();

	void resetRoundTactics();

	void resetRoundNoiseInvestigation();

	void resetRoundObjectiveCoordination();

	void beginMapPlantDecision();

	void invalidateObjectiveAssignments();

	void assignProfile(std::size_t index, const compat::ProfileRecord &profile);


	void observeRecognizedTargetChange(std::size_t index, float now);

	void resetBombApproach(std::size_t index);

	void observeC4SelectionDispatched(std::size_t index, const FakeClientHandle &handle);
	void observeEquipmentDispatched(std::size_t index, std::uint16_t weapon, bool success);

	void observeCloseFootstep(std::size_t index, const compat::FootstepEquipmentReceipt &receipt);
	void resetEquipment(std::size_t index)
	{
		if (index < actors_.size())
		{
			actors_[index].equipment.reset();
			actors_[index].closeFootstep = {};
		}
	}

	void observeC4AttackDispatched(std::size_t index, const FakeClientHandle &handle, std::uint16_t buttons);

	void observeNeutralC4AttackDispatched(std::size_t index, const FakeClientHandle &handle);

private:
	bool reactionEnemyCurrent(world::ActorKey enemy) const;
	bool receiptConnectionCurrent(world::ActorKey connection) const;
	enum class BehaviorPhaseResult
	{
		Continue,
		Complete
	};
	struct ManagedActionContext
	{
		std::size_t index;
		const runtime::MovementPhysicsState &before;
		const runtime::ViewAngles &movementAngles;
		ActionProposal proposal{};
		const FakeClientHandle *handle = nullptr;
		world::FrameIdentity frame{};
		world::ActorKey actor{};
		compat::CompatibilityObservation observation{};
		compat::ObservationTimingContext observationTiming{};
		world::WorldSnapshot snapshot{};
		world::ActorKey targetActor{};
		world::WorldPosition targetPosition{};
		int team = 0;
		float closeFootstepDistance = -1;
	};
	struct ScenarioUpdateData
	{
		const compat::ObservationTimingContext &timing;
		int observedTeam = 0;
		float automaticLeaderRange = (std::numeric_limits<float>::max)();
		nav::NavSnapshot navigation{};
		bool wasFollowing = false;
	};
	bool prepareManagedPerception(std::size_t index, world::WorldSnapshot *snapshot, world::ActorKey *targetActor,
		world::WorldPosition *targetPosition);
	BehaviorPhaseResult prepareManagedAction(ManagedActionContext &ctx);
	BehaviorPhaseResult decideObjectiveAction(ManagedActionContext &ctx);
	ActionProposal decideCombatAction(ManagedActionContext &ctx);
	void updateGuardLook(ManagedActionContext &ctx);
	bool decideEquipmentAction(ManagedActionContext &ctx, const compat::PublicActorState &weapon, bool attacking);
	bool cancelHeldGrenade(std::size_t index, const runtime::ViewAngles &movementAngles, ActionProposal &proposal);
	BehaviorPhaseResult updateBehaviorLifecycle(
		std::size_t index, const runtime::MovementPhysicsState &before, compat::StateUpdateContext &context);
	bool shouldBuy(std::size_t index, const compat::StateUpdateContext &context) const;
	void updateNoiseInvestigation(std::size_t index, const runtime::MovementPhysicsState &before,
		compat::StateUpdateContext &context, compat::StateBehaviorInput &stateBehavior);
	void updateBehaviorPriorities(std::size_t index, const runtime::MovementPhysicsState &before,
		compat::StateUpdateContext &context, compat::StateBehaviorInput &stateBehavior);
	void commitBehaviorState(
		std::size_t index, compat::StateUpdateContext &context, compat::StateBehaviorInput &stateBehavior);
	void updateScenarioState(std::size_t index, const runtime::MovementPhysicsState &before,
		compat::StateUpdateContext &context, compat::StateBehaviorInput &stateBehavior);
	void populateScenarioPublicState(std::size_t index, const runtime::MovementPhysicsState &before,
		compat::StateUpdateContext &context, compat::StateBehaviorInput &stateBehavior,
		const compat::CompatibilityObservation &observation, compat::ScenarioBehaviorInput &scenario,
		ScenarioUpdateData &data);
	void observeScenarioEntities(std::size_t index, const runtime::MovementPhysicsState &before,
		compat::StateUpdateContext &context, compat::StateBehaviorInput &stateBehavior,
		const compat::CompatibilityObservation &observation, compat::ScenarioBehaviorInput &scenario,
		ScenarioUpdateData &data);
	void updateScenarioNavigation(std::size_t index, const runtime::MovementPhysicsState &before,
		compat::StateUpdateContext &context, compat::StateBehaviorInput &stateBehavior,
		const compat::CompatibilityObservation &observation, compat::ScenarioBehaviorInput &scenario,
		ScenarioUpdateData &data);
	void updateScenarioTeamwork(std::size_t index, const runtime::MovementPhysicsState &before,
		compat::StateUpdateContext &context, compat::StateBehaviorInput &stateBehavior,
		const compat::CompatibilityObservation &observation, compat::ScenarioBehaviorInput &scenario,
		ScenarioUpdateData &data);
	void applyScenarioDecision(std::size_t index, const runtime::MovementPhysicsState &before,
		compat::StateUpdateContext &context, compat::StateBehaviorInput &stateBehavior,
		const compat::CompatibilityObservation &observation, compat::ScenarioBehaviorInput &scenario,
		ScenarioUpdateData &data);
	void fill_managedBotWasDead(const bool &value)
	{
		for (auto &actor : actors_)
			actor.managedBotWasDead = value;
	}

	void fill_managedBotStateRounds(const runtime::LifecycleGeneration &value)
	{
		for (auto &actor : actors_)
			actor.managedBotStateRounds = value;
	}

	void reset_managedBotObjectiveActions()
	{
		for (auto &actor : actors_)
			actor.managedBotObjectiveActions = {};
	}

	void fill_managedBotObjectiveActions(const ManagedObjectiveActionState &value)
	{
		for (auto &actor : actors_)
			actor.managedBotObjectiveActions = value;
	}

	void reset_managedStateInputs()
	{
		for (auto &actor : actors_)
			actor.managedStateInputs = {};
	}

	void reset_noiseInvestigation()
	{
		for (auto &actor : actors_)
		{
			actor.noiseInvestigation.reset();
			actor.noiseReportPolicy = {};
			actor.noiseReport = {};
		}
	}

	void reset_buyReceiptTimeoutLogged()
	{
		for (auto &actor : actors_)
			actor.buyReceiptTimeoutLogged = {};
	}

	void reset_escapingWithKnife()
	{
		for (auto &actor : actors_)
			actor.escapingWithKnife = {};
	}

	void reset_buyingComplete()
	{
		for (auto &actor : actors_)
			actor.buyingComplete = {};
	}

	void reset_buyControllers()
	{
		for (auto &actor : actors_)
			actor.buyControllers = {};
	}

	void reset_teamworkRadioApplied()
	{
		for (auto &actor : actors_)
			actor.teamworkRadioApplied = {};
	}

	void reset_teamworkPolicies()
	{
		for (auto &actor : actors_)
			actor.teamworkPolicies = {};
	}

	void reset_bombSearchTargets()
	{
		for (auto &actor : actors_)
			actor.bombSearchTargets = {};
	}

	void reset_bombSearchMemory()
	{
		for (auto &actor : actors_)
			actor.bombSearchMemory = {};
	}

	void reset_scenarioEscapeAreas()
	{
		for (auto &actor : actors_)
			actor.scenarioEscapeAreas = {};
	}

	struct ObstacleLook
	{
		edict_t *entity = nullptr;
		int serial = 0;
		runtime::LifecycleToken lifecycle{};
		nav::NavVector point{};
		compat::AimSpringState spring{};
		runtime::ViewAngles angles{};
		float touchedAt = 0.0f;
		float alignedAt = -1.0f;
		std::uint32_t attackUpdate = 0;
		bool pendingAttack = false;
	};

	struct BreakableMaterial
	{
		const edict_t *entity = nullptr;
		int serial = 0;
		int material = -1;
	};

	struct ChatterBank
	{
		std::string name;
		compat::ChatterVoiceCatalog catalog;
		runtime::LifecycleGeneration shuffledRound = 0U;
		bool filesChecked = false;
		bool loading = false, checking = false, shuffling = false;
	};
	struct RadioPreference
	{
		int serial = 0;
		runtime::LifecycleGeneration map = 0U;
		bool ignored = false;
	};
	struct VoiceFeedback
	{
		int serial = 0;
		float until = 0.0f;
	};
	std::shared_ptr<compat::ChatterVoiceCatalog> chatterBank(std::size_t index);
	void ensureOutgoingChatterCatalog();
	std::uint32_t allowedFixedChatterPhrases(const char *verbosity, compat::ChatterPhrase first, compat::ChatterPhrase last);
	bool killVictimVisible(std::size_t index, const edict_t *victim, const world::FrameIdentity &frame, bool checkFov = true) const;
	compat::ReactionContext reactionContext(std::size_t index) const;
	void observeFriendDeath(std::size_t index, const edict_t *victim, const edict_t *killer,
		const world::FrameIdentity &frame, world::ActorKey victimConnection, world::ActorKey killerConnection);
	void changeReactionState(std::size_t index, compat::CompatibilityStateId next, bool pursue = false);
	void updateEnemyReaction(std::size_t index, const runtime::MovementPhysicsState &before);
	bool reactionPursuing(std::size_t index) const;
	bool reactionIgnoring(std::size_t index) const;
	compat::ReactionDisposition reactionDisposition(std::size_t index) const;
	bool reactionHurrying(std::size_t index) const;
	void stopVoiceFeedback(std::size_t index);
	enum class ChatterVoicePurpose
	{
		Statement,
		DeathInterrupt
	};
	void finishDeathChatter(std::size_t index, const compat::ConfirmedChatterSpeech &previous,
		const world::FrameIdentity &deathFrame);
	bool sendChatterVoice(std::size_t index, compat::ChatterPhrase phrase, int count,
		std::shared_ptr<compat::ChatterVoiceCatalog> bank, float *duration, std::uint32_t criteriaPlace,
		ChatterVoicePurpose purpose = ChatterVoicePurpose::Statement, std::uint32_t placePhrase = 0,
		bool *selected = nullptr, bool *suppressed = nullptr);
	struct NoiseBendCache
	{
		bool valid = false;
		bool hasPoint = false;
		float noiseTime = -1.0f;
		std::uint64_t navRevision = 0U;
		world::WorldPosition origin{};
		world::WorldPosition noiseOrigin{};
		world::WorldPosition point{};
	};

	struct ActorState
	{
		ObstacleLook obstacleLook{};
		float obstacleNextEquipAt = 0.0f;
		bool obstaclePathActive = false;
		nav::NavVector obstaclePathGoal{};
		compat::ReactionQueue reactionQueues{};
		world::ActorKey recognizedTargets{};
		world::WorldPosition recognizedPoints{};
		std::uint32_t recognitionSequences{};
		float lastVisibleEnemyTimes{};
		world::WorldPosition lastVisibleEnemyPositions{};
		compat::NoiseLookPlanner noiseLook{};
		NoiseBendCache noiseBend{};
		std::uint32_t noiseLookUpdate = 0U;
		compat::BombGuardPlanner bombGuardPlanners{};
		compat::GuardLookPlanner guardLook{};
		compat::WeaponEquipmentPlanner equipment{};
		compat::FootstepEquipmentReceipt closeFootstep{};
		VoiceFeedback outgoingVoiceFeedback{};
		compat::OutgoingRadioScheduler outgoingRadioSchedulers{};
		compat::ConfirmedChatterSpeech confirmedSpeech{};
		compat::ChatterAdmissionClock chatterAdmissions{};
		compat::KnifeRunTimer knifeRun{};
		compat::EnemyReactionMemory enemyReaction{};
		world::EntityKey enemyWeaponSource{};
		compat::RadioContactMemory outgoingRadioContactMemory{};
		nav::AreaId scenarioEscapeAreas{};
		compat::BombSiteSearchMemory bombSearchMemory{};
		compat::ScenarioTarget bombSearchTargets{};
		compat::TeamworkPolicy teamworkPolicies{};
		std::uint32_t teamworkRadioApplied{};
		compat::ProfileRecord managedProfiles{};
		float lastCombatTimes{};
		float attackReadyTimes{};
		compat::BuyController buyControllers{};
		bool buyingComplete{};
		bool escapingWithKnife{};
		bool buyReceiptTimeoutLogged{};
		compat::WeaponActionPlanner weaponActions{};
		compat::NoiseInvestigationPlanner noiseInvestigation{};
		compat::NoiseReportPolicy noiseReportPolicy{};
		compat::HeardNoiseReport noiseReport{};
		bool investigationWalking = false;
		compat::StateBehaviorInput managedStateInputs{};
		combat::CombatController managedBotCombat{};
		objectives::RoundObjectivePlanner managedBotObjectives{};
		ManagedObjectiveTargetCache managedBotObjectiveTargets{};
		objectives::BombApproachState managedBombApproaches{};
		team::RoundObjectiveCoordinator compatibilityObjectiveCoordinators{};
		ManagedObjectiveActionState managedBotObjectiveActions{};
		compat::CompatibilityStateMachine managedBotStateMachines{};
		compat::ScenarioBehaviorPlanner scenarioPlanners{};
		compat::ScenarioBehaviorDecision scenarioDecisions{};
		runtime::LifecycleGeneration managedBotStateRounds{};
		bool managedBotWasDead{};
	};
	std::array<ActorState, NativeBotObservation::kClientSlotCount> actors_{};
	std::array<BreakableMaterial, 2048U> breakableMaterials_{};
	BotBehaviorServices *services_ = nullptr;
	compat::ChatterRadioCatalog outgoingChatterCatalog_{};
	std::vector<std::shared_ptr<ChatterBank>> outgoingVoiceBanks_{};
	compat::ChatterPlaceHistory outgoingPlaceHistory_{};
	std::uint64_t outgoingCatalogRevision_ = 0;
	std::array<RadioPreference, NativeBotObservation::kClientSlotCount> outgoingRadioPreferences_{};
	std::uint32_t outgoingVoiceMask_ = 0U;
	static constexpr unsigned kTeamRadioSlotCount = 3U;
	std::array<float, kTeamRadioSlotCount> outgoingTeamLastSpeech_{{-1.0f, -1.0f, -1.0f}};
	std::array<float, kTeamRadioSlotCount> outgoingTeamBusyUntil_{};
	std::array<float, kTeamRadioSlotCount> outgoingTeamFollowRadio_{};
	std::array<compat::ChatterPhrase, kTeamRadioSlotCount> outgoingTeamCurrentPhrase_{};
	std::array<std::uint16_t, kTeamRadioSlotCount> outgoingTeamCurrentPlace_{};
	std::array<bool, static_cast<unsigned>(compat::ChatterPhrase::Count)> outgoingMissingPhraseLogged_{};

	runtime::LifecycleGeneration outgoingRadioMap_ = runtime::LifecycleSession::kInvalidGeneration;
	runtime::LifecycleGeneration outgoingRadioRound_ = runtime::LifecycleSession::kInvalidGeneration;
	objectives::RoundPlantDecision roundPlantDecision_{};
	team::RoundObjectiveCoordinator managedTeamObjectiveCoordinator_{};
	std::size_t managedTeamObjectiveAssignmentContext_ = NativeBotObservation::kClientSlotCount;
	team::TeamObjectiveAssignmentSet managedTeamObjectiveAssignments_{};
	std::uint32_t managedTeamObjectiveAssignmentMapGeneration_ = 0U;
	std::uint32_t managedTeamObjectiveAssignmentRoundGeneration_ = 0U;
	std::uint32_t managedTeamObjectiveAssignmentFrame_ = 0U;
	bool managedTeamObjectiveAssignmentsValid_ = false;
	bool managedTeamObjectivePathFailurePending_ = false;
	world::ActorKey managedTeamObjectivePathFailureActor_{};
	std::uint32_t managedTeamObjectivePathFailureGeneration_ = 0U;
	nav::AreaId managedTeamObjectivePathFailureStartArea_ = 0U;
	std::uint32_t managedTeamObjectivePathFailureFrame_ = 0U;
	ManagedObjectiveSiteRegistry managedObjectiveSiteRegistry_{};
};
} // namespace metamod
} // namespace astrabot
#endif
