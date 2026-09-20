#include "plugin_runtime.hpp"
#include "astrabot/metamod/movement_execution_gate.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>

meta_globals_t *gpMetaGlobals = nullptr;
gamedll_funcs_t *gpGamedllFuncs = nullptr;
mutil_funcs_t *gpMetaUtilFuncs = nullptr;

namespace astrabot
{
	namespace metamod
	{
			namespace
		{
	constexpr float kRadiansToDegrees = 57.29577951308232f;
	constexpr float kStandingHalfHumanHeight = 36.0f;
	constexpr float kMaximumObjectiveDistance = 10000.0f;
	constexpr float kDefaultManagedBotMaxSpeed = 240.0f;
			constexpr std::uint32_t kRespawnSettleFrames = 2U;
			constexpr std::uint8_t kMovementPhysicsLogLimit = 64U;
		// Counter-Strike 1.6/ReHLDS registers these user messages before the
		// plugin utility lookup is available.  Keep the public lookup as the
		// first choice, but retain the engine's stable IDs as a bounded fallback.
	constexpr int kShowMenuMessageId = 96;
	constexpr int kVguiMenuMessageId = 114;
	constexpr int kTeamInfoMessageId = 86;

	const char *actionKindName(ActionKind kind)
	{
		switch (kind)
		{
		case ActionKind::Fire:
			return "attack";
		case ActionKind::Reload:
			return "reload";
		case ActionKind::Plant:
			return "plant";
		case ActionKind::Defuse:
			return "defuse";
		case ActionKind::None:
		default:
			return "none";
		}
	}

	const char *runtimeModeName(compat::RuntimeMode mode)
	{
		return mode == compat::RuntimeMode::Enhanced ? "enhanced" : "compatibility";
	}

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

			bool boundedStringLength(const char *value, std::size_t maximum, std::size_t *length)
			{
				if (value == nullptr || length == nullptr)
				{
					return false;
				}
				for (std::size_t index = 0U; index < maximum; ++index)
				{
					if (value[index] == '\0')
					{
						*length = index;
						return true;
					}
				}
				return false;
			}

			template <std::size_t Capacity>
			bool copyCommandText(std::array<char, Capacity> *destination, const char *source)
			{
				if (destination == nullptr || source == nullptr)
				{
					return false;
				}
				const std::size_t length = std::strlen(source);
				if (length >= destination->size())
				{
					return false;
				}
				std::memcpy(destination->data(), source, length);
				(*destination)[length] = '\0';
				return true;
			}

			bool buildMapPath(const char *gameDirectory, const char *mapName, const char *extension,
							  char *path, std::size_t pathCapacity)
			{
				std::size_t gameDirectoryLength = 0U;
				std::size_t mapNameLength = 0U;
				std::size_t extensionLength = 0U;
				if (!boundedStringLength(gameDirectory, pathCapacity, &gameDirectoryLength) ||
					!boundedStringLength(mapName, pathCapacity, &mapNameLength) ||
					!boundedStringLength(extension, pathCapacity, &extensionLength) ||
					path == nullptr || gameDirectoryLength == 0U)
				{
					return false;
				}

				const bool needsSeparator = gameDirectory[gameDirectoryLength - 1U] != '/' &&
											gameDirectory[gameDirectoryLength - 1U] != '\\';
				const std::size_t separatorLength = needsSeparator ? 1U : 0U;
				const std::size_t requiredLength =
					gameDirectoryLength + separatorLength + 5U + mapNameLength + extensionLength;
				if (requiredLength >= pathCapacity)
				{
					return false;
				}

				std::size_t position = 0U;
				std::memcpy(path + position, gameDirectory, gameDirectoryLength);
				position += gameDirectoryLength;
				if (needsSeparator)
				{
					path[position++] = '/';
				}
				std::memcpy(path + position, "maps/", 5U);
				position += 5U;
				std::memcpy(path + position, mapName, mapNameLength);
				position += mapNameLength;
				std::memcpy(path + position, extension, extensionLength);
				path[requiredLength] = '\0';
				return true;
			}

			bool buildGameFilePath(const char *gameDirectory, const char *fileName, char *path,
								   std::size_t pathCapacity)
			{
				std::size_t gameDirectoryLength = 0U;
				std::size_t fileNameLength = 0U;
				if (!boundedStringLength(gameDirectory, pathCapacity, &gameDirectoryLength) ||
					!boundedStringLength(fileName, pathCapacity, &fileNameLength) ||
					path == nullptr || gameDirectoryLength == 0U || fileNameLength == 0U)
				{
					return false;
				}

				const bool needsSeparator = gameDirectory[gameDirectoryLength - 1U] != '/' &&
											gameDirectory[gameDirectoryLength - 1U] != '\\';
				const std::size_t separatorLength = needsSeparator ? 1U : 0U;
				const std::size_t requiredLength =
					gameDirectoryLength + separatorLength + fileNameLength;
				if (requiredLength >= pathCapacity)
				{
					return false;
				}

				std::size_t position = 0U;
				std::memcpy(path + position, gameDirectory, gameDirectoryLength);
				position += gameDirectoryLength;
				if (needsSeparator)
				{
					path[position++] = '/';
				}
				std::memcpy(path + position, fileName, fileNameLength);
				path[requiredLength] = '\0';
				return true;
			}

	float movementYaw(const nav::NavVector &direction)
	{
		if (!std::isfinite(direction.x) || !std::isfinite(direction.y))
				{
					return 0.0f;
				}
		return std::atan2(direction.y, direction.x) * kRadiansToDegrees;
	}

float readOptionalCvarFloat(enginefuncs_t *engineFunctions, const char *name)
	{
		if (engineFunctions == nullptr || name == nullptr ||
			engineFunctions->pfnCVarGetPointer == nullptr ||
			engineFunctions->pfnCVarGetFloat == nullptr ||
			engineFunctions->pfnCVarGetPointer(name) == nullptr)
		{
			return 0.0f;
		}
	return engineFunctions->pfnCVarGetFloat(name);
}

void addNavSearchStats(nav::NavSearchStats *total, const nav::NavSearchStats &sample)
{
	if (total == nullptr)
	{
		return;
	}
	total->expandedUniqueAreas += sample.expandedUniqueAreas;
	total->enqueueCount += sample.enqueueCount;
	total->reopenCount += sample.reopenCount;
	total->staleQueueEntries += sample.staleQueueEntries;
	total->equalCostReplacements += sample.equalCostReplacements;
	total->searchCalls += sample.searchCalls;
	total->successCount += sample.successCount;
	total->failureCount += sample.failureCount;
	total->totalUsec += sample.totalUsec;
	if (sample.maxUsec > total->maxUsec)
	{
		total->maxUsec = sample.maxUsec;
	}
	if (sample.firstSearchId != 0U)
	{
		if (total->firstSearchId == 0U)
		{
			total->firstSearchId = sample.firstSearchId;
		}
		total->lastSearchId = sample.lastSearchId;
	}
}

	const char *runtimeProfilerStageName(RuntimeProfilerStage stage)
	{
		static const char *const names[] = {
			"StartFrame", "RegistryLifecycle", "Observation", "Vision", "TraceLine",
			"WorldPublish", "Perception", "RuntimeInput", "RuntimeFullUpdate",
			"NavCurrentAreaLookup", "PathSearch", "PathRecompute", "NavMovement",
			"MovementDispatch", "TraceSerialization"};
		const std::size_t index = static_cast<std::size_t>(stage);
		return index < static_cast<std::size_t>(RuntimeProfilerStage::Count)
			? names[index] : "Unknown";
	}

	class RuntimeProfilerScope
	{
	public:
		RuntimeProfilerScope(
			RuntimeProfiler &profiler, RuntimeProfilerStage stage) noexcept
			: profiler_(profiler), stage_(stage), enabled_(profiler.enabled()),
			  start_(enabled_ ? std::chrono::steady_clock::now()
						: std::chrono::steady_clock::time_point())
		{
		}

		~RuntimeProfilerScope()
		{
			if (!enabled_)
			{
				return;
			}
			const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
				std::chrono::steady_clock::now() - start_).count();
			profiler_.record(stage_, elapsed > 0 ? static_cast<std::uint64_t>(elapsed) : 0U);
		}

	private:
		RuntimeProfiler &profiler_;
		RuntimeProfilerStage stage_;
		bool enabled_;
		std::chrono::steady_clock::time_point start_;
	};

	bool entityObjectiveBounds(const edict_t *entity, nav::NavExtent *extent)
	{
		if (entity == nullptr || extent == nullptr)
		{
			return false;
		}
		const bool hasAbsoluteBounds =
			std::isfinite(entity->v.absmin.x) && std::isfinite(entity->v.absmin.y) &&
			std::isfinite(entity->v.absmin.z) && std::isfinite(entity->v.absmax.x) &&
			std::isfinite(entity->v.absmax.y) && std::isfinite(entity->v.absmax.z) &&
			entity->v.absmax.x >= entity->v.absmin.x &&
			entity->v.absmax.y >= entity->v.absmin.y &&
			entity->v.absmax.z >= entity->v.absmin.z &&
			(entity->v.absmax.x - entity->v.absmin.x > 1.0f ||
			 entity->v.absmax.y - entity->v.absmin.y > 1.0f ||
			 entity->v.absmax.z - entity->v.absmin.z > 1.0f);
		if (hasAbsoluteBounds)
		{
			extent->lo = {entity->v.absmin.x, entity->v.absmin.y, entity->v.absmin.z};
			extent->hi = {entity->v.absmax.x, entity->v.absmax.y, entity->v.absmax.z};
			return true;
		}
		const bool hasLocalBounds =
			std::isfinite(entity->v.mins.x) && std::isfinite(entity->v.mins.y) &&
			std::isfinite(entity->v.mins.z) && std::isfinite(entity->v.maxs.x) &&
			std::isfinite(entity->v.maxs.y) && std::isfinite(entity->v.maxs.z) &&
			entity->v.maxs.x >= entity->v.mins.x &&
			entity->v.maxs.y >= entity->v.mins.y &&
			entity->v.maxs.z >= entity->v.mins.z &&
			(entity->v.maxs.x - entity->v.mins.x > 1.0f ||
			 entity->v.maxs.y - entity->v.mins.y > 1.0f ||
			 entity->v.maxs.z - entity->v.mins.z > 1.0f);
		if (!hasLocalBounds)
		{
			const bool hasWorldSize =
				std::isfinite(entity->v.size.x) && std::isfinite(entity->v.size.y) &&
				std::isfinite(entity->v.size.z) &&
				(entity->v.size.x > 1.0f || entity->v.size.y > 1.0f ||
				 entity->v.size.z > 1.0f);
			if (!hasWorldSize)
			{
				return false;
			}
			extent->lo = {
				entity->v.origin.x - entity->v.size.x * 0.5f,
				entity->v.origin.y - entity->v.size.y * 0.5f,
				entity->v.origin.z - entity->v.size.z * 0.5f};
			extent->hi = {
				entity->v.origin.x + entity->v.size.x * 0.5f,
				entity->v.origin.y + entity->v.size.y * 0.5f,
				entity->v.origin.z + entity->v.size.z * 0.5f};
			return true;
		}
		extent->lo = {
			entity->v.origin.x + entity->v.mins.x,
			entity->v.origin.y + entity->v.mins.y,
			entity->v.origin.z + entity->v.mins.z};
		extent->hi = {
			entity->v.origin.x + entity->v.maxs.x,
			entity->v.origin.y + entity->v.maxs.y,
			entity->v.origin.z + entity->v.maxs.z};
		return true;
	}

	nav::NavVector entityObjectiveCenter(const edict_t *entity)
	{
		nav::NavExtent extent = {};
		if (entityObjectiveBounds(entity, &extent))
		{
			return {
				(extent.lo.x + extent.hi.x) * 0.5f,
				(extent.lo.y + extent.hi.y) * 0.5f,
				(extent.lo.z + extent.hi.z) * 0.5f};
		}
		return entity == nullptr
			? nav::NavVector{0.0f, 0.0f, 0.0f}
			: nav::NavVector{entity->v.origin.x, entity->v.origin.y, entity->v.origin.z};
	}

			std::uint8_t movementMilliseconds(const globalvars_t *globals)
			{
				if (globals == nullptr || !std::isfinite(globals->frametime) ||
						globals->frametime <= 0.0f)
				{
					return 50U;
				}

				const float milliseconds = globals->frametime * 1000.0f;
				if (milliseconds < 1.0f)
				{
					return 1U;
				}
				if (milliseconds >= 255.0f)
				{
					return 255U;
				}
				return static_cast<std::uint8_t>(milliseconds + 0.5f);
			}

			int localFileSize(const char *path)
			{
				if (path == nullptr)
				{
					return -1;
				}

	FILE *file = nullptr;
#ifdef _WIN32
	if (fopen_s(&file, path, "rb") != 0)
	{
		return -1;
	}
#else
	file = fopen(path, "rb");
#endif
	if (file == nullptr)
				{
					return -1;
				}
				if (fseek(file, 0L, SEEK_END) != 0)
				{
					fclose(file);
					return -1;
				}

				const long size = ftell(file);
				fclose(file);
				if (size < 0L || size > static_cast<long>((std::numeric_limits<int>::max)()))
				{
					return -1;
				}
				return static_cast<int>(size);
			}
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
constexpr std::size_t kPerformanceCvarCount = 3U;
cvar_t *const kPerformanceCvars[kPerformanceCvarCount] = {
	&kAstrabotPerfDisableVisionCvar,
	&kAstrabotPerfDisablePathSearchCvar,
	&kAstrabotPerfDisableTraceCvar};
const char *const kPerformanceCvarNames[kPerformanceCvarCount] = {
	kAstrabotPerfDisableVisionName,
	kAstrabotPerfDisablePathSearchName,
	kAstrabotPerfDisableTraceName};

		} // namespace

