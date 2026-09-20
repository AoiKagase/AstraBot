#include "astrabot/metamod/runtime_profiler.hpp"

#include <cstdint>
#include <iostream>

namespace
{
using astrabot::metamod::RuntimeProfiler;
using astrabot::metamod::RuntimeProfilerReport;
using astrabot::metamod::RuntimeProfilerStage;

bool check(bool condition, const char *message)
{
	if (!condition)
	{
		std::cerr << "FAIL: " << message << '\n';
		return false;
	}
	return true;
}
}

int main()
{
	RuntimeProfiler profiler;
	profiler.record(RuntimeProfilerStage::StartFrame, 100U);
	RuntimeProfilerReport report = {};
	if (!check(!profiler.consumeReport(1.1, &report),
		"disabled profiling emits no report"))
	{
		return 1;
	}

	profiler.setEnabled(true, 10.0);
	profiler.setAliveBots(4U);
	profiler.record(RuntimeProfilerStage::StartFrame, 100U);
	profiler.record(RuntimeProfilerStage::StartFrame, 300U);
	profiler.recordVisionCandidate();
	profiler.recordVisionFovCheck();
	profiler.recordVisionFovCheck();
	profiler.recordVisionLosCheck();
	profiler.recordBodyProbe();
	profiler.recordTraceLine(4U, 2U, 8U);
	profiler.recordPathSearch(true, true);
	profiler.recordPathSearch(false, true);
	profiler.recordPathSearchStats(true, true, 10U, 12U, 1U, 2U, 0U);
	profiler.recordPathSearchResults(true, 2U, 1U, 1U, 20U, 24U, 2U, 3U, 0U,
		500U, 300U, 101U, 102U);
	profiler.recordPathRecompute();
	profiler.recordRunPlayerMove();
	if (!check(profiler.consumeReport(10.5, &report) == false,
		"sub-second profiling window is retained"))
	{
		return 1;
	}
	if (!check(profiler.consumeReport(11.0, &report),
		"one-second profiling window reports once"))
	{
		return 1;
	}
	const auto &stage = report.stages[
		static_cast<std::size_t>(RuntimeProfilerStage::StartFrame)];
	if (!check(
		stage.calls == 2U && stage.totalUsec == 400U && stage.maxUsec == 300U &&
		report.aliveBots == 4U && report.traceLineCalls == 4U &&
		report.visibilityCandidates == 3U && report.fovChecks == 2U &&
		report.losChecks == 1U && report.bodyProbeCalls == 9U &&
		report.pathSearches == 5U && report.pathSearchSuccesses == 3U &&
		report.pathSearchFailures == 2U && report.pathRecomputes == 1U &&
		report.pathExpandedAreas == 30U && report.pathEnqueues == 36U &&
		report.pathReopens == 3U && report.pathStaleQueueEntries == 5U &&
		report.pathEqualCostReplacements == 0U &&
		report.pathSearchTotalUsec == 500U && report.pathSearchMaxUsec == 300U &&
		report.pathFirstSearchId == 101U && report.pathLastSearchId == 102U &&
		report.runPlayerMoves == 1U,
		"stage and vision/navigation counters aggregate independently"))
	{
		return 1;
	}
	if (!check(!profiler.consumeReport(11.1, &report),
		"a report window is emitted only once"))
	{
		return 1;
	}

	std::cout << "runtime profiler: PASS\n";
	return 0;
}
