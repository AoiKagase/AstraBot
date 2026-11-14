#ifndef ASTRABOT_ADAPTER_METAMOD_PLUGIN_RUNTIME_HPP
#define ASTRABOT_ADAPTER_METAMOD_PLUGIN_RUNTIME_HPP
#include "runtime/behavior/bot_behavior_runtime.hpp"
#include "runtime/command/bot_command_runtime.hpp"
#include "runtime/navigation/bot_navigation_runtime.hpp"
#include "runtime/observation/bot_observation_runtime.hpp"
#include "runtime/shared/bot_runtime_types.hpp"
#include "bridge/movement_sound_bridge.hpp"

namespace astrabot
{
namespace metamod
{
class PluginRuntime : public BotObservationServices,
					  public BotBehaviorServices,
					  public BotNavigationServices,
					  public BotCommandServices
{
public:
	enum class State
	{
		Cold,
		Attached,
		ActiveMap,
		Detached
	};

	struct Snapshot
	{
		State state;
		compat::RuntimeMode mode;
		runtime::LifecycleGeneration mapGeneration;
		runtime::LifecycleGeneration roundGeneration;
		NativeBotGuardState nativeGuardState;
		NativeBotGuardReason nativeGuardReason;
		bool managedBotCreationAllowed;
		bool roundFreezeActive;
	};

	static PluginRuntime &instance();

	bool attach(PLUG_LOADTIME loadTime, META_FUNCTIONS *functionTable, meta_globals_t *metaGlobals,
		gamedll_funcs_t *gameDllFunctions, plid_t pluginId);
	bool detach(PLUG_LOADTIME loadTime, PL_UNLOAD_REASON reason);
	bool provideEntityApi(DLL_FUNCTIONS *functionTable, int *interfaceVersion);
	bool provideEntityApiPost(DLL_FUNCTIONS *functionTable, int *interfaceVersion);
	bool provideEngineFunctions(enginefuncs_t *engineFunctions, int *interfaceVersion);
	bool provideEngineFunctionsPost(enginefuncs_t *engineFunctions, int *interfaceVersion);
	void giveEnginePointers(enginefuncs_t *engineFunctions, globalvars_t *globals);
	Snapshot snapshot() const;
	NavLoadResult loadNavigationFile(const NavLoadRequest *request);
	NavLoadDiagnostic navigationDiagnostic() const;
	nav::NavSnapshot navigationSnapshot() const;
	runtime::MovementPhysicsSample movementPhysicsSample(std::uint32_t slot) const;
	runtime::LifecycleToken tokenForSlot(std::uint32_t slot) const;

	void onClientDisconnect(edict_t *entity);
	void onKeyValue(edict_t *entity, KeyValueData *data);
	void onTouch(edict_t *entity, edict_t *other);
	void onClientPutInServer(edict_t *entity);
	void onCmdStart(const edict_t *entity, std::uint16_t buttons);
	void onServerActivate(edict_t *edictList, int edictCount, int clientMax);
	void onServerDeactivate();
	void onStartFrame();
	void onStartFramePost();
	void notifyMenuReady(edict_t *entity, JoinMenuKind menu, std::uint16_t validSlots = 0U,
		JoinMenuSource source = JoinMenuSource::Unknown);
	void onMessageBegin(int messageDestination, int messageType, const float *origin, edict_t *entity);
	void onMessageEnd();
	void onRoundLifecycleMessage(const char *message);
	bool beginRoundGeneration();
	void onWriteByte(int value);
	void onWriteChar(int value);
	void onWriteShort(int value);
	void onWriteLong(int value);
	void onWriteCoord(float value);
	void onEmitSound(edict_t *entity, const char *sample, float volume, float attenuation);
	void onPmMove(playermove_t *move, int server);
	void onPmMovePost(playermove_t *move, int server);
	void onMovementSound(int channel, const char *sample, float volume, float attenuation, int flags, int pitch);
	void onPrecacheEvent(int type, const char *script, unsigned short index);
	void onPlaybackEvent(
		int flags, const edict_t *invoker, unsigned short index, float delay, int bparam1, int bparam2);
	void onWriteString(const char *value);
	void onAddServerCommand(char *command, void (*function)(void));
	void onCompatibilityCommand();
	compat::CvarUpdateResult setCompatibilityFloat(const char *name, float value);
	compat::CvarUpdateResult setCompatibilityString(const char *name, const char *value);
	ProfileLoadResult loadCompatibilityProfiles(const char *path);
	CompatibilityCommandResult executeCompatibilityCommand(const compat::CommandRequest &request);
	FakeClientResult createFakeClient(const char *name, FakeClientHandle *handle);
	FakeClientResult removeFakeClient(FakeClientHandle *handle);
	runtime::QueueResult enqueueBotCommand(const runtime::BotCommand &command);
	runtime::CommandReceipt dispatchBotInput(const runtime::ActorId &actor, std::uint32_t dispatchFrame);

private:
	bool movementSoundOrigin(const edict_t *source, world::WorldVector *origin) const;
	void prepareNeutralManagedBotCommand(
		std::size_t index, FakeClientHandle &handle, const runtime::MovementPhysicsState &before);
	runtime::CommandReceipt dispatchNeutralMovement(
		std::size_t index, FakeClientHandle &handle, std::uint8_t milliseconds);
	void buildAndExecuteMovement(std::size_t index, FakeClientHandle &handle, const runtime::LifecycleToken &token,
		const runtime::MovementPhysicsState &before, NavigationUpdate &movement);
	static constexpr std::size_t kCompatibilityCvarCount = 7U;

	friend const char *HookCommandArgs();
	friend const char *HookCommandArgv(int index);
	friend int HookCommandArgc();
	PluginRuntime();
	NativeBotObservation collectNativeBotObservation() const;
	void synchronizeNativeBotControls();
	void synchronizeCompatibilityCvars();
	void registerCompatibilityCvars();
	void updateNativeBotGuard();
	bool armNativeBotGuard();
	void resetNativeBotGuard();
	void restoreNativeBotControls();
	void logNativeBotGuardTransition(const NativeBotGuardDecision &previousDecision) const;
	void configureFakeClientManager();
	void registerCompatibilityCommands();
	void loadCurrentMapNavigation();
	void updateManagedBots();

	void maintainPublicBotPopulation();

	void resetManagedBotTiming(std::size_t index, float spawnTime);

	void processJoinControllers();
	void processDeferredJoinActions();
	void applyJoinAction(std::size_t index, const JoinAction &action);
	void deferJoinAction(std::size_t index, const JoinAction &action);
	void cleanupManagedJoin(std::size_t index, JoinError error);

	std::uint32_t publicPlayerPriority(const edict_t *entity) const;

	bool roundFreezeActive() const;
	void resetManagedBotExecutionState();

	std::size_t managedBotCount() const;
	FakeClientHandle *findManagedBot(const char *name);
	void rememberManagedBot(const FakeClientHandle &handle, const char *name);
	void clearManagedBot(std::size_t index);
	void clearManagedBots();
	CompatibilityCommandResult executeRemoveCommand(const compat::CommandAction &action);
	CompatibilityCommandResult executeKillCommand(const compat::CommandAction &action);
	static CompatibilityCommandResult mapConfigurationResult(compat::BotActionResult result);
	static CompatibilityCommandResult mapFakeClientResult(FakeClientResult result);
	bool dispatchClientCommand(edict_t *entity, const char *name, const char *argument);
	const char *commandArgs() const;
	const char *commandArgv(int index) const;
	int commandArgc() const;

	using CommandArgsFunction = const char *(*)();
	using CommandArgvFunction = const char *(*)(int);
	using CommandArgcFunction = int (*)();

	State state_;
	meta_globals_t *metaGlobals_;
	gamedll_funcs_t *gameDllFunctions_;
	gamedll_funcs_t hookedGameDllFunctions_;
	enginefuncs_t *engineFunctions_;
	globalvars_t *globals_;
	bool roundFreezeScheduled_;
	float roundFreezeStartTime_;
	float roundFreezeUntil_;
	runtime::LifecycleGeneration roundFreezeGeneration_;
	RoundLifecycleTracker roundLifecycleTracker_;
	runtime::LifecycleSession lifecycle_;
	runtime::ActorRegistry actorRegistry_;
	FakeClientManager fakeClientManager_;
	InputDispatcher inputDispatcher_;
	CompatibilitySurface compatibilitySurface_;
	compat::EngineRandomSource compatibilityRandomSource_;

	float nextPopulationUpdate_ = 0.0f;
	bool populationAdjustment_ = false;

	mutable RuntimeProfiler runtimeProfiler_;
	NavLoader navLoader_;
	nav::NavSnapshotPublisher navPublisher_;
	NavLoadDiagnostic navLoadDiagnostic_;
	std::uint32_t adapterFrameCount_;
	plid_t pluginId_;
	NativeBotGuard nativeBotGuard_;
	NativeBotGuardDecision nativeGuardDecision_;
	std::array<bool, NativeBotObservation::kClientSlotCount> managedBotSlots_;

	std::array<std::uint32_t, NativeBotObservation::kClientSlotCount> managedBotCreationIds_{};
	std::uint32_t nextBotCreationId_ = 0U;

	std::array<FakeClientHandle, NativeBotObservation::kClientSlotCount> managedBotHandles_;
	std::array<std::array<char, compat::ProfileRecord::kNameCapacity + 1U>, NativeBotObservation::kClientSlotCount>
		managedBotNames_;
	std::array<JoinController, NativeBotObservation::kClientSlotCount> joinControllers_;
	DeferredJoinActionQueue pendingJoinActions_;
	std::array<bool, NativeBotObservation::kClientSlotCount> pendingJoinActionOverflow_{};

	std::array<std::uint32_t, NativeBotObservation::kClientSlotCount> managedBotFullUpdateSequences_;

	std::array<runtime::BotTimingScheduler, NativeBotObservation::kClientSlotCount> managedBotTiming_;

	CommandArgsFunction originalCommandArgs_;
	CommandArgvFunction originalCommandArgv_;
	CommandArgcFunction originalCommandArgc_;
	bool clientCommandContextActive_;
	int clientCommandArgumentCount_;
	static constexpr auto kClientCommandArgumentCapacity = 32U;
	std::array<char, kClientCommandArgumentCapacity> clientCommandArgv0_;
	std::array<char, kClientCommandArgumentCapacity> clientCommandArgv1_;
	static constexpr auto kClientCommandArgsCapacity = 128U;
	std::array<char, kClientCommandArgsCapacity> clientCommandArgs_;
	bool nativeGuardEnabled_;
	std::array<bool, kCompatibilityCvarCount> compatibilityCvarOwned_;
	bool compatibilityRegistrationInProgress_;
	bool nativeControlsCaptured_;
	bool performanceDisablePathSearch_;
	float originalBotEnable_;
	float originalBotQuota_;

	void observeJoinTeamInfo(std::size_t index, const char *teamName) override
	{
		deferJoinAction(index, joinControllers_[index].onTeamInfo(teamName));
	}

	void applyNavigationRequest(const NavigationRequest &request) override
	{
		navigation_.applyBehaviorRequest(request);
	}

	std::uint8_t consumeCommandMsec(std::size_t index, float now) override
	{
		return managedBotTiming_[index].consumeCommandMsec(now);
	}

	bool runtimeIsActive() const override
	{
		return state_ == State::ActiveMap;
	}

	MovementSoundBridge movementSounds_{};
	BotObservationRuntime observation_{};
	BotBehaviorRuntime behavior_{};
	BotNavigationRuntime navigation_{};
	BotCommandRuntime command_{};
	const Service_adapterFrameCount &service_adapterFrameCount() override
	{
		return adapterFrameCount_;
	}

	Service_compatibilityRandomSource &service_compatibilityRandomSource() override
	{
		return compatibilityRandomSource_;
	}

	const Service_compatibilitySurface &service_compatibilitySurface() override
	{
		return compatibilitySurface_;
	}

	const Service_engineFunctions &service_engineFunctions() override
	{
		return engineFunctions_;
	}

	const Service_globals &service_globals() override
	{
		return globals_;
	}

	Service_inputDispatcher &service_inputDispatcher() override
	{
		return inputDispatcher_;
	}

	const Service_joinControllers &service_joinControllers() override
	{
		return joinControllers_;
	}

	const Service_lifecycle &service_lifecycle() override
	{
		return lifecycle_;
	}

	const Service_managedBotFullUpdateSequences &service_managedBotFullUpdateSequences() override
	{
		return managedBotFullUpdateSequences_;
	}

	const Service_managedBotHandles &service_managedBotHandles() override
	{
		return managedBotHandles_;
	}

	const Service_managedBotSlots &service_managedBotSlots() override
	{
		return managedBotSlots_;
	}

	const Service_navPublisher &service_navPublisher() override
	{
		return navPublisher_;
	}

	const Service_pluginId &service_pluginId() override
	{
		return pluginId_;
	}

	const Service_roundFreezeGeneration &service_roundFreezeGeneration() override
	{
		return roundFreezeGeneration_;
	}

	const Service_roundFreezeScheduled &service_roundFreezeScheduled() override
	{
		return roundFreezeScheduled_;
	}

	const Service_roundFreezeStartTime &service_roundFreezeStartTime() override
	{
		return roundFreezeStartTime_;
	}

	const Service_roundFreezeUntil &service_roundFreezeUntil() override
	{
		return roundFreezeUntil_;
	}

	bool service_roundOver() override
	{
		return roundLifecycleTracker_.isRoundOver();
	}

	Service_runtimeProfiler &service_runtimeProfiler() override
	{
		return runtimeProfiler_;
	}

	void behavior_invalidateObjectiveAssignments() override
	{
		return behavior_.invalidateObjectiveAssignments();
	}

	const compat::ProfileRecord &behavior_managedProfiles(std::size_t index) const override
	{
		return behavior_.managedProfiles(index);
	}

	void behavior_observeRadioPreference(edict_t *recipient, bool ignored) override
	{
		behavior_.observeRadioPreference(recipient, ignored);
	}

	void behavior_observeRadioReceipt(std::size_t index) override
	{
		return behavior_.observeRadioReceipt(index);
	}

	const std::uint32_t &command_managedBotCommandSequences(std::size_t index) const override
	{
		return command_.managedBotCommandSequences(index);
	}

	void navigation_observePublicDeath(std::size_t victim) override
	{
		return navigation_.observePublicDeath(victim);
	}

	void navigation_observeResetHud(std::size_t index) override
	{
		return navigation_.observeResetHud(index);
	}

	bool command_dispatchClientCommand(edict_t *entity, const char *name, const char *argument) override
	{
		return command_.dispatchClientCommand(entity, name, argument);
	}

	bool navigation_hasActivePath(std::size_t index) const override
	{
		return navigation_.hasActivePath(index);
	}

	const ManagedNavigationFailure &navigation_managedNavigationFailures(std::size_t index) const override
	{
		return navigation_.managedNavigationFailures(index);
	}

	nav::NavPathCostContext navigation_managedPathCostContext(std::size_t index) override
	{
		return navigation_.managedPathCostContext(index);
	}

	const bool &observation_bombNoticeKnown(std::size_t index) const override
	{
		return observation_.bombNoticeKnown(index);
	}

	const bool &observation_bombNoticePlanted(std::size_t index) const override
	{
		return observation_.bombNoticePlanted(index);
	}

	bool observation_collectManagedPerception(
		std::size_t index, world::WorldSnapshot *snapshot, ThreatObservation *threats) override
	{
		return observation_.collectManagedPerception(index, snapshot, threats);
	}

	bool observation_consumeNoise(std::size_t index, const world::NoiseMemory &expected) override
	{
		return observation_.consumeNoise(index, expected);
	}
	bool behavior_forcedRunning(std::size_t index) override
	{
		return behavior_.forcedRunning(index);
	}

	bool behavior_noiseInvestigationWalking(std::size_t index) const override
	{
		return behavior_.noiseInvestigationWalking(index);
	}
	bool observation_readManagedWorldSnapshot(std::size_t index, world::WorldSnapshot *snapshot) const override
	{
		return observation_.readManagedWorldSnapshot(index, snapshot);
	}

	const std::uint32_t &observation_externalDefuserEntityIndex() const override
	{
		return observation_.externalDefuserEntityIndex();
	}

	const float &observation_externalDefuserLeaseUntil() const override
	{
		return observation_.externalDefuserLeaseUntil();
	}

	const std::uint32_t &observation_externalDefuserMapGeneration() const override
	{
		return observation_.externalDefuserMapGeneration();
	}

	const std::uint32_t &observation_externalDefuserRoundGeneration() const override
	{
		return observation_.externalDefuserRoundGeneration();
	}

	const ManagedCurrentWeaponObservation &observation_managedBotCurrentWeapons(std::size_t index) const override
	{
		return observation_.managedBotCurrentWeapons(index);
	}

	const perception::PerceptionAssembler &observation_managedBotPerception(std::size_t index) const override
	{
		return observation_.managedBotPerception(index);
	}

	const std::uint8_t &observation_teamInfoTeams(std::size_t index) const override
	{
		return observation_.teamInfoTeams(index);
	}

	const std::array<ObjectiveKnowledge, kObjectiveKnowledgeStateCount> &observation_objectiveKnowledge(
		std::size_t index) const override
	{
		return observation_.objectiveKnowledge(index);
	}

	const ObservationAdapter &observation_observationAdapter() const override
	{
		return observation_.observationAdapter();
	}

	bool observation_observeObjective(
		std::size_t index, edict_t *entity, bool planted, world::WorldVector *position, bool *currentlyVisible) override
	{
		return observation_.observeObjective(index, entity, planted, position, currentlyVisible);
	}

	void observation_observeRadioLeaderPosition(std::size_t index, const compat::ScenarioTarget &position) override
	{
		return observation_.observeRadioLeaderPosition(index, position);
	}

	const compat::PublicBombClock &observation_publicBombClocks(std::size_t index) const override
	{
		return observation_.publicBombClocks(index);
	}

	const compat::PublicObservationStore &observation_publicObservations() const override
	{
		return observation_.publicObservations();
	}

	const bool &observation_publicTeamKnown(std::size_t index) const override
	{
		return observation_.publicTeamKnown(index);
	}

	int observation_resolvePlayerTeam(const edict_t *entity) const override
	{
		return observation_.resolvePlayerTeam(entity);
	}

	const std::array<PublicScoreStatus, NativeBotObservation::kClientSlotCount> &observation_publicScoreStatuses(
		std::size_t index) const override
	{
		return observation_.publicScoreStatuses(index);
	}

	const compat::ScenarioRadioObservation &observation_scenarioRadio(std::size_t index) const override
	{
		return observation_.scenarioRadio(index);
	}

	const compat::SmokeObservations &observation_smokeObservations() const override
	{
		return observation_.smokeObservations();
	}

	bool behavior_buildManagedObjectiveTarget(std::size_t index, nav::NavVector *target,
		nav::NavSearchStats *searchStats, RuntimeObjectiveSelectionStats *objectiveStats) override
	{
		return behavior_.buildManagedObjectiveTarget(index, target, searchStats, objectiveStats);
	}

	const team::TeamObjectiveAssignment *behavior_findManagedTeamObjectiveAssignment(
		const world::ActorKey &actor) const override
	{
		return behavior_.findManagedTeamObjectiveAssignment(actor);
	}

	const ManagedObjectiveActionState &behavior_managedBotObjectiveActions(std::size_t index) const override
	{
		return behavior_.managedBotObjectiveActions(index);
	}

	const ManagedObjectiveTargetCache &behavior_managedBotObjectiveTargets(std::size_t index) const override
	{
		return behavior_.managedBotObjectiveTargets(index);
	}

	const compat::CompatibilityStateMachine &behavior_managedBotStateMachines(std::size_t index) const override
	{
		return behavior_.managedBotStateMachines(index);
	}

	void behavior_noteNavigationFailure(const FakeClientHandle &handle, runtime::NavRoamResult roamResult,
		const runtime::NavRoamDecision &roamDecision) override
	{
		return behavior_.noteNavigationFailure(handle, roamResult, roamDecision);
	}

	void behavior_resetBombApproach(std::size_t index) override
	{
		return behavior_.resetBombApproach(index);
	}

	const compat::ScenarioBehaviorDecision &behavior_scenarioDecisions(std::size_t index) const override
	{
		return behavior_.scenarioDecisions(index);
	}

	const bool &command_managedBotCommandTemplateValid(std::size_t index) const override
	{
		return command_.managedBotCommandTemplateValid(index);
	}

	const runtime::BotCommand &command_managedBotCommandTemplates(std::size_t index) const override
	{
		return command_.managedBotCommandTemplates(index);
	}

	void command_resetGateDiagnostics() override
	{
		return command_.resetGateDiagnostics();
	}

	runtime::MovementPhysicsState observation_captureMovementPhysicsState(
		const edict_t *entity, bool teamConfirmed, bool managedFakeClient) const override
	{
		return observation_.captureMovementPhysicsState(entity, teamConfirmed, managedFakeClient);
	}

	std::vector<nav::NavVector> observation_liveFriendlyPositions(std::size_t index, bool feet) const override
	{
		return observation_.liveFriendlyPositions(index, feet);
	}

	void behavior_applyNoiseLookCommand(std::size_t index, runtime::BotCommand &command) override
	{
		behavior_.applyNoiseLookCommand(index, command);
	}

	void behavior_applyObstacleCommand(std::size_t index, runtime::BotCommand &command) override
	{
		behavior_.applyObstacleCommand(index, command);
	}

	void behavior_observeC4AttackDispatched(
		std::size_t index, const FakeClientHandle &handle, std::uint16_t buttons) override
	{
		return behavior_.observeC4AttackDispatched(index, handle, buttons);
	}

	void behavior_observeC4SelectionDispatched(std::size_t index, const FakeClientHandle &handle) override
	{
		return behavior_.observeC4SelectionDispatched(index, handle);
	}

	void behavior_observeEquipmentDispatched(std::size_t index, std::uint16_t weapon, bool success) override
	{
		behavior_.observeEquipmentDispatched(index, weapon, success);
	}

	void behavior_resetEquipment(std::size_t index) override
	{
		behavior_.resetEquipment(index);
	}

	world::EntityKey behavior_currentEnemySource(std::size_t index) const override
	{
		return behavior_.currentEnemySource(index);
	}

	void behavior_observeEnemyWeaponFire(const edict_t *source, const world::FrameIdentity &frame, float now) override
	{
		behavior_.observeEnemyWeaponFire(source, frame, now);
	}

	void behavior_observeCloseFootstep(std::size_t index, const compat::FootstepEquipmentReceipt &receipt) override
	{
		behavior_.observeCloseFootstep(index, receipt);
	}

	void behavior_observePlayerDeath(int victimSlot, int killerSlot, edict_t *recipient, const world::FrameIdentity &frame) override
	{
		behavior_.observePlayerDeath(victimSlot, killerSlot, recipient, frame);
	}

	std::uint64_t behavior_chatterRevision(std::size_t index) const override
	{
		return behavior_.chatterRevision(index);
	}
	void behavior_observePublicDeath(std::size_t index, const world::FrameIdentity &deathFrame = {}) override
	{
		behavior_.observePublicDeath(index, deathFrame);
	}

	void behavior_observeNeutralC4AttackDispatched(std::size_t index, const FakeClientHandle &handle) override
	{
		return behavior_.observeNeutralC4AttackDispatched(index, handle);
	}

	void navigation_logMovementDiagnostic(
		std::size_t index, const char *reason, const runtime::NavRoamDecision *decision) override
	{
		return navigation_.logMovementDiagnostic(index, reason, decision);
	}

	void navigation_logMovementReversalDiagnostic(std::size_t index, const runtime::MovementPhysicsState &before,
		const runtime::NavRoamDecision &decision, const nav::LocomotionIntent &intent) override
	{
		return navigation_.logMovementReversalDiagnostic(index, before, decision, intent);
	}

	void navigation_logTraversalDiagnostic(std::size_t index, const runtime::MovementPhysicsState &before,
		const runtime::NavRoamDecision &decision, const nav::LocomotionIntent &intent, std::uint16_t buttons,
		const nav::LocomotionObservation &locomotion) override
	{
		return navigation_.logTraversalDiagnostic(index, before, decision, intent, buttons, locomotion);
	}

	void navigation_logTraversalOutcome(std::size_t index, const runtime::MovementPhysicsState &after,
		const runtime::NavRoamDecision &decision, const runtime::LandingDamageObservation &landingFeedback) override
	{
		return navigation_.logTraversalOutcome(index, after, decision, landingFeedback);
	}

	const compat::NavigationStep &navigation_managedNavigationSteps(std::size_t index) const override
	{
		return navigation_.managedNavigationSteps(index);
	}

	const std::uint8_t &navigation_movementDiagnosticSamples(std::size_t index) const override
	{
		return navigation_.movementDiagnosticSamples(index);
	}

	void navigation_noteMovementDiagnosticSample(std::size_t index) override
	{
		return navigation_.noteMovementDiagnosticSample(index);
	}

	void navigation_recordMovementPhysicsSample(
		std::size_t index, const runtime::MovementPhysicsState &before, const runtime::CommandReceipt &receipt) override
	{
		return navigation_.recordMovementPhysicsSample(index, before, receipt);
	}

	bool observation_hasObservedActiveWeapon(std::size_t index) const override
	{
		return observation_.hasObservedActiveWeapon(index);
	}

	void observation_observeReloadDispatched(world::ActorKey actor, world::FrameIdentity frame, float now) override
	{
		return observation_.observeReloadDispatched(actor, frame, now);
	}

	runtime::ActorId observation_publicPlayerIdentity(const edict_t *entity) const override
	{
		return observation_.publicPlayerIdentity(entity);
	}
};

FORCE_STACK_ALIGN void HookClientDisconnect(edict_t *entity);
FORCE_STACK_ALIGN void HookKeyValue(edict_t *entity, KeyValueData *data);
FORCE_STACK_ALIGN void HookTouch(edict_t *entity, edict_t *other);
FORCE_STACK_ALIGN void HookCmdStart(const edict_t *entity, const usercmd_t *command, unsigned int randomSeed);
FORCE_STACK_ALIGN void HookClientPutInServer(edict_t *entity);
FORCE_STACK_ALIGN void HookServerActivate(edict_t *edictList, int edictCount, int clientMax);
FORCE_STACK_ALIGN void HookServerDeactivate();
FORCE_STACK_ALIGN void HookStartFrame();
FORCE_STACK_ALIGN void HookStartFramePost();
FORCE_STACK_ALIGN void HookPmMove(playermove_t *move, int server);
FORCE_STACK_ALIGN void HookPmMovePost(playermove_t *move, int server);
FORCE_STACK_ALIGN void HookMovementSound(int channel, const char *sample, float volume, float attenuation, int flags, int pitch);
FORCE_STACK_ALIGN void HookMessageBegin(int messageDestination, int messageType, const float *origin, edict_t *entity);
FORCE_STACK_ALIGN void HookMessageEnd();
FORCE_STACK_ALIGN void HookWriteByte(int value);
FORCE_STACK_ALIGN void HookWriteChar(int value);
FORCE_STACK_ALIGN void HookWriteShort(int value);
FORCE_STACK_ALIGN void HookWriteLong(int value);
FORCE_STACK_ALIGN void HookWriteCoord(float value);
FORCE_STACK_ALIGN void HookEmitSound(
	edict_t *entity, int channel, const char *sample, float volume, float attenuation, int flags, int pitch);
FORCE_STACK_ALIGN unsigned short HookPrecacheEventPost(int type, const char *script);
FORCE_STACK_ALIGN void HookPlaybackEvent(int flags, const edict_t *invoker, unsigned short index, float delay,
	float *origin, float *angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2);
FORCE_STACK_ALIGN void HookWriteString(const char *value);
FORCE_STACK_ALIGN void HookAlertMessage(ALERT_TYPE type, char *format, ...);
FORCE_STACK_ALIGN void HookAddServerCommand(char *command, void (*function)(void));
FORCE_STACK_ALIGN void HookCompatibilityServerCommand();
const char *HookCommandArgs();
const char *HookCommandArgv(int index);
int HookCommandArgc();
} // namespace metamod
} // namespace astrabot

#endif
