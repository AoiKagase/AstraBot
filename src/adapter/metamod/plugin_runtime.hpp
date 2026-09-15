#ifndef ASTRABOT_ADAPTER_METAMOD_PLUGIN_RUNTIME_HPP
#define ASTRABOT_ADAPTER_METAMOD_PLUGIN_RUNTIME_HPP

#include "astrabot/metamod/compat_surface.hpp"
#include "astrabot/metamod/abi_contract.hpp"
#include "astrabot/metamod/fake_client_manager.hpp"
#include "astrabot/metamod/input_dispatcher.hpp"
#include "astrabot/metamod/native_bot_guard.hpp"
#include "astrabot/runtime/lifecycle.hpp"

#include <array>
#include <cstdint>

namespace astrabot
{
namespace metamod
{
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
			runtime::LifecycleGeneration mapGeneration;
			runtime::LifecycleGeneration roundGeneration;
			NativeBotGuardState nativeGuardState;
			NativeBotGuardReason nativeGuardReason;
			bool managedBotCreationAllowed;
		};

		static PluginRuntime &instance();

		bool attach(
			PLUG_LOADTIME loadTime,
			META_FUNCTIONS *functionTable,
			meta_globals_t *metaGlobals,
			gamedll_funcs_t *gameDllFunctions,
			plid_t pluginId);
		bool detach(PLUG_LOADTIME loadTime, PL_UNLOAD_REASON reason);
		bool provideEntityApi(DLL_FUNCTIONS *functionTable, int *interfaceVersion);
		bool provideEngineFunctions(enginefuncs_t *engineFunctions, int *interfaceVersion);
		void giveEnginePointers(enginefuncs_t *engineFunctions, globalvars_t *globals);
		Snapshot snapshot() const;
		runtime::LifecycleToken tokenForSlot(std::uint32_t slot) const;

		void onClientDisconnect(edict_t *entity);
		void onClientPutInServer(edict_t *entity);
		void onServerActivate(edict_t *edictList, int edictCount, int clientMax);
		void onServerDeactivate();
		void onStartFrame();
		void onAddServerCommand(char *command, void (*function)(void));
		void onCompatibilityCommand();
		FakeClientResult createFakeClient(const char *name, FakeClientHandle *handle);
		FakeClientResult removeFakeClient(FakeClientHandle *handle);
		runtime::QueueResult enqueueBotCommand(const runtime::BotCommand &command);
		runtime::CommandReceipt dispatchBotInput(
			const runtime::ActorId &actor,
			std::uint32_t dispatchFrame);

	private:
		PluginRuntime();
		NativeBotObservation collectNativeBotObservation() const;
		void updateNativeBotGuard();
		bool armNativeBotGuard();
		void resetNativeBotGuard();
		void restoreNativeBotControls();
		void logNativeBotGuardTransition(
			const NativeBotGuardDecision &previousDecision) const;
		void configureFakeClientManager();
		void registerCompatibilityCommands();

		State state_;
		meta_globals_t *metaGlobals_;
		gamedll_funcs_t *gameDllFunctions_;
		enginefuncs_t *engineFunctions_;
		globalvars_t *globals_;
		runtime::LifecycleSession lifecycle_;
		runtime::ActorRegistry actorRegistry_;
		FakeClientManager fakeClientManager_;
		InputDispatcher inputDispatcher_;
		CompatibilitySurface compatibilitySurface_;
		std::uint32_t adapterFrameCount_;
		plid_t pluginId_;
		NativeBotGuard nativeBotGuard_;
		NativeBotGuardDecision nativeGuardDecision_;
		std::array<bool, NativeBotObservation::kClientSlotCount> managedBotSlots_;
		bool nativeGuardEnabled_;
		bool compatibilityRegistrationInProgress_;
		bool nativeControlsCaptured_;
		float originalBotEnable_;
		float originalBotQuota_;
	};

	FORCE_STACK_ALIGN void HookClientDisconnect(edict_t *entity);
	FORCE_STACK_ALIGN void HookClientPutInServer(edict_t *entity);
	FORCE_STACK_ALIGN void HookServerActivate(edict_t *edictList, int edictCount, int clientMax);
	FORCE_STACK_ALIGN void HookServerDeactivate();
	FORCE_STACK_ALIGN void HookStartFrame();
	FORCE_STACK_ALIGN void HookAddServerCommand(char *command, void (*function)(void));
	FORCE_STACK_ALIGN void HookCompatibilityServerCommand();
}
}

#endif
