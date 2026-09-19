#ifndef ASTRABOT_COMPAT_RANDOM_TRACE_HPP
#define ASTRABOT_COMPAT_RANDOM_TRACE_HPP

#include <cstdint>

namespace astrabot
{
namespace compat
{
enum class RandomType
{
	Float,
	Long
};

enum class RandomStatus
{
	Ok,
	Unavailable,
	TapeExhausted,
	RequestMismatch,
	InvalidRequest
};

struct RandomActor
{
	std::uint32_t slot;
	std::uint32_t generation;
};

struct RandomTimingContext
{
	std::uint32_t commandSequence;
	std::uint32_t upkeepSequence;
	std::uint32_t fullUpdateSequence;
};

struct RandomRequest
{
	RandomType type;
	const char *semanticId;
	RandomActor actor;
	RandomTimingContext timing;
	float floatLower;
	float floatUpper;
	std::int32_t longLower;
	std::int32_t longUpper;

	static RandomRequest floatRequest(
		const char *semanticId,
		RandomActor actor,
		RandomTimingContext timing,
		float lower,
		float upper);
	static RandomRequest longRequest(
		const char *semanticId,
		RandomActor actor,
		RandomTimingContext timing,
		std::int32_t lower,
		std::int32_t upper);
};

struct RandomTraceRecord
{
	std::uint64_t sequence;
	RandomRequest request;
	float floatResult;
	std::int32_t longResult;
};

class IRandomTraceSink
{
public:
	virtual ~IRandomTraceSink() { }
	virtual void record(const RandomTraceRecord &record) = 0;
};
}
}

#endif
