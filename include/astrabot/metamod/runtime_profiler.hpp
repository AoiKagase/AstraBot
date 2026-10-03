#ifndef ASTRABOT_ADAPTER_METAMOD_RUNTIME_PROFILER_HPP
#define ASTRABOT_ADAPTER_METAMOD_RUNTIME_PROFILER_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <chrono>

namespace astrabot
{
namespace metamod
{
enum class RuntimeProfilerStage : std::uint8_t
{
	StartFrame,
	RegistryLifecycle,
	Observation,
	Vision,
	TraceLine,
	WorldPublish,
	Perception,
	RuntimeInput,
	RuntimeFullUpdate,
	FullUpdateObjective,
	ObjectiveCandidateNavEvaluate,
	NavCurrentAreaLookup,
	PathSearch,
	PathRecompute,
	NavMovement,
	MovementDispatch,
	TraceSerialization,
	RunPlayerMove,
	SynchronousLog,
	Count
};

enum class RuntimePathSearchCaller : std::uint8_t
{
	CorridorInitial,
	CorridorRecompute,
	ObjectiveCandidateEvaluation,
	BombSiteSelection,
	BombTargetSelection,
	RoamGoalEvaluation,
	RecoveryAreaEvaluation,
	TraversalRecovery,
	OffPathRecovery,
	DebugOrDiagnostic,
	Unknown,
	Count
};

struct RuntimePathSearchAggregate
{
	std::uint64_t calls;
	std::uint64_t totalUsec;
	std::uint64_t maxUsec;
	std::uint64_t expandedAreas;
	std::uint64_t enqueues;
};

struct RuntimeObjectiveSelectionStats
{
	std::uint32_t bombSites;
	std::uint32_t funcBombTargetSites;
	std::uint32_t infoBombTargetSites;
	std::uint32_t candidateAreas;
	std::uint32_t uniqueCandidateAreas;
	std::uint32_t candidateQueries;
	std::uint32_t duplicateCandidateAreas;
	std::uint32_t selectedGoalArea;
	std::uint32_t registeredSites;
	std::uint32_t registeredFuncBombTargetSites;
	std::uint32_t registeredInfoBombTargetSites;
	bool registeredSiteCountsAvailable;
	std::uint32_t evaluatedSites;
	std::uint32_t cacheHits;
	std::uint32_t selectedSiteIdentity;
};

struct RuntimeProfilerStageStats
{
	// Timings are inclusive: nested stage scopes are intentionally reported
	// in both their parent and child stages for boundary attribution.
	std::uint32_t calls;
	std::uint64_t totalUsec;
	std::uint64_t maxUsec;
};

struct RuntimeProfilerReport
{
	double windowSeconds;
	double wallWindowSeconds;
	std::uint32_t frameCount;
	std::array<RuntimeProfilerStageStats,
		static_cast<std::size_t>(RuntimeProfilerStage::Count)> stages;
	std::uint32_t aliveBots;
	std::uint64_t traceLineCalls;
	std::uint64_t visibilityCandidates;
	std::uint64_t fovChecks;
	std::uint64_t losChecks;
	std::uint64_t bodyProbeCalls;
	std::uint64_t pathSearches;
	std::uint64_t pathSearchSuccesses;
	std::uint64_t pathSearchFailures;
	std::uint64_t pathRecomputes;
	std::uint64_t pathExpandedAreas;
	std::uint64_t pathEnqueues;
	std::uint64_t pathReopens;
	std::uint64_t pathStaleQueueEntries;
	std::uint64_t pathEqualCostReplacements;
	std::uint64_t pathSearchTotalUsec;
	std::uint64_t pathSearchMaxUsec;
	std::uint64_t pathFirstSearchId;
	std::uint64_t pathLastSearchId;
	std::uint64_t runPlayerMoves;
	std::array<RuntimePathSearchAggregate,
		static_cast<std::size_t>(RuntimePathSearchCaller::Count)>
		pathSearchByCaller;
	std::uint64_t duplicateSearchSameFullUpdate;
	std::uint64_t duplicateSearchSameObjectiveGeneration;
	std::uint64_t uniqueSearchKeys;
	std::uint64_t objectiveBombSites;
	std::uint64_t objectiveFuncBombTargetSites;
	std::uint64_t objectiveInfoBombTargetSites;
	std::uint64_t objectiveCandidateAreas;
	std::uint64_t objectiveUniqueCandidateAreas;
	std::uint64_t objectiveCandidateQueries;
	std::uint64_t objectiveDuplicateCandidateAreas;
	std::uint64_t objectiveRegisteredSites;
	std::uint64_t objectiveRegisteredFuncBombTargetSites;
	std::uint64_t objectiveRegisteredInfoBombTargetSites;
	std::uint64_t objectiveEvaluatedSites;
	std::uint64_t objectiveCacheHits;
	std::uint32_t selectedObjectiveSiteIdentity;
	std::uint32_t selectedObjectiveGoalArea;
};

class RuntimeProfiler
{
public:
	RuntimeProfiler() noexcept;

