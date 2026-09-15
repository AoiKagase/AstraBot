#include "plugin_runtime.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <cstring>

meta_globals_t *gpMetaGlobals = nullptr;
gamedll_funcs_t *gpGamedllFuncs = nullptr;
mutil_funcs_t *gpMetaUtilFuncs = nullptr;

namespace astrabot
{
namespace metamod
{
	namespace
	{
		void setMetaResult(META_RES result)
		{
			if (gpMetaGlobals != nullptr)
			{
				gpMetaGlobals->mres = result;
			}
		}
	}

	PluginRuntime::PluginRuntime()
		: state_(State::Cold),
		  metaGlobals_(nullptr),
		  gameDllFunctions_(nullptr),
		  engineFunctions_(nullptr),
		  globals_(nullptr),
		  lifecycle_(),
		  adapterFrameCount_(0U),
		  pluginId_(nullptr),
		  nativeBotGuard_(),
		  nativeGuardDecision_({
			NativeBotGuardState::Unsupported,
			NativeBotGuardReason::ControlsUnavailable,
			false
		}),
		  managedBotSlots_(),
		  nativeGuardEnabled_(false),
		  nativeControlsCaptured_(false),
		  originalBotEnable_(0.0f),
		  originalBotQuota_(0.0f)
	{
	}

	PluginRuntime &PluginRuntime::instance()
	{
		static PluginRuntime runtime;
		return runtime;
	}

	bool PluginRuntime::attach(
		PLUG_LOADTIME loadTime,
		META_FUNCTIONS *functionTable,
		meta_globals_t *metaGlobals,
		gamedll_funcs_t *gameDllFunctions,
		plid_t pluginId)
	{
		(void)loadTime;

		if (functionTable == nullptr || metaGlobals == nullptr || gameDllFunctions == nullptr)
		{
			return false;
		}

		if (state_ == State::Attached || state_ == State::ActiveMap)
		{
			return false;
		}

		META_FUNCTIONS requestedFunctions{};
		requestedFunctions.pfnGetEntityAPI2 = &GetEntityAPI2;
		requestedFunctions.pfnGetEngineFunctions = &GetEngineFunctions;
		*functionTable = requestedFunctions;

		metaGlobals_ = metaGlobals;
		gameDllFunctions_ = gameDllFunctions;
		engineFunctions_ = nullptr;
		globals_ = nullptr;
		lifecycle_ = runtime::LifecycleSession();
		adapterFrameCount_ = 0U;
		pluginId_ = pluginId;
		resetNativeBotGuard();
		gpMetaGlobals = metaGlobals;
		gpGamedllFuncs = gameDllFunctions;
		nativeGuardEnabled_ = true;
		if (gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnGetHookTables != nullptr)
		{
			enginefuncs_t *hookedEngineFunctions = nullptr;
			DLL_FUNCTIONS *hookedDllFunctions = nullptr;
			NEW_DLL_FUNCTIONS *hookedNewDllFunctions = nullptr;
			gpMetaUtilFuncs->pfnGetHookTables(
				pluginId, &hookedEngineFunctions, &hookedDllFunctions, &hookedNewDllFunctions);
			engineFunctions_ = hookedEngineFunctions;
		}
		state_ = State::Attached;
		return true;
	}

	bool PluginRuntime::detach(PLUG_LOADTIME loadTime, PL_UNLOAD_REASON reason)
	{
		(void)loadTime;
		(void)reason;

		restoreNativeBotControls();
		state_ = State::Detached;
		metaGlobals_ = nullptr;
		gameDllFunctions_ = nullptr;
		engineFunctions_ = nullptr;
		globals_ = nullptr;
		lifecycle_.deactivateMap();
		adapterFrameCount_ = 0U;
		gpMetaGlobals = nullptr;
		gpGamedllFuncs = nullptr;
		gpMetaUtilFuncs = nullptr;
		pluginId_ = nullptr;
		nativeGuardEnabled_ = false;
		return true;
	}

