#ifndef ASTRABOT_ADAPTER_METAMOD_PLUGIN_RUNTIME_HPP
#define ASTRABOT_ADAPTER_METAMOD_PLUGIN_RUNTIME_HPP
#include "bot_behavior_runtime.hpp"
#include "bot_command_runtime.hpp"
#include "bot_navigation_runtime.hpp"
#include "bot_observation_runtime.hpp"
#include "bot_runtime_types.hpp"

namespace astrabot
{
namespace metamod
{
class PluginRuntime : public BotRuntimeServices
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
	void giveEnginePointers(enginefuncs_t *engineFunctions, globalvars_t *globals);
	Snapshot snapshot() const;
	NavLoadResult loadNavigationFile(const NavLoadRequest *request);
	NavLoadDiagnostic navigationDiagnostic() const;
	nav::NavSnapshot navigationSnapshot() const;
	runtime::MovementPhysicsSample movementPhysicsSample(std::uint32_t slot) const;
	runtime::LifecycleToken tokenForSlot(std::uint32_t slot) const;

	void onClientDisconnect(edict_t *entity);
	void onTouch(edict_t *entity, edict_t *other);
	void onClientPutInServer(edict_t *entity);
	void onCmdStart(const edict_t *entity, std::uint16_t buttons);
	void onServerActivate(edict_t *edictList, int edictCount, int clientMax);
	void onServerDeactivate();
	void onStartFrame();
	void onStartFramePost();
	void notifyMenuReady(edict_t *entity, JoinMenuKind menu, std::uint16_t validSlots = 0U,
						 JoinMenuSource source = JoinMenuSource::Unknown);
	void onMessageBegin(int messageDestination, int messageType, const float *origin,
						edict_t *entity);
	void onMessageEnd();
	void onRoundLifecycleMessage(const char *message);
	bool beginRoundGeneration();
	void onWriteByte(int value);
	void onWriteChar(int value);
	void onWriteShort(int value);
	void onWriteLong(int value);
	void onWriteCoord(float value);
	void onEmitSound(edict_t *entity, const char *sample, float volume, float attenuation);
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
	runtime::CommandReceipt dispatchBotInput(const runtime::ActorId &actor,
											 std::uint32_t dispatchFrame);

  private:
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
	void updateManagedBotMovement();

	void maintainPublicBotPopulation();

	void resetManagedBotTiming(std::size_t index, float spawnTime);

	void processJoinControllers();
	void processDeferredJoinActions();
	void applyJoinAction(std::size_t index, const JoinAction &action);
	void deferJoinAction(std::size_t index, const JoinAction &action);
	void cleanupManagedJoin(std::size_t index, JoinError error);

	std::uint32_t publicPlayerPriority(const edict_t *entity) const;

		bool roundFreezeActive() const;
			void resetManagedBotMovement();

			std::size_t managedBotCount() const;
			FakeClientHandle *findManagedBot(const char *name);
			void rememberManagedBot(const FakeClientHandle &handle, const char *name);
			void clearManagedBot(std::size_t index);
			void clearManagedBots();
			CompatibilityCommandResult executeRemoveCommand(const compat::CommandAction &action);
			CompatibilityCommandResult executeKillCommand(const compat::CommandAction &action);
			static CompatibilityCommandResult
			mapConfigurationResult(compat::BotActionResult result);
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
			std::array<std::array<char, compat::ProfileRecord::kNameCapacity + 1U>,
					   NativeBotObservation::kClientSlotCount>
				managedBotNames_;
		std::array<JoinController, NativeBotObservation::kClientSlotCount>
				joinControllers_;
				DeferredJoinActionQueue pendingJoinActions_;
		std::array<bool, NativeBotObservation::kClientSlotCount> pendingJoinActionOverflow_{};

			std::array<std::uint32_t,
					   NativeBotObservation::kClientSlotCount>
				managedBotFullUpdateSequences_;

			std::array<runtime::BotTimingScheduler,
					   NativeBotObservation::kClientSlotCount>
				managedBotTiming_;

			CommandArgsFunction originalCommandArgs_;
			CommandArgvFunction originalCommandArgv_;
			CommandArgcFunction originalCommandArgc_;
			bool clientCommandContextActive_;
			int clientCommandArgumentCount_;
			std::array<char, 32U> clientCommandArgv0_;
			std::array<char, 32U> clientCommandArgv1_;
			std::array<char, 128U> clientCommandArgs_;
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
			BotObservationRuntime observation_{};
			BotObservationRuntime &observation() override
			{
				return observation_;
			}
			BotBehaviorRuntime behavior_{};
			BotBehaviorRuntime &behavior() override
			{
				return behavior_;
			}
			BotNavigationRuntime navigation_{};
			BotNavigationRuntime &navigation() override
			{
				return navigation_;
			}
			BotCommandRuntime command_{};
			BotCommandRuntime &command() override
			{
				return command_;
			}
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
			const Service_managedBotFullUpdateSequences &service_managedBotFullUpdateSequences()
				override
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
			const Service_managedBotTiming &service_managedBotTiming() override
			{
				return managedBotTiming_;
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
			Service_runtimeProfiler &service_runtimeProfiler() override
			{
				return runtimeProfiler_;
			}
};

		FORCE_STACK_ALIGN void HookClientDisconnect(edict_t *entity);
		FORCE_STACK_ALIGN void HookTouch(edict_t *entity, edict_t *other);
		FORCE_STACK_ALIGN void HookCmdStart(const edict_t *entity, const usercmd_t *command, unsigned int randomSeed);
		FORCE_STACK_ALIGN void HookClientPutInServer(edict_t *entity);
		FORCE_STACK_ALIGN void HookServerActivate(edict_t *edictList, int edictCount,
												  int clientMax);
		FORCE_STACK_ALIGN void HookServerDeactivate();
		FORCE_STACK_ALIGN void HookStartFrame();
		FORCE_STACK_ALIGN void HookStartFramePost();
		FORCE_STACK_ALIGN void HookMessageBegin(int messageDestination, int messageType,
			const float *origin, edict_t *entity);
		FORCE_STACK_ALIGN void HookMessageEnd();
		FORCE_STACK_ALIGN void HookWriteByte(int value);
		FORCE_STACK_ALIGN void HookWriteChar(int value);
		FORCE_STACK_ALIGN void HookWriteShort(int value);
		FORCE_STACK_ALIGN void HookWriteLong(int value);
		FORCE_STACK_ALIGN void HookWriteCoord(float value);
		FORCE_STACK_ALIGN void HookEmitSound(edict_t *entity, int channel, const char *sample,
			float volume, float attenuation, int flags, int pitch);
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
