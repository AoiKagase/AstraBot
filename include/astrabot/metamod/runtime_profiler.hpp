#ifndef ASTRABOT_ADAPTER_METAMOD_RUNTIME_PROFILER_HPP
#define ASTRABOT_ADAPTER_METAMOD_RUNTIME_PROFILER_HPP

#include <array>
#include <cstddef>
#include <cstdint>

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
	NavCurrentAreaLookup,
	PathSearch,
	PathRecompute,
	NavMovement,
	MovementDispatch,
	TraceSerialization,
	Count
};

struct RuntimeProfilerStageStats
{
	std::uint32_t calls;
	std::uint64_t totalUsec;
	std::uint64_t maxUsec;
};

struct RuntimeProfilerReport
{
	double windowSeconds;
	std::array<RuntimeProfilerStageStats,
		static_cast<std::size_t>(RuntimeProfilerStage::Count)> stages;
	std::uint64_t traceLineCalls;
	std::uint64_t visibilityCandidates;
	std::uint64_t bodyProbes;
	std::uint64_t pathSearches;
	std::uint64_t pathSearchSuccesses;
	std::uint64_t pathSearchFailures;
	std::uint64_t pathRecomputes;
	std::uint64_t runPlayerMoves;
};

class RuntimeProfiler
{
public:
	RuntimeProfiler() noexcept;

	void setEnabled(bool enabled, double nowSeconds) noexcept;
	bool enabled() const noexcept;
	void record(RuntimeProfilerStage stage, std::uint64_t elapsedUsec) noexcept;
	void recordTraceLine(
		std::uint64_t calls,
		std::uint64_t visibilityCandidates,
		std::uint64_t bodyProbes) noexcept;
	void recordPathSearch(bool success, bool requested) noexcept;
	void recordPathRecompute() noexcept;
	void recordRunPlayerMove() noexcept;
	bool consumeReport(double nowSeconds, RuntimeProfilerReport *report) noexcept;

private:
	void clearCounters() noexcept;

	bool enabled_;
	double windowStartSeconds_;
	RuntimeProfilerReport counters_;
};
}
}

#endif