	bool PluginRuntime::provideEntityApi(DLL_FUNCTIONS *functionTable, int *interfaceVersion)
	{
		if (interfaceVersion == nullptr)
		{
			return false;
		}

		if (*interfaceVersion != INTERFACE_VERSION)
		{
			*interfaceVersion = INTERFACE_VERSION;
			return false;
		}

		if (functionTable == nullptr || state_ == State::Cold || state_ == State::Detached)
		{
			return false;
		}

		DLL_FUNCTIONS requestedFunctions{};
		requestedFunctions.pfnClientDisconnect = &HookClientDisconnect;
		requestedFunctions.pfnClientPutInServer = &HookClientPutInServer;
		requestedFunctions.pfnServerActivate = &HookServerActivate;
		requestedFunctions.pfnServerDeactivate = &HookServerDeactivate;
		requestedFunctions.pfnStartFrame = &HookStartFrame;
		*functionTable = requestedFunctions;
		return true;
	}

	bool PluginRuntime::provideEngineFunctions(enginefuncs_t *engineFunctions, int *interfaceVersion)
	{
		if (interfaceVersion == nullptr)
		{
			return false;
		}

		if (*interfaceVersion != ENGINE_INTERFACE_VERSION)
		{
			*interfaceVersion = ENGINE_INTERFACE_VERSION;
			return false;
		}

		if (engineFunctions == nullptr || state_ == State::Cold || state_ == State::Detached)
		{
			return false;
		}

		enginefuncs_t requestedFunctions{};
		requestedFunctions.pfnAddServerCommand = &HookAddServerCommand;
		*engineFunctions = requestedFunctions;
		return true;
	}

	void PluginRuntime::giveEnginePointers(enginefuncs_t *engineFunctions, globalvars_t *globals)
	{
		engineFunctions_ = engineFunctions;
		globals_ = globals;
	}

	PluginRuntime::Snapshot PluginRuntime::snapshot() const
	{
		Snapshot current = {
			state_,
			lifecycle_.mapGeneration(),
			lifecycle_.roundGeneration(),
			nativeGuardDecision_.state,
			nativeGuardDecision_.reason,
			nativeGuardDecision_.managedBotCreationAllowed
		};
		return current;
	}

	runtime::LifecycleToken PluginRuntime::tokenForSlot(std::uint32_t slot) const
	{
		return lifecycle_.tokenForSlot(slot);
	}

	void PluginRuntime::onClientDisconnect(edict_t *entity)
	{
		if (state_ != State::ActiveMap || entity == nullptr ||
				engineFunctions_ == nullptr || engineFunctions_->pfnIndexOfEdict == nullptr)
		{
			return;
		}

		const int slot = engineFunctions_->pfnIndexOfEdict(entity);
		if (slot > 0)
		{
			lifecycle_.disconnectSlot(static_cast<std::uint32_t>(slot));
			if (slot <= static_cast<int>(NativeBotObservation::kClientSlotCount))
			{
				managedBotSlots_[static_cast<std::size_t>(slot - 1)] = false;
			}
		}
	}

	void PluginRuntime::onClientPutInServer(edict_t *entity)
	{
		if (state_ != State::ActiveMap || entity == nullptr ||
				engineFunctions_ == nullptr || engineFunctions_->pfnIndexOfEdict == nullptr)
		{
			return;
		}

		const int slot = engineFunctions_->pfnIndexOfEdict(entity);
		if (slot > 0)
		{
			lifecycle_.connectSlot(static_cast<std::uint32_t>(slot));
		}
	}

	void PluginRuntime::onServerActivate(edict_t *edictList, int edictCount, int clientMax)
	{
		(void)edictList;
		(void)edictCount;
		(void)clientMax;

		if (state_ == State::Attached)
		{
			if (lifecycle_.activateMap())
			{
				adapterFrameCount_ = 0U;
				state_ = State::ActiveMap;
				armNativeBotGuard();
			}
		}
	}