		PluginRuntime::PluginRuntime()
		: state_(State::Cold), metaGlobals_(nullptr), gameDllFunctions_(nullptr),
		  hookedGameDllFunctions_(),
		  engineFunctions_(nullptr), globals_(nullptr), lifecycle_(), actorRegistry_(),
			  fakeClientManager_(lifecycle_, actorRegistry_),
			  inputDispatcher_(lifecycle_, actorRegistry_), compatibilitySurface_(),
	compatibilityRandomSource_(), observationAdapter_(), runtimeProfiler_(), navLoader_(),
			  navPublisher_(), navLoadDiagnostic_(), adapterFrameCount_(0U), pluginId_(nullptr),
			  nativeBotGuard_(),
	nativeGuardDecision_({NativeBotGuardState::Unsupported,
	NativeBotGuardReason::ControlsUnavailable, false}),
	managedBotSlots_(), managedBotTeamNumbers_(), managedBotHandles_(), managedBotNames_(),
			  joinControllers_(),
		userMessageKind_(UserMessageKind::None), userMessageTarget_(nullptr),
		userMessageFieldCount_(0U), userMessageMenuType_(0U), userMessageNeedMore_(0U),
		userMessageValidSlots_(0U), userMessageTeamSlot_(0U), userMessageShowFragmentActive_(false),
		userMessageText_(),
		userMessageTextLength_(0U),
	managedBotMovement_(), managedBotCombat_(), managedBotObjectives_(),
	managedBotStateMachines_(), managedBotPerception_(),
	managedBotPerceptionFullUpdates_(), managedBotCommandSequences_(),
	managedBotFullUpdateSequences_(), managedBotStateRounds_(), managedBotWasDead_(),
	managedBotTiming_(), managedBotCommandTemplates_(),
	managedBotCommandTemplateValid_(),
			  movementDiagnosticSamples_(),
			  movementDiagnosticAttempts_(),
			  movementDiagnosticUnavailable_(),
			  movementResumeFrames_(),
			  movementLastDeadFrames_(),
			movementSettledDeadFrames_(),
		movementWarmupFrames_(),
		movementPhysicsSamples_(),
		movementReadyLogged_(),
		movementDispatchFrames_(),
			movementWasAirborne_(),
			  movementDiagnosticRound_(0U),
			  movementDiagnosticGlobal_(false),
			  originalCommandArgs_(nullptr), originalCommandArgv_(nullptr),
			  originalCommandArgc_(nullptr), clientCommandContextActive_(false),
			  clientCommandArgumentCount_(0), clientCommandArgv0_(), clientCommandArgv1_(),
			  clientCommandArgs_(), nativeGuardEnabled_(false), compatibilityCvarOwned_(),
	compatibilityRegistrationInProgress_(false), nativeControlsCaptured_(false),
	performanceDisablePathSearch_(false),
			  originalBotEnable_(0.0f), originalBotQuota_(0.0f)
		{
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
			resetManagedBotMovement();
			movementDiagnosticGlobal_ = false;
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
			resetManagedBotMovement();
			movementDiagnosticGlobal_ = false;
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
			requestedFunctions.pfnWriteByte = &HookWriteByte;
			requestedFunctions.pfnWriteChar = &HookWriteChar;
			requestedFunctions.pfnWriteShort = &HookWriteShort;
			requestedFunctions.pfnWriteString = &HookWriteString;
			*engineFunctions = requestedFunctions;
			return true;
		}

