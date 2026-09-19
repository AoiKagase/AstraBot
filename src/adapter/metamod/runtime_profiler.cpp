#include "astrabot/metamod/runtime_profiler.hpp"

#include <cmath>
#include <limits>

namespace astrabot
{
namespace metamod
{
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
	counters_.traceLineCalls += calls;
	counters_.visibilityCandidates += visibilityCandidates;
	counters_.bodyProbes += bodyProbes;
}

void RuntimeProfiler::recordPathSearch(bool success, bool requested) noexcept
{
	if (!enabled_ || !requested)
	{
		return;
	}
	++counters_.pathSearches;
	if (success)
	{
		++counters_.pathSearchSuccesses;
	}
	else
	{
		++counters_.pathSearchFailures;
	}
}

void RuntimeProfiler::recordPathRecompute() noexcept
{
	if (enabled_)
	{
		++counters_.pathRecomputes;
	}
}

void RuntimeProfiler::recordRunPlayerMove() noexcept
{
	if (enabled_)
	{
		++counters_.runPlayerMoves;
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