	void PluginRuntime::onServerDeactivate()
	{
		if (state_ == State::ActiveMap)
		{
			lifecycle_.deactivateMap();
			adapterFrameCount_ = 0U;
			updateNativeBotGuard();
			state_ = State::Attached;
		}
	}

	void PluginRuntime::onStartFrame()
	{
		if (state_ != State::ActiveMap)
		{
			return;
		}
		if (globals_ == nullptr || !std::isfinite(globals_->time))
		{
			return;
		}
		if (adapterFrameCount_ == (std::numeric_limits<std::uint32_t>::max)())
		{
			adapterFrameCount_ = 0U;
		}
		else
		{
			++adapterFrameCount_;
		}
		lifecycle_.observeFrame(adapterFrameCount_, globals_->time);
		updateNativeBotGuard();
	}

	NativeBotObservation PluginRuntime::collectNativeBotObservation() const
	{
		NativeBotObservation observation{};
		if (engineFunctions_ == nullptr ||
				engineFunctions_->pfnCVarGetPointer == nullptr ||
				engineFunctions_->pfnCVarGetFloat == nullptr ||
				engineFunctions_->pfnCVarSetFloat == nullptr ||
				engineFunctions_->pfnPEntityOfEntIndex == nullptr)
		{
			return observation;
		}

		if (engineFunctions_->pfnCVarGetPointer("bot_enable") == nullptr ||
				engineFunctions_->pfnCVarGetPointer("bot_quota") == nullptr)
		{
			return observation;
		}

		observation.controlsAvailable = true;
		observation.controlsWritable = true;
		observation.suppressionApplied = false;
		observation.botEnable = engineFunctions_->pfnCVarGetFloat("bot_enable");
		observation.botQuota = engineFunctions_->pfnCVarGetFloat("bot_quota");
		observation.managedClientSlots = managedBotSlots_;

		int clientMax = static_cast<int>(NativeBotObservation::kClientSlotCount);
		if (globals_ != nullptr && globals_->maxClients > 0)
		{
			clientMax = (std::min)(
				globals_->maxClients,
				static_cast<int>(NativeBotObservation::kClientSlotCount));
		}
		for (int slot = 1; slot <= clientMax; ++slot)
		{
			edict_t *entity = engineFunctions_->pfnPEntityOfEntIndex(slot);
			if (entity != nullptr && !entity->free && (entity->v.flags & FL_FAKECLIENT) != 0)
			{
				observation.fakeClientSlots[static_cast<std::size_t>(slot - 1)] = true;
			}
		}
		return observation;
	}

	void PluginRuntime::updateNativeBotGuard()
	{
		const NativeBotGuardDecision previousDecision = nativeGuardDecision_;
		NativeBotObservation observation = collectNativeBotObservation();
		observation.suppressionApplied = nativeGuardEnabled_ && nativeControlsCaptured_ &&
			observation.controlsAvailable && observation.controlsWritable &&
			observation.botEnable == 0.0f && observation.botQuota == 0.0f;
			nativeGuardDecision_ = nativeBotGuard_.evaluate(observation);
		logNativeBotGuardTransition(previousDecision);
	}

	bool PluginRuntime::armNativeBotGuard()
	{
		NativeBotObservation observation = collectNativeBotObservation();
		if (!observation.controlsAvailable || !observation.controlsWritable)
		{
			nativeGuardDecision_ = nativeBotGuard_.evaluate(observation);
			return false;
		}

		if (!nativeControlsCaptured_)
		{
			originalBotEnable_ = observation.botEnable;
			originalBotQuota_ = observation.botQuota;
			nativeControlsCaptured_ = true;
		}
		engineFunctions_->pfnCVarSetFloat("bot_enable", 0.0f);
		engineFunctions_->pfnCVarSetFloat("bot_quota", 0.0f);
		nativeGuardEnabled_ = true;
		updateNativeBotGuard();
		return nativeGuardDecision_.managedBotCreationAllowed;
	}

