#include "plugin_runtime.hpp"
#include "astrabot/metamod/movement_execution_gate.hpp"
#include "astrabot/objectives/bomb_site_route_selection.hpp"
#include "astrabot/compat/bomb_navigation.hpp"

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
			namespace
		{
	constexpr float kRadiansToDegrees = 57.29577951308232f;
	constexpr float kStandingHalfHumanHeight = 36.0f;
	constexpr float kGroundProbeDepth = 80.0f;
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
		case ActionKind::PlantContinue:
			return "plant_continue";
		case ActionKind::SelectC4:
			return "select_c4";
		case ActionKind::Defuse:
			return "defuse";
		case ActionKind::UseEntity:
			return "use_entity";
		case ActionKind::SelectPistol:
			return "select_pistol";
		case ActionKind::SelectKnife:
			return "select_knife";
		case ActionKind::SelectPrimary:
			return "select_primary";
		case ActionKind::None:
		default:
			return "none";
		}
	}

	const char *runtimeModeName(compat::RuntimeMode mode)
	{
		return mode == compat::RuntimeMode::Enhanced ? "enhanced" : "compatibility";
	}

const char *goalSelectionReasonName(runtime::NavGoalSelectionReason reason)
{
	switch (reason)
	{
	case runtime::GoalSelectionInitial:
		return "initial";
	case runtime::GoalSelectionReached:
		return "reached";
	case runtime::GoalSelectionPathFailed:
		return "path_failed";
	case runtime::GoalSelectionNoEligibleArea:
		return "no_eligible_area";
	case runtime::GoalSelectionNone:
	default:
		return "none";
	}
}

const char *goalSelectionStrategyName(runtime::NavGoalSelectionStrategy strategy)
{
	switch (strategy)
	{
	case runtime::GoalSelectionStrategyOldestVisitedArea:
		return "oldest_visited_area";
	case runtime::GoalSelectionStrategyRandomFallback:
		return "random_fallback";
	case runtime::GoalSelectionStrategyNoEligibleArea:
		return "no_eligible_area";
	case runtime::GoalSelectionStrategyNone:
	default:
		return "none";
	}
}

RuntimePathSearchCaller navSearchCaller(const runtime::NavRoamDecision &decision)
{
	if (decision.goalKind == runtime::NavGoalKind::Objective)
	{
		return decision.recomputeReason == runtime::NavRecomputeReason::InitialGoal
			? RuntimePathSearchCaller::CorridorInitial
			: RuntimePathSearchCaller::CorridorRecompute;
	}
	if (decision.goalKind == runtime::NavGoalKind::Roam)
	{
		return RuntimePathSearchCaller::RoamGoalEvaluation;
	}
	if (decision.failureReason == runtime::NavFailureReason::CurrentAreaMissing)
	{
		return RuntimePathSearchCaller::OffPathRecovery;
	}
	if (decision.failureReason == runtime::NavFailureReason::PathSearchFailed)
	{
		return RuntimePathSearchCaller::RecoveryAreaEvaluation;
	}
	return RuntimePathSearchCaller::DebugOrDiagnostic;
}

const char *jumpCrouchTransitionName(runtime::JumpCrouchTransition transition)
{
	switch (transition)
	{
	case runtime::JumpCrouchTransition::Armed:
		return "Armed";
	case runtime::JumpCrouchTransition::DuckStarted:
		return "DuckStarted";
	case runtime::JumpCrouchTransition::DuckReleased:
		return "DuckReleased";
	case runtime::JumpCrouchTransition::Landed:
		return "Landed";
	case runtime::JumpCrouchTransition::Timeout:
		return "Timeout";
	case runtime::JumpCrouchTransition::Cancelled:
		return "Cancelled";
	case runtime::JumpCrouchTransition::None:
	default:
		return "None";
	}
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
			total->firstStartArea = sample.firstStartArea;
			total->firstGoalArea = sample.firstGoalArea;
			total->firstRouteType = sample.firstRouteType;
		}
		total->lastSearchId = sample.lastSearchId;
		total->lastStartArea = sample.lastStartArea;
		total->lastGoalArea = sample.lastGoalArea;
		total->lastRouteType = sample.lastRouteType;
	}
}

	const char *runtimeProfilerStageName(RuntimeProfilerStage stage)
	{
		static const char *const names[] = {
			"StartFrame", "RegistryLifecycle", "Observation", "Vision", "TraceLine",
			"WorldPublish", "Perception", "RuntimeInput", "RuntimeFullUpdate",
			"FullUpdateObjective", "ObjectiveCandidateNavEvaluate",
			"NavCurrentAreaLookup", "PathSearch", "PathRecompute", "NavMovement",
			"MovementDispatch", "TraceSerialization", "RunPlayerMove", "SynchronousLog",
			"ScenarioUpdate", "BombGuardSelection", "PathCostContext"};
		const std::size_t index = static_cast<std::size_t>(stage);
		return index < static_cast<std::size_t>(RuntimeProfilerStage::Count)
			? names[index] : "Unknown";
	}

	const char *runtimePathSearchCallerName(RuntimePathSearchCaller caller)
	{
		static const char *const names[] = {
			"CorridorInitial", "CorridorRecompute", "ObjectiveCandidateEvaluation",
			"BombSiteSelection", "BombTargetSelection", "RoamGoalEvaluation",
			"RecoveryAreaEvaluation", "TraversalRecovery", "OffPathRecovery",
			"DebugOrDiagnostic", "Unknown"};
		const std::size_t index = static_cast<std::size_t>(caller);
		return index < static_cast<std::size_t>(RuntimePathSearchCaller::Count)
			? names[index] : "Unknown";
	}

	template<typename... Args>
	void profiledLogConsole(RuntimeProfiler &profiler, plid_t pluginId,
		const char *format, Args... args)
	{
		RuntimeProfilerScope logScope(profiler, RuntimeProfilerStage::SynchronousLog);
		gpMetaUtilFuncs->pfnLogConsole(pluginId, format, args...);
	}

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