	void setEnabled(bool enabled, double nowSeconds) noexcept;
	bool enabled() const noexcept;
	void setAliveBots(std::uint32_t aliveBots) noexcept;
	void recordVisionCandidate() noexcept;
	void recordVisionFovCheck() noexcept;
	void recordVisionLosCheck() noexcept;
	void recordBodyProbe() noexcept;
	void record(RuntimeProfilerStage stage, std::uint64_t elapsedUsec) noexcept;
	void recordTraceLine(
		std::uint64_t calls,
		std::uint64_t visibilityCandidates,
		std::uint64_t bodyProbes) noexcept;
	void recordPathSearch(bool success, bool requested) noexcept;
	void recordPathSearchStats(
		bool success,
		bool requested,
		std::uint32_t expandedAreas,
		std::uint32_t enqueues,
		std::uint32_t reopens,
		std::uint32_t staleQueueEntries,
		std::uint32_t equalCostReplacements) noexcept;
	void recordPathSearchResults(
		bool requested,
		std::uint32_t searchCalls,
		std::uint32_t successCount,
		std::uint32_t failureCount,
		std::uint32_t expandedAreas,
		std::uint32_t enqueues,
		std::uint32_t reopens,
		std::uint32_t staleQueueEntries,
		std::uint32_t equalCostReplacements,
		std::uint64_t totalUsec,
		std::uint64_t maxUsec,
		std::uint64_t firstSearchId,
		std::uint64_t lastSearchId) noexcept;
	void recordPathSearchResults(
		RuntimePathSearchCaller caller,
		std::uint32_t bot,
		std::uint32_t fullUpdateId,
		std::uint32_t objectiveGeneration,
		std::uint32_t startArea,
		std::uint32_t goalArea,
		std::uint8_t routeType,
		bool requested,
		std::uint32_t searchCalls,
		std::uint32_t successCount,
		std::uint32_t failureCount,
		std::uint32_t expandedAreas,
		std::uint32_t enqueues,
		std::uint32_t reopens,
		std::uint32_t staleQueueEntries,
		std::uint32_t equalCostReplacements,
		std::uint64_t totalUsec,
		std::uint64_t maxUsec,
		std::uint64_t firstSearchId,
		std::uint64_t lastSearchId) noexcept;
	void recordObjectiveSelection(
		std::uint32_t bombSites,
		std::uint32_t funcBombTargetSites,
		std::uint32_t infoBombTargetSites,
		std::uint32_t candidateAreas,
		std::uint32_t uniqueCandidateAreas,
		std::uint32_t candidateQueries,
		std::uint32_t duplicateCandidateAreas,
		std::uint32_t selectedGoalArea,
		std::uint32_t registeredSites,
		std::uint32_t registeredFuncBombTargetSites,
		std::uint32_t registeredInfoBombTargetSites,
		bool registeredSiteCountsAvailable,
		std::uint32_t evaluatedSites,
		std::uint32_t cacheHits,
		std::uint32_t selectedSiteIdentity) noexcept;
	void recordPathRecompute() noexcept;
	void recordRunPlayerMove() noexcept;
	bool consumeReport(double nowSeconds, RuntimeProfilerReport *report) noexcept;

private:
	struct SearchKey
	{
		bool valid;
		std::uint32_t bot;
		std::uint32_t fullUpdateId;
		std::uint32_t objectiveGeneration;
		std::uint32_t startArea;
		std::uint32_t goalArea;
		std::uint8_t routeType;
		RuntimePathSearchCaller caller;
	};

	void recordPathSearchAggregate(
		RuntimePathSearchCaller caller,
		std::uint32_t searchCalls,
		std::uint32_t expandedAreas,
		std::uint32_t enqueues,
		std::uint64_t totalUsec,
		std::uint64_t maxUsec) noexcept;
	void recordSearchKey(
		RuntimePathSearchCaller caller,
		std::uint32_t bot,
		std::uint32_t fullUpdateId,
		std::uint32_t objectiveGeneration,
		std::uint32_t startArea,
		std::uint32_t goalArea,
		std::uint8_t routeType) noexcept;
	void clearCounters() noexcept;

	bool enabled_;
	double windowStartSeconds_;
	std::chrono::steady_clock::time_point wallWindowStart_;
	RuntimeProfilerReport counters_;
	std::array<SearchKey, 256U> searchKeys_;
	std::size_t searchKeyCount_;
};

// No clock reads occur while profiling is disabled. Timings are inclusive;
// callers must not sum overlapping parent and child stages.
class RuntimeProfilerScope
{
public:
	RuntimeProfilerScope(RuntimeProfiler &profiler, RuntimeProfilerStage stage) noexcept
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
	RuntimeProfilerScope(const RuntimeProfilerScope &) = delete;
	RuntimeProfilerScope &operator=(const RuntimeProfilerScope &) = delete;

private:
	RuntimeProfiler &profiler_;
	RuntimeProfilerStage stage_;
	bool enabled_;
	std::chrono::steady_clock::time_point start_;
};
}
}

#endif
