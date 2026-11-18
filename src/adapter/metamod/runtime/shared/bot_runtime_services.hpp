#ifndef ASTRABOT_ADAPTER_BOT_RUNTIME_SERVICES_HPP
#define ASTRABOT_ADAPTER_BOT_RUNTIME_SERVICES_HPP
#include "runtime/shared/bot_runtime_types.hpp"
#include "astrabot/compat/footstep_equipment.hpp"
#include <bitset>
namespace astrabot
{
namespace metamod
{
using LateDefuseRecipients = std::bitset<NativeBotObservation::kClientSlotCount>;

struct BotRuntimeServiceTypes
{
	using Service_adapterFrameCount = std::uint32_t;
	using Service_compatibilityRandomSource = compat::EngineRandomSource;
	using Service_compatibilitySurface = CompatibilitySurface;
	using Service_engineFunctions = enginefuncs_t *;
	using Service_globals = globalvars_t *;
	using Service_inputDispatcher = InputDispatcher;
	using Service_joinControllers = std::array<JoinController, NativeBotObservation::kClientSlotCount>;
	using Service_lifecycle = runtime::LifecycleSession;
	using Service_managedBotFullUpdateSequences = std::array<std::uint32_t, NativeBotObservation::kClientSlotCount>;
	using Service_managedBotHandles = std::array<FakeClientHandle, NativeBotObservation::kClientSlotCount>;
	using Service_managedBotSlots = std::array<bool, NativeBotObservation::kClientSlotCount>;
	using Service_managedBotTiming = std::array<runtime::BotTimingScheduler, NativeBotObservation::kClientSlotCount>;
	using Service_navPublisher = nav::NavSnapshotPublisher;
	using Service_pluginId = plid_t;
	using Service_roundFreezeGeneration = runtime::LifecycleGeneration;
	using Service_roundFreezeScheduled = bool;
	using Service_roundFreezeStartTime = float;
	using Service_roundFreezeUntil = float;
	using Service_runtimeProfiler = RuntimeProfiler;
};
class BotObservationServices : public BotRuntimeServiceTypes
{
public:
	virtual ~BotObservationServices() = default;
	virtual void notifyMenuReady(edict_t *entity, JoinMenuKind menu, std::uint16_t validSlots = 0U,
		JoinMenuSource source = JoinMenuSource::Unknown) = 0;
	virtual void observeJoinTeamInfo(std::size_t index, const char *teamName) = 0;
	virtual bool runtimeIsActive() const = 0;
	virtual Service_compatibilityRandomSource &service_compatibilityRandomSource() = 0;
	virtual const Service_navPublisher &service_navPublisher() = 0;
	virtual const Service_adapterFrameCount &service_adapterFrameCount() = 0;
	virtual const Service_compatibilitySurface &service_compatibilitySurface() = 0;
	virtual const Service_engineFunctions &service_engineFunctions() = 0;
	virtual const Service_globals &service_globals() = 0;
	virtual const Service_joinControllers &service_joinControllers() = 0;
	virtual const Service_lifecycle &service_lifecycle() = 0;
	virtual const Service_managedBotFullUpdateSequences &service_managedBotFullUpdateSequences() = 0;
	virtual const Service_managedBotHandles &service_managedBotHandles() = 0;
	virtual const Service_managedBotSlots &service_managedBotSlots() = 0;
	virtual const Service_pluginId &service_pluginId() = 0;
	virtual Service_runtimeProfiler &service_runtimeProfiler() = 0;
	virtual void behavior_invalidateObjectiveAssignments() = 0;
	virtual const compat::ProfileRecord &behavior_managedProfiles(std::size_t index) const = 0;
	virtual void behavior_observeRadioPreference(edict_t *recipient, bool ignored) = 0;
	virtual void observeBroadcastTeamScore(int team, int score, world::FrameIdentity frame) = 0;
	virtual void behavior_observeRoundWin(int team, world::FrameIdentity frame, bool rescuedHostages = false,
		const LateDefuseRecipients &lateDefuse = {}) = 0;
	virtual void behavior_resetEquipment(std::size_t index, bool spawnHud = false) = 0;
	virtual std::uint64_t behavior_chatterRevision(std::size_t index) const = 0;
	virtual void behavior_observePublicDeath(std::size_t index, const world::FrameIdentity &deathFrame = {}) = 0;
	virtual void behavior_observePlayerDeath(int victimSlot, int killerSlot, edict_t *recipient, const world::FrameIdentity &frame) = 0;
	virtual void behavior_observePlayerBlind(std::size_t index, world::ActorKey actor, world::FrameIdentity frame) = 0;
	virtual void behavior_observePlayerDamage(std::size_t index, world::ActorKey actor, world::FrameIdentity frame,
		const edict_t *source, int serial, world::ActorKey sourceConnection) = 0;
	virtual world::EntityKey behavior_currentEnemySource(std::size_t index) const = 0;
	virtual void behavior_observeEnemyWeaponFire(const edict_t *source, const world::FrameIdentity &frame, float now) = 0;
	virtual void behavior_observeCloseFootstep(std::size_t index, const compat::FootstepEquipmentReceipt &receipt) = 0;
	virtual const std::uint32_t &command_managedBotCommandSequences(std::size_t index) const = 0;
	virtual void navigation_observePublicDeath(std::size_t victim) = 0;
	virtual void navigation_observeResetHud(std::size_t index) = 0;
};
class BotBehaviorServices : public BotRuntimeServiceTypes
{
public:
	virtual ~BotBehaviorServices() = default;
	virtual void applyNavigationRequest(const NavigationRequest &request) = 0;
	virtual const Service_adapterFrameCount &service_adapterFrameCount() = 0;
	virtual Service_compatibilityRandomSource &service_compatibilityRandomSource() = 0;
	virtual const Service_compatibilitySurface &service_compatibilitySurface() = 0;
	virtual const Service_engineFunctions &service_engineFunctions() = 0;
	virtual const Service_globals &service_globals() = 0;
	virtual const Service_joinControllers &service_joinControllers() = 0;
	virtual const Service_lifecycle &service_lifecycle() = 0;
	virtual const Service_managedBotFullUpdateSequences &service_managedBotFullUpdateSequences() = 0;
	virtual const Service_managedBotHandles &service_managedBotHandles() = 0;
	virtual const Service_managedBotSlots &service_managedBotSlots() = 0;
	virtual const Service_navPublisher &service_navPublisher() = 0;
	virtual const Service_pluginId &service_pluginId() = 0;
	virtual const Service_roundFreezeGeneration &service_roundFreezeGeneration() = 0;
	virtual const Service_roundFreezeStartTime &service_roundFreezeStartTime() = 0;
	virtual const Service_roundFreezeUntil &service_roundFreezeUntil() = 0;
	virtual Service_runtimeProfiler &service_runtimeProfiler() = 0;
	virtual bool service_roundOver() = 0;
	virtual bool service_deferToHuman() = 0;
	virtual bool service_excellentMorale() = 0;
	virtual bool command_dispatchClientCommand(edict_t *entity, const char *name, const char *argument) = 0;
	virtual const std::uint32_t &command_managedBotCommandSequences(std::size_t index) const = 0;
	virtual nav::AreaId navigation_knownArea(
		std::size_t index, world::ActorKey actor, world::FrameIdentity frame, std::uint64_t revision) const = 0;
	virtual bool navigation_hasActivePath(std::size_t index) const = 0;
	virtual float navigation_pathDistanceRemaining(std::size_t index, const nav::NavSnapshot &snapshot) const = 0;
	virtual const ManagedNavigationFailure &navigation_managedNavigationFailures(std::size_t index) const = 0;
	virtual nav::NavPathCostContext navigation_managedPathCostContext(std::size_t index) = 0;
	virtual bool navigation_ladderTopology(nav::NavLadderTopology *topology) = 0;
	virtual bool navigation_nativeLadderLook(std::size_t index, nav::NativeLadderLook *look) = 0;
	virtual const bool &observation_bombNoticeKnown(std::size_t index) const = 0;
	virtual const bool &observation_bombNoticePlanted(std::size_t index) const = 0;
	virtual bool observation_collectManagedPerception(
		std::size_t index, world::WorldSnapshot *snapshot, ThreatObservation *threats) = 0;
	virtual bool observation_activeSniper(
		world::ActorKey actor, world::FrameIdentity frame, int observerTeam) const = 0;
	virtual bool observation_readManagedWorldSnapshot(std::size_t index, world::WorldSnapshot *snapshot) const = 0;
	virtual const std::uint32_t &observation_externalDefuserEntityIndex() const = 0;
	virtual const float &observation_externalDefuserLeaseUntil() const = 0;
	virtual const std::uint32_t &observation_externalDefuserMapGeneration() const = 0;
	virtual const std::uint32_t &observation_externalDefuserRoundGeneration() const = 0;
	virtual const ManagedCurrentWeaponObservation &observation_managedBotCurrentWeapons(std::size_t index) const = 0;
	virtual const perception::PerceptionAssembler &observation_managedBotPerception(std::size_t index) const = 0;
	virtual bool observation_consumeNoise(std::size_t index, const world::NoiseMemory &expected) = 0;
	virtual const std::uint8_t &observation_teamInfoTeams(std::size_t index) const = 0;
	virtual const bool &observation_publicTeamKnown(std::size_t index) const = 0;
	virtual const std::array<ObjectiveKnowledge, kObjectiveKnowledgeStateCount> &observation_objectiveKnowledge(
		std::size_t index) const = 0;
	virtual const ObservationAdapter &observation_observationAdapter() const = 0;
	virtual bool observation_observeObjective(std::size_t index, edict_t *entity, bool planted,
		world::WorldVector *position, bool *currentlyVisible = nullptr) = 0;
	virtual void observation_observeRadioLeaderPosition(std::size_t index, const compat::ScenarioTarget &position) = 0;
	virtual const compat::PublicBombClock &observation_publicBombClocks(std::size_t index) const = 0;
	virtual const compat::PublicObservationStore &observation_publicObservations() const = 0;
	virtual int observation_resolvePlayerTeam(const edict_t *entity) const = 0;
	virtual const std::array<PublicScoreStatus, NativeBotObservation::kClientSlotCount> &
	observation_publicScoreStatuses(std::size_t index) const = 0;
	virtual const compat::ScenarioRadioObservation &observation_scenarioRadio(std::size_t index) const = 0;
	virtual const compat::SmokeObservations &observation_smokeObservations() const = 0;
};
class BotNavigationServices : public BotRuntimeServiceTypes
{
public:
	virtual ~BotNavigationServices() = default;
	virtual bool roundFreezeActive() const = 0;
	virtual const Service_adapterFrameCount &service_adapterFrameCount() = 0;
	virtual Service_compatibilityRandomSource &service_compatibilityRandomSource() = 0;
	virtual const Service_compatibilitySurface &service_compatibilitySurface() = 0;
	virtual const Service_engineFunctions &service_engineFunctions() = 0;
	virtual const Service_globals &service_globals() = 0;
	virtual const Service_joinControllers &service_joinControllers() = 0;
	virtual const Service_lifecycle &service_lifecycle() = 0;
	virtual const Service_managedBotFullUpdateSequences &service_managedBotFullUpdateSequences() = 0;
	virtual const Service_managedBotHandles &service_managedBotHandles() = 0;
	virtual const Service_managedBotSlots &service_managedBotSlots() = 0;
	virtual const Service_navPublisher &service_navPublisher() = 0;
	virtual const Service_pluginId &service_pluginId() = 0;
	virtual Service_runtimeProfiler &service_runtimeProfiler() = 0;
	virtual std::uint64_t behavior_executionRevision(std::size_t index) const = 0;
	virtual bool behavior_noiseInvestigationWalking(std::size_t index) const = 0;
	virtual bool behavior_buildManagedObjectiveTarget(std::size_t index, nav::NavVector *target,
		nav::NavSearchStats *searchStats, RuntimeObjectiveSelectionStats *objectiveStats = nullptr) = 0;
	virtual const team::TeamObjectiveAssignment *behavior_findManagedTeamObjectiveAssignment(
		const world::ActorKey &actor) const = 0;
	virtual const ManagedObjectiveActionState &behavior_managedBotObjectiveActions(std::size_t index) const = 0;
	virtual const ManagedObjectiveTargetCache &behavior_managedBotObjectiveTargets(std::size_t index) const = 0;
	virtual const compat::CompatibilityStateMachine &behavior_managedBotStateMachines(std::size_t index) const = 0;
	virtual const compat::ProfileRecord &behavior_managedProfiles(std::size_t index) const = 0;
	virtual void behavior_noteNavigationFailure(const FakeClientHandle &handle, runtime::NavRoamResult roamResult,
		const runtime::NavRoamDecision &roamDecision) = 0;
	virtual void behavior_resetBombApproach(std::size_t index) = 0;
	virtual const compat::ScenarioBehaviorDecision &behavior_scenarioDecisions(std::size_t index) const = 0;
	virtual std::uint64_t behavior_chatterRevision(std::size_t index) const = 0;
	virtual const bool &command_managedBotCommandTemplateValid(std::size_t index) const = 0;
	virtual const runtime::BotCommand &command_managedBotCommandTemplates(std::size_t index) const = 0;
	virtual void command_resetGateDiagnostics() = 0;
	virtual runtime::MovementPhysicsState observation_captureMovementPhysicsState(
		const edict_t *entity, bool teamConfirmed, bool managedFakeClient) const = 0;
	virtual std::vector<nav::NavVector> observation_liveFriendlyPositions(std::size_t index, bool feet) const = 0;
	virtual const std::uint8_t &observation_teamInfoTeams(std::size_t index) const = 0;
	virtual int observation_resolvePlayerTeam(const edict_t *entity) const = 0;
	virtual const bool &observation_publicTeamKnown(std::size_t index) const = 0;
};
class BotCommandServices : public BotRuntimeServiceTypes
{
public:
	virtual ~BotCommandServices() = default;
	virtual std::uint8_t consumeCommandMsec(std::size_t index, float now) = 0;
	virtual bool dispatchClientCommand(edict_t *entity, const char *name, const char *argument) = 0;
	virtual std::uint32_t publicPlayerPriority(const edict_t *entity) const = 0;
	virtual bool roundFreezeActive() const = 0;
	virtual bool runtimeIsActive() const = 0;
	virtual const Service_adapterFrameCount &service_adapterFrameCount() = 0;
	virtual const Service_compatibilitySurface &service_compatibilitySurface() = 0;
	virtual const Service_engineFunctions &service_engineFunctions() = 0;
	virtual const Service_globals &service_globals() = 0;
	virtual Service_inputDispatcher &service_inputDispatcher() = 0;
	virtual const Service_joinControllers &service_joinControllers() = 0;
	virtual const Service_lifecycle &service_lifecycle() = 0;
	virtual const Service_managedBotFullUpdateSequences &service_managedBotFullUpdateSequences() = 0;
	virtual const Service_managedBotHandles &service_managedBotHandles() = 0;
	virtual const Service_managedBotSlots &service_managedBotSlots() = 0;
	virtual const Service_pluginId &service_pluginId() = 0;
	virtual const Service_roundFreezeScheduled &service_roundFreezeScheduled() = 0;
	virtual const Service_roundFreezeUntil &service_roundFreezeUntil() = 0;
	virtual Service_runtimeProfiler &service_runtimeProfiler() = 0;
	virtual void behavior_applyObstacleCommand(std::size_t index, runtime::BotCommand &command) = 0;
	virtual bool behavior_forcedRunning(std::size_t index) = 0;
	virtual bool behavior_noiseInvestigationWalking(std::size_t index) const = 0;
	virtual void behavior_applyNativeLadderLookCommand(
		std::size_t index, runtime::BotCommand &command, bool localMovement) = 0;
	virtual void behavior_applyNoiseLookCommand(std::size_t index, runtime::BotCommand &command) = 0;
	virtual const ManagedObjectiveTargetCache &behavior_managedBotObjectiveTargets(std::size_t index) const = 0;
	virtual const compat::CompatibilityStateMachine &behavior_managedBotStateMachines(std::size_t index) const = 0;
	virtual void behavior_observeC4AttackDispatched(
		std::size_t index, const FakeClientHandle &handle, std::uint16_t buttons) = 0;
	virtual void behavior_observeC4SelectionDispatched(std::size_t index, const FakeClientHandle &handle) = 0;
	virtual void behavior_observeEquipmentDispatched(std::size_t index, std::uint16_t weapon, bool success) = 0;
	virtual void behavior_observeNeutralC4AttackDispatched(std::size_t index, const FakeClientHandle &handle) = 0;
	virtual void navigation_logMovementDiagnostic(
		std::size_t index, const char *reason, const runtime::NavRoamDecision *decision = nullptr) = 0;
	virtual void navigation_logMovementReversalDiagnostic(std::size_t index,
		const runtime::MovementPhysicsState &before, const runtime::NavRoamDecision &decision,
		const nav::LocomotionIntent &intent) = 0;
	virtual void navigation_logTraversalDiagnostic(std::size_t index, const runtime::MovementPhysicsState &before,
		const runtime::NavRoamDecision &decision, const nav::LocomotionIntent &intent, std::uint16_t buttons,
		const nav::LocomotionObservation &locomotion) = 0;
	virtual void navigation_logTraversalOutcome(std::size_t index, const runtime::MovementPhysicsState &after,
		const runtime::NavRoamDecision &decision, const runtime::LandingDamageObservation &landingFeedback) = 0;
	virtual const compat::NavigationStep &navigation_managedNavigationSteps(std::size_t index) const = 0;
	virtual const std::uint8_t &navigation_movementDiagnosticSamples(std::size_t index) const = 0;
	virtual void navigation_noteMovementDiagnosticSample(std::size_t index) = 0;
	virtual void navigation_recordMovementPhysicsSample(
		std::size_t index, const runtime::MovementPhysicsState &before, const runtime::CommandReceipt &receipt) = 0;
	virtual runtime::MovementPhysicsState observation_captureMovementPhysicsState(
		const edict_t *entity, bool teamConfirmed, bool managedFakeClient) const = 0;
	virtual bool observation_hasObservedActiveWeapon(std::size_t index) const = 0;
	virtual const ManagedCurrentWeaponObservation &observation_managedBotCurrentWeapons(std::size_t index) const = 0;
	virtual void observation_observeReloadDispatched(world::ActorKey actor, world::FrameIdentity frame, float now) = 0;
	virtual runtime::ActorId observation_publicPlayerIdentity(const edict_t *entity) const = 0;
};
} // namespace metamod
} // namespace astrabot
#endif
