#ifndef ASTRABOT_ADAPTER_METAMOD_PLUGIN_RUNTIME_HPP
#define ASTRABOT_ADAPTER_METAMOD_PLUGIN_RUNTIME_HPP

#include "astrabot/metamod/abi_contract.hpp"
#include "astrabot/metamod/compat_surface.hpp"
#include "astrabot/metamod/fake_client_manager.hpp"
#include "astrabot/metamod/join_controller.hpp"
#include "astrabot/metamod/input_dispatcher.hpp"
#include "astrabot/metamod/native_bot_guard.hpp"
#include "astrabot/metamod/nav_loader.hpp"
#include "astrabot/combat/combat_intent.hpp"
#include "astrabot/compat/state_machine.hpp"
#include "astrabot/objectives/round_objectives.hpp"
#include "astrabot/perception/perception.hpp"
#include "astrabot/runtime/nav_roam_controller.hpp"
#include "astrabot/runtime/movement_physics.hpp"
#include "astrabot/runtime/bot_timing_scheduler.hpp"
#include "astrabot/runtime/lifecycle.hpp"
#include "engine_random_source.hpp"
#include "action_adapter.hpp"
#include "observation_adapter.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace astrabot
{
	namespace metamod
	{
		enum class CompatibilityCommandResult
		{
			Handled,
			Unknown,
			InvalidArguments,
			InvalidOutput,
			NotActive,
			Disabled,
			Stopped,
			QuotaReached,
			NativeGuardDenied,
			NoTarget,
			ProfileUnavailable,
			NameTaken,
			ActorOperationFailed
		};

		class PluginRuntime
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
			};

			static PluginRuntime &instance();

			bool attach(PLUG_LOADTIME loadTime, META_FUNCTIONS *functionTable,
						meta_globals_t *metaGlobals, gamedll_funcs_t *gameDllFunctions,
						plid_t pluginId);
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
			void onClientPutInServer(edict_t *entity);
			void onServerActivate(edict_t *edictList, int edictCount, int clientMax);
			void onServerDeactivate();
			void onStartFrame();
			void onStartFramePost();
		void notifyMenuReady(edict_t *entity, JoinMenuKind menu,
							 std::uint16_t validSlots = 0U,
							 JoinMenuSource source = JoinMenuSource::Unknown);
			void onMessageBegin(int messageDestination, int messageType,
							const float *origin, edict_t *entity);
		void onMessageEnd();
		void onWriteByte(int value);
		void onWriteChar(int value);
		void onWriteShort(int value);
		void onWriteString(const char *value);
			void onAddServerCommand(char *command, void (*function)(void));
			void onCompatibilityCommand();
			compat::CvarUpdateResult setCompatibilityFloat(const char *name, float value);
			compat::CvarUpdateResult setCompatibilityString(const char *name, const char *value);
			ProfileLoadResult loadCompatibilityProfiles(const char *path);
			CompatibilityCommandResult
			executeCompatibilityCommand(const compat::CommandRequest &request);
			FakeClientResult createFakeClient(const char *name, FakeClientHandle *handle);
			FakeClientResult removeFakeClient(FakeClientHandle *handle);
			runtime::QueueResult enqueueBotCommand(const runtime::BotCommand &command);
			runtime::CommandReceipt dispatchBotInput(const runtime::ActorId &actor,
													 std::uint32_t dispatchFrame);

		  private:
		static constexpr std::size_t kCompatibilityCvarCount = 6U;

		enum class UserMessageKind : std::uint8_t
		{
			None,
			ShowMenu,
			VguiMenu,
			TeamInfo
		};

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
			void resetManagedBotTiming(std::size_t index, float spawnTime);
			void resetManagedBotCommandTemplate(std::size_t index);
			void updateManagedBotCompatibilityState(
				std::size_t index,
				const runtime::MovementPhysicsState &before);
			compat::StateUpdateContext makeManagedBotStateContext(
				std::size_t index,
				const runtime::MovementPhysicsState &before) const;
			void prepareNeutralManagedBotCommand(
				std::size_t index,
				FakeClientHandle &handle,
				const runtime::MovementPhysicsState &before);
			runtime::CommandReceipt executeManagedBotCommand(
				std::size_t index,
				FakeClientHandle &handle);
		void processJoinControllers();
		void applyJoinAction(std::size_t index, const JoinAction &action);
		void cleanupManagedJoin(std::size_t index, JoinError error);
		void notifyTeamInfo(std::uint8_t slot, const char *teamName);
	runtime::MovementPhysicsState captureMovementPhysicsState(
		const edict_t *entity, bool teamConfirmed, bool managedFakeClient) const;
		runtime::CommandReceipt dispatchNeutralMovement(
			std::size_t index,
			FakeClientHandle &handle,
			std::uint8_t milliseconds);
		runtime::CommandReceipt dispatchJoinHeartbeat(
			std::size_t index,
			FakeClientHandle &handle,
			std::uint8_t milliseconds);
			void recordMovementPhysicsSample(
				std::size_t index,
				const runtime::MovementPhysicsState &before,
				const runtime::CommandReceipt &receipt);
			ActionProposal decideManagedBotAction(
				std::size_t index,
				const runtime::MovementPhysicsState &before,
				const runtime::ViewAngles &movementAngles,
				std::uint16_t movementButtons);
			bool buildManagedWorldSnapshot(
				std::size_t index,
				world::WorldSnapshot *snapshot,
				world::ActorKey *targetActor,
				world::WorldPosition *targetPosition) const;
			bool buildManagedObjectiveTarget(
				std::size_t index,
				nav::NavVector *target) const;
			void resetManagedBotMovement();
			void logMovementDiagnostic(
				std::size_t index,
				const char *reason,
				const runtime::NavRoamDecision *decision = nullptr);
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
			runtime::LifecycleSession lifecycle_;
			runtime::ActorRegistry actorRegistry_;
			FakeClientManager fakeClientManager_;
			InputDispatcher inputDispatcher_;
			CompatibilitySurface compatibilitySurface_;
			compat::EngineRandomSource compatibilityRandomSource_;
			ObservationAdapter observationAdapter_;
			NavLoader navLoader_;
			nav::NavSnapshotPublisher navPublisher_;
			NavLoadDiagnostic navLoadDiagnostic_;
			std::uint32_t adapterFrameCount_;
			plid_t pluginId_;
			NativeBotGuard nativeBotGuard_;
			NativeBotGuardDecision nativeGuardDecision_;
			std::array<bool, NativeBotObservation::kClientSlotCount> managedBotSlots_;
			std::array<std::uint8_t, NativeBotObservation::kClientSlotCount> managedBotTeamNumbers_;
			std::array<FakeClientHandle, NativeBotObservation::kClientSlotCount> managedBotHandles_;
			std::array<std::array<char, compat::ProfileRecord::kNameCapacity + 1U>,
					   NativeBotObservation::kClientSlotCount>
				managedBotNames_;
		std::array<JoinController, NativeBotObservation::kClientSlotCount>
				joinControllers_;
		UserMessageKind userMessageKind_;
		edict_t *userMessageTarget_;
		std::uint8_t userMessageFieldCount_;
		std::uint8_t userMessageMenuType_;
		std::uint8_t userMessageNeedMore_;
		std::uint16_t userMessageValidSlots_;
		std::uint8_t userMessageTeamSlot_;
		bool userMessageShowFragmentActive_;
		std::array<char, 257U> userMessageText_;
		std::uint16_t userMessageTextLength_;
			std::array<runtime::NavRoamController,
					   NativeBotObservation::kClientSlotCount>
					managedBotMovement_;
			std::array<combat::CombatController,
					   NativeBotObservation::kClientSlotCount>
					managedBotCombat_;
			std::array<objectives::RoundObjectivePlanner,
					   NativeBotObservation::kClientSlotCount>
					managedBotObjectives_;
			std::array<compat::CompatibilityStateMachine,
					   NativeBotObservation::kClientSlotCount>
				managedBotStateMachines_;
			std::array<std::uint32_t,
					   NativeBotObservation::kClientSlotCount>
				managedBotCommandSequences_;
			std::array<std::uint32_t,
					   NativeBotObservation::kClientSlotCount>
				managedBotFullUpdateSequences_;
			std::array<runtime::LifecycleGeneration,
					   NativeBotObservation::kClientSlotCount>
				managedBotStateRounds_;
			std::array<bool, NativeBotObservation::kClientSlotCount>
				managedBotWasDead_;
			std::array<runtime::BotTimingScheduler,
					   NativeBotObservation::kClientSlotCount>
				managedBotTiming_;
			std::array<runtime::BotCommand,
					   NativeBotObservation::kClientSlotCount>
				managedBotCommandTemplates_;
			std::array<bool, NativeBotObservation::kClientSlotCount>
				managedBotCommandTemplateValid_;
			std::array<std::uint8_t,
					   NativeBotObservation::kClientSlotCount>
				movementDiagnosticSamples_;
			std::array<bool,
					   NativeBotObservation::kClientSlotCount>
				movementDiagnosticAttempts_;
			std::array<bool,
					   NativeBotObservation::kClientSlotCount>
				movementDiagnosticUnavailable_;
			std::array<std::uint32_t,
					   NativeBotObservation::kClientSlotCount>
				movementResumeFrames_;
			std::array<std::uint32_t,
					   NativeBotObservation::kClientSlotCount>
				movementLastDeadFrames_;
			std::array<std::uint32_t,
					   NativeBotObservation::kClientSlotCount>
				movementSettledDeadFrames_;
		std::array<std::uint8_t,
					   NativeBotObservation::kClientSlotCount>
			movementWarmupFrames_;
		std::array<runtime::MovementPhysicsSample,
			NativeBotObservation::kClientSlotCount>
			movementPhysicsSamples_;
		std::array<bool, NativeBotObservation::kClientSlotCount>
			movementReadyLogged_;
		std::array<std::uint32_t,
					   NativeBotObservation::kClientSlotCount>
			movementDispatchFrames_;
		std::array<bool, NativeBotObservation::kClientSlotCount>
			movementWasAirborne_;
			std::uint32_t movementDiagnosticRound_;
			bool movementDiagnosticGlobal_;
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
			float originalBotEnable_;
			float originalBotQuota_;
		};

		FORCE_STACK_ALIGN void HookClientDisconnect(edict_t *entity);
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
		FORCE_STACK_ALIGN void HookWriteString(const char *value);
		FORCE_STACK_ALIGN void HookAddServerCommand(char *command, void (*function)(void));
		FORCE_STACK_ALIGN void HookCompatibilityServerCommand();
		const char *HookCommandArgs();
		const char *HookCommandArgv(int index);
		int HookCommandArgc();
	} // namespace metamod
} // namespace astrabot

#endif
