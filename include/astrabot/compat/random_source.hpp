#ifndef ASTRABOT_COMPAT_RANDOM_SOURCE_HPP
#define ASTRABOT_COMPAT_RANDOM_SOURCE_HPP

#include "astrabot/compat/random_trace.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace astrabot
{
namespace compat
{
struct RandomFloatResult
{
	RandomStatus status;
	float value;
};

struct RandomLongResult
{
	RandomStatus status;
	std::int32_t value;
};

class ICompatibilityRandomSource
{
public:
	virtual ~ICompatibilityRandomSource() { }
	virtual RandomFloatResult nextFloat(const RandomRequest &request) = 0;
	virtual RandomLongResult nextLong(const RandomRequest &request) = 0;
};

struct RandomTapeEntry
{
	RandomRequest request;
	float floatValue;
	std::int32_t longValue;
	bool requireSemanticId;
	std::string semanticId;

	static RandomTapeEntry floatEntry(
		const char *semanticId,
		RandomActor actor,
		RandomTimingContext timing,
		float lower,
		float upper,
		float value);
	static RandomTapeEntry longEntry(
		const char *semanticId,
		RandomActor actor,
		RandomTimingContext timing,
		std::int32_t lower,
		std::int32_t upper,
		std::int32_t value);
};

class ScriptedRandomSource : public ICompatibilityRandomSource
{
public:
	explicit ScriptedRandomSource(const std::vector<RandomTapeEntry> &tape);

	RandomFloatResult nextFloat(const RandomRequest &request) override;
	RandomLongResult nextLong(const RandomRequest &request) override;

	void setTraceSink(IRandomTraceSink *sink);
	bool verifyComplete() const;
	bool failed() const;
	std::size_t position() const;

private:
	std::vector<RandomTapeEntry> tape_;
	std::size_t position_;
	bool failed_;
	std::uint64_t sequence_;
	IRandomTraceSink *traceSink_;
};
}
}

#endif
