#ifndef ASTRABOT_ADAPTER_METAMOD_ENGINE_RANDOM_SOURCE_HPP
#define ASTRABOT_ADAPTER_METAMOD_ENGINE_RANDOM_SOURCE_HPP

#include "astrabot/compat/random_source.hpp"

#include <cstdint>

namespace astrabot
{
namespace compat
{
using RandomFloatFunction = float (*)(float, float);
using RandomLongFunction = std::int32_t (*)(std::int32_t, std::int32_t);

struct EngineRandomCallbacks
{
	RandomLongFunction randomLong;
	RandomFloatFunction randomFloat;
};

class EngineRandomSource : public ICompatibilityRandomSource
{
public:
	EngineRandomSource();
	void configure(const EngineRandomCallbacks &callbacks);
	bool available() const;
	void setTraceSink(IRandomTraceSink *sink);

	RandomFloatResult nextFloat(const RandomRequest &request) override;
	RandomLongResult nextLong(const RandomRequest &request) override;

private:
	EngineRandomCallbacks callbacks_;
	std::uint64_t sequence_;
	IRandomTraceSink *traceSink_;
};
}
}

#endif
