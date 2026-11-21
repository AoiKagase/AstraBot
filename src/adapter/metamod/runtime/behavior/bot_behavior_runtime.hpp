#ifndef ASTRABOT_ADAPTER_BOT_BEHAVIOR_RUNTIME_HPP
#define ASTRABOT_ADAPTER_BOT_BEHAVIOR_RUNTIME_HPP
#include "runtime/shared/bot_runtime_services.hpp"
#include "astrabot/compat/bomb_status.hpp"
#include "astrabot/compat/bot_morale.hpp"
#include "astrabot/compat/enemy_count_chatter.hpp"
#include "astrabot/compat/kill_praise.hpp"
#include "astrabot/compat/friend_death_chatter.hpp"
#include "astrabot/compat/knife_run.hpp"
#include "astrabot/compat/help_request.hpp"
#include "astrabot/compat/friend_death.hpp"
#include "astrabot/compat/noise_investigation.hpp"
#include "astrabot/compat/guard_look.hpp"
#include "astrabot/compat/hunt_encounter.hpp"
#include "astrabot/compat/weapon_equipment.hpp"
#include "astrabot/compat/chatter_voice.hpp"
#include "astrabot/compat/noise_look.hpp"
#include <functional>
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
	bool followWalking(std::size_t index) const;
	nav::AreaId selectFollowGoal(std::size_t index);
	bool takeNoisePathRequest(std::size_t index, bool &targetCleared)
	{
		auto &actor = actors_[index];
		const bool requested = actor.noisePathPending && noiseInvestigationCurrent(index);
		targetCleared = requested && actor.managedStateInputs.noiseTargetCleared;
		actor.noisePathPending = false;
		return requested;
	}
	void updateVoiceFeedback(bool force = false);
	void observeRadioPreference(edict_t *recipient, bool ignored);
	void observeRoundWin(int team, world::FrameIdentity frame, bool rescuedHostages,
		const LateDefuseRecipients &lateDefuse);
	void reportRoundVictory(std::size_t index, int team, bool barelyDefused, const std::function<bool()> &current);
	void resetMatchMorale();
	void observeObstacleKeyValue(edict_t *entity, const KeyValueData *data);
	void observeObstacleTouch(edict_t *entity, edict_t *other);
	void observeWeaponTouch(edict_t *box, edict_t *self);
	void applyNativeLadderLookCommand(std::size_t index, runtime::BotCommand &command, bool localMovement);
	void applyNoiseLookCommand(std::size_t index, runtime::BotCommand &command, bool upkeepOnly);
	bool refreshHuntApproaches(std::size_t index, const runtime::MovementPhysicsState &before,
		const nav::NavSnapshot &navigation);
	bool updateEncounterPeripheralVision(std::size_t index, const runtime::MovementPhysicsState &before,
		const nav::NavSnapshot &navigation, nav::NavHuntEncounter path, const std::function<bool()> &routeCurrent);
	bool updateHuntApproachLook(std::size_t index, const runtime::MovementPhysicsState &before,
		const nav::NavSnapshot &navigation, nav::NavHuntEncounter path, bool lookingAtSpot,
		const std::function<bool()> &routeCurrent, HuntApproachLook &look);
	void inhibitHuntApproachLook(std::size_t index, HuntApproachLook &look);
	void applyObstacleCommand(std::size_t index, runtime::BotCommand &command);
	void resetObstacleMaterials();
	void resetRoundRecognition(bool endingRound);
	void finishRoundSpeech();
	void resetEncouragement() { encouragementUntil_ = -1.0f; }
	void discardEndedRoundSpeech(std::size_t index);
	void observePublicDeath(std::size_t index, const world::FrameIdentity &deathFrame = {});
	std::uint64_t chatterRevision(std::size_t index) const
	{
		return actors_[index].confirmedSpeech.revision();
	}
	void observeEnemyWeaponFire(const edict_t *source, const world::FrameIdentity &frame, float now);
	void observePlayerBlind(std::size_t index, world::ActorKey actor, world::FrameIdentity frame);
	BlindCommand blindCommand(std::size_t index, bool retainUntilFullUpdate = false) const;
	void observePlayerDamage(std::size_t index, world::ActorKey actor, world::FrameIdentity frame,
		const edict_t *source, int serial, world::ActorKey sourceConnection);
	bool forcedRunning(std::size_t index);
	world::EntityKey currentEnemySource(std::size_t index) const;
	bool bombObjectiveEnemyAim(std::size_t index, const compat::AimSpringState &spring, world::WorldPosition *point);
	void recordLookGoal(std::size_t index, float yaw);
	void observePlayerDeath(int victimSlot, int killerSlot, edict_t *recipient, const world::FrameIdentity &frame);
	void bindServices(BotBehaviorServices &services)
	{
		services_ = &services;
	}

	bool buildManagedObjectiveTarget(std::size_t index, nav::NavVector *target, nav::NavSearchStats *searchStats,
		RuntimeObjectiveSelectionStats *objectiveStats = nullptr, nav::AreaId *noiseArea = nullptr);
	ActionProposal decideManagedBotAction(std::size_t index, const runtime::MovementPhysicsState &before,
		const runtime::ViewAngles &movementAngles, std::uint16_t movementButtons);
	const team::TeamObjectiveAssignment *findManagedTeamObjectiveAssignment(const world::ActorKey &actor) const;
	bool finishManagedBotNavigation(std::size_t index, const runtime::MovementPhysicsState &before,
		runtime::NavRoamResult result, const runtime::NavRoamDecision &decision, nav::LocomotionIntent *intent,
		const std::function<bool()> &lookAfterFetchFailure);
	compat::StateUpdateContext makeManagedBotStateContext(std::size_t index,
		const runtime::MovementPhysicsState &before, compat::StateBehaviorInput *behavior = nullptr) const;
	bool managedActorAtBombSite(std::size_t index) const;
	float managedRoundRemainingSeconds() const;
	void refreshManagedObjectiveSiteRegistry();
	void refreshManagedTeamObjectiveAssignments(std::size_t contextIndex);
	void updateBombGuardTarget(
		std::size_t index, const compat::ScenarioBehaviorInput &input, compat::ScenarioBehaviorDecision *decision);
	struct PlantMoveChatter
	{
		std::uint64_t epoch = 0;
		float age = 0;
		bool coverDue = false, hasGoal = false;
		nav::NavVector goal{};
		nav::NavVector reportPosition{};
	};
	void updateManagedBotCompatibilityState(
		std::size_t index, const runtime::MovementPhysicsState &before, bool recoveringFromBlind,
		PlantMoveChatter *plantMove = nullptr);
	void updateOutgoingPublicRadio(std::size_t index, const PlantMoveChatter *plantMove = nullptr);
	void observeBombPickup(Vector origin, world::FrameIdentity frame);
	void observeHostageUsed(Vector origin, world::FrameIdentity frame, world::EntityKey following);
	compat::ChatterScheduleAdmission chatterAdmission(std::size_t index);
	compat::ChatterScheduleEntry activeTeamChatter(std::size_t index, int team, bool *current);
	void updatePublicBuying(std::size_t index);
	std::uint64_t executionRevision(std::size_t index) const
	{
		return actors_[index].executionRevision;
	}
	CombatUpkeepUpdate upkeepCombat(
		std::size_t index, const runtime::MovementPhysicsState &before, std::uint16_t cachedButtons, bool blind = false);
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
	void observeHostageUseDispatched(std::size_t index, world::EntityKey target, const world::WorldPosition &origin);

	void observeNeutralC4AttackDispatched(std::size_t index, const FakeClientHandle &handle);
	void observeLooseBombModel(edict_t *box, const char *model);

