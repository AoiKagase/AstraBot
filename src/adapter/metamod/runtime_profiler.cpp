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
	: enabled_(false), windowStartSeconds_(0.0), counters_()
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

void RuntimeProfiler::recordPathRecompute() noexcept
{
	if (enabled_)
	{
		saturatingAdd(&counters_.pathRecomputes, 1U);
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
}
}
}
