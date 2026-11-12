#ifndef ASTRABOT_ADAPTER_BOT_RUNTIME_SUPPORT_HPP
#define ASTRABOT_ADAPTER_BOT_RUNTIME_SUPPORT_HPP
#include "astrabot/compat/bomb_navigation.hpp"
#include "astrabot/metamod/movement_execution_gate.hpp"
#include "astrabot/objectives/bomb_site_route_selection.hpp"
#include "bot_runtime_types.hpp"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <vector>
namespace astrabot
{
namespace metamod
{
namespace runtime_detail
{
struct ObjectiveCandidateKey
{
	int entityIndex;
	std::uint32_t siteIdentity;
	nav::AreaId targetArea;

	bool operator==(const ObjectiveCandidateKey &other) const
	{
		return entityIndex == other.entityIndex && siteIdentity == other.siteIdentity &&
			   targetArea == other.targetArea;
	}
};

constexpr float kRadiansToDegrees = 57.29577951308232f;
constexpr float kStandingHalfHumanHeight = 36.0f;
constexpr float kGroundProbeDepth = 80.0f;
constexpr float kMaximumObjectiveDistance = 10000.0f;
constexpr float kDefaultManagedBotMaxSpeed = 240.0f;
constexpr std::uint32_t kRespawnSettleFrames = 2U;
constexpr std::uint8_t kMovementPhysicsLogLimit = 64U;
constexpr int kShowMenuMessageId = 96;
constexpr int kVguiMenuMessageId = 114;
constexpr int kTeamInfoMessageId = 86;
constexpr std::size_t kPerformanceCvarCount = 3U;
inline const char *actionKindName(ActionKind kind)
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

inline const char *runtimeModeName(compat::RuntimeMode mode)
{
	return mode == compat::RuntimeMode::Enhanced ? "enhanced" : "compatibility";
}

inline const char *goalSelectionReasonName(runtime::NavGoalSelectionReason reason)
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

inline const char *goalSelectionStrategyName(runtime::NavGoalSelectionStrategy strategy)
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

inline RuntimePathSearchCaller navSearchCaller(const runtime::NavRoamDecision &decision)
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

inline const char *jumpCrouchTransitionName(runtime::JumpCrouchTransition transition)
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

inline bool equalsIgnoreCase(const char *left, const char *right)
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

inline void setMetaResult(META_RES result)
{
	if (gpMetaGlobals != nullptr)
	{
		gpMetaGlobals->mres = result;
	}
}

inline bool boundedStringLength(const char *value, std::size_t maximum, std::size_t *length)
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
inline bool copyCommandText(std::array<char, Capacity> *destination, const char *source)
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

inline bool buildMapPath(const char *gameDirectory, const char *mapName, const char *extension,
						 char *path, std::size_t pathCapacity)
{
	std::size_t gameDirectoryLength = 0U;
	std::size_t mapNameLength = 0U;
	std::size_t extensionLength = 0U;
	if (!boundedStringLength(gameDirectory, pathCapacity, &gameDirectoryLength) ||
		!boundedStringLength(mapName, pathCapacity, &mapNameLength) ||
		!boundedStringLength(extension, pathCapacity, &extensionLength) || path == nullptr ||
		gameDirectoryLength == 0U)
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

inline bool buildGameFilePath(const char *gameDirectory, const char *fileName, char *path,
							  std::size_t pathCapacity)
{
	std::size_t gameDirectoryLength = 0U;
	std::size_t fileNameLength = 0U;
	if (!boundedStringLength(gameDirectory, pathCapacity, &gameDirectoryLength) ||
		!boundedStringLength(fileName, pathCapacity, &fileNameLength) || path == nullptr ||
		gameDirectoryLength == 0U || fileNameLength == 0U)
	{
		return false;
	}

	const bool needsSeparator = gameDirectory[gameDirectoryLength - 1U] != '/' &&
								gameDirectory[gameDirectoryLength - 1U] != '\\';
	const std::size_t separatorLength = needsSeparator ? 1U : 0U;
	const std::size_t requiredLength = gameDirectoryLength + separatorLength + fileNameLength;
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

inline float movementYaw(const nav::NavVector &direction)
{
	if (!std::isfinite(direction.x) || !std::isfinite(direction.y))
	{
		return 0.0f;
	}
	return std::atan2(direction.y, direction.x) * kRadiansToDegrees;
}

inline float readOptionalCvarFloat(enginefuncs_t *engineFunctions, const char *name)
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

inline void addNavSearchStats(nav::NavSearchStats *total, const nav::NavSearchStats &sample)
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

inline const char *runtimeProfilerStageName(RuntimeProfilerStage stage)
{
	static const char *const names[] = {"StartFrame",
										"RegistryLifecycle",
										"Observation",
										"Vision",
										"TraceLine",
										"WorldPublish",
										"Perception",
										"RuntimeInput",
										"RuntimeFullUpdate",
										"FullUpdateObjective",
										"ObjectiveCandidateNavEvaluate",
										"NavCurrentAreaLookup",
										"PathSearch",
										"PathRecompute",
										"NavMovement",
										"MovementDispatch",
										"TraceSerialization",
										"RunPlayerMove",
										"SynchronousLog",
										"ScenarioUpdate",
										"BombGuardSelection",
										"PathCostContext"};
	const std::size_t index = static_cast<std::size_t>(stage);
	return index < static_cast<std::size_t>(RuntimeProfilerStage::Count) ? names[index] : "Unknown";
}

inline const char *runtimePathSearchCallerName(RuntimePathSearchCaller caller)
{
	static const char *const names[] = {"CorridorInitial",
										"CorridorRecompute",
										"ObjectiveCandidateEvaluation",
										"BombSiteSelection",
										"BombTargetSelection",
										"RoamGoalEvaluation",
										"RecoveryAreaEvaluation",
										"TraversalRecovery",
										"OffPathRecovery",
										"DebugOrDiagnostic",
										"Unknown"};
	const std::size_t index = static_cast<std::size_t>(caller);
	return index < static_cast<std::size_t>(RuntimePathSearchCaller::Count) ? names[index]
																			: "Unknown";
}

template <typename... Args>
inline void profiledLogConsole(RuntimeProfiler &profiler, plid_t pluginId, const char *format,
							   Args... args)
{
	RuntimeProfilerScope logScope(profiler, RuntimeProfilerStage::SynchronousLog);
	gpMetaUtilFuncs->pfnLogConsole(pluginId, format, args...);
}

inline bool entityObjectiveBounds(const edict_t *entity, nav::NavExtent *extent)
{
	if (entity == nullptr || extent == nullptr)
	{
		return false;
	}
	const bool hasAbsoluteBounds =
		std::isfinite(entity->v.absmin.x) && std::isfinite(entity->v.absmin.y) &&
		std::isfinite(entity->v.absmin.z) && std::isfinite(entity->v.absmax.x) &&
		std::isfinite(entity->v.absmax.y) && std::isfinite(entity->v.absmax.z) &&
		entity->v.absmax.x >= entity->v.absmin.x && entity->v.absmax.y >= entity->v.absmin.y &&
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
		entity->v.maxs.x >= entity->v.mins.x && entity->v.maxs.y >= entity->v.mins.y &&
		entity->v.maxs.z >= entity->v.mins.z &&
		(entity->v.maxs.x - entity->v.mins.x > 1.0f || entity->v.maxs.y - entity->v.mins.y > 1.0f ||
		 entity->v.maxs.z - entity->v.mins.z > 1.0f);
	if (!hasLocalBounds)
	{
		const bool hasWorldSize =
			std::isfinite(entity->v.size.x) && std::isfinite(entity->v.size.y) &&
			std::isfinite(entity->v.size.z) &&
			(entity->v.size.x > 1.0f || entity->v.size.y > 1.0f || entity->v.size.z > 1.0f);
		if (!hasWorldSize)
		{
			return false;
		}
		extent->lo = {entity->v.origin.x - entity->v.size.x * 0.5f,
					  entity->v.origin.y - entity->v.size.y * 0.5f,
					  entity->v.origin.z - entity->v.size.z * 0.5f};
		extent->hi = {entity->v.origin.x + entity->v.size.x * 0.5f,
					  entity->v.origin.y + entity->v.size.y * 0.5f,
					  entity->v.origin.z + entity->v.size.z * 0.5f};
		return true;
	}
	extent->lo = {entity->v.origin.x + entity->v.mins.x, entity->v.origin.y + entity->v.mins.y,
				  entity->v.origin.z + entity->v.mins.z};
	extent->hi = {entity->v.origin.x + entity->v.maxs.x, entity->v.origin.y + entity->v.maxs.y,
				  entity->v.origin.z + entity->v.maxs.z};
	return true;
}

inline nav::NavVector entityObjectiveCenter(const edict_t *entity)
{
	nav::NavExtent extent = {};
	if (entityObjectiveBounds(entity, &extent))
	{
		return {(extent.lo.x + extent.hi.x) * 0.5f, (extent.lo.y + extent.hi.y) * 0.5f,
				(extent.lo.z + extent.hi.z) * 0.5f};
	}
	return entity == nullptr
			   ? nav::NavVector{0.0f, 0.0f, 0.0f}
			   : nav::NavVector{entity->v.origin.x, entity->v.origin.y, entity->v.origin.z};
}

inline bool isBombTargetClassname(const char *classname)
{
	return ActionAdapter::isBombTargetClassname(classname);
}

inline bool objectiveSiteExtent(const edict_t *entity, const char *classname,
								nav::NavExtent *extent)
{
	if (entity == nullptr || extent == nullptr)
	{
		return false;
	}
	if (ActionAdapter::isLegacyInfoBombTargetClassname(classname))
	{
		const nav::NavVector center = entityObjectiveCenter(entity);
		if (!std::isfinite(center.x) || !std::isfinite(center.y) || !std::isfinite(center.z))
		{
			return false;
		}
		extent->lo = {center.x - ActionAdapter::kLegacyBombTargetRadius,
					  center.y - ActionAdapter::kLegacyBombTargetRadius,
					  center.z - ActionAdapter::kLegacyBombTargetRadius};
		extent->hi = {center.x + ActionAdapter::kLegacyBombTargetRadius,
					  center.y + ActionAdapter::kLegacyBombTargetRadius,
					  center.z + ActionAdapter::kLegacyBombTargetRadius};
		return true;
	}
	return entityObjectiveBounds(entity, extent);
}

inline void logObjectiveTargetDiagnostic(
	RuntimeProfiler &profiler, plid_t pluginId, std::uint32_t targetId, int entityIndex,
	const char *classname, std::uint32_t registeredSites,
	std::uint32_t registeredFuncBombTargetSites, std::uint32_t registeredInfoBombTargetSites,
	const edict_t *entity, const nav::NavVector &center, nav::AreaId nearestNavArea,
	nav::AreaId candidateNavArea, std::uint32_t candidateAreaSamples,
	std::uint32_t uniqueCandidateAreas, std::uint32_t reachableCandidateAreas,
	float bestReachableCost, const char *cacheState, bool valid, const char *rejectionReason)
{
	if (pluginId == nullptr || gpMetaUtilFuncs == nullptr ||
		gpMetaUtilFuncs->pfnLogConsole == nullptr || entity == nullptr)
	{
		return;
	}
	profiledLogConsole(
		profiler, pluginId,
		"profile objectiveTarget target_id=%u entity_index=%d classname=%s "
		"registered_site_count=%u registered_func_bomb_target_count=%u "
		"registered_info_bomb_target_count=%u "
		"origin=(%.1f %.1f %.1f) mins=(%.1f %.1f %.1f) "
		"maxs=(%.1f %.1f %.1f) center=(%.1f %.1f %.1f) "
		"nearest_nav_area=%u candidate_nav_area=%u site_identity=%u "
		"candidate_area_samples=%u unique_nav_candidate_areas=%u "
		"reachable_nav_candidate_areas=%u best_reachable_cost=%.1f "
		"cache_state=%s valid=%d rejection_reason=%s",
		static_cast<unsigned int>(targetId), entityIndex, classname == nullptr ? "" : classname,
		static_cast<unsigned int>(registeredSites),
		static_cast<unsigned int>(registeredFuncBombTargetSites),
		static_cast<unsigned int>(registeredInfoBombTargetSites), entity->v.origin.x,
		entity->v.origin.y, entity->v.origin.z, entity->v.mins.x, entity->v.mins.y,
		entity->v.mins.z, entity->v.maxs.x, entity->v.maxs.y, entity->v.maxs.z, center.x, center.y,
		center.z, static_cast<unsigned int>(nearestNavArea),
		static_cast<unsigned int>(candidateNavArea), static_cast<unsigned int>(targetId),
		static_cast<unsigned int>(candidateAreaSamples),
		static_cast<unsigned int>(uniqueCandidateAreas),
		static_cast<unsigned int>(reachableCandidateAreas), bestReachableCost,
		cacheState == nullptr ? "unknown" : cacheState, valid ? 1 : 0,
		rejectionReason == nullptr ? "unknown" : rejectionReason);
}

inline std::uint8_t movementMilliseconds(const globalvars_t *globals)
{
	if (globals == nullptr || !std::isfinite(globals->frametime) || globals->frametime <= 0.0f)
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

inline int localFileSize(const char *path)
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
} // namespace runtime_detail
} // namespace metamod
} // namespace astrabot
#endif