private:
	struct ActorTaskEnvironment;
	void reportBombPickedUp(std::size_t index);
	void hideNearLooseBomb(std::size_t index, bool announce);
	bool selectHostageDefense(std::size_t index, bool rogue, const compat::ScenarioBehaviorInput &scenario);
	bool renewHostageRescueGuard(std::size_t index);
	bool selectPreplantDefense(std::size_t index, bool cooperative, const compat::ScenarioBehaviorInput &scenario);
	void hideAtPlantedBombsite(std::size_t index, compat::ScenarioTarget target);
	void guardAcceptedDefuser(std::size_t index, const compat::ScenarioTarget &bomb, unsigned defuserSlot);
	bool reportCoveringDefuser(std::size_t index, unsigned defuserSlot);
	bool renewBombsiteGuard(std::size_t index);
	void rememberLooseBombDrop(world::EntityKey box, world::FrameIdentity frame, nav::NavVector origin);
	nav::AreaId looseBombArea(const nav::NavSnapshot &navigation, const std::function<bool()> &current);
	bool guardedBombPosition(std::size_t index, nav::NavVector *position) const;
	void transmitTeammateRequest(std::size_t sender, compat::ChatterMeme meme, const std::function<bool()> &current);
	bool helpGoalCurrent(std::size_t index) const;
	bool executionCurrent(std::size_t index, const compat::ReactionContext &receipt, std::uint64_t revision) const;
	void invalidateHelpExecution(std::size_t index);
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
		compat::KnownWeaponFlag shield{};
	};
	struct SniperWarningTiming
	{
		float sightUntil = -1.0f;
		float warnedUntil = -1.0f;
	};
	struct RememberedHostage
	{
		world::EntityKey entity{};
		world::WorldPosition position{};
		bool valid = false;
		bool alive = false;
	};
	struct HostagePositionMemory
	{
		static constexpr std::size_t kMaximumHostages = 12;
		std::array<RememberedHostage, kMaximumHostages> entries{};
		float nextCheck = -1.0f;
		bool hasKnownPosition() const
		{
			return std::any_of(entries.begin(), entries.end(), [](const RememberedHostage &hostage) { return hostage.valid; });
		}
	};
	struct ScenarioUpdateData
	{
		const compat::ObservationTimingContext &timing;
		int observedTeam = 0;
		unsigned defuserSlot = 0;
		float automaticLeaderRange = (std::numeric_limits<float>::max)();
		compat::ScenarioTarget looseBomb{};
		nav::NavVector looseBombOrigin{};
		nav::NavSnapshot navigation{};
		HostagePositionMemory initialHostages{};
		bool wasFollowing = false;
	};
	bool refreshRecoveredPerception(std::size_t index);
	bool prepareManagedPerception(std::size_t index, world::WorldSnapshot *snapshot, world::ActorKey *targetActor,
		world::WorldPosition *targetPosition);
	BehaviorPhaseResult prepareManagedAction(ManagedActionContext &ctx);
	BehaviorPhaseResult decideObjectiveAction(ManagedActionContext &ctx);
	ActionProposal decideCombatAction(ManagedActionContext &ctx);
	struct SelfDefenseThreat
	{
		const edict_t *entity = nullptr;
		world::ActorKey actor{};
		world::EntityKey source{};
		compat::ReactionContext receipt{};
		std::uint64_t execution = 0;
		std::uint64_t normalEpoch = 0;
		std::uint64_t attackEpoch = 0;
	};
	SelfDefenseThreat evaluateSelfDefense(std::size_t index, world::ActorKey target, bool visible);
	bool selfDefenseThreatCurrent(std::size_t index, const SelfDefenseThreat &threat) const;
	void tryBombObjectiveAttack(std::size_t index, const compat::StateUpdateContext &context);
	bool prepareNativeAttack(std::size_t index, world::ActorKey target, std::uint16_t weapon);
	bool applyAttackShield(ManagedActionContext &context, bool visible, bool knife);
	bool enemyLookingAtUs(std::size_t index, const edict_t *enemy, world::FrameIdentity frame);
	void applyAttackPosture(std::size_t index, ActionProposal &proposal);
	bool updateCombatRetreat(std::size_t index, int team);
	bool turnToThreateningGunfire(const ManagedActionContext &ctx, const compat::StateUpdateContext &stateContext);
	bool retaliateAgainstRecentAttacker(const ManagedActionContext &ctx, bool sniper);
	bool updateCombatAmbush(const ManagedActionContext &ctx, const compat::StateUpdateContext &stateContext,
		bool enemyVisible);
	bool tryCombatAmbush(std::size_t index, int team);
	bool decideReloadAction(ManagedActionContext &ctx);
	bool tryReloadCover(std::size_t index, int team, int remainingEnemies, float timeSinceEnemy);
	bool respondToFallBack(std::size_t index, int team, const compat::ScenarioFallBackResponse &response);
	void encourageHumanDefuse(std::size_t index, float remaining, const std::function<bool()> &current);
	void reportAutomaticFollow(std::size_t index, const std::function<bool()> &current);
	bool waitAtEndOfSafeTime(std::size_t index, const compat::ScenarioBehaviorInput &scenario,
		const std::function<bool()> &current);
	void reportLostEnemy(std::size_t index, world::EntityKey target, const std::function<bool()> &current);
	void updateGuardLook(ManagedActionContext &ctx);
	bool decideEquipmentAction(ManagedActionContext &ctx, const compat::PublicActorState &weapon, bool attacking);
	bool cancelHeldGrenade(std::size_t index, const runtime::ViewAngles &movementAngles, ActionProposal &proposal);
	BehaviorPhaseResult updateBehaviorLifecycle(
		std::size_t index, const runtime::MovementPhysicsState &before, compat::StateUpdateContext &context);
	bool applyBombPlantNotice(std::size_t index, const compat::StateUpdateContext &context);
	bool shouldBuy(std::size_t index, const compat::StateUpdateContext &context);
	bool usesCsBotBuying() const;
	void updateNoiseInvestigation(std::size_t index, const runtime::MovementPhysicsState &before,
		compat::StateUpdateContext &context, compat::StateBehaviorInput &stateBehavior, bool wasAtDefenseSpot);
	void updateBehaviorPriorities(std::size_t index, const runtime::MovementPhysicsState &before,
		compat::StateUpdateContext &context, compat::StateBehaviorInput &stateBehavior, bool wasAtDefenseSpot);
	void commitBehaviorState(
		std::size_t index, compat::StateUpdateContext &context, compat::StateBehaviorInput &stateBehavior);
	void updateScenarioState(std::size_t index, const runtime::MovementPhysicsState &before,
		compat::StateUpdateContext &context, compat::StateBehaviorInput &stateBehavior, PlantMoveChatter *plantMove);
	bool updateFetchBomb(std::size_t index, compat::StateUpdateContext &context,
		compat::StateBehaviorInput &stateBehavior, const compat::CompatibilityObservation &observation,
		const compat::ScenarioBehaviorInput &scenario, const ScenarioUpdateData &data);
	void populateScenarioPublicState(std::size_t index, const runtime::MovementPhysicsState &before,
		compat::StateUpdateContext &context, compat::StateBehaviorInput &stateBehavior,
		const compat::CompatibilityObservation &observation, compat::ScenarioBehaviorInput &scenario,
		ScenarioUpdateData &data);
	void observeScenarioEntities(std::size_t index, const runtime::MovementPhysicsState &before,
		compat::StateUpdateContext &context, compat::StateBehaviorInput &stateBehavior,
		const compat::CompatibilityObservation &observation, compat::ScenarioBehaviorInput &scenario,
		ScenarioUpdateData &data);
	void updateHostagePositions(std::size_t index, const compat::StateUpdateContext &context,
		const HostagePositionMemory &initial);
	void reactToHostageLoss(std::size_t index, bool allGone);
	bool allHostagesRescued(world::FrameIdentity frame) const;
	void updateScenarioNavigation(std::size_t index, const runtime::MovementPhysicsState &before,
		compat::StateUpdateContext &context, compat::StateBehaviorInput &stateBehavior,
		const compat::CompatibilityObservation &observation, compat::ScenarioBehaviorInput &scenario,
		ScenarioUpdateData &data);
	void updateScenarioTeamwork(std::size_t index, const runtime::MovementPhysicsState &before,
		compat::StateUpdateContext &context, compat::StateBehaviorInput &stateBehavior,
		compat::ScenarioBehaviorInput &scenario, ScenarioUpdateData &data);
	void applyScenarioDecision(std::size_t index, const runtime::MovementPhysicsState &before,
		compat::StateUpdateContext &context, compat::StateBehaviorInput &stateBehavior,
		const compat::CompatibilityObservation &observation, compat::ScenarioBehaviorInput &scenario,
		ScenarioUpdateData &data, PlantMoveChatter *plantMove);
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
			actor.teamworkPolicies.resetValues();
	}

	void reset_bombSearchTargets()
	{
		for (auto &actor : actors_)
		{
			actor.bombSearchTargets = {};
			actor.plantedBombsite = {};
			actor.discoveredBombsiteReport = {};
		}
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

	compat::RadioContactCounts livingRadioPeers(const edict_t *self, int ownTeam) const;
	struct ChatterPeerReceipt
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
	ChatterPeerReceipt snapshotChatterPeer(std::size_t index) const;
	static bool sameChatterPeer(const ChatterPeerReceipt &a, const ChatterPeerReceipt &b);
	struct ChatterBank
	{
		std::string name;
		compat::ChatterVoiceCatalog catalog;
		runtime::LifecycleGeneration shuffledRound = 0U;
		bool filesChecked = false;
		bool loading = false, checking = false;
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
	void shuffleReadyChatterBanks();
	std::uint32_t allowedFixedChatterPhrases(const char *verbosity, compat::ChatterPhrase first, compat::ChatterPhrase last);
	void updateFollowMotion(std::size_t index, const compat::StateUpdateContext &context,
		compat::ScenarioBehaviorInput &scenario, bool reloading);
	bool enterFollowMotion(std::size_t index, const compat::StateUpdateContext &context);
	bool tryFollowCover(std::size_t index, const std::function<bool()> &leaderCurrent);
	bool canSeePoint(std::size_t index, world::WorldPosition point, const world::FrameIdentity &frame,
		bool checkFov) const;
	bool canSeeEntityOrigin(std::size_t index, const edict_t *entity, const world::FrameIdentity &frame,
		bool checkFov = true) const;
	bool isBlinded(world::ActorKey actor, world::FrameIdentity frame, float now) const;
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
		RetainedRoundEnd,
		DeathInterrupt
	};
	void finishDeathChatter(std::size_t index, const compat::ConfirmedChatterSpeech &previous,
		const world::FrameIdentity &deathFrame);
	bool sendChatterVoice(std::size_t index, compat::ChatterPhrase phrase, int count,
		std::shared_ptr<compat::ChatterVoiceCatalog> bank, float *duration, std::uint32_t criteriaPlace,
		ChatterVoicePurpose purpose = ChatterVoicePurpose::Statement, std::uint32_t placePhrase = 0,
		bool *selected = nullptr, bool *suppressed = nullptr, std::uint32_t namedPhrase = 0);
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

	struct HelpGoal
	{
		bool active = false;
		nav::NavVector position{};
		world::FrameIdentity frame{};
		std::uint64_t epoch = 0;
	};

	struct TeammateFollow
	{
		world::ActorKey actor{}, entity{};
		std::uint32_t connection = 0;
	};

	struct LooseBombDrop
	{
		world::EntityKey box{};
		world::FrameIdentity frame{};
		nav::NavVector origin{};
		nav::AreaId area = 0;
		std::uint64_t navRevision = 0;
		bool matches(world::EntityKey key, world::FrameIdentity current) const
		{
			return box == key && box.isValid() && frame.mapGeneration == current.mapGeneration &&
				frame.roundGeneration == current.roundGeneration;
		}
	};

	struct AttackVisibility
	{
		std::uint64_t attackEpoch = 0;
		bool seenEnemy = false;
		bool ambushChecked = false;
		bool enemyHidden = false;
		float reacquireAfter = -1.0f;

		bool canFire(std::uint64_t epoch, float now) const
		{
			return attackEpoch != epoch || now > reacquireAfter;
		}
	};
	struct CombatRetreatState
	{
		std::uint64_t attackEpoch = 0;
		std::uint64_t hideEpoch = 0;
		std::uint64_t fallBackEquipEpoch = 0;
		std::uint32_t updateSequence = 0;
		float retryAfter = -1;
		AttackVisibility visibility{};
		compat::AttackEntryPlan posture{};
		compat::AttackShieldControl shield{};
		std::uint64_t postureNormalEpoch = 0;
	};
	enum class LeaderMotion { Unknown, Stopped, Walking, Running };
	struct FollowMotion
	{
		world::ActorKey leader{};
		std::uint64_t epoch = 0;
		LeaderMotion motion = LeaderMotion::Unknown;
		float walkStartedAt = -1.0f;
		float lastVisibleAt = 0.0f;
		bool walking = false;
		bool idleSneak = false;
		float motionStartedAt = 0.0f;
		float coverWait = 0.0f;
		float idleDuration = 0.0f;
		float idleDeadline = 0.0f;
		float repathDeadline = -1.0f;
		std::uint32_t requestUpdate = 0U;
		world::ActorKey leaderConnection{};
		int leaderSerial = -1;
		nav::AreaId goalArea = 0U;
		std::uint64_t goalNavRevision = 0U;
		std::uint32_t goalUpdate = 0U;
		std::uint64_t coverEpoch = 0U;
		world::WorldPosition coverLeaderAnchor{};
	};
	struct ActorState
	{
		compat::BotMorale morale{};
		std::uint64_t executionRevision = 1;
		HelpGoal helpGoal{};
		TeammateFollow teammateFollow{};
		FollowMotion followMotion{};
		world::EntityKey managedTaskEntity{};
		ObstacleLook obstacleLook{};
		float obstacleNextEquipAt = 0.0f;
		bool obstaclePathActive = false;
		nav::NavVector obstaclePathGoal{};
		compat::ReactionQueue reactionQueues{};
		world::ActorKey recognizedTargets{};
		bool recognizedReloading = false;
		world::WorldPosition recognizedPoints{};
		struct OccludedRecognition
		{
			const edict_t *entity = nullptr;
			world::EntityKey source{};
			std::uint32_t frame = 0U;
		};
		OccludedRecognition occludedRecognition{};
		struct BombObjectiveAimDecision
		{
			world::ActorKey target{};
			world::EntityKey source{};
			std::uint64_t stateEpoch = 0;
		};
		BombObjectiveAimDecision bombObjectiveAimDecision{};
		world::WorldPosition plantFloorLook{};
		float desiredLookYaw{};
		std::uint32_t recognitionSequences{};
		float lastVisibleEnemyTimes{};
		compat::NoiseEnemyContact noiseEnemyContact{};
		compat::ReactionContext blindReceipt{};
		std::uint64_t blindLifeRevision = 0;
		std::uint64_t blindEventRevision = 0;
		BlindCommand flashResponse{};
		compat::NoiseLookPlanner noiseLook{};
		NoiseBendCache noiseBend{};
		std::uint32_t noiseLookUpdate = 0U;
		compat::BombGuardPlanner bombGuardPlanners{};
		compat::GuardLookPlanner guardLook{};
		compat::HuntEncounterPlanner huntEncounter{};
		struct HuntApproaches
		{
			nav::NavVector viewPosition{};
			std::array<nav::NavVector, 16> points{};
			std::int32_t count = 0;
			std::uint64_t navRevision = 0;
			float nextLookAt = 0.0f;
			float inhibitUntil = 0.0f;
		};
		HuntApproaches huntApproaches{};
		compat::WeaponEquipmentPlanner equipment{};
		compat::FootstepEquipmentReceipt closeFootstep{};
		VoiceFeedback outgoingVoiceFeedback{};
		compat::OutgoingRadioScheduler outgoingRadioSchedulers{};
		compat::ConfirmedChatterSpeech confirmedSpeech{};
		compat::ChatterAdmissionClock chatterAdmissions{};
		compat::KnifeRunTimer knifeRun{};
		compat::EnemyReactionMemory enemyReaction{};
		CombatRetreatState combatRetreat{};
		world::EntityKey enemyWeaponSource{};
		compat::RadioContactMemory outgoingRadioContactMemory{};
		nav::AreaId scenarioEscapeAreas{};
		compat::BombSiteSearchMemory bombSearchMemory{};
		compat::BombStatusMemory bombStatus{};
		SniperWarningTiming sniperWarning{};
		bool someHostagesTaken = false;
		HostagePositionMemory hostagePositions{};
		compat::ScenarioTarget looseBombObservation{};
		compat::ScenarioTarget bombSearchTargets{};
		compat::ScenarioTarget clearedBombsite{};
		compat::ScenarioTarget plantedBombsite{};
		compat::ChatterStatementRequest discoveredBombsiteReport{};
		compat::TeamworkPolicy teamworkPolicies{};
		std::uint32_t teamworkRadioApplied{};
		compat::ProfileRecord managedProfiles{};
		float lastCombatTimes{};
		float attackReadyTimes{};
		std::uint32_t enemyIdleEntries{};
		compat::BuyController buyControllers{};
		bool buyingComplete{};
		bool buyUsesCsBot{};
		bool escapingWithKnife{};
		bool buyReceiptTimeoutLogged{};
		compat::WeaponActionPlanner weaponActions{};
		compat::NoiseInvestigationPlanner noiseInvestigation{};
		compat::NoiseReportPolicy noiseReportPolicy{};
		compat::HeardNoiseReport noiseReport{};
		bool investigationWalking = false;
		compat::StateBehaviorInput managedStateInputs{};
		bool noisePathPending = false;
		combat::CombatController managedBotCombat{};
		objectives::RoundObjectivePlanner managedBotObjectives{};
		ManagedObjectiveTargetCache managedBotObjectiveTargets{};
		objectives::BombApproachState managedBombApproaches{};
		team::RoundObjectiveCoordinator compatibilityObjectiveCoordinators{};
		ManagedObjectiveActionState managedBotObjectiveActions{};
		compat::CompatibilityStateMachine managedBotStateMachines{};
		compat::ScenarioBehaviorPlanner scenarioPlanners{};
		world::FrameIdentity handledBombPlant{};
		compat::ScenarioBehaviorDecision scenarioDecisions{};
		compat::ScenarioTarget fetchBombGoal{};
		runtime::LifecycleGeneration managedBotStateRounds{};
		bool managedBotWasDead{};
	};
	std::array<ActorState, NativeBotObservation::kClientSlotCount> actors_{};
	LooseBombDrop looseBombDrop_{};
	std::array<BreakableMaterial, 2048U> breakableMaterials_{};
	BotBehaviorServices *services_ = nullptr;
	compat::ChatterRadioCatalog outgoingChatterCatalog_{};
	std::vector<std::shared_ptr<ChatterBank>> outgoingVoiceBanks_{};
	compat::ChatterVoiceRandom outgoingVoiceRandom_{};
	compat::ChatterPlaceHistory outgoingPlaceHistory_{};
	std::uint64_t outgoingCatalogRevision_ = 0;
	std::array<RadioPreference, NativeBotObservation::kClientSlotCount> outgoingRadioPreferences_{};
	std::uint32_t outgoingVoiceMask_ = 0U;
	static constexpr unsigned kTeamRadioSlotCount = 3U;
	float encouragementUntil_ = -1.0f;
	std::array<float, kTeamRadioSlotCount> outgoingTeamLastSpeech_{{-1.0f, -1.0f, -1.0f}};
	std::array<float, kTeamRadioSlotCount> outgoingTeamBusyUntil_{};
	std::array<float, kTeamRadioSlotCount> outgoingTeamFollowRadio_{};
	std::array<compat::ChatterPhrase, kTeamRadioSlotCount> outgoingTeamCurrentPhrase_{};
	std::array<std::uint16_t, kTeamRadioSlotCount> outgoingTeamCurrentPlace_{};
	std::array<bool, static_cast<unsigned>(compat::ChatterPhrase::Count)> outgoingMissingPhraseLogged_{};

	runtime::LifecycleGeneration outgoingRadioMap_ = runtime::LifecycleSession::kInvalidGeneration;
	runtime::LifecycleGeneration outgoingRadioRound_ = runtime::LifecycleSession::kInvalidGeneration;
	objectives::RoundPlantDecision roundPlantDecision_{};
	world::FrameIdentity hostageRescueResult_{};
	std::uint32_t moraleResultRound_ = 0;
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
