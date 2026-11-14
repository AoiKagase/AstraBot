#ifndef ASTRABOT_ADAPTER_BOT_BEHAVIOR_RUNTIME_HPP
#define ASTRABOT_ADAPTER_BOT_BEHAVIOR_RUNTIME_HPP
#include "runtime/shared/bot_runtime_services.hpp"
#include "astrabot/compat/guard_look.hpp"
#include "astrabot/compat/weapon_equipment.hpp"
#include "astrabot/compat/chatter_voice.hpp"
#include <limits>
namespace astrabot
{
namespace metamod
{
class BotBehaviorRuntime
{
public:
	void updateVoiceFeedback(bool force = false);
	void observeRadioPreference(edict_t *recipient, bool ignored);
	void observeObstacleKeyValue(edict_t *entity, const KeyValueData *data);
	void observeObstacleTouch(edict_t *entity, edict_t *other);
	void observeWeaponTouch(edict_t *box, edict_t *self);
	void applyObstacleCommand(std::size_t index, runtime::BotCommand &command);
	void resetObstacleMaterials();
	void resetRoundRecognition();
	void observePublicDeath(std::size_t index);
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

	void observeRadioReceipt(std::size_t index);

	void observeRecognizedTargetChange(std::size_t index, float now);

	void resetBombApproach(std::size_t index);

	void observeC4SelectionDispatched(std::size_t index, const FakeClientHandle &handle);
	void observeEquipmentDispatched(std::size_t index, std::uint16_t weapon, bool success);
	void resetEquipment(std::size_t index)
	{
		if (index < actors_.size())
			actors_[index].equipment.reset();
	}

	void observeC4AttackDispatched(std::size_t index, const FakeClientHandle &handle, std::uint16_t buttons);

	void observeNeutralC4AttackDispatched(std::size_t index, const FakeClientHandle &handle);

private:
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

	void reset_investigatedNoiseTicks()
	{
		for (auto &actor : actors_)
			actor.investigatedNoiseTicks = {};
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

	void fill_outgoingRadioAttemptChoices(const compat::RadioMenuChoice &value)
	{
		for (auto &actor : actors_)
			actor.outgoingRadioAttemptChoices = value;
	}

	void fill_outgoingRadioAttemptTimes(const float &value)
	{
		for (auto &actor : actors_)
			actor.outgoingRadioAttemptTimes = value;
	}

	void fill_outgoingRadioAttempts(const unsigned &value)
	{
		for (auto &actor : actors_)
			actor.outgoingRadioAttempts = value;
	}

	void fill_outgoingRadioSchedulers(const compat::OutgoingRadioScheduler &value)
	{
		for (auto &actor : actors_)
			actor.outgoingRadioSchedulers = value;
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
	compat::ChatterVoiceCatalog *chatterBank(std::size_t index);
	void stopVoiceFeedback(std::size_t index);
	bool sendChatterVoice(std::size_t index, compat::ChatterPhrase phrase, int count,
		compat::ChatterVoiceCatalog &bank, float *duration);
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
		compat::BombGuardPlanner bombGuardPlanners{};
		compat::GuardLookPlanner guardLook{};
		compat::WeaponEquipmentPlanner equipment{};
		VoiceFeedback outgoingVoiceFeedback{};
		compat::OutgoingRadioScheduler outgoingRadioSchedulers{};
		compat::RadioContactMemory outgoingRadioContactMemory{};
		std::uint32_t outgoingRadioReceipts{};
		std::uint32_t outgoingRadioAttemptGenerations{};
		unsigned outgoingRadioAttempts{};
		float outgoingRadioAttemptTimes{};
		compat::RadioMenuChoice outgoingRadioAttemptChoices{};
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
		std::uint32_t investigatedNoiseTicks{};
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
	std::vector<ChatterBank> outgoingVoiceBanks_{};
	std::array<RadioPreference, NativeBotObservation::kClientSlotCount> outgoingRadioPreferences_{};
	std::uint32_t outgoingVoiceMask_ = 0U;
	static constexpr unsigned kTeamRadioSlotCount = 3U;
	std::array<float, kTeamRadioSlotCount> outgoingTeamLastSpeech_{{-1.0f, -1.0f, -1.0f}};
	std::array<float, kTeamRadioSlotCount> outgoingTeamBusyUntil_{};
	std::array<compat::ChatterPhrase, kTeamRadioSlotCount> outgoingTeamCurrentPhrase_{};
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
