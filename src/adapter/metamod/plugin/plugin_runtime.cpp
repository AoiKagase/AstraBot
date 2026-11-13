#include "plugin/plugin_runtime.hpp"
#include "astrabot/compat/bomb_navigation.hpp"
#include "astrabot/metamod/movement_execution_gate.hpp"
#include "astrabot/objectives/bomb_site_route_selection.hpp"
#include "runtime/shared/bot_runtime_support.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdarg>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <limits>
#include <vector>

meta_globals_t *gpMetaGlobals = nullptr;
gamedll_funcs_t *gpGamedllFuncs = nullptr;
mutil_funcs_t *gpMetaUtilFuncs = nullptr;

namespace astrabot
{
	namespace metamod
	{
	using namespace runtime_detail;
	namespace
	{

	// Counter-Strike 1.6/ReHLDS registers these user messages before the
	// plugin utility lookup is available.  Keep the public lookup as the
	// first choice, but retain the engine's stable IDs as a bounded fallback.

	enum class CompatibilityCvarIndex : std::size_t
	{
		Enable,
		Stop,
		Difficulty,
		Quota,
		JoinTeam,
		Mode,
		Profile
			};

			char kBotEnableName[] = "bot_enable";
			char kBotEnableDefault[] = "0";
			char kBotStopName[] = "bot_stop";
			char kBotStopDefault[] = "0";
			char kBotDifficultyName[] = "bot_difficulty";
			char kBotDifficultyDefault[] = "0";
			char kBotQuotaName[] = "bot_quota";
			char kBotQuotaDefault[] = "0";
			char kBotJoinTeamName[] = "bot_join_team";
			char kBotJoinTeamDefault[] = "any";
	char kAstrabotModeName[] = "astrabot_mode";
	char kAstrabotModeDefault[] = "compatibility";
char kAstrabotProfileName[] = "astrabot_profile";
char kAstrabotProfileDefault[] = "0";
char kAstrabotPerfDisableVisionName[] = "astrabot_perf_disable_vision";
char kAstrabotPerfDisableVisionDefault[] = "0";
char kAstrabotPerfDisablePathSearchName[] = "astrabot_perf_disable_pathsearch";
char kAstrabotPerfDisablePathSearchDefault[] = "0";
char kAstrabotPerfDisableTraceName[] = "astrabot_perf_disable_trace";
char kAstrabotPerfDisableTraceDefault[] = "0";

			cvar_t kBotEnableCvar = {kBotEnableName, kBotEnableDefault, FCVAR_SERVER, 0.0f,
									 nullptr};
			cvar_t kBotStopCvar = {kBotStopName, kBotStopDefault, FCVAR_SERVER, 0.0f, nullptr};
			cvar_t kBotDifficultyCvar = {kBotDifficultyName, kBotDifficultyDefault, FCVAR_SERVER,
										 0.0f, nullptr};
			cvar_t kBotQuotaCvar = {kBotQuotaName, kBotQuotaDefault, FCVAR_SERVER, 0.0f, nullptr};
			cvar_t kBotJoinTeamCvar = {kBotJoinTeamName, kBotJoinTeamDefault, FCVAR_SERVER, 0.0f,
									   nullptr};
	cvar_t kAstrabotModeCvar = {kAstrabotModeName, kAstrabotModeDefault, FCVAR_SERVER, 0.0f,
		nullptr};
cvar_t kAstrabotProfileCvar = {kAstrabotProfileName, kAstrabotProfileDefault, FCVAR_SERVER, 0.0f,
	nullptr};
cvar_t kAstrabotPerfDisableVisionCvar = {
	kAstrabotPerfDisableVisionName, kAstrabotPerfDisableVisionDefault, FCVAR_SERVER, 0.0f,
	nullptr};
cvar_t kAstrabotPerfDisablePathSearchCvar = {
	kAstrabotPerfDisablePathSearchName, kAstrabotPerfDisablePathSearchDefault, FCVAR_SERVER,
	0.0f, nullptr};
cvar_t kAstrabotPerfDisableTraceCvar = {
	kAstrabotPerfDisableTraceName, kAstrabotPerfDisableTraceDefault, FCVAR_SERVER, 0.0f,
	nullptr};

			cvar_t *const kCompatibilityCvars[] = {&kBotEnableCvar, &kBotStopCvar,
												   &kBotDifficultyCvar, &kBotQuotaCvar,
																				   &kBotJoinTeamCvar,
		&kAstrabotModeCvar, &kAstrabotProfileCvar};

const char *const kCompatibilityCvarNames[] = {
				kBotEnableName, kBotStopName, kBotDifficultyName, kBotQuotaName, kBotJoinTeamName,
	kAstrabotModeName, kAstrabotProfileName};

cvar_t *const kPerformanceCvars[kPerformanceCvarCount] = {
	&kAstrabotPerfDisableVisionCvar,
	&kAstrabotPerfDisablePathSearchCvar,
	&kAstrabotPerfDisableTraceCvar};
const char *const kPerformanceCvarNames[kPerformanceCvarCount] = {
	kAstrabotPerfDisableVisionName,
	kAstrabotPerfDisablePathSearchName,
	kAstrabotPerfDisableTraceName};

char kQuotaModeName[] = "bot_quota_mode";
char kQuotaModeDefault[] = "normal";
char kJoinAfterPlayerName[] = "bot_join_after_player";
char kJoinAfterPlayerDefault[] = "1";
char kAutoVacateName[] = "bot_auto_vacate";
char kAutoVacateDefault[] = "1";
char kJoinDelayName[] = "bot_join_delay";
char kJoinDelayDefault[] = "0";
char kAllowRoguesName[] = "bot_allow_rogues";
char kAllowRoguesDefault[] = "1";
cvar_t kManagerCvars[] = {
	{kQuotaModeName, kQuotaModeDefault, FCVAR_SERVER, 0.0f, nullptr},
	{kJoinAfterPlayerName, kJoinAfterPlayerDefault, FCVAR_SERVER, 1.0f, nullptr},
	{kAutoVacateName, kAutoVacateDefault, FCVAR_SERVER, 1.0f, nullptr},
	{kJoinDelayName, kJoinDelayDefault, FCVAR_SERVER, 0.0f, nullptr},
	{kAllowRoguesName, kAllowRoguesDefault, FCVAR_SERVER, 1.0f, nullptr}};

		} // namespace

		PluginRuntime::PluginRuntime()
			: state_(State::Cold), metaGlobals_(nullptr), gameDllFunctions_(nullptr),
			  hookedGameDllFunctions_(), engineFunctions_(nullptr), globals_(nullptr),
			  roundFreezeScheduled_(false), roundFreezeStartTime_(0.0f), roundFreezeUntil_(0.0f),
			  roundFreezeGeneration_(runtime::LifecycleSession::kInvalidGeneration),
			  roundLifecycleTracker_(), lifecycle_(), actorRegistry_(),
			  fakeClientManager_(lifecycle_, actorRegistry_),
			  inputDispatcher_(lifecycle_, actorRegistry_), compatibilitySurface_(),
			  compatibilityRandomSource_(), runtimeProfiler_(), navLoader_(), navPublisher_(),
			  navLoadDiagnostic_(), adapterFrameCount_(0U), pluginId_(nullptr), nativeBotGuard_(),
			  nativeGuardDecision_({NativeBotGuardState::Unsupported,
									NativeBotGuardReason::ControlsUnavailable, false}),
			  managedBotSlots_(), managedBotHandles_(), managedBotNames_(), joinControllers_(),
			  pendingJoinActions_(), managedBotFullUpdateSequences_(), managedBotTiming_(),
			  originalCommandArgs_(nullptr), originalCommandArgv_(nullptr),
			  originalCommandArgc_(nullptr), clientCommandContextActive_(false),
			  clientCommandArgumentCount_(0), clientCommandArgv0_(), clientCommandArgv1_(),
			  clientCommandArgs_(), nativeGuardEnabled_(false), compatibilityCvarOwned_(),
			  compatibilityRegistrationInProgress_(false), nativeControlsCaptured_(false),
			  performanceDisablePathSearch_(false), originalBotEnable_(0.0f),
			  originalBotQuota_(0.0f)
		{
			observation_.bindServices(*this);
			behavior_.bindServices(*this);
			navigation_.bindServices(*this);
			command_.bindServices(*this);
		}

		PluginRuntime &PluginRuntime::instance()
		{
			static PluginRuntime runtime;
			return runtime;
		}

		bool PluginRuntime::attach(PLUG_LOADTIME loadTime, META_FUNCTIONS *functionTable,
								   meta_globals_t *metaGlobals, gamedll_funcs_t *gameDllFunctions,
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
			requestedFunctions.pfnGetEntityAPI2_Post = &GetEntityAPI2_Post;
			requestedFunctions.pfnGetEngineFunctions = &GetEngineFunctions;
			*functionTable = requestedFunctions;

			metaGlobals_ = metaGlobals;
		gameDllFunctions_ = gameDllFunctions;
		hookedGameDllFunctions_ = {};
			engineFunctions_ = nullptr;
			lifecycle_ = runtime::LifecycleSession();
			actorRegistry_ = runtime::ActorRegistry();
			resetManagedBotExecutionState();
			navigation_.resetAvailabilityDiagnostic();
			inputDispatcher_.reset();
			navPublisher_.invalidate(0U);
			navLoadDiagnostic_ = {};
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
		gpMetaUtilFuncs->pfnGetHookTables(pluginId, &hookedEngineFunctions,
										&hookedDllFunctions, &hookedNewDllFunctions);
		engineFunctions_ = hookedEngineFunctions;
		hookedGameDllFunctions_.dllapi_table = hookedDllFunctions;
		hookedGameDllFunctions_.newapi_table = hookedNewDllFunctions;
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
			resetManagedBotExecutionState();
			navigation_.resetAvailabilityDiagnostic();
			navPublisher_.invalidate(lifecycle_.mapGeneration());
			navLoadDiagnostic_ = {};
			lifecycle_.deactivateMap();
			state_ = State::Detached;
			metaGlobals_ = nullptr;
		gameDllFunctions_ = nullptr;
		hookedGameDllFunctions_ = {};
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
			requestedFunctions.pfnCmdStart = &HookCmdStart;
			requestedFunctions.pfnTouch = &HookTouch;
			requestedFunctions.pfnClientDisconnect = &HookClientDisconnect;
			requestedFunctions.pfnClientPutInServer = &HookClientPutInServer;
			requestedFunctions.pfnServerActivate = &HookServerActivate;
			requestedFunctions.pfnServerDeactivate = &HookServerDeactivate;
			requestedFunctions.pfnStartFrame = &HookStartFrame;
			*functionTable = requestedFunctions;
			return true;
		}

		bool PluginRuntime::provideEntityApiPost(DLL_FUNCTIONS *functionTable,
				int *interfaceVersion)
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
			requestedFunctions.pfnStartFrame = &HookStartFramePost;
			*functionTable = requestedFunctions;
			return true;
		}

