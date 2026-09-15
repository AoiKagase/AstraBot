#include "plugin_runtime.hpp"

#include <algorithm>
#include <cctype>
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
		bool equalsIgnoreCase(const char *left, const char *right)
		{
			if (left == nullptr || right == nullptr)
			{
				return false;
			}
			std::size_t index = 0U;
			while (left[index] != '\0' && right[index] != '\0')
			{
				if (std::tolower(static_cast<unsigned char>(left[index])) !=
						std::tolower(static_cast<unsigned char>(right[index])))
				{
					return false;
				}
				++index;
			}
			return left[index] == '\0' && right[index] == '\0';
		}

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
		  actorRegistry_(),
		  fakeClientManager_(lifecycle_, actorRegistry_),
		  inputDispatcher_(lifecycle_, actorRegistry_),
		  compatibilitySurface_(),
		  adapterFrameCount_(0U),
		  pluginId_(nullptr),
		  nativeBotGuard_(),
		  nativeGuardDecision_({
			NativeBotGuardState::Unsupported,
			NativeBotGuardReason::ControlsUnavailable,
			false
		  }),
		  managedBotSlots_(),
		  managedBotHandles_(),
		  managedBotNames_(),
		  nativeGuardEnabled_(false),
		  compatibilityRegistrationInProgress_(false),
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
		actorRegistry_ = runtime::ActorRegistry();
		inputDispatcher_.reset();
		adapterFrameCount_ = 0U;
		compatibilityRegistrationInProgress_ = false;
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
		configureFakeClientManager();
		state_ = State::Attached;
		return true;
	}

	bool PluginRuntime::detach(PLUG_LOADTIME loadTime, PL_UNLOAD_REASON reason)
	{
		(void)loadTime;
		(void)reason;

		configureFakeClientManager();
		restoreNativeBotControls();
		inputDispatcher_.reset();
		actorRegistry_ = runtime::ActorRegistry();
		lifecycle_.deactivateMap();
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
		compatibilityRegistrationInProgress_ = false;
		configureFakeClientManager();
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
		configureFakeClientManager();
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
			const std::uint32_t clientSlot = static_cast<std::uint32_t>(slot);
			const runtime::ActorId actor = actorRegistry_.actorForSlot(clientSlot);
			if (actor.actorGeneration != 0U)
			{
				inputDispatcher_.unbindActor(actor);
				if (actorRegistry_.beginRemoval(actor) == runtime::ActorResult::Accepted)
				{
					actorRegistry_.release(actor);
				}
			}
			lifecycle_.disconnectSlot(clientSlot);
			if (slot <= static_cast<int>(NativeBotObservation::kClientSlotCount))
			{
				clearManagedBot(static_cast<std::size_t>(slot - 1));
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
				configureFakeClientManager();
				registerCompatibilityCommands();
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
			inputDispatcher_.reset();
			clearManagedBots();
			actorRegistry_ = runtime::ActorRegistry();
			state_ = State::Attached;
			compatibilityRegistrationInProgress_ = false;
			configureFakeClientManager();
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
		configureFakeClientManager();
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
		clearManagedBots();
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
		if (compatibilityRegistrationInProgress_)
		{
			setMetaResult(MRES_IGNORED);
			return;
		}
		if (nativeGuardEnabled_ && nativeBotGuard_.shouldBlockServerCommand(command))
		{
			setMetaResult(MRES_SUPERCEDE);
			return;
		}
		setMetaResult(MRES_IGNORED);
	}

	void PluginRuntime::onCompatibilityCommand()
	{
		if (engineFunctions_ == nullptr || engineFunctions_->pfnCmd_Argc == nullptr ||
				engineFunctions_->pfnCmd_Argv == nullptr)
		{
			return;
		}

		const int argumentCount = engineFunctions_->pfnCmd_Argc();
		if (argumentCount < 1 || argumentCount > 3)
		{
			return;
		}
		compat::CommandRequest request = {
			engineFunctions_->pfnCmd_Argv(0),
			static_cast<std::size_t>(argumentCount - 1),
			{nullptr, nullptr}
		};
		for (std::size_t index = 0U; index < request.argumentCount; ++index)
		{
			request.arguments[index] = engineFunctions_->pfnCmd_Argv(
				static_cast<int>(index + 1U));
		}
		(void)executeCompatibilityCommand(request);
	}

	compat::CvarUpdateResult PluginRuntime::setCompatibilityFloat(
		const char *name,
		float value)
	{
		return compatibilitySurface_.setFloat(name, value);
	}

	compat::CvarUpdateResult PluginRuntime::setCompatibilityString(
		const char *name,
		const char *value)
	{
		return compatibilitySurface_.setString(name, value);
	}

	ProfileLoadResult PluginRuntime::loadCompatibilityProfiles(const char *path)
	{
		return compatibilitySurface_.loadProfiles(path);
	}

	CompatibilityCommandResult PluginRuntime::executeCompatibilityCommand(
		const compat::CommandRequest &request)
	{
		compat::CommandAction action{};
		const compat::CommandResult commandResult =
			compatibilitySurface_.resolve(request, &action);
		if (commandResult == compat::CommandResult::Unknown)
		{
			return CompatibilityCommandResult::Unknown;
		}
		if (commandResult == compat::CommandResult::InvalidArguments)
		{
			return CompatibilityCommandResult::InvalidArguments;
		}
		if (commandResult == compat::CommandResult::InvalidOutput)
		{
			return CompatibilityCommandResult::InvalidOutput;
		}
		if (state_ != State::ActiveMap && (action.id == compat::CommandId::Add ||
				action.id == compat::CommandId::Kick || action.id == compat::CommandId::Kill))
		{
			return CompatibilityCommandResult::NotActive;
		}

		const compat::BotActionResult authorization = compatibilitySurface_.authorize(
			action, managedBotCount(), nativeGuardDecision_.managedBotCreationAllowed);
		if (authorization != compat::BotActionResult::Allowed)
		{
			return mapConfigurationResult(authorization);
		}

		switch (action.id)
		{
		case compat::CommandId::Add:
		{
			const compat::CvarSnapshot configuration = compatibilitySurface_.configuration();
			compat::ProfileRecord profile = {};
			const compat::ProfileSelectionResult profileResult =
				compatibilitySurface_.selectProfile(
					action, configuration.botDifficulty, managedBotCount(), &profile);
			if (profileResult != compat::ProfileSelectionResult::Selected &&
					profileResult != compat::ProfileSelectionResult::Fallback)
			{
				return CompatibilityCommandResult::ProfileUnavailable;
			}
			if (findManagedBot(profile.name) != nullptr)
			{
				return CompatibilityCommandResult::NameTaken;
			}
			FakeClientHandle handle{};
			return mapFakeClientResult(createFakeClient(profile.name, &handle));
		}
		case compat::CommandId::Kick:
			return executeRemoveCommand(action);
		case compat::CommandId::Kill:
			return executeKillCommand(action);
		case compat::CommandId::About:
		case compat::CommandId::KnivesOnly:
		case compat::CommandId::PistolsOnly:
		case compat::CommandId::SnipersOnly:
		case compat::CommandId::AllWeapons:
			return CompatibilityCommandResult::Handled;
		default:
			return CompatibilityCommandResult::InvalidOutput;
		}
	}

	FakeClientResult PluginRuntime::createFakeClient(const char *name, FakeClientHandle *handle)
	{
		configureFakeClientManager();
		const FakeClientResult result = fakeClientManager_.create(name, handle);
		if (result == FakeClientResult::Created && handle != nullptr &&
				handle->actor.slot >= 1U &&
				handle->actor.slot <= NativeBotObservation::kClientSlotCount)
		{
			const std::size_t slotIndex =
				static_cast<std::size_t>(handle->actor.slot - 1U);
			managedBotSlots_[slotIndex] = true;
			if (!inputDispatcher_.bindActor(handle->actor, handle->entity))
			{
				fakeClientManager_.remove(handle);
				managedBotSlots_[slotIndex] = false;
				return FakeClientResult::CleanupFailed;
			}
			rememberManagedBot(*handle, name);
		}
		return result;
	}

	FakeClientResult PluginRuntime::removeFakeClient(FakeClientHandle *handle)
	{
		if (handle == nullptr)
		{
			return FakeClientResult::NotFound;
		}

		const std::uint32_t slot = handle->actor.slot;
		inputDispatcher_.unbindActor(handle->actor);
		configureFakeClientManager();
		const FakeClientResult result = fakeClientManager_.remove(handle);
		if (result == FakeClientResult::Removed && slot >= 1U &&
				slot <= NativeBotObservation::kClientSlotCount)
		{
			clearManagedBot(static_cast<std::size_t>(slot - 1U));
		}
		return result;
	}

	runtime::QueueResult PluginRuntime::enqueueBotCommand(const runtime::BotCommand &command)
	{
		return inputDispatcher_.enqueue(command);
	}

	runtime::CommandReceipt PluginRuntime::dispatchBotInput(
		const runtime::ActorId &actor,
		std::uint32_t dispatchFrame)
	{
		return inputDispatcher_.dispatchNext(actor, dispatchFrame);
	}

	void PluginRuntime::configureFakeClientManager()
	{
		const bool allowed = state_ == State::ActiveMap &&
			nativeGuardDecision_.managedBotCreationAllowed;
		fakeClientManager_.configure(engineFunctions_, gpGamedllFuncs, allowed);
		inputDispatcher_.configure(engineFunctions_);
	}

	std::size_t PluginRuntime::managedBotCount() const
	{
		std::size_t count = 0U;
		for (const bool managed : managedBotSlots_)
		{
			if (managed)
			{
				++count;
			}
		}
		return count;
	}

	FakeClientHandle *PluginRuntime::findManagedBot(const char *name)
	{
		if (name == nullptr || name[0] == '\0')
		{
			return nullptr;
		}
		for (std::size_t index = 0U; index < managedBotSlots_.size(); ++index)
		{
			if (managedBotSlots_[index] &&
					equalsIgnoreCase(managedBotNames_[index].data(), name))
			{
				return &managedBotHandles_[index];
			}
		}
		return nullptr;
	}

	void PluginRuntime::rememberManagedBot(
		const FakeClientHandle &handle,
		const char *name)
	{
		if (name == nullptr || handle.actor.slot < 1U ||
				handle.actor.slot > NativeBotObservation::kClientSlotCount)
		{
			return;
		}
		const std::size_t index = static_cast<std::size_t>(handle.actor.slot - 1U);
		managedBotHandles_[index] = handle;
		managedBotSlots_[index] = true;
		const std::size_t length = std::strlen(name);
		const std::size_t copyLength = length < compat::ProfileRecord::kNameCapacity ?
			length : compat::ProfileRecord::kNameCapacity;
		for (std::size_t character = 0U; character < copyLength; ++character)
		{
			managedBotNames_[index][character] = name[character];
		}
		managedBotNames_[index][copyLength] = '\0';
	}

	void PluginRuntime::clearManagedBot(std::size_t index)
	{
		if (index >= managedBotSlots_.size())
		{
			return;
		}
		managedBotSlots_[index] = false;
		managedBotHandles_[index] = FakeClientHandle{};
		managedBotNames_[index].fill('\0');
	}

	void PluginRuntime::clearManagedBots()
	{
		for (std::size_t index = 0U; index < managedBotSlots_.size(); ++index)
		{
			clearManagedBot(index);
		}
	}

	CompatibilityCommandResult PluginRuntime::executeRemoveCommand(
		const compat::CommandAction &action)
	{
		const bool removeAll = action.allTargets || action.target == nullptr;
		if (!removeAll)
		{
			FakeClientHandle *handle = findManagedBot(action.target);
			if (handle == nullptr)
			{
				return CompatibilityCommandResult::NoTarget;
			}
			return mapFakeClientResult(removeFakeClient(handle));
		}

		std::size_t removedCount = 0U;
		bool operationFailed = false;
		for (std::size_t index = 0U; index < managedBotSlots_.size(); ++index)
		{
			if (!managedBotSlots_[index])
			{
				continue;
			}
			const FakeClientResult result = removeFakeClient(&managedBotHandles_[index]);
			if (result == FakeClientResult::Removed)
			{
				++removedCount;
			}
			else
			{
				operationFailed = true;
			}
		}
		if (removedCount == 0U)
		{
			return operationFailed ? CompatibilityCommandResult::ActorOperationFailed :
				CompatibilityCommandResult::NoTarget;
		}
		return operationFailed ? CompatibilityCommandResult::ActorOperationFailed :
			CompatibilityCommandResult::Handled;
	}

	CompatibilityCommandResult PluginRuntime::executeKillCommand(
		const compat::CommandAction &action)
	{
		const bool killAll = action.allTargets || action.target == nullptr;
		if (!killAll)
		{
			FakeClientHandle *handle = findManagedBot(action.target);
			if (handle == nullptr)
			{
				return CompatibilityCommandResult::NoTarget;
			}
			return mapFakeClientResult(fakeClientManager_.kill(handle));
		}

		std::size_t killedCount = 0U;
		bool operationFailed = false;
		for (std::size_t index = 0U; index < managedBotSlots_.size(); ++index)
		{
			if (!managedBotSlots_[index])
			{
				continue;
			}
			const FakeClientResult result = fakeClientManager_.kill(
				&managedBotHandles_[index]);
			if (result == FakeClientResult::Killed)
			{
				++killedCount;
			}
			else
			{
				operationFailed = true;
			}
		}
		if (killedCount == 0U)
		{
			return operationFailed ? CompatibilityCommandResult::ActorOperationFailed :
				CompatibilityCommandResult::NoTarget;
		}
		return operationFailed ? CompatibilityCommandResult::ActorOperationFailed :
			CompatibilityCommandResult::Handled;
	}

	CompatibilityCommandResult PluginRuntime::mapConfigurationResult(
		compat::BotActionResult result)
	{
		switch (result)
		{
		case compat::BotActionResult::Allowed:
			return CompatibilityCommandResult::Handled;
		case compat::BotActionResult::InvalidRequest:
			return CompatibilityCommandResult::InvalidOutput;
		case compat::BotActionResult::Disabled:
			return CompatibilityCommandResult::Disabled;
		case compat::BotActionResult::Stopped:
			return CompatibilityCommandResult::Stopped;
		case compat::BotActionResult::QuotaReached:
			return CompatibilityCommandResult::QuotaReached;
		case compat::BotActionResult::NoTarget:
			return CompatibilityCommandResult::NoTarget;
		case compat::BotActionResult::NativeGuardDenied:
			return CompatibilityCommandResult::NativeGuardDenied;
		default:
			return CompatibilityCommandResult::InvalidOutput;
		}
	}

	CompatibilityCommandResult PluginRuntime::mapFakeClientResult(
		FakeClientResult result)
	{
		switch (result)
		{
		case FakeClientResult::Created:
		case FakeClientResult::Removed:
		case FakeClientResult::Killed:
			return CompatibilityCommandResult::Handled;
		case FakeClientResult::NativeGuardDenied:
			return CompatibilityCommandResult::NativeGuardDenied;
		case FakeClientResult::NotFound:
			return CompatibilityCommandResult::NoTarget;
		default:
			return CompatibilityCommandResult::ActorOperationFailed;
		}
	}

	void PluginRuntime::registerCompatibilityCommands()
	{
		if (engineFunctions_ == nullptr || engineFunctions_->pfnAddServerCommand == nullptr)
		{
			return;
		}

		compatibilityRegistrationInProgress_ = true;
		compatibilitySurface_.registerCommands(engineFunctions_, &HookCompatibilityServerCommand);
		compatibilityRegistrationInProgress_ = false;
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

	void HookCompatibilityServerCommand()
	{
		PluginRuntime::instance().onCompatibilityCommand();
	}
}
}
