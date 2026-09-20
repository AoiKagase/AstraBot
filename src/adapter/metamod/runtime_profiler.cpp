#include "astrabot/metamod/runtime_profiler.hpp"

#include <cmath>
#include <limits>

namespace astrabot
{
namespace metamod
{
namespace
{
void saturatingAdd(std::uint64_t *value, std::uint64_t amount) noexcept
{
	if (value == nullptr)
	{
		return;
	}
	if (amount > (std::numeric_limits<std::uint64_t>::max)() - *value)
	{
		*value = (std::numeric_limits<std::uint64_t>::max)();
		return;
	}
	*value += amount;
}
}

RuntimeProfiler::RuntimeProfiler() noexcept
	: enabled_(false), windowStartSeconds_(0.0), counters_(), searchKeys_(),
	  searchKeyCount_(0U)
{
	clearCounters();
}

void RuntimeProfiler::setEnabled(bool enabled, double nowSeconds) noexcept
{
	if (enabled_ == enabled)
	{
		return;
	}
	enabled_ = enabled;
	windowStartSeconds_ = std::isfinite(nowSeconds) ? nowSeconds : 0.0;
	clearCounters();
}

bool RuntimeProfiler::enabled() const noexcept
{
	return enabled_;
}

void RuntimeProfiler::setAliveBots(std::uint32_t aliveBots) noexcept
{
	if (enabled_)
	{
		counters_.aliveBots = aliveBots;
	}
}

void RuntimeProfiler::recordVisionCandidate() noexcept
{
	if (enabled_)
	{
		saturatingAdd(&counters_.visibilityCandidates, 1U);
	}
}

void RuntimeProfiler::recordVisionFovCheck() noexcept
{
	if (enabled_)
	{
		saturatingAdd(&counters_.fovChecks, 1U);
	}
}

void RuntimeProfiler::recordVisionLosCheck() noexcept
{
	if (enabled_)
	{
		saturatingAdd(&counters_.losChecks, 1U);
	}
}

void RuntimeProfiler::recordBodyProbe() noexcept
{
	if (enabled_)
	{
		saturatingAdd(&counters_.bodyProbeCalls, 1U);
	}
}

void RuntimeProfiler::record(
	RuntimeProfilerStage stage, std::uint64_t elapsedUsec) noexcept
{
	if (!enabled_ || stage >= RuntimeProfilerStage::Count)
	{
		return;
	}
	RuntimeProfilerStageStats &stats =
		counters_.stages[static_cast<std::size_t>(stage)];
	if (stats.calls != (std::numeric_limits<std::uint32_t>::max)())
	{
		++stats.calls;
	}
	if (elapsedUsec > (std::numeric_limits<std::uint64_t>::max)() - stats.totalUsec)
	{
		stats.totalUsec = (std::numeric_limits<std::uint64_t>::max)();
	}
	else
	{
		stats.totalUsec += elapsedUsec;
	}
	if (elapsedUsec > stats.maxUsec)
	{
		stats.maxUsec = elapsedUsec;
	}
}

void RuntimeProfiler::recordTraceLine(
	std::uint64_t calls,
	std::uint64_t visibilityCandidates,
	std::uint64_t bodyProbes) noexcept
{
	if (!enabled_)
	{
		return;
	}
	saturatingAdd(&counters_.traceLineCalls, calls);
	saturatingAdd(&counters_.visibilityCandidates, visibilityCandidates);
	saturatingAdd(&counters_.bodyProbeCalls, bodyProbes);
}

void RuntimeProfiler::recordPathSearch(bool success, bool requested) noexcept
{
	if (!enabled_ || !requested)
	{
		return;
	}
	saturatingAdd(&counters_.pathSearches, 1U);
	if (success)
	{
		saturatingAdd(&counters_.pathSearchSuccesses, 1U);
	}
	else
	{
		saturatingAdd(&counters_.pathSearchFailures, 1U);
	}
	record(RuntimeProfilerStage::PathSearch, 0U);
}

void RuntimeProfiler::recordPathSearchStats(
	bool success,
	bool requested,
	std::uint32_t expandedAreas,
	std::uint32_t enqueues,
	std::uint32_t reopens,
	std::uint32_t staleQueueEntries,
	std::uint32_t equalCostReplacements) noexcept
{
	if (!enabled_ || !requested)
	{
		return;
	}
	recordPathSearch(success, true);
	saturatingAdd(&counters_.pathExpandedAreas, expandedAreas);
	saturatingAdd(&counters_.pathEnqueues, enqueues);
	saturatingAdd(&counters_.pathReopens, reopens);
	saturatingAdd(&counters_.pathStaleQueueEntries, staleQueueEntries);
	saturatingAdd(&counters_.pathEqualCostReplacements, equalCostReplacements);
}

void RuntimeProfiler::recordPathSearchResults(
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
	std::uint64_t lastSearchId) noexcept
{
	if (!enabled_ || !requested || searchCalls == 0U)
	{
		return;
	}
	saturatingAdd(&counters_.pathSearches, searchCalls);
	saturatingAdd(&counters_.pathSearchSuccesses, successCount);
	saturatingAdd(&counters_.pathSearchFailures, failureCount);
	saturatingAdd(&counters_.pathExpandedAreas, expandedAreas);
	saturatingAdd(&counters_.pathEnqueues, enqueues);
	saturatingAdd(&counters_.pathReopens, reopens);
	saturatingAdd(&counters_.pathStaleQueueEntries, staleQueueEntries);
	saturatingAdd(&counters_.pathEqualCostReplacements, equalCostReplacements);
	saturatingAdd(&counters_.pathSearchTotalUsec, totalUsec);
	if (maxUsec > counters_.pathSearchMaxUsec)
	{
		counters_.pathSearchMaxUsec = maxUsec;
	}
	if (firstSearchId != 0U)
	{
		if (counters_.pathFirstSearchId == 0U)
		{
			counters_.pathFirstSearchId = firstSearchId;
		}
		counters_.pathLastSearchId = lastSearchId;
	}
	RuntimeProfilerStageStats &stage = counters_.stages[
		static_cast<std::size_t>(RuntimeProfilerStage::PathSearch)];
	const std::uint64_t maxCalls = (std::numeric_limits<std::uint32_t>::max)();
	stage.calls = searchCalls > maxCalls - stage.calls
		? (std::numeric_limits<std::uint32_t>::max)()
		: stage.calls + searchCalls;
	saturatingAdd(&stage.totalUsec, totalUsec);
	if (maxUsec > stage.maxUsec)
	{
		stage.maxUsec = maxUsec;
	}
}

void RuntimeProfiler::recordPathSearchResults(
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
	std::uint64_t lastSearchId) noexcept
{
	if (!enabled_ || !requested || searchCalls == 0U)
	{
		return;
	}
	recordPathSearchResults(
		requested, searchCalls, successCount, failureCount, expandedAreas,
		enqueues, reopens, staleQueueEntries, equalCostReplacements, totalUsec,
		maxUsec, firstSearchId, lastSearchId);
	recordPathSearchAggregate(
		caller, searchCalls, expandedAreas, enqueues, totalUsec, maxUsec);
	recordSearchKey(
		caller, bot, fullUpdateId, objectiveGeneration, startArea, goalArea,
		routeType);
}

void RuntimeProfiler::recordObjectiveSelection(
	std::uint32_t bombSites,
	std::uint32_t candidateAreas,
	std::uint32_t uniqueCandidateAreas,
	std::uint32_t candidateQueries,
	std::uint32_t duplicateCandidateAreas,
	std::uint32_t selectedGoalArea,
	std::uint32_t registeredSites,
	std::uint32_t evaluatedSites,
	std::uint32_t cacheHits,
	std::uint32_t selectedSiteIdentity) noexcept
{
	if (!enabled_)
	{
		return;
	}
	saturatingAdd(&counters_.objectiveBombSites, bombSites);
	saturatingAdd(&counters_.objectiveCandidateAreas, candidateAreas);
	saturatingAdd(&counters_.objectiveUniqueCandidateAreas, uniqueCandidateAreas);
	saturatingAdd(&counters_.objectiveCandidateQueries, candidateQueries);
	saturatingAdd(
		&counters_.objectiveDuplicateCandidateAreas, duplicateCandidateAreas);
	if (selectedGoalArea != 0U)
	{
		counters_.selectedObjectiveGoalArea = selectedGoalArea;
	}
	saturatingAdd(&counters_.objectiveRegisteredSites, registeredSites);
	saturatingAdd(&counters_.objectiveEvaluatedSites, evaluatedSites);
	saturatingAdd(&counters_.objectiveCacheHits, cacheHits);
	if (selectedSiteIdentity != 0U)
	{
		counters_.selectedObjectiveSiteIdentity = selectedSiteIdentity;
	}
}

void RuntimeProfiler::recordPathSearchAggregate(
	RuntimePathSearchCaller caller,
	std::uint32_t searchCalls,
	std::uint32_t expandedAreas,
	std::uint32_t enqueues,
	std::uint64_t totalUsec,
	std::uint64_t maxUsec) noexcept
{
	const std::size_t index = static_cast<std::size_t>(caller) <
			static_cast<std::size_t>(RuntimePathSearchCaller::Count)
		? static_cast<std::size_t>(caller)
		: static_cast<std::size_t>(RuntimePathSearchCaller::Unknown);
	RuntimePathSearchAggregate &aggregate = counters_.pathSearchByCaller[index];
	saturatingAdd(&aggregate.calls, searchCalls);
	saturatingAdd(&aggregate.totalUsec, totalUsec);
	saturatingAdd(&aggregate.expandedAreas, expandedAreas);
	saturatingAdd(&aggregate.enqueues, enqueues);
	if (maxUsec > aggregate.maxUsec)
	{
		aggregate.maxUsec = maxUsec;
	}
}

void RuntimeProfiler::recordSearchKey(
	RuntimePathSearchCaller caller,
	std::uint32_t bot,
	std::uint32_t fullUpdateId,
	std::uint32_t objectiveGeneration,
	std::uint32_t startArea,
	std::uint32_t goalArea,
	std::uint8_t routeType) noexcept
{
	SearchKey key = {
		true, bot, fullUpdateId, objectiveGeneration, startArea, goalArea,
		routeType, caller};
	for (std::size_t index = 0U; index < searchKeyCount_; ++index)
	{
		const SearchKey &existing = searchKeys_[index];
		const bool sameQuery = existing.bot == key.bot &&
			existing.startArea == key.startArea &&
			existing.goalArea == key.goalArea &&
			existing.routeType == key.routeType &&
			existing.caller == key.caller;
		if (sameQuery && existing.fullUpdateId == key.fullUpdateId)
		{
			saturatingAdd(&counters_.duplicateSearchSameFullUpdate, 1U);
		}
		if (sameQuery && existing.objectiveGeneration != 0U &&
			existing.objectiveGeneration == key.objectiveGeneration)
		{
			saturatingAdd(&counters_.duplicateSearchSameObjectiveGeneration, 1U);
		}
		if (sameQuery && existing.fullUpdateId == key.fullUpdateId &&
			existing.objectiveGeneration == key.objectiveGeneration)
		{
			return;
		}
	}
	saturatingAdd(&counters_.uniqueSearchKeys, 1U);
	if (searchKeyCount_ < searchKeys_.size())
	{
		searchKeys_[searchKeyCount_++] = key;
	}
}

void RuntimeProfiler::recordPathRecompute() noexcept
{
	if (enabled_)
	{
		saturatingAdd(&counters_.pathRecomputes, 1U);
		record(RuntimeProfilerStage::PathRecompute, 0U);
	}
}

void RuntimeProfiler::recordRunPlayerMove() noexcept
{
	if (enabled_)
	{
		saturatingAdd(&counters_.runPlayerMoves, 1U);
	}
}

bool RuntimeProfiler::consumeReport(
	double nowSeconds, RuntimeProfilerReport *report) noexcept
{
	if (!enabled_ || report == nullptr || !std::isfinite(nowSeconds) ||
		!std::isfinite(windowStartSeconds_) || nowSeconds < windowStartSeconds_ ||
		nowSeconds - windowStartSeconds_ < 1.0)
	{
		return false;
	}
	*report = counters_;
	report->windowSeconds = nowSeconds - windowStartSeconds_;
	windowStartSeconds_ = nowSeconds;
	clearCounters();
	return true;
}

void RuntimeProfiler::clearCounters() noexcept
{
	counters_ = {};
	for (RuntimeProfilerStageStats &stats : counters_.stages)
	{
		stats = {};
	}
	searchKeys_.fill(SearchKey{});
	searchKeyCount_ = 0U;
}
}
}