		bool PluginRuntime::provideEngineFunctions(enginefuncs_t *engineFunctions,
												   int *interfaceVersion)
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
			requestedFunctions.pfnCmd_Args = &HookCommandArgs;
			requestedFunctions.pfnCmd_Argv = &HookCommandArgv;
			requestedFunctions.pfnCmd_Argc = &HookCommandArgc;
			requestedFunctions.pfnMessageBegin = &HookMessageBegin;
			requestedFunctions.pfnMessageEnd = &HookMessageEnd;
			requestedFunctions.pfnWriteLong = &HookWriteLong;
			requestedFunctions.pfnWriteCoord = &HookWriteCoord;
			requestedFunctions.pfnEmitSound = &HookEmitSound;
		requestedFunctions.pfnWriteByte = &HookWriteByte;
		requestedFunctions.pfnWriteChar = &HookWriteChar;
		requestedFunctions.pfnWriteShort = &HookWriteShort;
		requestedFunctions.pfnWriteString = &HookWriteString;
		requestedFunctions.pfnAlertMessage = &HookAlertMessage;
		*engineFunctions = requestedFunctions;
			return true;
		}

		void PluginRuntime::giveEnginePointers(enginefuncs_t *engineFunctions,
											   globalvars_t *globals)
		{
		if (gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr &&
				pluginId_ != nullptr)
			{
				profiledLogConsole(runtimeProfiler_,
					pluginId_, "give engine pointers engine=%p globals=%p",
					static_cast<const void *>(engineFunctions), static_cast<const void *>(globals));
			}
			if (engineFunctions != nullptr)
			{
				engineFunctions_ = engineFunctions;
				compatibilityRandomSource_.configure({
					engineFunctions->pfnRandomLong,
					engineFunctions->pfnRandomFloat});
				if (engineFunctions->pfnCmd_Args != &HookCommandArgs)
				{
					originalCommandArgs_ = engineFunctions->pfnCmd_Args;
				}
				if (engineFunctions->pfnCmd_Argv != &HookCommandArgv)
				{
					originalCommandArgv_ = engineFunctions->pfnCmd_Argv;
				}
				if (engineFunctions->pfnCmd_Argc != &HookCommandArgc)
				{
					originalCommandArgc_ = engineFunctions->pfnCmd_Argc;
				}
			}
			if (globals != nullptr)
			{
				globals_ = globals;
			}
			observation_.configureEngine();

			registerCompatibilityCvars();
			configureFakeClientManager();
		}

		void PluginRuntime::registerCompatibilityCvars()
		{
			if (engineFunctions_ == nullptr || engineFunctions_->pfnCVarGetPointer == nullptr ||
				engineFunctions_->pfnCvar_RegisterVariable == nullptr)
			{
				return;
			}

	for (std::size_t index = 0U; index < kCompatibilityCvarCount; ++index)
	{
				if (engineFunctions_->pfnCVarGetPointer(kCompatibilityCvarNames[index]) == nullptr)
				{
					engineFunctions_->pfnCvar_RegisterVariable(kCompatibilityCvars[index]);
					compatibilityCvarOwned_[index] = true;
		}
	}
	for (std::size_t index = 0U; index < kPerformanceCvarCount; ++index)
	{
		if (engineFunctions_->pfnCVarGetPointer(kPerformanceCvarNames[index]) == nullptr)
		{
			engineFunctions_->pfnCvar_RegisterVariable(kPerformanceCvars[index]);
		}
	}
	for (cvar_t &variable : kManagerCvars)
	{
		if (engineFunctions_->pfnCVarGetPointer(variable.name) == nullptr)
		{
			engineFunctions_->pfnCvar_RegisterVariable(&variable);
		}
	}
}

		void PluginRuntime::synchronizeCompatibilityCvars()
		{
			if (engineFunctions_ == nullptr || engineFunctions_->pfnCVarGetPointer == nullptr ||
				engineFunctions_->pfnCVarGetFloat == nullptr)
			{
				return;
			}

			if (compatibilityCvarOwned_[static_cast<std::size_t>(CompatibilityCvarIndex::Enable)] &&
				compatibilityCvarOwned_[static_cast<std::size_t>(CompatibilityCvarIndex::Quota)])
			{
				compatibilitySurface_.setFloat("bot_enable",
											   engineFunctions_->pfnCVarGetFloat("bot_enable"));
				compatibilitySurface_.setFloat("bot_quota",
											   engineFunctions_->pfnCVarGetFloat("bot_quota"));
			}

			if (engineFunctions_->pfnCVarGetPointer("bot_stop") != nullptr)
			{
				compatibilitySurface_.setFloat("bot_stop",
											   engineFunctions_->pfnCVarGetFloat("bot_stop"));
			}
			if (engineFunctions_->pfnCVarGetPointer("bot_difficulty") != nullptr)
			{
				compatibilitySurface_.setFloat("bot_difficulty",
											   engineFunctions_->pfnCVarGetFloat("bot_difficulty"));
			}
			if (engineFunctions_->pfnCVarGetPointer("bot_join_team") != nullptr &&
				engineFunctions_->pfnCVarGetString != nullptr)
			{
				compatibilitySurface_.setString(
					"bot_join_team", engineFunctions_->pfnCVarGetString("bot_join_team"));
			}
		if (engineFunctions_->pfnCVarGetPointer("astrabot_mode") != nullptr &&
				engineFunctions_->pfnCVarGetString != nullptr)
			{
				compatibilitySurface_.setString(
					"astrabot_mode", engineFunctions_->pfnCVarGetString("astrabot_mode"));
		}
	if (engineFunctions_->pfnCVarGetPointer("astrabot_profile") != nullptr &&
		engineFunctions_->pfnCVarGetFloat != nullptr)
		{
			const bool wasProfiling = runtimeProfiler_.enabled();
			runtimeProfiler_.setEnabled(
				engineFunctions_->pfnCVarGetFloat("astrabot_profile") > 0.0f,
			globals_ != nullptr ? static_cast<double>(globals_->time) : 0.0);
			if (!wasProfiling && runtimeProfiler_.enabled()) navigation_.restartLoggingDiagnostics();
			if (wasProfiling != runtimeProfiler_.enabled() && gpMetaUtilFuncs != nullptr &&
				gpMetaUtilFuncs->pfnLogConsole != nullptr && pluginId_ != nullptr)
				importantLogConsole(runtimeProfiler_, pluginId_,
					"profile capture enabled=%d map=%u round=%u frame=%u now=%.3f sparse=1",
					runtimeProfiler_.enabled() ? 1 : 0, lifecycle_.mapGeneration(), lifecycle_.roundGeneration(),
					adapterFrameCount_, globals_ != nullptr ? globals_->time : 0.0f);
	}
	const bool disableVision = readOptionalCvarFloat(
		engineFunctions_, kAstrabotPerfDisableVisionName) > 0.0f;
	const bool disableTrace = readOptionalCvarFloat(
		engineFunctions_, kAstrabotPerfDisableTraceName) > 0.0f;
	performanceDisablePathSearch_ = readOptionalCvarFloat(
		engineFunctions_, kAstrabotPerfDisablePathSearchName) > 0.0f;
	observation_.configurePerformance(disableVision, disableTrace);
}

		PluginRuntime::Snapshot PluginRuntime::snapshot() const
		{
			Snapshot current = {state_,
								compatibilitySurface_.configuration().mode,
				lifecycle_.mapGeneration(),
				lifecycle_.roundGeneration(),
				nativeGuardDecision_.state,
				nativeGuardDecision_.reason,
				nativeGuardDecision_.managedBotCreationAllowed,
				roundFreezeActive()};
			return current;
		}

		bool PluginRuntime::roundFreezeActive() const
		{
			const RoundFreezeSchedule schedule = {
				roundFreezeScheduled_, roundFreezeStartTime_, roundFreezeUntil_};
			return globals_ != nullptr &&
				roundFreezeGeneration_ == lifecycle_.roundGeneration() &&
				schedule.isActive(globals_->time);
		}

		bool PluginRuntime::beginRoundGeneration()
		{
			if (!lifecycle_.beginRound())
			{
				return false;
			}
			observation_.resetRoundWeapons();
			behavior_.resetRoundBuyingState();

			navigation_.resetRoundDeathReceipts();
			behavior_.beginRoundPlantDecision();

			observation_.resetRoundObjectiveKnowledge();

			behavior_.resetRoundTactics();

			observation_.resetRoundRadioAndSound();

			behavior_.resetRoundNoiseInvestigation();
			behavior_.resetRoundRecognition();
			observation_.rebindRoundObservations();

			behavior_.resetRoundObjectiveCoordination();

			return true;
		}

		NavLoadResult PluginRuntime::loadNavigationFile(const NavLoadRequest *request)
		{
			return navLoader_.loadFile(request, &navPublisher_, &navLoadDiagnostic_);
		}

		NavLoadDiagnostic PluginRuntime::navigationDiagnostic() const { return navLoadDiagnostic_; }

nav::NavSnapshot PluginRuntime::navigationSnapshot() const
{
	return navPublisher_.snapshot();
}

runtime::MovementPhysicsSample PluginRuntime::movementPhysicsSample(std::uint32_t slot) const
{
	if (slot < runtime::LifecycleSession::kFirstClientSlot ||
			slot > runtime::LifecycleSession::kLastClientSlot)
	{
		return {};
	}
	return navigation_.movementPhysicsSamples(static_cast<std::size_t>(slot - 1U));
}

		runtime::LifecycleToken PluginRuntime::tokenForSlot(std::uint32_t slot) const
		{
			return lifecycle_.tokenForSlot(slot);
		}

		void PluginRuntime::onClientDisconnect(edict_t *entity)
		{
			if (state_ != State::ActiveMap || entity == nullptr || engineFunctions_ == nullptr ||
				engineFunctions_->pfnIndexOfEdict == nullptr)
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
					joinControllers_[static_cast<std::size_t>(slot - 1)].cancel(
						JoinError::Disconnected);
					clearManagedBot(static_cast<std::size_t>(slot - 1));
				}
			}
		}

		void PluginRuntime::onClientPutInServer(edict_t *entity)
		{
			if (state_ != State::ActiveMap || entity == nullptr || engineFunctions_ == nullptr ||
				engineFunctions_->pfnIndexOfEdict == nullptr)
			{
				return;
			}

			const int slot = engineFunctions_->pfnIndexOfEdict(entity);
			if (slot > 0)
			{
				lifecycle_.connectSlot(static_cast<std::uint32_t>(slot));
			}
		}

		void PluginRuntime::onCmdStart(const edict_t *entity, std::uint16_t buttons)
		{
			observation_.observeExternalCommand(entity, buttons);
		}
