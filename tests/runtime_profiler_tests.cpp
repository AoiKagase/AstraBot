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
	profiler.record(RuntimeProfilerStage::StartFrame, 100U);
	profiler.record(RuntimeProfilerStage::StartFrame, 300U);
	profiler.recordTraceLine(4U, 2U, 8U);
	profiler.recordPathSearch(true, true);
	profiler.recordPathSearch(false, true);
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
	if (!check(stage.calls == 2U && stage.totalUsec == 400U &&
			stage.maxUsec == 300U && report.traceLineCalls == 4U &&
			report.visibilityCandidates == 2U && report.bodyProbes == 8U &&
			report.pathSearches == 2U && report.pathSearchSuccesses == 1U &&
			report.pathSearchFailures == 1U && report.pathRecomputes == 1U &&
			report.runPlayerMoves == 1U,
			"stage and navigation counters aggregate independently"))
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