	void PluginRuntime::resetNativeBotGuard()
	{
		nativeGuardDecision_ = {
			NativeBotGuardState::Unsupported,
			NativeBotGuardReason::ControlsUnavailable,
			false
		};
		managedBotSlots_.fill(false);
		nativeGuardEnabled_ = false;
		nativeControlsCaptured_ = false;
		originalBotEnable_ = 0.0f;
		originalBotQuota_ = 0.0f;
	}

	void PluginRuntime::restoreNativeBotControls()
	{
		if (nativeControlsCaptured_ && engineFunctions_ != nullptr &&
				engineFunctions_->pfnCVarSetFloat != nullptr)
		{
			engineFunctions_->pfnCVarSetFloat("bot_enable", originalBotEnable_);
			engineFunctions_->pfnCVarSetFloat("bot_quota", originalBotQuota_);
		}
		resetNativeBotGuard();
	}

	void PluginRuntime::logNativeBotGuardTransition(
		const NativeBotGuardDecision &previousDecision) const
	{
		if (gpMetaUtilFuncs == nullptr || gpMetaUtilFuncs->pfnLogError == nullptr ||
				pluginId_ == nullptr)
		{
			return;
		}

		if (previousDecision.state == nativeGuardDecision_.state &&
				previousDecision.reason == nativeGuardDecision_.reason &&
				previousDecision.managedBotCreationAllowed ==
					nativeGuardDecision_.managedBotCreationAllowed)
		{
			return;
		}
		if (nativeGuardDecision_.state == NativeBotGuardState::Clean ||
				nativeGuardDecision_.state == NativeBotGuardState::Suppressed)
		{
			return;
		}

		const char *state = nativeGuardDecision_.state == NativeBotGuardState::Conflict ?
			"conflict" : "unsupported";
		const char *reason = "unknown";
		switch (nativeGuardDecision_.reason)
		{
		case NativeBotGuardReason::ControlsUnavailable:
			reason = "controls_unavailable";
			break;
		case NativeBotGuardReason::InvalidObservation:
			reason = "invalid_observation";
			break;
		case NativeBotGuardReason::NativeControlsActive:
			reason = "native_controls_active";
			break;
		case NativeBotGuardReason::UnmanagedFakeClient:
			reason = "unmanaged_fakeclient";
			break;
		case NativeBotGuardReason::None:
			break;
		}
		gpMetaUtilFuncs->pfnLogError(
				pluginId_,
				"native guard state=%s reason=%s map=%u round=%u frame=%u",
				state,
				reason,
				lifecycle_.mapGeneration(),
				lifecycle_.roundGeneration(),
				adapterFrameCount_);
	}

	void PluginRuntime::onAddServerCommand(char *command, void (*function)(void))
	{
		(void)function;
		if (nativeGuardEnabled_ && nativeBotGuard_.shouldBlockServerCommand(command))
		{
			setMetaResult(MRES_SUPERCEDE);
			return;
		}
		setMetaResult(MRES_IGNORED);
	}

	void HookClientDisconnect(edict_t *entity)
	{
		PluginRuntime::instance().onClientDisconnect(entity);
		setMetaResult(MRES_IGNORED);
	}

	void HookClientPutInServer(edict_t *entity)
	{
		PluginRuntime::instance().onClientPutInServer(entity);
		setMetaResult(MRES_IGNORED);
	}

	void HookServerActivate(edict_t *edictList, int edictCount, int clientMax)
	{
		PluginRuntime::instance().onServerActivate(edictList, edictCount, clientMax);
		setMetaResult(MRES_IGNORED);
	}

	void HookServerDeactivate()
	{
		PluginRuntime::instance().onServerDeactivate();
		setMetaResult(MRES_IGNORED);
	}

	void HookStartFrame()
	{
		PluginRuntime::instance().onStartFrame();
		setMetaResult(MRES_IGNORED);
	}

	void HookAddServerCommand(char *command, void (*function)(void))
	{
		PluginRuntime::instance().onAddServerCommand(command, function);
	}
}
}