void PluginRuntime::onServerActivate(edict_t *edictList, int edictCount, int clientMax)
		{
			(void)edictList;
			(void)edictCount;
			(void)clientMax;
			if (gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr &&
				pluginId_ != nullptr)
			{
				profiledLogConsole(runtimeProfiler_,
					pluginId_, "server activate state=%d engine=%p globals=%p",
					static_cast<int>(state_), static_cast<const void *>(engineFunctions_),
					static_cast<const void *>(globals_));
			}

			if (state_ == State::Attached)
			{
		if (lifecycle_.activateMap())
		{
			observation_.probeMapProvider();
			navigation_.resetMapDanger();
			observation_.resetMapPublicObservations();

			behavior_.beginMapPlantDecision();

			observation_.resetMapSoundAndSmoke();

			nextPopulationUpdate_ = 0.0f;
			observation_.resetMapObjectiveKnowledge();
			roundLifecycleTracker_.reset();
			roundFreezeScheduled_ = false;
				roundFreezeStartTime_ = 0.0f;
				roundFreezeUntil_ = 0.0f;
				roundFreezeGeneration_ = lifecycle_.roundGeneration();
				adapterFrameCount_ = 0U;
					state_ = State::ActiveMap;
					navigation_.resetAvailabilityDiagnostic();
					registerCompatibilityCvars();
					armNativeBotGuard();
					configureFakeClientManager();
					registerCompatibilityCommands();
					loadCurrentMapNavigation();
					if (gpMetaUtilFuncs != nullptr &&
							gpMetaUtilFuncs->pfnGetUserMsgID != nullptr &&
							gpMetaUtilFuncs->pfnLogConsole != nullptr && pluginId_ != nullptr)
					{
						int messageSize = 0;
						const int showMenuId = gpMetaUtilFuncs->pfnGetUserMsgID(
							pluginId_, "ShowMenu", &messageSize);
						const int vguiMenuId = gpMetaUtilFuncs->pfnGetUserMsgID(
							pluginId_, "VGUIMenu", &messageSize);
						const int teamInfoId = gpMetaUtilFuncs->pfnGetUserMsgID(
							pluginId_, "TeamInfo", &messageSize);
						profiledLogConsole(runtimeProfiler_,
							pluginId_, "menu message ids show=%d vgui=%d team=%d",
							showMenuId, vguiMenuId, teamInfoId);
						const compat::RuntimeMode mode = compatibilitySurface_.configuration().mode;
						importantLogConsole(runtimeProfiler_,
							pluginId_, "runtime mode=%s map=%u round=%u",
							runtimeModeName(mode), lifecycle_.mapGeneration(), lifecycle_.roundGeneration());
					}
				}
			}
		}

		void PluginRuntime::onServerDeactivate()
		{
			pendingJoinActions_.clear();
			pendingJoinActionOverflow_.fill(false);
		if (state_ == State::ActiveMap)
		{
			roundLifecycleTracker_.reset();
			roundFreezeScheduled_ = false;
				roundFreezeStartTime_ = 0.0f;
				roundFreezeUntil_ = 0.0f;
				roundFreezeGeneration_ = runtime::LifecycleSession::kInvalidGeneration;
				lifecycle_.deactivateMap();
				observation_.deactivateMapProvider();
				navPublisher_.invalidate(lifecycle_.mapGeneration());
				navLoadDiagnostic_ = {};
				adapterFrameCount_ = 0U;
				updateNativeBotGuard();
			inputDispatcher_.reset();
			clearManagedBots();
			actorRegistry_ = runtime::ActorRegistry();
			resetManagedBotExecutionState();
			navigation_.resetAvailabilityDiagnostic();
			state_ = State::Attached;
				compatibilityRegistrationInProgress_ = false;
				configureFakeClientManager();
			}
		}

		void PluginRuntime::loadCurrentMapNavigation()
		{
			navPublisher_.invalidate(lifecycle_.mapGeneration());
			navLoadDiagnostic_ = {};
			navLoadDiagnostic_.result = NavLoadResult::BoundaryUnavailable;
			navLoadDiagnostic_.readerResult = nav::NavReadResult::InvalidArgument;
			navLoadDiagnostic_.snapshotResult = nav::NavSnapshotResult::Invalidated;
			navLoadDiagnostic_.mapGeneration = lifecycle_.mapGeneration();

			if (engineFunctions_ == nullptr || globals_ == nullptr ||
				engineFunctions_->pfnGetGameDir == nullptr)
			{
				return;
			}
			char gameDirectory[NavLoader::kMaximumPathLength] = {};
			engineFunctions_->pfnGetGameDir(gameDirectory);
			char profilePath[NavLoader::kMaximumPathLength] = {};
			if (buildGameFilePath(gameDirectory, "BotProfile.db", profilePath, sizeof(profilePath)))
			{
				const ProfileLoadResult profileResult = loadCompatibilityProfiles(profilePath);
				if (gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr &&
					pluginId_ != nullptr)
				{
					importantLogConsole(runtimeProfiler_,
						pluginId_, "profile load result=%d map=%u path=%s",
						static_cast<int>(profileResult), lifecycle_.mapGeneration(), profilePath);
				}
			}
			if (engineFunctions_->pfnSzFromIndex == nullptr)
			{
				return;
			}

			const char *mapName = nullptr;
			if (engineFunctions_->pfnSzFromIndex != nullptr)
			{
				mapName = engineFunctions_->pfnSzFromIndex(globals_->mapname);
			}
			if ((mapName == nullptr || mapName[0] == '\0') &&
					engineFunctions_->pfnCVarGetString != nullptr)
			{
				mapName = engineFunctions_->pfnCVarGetString("mapname");
			}
			if ((mapName == nullptr || mapName[0] == '\0') &&
					engineFunctions_->pfnCVarGetPointer != nullptr)
			{
				cvar_t *mapNameCvar = engineFunctions_->pfnCVarGetPointer("mapname");
				if (mapNameCvar != nullptr)
				{
					mapName = mapNameCvar->string;
				}
			}
			if (mapName == nullptr || mapName[0] == '\0')
			{
				return;
			}
			char navPath[NavLoader::kMaximumPathLength] = {};
			char bspPath[NavLoader::kMaximumPathLength] = {};
			if (!buildMapPath(gameDirectory, mapName, ".nav", navPath, sizeof(navPath)) ||
				!buildMapPath(gameDirectory, mapName, ".bsp", bspPath, sizeof(bspPath)))
			{
				return;
			}

			int bspSize = localFileSize(bspPath);
			if (engineFunctions_->pfnGetFileSize != nullptr)
			{
				const int engineBspSize = engineFunctions_->pfnGetFileSize(bspPath);
				if (engineBspSize >= 0)
				{
					bspSize = engineBspSize;
				}
			}
			const NavLoadRequest request = {navPath,
											mapName,
											lifecycle_.mapGeneration(),
											bspSize >= 0,
											bspSize >= 0 ? static_cast<std::uint32_t>(bspSize) : 0U,
											false,
											0U};
			const NavLoadResult result = loadNavigationFile(&request);
			if (gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr &&
					pluginId_ != nullptr)
			{
				importantLogConsole(runtimeProfiler_,
					pluginId_,
					"nav load result=%d reader=%d snapshot=%d valid=%d path=%s bspSize=%d",
					static_cast<int>(result),
					static_cast<int>(navLoadDiagnostic_.readerResult),
					static_cast<int>(navLoadDiagnostic_.snapshotResult),
					navPublisher_.snapshot().isValid() ? 1 : 0,
					navPath,
					bspSize);
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
		RuntimeProfilerScope profilerScope(runtimeProfiler_, RuntimeProfilerStage::StartFrame);
			if (adapterFrameCount_ == (std::numeric_limits<std::uint32_t>::max)())
			{
				adapterFrameCount_ = 0U;
			}
			else
			{
				++adapterFrameCount_;
			}
			lifecycle_.observeFrame(adapterFrameCount_, globals_->time);
			observation_.refreshPublicSmoke();
			command_.stabilizeFrozenActors();
			synchronizeNativeBotControls();
				synchronizeCompatibilityCvars();
				updateNativeBotGuard();
				configureFakeClientManager();
				maintainPublicBotPopulation();
			}

void PluginRuntime::onStartFramePost()
{
	if (state_ == State::ActiveMap)
	{
		{
			RuntimeProfilerScope profilerScope(
				runtimeProfiler_, RuntimeProfilerStage::RuntimeInput);
			processDeferredJoinActions();
			processJoinControllers();
		}
			if (runtimeProfiler_.enabled())
			{
				std::uint32_t aliveBots = 0U;
				for (std::size_t index = 0U; index < managedBotSlots_.size(); ++index)
				{
					const FakeClientHandle &handle = managedBotHandles_[index];
					if (managedBotSlots_[index] && handle.entity != nullptr &&
						actorRegistry_.state(handle.actor) == runtime::ActorState::Joined &&
						handle.entity->v.deadflag == DEAD_NO && handle.entity->v.health > 0.0f)
					{
						++aliveBots;
					}
				}
				runtimeProfiler_.setAliveBots(aliveBots);
			}
			updateManagedBots();
			}
			if (globals_ != nullptr && runtimeProfiler_.enabled())
			{
				RuntimeProfilerReport report = {};
				if (runtimeProfiler_.consumeReport(
						static_cast<double>(globals_->time), &report) &&
					gpMetaUtilFuncs != nullptr &&
					gpMetaUtilFuncs->pfnLogConsole != nullptr && pluginId_ != nullptr)
				{
					profiledLogConsole(runtimeProfiler_, pluginId_,
						"profile window game_seconds=%.3f wall_seconds=%.6f frames=%u sparse=1",
						report.windowSeconds, report.wallWindowSeconds,
						static_cast<unsigned int>(report.frameCount));
					for (std::size_t stageIndex = 0U;
						stageIndex < static_cast<std::size_t>(RuntimeProfilerStage::Count);
						++stageIndex)
					{
						const RuntimeProfilerStage stage =
							static_cast<RuntimeProfilerStage>(stageIndex);
						const RuntimeProfilerStageStats &stats = report.stages[stageIndex];
						if (stats.calls == 0U) continue;
						const double average = stats.calls == 0U ? 0.0
							: static_cast<double>(stats.totalUsec) /
								static_cast<double>(stats.calls);
						profiledLogConsole(runtimeProfiler_,
							pluginId_,
							"profile stage=%s calls=%u total_usec=%llu avg_usec=%.1f max_usec=%llu",
							runtimeProfilerStageName(stage),
							static_cast<unsigned int>(stats.calls),
							static_cast<unsigned long long>(stats.totalUsec), average,
							static_cast<unsigned long long>(stats.maxUsec));
					}
					profiledLogConsole(runtimeProfiler_,
						pluginId_,
						"profile counters aliveBots=%u traceLine=%llu visibilityCandidates=%llu "
						"fovChecks=%llu losChecks=%llu bodyProbeCalls=%llu "
						"pathSearch=%llu pathSuccess=%llu pathFailure=%llu pathRecompute=%llu "
						"expanded=%llu enqueues=%llu reopens=%llu staleQueue=%llu equalCostRepl=%llu "
						"pathSearchUsec=%llu pathSearchMaxUsec=%llu "
						"pathSearchFirstId=%llu pathSearchLastId=%llu "
						"runPlayerMove=%llu",
						static_cast<unsigned int>(report.aliveBots),
						static_cast<unsigned long long>(report.traceLineCalls),
						static_cast<unsigned long long>(report.visibilityCandidates),
						static_cast<unsigned long long>(report.fovChecks),
						static_cast<unsigned long long>(report.losChecks),
						static_cast<unsigned long long>(report.bodyProbeCalls),
						static_cast<unsigned long long>(report.pathSearches),
						static_cast<unsigned long long>(report.pathSearchSuccesses),
						static_cast<unsigned long long>(report.pathSearchFailures),
						static_cast<unsigned long long>(report.pathRecomputes),
						static_cast<unsigned long long>(report.pathExpandedAreas),
						static_cast<unsigned long long>(report.pathEnqueues),
						static_cast<unsigned long long>(report.pathReopens),
						static_cast<unsigned long long>(report.pathStaleQueueEntries),
						static_cast<unsigned long long>(report.pathEqualCostReplacements),
						static_cast<unsigned long long>(report.pathSearchTotalUsec),
						static_cast<unsigned long long>(report.pathSearchMaxUsec),
						static_cast<unsigned long long>(report.pathFirstSearchId),
						static_cast<unsigned long long>(report.pathLastSearchId),
					static_cast<unsigned long long>(report.runPlayerMoves));
					for (std::size_t callerIndex = 0U;
						callerIndex < static_cast<std::size_t>(RuntimePathSearchCaller::Count);
						++callerIndex)
					{
						const RuntimePathSearchCaller caller =
							static_cast<RuntimePathSearchCaller>(callerIndex);
						const RuntimePathSearchAggregate &aggregate =
							report.pathSearchByCaller[callerIndex];
						if (aggregate.calls == 0U) continue;
						const double average = aggregate.calls == 0U ? 0.0
							: static_cast<double>(aggregate.totalUsec) /
								static_cast<double>(aggregate.calls);
						profiledLogConsole(runtimeProfiler_,
							pluginId_,
							"profile pathSearchByCaller.%s calls=%llu total_usec=%llu "
							"avg_usec=%.1f max_usec=%llu expanded=%llu enqueues=%llu",
							runtimePathSearchCallerName(caller),
							static_cast<unsigned long long>(aggregate.calls),
							static_cast<unsigned long long>(aggregate.totalUsec), average,
							static_cast<unsigned long long>(aggregate.maxUsec),
							static_cast<unsigned long long>(aggregate.expandedAreas),
							static_cast<unsigned long long>(aggregate.enqueues));
					}
					profiledLogConsole(runtimeProfiler_,
						pluginId_,
						"profile objectiveCandidates evaluatedBombSites=%llu "
						"evaluated_func_bomb_target_count=%llu "
						"evaluated_info_bomb_target_count=%llu "
						"candidateAreas=%llu "
						"uniqueCandidateAreas=%llu candidateQueries=%llu "
						"duplicateCandidateAreas=%llu selectedGoalArea=%u "
						"registered_site_count=%llu "
						"registered_func_bomb_target_count=%llu "
						"registered_info_bomb_target_count=%llu "
						"evaluated_this_window=%llu "
			"cache_hit=%llu selected_site_id=%u "
						"duplicateSearchSameFullUpdate=%llu "
						"duplicateSearchSameObjectiveGeneration=%llu uniqueSearchKeys=%llu",
						static_cast<unsigned long long>(report.objectiveBombSites),
						static_cast<unsigned long long>(
							report.objectiveFuncBombTargetSites),
						static_cast<unsigned long long>(
							report.objectiveInfoBombTargetSites),
						static_cast<unsigned long long>(report.objectiveCandidateAreas),
						static_cast<unsigned long long>(report.objectiveUniqueCandidateAreas),
						static_cast<unsigned long long>(report.objectiveCandidateQueries),
						static_cast<unsigned long long>(report.objectiveDuplicateCandidateAreas),
						static_cast<unsigned int>(report.selectedObjectiveGoalArea),
						static_cast<unsigned long long>(report.objectiveRegisteredSites),
						static_cast<unsigned long long>(
							report.objectiveRegisteredFuncBombTargetSites),
						static_cast<unsigned long long>(
							report.objectiveRegisteredInfoBombTargetSites),
						static_cast<unsigned long long>(report.objectiveEvaluatedSites),
			static_cast<unsigned long long>(report.objectiveCacheHits),
			static_cast<unsigned int>(report.selectedObjectiveSiteIdentity),
						static_cast<unsigned long long>(report.duplicateSearchSameFullUpdate),
						static_cast<unsigned long long>(report.duplicateSearchSameObjectiveGeneration),
						static_cast<unsigned long long>(report.uniqueSearchKeys));
				}
			}
		}

void PluginRuntime::deferJoinAction(
	std::size_t index, const JoinAction &action)
{
	if (index >= managedBotSlots_.size() || !managedBotSlots_[index] ||
		action.kind == JoinActionKind::None)
	{
		return;
	}
	const FakeClientHandle &handle = managedBotHandles_[index];
	if (handle.entity == nullptr)
	{
		return;
	}
	const runtime::LifecycleToken token = lifecycle_.tokenForSlot(handle.actor.slot);
	if (pendingJoinActions_.enqueue(handle.actor, token, adapterFrameCount_, action))
	{
		return;
	}
	pendingJoinActionOverflow_[index] = true;
	if (gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr &&
		pluginId_ != nullptr)
	{
		importantLogConsole(runtimeProfiler_,
			pluginId_,
			"join action queue full slot=%u generation=%u frame=%u action=%d error=%d",
			static_cast<unsigned int>(handle.actor.slot),
			static_cast<unsigned int>(handle.actor.actorGeneration),
			static_cast<unsigned int>(adapterFrameCount_),
			static_cast<int>(action.kind),
			static_cast<int>(action.error));
	}
}

void PluginRuntime::processDeferredJoinActions()
{
	const std::size_t batchCount = pendingJoinActions_.size();
	for (std::size_t actionIndex = 0U; actionIndex < batchCount; ++actionIndex)
	{
		metamod::DeferredJoinAction pending{};
		if (!pendingJoinActions_.dequeue(&pending) ||
			pending.actor.slot < runtime::LifecycleSession::kFirstClientSlot ||
			pending.actor.slot > runtime::LifecycleSession::kLastClientSlot)
		{
			continue;
		}
		const std::size_t index =
			static_cast<std::size_t>(pending.actor.slot - 1U);
		const FakeClientHandle &handle = managedBotHandles_[index];
		if (!managedBotSlots_[index] || handle.entity == nullptr ||
			handle.actor.actorGeneration != pending.actor.actorGeneration ||
			handle.actor.slot != pending.actor.slot)
		{
			continue;
		}
		const runtime::LifecycleToken current =
			lifecycle_.tokenForSlot(pending.actor.slot);
		if (!lifecycle_.isCurrent(current) ||
			current.mapGeneration != pending.mapGeneration ||
			current.slotGeneration != pending.slotGeneration)
		{
			continue;
		}
		applyJoinAction(index, pending.action);
	}
	for (std::size_t index = 0U; index < pendingJoinActionOverflow_.size(); ++index)
	{
		if (!pendingJoinActionOverflow_[index])
		{
			continue;
		}
		pendingJoinActionOverflow_[index] = false;
		if (managedBotSlots_[index] &&
			(joinControllers_[index].phase() == JoinPhase::Failed ||
			joinControllers_[index].phase() == JoinPhase::Cancelled))
		{
			cleanupManagedJoin(index, joinControllers_[index].error());
		}
	}
}

void PluginRuntime::notifyMenuReady(
	edict_t *entity, JoinMenuKind menu, std::uint16_t validSlots, JoinMenuSource source)
		{
			if (entity == nullptr || engineFunctions_ == nullptr ||
				engineFunctions_->pfnIndexOfEdict == nullptr)
			{
				return;
			}

			const int slot = engineFunctions_->pfnIndexOfEdict(entity);
			if (slot < static_cast<int>(runtime::LifecycleSession::kFirstClientSlot) ||
				slot > static_cast<int>(runtime::LifecycleSession::kClientSlotCount))
			{
				return;
			}

			const std::size_t index = static_cast<std::size_t>(slot - 1U);
			if (!managedBotSlots_[index] || managedBotHandles_[index].entity != entity)
			{
				return;
			}
	deferJoinAction(index, joinControllers_[index].onMenu(menu, validSlots, adapterFrameCount_, source));
		}
				void PluginRuntime::onMessageBegin(int messageDestination, int messageType,
			const float *origin, edict_t *entity)
		{
			observation_.onMessageBegin(messageDestination, messageType, origin, entity);
		}

		void PluginRuntime::onMessageEnd()
		{
			observation_.onMessageEnd();
		}

		void PluginRuntime::onRoundLifecycleMessage(const char *message)
		{
			if (state_ != State::ActiveMap || globals_ == nullptr || message == nullptr ||
				!std::isfinite(globals_->time))
			{
				return;
			}

			static constexpr char kGameCommencing[] =
				"World triggered \"Game_Commencing\"";
			static constexpr char kRestartPrefix[] =
				"World triggered \"Restart_Round_(";
			static constexpr char kRoundEnded[] =
				"World triggered \"Round_End\"";
			static constexpr char kRoundStarted[] =
				"World triggered \"Round_Start\"";

		if (std::strncmp(message, kRoundStarted, sizeof(kRoundStarted) - 1U) == 0)
		{
			if (roundLifecycleTracker_.isDuplicateRoundStart())
			{
				return;
			}
			if (roundLifecycleTracker_.shouldBeginNewGeneration(
						RoundLifecycleEvent::RoundStart) && !beginRoundGeneration())
				{
					return;
				}
			const RoundFreezeSchedule noFreeze =
				RoundFreezeSchedule::create(globals_->time, 0.0f);
			roundFreezeStartTime_ = noFreeze.startTime;
			roundFreezeUntil_ = noFreeze.untilTime;
			roundFreezeGeneration_ = lifecycle_.roundGeneration();
			roundFreezeScheduled_ = noFreeze.scheduled;
			return;
		}
	if (std::strncmp(message, kRoundEnded, sizeof(kRoundEnded) - 1U) == 0)
	{
		if (!roundLifecycleTracker_.shouldBeginNewGeneration(
				RoundLifecycleEvent::RoundEnd))
		{
			return;
		}
		if (!beginRoundGeneration())
		{
			return;
		}
		float restartDelay = readOptionalCvarFloat(
			engineFunctions_, "mp_round_restart_delay");
		if (!std::isfinite(restartDelay) || restartDelay < 0.0f)
		{
			restartDelay = 0.0f;
		}
		restartDelay = (std::min)(restartDelay, 60.0f);
		const RoundFreezeSchedule freeze = RoundFreezeSchedule::create(
			globals_->time + restartDelay,
			readOptionalCvarFloat(engineFunctions_, "mp_freezetime"));
		roundFreezeScheduled_ = freeze.scheduled;
		roundFreezeStartTime_ = freeze.startTime;
		roundFreezeUntil_ = freeze.untilTime;
		roundFreezeGeneration_ = lifecycle_.roundGeneration();
		return;
		}

		if (std::strncmp(message, kGameCommencing, sizeof(kGameCommencing) - 1U) == 0)
		{
			if (!roundLifecycleTracker_.shouldBeginNewGeneration(
					RoundLifecycleEvent::GameCommencing) || !beginRoundGeneration())
			{
				return;
			}
			// Pinned ReGameDLL b0889847 NeededPlayersCheck(): ordinary games
			// commence after three seconds, then RestartRound begins FreezeTime.
			// Career/custom GameDLL hook delays are unavailable at this boundary.
			constexpr float kGameCommencingRestartDelay = 3.0f;
			const RoundFreezeSchedule freeze = RoundFreezeSchedule::create(
				globals_->time + kGameCommencingRestartDelay,
				readOptionalCvarFloat(engineFunctions_, "mp_freezetime"));
			roundFreezeStartTime_ = freeze.startTime;
			roundFreezeUntil_ = freeze.untilTime;
			roundFreezeGeneration_ = lifecycle_.roundGeneration();
			roundFreezeScheduled_ = freeze.scheduled;
			return;
		}

		float restartDelay = 0.0f;
			bool beginsRound =
				std::strncmp(message, kGameCommencing, sizeof(kGameCommencing) - 1U) == 0;
			const char *restart = std::strstr(message, kRestartPrefix);
			if (restart != nullptr)
			{
				const char *delayText = restart + sizeof(kRestartPrefix) - 1U;
				char *delayEnd = nullptr;
				const long delaySeconds = std::strtol(delayText, &delayEnd, 10);
				if (delayEnd == delayText || delaySeconds < 0L || delaySeconds > 300L ||
					delayEnd[0] != '_' || std::strncmp(delayEnd + 1, "second", 6U) != 0)
				{
					return;
				}
				const char *suffix = delayEnd + 7;
				if (*suffix == 's')
				{
					++suffix;
				}
				if (*suffix != ')')
				{
					return;
				}
				restartDelay = static_cast<float>(delaySeconds);
				beginsRound = true;
			}

			if (!beginsRound)
			{
				return;
			}

			const float scheduledStart = globals_->time + restartDelay;
			const RoundFreezeSchedule freeze = RoundFreezeSchedule::create(
				scheduledStart,
				readOptionalCvarFloat(engineFunctions_, "mp_freezetime"));
			const float freezeUntil = freeze.untilTime;
			if (roundFreezeGeneration_ == lifecycle_.roundGeneration() &&
				roundFreezeStartTime_ == scheduledStart &&
				roundFreezeUntil_ == freezeUntil)
			{
				return;
			}
			const RoundLifecycleEvent roundEvent = restart != nullptr
				? RoundLifecycleEvent::RestartScheduled
				: RoundLifecycleEvent::GameCommencing;
			if (roundLifecycleTracker_.shouldBeginNewGeneration(roundEvent) &&
				!beginRoundGeneration())
			{
				return;
			}
			roundFreezeStartTime_ = freeze.startTime;
			roundFreezeUntil_ = freeze.untilTime;
			roundFreezeGeneration_ = lifecycle_.roundGeneration();
			roundFreezeScheduled_ = freeze.scheduled;
		}

		void PluginRuntime::onWriteByte(int value)
		{
			observation_.onWriteByte(value);
		}

		void PluginRuntime::onWriteChar(int value)
		{
			observation_.onWriteChar(value);
		}

		void PluginRuntime::onWriteShort(int value)
		{
			observation_.onWriteShort(value);
		}

		void PluginRuntime::onWriteString(const char *value)
		{
			observation_.onWriteString(value);
}

void PluginRuntime::maintainPublicBotPopulation()
{
	if (globals_ == nullptr || engineFunctions_ == nullptr ||
		engineFunctions_->pfnPEntityOfEntIndex == nullptr || globals_->time < nextPopulationUpdate_)
		return;
	nextPopulationUpdate_ = globals_->time + 0.3f;
	if (!nativeGuardDecision_.managedBotCreationAllowed)
		return;
	const char *const floatNames[] = {"bot_join_after_player", "bot_auto_vacate", "bot_join_delay"};
	for (const char *name : floatNames)
	{
		if (engineFunctions_->pfnCVarGetPointer == nullptr || engineFunctions_->pfnCVarGetPointer(name) == nullptr)
			continue;
		const float value = readOptionalCvarFloat(engineFunctions_, name);
		if (std::isfinite(value))
			(void)compatibilitySurface_.setFloat(name, value);
	}
	if (engineFunctions_->pfnCVarGetPointer != nullptr && engineFunctions_->pfnCVarGetString != nullptr &&
		engineFunctions_->pfnCVarGetPointer("bot_quota_mode") != nullptr)
		(void)compatibilitySurface_.setString("bot_quota_mode", engineFunctions_->pfnCVarGetString("bot_quota_mode"));
	compat::QuotaPopulation population{};
	population.maxClients = globals_->maxClients;
	population.managedBotClients = static_cast<int>(managedBotCount());
	population.elapsedMapSeconds = globals_->time;
	// The original public CS round rules accept respawns for 20 seconds.
	// ReGameDLL exposes the override as mp_roundrespawn_time (-1 means unlimited).
	// RoundStart/End are public notifications; an ended round must not pin fill/match.
	float respawnWindow = 20.0f;
	if (engineFunctions_->pfnCVarGetPointer != nullptr &&
		engineFunctions_->pfnCVarGetPointer("mp_roundrespawn_time") != nullptr)
	{
		const float observed = readOptionalCvarFloat(engineFunctions_, "mp_roundrespawn_time");
		if (std::isfinite(observed) && observed >= -1.0f) respawnWindow = observed;
	}
	population.lateJoinBlocked = roundLifecycleTracker_.isDuplicateRoundStart() &&
		roundFreezeGeneration_ == lifecycle_.roundGeneration() &&
		std::isfinite(roundFreezeUntil_) && globals_->time >= roundFreezeUntil_ &&
		respawnWindow >= 0.0f && globals_->time - roundFreezeUntil_ >= respawnWindow;
	std::array<int, 3U> teamCounts{};
	for (int slot = 1; slot <= (std::min)(32, globals_->maxClients); ++slot)
	{
		edict_t *entity = engineFunctions_->pfnPEntityOfEntIndex(slot);
		if (entity == nullptr || entity->free || (entity->v.flags & FL_CLIENT) == 0)
			continue;
		++population.totalClients;
		// TeamInfo is authoritative for humans too: CS can leave entvars.team at zero.
		const int team =
			observation_.publicTeamKnown(static_cast<std::size_t>(slot - 1))
				? observation_.teamInfoTeams(static_cast<std::size_t>(slot - 1))
				: entity->v.team;
		if (team == 1 || team == 2)
		{
			++teamCounts[team];
		}
		if ((entity->v.flags & FL_FAKECLIENT) == 0)
		{
			++population.humanClients;
			if (team == 1 || team == 2)
				++population.activeHumanClients;
		}
	}
	const compat::CvarSnapshot settings = compatibilitySurface_.configuration();
	const int desired = compat::calculateDesiredBotCount(settings, population);
	if (desired > population.managedBotClients && settings.botStop == 0.0f)
	{
		const char *command = settings.botJoinTeam == compat::JoinTeam::Terrorist ? "bot_add_t"
			: settings.botJoinTeam == compat::JoinTeam::CounterTerrorist ? "bot_add_ct" : "bot_add";
		populationAdjustment_ = true;
		(void)executeCompatibilityCommand({command, 0U, {nullptr, nullptr}});
		populationAdjustment_ = false;
	}
	else if (desired < population.managedBotClients)
	{
		const auto removeFromTeam = [this](int team) -> bool
		{
			for (std::size_t index = 0U; index < managedBotSlots_.size(); ++index)
			{
				if (!managedBotSlots_[index] || observation_.teamInfoTeams(index) != team)
				{
					continue;
				}
				if (removeFakeClient(&managedBotHandles_[index]) == FakeClientResult::Removed)
				{
					clearManagedBot(index);
					return true;
				}
			}
			return false;
		};
		if (removeFromTeam(0))
		{
			return;
		}
		int team = teamCounts[1] > teamCounts[2] ? 1 : teamCounts[2] > teamCounts[1] ? 2 : 0;
		if (team == 0 && observation_.teamScoresKnown()[1] && observation_.teamScoresKnown()[2])
		{
			team = observation_.teamScores()[1] > observation_.teamScores()[2]	 ? 1
				   : observation_.teamScores()[2] > observation_.teamScores()[1] ? 2
																				 : 0;
		}
		if (team == 0)
		{
			const auto choice = compatibilityRandomSource_.nextLong(compat::RandomRequest::longRequest(
				"Quota.KickTeam", {}, {adapterFrameCount_, 0U, 0U}, 0, 1));
			if (choice.status != compat::RandomStatus::Ok)
			{
				return;
			}
			team = choice.value == 0 ? 2 : 1;
		}
		if (!removeFromTeam(team))
		{
			(void)removeFromTeam(team == 1 ? 2 : 1);
		}
	}
}

void PluginRuntime::onEmitSound(edict_t *entity, const char *sample, float volume, float attenuation)
{
	observation_.onEmitSound(entity, sample, volume, attenuation);
}

void PluginRuntime::onWriteLong(int value)
{
	observation_.onWriteLong(value);
}

void PluginRuntime::onWriteCoord(float value)
{
	observation_.onWriteCoord(value);
}

void PluginRuntime::prepareNeutralManagedBotCommand(std::size_t index, FakeClientHandle &handle,
													const runtime::MovementPhysicsState &before)
{
	const auto draft = command_.beginNeutralCommand(index, handle, before);
	if (!draft.ready)
	{
		return;
	}
	const auto action =
		behavior_.decideManagedBotAction(index, before, draft.command.viewAngles, draft.command.movement.buttons);
	command_.finishNeutralCommand(index, handle, before, draft, action);
}

runtime::CommandReceipt PluginRuntime::dispatchNeutralMovement(std::size_t index, FakeClientHandle &handle,
															   std::uint8_t milliseconds)
{
	const auto draft = command_.beginDirectNeutralCommand(index, handle, milliseconds);
	if (!draft.ready)
	{
		return draft.receipt;
	}
	const auto action =
		behavior_.decideManagedBotAction(index, draft.before, draft.command.viewAngles, draft.command.movement.buttons);
	return command_.finishDirectNeutralCommand(index, handle, milliseconds, draft, action);
}

void PluginRuntime::buildAndExecuteMovement(std::size_t index, FakeClientHandle &handle,
											const runtime::LifecycleToken &token,
											const runtime::MovementPhysicsState &before, NavigationUpdate &movement)
{
	const auto draft = command_.beginMovementCommand(index, handle, token, before, movement);
	if (!draft.ready)
	{
		return;
	}
	const auto action =
		behavior_.decideManagedBotAction(index, before, draft.command.viewAngles, draft.command.movement.buttons);
	command_.finishMovementCommand(index, handle, token, before, movement, draft, action);
}

void PluginRuntime::updateManagedBots()
{
	const compat::CvarSnapshot configuration = compatibilitySurface_.configuration();
	const nav::NavSnapshot navigation = navPublisher_.snapshot();
	const bool movementUnavailable = configuration.botEnable <= 0.0f ||
		configuration.botStop > 0.0f || !navigation.isValid();
	navigation_.beginMovementUpdate(configuration, navigation, movementUnavailable);
	const std::uint8_t milliseconds = movementMilliseconds(globals_);
	for (std::size_t index = 0U; index < managedBotSlots_.size(); ++index)
	{
		if (!managedBotSlots_[index])
		{
			continue;
		}
		if (movementUnavailable)
		{
			command_.resetJumpCrouch(index);
		}

		FakeClientHandle &handle = managedBotHandles_[index];
		if (handle.entity == nullptr)
		{
			continue;
		}
		const runtime::LifecycleToken token = lifecycle_.tokenForSlot(handle.actor.slot);
		if (!lifecycle_.isCurrent(token))
		{
			continue;
		}
		const runtime::MovementPhysicsState before = observation_.captureMovementPhysicsState(
			handle.entity, joinControllers_[index].teamConfirmed(), true);
		const runtime::ActorState actorState = actorRegistry_.state(handle.actor);
		if (actorState == runtime::ActorState::Joining)
		{
			if (!joinControllers_[index].active())
			{
				continue;
			}
			const runtime::CommandReceipt receipt =
				command_.dispatchJoinHeartbeat(index, handle, milliseconds);
			navigation_.recordMovementPhysicsSample(index, before, receipt);
			continue;
		}
		if (actorState != runtime::ActorState::Joined)
		{
			continue;
		}
		if (!managedBotTiming_[index].initialized())
		{
			resetManagedBotTiming(index, globals_->time);
		}
		const runtime::BotTimingStep timingStep = managedBotTiming_[index].advance(
			globals_->time);
		bool commandResetDue = false;
		bool fullUpdateDue = false;
		bool commandExecuteDue = false;
		bool upkeepDue = false;
		for (std::size_t eventIndex = 0U; eventIndex < timingStep.eventCount; ++eventIndex)
		{
			switch (timingStep.events[eventIndex])
			{
			case runtime::BotTimingEvent::CommandReset:
				commandResetDue = true;
				break;
			case runtime::BotTimingEvent::FullUpdate:
				fullUpdateDue = true;
				break;
			case runtime::BotTimingEvent::CommandExecute:
				commandExecuteDue = true;
				break;
			case runtime::BotTimingEvent::Upkeep:
				upkeepDue = true;
				break;
			}
		}
		if (!commandExecuteDue)
		{
			continue;
		}
		if (commandResetDue)
		{
			command_.resetManagedBotCommandTemplate(index);
		}
		if (!fullUpdateDue)
		{
			if (!command_.managedBotCommandTemplateValid(index))
			{
				prepareNeutralManagedBotCommand(index, handle, before);
			}
			if (upkeepDue && command_.managedBotCommandTemplateValid(index))
				command_.applyCombatUpkeep(index, behavior_.upkeepCombat(index, before,
					command_.managedBotCommandTemplates(index).movement.buttons));
			const runtime::CommandReceipt receipt =
				command_.executeManagedBotCommand(index, handle);
			navigation_.recordMovementPhysicsSample(index, before, receipt);
			continue;
		}
		RuntimeProfilerScope fullUpdateScope(
			runtimeProfiler_, RuntimeProfilerStage::RuntimeFullUpdate);
		behavior_.clearCombatUpkeep(index);
		if (managedBotFullUpdateSequences_[index] ==
			(std::numeric_limits<std::uint32_t>::max)())
		{
			managedBotFullUpdateSequences_[index] = 0U;
		}
		else
		{
			++managedBotFullUpdateSequences_[index];
		}
		navigation_.observeFullUpdateMotion(index, handle, before, configuration);
		behavior_.updateManagedBotCompatibilityState(index, before);
		behavior_.updateOutgoingPublicRadio(index);
		if (behavior_.managedBotStateMachines(index).behaviorDecision().action ==
			compat::StateAction::Buy)
		{
			behavior_.updatePublicBuying(index);
		}
		const auto preparation = navigation_.prepareMovement(index, handle, before, movementUnavailable);
		if (preparation.kind != MovementPreparationKind::Ready)
		{
			CombatUpkeepUpdate clearFire{}; clearFire.setPrimary = true;
			command_.applyCombatUpkeep(index, clearFire);
			if (preparation.kind == MovementPreparationKind::Neutral)
			{
				prepareNeutralManagedBotCommand(index, handle, before);
				const auto receipt = command_.executeManagedBotCommand(index, handle);
				navigation_.recordMovementPhysicsSample(index, before, receipt);
				navigation_.completePreparation(index, preparation.completion);
			}
			continue;
		}
		if (behavior_.combatMovementActive(index))
		{
			prepareNeutralManagedBotCommand(index, handle, before);
			const auto receipt = command_.executeManagedBotCommand(index, handle);
			navigation_.recordMovementPhysicsSample(index, before, receipt);
			continue;
		}
		NavigationUpdate movement = navigation_.updateNavigation(index, handle, before, navigation,
																 performanceDisablePathSearch_);
		auto &locomotionIntent = movement.locomotionIntent;
		auto &roamDecision = movement.roamDecision;
		const auto roamResult = movement.roamResult;
		behavior_.finishManagedBotNavigation(index, before, roamResult, roamDecision,
											 &locomotionIntent);
		navigation_.recordNavigationUpdate(index, before, movement);
		if (roamResult != runtime::NavRoamResult::IntentReady)
		{
			if (roamDecision.failureReason == runtime::NavFailureReason::None &&
				roamResult!=runtime::NavRoamResult::MovementSuppressed && roamResult!=runtime::NavRoamResult::TargetReached &&
				roamResult!=runtime::NavRoamResult::ReplanRequired)
			{
				roamDecision.failureReason = runtime::NavFailureReason::MovementNotProduced;
			}
			navigation_.logMovementDiagnostic(index, "roam_no_intent", &roamDecision);
			prepareNeutralManagedBotCommand(index, handle, before);
			const runtime::CommandReceipt receipt =
				command_.executeManagedBotCommand(index, handle);
			navigation_.recordMovementPhysicsSample(index, before, receipt);
			continue;
		}

		if (!lifecycle_.isCurrent(token) || command_.managedBotCommandSequences(index) ==
												(std::numeric_limits<std::uint32_t>::max)())
		{
			continue;
				}

				buildAndExecuteMovement(index, handle, token, before, movement);
}

}

std::uint32_t PluginRuntime::publicPlayerPriority(const edict_t *entity) const
{
	const auto actor = observation_.publicPlayerIdentity(entity);
	if (actor.slot == 0U) return (std::numeric_limits<std::uint32_t>::max)();
	const auto index = static_cast<std::size_t>(actor.slot - 1U);
	const int team = observation_.resolvePlayerTeam(entity);
	bool important = team == 1 && (static_cast<std::uint32_t>(entity->v.weapons) & (1U << ActionAdapter::kC4WeaponId)) != 0U;
	const auto &score = observation_.publicScoreStatuses(index)[index];
	if (team == 2 && score.known && score.serial == static_cast<std::uint32_t>(entity->serialnumber) &&
		score.frame.mapGeneration == lifecycle_.mapGeneration() && score.frame.roundGeneration == lifecycle_.roundGeneration())
		important = (score.flags & 4) != 0;
	return runtime::playerAvoidancePriority((entity->v.flags & FL_FAKECLIENT) == 0,
		important, managedBotSlots_[index] && managedBotHandles_[index].entity == entity
			? managedBotCreationIds_[index] : 0U);
}

void PluginRuntime::onTouch(edict_t *entity, edict_t *other)
{
	command_.onTouch(entity, other);
}

void PluginRuntime::resetManagedBotTiming(std::size_t index, float spawnTime)
{
	if (index >= managedBotTiming_.size())
	{
		return;
	}
	managedBotTiming_[index].reset(spawnTime);
	command_.resetJumpCrouch(index);
	command_.resetManagedBotCommandTemplate(index);
}

void PluginRuntime::resetManagedBotExecutionState()
{
	navigation_.resetSharedExecutionState();

	behavior_.resetSharedExecutionState();
	command_.resetSharedExecutionState();

	for (std::size_t index = 0U; index < NativeBotObservation::kClientSlotCount; ++index)
	{
		navigation_.resetActorExecutionState(index);

		behavior_.resetActorExecutionState(index);

		observation_.resetActorExecutionState(index);

		managedBotTiming_[index] = runtime::BotTimingScheduler();
		command_.resetActorExecutionState(index);
	}

	managedBotFullUpdateSequences_.fill(0U);
	observation_.resetSharedExecutionState();
		}

NativeBotObservation PluginRuntime::collectNativeBotObservation() const
		{
			NativeBotObservation observation{};
			if (engineFunctions_ == nullptr || engineFunctions_->pfnPEntityOfEntIndex == nullptr)
			{
				return observation;
			}
			observation.clientObservationAvailable = true;
			observation.managedClientSlots = managedBotSlots_;

			int clientMax = static_cast<int>(NativeBotObservation::kClientSlotCount);
			if (globals_ != nullptr && globals_->maxClients > 0)
			{
				clientMax = (std::min)(globals_->maxClients,
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
			const bool compatibilityControlsOwned =
				compatibilityCvarOwned_[static_cast<std::size_t>(CompatibilityCvarIndex::Enable)] &&
				compatibilityCvarOwned_[static_cast<std::size_t>(CompatibilityCvarIndex::Quota)];
			if (engineFunctions_->pfnCVarGetPointer == nullptr ||
				engineFunctions_->pfnCVarGetFloat == nullptr ||
				engineFunctions_->pfnCVarSetFloat == nullptr ||
				engineFunctions_->pfnCVarGetPointer("bot_enable") == nullptr ||
				engineFunctions_->pfnCVarGetPointer("bot_quota") == nullptr ||
				compatibilityControlsOwned)
			{
				return observation;
			}

			observation.controlsAvailable = true;
			observation.controlsWritable = true;
			observation.suppressionApplied = false;
			observation.botEnable = engineFunctions_->pfnCVarGetFloat("bot_enable");
			observation.botQuota = engineFunctions_->pfnCVarGetFloat("bot_quota");
			return observation;
		}

		void PluginRuntime::synchronizeNativeBotControls()
		{
			if (!nativeGuardEnabled_ || !nativeControlsCaptured_ || engineFunctions_ == nullptr ||
				engineFunctions_->pfnCVarSetFloat == nullptr)
			{
				return;
			}

			const NativeBotObservation observation = collectNativeBotObservation();
			if (!observation.controlsAvailable || !observation.controlsWritable)
			{
				return;
			}
			for (std::size_t index = 0U; index < observation.fakeClientSlots.size(); ++index)
			{
				if (observation.fakeClientSlots[index] && !managedBotSlots_[index])
				{
					return;
				}
			}
			if (observation.botEnable == 0.0f && observation.botQuota == 0.0f)
			{
				return;
			}

			compat::CvarUpdateResult enableResult = compat::CvarUpdateResult::NoChange;
			if (observation.botEnable > 0.0f)
			{
				enableResult = compatibilitySurface_.setFloat("bot_enable", 1.0f);
			}
			compat::CvarUpdateResult quotaResult = compat::CvarUpdateResult::NoChange;
			if (observation.botQuota > 0.0f)
			{
				quotaResult = compatibilitySurface_.setFloat("bot_quota", observation.botQuota);
			}
			if (enableResult == compat::CvarUpdateResult::InvalidValue ||
				quotaResult == compat::CvarUpdateResult::InvalidValue ||
				enableResult == compat::CvarUpdateResult::Unknown ||
				quotaResult == compat::CvarUpdateResult::Unknown)
			{
				return;
			}
			engineFunctions_->pfnCVarSetFloat("bot_enable", 0.0f);
			engineFunctions_->pfnCVarSetFloat("bot_quota", 0.0f);
		}

		void PluginRuntime::updateNativeBotGuard()
		{
			const NativeBotGuardDecision previousDecision = nativeGuardDecision_;
			NativeBotObservation observation = collectNativeBotObservation();
			observation.suppressionApplied =
				nativeGuardEnabled_ && nativeControlsCaptured_ && observation.controlsAvailable &&
				observation.controlsWritable && observation.botEnable == 0.0f &&
				observation.botQuota == 0.0f;
			nativeGuardDecision_ = nativeBotGuard_.evaluate(observation);
			logNativeBotGuardTransition(previousDecision);
		}

		bool PluginRuntime::armNativeBotGuard()
		{
			NativeBotObservation observation = collectNativeBotObservation();
			if (!observation.controlsAvailable || !observation.controlsWritable)
			{
				nativeGuardDecision_ = nativeBotGuard_.evaluate(observation);
				return nativeGuardDecision_.managedBotCreationAllowed;
			}

			if (!nativeControlsCaptured_)
			{
				originalBotEnable_ = observation.botEnable;
				originalBotQuota_ = observation.botQuota;
				nativeControlsCaptured_ = true;
				compatibilitySurface_.setFloat("bot_enable",
											   originalBotEnable_ > 0.0f ? 1.0f : 0.0f);
				compatibilitySurface_.setFloat("bot_quota", originalBotQuota_);
			}
			engineFunctions_->pfnCVarSetFloat("bot_enable", 0.0f);
			engineFunctions_->pfnCVarSetFloat("bot_quota", 0.0f);
			nativeGuardEnabled_ = true;
			updateNativeBotGuard();
			return nativeGuardDecision_.managedBotCreationAllowed;
		}

		void PluginRuntime::resetNativeBotGuard()
		{
			nativeGuardDecision_ = {NativeBotGuardState::Unsupported,
									NativeBotGuardReason::ControlsUnavailable, false};
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

			const char *state = nativeGuardDecision_.state == NativeBotGuardState::Conflict
									? "conflict"
									: "unsupported";
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
			gpMetaUtilFuncs->pfnLogError(pluginId_,
										 "native guard state=%s reason=%s map=%u round=%u frame=%u",
										 state, reason, lifecycle_.mapGeneration(),
										 lifecycle_.roundGeneration(), adapterFrameCount_);
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
			synchronizeNativeBotControls();
			synchronizeCompatibilityCvars();

			const int argumentCount = engineFunctions_->pfnCmd_Argc();
			if (argumentCount < 1 || argumentCount > 3)
			{
				return;
			}
			compat::CommandRequest request = {engineFunctions_->pfnCmd_Argv(0),
											  static_cast<std::size_t>(argumentCount - 1),
											  {nullptr, nullptr}};
			for (std::size_t index = 0U; index < request.argumentCount; ++index)
			{
				request.arguments[index] =
					engineFunctions_->pfnCmd_Argv(static_cast<int>(index + 1U));
			}
			const CompatibilityCommandResult result = executeCompatibilityCommand(request);
			if (gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr &&
				pluginId_ != nullptr)
			{
				importantLogConsole(runtimeProfiler_,
					pluginId_, "compat command=%s result=%d mode=%s map=%u round=%u frame=%u",
					request.name != nullptr ? request.name : "<null>", static_cast<int>(result),
					runtimeModeName(compatibilitySurface_.configuration().mode),
					lifecycle_.mapGeneration(), lifecycle_.roundGeneration(), adapterFrameCount_);
			}
		}

		compat::CvarUpdateResult PluginRuntime::setCompatibilityFloat(const char *name, float value)
		{
			return compatibilitySurface_.setFloat(name, value);
		}

		compat::CvarUpdateResult PluginRuntime::setCompatibilityString(const char *name,
																	   const char *value)
		{
			return compatibilitySurface_.setString(name, value);
		}

		ProfileLoadResult PluginRuntime::loadCompatibilityProfiles(const char *path)
		{
			return compatibilitySurface_.loadProfiles(path);
		}

		CompatibilityCommandResult
		PluginRuntime::executeCompatibilityCommand(const compat::CommandRequest &request)
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
			if (state_ != State::ActiveMap &&
				(action.id == compat::CommandId::Add || action.id == compat::CommandId::Kick ||
				 action.id == compat::CommandId::Kill))
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
			case compat::CommandId::Add: {
				if (action.target != nullptr && findManagedBot(action.target) != nullptr)
				{
					return CompatibilityCommandResult::NameTaken;
				}
				const compat::CvarSnapshot configuration = compatibilitySurface_.configuration();
				compat::ProfileRecord profile = {};
				std::array<const char *, 32U> usedNames{};
				std::size_t usedCount = 0U;
				if (engineFunctions_ != nullptr && engineFunctions_->pfnPEntityOfEntIndex != nullptr &&
					engineFunctions_->pfnSzFromIndex != nullptr && globals_ != nullptr)
				{
					for (int slot = 1; slot <= (std::min)(32, globals_->maxClients); ++slot)
					{
						edict_t *entity = engineFunctions_->pfnPEntityOfEntIndex(slot);
						if (entity != nullptr && !entity->free && entity->v.netname != 0)
							usedNames[usedCount++] = engineFunctions_->pfnSzFromIndex(entity->v.netname);
					}
				}
				const compat::ProfileSelectionResult profileResult =
					compatibilitySurface_.selectUnusedProfile(action, configuration.botDifficulty,
						usedNames.data(), usedCount, compatibilityRandomSource_, {}, {}, &profile);
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
				const FakeClientResult createResult = createFakeClient(profile.name, &handle);
				if (createResult != FakeClientResult::Created)
				{
					return mapFakeClientResult(createResult);
				}
				if (handle.actor.slot < runtime::LifecycleSession::kFirstClientSlot ||
					handle.actor.slot > runtime::LifecycleSession::kClientSlotCount)
				{
					(void)removeFakeClient(&handle);
					return CompatibilityCommandResult::ActorOperationFailed;
				}
				const std::size_t slotIndex = static_cast<std::size_t>(handle.actor.slot - 1U);
				behavior_.assignProfile(slotIndex, profile);
				const JoinAction beginAction =
					joinControllers_[slotIndex].begin(handle.actor, action.team, adapterFrameCount_);
				if (beginAction.kind == JoinActionKind::Failed)
				{
					(void)removeFakeClient(&handle);
					return CompatibilityCommandResult::ActorOperationFailed;
				}
				if (!populationAdjustment_)
				{
					const int quota = (std::min)(globals_ != nullptr ? globals_->maxClients : 32,
						configuration.botQuota + 1);
					(void)compatibilitySurface_.setFloat("bot_quota", static_cast<float>(quota));
					if (engineFunctions_ != nullptr && engineFunctions_->pfnCVarSetFloat != nullptr)
					{
						engineFunctions_->pfnCVarSetFloat("bot_quota", static_cast<float>(quota));
					}
				}
				return CompatibilityCommandResult::Handled;
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

		bool PluginRuntime::dispatchClientCommand(edict_t *entity, const char *name,
															  const char *argument)
		{
			// Meta_Attach receives a private copy of the GameDLL table.  Commands
			// synthesized for a FakeClient must use Metamod's hooked dispatcher so
			// the normal GameDLL command chain and command-argument hooks remain
			// active, just like a real client command.
			const bool hookedClientCommandAvailable =
				hookedGameDllFunctions_.dllapi_table != nullptr &&
				hookedGameDllFunctions_.dllapi_table->pfnClientCommand != nullptr;
			gamedll_funcs_t *dispatchFunctions =
				hookedClientCommandAvailable ? &hookedGameDllFunctions_ : gameDllFunctions_;
			if (entity == nullptr || clientCommandContextActive_ || dispatchFunctions == nullptr ||
				dispatchFunctions->dllapi_table == nullptr ||
				dispatchFunctions->dllapi_table->pfnClientCommand == nullptr ||
				!copyCommandText(&clientCommandArgv0_, name) ||
				(argument != nullptr && !copyCommandText(&clientCommandArgv1_, argument)))
			{
				return false;
			}
			clientCommandArgv1_.fill('\0');
			clientCommandArgs_.fill('\0');
			clientCommandArgumentCount_ = 1;
			if (argument != nullptr)
			{
				if (!copyCommandText(&clientCommandArgv1_, argument) ||
					!copyCommandText(&clientCommandArgs_, argument))
				{
					return false;
				}
				clientCommandArgumentCount_ = 2;
			}
			clientCommandContextActive_ = true;
			dispatchFunctions->dllapi_table->pfnClientCommand(entity);
			clientCommandContextActive_ = false;
			clientCommandArgumentCount_ = 0;
			clientCommandArgv0_.fill('\0');
			clientCommandArgv1_.fill('\0');
			clientCommandArgs_.fill('\0');
			return true;
		}

		const char *PluginRuntime::commandArgs() const
		{
			if (clientCommandContextActive_)
			{
				return clientCommandArgs_.data();
			}
			return originalCommandArgs_ != nullptr ? originalCommandArgs_() : "";
		}

		const char *PluginRuntime::commandArgv(int index) const
		{
			if (clientCommandContextActive_)
			{
				if (index == 0)
				{
					return clientCommandArgv0_.data();
				}
				if (index == 1 && clientCommandArgumentCount_ > 1)
				{
					return clientCommandArgv1_.data();
				}
				return "";
			}
			return originalCommandArgv_ != nullptr ? originalCommandArgv_(index) : "";
		}

		int PluginRuntime::commandArgc() const
		{
			if (clientCommandContextActive_)
			{
				return clientCommandArgumentCount_;
			}
			return originalCommandArgc_ != nullptr ? originalCommandArgc_() : 0;
		}

		FakeClientResult PluginRuntime::createFakeClient(const char *name, FakeClientHandle *handle)
		{
			configureFakeClientManager();
			const FakeClientResult result = fakeClientManager_.create(name, handle);
			if (result == FakeClientResult::Created && handle != nullptr &&
				handle->actor.slot >= 1U &&
				handle->actor.slot <= NativeBotObservation::kClientSlotCount)
				{
					const std::size_t slotIndex = static_cast<std::size_t>(handle->actor.slot - 1U);
					managedBotSlots_[slotIndex] = true;
					navigation_.resetCreatedActorRoute(slotIndex);
					managedBotTiming_[slotIndex] = runtime::BotTimingScheduler();
					command_.resetCreatedActorCommand(slotIndex);

					navigation_.resetCreatedActorSampling(slotIndex);

					command_.resetCreatedActorGateDiagnostic(slotIndex);
					navigation_.resetCreatedActorReadiness(slotIndex);

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
			return command_.enqueue(command);
		}

		runtime::CommandReceipt PluginRuntime::dispatchBotInput(const runtime::ActorId &actor,
																std::uint32_t dispatchFrame)
		{
			return command_.dispatchInput(actor, dispatchFrame);
		}

		void PluginRuntime::configureFakeClientManager()
		{
			const bool allowed =
				state_ == State::ActiveMap && nativeGuardDecision_.managedBotCreationAllowed;
			fakeClientManager_.configure(engineFunctions_, gameDllFunctions_, allowed,
										 gpMetaUtilFuncs, pluginId_);
			inputDispatcher_.configure(engineFunctions_, &runtimeProfiler_);
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

void PluginRuntime::rememberManagedBot(const FakeClientHandle &handle, const char *name)
		{
			if (name == nullptr || handle.actor.slot < 1U ||
				handle.actor.slot > NativeBotObservation::kClientSlotCount)
			{
				return;
			}
	const std::size_t index = static_cast<std::size_t>(handle.actor.slot - 1U);
	managedBotHandles_[index] = handle;
	if (++nextBotCreationId_ == 0U) ++nextBotCreationId_;
	managedBotCreationIds_[index] = nextBotCreationId_;
	command_.onActorRemembered(index);
	observation_.onActorRemembered(index, handle);

	behavior_.onActorRemembered(index, handle);

	managedBotFullUpdateSequences_[index] = 0U;

	managedBotSlots_[index] = true;

			const std::size_t length = std::strlen(name);
			const std::size_t copyLength = length < compat::ProfileRecord::kNameCapacity
											   ? length
											   : compat::ProfileRecord::kNameCapacity;
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
	managedBotCreationIds_[index] = 0U;
	command_.onActorRemoved(index);
	observation_.onActorRemoved(index);

	behavior_.onActorRemoved(index);

	navigation_.onActorRemoved(index);

	managedBotTiming_[index] = runtime::BotTimingScheduler();

	managedBotFullUpdateSequences_[index] = 0U;

			joinControllers_[index].reset();
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

		CompatibilityCommandResult
		PluginRuntime::executeRemoveCommand(const compat::CommandAction &action)
		{
			const bool removeAll = action.allTargets || action.target == nullptr;
			const auto reduceQuota = [this](std::size_t count)
			{
				const int quota = (std::max)(0, compatibilitySurface_.configuration().botQuota - static_cast<int>(count));
				(void)compatibilitySurface_.setFloat("bot_quota", static_cast<float>(quota));
				if (engineFunctions_ != nullptr && engineFunctions_->pfnCVarSetFloat != nullptr)
				{
					engineFunctions_->pfnCVarSetFloat("bot_quota", static_cast<float>(quota));
				}
			};
			if (!removeAll)
			{
				FakeClientHandle *handle = findManagedBot(action.target);
				if (handle == nullptr)
				{
					return CompatibilityCommandResult::NoTarget;
				}
				const auto result = removeFakeClient(handle);
				if (result == FakeClientResult::Removed)
				{
					reduceQuota(1U);
				}
				return mapFakeClientResult(result);
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
				return operationFailed ? CompatibilityCommandResult::ActorOperationFailed
									   : CompatibilityCommandResult::NoTarget;
			}
			reduceQuota(removedCount);
			return operationFailed ? CompatibilityCommandResult::ActorOperationFailed
								   : CompatibilityCommandResult::Handled;
		}

		CompatibilityCommandResult
		PluginRuntime::executeKillCommand(const compat::CommandAction &action)
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
				const FakeClientResult result = fakeClientManager_.kill(&managedBotHandles_[index]);
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
				return operationFailed ? CompatibilityCommandResult::ActorOperationFailed
									   : CompatibilityCommandResult::NoTarget;
			}
			return operationFailed ? CompatibilityCommandResult::ActorOperationFailed
								   : CompatibilityCommandResult::Handled;
		}

		CompatibilityCommandResult
		PluginRuntime::mapConfigurationResult(compat::BotActionResult result)
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

		CompatibilityCommandResult PluginRuntime::mapFakeClientResult(FakeClientResult result)
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
			compatibilitySurface_.registerCommands(engineFunctions_,
												   &HookCompatibilityServerCommand);
			compatibilityRegistrationInProgress_ = false;
		}

		void HookClientDisconnect(edict_t *entity)
		{
			PluginRuntime::instance().onClientDisconnect(entity);
			setMetaResult(MRES_IGNORED);
		}
		void HookTouch(edict_t *entity, edict_t *other)
		{
			PluginRuntime::instance().onTouch(entity, other);
			setMetaResult(MRES_IGNORED);
		}
		void HookCmdStart(const edict_t *entity, const usercmd_t *command, unsigned int randomSeed)
		{
			(void)randomSeed;
			PluginRuntime::instance().onCmdStart(entity, command != nullptr ? command->buttons : 0U);
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

		void HookStartFramePost()
		{
			PluginRuntime::instance().onStartFramePost();
			setMetaResult(MRES_IGNORED);
		}

		void HookMessageBegin(int messageDestination, int messageType,
							  const float *origin, edict_t *entity)
		{
			PluginRuntime::instance().onMessageBegin(
				messageDestination, messageType, origin, entity);
			setMetaResult(MRES_IGNORED);
		}

		void HookMessageEnd()
		{
			PluginRuntime::instance().onMessageEnd();
			setMetaResult(MRES_IGNORED);
		}

		void HookWriteByte(int value)
		{
			PluginRuntime::instance().onWriteByte(value);
			setMetaResult(MRES_IGNORED);
		}

		void HookWriteChar(int value)
		{
			PluginRuntime::instance().onWriteChar(value);
			setMetaResult(MRES_IGNORED);
		}

		void HookWriteShort(int value)
		{
			PluginRuntime::instance().onWriteShort(value);
			setMetaResult(MRES_IGNORED);
		}

		void HookWriteLong(int value)
		{
			PluginRuntime::instance().onWriteLong(value);
			setMetaResult(MRES_IGNORED);
		}

		void HookWriteCoord(float value)
		{
			PluginRuntime::instance().onWriteCoord(value);
			setMetaResult(MRES_IGNORED);
		}

		void HookEmitSound(edict_t *entity, int channel, const char *sample,
			float volume, float attenuation, int flags, int pitch)
		{
			(void)channel;
			(void)flags;
			(void)pitch;
			PluginRuntime::instance().onEmitSound(entity, sample, volume, attenuation);
			setMetaResult(MRES_IGNORED);
		}

		void HookWriteString(const char *value)
		{
			PluginRuntime::instance().onWriteString(value);
			setMetaResult(MRES_IGNORED);
		}

		void HookAlertMessage(ALERT_TYPE type, char *format, ...)
		{
			if (type == at_logged && format != nullptr)
			{
				char message[512] = {};
				va_list arguments;
				va_start(arguments, format);
#if defined(_WIN32)
				const int written = _vsnprintf_s(
					message, sizeof(message), _TRUNCATE, format, arguments);
#else
				const int written = std::vsnprintf(
					message, sizeof(message), format, arguments);
#endif
				va_end(arguments);
				if (written >= 0)
				{
					PluginRuntime::instance().onRoundLifecycleMessage(message);
				}
			}
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

		const char *HookCommandArgs()
		{
			PluginRuntime &runtime = PluginRuntime::instance();
			const char *value = runtime.commandArgs();
			setMetaResult(runtime.clientCommandContextActive_ ? MRES_SUPERCEDE : MRES_IGNORED);
			return value;
		}

		const char *HookCommandArgv(int index)
		{
			PluginRuntime &runtime = PluginRuntime::instance();
			const char *value = runtime.commandArgv(index);
			setMetaResult(runtime.clientCommandContextActive_ ? MRES_SUPERCEDE : MRES_IGNORED);
			return value;
		}

		int HookCommandArgc()
		{
			PluginRuntime &runtime = PluginRuntime::instance();
			const int value = runtime.commandArgc();
			setMetaResult(runtime.clientCommandContextActive_ ? MRES_SUPERCEDE : MRES_IGNORED);
			return value;
		}
		void PluginRuntime::processJoinControllers()
		{
	for (std::size_t index = 0U; index < joinControllers_.size(); ++index)
	{
		if (!managedBotSlots_[index] || managedBotHandles_[index].entity == nullptr)
		{
			if (managedBotSlots_[index])
			{
				cleanupManagedJoin(index, JoinError::InvalidActor);
			}
			else
			{
				joinControllers_[index].reset();
			}
			continue;
				}
				const runtime::ActorId &actor = managedBotHandles_[index].actor;
		const runtime::LifecycleToken token = lifecycle_.tokenForSlot(actor.slot);
		const runtime::ActorState actorState = actorRegistry_.state(actor);
				if (!joinControllers_[index].active())
				{
					continue;
				}
		if (!joinControllers_[index].isCurrent(actor) || !lifecycle_.isCurrent(token) ||
				(actorState != runtime::ActorState::Joining &&
				 actorState != runtime::ActorState::Joined))
		{
			cleanupManagedJoin(index, JoinError::InvalidActor);
			continue;
		}
		const JoinAction action = joinControllers_[index].onFrame(
			managedBotHandles_[index].entity, adapterFrameCount_);
				applyJoinAction(index, action);
	}
}

void PluginRuntime::cleanupManagedJoin(std::size_t index, JoinError error)
{
	if (index >= managedBotSlots_.size() || !managedBotSlots_[index])
	{
		return;
	}

	FakeClientHandle &handle = managedBotHandles_[index];
	const runtime::ActorId actor = handle.actor;
	const JoinPhase phase = joinControllers_[index].phase();
	const compat::CommandTeam requestedTeam = joinControllers_[index].requestedTeam();
	const int entityTeam = handle.entity != nullptr ? handle.entity->v.team : -1;
	const int deadflag = handle.entity != nullptr ? handle.entity->v.deadflag : -1;
	const int spectator = handle.entity != nullptr &&
			(handle.entity->v.flags & FL_SPECTATOR) != 0 ? 1 : 0;

	if (joinControllers_[index].active())
	{
		(void)joinControllers_[index].cancel(error);
	}

	FakeClientResult cleanupResult = FakeClientResult::NotFound;
	if (handle.entity != nullptr)
	{
		cleanupResult = removeFakeClient(&handle);
	}
	else
	{
		inputDispatcher_.unbindActor(actor);
		const runtime::ActorState actorState = actorRegistry_.state(actor);
		if (actorState == runtime::ActorState::Joining ||
				actorState == runtime::ActorState::Joined)
		{
			if (actorRegistry_.beginRemoval(actor) == runtime::ActorResult::Accepted)
			{
				lifecycle_.disconnectSlot(actor.slot);
				if (actorRegistry_.release(actor) == runtime::ActorResult::Accepted)
				{
					cleanupResult = FakeClientResult::Removed;
					clearManagedBot(index);
				}
			}
		}
		else if (actorState == runtime::ActorState::Vacant)
		{
			cleanupResult = FakeClientResult::Removed;
			clearManagedBot(index);
		}
	}

	if (cleanupResult != FakeClientResult::Removed)
	{
		(void)joinControllers_[index].commandFailed(JoinError::CleanupFailed);
	}

	if (gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr &&
			pluginId_ != nullptr)
	{
		importantLogConsole(runtimeProfiler_,
			pluginId_,
			"join cleanup slot=%u generation=%u map=%u round=%u frame=%u phase=%d team=%d "
			"entity_team=%d deadflag=%d spectator=%d error=%d result=%d",
			static_cast<unsigned int>(actor.slot),
			static_cast<unsigned int>(actor.actorGeneration),
			static_cast<unsigned int>(lifecycle_.mapGeneration()),
			static_cast<unsigned int>(lifecycle_.roundGeneration()),
			static_cast<unsigned int>(adapterFrameCount_),
			static_cast<int>(phase),
			static_cast<int>(requestedTeam), entityTeam,
			deadflag, spectator, static_cast<int>(error),
			static_cast<int>(cleanupResult));
	}
}

void PluginRuntime::applyJoinAction(std::size_t index, const JoinAction &action)
		{
	if (index >= joinControllers_.size() || !managedBotSlots_[index])
	{
		return;
	}
	const FakeClientHandle &handle = managedBotHandles_[index];
	const runtime::ActorId actor = handle.actor;
	const int entityTeam = handle.entity != nullptr ? handle.entity->v.team : -1;
	const int deadflag = handle.entity != nullptr ? handle.entity->v.deadflag : -1;
	const int spectator = handle.entity != nullptr &&
			(handle.entity->v.flags & FL_SPECTATOR) != 0 ? 1 : 0;
	const int requestedTeam = static_cast<int>(joinControllers_[index].requestedTeam());
	if (action.kind == JoinActionKind::Joined)
			{
				const runtime::ActorResult result = actorRegistry_.markJoined(
						managedBotHandles_[index].actor);
				if (gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr &&
						pluginId_ != nullptr)
		{
			importantLogConsole(runtimeProfiler_,
				pluginId_,
				"join confirmed slot=%u generation=%u map=%u round=%u phase=%d team=%d "
				"entity_team=%d deadflag=%d spectator=%d actor_result=%d frame=%u",
				static_cast<unsigned int>(actor.slot),
				static_cast<unsigned int>(actor.actorGeneration),
				static_cast<unsigned int>(lifecycle_.mapGeneration()),
				static_cast<unsigned int>(lifecycle_.roundGeneration()),
				static_cast<int>(joinControllers_[index].phase()), requestedTeam, entityTeam,
				deadflag, spectator, static_cast<int>(result),
				static_cast<unsigned int>(adapterFrameCount_));
		}
		if (result != runtime::ActorResult::Accepted)
		{
			cleanupManagedJoin(index, JoinError::InvalidActor);
		}
		return;
			}
			if (action.kind == JoinActionKind::Failed || action.kind == JoinActionKind::Cancelled)
			{
				if (gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr &&
						pluginId_ != nullptr)
		{
			importantLogConsole(runtimeProfiler_,
				pluginId_,
				"join controller stopped slot=%u generation=%u action=%d error=%d map=%u "
				"round=%u phase=%d team=%d entity_team=%d deadflag=%d spectator=%d frame=%u",
				static_cast<unsigned int>(actor.slot),
				static_cast<unsigned int>(actor.actorGeneration), static_cast<int>(action.kind),
				static_cast<int>(action.error),
				static_cast<unsigned int>(lifecycle_.mapGeneration()),
				static_cast<unsigned int>(lifecycle_.roundGeneration()),
				static_cast<int>(joinControllers_[index].phase()), requestedTeam, entityTeam,
				deadflag, spectator, static_cast<unsigned int>(adapterFrameCount_));
				}
		cleanupManagedJoin(index, action.error);
			return;
		}
	if (action.kind == JoinActionKind::None)
	{
		return;
	}
	if (action.kind != JoinActionKind::SendMenuSelect || action.selection == 0U ||
			action.selection > 9U)
	{
		cleanupManagedJoin(index, JoinError::MenuOptionUnavailable);
			return;
			}
			const char *command = "menuselect";
			switch (action.command)
			{
			case JoinCommandKind::JoinTeam:
				command = "jointeam";
				break;
			case JoinCommandKind::JoinClass:
				command = "joinclass";
				break;
			case JoinCommandKind::MenuSelect:
			default:
				break;
			}
			const char selection[] = {
				static_cast<char>('0' + action.selection),
				'\0'};
			const bool dispatched =
				command_.dispatchClientCommand(managedBotHandles_[index].entity, command, selection);
			if (gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr &&
					pluginId_ != nullptr)
			{
				profiledLogConsole(runtimeProfiler_,
					pluginId_,
					"join dispatch slot=%u generation=%u command=%s selection=%u dispatched=%d "
					"map=%u round=%u phase=%d team=%d entity_team=%d deadflag=%d spectator=%d",
					static_cast<unsigned int>(actor.slot),
					static_cast<unsigned int>(actor.actorGeneration), command,
					static_cast<unsigned int>(action.selection), dispatched ? 1 : 0,
					static_cast<unsigned int>(lifecycle_.mapGeneration()),
					static_cast<unsigned int>(lifecycle_.roundGeneration()),
					static_cast<int>(joinControllers_[index].phase()), requestedTeam, entityTeam,
					deadflag, spectator);
			}
			if (dispatched)
			{
				const JoinAction completed =
						joinControllers_[index].commandCompleted(adapterFrameCount_);
				applyJoinAction(index, completed);
			}
		else
		{
			const JoinAction failed =
				joinControllers_[index].commandFailed(JoinError::CommandDispatchFailed);
			applyJoinAction(index, failed);
		}
		}
	} // namespace metamod
} // namespace astrabot