		void PluginRuntime::giveEnginePointers(enginefuncs_t *engineFunctions,
											   globalvars_t *globals)
		{
		if (gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr &&
				pluginId_ != nullptr)
			{
				gpMetaUtilFuncs->pfnLogConsole(
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
		observationAdapter_.configure(engineFunctions_, globals_);
		observationAdapter_.setProfiler(&runtimeProfiler_);
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
			runtimeProfiler_.setEnabled(
				engineFunctions_->pfnCVarGetFloat("astrabot_profile") > 0.0f,
			globals_ != nullptr ? static_cast<double>(globals_->time) : 0.0);
	}
	const bool disableVision = readOptionalCvarFloat(
		engineFunctions_, kAstrabotPerfDisableVisionName) > 0.0f;
	const bool disableTrace = readOptionalCvarFloat(
		engineFunctions_, kAstrabotPerfDisableTraceName) > 0.0f;
	performanceDisablePathSearch_ = readOptionalCvarFloat(
		engineFunctions_, kAstrabotPerfDisablePathSearchName) > 0.0f;
	observationAdapter_.setPerformanceToggles(disableVision, disableTrace);
}

		PluginRuntime::Snapshot PluginRuntime::snapshot() const
		{
			Snapshot current = {state_,
								compatibilitySurface_.configuration().mode,
								lifecycle_.mapGeneration(),
								lifecycle_.roundGeneration(),
								nativeGuardDecision_.state,
								nativeGuardDecision_.reason,
								nativeGuardDecision_.managedBotCreationAllowed};
			return current;
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
	return movementPhysicsSamples_[static_cast<std::size_t>(slot - 1U)];
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

		void PluginRuntime::onServerActivate(edict_t *edictList, int edictCount, int clientMax)
		{
			(void)edictList;
			(void)edictCount;
			(void)clientMax;
			if (gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr &&
				pluginId_ != nullptr)
			{
				gpMetaUtilFuncs->pfnLogConsole(
					pluginId_, "server activate state=%d engine=%p globals=%p",
					static_cast<int>(state_), static_cast<const void *>(engineFunctions_),
					static_cast<const void *>(globals_));
			}

			if (state_ == State::Attached)
			{
				if (lifecycle_.activateMap())
				{
					adapterFrameCount_ = 0U;
					state_ = State::ActiveMap;
					movementDiagnosticGlobal_ = false;
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
						gpMetaUtilFuncs->pfnLogConsole(
							pluginId_, "menu message ids show=%d vgui=%d team=%d",
							showMenuId, vguiMenuId, teamInfoId);
						const compat::RuntimeMode mode = compatibilitySurface_.configuration().mode;
						gpMetaUtilFuncs->pfnLogConsole(
							pluginId_, "runtime mode=%s map=%u round=%u",
							runtimeModeName(mode), lifecycle_.mapGeneration(), lifecycle_.roundGeneration());
					}
				}
			}
		}

		void PluginRuntime::onServerDeactivate()
		{
			if (state_ == State::ActiveMap)
			{
				lifecycle_.deactivateMap();
				navPublisher_.invalidate(lifecycle_.mapGeneration());
				navLoadDiagnostic_ = {};
				adapterFrameCount_ = 0U;
				updateNativeBotGuard();
			inputDispatcher_.reset();
			clearManagedBots();
			actorRegistry_ = runtime::ActorRegistry();
			resetManagedBotMovement();
			movementDiagnosticGlobal_ = false;
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
					gpMetaUtilFuncs->pfnLogConsole(
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
				gpMetaUtilFuncs->pfnLogConsole(
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
			synchronizeNativeBotControls();
				synchronizeCompatibilityCvars();
				updateNativeBotGuard();
				configureFakeClientManager();
			}

void PluginRuntime::onStartFramePost()
{
	if (state_ == State::ActiveMap)
	{
		{
			RuntimeProfilerScope profilerScope(
				runtimeProfiler_, RuntimeProfilerStage::RuntimeInput);
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
			updateManagedBotMovement();
			}
			if (globals_ != nullptr && runtimeProfiler_.enabled())
			{
				RuntimeProfilerReport report = {};
				if (runtimeProfiler_.consumeReport(
						static_cast<double>(globals_->time), &report) &&
					gpMetaUtilFuncs != nullptr &&
					gpMetaUtilFuncs->pfnLogConsole != nullptr && pluginId_ != nullptr)
				{
					for (std::size_t stageIndex = 0U;
						stageIndex < static_cast<std::size_t>(RuntimeProfilerStage::Count);
						++stageIndex)
					{
						const RuntimeProfilerStage stage =
							static_cast<RuntimeProfilerStage>(stageIndex);
						const RuntimeProfilerStageStats &stats = report.stages[stageIndex];
						const double average = stats.calls == 0U ? 0.0
							: static_cast<double>(stats.totalUsec) /
								static_cast<double>(stats.calls);
						gpMetaUtilFuncs->pfnLogConsole(
							pluginId_,
							"profile stage=%s calls=%u total_usec=%llu avg_usec=%.1f max_usec=%llu",
							runtimeProfilerStageName(stage),
							static_cast<unsigned int>(stats.calls),
							static_cast<unsigned long long>(stats.totalUsec), average,
							static_cast<unsigned long long>(stats.maxUsec));
					}
					gpMetaUtilFuncs->pfnLogConsole(
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
	applyJoinAction(
		index, joinControllers_[index].onMenu(menu, validSlots, adapterFrameCount_, source));
		}
				void PluginRuntime::onMessageBegin(int messageDestination, int messageType,
			const float *origin, edict_t *entity)
		{
			(void)messageDestination;
			(void)origin;
			const bool continueShowMenu = userMessageShowFragmentActive_;
			userMessageKind_ = UserMessageKind::None;
			userMessageTarget_ = nullptr;
			userMessageFieldCount_ = 0U;
			userMessageMenuType_ = 0U;
			userMessageNeedMore_ = 0U;
			userMessageValidSlots_ = 0U;
			userMessageTeamSlot_ = 0U;
			if (!continueShowMenu)
			{
				userMessageText_.fill('\0');
				userMessageTextLength_ = 0U;
			}
			userMessageShowFragmentActive_ = false;

			int messageSize = 0;
			int showMenuId = kShowMenuMessageId;
			int vguiMenuId = kVguiMenuMessageId;
			int teamInfoId = kTeamInfoMessageId;
			if (gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnGetUserMsgID != nullptr &&
				pluginId_ != nullptr)
			{
				const int resolvedShowMenuId =
					gpMetaUtilFuncs->pfnGetUserMsgID(pluginId_, "ShowMenu", &messageSize);
				const int resolvedVguiMenuId =
					gpMetaUtilFuncs->pfnGetUserMsgID(pluginId_, "VGUIMenu", &messageSize);
				const int resolvedTeamInfoId =
					gpMetaUtilFuncs->pfnGetUserMsgID(pluginId_, "TeamInfo", &messageSize);
				if (resolvedShowMenuId > 0)
				{
					showMenuId = resolvedShowMenuId;
				}
				if (resolvedVguiMenuId > 0)
				{
					vguiMenuId = resolvedVguiMenuId;
				}
				if (resolvedTeamInfoId > 0)
				{
					teamInfoId = resolvedTeamInfoId;
				}
			}
			if (showMenuId > 0 && messageType == showMenuId)
			{
				userMessageKind_ = UserMessageKind::ShowMenu;
			}
			else if (vguiMenuId > 0 && messageType == vguiMenuId)
			{
				userMessageKind_ = UserMessageKind::VguiMenu;
			}
			else if (teamInfoId > 0 && messageType == teamInfoId)
			{
				userMessageKind_ = UserMessageKind::TeamInfo;
			}
			if (userMessageKind_ == UserMessageKind::None ||
				(userMessageKind_ != UserMessageKind::TeamInfo && entity == nullptr))
			{
				return;
			}

			if (gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr &&
				pluginId_ != nullptr)
			{
				const int slot = entity != nullptr && engineFunctions_ != nullptr &&
						engineFunctions_->pfnIndexOfEdict != nullptr
					? engineFunctions_->pfnIndexOfEdict(entity)
					: 0;
				gpMetaUtilFuncs->pfnLogConsole(
					pluginId_, "menu message begin kind=%d type=%d slot=%d frame=%u",
					static_cast<int>(userMessageKind_), messageType, slot,
					static_cast<unsigned int>(adapterFrameCount_));
			}
			userMessageTarget_ = entity;
		}

		void PluginRuntime::onMessageEnd()
		{
	if (userMessageKind_ == UserMessageKind::VguiMenu && userMessageTarget_ != nullptr)
	{
		if (userMessageMenuType_ == 2U)
		{
			notifyMenuReady(userMessageTarget_, JoinMenuKind::Team, userMessageValidSlots_,
				JoinMenuSource::Vgui);
		}
		else if (userMessageMenuType_ == 26U)
		{
			notifyMenuReady(
				userMessageTarget_, JoinMenuKind::TerroristClass, userMessageValidSlots_,
				JoinMenuSource::Vgui);
		}
		else if (userMessageMenuType_ == 27U)
		{
			notifyMenuReady(
				userMessageTarget_, JoinMenuKind::CounterTerroristClass, userMessageValidSlots_,
				JoinMenuSource::Vgui);
		}
			}
			userMessageShowFragmentActive_ =
				userMessageKind_ == UserMessageKind::ShowMenu && userMessageNeedMore_ != 0U &&
				userMessageTextLength_ != 0U;
			userMessageKind_ = UserMessageKind::None;
			userMessageTarget_ = nullptr;
			userMessageFieldCount_ = 0U;
			userMessageMenuType_ = 0U;
			userMessageNeedMore_ = 0U;
			userMessageValidSlots_ = 0U;
			userMessageTeamSlot_ = 0U;
			if (!userMessageShowFragmentActive_)
			{
				userMessageText_.fill('\0');
				userMessageTextLength_ = 0U;
			}
		}

		void PluginRuntime::onWriteByte(int value)
		{
			if (userMessageKind_ == UserMessageKind::TeamInfo && userMessageFieldCount_ == 0U)
			{
				if (value >= static_cast<int>(runtime::LifecycleSession::kFirstClientSlot) &&
					value <= static_cast<int>(runtime::LifecycleSession::kClientSlotCount))
				{
					userMessageTeamSlot_ = static_cast<std::uint8_t>(value);
				}
			}
			else if (userMessageKind_ == UserMessageKind::VguiMenu && userMessageFieldCount_ == 0U)
			{
				if (value >= 0 && value <= 255)
				{
					userMessageMenuType_ = static_cast<std::uint8_t>(value);
				}
				if (gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr &&
						pluginId_ != nullptr)
				{
					gpMetaUtilFuncs->pfnLogConsole(
						pluginId_, "menu vgui value=%d frame=%u", value,
						static_cast<unsigned int>(adapterFrameCount_));
				}
			}
			else if (userMessageKind_ == UserMessageKind::ShowMenu && userMessageFieldCount_ == 2U)
			{
				userMessageNeedMore_ = value == 0 ? 0U : 1U;
			}
			if (userMessageFieldCount_ < (std::numeric_limits<std::uint8_t>::max)())
			{
				++userMessageFieldCount_;
			}
		}

		void PluginRuntime::onWriteChar(int value)
		{
			onWriteByte(value);
		}

		void PluginRuntime::onWriteShort(int value)
		{
			const bool showMenuSlots = userMessageKind_ == UserMessageKind::ShowMenu &&
				userMessageFieldCount_ == 0U;
			const bool vguiMenuSlots = userMessageKind_ == UserMessageKind::VguiMenu &&
				userMessageFieldCount_ == 1U;
			if ((showMenuSlots || vguiMenuSlots) && value >= 0 && value <= 65535)
			{
				userMessageValidSlots_ = static_cast<std::uint16_t>(value);
			}
			if (userMessageFieldCount_ < (std::numeric_limits<std::uint8_t>::max)())
			{
				++userMessageFieldCount_;
			}
		}

		void PluginRuntime::onWriteString(const char *value)
		{
			if (value == nullptr)
			{
				return;
			}
			if (userMessageKind_ == UserMessageKind::TeamInfo)
			{
				notifyTeamInfo(userMessageTeamSlot_, value);
				return;
			}
			if (userMessageKind_ != UserMessageKind::ShowMenu)
			{
				return;
			}
			const std::size_t valueLength = std::strlen(value);
			if (valueLength > userMessageText_.size() - 1U ||
				userMessageTextLength_ > userMessageText_.size() - 1U - valueLength)
			{
				userMessageText_.fill('\0');
				userMessageTextLength_ = 0U;
				return;
			}
			for (std::size_t index = 0U; index < valueLength; ++index)
			{
				userMessageText_[userMessageTextLength_ + index] = value[index];
			}
			userMessageTextLength_ = static_cast<std::uint16_t>(
				userMessageTextLength_ + valueLength);
			userMessageText_[userMessageTextLength_] = '\0';
			if (userMessageNeedMore_ != 0U)
			{
				return;
			}
			const char *menuText = userMessageText_.data();
			const bool teamMenu = std::strcmp(menuText, "#Team_Select") == 0 ||
				std::strcmp(menuText, "#Team_Select_Spect") == 0 ||
				std::strcmp(menuText, "#IG_Team_Select") == 0 ||
				std::strcmp(menuText, "#IG_Team_Select_Spect") == 0 ||
				std::strcmp(menuText, "#IG_VIP_Team_Select") == 0 ||
				std::strcmp(menuText, "#IG_VIP_Team_Select_Spect") == 0;
			if (teamMenu)
			{
				if (gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr &&
						pluginId_ != nullptr)
				{
					gpMetaUtilFuncs->pfnLogConsole(
						pluginId_, "menu show team value=%s frame=%u", value,
						static_cast<unsigned int>(adapterFrameCount_));
				}
	notifyMenuReady(userMessageTarget_, JoinMenuKind::Team, userMessageValidSlots_,
		JoinMenuSource::LegacyShowMenu);
}
else if (std::strcmp(menuText, "#Terrorist_Select") == 0)
{
				if (gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr &&
						pluginId_ != nullptr)
				{
					gpMetaUtilFuncs->pfnLogConsole(
						pluginId_, "menu show class value=%s frame=%u", value,
						static_cast<unsigned int>(adapterFrameCount_));
				}
	notifyMenuReady(
		userMessageTarget_, JoinMenuKind::TerroristClass, userMessageValidSlots_,
		JoinMenuSource::LegacyShowMenu);
}
else if (std::strcmp(menuText, "#CT_Select") == 0)
{
	if (gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr &&
			pluginId_ != nullptr)
	{
		gpMetaUtilFuncs->pfnLogConsole(
			pluginId_, "menu show class value=%s frame=%u", value,
			static_cast<unsigned int>(adapterFrameCount_));
	}
	notifyMenuReady(
		userMessageTarget_, JoinMenuKind::CounterTerroristClass, userMessageValidSlots_,
		JoinMenuSource::LegacyShowMenu);
}
		}

		void PluginRuntime::notifyTeamInfo(std::uint8_t slot, const char *teamName)
		{
			if (slot < runtime::LifecycleSession::kFirstClientSlot ||
				slot > runtime::LifecycleSession::kClientSlotCount || teamName == nullptr)
			{
				return;
			}
	const std::size_t index = static_cast<std::size_t>(slot - 1U);
	managedBotTeamNumbers_[index] = equalsIgnoreCase(teamName, "TERRORIST") ? 1U :
		equalsIgnoreCase(teamName, "CT") || equalsIgnoreCase(teamName, "COUNTER-TERRORIST") ? 2U : 0U;
	if (!managedBotSlots_[index] || managedBotHandles_[index].entity == nullptr)
	{
		return;
	}
	const FakeClientHandle &handle = managedBotHandles_[index];
	if (gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr &&
			pluginId_ != nullptr)
	{
		gpMetaUtilFuncs->pfnLogConsole(
			pluginId_,
			"team info slot=%u generation=%u name=%s phase=%d requested_team=%d "
			"entity_team=%d deadflag=%d spectator=%d frame=%u",
			static_cast<unsigned int>(slot),
			static_cast<unsigned int>(handle.actor.actorGeneration), teamName,
			static_cast<int>(joinControllers_[index].phase()),
			static_cast<int>(joinControllers_[index].requestedTeam()),
			static_cast<int>(handle.entity->v.team), static_cast<int>(handle.entity->v.deadflag),
			(handle.entity->v.flags & FL_SPECTATOR) != 0 ? 1 : 0,
			static_cast<unsigned int>(adapterFrameCount_));
	}
	applyJoinAction(index, joinControllers_[index].onTeamInfo(teamName));
}
runtime::MovementPhysicsState PluginRuntime::captureMovementPhysicsState(
	const edict_t *entity, bool teamConfirmed, bool managedFakeClient) const
{
	runtime::MovementPhysicsState state{};
	if (entity == nullptr)
	{
		return state;
	}
	state.entityValid = true;
	state.fakeClient = managedFakeClient || (entity->v.flags & FL_FAKECLIENT) != 0;
	state.spectator = (entity->v.flags & FL_SPECTATOR) != 0;
	state.dead = entity->v.deadflag != DEAD_NO || entity->v.health <= 0.0f;
	state.grounded = (entity->v.flags & FL_ONGROUND) != 0;
	state.ducked = (entity->v.flags & FL_DUCKING) != 0;
	state.onLadder = entity->v.movetype == MOVETYPE_FLY;
	state.flags = entity->v.flags;
	state.deadflag = entity->v.deadflag;
	state.team = entity->v.team;
	state.teamConfirmed = teamConfirmed;
	state.solid = entity->v.solid;
	state.movetype = entity->v.movetype;
	state.health = entity->v.health;
	state.origin = {entity->v.origin.x, entity->v.origin.y, entity->v.origin.z};
	state.velocity = {entity->v.velocity.x, entity->v.velocity.y, entity->v.velocity.z};
	if (entity->v.groundentity != nullptr && engineFunctions_ != nullptr &&
			engineFunctions_->pfnIndexOfEdict != nullptr)
	{
		const int groundIndex = engineFunctions_->pfnIndexOfEdict(entity->v.groundentity);
		if (groundIndex >= 0)
		{
			state.groundEntityIndex = static_cast<std::uint32_t>(groundIndex);
		}
	}
	return state;
}

void PluginRuntime::recordMovementPhysicsSample(
	std::size_t index,
	const runtime::MovementPhysicsState &before,
	const runtime::CommandReceipt &receipt)
{
	if (index >= movementPhysicsSamples_.size() || !managedBotSlots_[index])
	{
		return;
	}
	const FakeClientHandle &handle = managedBotHandles_[index];
	runtime::MovementPhysicsSample sample{};
	sample.actor = handle.actor;
	sample.frame = {
		lifecycle_.mapGeneration(), lifecycle_.roundGeneration(), adapterFrameCount_};
	sample.commandSequence = receipt.sequence;
	sample.before = before;
		sample.after = captureMovementPhysicsState(
			handle.entity, joinControllers_[index].teamConfirmed(), true);
		sample.dispatched = receipt.result == runtime::DispatchResult::Dispatched;
		if (sample.dispatched)
		{
			runtimeProfiler_.recordRunPlayerMove();
		}
		sample.readiness = runtime::spawnReadiness(sample.after);
		movementPhysicsSamples_[index] = sample;
		if (sample.after.dead || sample.after.spectator)
		{
			movementReadyLogged_[index] = false;
		}
		else if (sample.readiness == runtime::SpawnReadiness::Ready &&
				!movementReadyLogged_[index])
		{
			movementDiagnosticSamples_[index] = 0U;
			movementReadyLogged_[index] = true;
		}

	const float maxSpeed = handle.entity != nullptr ? handle.entity->v.maxspeed : 0.0f;
	if (sample.dispatched)
	{
			if (movementDispatchFrames_[index] !=
					(std::numeric_limits<std::uint32_t>::max)() &&
					movementDispatchFrames_[index] == adapterFrameCount_ &&
				gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogError != nullptr &&
				pluginId_ != nullptr)
		{
			gpMetaUtilFuncs->pfnLogError(
				pluginId_, "movement duplicate dispatch slot=%u frame=%u",
				static_cast<unsigned int>(index + 1U),
				static_cast<unsigned int>(adapterFrameCount_));
		}
		movementDispatchFrames_[index] = adapterFrameCount_;
	}

	if (gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr &&
				pluginId_ != nullptr &&
				movementDiagnosticSamples_[index] < kMovementPhysicsLogLimit)
	{
		gpMetaUtilFuncs->pfnLogConsole(
			pluginId_,
			"movement physics actor=%u generation=%u frame=%u sequence=%u dispatched=%d "
			"before=(%.1f %.1f %.1f) after=(%.1f %.1f %.1f) velocity=(%.1f %.1f %.1f) "
			"grounded=%d ducked=%d ladder=%d groundentity=%u flags=%d deadflag=%d "
				"team=%d teamConfirmed=%d solid=%d movetype=%d maxspeed=%.1f health=%.1f ready=%d result=%d",
			static_cast<unsigned int>(sample.actor.slot),
			static_cast<unsigned int>(sample.actor.actorGeneration),
			static_cast<unsigned int>(adapterFrameCount_),
			static_cast<unsigned int>(sample.commandSequence), sample.dispatched ? 1 : 0,
			sample.before.origin.x, sample.before.origin.y, sample.before.origin.z,
			sample.after.origin.x, sample.after.origin.y, sample.after.origin.z,
			sample.after.velocity.x, sample.after.velocity.y, sample.after.velocity.z,
			sample.after.grounded ? 1 : 0, sample.after.ducked ? 1 : 0,
			sample.after.onLadder ? 1 : 0,
			static_cast<unsigned int>(sample.after.groundEntityIndex),
			sample.after.flags, sample.after.deadflag, sample.after.team,
			sample.after.teamConfirmed ? 1 : 0, sample.after.solid,
				sample.after.movetype, maxSpeed, sample.after.health,
			sample.readiness == runtime::SpawnReadiness::Ready ? 1 : 0,
			static_cast<int>(receipt.result));
		++movementDiagnosticSamples_[index];
	}
}

void PluginRuntime::updateManagedBotMovement()
{
	const compat::CvarSnapshot configuration = compatibilitySurface_.configuration();
	const nav::NavSnapshot navigation = navPublisher_.snapshot();
	const bool movementUnavailable = configuration.botEnable <= 0.0f ||
			configuration.botStop > 0.0f || !navigation.isValid();
			if (movementDiagnosticRound_ != lifecycle_.roundGeneration())
			{
				movementDiagnosticRound_ = lifecycle_.roundGeneration();
				movementDiagnosticSamples_.fill(0U);
				movementDiagnosticAttempts_.fill(false);
				movementReadyLogged_.fill(false);
			}
	if (movementUnavailable)
	{
				if (!movementDiagnosticGlobal_ && gpMetaUtilFuncs != nullptr &&
						gpMetaUtilFuncs->pfnLogConsole != nullptr && pluginId_ != nullptr)
				{
					gpMetaUtilFuncs->pfnLogConsole(
						pluginId_,
						"movement unavailable frame=%u enable=%.1f stop=%.1f nav=%d",
						static_cast<unsigned int>(adapterFrameCount_),
						configuration.botEnable,
						configuration.botStop,
						navigation.isValid() ? 1 : 0);
			movementDiagnosticGlobal_ = true;
		}
	}

			const std::uint8_t milliseconds = movementMilliseconds(globals_);
			for (std::size_t index = 0U; index < managedBotSlots_.size(); ++index)
			{
				if (!managedBotSlots_[index])
				{
					continue;
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
		const runtime::MovementPhysicsState before = captureMovementPhysicsState(
			handle.entity, joinControllers_[index].teamConfirmed(), true);
		const runtime::ActorState actorState = actorRegistry_.state(handle.actor);
		if (actorState == runtime::ActorState::Joining)
		{
			if (!joinControllers_[index].active())
			{
				continue;
			}
			const runtime::CommandReceipt receipt =
				dispatchJoinHeartbeat(index, handle, milliseconds);
			recordMovementPhysicsSample(index, before, receipt);
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
				break;
			}
		}
		if (!commandExecuteDue)
		{
			continue;
		}
		if (commandResetDue)
		{
			resetManagedBotCommandTemplate(index);
		}
		if (!fullUpdateDue)
		{
			if (!managedBotCommandTemplateValid_[index])
			{
				prepareNeutralManagedBotCommand(index, handle, before);
			}
			const runtime::CommandReceipt receipt = executeManagedBotCommand(index, handle);
			recordMovementPhysicsSample(index, before, receipt);
			continue;
		}
		RuntimeProfilerScope fullUpdateScope(
			runtimeProfiler_, RuntimeProfilerStage::RuntimeFullUpdate);
		if (managedBotFullUpdateSequences_[index] ==
			(std::numeric_limits<std::uint32_t>::max)())
		{
			managedBotFullUpdateSequences_[index] = 0U;
		}
		else
		{
			++managedBotFullUpdateSequences_[index];
		}
		updateManagedBotCompatibilityState(index, before);
		if (before.dead)
		{
			if (movementLastDeadFrames_[index] == 0U)
			{
				movementLastDeadFrames_[index] = adapterFrameCount_;
				movementResumeFrames_[index] = adapterFrameCount_ >
						(std::numeric_limits<std::uint32_t>::max)() - kRespawnSettleFrames
					? (std::numeric_limits<std::uint32_t>::max)()
					: adapterFrameCount_ + kRespawnSettleFrames;
				movementWarmupFrames_[index] = 0U;
				managedBotMovement_[index].reset();
			}
			prepareNeutralManagedBotCommand(index, handle, before);
			const runtime::CommandReceipt receipt = executeManagedBotCommand(index, handle);
			recordMovementPhysicsSample(index, before, receipt);
			continue;
		}
		if (movementLastDeadFrames_[index] != 0U)
		{
			if (adapterFrameCount_ < movementResumeFrames_[index])
			{
				prepareNeutralManagedBotCommand(index, handle, before);
				const runtime::CommandReceipt receipt = executeManagedBotCommand(index, handle);
				recordMovementPhysicsSample(index, before, receipt);
				continue;
			}
			movementLastDeadFrames_[index] = 0U;
			movementResumeFrames_[index] = 0U;
			movementSettledDeadFrames_[index] = 0U;
		}
		if (movementUnavailable || runtime::spawnReadiness(before) == runtime::SpawnReadiness::NotReady)
		{
			prepareNeutralManagedBotCommand(index, handle, before);
			const runtime::CommandReceipt receipt = executeManagedBotCommand(index, handle);
			recordMovementPhysicsSample(index, before, receipt);
			managedBotMovement_[index].reset();
			continue;
		}
		if (handle.entity == nullptr ||
						handle.entity->v.deadflag != DEAD_NO ||
						handle.entity->v.health <= 0.0f)
				{
					if (!movementDiagnosticUnavailable_[index])
					{
						logMovementDiagnostic(index, "entity_unavailable");
						movementDiagnosticUnavailable_[index] = true;
					}
					movementResumeFrames_[index] =
						adapterFrameCount_ >
							(std::numeric_limits<std::uint32_t>::max)() -
								kRespawnSettleFrames
								? (std::numeric_limits<std::uint32_t>::max)()
								: adapterFrameCount_ + kRespawnSettleFrames;
				movementLastDeadFrames_[index] = adapterFrameCount_;
				movementWarmupFrames_[index] = 0U;
				managedBotMovement_[index].reset();
					continue;
				}
				if (movementLastDeadFrames_[index] != 0U &&
						movementLastDeadFrames_[index] != movementSettledDeadFrames_[index])
				{
					movementSettledDeadFrames_[index] = movementLastDeadFrames_[index];
					movementResumeFrames_[index] =
							adapterFrameCount_ >
									(std::numeric_limits<std::uint32_t>::max)() -
										kRespawnSettleFrames
								? (std::numeric_limits<std::uint32_t>::max)()
								: adapterFrameCount_ + kRespawnSettleFrames;
				}
				if (movementResumeFrames_[index] != 0U &&
						adapterFrameCount_ < movementResumeFrames_[index])
				{
					continue;
				}
				if (movementDiagnosticUnavailable_[index])
				{
					if (adapterFrameCount_ < movementResumeFrames_[index])
					{
						continue;
					}
					movementDiagnosticAttempts_[index] = false;
					movementDiagnosticUnavailable_[index] = false;
				}
		if (movementWarmupFrames_[index] < 2U)
		{
			prepareNeutralManagedBotCommand(index, handle, before);
			const runtime::CommandReceipt receipt = executeManagedBotCommand(index, handle);
			recordMovementPhysicsSample(index, before, receipt);
			++movementWarmupFrames_[index];
			continue;
				}
				handle.entity->v.flags |= (FL_CLIENT | FL_FAKECLIENT);

				runtime::NavRoamObservation observation = {};
				observation.actor = handle.actor;
				observation.frame = {
					lifecycle_.mapGeneration(),
					lifecycle_.roundGeneration(),
					adapterFrameCount_};
			float feetOffset = handle.entity->v.mins.z;
			if (!std::isfinite(feetOffset) || feetOffset >= 0.0f)
			{
				feetOffset = -kStandingHalfHumanHeight;
			}
		observation.locomotion.position = {
				handle.entity->v.origin.x,
				handle.entity->v.origin.y,
				handle.entity->v.origin.z + feetOffset};
		observation.locomotion.velocity = {
			handle.entity->v.velocity.x,
			handle.entity->v.velocity.y,
			handle.entity->v.velocity.z};
		observation.locomotion.standingClearance = 72.0f;
		observation.locomotion.crouchingClearance = 36.0f;
		observation.locomotion.grounded = before.grounded;
		observation.locomotion.ducked = before.ducked;
		observation.locomotion.onLadder = before.onLadder;
		observation.airborne = !before.grounded;
		observation.landingConfirmed = movementWasAirborne_[index] && before.grounded;
		observation.hasLandingDamage = false;
		observation.landingDamage = 0.0f;
		observation.ladderContact = before.onLadder;
		observation.entryConfirmed = true;
		observation.exitConfirmed = before.grounded;
		observation.collectPathStats = runtimeProfiler_.enabled();
		nav::NavSearchStats objectiveSearchStats = {};
		observation.hasObjectiveTarget = buildManagedObjectiveTarget(
			index,
			&observation.objectiveTarget,
			runtimeProfiler_.enabled() ? &objectiveSearchStats : nullptr);
		runtimeProfiler_.recordPathSearchResults(
			objectiveSearchStats.searchCalls != 0U,
			objectiveSearchStats.searchCalls,
			objectiveSearchStats.successCount,
			objectiveSearchStats.failureCount,
			objectiveSearchStats.expandedUniqueAreas,
			objectiveSearchStats.enqueueCount,
			objectiveSearchStats.reopenCount,
			objectiveSearchStats.staleQueueEntries,
			objectiveSearchStats.equalCostReplacements,
			objectiveSearchStats.totalUsec,
			objectiveSearchStats.maxUsec,
			objectiveSearchStats.firstSearchId,
			objectiveSearchStats.lastSearchId);
		movementWasAirborne_[index] = !before.grounded;

		nav::LocomotionIntent locomotionIntent = {};
		runtime::NavRoamDecision roamDecision = {};
		runtime::NavRoamResult roamResult = runtime::NavRoamResult::NoRoute;
		RuntimeProfilerScope navMovementScope(
			runtimeProfiler_, RuntimeProfilerStage::NavMovement);
		if (performanceDisablePathSearch_)
		{
			roamDecision.failureReason = runtime::NavFailureReason::PathSearchFailed;
			roamDecision.stage = runtime::NavRoamStage::Failed;
		}
		else
		{
			roamResult = managedBotMovement_[index].update(
				navigation,
				observation,
				&locomotionIntent,
				&roamDecision);
		}
		runtimeProfiler_.recordPathSearchResults(
			roamDecision.pathRequested,
			roamDecision.pathSearchStats.searchCalls,
			roamDecision.pathSearchStats.successCount,
			roamDecision.pathSearchStats.failureCount,
			roamDecision.pathSearchStats.expandedUniqueAreas,
			roamDecision.pathSearchStats.enqueueCount,
			roamDecision.pathSearchStats.reopenCount,
			roamDecision.pathSearchStats.staleQueueEntries,
			roamDecision.pathSearchStats.equalCostReplacements,
			roamDecision.pathSearchStats.totalUsec,
			roamDecision.pathSearchStats.maxUsec,
			roamDecision.pathSearchStats.firstSearchId,
			roamDecision.pathSearchStats.lastSearchId);
		if (roamDecision.recomputeReason != runtime::NavRecomputeReason::None)
		{
			runtimeProfiler_.recordPathRecompute();
		}
		if (roamResult != runtime::NavRoamResult::IntentReady)
		{
			if (roamDecision.failureReason == runtime::NavFailureReason::None)
			{
				roamDecision.failureReason = runtime::NavFailureReason::MovementNotProduced;
			}
			logMovementDiagnostic(index, "roam_no_intent", &roamDecision);
			prepareNeutralManagedBotCommand(index, handle, before);
			const runtime::CommandReceipt receipt = executeManagedBotCommand(index, handle);
			recordMovementPhysicsSample(index, before, receipt);
			continue;
		}

		if (!lifecycle_.isCurrent(token) ||
						managedBotCommandSequences_[index] ==
							(std::numeric_limits<std::uint32_t>::max)())
				{
					continue;
				}

			runtime::BotCommand command = {};
			command.actor = handle.actor;
			command.lifecycle = token;
			command.sequence = 0U;
			command.issueFrame = 0U;
			const runtime::ViewAngles movementViewAngles = {
				0.0f,
				movementYaw(locomotionIntent.direction),
				0.0f};
			command.viewAngles = movementViewAngles;
			std::uint16_t movementButtons =
				locomotionIntent.posture == nav::LocomotionPosture::Crouching
						? static_cast<std::uint16_t>(IN_DUCK)
						: static_cast<std::uint16_t>(0U);
			if (locomotionIntent.traversal == nav::TraversalAction::Jump)
			{
				movementButtons = static_cast<std::uint16_t>(movementButtons | IN_JUMP);
			}
			if (locomotionIntent.traversal == nav::TraversalAction::Crouch ||
					locomotionIntent.traversal == nav::TraversalAction::NarrowPassage)
			{
				movementButtons = static_cast<std::uint16_t>(movementButtons | IN_DUCK);
			}
		command.movement = {
			0.0f,
			0.0f,
			0.0f,
			movementButtons,
			0U,
			1U};
		const ActionProposal actionProposal = ActionAdapter::forLiveDispatch(
			decideManagedBotAction(index, before, movementViewAngles, movementButtons),
			false,
			movementViewAngles);
		const ActionDispatch actionDispatch = ActionAdapter::translate(actionProposal);
		command.viewAngles = actionDispatch.viewAngles;
		command.movement.buttons = actionDispatch.buttons;
		const MovementProjection movementProjection = ActionAdapter::projectMovement(
			locomotionIntent.direction.x,
			locomotionIntent.direction.y,
			locomotionIntent.speed,
			command.viewAngles.yaw);
		command.movement.forward = actionDispatch.stopMovement ? 0.0f : movementProjection.forward;
		command.movement.side = actionDispatch.stopMovement ? 0.0f : movementProjection.side;
		if (!actionDispatch.stopMovement)
		{
			command.movement.buttons = static_cast<std::uint16_t>(
				command.movement.buttons |
				ActionAdapter::movementButtons(
					command.movement.forward, command.movement.side));
		}
		if (movementDiagnosticSamples_[index] < kMovementPhysicsLogLimit &&
			gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr &&
			pluginId_ != nullptr)
		{
			gpMetaUtilFuncs->pfnLogConsole(
				pluginId_,
				"movement command actor=%u generation=%u frame=%u forward=%.1f side=%.1f up=%.1f yaw=%.1f buttons=%u msec=%u targetArea=%u nav=(%.2f %.2f) action=%d stop=%d",
				static_cast<unsigned int>(handle.actor.slot),
				static_cast<unsigned int>(handle.actor.actorGeneration),
				static_cast<unsigned int>(adapterFrameCount_), command.movement.forward,
				command.movement.side, command.movement.up, command.viewAngles.yaw,
				static_cast<unsigned int>(command.movement.buttons),
				static_cast<unsigned int>(command.movement.msec),
				static_cast<unsigned int>(locomotionIntent.targetArea),
				locomotionIntent.direction.x, locomotionIntent.direction.y,
				static_cast<int>(actionProposal.kind), actionDispatch.stopMovement ? 1 : 0);
		}
		if (actionProposal.kind != ActionKind::None && gpMetaUtilFuncs != nullptr &&
			gpMetaUtilFuncs->pfnLogConsole != nullptr && pluginId_ != nullptr &&
			(adapterFrameCount_ % 16U) == 0U)
		{
			gpMetaUtilFuncs->pfnLogConsole(
				pluginId_, "bot action=%s slot=%u generation=%u frame=%u buttons=%u",
				actionKindName(actionProposal.kind), static_cast<unsigned int>(handle.actor.slot),
				static_cast<unsigned int>(handle.actor.actorGeneration),
				static_cast<unsigned int>(adapterFrameCount_),
				static_cast<unsigned int>(actionDispatch.buttons));
		}
		if (actionDispatch.clientCommand != nullptr)
		{
			(void)dispatchClientCommand(handle.entity, actionDispatch.clientCommand, "");
		}
		handle.entity->v.v_angle[0] = command.viewAngles.pitch;
				handle.entity->v.v_angle[1] = command.viewAngles.yaw;
				handle.entity->v.v_angle[2] = command.viewAngles.roll;
				handle.entity->v.angles[0] = -command.viewAngles.pitch / 3.0f;
				handle.entity->v.angles[1] = command.viewAngles.yaw;
				handle.entity->v.angles[2] = command.viewAngles.roll;
		managedBotCommandTemplates_[index] = command;
		managedBotCommandTemplateValid_[index] = true;
		const runtime::CommandReceipt receipt = executeManagedBotCommand(index, handle);
		if (receipt.result != runtime::DispatchResult::Dispatched)
		{
			roamDecision.failureReason = runtime::NavFailureReason::NavApplyRejected;
			logMovementDiagnostic(index, "nav_apply_rejected", &roamDecision);
		}
			recordMovementPhysicsSample(index, before, receipt);
			if (receipt.result == runtime::DispatchResult::Dispatched &&
							movementDiagnosticSamples_[index] < 4U &&
							gpMetaUtilFuncs != nullptr &&
							gpMetaUtilFuncs->pfnLogConsole != nullptr &&
							pluginId_ != nullptr)
					{
						gpMetaUtilFuncs->pfnLogConsole(
							pluginId_,
							"movement actor=%u generation=%u frame=%u origin=(%.1f %.1f %.1f) targetArea=%u sequence=%u stage=%d currentArea=%u recoveryArea=%u",
							static_cast<unsigned int>(handle.actor.slot),
							static_cast<unsigned int>(handle.actor.actorGeneration),
							static_cast<unsigned int>(adapterFrameCount_),
							handle.entity->v.origin.x,
							handle.entity->v.origin.y,
							handle.entity->v.origin.z,
							static_cast<unsigned int>(locomotionIntent.targetArea),
							static_cast<unsigned int>(command.sequence),
							static_cast<int>(roamDecision.stage),
							static_cast<unsigned int>(roamDecision.currentArea),
							static_cast<unsigned int>(roamDecision.recoveryArea));
						++movementDiagnosticSamples_[index];
	}
}

}

bool PluginRuntime::buildManagedWorldSnapshot(
	std::size_t index,
	world::WorldSnapshot *snapshot,
	world::ActorKey *targetActor,
	world::WorldPosition *targetPosition)
{
	RuntimeProfilerScope profilerScope(runtimeProfiler_, RuntimeProfilerStage::Observation);
	if (index >= managedBotHandles_.size() || snapshot == nullptr || targetActor == nullptr ||
		targetPosition == nullptr || engineFunctions_ == nullptr ||
		engineFunctions_->pfnPEntityOfEntIndex == nullptr || globals_ == nullptr)
	{
		return false;
	}
	const FakeClientHandle &handle = managedBotHandles_[index];
	if (handle.entity == nullptr || handle.actor.actorGeneration == 0U)
	{
		return false;
	}
	const auto selectVisibleTarget = [targetActor, targetPosition](
		const world::WorldSnapshot &belief,
		const world::ActorKey &observer) -> bool
	{
		world::ActorKey selected = {};
		world::WorldPosition selectedPosition = {};
		float selectedDistanceSquared = (std::numeric_limits<float>::max)();
		const world::ActorObservation *observerObservation = nullptr;
		if (belief.findActor(observer, &observerObservation) !=
			world::ContactLookupResult::Found || observerObservation == nullptr ||
			!observerObservation->hasCurrentPosition())
		{
			return false;
		}
		for (std::size_t actorIndex = 0U; actorIndex < belief.actorCount(); ++actorIndex)
		{
			const world::ActorObservation *candidate = belief.actorAt(actorIndex);
			if (candidate == nullptr || candidate->actor == observer ||
				candidate->relation != world::TeamRelation::Hostile ||
				!candidate->visible || !candidate->hasCurrentPosition())
			{
				continue;
			}
			const float dx = candidate->position.x - observerObservation->position.x;
			const float dy = candidate->position.y - observerObservation->position.y;
			const float dz = candidate->position.z - observerObservation->position.z;
			const float distanceSquared = dx * dx + dy * dy + dz * dz;
			if (distanceSquared < selectedDistanceSquared)
			{
				selectedDistanceSquared = distanceSquared;
				selected = candidate->actor;
				selectedPosition = candidate->position;
			}
		}
		if (!selected.isValid())
			return false;
		*targetActor = selected;
		*targetPosition = selectedPosition;
		return true;
	};
	const world::ActorKey observer = {handle.actor.slot, handle.actor.actorGeneration};
	if (managedBotPerceptionFullUpdates_[index] == managedBotFullUpdateSequences_[index] &&
		managedBotPerception_[index].snapshot().isValid())
	{
		*snapshot = managedBotPerception_[index].snapshot();
		(void)selectVisibleTarget(*snapshot, observer);
		return true;
	}
	const world::FrameIdentity frame = {
		lifecycle_.mapGeneration(), lifecycle_.roundGeneration(), adapterFrameCount_};
	const world::SnapshotIdentity identity = {frame, observer, 0U};
	perception::PerceptionInput input(identity);
	const compat::ObservationTimingContext observationTiming = {
		managedBotCommandSequences_[index], 0U, 0U};
	const auto effectiveTeam = [this](edict_t *entity) {
		int team = entity == nullptr ? 0 : static_cast<int>(entity->v.team);
		if (team >= 1)
		{
			return team;
		}
		for (std::size_t managedIndex = 0U; managedIndex < managedBotHandles_.size(); ++managedIndex)
		{
			if (managedBotHandles_[managedIndex].entity != entity)
			{
				continue;
			}
			if (managedBotTeamNumbers_[managedIndex] >= 1U &&
				managedBotTeamNumbers_[managedIndex] <= 2U)
			{
				return static_cast<int>(managedBotTeamNumbers_[managedIndex]);
			}
			switch (joinControllers_[managedIndex].requestedTeam())
			{
			case compat::CommandTeam::Terrorist:
				return 1;
			case compat::CommandTeam::CounterTerrorist:
				return 2;
			case compat::CommandTeam::Any:
			default:
				return 0;
			}
		}
		return 0;
	};
	compat::CompatibilityObservation observerObservation = {};
	if (observationAdapter_.collectActor(
			handle.entity, observer, frame, observationTiming, &observerObservation) !=
		ObservationAdapterResult::Accepted)
	{
		return false;
	}
	const int observedObserverTeam = observerObservation.player.team.isAvailable()
		? observerObservation.player.team.value
		: 0;
	const int observerTeam = observedObserverTeam >= 1
		? observedObserverTeam
		: effectiveTeam(handle.entity);
	const int maxClients = std::max(0, std::min(globals_->maxClients, 32));
	for (int slot = 1; slot <= maxClients; ++slot)
	{
		edict_t *entity = engineFunctions_->pfnPEntityOfEntIndex(slot);
		const int entityTeam = effectiveTeam(entity);
		if (entity == nullptr || entity->v.deadflag != DEAD_NO || entity->v.health <= 0.0f ||
			(entity->v.flags & FL_SPECTATOR) != 0 || entityTeam < 1)
		{
			continue;
		}
		const runtime::LifecycleToken token = lifecycle_.tokenForSlot(static_cast<std::uint32_t>(slot));
		const std::uint32_t generation = token.slotGeneration == 0U ? 1U : token.slotGeneration;
		const world::ActorKey actor = {
			static_cast<std::uint32_t>(slot), generation};
		compat::CompatibilityObservation compatObservation = {};
		if (observationAdapter_.collectActor(
				entity, actor, frame, observationTiming, &compatObservation) !=
			ObservationAdapterResult::Accepted ||
			!compatObservation.player.origin.isAvailable() ||
			!compatObservation.player.velocity.isAvailable())
		{
			continue;
		}
		perception::VisionObservation visibility = {};
		const bool isObserver = actor == observer;
		const bool visible = isObserver ||
			(observationAdapter_.collectVisibility(
				handle.entity, entity, actor, frame, &visibility) ==
				ObservationAdapterResult::Accepted && visibility.visible);
		world::ActorObservation observation = {};
		observation.actor = actor;
		observation.relation = entityTeam == observerTeam
			? world::TeamRelation::Friendly
			: world::TeamRelation::Hostile;
		observation.state = visible
			? world::ObservationState::ObservedPresent
			: world::ObservationState::ObservedAbsent;
		observation.position = visible ? compatObservation.player.origin.value : world::WorldPosition{};
		observation.velocity = visible ? compatObservation.player.velocity.value : world::WorldVelocity{};
		observation.confidence = visible ? world::ContactConfidence{1.0f, 0U} :
			world::ContactConfidence{0.0f, 0U};
		observation.visible = visible;
		observation.fovPassed = isObserver || visibility.fovPassed;
		observation.losPassed = isObserver || visibility.losPassed;
		observation.visibleParts = isObserver ?
			static_cast<std::uint8_t>(world::VisibleChest | world::VisibleHead |
				world::VisibleFeet | world::VisibleLeftSide | world::VisibleRightSide) :
			visibility.visibleParts;
		if (input.addActor(observation) != perception::PerceptionInputResult::Accepted)
		{
			return false;
		}
	}
	perception::PerceptionAssembler &assembler = managedBotPerception_[index];
	RuntimeProfilerScope worldPublishScope(
		runtimeProfiler_, RuntimeProfilerStage::WorldPublish);
	if (assembler.publish(input, snapshot) != perception::PerceptionResult::Published)
	{
		return false;
	}
	managedBotPerceptionFullUpdates_[index] = managedBotFullUpdateSequences_[index];
	*snapshot = assembler.snapshot();
	(void)selectVisibleTarget(*snapshot, observer);
	return true;
}

bool PluginRuntime::buildManagedObjectiveTarget(
	std::size_t index,
	nav::NavVector *target,
	nav::NavSearchStats *searchStats) const
{
	if (index >= managedBotHandles_.size() || target == nullptr ||
		engineFunctions_ == nullptr ||
		engineFunctions_->pfnPEntityOfEntIndex == nullptr ||
		engineFunctions_->pfnSzFromIndex == nullptr || globals_ == nullptr)
	{
		return false;
	}
	const FakeClientHandle &handle = managedBotHandles_[index];
	if (handle.entity == nullptr)
	{
		return false;
	}
	const world::FrameIdentity frame = {
		lifecycle_.mapGeneration(), lifecycle_.roundGeneration(), adapterFrameCount_};
	const world::ActorKey actor = {handle.actor.slot, handle.actor.actorGeneration};
	const compat::ObservationTimingContext observationTiming = {
		managedBotCommandSequences_[index], 0U, 0U};
	compat::CompatibilityObservation compatObservation = {};
	if (observationAdapter_.collectActor(
			handle.entity, actor, frame, observationTiming, &compatObservation) !=
		ObservationAdapterResult::Accepted)
	{
		return false;
	}
	int team = compatObservation.player.team.isAvailable()
		? compatObservation.player.team.value
		: static_cast<int>(handle.entity->v.team);
	if (team < 1 && managedBotTeamNumbers_[index] >= 1U &&
		managedBotTeamNumbers_[index] <= 2U)
	{
		team = static_cast<int>(managedBotTeamNumbers_[index]);
	}
	if (team < 1 && joinControllers_[index].teamConfirmed())
	{
		team = joinControllers_[index].requestedTeam() == compat::CommandTeam::Terrorist ? 1 :
			joinControllers_[index].requestedTeam() == compat::CommandTeam::CounterTerrorist ? 2 : 0;
	}
	if (team != 1 && team != 2)
	{
		return false;
	}
	const bool carryingBomb = compatObservation.objective.carryingC4.isAvailable() &&
		compatObservation.objective.carryingC4.value;
	const bool needsBombSite = team == 1 && carryingBomb;
	const bool needsPlantedBomb = team == 2;
	if (!needsBombSite && !needsPlantedBomb)
	{
		return false;
	}

	edict_t *plantedBomb = nullptr;
	compat::ObjectiveObservation plantedObservation = {};
	nav::NavVector selectedBombSite = {0.0f, 0.0f, 0.0f};
	std::size_t selectedPathLength = (std::numeric_limits<std::size_t>::max)();
	const nav::NavSnapshot navigation = navPublisher_.snapshot();
	const nav::NavDocument *document = navigation.document();
	nav::NavAreaMatch currentMatch = {};
	bool haveCurrentArea = false;
	if (navigation.isValid() && document != nullptr)
	{
		nav::NavQuery query(navigation);
		const nav::NavVector position = {
			handle.entity->v.origin.x,
			handle.entity->v.origin.y,
			handle.entity->v.origin.z};
		haveCurrentArea = query.findContaining(position, 64.0f, &currentMatch) ==
			nav::NavQueryResult::Found;
		if (!haveCurrentArea)
		{
			haveCurrentArea = query.findNearest(
				position, kMaximumObjectiveDistance, &currentMatch) ==
				nav::NavQueryResult::Found;
		}
	}
	const int maxEntities = (std::max)(0, (std::min)(globals_->maxEntities, 2048));
	for (int entityIndex = 1; entityIndex <= maxEntities; ++entityIndex)
	{
		edict_t *entity = engineFunctions_->pfnPEntityOfEntIndex(entityIndex);
		if (entity == nullptr || entity->free)
		{
			continue;
		}
		const char *classname = engineFunctions_->pfnSzFromIndex(entity->v.classname);
		if (classname == nullptr)
		{
			continue;
		}
		if (needsBombSite && std::strcmp(classname, "func_bomb_target") == 0)
		{
			const nav::NavVector candidate = entityObjectiveCenter(entity);
			nav::NavExtent siteExtent = {};
			const bool hasSiteExtent = entityObjectiveBounds(entity, &siteExtent);
			std::size_t pathLength = (std::numeric_limits<std::size_t>::max)();
			nav::NavVector reachablePoint = candidate;
		if (haveCurrentArea && document != nullptr)
			{
				nav::NavQuery query(navigation);
				for (const nav::NavArea &area : document->areas())
				{
					if (!hasSiteExtent)
					{
						continue;
					}
					const float overlapLoX = (std::max)(area.extent.lo.x, siteExtent.lo.x);
					const float overlapHiX = (std::min)(area.extent.hi.x, siteExtent.hi.x);
					const float overlapLoY = (std::max)(area.extent.lo.y, siteExtent.lo.y);
					const float overlapHiY = (std::min)(area.extent.hi.y, siteExtent.hi.y);
					if (overlapLoX > overlapHiX || overlapLoY > overlapHiY)
					{
						continue;
					}
					nav::NavCorridor corridor = {};
					nav::NavSearchStats sample = {};
					if (query.buildCorridor(currentMatch.area, area.id, &corridor,
						searchStats == nullptr ? nullptr : &sample) != nav::NavQueryResult::Found)
					{
						addNavSearchStats(searchStats, sample);
						continue;
					}
					addNavSearchStats(searchStats, sample);
					if (corridor.areas.size() < pathLength)
					{
						pathLength = corridor.areas.size();
						reachablePoint = {
							(overlapLoX + overlapHiX) * 0.5f,
							(overlapLoY + overlapHiY) * 0.5f,
							area.northEastZ * 0.5f + area.southWestZ * 0.5f};
					}
				}
			}
			if (pathLength == (std::numeric_limits<std::size_t>::max)())
			{
				pathLength = haveCurrentArea ? 1U : 0U;
			}
			if (pathLength < selectedPathLength)
			{
				selectedPathLength = pathLength;
				selectedBombSite = reachablePoint;
			}
		}
		const char *model = engineFunctions_->pfnSzFromIndex(entity->v.model);
	if (needsPlantedBomb && plantedBomb == nullptr && observationAdapter_.collectPlantedBomb(
			entity, classname, model, globals_->time, actor, frame,
			observationTiming, &plantedObservation) == ObservationAdapterResult::Accepted)
		{
			plantedBomb = entity;
		}
	}
	if (team == 1 && carryingBomb && selectedPathLength !=
		(std::numeric_limits<std::size_t>::max)())
	{
		*target = selectedBombSite;
	}
	else if (team == 2 && plantedBomb != nullptr)
	{
		*target = {
			plantedBomb->v.origin.x,
			plantedBomb->v.origin.y,
			plantedBomb->v.origin.z};
	}
	else
	{
		return false;
	}
	return std::isfinite(target->x) && std::isfinite(target->y) &&
		std::isfinite(target->z);
}

ActionProposal PluginRuntime::decideManagedBotAction(
	std::size_t index,
	const runtime::MovementPhysicsState &before,
	const runtime::ViewAngles &movementAngles,
	std::uint16_t movementButtons)
{
	ActionProposal proposal = {ActionKind::None, movementAngles, movementButtons};
	if (index >= managedBotHandles_.size() || before.dead || managedBotHandles_[index].entity == nullptr)
	{
		return proposal;
	}
	const FakeClientHandle &handle = managedBotHandles_[index];
	const world::FrameIdentity frame = {
		lifecycle_.mapGeneration(), lifecycle_.roundGeneration(), adapterFrameCount_};
	const world::ActorKey actor = {handle.actor.slot, handle.actor.actorGeneration};
	const compat::ObservationTimingContext observationTiming = {
		managedBotCommandSequences_[index], 0U, 0U};
	compat::CompatibilityObservation compatObservation = {};
	if (observationAdapter_.collectActor(
			handle.entity, actor, frame, observationTiming, &compatObservation) !=
		ObservationAdapterResult::Accepted)
	{
		return proposal;
	}
	world::WorldSnapshot snapshot;
	world::ActorKey targetActor = {};
	world::WorldPosition targetPosition = {};
	if (!buildManagedWorldSnapshot(index, &snapshot, &targetActor, &targetPosition))
	{
		if ((adapterFrameCount_ % 16U) == 0U && gpMetaUtilFuncs != nullptr &&
			gpMetaUtilFuncs->pfnLogConsole != nullptr && pluginId_ != nullptr)
		{
			gpMetaUtilFuncs->pfnLogConsole(
				pluginId_, "action sensor slot=%u frame=%u team=%d teamInfo=%u target=0",
				static_cast<unsigned int>(handle.actor.slot),
				static_cast<unsigned int>(adapterFrameCount_), static_cast<int>(handle.entity->v.team),
				static_cast<unsigned int>(managedBotTeamNumbers_[index]));
		}
		return proposal;
	}
	int team = compatObservation.player.team.isAvailable()
		? compatObservation.player.team.value
		: static_cast<int>(handle.entity->v.team);
	if (team < 1 && managedBotTeamNumbers_[index] >= 1U && managedBotTeamNumbers_[index] <= 2U)
	{
		team = static_cast<int>(managedBotTeamNumbers_[index]);
	}
	if (team < 1 && joinControllers_[index].teamConfirmed())
	{
		team = joinControllers_[index].requestedTeam() == compat::CommandTeam::Terrorist ? 1 :
			joinControllers_[index].requestedTeam() == compat::CommandTeam::CounterTerrorist ? 2 : 0;
	}
	const objectives::TeamRole teamRole =
		team == 1 ? objectives::TeamRole::Terrorist :
		team == 2 ? objectives::TeamRole::CounterTerrorist : objectives::TeamRole::Unknown;
	if (teamRole != objectives::TeamRole::Unknown && engineFunctions_ != nullptr &&
		engineFunctions_->pfnPEntityOfEntIndex != nullptr &&
		engineFunctions_->pfnSzFromIndex != nullptr && globals_ != nullptr)
	{
		const auto findEntityByClassname = [this](const char *wantedClassname) -> edict_t *
		{
			if (wantedClassname == nullptr || engineFunctions_ == nullptr ||
				engineFunctions_->pfnPEntityOfEntIndex == nullptr ||
				engineFunctions_->pfnSzFromIndex == nullptr || globals_ == nullptr)
			{
				return nullptr;
			}
			const int maxEntities = (std::max)(0, (std::min)(globals_->maxEntities, 2048));
			for (int entityIndex = 1; entityIndex <= maxEntities; ++entityIndex)
			{
				edict_t *entity = engineFunctions_->pfnPEntityOfEntIndex(entityIndex);
				if (entity == nullptr || entity->free)
				{
					continue;
				}
				const char *classname = engineFunctions_->pfnSzFromIndex(entity->v.classname);
				if (classname != nullptr && std::strcmp(classname, wantedClassname) == 0)
				{
					return entity;
				}
			}
			return nullptr;
		};

	edict_t *bombSite = findEntityByClassname("func_bomb_target");
	edict_t *plantedBomb = nullptr;
	compat::ObjectiveObservation plantedObservation = {};
		const int maxEntities = (std::max)(0, (std::min)(globals_->maxEntities, 2048));
		for (int entityIndex = 1; entityIndex <= maxEntities; ++entityIndex)
		{
			edict_t *entity = engineFunctions_->pfnPEntityOfEntIndex(entityIndex);
			if (entity == nullptr || entity->free)
			{
				continue;
			}
			const char *classname = engineFunctions_->pfnSzFromIndex(entity->v.classname);
			const char *model = engineFunctions_->pfnSzFromIndex(entity->v.model);
		if (plantedBomb == nullptr && observationAdapter_.collectPlantedBomb(
				entity, classname, model, globals_->time, actor, frame,
				observationTiming, &plantedObservation) == ObservationAdapterResult::Accepted)
		{
			plantedBomb = entity;
			break;
		}
	}
	const bool carryingBomb = compatObservation.objective.carryingC4.isAvailable() &&
		compatObservation.objective.carryingC4.value;
		if ((adapterFrameCount_ % 16U) == 0U && gpMetaUtilFuncs != nullptr &&
				gpMetaUtilFuncs->pfnLogConsole != nullptr && pluginId_ != nullptr)
		{
			gpMetaUtilFuncs->pfnLogConsole(
				pluginId_, "objective sensor slot=%u frame=%u team=%d weapons=%d carryingC4=%d bombSite=%d plantedC4=%d",
				static_cast<unsigned int>(handle.actor.slot),
				static_cast<unsigned int>(adapterFrameCount_), team,
				handle.entity->v.weapons, carryingBomb ? 1 : 0,
				bombSite != nullptr ? 1 : 0, plantedBomb != nullptr ? 1 : 0);
		}
		if (plantedBomb != nullptr || carryingBomb)
		{
			objectives::ScenarioObservation scenario = {};
			scenario.scenario.frame = snapshot.identity().frame;
			scenario.scenario.scenarioGeneration = 1U;
			scenario.scenario.kind = objectives::ScenarioKind::Bomb;
			scenario.scenario.team = teamRole;
			scenario.phase = objectives::RoundPhase::Live;
			scenario.buyAvailability = objectives::Availability::Unavailable;
			if (plantedBomb != nullptr)
			{
				scenario.eventCount = 1U;
				scenario.events[0].id = 1U;
				scenario.events[0].frame = snapshot.identity().frame;
				scenario.events[0].kind = objectives::ScenarioEventKind::BombPlanted;
				scenario.events[0].state = objectives::EventState::Observed;
				scenario.events[0].actor = {handle.actor.slot, handle.actor.actorGeneration};
				scenario.events[0].team = objectives::TeamRole::Terrorist;
			}
			else if (carryingBomb)
			{
				scenario.eventCount = 1U;
				scenario.events[0].id = 1U;
				scenario.events[0].frame = snapshot.identity().frame;
				scenario.events[0].kind = objectives::ScenarioEventKind::BombCarried;
				scenario.events[0].state = objectives::EventState::Observed;
				scenario.events[0].actor = {handle.actor.slot, handle.actor.actorGeneration};
				scenario.events[0].team = objectives::TeamRole::Terrorist;
			}
			objectives::ObjectiveProposal objective = {};
	const behavior::BehaviorState compatibilityBehaviorState =
		managedBotStateMachines_[index].attackOverlayActive()
			? behavior::BehaviorState::Engage
			: behavior::BehaviorState::Roam;
	const objectives::ObjectiveResult objectiveResult =
		managedBotObjectives_[index].plan(
			snapshot, compatibilityBehaviorState, scenario, nullptr, &objective);
	objectives::ObjectiveKind objectiveKind = objectives::ObjectiveKind::None;
	if (objectiveResult == objectives::ObjectiveResult::Proposed)
	{
		objectiveKind = objective.objective.kind;
	}
	else if (carryingBomb && bombSite != nullptr &&
			teamRole == objectives::TeamRole::Terrorist)
	{
		objectiveKind = objectives::ObjectiveKind::Plant;
	}
	else if (plantedBomb != nullptr && teamRole == objectives::TeamRole::CounterTerrorist)
	{
		objectiveKind = objectives::ObjectiveKind::Defuse;
	}
	if (objectiveKind == objectives::ObjectiveKind::Plant ||
			objectiveKind == objectives::ObjectiveKind::Defuse)
			{
				nav::NavVector objectivePosition = {0.0f, 0.0f, 0.0f};
				bool haveObjectivePosition = false;
		if (objectiveKind == objectives::ObjectiveKind::Defuse)
				{
					if (plantedBomb != nullptr)
					{
						objectivePosition = {
							plantedBomb->v.origin.x,
							plantedBomb->v.origin.y,
							plantedBomb->v.origin.z};
						haveObjectivePosition = true;
					}
				}
				else if (bombSite != nullptr)
				{
			haveObjectivePosition = buildManagedObjectiveTarget(
				index, &objectivePosition, nullptr);
				}
				if (haveObjectivePosition)
				{
					const float dx = objectivePosition.x - handle.entity->v.origin.x;
					const float dy = objectivePosition.y - handle.entity->v.origin.y;
					const float dz = objectivePosition.z - handle.entity->v.origin.z;
					if (dx * dx + dy * dy + dz * dz <= 96.0f * 96.0f)
					{
			proposal.kind = objectiveKind == objectives::ObjectiveKind::Defuse
							? ActionKind::Defuse : ActionKind::Plant;
						proposal.viewAngles = {
							-kRadiansToDegrees * std::atan2(dz, std::sqrt(dx * dx + dy * dy)),
							kRadiansToDegrees * std::atan2(dy, dx), movementAngles.roll};
						return proposal;
					}
				}
			}
		}
	}
	combat::WeaponInventory inventory;
	combat::WeaponRecord weapon = {};
	const int weaponValue = 1;
	weapon.weapon = {static_cast<std::uint16_t>(weaponValue)};
	weapon.slot = 1U;
	weapon.kind = combat::WeaponKind::Rifle;
	weapon.availability = combat::WeaponAvailability::Available;
	weapon.priority = 1U;
	weapon.ammo = {30U, 90U, 90U};
	weapon.reloadState = combat::ReloadState::NotReloading;
	if (inventory.add(weapon) != combat::WeaponInventoryResult::Accepted)
	{
		return proposal;
	}
	combat::TargetBelief target = {};
	target.target = targetActor;
	target.state = combat::TargetBeliefState::Visible;
	target.position = targetPosition;
	target.confidence = 1.0f;
	target.ageTicks = 0U;
	target.friendlyFireRisk = false;
	combat::CombatDecisionContext context = {};
	context.actor = {handle.actor.slot, handle.actor.actorGeneration};
	context.frame = snapshot.identity().frame;
	context.origin = {
		handle.entity->v.origin.x, handle.entity->v.origin.y, handle.entity->v.origin.z};
	combat::CombatDecision decision = {};
	const combat::CombatResult result = managedBotCombat_[index].decide(
		snapshot, inventory, target, context, &decision);
	const bool fireIntentReady =
		result == combat::CombatResult::FireIntentReady &&
		decision.hasFire && decision.hasAim;
	const compat::StateUpdateContext stateContext =
		makeManagedBotStateContext(index, before);
	if (fireIntentReady)
	{
		(void)managedBotStateMachines_[index].beginAttack(stateContext);
	}
	else if (managedBotStateMachines_[index].attackOverlayActive())
	{
		(void)managedBotStateMachines_[index].stopAttack(stateContext);
	}
	if ((adapterFrameCount_ % 16U) == 0U && gpMetaUtilFuncs != nullptr &&
		gpMetaUtilFuncs->pfnLogConsole != nullptr && pluginId_ != nullptr)
	{
		gpMetaUtilFuncs->pfnLogConsole(
			pluginId_, "action sensor slot=%u frame=%u team=%d teamInfo=%u target=1 combatResult=%d "
			"hasAim=%d hasFire=%d",
			static_cast<unsigned int>(handle.actor.slot), static_cast<unsigned int>(adapterFrameCount_),
			static_cast<int>(handle.entity->v.team),
			static_cast<unsigned int>(managedBotTeamNumbers_[index]), static_cast<int>(result),
			decision.hasAim ? 1 : 0, decision.hasFire ? 1 : 0);
	}
	if (fireIntentReady)
	{
		proposal.kind = ActionKind::Fire;
		proposal.viewAngles = {
			decision.aim.pitch, decision.aim.yaw, movementAngles.roll};
	}
	else if (result == combat::CombatResult::ReloadIntentReady && decision.hasReload)
	{
		proposal.kind = ActionKind::Reload;
	}
	return proposal;
}

void PluginRuntime::resetManagedBotTiming(std::size_t index, float spawnTime)
{
	if (index >= managedBotTiming_.size())
	{
		return;
	}
	managedBotTiming_[index].reset(spawnTime);
	resetManagedBotCommandTemplate(index);
}

void PluginRuntime::resetManagedBotCommandTemplate(std::size_t index)
{
	if (index >= managedBotCommandTemplates_.size())
	{
		return;
	}
	managedBotCommandTemplates_[index] = {};
	managedBotCommandTemplateValid_[index] = false;
}

void PluginRuntime::prepareNeutralManagedBotCommand(
	std::size_t index,
	FakeClientHandle &handle,
	const runtime::MovementPhysicsState &before)
{
	if (index >= managedBotCommandTemplates_.size() || handle.entity == nullptr)
	{
		return;
	}
	const runtime::LifecycleToken token = lifecycle_.tokenForSlot(handle.actor.slot);
	if (!lifecycle_.isCurrent(token))
	{
		resetManagedBotCommandTemplate(index);
		return;
	}
	runtime::BotCommand command = {};
	command.actor = handle.actor;
	command.lifecycle = token;
	command.viewAngles = {
		handle.entity->v.v_angle[0],
		handle.entity->v.v_angle[1],
		handle.entity->v.v_angle[2]};
	command.movement = {0.0f, 0.0f, 0.0f, 0U, 0U, 1U};
	const ActionProposal actionProposal = ActionAdapter::forLiveDispatch(
		decideManagedBotAction(index, before, command.viewAngles, command.movement.buttons),
		false,
		command.viewAngles);
	const ActionDispatch actionDispatch = ActionAdapter::translate(actionProposal);
	command.viewAngles = actionDispatch.viewAngles;
	command.movement.buttons = actionDispatch.buttons;
	if (actionDispatch.clientCommand != nullptr)
	{
		(void)dispatchClientCommand(handle.entity, actionDispatch.clientCommand, "");
	}
	managedBotCommandTemplates_[index] = command;
	managedBotCommandTemplateValid_[index] = true;
}

runtime::CommandReceipt PluginRuntime::executeManagedBotCommand(
	std::size_t index,
	FakeClientHandle &handle)
{
	RuntimeProfilerScope profilerScope(
		runtimeProfiler_, RuntimeProfilerStage::MovementDispatch);
	runtime::CommandReceipt receipt = {
		handle.actor, 0U, adapterFrameCount_, runtime::DispatchResult::NoCommand};
	if (index >= managedBotCommandTemplates_.size() || handle.entity == nullptr ||
		managedBotCommandSequences_[index] == (std::numeric_limits<std::uint32_t>::max)())
	{
		return receipt;
	}
	if (!managedBotCommandTemplateValid_[index])
	{
		const runtime::LifecycleToken neutralToken = lifecycle_.tokenForSlot(handle.actor.slot);
		if (!lifecycle_.isCurrent(neutralToken))
		{
			receipt.result = runtime::DispatchResult::StaleActor;
			return receipt;
		}
		runtime::BotCommand neutralCommand = {};
		neutralCommand.actor = handle.actor;
		neutralCommand.lifecycle = neutralToken;
		neutralCommand.viewAngles = {
			handle.entity->v.v_angle[0],
			handle.entity->v.v_angle[1],
			handle.entity->v.v_angle[2]};
		neutralCommand.movement = {0.0f, 0.0f, 0.0f, 0U, 0U, 1U};
		managedBotCommandTemplates_[index] = neutralCommand;
		managedBotCommandTemplateValid_[index] = true;
	}
	if (!managedBotCommandTemplateValid_[index])
	{
		return receipt;
	}
	const runtime::LifecycleToken token = lifecycle_.tokenForSlot(handle.actor.slot);
	if (!lifecycle_.isCurrent(token))
	{
		receipt.result = runtime::DispatchResult::StaleActor;
		return receipt;
	}
	runtime::BotCommand command = managedBotCommandTemplates_[index];
	command.sequence = ++managedBotCommandSequences_[index];
	command.issueFrame = adapterFrameCount_;
	if (globals_ == nullptr)
	{
		receipt.result = runtime::DispatchResult::EngineUnavailable;
		return receipt;
	}
	command.movement.msec = managedBotTiming_[index].consumeCommandMsec(globals_->time);
	const float maxSpeed = handle.entity->v.maxspeed;
	MovementExecutionObservation gateObservation = {};
	gateObservation.explicitFrozen = (handle.entity->v.flags & FL_FROZEN) != 0;
	gateObservation.maxSpeedAvailable = std::isfinite(maxSpeed) && maxSpeed > 0.0f;
	gateObservation.maxSpeed = maxSpeed;
	gateObservation.forward = command.movement.forward;
	gateObservation.side = command.movement.side;
	gateObservation.up = command.movement.up;
	gateObservation.buttons = command.movement.buttons;
	gateObservation.msec = command.movement.msec;
	gateObservation.freezetimeDuck = readOptionalCvarFloat(
		engineFunctions_, "freezetime_duck");
	gateObservation.freezetimeJump = readOptionalCvarFloat(
		engineFunctions_, "freezetime_jump");
	const MovementExecutionDecision gateDecision =
		MovementExecutionGate::evaluate(gateObservation);
	if (gateDecision.phase == MovementExecutionPhase::Live &&
			(!std::isfinite(maxSpeed) || maxSpeed <= 0.0f))
	{
		handle.entity->v.maxspeed = kDefaultManagedBotMaxSpeed;
		if (engineFunctions_ != nullptr &&
				engineFunctions_->pfnSetClientMaxspeed != nullptr)
		{
			engineFunctions_->pfnSetClientMaxspeed(
				handle.entity, kDefaultManagedBotMaxSpeed);
		}
	}
	if (gateDecision.phase == MovementExecutionPhase::ControlFrozen)
	{
		if (engineFunctions_ == nullptr || engineFunctions_->pfnRunPlayerMove == nullptr)
		{
			receipt.result = runtime::DispatchResult::EngineUnavailable;
			resetManagedBotCommandTemplate(index);
			return receipt;
		}
		const float viewAngles[3] = {
			command.viewAngles.pitch,
			command.viewAngles.yaw,
			command.viewAngles.roll};
		handle.entity->v.button = 0U;
		handle.entity->v.impulse = 0U;
		engineFunctions_->pfnRunPlayerMove(
			handle.entity, viewAngles, 0.0f, 0.0f, 0.0f, 0U, 0U, 0U);
		receipt.sequence = command.sequence;
		receipt.result = runtime::DispatchResult::Dispatched;
		resetManagedBotCommandTemplate(index);
		return receipt;
	}
	command.movement.forward = gateDecision.forward;
	command.movement.side = gateDecision.side;
	command.movement.up = gateDecision.up;
	command.movement.buttons = gateDecision.buttons;
	command.movement.msec = gateDecision.msec;
	if (inputDispatcher_.enqueue(command) != runtime::QueueResult::Accepted)
	{
		receipt.result = runtime::DispatchResult::InvalidCommand;
		if (gateDecision.invalidateTemplate)
		{
			resetManagedBotCommandTemplate(index);
		}
		return receipt;
	}
	const runtime::CommandReceipt dispatchReceipt =
		inputDispatcher_.dispatchNext(handle.actor, adapterFrameCount_);
	if (gateDecision.invalidateTemplate)
	{
		resetManagedBotCommandTemplate(index);
	}
	return dispatchReceipt;
}

runtime::CommandReceipt PluginRuntime::dispatchNeutralMovement(
		std::size_t index,
		FakeClientHandle &handle,
		std::uint8_t milliseconds)
	{
		runtime::CommandReceipt receipt = {
			handle.actor, 0U, adapterFrameCount_, runtime::DispatchResult::NoCommand};
		if (index >= managedBotCommandSequences_.size() || handle.entity == nullptr ||
				managedBotCommandSequences_[index] ==
						(std::numeric_limits<std::uint32_t>::max)())
		{
			return receipt;
			}

			const runtime::LifecycleToken token = lifecycle_.tokenForSlot(handle.actor.slot);
		if (!lifecycle_.isCurrent(token))
		{
			receipt.result = runtime::DispatchResult::StaleActor;
			return receipt;
			}

			runtime::BotCommand command = {};
			command.actor = handle.actor;
			command.lifecycle = token;
			command.sequence = ++managedBotCommandSequences_[index];
			command.issueFrame = adapterFrameCount_;
	command.viewAngles = {
		handle.entity->v.v_angle[0],
		handle.entity->v.v_angle[1],
		handle.entity->v.v_angle[2]};
	command.movement = {0.0f, 0.0f, 0.0f, 0U, 0U, milliseconds};
	const runtime::MovementPhysicsState before = captureMovementPhysicsState(
		handle.entity, joinControllers_[index].teamConfirmed(), true);
	if (before.dead)
	{
		receipt.result = runtime::DispatchResult::NoCommand;
		return receipt;
	}
	const ActionProposal actionProposal = ActionAdapter::forLiveDispatch(
		decideManagedBotAction(index, before, command.viewAngles, command.movement.buttons),
		false,
		command.viewAngles);
	const ActionDispatch actionDispatch = ActionAdapter::translate(actionProposal);
	command.viewAngles = actionDispatch.viewAngles;
	command.movement.buttons = actionDispatch.buttons;
	if (actionProposal.kind != ActionKind::None && gpMetaUtilFuncs != nullptr &&
		gpMetaUtilFuncs->pfnLogConsole != nullptr && pluginId_ != nullptr &&
		(adapterFrameCount_ % 16U) == 0U)
	{
		gpMetaUtilFuncs->pfnLogConsole(
			pluginId_, "bot action=%s slot=%u generation=%u frame=%u buttons=%u",
			actionKindName(actionProposal.kind), static_cast<unsigned int>(handle.actor.slot),
			static_cast<unsigned int>(handle.actor.actorGeneration),
			static_cast<unsigned int>(adapterFrameCount_),
			static_cast<unsigned int>(actionDispatch.buttons));
	}
	if (actionDispatch.clientCommand != nullptr)
	{
		(void)dispatchClientCommand(handle.entity, actionDispatch.clientCommand, "");
	}
	if (inputDispatcher_.enqueue(command) != runtime::QueueResult::Accepted)
		{
			receipt.result = runtime::DispatchResult::InvalidCommand;
			return receipt;
		}
		return inputDispatcher_.dispatchNext(handle.actor, adapterFrameCount_);
	}

runtime::CommandReceipt PluginRuntime::dispatchJoinHeartbeat(
		std::size_t index,
		FakeClientHandle &handle,
		std::uint8_t milliseconds)
	{
		runtime::CommandReceipt receipt = {
			handle.actor, 0U, adapterFrameCount_, runtime::DispatchResult::NoCommand};
		if (index >= managedBotCommandSequences_.size() || handle.entity == nullptr ||
			managedBotCommandSequences_[index] ==
				(std::numeric_limits<std::uint32_t>::max)())
		{
			return receipt;
		}
		const runtime::LifecycleToken token = lifecycle_.tokenForSlot(handle.actor.slot);
		if (!lifecycle_.isCurrent(token) || !joinControllers_[index].isCurrent(handle.actor))
		{
			receipt.result = runtime::DispatchResult::StaleActor;
			return receipt;
		}
		if (engineFunctions_ == nullptr || engineFunctions_->pfnRunPlayerMove == nullptr)
		{
			receipt.result = runtime::DispatchResult::EngineUnavailable;
			return receipt;
		}
		const float viewAngles[3] = {
			handle.entity->v.v_angle[0],
			handle.entity->v.v_angle[1],
			handle.entity->v.v_angle[2]};
		receipt.sequence = ++managedBotCommandSequences_[index];
		const unsigned short buttons = joinControllers_[index].phase() == JoinPhase::WaitingTeamMenu
			? static_cast<unsigned short>(IN_ATTACK)
			: 0U;
		engineFunctions_->pfnRunPlayerMove(
			handle.entity, viewAngles, 0.0f, 0.0f, 0.0f, buttons, 0U, milliseconds);
		receipt.result = runtime::DispatchResult::Dispatched;
		return receipt;
	}

compat::StateUpdateContext PluginRuntime::makeManagedBotStateContext(
	std::size_t index,
	const runtime::MovementPhysicsState &before) const
{
	compat::StateUpdateContext context = {};
	if (index >= managedBotHandles_.size())
	{
		return context;
	}
	const FakeClientHandle &handle = managedBotHandles_[index];
	context.actor = {handle.actor.slot, handle.actor.actorGeneration};
	context.frame = {
		lifecycle_.mapGeneration(), lifecycle_.roundGeneration(), adapterFrameCount_};
	context.timestamp = globals_ != nullptr ? globals_->time : 0.0f;
	context.fullUpdateSequence = managedBotFullUpdateSequences_[index];
	context.fullUpdate = true;
	context.lifeAvailability = handle.entity != nullptr
		? compat::ObservationAvailability::Available
		: compat::ObservationAvailability::Unavailable;
	context.alive = !before.dead;
	context.roundAvailability = lifecycle_.isMapActive()
		? compat::ObservationAvailability::Available
		: compat::ObservationAvailability::Unavailable;
	context.roundActive = lifecycle_.isMapActive();
	context.requestTransition = false;
	context.requestedState = compat::CompatibilityStateId::None;
	context.transitionId = compat::StateTransitionId::None;
	context.transitionReason = compat::StateTransitionReason::None;
	context.transitionRequest = compat::ObservationAvailability::Available;
	context.attackObservation = compat::ObservationAvailability::Available;
	context.rngAvailability = compat::ObservationAvailability::Available;
	return context;
}

void PluginRuntime::updateManagedBotCompatibilityState(
	std::size_t index,
	const runtime::MovementPhysicsState &before)
{
	if (index >= managedBotStateMachines_.size() ||
		!managedBotSlots_[index] || managedBotHandles_[index].entity == nullptr)
	{
		return;
	}
	compat::StateUpdateContext context = makeManagedBotStateContext(index, before);
	compat::CompatibilityStateMachine &machine = managedBotStateMachines_[index];
	if (managedBotStateRounds_[index] != 0U &&
		managedBotStateRounds_[index] != lifecycle_.roundGeneration() &&
		machine.isInitialized())
	{
		(void)machine.onRoundReset(context);
	}
	managedBotStateRounds_[index] = lifecycle_.roundGeneration();
	if (!machine.isInitialized())
	{
		(void)machine.initialize(context);
		if (before.dead)
		{
			(void)machine.onDeath(context);
			managedBotWasDead_[index] = true;
		}
		return;
	}
	if (before.dead)
	{
		if (!managedBotWasDead_[index])
		{
			(void)machine.onDeath(context);
		}
		managedBotWasDead_[index] = true;
		return;
	}
	if (managedBotWasDead_[index])
	{
		(void)machine.onRespawn(context);
		managedBotWasDead_[index] = false;
		return;
	}

	const compat::RuntimeMode mode = compatibilitySurface_.configuration().mode;
	if (mode == compat::RuntimeMode::Compatibility)
	{
		const compat::ObservationTimingContext timing = {
			managedBotCommandSequences_[index], 0U,
			managedBotFullUpdateSequences_[index]};
		compat::CompatibilityObservation observation = {};
		const ObservationAdapterResult observationResult = observationAdapter_.collectActor(
			managedBotHandles_[index].entity,
			context.actor,
			context.frame,
			timing,
			&observation);
		if (observationResult == ObservationAdapterResult::Accepted)
		{
			const bool carryingC4 = observation.objective.carryingC4.isAvailable() &&
				observation.objective.carryingC4.value;
			const bool teamAvailable = observation.player.team.isAvailable();
			const int team = teamAvailable ? observation.player.team.value : 0;
			if (carryingC4 && team == 1)
			{
				context.requestTransition = machine.state() !=
					compat::CompatibilityStateId::PlantBomb;
				context.requestedState = compat::CompatibilityStateId::PlantBomb;
				context.transitionId = compat::StateTransitionId::ExplicitStateChange;
				context.transitionReason = compat::StateTransitionReason::ObjectiveChanged;
			}
		}
	}
	(void)machine.update(context);
}

void PluginRuntime::resetManagedBotMovement()
{
	for (std::size_t index = 0U; index < managedBotMovement_.size(); ++index)
	{
		managedBotMovement_[index].reset();
		managedBotCombat_[index] = combat::CombatController();
		managedBotObjectives_[index] = objectives::RoundObjectivePlanner();
		managedBotStateMachines_[index] = compat::CompatibilityStateMachine();
		managedBotPerception_[index] = perception::PerceptionAssembler();
		managedBotTiming_[index] = runtime::BotTimingScheduler();
		resetManagedBotCommandTemplate(index);
	}
	managedBotCommandSequences_.fill(0U);
	managedBotFullUpdateSequences_.fill(0U);
	managedBotPerceptionFullUpdates_.fill((std::numeric_limits<std::uint32_t>::max)());
			managedBotStateRounds_.fill(0U);
			managedBotWasDead_.fill(false);
			movementDiagnosticSamples_.fill(0U);
			movementDiagnosticAttempts_.fill(false);
			movementDiagnosticUnavailable_.fill(false);
			movementResumeFrames_.fill(0U);
			movementLastDeadFrames_.fill(0U);
	movementSettledDeadFrames_.fill(0U);
		movementWarmupFrames_.fill(0U);
		movementPhysicsSamples_.fill({});
		movementReadyLogged_.fill(false);
		movementDispatchFrames_.fill((std::numeric_limits<std::uint32_t>::max)());
	movementWasAirborne_.fill(false);
	movementDiagnosticRound_ = 0U;
		}

		void PluginRuntime::logMovementDiagnostic(
			std::size_t index,
			const char *reason,
			const runtime::NavRoamDecision *decision)
		{
			if (index >= movementDiagnosticAttempts_.size() ||
					movementDiagnosticAttempts_[index] ||
					gpMetaUtilFuncs == nullptr || gpMetaUtilFuncs->pfnLogConsole == nullptr ||
					pluginId_ == nullptr)
			{
				return;
			}

			const FakeClientHandle &handle = managedBotHandles_[index];
			const bool fakeClient = handle.entity != nullptr &&
					(handle.entity->v.flags & FL_FAKECLIENT) != 0;
			const bool spectator = handle.entity != nullptr &&
					(handle.entity->v.flags & FL_SPECTATOR) != 0;
			const nav::NavSnapshot navigation = navPublisher_.snapshot();
			nav::NavAreaMatch area = {};
			float feetOffset = handle.entity != nullptr ? handle.entity->v.mins.z : 0.0f;
			if (!std::isfinite(feetOffset) || feetOffset >= 0.0f)
			{
				feetOffset = -kStandingHalfHumanHeight;
			}
			const nav::NavVector feetPosition = handle.entity != nullptr ?
				nav::NavVector{
					handle.entity->v.origin.x,
					handle.entity->v.origin.y,
					handle.entity->v.origin.z + feetOffset} :
				nav::NavVector{0.0f, 0.0f, 0.0f};
			const nav::NavQueryResult areaResult =
				nav::NavQuery(navigation).findContaining(
						feetPosition,
						64.0f,
							&area);
			gpMetaUtilFuncs->pfnLogConsole(
				pluginId_,
				"movement diagnostic actor=%u generation=%u frame=%u reason=%s fake=%d spectator=%d team=%d deadflag=%d health=%.1f origin=(%.1f %.1f %.1f) solid=%d movetype=%d effects=%d nav=%d navAreaResult=%d area=%u runmove=%d stage=%d currentResult=%d nearestResult=%d currentArea=%u recoveryArea=%u targetArea=%u target=(%.1f %.1f %.1f) intent=(%.2f %.2f %.2f) observedVelocity=(%.1f %.1f %.1f) corridorAreas=%u corridorIndex=%u link=(%u->%u how=%u dir=%u) linkResult=%d corridorResult=%d locomotionResult=%d nearestDistanceSquared=%.1f goalPresent=%d goalKind=%d goalArea=%u pathRequested=%d pathResult=%d failureReason=%d fullUpdate=%u recomputeReason=%d searchCalls=%u expanded=%u enqueues=%u reopens=%u staleQueue=%u equalCostRepl=%u searchUsec=%llu searchMaxUsec=%llu searchFirstId=%llu searchLastId=%llu routeType=%d",
				static_cast<unsigned int>(handle.actor.slot),
				static_cast<unsigned int>(handle.actor.actorGeneration),
				static_cast<unsigned int>(adapterFrameCount_),
				reason != nullptr ? reason : "unknown",
				fakeClient ? 1 : 0,
				spectator ? 1 : 0,
				handle.entity != nullptr ? handle.entity->v.team : -1,
				handle.entity != nullptr ? handle.entity->v.deadflag : -1,
				handle.entity != nullptr ? handle.entity->v.health : 0.0f,
				handle.entity != nullptr ? handle.entity->v.origin.x : 0.0f,
				handle.entity != nullptr ? handle.entity->v.origin.y : 0.0f,
				handle.entity != nullptr ? handle.entity->v.origin.z : 0.0f,
				handle.entity != nullptr ? handle.entity->v.solid : -1,
				handle.entity != nullptr ? handle.entity->v.movetype : -1,
				handle.entity != nullptr ? handle.entity->v.effects : -1,
				navigation.isValid() ? 1 : 0,
				static_cast<int>(areaResult),
				static_cast<unsigned int>(area.area),
				engineFunctions_ != nullptr && engineFunctions_->pfnRunPlayerMove != nullptr ? 1 : 0,
				decision != nullptr ? static_cast<int>(decision->stage) : -1,
				decision != nullptr ? static_cast<int>(decision->currentAreaResult) : -1,
				decision != nullptr ? static_cast<int>(decision->nearestAreaResult) : -1,
				decision != nullptr ? static_cast<unsigned int>(decision->currentArea) : 0U,
				decision != nullptr ? static_cast<unsigned int>(decision->recoveryArea) : 0U,
				decision != nullptr ? static_cast<unsigned int>(decision->targetArea) : 0U,
				decision != nullptr ? decision->targetPosition.x : 0.0f,
				decision != nullptr ? decision->targetPosition.y : 0.0f,
				decision != nullptr ? decision->targetPosition.z : 0.0f,
				decision != nullptr ? decision->intentDirection.x : 0.0f,
				decision != nullptr ? decision->intentDirection.y : 0.0f,
				decision != nullptr ? decision->intentDirection.z : 0.0f,
				decision != nullptr ? decision->observationVelocity.x : 0.0f,
				decision != nullptr ? decision->observationVelocity.y : 0.0f,
				decision != nullptr ? decision->observationVelocity.z : 0.0f,
				decision != nullptr ? static_cast<unsigned int>(decision->corridorAreaCount) : 0U,
				decision != nullptr ? static_cast<unsigned int>(decision->corridorIndex) : 0U,
				decision != nullptr ? static_cast<unsigned int>(decision->linkFromArea) : 0U,
				decision != nullptr ? static_cast<unsigned int>(decision->linkToArea) : 0U,
				decision != nullptr ? static_cast<unsigned int>(decision->linkHow) : 0U,
				decision != nullptr ? static_cast<unsigned int>(decision->linkDirection) : 0U,
				decision != nullptr ? static_cast<int>(decision->linkResult) : -1,
				decision != nullptr ? static_cast<int>(decision->corridorResult) : -1,
				decision != nullptr ? static_cast<int>(decision->locomotionResult) : -1,
				decision != nullptr ? decision->nearestDistanceSquared : 0.0f,
				decision != nullptr && decision->goalPresent ? 1 : 0,
				decision != nullptr ? static_cast<int>(decision->goalKind) : -1,
				decision != nullptr ? static_cast<unsigned int>(decision->goalArea) : 0U,
				decision != nullptr && decision->pathRequested ? 1 : 0,
				decision != nullptr ? static_cast<int>(decision->pathResult) : -1,
				decision != nullptr ? static_cast<int>(decision->failureReason) : -1,
				decision != nullptr ? decision->fullUpdateSequence : 0U,
				decision != nullptr ? static_cast<int>(decision->recomputeReason) : -1,
				decision != nullptr ? decision->pathSearchStats.searchCalls : 0U,
				decision != nullptr ? decision->pathSearchStats.expandedUniqueAreas : 0U,
				decision != nullptr ? decision->pathSearchStats.enqueueCount : 0U,
				decision != nullptr ? decision->pathSearchStats.reopenCount : 0U,
				decision != nullptr ? decision->pathSearchStats.staleQueueEntries : 0U,
				decision != nullptr ? decision->pathSearchStats.equalCostReplacements : 0U,
				decision != nullptr ? static_cast<unsigned long long>(decision->pathSearchStats.totalUsec) : 0ULL,
				decision != nullptr ? static_cast<unsigned long long>(decision->pathSearchStats.maxUsec) : 0ULL,
				decision != nullptr ? static_cast<unsigned long long>(decision->pathSearchStats.firstSearchId) : 0ULL,
				decision != nullptr ? static_cast<unsigned long long>(decision->pathSearchStats.lastSearchId) : 0ULL,
				decision != nullptr ? static_cast<int>(decision->routeType) : -1);
			movementDiagnosticAttempts_[index] = true;
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
				gpMetaUtilFuncs->pfnLogConsole(
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
				const compat::CvarSnapshot configuration = compatibilitySurface_.configuration();
				compat::ProfileRecord profile = {};
				const compat::ProfileSelectionResult profileResult =
					compatibilitySurface_.selectProfile(action, configuration.botDifficulty,
														managedBotCount(), &profile);
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
				const JoinAction beginAction =
					joinControllers_[slotIndex].begin(handle.actor, action.team, adapterFrameCount_);
				if (beginAction.kind == JoinActionKind::Failed)
				{
					(void)removeFakeClient(&handle);
					return CompatibilityCommandResult::ActorOperationFailed;
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
					managedBotMovement_[slotIndex].reset();
					managedBotTiming_[slotIndex] = runtime::BotTimingScheduler();
					resetManagedBotCommandTemplate(slotIndex);
			managedBotCommandSequences_[slotIndex] = 0U;
					movementDiagnosticSamples_[slotIndex] = 0U;
					movementDiagnosticAttempts_[slotIndex] = false;
					movementReadyLogged_[slotIndex] = false;
					movementDiagnosticUnavailable_[slotIndex] = false;
					movementResumeFrames_[slotIndex] = 0U;
					movementLastDeadFrames_[slotIndex] = 0U;
					movementSettledDeadFrames_[slotIndex] = 0U;
					movementWarmupFrames_[slotIndex] = 0U;
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

		runtime::CommandReceipt PluginRuntime::dispatchBotInput(const runtime::ActorId &actor,
																std::uint32_t dispatchFrame)
		{
			return inputDispatcher_.dispatchNext(actor, dispatchFrame);
		}

		void PluginRuntime::configureFakeClientManager()
		{
			const bool allowed =
				state_ == State::ActiveMap && nativeGuardDecision_.managedBotCreationAllowed;
			fakeClientManager_.configure(engineFunctions_, gameDllFunctions_, allowed,
										 gpMetaUtilFuncs, pluginId_);
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

void PluginRuntime::rememberManagedBot(const FakeClientHandle &handle, const char *name)
		{
			if (name == nullptr || handle.actor.slot < 1U ||
				handle.actor.slot > NativeBotObservation::kClientSlotCount)
			{
				return;
			}
	const std::size_t index = static_cast<std::size_t>(handle.actor.slot - 1U);
	managedBotHandles_[index] = handle;
	managedBotTeamNumbers_[index] = 0U;
	const world::ActorKey actor = {handle.actor.slot, handle.actor.actorGeneration};
	managedBotCombat_[index] = combat::CombatController(actor);
	managedBotObjectives_[index] = objectives::RoundObjectivePlanner(actor);
	managedBotStateMachines_[index] = compat::CompatibilityStateMachine(actor);
	managedBotFullUpdateSequences_[index] = 0U;
	managedBotStateRounds_[index] = lifecycle_.roundGeneration();
	managedBotWasDead_[index] = false;
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
	managedBotTeamNumbers_[index] = 0U;
	managedBotTiming_[index] = runtime::BotTimingScheduler();
	resetManagedBotCommandTemplate(index);
	managedBotMovement_[index].reset();
	managedBotCombat_[index] = combat::CombatController();
	managedBotObjectives_[index] = objectives::RoundObjectivePlanner();
	managedBotStateMachines_[index] = compat::CompatibilityStateMachine();
	managedBotPerception_[index] = perception::PerceptionAssembler();
	managedBotCommandSequences_[index] = 0U;
	managedBotFullUpdateSequences_[index] = 0U;
	managedBotPerceptionFullUpdates_[index] =
		(std::numeric_limits<std::uint32_t>::max)();
	managedBotStateRounds_[index] = 0U;
	managedBotWasDead_[index] = false;
			movementDiagnosticSamples_[index] = 0U;
			movementDiagnosticAttempts_[index] = false;
			movementDiagnosticUnavailable_[index] = false;
			movementResumeFrames_[index] = 0U;
			movementLastDeadFrames_[index] = 0U;
	movementSettledDeadFrames_[index] = 0U;
			movementWarmupFrames_[index] = 0U;
			movementPhysicsSamples_[index] = {};
			movementReadyLogged_[index] = false;
	movementDispatchFrames_[index] = (std::numeric_limits<std::uint32_t>::max)();
	movementWasAirborne_[index] = false;
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
				return operationFailed ? CompatibilityCommandResult::ActorOperationFailed
									   : CompatibilityCommandResult::NoTarget;
			}
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

		void HookWriteString(const char *value)
		{
			PluginRuntime::instance().onWriteString(value);
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
		gpMetaUtilFuncs->pfnLogConsole(
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
			gpMetaUtilFuncs->pfnLogConsole(
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
			gpMetaUtilFuncs->pfnLogConsole(
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
				dispatchClientCommand(managedBotHandles_[index].entity, command, selection);
			if (gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr &&
					pluginId_ != nullptr)
			{
				gpMetaUtilFuncs->pfnLogConsole(
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