bool isBombTargetClassname(const char *classname)
{
	return ActionAdapter::isBombTargetClassname(classname);
}

	bool objectiveSiteExtent(
		const edict_t *entity, const char *classname, nav::NavExtent *extent)
	{
		if (entity == nullptr || extent == nullptr)
		{
			return false;
		}
		if (ActionAdapter::isLegacyInfoBombTargetClassname(classname))
		{
			const nav::NavVector center = entityObjectiveCenter(entity);
			if (!std::isfinite(center.x) || !std::isfinite(center.y) ||
				!std::isfinite(center.z))
			{
				return false;
			}
			extent->lo = {
				center.x - ActionAdapter::kLegacyBombTargetRadius,
				center.y - ActionAdapter::kLegacyBombTargetRadius,
				center.z - ActionAdapter::kLegacyBombTargetRadius};
			extent->hi = {
				center.x + ActionAdapter::kLegacyBombTargetRadius,
				center.y + ActionAdapter::kLegacyBombTargetRadius,
				center.z + ActionAdapter::kLegacyBombTargetRadius};
			return true;
		}
		return entityObjectiveBounds(entity, extent);
	}

	struct ObjectiveCandidateKey
	{
		int entityIndex;
		std::uint32_t siteIdentity;
		nav::AreaId targetArea;

		bool operator==(const ObjectiveCandidateKey &other) const
		{
			return entityIndex == other.entityIndex &&
				siteIdentity == other.siteIdentity && targetArea == other.targetArea;
		}
	};

	void logObjectiveTargetDiagnostic(
		RuntimeProfiler &profiler, plid_t pluginId,
		std::uint32_t targetId,
		int entityIndex,
		const char *classname,
		std::uint32_t registeredSites,
		std::uint32_t registeredFuncBombTargetSites,
		std::uint32_t registeredInfoBombTargetSites,
		const edict_t *entity,
		const nav::NavVector &center,
		nav::AreaId nearestNavArea,
		nav::AreaId candidateNavArea,
		std::uint32_t candidateAreaSamples,
		std::uint32_t uniqueCandidateAreas,
		std::uint32_t reachableCandidateAreas,
		float bestReachableCost,
		const char *cacheState,
		bool valid,
		const char *rejectionReason)
	{
		if (pluginId == nullptr || gpMetaUtilFuncs == nullptr ||
			gpMetaUtilFuncs->pfnLogConsole == nullptr || entity == nullptr)
		{
			return;
		}
		profiledLogConsole(profiler,
			pluginId,
			"profile objectiveTarget target_id=%u entity_index=%d classname=%s "
			"registered_site_count=%u registered_func_bomb_target_count=%u "
			"registered_info_bomb_target_count=%u "
			"origin=(%.1f %.1f %.1f) mins=(%.1f %.1f %.1f) "
			"maxs=(%.1f %.1f %.1f) center=(%.1f %.1f %.1f) "
			"nearest_nav_area=%u candidate_nav_area=%u site_identity=%u "
			"candidate_area_samples=%u unique_nav_candidate_areas=%u "
			"reachable_nav_candidate_areas=%u best_reachable_cost=%.1f "
			"cache_state=%s valid=%d rejection_reason=%s",
		static_cast<unsigned int>(targetId), entityIndex,
		classname == nullptr ? "" : classname,
		static_cast<unsigned int>(registeredSites),
		static_cast<unsigned int>(registeredFuncBombTargetSites),
		static_cast<unsigned int>(registeredInfoBombTargetSites),
		entity->v.origin.x, entity->v.origin.y, entity->v.origin.z,
			entity->v.mins.x, entity->v.mins.y, entity->v.mins.z,
			entity->v.maxs.x, entity->v.maxs.y, entity->v.maxs.z,
			center.x, center.y, center.z,
			static_cast<unsigned int>(nearestNavArea),
			static_cast<unsigned int>(candidateNavArea),
			static_cast<unsigned int>(targetId),
			static_cast<unsigned int>(candidateAreaSamples),
			static_cast<unsigned int>(uniqueCandidateAreas),
			static_cast<unsigned int>(reachableCandidateAreas),
			bestReachableCost,
			cacheState == nullptr ? "unknown" : cacheState,
			valid ? 1 : 0,
			rejectionReason == nullptr ? "unknown" : rejectionReason);
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
		  hookedGameDllFunctions_(),
		  engineFunctions_(nullptr), globals_(nullptr), roundFreezeScheduled_(false),
		  roundFreezeStartTime_(0.0f), roundFreezeUntil_(0.0f),
		  roundFreezeGeneration_(runtime::LifecycleSession::kInvalidGeneration),
		  roundLifecycleTracker_(),
		  lifecycle_(), actorRegistry_(),
			  fakeClientManager_(lifecycle_, actorRegistry_),
			  inputDispatcher_(lifecycle_, actorRegistry_), compatibilitySurface_(),
	compatibilityRandomSource_(), observationAdapter_(), runtimeProfiler_(), navLoader_(),
			  navPublisher_(), navLoadDiagnostic_(), adapterFrameCount_(0U), pluginId_(nullptr),
			  nativeBotGuard_(),
	nativeGuardDecision_({NativeBotGuardState::Unsupported,
	NativeBotGuardReason::ControlsUnavailable, false}),
	managedBotSlots_(), managedBotTeamNumbers_(), managedBotHandles_(), managedBotNames_(),
			  joinControllers_(),
			  pendingJoinActions_(),
		userMessageKind_(UserMessageKind::None), userMessageTarget_(nullptr),
		userMessageFieldCount_(0U), userMessageWeaponState_(0U),
		userMessageWeaponId_(0U), userMessageWeaponMessageValid_(false),
		userMessageMenuType_(0U), userMessageNeedMore_(0U),
		userMessageValidSlots_(0U), userMessageTeamSlot_(0U), userMessageShowFragmentActive_(false),
		userMessageText_(),
		userMessageTextLength_(0U),
		managedBotAreaVisitHistory_(),
		managedBotMovement_(), managedBotCombat_(), managedBotObjectives_(),
	managedBotObjectiveTargets_(),
	managedObjectiveSiteRegistry_(),
	managedBotStateMachines_(), managedBotPerception_(),
	managedBotPerceptionFullUpdates_(), managedBotCommandSequences_(),
	managedBotFullUpdateSequences_(), managedBotStateRounds_(), managedBotWasDead_(),
	managedBotTiming_(), managedBotJumpCrouch_(), managedBotCommandTemplates_(),
	managedBotCommandTemplateValid_(), managedBotActionStopsMovement_(),
			  movementDiagnosticSamples_(),
			  movementDiagnosticAttempts_(),
			  movementDiagnosticUnavailable_(),
			  movementResumeFrames_(),
			  movementLastDeadFrames_(),
			movementSettledDeadFrames_(),
		movementWarmupFrames_(),
		movementGroundLookaheadRoutes_(),
		movementPhysicsSamples_(),
		movementReadyLogged_(),
		movementGoalDiagnosticSeconds_(),
		movementGateDiagnosticTimes_(),
		movementTraversalDiagnosticSeconds_(),
		movementTraversalPending_(),
		movementDispatchFrames_(),
		movementWasAirborne_(), movementFallLandingTrackers_(),
		movementReversalDiagnostics_(),
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
			movementGateDiagnosticTimes_.fill(std::numeric_limits<float>::quiet_NaN());
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
		observationAdapter_.configure(engineFunctions_, globals_);
		observationAdapter_.setObservationProviders(&publicObservations_, &optionalObservations_);
		observationAdapter_.setSmokeObservations(&smokeObservations_);
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
			managedBotCurrentWeapons_.fill(ManagedCurrentWeaponObservation{});
			buyingComplete_ = {};
			escapingWithKnife_ = {};
			buyReceiptTimeoutLogged_ = {};
			deathDangerRecorded_ = {};
			buyControllers_ = {};
			if (globals_ != nullptr && compatibilitySurface_.configuration().mode == compat::RuntimeMode::Compatibility)
			{
				roundPlantDecision_.begin(lifecycle_.mapGeneration(), lifecycle_.roundGeneration(),
					globals_->time, compatibilityRandomSource_);
			}
			objectiveKnowledge_ = {};
			publicScoreStatuses_ = {};
			bombNoticeKnown_ = {};
			bombNoticePlanted_ = {};
			publicBombClocks_ = {};
			scenarioEscapeAreas_ = {};
			bombSearchMemory_ = {};
			bombSearchTargets_ = {};
			teamworkPolicies_ = {};
			teamworkRadioApplied_ = {};
			soundEvents_ = {};
			scenarioRadio_ = {};
			radioSequence_ = 0U;
			soundSequence_ = 0U;
			deliveredSoundSequences_ = {};
			investigatedNoiseTicks_ = {};
			smokeObservations_.reset();
			publicSmokeEntities_ = {};
			nextSmokeEntity_ = 0U;
			recognizedTargets_ = {};
			recognitionSequences_ = {};
			reactionQueues_ = {};
			for (std::size_t i = 0U; i < managedBotHandles_.size(); ++i)
			{
				publicObservations_.resetSlot(static_cast<std::uint32_t>(i + 1U));
				const auto &handle = managedBotHandles_[i];
				if (handle.actor.actorGeneration != 0U)
					publicObservations_.bind({handle.actor.slot, handle.actor.actorGeneration},
						{lifecycle_.mapGeneration(), lifecycle_.roundGeneration(), adapterFrameCount_});
			}
			managedBotObjectiveActions_.fill(ManagedObjectiveActionState{});
			managedTeamObjectiveAssignmentsValid_ = false;
			managedTeamObjectivePathFailurePending_ = false;
			managedTeamObjectivePathFailureActor_ = {};
			managedTeamObjectivePathFailureGeneration_ = 0U;
			managedTeamObjectivePathFailureStartArea_ = 0U;
			managedTeamObjectivePathFailureFrame_ = 0U;
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

		void PluginRuntime::onCmdStart(const edict_t *entity, std::uint16_t buttons)
		{
			if (state_ != State::ActiveMap || entity == nullptr || entity->free != 0 ||
				engineFunctions_ == nullptr || engineFunctions_->pfnIndexOfEdict == nullptr ||
				globals_ == nullptr || !std::isfinite(globals_->time))
			{
				return;
			}
		
			const int entityIndex = engineFunctions_->pfnIndexOfEdict(entity);
			if (entityIndex <= 0)
			{
				return;
			}
			for (const FakeClientHandle &handle : managedBotHandles_)
			{
				if (handle.entity == entity)
				{
					return;
				}
			}
		
			const bool sameExternalActor =
				static_cast<std::uint32_t>(entityIndex) == externalDefuserEntityIndex_ &&
				static_cast<std::uint32_t>(entity->serialnumber) == externalDefuserEntitySerial_;
			const bool usingButton =
				(buttons & static_cast<std::uint16_t>(IN_USE)) != 0U;
			const bool livingCounterTerrorist = static_cast<int>(entity->v.team) == 2 &&
				entity->v.deadflag == DEAD_NO && std::isfinite(entity->v.health) &&
				entity->v.health > 0.0f;
			if (!usingButton || !livingCounterTerrorist ||
				!std::isfinite(entity->v.origin.x) || !std::isfinite(entity->v.origin.y) ||
				!std::isfinite(entity->v.origin.z))
			{
				if (sameExternalActor)
				{
					externalDefuserEntityIndex_ = 0U;
					externalDefuserEntitySerial_ = 0U;
					externalDefuserLeaseUntil_ = 0.0f;
					managedTeamObjectiveAssignmentsValid_ = false;
				}
				return;
			}
			if (engineFunctions_->pfnFindEntityByString == nullptr ||
				engineFunctions_->pfnSzFromIndex == nullptr)
			{
				return;
			}
		
			constexpr float kDefuseUseRadius = 100.0f;
			constexpr float kDefuseUseRadiusSquared = kDefuseUseRadius * kDefuseUseRadius;
			edict_t *searchAfter = nullptr;
			bool nearPlantedBomb = false;
			const int searchLimit = (std::max)(1, (std::min)(globals_->maxEntities, 2048));
			for (int searchIndex = 0; searchIndex < searchLimit; ++searchIndex)
			{
				edict_t *candidate = engineFunctions_->pfnFindEntityByString(
					searchAfter, "classname", "grenade");
				if (candidate == nullptr || candidate == searchAfter)
				{
					break;
				}
				searchAfter = candidate;
				if (candidate->free != 0)
				{
					continue;
				}
				const char *classname = engineFunctions_->pfnSzFromIndex(candidate->v.classname);
				const char *model = engineFunctions_->pfnSzFromIndex(candidate->v.model);
				if (classname == nullptr || std::strcmp(classname, "grenade") != 0 ||
					model == nullptr || std::strstr(model, "w_c4.mdl") == nullptr ||
					!std::isfinite(candidate->v.origin.x) ||
					!std::isfinite(candidate->v.origin.y) ||
					!std::isfinite(candidate->v.origin.z))
				{
					continue;
				}
		
				const float dx = entity->v.origin.x - candidate->v.origin.x;
				const float dy = entity->v.origin.y - candidate->v.origin.y;
				const float dz = entity->v.origin.z - candidate->v.origin.z;
				if (dx * dx + dy * dy + dz * dz <= kDefuseUseRadiusSquared)
				{
					nearPlantedBomb = true;
					break;
				}
			}
			if (!nearPlantedBomb)
			{
				if (sameExternalActor)
				{
					externalDefuserEntityIndex_ = 0U;
					externalDefuserEntitySerial_ = 0U;
					externalDefuserLeaseUntil_ = 0.0f;
					managedTeamObjectiveAssignmentsValid_ = false;
				}
				return;
			}
		
			externalDefuserEntityIndex_ = static_cast<std::uint32_t>(entityIndex);
			externalDefuserEntitySerial_ = static_cast<std::uint32_t>(entity->serialnumber);
			externalDefuserLeaseUntil_ = globals_->time + 0.25f;
			externalDefuserMapGeneration_ = lifecycle_.mapGeneration();
			externalDefuserRoundGeneration_ = lifecycle_.roundGeneration();
			managedTeamObjectiveAssignmentsValid_ = false;
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
			optionalObservations_.probeLoadedGameDll();
			dangerMemory_.reset(lifecycle_.mapGeneration());
			if (gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr && pluginId_ != nullptr)
			{
				profiledLogConsole(runtimeProfiler_, pluginId_, "observation provider: %s (API %d.%d)",
					optionalObservations_.diagnostic(), optionalObservations_.majorVersion(), optionalObservations_.minorVersion());
			}
			publicObservations_.reset();
			teamScores_ = {};
			teamScoresKnown_ = {};
			scenarioRadio_ = {};
			radioSequence_ = 0U;
			roundPlantDecision_ = {};
			if (globals_ != nullptr && compatibilitySurface_.configuration().mode == compat::RuntimeMode::Compatibility)
			{
				roundPlantDecision_.begin(lifecycle_.mapGeneration(), lifecycle_.roundGeneration(),
					globals_->time, compatibilityRandomSource_);
			}
			soundEvents_ = {};
			soundSequence_ = 0U;
			deliveredSoundSequences_ = {};
			smokeObservations_.reset();
			publicSmokeEntities_ = {};
			nextSmokeEntity_ = 0U;
			nextPopulationUpdate_ = 0.0f;
			objectiveKnowledge_ = {};
			roundLifecycleTracker_.reset();
			roundFreezeScheduled_ = false;
				roundFreezeStartTime_ = 0.0f;
				roundFreezeUntil_ = 0.0f;
				roundFreezeGeneration_ = lifecycle_.roundGeneration();
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
						profiledLogConsole(runtimeProfiler_,
							pluginId_, "menu message ids show=%d vgui=%d team=%d",
							showMenuId, vguiMenuId, teamInfoId);
						const compat::RuntimeMode mode = compatibilitySurface_.configuration().mode;
						profiledLogConsole(runtimeProfiler_,
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
				optionalObservations_.reset();
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
					profiledLogConsole(runtimeProfiler_,
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
				profiledLogConsole(runtimeProfiler_,
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
			if (engineFunctions_ != nullptr && engineFunctions_->pfnPEntityOfEntIndex != nullptr)
			{
				for (const PublicSmoke &smoke : publicSmokeEntities_)
				{
					if (!smoke.entity.isValid())
					{
						continue;
					}
					edict_t *entity = engineFunctions_->pfnPEntityOfEntIndex(static_cast<int>(smoke.entity.id));
					if (entity != nullptr && !entity->free &&
						static_cast<std::uint32_t>(entity->serialnumber) == smoke.entity.generation)
					{
						smokeObservations_.observe(smoke.entity, smoke.position, globals_->time + 4.0f);
					}
				}
			}
			if (roundFreezeActive())
			{
				for (std::size_t index = 0U; index < managedBotHandles_.size(); ++index)
				{
					if (!managedBotSlots_[index])
					{
						continue;
					}
					edict_t *entity = managedBotHandles_[index].entity;
					if (entity == nullptr || entity->free != 0)
					{
						continue;
					}
					const MovementVelocity velocity =
						MovementExecutionGate::stabilizeVelocity(
							MovementExecutionPhase::RoundFreeze,
							{entity->v.velocity.x,
							 entity->v.velocity.y,
							 entity->v.velocity.z});
					entity->v.velocity.x = velocity.x;
					entity->v.velocity.y = velocity.y;
					entity->v.velocity.z = velocity.z;
				}
			}
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
					profiledLogConsole(runtimeProfiler_, pluginId_,
						"profile window game_seconds=%.3f wall_seconds=%.6f frames=%u",
						report.windowSeconds, report.wallWindowSeconds,
						static_cast<unsigned int>(report.frameCount));
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
		profiledLogConsole(runtimeProfiler_,
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
			(void)messageDestination;
			(void)origin;
			publicMessageName_ = nullptr;
			publicMessageTarget_ = entity;
			publicMessageDestination_ = messageDestination;
			publicMessageActor_ = {};
			publicMessageValues_ = {};
			publicMessageCoords_ = {};
			publicMessageCoordMask_ = 0U;
			publicMessageString_.fill('\0');
			publicMessageStringMask_ = 0U;
			publicMessageFieldCount_ = 0U;
			publicMessageMalformed_ = false;
			publicMessageFrame_ = {lifecycle_.mapGeneration(), lifecycle_.roundGeneration(), adapterFrameCount_};
			if (gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnGetUserMsgID != nullptr && pluginId_ != nullptr)
			{
				const char *const names[] = {"WeaponList", "CurWeapon", "AmmoX", "AmmoPickup", "WeapPickup",
					"Money", "StatusIcon", "ScreenFade", "BarTime", "DeathMsg", "ResetHUD", "TextMsg", "TeamScore", "SendAudio", "ScoreAttrib", "ArmorType", "BombDrop", "BombPickup"};
				for (const char *name : names)
				{
					int size = 0;
					const int id = gpMetaUtilFuncs->pfnGetUserMsgID(pluginId_, name, &size);
					if (id > 0 && id == messageType)
					{
						publicMessageName_ = name;
						break;
					}
				}
			}
			if (publicMessageName_ != nullptr && entity != nullptr)
			{
				for (const FakeClientHandle &handle : managedBotHandles_)
				{
					if (handle.entity == entity && handle.actor.actorGeneration != 0U)
					{
						publicMessageActor_ = {handle.actor.slot, handle.actor.actorGeneration};
						publicObservations_.bind(publicMessageActor_, publicMessageFrame_);
						break;
					}
				}
			}
			const bool continueShowMenu = userMessageShowFragmentActive_;
			userMessageKind_ = UserMessageKind::None;
			userMessageTarget_ = nullptr;
			userMessageFieldCount_ = 0U;
			userMessageWeaponState_ = 0U;
			userMessageWeaponId_ = 0U;
			userMessageWeaponMessageValid_ = false;
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
			int curWeaponId = 0;
			if (gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnGetUserMsgID != nullptr &&
				pluginId_ != nullptr)
			{
				const int resolvedShowMenuId =
					gpMetaUtilFuncs->pfnGetUserMsgID(pluginId_, "ShowMenu", &messageSize);
				const int resolvedVguiMenuId =
					gpMetaUtilFuncs->pfnGetUserMsgID(pluginId_, "VGUIMenu", &messageSize);
				const int resolvedTeamInfoId =
					gpMetaUtilFuncs->pfnGetUserMsgID(pluginId_, "TeamInfo", &messageSize);
				const int resolvedCurWeaponId =
					gpMetaUtilFuncs->pfnGetUserMsgID(pluginId_, "CurWeapon", &messageSize);
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
				if (resolvedCurWeaponId > 0)
				{
					curWeaponId = resolvedCurWeaponId;
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
			else if (curWeaponId > 0 && messageType == curWeaponId)
			{
				userMessageKind_ = UserMessageKind::CurWeapon;
			}
			if (userMessageKind_ == UserMessageKind::None ||
				(userMessageKind_ != UserMessageKind::TeamInfo && entity == nullptr))
			{
				return;
			}

			if (userMessageKind_ != UserMessageKind::CurWeapon &&
				gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr &&
				pluginId_ != nullptr)
			{
				const int slot = entity != nullptr && engineFunctions_ != nullptr &&
						engineFunctions_->pfnIndexOfEdict != nullptr
					? engineFunctions_->pfnIndexOfEdict(entity)
					: 0;
				profiledLogConsole(runtimeProfiler_,
					pluginId_, "menu message begin kind=%d type=%d slot=%d frame=%u",
					static_cast<int>(userMessageKind_), messageType, slot,
					static_cast<unsigned int>(adapterFrameCount_));
			}
			userMessageTarget_ = entity;
		}

		void PluginRuntime::onMessageEnd()
		{
			capturePublicMessage();
			if (userMessageKind_ == UserMessageKind::CurWeapon)
			{
				captureManagedCurrentWeapon();
			}
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
			userMessageWeaponState_ = 0U;
			userMessageWeaponId_ = 0U;
			userMessageWeaponMessageValid_ = false;
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
			appendPublicMessageInteger(value);
			if (userMessageKind_ == UserMessageKind::CurWeapon)
			{
				if (userMessageFieldCount_ == 0U && (value == 0 || value == 1))
				{
					userMessageWeaponState_ = static_cast<std::uint8_t>(value);
					userMessageWeaponMessageValid_ = true;
				}
				else if (userMessageFieldCount_ == 1U && value >= 0 && value <= 255)
				{
					userMessageWeaponId_ = static_cast<std::uint8_t>(value);
				}
			}
			else if (userMessageKind_ == UserMessageKind::TeamInfo && userMessageFieldCount_ == 0U)
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
					profiledLogConsole(runtimeProfiler_,
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
			appendPublicMessageInteger(value);
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
			if (publicMessageName_ != nullptr)
			{
				if (value == nullptr || publicMessageFieldCount_ >= publicMessageValues_.size() ||
					std::strlen(value) >= publicMessageString_.size())
					publicMessageMalformed_ = true;
				else
				{
					publicMessageStringMask_ |= static_cast<std::uint16_t>(1U << publicMessageFieldCount_);
					std::memcpy(publicMessageString_.data(), value, std::strlen(value) + 1U);
					std::memcpy(publicMessageStrings_[publicMessageFieldCount_].data(), value, std::strlen(value) + 1U);
					++publicMessageFieldCount_;
				}
			}
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
					profiledLogConsole(runtimeProfiler_,
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
					profiledLogConsole(runtimeProfiler_,
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
		profiledLogConsole(runtimeProfiler_,
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
publicTeamKnown_[index] = true;
	if (!managedBotSlots_[index] || managedBotHandles_[index].entity == nullptr)
	{
		return;
	}
	const FakeClientHandle &handle = managedBotHandles_[index];
	if (gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr &&
			pluginId_ != nullptr)
	{
		profiledLogConsole(runtimeProfiler_,
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
	deferJoinAction(index, joinControllers_[index].onTeamInfo(teamName));
}

bool PluginRuntime::hasObservedActiveWeapon(std::size_t index) const
{
	if (index >= managedBotHandles_.size())
	{
		return false;
	}
	const auto &handle = managedBotHandles_[index];
	compat::PublicActorState state{};
	return publicObservations_.read({handle.actor.slot, handle.actor.actorGeneration},
		{lifecycle_.mapGeneration(), lifecycle_.roundGeneration(), adapterFrameCount_}, &state) && state.weaponKnown;
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
		const int team = publicTeamKnown_[static_cast<std::size_t>(slot - 1)]
			? managedBotTeamNumbers_[static_cast<std::size_t>(slot - 1)] : entity->v.team;
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
				if (!managedBotSlots_[index] || managedBotTeamNumbers_[index] != team)
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
		if (team == 0 && teamScoresKnown_[1] && teamScoresKnown_[2])
		{
			team = teamScores_[1] > teamScores_[2] ? 1 : teamScores_[2] > teamScores_[1] ? 2 : 0;
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

void PluginRuntime::updateOutgoingPublicRadio(std::size_t index)
{
	if (index >= managedBotHandles_.size() || globals_ == nullptr || engineFunctions_ == nullptr ||
		engineFunctions_->pfnPEntityOfEntIndex == nullptr) return;
	const auto &handle = managedBotHandles_[index];
	if (handle.entity == nullptr || handle.entity->free) return;
	const char *verbosity = engineFunctions_->pfnCVarGetString != nullptr ?
		engineFunctions_->pfnCVarGetString("bot_chatter") : nullptr;
	if (outgoingRadioMap_ != lifecycle_.mapGeneration())
	{
		outgoingRadioMap_ = lifecycle_.mapGeneration();
		outgoingChatterCatalog_ = {};
		outgoingMissingPhraseLogged_.fill(false);
		if (engineFunctions_->pfnLoadFileForMe != nullptr && engineFunctions_->pfnFreeFile != nullptr)
		{
			int length = 0;
			char filename[] = "BotChatter.db";
			unsigned char *bytes = engineFunctions_->pfnLoadFileForMe(filename, &length);
			if (bytes != nullptr)
			{
				if (length > 0 && length <= 1024 * 1024)
					(void)outgoingChatterCatalog_.loadText(reinterpret_cast<const char *>(bytes), static_cast<std::size_t>(length));
				engineFunctions_->pfnFreeFile(bytes);
			}
		}
	}
	if (outgoingRadioRound_ != lifecycle_.roundGeneration())
	{
		outgoingRadioRound_ = lifecycle_.roundGeneration();
		outgoingRadioSchedulers_.fill(compat::OutgoingRadioScheduler{});
		outgoingRadioAttempts_.fill(0U);
		outgoingRadioAttemptTimes_.fill(-1.0f);
		outgoingRadioAttemptChoices_.fill(compat::RadioMenuChoice{});
		outgoingTeamLastSpeech_.fill(-1.0f);
		outgoingTeamBusyUntil_.fill(0.0f);
		outgoingTeamCurrentPhrase_.fill(compat::ChatterPhrase::None);
	}
	compat::OutgoingRadioInput input{};
	input.actor = {handle.actor.slot, handle.actor.actorGeneration};
	input.frame = {lifecycle_.mapGeneration(), lifecycle_.roundGeneration(), adapterFrameCount_};
	input.now = globals_->time;
	input.alive = handle.entity->v.deadflag == DEAD_NO && handle.entity->v.health > 0.0f;
	input.team = managedBotTeamNumbers_[index];
	if (input.team != 1 && input.team != 2) return;
	input.teamLastSpeech = outgoingTeamLastSpeech_[input.team];
	input.teamBusyUntil = outgoingTeamBusyUntil_[input.team];
	input.teamCurrentPhrase = outgoingTeamCurrentPhrase_[input.team];
	input.reactionSeconds = managedProfiles_[index].reactionTimeSeconds;
	input.inCombat = managedBotStateMachines_[index].attackOverlayActive();
	for (int slot = 1; slot <= (std::min)(32, globals_->maxClients); ++slot)
	{
		edict_t *player = engineFunctions_->pfnPEntityOfEntIndex(slot);
		if (player == nullptr || player == handle.entity || player->free ||
			(player->v.flags & FL_CLIENT) == 0 || player->v.deadflag != DEAD_NO || player->v.health <= 0) continue;
		if (player->v.team == input.team)
		{
			++input.friendCount;
		}
		else if (player->v.team == 1 || player->v.team == 2) ++input.enemiesRemaining;
	}
	// Count the observer's current public FOV/LOS/smoke-filtered contacts. A
	// selected combat target is only one contact and cannot express outnumbering.
	world::WorldSnapshot belief;
	world::ActorKey selected{};
	world::WorldPosition selectedPosition{};
	compat::PublicActorState ownObservation{};
	const bool blind = publicObservations_.read(input.actor, input.frame, &ownObservation) &&
		globals_->time < ownObservation.blindUntil;
	const bool haveBelief = !blind && buildManagedWorldSnapshot(index, &belief, &selected, &selectedPosition);
	const auto contacts = outgoingRadioContactMemory_[index].update(input.actor, input.frame, input.now,
		haveBelief ? &belief : nullptr, blind, input.friendCount, input.enemiesRemaining);
	input.visibleEnemyCount = contacts.enemies;
	input.nearbyFriends = contacts.friends;
	const auto &order = scenarioRadio_[index];
	const auto &decision = scenarioDecisions_[index];
	if (order.senderHuman && static_cast<int>(order.team) == input.team)
	{
		input.humanOrderSequence = order.sequence;
		input.humanOrderReceived = order.receivedTime;
		input.humanOrderAccepted = decision.action == compat::ScenarioAction::Follow ||
			decision.action == compat::ScenarioAction::Guard || decision.action == compat::ScenarioAction::Hunt ||
			decision.action == compat::ScenarioAction::Escape;
		input.humanOrderHelp = order.command == compat::ScenarioRadioCommand::NeedBackup;
		outgoingTeamLastSpeech_[input.team] = (std::max)(outgoingTeamLastSpeech_[input.team], order.receivedTime);
	}
	const char *mapName = engineFunctions_->pfnSzFromIndex != nullptr ? engineFunctions_->pfnSzFromIndex(globals_->mapname) : nullptr;
	input.vipEscaping = mapName != nullptr && std::strncmp(mapName, "as_", 3U) == 0 && input.team == 2 &&
		decision.action == compat::ScenarioAction::Escape;
	input.plantingBomb = managedBotStateMachines_[index].state() == compat::CompatibilityStateId::PlantBomb;
	input.escortingHostages = decision.escortInferred;
	input.bombLocationUnknown = input.team == 2 && bombNoticeKnown_[index] && bombNoticePlanted_[index] &&
		decision.action == compat::ScenarioAction::Move && decision.hasTarget &&
		bombSearchTargets_[index].knowledge != compat::ScenarioKnowledge::Unknown &&
		bombSearchTargets_[index].frame.mapGeneration == input.frame.mapGeneration &&
		bombSearchTargets_[index].frame.roundGeneration == input.frame.roundGeneration &&
		decision.target.entity == bombSearchTargets_[index].entity;
	const auto output = outgoingRadioSchedulers_[index].update(input, outgoingChatterCatalog_);
	if (verbosity != nullptr && std::strcmp(verbosity, "off") == 0 && output.phrase != compat::ChatterPhrase::None)
	{
		outgoingRadioSchedulers_[index].discardPending();
		return;
	}
	if (output.status == compat::OutgoingRadioStatus::MissingPhrase)
	{
		const auto phrase = static_cast<unsigned>(output.phrase);
		if (phrase < outgoingMissingPhraseLogged_.size() && !outgoingMissingPhraseLogged_[phrase])
		{
			outgoingMissingPhraseLogged_[phrase] = true;
			if (gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr && pluginId_ != nullptr)
				gpMetaUtilFuncs->pfnLogConsole(pluginId_, "outgoing chatter unavailable phrase=%s host_radio_mapping=%d voice_bank=unavailable",
					compat::ChatterRadioCatalog::name(output.phrase), outgoingChatterCatalog_.loaded() ? 1 : 0);
		}
		return;
	}
	if (output.status != compat::OutgoingRadioStatus::Ready) return;
	if (verbosity != nullptr && std::strcmp(verbosity, "minimal") == 0 &&
		output.phrase != compat::ChatterPhrase::None && !outgoingChatterCatalog_.important(output.phrase))
	{
		outgoingRadioSchedulers_[index].discardPending();
		return;
	}
	const char *menu = output.menu == 1 ? "radio1" : output.menu == 2 ? "radio2" : "radio3";
	if (outgoingRadioAttemptGenerations_[index] != handle.actor.actorGeneration ||
		outgoingRadioAttemptChoices_[index].menu != output.menu ||
		outgoingRadioAttemptChoices_[index].slot != output.slot)
	{
		outgoingRadioAttemptGenerations_[index] = handle.actor.actorGeneration;
		outgoingRadioAttemptChoices_[index] = {output.menu, output.slot};
		outgoingRadioAttempts_[index] = 0U;
		outgoingRadioAttemptTimes_[index] = -1.0f;
	}
	if (outgoingRadioAttempts_[index] >= 3U)
	{
		outgoingRadioSchedulers_[index].discardPending();
		outgoingRadioAttemptChoices_[index] = {};
		if (gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr && pluginId_ != nullptr)
			gpMetaUtilFuncs->pfnLogConsole(pluginId_, "outgoing radio receipt unavailable bot=%u menu=%d slot=%d attempts=3",
				static_cast<unsigned>(handle.actor.slot), output.menu, output.slot);
		return;
	}
	if (outgoingRadioAttemptTimes_[index] >= 0.0f &&
		globals_->time - outgoingRadioAttemptTimes_[index] < 0.2f) return;
	outgoingRadioAttemptTimes_[index] = globals_->time;
	++outgoingRadioAttempts_[index];
	const std::uint32_t receiptBefore = outgoingRadioReceipts_[index];
	char slot[] = {static_cast<char>('0' + output.slot), '\0'};
	const bool opened = dispatchClientCommand(handle.entity, menu, nullptr);
	const bool menuSelected = opened && dispatchClientCommand(handle.entity, "menuselect", slot);
	if (opened) (void)dispatchClientCommand(handle.entity, "menuselect", "10");
	// A client command reaching the GameDLL does not prove that its radio
	// quota/cooldown accepted it. Only public SendAudio confirms a sent phrase.
	if (menuSelected && outgoingRadioReceipts_[index] != receiptBefore)
	{
		outgoingRadioSchedulers_[index].confirmSent(input, output);
		outgoingRadioAttempts_[index] = 0U;
		outgoingRadioAttemptChoices_[index] = {};
		outgoingTeamLastSpeech_[input.team] = globals_->time;
		outgoingTeamBusyUntil_[input.team] = globals_->time + 2.1f;
		outgoingTeamCurrentPhrase_[input.team] = output.phrase;
	}
}

void PluginRuntime::updatePublicBuying(std::size_t index)
{
	if (index >= managedBotHandles_.size() || globals_ == nullptr)
		return;
	const FakeClientHandle &handle = managedBotHandles_[index];
	if (handle.entity == nullptr || handle.entity->free || handle.entity->v.deadflag != DEAD_NO)
		return;
	compat::PublicActorState observation{};
	if (!publicObservations_.read({handle.actor.slot, handle.actor.actorGeneration},
		{lifecycle_.mapGeneration(), lifecycle_.roundGeneration(), adapterFrameCount_}, &observation))
		return;
	compat::BuyContext context{};
	context.nowSeconds = globals_->time;
	context.stateEnteredSeconds = roundFreezeStartTime_;
	context.generation = handle.actor.actorGeneration;
	context.round = lifecycle_.roundGeneration();
	context.alive = true;
	context.inBuyZone = observation.buyZoneKnown && observation.inBuyZone;
	const float buyTime = readOptionalCvarFloat(engineFunctions_, "mp_buytime");
	context.buyTimeOpen = std::isfinite(buyTime) && globals_->time <= roundFreezeStartTime_ + buyTime * 60.0f;
	context.team = managedBotTeamNumbers_[index] == 1U ? compat::ProfileTeam::Terrorist
		: managedBotTeamNumbers_[index] == 2U ? compat::ProfileTeam::CounterTerrorist : compat::ProfileTeam::Any;
	context.moneyKnown = observation.moneyKnown;
	context.money = observation.money;
	context.weaponMaskKnown = true;
	context.weaponMask = static_cast<std::uint32_t>(handle.entity->v.weapons);
	const auto allowed = [this](const char *name) {
		if (engineFunctions_ == nullptr || engineFunctions_->pfnCVarGetPointer == nullptr ||
			engineFunctions_->pfnCVarGetPointer(name) == nullptr)
			return true;
		const float value = readOptionalCvarFloat(engineFunctions_, name);
		return !std::isfinite(value) || value != 0.0f;
	};
	context.allowRifles = allowed("bot_allow_rifles");
	context.allowSnipers = allowed("bot_allow_snipers");
	context.allowShotguns = allowed("bot_allow_shotguns");
	context.allowMachineGuns = allowed("bot_allow_machine_guns");
	context.allowSubMachineGuns = allowed("bot_allow_sub_machine_guns");
	context.allowPistols = allowed("bot_allow_pistols");
	context.allowGrenades = allowed("bot_allow_grenades");
	context.allowArmor = true;
	context.armorKnown = std::isfinite(handle.entity->v.armorvalue);
	context.armor = static_cast<int>(handle.entity->v.armorvalue);
	context.helmetKnown = observation.helmetKnown;
	context.hasHelmet = observation.hasHelmet;
	context.allowDefuseKit = true;
	context.defuseKitKnown = observation.defuseKitKnown;
	context.hasDefuseKit = observation.hasDefuseKit;
	context.random = &compatibilityRandomSource_;
	context.randomActor = {handle.actor.slot, handle.actor.actorGeneration};
	context.randomTiming = {managedBotCommandSequences_[index], 0U, managedBotFullUpdateSequences_[index]};
	context.bombScenario = managedObjectiveSiteRegistry_.registeredCount != 0U;
	const compat::PublicWeaponDefinition weapon = publicObservations_.definition(observation.weaponId);
	context.ammoKnown = observation.weaponKnown && weapon.known && weapon.ammoIndex >= 0 &&
		observation.ammoKnown[static_cast<std::size_t>(weapon.ammoIndex)];
	if (context.ammoKnown)
	{
		const bool needsAmmo = observation.ammo[static_cast<std::size_t>(weapon.ammoIndex)] < weapon.maxAmmo;
		context.needsPrimaryAmmo = weapon.slot == 0 && needsAmmo;
		context.needsSecondaryAmmo = weapon.slot == 1 && needsAmmo;
	}
	const compat::BuyDecision decision = buyControllers_[index].update(context, managedProfiles_[index]);
	buyingComplete_[index] = decision.status == compat::BuyStatus::Complete;
	if (decision.status != compat::BuyStatus::ReceiptTimeout)
		buyReceiptTimeoutLogged_[index] = false;
	if (decision.status == compat::BuyStatus::ReceiptTimeout && !buyReceiptTimeoutLogged_[index] && gpMetaUtilFuncs != nullptr &&
		gpMetaUtilFuncs->pfnLogConsole != nullptr && pluginId_ != nullptr)
	{
		buyReceiptTimeoutLogged_[index] = true;
		profiledLogConsole(runtimeProfiler_, pluginId_, "buy receipt unconfirmed actor=%u map=%u round=%u; equipment remains unknown",
			static_cast<unsigned int>(handle.actor.slot), lifecycle_.mapGeneration(), lifecycle_.roundGeneration());
	}
	if (decision.status == compat::BuyStatus::Command && decision.command != nullptr)
		(void)dispatchClientCommand(handle.entity, decision.command, "");
}

void PluginRuntime::onEmitSound(edict_t *entity, const char *sample, float volume, float attenuation)
{
	if (entity == nullptr || entity->free || sample == nullptr || globals_ == nullptr ||
		engineFunctions_ == nullptr || engineFunctions_->pfnIndexOfEdict == nullptr ||
		!std::isfinite(volume) || volume <= 0.0f || volume > 1.0f || !std::isfinite(attenuation))
	{
		return;
	}
	const int entityIndex = engineFunctions_->pfnIndexOfEdict(entity);
	const world::WorldVector origin{entity->v.origin.x, entity->v.origin.y, entity->v.origin.z};
	const bool bombBeep = std::strstr(sample, "c4_beep") != nullptr;
	if (std::strstr(sample, "sg_explode") != nullptr && entityIndex > 0 && entity->serialnumber != 0)
	{
		const world::EntityKey key{static_cast<std::uint32_t>(entityIndex),
			static_cast<std::uint32_t>(entity->serialnumber)};
		publicSmokeEntities_[nextSmokeEntity_++ % publicSmokeEntities_.size()] = {key, origin};
		smokeObservations_.observe(key, origin, globals_->time + 4.0f);
	}
	world::AudibleEvent event{};
	event.id = ++soundSequence_;
	if (event.id == 0U)
	{
		event.id = ++soundSequence_;
		deliveredSoundSequences_ = {};
		soundEvents_ = {};
		soundTeams_ = {};
	}
	event.kind = bombBeep ? world::SoundKind::Objective
		: std::strstr(sample, "pl_step") != nullptr ? world::SoundKind::Footstep
		: std::strncmp(sample, "weapons/", 8U) == 0 ? world::SoundKind::WeaponFire : world::SoundKind::Other;
	event.origin = origin;
	event.loudness = volume;
	event.confidence = {1.0f, 0U};
	event.priority = bombBeep ? world::NoisePriority::High : world::NoisePriority::Medium;
	// Derived public sound attenuation, not an exact GameEvent audibility observation.
	event.distance = bombBeep ? 1000.0f
		: attenuation > 0.0f ? (std::min)(4096.0f, 1000.0f * volume / attenuation) : 4096.0f;
	event.timestampTick = adapterFrameCount_;
	if (entityIndex >= 1 && entityIndex <= 32 && (entity->v.flags & FL_CLIENT) != 0)
	{
		event.source = {static_cast<std::uint32_t>(entityIndex), static_cast<std::uint32_t>(entity->serialnumber) + 1U};
	}
	// Multiple callbacks for one public sound in the same tick are one receipt.
	for (const auto &previous : soundEvents_)
	{
		if (previous.id != 0U && previous.timestampTick == event.timestampTick &&
			previous.kind == event.kind && previous.source == event.source &&
			previous.origin.x == event.origin.x && previous.origin.y == event.origin.y && previous.origin.z == event.origin.z)
		{
			--soundSequence_;
			return;
		}
	}
	soundEvents_[event.id % soundEvents_.size()] = event;
	soundTeams_[event.id % soundEvents_.size()] = event.source.isValid() ? entity->v.team : 0;
	if (bombBeep && entityIndex > 0)
	{
		for (std::size_t index = 0U; index < managedBotHandles_.size(); ++index)
		{
			const auto &handle = managedBotHandles_[index];
			if (!managedBotSlots_[index] || handle.entity == nullptr || handle.entity->v.deadflag != DEAD_NO)
			{
				continue;
			}
			const float dx = handle.entity->v.origin.x - origin.x;
			const float dy = handle.entity->v.origin.y - origin.y;
			const float dz = handle.entity->v.origin.z - origin.z;
			if (dx * dx + dy * dy + dz * dz < 1000.0f * 1000.0f)
			{
				objectiveKnowledge_[index][1U] = {
					{static_cast<std::uint32_t>(entityIndex), static_cast<std::uint32_t>(entity->serialnumber)}, origin};
			}
		}
		managedTeamObjectiveAssignmentsValid_ = false;
	}
}

void PluginRuntime::appendPublicMessageInteger(int value)
{
	if (publicMessageName_ == nullptr)
		return;
	if (publicMessageFieldCount_ >= publicMessageValues_.size())
	{
		publicMessageMalformed_ = true;
		return;
	}
	publicMessageValues_[publicMessageFieldCount_++] = value;
}

void PluginRuntime::onWriteLong(int value)
{
	appendPublicMessageInteger(value);
}

void PluginRuntime::onWriteCoord(float value)
{
	if (publicMessageName_ == nullptr) return;
	if (publicMessageFieldCount_ >= publicMessageCoords_.size() || !std::isfinite(value))
	{
		publicMessageMalformed_ = true;
		return;
	}
	publicMessageCoords_[publicMessageFieldCount_] = value;
	publicMessageCoordMask_ |= static_cast<std::uint16_t>(1U << publicMessageFieldCount_);
	++publicMessageFieldCount_;
}

void PluginRuntime::capturePublicMessage()
{
	const char *name = publicMessageName_;
	publicMessageName_ = nullptr;
	if (name == nullptr || publicMessageMalformed_ || !publicMessageFrame_.isValid())
		return;
	const std::size_t count = publicMessageFieldCount_;
	const auto &v = publicMessageValues_;
	if (std::strcmp(name, "BombDrop") == 0 || std::strcmp(name, "BombPickup") == 0)
	{
		// Vanilla sends radar updates individually to T. Never redistribute a
		// message sent to a human, CT, or an unrelated/reused client slot.
		if (!publicMessageActor_.isValid() || publicMessageTarget_ == nullptr ||
			(publicMessageDestination_ != MSG_ONE && publicMessageDestination_ != MSG_ONE_UNRELIABLE)) return;
		const std::size_t index = static_cast<std::size_t>(publicMessageActor_.slot - 1U);
		if (index >= managedBotSlots_.size() || !managedBotSlots_[index] ||
			managedBotHandles_[index].entity != publicMessageTarget_ ||
			managedBotHandles_[index].actor.actorGeneration != publicMessageActor_.generation ||
			managedBotTeamNumbers_[index] != 1U) return;
		if (std::strcmp(name, "BombPickup") == 0 && count == 0U)
			publicBombRadars_[index].reset();
		else if (std::strcmp(name, "BombDrop") == 0 && count == 4U &&
			publicMessageCoordMask_ == 7U && publicMessageStringMask_ == 0U && (v[3] == 0 || v[3] == 1))
		{
			if (v[3] == 0) publicBombRadars_[index].drop(publicMessageFrame_,
				{publicMessageCoords_[0], publicMessageCoords_[1], publicMessageCoords_[2]});
			else publicBombRadars_[index].reset();
		}
		return;
	}
	if (publicMessageCoordMask_ != 0U) return;
	const auto integers = [this](std::uint16_t mask) {
		return publicMessageStringMask_ == mask;
	};
	if (std::strcmp(name, "WeaponList") == 0 && count == 9U && integers(1U))
	{
		(void)publicObservations_.weaponDefinition(v[7], v[1] == 255 ? -1 : v[1],
			v[2] == 255 ? -1 : v[2], v[5]);
		return;
	}
	if (std::strcmp(name, "DeathMsg") == 0 && count == 4U && integers(8U) && v[1] > 0 && v[1] <= 32)
	{
		const std::size_t victim = static_cast<std::size_t>(v[1] - 1);
		if (managedBotSlots_[victim] && !deathDangerRecorded_[victim] && globals_ != nullptr &&
			managedBotHandles_[victim].entity != nullptr)
		{
			const auto navigation = navPublisher_.snapshot();
			const auto *document = navigation.document();
			const auto *entity = managedBotHandles_[victim].entity;
			nav::NavAreaMatch area{};
			nav::NavQuery query(navigation);
			const nav::NavVector position{entity->v.origin.x, entity->v.origin.y, entity->v.origin.z};
			if (document != nullptr && query.findContaining(position, 64.0f, &area) == nav::NavQueryResult::Found)
			{
				(void)dangerMemory_.recordDeath(*document, lifecycle_.mapGeneration(), managedBotTeamNumbers_[victim],
					area.area, position, globals_->time);
			}
			deathDangerRecorded_[victim] = true;
		}
		publicObservations_.resetSlot(static_cast<std::uint32_t>(v[1]));
		reactionQueues_[static_cast<std::size_t>(v[1] - 1)] = {};
		return;
	}
	if (std::strcmp(name, "TeamScore") == 0 && count == 2U && integers(1U))
	{
		const char *teamName = publicMessageStrings_[0U].data();
		const int team = std::strcmp(teamName, "TERRORIST") == 0 ? 1 : std::strcmp(teamName, "CT") == 0 ? 2 : 0;
		if (team != 0 && v[1] >= 0)
		{
			teamScores_[team] = v[1];
			teamScoresKnown_[team] = true;
		}
		return;
	}
	if (std::strcmp(name, "ScoreAttrib") == 0 && count == 2U && integers(0U) && v[0] >= 1 && v[0] <= 32 &&
		v[1] >= 0 && v[1] <= 255 && engineFunctions_ != nullptr && engineFunctions_->pfnPEntityOfEntIndex != nullptr)
	{
		edict_t *player = engineFunctions_->pfnPEntityOfEntIndex(v[0]);
		if (player != nullptr && !player->free)
		{
			for (std::size_t i = 0U; i < publicScoreStatuses_.size(); ++i)
			{
				if ((publicMessageTarget_ == nullptr && (publicMessageDestination_ == MSG_ALL || publicMessageDestination_ == MSG_BROADCAST)) ||
					(publicMessageActor_.isValid() && publicMessageActor_.slot == i + 1U))
				{
					publicScoreStatuses_[i][static_cast<std::size_t>(v[0] - 1)] =
						{true, static_cast<std::uint32_t>(player->serialnumber), publicMessageFrame_, v[1]};
				}
			}
		}
		return;
	}
	if (std::strcmp(name, "TextMsg") == 0 && count >= 2U && (publicMessageStringMask_ & 2U) != 0U)
	{
		const char *token = publicMessageStrings_[1U].data();
		const bool planted = std::strcmp(token, "#Bomb_Planted") == 0;
		const bool cleared = std::strcmp(token, "#Bomb_Defused") == 0 || std::strcmp(token, "#Target_Bombed") == 0;
		if (planted || cleared)
		{
			for (std::size_t i = 0U; i < bombNoticeKnown_.size(); ++i)
			{
				if ((publicMessageTarget_ == nullptr && (publicMessageDestination_ == MSG_ALL || publicMessageDestination_ == MSG_BROADCAST)) ||
					(publicMessageActor_.isValid() && publicMessageActor_.slot == i + 1U))
				{
					bombNoticeKnown_[i] = true;
					bombNoticePlanted_[i] = planted;
					if (globals_ != nullptr)
						publicBombClocks_[i].observe(publicMessageFrame_, globals_->time, planted);
				}
			}
		}
		return;
	}
	if (std::strcmp(name, "SendAudio") == 0 && count == 3U && integers(2U) &&
		v[0] >= 1 && v[0] <= 32 && globals_ != nullptr && engineFunctions_ != nullptr &&
		engineFunctions_->pfnPEntityOfEntIndex != nullptr)
	{
		edict_t *sender = engineFunctions_->pfnPEntityOfEntIndex(v[0]);
		// Observe our own public radio receipt before excluding bot-originated
		// commands from the human-order planner below.
		if (sender != nullptr && !sender->free && (sender->v.flags & FL_FAKECLIENT) != 0 &&
			(publicMessageStringMask_ & 2U) != 0U &&
			std::strncmp(publicMessageStrings_[1U].data(), "%!MRAD_", 7U) == 0)
		{
			const std::size_t senderIndex = static_cast<std::size_t>(v[0] - 1);
			if (managedBotSlots_[senderIndex] && managedBotHandles_[senderIndex].entity == sender)
				++outgoingRadioReceipts_[senderIndex];
		}
		if ((publicMessageTarget_ != nullptr && !publicMessageActor_.isValid()) ||
			(publicMessageTarget_ == nullptr && publicMessageDestination_ != MSG_ALL && publicMessageDestination_ != MSG_BROADCAST))
		{
			return;
		}
		if (sender == nullptr || sender->free || (sender->v.flags & FL_FAKECLIENT) != 0 || sender->serialnumber <= 0)
		{
			return;
		}
		const char *token = publicMessageStrings_[1U].data();
		const char *const names[] = {"%!MRAD_FOLLOWME", "%!MRAD_COVERME", "%!MRAD_STICKTOG", "%!MRAD_REGROUP",
			"%!MRAD_BACKUP", "%!MRAD_POSITION", "%!MRAD_GO", "%!MRAD_GETOUT"};
		const compat::ScenarioRadioCommand commands[] = {compat::ScenarioRadioCommand::FollowMe,
			compat::ScenarioRadioCommand::CoverMe, compat::ScenarioRadioCommand::StickTogether,
			compat::ScenarioRadioCommand::Regroup, compat::ScenarioRadioCommand::NeedBackup,
			compat::ScenarioRadioCommand::HoldPosition, compat::ScenarioRadioCommand::GoGoGo,
			compat::ScenarioRadioCommand::GetOut};
		compat::ScenarioRadioObservation radio{};
		for (std::size_t i = 0U; i < 8U; ++i)
		{
			if (std::strcmp(names[i], token) == 0)
			{
				radio.command = commands[i];
				break;
			}
		}
		if (radio.command == compat::ScenarioRadioCommand::None)
		{
			return;
		}
		radio.sequence = ++radioSequence_;
		radio.sender = {static_cast<std::uint32_t>(v[0]), static_cast<std::uint32_t>(sender->serialnumber)};
		radio.senderHuman = true;
		radio.team = sender->v.team == 1 ? objectives::TeamRole::Terrorist :
			sender->v.team == 2 ? objectives::TeamRole::CounterTerrorist : objectives::TeamRole::Unknown;
		radio.receivedTime = globals_->time;
		// A radio receipt reveals the order and sender, never the sender's coordinates.
		for (std::size_t i = 0U; i < managedBotHandles_.size(); ++i)
		{
			if (managedBotSlots_[i] && managedBotTeamNumbers_[i] == sender->v.team &&
				(!publicMessageActor_.isValid() || publicMessageActor_.slot == i + 1U))
			{
				if (scenarioRadio_[i].sender == radio.sender &&
					scenarioRadio_[i].command == radio.command && scenarioRadio_[i].receivedTime == radio.receivedTime)
				{
					continue;
				}
				scenarioRadio_[i] = radio;
			}
		}
		return;
	}
	if (!publicMessageActor_.isValid())
		return;
	if (std::strcmp(name, "CurWeapon") == 0 && count == 3U && integers(0U))
		(void)publicObservations_.currentWeapon(publicMessageActor_, publicMessageFrame_, v[0], v[1],
			v[2] == 255 ? -1 : v[2]);
	else if (std::strcmp(name, "AmmoX") == 0 && count == 2U && integers(0U))
		(void)publicObservations_.ammo(publicMessageActor_, publicMessageFrame_, v[0], v[1]);
	else if (std::strcmp(name, "AmmoPickup") == 0 && count == 2U && integers(0U))
		(void)publicObservations_.ammoPickup(publicMessageActor_, publicMessageFrame_, v[0], v[1]);
	else if (std::strcmp(name, "WeapPickup") == 0 && count == 1U && integers(0U))
		(void)publicObservations_.weaponPickup(publicMessageActor_, publicMessageFrame_, v[0]);
	else if (std::strcmp(name, "Money") == 0 && (count == 1U || count == 2U) && integers(0U))
		(void)publicObservations_.money(publicMessageActor_, publicMessageFrame_, v[0]);
	else if (std::strcmp(name, "ArmorType") == 0 && count == 1U && integers(0U))
		(void)publicObservations_.armorType(publicMessageActor_, publicMessageFrame_, v[0]);
	else if (std::strcmp(name, "StatusIcon") == 0 && (count == 2U || count == 5U) && integers(2U))
		(void)publicObservations_.statusIcon(publicMessageActor_, publicMessageFrame_,
			publicMessageString_.data(), v[0]);
	else if (std::strcmp(name, "BarTime") == 0 && count == 1U && integers(0U))
		(void)publicObservations_.progress(publicMessageActor_, publicMessageFrame_, v[0],
			globals_!=nullptr ? globals_->time : 0.0f);
	else if (std::strcmp(name, "ScreenFade") == 0 && count == 7U && integers(0U) &&
		globals_ != nullptr && v[3] == 255 && v[4] == 255 && v[5] == 255 && v[6] > 180)
	{
		// HUD fade duration is fixed point (1 << 12); private blindness remains unavailable.
		const float duration = static_cast<float>((std::max)(0, v[0]) + (std::max)(0, v[1])) / 4096.0f;
		(void)publicObservations_.blind(publicMessageActor_, publicMessageFrame_, globals_->time + duration);
	}
	else if (std::strcmp(name, "ResetHUD") == 0)
	{
		deathDangerRecorded_[publicMessageActor_.slot - 1U] = false;
		publicObservations_.resetSlot(publicMessageActor_.slot);
		publicObservations_.bind(publicMessageActor_, publicMessageFrame_);
	}
}

void PluginRuntime::captureManagedCurrentWeapon()
{
	if (!userMessageWeaponMessageValid_ || userMessageFieldCount_ < 2U ||
		userMessageTarget_ == nullptr || engineFunctions_ == nullptr ||
		engineFunctions_->pfnIndexOfEdict == nullptr)
	{
		return;
	}
	const int slot = engineFunctions_->pfnIndexOfEdict(userMessageTarget_);
	if (slot < static_cast<int>(runtime::LifecycleSession::kFirstClientSlot) ||
		slot > static_cast<int>(runtime::LifecycleSession::kClientSlotCount))
	{
		return;
	}
	const std::size_t index = static_cast<std::size_t>(slot - 1);
	if (!managedBotSlots_[index] || managedBotHandles_[index].entity != userMessageTarget_)
	{
		return;
	}
	const FakeClientHandle &handle = managedBotHandles_[index];
	const auto &current=managedBotCurrentWeapons_[index];
	// CurWeapon also reports inactive inventory entries, sometimes after the
	// active entry. Only deactivation of the current weapon invalidates it.
	const bool inactiveOther=userMessageWeaponState_==0U && userMessageWeaponId_>0U &&
		userMessageWeaponId_<=30U && current.available && current.active &&
		current.actorGeneration==handle.actor.actorGeneration &&
		current.mapGeneration==lifecycle_.mapGeneration() && current.roundGeneration==lifecycle_.roundGeneration() &&
		current.weaponId!=userMessageWeaponId_;
	if(!inactiveOther)managedBotCurrentWeapons_[index] = {
		true,
		userMessageWeaponState_ != 0U,
		userMessageWeaponId_,
		handle.actor.actorGeneration,
		lifecycle_.mapGeneration(),
		lifecycle_.roundGeneration(),
		adapterFrameCount_};
	if (runtimeProfiler_.enabled() && gpMetaUtilFuncs != nullptr &&
		gpMetaUtilFuncs->pfnLogConsole != nullptr && pluginId_ != nullptr)
	{
		profiledLogConsole(runtimeProfiler_,
			pluginId_,
			"profile currentWeapon actor=%u actor_generation=%u map=%u round=%u "
			"frame=%u state=%u weapon_id=%u is_c4=%d",
			static_cast<unsigned int>(handle.actor.slot),
			static_cast<unsigned int>(handle.actor.actorGeneration),
			static_cast<unsigned int>(lifecycle_.mapGeneration()),
			static_cast<unsigned int>(lifecycle_.roundGeneration()),
			static_cast<unsigned int>(adapterFrameCount_),
			static_cast<unsigned int>(userMessageWeaponState_),
			static_cast<unsigned int>(userMessageWeaponId_),
			userMessageWeaponState_ != 0U &&
				userMessageWeaponId_ == ActionAdapter::kC4WeaponId ? 1 : 0);
	}
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
		profiledLogConsole(runtimeProfiler_,
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
	const bool jumpCommand = index < managedBotCommandTemplates_.size() &&
		managedBotCommandTemplateValid_[index] &&
		(managedBotCommandTemplates_[index].movement.buttons &
			static_cast<std::uint16_t>(IN_JUMP)) != 0U;
	if (jumpCommand && runtimeProfiler_.enabled() && gpMetaUtilFuncs != nullptr &&
			gpMetaUtilFuncs->pfnLogConsole != nullptr && pluginId_ != nullptr)
	{
		profiledLogConsole(runtimeProfiler_,
			pluginId_,
			"profile jumpFeedback bot_id=%u command_sequence=%u dispatched=%d "
			"before_origin=(%.1f %.1f %.1f) after_origin=(%.1f %.1f %.1f) "
			"after_velocity=(%.1f %.1f %.1f) after_grounded=%d",
			static_cast<unsigned int>(sample.actor.slot),
			static_cast<unsigned int>(sample.commandSequence), sample.dispatched ? 1 : 0,
			sample.before.origin.x, sample.before.origin.y, sample.before.origin.z,
			sample.after.origin.x, sample.after.origin.y, sample.after.origin.z,
			sample.after.velocity.x, sample.after.velocity.y, sample.after.velocity.z,
			sample.after.grounded ? 1 : 0);
	}
}

void PluginRuntime::updateManagedBotMovement()
{
	const compat::CvarSnapshot configuration = compatibilitySurface_.configuration();
	const nav::NavSnapshot navigation = navPublisher_.snapshot();
	const bool movementUnavailable = configuration.botEnable <= 0.0f ||
		configuration.botStop > 0.0f || !navigation.isValid();
	if (movementUnavailable)
	{
		movementGroundLookaheadRoutes_.fill(
			runtime::GroundLookaheadRoute{});
	}
	if (movementDiagnosticRound_ != lifecycle_.roundGeneration())
			{
				movementDiagnosticRound_ = lifecycle_.roundGeneration();
				movementDiagnosticSamples_.fill(0U);
				movementDiagnosticAttempts_.fill(false);
				movementReadyLogged_.fill(false);
				movementGateDiagnosticTimes_.fill(std::numeric_limits<float>::quiet_NaN());
		movementGoalDiagnosticSeconds_.fill(
					(std::numeric_limits<std::uint32_t>::max)());
		movementTraversalDiagnosticSeconds_.fill(
			(std::numeric_limits<std::uint32_t>::max)());
		movementTraversalPending_.fill({});
		movementReversalDiagnostics_.fill({});
		movementGroundLookaheadRoutes_.fill(
			runtime::GroundLookaheadRoute{});
	}
	if (movementUnavailable)
	{
				if (!movementDiagnosticGlobal_ && gpMetaUtilFuncs != nullptr &&
						gpMetaUtilFuncs->pfnLogConsole != nullptr && pluginId_ != nullptr)
				{
					profiledLogConsole(runtimeProfiler_,
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
		if (movementUnavailable)
		{
			managedBotJumpCrouch_[index].reset();
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
		managedBotMovement_[index].setRuntimeMode(configuration.mode);
		if(!before.dead && !roundFreezeActive() && (handle.entity->v.flags&FL_FROZEN)==0)
		{
			nav::LocomotionObservation motion{};
			motion.position={before.origin.x,before.origin.y,before.origin.z};
			motion.grounded=before.grounded;motion.onLadder=before.onLadder;motion.ducked=before.ducked;
			motion.timingAvailable=true;motion.observedAt=globals_->time;
			managedBotMovement_[index].observeMotion(handle.actor,
				{lifecycle_.mapGeneration(),lifecycle_.roundGeneration(),adapterFrameCount_},motion);
		}
		else managedBotMovement_[index].resetProgressMonitor();
		updateManagedBotCompatibilityState(index, before);
		updateOutgoingPublicRadio(index);
		if (managedBotStateMachines_[index].behaviorDecision().action == compat::StateAction::Buy)
		{
			updatePublicBuying(index);
		}
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
		managedBombApproaches_[index] = {};
		managedBotMovement_[index].reset();
		movementFallLandingTrackers_[index].reset();
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
		const bool movementEligible =
			runtime::movementReadiness(before) == runtime::SpawnReadiness::Ready;
		const bool initialSpawnPending = movementWarmupFrames_[index] < 2U &&
			runtime::spawnReadiness(before) == runtime::SpawnReadiness::NotReady;
		if (movementUnavailable || !movementEligible || initialSpawnPending)
		{
			prepareNeutralManagedBotCommand(index, handle, before);
			const runtime::CommandReceipt receipt = executeManagedBotCommand(index, handle);
		recordMovementPhysicsSample(index, before, receipt);
		managedBotMovement_[index].reset();
		movementFallLandingTrackers_[index].reset();
		if (!movementEligible)
		{
			movementWarmupFrames_[index] = 0U;
		}
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
		managedBombApproaches_[index] = {};
		managedBotMovement_[index].reset();
		movementFallLandingTrackers_[index].reset();
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
	observation.locomotion.timingAvailable = std::isfinite(globals_->time);
	observation.locomotion.observedAt = globals_->time;
	observation.locomotion.viewYawAvailable = std::isfinite(handle.entity->v.v_angle.y);
	observation.locomotion.viewYaw = handle.entity->v.v_angle.y;
	if (compatibilitySurface_.configuration().mode == compat::RuntimeMode::Compatibility)
	{
		managedBotMovement_[index].setPathCostContext(managedPathCostContext(index));
	}
	if (engineFunctions_ != nullptr &&
		engineFunctions_->pfnCVarGetFloat != nullptr)
	{
		const float serverGravity =
			engineFunctions_->pfnCVarGetFloat("sv_gravity");
		const float entityGravity = handle.entity->v.gravity;
		const float downwardSpeed = (std::max)(0.0f, -before.velocity.z);
		const float safeSpeedSquared =
			nav::LocomotionConfig::kMaximumSafeFallSpeed *
			nav::LocomotionConfig::kMaximumSafeFallSpeed -
			downwardSpeed * downwardSpeed;
		if (std::isfinite(serverGravity) && serverGravity > 0.0f &&
			std::isfinite(entityGravity) && entityGravity > 0.0f &&
			std::isfinite(safeSpeedSquared) && safeSpeedSquared > 0.0f)
		{
			observation.locomotion.maximumSafeDropHeight =
				safeSpeedSquared / (2.0f * serverGravity * entityGravity);
			observation.locomotion.safeDropHeightAvailable = true;
		}
	}
		observation.airborne = !before.grounded;
		const runtime::LandingDamageObservation landingFeedback =
			movementFallLandingTrackers_[index].observe(
				handle.actor.actorGeneration, before);
		observation.landingConfirmed = landingFeedback.landingConfirmed;
		observation.hasLandingDamage = landingFeedback.hasLandingDamage;
		observation.landingDamage = landingFeedback.landingDamage;
		if (runtimeProfiler_.enabled() && landingFeedback.landingConfirmed &&
			gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr &&
			pluginId_ != nullptr)
		{
			profiledLogConsole(runtimeProfiler_,
				pluginId_,
				"profile landingFeedback bot_id=%u actor_generation=%u map=%u round=%u frame=%u "
				"landing_damage_available=%d landing_damage=%.1f",
				static_cast<unsigned int>(handle.actor.slot),
				static_cast<unsigned int>(handle.actor.actorGeneration),
				lifecycle_.mapGeneration(), lifecycle_.roundGeneration(),
				static_cast<unsigned int>(adapterFrameCount_),
				landingFeedback.hasLandingDamage ? 1 : 0, landingFeedback.landingDamage);
		}
		observation.ladderContact = before.onLadder;
		observation.entryConfirmed = true;
		observation.exitConfirmed = before.grounded;
		observation.collectPathStats = runtimeProfiler_.enabled();
		nav::NavSearchStats objectiveSearchStats = {};
		RuntimeObjectiveSelectionStats objectiveSelectionStats = {};
		{
			RuntimeProfilerScope objectiveScope(
				runtimeProfiler_, RuntimeProfilerStage::FullUpdateObjective);
			RuntimeProfilerScope candidateNavScope(
				runtimeProfiler_,
				RuntimeProfilerStage::ObjectiveCandidateNavEvaluate);
			observation.hasObjectiveTarget = buildManagedObjectiveTarget(
				index,
				&observation.objectiveTarget,
				runtimeProfiler_.enabled() ? &objectiveSearchStats : nullptr,
				runtimeProfiler_.enabled() ? &objectiveSelectionStats : nullptr);
		}
		const world::ActorKey objectiveActor = {handle.actor.slot, handle.actor.actorGeneration};
		const team::TeamObjectiveAssignment *objectiveAssignment =
			findManagedTeamObjectiveAssignment(objectiveActor);
		if (observation.hasObjectiveTarget && objectiveAssignment != nullptr)
		{
			observation.objectiveTargetEntity = objectiveAssignment->targetEntity;
		}
		observation.team = static_cast<std::uint8_t>(
			managedBotObjectiveTargets_[index].effectiveTeam);
		observation.movementSuppressed = roundFreezeActive();
		const auto &objectiveAction = managedBotObjectiveActions_[index];
		observation.movementSuppressed = observation.movementSuppressed ||
			(objectiveAction.active && objectiveAction.kind == team::TeamObjectiveKind::PlantC4 &&
			 objectiveAction.actor.slot == handle.actor.slot &&
			 objectiveAction.actor.generation == handle.actor.actorGeneration &&
			 objectiveAction.mapGeneration == lifecycle_.mapGeneration() &&
			 objectiveAction.roundGeneration == lifecycle_.roundGeneration() &&
			 objectiveAction.plantAttempt.active);
		// Navigation-only teammate occupancy follows CSBot's live player scan.
		// Enemy perception and objective knowledge retain their existing gates.
		observation.friendRayOriginAvailable = true;
		observation.friendRayOrigin = {before.origin.x,before.origin.y,before.origin.z};
		observation.nearbyFriends = liveFriendlyPositions(index, false);
		runtime::GroundLookaheadRoute &groundLookaheadRoute =
			movementGroundLookaheadRoutes_[index];
		const runtime::GroundLookaheadRoute probeRoute = groundLookaheadRoute;
		if (observation.movementSuppressed || !before.grounded ||
				before.onLadder ||
				!groundLookaheadRoute.matchesNextUpdate(
					handle.actor, observation.frame,
					managedBotFullUpdateSequences_[index], globals_->time) ||
				engineFunctions_ == nullptr ||
				engineFunctions_->pfnTraceLine == nullptr)
		{
			groundLookaheadRoute.reset();
		}
		else
		{
			const runtime::GroundLookaheadRoute previousRoute =
				groundLookaheadRoute;
			nav::GroundLookaheadObservation &lookahead =
				observation.locomotion.groundLookahead;
			lookahead.targetArea = previousRoute.targetArea;
			lookahead.direction = previousRoute.direction;
			lookahead.running = previousRoute.running;

			const auto traceGroundAt = [&](float distance)
				-> nav::GroundProbeObservation
			{
				nav::GroundProbeObservation probe = {};
				const float x = observation.locomotion.position.x +
					previousRoute.direction.x * distance;
				const float y = observation.locomotion.position.y +
					previousRoute.direction.y * distance;
				const float start[3] = {
					x,
					y,
					handle.entity->v.origin.z + kStandingHalfHumanHeight};
				const float end[3] = {
					x,
					y,
					observation.locomotion.position.z - kGroundProbeDepth};
				if (!std::isfinite(x) || !std::isfinite(y) ||
						!std::isfinite(start[2]) || !std::isfinite(end[2]))
				{
					return probe;
				}

				TraceResult trace = {};
				RuntimeProfilerScope traceScope(
					runtimeProfiler_, RuntimeProfilerStage::TraceLine);
				if (runtimeProfiler_.enabled())
				{
					runtimeProfiler_.recordTraceLine(1U, 0U, 0U);
				}
			engineFunctions_->pfnTraceLine(
					start, end, 1, handle.entity, &trace);
				probe.sampled = true;
				const float floorZ = trace.vecEndPos[2];
				const float normalZ = trace.vecPlaneNormal[2];
				if (!trace.fStartSolid && !trace.fAllSolid &&
						std::isfinite(trace.flFraction) &&
						trace.flFraction >= 0.0f && trace.flFraction < 1.0f &&
						std::isfinite(floorZ) && std::isfinite(normalZ) &&
						normalZ >= -1.0f && normalZ <= 1.0f && normalZ > 0.0f)
				{
					probe.hasGround = true;
					probe.floorZ = floorZ;
					probe.normalZ = normalZ;
				}
				return probe;
			};

			if (previousRoute.running)
			{
				lookahead.far80 = traceGroundAt(80.0f);
			}
			lookahead.near30 = traceGroundAt(30.0f);
			lookahead.gap10 = traceGroundAt(10.0f);
			// Near-ground fallback remains usable when the optional far probe misses
			// or finds a slope (CSBot b0889847 cs_bot_nav.cpp:229-238).
			lookahead.valid = lookahead.near30.sampled &&
				lookahead.gap10.sampled;
		}
		runtimeProfiler_.recordObjectiveSelection(
			objectiveSelectionStats.bombSites,
			objectiveSelectionStats.funcBombTargetSites,
			objectiveSelectionStats.infoBombTargetSites,
			objectiveSelectionStats.candidateAreas,
			objectiveSelectionStats.uniqueCandidateAreas,
			objectiveSelectionStats.candidateQueries,
			objectiveSelectionStats.duplicateCandidateAreas,
			objectiveSelectionStats.selectedGoalArea,
			objectiveSelectionStats.registeredSites,
			objectiveSelectionStats.registeredFuncBombTargetSites,
			objectiveSelectionStats.registeredInfoBombTargetSites,
			objectiveSelectionStats.registeredSiteCountsAvailable,
			objectiveSelectionStats.evaluatedSites,
			objectiveSelectionStats.cacheHits,
			objectiveSelectionStats.selectedSiteIdentity);
		runtimeProfiler_.recordPathSearchResults(
			RuntimePathSearchCaller::ObjectiveCandidateEvaluation,
			managedBotHandles_[index].actor.slot,
			managedBotFullUpdateSequences_[index],
			managedBotObjectiveTargets_[index].generation,
			objectiveSearchStats.firstStartArea,
			objectiveSearchStats.firstGoalArea,
			objectiveSearchStats.firstRouteType,
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
		groundLookaheadRoute.reset();
		const auto &previousStep=managedNavigationSteps_[index];
		const bool destroyedPath=previousStep.pathDestroyedAfterMovement &&
			previousStep.actor==objectiveActor && previousStep.frame.mapGeneration==observation.frame.mapGeneration &&
			previousStep.frame.roundGeneration==observation.frame.roundGeneration && previousStep.frame.tick<observation.frame.tick &&
			previousStep.goalGeneration==managedBotObjectiveTargets_[index].generation &&
			(managedBotStateMachines_[index].state()==compat::CompatibilityStateId::MoveTo ||
			 managedBotStateMachines_[index].state()==compat::CompatibilityStateId::EscapeFromBomb) &&
			!managedBotStateMachines_[index].attackOverlayActive();
		const auto navigationState=managedBotStateMachines_[index].state();
		const bool stateHold=compatibilitySurface_.configuration().mode==compat::RuntimeMode::Compatibility &&
			(navigationState==compat::CompatibilityStateId::DefuseBomb || navigationState==compat::CompatibilityStateId::PlantBomb ||
			 navigationState==compat::CompatibilityStateId::Buy ||
			 (navigationState==compat::CompatibilityStateId::Hide && scenarioDecisions_[index].action==compat::ScenarioAction::Guard &&
			  scenarioDecisions_[index].hasTarget && scenarioDecisions_[index].target.inUseRange));
		if(stateHold)
		{
			roamResult=runtime::NavRoamResult::MovementSuppressed;
			roamDecision.stage=runtime::NavRoamStage::MovementSuppressed;
		}
		else if (performanceDisablePathSearch_)
		{
			roamDecision.failureReason = runtime::NavFailureReason::PathSearchFailed;
			roamDecision.stage = runtime::NavRoamStage::Failed;
		}
		else if(destroyedPath)
		{
			// The preceding update moved before DestroyPath. This update must
			// report the missing path to MoveTo instead of silently rebuilding it.
			roamDecision.failureReason=runtime::NavFailureReason::MovementNotProduced;
			roamDecision.stage=runtime::NavRoamStage::Failed;
		}
		else
		{
			managedBotMovement_[index].setRandomSource(&compatibilityRandomSource_);
			managedBotMovement_[index].setAreaVisitHistory(
				&managedBotAreaVisitHistory_);
			managedBotRoamReservationBoard_.reset(
				lifecycle_.mapGeneration(), lifecycle_.roundGeneration());
			managedBotMovement_[index].setReservationBoard(
				&managedBotRoamReservationBoard_);
			{
				RuntimeProfilerScope navMovementScope(
					runtimeProfiler_, RuntimeProfilerStage::NavMovement);
				roamResult = managedBotMovement_[index].update(
					navigation, observation, &locomotionIntent, &roamDecision);
			}
			const float routeDirectionLength = std::hypot(
				locomotionIntent.direction.x,
				locomotionIntent.direction.y);
			if (roamResult == runtime::NavRoamResult::IntentReady &&
					!observation.movementSuppressed && before.grounded &&
					!before.onLadder && locomotionIntent.targetArea != 0U &&
					std::isfinite(locomotionIntent.direction.x) &&
					std::isfinite(locomotionIntent.direction.y) &&
					std::isfinite(locomotionIntent.direction.z) &&
					std::isfinite(routeDirectionLength) &&
					routeDirectionLength >= 0.9f &&
					routeDirectionLength <= 1.1f &&
					std::isfinite(locomotionIntent.speed) &&
					locomotionIntent.speed > 0.0f)
			{
				groundLookaheadRoute.actor = handle.actor;
				groundLookaheadRoute.frame = observation.frame;
				groundLookaheadRoute.fullUpdateSequence = managedBotFullUpdateSequences_[index];
				groundLookaheadRoute.createdAt = globals_->time;
				groundLookaheadRoute.targetArea = locomotionIntent.targetArea;
				groundLookaheadRoute.direction = locomotionIntent.direction;
				groundLookaheadRoute.running =
					locomotionIntent.posture == nav::LocomotionPosture::Standing &&
					locomotionIntent.speed >= 200.0f;
				groundLookaheadRoute.active = true;
			}
			const bool objectivePathFailed =
				(roamResult == runtime::NavRoamResult::NoRoute ||
					roamResult == runtime::NavRoamResult::ReplanRequired) &&
				(roamDecision.failureReason ==
					runtime::NavFailureReason::PathSearchFailed ||
					roamDecision.failureReason == runtime::NavFailureReason::UnsafeDrop ||
					roamDecision.failureReason == runtime::NavFailureReason::NavApplyRejected ||
					roamDecision.failureReason == runtime::NavFailureReason::MovementNotProduced ||
					(roamDecision.failureReason == runtime::NavFailureReason::RecoveryNoProgress &&
						roamDecision.recoveryCandidateFailed));
			if (objectivePathFailed &&
				roamDecision.goalKind == runtime::NavGoalKind::Objective)
			{
				const world::ActorKey actor = {
					handle.actor.slot, handle.actor.actorGeneration};
				const team::TeamObjectiveAssignment *assignment =
					findManagedTeamObjectiveAssignment(actor);
				if (assignment != nullptr)
				{
					managedTeamObjectivePathFailurePending_ = true;
					managedTeamObjectivePathFailureActor_ = actor;
					managedTeamObjectivePathFailureGeneration_ =
						assignment->generation;
					// A portal-area switch alone does not clear a physical drop veto.
					// Zero keeps the coordinator's bounded retry lease across that jitter.
					managedTeamObjectivePathFailureStartArea_ =
						roamDecision.failureReason == runtime::NavFailureReason::UnsafeDrop
							? 0U
							: (roamDecision.currentArea != 0U
								? roamDecision.currentArea
								: roamDecision.recoveryArea);
					managedTeamObjectivePathFailureFrame_ =
						adapterFrameCount_;
				}
			}
		}
		finishManagedBotNavigation(index,before,roamResult,roamDecision,&locomotionIntent);
			runtimeProfiler_.recordPathSearchResults(
				 navSearchCaller(roamDecision),
			managedBotHandles_[index].actor.slot,
			managedBotFullUpdateSequences_[index],
			managedBotObjectiveTargets_[index].generation,
			roamDecision.pathSearchStats.firstStartArea,
			roamDecision.pathSearchStats.firstGoalArea,
			roamDecision.pathSearchStats.firstRouteType,
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
		logGoalAssignmentDiagnostic(
			index, before, roamDecision, locomotionIntent, roamResult,
			observation.locomotion.groundLookahead, probeRoute);
		if (roamDecision.recomputeReason != runtime::NavRecomputeReason::None)
		{
			runtimeProfiler_.recordPathRecompute();
		}
		if (roamResult != runtime::NavRoamResult::IntentReady)
		{
			if (roamDecision.failureReason == runtime::NavFailureReason::None &&
				roamResult!=runtime::NavRoamResult::MovementSuppressed && roamResult!=runtime::NavRoamResult::TargetReached &&
				roamResult!=runtime::NavRoamResult::ReplanRequired)
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
			hasObservedActiveWeapon(index),
			movementViewAngles);
		const ActionDispatch actionDispatch = ActionAdapter::translate(actionProposal);
		command.viewAngles = actionDispatch.viewAngles;
		command.movement.buttons = actionDispatch.buttons;
		const bool compatibilityMovement=compatibilitySurface_.configuration().mode==compat::RuntimeMode::Compatibility;
		float commandSpeed=locomotionIntent.speed;
		if(compatibilityMovement && commandSpeed>0.0f && std::isfinite(handle.entity->v.maxspeed) && handle.entity->v.maxspeed>0.0f)
		{
			commandSpeed=handle.entity->v.maxspeed;
			const auto &goal=managedBotObjectiveTargets_[index];
			const float x=goal.target.x-before.origin.x,y=goal.target.y-before.origin.y;
			const bool finalArea=roamDecision.currentArea!=0U && roamDecision.currentArea==roamDecision.goalArea;
			if(goal.valid && finalArea && x*x+y*y<200.0f*200.0f &&
				locomotionIntent.posture!=nav::LocomotionPosture::Crouching)commandSpeed*=0.4f;
		}
		managedMoveSpeeds_[index]=commandSpeed;
		const MovementProjection movementProjection = ActionAdapter::projectMovement(
			locomotionIntent.direction.x,
			locomotionIntent.direction.y,
			commandSpeed,
			command.viewAngles.yaw,compatibilityMovement);
		command.movement.forward = actionDispatch.stopMovement ? 0.0f : movementProjection.forward;
		command.movement.side = actionDispatch.stopMovement ? 0.0f : movementProjection.side;
		if (!actionDispatch.stopMovement)
		{
			command.movement.buttons = static_cast<std::uint16_t>(
				command.movement.buttons |
					ActionAdapter::movementButtons(
						command.movement.forward, command.movement.side));
			}
	logTraversalOutcome(index, before, roamDecision, landingFeedback);
	logTraversalDiagnostic(
		index, before, roamDecision, locomotionIntent, command.movement.buttons,
		observation.locomotion);
	logMovementReversalDiagnostic(index, before, roamDecision, locomotionIntent);
			if (movementDiagnosticSamples_[index] < kMovementPhysicsLogLimit &&
			gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr &&
			pluginId_ != nullptr)
		{
			profiledLogConsole(runtimeProfiler_,
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
			profiledLogConsole(runtimeProfiler_,
				pluginId_, "bot action=%s slot=%u generation=%u frame=%u buttons=%u",
				actionKindName(actionProposal.kind), static_cast<unsigned int>(handle.actor.slot),
				static_cast<unsigned int>(handle.actor.actorGeneration),
				static_cast<unsigned int>(adapterFrameCount_),
				static_cast<unsigned int>(actionDispatch.buttons));
		}
		if (actionDispatch.clientCommand != nullptr)
		{
			(void)dispatchManagedActionCommand(
				index, handle, actionProposal, actionDispatch);
		}
		handle.entity->v.v_angle[0] = command.viewAngles.pitch;
				handle.entity->v.v_angle[1] = command.viewAngles.yaw;
				handle.entity->v.v_angle[2] = command.viewAngles.roll;
				handle.entity->v.angles[0] = -command.viewAngles.pitch / 3.0f;
		handle.entity->v.angles[1] = command.viewAngles.yaw;
		handle.entity->v.angles[2] = command.viewAngles.roll;
		managedBotCommandTemplates_[index] = command;
		managedBotCommandTemplateValid_[index] = true;
		managedBotActionStopsMovement_[index] = actionDispatch.stopMovement;
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
						profiledLogConsole(runtimeProfiler_,
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

void PluginRuntime::finishManagedBotNavigation(std::size_t index,
	const runtime::MovementPhysicsState &before,runtime::NavRoamResult result,
	const runtime::NavRoamDecision &decision,nav::LocomotionIntent *intent)
{
	if(compatibilitySurface_.configuration().mode!=compat::RuntimeMode::Compatibility ||
		index>=managedBotHandles_.size() || globals_==nullptr)return;
	compat::StateBehaviorInput input{};
	auto context=makeManagedBotStateContext(index,before,&input);
	const auto &prepared=managedStateInputs_[index];
	if(!(prepared.navigation.actor==context.actor) || prepared.navigation.frame.tick!=context.frame.tick ||
		prepared.navigation.frame.mapGeneration!=context.frame.mapGeneration ||
		prepared.navigation.frame.roundGeneration!=context.frame.roundGeneration)return;
	input=prepared;
	compat::NavigationStep step{};
	step.actor=context.actor;step.frame=context.frame;
	step.goalGeneration=managedBotObjectiveTargets_[index].generation;
	step.pathSequence=decision.pathSequence;step.fullUpdateSequence=context.fullUpdateSequence;
	using Outcome=compat::NavigationStepOutcome;
	if(result==runtime::NavRoamResult::IntentReady || result==runtime::NavRoamResult::MovementSuppressed ||
		(result==runtime::NavRoamResult::ReplanRequired && decision.failureReason==runtime::NavFailureReason::None))
		step.outcome=Outcome::Progressing;
	else if(result==runtime::NavRoamResult::TargetReached)step.outcome=Outcome::EndOfPath;
	else if(result==runtime::NavRoamResult::NoRoute || result==runtime::NavRoamResult::ReplanRequired)
		step.outcome=Outcome::PathFailure;
	else step.outcome=Outcome::ObservationUnavailable;
	const auto currentState=managedBotStateMachines_[index].state();
	if(currentState==compat::CompatibilityStateId::DefuseBomb || currentState==compat::CompatibilityStateId::PlantBomb ||
		currentState==compat::CompatibilityStateId::Buy)step.outcome=Outcome::NotEvaluated;
	step.pathDestroyedAfterMovement=step.outcome==Outcome::Progressing &&
		decision.recomputeReason==runtime::NavRecomputeReason::PathInvalidated;
	input.navigation=step;
	if(currentState==compat::CompatibilityStateId::EscapeFromBomb &&
		(step.outcome==Outcome::EndOfPath || step.outcome==Outcome::PathFailure) && scenarioEscapeAreas_[index]!=0U)
	{
		if(runtimeProfiler_.enabled())profiledLogConsole(runtimeProfiler_,pluginId_,
			"profile bombEscapeRetry bot_id=%u frame=%u previous_area=%u result=%d reason=%d",
			context.actor.slot,context.frame.tick,scenarioEscapeAreas_[index],static_cast<int>(result),static_cast<int>(decision.recomputeReason));
		scenarioEscapeAreas_[index]=0U;
	}
	const auto &target=scenarioDecisions_[index].target;
	const auto &bomb=objectiveKnowledge_[index][1U];
	input.defuseGoalKnown=publicPlayerTeam(managedBotHandles_[index].entity)==2 &&
		scenarioDecisions_[index].hasTarget && bomb.entity.id!=0U && target.entity.id==bomb.entity.id;
	if(input.defuseGoalKnown)
	{
		const auto *entity=managedBotHandles_[index].entity;
		const float footOffset=entity->v.mins.z<0.0f ? entity->v.mins.z : -kStandingHalfHumanHeight;
		const float x=bomb.position.x-before.origin.x,y=bomb.position.y-before.origin.y,
			z=bomb.position.z-before.origin.z-footOffset;
		input.withinDefuseRange=x*x+y*y+z*z<100.0f*100.0f;
	}
	compat::CompatibilityObservation observation{};
	const compat::ObservationTimingContext timing={managedBotCommandSequences_[index],0U,context.fullUpdateSequence};
	if(observationAdapter_.collectActor(managedBotHandles_[index].entity,context.actor,context.frame,timing,&observation)==ObservationAdapterResult::Accepted &&
		observation.weapon.reloadState.isAvailable() && observation.weapon.reloadState.value!=combat::ReloadState::Unknown)
	{
		input.reloadKnown=true;
		input.reloading=observation.weapon.reloadState.value==combat::ReloadState::Reloading;
	}
	else
	{
		// Vanilla cannot expose the internal reload flag. Own delivered reload
		// requests conservatively block use; no pending request is an inference.
		input.reloadInferred=true;
		input.reloading=publicObservations_.pendingReloadRequest(context.actor,context.frame,context.timestamp);
	}
	auto &machine=managedBotStateMachines_[index];
	const auto previous=machine.state();
	(void)machine.finishNavigation(context);
	if(!machine.attackOverlayActive())managedNavigationSteps_[index]=step;
	if(machine.state()!=previous)
	{
		managedBotMovement_[index].destroyPath();
		if(machine.state()==compat::CompatibilityStateId::Idle)
		{
			managedBotObjectiveTargets_[index]={};scenarioDecisions_[index]={};
			if(runtimeProfiler_.enabled())profiledLogConsole(runtimeProfiler_,pluginId_,
				"profile navigationFailure bot_id=%u frame=%u state_from=%s state_to=%s",
				context.actor.slot,context.frame.tick,compat::toString(previous),compat::toString(machine.state()));
		}
	}
	if(machine.state()==compat::CompatibilityStateId::DefuseBomb && intent!=nullptr)*intent={};
	if(runtimeProfiler_.enabled())
		profiledLogConsole(runtimeProfiler_,pluginId_,
			"profile navigationStep bot_id=%u frame=%u update=%u goal_generation=%u path=%u outcome=%u destroyed=%d state_from=%s state_to=%s reload_known=%d reload_inferred=%d goal_position=(%.1f %.1f %.1f) failure_reason=%d current_area=%u goal_area=%u path_link=(%u->%u)",
			context.actor.slot,context.frame.tick,context.fullUpdateSequence,step.goalGeneration,step.pathSequence,
			static_cast<unsigned>(step.outcome),step.pathDestroyedAfterMovement?1:0,
			compat::toString(previous),compat::toString(machine.state()),input.reloadKnown?1:0,input.reloadInferred?1:0,
			decision.goalPosition.x,decision.goalPosition.y,decision.goalPosition.z,
			static_cast<int>(decision.failureReason),decision.currentArea,decision.goalArea,
			decision.linkFromArea,decision.linkToArea);
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
	const auto selectVisibleTarget = [this, index, targetActor, targetPosition](
		const world::WorldSnapshot &belief,
		const world::ActorKey &observer) -> bool
	{
		world::ActorKey selected = {};
		world::WorldPosition selectedPosition = {};
		float selectedDistanceSquared = (std::numeric_limits<float>::max)();
		std::array<compat::ThreatCandidate, 32U> candidates{};
		std::size_t candidateCount = 0U;
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
			if (candidateCount < candidates.size())
			{
				world::WorldPosition visiblePoint = candidate->position;
				if ((candidate->visibleParts & world::VisibleHead) != 0U)
					visiblePoint.z += 25.0f;
				else if ((candidate->visibleParts & world::VisibleChest) == 0U &&
					(candidate->visibleParts & world::VisibleFeet) != 0U)
					visiblePoint.z -= 14.0f;
				bool attackingObserver = false;
				if (engineFunctions_ != nullptr && engineFunctions_->pfnPEntityOfEntIndex != nullptr &&
					engineFunctions_->pfnTraceLine != nullptr)
				{
					edict_t *enemy = engineFunctions_->pfnPEntityOfEntIndex(static_cast<int>(candidate->actor.slot));
					if (enemy != nullptr && !enemy->free && (enemy->v.button & IN_ATTACK) != 0)
					{
						const float yaw = enemy->v.v_angle.y * 3.14159265358979323846f / 180.0f;
						const float pitch = enemy->v.v_angle.x * 3.14159265358979323846f / 180.0f;
						const float eye[] = {enemy->v.origin.x + enemy->v.view_ofs.x,
							enemy->v.origin.y + enemy->v.view_ofs.y, enemy->v.origin.z + enemy->v.view_ofs.z};
						const float end[] = {eye[0] + 8192.0f * std::cos(pitch) * std::cos(yaw),
							eye[1] + 8192.0f * std::cos(pitch) * std::sin(yaw), eye[2] - 8192.0f * std::sin(pitch)};
						TraceResult trace{};
						engineFunctions_->pfnTraceLine(eye, end, 0, enemy, &trace);
						attackingObserver = trace.pHit == managedBotHandles_[index].entity;
					}
				}
				candidates[candidateCount++] = {candidate->actor, visiblePoint, std::sqrt(distanceSquared), attackingObserver};
			}
			if (distanceSquared < selectedDistanceSquared)
			{
				selectedDistanceSquared = distanceSquared;
				selected = candidate->actor;
				selectedPosition = candidate->position;
			}
		}
		if (recognitionSequences_[index] != managedBotFullUpdateSequences_[index])
		{
			recognitionSequences_[index] = managedBotFullUpdateSequences_[index];
			compat::ThreatCandidate threat{};
			compat::PublicActorState publicState{};
			const bool blinded = publicObservations_.read(observer, belief.identity().frame, &publicState) &&
				globals_ != nullptr && globals_->time < publicState.blindUntil;
			if (!blinded && candidateCount != 0U && globals_ != nullptr)
			{
				lastVisibleEnemyTimes_[index] = globals_->time;
			}
			if (globals_ != nullptr && reactionQueues_[index].update(observer, belief.identity().frame,
				globals_->time, managedProfiles_[index].reactionTimeSeconds, candidates.data(), candidateCount,
				blinded, &threat))
			{
				if (!(recognizedTargets_[index] == threat.actor))
				{
					attackReadyTimes_[index] = globals_->time + managedProfiles_[index].attackDelaySeconds;
				}
				recognizedTargets_[index] = threat.actor;
				recognizedPoints_[index] = threat.visiblePoint;
			}
			else
				recognizedTargets_[index] = {};
		}
		selected = recognizedTargets_[index];
		selectedPosition = recognizedPoints_[index];
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
	const std::uint64_t firstSound = (std::max)(std::uint64_t(deliveredSoundSequences_[index]) + 1U,
		soundSequence_ > soundEvents_.size() ? std::uint64_t(soundSequence_ - soundEvents_.size() + 1U) : std::uint64_t(1U));
	for (std::uint64_t sequence = firstSound; sequence <= soundSequence_; ++sequence)
	{
		const auto &observed = soundEvents_[sequence % soundEvents_.size()];
		if (observed.id != sequence || observed.source.slot == handle.actor.slot ||
			(soundTeams_[sequence % soundTeams_.size()] != 0 &&
			 soundTeams_[sequence % soundTeams_.size()] == managedBotTeamNumbers_[index]))
		{
			continue;
		}
		const float dx = observed.origin.x - handle.entity->v.origin.x;
		const float dy = observed.origin.y - handle.entity->v.origin.y;
		const float dz = observed.origin.z - handle.entity->v.origin.z;
		const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
		if (std::isfinite(distance) && distance < observed.distance)
		{
			world::AudibleEvent audible = observed;
			audible.distance = distance;
			(void)input.addSound(audible);
		}
	}
	deliveredSoundSequences_[index] = soundSequence_;
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

void PluginRuntime::refreshManagedObjectiveSiteRegistry()
{
	const runtime::LifecycleGeneration mapGeneration = lifecycle_.mapGeneration();
	const runtime::LifecycleGeneration roundGeneration = lifecycle_.roundGeneration();
	if (managedObjectiveSiteRegistry_.initialized &&
		managedObjectiveSiteRegistry_.mapGeneration == mapGeneration &&
		managedObjectiveSiteRegistry_.roundGeneration == roundGeneration)
	{
		return;
	}
	managedObjectiveSiteRegistry_ = {};
	managedObjectiveSiteRegistry_.initialized = true;
	managedObjectiveSiteRegistry_.mapGeneration = mapGeneration;
	managedObjectiveSiteRegistry_.roundGeneration = roundGeneration;
	if (engineFunctions_ == nullptr || globals_ == nullptr ||
		engineFunctions_->pfnPEntityOfEntIndex == nullptr ||
		engineFunctions_->pfnSzFromIndex == nullptr)
	{
		return;
	}
	const int maxEntities = (std::max)(0, (std::min)(globals_->maxEntities, 2048));
	for (int entityIndex = 1; entityIndex <= maxEntities; ++entityIndex)
	{
		if (managedObjectiveSiteRegistry_.registeredCount >=
			ManagedObjectiveSiteRegistry::kMaximumSites)
		{
			break;
		}
		edict_t *entity = engineFunctions_->pfnPEntityOfEntIndex(entityIndex);
		if (entity == nullptr || entity->free)
		{
			continue;
		}
		const char *classname = engineFunctions_->pfnSzFromIndex(entity->v.classname);
		if (!isBombTargetClassname(classname))
		{
			continue;
		}
		if (equalsIgnoreCase(classname, "func_bomb_target"))
		{
			++managedObjectiveSiteRegistry_.registeredFuncBombTargetCount;
		}
		else if (equalsIgnoreCase(classname, "info_bomb_target"))
		{
			++managedObjectiveSiteRegistry_.registeredInfoBombTargetCount;
		}
		ManagedObjectiveSiteRegistry::Entry &entry =
			managedObjectiveSiteRegistry_.entries[
				managedObjectiveSiteRegistry_.registeredCount];
		entry.registered = true;
		entry.targetId = managedObjectiveSiteRegistry_.registeredCount + 1U;
		entry.entityIndex = entityIndex;
		entry.siteIdentity = entry.targetId;
		entry.center = entityObjectiveCenter(entity);
		entry.hasExtent = objectiveSiteExtent(entity, classname, &entry.extent);
		++managedObjectiveSiteRegistry_.registeredCount;
	}
}

bool PluginRuntime::observeObjective(std::size_t index, edict_t *entity, bool planted,
	world::WorldVector *position, bool *currentlyVisible)
{
	if (currentlyVisible != nullptr) *currentlyVisible = false;
	if (index >= managedBotHandles_.size() || entity == nullptr || entity->free != 0 ||
		position == nullptr || engineFunctions_ == nullptr || engineFunctions_->pfnIndexOfEdict == nullptr)
		return false;
	const FakeClientHandle &handle = managedBotHandles_[index];
	if (handle.entity == nullptr)
		return false;
	const bool compatibility = compatibilitySurface_.configuration().mode == compat::RuntimeMode::Compatibility;
	ObjectiveKnowledge &knowledge = objectiveKnowledge_[index][planted ? 1U : 0U];
	const world::EntityKey key{static_cast<std::uint32_t>(engineFunctions_->pfnIndexOfEdict(entity)),
		static_cast<std::uint32_t>(entity->serialnumber)};
	const float eye[] = {handle.entity->v.origin.x + handle.entity->v.view_ofs.x,
		handle.entity->v.origin.y + handle.entity->v.view_ofs.y,
		handle.entity->v.origin.z + handle.entity->v.view_ofs.z};
	const float target[] = {entity->v.origin.x, entity->v.origin.y, entity->v.origin.z};
	const float dx = target[0] - eye[0];
	const float dy = target[1] - eye[1];
	const float length = std::hypot(dx, dy);
	const float angle = handle.entity->v.v_angle.y * 3.14159265358979323846f / 180.0f;
	// The reference gives T the planted location on the announced plant event,
	// not merely because the server scan found a grenade (CSGameState::OnEvent).
	bool visible = !compatibility || (planted && managedBotTeamNumbers_[index] == 1U &&
		bombNoticeKnown_[index] && bombNoticePlanted_[index]);
	if (!planted && compatibility && managedBotTeamNumbers_[index] == 1U)
	{
		world::WorldVector radar{};
		if (publicBombRadars_[index].read(
			{lifecycle_.mapGeneration(), lifecycle_.roundGeneration(), adapterFrameCount_}, &radar))
		{
			knowledge.entity = key;
			knowledge.position = radar;
			*position = radar;
			return true;
		}
	}
	compat::PublicActorState publicState{};
	const bool blind = globals_ != nullptr && publicObservations_.read(
		{handle.actor.slot, handle.actor.actorGeneration},
		{lifecycle_.mapGeneration(), lifecycle_.roundGeneration(), adapterFrameCount_}, &publicState) &&
		globals_->time < publicState.blindUntil;
	const bool smokeBlocked = globals_ != nullptr && smokeObservations_.blocks(
		{eye[0], eye[1], eye[2]}, {target[0], target[1], target[2]}, globals_->time);
	if (!visible && !blind && !smokeBlocked && engineFunctions_->pfnTraceLine != nullptr &&
		(length < 0.001f || (dx * std::cos(angle) + dy * std::sin(angle)) / length > 0.5f))
	{
		TraceResult trace{};
		engineFunctions_->pfnTraceLine(eye, target, 1, handle.entity, &trace);
		visible = trace.flFraction == 1.0f || trace.pHit == entity;
	}
	if (visible)
	{
		knowledge.entity = key;
		knowledge.position = {target[0], target[1], target[2]};
	}
	if (!visible && (!key.isValid() || knowledge.entity.id != key.id ||
		knowledge.entity.generation != key.generation))
		return false;
	*position = knowledge.position;
	if (currentlyVisible != nullptr) *currentlyVisible = visible;
	return true;
}

void PluginRuntime::updateBombGuardTarget(std::size_t index,
	const compat::ScenarioBehaviorInput &input, compat::ScenarioBehaviorDecision *decision)
{
	RuntimeProfilerScope guardScope(runtimeProfiler_, RuntimeProfilerStage::BombGuardSelection);
	if (index >= bombGuardPlanners_.size() || decision == nullptr) return;
	if (!input.bombPlantedKnown || !input.bombPlanted || !decision->hasTarget ||
		decision->action != compat::ScenarioAction::Guard ||
		!(decision->target.entity == input.bomb.entity))
	{
		bombGuardPlanners_[index].reset(); return;
	}
	const auto &handle = managedBotHandles_[index];
	if (handle.entity == nullptr || engineFunctions_ == nullptr || globals_ == nullptr) return;
	const nav::NavVector self = {handle.entity->v.origin.x, handle.entity->v.origin.y,
		handle.entity->v.origin.z + handle.entity->v.mins.z};
	std::vector<nav::NavVector> occupied;
	compat::PublicActorState weapon{};
	const bool publicStateKnown = publicObservations_.read(input.actor, input.frame, &weapon);
	const bool blind = publicStateKnown && input.timestamp < weapon.blindUntil;
	if (!blind && engineFunctions_->pfnPEntityOfEntIndex != nullptr)
	{
		for (int slot = 1; slot <= (std::min)(32, globals_->maxClients); ++slot)
		{
			edict_t *other = engineFunctions_->pfnPEntityOfEntIndex(slot);
			if (other == nullptr || other == handle.entity || other->free ||
				(other->v.flags & FL_CLIENT) == 0 || other->v.deadflag != DEAD_NO || other->v.health <= 0.0f) continue;
			const float eye[] = {handle.entity->v.origin.x + handle.entity->v.view_ofs.x,
				handle.entity->v.origin.y + handle.entity->v.view_ofs.y,
				handle.entity->v.origin.z + handle.entity->v.view_ofs.z};
			const float point[] = {other->v.origin.x, other->v.origin.y, other->v.origin.z};
			const float dx = point[0] - eye[0], dy = point[1] - eye[1];
			const float length = std::hypot(dx,dy);
			const float yaw = handle.entity->v.v_angle.y * 0.017453292519943295f;
			if (length > 75.0f && (dx*std::cos(yaw) + dy*std::sin(yaw)) < 0.5f*length) continue;
			if (engineFunctions_->pfnTraceLine == nullptr) continue;
			TraceResult trace{};
			engineFunctions_->pfnTraceLine(eye, point, 1, handle.entity, &trace);
			if (trace.flFraction != 1.0f && trace.pHit != other) continue;
			occupied.push_back({point[0], point[1], point[2] + other->v.mins.z});
		}
	}
	const bool sniper = publicStateKnown && weapon.weaponKnown &&
		(weapon.weaponId == 3 || weapon.weaponId == 13 || weapon.weaponId == 18 || weapon.weaponId == 24);
	nav::NavVector spot{};
	const auto navigation = navPublisher_.snapshot();
	const nav::NavVector bombPosition = {input.bomb.position.x, input.bomb.position.y, input.bomb.position.z};
	// The active guard target is checked before constructing the danger/team
	// path-cost context. Retained choices need neither a new route nor new costs.
	nav::NavPathCostContext cost{};
	if (bombGuardPlanners_[index].needsSelection(navigation, input.frame, input.bomb.entity,
		bombPosition, self, occupied, input.timestamp)) cost = managedPathCostContext(index);
	if (!bombGuardPlanners_[index].update(navigation, input.frame, input.bomb.entity,
		{input.bomb.position.x, input.bomb.position.y, input.bomb.position.z}, self, sniper, occupied,
		input.timestamp, compatibilityRandomSource_, {input.actor.slot, input.actor.generation},
		{managedBotCommandSequences_[index], 0U, managedBotFullUpdateSequences_[index]}, &spot,
		cost))
	{
		// Unknown NAV/RNG does not authorize converging on the C4 as cover.
		spot = self;
	}
	decision->target.position = {spot.x, spot.y, spot.z};
	decision->target.frame = input.frame;
	const float dx = spot.x - self.x, dy = spot.y - self.y, dz = spot.z - self.z;
	decision->target.pathCost = std::sqrt(dx*dx + dy*dy + dz*dz);
	decision->target.inUseRange = decision->target.pathCost <= 20.0f;
}

int PluginRuntime::publicPlayerTeam(const edict_t *entity) const
{
	if (entity == nullptr || engineFunctions_ == nullptr || engineFunctions_->pfnIndexOfEdict == nullptr) return 0;
	const int slot = engineFunctions_->pfnIndexOfEdict(entity);
	if (slot >= 1 && slot <= 32 && publicTeamKnown_[static_cast<std::size_t>(slot - 1)])
		return managedBotTeamNumbers_[static_cast<std::size_t>(slot - 1)];
	return entity->v.team >= 1 && entity->v.team <= 2 ? entity->v.team : 0;
}

runtime::ActorId PluginRuntime::publicPlayerIdentity(const edict_t *entity) const
{
	if (entity == nullptr || entity->free || engineFunctions_ == nullptr ||
		engineFunctions_->pfnIndexOfEdict == nullptr) return {};
	const int slot = engineFunctions_->pfnIndexOfEdict(entity);
	if (slot < 1 || slot > 32) return {};
	const auto index = static_cast<std::size_t>(slot - 1);
	if (managedBotSlots_[index] && managedBotHandles_[index].entity == entity)
		return managedBotHandles_[index].actor;
	return {static_cast<std::uint32_t>(slot), static_cast<std::uint32_t>(entity->serialnumber) + 1U};
}

std::uint32_t PluginRuntime::publicPlayerPriority(const edict_t *entity) const
{
	const auto actor = publicPlayerIdentity(entity);
	if (actor.slot == 0U) return (std::numeric_limits<std::uint32_t>::max)();
	const auto index = static_cast<std::size_t>(actor.slot - 1U);
	const int team = publicPlayerTeam(entity);
	bool important = team == 1 && (static_cast<std::uint32_t>(entity->v.weapons) & (1U << ActionAdapter::kC4WeaponId)) != 0U;
	const auto &score = publicScoreStatuses_[index][index];
	if (team == 2 && score.known && score.serial == static_cast<std::uint32_t>(entity->serialnumber) &&
		score.frame.mapGeneration == lifecycle_.mapGeneration() && score.frame.roundGeneration == lifecycle_.roundGeneration())
		important = (score.flags & 4) != 0;
	return runtime::playerAvoidancePriority((entity->v.flags & FL_FAKECLIENT) == 0,
		important, managedBotSlots_[index] && managedBotHandles_[index].entity == entity
			? managedBotCreationIds_[index] : 0U);
}

std::vector<nav::NavVector> PluginRuntime::liveFriendlyPositions(std::size_t index, bool feet) const
{
	std::vector<nav::NavVector> positions;
	if (index >= managedBotHandles_.size() || globals_ == nullptr || engineFunctions_ == nullptr ||
		engineFunctions_->pfnPEntityOfEntIndex == nullptr) return positions;
	const auto *self = managedBotHandles_[index].entity;
	const int team = publicPlayerTeam(self);
	if (team == 0) return positions;
	for (int slot = 1; slot <= (std::min)(32, globals_->maxClients); ++slot)
	{
		const auto *other = engineFunctions_->pfnPEntityOfEntIndex(slot);
		if (other == nullptr || other == self || other->free || (other->v.flags & FL_CLIENT) == 0 ||
			other->v.deadflag != DEAD_NO || other->v.health <= 0.0f || publicPlayerTeam(other) != team) continue;
		const float offset = feet ? other->v.mins.z : 0.0f;
		const nav::NavVector position = {other->v.origin.x, other->v.origin.y, other->v.origin.z + offset};
		if (std::isfinite(position.x) && std::isfinite(position.y) && std::isfinite(position.z)) positions.push_back(position);
	}
	return positions;
}

void PluginRuntime::onTouch(edict_t *entity, edict_t *other)
{
	if (state_ != State::ActiveMap || globals_ == nullptr || !std::isfinite(globals_->time) ||
		compatibilitySurface_.configuration().mode != compat::RuntimeMode::Compatibility) return;
	const auto observe = [&](edict_t *self, edict_t *contact) {
		if (self == nullptr || contact == nullptr || self == contact || self->free || contact->free ||
			(self->v.flags & FL_CLIENT) == 0 || (contact->v.flags & FL_CLIENT) == 0 ||
			self->v.deadflag != DEAD_NO || contact->v.deadflag != DEAD_NO || self->v.health <= 0.0f || contact->v.health <= 0.0f) return;
		const auto actor = publicPlayerIdentity(self), touched = publicPlayerIdentity(contact);
		if (actor.slot == 0U || touched.slot == 0U) return;
		const auto index = static_cast<std::size_t>(actor.slot - 1U);
		if (!managedBotSlots_[index] || managedBotHandles_[index].entity != self ||
			!lifecycle_.isCurrent(lifecycle_.tokenForSlot(actor.slot))) return;
		const world::FrameIdentity frame = {lifecycle_.mapGeneration(), lifecycle_.roundGeneration(), adapterFrameCount_};
		runtime::PlayerAvoidanceTouch touch = {actor, frame, touched, publicPlayerPriority(self),
			publicPlayerPriority(contact), globals_->time,
			managedBotStateMachines_[index].state() == compat::CompatibilityStateId::DefuseBomb};
		runtime::ActorId previous{};
		if (playerAvoidance_[index].target(actor, frame, globals_->time, &previous) &&
			engineFunctions_ != nullptr && engineFunctions_->pfnPEntityOfEntIndex != nullptr)
		{
			const auto *incumbent = engineFunctions_->pfnPEntityOfEntIndex(static_cast<int>(previous.slot));
			const auto current = publicPlayerIdentity(incumbent);
			if (current.slot == previous.slot && current.actorGeneration == previous.actorGeneration)
			{
				touch.currentTargetPriority = publicPlayerPriority(incumbent);
				touch.currentTargetPriorityKnown = true;
			}
			else playerAvoidance_[index].reset();
		}
		playerAvoidance_[index].touch(touch);
	};
	observe(entity, other);
	observe(other, entity);
}

void PluginRuntime::applyPlayerAvoidance(std::size_t index, runtime::BotCommand *command)
{
	if (command == nullptr || index >= managedBotHandles_.size() || globals_ == nullptr ||
		compatibilitySurface_.configuration().mode != compat::RuntimeMode::Compatibility ||
		engineFunctions_ == nullptr || engineFunctions_->pfnPEntityOfEntIndex == nullptr) return;
	const auto &handle = managedBotHandles_[index];
	runtime::ActorId target{};
	const world::FrameIdentity frame = {lifecycle_.mapGeneration(), lifecycle_.roundGeneration(), adapterFrameCount_};
	if (!playerAvoidance_[index].target(handle.actor, frame, globals_->time, &target)) return;
	const auto *other = engineFunctions_->pfnPEntityOfEntIndex(static_cast<int>(target.slot));
	const auto identity = publicPlayerIdentity(other);
	const auto *self = handle.entity;
	if (identity.slot != target.slot || identity.actorGeneration != target.actorGeneration ||
		other == nullptr || other->v.deadflag != DEAD_NO || other->v.health <= 0.0f ||
		self == nullptr || self->v.deadflag != DEAD_NO || self->v.health <= 0.0f)
	{
		playerAvoidance_[index].reset(); return;
	}
	if ((self->v.flags & FL_ONGROUND) == 0 || self->v.movetype == MOVETYPE_FLY ||
		engineFunctions_->pfnTraceLine == nullptr) return;
	const float yaw = command->viewAngles.yaw * 0.017453292519943295f;
	const float dx = other->v.origin.x - self->v.origin.x, dy = other->v.origin.y - self->v.origin.y;
	const float length = std::hypot(dx, dy);
	if (!std::isfinite(length) || length <= 0.0f || !std::isfinite(yaw)) return;
	const auto move=runtime::playerAvoidanceMove(command->viewAngles.yaw,dx,dy,
		command->movement.forward,command->movement.side,
		managedMoveSpeeds_[index]>0.0f ? managedMoveSpeeds_[index] : self->v.maxspeed);
	if (!move.valid) return;
	const float worldX = move.forward * std::cos(yaw) + move.side * std::sin(yaw);
	const float worldY = move.forward * std::sin(yaw) - move.side * std::cos(yaw);
	const float speed = std::hypot(worldX, worldY);
	const float feetZ = self->v.origin.z + self->v.mins.z;
	const float start[3] = {self->v.origin.x + 30.0f * worldX / speed,
		self->v.origin.y + 30.0f * worldY / speed, feetZ + 36.0f};
	const float end[3] = {start[0], start[1], feetZ - nav::LocomotionConfig::kMaximumStepHeight};
	TraceResult ground{};
	engineFunctions_->pfnTraceLine(start, end, 1, handle.entity, &ground);
	const bool safe = !ground.fStartSolid && !ground.fAllSolid && std::isfinite(ground.flFraction) &&
		ground.flFraction < 1.0f && std::isfinite(ground.vecEndPos[2]) &&
		std::fabs(ground.vecEndPos[2] - feetZ) <= nav::LocomotionConfig::kMaximumStepHeight;
	if (safe)
	{
		command->movement.forward = move.forward;
		command->movement.side = move.side;
		command->movement.buttons = static_cast<std::uint16_t>((command->movement.buttons &
			~(IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT)) |
			ActionAdapter::movementButtons(move.forward, move.side));
	}
	if (runtimeProfiler_.enabled() && (adapterFrameCount_ % 16U) == 0U && gpMetaUtilFuncs != nullptr &&
		gpMetaUtilFuncs->pfnLogConsole != nullptr && pluginId_ != nullptr)
		profiledLogConsole(runtimeProfiler_, pluginId_,
			"profile playerAvoidance bot_id=%u target=%u frame=%u applied=%d side=%.1f ground_safe=%d forward=%.1f",
			handle.actor.slot, target.slot, adapterFrameCount_, safe ? 1 : 0, move.side, safe ? 1 : 0, move.forward);
}

nav::NavPathCostContext PluginRuntime::managedPathCostContext(std::size_t index)
{
	RuntimeProfilerScope costScope(runtimeProfiler_, RuntimeProfilerStage::PathCostContext);
	nav::NavPathCostContext cost{};
	if (index >= managedBotHandles_.size() || managedBotHandles_[index].entity == nullptr)
	{
		return cost;
	}
	const auto &handle = managedBotHandles_[index];
	cost.health = std::isfinite(handle.entity->v.health) ? (std::max)(0.0f, handle.entity->v.health) : 0.0f;
	cost.aggression = static_cast<float>(managedProfiles_[index].aggression) / 100.0f;
	cost.attacking = managedBotStateMachines_[index].attackOverlayActive();
	const auto navigation = navPublisher_.snapshot();
	const auto *document = navigation.document();
	if (document == nullptr || globals_ == nullptr)
	{
		return cost;
	}
	for (const auto &area : document->areas())
	{
		float danger = 0.0f;
		if (dangerMemory_.mapGeneration() == lifecycle_.mapGeneration() &&
			dangerMemory_.query(globals_->time, managedBotTeamNumbers_[index], area.id, &danger) && danger > 0.0f)
		{
			cost.areas.push_back({area.id, danger, 0U});
		}
	}
	nav::NavQuery query(navigation);
	for (const auto &position : liveFriendlyPositions(index, true))
	{
		nav::NavAreaMatch area{};
		if (query.findContaining(position, 64.0f, &area) != nav::NavQueryResult::Found)
		{
			continue;
		}
		auto found = std::find_if(cost.areas.begin(), cost.areas.end(), [&area](const nav::NavAreaCostState &entry) { return entry.area == area.area; });
		if (found == cost.areas.end())
		{
			cost.areas.push_back({area.area, 0.0f, 1U});
		}
		else
		{
			++found->teammates;
		}
	}
	return cost;
}

void PluginRuntime::refreshManagedTeamObjectiveAssignments(std::size_t contextIndex)
{
	const bool compatibility = compatibilitySurface_.configuration().mode == compat::RuntimeMode::Compatibility;
	const std::uint32_t mapGeneration = lifecycle_.mapGeneration();
	const std::uint32_t roundGeneration = lifecycle_.roundGeneration();
	const world::FrameIdentity frame = {
		mapGeneration, roundGeneration, adapterFrameCount_};
	if (managedTeamObjectiveAssignmentsValid_ && (!compatibility || managedTeamObjectiveAssignmentContext_ == contextIndex) &&
		managedTeamObjectiveAssignmentMapGeneration_ == mapGeneration &&
		managedTeamObjectiveAssignmentRoundGeneration_ == roundGeneration &&
		managedTeamObjectiveAssignmentFrame_ == adapterFrameCount_)
	{
		return;
	}

	managedTeamObjectiveAssignments_ = {};
	managedTeamObjectiveAssignmentsValid_ = false;
	if (contextIndex >= managedBotHandles_.size() ||
		engineFunctions_ == nullptr ||
		engineFunctions_->pfnPEntityOfEntIndex == nullptr ||
		engineFunctions_->pfnSzFromIndex == nullptr ||
		globals_ == nullptr || !frame.isValid())
	{
		return;
	}

	const FakeClientHandle &contextHandle = managedBotHandles_[contextIndex];
	managedTeamObjectiveAssignmentContext_ = contextIndex;
	team::RoundObjectiveCoordinator &coordinator = compatibility
		? compatibilityObjectiveCoordinators_[contextIndex] : managedTeamObjectiveCoordinator_;
	const world::ActorKey contextActor = {
		contextHandle.actor.slot, contextHandle.actor.actorGeneration};
	const compat::ObservationTimingContext timing = {
		managedBotCommandSequences_[contextIndex], 0U, 0U};
	team::TeamObjectiveInput input = {};
	input.mapGeneration = mapGeneration;
	input.roundGeneration = roundGeneration;
	input.frameSequence = adapterFrameCount_;
	const nav::NavSnapshot navigation = navPublisher_.snapshot();
	const nav::NavDocument *document = navigation.document();
	nav::NavQuery query(navigation);
	if (compatibility)
	{
		query.setPathCostContext(managedPathCostContext(contextIndex));
	}

	for (std::size_t index = 0U; index < managedBotHandles_.size() &&
		input.actorCount < team::kMaximumTeamObjectiveActors; ++index)
	{
		if (!managedBotSlots_[index] || managedBotHandles_[index].entity == nullptr ||
			managedBotHandles_[index].entity->free != 0 || (compatibility && index != contextIndex))
		{
			continue;
		}
		const FakeClientHandle &handle = managedBotHandles_[index];
		compat::CompatibilityObservation observation = {};
		const compat::ObservationTimingContext actorTiming = {
			managedBotCommandSequences_[index], 0U, 0U};
		if (observationAdapter_.collectActor(
				handle.entity,
				{handle.actor.slot, handle.actor.actorGeneration},
				frame,
				actorTiming,
				&observation) != ObservationAdapterResult::Accepted)
		{
			continue;
		}

		team::TeamObjectiveActorObservation &actor =
			input.actors[input.actorCount];
		actor = {};
		actor.actor = {handle.actor.slot, handle.actor.actorGeneration};
		const bool teamInfoConfirmed =
			joinControllers_[index].teamConfirmed() &&
			managedBotTeamNumbers_[index] >= 1U &&
			managedBotTeamNumbers_[index] <= 2U;
		actor.team = !teamInfoConfirmed
			? objectives::TeamRole::Unknown
			: managedBotTeamNumbers_[index] == 1U
				? objectives::TeamRole::Terrorist
				: objectives::TeamRole::CounterTerrorist;
		actor.alive = observation.player.deadflag.isAvailable() &&
			observation.player.deadflag.value == DEAD_NO;
		actor.carryingC4 = observation.objective.carryingC4.isAvailable() &&
			observation.objective.carryingC4.value;
		actor.positionAvailable = observation.player.origin.isAvailable();
		if (actor.positionAvailable)
		{
			actor.position = observation.player.origin.value;
			if (navigation.isValid() && document != nullptr)
			{
				nav::NavAreaMatch area = {};
				const nav::NavVector position = {
					actor.position.x, actor.position.y, actor.position.z};
				if (query.findContaining(position, 64.0f, &area) !=
					nav::NavQueryResult::Found &&
					query.findNearest(position, kMaximumObjectiveDistance, &area) !=
						nav::NavQueryResult::Found)
				{
					area.area = 0U;
				}
				actor.currentArea = area.area;
			}
		}
		++input.actorCount;
	}

	const int maximumEntities =
		(std::max)(0, (std::min)(globals_->maxEntities, 2048));
	compat::DroppedC4Observation droppedObservation = {};
	compat::ObjectiveObservation plantedObservation = {};
	for (int entityIndex = 1; entityIndex <= maximumEntities; ++entityIndex)
	{
		edict_t *entity = engineFunctions_->pfnPEntityOfEntIndex(entityIndex);
		if (entity == nullptr || entity->free != 0)
		{
			continue;
		}
		const char *classname =
			engineFunctions_->pfnSzFromIndex(entity->v.classname);
		const char *model = engineFunctions_->pfnSzFromIndex(entity->v.model);
		if (!input.droppedC4.available)
		{
			compat::DroppedC4Observation candidate = {};
			if (observationAdapter_.collectDroppedC4(
					entity, classname, model, contextActor, frame, timing, &candidate) ==
				ObservationAdapterResult::Accepted &&
				candidate.isCurrent(frame))
			{
				droppedObservation = candidate;
				input.droppedC4.available = true;
				input.droppedC4.entity = candidate.entity;
				world::WorldVector knownPosition{};
				if (!observeObjective(contextIndex, entity, false, &knownPosition))
				{
					input.droppedC4 = {};
					continue;
				}
				input.droppedC4.position = knownPosition;
			}
		}
		if (!input.plantedC4.available &&
			observationAdapter_.collectPlantedBomb(
				entity, classname, model, globals_->time, contextActor, frame,
				timing, &plantedObservation) == ObservationAdapterResult::Accepted &&
			plantedObservation.bombPosition.isCurrent(frame) &&
			engineFunctions_->pfnIndexOfEdict != nullptr &&
			entity->serialnumber != 0)
		{
			const int plantedIndex = engineFunctions_->pfnIndexOfEdict(entity);
			if (plantedIndex > 0)
			{
				input.plantedC4.available = true;
				input.plantedC4.entity = {
					static_cast<std::uint32_t>(plantedIndex),
					static_cast<std::uint32_t>(entity->serialnumber)};
				world::WorldVector knownPosition{};
				if (!observeObjective(contextIndex, entity, true, &knownPosition))
				{
					input.plantedC4 = {};
					continue;
				}
				input.plantedC4.position = knownPosition;
			}
		}
	}

	if (navigation.isValid() && document != nullptr)
	{
		for (team::TeamBombTargetObservation *target :
			{&input.droppedC4, &input.plantedC4})
		{
			if (!target->available)
			{
				continue;
			}
			nav::NavAreaMatch area = {};
			if (query.findNearest(
					{target->position.x, target->position.y, target->position.z},
					kMaximumObjectiveDistance, &area) == nav::NavQueryResult::Found)
			{
				target->area = area.area;
			}
		}
	}

	if (managedTeamObjectivePathFailurePending_)
	{
		coordinator.reportPathFailure(
			managedTeamObjectivePathFailureActor_,
			managedTeamObjectivePathFailureGeneration_,
			managedTeamObjectivePathFailureStartArea_,
			managedTeamObjectivePathFailureFrame_);
		managedTeamObjectivePathFailurePending_ = false;
		managedTeamObjectivePathFailureActor_ = {};
		managedTeamObjectivePathFailureGeneration_ = 0U;
		managedTeamObjectivePathFailureStartArea_ = 0U;
		managedTeamObjectivePathFailureFrame_ = 0U;
	}
	const bool externalDefuserActive = externalDefuserEntityIndex_ != 0U &&
		externalDefuserMapGeneration_ == mapGeneration &&
		externalDefuserRoundGeneration_ == roundGeneration &&
		std::isfinite(globals_->time) && std::isfinite(externalDefuserLeaseUntil_) &&
		globals_->time <= externalDefuserLeaseUntil_;
	input.externalDefuserActive = externalDefuserActive;
	(void)coordinator.assign(
		input, navigation, &managedTeamObjectiveAssignments_);
	if (runtimeProfiler_.enabled() && gpMetaUtilFuncs != nullptr &&
		gpMetaUtilFuncs->pfnLogConsole != nullptr && pluginId_ != nullptr)
	{
		for (std::size_t index = 0U;
			index < managedTeamObjectiveAssignments_.count; ++index)
		{
			const team::TeamObjectiveAssignment &assignment =
				managedTeamObjectiveAssignments_.assignments[index];
			profiledLogConsole(runtimeProfiler_,
				pluginId_,
				"team_objective map=%u round=%u frame=%u actor=%u:%u kind=%u target=%u:%u area=%u generation=%u cost=%.2f cost_fallback=%u dropped=%u planted=%u",
				static_cast<unsigned int>(mapGeneration),
				static_cast<unsigned int>(roundGeneration),
				static_cast<unsigned int>(adapterFrameCount_),
				static_cast<unsigned int>(assignment.actor.slot),
				static_cast<unsigned int>(assignment.actor.generation),
				static_cast<unsigned int>(assignment.kind),
				static_cast<unsigned int>(assignment.targetEntity.id),
				static_cast<unsigned int>(assignment.targetEntity.generation),
				static_cast<unsigned int>(assignment.targetArea),
				static_cast<unsigned int>(assignment.generation),
				assignment.routeCostAvailable ? assignment.routeCost : 0.0f,
				assignment.routeCostGeometricFallback ? 1U : 0U,
				input.droppedC4.available ? 1U : 0U,
				input.plantedC4.available ? 1U : 0U);
		}
	}
	managedTeamObjectiveAssignmentMapGeneration_ = mapGeneration;
	managedTeamObjectiveAssignmentRoundGeneration_ = roundGeneration;
	managedTeamObjectiveAssignmentFrame_ = adapterFrameCount_;
	managedTeamObjectiveAssignmentsValid_ = true;
}

const team::TeamObjectiveAssignment *
PluginRuntime::findManagedTeamObjectiveAssignment(
	const world::ActorKey &actor) const
{
	if (!managedTeamObjectiveAssignmentsValid_)
	{
		return nullptr;
	}
	for (std::size_t index = 0U;
		index < managedTeamObjectiveAssignments_.count; ++index)
	{
		const team::TeamObjectiveAssignment &assignment =
			managedTeamObjectiveAssignments_.assignments[index];
		if (assignment.actor == actor)
		{
			return &assignment;
		}
	}
	return nullptr;
}

float PluginRuntime::managedRoundRemainingSeconds() const
{
	const float minutes = readOptionalCvarFloat(engineFunctions_, "mp_roundtime");
	if (globals_ == nullptr || !std::isfinite(globals_->time) ||
		!std::isfinite(minutes) || minutes <= 0.0f ||
		roundFreezeGeneration_ != lifecycle_.roundGeneration())
	{
		return std::numeric_limits<float>::quiet_NaN();
	}
	return (std::max)(0.0f, minutes * 60.0f -
		(std::max)(0.0f, globals_->time - roundFreezeUntil_));
}

bool PluginRuntime::managedActorAtBombSite(std::size_t index) const
{
	if (index >= managedBotHandles_.size() || managedBotHandles_[index].entity == nullptr ||
		engineFunctions_ == nullptr || engineFunctions_->pfnPEntityOfEntIndex == nullptr ||
		engineFunctions_->pfnSzFromIndex == nullptr)
	{
		return false;
	}
	const edict_t *actor = managedBotHandles_[index].entity;
	const PlantTargetBounds actorBounds = {actor->v.absmin.x, actor->v.absmin.y,
		actor->v.absmin.z, actor->v.absmax.x, actor->v.absmax.y, actor->v.absmax.z};
	for (std::size_t siteIndex = 0U;
		siteIndex < managedObjectiveSiteRegistry_.registeredCount; ++siteIndex)
	{
		const auto &entry = managedObjectiveSiteRegistry_.entries[siteIndex];
		edict_t *site = engineFunctions_->pfnPEntityOfEntIndex(entry.entityIndex);
		if (site == nullptr || site->free) continue;
		const char *classname = engineFunctions_->pfnSzFromIndex(site->v.classname);
		if (ActionAdapter::isLegacyInfoBombTargetClassname(classname))
		{
			const auto center = entityObjectiveCenter(site);
			if (ActionAdapter::withinLegacyBombTargetRadius({center.x, center.y, center.z},
				{actor->v.origin.x, actor->v.origin.y, actor->v.origin.z})) return true;
		}
		else if (isBombTargetClassname(classname))
		{
			const PlantTargetBounds siteBounds = {site->v.absmin.x, site->v.absmin.y,
				site->v.absmin.z, site->v.absmax.x, site->v.absmax.y, site->v.absmax.z};
			if (siteBounds.maximumX > siteBounds.minimumX &&
				siteBounds.maximumY > siteBounds.minimumY &&
				ActionAdapter::overlapsPlantTarget(siteBounds, actorBounds)) return true;
		}
	}
	return false;
}

bool PluginRuntime::buildManagedObjectiveTarget(
	std::size_t index,
	nav::NavVector *target,
	nav::NavSearchStats *searchStats,
	RuntimeObjectiveSelectionStats *objectiveStats)
{
	if (objectiveStats != nullptr)
	{
		*objectiveStats = {};
	}
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
	if (compatibilitySurface_.configuration().mode == compat::RuntimeMode::Compatibility)
	{
		const auto &belief = managedBotPerception_[index].snapshot();
		if (managedBotStateMachines_[index].state() == compat::CompatibilityStateId::InvestigateNoise &&
			belief.isValid() && belief.identity().frame.mapGeneration == lifecycle_.mapGeneration() &&
			belief.identity().frame.roundGeneration == lifecycle_.roundGeneration() &&
			belief.noise().active && belief.noise().ready)
		{
			const auto &position = belief.noise().position;
			*target = {position.x, position.y, position.z};
			return true;
		}
		const auto &scenario = scenarioDecisions_[index];
		if (scenario.hasTarget && !scenario.unavailable &&
			scenario.target.frame.mapGeneration == lifecycle_.mapGeneration() &&
			scenario.target.frame.roundGeneration == lifecycle_.roundGeneration() &&
			scenario.target.frame.tick <= adapterFrameCount_)
		{
			*target = {scenario.target.position.x, scenario.target.position.y, scenario.target.position.z};
			return true;
		}
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
	const int rawTeam = compatObservation.player.team.isAvailable()
		? compatObservation.player.team.value
		: static_cast<int>(handle.entity->v.team);
	const int team = resolveManagedBotTeam(index, compatObservation);
	ManagedObjectiveTargetCache &cache = managedBotObjectiveTargets_[index];
	cache.rawTeam = rawTeam;
	cache.effectiveTeam = team;
	cache.teamInfoFresh = joinControllers_[index].teamConfirmed() &&
		managedBotTeamNumbers_[index] >= 1U && managedBotTeamNumbers_[index] <= 2U;
	if (team != 1 && team != 2)
	{
		return false;
	}
	const bool carryingBomb = compatObservation.objective.carryingC4.isAvailable() &&
		compatObservation.objective.carryingC4.value;
	refreshManagedTeamObjectiveAssignments(index);
	const world::ActorKey actorKey = {
		handle.actor.slot, handle.actor.actorGeneration};
	const team::TeamObjectiveAssignment *assignment =
		findManagedTeamObjectiveAssignment(actorKey);
	const team::TeamObjectiveKind assignmentKind = assignment == nullptr
		? team::TeamObjectiveKind::None
		: assignment->kind;
	const std::uint32_t assignmentGeneration = assignment == nullptr
		? 0U
		: assignment->generation;
	const bool needsDroppedC4 =
		assignmentKind == team::TeamObjectiveKind::RetrieveDroppedC4;
	const bool needsBombSite = team == 1 && carryingBomb &&
		assignmentKind == team::TeamObjectiveKind::PlantC4;
	const bool needsPlantedBomb = team == 2 &&
		assignmentKind == team::TeamObjectiveKind::DefuseC4;
	objectives::BombApproachState &approach = managedBombApproaches_[index];
	if (needsBombSite && compatibilitySurface_.configuration().mode == compat::RuntimeMode::Compatibility)
	{
		approach.mapGeneration = frame.mapGeneration;
		approach.roundGeneration = frame.roundGeneration;
		approach.actorSlot = actor.slot;
		approach.actorGeneration = actor.generation;
		approach.initialized = true;
		approach.plantAt = roundPlantDecision_.plantAt;
		approach.finished = (roundPlantDecision_.known && globals_->time >= approach.plantAt) ||
			(managedActorAtBombSite(index) && globals_->time - lastVisibleEnemyTimes_[index] > 3.0f);
		if (!approach.finished)
		{
			cache.valid = false;
			return false;
		}
	}
	if (needsBombSite)
	{
		refreshManagedObjectiveSiteRegistry();
		if (objectiveStats != nullptr)
		{
			objectiveStats->registeredSites = managedObjectiveSiteRegistry_.registeredCount;
			objectiveStats->registeredFuncBombTargetSites =
				managedObjectiveSiteRegistry_.registeredFuncBombTargetCount;
			objectiveStats->registeredInfoBombTargetSites =
				managedObjectiveSiteRegistry_.registeredInfoBombTargetCount;
			objectiveStats->registeredSiteCountsAvailable = true;
		}
	}
	if (!needsBombSite)
	{
		approach = {};
	}
	else if (approach.initialized && !approach.finished &&
		objectives::bombApproachPhase(&approach, globals_->time,
			managedRoundRemainingSeconds(), cache.approachRouteSeconds,
			carryingBomb, false, managedActorAtBombSite(index)) ==
			objectives::BombApproachPhase::Plant)
	{
		// One phase transition permits a fresh nearest route from the moved position.
		cache.valid = false;
		cache.generation = cache.generation ==
			(std::numeric_limits<std::uint32_t>::max)() ? 1U : cache.generation + 1U;
	}
	const bool sameObjectiveState = cache.mapGeneration == lifecycle_.mapGeneration() &&
		cache.roundGeneration == lifecycle_.roundGeneration() &&
		cache.team == static_cast<std::uint8_t>(team) &&
		cache.carryingBomb == carryingBomb &&
		cache.teamObjectiveKind == assignmentKind &&
		cache.teamObjectiveAssignmentGeneration == assignmentGeneration;
	const bool diagnosticCacheRefresh = sameObjectiveState && needsBombSite &&
		cache.valid && objectiveStats != nullptr && !cache.diagnosticsEmitted;
	const char *objectiveCacheState = diagnosticCacheRefresh
		? "profile_refresh"
		: "miss";
	if (sameObjectiveState && needsBombSite && cache.valid && !diagnosticCacheRefresh)
	{
		edict_t *cachedSite = cache.selectedEntityIndex > 0
			? engineFunctions_->pfnPEntityOfEntIndex(cache.selectedEntityIndex)
			: nullptr;
		const char *cachedClassname = cachedSite != nullptr && !cachedSite->free
			? engineFunctions_->pfnSzFromIndex(cachedSite->v.classname)
			: nullptr;
		if (cachedClassname != nullptr && isBombTargetClassname(cachedClassname))
		{
			*target = cache.target;
			if (objectiveStats != nullptr)
			{
				++objectiveStats->cacheHits;
				objectiveStats->selectedSiteIdentity = cache.selectedSiteIdentity;
			}
			cache.lastCacheHit = true;
			return std::isfinite(target->x) && std::isfinite(target->y) &&
				std::isfinite(target->z);
		}
		cache.valid = false;
		cache.diagnosticsEmitted = false;
		cache.lastCacheHit = false;
		cache.generation = cache.generation ==
			(std::numeric_limits<std::uint32_t>::max)() ? 1U : cache.generation + 1U;
	}
	if (!sameObjectiveState)
	{
		cache.generation = cache.generation ==
			(std::numeric_limits<std::uint32_t>::max)() ? 1U : cache.generation + 1U;
	}
	cache.valid = false;
	cache.selectedEntityIndex = 0;
	cache.selectedSiteIdentity = 0U;
		cache.mapGeneration = lifecycle_.mapGeneration();
		cache.roundGeneration = lifecycle_.roundGeneration();
	cache.team = static_cast<std::uint8_t>(team);
	cache.carryingBomb = carryingBomb;
	cache.teamObjectiveKind = assignmentKind;
	cache.teamObjectiveAssignmentGeneration = assignmentGeneration;
	cache.diagnosticsEmitted = false;
	cache.lastCacheHit = false;
	if (needsDroppedC4 && assignment != nullptr)
	{
		*target = {
			assignment->targetPosition.x,
			assignment->targetPosition.y,
			assignment->targetPosition.z};
		return std::isfinite(target->x) && std::isfinite(target->y) &&
			std::isfinite(target->z);
	}
	if (assignmentKind == team::TeamObjectiveKind::GuardBombDefuser &&
		assignment != nullptr)
	{
		*target = {
			assignment->targetPosition.x,
			assignment->targetPosition.y,
			assignment->targetPosition.z};
		return std::isfinite(target->x) && std::isfinite(target->y) &&
			std::isfinite(target->z);
	}
	if (!needsBombSite && !needsPlantedBomb)
	{
		return false;
	}

	edict_t *plantedBomb = nullptr;
	compat::ObjectiveObservation plantedObservation = {};
	nav::NavVector selectedBombSite = {0.0f, 0.0f, 0.0f};
	float selectedPathCost = (std::numeric_limits<float>::max)();
	int selectedBombSiteEntityIndex = 0;
	std::uint32_t selectedSiteIdentity = 0U;
	struct ReachableBombSite
	{
		std::uint32_t identity;
		int entityIndex;
		float cost;
		nav::NavVector point;
	};
	std::vector<ReachableBombSite> reachableBombSites;
	const nav::NavSnapshot navigation = navPublisher_.snapshot();
	const nav::NavDocument *document = navigation.document();
	nav::NavAreaMatch currentMatch = {};
	bool haveCurrentArea = false;
	std::vector<ObjectiveCandidateKey> evaluatedCandidateKeys;
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
	std::uint32_t targetId = 0U;
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
		if (needsBombSite && isBombTargetClassname(classname))
		{
			const std::uint32_t siteIdentity = ++targetId;
		if (objectiveStats != nullptr)
		{
			++objectiveStats->bombSites;
			++objectiveStats->evaluatedSites;
			if (equalsIgnoreCase(classname, "func_bomb_target"))
			{
				++objectiveStats->funcBombTargetSites;
			}
			else if (equalsIgnoreCase(classname, "info_bomb_target"))
			{
				++objectiveStats->infoBombTargetSites;
			}
		}
			const nav::NavVector candidate = entityObjectiveCenter(entity);
			nav::NavExtent siteExtent = {};
			const bool hasSiteExtent = objectiveSiteExtent(entity, classname, &siteExtent);
			float pathCost = (std::numeric_limits<float>::max)();
			nav::NavVector reachablePoint = candidate;
			nav::AreaId nearestNavArea = 0U;
			nav::AreaId candidateNavArea = 0U;
			bool validCandidate = false;
			std::uint32_t siteCandidateAreaSamples = 0U;
			std::uint32_t siteUniqueCandidateAreas = 0U;
			std::uint32_t siteReachableCandidateAreas = 0U;
			const char *rejectionReason = "no_candidate_nav_area";
			if (haveCurrentArea && document != nullptr && hasSiteExtent)
			{
				nav::NavQuery query(navigation);
				const bool compatibility = compatibilitySurface_.configuration().mode == compat::RuntimeMode::Compatibility;
				if (compatibility)
				{
					query.setPathCostContext(managedPathCostContext(index));
				}
				const nav::NavVector samplePoints[] = {
					candidate,
					{siteExtent.lo.x, (siteExtent.lo.y + siteExtent.hi.y) * 0.5f, candidate.z},
					{siteExtent.hi.x, (siteExtent.lo.y + siteExtent.hi.y) * 0.5f, candidate.z},
					{(siteExtent.lo.x + siteExtent.hi.x) * 0.5f, siteExtent.lo.y, candidate.z},
					{(siteExtent.lo.x + siteExtent.hi.x) * 0.5f, siteExtent.hi.y, candidate.z}};
				for (const nav::NavVector &samplePoint : samplePoints)
				{
					nav::NavAreaMatch siteMatch = {};
					if (query.findNearest(
							samplePoint, kMaximumObjectiveDistance, &siteMatch) !=
						nav::NavQueryResult::Found)
					{
						continue;
					}
					if (nearestNavArea == 0U)
					{
						nearestNavArea = siteMatch.area;
					}
					const nav::NavArea *siteArea = document->findArea(siteMatch.area);
					if (siteArea == nullptr ||
						siteMatch.closestPoint.x < siteExtent.lo.x ||
						siteMatch.closestPoint.x > siteExtent.hi.x ||
						siteMatch.closestPoint.y < siteExtent.lo.y ||
						siteMatch.closestPoint.y > siteExtent.hi.y)
					{
						if (!validCandidate)
						{
							rejectionReason = "candidate_outside_site_extent";
						}
					continue;
				}
				if (equalsIgnoreCase(classname, "info_bomb_target") &&
					!ActionAdapter::withinLegacyBombTargetRadius(
						{candidate.x, candidate.y, candidate.z},
						{siteMatch.closestPoint.x,
						 siteMatch.closestPoint.y,
						 siteMatch.closestPoint.z}))
				{
					rejectionReason = "outside_legacy_info_radius";
					continue;
				}
				if (objectiveStats != nullptr)
					{
						++objectiveStats->candidateAreas;
					}
					++siteCandidateAreaSamples;
					const ObjectiveCandidateKey candidateKey = {
						entityIndex, siteIdentity, siteMatch.area};
					if (std::find(
							evaluatedCandidateKeys.begin(), evaluatedCandidateKeys.end(),
							candidateKey) != evaluatedCandidateKeys.end())
					{
						if (objectiveStats != nullptr)
						{
							++objectiveStats->duplicateCandidateAreas;
						}
						if (!validCandidate)
						{
							rejectionReason = "duplicate_candidate_identity";
						}
						continue;
					}
					evaluatedCandidateKeys.push_back(candidateKey);
					++siteUniqueCandidateAreas;
					if (objectiveStats != nullptr)
					{
						++objectiveStats->uniqueCandidateAreas;
						++objectiveStats->candidateQueries;
					}
					const nav::NavArea &area = *siteArea;
					nav::NavCorridor corridor = {};
					nav::NavSearchStats sample = {};
					if (query.buildCorridor(currentMatch.area, area.id,
							compatibility ? nav::NavRouteType::Safest : nav::NavRouteType::Fastest, &corridor,
							searchStats == nullptr ? nullptr : &sample) ==
						nav::NavQueryResult::Found && std::isfinite(corridor.cost))
					{
						++siteReachableCandidateAreas;
						if (corridor.cost < pathCost)
						{
							pathCost = corridor.cost;
							reachablePoint = siteMatch.closestPoint;
							candidateNavArea = siteMatch.area;
						}
						validCandidate = true;
						rejectionReason = "none";
					}
					else
					{
						if (!validCandidate)
						{
							rejectionReason = "no_route";
						}
					}
					addNavSearchStats(searchStats, sample);
				}
			}
			else
			{
				pathCost = haveCurrentArea ? 1.0f : 0.0f;
				validCandidate = true;
				rejectionReason = "nav_unavailable_fallback";
			}
			if (objectiveStats != nullptr && !cache.diagnosticsEmitted)
			{
				logObjectiveTargetDiagnostic(
					runtimeProfiler_, pluginId_, targetId, entityIndex, classname,
					managedObjectiveSiteRegistry_.registeredCount,
					managedObjectiveSiteRegistry_.registeredFuncBombTargetCount,
					managedObjectiveSiteRegistry_.registeredInfoBombTargetCount,
					entity, candidate,
					nearestNavArea, candidateNavArea, siteCandidateAreaSamples,
					siteUniqueCandidateAreas, siteReachableCandidateAreas,
					pathCost == (std::numeric_limits<float>::max)() ? -1.0f : pathCost,
					objectiveCacheState, validCandidate, rejectionReason);
			}
			if (validCandidate)
			{
				reachableBombSites.push_back({siteIdentity, entityIndex, pathCost, reachablePoint});
			}
		if (validCandidate && objectives::isBetterBombSiteRoute(
				pathCost, selectedPathCost))
			{
				selectedPathCost = pathCost;
				selectedBombSite = reachablePoint;
				selectedBombSiteEntityIndex = entityIndex;
				selectedSiteIdentity = siteIdentity;
			}
		}
		const char *model = engineFunctions_->pfnSzFromIndex(entity->v.model);
	if (needsPlantedBomb && plantedBomb == nullptr && observationAdapter_.collectPlantedBomb(
			entity, classname, model, globals_->time, actor, frame,
			observationTiming, &plantedObservation) == ObservationAdapterResult::Accepted)
		{
			world::WorldVector knownPosition{};
			if (observeObjective(index, entity, true, &knownPosition))
			{
				plantedBomb = entity;
			}
			}
		}
		if (objectiveStats != nullptr)
		{
			cache.diagnosticsEmitted = true;
		}
		if (team == 1 && carryingBomb && selectedPathCost !=
		(std::numeric_limits<float>::max)())
	{
		const float speed = std::isfinite(handle.entity->v.maxspeed) &&
			handle.entity->v.maxspeed > 0.0f ? handle.entity->v.maxspeed : 240.0f;
		const float nearestTravel = selectedPathCost / speed;
		const float roundRemaining = managedRoundRemainingSeconds();
		const compat::RandomActor randomActor = {handle.actor.slot, handle.actor.actorGeneration};
		const compat::RandomTimingContext randomTiming = {
			managedBotCommandSequences_[index], adapterFrameCount_, managedBotFullUpdateSequences_[index]};
		const float decisionStart = roundFreezeGeneration_ == lifecycle_.roundGeneration()
			? roundFreezeStartTime_ : globals_->time;
		objectives::beginBombApproach(&approach, frame.mapGeneration, frame.roundGeneration,
			randomActor, globals_->time, decisionStart, roundRemaining, nearestTravel,
			haveCurrentArea && document != nullptr ? &compatibilityRandomSource_ : nullptr, randomTiming);
		const auto phase = objectives::bombApproachPhase(&approach, globals_->time,
			roundRemaining, nearestTravel, carryingBomb, false, managedActorAtBombSite(index));
		if (phase == objectives::BombApproachPhase::Approach)
		{
			if (approach.preferredSiteIdentity == 0U)
			{
				std::vector<float> costs;
				const float delayLeft = (std::max)(0.0f, approach.plantAt - globals_->time);
				for (const auto &site : reachableBombSites)
				{
					costs.push_back(roundRemaining > delayLeft + site.cost / speed + 5.0f
						? site.cost : -1.0f);
				}
				const auto choice = objectives::chooseBombApproachSite(costs.data(), costs.size(),
					&compatibilityRandomSource_, randomActor, randomTiming);
				if (choice < reachableBombSites.size())
					approach.preferredSiteIdentity = reachableBombSites[choice].identity;
			}
			const auto preferred = std::find_if(reachableBombSites.begin(), reachableBombSites.end(),
				[&approach](const ReachableBombSite &site)
				{
					return site.identity == approach.preferredSiteIdentity;
				});
			if (preferred != reachableBombSites.end())
			{
				selectedBombSite = preferred->point;
				selectedBombSiteEntityIndex = preferred->entityIndex;
				selectedSiteIdentity = preferred->identity;
				selectedPathCost = preferred->cost;
			}
			else
			{
				approach.finished = true;
			}
		}
		cache.approachRouteSeconds = selectedPathCost / speed;
		*target = selectedBombSite;
		cache.target = *target;
		cache.selectedEntityIndex = selectedBombSiteEntityIndex;
		cache.selectedSiteIdentity = selectedSiteIdentity;
		cache.valid = true;
		if (objectiveStats != nullptr)
		{
			objectiveStats->selectedSiteIdentity = selectedSiteIdentity;
		}
	}
	else if (team == 2 && plantedBomb != nullptr)
	{
		world::WorldVector knownPosition{};
		if (!observeObjective(index, plantedBomb, true, &knownPosition))
		{
			return false;
		}
		*target = {knownPosition.x, knownPosition.y, knownPosition.z};
	}
	else
	{
		return false;
	}
	cache.diagnosticsEmitted = objectiveStats != nullptr;
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
	if(index<managedBotStateMachines_.size() &&
		compatibilitySurface_.configuration().mode==compat::RuntimeMode::Compatibility &&
		!managedBotStateMachines_[index].attackOverlayActive() &&
		(managedBotStateMachines_[index].state()==compat::CompatibilityStateId::PlantBomb ||
		 managedBotStateMachines_[index].state()==compat::CompatibilityStateId::DefuseBomb))
	{
		proposal.stopForWeapon=true;
		proposal.movementButtons=static_cast<std::uint16_t>((proposal.movementButtons &
			~(IN_JUMP|IN_FORWARD|IN_BACK|IN_MOVELEFT|IN_MOVERIGHT))|IN_DUCK);
	}
	bool objectiveActionDispatched = false;
	bool preserveObjectiveActionState = false;
	const auto cancelHeldGrenade = [&]() -> bool
	{
		if (index >= managedBotHandles_.size() || !weaponActions_[index].grenadeInProgress())
			return false;
		const FakeClientHandle &cancelHandle = managedBotHandles_[index];
		compat::WeaponActionInput cancel = {};
		cancel.actor = {cancelHandle.actor.slot,cancelHandle.actor.actorGeneration};
		cancel.frame = {lifecycle_.mapGeneration(),lifecycle_.roundGeneration(),adapterFrameCount_};
		cancel.timestamp = globals_ != nullptr ? globals_->time : 0.0f;
		cancel.weaponId = weaponActions_[index].currentWeaponId();
		compat::PublicActorState activeWeapon = {};
		if (publicObservations_.read(cancel.actor,cancel.frame,&activeWeapon) && activeWeapon.weaponKnown &&
			activeWeapon.weaponId != static_cast<int>(cancel.weaponId))
		{
			weaponActions_[index].reset();
			return false;
		}
		const auto ammoDefinition = publicObservations_.definition(cancel.weaponId);
		if (activeWeapon.bound && ammoDefinition.known && ammoDefinition.ammoIndex >= 0 &&
			static_cast<std::size_t>(ammoDefinition.ammoIndex) < activeWeapon.ammo.size() &&
			activeWeapon.ammoKnown[static_cast<std::size_t>(ammoDefinition.ammoIndex)])
		{
			const auto ammoIndex = static_cast<std::size_t>(ammoDefinition.ammoIndex);
			cancel.reserveKnown = true;
			cancel.reserve = static_cast<std::uint16_t>(activeWeapon.ammo[ammoIndex]);
			cancel.reserveObservationTick = activeWeapon.ammoTicks[ammoIndex];
		}
		const auto canceled = weaponActions_[index].update(cancel);
		proposal.kind = canceled.primary ? ActionKind::Fire : ActionKind::None;
		proposal.stopForWeapon = canceled.stopMovement;
		if (canceled.hasRetainedGrenadeAim)
			proposal.viewAngles = {canceled.grenadePitch,canceled.grenadeYaw,movementAngles.roll};
		return true;
	};
	if (index >= managedBotHandles_.size())
	{
		return proposal;
	}
	if (before.dead || managedBotHandles_[index].entity == nullptr)
	{
		managedBotObjectiveActions_[index].active = false;
		weaponActions_[index].reset();
		escapingWithKnife_[index] = false;
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
		managedBotObjectiveActions_[index].active = false;
		(void)cancelHeldGrenade();
		return proposal;
	}
	if(compatibilitySurface_.configuration().mode==compat::RuntimeMode::Compatibility &&
		managedBotStateMachines_[index].state()==compat::CompatibilityStateId::PlantBomb &&
		!managedBotStateMachines_[index].attackOverlayActive() &&
		compatObservation.objective.carryingC4.isAvailable() && compatObservation.objective.carryingC4.value &&
		managedActorAtBombSite(index))
	{
		// Plant owns the interaction after arrival. A cleared corridor or an old
		// destination farther than96 must not suppress its C4 selection/attack.
		if(cancelHeldGrenade())return proposal;
		auto &action=managedBotObjectiveActions_[index];
		action.active=true;action.actor=actor;action.kind=team::TeamObjectiveKind::PlantC4;
		action.mapGeneration=frame.mapGeneration;action.roundGeneration=frame.roundGeneration;
		const auto &weapon=managedBotCurrentWeapons_[index];
		const bool current=weapon.available && weapon.actorGeneration==actor.generation &&
			weapon.mapGeneration==frame.mapGeneration && weapon.roundGeneration==frame.roundGeneration;
		proposal.kind=ActionAdapter::plantActionForWeaponObservation(current,current && weapon.active,
			weapon.weaponId,action.c4AttackInputDispatched);
		proposal.stopForWeapon=true;
		proposal.movementButtons=IN_DUCK;
		proposal.viewAngles={90.0f,movementAngles.yaw,movementAngles.roll};
		return proposal;
	}
	if (compatibilitySurface_.configuration().mode == compat::RuntimeMode::Compatibility &&
		scenarioDecisions_[index].action == compat::ScenarioAction::Escape &&
		managedBotStateMachines_[index].state() == compat::CompatibilityStateId::EscapeFromBomb)
	{
		if (cancelHeldGrenade()) return proposal;
		escapingWithKnife_[index] = true;
		if (!compatObservation.weapon.activeWeaponId.isAvailable() ||
			compatObservation.weapon.activeWeaponId.value != 29U)
			proposal.kind = ActionKind::SelectKnife;
		return proposal;
	}
	if (escapingWithKnife_[index])
	{
		escapingWithKnife_[index] = false;
		const auto inventory = static_cast<std::uint32_t>(handle.entity->v.weapons);
		for (const unsigned id : {3U,5U,7U,8U,12U,13U,14U,15U,18U,19U,20U,21U,22U,23U,24U,27U,28U,30U})
			if ((inventory & (1U << id)) != 0U) { proposal.kind = ActionKind::SelectPrimary; return proposal; }
		for (const unsigned id : {1U,10U,11U,16U,17U,26U})
			if ((inventory & (1U << id)) != 0U) { proposal.kind = ActionKind::SelectPistol; return proposal; }
	}
	refreshManagedTeamObjectiveAssignments(index);
	const team::TeamObjectiveAssignment *objectiveAssignment =
		findManagedTeamObjectiveAssignment(actor);
	world::WorldSnapshot snapshot;
	world::ActorKey targetActor = {};
	world::WorldPosition targetPosition = {};
	if (!buildManagedWorldSnapshot(index, &snapshot, &targetActor, &targetPosition))
	{
		managedBotObjectiveActions_[index].active = false;
		(void)cancelHeldGrenade();
		if ((adapterFrameCount_ % 16U) == 0U && gpMetaUtilFuncs != nullptr &&
			gpMetaUtilFuncs->pfnLogConsole != nullptr && pluginId_ != nullptr)
		{
			profiledLogConsole(runtimeProfiler_,
				pluginId_, "action sensor slot=%u frame=%u team=%d teamInfo=%u target=0",
				static_cast<unsigned int>(handle.actor.slot),
				static_cast<unsigned int>(adapterFrameCount_), static_cast<int>(handle.entity->v.team),
				static_cast<unsigned int>(managedBotTeamNumbers_[index]));
		}
		return proposal;
	}
	const int team = resolveManagedBotTeam(index, compatObservation);
	const objectives::TeamRole teamRole =
		team == 1 ? objectives::TeamRole::Terrorist :
		team == 2 ? objectives::TeamRole::CounterTerrorist : objectives::TeamRole::Unknown;
	if (!targetActor.isValid() && compatibilitySurface_.configuration().mode == compat::RuntimeMode::Compatibility)
	{
		if (cancelHeldGrenade())
			return proposal;
		const auto &scenario = scenarioDecisions_[index];
		if (!scenario.unavailable)
		{
			if (scenario.action == compat::ScenarioAction::Stop ||
				(scenario.action == compat::ScenarioAction::Guard && scenario.hasTarget && scenario.target.inUseRange))
			{
				proposal.stopForWeapon = true;
				return proposal;
			}
			if (scenario.action == compat::ScenarioAction::Use && scenario.hasTarget &&
				scenario.target.entity.isValid() && engineFunctions_ != nullptr &&
				engineFunctions_->pfnPEntityOfEntIndex != nullptr)
			{
				edict_t *useEntity = engineFunctions_->pfnPEntityOfEntIndex(static_cast<int>(scenario.target.entity.id));
				if (useEntity != nullptr && !useEntity->free &&
					static_cast<std::uint32_t>(useEntity->serialnumber) + 1U == scenario.target.entity.generation)
				{
					const float dx = scenario.target.position.x - handle.entity->v.origin.x;
					const float dy = scenario.target.position.y - handle.entity->v.origin.y;
					proposal.kind = ActionKind::UseEntity;
					proposal.viewAngles.yaw = std::atan2(dy, dx) * 180.0f / 3.14159265358979323846f;
					return proposal;
				}
			}
		}
	}
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
		std::uint32_t grenadeEntityCount = 0U;
		std::uint32_t c4ModelEntityCount = 0U;
		int firstGrenadeEntityIndex = 0;
	int firstC4ModelEntityIndex = 0;
		const char *firstGrenadeClassname = "(none)";
		const char *firstGrenadeModel = "(none)";
	const char *firstC4ModelClassname = "(none)";
	const char *firstC4ModelName = "(none)";
		float firstGrenadeDamageTime = 0.0f;
	float firstC4ModelDamageTime = 0.0f;
	int firstGrenadeObservationResult = -1;
	int firstC4ModelObservationResult = -1;
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
			const bool grenadeEntity = equalsIgnoreCase(classname, "grenade");
			const bool c4Model = model != nullptr &&
				std::strstr(model, "w_c4.mdl") != nullptr;
			if (grenadeEntity)
			{
				++grenadeEntityCount;
			}
			if (c4Model)
			{
				++c4ModelEntityCount;
			}
			ObservationAdapterResult plantedResult =
				ObservationAdapterResult::InvalidEntity;
			if (plantedBomb == nullptr)
			{
				plantedResult = observationAdapter_.collectPlantedBomb(
					entity, classname, model, globals_->time, actor, frame,
					observationTiming, &plantedObservation);
			}
			if (grenadeEntity && firstGrenadeEntityIndex == 0)
			{
				firstGrenadeEntityIndex = entityIndex;
				firstGrenadeClassname = classname != nullptr ? classname : "(null)";
				firstGrenadeModel = model != nullptr ? model : "(null)";
				firstGrenadeDamageTime = entity->v.dmgtime;
				firstGrenadeObservationResult = static_cast<int>(plantedResult);
			}
			if (c4Model && firstC4ModelEntityIndex == 0)
			{
				firstC4ModelEntityIndex = entityIndex;
				firstC4ModelClassname = classname != nullptr ? classname : "(null)";
				firstC4ModelName = model != nullptr ? model : "(null)";
				firstC4ModelDamageTime = entity->v.dmgtime;
				firstC4ModelObservationResult = static_cast<int>(plantedResult);
			}
		if (plantedResult == ObservationAdapterResult::Accepted)
		{
			world::WorldVector knownPosition{};
			if (observeObjective(index, entity, true, &knownPosition))
			{
				plantedBomb = entity;
				break;
			}
		}
	}
		if (plantedBomb == nullptr && teamRole == objectives::TeamRole::CounterTerrorist &&
			runtimeProfiler_.enabled() && (adapterFrameCount_ % 64U) == 0U &&
			gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr &&
			pluginId_ != nullptr)
		{
			profiledLogConsole(runtimeProfiler_,
				pluginId_,
				"profile plantedBombScan actor=%u actor_generation=%u map=%u round=%u "
				"frame=%u now=%.2f entities=%d grenade_count=%u "
				"first_grenade=(%d %s %s dmgtime=%.2f result=%d) "
				"c4_model_count=%u first_c4_model=(%d %s %s dmgtime=%.2f result=%d)",
				static_cast<unsigned int>(handle.actor.slot),
				static_cast<unsigned int>(handle.actor.actorGeneration),
				static_cast<unsigned int>(lifecycle_.mapGeneration()),
				static_cast<unsigned int>(lifecycle_.roundGeneration()),
				static_cast<unsigned int>(adapterFrameCount_), globals_->time,
				maxEntities, grenadeEntityCount, firstGrenadeEntityIndex,
				firstGrenadeClassname, firstGrenadeModel, firstGrenadeDamageTime,
				firstGrenadeObservationResult, c4ModelEntityCount,
				firstC4ModelEntityIndex, firstC4ModelClassname, firstC4ModelName,
				firstC4ModelDamageTime, firstC4ModelObservationResult);
		}
		const bool carryingBomb = compatObservation.objective.carryingC4.isAvailable() &&
			compatObservation.objective.carryingC4.value;
		ManagedObjectiveActionState &plantState = managedBotObjectiveActions_[index];
		if (plantedBomb != nullptr && plantState.active &&
			plantState.actor == actor &&
			plantState.kind == team::TeamObjectiveKind::PlantC4)
		{
			(void)ActionAdapter::evaluatePlantAttempt(
				plantState.plantAttempt, false, true,
				globals_ != nullptr ? globals_->time
					: std::numeric_limits<float>::quiet_NaN());
			if (runtimeProfiler_.enabled() && gpMetaUtilFuncs != nullptr &&
				gpMetaUtilFuncs->pfnLogConsole != nullptr && pluginId_ != nullptr)
			{
				const int plantedEntityIndex = engineFunctions_ != nullptr &&
					engineFunctions_->pfnIndexOfEdict != nullptr
					? engineFunctions_->pfnIndexOfEdict(plantedBomb) : 0;
				profiledLogConsole(runtimeProfiler_,
					pluginId_,
					"profile plantConfirm actor=%u actor_generation=%u map=%u round=%u "
					"frame=%u assignment=%u site=%d site_id=%u planted_entity=%d "
					"source=public_planted_c4_observation",
					static_cast<unsigned int>(handle.actor.slot),
					static_cast<unsigned int>(handle.actor.actorGeneration),
					static_cast<unsigned int>(lifecycle_.mapGeneration()),
					static_cast<unsigned int>(lifecycle_.roundGeneration()),
					static_cast<unsigned int>(adapterFrameCount_),
					plantState.assignmentGeneration, plantState.targetEntityIndex,
					plantState.targetSiteIdentity, plantedEntityIndex);
			}
			plantState = {};
		}
		if ((adapterFrameCount_ % 16U) == 0U && gpMetaUtilFuncs != nullptr &&
				gpMetaUtilFuncs->pfnLogConsole != nullptr && pluginId_ != nullptr)
		{
			profiledLogConsole(runtimeProfiler_,
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
	const bool hasPlantAssignment = objectiveAssignment != nullptr &&
		objectiveAssignment->kind == team::TeamObjectiveKind::PlantC4;
	nav::NavVector selectedPlantTarget{};
	const bool haveSelectedPlantTarget = hasPlantAssignment && carryingBomb &&
		teamRole == objectives::TeamRole::Terrorist &&
		buildManagedObjectiveTarget(index, &selectedPlantTarget, nullptr);
	objectives::ObjectiveKind objectiveKind = objectives::ObjectiveKind::None;
	if (objectiveResult == objectives::ObjectiveResult::Proposed &&
		objective.objective.kind != objectives::ObjectiveKind::Plant &&
		objective.objective.kind != objectives::ObjectiveKind::Defuse)
	{
		objectiveKind = objective.objective.kind;
	}
	if (ActionAdapter::canBeginPlantObjective(
		hasPlantAssignment, carryingBomb,
		teamRole == objectives::TeamRole::Terrorist,
		haveSelectedPlantTarget && managedBombApproaches_[index].finished))
	{
		objectiveKind = objectives::ObjectiveKind::Plant;
	}
	else if (objectiveAssignment != nullptr &&
		objectiveAssignment->kind == team::TeamObjectiveKind::DefuseC4 &&
		plantedBomb != nullptr &&
		teamRole == objectives::TeamRole::CounterTerrorist)
	{
		objectiveKind = objectives::ObjectiveKind::Defuse;
	}
	else if (objectiveAssignment == nullptr ||
		(objectiveAssignment->kind != team::TeamObjectiveKind::PlantC4 &&
		 objectiveAssignment->kind != team::TeamObjectiveKind::DefuseC4))
	{
		objectiveKind = objectives::ObjectiveKind::None;
	}
	// MoveTo owns the approach. A legacy wider action radius must not stop
	// an actor before its observed scenario has entered the defuse state.
	if (objectiveKind == objectives::ObjectiveKind::Defuse &&
		compatibilitySurface_.configuration().mode == compat::RuntimeMode::Compatibility &&
		managedBotStateMachines_[index].state() != compat::CompatibilityStateId::DefuseBomb)
		objectiveKind = objectives::ObjectiveKind::None;
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
				else
				{
					objectivePosition = selectedPlantTarget;
					haveObjectivePosition = haveSelectedPlantTarget;
				}
				if (haveObjectivePosition)
				{
					const float dx = objectivePosition.x - handle.entity->v.origin.x;
					const float dy = objectivePosition.y - handle.entity->v.origin.y;
					const float dz = objectivePosition.z - handle.entity->v.origin.z;
		if ((objectiveKind==objectives::ObjectiveKind::Defuse &&
			compatibilitySurface_.configuration().mode==compat::RuntimeMode::Compatibility &&
			managedBotStateMachines_[index].state()==compat::CompatibilityStateId::DefuseBomb) ||
			dx * dx + dy * dy + dz * dz <= 96.0f * 96.0f)
		{
			ManagedObjectiveActionState &actionState =
				managedBotObjectiveActions_[index];
			const bool planting =
				objectiveAssignment->kind == team::TeamObjectiveKind::PlantC4;
			const int targetEntityIndex = planting
				? managedBotObjectiveTargets_[index].selectedEntityIndex
				: static_cast<int>(objectiveAssignment->targetEntity.id);
			const std::uint32_t targetIdentity = planting
				? managedBotObjectiveTargets_[index].selectedSiteIdentity
				: objectiveAssignment->targetEntity.generation;
			const bool sameAction = actionState.active &&
				actionState.actor == actor &&
				actionState.kind == objectiveAssignment->kind &&
				actionState.mapGeneration == lifecycle_.mapGeneration() &&
				actionState.roundGeneration == lifecycle_.roundGeneration() &&
				actionState.assignmentGeneration == objectiveAssignment->generation &&
				actionState.targetEntityIndex == targetEntityIndex &&
				actionState.targetSiteIdentity == targetIdentity;
			if (!sameAction)
			{
				actionState = {};
				actionState.active = true;
				actionState.actor = actor;
				actionState.kind = objectiveAssignment->kind;
				actionState.mapGeneration = lifecycle_.mapGeneration();
				actionState.roundGeneration = lifecycle_.roundGeneration();
				actionState.assignmentGeneration =
					objectiveAssignment->generation;
				actionState.targetEntityIndex = targetEntityIndex;
				actionState.targetSiteIdentity = targetIdentity;
			}
			if (planting)
			{
				preserveObjectiveActionState = true;
				PlantTargetBounds siteBounds{};
				PlantTargetBounds actorBounds{};
				bool siteBoundsAvailable = false;
				bool legacyInfoTarget = false;
				PlantTargetPoint legacyInfoTargetCenter{};
				if (targetEntityIndex > 0 && engineFunctions_ != nullptr &&
					engineFunctions_->pfnPEntityOfEntIndex != nullptr &&
					engineFunctions_->pfnSzFromIndex != nullptr)
				{
					edict_t *site = engineFunctions_->pfnPEntityOfEntIndex(targetEntityIndex);
					const char *siteClassname = site != nullptr && !site->free
						? engineFunctions_->pfnSzFromIndex(site->v.classname)
						: nullptr;
					if (site != nullptr && !site->free &&
						isBombTargetClassname(siteClassname))
					{
						legacyInfoTarget = equalsIgnoreCase(
							siteClassname, "info_bomb_target");
						if (legacyInfoTarget)
						{
							const nav::NavVector center = entityObjectiveCenter(site);
						legacyInfoTargetCenter = {center.x, center.y, center.z};
						const float radius = ActionAdapter::kLegacyBombTargetRadius;
						siteBounds = {
							center.x - radius, center.y - radius, center.z - radius,
							center.x + radius, center.y + radius, center.z + radius};
						siteBoundsAvailable =
							ActionAdapter::validPlantTargetBounds(siteBounds);
						}
						else
						{
							siteBounds = {
								site->v.absmin.x, site->v.absmin.y, site->v.absmin.z,
								site->v.absmax.x, site->v.absmax.y, site->v.absmax.z};
							siteBoundsAvailable =
								ActionAdapter::validPlantTargetBounds(siteBounds) &&
								siteBounds.maximumX > siteBounds.minimumX &&
								siteBounds.maximumY > siteBounds.minimumY;
						}
					}
				}
				actorBounds = {
					handle.entity->v.absmin.x, handle.entity->v.absmin.y,
					handle.entity->v.absmin.z, handle.entity->v.absmax.x,
					handle.entity->v.absmax.y, handle.entity->v.absmax.z};
				const PlantTargetPoint actorPosition = {
					handle.entity->v.origin.x,
					handle.entity->v.origin.y,
					handle.entity->v.origin.z};
				const bool actorInSite = siteBoundsAvailable &&
					(legacyInfoTarget
						? ActionAdapter::withinLegacyBombTargetRadius(
							legacyInfoTargetCenter, actorPosition)
						: ActionAdapter::overlapsPlantTarget(siteBounds, actorBounds));
				const float now = globals_ != nullptr ? globals_->time
					: std::numeric_limits<float>::quiet_NaN();
				const PlantAttemptResult attempt = ActionAdapter::evaluatePlantAttempt(
					actionState.plantAttempt, actorInSite,
					plantedBomb != nullptr, now);
				if (attempt == PlantAttemptResult::TimedOut)
				{
					actionState.c4SelectionCommandDispatched = false;
					actionState.c4AttackInputDispatched = false;
					managedBotObjectiveTargets_[index].valid = false;
					managedBotObjectiveTargets_[index].diagnosticsEmitted = false;
					managedBotMovement_[index].reset();
				}
				const ManagedCurrentWeaponObservation &weapon =
					managedBotCurrentWeapons_[index];
				const bool weaponObservationCurrent = weapon.available &&
					weapon.actorGeneration == handle.actor.actorGeneration &&
					weapon.mapGeneration == lifecycle_.mapGeneration() &&
					weapon.roundGeneration == lifecycle_.roundGeneration();
				const bool canContinuePlant = attempt == PlantAttemptResult::Ready ||
					attempt == PlantAttemptResult::Continuing;
				if (runtimeProfiler_.enabled() && (adapterFrameCount_ % 16U) == 0U &&
					gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr &&
					pluginId_ != nullptr)
				{
					profiledLogConsole(runtimeProfiler_,
						pluginId_,
						"profile plantGate actor=%u actor_generation=%u map=%u round=%u "
						"frame=%u assignment=%u site=%d site_id=%u bounds_available=%d "
						"zone_mode=%s "
						"actor_in_site=%d gate=%d site_bounds=(%.1f %.1f %.1f %.1f %.1f %.1f) "
						"actor_bounds=(%.1f %.1f %.1f %.1f %.1f %.1f) "
						"nav_target=(%.1f %.1f %.1f) actor_origin=(%.1f %.1f %.1f) "
						"weapon_known=%d weapon_active=%d weapon_id=%u",
						static_cast<unsigned int>(handle.actor.slot),
						static_cast<unsigned int>(handle.actor.actorGeneration),
						static_cast<unsigned int>(lifecycle_.mapGeneration()),
						static_cast<unsigned int>(lifecycle_.roundGeneration()),
					static_cast<unsigned int>(adapterFrameCount_),
					objectiveAssignment->generation, targetEntityIndex, targetIdentity,
					siteBoundsAvailable ? 1 : 0,
					legacyInfoTarget ? "legacy_info_radius" : "func_bounds",
					actorInSite ? 1 : 0,
						static_cast<int>(attempt),
						siteBounds.minimumX, siteBounds.minimumY, siteBounds.minimumZ,
						siteBounds.maximumX, siteBounds.maximumY, siteBounds.maximumZ,
						actorBounds.minimumX, actorBounds.minimumY, actorBounds.minimumZ,
						actorBounds.maximumX, actorBounds.maximumY, actorBounds.maximumZ,
						objectivePosition.x, objectivePosition.y, objectivePosition.z,
						handle.entity->v.origin.x, handle.entity->v.origin.y,
						handle.entity->v.origin.z,
						weaponObservationCurrent ? 1 : 0,
						weaponObservationCurrent && weapon.active ? 1 : 0,
						static_cast<unsigned int>(weapon.weaponId));
				}
				if (!canContinuePlant)
				{
					return proposal;
				}
				proposal.kind = ActionAdapter::plantActionForWeaponObservation(
					weaponObservationCurrent,
					weaponObservationCurrent && weapon.active,
					weapon.weaponId, actionState.c4AttackInputDispatched);
			}
			else
			{
				proposal.kind = ActionKind::Defuse;
				if(compatibilitySurface_.configuration().mode==compat::RuntimeMode::Compatibility)
					proposal.movementButtons=static_cast<std::uint16_t>((proposal.movementButtons &
						~(IN_JUMP|IN_FORWARD|IN_BACK|IN_MOVELEFT|IN_MOVERIGHT))|IN_DUCK);
			}
			proposal.viewAngles = {
				-kRadiansToDegrees * std::atan2(dz, std::sqrt(dx * dx + dy * dy)),
				kRadiansToDegrees * std::atan2(dy, dx), movementAngles.roll};
			objectiveActionDispatched = true;
			return proposal;
		}
				}
			}
		}
	}
	if (!objectiveActionDispatched)
	{
		if (!preserveObjectiveActionState)
		{
			managedBotObjectiveActions_[index].active = false;
		}
	}
	combat::WeaponInventory inventory;
	combat::WeaponRecord weapon = {};
	compat::PublicActorState observedWeapon{};
	if (!publicObservations_.read(actor, frame, &observedWeapon) || !observedWeapon.weaponKnown)
		return proposal;
	const int weaponValue = observedWeapon.weaponId;
	weapon.weapon = {static_cast<std::uint16_t>(weaponValue)};
	const compat::PublicWeaponDefinition definition = publicObservations_.definition(weaponValue);
	weapon.slot = static_cast<std::uint8_t>(definition.known ? definition.slot : 0);
	switch (weaponValue)
	{
	case 1: case 10: case 11: case 16: case 17: case 26:
		weapon.kind = combat::WeaponKind::Pistol;
		break;
	case 5: case 21:
		weapon.kind = combat::WeaponKind::Shotgun;
		break;
	case 7: case 12: case 19: case 23: case 30:
		weapon.kind = combat::WeaponKind::Smg;
		break;
	case 3: case 13: case 18: case 24:
		weapon.kind = combat::WeaponKind::Sniper;
		break;
	case 20:
		weapon.kind = combat::WeaponKind::Heavy;
		break;
	case 29:
		weapon.kind = combat::WeaponKind::Knife;
		break;
	case 4: case 9: case 25:
		weapon.kind = combat::WeaponKind::Grenade;
		break;
	case 8: case 14: case 15: case 22: case 27: case 28:
		weapon.kind = combat::WeaponKind::Rifle;
		break;
	default:
		return proposal;
	}
	weapon.availability = combat::WeaponAvailability::Available;
	weapon.priority = 1U;
	const bool knife = weapon.kind == combat::WeaponKind::Knife;
	const bool reserveKnown = definition.known && definition.ammoIndex >= 0 &&
		observedWeapon.ammoKnown[static_cast<std::size_t>(definition.ammoIndex)];
	const std::uint16_t reserve = reserveKnown
		? static_cast<std::uint16_t>(observedWeapon.ammo[static_cast<std::size_t>(definition.ammoIndex)]) : 0U;
	weapon.ammo = {static_cast<std::uint16_t>(knife ? 1 : (std::max)(0, observedWeapon.clip)), reserve, 255U,
		knife || observedWeapon.clip >= 0, reserveKnown};
	weapon.reloadState = compatObservation.weapon.reloadState.isAvailable()
		? compatObservation.weapon.reloadState.value : combat::ReloadState::Unknown;
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
	if (engineFunctions_->pfnTraceLine != nullptr)
	{
		const float eye[] = {handle.entity->v.origin.x + handle.entity->v.view_ofs.x,
			handle.entity->v.origin.y + handle.entity->v.view_ofs.y,
			handle.entity->v.origin.z + handle.entity->v.view_ofs.z};
		const float aim[] = {targetPosition.x, targetPosition.y, targetPosition.z};
		TraceResult trace{};
		engineFunctions_->pfnTraceLine(eye, aim, 0, handle.entity, &trace);
		if (trace.pHit != nullptr && trace.pHit != handle.entity && trace.pHit->v.deadflag == DEAD_NO)
		{
			int hitTeam = static_cast<int>(trace.pHit->v.team);
			for (std::size_t slot = 0U; slot < managedBotHandles_.size(); ++slot)
				if (managedBotHandles_[slot].entity == trace.pHit && managedBotTeamNumbers_[slot] != 0U)
					hitTeam = managedBotTeamNumbers_[slot];
			target.friendlyFireRisk = hitTeam == team && hitTeam != 0;
		}
	}
	combat::CombatDecisionContext context = {};
	context.actor = {handle.actor.slot, handle.actor.actorGeneration};
	context.frame = snapshot.identity().frame;
	context.origin = {
		handle.entity->v.origin.x + handle.entity->v.view_ofs.x,
		handle.entity->v.origin.y + handle.entity->v.view_ofs.y,
		handle.entity->v.origin.z + handle.entity->v.view_ofs.z};
	context.aimTrackingEnabled = true;
	context.currentPitch = handle.entity->v.v_angle.x;
	context.currentYaw = handle.entity->v.v_angle.y;
	context.elapsedSeconds = lastCombatTimes_[index] > 0.0f
		? (std::max)(0.0f, (std::min)(0.1f, globals_->time - lastCombatTimes_[index])) : 1.0f / 30.0f;
	context.timestamp = globals_->time;
	context.skill = static_cast<float>(managedProfiles_[index].skill) / 100.0f;
	context.fieldOfView = handle.entity->v.fov > 0.0f ? handle.entity->v.fov : 90.0f;
	context.randomSource = &compatibilityRandomSource_;
	context.randomTiming = {managedBotCommandSequences_[index], 0U, managedBotFullUpdateSequences_[index]};
	lastCombatTimes_[index] = globals_->time;
	combat::CombatDecision decision = {};
	const combat::CombatResult result = managedBotCombat_[index].decide(
		snapshot, inventory, target, context, &decision);
	const bool fireIntentReady =
		result == combat::CombatResult::FireIntentReady &&
		decision.hasFire && decision.hasAim && globals_->time >= attackReadyTimes_[index];
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
		profiledLogConsole(runtimeProfiler_,
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
	if (decision.hasAim)
	{
		proposal.viewAngles = {decision.aim.pitch, decision.aim.yaw, movementAngles.roll};
	}
	compat::WeaponActionInput weaponInput{};
	weaponInput.actor = actor;
	weaponInput.frame = frame;
	weaponInput.timestamp = globals_->time;
	weaponInput.weaponId = static_cast<std::uint16_t>(weaponValue);
	weaponInput.clipKnown = observedWeapon.clip >= 0;
	weaponInput.clip = static_cast<std::uint16_t>((std::max)(0, observedWeapon.clip));
	weaponInput.fovKnown = std::isfinite(handle.entity->v.fov);
	weaponInput.fieldOfView = handle.entity->v.fov > 0.0f ? handle.entity->v.fov : 90.0f;
	weaponInput.targetDistance =
		std::sqrt((targetPosition.x - context.origin.x) * (targetPosition.x - context.origin.x) +
				  (targetPosition.y - context.origin.y) * (targetPosition.y - context.origin.y) +
				  (targetPosition.z - context.origin.z) * (targetPosition.z - context.origin.z));
	weaponInput.reactionSeconds = managedProfiles_[index].reactionTimeSeconds;
	weaponInput.targetVisible = targetActor.isValid();
	const float weaponDx = targetPosition.x - context.origin.x;
	const float weaponDy = targetPosition.y - context.origin.y;
	const float weaponDz = targetPosition.z - context.origin.z;
	const float desiredYaw = std::atan2(weaponDy, weaponDx) * 180.0f / 3.14159265358979323846f;
	const float desiredPitch =
		-std::atan2(weaponDz, std::hypot(weaponDx, weaponDy)) * 180.0f / 3.14159265358979323846f;
	weaponInput.aimAligned =
		fireIntentReady ||
		(decision.hasAim &&
		 std::fabs(compat::normalizedAimAngle(desiredYaw - handle.entity->v.v_angle.y)) <= 1.0f &&
		 std::fabs(compat::normalizedAimAngle(desiredPitch - handle.entity->v.v_angle.x)) <= 1.0f);
	weaponInput.stationary =
		std::hypot(handle.entity->v.velocity.x, handle.entity->v.velocity.y) < 10.0f;
	weaponInput.requestFire = weaponInput.targetVisible && weaponInput.aimAligned &&
							  !target.friendlyFireRisk &&
							  globals_->time >= attackReadyTimes_[index];
	weaponInput.attacking = managedBotStateMachines_[index].attackOverlayActive();
	compat::CompatibilityObservation publicWeapon = {};
	const compat::ObservationTimingContext weaponTiming = {managedBotCommandSequences_[index], 0U,
														   managedBotFullUpdateSequences_[index]};
	if (observationAdapter_.collectActor(handle.entity, actor, frame, weaponTiming,
										 &publicWeapon) == ObservationAdapterResult::Accepted)
	{
		if (publicWeapon.weapon.silencer.isAvailable())
			weaponInput.silenced = {true, publicWeapon.weapon.silencer.value};
		if (publicWeapon.weapon.reserve.isAvailable() &&
			publicWeapon.weapon.reserve.context.delayTicks <= frame.tick)
		{
			weaponInput.reserveKnown = true;
			weaponInput.reserve = publicWeapon.weapon.reserve.value;
			weaponInput.reserveObservationTick = frame.tick - publicWeapon.weapon.reserve.context.delayTicks;
		}
		if (publicWeapon.weapon.burst.isAvailable())
			weaponInput.burstMode = {true, publicWeapon.weapon.burst.value};
		if (publicWeapon.weapon.reloadState.isAvailable() &&
			publicWeapon.weapon.reloadState.value != combat::ReloadState::Unknown)
			weaponInput.reloading = {true, publicWeapon.weapon.reloadState.value ==
											   combat::ReloadState::Reloading};
		if (publicWeapon.weapon.nextPrimaryAttack.isAvailable() &&
			publicWeapon.weapon.nextPrimaryAttack.value > globals_->time)
			weaponInput.requestFire = false;
		if (publicWeapon.weapon.nextSecondaryAttack.isAvailable())
			weaponInput.secondaryReady = {true, publicWeapon.weapon.nextSecondaryAttack.value <= globals_->time};
	}
	const std::uint32_t inventoryMask = static_cast<std::uint32_t>(handle.entity->v.weapons);
	bool havePistol = false;
	for (const unsigned id : {1U, 10U, 11U, 16U, 17U, 26U})
		havePistol = havePistol || (inventoryMask & (1U << id)) != 0U;
	if (weaponValue == 29 && weaponInput.requestFire && weaponInput.targetDistance < 75.0f)
	{
		edict_t *enemy =
			engineFunctions_->pfnPEntityOfEntIndex != nullptr
				? engineFunctions_->pfnPEntityOfEntIndex(static_cast<int>(targetActor.slot))
				: nullptr;
		if (enemy != nullptr && enemy->free == 0)
		{
			const float toUsX = handle.entity->v.origin.x - enemy->v.origin.x;
			const float toUsY = handle.entity->v.origin.y - enemy->v.origin.y;
			const float pitchDegrees = enemy->v.v_angle.x + enemy->v.punchangle.x;
			const float yawDegrees = enemy->v.v_angle.y + enemy->v.punchangle.y;
			const float rollDegrees = enemy->v.v_angle.z + enemy->v.punchangle.z;
			if (std::isfinite(pitchDegrees) && std::isfinite(yawDegrees) &&
				std::isfinite(rollDegrees) && std::isfinite(toUsX) && std::isfinite(toUsY))
			{
				const float yaw = yawDegrees * 3.14159265358979323846f / 180.0f;
				const float pitch = pitchDegrees * 3.14159265358979323846f / 180.0f;
				weaponInput.enemyFacingObserver = {
					true, std::cos(pitch) * (toUsX * std::cos(yaw) + toUsY * std::sin(yaw)) > 0.0f};
			}
		}
		if (!weaponInput.enemyFacingObserver.known || weaponInput.enemyFacingObserver.value)
		{
			const auto choice =
				compatibilityRandomSource_.nextFloat(compat::RandomRequest::floatRequest(
					"Knife.AttackChoice", {actor.slot, actor.generation},
					{managedBotCommandSequences_[index], adapterFrameCount_,
					 managedBotFullUpdateSequences_[index]},
					0.0f, 100.0f));
			weaponInput.knifeRollKnown = choice.status == compat::RandomStatus::Ok;
			weaponInput.knifeRoll = choice.value;
		}
	}
	if (compat::WeaponActionPlanner::isGrenade(static_cast<std::uint16_t>(weaponValue)) &&
		weaponInput.targetVisible && !target.friendlyFireRisk &&
		engineFunctions_->pfnCVarGetFloat != nullptr && engineFunctions_->pfnTraceLine != nullptr &&
		engineFunctions_->pfnTraceHull != nullptr)
	{
		const auto launch = compat::solveGrenadeLaunch(
			context.origin,
			{handle.entity->v.velocity.x, handle.entity->v.velocity.y, handle.entity->v.velocity.z},
			targetPosition, engineFunctions_->pfnCVarGetFloat("sv_gravity"), 16.0f);
		if (launch.valid)
		{
			edict_t *intendedTarget =
				engineFunctions_->pfnPEntityOfEntIndex != nullptr
					? engineFunctions_->pfnPEntityOfEntIndex(static_cast<int>(targetActor.slot))
					: nullptr;
			bool lineClear = true, hullClear = true;
			world::WorldPosition previous = context.origin;
			const unsigned steps = static_cast<unsigned>(std::ceil(launch.flightSeconds / 0.05f));
			for (unsigned step = 0U; step <= steps && lineClear && hullClear; ++step)
			{
				const auto next =
					step == 0U ? launch.origin
							   : compat::grenadePosition(launch, launch.flightSeconds *
																	 static_cast<float>(step) /
																	 static_cast<float>(steps));
				const float start[] = {previous.x, previous.y, previous.z};
				const float end[] = {next.x, next.y, next.z};
				TraceResult line = {}, hull = {};
				engineFunctions_->pfnTraceLine(start, end, 0, handle.entity, &line);
				engineFunctions_->pfnTraceHull(start, end, 0, 3, handle.entity, &hull);
				lineClear = (line.flFraction >= 1.0f ||
							 (intendedTarget != nullptr && line.pHit == intendedTarget)) &&
							line.fStartSolid == 0 && line.fAllSolid == 0;
				hullClear = (hull.flFraction >= 1.0f ||
							 (intendedTarget != nullptr && hull.pHit == intendedTarget)) &&
							hull.fStartSolid == 0 && hull.fAllSolid == 0;
				previous = next;
				if (lineClear && hullClear && intendedTarget != nullptr &&
					(line.pHit == intendedTarget || hull.pHit == intendedTarget))
					break;
			}
			const auto landing = compat::grenadePosition(launch, launch.flightSeconds);
			const float groundStart[] = {landing.x, landing.y, landing.z};
			const float groundEnd[] = {landing.x, landing.y, landing.z - 72.0f};
			TraceResult ground = {};
			engineFunctions_->pfnTraceHull(
				groundStart, groundEnd, 0, 3,
				intendedTarget != nullptr ? intendedTarget : handle.entity, &ground);
			const bool landingSafe = ground.flFraction < 1.0f && ground.fStartSolid == 0 &&
									 ground.fAllSolid == 0 && ground.vecPlaneNormal.z >= 0.7f;
			weaponInput.trajectory = {true, lineClear, hullClear, landingSafe};
			weaponInput.grenadeAimKnown = lineClear && hullClear && landingSafe;
			weaponInput.grenadePitch = launch.pitch - handle.entity->v.punchangle.x;
			weaponInput.grenadeYaw = launch.yaw - handle.entity->v.punchangle.y;
			weaponInput.requestGrenade =
				lineClear && hullClear && landingSafe && globals_->time >= attackReadyTimes_[index];
			weaponInput.aimAligned =
				std::fabs(compat::normalizedAimAngle(launch.yaw - handle.entity->v.v_angle.y -
													 handle.entity->v.punchangle.y)) <= 1.0f &&
				std::fabs(compat::normalizedAimAngle(launch.pitch - handle.entity->v.v_angle.x -
													 handle.entity->v.punchangle.x)) <= 1.0f;
			if (weaponInput.requestGrenade)
				proposal.viewAngles = {launch.pitch - handle.entity->v.punchangle.x,
									   launch.yaw - handle.entity->v.punchangle.y,
									   movementAngles.roll};
		}
	}
	const compat::WeaponActionDecision weaponDecision = weaponActions_[index].update(weaponInput);
	if (proposal.kind == ActionKind::Fire && !weaponDecision.primary)
	{
		proposal.kind = ActionKind::None;
	}
	if (weaponDecision.primary)
		proposal.kind = ActionKind::Fire;
	if (weaponDecision.switchToPistol && havePistol)
		proposal.kind = ActionKind::SelectPistol;
	proposal.secondaryAttack = weaponDecision.secondary;
	proposal.stopForWeapon = weaponDecision.stopMovement;
	if (weaponDecision.hasRetainedGrenadeAim)
		proposal.viewAngles = {weaponDecision.grenadePitch,weaponDecision.grenadeYaw,movementAngles.roll};
	return proposal;
}

void PluginRuntime::resetManagedBotTiming(std::size_t index, float spawnTime)
{
	if (index >= managedBotTiming_.size())
	{
		return;
	}
	managedBotTiming_[index].reset(spawnTime);
	managedBotJumpCrouch_[index].reset();
	resetManagedBotCommandTemplate(index);
}

bool PluginRuntime::dispatchManagedActionCommand(
	std::size_t index,
	FakeClientHandle &handle,
	const ActionProposal &proposal,
	const ActionDispatch &dispatch)
{
	if (dispatch.clientCommand == nullptr)
	{
		return true;
	}
	const bool commandDispatched = dispatchClientCommand(
		handle.entity, dispatch.clientCommand, "");
	if (commandDispatched && proposal.kind == ActionKind::Reload && globals_ != nullptr)
	{
		publicObservations_.requestReload({handle.actor.slot, handle.actor.actorGeneration},
			{lifecycle_.mapGeneration(), lifecycle_.roundGeneration(), adapterFrameCount_}, globals_->time);
	}
	const bool c4SelectionRequest = proposal.kind == ActionKind::SelectC4 ||
		proposal.kind == ActionKind::Plant ||
		proposal.kind == ActionKind::PlantContinue;
	if (commandDispatched && c4SelectionRequest &&
		index < managedBotObjectiveActions_.size())
	{
		ManagedObjectiveActionState &state = managedBotObjectiveActions_[index];
		if (state.active && state.actor.slot == handle.actor.slot &&
			state.actor.generation == handle.actor.actorGeneration &&
			state.kind == team::TeamObjectiveKind::PlantC4)
		{
			state.c4SelectionCommandDispatched = true;
			ActionAdapter::recordPlantAttemptDispatched(
				state.plantAttempt,
				globals_ != nullptr ? globals_->time
					: std::numeric_limits<float>::quiet_NaN());
		}
	}
	if (c4SelectionRequest && runtimeProfiler_.enabled() &&
		(adapterFrameCount_ % 16U) == 0U &&
		gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr &&
		pluginId_ != nullptr)
	{
		const ManagedCurrentWeaponObservation &weapon =
			managedBotCurrentWeapons_[index];
		profiledLogConsole(runtimeProfiler_,
			pluginId_,
			"profile plantCommand actor=%u actor_generation=%u map=%u round=%u "
			"frame=%u action=%s command=%s dispatched=%d current_weapon_known=%d "
			"current_weapon_active=%d current_weapon_id=%u active_c4=%d",
			static_cast<unsigned int>(handle.actor.slot),
			static_cast<unsigned int>(handle.actor.actorGeneration),
			static_cast<unsigned int>(lifecycle_.mapGeneration()),
			static_cast<unsigned int>(lifecycle_.roundGeneration()),
			static_cast<unsigned int>(adapterFrameCount_), actionKindName(proposal.kind),
			dispatch.clientCommand, commandDispatched ? 1 : 0,
			weapon.available ? 1 : 0, weapon.active ? 1 : 0,
			static_cast<unsigned int>(weapon.weaponId),
			weapon.available && weapon.active &&
				weapon.weaponId == ActionAdapter::kC4WeaponId ? 1 : 0);
	}
	return commandDispatched;
}

void PluginRuntime::resetManagedBotCommandTemplate(std::size_t index)
{
	if (index >= managedBotCommandTemplates_.size())
	{
		return;
	}
	managedBotCommandTemplates_[index] = {};
	managedBotCommandTemplateValid_[index] = false;
	managedBotActionStopsMovement_[index] = false;
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
		hasObservedActiveWeapon(index),
		command.viewAngles);
	const ActionDispatch actionDispatch = ActionAdapter::translate(actionProposal);
	command.viewAngles = actionDispatch.viewAngles;
	command.movement.buttons = actionDispatch.buttons;
	if (actionDispatch.clientCommand != nullptr)
	{
		(void)dispatchManagedActionCommand(
			index, handle, actionProposal, actionDispatch);
	}
	managedBotCommandTemplates_[index] = command;
	managedBotCommandTemplateValid_[index] = true;
	managedBotActionStopsMovement_[index] = actionDispatch.stopMovement;
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
	const bool explicitFrozen = (handle.entity->v.flags & FL_FROZEN) != 0;
	const bool roundFreeze = roundFreezeActive();
	if (!explicitFrozen && !roundFreeze && !managedBotActionStopsMovement_[index])
		applyPlayerAvoidance(index, &command);
	if (managedBotActionStopsMovement_[index])
	{
		// Do not let a delayed jump-crouch sequence survive an objective hold.
		managedBotJumpCrouch_[index].reset();
	}
	const std::uint16_t buttonsBeforeJumpCrouch = command.movement.buttons;
	const runtime::JumpCrouchObservation jumpCrouchObservation = {
		globals_->time,
		true,
		(handle.entity->v.flags & FL_ONGROUND) != 0,
		handle.entity->v.deadflag != DEAD_NO || handle.entity->v.health <= 0.0f,
		handle.entity->v.movetype == MOVETYPE_FLY,
		explicitFrozen || roundFreeze,
		(command.movement.buttons & static_cast<std::uint16_t>(IN_JUMP)) != 0U};
	const runtime::JumpCrouchCommandDecision jumpCrouchDecision =
		managedBotJumpCrouch_[index].updateCommand(
			jumpCrouchObservation,
			command.movement.buttons,
			static_cast<std::uint16_t>(IN_DUCK));
	command.movement.buttons = jumpCrouchDecision.buttons;
	if (!jumpCrouchObservation.grounded)
	{
		command.movement.buttons = static_cast<std::uint16_t>(
			command.movement.buttons & ~static_cast<std::uint16_t>(IN_JUMP));
	}
	if (runtimeProfiler_.enabled() &&
		jumpCrouchDecision.sequencing.transition !=
			runtime::JumpCrouchTransition::None &&
		gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr &&
		pluginId_ != nullptr)
	{
		profiledLogConsole(runtimeProfiler_,
			pluginId_,
			"profile jumpCrouch bot_id=%u command_sequence=%u transition=%s phase=%d "
			"elapsed=%.3f grounded=%d before_buttons=%u after_buttons=%u",
			static_cast<unsigned int>(handle.actor.slot),
			static_cast<unsigned int>(command.sequence),
			jumpCrouchTransitionName(jumpCrouchDecision.sequencing.transition),
			static_cast<int>(jumpCrouchDecision.sequencing.phase),
			jumpCrouchDecision.sequencing.elapsed,
			jumpCrouchObservation.grounded ? 1 : 0,
			static_cast<unsigned int>(buttonsBeforeJumpCrouch),
			static_cast<unsigned int>(command.movement.buttons));
	}
	MovementExecutionObservation gateObservation = {};
	gateObservation.explicitFrozen = explicitFrozen;
	gateObservation.roundFreezeActive = roundFreeze;
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
	bool speedRestored = false;
	if (gateDecision.phase == MovementExecutionPhase::Live &&
			!managedBotActionStopsMovement_[index] &&
			(!std::isfinite(maxSpeed) || maxSpeed <= 1.0f))
	{
		handle.entity->v.maxspeed = kDefaultManagedBotMaxSpeed;
		speedRestored = true;
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
		{
			RuntimeProfilerScope moveScope(runtimeProfiler_, RuntimeProfilerStage::RunPlayerMove);
			engineFunctions_->pfnRunPlayerMove(
				handle.entity, viewAngles, 0.0f, 0.0f, 0.0f, 0U, 0U, 0U);
		}
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
	const runtime::QueueResult enqueueResult = inputDispatcher_.enqueue(command);
	if (enqueueResult != runtime::QueueResult::Accepted)
	{
		if (runtimeProfiler_.enabled() && (adapterFrameCount_ % 32U) == 0U &&
			gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr &&
			pluginId_ != nullptr)
		{
			const runtime::LifecycleToken currentToken =
				lifecycle_.tokenForSlot(handle.actor.slot);
			profiledLogConsole(runtimeProfiler_,
				pluginId_,
				"profile movementReject bot_id=%u actor_generation=%u frame=%u "
				"queue_result=%d command_valid=%d sequence=%u msec=%u "
				"command_lifecycle=(%u,%u,%u) current_lifecycle=(%u,%u,%u) "
				"view=(%.2f %.2f %.2f) move=(%.2f %.2f %.2f) buttons=%u",
				static_cast<unsigned int>(handle.actor.slot),
				static_cast<unsigned int>(handle.actor.actorGeneration),
				static_cast<unsigned int>(adapterFrameCount_),
				static_cast<int>(enqueueResult),
				command.isValid() ? 1 : 0,
				static_cast<unsigned int>(command.sequence),
				static_cast<unsigned int>(command.movement.msec),
				static_cast<unsigned int>(command.lifecycle.mapGeneration),
				static_cast<unsigned int>(command.lifecycle.roundGeneration),
				static_cast<unsigned int>(command.lifecycle.slotGeneration),
				static_cast<unsigned int>(currentToken.mapGeneration),
				static_cast<unsigned int>(currentToken.roundGeneration),
				static_cast<unsigned int>(currentToken.slotGeneration),
				command.viewAngles.pitch,
				command.viewAngles.yaw,
				command.viewAngles.roll,
				command.movement.forward,
				command.movement.side,
				command.movement.up,
				static_cast<unsigned int>(command.movement.buttons));
		}
		receipt.sequence = command.sequence;
		receipt.result = enqueueResult == runtime::QueueResult::StaleActor
			? runtime::DispatchResult::StaleActor
			: runtime::DispatchResult::InvalidCommand;
		// Never retry a rejected cached command unchanged; rebuild with the current lifecycle.
		resetManagedBotCommandTemplate(index);
		return receipt;
	}
	const Vector commandStart=handle.entity->v.origin;
	const runtime::CommandReceipt dispatchReceipt =
		inputDispatcher_.dispatchNext(handle.actor, adapterFrameCount_);
	if(runtimeProfiler_.enabled() && managedCommandDiagnosticUpdates_[index]!=managedBotFullUpdateSequences_[index] &&
		handle.entity!=nullptr && !handle.entity->free)
	{
		managedCommandDiagnosticUpdates_[index]=managedBotFullUpdateSequences_[index];
		const auto &step=managedNavigationSteps_[index];
		profiledLogConsole(runtimeProfiler_,pluginId_,
			"profile motionCommand bot_id=%u actor_generation=%u map=%u round=%u frame=%u update=%u now=%.3f state=%s outcome=%u path=%u input=(%.1f %.1f %.1f) buttons=%u msec=%u dispatched=%d start=(%.1f %.1f %.1f) after=(%.1f %.1f %.1f)",
			handle.actor.slot,handle.actor.actorGeneration,lifecycle_.mapGeneration(),lifecycle_.roundGeneration(),
			adapterFrameCount_,managedBotFullUpdateSequences_[index],globals_->time,
			compat::toString(managedBotStateMachines_[index].state()),static_cast<unsigned>(step.outcome),step.pathSequence,
			command.movement.forward,command.movement.side,command.movement.up,
			static_cast<unsigned>(command.movement.buttons),static_cast<unsigned>(command.movement.msec),
			dispatchReceipt.result==runtime::DispatchResult::Dispatched?1:0,
			commandStart.x,commandStart.y,commandStart.z,
			handle.entity->v.origin.x,handle.entity->v.origin.y,handle.entity->v.origin.z);
	}
	if (dispatchReceipt.result == runtime::DispatchResult::Dispatched &&
		(command.movement.buttons & IN_RELOAD) != 0U && globals_ != nullptr)
	{
		publicObservations_.requestReload(
			{handle.actor.slot, handle.actor.actorGeneration},
			{lifecycle_.mapGeneration(), lifecycle_.roundGeneration(), adapterFrameCount_},
			globals_->time);
	}
	if (dispatchReceipt.result == runtime::DispatchResult::Dispatched &&
		(command.movement.buttons & ActionAdapter::kAttackButton) != 0U)
	{
		ManagedObjectiveActionState &actionState = managedBotObjectiveActions_[index];
		if (actionState.active &&
			actionState.actor.slot == handle.actor.slot &&
			actionState.actor.generation == handle.actor.actorGeneration &&
			actionState.kind == team::TeamObjectiveKind::PlantC4 &&
			actionState.mapGeneration == lifecycle_.mapGeneration() &&
			actionState.roundGeneration == lifecycle_.roundGeneration())
		{
			actionState.c4AttackInputDispatched = true;
			ActionAdapter::recordPlantAttemptDispatched(
				actionState.plantAttempt, globals_ != nullptr ? globals_->time
					: std::numeric_limits<float>::quiet_NaN());
			if (runtimeProfiler_.enabled() && (adapterFrameCount_ % 16U) == 0U &&
				gpMetaUtilFuncs != nullptr &&
				gpMetaUtilFuncs->pfnLogConsole != nullptr && pluginId_ != nullptr)
			{
				profiledLogConsole(runtimeProfiler_,
					pluginId_,
					"profile plantInput actor=%u actor_generation=%u map=%u round=%u "
					"frame=%u stage=attack_dispatched buttons=%u",
					static_cast<unsigned int>(handle.actor.slot),
					static_cast<unsigned int>(handle.actor.actorGeneration),
					static_cast<unsigned int>(lifecycle_.mapGeneration()),
					static_cast<unsigned int>(lifecycle_.roundGeneration()),
					static_cast<unsigned int>(adapterFrameCount_),
					static_cast<unsigned int>(command.movement.buttons));
			}
		}
	}
	if (runtimeProfiler_.enabled() && gpMetaUtilFuncs != nullptr &&
		gpMetaUtilFuncs->pfnLogConsole != nullptr && pluginId_ != nullptr &&
		globals_ != nullptr && std::isfinite(globals_->time) &&
		(!std::isfinite(movementGateDiagnosticTimes_[index]) ||
		 globals_->time < movementGateDiagnosticTimes_[index] ||
		 globals_->time - movementGateDiagnosticTimes_[index] >= 1.0f))
	{
		movementGateDiagnosticTimes_[index] = globals_->time;
		profiledLogConsole(runtimeProfiler_,
			pluginId_,
			"profile movementGate actor=%u generation=%u map=%u round=%u frame=%u "
			"freeze_active=%d freeze_until=%.3f explicit_frozen=%d phase=%d "
			"maxspeed_before=%.1f maxspeed_after=%.1f speed_restored=%d action_stop=%d "
			"input=(%.1f %.1f %.1f) buttons=%u dispatch=%d",
			static_cast<unsigned int>(handle.actor.slot),
			static_cast<unsigned int>(handle.actor.actorGeneration),
			static_cast<unsigned int>(lifecycle_.mapGeneration()),
			static_cast<unsigned int>(lifecycle_.roundGeneration()),
			static_cast<unsigned int>(adapterFrameCount_),
			roundFreeze ? 1 : 0,
			roundFreezeScheduled_ ? roundFreezeUntil_ : 0.0f,
			explicitFrozen ? 1 : 0,
			static_cast<int>(gateDecision.phase),
			maxSpeed,
			handle.entity->v.maxspeed,
			speedRestored ? 1 : 0,
			managedBotActionStopsMovement_[index] ? 1 : 0,
			gateDecision.forward,
			gateDecision.side,
			gateDecision.up,
			static_cast<unsigned int>(gateDecision.buttons),
			static_cast<int>(dispatchReceipt.result));
	}
	const bool freezeActiveAfterDispatch = roundFreezeActive();
	const bool freezeMovementInput = std::fabs(gateDecision.forward) > 0.01f ||
		std::fabs(gateDecision.side) > 0.01f || std::fabs(gateDecision.up) > 0.01f;
	if (runtimeProfiler_.enabled() && (roundFreeze || freezeActiveAfterDispatch) &&
		((adapterFrameCount_ % 6U) == 0U || roundFreeze != freezeActiveAfterDispatch ||
		 freezeMovementInput) &&
		gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr &&
		pluginId_ != nullptr)
	{
		profiledLogConsole(runtimeProfiler_,
			pluginId_,
			"profile freezeMotion actor=%u generation=%u map=%u round=%u frame=%u "
			"freeze_gate=%d freeze_after=%d origin=(%.1f %.1f %.1f) "
			"velocity=(%.1f %.1f %.1f) input=(%.1f %.1f %.1f) buttons=%u",
			static_cast<unsigned int>(handle.actor.slot),
			static_cast<unsigned int>(handle.actor.actorGeneration),
			static_cast<unsigned int>(lifecycle_.mapGeneration()),
			static_cast<unsigned int>(lifecycle_.roundGeneration()),
			static_cast<unsigned int>(adapterFrameCount_),
			roundFreeze ? 1 : 0, freezeActiveAfterDispatch ? 1 : 0,
			handle.entity->v.origin.x, handle.entity->v.origin.y, handle.entity->v.origin.z,
			handle.entity->v.velocity.x, handle.entity->v.velocity.y, handle.entity->v.velocity.z,
			gateDecision.forward, gateDecision.side, gateDecision.up,
			static_cast<unsigned int>(gateDecision.buttons));
	}
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
		hasObservedActiveWeapon(index),
		command.viewAngles);
	const ActionDispatch actionDispatch = ActionAdapter::translate(actionProposal);
	command.viewAngles = actionDispatch.viewAngles;
	command.movement.buttons = actionDispatch.buttons;
	if (actionProposal.kind != ActionKind::None && gpMetaUtilFuncs != nullptr &&
		gpMetaUtilFuncs->pfnLogConsole != nullptr && pluginId_ != nullptr &&
		(adapterFrameCount_ % 16U) == 0U)
	{
		profiledLogConsole(runtimeProfiler_,
			pluginId_, "bot action=%s slot=%u generation=%u frame=%u buttons=%u",
			actionKindName(actionProposal.kind), static_cast<unsigned int>(handle.actor.slot),
			static_cast<unsigned int>(handle.actor.actorGeneration),
			static_cast<unsigned int>(adapterFrameCount_),
			static_cast<unsigned int>(actionDispatch.buttons));
	}
	if (actionDispatch.clientCommand != nullptr)
	{
		(void)dispatchManagedActionCommand(
			index, handle, actionProposal, actionDispatch);
	}
	if (inputDispatcher_.enqueue(command) != runtime::QueueResult::Accepted)
		{
			receipt.result = runtime::DispatchResult::InvalidCommand;
			return receipt;
		}
	const runtime::CommandReceipt dispatchReceipt =
		inputDispatcher_.dispatchNext(handle.actor, adapterFrameCount_);
	if (dispatchReceipt.result == runtime::DispatchResult::Dispatched &&
		(actionDispatch.buttons & ActionAdapter::kAttackButton) != 0U)
	{
		ManagedObjectiveActionState &actionState = managedBotObjectiveActions_[index];
		if (actionState.active &&
			actionState.actor.slot == handle.actor.slot &&
			actionState.actor.generation == handle.actor.actorGeneration &&
			actionState.kind == team::TeamObjectiveKind::PlantC4)
		{
			actionState.c4AttackInputDispatched = true;
			ActionAdapter::recordPlantAttemptDispatched(
				actionState.plantAttempt, globals_ != nullptr ? globals_->time
					: std::numeric_limits<float>::quiet_NaN());
		}
	}
	return dispatchReceipt;
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
		{
			RuntimeProfilerScope moveScope(runtimeProfiler_, RuntimeProfilerStage::RunPlayerMove);
			engineFunctions_->pfnRunPlayerMove(
				handle.entity, viewAngles, 0.0f, 0.0f, 0.0f, buttons, 0U, milliseconds);
		}
		receipt.result = runtime::DispatchResult::Dispatched;
		return receipt;
	}

compat::StateUpdateContext PluginRuntime::makeManagedBotStateContext(
	std::size_t index,
	const runtime::MovementPhysicsState &before,
	compat::StateBehaviorInput *behavior) const
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
	if (behavior != nullptr)
	{
		*behavior = {};
		behavior->navigationOwned=compatibilitySurface_.configuration().mode==compat::RuntimeMode::Compatibility;
		behavior->navigationReady=navPublisher_.snapshot().isValid();
		const ManagedObjectiveTargetCache &target = managedBotObjectiveTargets_[index];
		behavior->goalAvailable = target.valid && target.mapGeneration == lifecycle_.mapGeneration() &&
			target.roundGeneration == lifecycle_.roundGeneration();
		if (behavior->goalAvailable)
		{
			const float dx = before.origin.x - target.target.x;
			const float dy = before.origin.y - target.target.y;
			const float dz = before.origin.z - target.target.z;
			behavior->goalReached = dx * dx + dy * dy + dz * dz <= 64.0f * 64.0f;
		}
		const auto &failure=managedNavigationFailures_[index];
		const bool ownPathFailed=failure.actor==context.actor && failure.frame.isValid() &&
			failure.frame.mapGeneration==context.frame.mapGeneration &&
			failure.frame.roundGeneration==context.frame.roundGeneration &&
			failure.frame.tick<context.frame.tick && failure.goalGeneration==target.generation &&
			managedBotStateMachines_[index].state()==compat::CompatibilityStateId::MoveTo;
		behavior->pathFailed = ownPathFailed || (managedTeamObjectivePathFailurePending_ &&
			managedTeamObjectivePathFailureActor_ == context.actor);
		behavior->objectiveActive = behavior->goalAvailable;
		if(behavior->navigationOwned && managedBotStateMachines_[index].state()==compat::CompatibilityStateId::DefuseBomb)
		{
			behavior->objectiveActive=bombNoticeKnown_[index] && bombNoticePlanted_[index];
			behavior->defuseReceipt=publicObservations_.progressReceipt(context.actor,context.frame,
				context.timestamp,managedBotStateMachines_[index].stateTimestamp());
		}
		context.behavior = behavior;
	}
	return context;
}

int PluginRuntime::resolveManagedBotTeam(
	std::size_t index,
	const compat::CompatibilityObservation &observation) const
{
	if (index < managedBotTeamNumbers_.size() &&
		joinControllers_[index].teamConfirmed() &&
		managedBotTeamNumbers_[index] >= 1U &&
		managedBotTeamNumbers_[index] <= 2U)
	{
		return static_cast<int>(managedBotTeamNumbers_[index]);
	}
	if (observation.player.team.isAvailable() &&
		observation.player.team.value >= 1 && observation.player.team.value <= 2)
	{
		return observation.player.team.value;
	}
	return 0;
}

void PluginRuntime::updateManagedBotCompatibilityState(
	std::size_t index,
	const runtime::MovementPhysicsState &before)
{
	RuntimeProfilerScope scenarioScope(runtimeProfiler_, RuntimeProfilerStage::ScenarioUpdate);
	if (index >= managedBotStateMachines_.size() ||
		!managedBotSlots_[index] || managedBotHandles_[index].entity == nullptr)
	{
		return;
	}
	compat::StateBehaviorInput stateBehavior = {};
	compat::StateUpdateContext context = makeManagedBotStateContext(index, before, &stateBehavior);
	compat::CompatibilityStateMachine &machine = managedBotStateMachines_[index];
	scenarioDecisions_[index] = {};
	if (managedBotStateRounds_[index] != 0U &&
		managedBotStateRounds_[index] != lifecycle_.roundGeneration() &&
		machine.isInitialized())
	{
		scenarioPlanners_[index].reset();
		(void)machine.onRoundReset(context);
	}
	managedBotStateRounds_[index] = lifecycle_.roundGeneration();
	if (!machine.isInitialized())
	{
		scenarioPlanners_[index].reset();
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
		scenarioPlanners_[index].reset();
		managedNavigationFailures_[index] = {};
		if (!managedBotWasDead_[index])
		{
			(void)machine.onDeath(context);
		}
		managedBotWasDead_[index] = true;
		return;
	}
	if (managedBotWasDead_[index])
	{
		scenarioPlanners_[index].reset();
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
			compat::ScenarioBehaviorInput scenario = {};
			scenario.actor = context.actor;
			scenario.frame = context.frame;
			scenario.timestamp = context.timestamp;
			scenario.selfPositionKnown = std::isfinite(before.origin.x) &&
				std::isfinite(before.origin.y) && std::isfinite(before.origin.z);
			scenario.selfPosition = {before.origin.x, before.origin.y, before.origin.z};
			scenario.lifeKnown = context.lifeAvailability == compat::ObservationAvailability::Available;
			scenario.alive = context.alive;
			scenario.reactionSeconds = managedProfiles_[index].reactionTimeSeconds;
			scenario.aggression = static_cast<float>(managedProfiles_[index].aggression) / 100.0f;
			scenario.attacking = machine.attackOverlayActive();
			scenario.currentDefusing = machine.state() == compat::CompatibilityStateId::DefuseBomb;
			scenario.alreadyEscapingFromBomb = machine.state() == compat::CompatibilityStateId::EscapeFromBomb;
			const int observedTeam = resolveManagedBotTeam(index, observation);
			scenario.team = observedTeam == 1 ? objectives::TeamRole::Terrorist :
				observedTeam == 2 ? objectives::TeamRole::CounterTerrorist : objectives::TeamRole::Unknown;
			compat::PublicActorState publicState = {};
			scenario.blind = publicObservations_.read(context.actor, context.frame, &publicState) &&
				context.timestamp < publicState.blindUntil;
			scenario.isVipKnown = observation.objective.vip.isAvailable();
			scenario.isVip = scenario.isVipKnown && observation.objective.vip.value;
			scenario.bombPlantedKnown = observation.objective.bombPlanted.isAvailable();
			scenario.bombPlanted = scenario.bombPlantedKnown && observation.objective.bombPlanted.value;
			if (observation.objective.bombPosition.isAvailable())
			{
				const auto &position = observation.objective.bombPosition.value;
				scenario.bomb.knowledge = compat::ScenarioKnowledge::Remembered;
				scenario.bomb.frame = context.frame;
				scenario.bomb.position = {position.x, position.y, position.z};
				const float dx = position.x - before.origin.x, dy = position.y - before.origin.y,
					dz = position.z - before.origin.z;
				scenario.bomb.pathCost = std::sqrt(dx * dx + dy * dy + dz * dz);
			}
			if (bombNoticeKnown_[index])
			{
				scenario.bombPlantedKnown = true;
				scenario.bombPlanted = bombNoticePlanted_[index];
			}
			if (scenario.bombPlantedKnown && scenario.bombPlanted)
			{
				const float publicFuse = readOptionalCvarFloat(engineFunctions_, "mp_c4timer");
				scenario.timerEstimated = publicBombClocks_[index].estimate(context.frame,
					context.timestamp, publicFuse, &scenario.bombSecondsRemaining);
			}
			const auto &score = publicScoreStatuses_[index][index];
			if (score.known && score.serial == static_cast<std::uint32_t>(managedBotHandles_[index].entity->serialnumber) &&
				score.frame.mapGeneration == context.frame.mapGeneration && score.frame.roundGeneration == context.frame.roundGeneration)
			{
				scenario.isVipKnown = true;
				scenario.isVip = (score.flags & 4) != 0;
			}
			scenario.defuseKitKnown = observation.objective.hasDefuseKit.isAvailable();
			scenario.defuseKit = scenario.defuseKitKnown && observation.objective.hasDefuseKit.value;
			scenario.radio = &scenarioRadio_[index];
			float automaticLeaderRange = (std::numeric_limits<float>::max)();
			if (engineFunctions_ != nullptr && globals_ != nullptr &&
				engineFunctions_->pfnSzFromIndex != nullptr && engineFunctions_->pfnPEntityOfEntIndex != nullptr)
			{
				const char *map = engineFunctions_->pfnSzFromIndex(globals_->mapname);
				scenario.scenario = map != nullptr && std::strncmp(map, "cs_", 3U) == 0 ? compat::PublicScenarioKind::Hostage :
					map != nullptr && std::strncmp(map, "as_", 3U) == 0 ? compat::PublicScenarioKind::Vip :
					map != nullptr && std::strncmp(map, "es_", 3U) == 0 ? compat::PublicScenarioKind::Escape :
					map != nullptr && std::strncmp(map, "de_", 3U) == 0 ? compat::PublicScenarioKind::Bomb : compat::PublicScenarioKind::Unknown;
				const float eye[] = {before.origin.x + managedBotHandles_[index].entity->v.view_ofs.x,
					before.origin.y + managedBotHandles_[index].entity->v.view_ofs.y,
					before.origin.z + managedBotHandles_[index].entity->v.view_ofs.z};
				const auto project = [&](edict_t *entity, int entityIndex, bool mapKnown) -> compat::ScenarioTarget
				{
					compat::ScenarioTarget target = {};
					if (entity == nullptr || entity->free != 0)
					{
						return target;
					}
					const bool brush = entity->v.solid == SOLID_BSP || entity->v.solid == SOLID_TRIGGER;
					const float point[] = {brush ? (entity->v.absmin.x + entity->v.absmax.x) * 0.5f : entity->v.origin.x,
						brush ? (entity->v.absmin.y + entity->v.absmax.y) * 0.5f : entity->v.origin.y,
						brush ? (entity->v.absmin.z + entity->v.absmax.z) * 0.5f : entity->v.origin.z};
					const float dx = point[0] - eye[0], dy = point[1] - eye[1], dz = point[2] - eye[2];
					const float planar = std::hypot(dx, dy);
					const float yaw = managedBotHandles_[index].entity->v.v_angle.y * 3.14159265358979323846f / 180.0f;
					bool visible = false;
					if (!mapKnown && !scenario.blind && engineFunctions_->pfnTraceLine != nullptr &&
						!smokeObservations_.blocks({eye[0],eye[1],eye[2]}, {point[0],point[1],point[2]}, context.timestamp) &&
						(planar < 0.001f || (dx * std::cos(yaw) + dy * std::sin(yaw)) / planar > 0.5f))
					{
						TraceResult trace = {};
						engineFunctions_->pfnTraceLine(eye, point, 1, managedBotHandles_[index].entity, &trace);
						visible = trace.flFraction == 1.0f || trace.pHit == entity;
					}
					if (!mapKnown && !visible)
					{
						return target;
					}
					target.knowledge = mapKnown ? compat::ScenarioKnowledge::Remembered : compat::ScenarioKnowledge::Visible;
					target.frame = context.frame;
					// Core generations reserve zero; map entities may publicly have serial zero.
					target.entity = {static_cast<std::uint32_t>(entityIndex), static_cast<std::uint32_t>(entity->serialnumber) + 1U};
					target.position = {point[0],point[1],point[2]};
					target.pathCost = std::sqrt(dx * dx + dy * dy + dz * dz);
					target.inUseRange = target.pathCost <= 64.0f;
					return target;
				};
				const auto nearer = [](const compat::ScenarioTarget &candidate, const compat::ScenarioTarget &current)
				{
					return candidate.knowledge != compat::ScenarioKnowledge::Unknown &&
						(current.knowledge == compat::ScenarioKnowledge::Unknown || candidate.pathCost < current.pathCost);
				};
				const int maxEntities = (std::max)(0, (std::min)(globals_->maxEntities, 2048));
				for (int entityIndex = 1; entityIndex < maxEntities; ++entityIndex)
				{
					edict_t *entity = engineFunctions_->pfnPEntityOfEntIndex(entityIndex);
					if (entity == nullptr || entity->free != 0)
					{
						continue;
					}
					const char *name = engineFunctions_->pfnSzFromIndex(entity->v.classname);
					if (name == nullptr)
					{
						continue;
					}
					if (entityIndex <= 32)
					{
						const auto &vipScore = publicScoreStatuses_[index][static_cast<std::size_t>(entityIndex - 1)];
						if (vipScore.known && vipScore.serial == static_cast<std::uint32_t>(entity->serialnumber) &&
							vipScore.frame.mapGeneration == context.frame.mapGeneration && vipScore.frame.roundGeneration == context.frame.roundGeneration &&
							(vipScore.flags & 4) != 0 && entity->v.deadflag == DEAD_NO)
						{
							scenario.vip = project(entity, entityIndex, false);
							scenario.vip.actor = {static_cast<std::uint32_t>(entityIndex), static_cast<std::uint32_t>(entity->serialnumber) + 1U};
						}
					}
					if (entityIndex <= globals_->maxClients && (entity->v.flags & FL_FAKECLIENT) == 0 &&
						entity->v.team == observedTeam && entity->v.deadflag == DEAD_NO && entity->v.health > 0.0f)
					{
						const auto location = project(entity,entityIndex,false);
						if (location.knowledge == compat::ScenarioKnowledge::Visible)
						{
							const float dx=location.position.x-before.origin.x, dy=location.position.y-before.origin.y,
								dz=location.position.z-before.origin.z;
							const float distance=std::sqrt(dx*dx+dy*dy+dz*dz);
							if (distance < automaticLeaderRange)
							{
								automaticLeaderRange=distance;
								scenario.automaticLeader={{static_cast<std::uint32_t>(entityIndex),static_cast<std::uint32_t>(entity->serialnumber)+1U},
									scenario.team,true,true,true,location};
								scenario.automaticLeader.location.actor=scenario.automaticLeader.actor;
							}
						}
					}
					if (std::strcmp(name, "grenade") == 0)
					{
						compat::ObjectiveObservation planted{};
						world::WorldVector knownPosition{};
						bool bombVisible = false;
						if (observationAdapter_.collectPlantedBomb(entity, name, engineFunctions_->pfnSzFromIndex(entity->v.model),
							context.timestamp, context.actor, context.frame, timing, &planted) == ObservationAdapterResult::Accepted &&
							observeObjective(index, entity, true, &knownPosition, &bombVisible))
						{
							scenario.bombPlantedKnown = true;
							scenario.bombPlanted = true;
							scenario.bomb.knowledge = bombVisible ? compat::ScenarioKnowledge::Visible
								: compat::ScenarioKnowledge::Remembered;
							scenario.bomb.frame = context.frame;
							scenario.bomb.entity = {static_cast<std::uint32_t>(entityIndex), static_cast<std::uint32_t>(entity->serialnumber) + 1U};
							scenario.bomb.position = {knownPosition.x, knownPosition.y, knownPosition.z};
							const float dx = knownPosition.x - before.origin.x, dy = knownPosition.y - before.origin.y, dz = knownPosition.z - before.origin.z;
							scenario.bomb.pathCost = std::sqrt(dx * dx + dy * dy + dz * dz);
							scenario.bomb.inUseRange = scenario.bomb.pathCost <= 64.0f;
							scenario.humanDefusingKnown = true;
							scenario.humanDefusing = externalDefuserEntityIndex_ != 0U && externalDefuserLeaseUntil_ >= context.timestamp &&
								externalDefuserMapGeneration_ == context.frame.mapGeneration && externalDefuserRoundGeneration_ == context.frame.roundGeneration;
						}
					}
					compat::ScenarioTarget *destination = nullptr;
					bool mapKnown = true;
					if (std::strcmp(name, "hostage_entity") == 0 && entity->v.health > 0.0f)
					{
						destination = &scenario.hostage;
						mapKnown = false;
					}
					else if (std::strcmp(name, "func_hostage_rescue") == 0 || std::strcmp(name, "info_hostage_rescue") == 0)
					{
						destination = &scenario.rescueZone;
					}
					else if (std::strcmp(name, "func_vip_safetyzone") == 0)
					{
						destination = &scenario.vipZone;
					}
					else if (std::strcmp(name, "func_escapezone") == 0)
					{
						destination = &scenario.escapeZone;
					}
					if (destination != nullptr)
					{
						const auto target = project(entity, entityIndex, mapKnown);
						if (nearer(target, *destination))
						{
							*destination = target;
						}
					}
				}
				// Nearby CTs veto motion-based ownership inference. Their public
				// positions never establish escort ownership or provide a target.
				if (scenario.hostage.entity.isValid())
				{
					for (int player = 1; player <= globals_->maxClients; ++player)
					{
						edict_t *other = engineFunctions_->pfnPEntityOfEntIndex(player);
						if (other == nullptr || other == managedBotHandles_[index].entity ||
							other->free != 0 || other->v.team != 2 || other->v.deadflag != DEAD_NO || other->v.health <= 0.0f)
							continue;
						const float dx = other->v.origin.x - scenario.hostage.position.x;
						const float dy = other->v.origin.y - scenario.hostage.position.y;
						const float dz = other->v.origin.z - scenario.hostage.position.z;
						if (dx * dx + dy * dy + dz * dz <= 256.0f * 256.0f)
						{
							scenario.hostageCompetitorNearby = true;
							break;
						}
					}
				}
				const auto tracked = scenarioPlanners_[index].followLeader();
				if (scenarioPlanners_[index].isFollowing() && tracked.isValid() && tracked.slot <= static_cast<std::uint32_t>(globals_->maxClients))
				{
					edict_t *human=engineFunctions_->pfnPEntityOfEntIndex(static_cast<int>(tracked.slot));
					if (human != nullptr && !human->free && static_cast<std::uint32_t>(human->serialnumber)+1U == tracked.generation)
					{
						scenario.automaticLeader={tracked,human->v.team==1 ? objectives::TeamRole::Terrorist :
							human->v.team==2 ? objectives::TeamRole::CounterTerrorist : objectives::TeamRole::Unknown,
							(human->v.flags & FL_FAKECLIENT)==0,true,human->v.deadflag==DEAD_NO && human->v.health>0.0f,
							project(human,static_cast<int>(tracked.slot),false)};
						scenario.automaticLeader.location.actor=tracked;
					}
				}
				const auto sender = scenarioRadio_[index].sender;
				if (sender.isValid() && sender.slot <= static_cast<std::uint32_t>(globals_->maxClients))
				{
					edict_t *leader = engineFunctions_->pfnPEntityOfEntIndex(static_cast<int>(sender.slot));
					if (leader != nullptr && leader->free == 0 && static_cast<std::uint32_t>(leader->serialnumber) == sender.generation)
					{
						scenario.leader.actor = sender;
						scenario.leader.team = leader->v.team == 1 ? objectives::TeamRole::Terrorist :
							leader->v.team == 2 ? objectives::TeamRole::CounterTerrorist : objectives::TeamRole::Unknown;
						scenario.leader.human = (leader->v.flags & FL_FAKECLIENT) == 0;
						scenario.leader.lifeKnown = true;
						scenario.leader.alive = leader->v.deadflag == DEAD_NO && leader->v.health > 0.0f;
						scenario.leader.location = project(leader, static_cast<int>(sender.slot), false);
						scenario.leader.location.actor = sender;
						if (scenario.leader.location.knowledge == compat::ScenarioKnowledge::Visible)
						{
							scenarioRadio_[index].position = scenario.leader.location;
						}
					}
				}
			}
			const nav::NavSnapshot scenarioNavigation = navPublisher_.snapshot();
			const nav::NavDocument *scenarioDocument = scenarioNavigation.document();
			if (scenario.scenario == compat::PublicScenarioKind::Bomb && scenario.team == objectives::TeamRole::CounterTerrorist &&
				scenario.bombPlantedKnown && scenario.bombPlanted && scenario.bomb.knowledge == compat::ScenarioKnowledge::Unknown &&
				scenarioNavigation.isValid() && scenarioNavigation.mapGeneration() == context.frame.mapGeneration && scenarioDocument != nullptr && globals_ != nullptr && engineFunctions_ != nullptr &&
				engineFunctions_->pfnPEntityOfEntIndex != nullptr && engineFunctions_->pfnSzFromIndex != nullptr)
			{
				compat::ScenarioTarget &search = bombSearchTargets_[index];
				if (search.knowledge != compat::ScenarioKnowledge::Unknown)
				{
					const float dx = search.position.x - before.origin.x, dy = search.position.y - before.origin.y,
						dz = search.position.z - before.origin.z;
					if (dx*dx + dy*dy + dz*dz <= 64.0f*64.0f)
					{ bombSearchMemory_[index].clear(search.entity); search = {}; }
				}
				if (search.knowledge == compat::ScenarioKnowledge::Unknown)
				{
					std::vector<compat::BombSearchSite> sites;
					nav::NavQuery query(scenarioNavigation);
					nav::NavAreaMatch start{};
					const nav::NavVector self = {before.origin.x, before.origin.y, before.origin.z};
					const bool haveStart = query.findNearest3D(self, kMaximumObjectiveDistance, &start) == nav::NavQueryResult::Found;
					const int limit = (std::min)(globals_->maxEntities, 2048);
					for (int entityIndex = 1; entityIndex < limit; ++entityIndex)
					{
						edict_t *entity = engineFunctions_->pfnPEntityOfEntIndex(entityIndex);
						if (entity == nullptr || entity->free) continue;
						const char *name = engineFunctions_->pfnSzFromIndex(entity->v.classname);
						if (name == nullptr || (std::strcmp(name,"func_bomb_target") != 0 && std::strcmp(name,"info_bomb_target") != 0)) continue;
						compat::BombSearchSite site{};
						site.entity = {static_cast<std::uint32_t>(entityIndex), static_cast<std::uint32_t>(entity->serialnumber)+1U};
						site.legacy = std::strcmp(name,"info_bomb_target") == 0;
						if (site.legacy)
						{
							site.center = {entity->v.origin.x,entity->v.origin.y,entity->v.origin.z};
							site.extent = {{site.center.x-256.0f,site.center.y-256.0f,site.center.z-256.0f},
								{site.center.x+256.0f,site.center.y+256.0f,site.center.z+256.0f}};
						}
						else
						{
							site.extent = {{entity->v.absmin.x,entity->v.absmin.y,entity->v.absmin.z},
								{entity->v.absmax.x,entity->v.absmax.y,entity->v.absmax.z}};
							site.center = {(site.extent.lo.x+site.extent.hi.x)*0.5f,(site.extent.lo.y+site.extent.hi.y)*0.5f,
								(site.extent.lo.z+site.extent.hi.z)*0.5f};
						}
						site.extent.lo.z -= 50.0f; site.extent.hi.z += 50.0f;
						nav::NavAreaMatch finish{}; nav::NavCorridor route{};
						if (haveStart && query.findNearest3D(site.center,kMaximumObjectiveDistance,&finish) == nav::NavQueryResult::Found &&
							query.buildCorridor(start.area,finish.area,nav::NavRouteType::Fastest,&route) == nav::NavQueryResult::Found)
							site.travel = route.cost;
						sites.push_back(site);
					}
					const compat::RandomActor randomActor = {context.actor.slot,context.actor.generation};
					const compat::RandomTimingContext randomTiming = {managedBotCommandSequences_[index],0U,managedBotFullUpdateSequences_[index]};
					std::size_t selected = 0U;
					if (bombSearchMemory_[index].choose(context.frame,sites,compatibilityRandomSource_,randomActor,randomTiming,&selected))
					{
						nav::NavVector point{};
						nav::NavAreaMatch chosenArea{}; nav::NavCorridor chosenRoute{};
						if (compat::bombSitePoint(*scenarioDocument,sites[selected],16U,compatibilityRandomSource_,randomActor,randomTiming,&point) &&
							haveStart && query.findNearest3D(point,kMaximumObjectiveDistance,&chosenArea) == nav::NavQueryResult::Found &&
							query.buildCorridor(start.area,chosenArea.area,nav::NavRouteType::Fastest,&chosenRoute) == nav::NavQueryResult::Found)
						{
							search.knowledge = compat::ScenarioKnowledge::Remembered; search.frame = context.frame;
							search.entity = sites[selected].entity; search.position = {point.x,point.y,point.z};
							search.pathCost = chosenRoute.cost;
						}
						else bombSearchMemory_[index].clear(sites[selected].entity);
					}
				}
				if (search.knowledge != compat::ScenarioKnowledge::Unknown)
				{ search.frame = context.frame; scenario.bombSearch = search; }
			}
			if (scenario.scenario == compat::PublicScenarioKind::Bomb && scenario.bombPlantedKnown &&
				scenario.bombPlanted && scenario.bomb.knowledge != compat::ScenarioKnowledge::Unknown &&
				scenarioNavigation.isValid() && scenarioNavigation.mapGeneration() == context.frame.mapGeneration && scenarioDocument != nullptr)
			{
				const nav::NavVector self = {before.origin.x, before.origin.y, before.origin.z};
				const nav::NavVector bomb = {scenario.bomb.position.x, scenario.bomb.position.y, scenario.bomb.position.z};
					scenario.travelTimeEstimated = scenario.team==objectives::TeamRole::CounterTerrorist &&
						!scenario.alreadyEscapingFromBomb && !scenario.currentDefusing &&
						compat::estimateBombTravel(scenarioNavigation, self, bomb,
					managedBotHandles_[index].entity->v.maxspeed, kMaximumObjectiveDistance, &scenario.travelSecondsToBomb);
				const bool lateDefuse = scenario.team == objectives::TeamRole::CounterTerrorist &&
					scenario.timerEstimated && scenario.travelTimeEstimated && scenario.defuseKitKnown &&
					scenario.bombSecondsRemaining <= scenario.travelSecondsToBomb + (scenario.defuseKit ? 5.0f : 10.0f);
				const bool requestedEscape = compat::ScenarioBehaviorPlanner::autoEscapeRequested(scenario) ||
					(lateDefuse && !scenario.attacking && !scenario.currentDefusing) || scenario.alreadyEscapingFromBomb ||
					(scenario.radio != nullptr && scenario.radio->command == compat::ScenarioRadioCommand::GetOut);
				if (requestedEscape)
				{
					nav::AreaId &goal = scenarioEscapeAreas_[index];
					const nav::NavArea *area = scenarioDocument->findArea(goal);
					if (area == nullptr)
					{
						const compat::RandomActor randomActor = {context.actor.slot, context.actor.generation};
						const compat::RandomTimingContext randomTiming = {managedBotCommandSequences_[index], 0U,
							managedBotFullUpdateSequences_[index]};
						if (compat::selectBombEscapeArea(*scenarioDocument, bomb, compatibilityRandomSource_, randomActor, randomTiming, &goal))
							area = scenarioDocument->findArea(goal);
					}
					if (area != nullptr)
					{
						const auto point = compat::bombAreaCenter(*area);
						scenario.escapeZone.knowledge = compat::ScenarioKnowledge::Remembered;
						scenario.escapeZone.frame = context.frame;
						scenario.escapeZone.position = {point.x,point.y,point.z};
						const float dx=point.x-self.x,dy=point.y-self.y,dz=point.z-self.z;
						// A lower-bound ordering value; actual reachability belongs
						// to the state-owned navigation step, not a preflight search.
						scenario.escapeZone.pathCost=std::sqrt(dx*dx+dy*dy+dz*dz);
					}
				}
			}
			const auto validateRadioSender = [&](world::ActorKey sender) -> compat::ScenarioSenderValidation
			{
				compat::ScenarioSenderValidation validation{sender,false,false};
				if (!sender.isValid() || globals_==nullptr || engineFunctions_==nullptr || engineFunctions_->pfnPEntityOfEntIndex==nullptr)
					return validation;
				validation.known=true;
				if (sender.slot>static_cast<std::uint32_t>(globals_->maxClients)) return validation;
				edict_t *human=engineFunctions_->pfnPEntityOfEntIndex(static_cast<int>(sender.slot));
				validation.valid=(observedTeam==1 || observedTeam==2) && human!=nullptr && !human->free && (human->v.flags&FL_CLIENT)!=0 &&
					(human->v.flags&FL_FAKECLIENT)==0 && human->v.team==observedTeam &&
					static_cast<std::uint32_t>(human->serialnumber)==sender.generation;
				return validation;
			};
			if (scenario.radio!=nullptr) scenario.radioSenderValidation=validateRadioSender(scenario.radio->sender);
			scenario.pendingRadioSenderValidation=validateRadioSender(scenarioPlanners_[index].pendingRadioSender());
			const bool wasFollowing = scenarioPlanners_[index].isFollowing();
			compat::TeamworkInput teamwork{};
			teamwork.actor=context.actor; teamwork.frame=context.frame; teamwork.now=context.timestamp;
			teamwork.teamwork=static_cast<float>(managedProfiles_[index].teamwork)/100.0f;
			teamwork.roundTimeKnown=roundFreezeGeneration_==lifecycle_.roundGeneration() &&
				std::isfinite(roundFreezeStartTime_) && context.timestamp>=roundFreezeStartTime_;
			teamwork.elapsedRound=context.timestamp-roundFreezeStartTime_;
			teamwork.allowRoguesKnown=engineFunctions_!=nullptr && engineFunctions_->pfnCVarGetFloat!=nullptr && engineFunctions_->pfnCVarGetPointer!=nullptr &&
				engineFunctions_->pfnCVarGetPointer("bot_allow_rogues")!=nullptr;
			const float allowRogues=readOptionalCvarFloat(engineFunctions_,"bot_allow_rogues");
			teamwork.allowRoguesKnown=teamwork.allowRoguesKnown && std::isfinite(allowRogues);
			teamwork.allowRogues=allowRogues!=0.0f;
			const auto currentState=machine.state();
			const bool sniper=observation.weapon.activeWeaponId.isAvailable() &&
				(observation.weapon.activeWeaponId.value==3U || observation.weapon.activeWeaponId.value==18U ||
				 observation.weapon.activeWeaponId.value==13U || observation.weapon.activeWeaponId.value==24U);
			scenario.busy=scenario.attacking || currentState==compat::CompatibilityStateId::Buy ||
				currentState==compat::CompatibilityStateId::DefuseBomb || currentState==compat::CompatibilityStateId::PlantBomb ||
				scenarioPlanners_[index].awaitingHostageEscort() || (currentState==compat::CompatibilityStateId::Hide && sniper);
			teamwork.busy=scenario.busy; teamwork.following=wasFollowing;
			teamwork.followDuration=wasFollowing ? context.timestamp-scenarioPlanners_[index].followSince() : 0.0f;
			teamwork.candidate=scenario.automaticLeader.actor;
			teamwork.candidateVisibleHumanFriend=scenario.automaticLeader.human && scenario.automaticLeader.lifeKnown &&
				scenario.automaticLeader.alive && scenario.automaticLeader.team==scenario.team &&
				scenario.automaticLeader.location.knowledge==compat::ScenarioKnowledge::Visible;
			teamwork.candidateRange=automaticLeaderRange;
			if (teamwork.candidateVisibleHumanFriend && scenarioNavigation.isValid() && scenarioDocument!=nullptr &&
				scenarioNavigation.mapGeneration()==context.frame.mapGeneration)
			{
				nav::NavQuery query(scenarioNavigation); nav::NavAreaMatch selfArea{},humanArea{}; nav::NavCorridor route{};
				const auto &position=scenario.automaticLeader.location.position;
				if (query.findNearest3D({before.origin.x,before.origin.y,before.origin.z},kMaximumObjectiveDistance,&selfArea)==nav::NavQueryResult::Found &&
					query.findContaining({position.x,position.y,position.z},64.0f,&humanArea)==nav::NavQueryResult::Found &&
					query.buildCorridor(selfArea.area,humanArea.area,nav::NavRouteType::Fastest,&route)==nav::NavQueryResult::Found)
				{ teamwork.candidateReachable=true; teamwork.candidateTravel=route.cost; }
				for (std::size_t bot=0U;bot<scenarioPlanners_.size();++bot)
					if (scenarioPlanners_[bot].isFollowing() && scenarioPlanners_[bot].followLeader().slot==teamwork.candidate.slot)
						++teamwork.followerCount;
			}
			if (scenario.bombPlantedKnown && scenario.bombPlanted && scenario.bomb.knowledge!=compat::ScenarioKnowledge::Unknown &&
				engineFunctions_!=nullptr && engineFunctions_->pfnPEntityOfEntIndex!=nullptr && engineFunctions_->pfnSzFromIndex!=nullptr && globals_!=nullptr)
			{
				for (int entityIndex=1;entityIndex<(std::min)(globals_->maxEntities,2048);++entityIndex)
				{
					edict_t *zone=engineFunctions_->pfnPEntityOfEntIndex(entityIndex);
					if(zone==nullptr || zone->free) continue;
					const char *name=engineFunctions_->pfnSzFromIndex(zone->v.classname);
					if(name==nullptr || (std::strcmp(name,"func_bomb_target")!=0 && std::strcmp(name,"info_bomb_target")!=0)) continue;
					const bool legacy=std::strcmp(name,"info_bomb_target")==0;
					const float loX=legacy?zone->v.origin.x-256.0f:zone->v.absmin.x;
					const float loY=legacy?zone->v.origin.y-256.0f:zone->v.absmin.y;
					const float loZ=(legacy?zone->v.origin.z-256.0f:zone->v.absmin.z)-50.0f;
					const float hiX=legacy?zone->v.origin.x+256.0f:zone->v.absmax.x;
					const float hiY=legacy?zone->v.origin.y+256.0f:zone->v.absmax.y;
					const float hiZ=(legacy?zone->v.origin.z+256.0f:zone->v.absmax.z)+50.0f;
					const auto contains=[&](float x,float y,float z){return x>=loX && x<=hiX && y>=loY && y<=hiY && z>=loZ && z<=hiZ;};
					if(contains(before.origin.x,before.origin.y,before.origin.z) &&
						contains(scenario.bomb.position.x,scenario.bomb.position.y,scenario.bomb.position.z))
					{ teamwork.atPlantedBombsite=true; break; }
				}
			}
			compat::TeamworkDecision cooperative{};
			if (scenario.lifeKnown && scenario.alive)
				cooperative=teamworkPolicies_[index].update(teamwork,compatibilityRandomSource_,
					{context.actor.slot,context.actor.generation},{managedBotCommandSequences_[index],0U,managedBotFullUpdateSequences_[index]});
			scenario.rogue=cooperative.rogue;
			scenario.automaticFollowRequested=cooperative.available && cooperative.startFollowing;
			scenario.stopFollowingRequested=cooperative.available && cooperative.stopFollowing;
			scenario.autoFollowPermissionInferred=cooperative.humanPermissionInferred;
			if (!cooperative.rogueKnown && !wasFollowing) scenario.radio=nullptr;
			scenarioDecisions_[index] = scenarioPlanners_[index].update(scenario);
			updateBombGuardTarget(index, scenario, &scenarioDecisions_[index]);
			if (wasFollowing && !scenarioPlanners_[index].isFollowing()) teamworkPolicies_[index].followingStopped(context.timestamp);
			if (scenario.radio!=nullptr && scenario.radio->sequence>teamworkRadioApplied_[index] && scenario.radio->senderHuman &&
				scenario.radio->team==scenario.team && !scenario.busy && (!scenario.rogue || wasFollowing) &&
				context.timestamp-scenario.radio->receivedTime>=1.0f+2.0f*scenario.reactionSeconds+(scenario.rogue?2.0f:0.0f))
			{
				const auto &decision=scenarioDecisions_[index]; const auto command=scenario.radio->command;
				const bool allow=decision.action==compat::ScenarioAction::Follow && scenarioPlanners_[index].followLeader()==scenario.radio->sender;
				const bool inhibit=(command==compat::ScenarioRadioCommand::HoldPosition && decision.action==compat::ScenarioAction::Guard &&
					scenario.radio->position.knowledge!=compat::ScenarioKnowledge::Unknown) ||
					(command==compat::ScenarioRadioCommand::GoGoGo && decision.action==compat::ScenarioAction::Hunt) ||
					(command==compat::ScenarioRadioCommand::GetOut && decision.action==compat::ScenarioAction::Escape && scenario.bombPlantedKnown && scenario.bombPlanted);
				if ((allow || inhibit) && scenario.radioSenderValidation.known && scenario.radioSenderValidation.valid &&
					scenario.radioSenderValidation.actor==scenario.radio->sender)
				{
					world::ActorKey human=scenario.radio->sender;
					edict_t *entity=engineFunctions_->pfnPEntityOfEntIndex(static_cast<int>(human.slot));
					if(entity!=nullptr && !entity->free && (entity->v.flags&FL_CLIENT)!=0 &&
						(entity->v.flags&FL_FAKECLIENT)==0 && entity->v.team==observedTeam &&
						static_cast<std::uint32_t>(entity->serialnumber)==human.generation)
					{
						human.generation=static_cast<std::uint32_t>(entity->serialnumber)+1U;
						teamworkPolicies_[index].observeHumanPolicy(human,context.timestamp,allow);
						teamworkRadioApplied_[index]=scenario.radio->sequence;
					}
				}
			}
			const compat::ScenarioBehaviorDecision &scenarioDecision = scenarioDecisions_[index];
			stateBehavior.goalAvailable = scenarioDecision.hasTarget || stateBehavior.goalAvailable;
			stateBehavior.leaderAvailable = scenarioPlanners_[index].hasFollowingLeader(scenario);
			stateBehavior.objectiveActive = scenarioDecision.hasTarget || stateBehavior.objectiveActive;
			if(machine.state()==compat::CompatibilityStateId::DefuseBomb || machine.state()==compat::CompatibilityStateId::EscapeFromBomb)
				stateBehavior.objectiveActive=scenario.bombPlantedKnown && scenario.bombPlanted;
			stateBehavior.inInteractionRange = scenarioDecision.hasTarget && scenarioDecision.target.inUseRange;
			stateBehavior.guardObjectiveEnded=machine.state()==compat::CompatibilityStateId::Hide &&
				scenario.team==objectives::TeamRole::CounterTerrorist && scenario.humanDefusingKnown &&
				!scenario.humanDefusing && scenarioDecision.state==compat::CompatibilityStateId::MoveTo;
			if(machine.state()==compat::CompatibilityStateId::DefuseBomb && scenario.humanDefusing &&
				scenario.bomb.knowledge==compat::ScenarioKnowledge::Visible)
				stateBehavior.defuseReceipt=compat::DefuseReceipt::Other;
			const bool ownsCurrentTask=machine.state()!=compat::CompatibilityStateId::Idle &&
				machine.state()!=compat::CompatibilityStateId::Hunt && machine.state()!=compat::CompatibilityStateId::MoveTo;
			if (scenarioDecision.action != compat::ScenarioAction::None && scenarioDecision.state != compat::CompatibilityStateId::None &&
				!(ownsCurrentTask && scenarioDecision.state==compat::CompatibilityStateId::MoveTo) &&
				!(machine.state()==compat::CompatibilityStateId::DefuseBomb && scenarioDecision.state!=compat::CompatibilityStateId::DefuseBomb))
			{
				context.requestedState = scenarioDecision.state;
				context.requestTransition = machine.state() != context.requestedState;
				context.transitionId = compat::StateTransitionId::ExplicitStateChange;
				context.transitionReason = compat::StateTransitionReason::ObjectiveChanged;
			}
			const bool carryingC4 = observation.objective.carryingC4.isAvailable() &&
				observation.objective.carryingC4.value;
			const int team = resolveManagedBotTeam(index, observation);
			if (carryingC4 && team == 1)
			{
				const bool atBombsite = managedActorAtBombSite(index);
				const bool plantPhase = managedBombApproaches_[index].initialized &&
					managedBombApproaches_[index].mapGeneration == context.frame.mapGeneration &&
					managedBombApproaches_[index].roundGeneration == context.frame.roundGeneration &&
					managedBombApproaches_[index].finished;
				stateBehavior.objectiveActive = true;
				stateBehavior.inInteractionRange = plantPhase && atBombsite;
				context.requestedState = plantPhase && atBombsite ? compat::CompatibilityStateId::PlantBomb :
					plantPhase && stateBehavior.goalAvailable ? compat::CompatibilityStateId::MoveTo : compat::CompatibilityStateId::Hunt;
				context.requestTransition = machine.state() != context.requestedState;
				context.transitionId = compat::StateTransitionId::ExplicitStateChange;
				context.transitionReason = compat::StateTransitionReason::ObjectiveChanged;
			}
		}
	}
	compat::PublicActorState buyingObservation{};
	const float buyMinutes = readOptionalCvarFloat(engineFunctions_, "mp_buytime");
	const bool shouldBuy = !buyingComplete_[index] && context.alive && globals_ != nullptr &&
		std::isfinite(buyMinutes) && buyMinutes > 0.0f &&
		globals_->time <= roundFreezeStartTime_ + buyMinutes * 60.0f &&
		publicObservations_.read(context.actor, context.frame, &buyingObservation) &&
		buyingObservation.buyZoneKnown && buyingObservation.inBuyZone;
	if (shouldBuy || machine.state() == compat::CompatibilityStateId::Buy)
	{
		context.requestedState = shouldBuy ? compat::CompatibilityStateId::Buy : compat::CompatibilityStateId::Idle;
		context.requestTransition = machine.state() != context.requestedState;
		context.transitionId = compat::StateTransitionId::ExplicitStateChange;
		context.transitionReason = compat::StateTransitionReason::ObjectiveChanged;
	}
	else if (!context.requestTransition && !machine.attackOverlayActive())
	{
		const auto &belief = managedBotPerception_[index].snapshot();
		const auto &noise = belief.noise();
		stateBehavior.noiseAvailable = belief.isValid() &&
			belief.identity().frame.mapGeneration == context.frame.mapGeneration &&
			belief.identity().frame.roundGeneration == context.frame.roundGeneration &&
			noise.active && noise.ready && noise.timestampTick > investigatedNoiseTicks_[index];
		if (stateBehavior.noiseAvailable)
		{
			const float dx = before.origin.x - noise.position.x;
			const float dy = before.origin.y - noise.position.y;
			const float dz = before.origin.z - noise.position.z;
			if (dx * dx + dy * dy + dz * dz <= 50.0f * 50.0f)
			{
				investigatedNoiseTicks_[index] = noise.timestampTick;
				stateBehavior.noiseAvailable = false;
				stateBehavior.goalReached = true;
			}
			else if (machine.state() == compat::CompatibilityStateId::Hunt || machine.state() == compat::CompatibilityStateId::Idle)
			{
				context.requestedState = compat::CompatibilityStateId::InvestigateNoise;
				context.requestTransition = true;
				context.transitionId = compat::StateTransitionId::ExplicitStateChange;
				context.transitionReason = compat::StateTransitionReason::ObjectiveChanged;
			}
		}
	}
	const auto previousState=machine.state();
	const float previousStateTimestamp=machine.stateTimestamp();
	stateBehavior.navigation.actor=context.actor;
	stateBehavior.navigation.frame=context.frame;
	stateBehavior.navigation.fullUpdateSequence=context.fullUpdateSequence;
	managedStateInputs_[index]=stateBehavior;
	(void)machine.update(context);
	if(previousState!=machine.state())
	{
		managedNavigationSteps_[index]={};
		// Reference state entry computes or abandons its own path. Never carry
		// Idle's roam corridor into a newly assigned MoveTo task.
		managedBotMovement_[index].destroyPath();
		if(previousState==compat::CompatibilityStateId::DefuseBomb || previousState==compat::CompatibilityStateId::PlantBomb ||
			previousState==compat::CompatibilityStateId::Hide)
			managedBotMovement_[index].resetProgressMonitor();
		if(machine.state()==compat::CompatibilityStateId::Idle &&
			(previousState==compat::CompatibilityStateId::DefuseBomb || previousState==compat::CompatibilityStateId::PlantBomb))
		{
			managedBotObjectiveActions_[index]={};
			scenarioDecisions_[index]={};
			if(runtimeProfiler_.enabled())profiledLogConsole(runtimeProfiler_,pluginId_,
				"profile objectiveExit bot_id=%u frame=%u state_from=%s receipt=%u elapsed=%.3f",
				context.actor.slot,context.frame.tick,compat::toString(previousState),
				static_cast<unsigned>(stateBehavior.defuseReceipt),context.timestamp-previousStateTimestamp);
		}
	}
	if (stateBehavior.pathFailed && previousState==compat::CompatibilityStateId::MoveTo &&
		machine.state()==compat::CompatibilityStateId::Idle)
	{
		// The reference state's failure exit abandons this path/goal. Do not
		// let the outer objective loop immediately reuse its old MoveTo intent.
		managedBotMovement_[index].destroyPath();
		managedBotObjectiveTargets_[index] = {};
		scenarioDecisions_[index] = {};
		if (runtimeProfiler_.enabled())
			profiledLogConsole(runtimeProfiler_,pluginId_,
				"profile navigationFailure bot_id=%u frame=%u state_from=%s state_to=%s",
				context.actor.slot,context.frame.tick,compat::toString(previousState),compat::toString(machine.state()));
	}
	if (!stateBehavior.pathFailed || !machine.attackOverlayActive())
		managedNavigationFailures_[index] = {};
}

void PluginRuntime::resetManagedBotMovement()
{
	managedNavigationFailures_ = {};
	managedNavigationSteps_={};
	managedStateInputs_={};
	managedMoveSpeeds_={};
	managedCommandDiagnosticUpdates_={};
	managedBotRoamReservationBoard_.reset(
		lifecycle_.mapGeneration(), lifecycle_.roundGeneration());
	managedTeamObjectiveCoordinator_.reset(
		lifecycle_.mapGeneration(), lifecycle_.roundGeneration());
	managedTeamObjectiveAssignments_ = {};
	managedTeamObjectiveAssignmentMapGeneration_ =
		lifecycle_.mapGeneration();
	managedTeamObjectiveAssignmentRoundGeneration_ =
		lifecycle_.roundGeneration();
	managedTeamObjectiveAssignmentFrame_ = 0U;
	managedTeamObjectiveAssignmentsValid_ = false;
	managedTeamObjectivePathFailurePending_ = false;
	managedTeamObjectivePathFailureActor_ = {};
	managedTeamObjectivePathFailureGeneration_ = 0U;
	managedTeamObjectivePathFailureStartArea_ = 0U;
	managedTeamObjectivePathFailureFrame_ = 0U;
	managedBotObjectiveActions_ = {};
	for (std::size_t index = 0U; index < managedBotMovement_.size(); ++index)
	{
		managedBotMovement_[index].reset();
		movementFallLandingTrackers_[index].reset();
		managedBotCombat_[index] = combat::CombatController();
		managedBotObjectives_[index] = objectives::RoundObjectivePlanner();
		managedBotObjectiveTargets_[index] = {};
		managedBombApproaches_[index] = {};
		managedBotCurrentWeapons_[index] = {};
		managedBotObjectiveActions_[index] = {};
		managedBotStateMachines_[index] = compat::CompatibilityStateMachine();
		managedBotPerception_[index] = perception::PerceptionAssembler();
		managedBotTiming_[index] = runtime::BotTimingScheduler();
		managedBotJumpCrouch_[index].reset();
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
		movementGateDiagnosticTimes_.fill(std::numeric_limits<float>::quiet_NaN());
		movementGoalDiagnosticSeconds_.fill((std::numeric_limits<std::uint32_t>::max)());
		movementTraversalDiagnosticSeconds_.fill(
			(std::numeric_limits<std::uint32_t>::max)());
		movementTraversalPending_.fill({});
		movementDispatchFrames_.fill((std::numeric_limits<std::uint32_t>::max)());
	movementWasAirborne_.fill(false);
	movementReversalDiagnostics_.fill({});
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
			profiledLogConsole(runtimeProfiler_,
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

void PluginRuntime::logGoalAssignmentDiagnostic(
	std::size_t index,
	const runtime::MovementPhysicsState &before,
	const runtime::NavRoamDecision &decision,
	const nav::LocomotionIntent &intent,
	runtime::NavRoamResult roamResult,
	const nav::GroundLookaheadObservation &ground,
	const runtime::GroundLookaheadRoute &probeRoute)
{
	if (!runtimeProfiler_.enabled() || index >= movementGoalDiagnosticSeconds_.size() ||
		globals_ == nullptr || gpMetaUtilFuncs == nullptr ||
		gpMetaUtilFuncs->pfnLogConsole == nullptr || pluginId_ == nullptr)
	{
		return;
	}
	const double now = std::isfinite(static_cast<double>(globals_->time))
		? static_cast<double>(globals_->time)
		: 0.0;
	const std::uint32_t second = now <= 0.0
		? 0U
		: static_cast<std::uint32_t>(now);
	if (movementGoalDiagnosticSeconds_[index] == second)
	{
		return;
	}
	movementGoalDiagnosticSeconds_[index] = second;
	const ManagedObjectiveTargetCache &cache = managedBotObjectiveTargets_[index];
	const int rawTeam = before.team != 0 ? before.team : cache.rawTeam;
	const char *goalSource = decision.goalKind == runtime::NavGoalKind::Objective
		? "objective"
		: decision.goalKind == runtime::NavGoalKind::Roam ? "roam" : "none";
	profiledLogConsole(runtimeProfiler_,
		pluginId_,
		"profile goalAssignment bot_id=%u raw_team=%d effective_team=%d "
		"team_info=%d team_fresh=%d carrying_c4=%d state=%s task=%s "
		"disposition=UNAVAILABLE goal_present=%d goal_kind=%d goal_area=%u "
		"goal_position=(%.1f %.1f %.1f) goal_source=%s objective_target_id=%u "
		"goal_generation=%u cache_hit=%d cache_scope=actor corridor_goal_area=%u "
		"next_area=%u movement_target=(%.1f %.1f %.1f) current_area=%u",
		static_cast<unsigned int>(managedBotHandles_[index].actor.slot),
		rawTeam, cache.effectiveTeam,
		cache.teamInfoFresh ? cache.effectiveTeam : 0,
		cache.teamInfoFresh ? 1 : 0, cache.carryingBomb ? 1 : 0,
		compat::toString(managedBotStateMachines_[index].state()),
		compat::toString(managedBotStateMachines_[index].task()),
		decision.goalPresent ? 1 : 0, static_cast<int>(decision.goalKind),
		static_cast<unsigned int>(decision.goalArea), decision.goalPosition.x,
		decision.goalPosition.y, decision.goalPosition.z, goalSource,
		static_cast<unsigned int>(cache.selectedSiteIdentity),
		static_cast<unsigned int>(cache.generation), cache.lastCacheHit ? 1 : 0,
		static_cast<unsigned int>(decision.goalArea),
		static_cast<unsigned int>(intent.targetArea), decision.targetPosition.x,
		decision.targetPosition.y, decision.targetPosition.z,
		static_cast<unsigned int>(decision.currentArea));
	profiledLogConsole(runtimeProfiler_,
		pluginId_,
		"profile goalAssignmentCorrelation bot_id=%u actor_generation=%u "
		"frame=%u nav_update=%u goal_generation=%u reselection_reason=%s "
		"selection_strategy=%s objective_generation=%u selected_site_id=%u "
		"cache_hit=%d nav_result=%d stage=%d recompute_reason=%d path_result=%d "
		"failure_reason=%d path_search_calls=%u path_search_expanded=%u path_search_enqueues=%u path_search_failures=%u locomotion_result=%d path_sequence=%u "
				"route_type=%d path_cost=%.1f goal_kind=%d goal_area=%u current_area=%u "
				"reservation_fallback=%d fallback_reason=%u "
		"path_link=(%u->%u how=%u dir=%u) input=(%.2f %.2f %.2f) "
		"speed=%.1f observed_origin=(%.1f %.1f %.1f) grounded=%d",
		static_cast<unsigned int>(managedBotHandles_[index].actor.slot),
		static_cast<unsigned int>(managedBotHandles_[index].actor.actorGeneration),
		static_cast<unsigned int>(adapterFrameCount_), decision.fullUpdateSequence,
		decision.goalGeneration,
		goalSelectionReasonName(decision.goalSelectionReason),
		goalSelectionStrategyName(decision.goalSelectionStrategy),
		static_cast<unsigned int>(cache.generation),
		static_cast<unsigned int>(cache.selectedSiteIdentity),
		cache.lastCacheHit ? 1 : 0,
		static_cast<int>(roamResult),
		static_cast<int>(decision.stage),
		static_cast<int>(decision.recomputeReason),
		static_cast<int>(decision.pathResult),
		static_cast<int>(decision.failureReason),
		static_cast<unsigned int>(decision.pathSearchStats.searchCalls),
		static_cast<unsigned int>(decision.pathSearchStats.expandedUniqueAreas),
		static_cast<unsigned int>(decision.pathSearchStats.enqueueCount),
		static_cast<unsigned int>(decision.pathSearchStats.failureCount),
		static_cast<int>(decision.locomotionResult),
				decision.pathSequence,
				static_cast<int>(decision.routeType), decision.pathCost,
				static_cast<int>(decision.goalKind),
				static_cast<unsigned int>(decision.goalArea),
				static_cast<unsigned int>(decision.currentArea),
				decision.reservationFallback ? 1 : 0,
				static_cast<unsigned int>(decision.reservationFallbackReason),
		static_cast<unsigned int>(decision.linkFromArea),
		static_cast<unsigned int>(decision.linkToArea),
		static_cast<unsigned int>(decision.linkHow),
		static_cast<unsigned int>(decision.linkDirection),
		intent.direction.x, intent.direction.y, intent.direction.z, intent.speed,
		before.origin.x, before.origin.y, before.origin.z,
		before.grounded ? 1 : 0);
	profiledLogConsole(runtimeProfiler_,
		pluginId_,
		"profile groundRecovery bot_id=%u frame=%u now=%.3f "
		"full_update=%u route_frame=%u route_update=%u route_age=%.3f "
		"ground_valid=%d sampled_target=%u intent_target=%u "
		"sample_direction=(%.3f %.3f %.3f) feet_z=%.1f "
		"far80=(%d %d %.1f %.3f) near30=(%d %d %.1f %.3f) gap10=(%d %d %.1f %.3f) "
		"recovery_area=%u recovery_target=(%.1f %.1f %.1f) "
		"recovery_no_progress_updates=%u geometry_only=%d candidate_failed=%d failure_reason=%d",
		static_cast<unsigned int>(managedBotHandles_[index].actor.slot),
		static_cast<unsigned int>(adapterFrameCount_), globals_->time,
		managedBotFullUpdateSequences_[index], probeRoute.frame.tick,
		probeRoute.fullUpdateSequence, globals_->time - probeRoute.createdAt,
		ground.valid ? 1 : 0, ground.targetArea, intent.targetArea,
		ground.direction.x, ground.direction.y, ground.direction.z,
		decision.observationPosition.z,
		ground.far80.sampled ? 1 : 0, ground.far80.hasGround ? 1 : 0,
		ground.far80.floorZ, ground.far80.normalZ,
		ground.near30.sampled ? 1 : 0, ground.near30.hasGround ? 1 : 0,
		ground.near30.floorZ, ground.near30.normalZ,
		ground.gap10.sampled ? 1 : 0, ground.gap10.hasGround ? 1 : 0,
		ground.gap10.floorZ, ground.gap10.normalZ,
		decision.recoveryArea, decision.targetPosition.x,
		decision.targetPosition.y, decision.targetPosition.z,
		decision.recoveryNoProgressUpdates, decision.recoveryGeometryOnly ? 1 : 0,
		decision.recoveryCandidateFailed ? 1 : 0, static_cast<int>(decision.failureReason));
}

void PluginRuntime::logTraversalDiagnostic(
	std::size_t index,
	const runtime::MovementPhysicsState &before,
	const runtime::NavRoamDecision &decision,
	const nav::LocomotionIntent &intent,
	std::uint16_t buttons,
	const nav::LocomotionObservation &locomotion)
{
	if (!runtimeProfiler_.enabled() || index >= movementTraversalDiagnosticSeconds_.size() ||
		globals_ == nullptr || gpMetaUtilFuncs == nullptr ||
		gpMetaUtilFuncs->pfnLogConsole == nullptr || pluginId_ == nullptr)
	{
		return;
	}
	const double now = std::isfinite(static_cast<double>(globals_->time))
		? static_cast<double>(globals_->time)
		: 0.0;
	const std::uint32_t second = now <= 0.0
		? 0U
		: static_cast<std::uint32_t>(now);
	std::uint8_t currentAttributes = 0U;
	std::uint8_t nextAttributes = 0U;
	const nav::NavSnapshot navigation = navPublisher_.snapshot();
	const nav::NavDocument *document = navigation.document();
	const nav::NavArea *sourceArea = document == nullptr
		? nullptr : document->findArea(intent.currentArea);
	const nav::NavArea *targetArea = document == nullptr
		? nullptr : document->findArea(intent.targetArea);
	float sourceFloorZ = std::numeric_limits<float>::quiet_NaN();
	float targetFloorZ = intent.targetPosition.z;
	float horizontalGap = std::numeric_limits<float>::quiet_NaN();
	float dropHeight = std::numeric_limits<float>::quiet_NaN();
	if (sourceArea != nullptr && targetArea != nullptr)
	{
		sourceFloorZ = nav::surfaceZAt(
			*sourceArea, intent.targetPosition.x, intent.targetPosition.y);
		dropHeight = sourceFloorZ - targetFloorZ;
		const auto axisGap = [](float firstLo, float firstHi,
			float secondLo, float secondHi) noexcept
		{
			if (firstHi < secondLo)
			{
				return secondLo - firstHi;
			}
			if (secondHi < firstLo)
			{
				return firstLo - secondHi;
			}
			return 0.0f;
		};
		const float gapX = axisGap(
			sourceArea->extent.lo.x, sourceArea->extent.hi.x,
			targetArea->extent.lo.x, targetArea->extent.hi.x);
		const float gapY = axisGap(
			sourceArea->extent.lo.y, sourceArea->extent.hi.y,
			targetArea->extent.lo.y, targetArea->extent.hi.y);
		horizontalGap = std::hypot(gapX, gapY);
	}
	const bool descendingOrGapEdge = std::isfinite(dropHeight) && dropHeight > 8.0f;
	const bool horizontalGapEdge = std::isfinite(horizontalGap) && horizontalGap > 1.0f;
	if (intent.traversal == nav::TraversalAction::Walk &&
		!descendingOrGapEdge && !horizontalGapEdge)
	{
		return;
	}
	if (movementTraversalDiagnosticSeconds_[index] == second)
	{
		return;
	}
	movementTraversalDiagnosticSeconds_[index] = second;
	if (document != nullptr)
	{
		const nav::NavArea *current = document->findArea(decision.currentArea);
		const nav::NavArea *next = document->findArea(intent.targetArea);
		currentAttributes = current == nullptr ? 0U : current->attributes;
		nextAttributes = next == nullptr ? 0U : next->attributes;
	}
	const float distanceToTransition = std::hypot(
		decision.targetPosition.x - before.origin.x,
		decision.targetPosition.y - before.origin.y);
	profiledLogConsole(runtimeProfiler_,
		pluginId_,
		"profile traversal bot_id=%u map=%u round=%u actor_generation=%u "
		"current_area=%u next_area=%u "
		"connection_direction=%u connection_how=%u current_attributes=%u "
		"next_attributes=%u traversal_intent=%d jump_required=%d "
		"source_floor_z=%.1f target_floor_z=%.1f drop_height=%.1f "
		"horizontal_gap=%.1f safe_drop_available=%d max_safe_drop=%.1f "
		"target_point=(%.1f %.1f %.1f) distance_to_transition=%.1f "
		"before_grounded=%d before_ducked=%d before_velocity_z=%.1f "
		"buttons=%u IN_JUMP=%d "
		"stuck_time=%d recovery_state=%d corridor_index=%u",
		static_cast<unsigned int>(managedBotHandles_[index].actor.slot),
		static_cast<unsigned int>(lifecycle_.mapGeneration()),
		static_cast<unsigned int>(lifecycle_.roundGeneration()),
		static_cast<unsigned int>(managedBotHandles_[index].actor.actorGeneration),
		static_cast<unsigned int>(decision.currentArea),
		static_cast<unsigned int>(intent.targetArea),
		static_cast<unsigned int>(decision.linkDirection),
		static_cast<unsigned int>(decision.linkHow),
		static_cast<unsigned int>(currentAttributes),
		static_cast<unsigned int>(nextAttributes),
		static_cast<int>(intent.traversal),
		intent.traversal == nav::TraversalAction::Jump ? 1 : 0,
		sourceFloorZ, targetFloorZ, dropHeight, horizontalGap,
		locomotion.safeDropHeightAvailable ? 1 : 0,
		locomotion.maximumSafeDropHeight,
		decision.targetPosition.x, decision.targetPosition.y, decision.targetPosition.z,
		distanceToTransition, before.grounded ? 1 : 0, before.ducked ? 1 : 0,
		before.velocity.z, static_cast<unsigned int>(buttons),
		(buttons & static_cast<std::uint16_t>(IN_JUMP)) != 0U ? 1 : 0,
		decision.locomotionResult == nav::LocomotionResult::Stuck ? 1 : 0,
		static_cast<int>(decision.failureReason),
		static_cast<unsigned int>(decision.corridorIndex));
	profiledLogConsole(runtimeProfiler_,
		pluginId_,
		"profile traversalCorrelation bot_id=%u actor_generation=%u frame=%u "
		"map=%u round=%u "
		"nav_update=%u goal_generation=%u reselection_reason=%s "
		"selection_strategy=%s current_area=%u path_link=(%u->%u how=%u dir=%u) "
		"target_area=%u input=(%.2f %.2f %.2f) speed=%.1f buttons=%u "
		"observed_origin=(%.1f %.1f %.1f) grounded=%d",
		static_cast<unsigned int>(managedBotHandles_[index].actor.slot),
		static_cast<unsigned int>(managedBotHandles_[index].actor.actorGeneration),
		static_cast<unsigned int>(adapterFrameCount_),
		static_cast<unsigned int>(lifecycle_.mapGeneration()),
		static_cast<unsigned int>(lifecycle_.roundGeneration()),
		decision.fullUpdateSequence,
		decision.goalGeneration,
		goalSelectionReasonName(decision.goalSelectionReason),
		goalSelectionStrategyName(decision.goalSelectionStrategy),
		static_cast<unsigned int>(decision.currentArea),
		static_cast<unsigned int>(decision.linkFromArea),
		static_cast<unsigned int>(decision.linkToArea),
		static_cast<unsigned int>(decision.linkHow),
		static_cast<unsigned int>(decision.linkDirection),
		static_cast<unsigned int>(intent.targetArea),
		intent.direction.x, intent.direction.y, intent.direction.z, intent.speed,
		static_cast<unsigned int>(buttons),
		before.origin.x, before.origin.y, before.origin.z,
		before.grounded ? 1 : 0);
	PendingTraversalDiagnostic &pending = movementTraversalPending_[index];
	pending = {};
	pending.active = true;
	pending.actorGeneration = managedBotHandles_[index].actor.actorGeneration;
	pending.commandFrame = adapterFrameCount_;
	pending.navUpdateSequence = decision.fullUpdateSequence;
	pending.goalGeneration = decision.goalGeneration;
	pending.mapGeneration = lifecycle_.mapGeneration();
	pending.roundGeneration = lifecycle_.roundGeneration();
	pending.corridorIndex = decision.corridorIndex;
	pending.goalSelectionReason = decision.goalSelectionReason;
	pending.goalSelectionStrategy = decision.goalSelectionStrategy;
	pending.currentArea = decision.currentArea;
	pending.targetArea = intent.targetArea;
	pending.targetPosition = decision.targetPosition;
	pending.intentDirection = intent.direction;
	pending.intentSpeed = intent.speed;
	pending.sourceFloorZ = sourceFloorZ;
	pending.targetFloorZ = targetFloorZ;
	pending.dropHeight = dropHeight;
	pending.horizontalGap = horizontalGap;
	pending.maximumSafeDropHeight = locomotion.maximumSafeDropHeight;
	pending.safeDropHeightAvailable = locomotion.safeDropHeightAvailable;
	pending.linkDirection = decision.linkDirection;
	pending.linkHow = decision.linkHow;
	pending.action = intent.traversal;
	pending.buttons = buttons;
	pending.startPosition = before.origin;
	pending.startVelocityZ = before.velocity.z;
	pending.startGrounded = before.grounded;
	pending.startHealth = before.health;
}

void PluginRuntime::logTraversalOutcome(
	std::size_t index,
	const runtime::MovementPhysicsState &after,
	const runtime::NavRoamDecision &decision,
	const runtime::LandingDamageObservation &landingFeedback)
{
	if (!runtimeProfiler_.enabled() || index >= movementTraversalPending_.size())
	{
		return;
	}
	PendingTraversalDiagnostic &pending = movementTraversalPending_[index];
	if (!pending.active)
	{
		return;
	}
	const FakeClientHandle &handle = managedBotHandles_[index];
	if (handle.actor.actorGeneration != pending.actorGeneration)
	{
		pending.active = false;
		return;
	}
	if (gpMetaUtilFuncs == nullptr || gpMetaUtilFuncs->pfnLogConsole == nullptr ||
		pluginId_ == nullptr)
	{
		return;
	}
	const float healthDelta = pending.startHealth - after.health;
	const std::uint32_t corridorAdvance = decision.corridorIndex >= pending.corridorIndex
		? decision.corridorIndex - pending.corridorIndex
		: 0U;
	profiledLogConsole(runtimeProfiler_,
		pluginId_,
		"profile traversalResult bot_id=%u actor_generation=%u map=%u round=%u command_frame=%u "
		"observed_frame=%u nav_update=%u goal_generation=%u reselection_reason=%s "
		"selection_strategy=%s from_area=%u target_area=%u observed_area=%u "
		"path_link=(%u->%u how=%u dir=%u) action=%d buttons=%u IN_JUMP=%d "
		"source_floor_z=%.1f target_floor_z=%.1f drop_height=%.1f "
		"horizontal_gap=%.1f safe_drop_available=%d max_safe_drop=%.1f "
		"input=(%.2f %.2f %.2f) speed=%.1f "
		"start=(%.1f %.1f %.1f) after_move=(%.1f %.1f %.1f) "
		"start_grounded=%d grounded_after=%d velocity_z_before=%.1f velocity_z_after=%.1f "
		"health_before=%.1f health_after=%.1f health_delta=%.1f "
		"landing_damage_available=%d landing_damage=%.1f "
		"corridor_before=%u corridor_after=%u corridor_advance=%u",
		static_cast<unsigned int>(handle.actor.slot),
		static_cast<unsigned int>(handle.actor.actorGeneration),
		pending.mapGeneration, pending.roundGeneration,
		pending.commandFrame, static_cast<unsigned int>(adapterFrameCount_),
		pending.navUpdateSequence, pending.goalGeneration,
		goalSelectionReasonName(pending.goalSelectionReason),
		goalSelectionStrategyName(pending.goalSelectionStrategy),
		static_cast<unsigned int>(pending.currentArea),
		static_cast<unsigned int>(pending.targetArea),
		static_cast<unsigned int>(decision.currentArea),
		static_cast<unsigned int>(pending.currentArea),
		static_cast<unsigned int>(pending.targetArea),
		static_cast<unsigned int>(pending.linkHow),
		static_cast<unsigned int>(pending.linkDirection),
		static_cast<int>(pending.action),
		static_cast<unsigned int>(pending.buttons),
		(pending.buttons & static_cast<std::uint16_t>(IN_JUMP)) != 0U ? 1 : 0,
		pending.sourceFloorZ, pending.targetFloorZ, pending.dropHeight,
		pending.horizontalGap, pending.safeDropHeightAvailable ? 1 : 0,
		pending.maximumSafeDropHeight,
		pending.intentDirection.x, pending.intentDirection.y,
		pending.intentDirection.z, pending.intentSpeed,
		pending.startPosition.x, pending.startPosition.y, pending.startPosition.z,
		after.origin.x, after.origin.y, after.origin.z,
		pending.startGrounded ? 1 : 0, after.grounded ? 1 : 0,
		pending.startVelocityZ, after.velocity.z,
		pending.startHealth, after.health, healthDelta,
		landingFeedback.hasLandingDamage ? 1 : 0,
		landingFeedback.landingDamage,
		static_cast<unsigned int>(pending.corridorIndex),
		static_cast<unsigned int>(decision.corridorIndex), corridorAdvance);
	pending.active = false;
}

void PluginRuntime::logMovementReversalDiagnostic(
	std::size_t index,
	const runtime::MovementPhysicsState &before,
	const runtime::NavRoamDecision &decision,
	const nav::LocomotionIntent &intent)
{
	if (!runtimeProfiler_.enabled() ||
			index >= movementReversalDiagnostics_.size())
	{
		return;
	}
	MovementReversalDiagnosticState &state = movementReversalDiagnostics_[index];
	const runtime::PhysicsVector currentDirection = {
		intent.direction.x, intent.direction.y, intent.direction.z};
	const bool reversed = state.valid && runtime::isMovementDirectionReversal(
		state.direction, currentDirection);
	if (reversed)
	{
		if (state.reversalCount != (std::numeric_limits<std::uint32_t>::max)())
		{
			++state.reversalCount;
		}
		if (gpMetaUtilFuncs != nullptr && gpMetaUtilFuncs->pfnLogConsole != nullptr &&
				pluginId_ != nullptr)
		{
			profiledLogConsole(runtimeProfiler_,
				pluginId_,
				"profile movementReversal bot_id=%u frame=%u reversal_count=%u "
				"previous_feet=(%.1f %.1f %.1f) current_feet=(%.1f %.1f %.1f) "
				"previous_local_target=(%.1f %.1f %.1f) "
				"current_local_target=(%.1f %.1f %.1f) "
				"previous_direction=(%.3f %.3f %.3f) current_direction=(%.3f %.3f %.3f) "
				"previous_segment=(%u->%u) current_segment=(%u->%u) "
				"previous_path_sequence=%u path_sequence=%u corridor_index=%u "
				"link=(%u->%u how=%u dir=%u) recompute_reason=%d traversal=%d "
				"grounded=%d velocity=(%.1f %.1f %.1f)",
				static_cast<unsigned int>(managedBotHandles_[index].actor.slot),
				static_cast<unsigned int>(adapterFrameCount_),
				static_cast<unsigned int>(state.reversalCount),
				state.feetPosition.x, state.feetPosition.y, state.feetPosition.z,
				before.origin.x, before.origin.y, before.origin.z,
				state.localTarget.x, state.localTarget.y, state.localTarget.z,
				intent.targetPosition.x, intent.targetPosition.y,
				intent.targetPosition.z,
				state.direction.x, state.direction.y, state.direction.z,
				currentDirection.x, currentDirection.y, currentDirection.z,
				static_cast<unsigned int>(state.currentArea),
				static_cast<unsigned int>(state.targetArea),
				static_cast<unsigned int>(decision.currentArea),
				static_cast<unsigned int>(intent.targetArea),
				static_cast<unsigned int>(state.pathSequence),
				static_cast<unsigned int>(decision.pathSequence),
				static_cast<unsigned int>(decision.corridorIndex),
				static_cast<unsigned int>(decision.linkFromArea),
				static_cast<unsigned int>(decision.linkToArea),
				static_cast<unsigned int>(decision.linkHow),
				static_cast<unsigned int>(decision.linkDirection),
				static_cast<int>(decision.recomputeReason),
				static_cast<int>(intent.traversal), before.grounded ? 1 : 0,
				before.velocity.x, before.velocity.y, before.velocity.z);
		}
	}
	else if (state.valid)
	{
		state.reversalCount = 0U;
	}
	state.valid = true;
	state.feetPosition = before.origin;
	state.direction = currentDirection;
	state.localTarget = intent.targetPosition;
	state.currentArea = decision.currentArea;
	state.targetArea = intent.targetArea;
	state.pathSequence = decision.pathSequence;
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
				profiledLogConsole(runtimeProfiler_,
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
				managedProfiles_[slotIndex] = profile;
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
					managedBotMovement_[slotIndex].reset();
					managedBotTiming_[slotIndex] = runtime::BotTimingScheduler();
					resetManagedBotCommandTemplate(slotIndex);
			managedBotCommandSequences_[slotIndex] = 0U;
					movementDiagnosticSamples_[slotIndex] = 0U;
		movementDiagnosticAttempts_[slotIndex] = false;
		movementReadyLogged_[slotIndex] = false;
		movementGateDiagnosticTimes_[slotIndex] = std::numeric_limits<float>::quiet_NaN();
		movementGoalDiagnosticSeconds_[slotIndex] =
			(std::numeric_limits<std::uint32_t>::max)();
		movementTraversalDiagnosticSeconds_[slotIndex] =
			(std::numeric_limits<std::uint32_t>::max)();
		movementTraversalPending_[slotIndex] = {};
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
	playerAvoidance_[index].reset();
	managedBotTeamNumbers_[index] = 0U;
	publicTeamKnown_[index] = false;
	const world::ActorKey actor = {handle.actor.slot, handle.actor.actorGeneration};
	managedBotCombat_[index] = combat::CombatController(actor);
	managedBotObjectives_[index] = objectives::RoundObjectivePlanner(actor);
	managedBotStateMachines_[index] = compat::CompatibilityStateMachine(actor);
	managedBotFullUpdateSequences_[index] = 0U;
	managedBotStateRounds_[index] = lifecycle_.roundGeneration();
	managedBotWasDead_[index] = false;
	managedBotSlots_[index] = true;
	publicObservations_.bind(actor,
		{lifecycle_.mapGeneration(), lifecycle_.roundGeneration(), adapterFrameCount_});
	reactionQueues_[index].reset();
	recognizedTargets_[index] = {};
	recognitionSequences_[index] = 0U;
	lastCombatTimes_[index] = 0.0f;
	buyControllers_[index].reset();
	buyingComplete_[index] = false;
	escapingWithKnife_[index] = false;
	buyReceiptTimeoutLogged_[index] = false;
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
	playerAvoidance_[index].reset();
	publicObservations_.resetSlot(static_cast<std::uint32_t>(index + 1U));
	publicBombRadars_[index].reset();
	bombGuardPlanners_[index].reset();
	scenarioRadio_[index] = {};
	publicScoreStatuses_[index] = {};
	bombNoticeKnown_[index] = false;
	bombNoticePlanted_[index] = false;
	publicBombClocks_[index].reset();
	scenarioEscapeAreas_[index] = 0U;
	managedNavigationFailures_[index] = {};
	bombSearchMemory_[index].reset();
	managedNavigationSteps_[index]={};
	managedMoveSpeeds_[index]=0.0f;
	managedCommandDiagnosticUpdates_[index]=0U;
	managedStateInputs_[index]={};
	bombSearchTargets_[index] = {};
	teamworkPolicies_[index].reset();
	teamworkRadioApplied_[index] = 0U;
	deliveredSoundSequences_[index] = soundSequence_;
	investigatedNoiseTicks_[index] = 0U;
	weaponActions_[index].reset();
	reactionQueues_[index].reset();
	recognizedTargets_[index] = {};
	objectiveKnowledge_[index] = {};
	buyControllers_[index].reset();
	managedBotTeamNumbers_[index] = 0U;
	publicTeamKnown_[index] = false;
	managedBotTiming_[index] = runtime::BotTimingScheduler();
	managedBotJumpCrouch_[index].reset();
	resetManagedBotCommandTemplate(index);
		managedBotMovement_[index].reset();
		movementFallLandingTrackers_[index].reset();
		managedBotCombat_[index] = combat::CombatController();
	managedBotObjectives_[index] = objectives::RoundObjectivePlanner();
	managedBotObjectiveTargets_[index] = {};
	managedBombApproaches_[index] = {};
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
		movementGateDiagnosticTimes_[index] = std::numeric_limits<float>::quiet_NaN();
		movementGoalDiagnosticSeconds_[index] =
			(std::numeric_limits<std::uint32_t>::max)();
		movementTraversalDiagnosticSeconds_[index] =
			(std::numeric_limits<std::uint32_t>::max)();
		movementTraversalPending_[index] = {};
		movementDispatchFrames_[index] = (std::numeric_limits<std::uint32_t>::max)();
	movementWasAirborne_[index] = false;
	movementReversalDiagnostics_[index] = {};
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
		profiledLogConsole(runtimeProfiler_,
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
			profiledLogConsole(runtimeProfiler_,
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
			profiledLogConsole(runtimeProfiler_,
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
