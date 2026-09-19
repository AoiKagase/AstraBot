#include "engine_random_source.hpp"

#include <cstdio>

namespace
{
using astrabot::compat::EngineRandomCallbacks;
using astrabot::compat::EngineRandomSource;
using astrabot::compat::RandomActor;
using astrabot::compat::RandomFloatResult;
using astrabot::compat::RandomLongResult;
using astrabot::compat::RandomRequest;
using astrabot::compat::RandomStatus;
using astrabot::compat::RandomTimingContext;

int g_floatCalls = 0;
int g_longCalls = 0;
float g_floatLower = 0.0f;
float g_floatUpper = 0.0f;
std::int32_t g_longLower = 0;
std::int32_t g_longUpper = 0;

float fakeRandomFloat(float lower, float upper)
{
	++g_floatCalls;
	g_floatLower = lower;
	g_floatUpper = upper;
	return 4.25f;
}

std::int32_t fakeRandomLong(std::int32_t lower, std::int32_t upper)
{
	++g_longCalls;
	g_longLower = lower;
	g_longUpper = upper;
	return 1;
}

bool check(bool condition, const char *description)
{
	if (condition)
	{
		return true;
	}
	std::fprintf(stderr, "check failed: %s\n", description);
	return false;
}

RandomActor actor()
{
	return {4U, 8U};
}

RandomTimingContext timing()
{
	return {20U, 19U, 7U};
}

bool testEngineFloatForwardsExactBounds()
{
	g_floatCalls = 0;
	EngineRandomSource source;
	source.configure({&fakeRandomLong, &fakeRandomFloat});
	const RandomFloatResult result = source.nextFloat(RandomRequest::floatRequest(
		"RNG-ENGINE-FLOAT", actor(), timing(), -12.5f, 12.5f));
	return check(source.available(), "engine source is available") &&
		check(result.status == RandomStatus::Ok && result.value == 4.25f,
			"engine float result is returned") &&
		check(g_floatCalls == 1, "engine float is called once") &&
		check(g_floatLower == -12.5f && g_floatUpper == 12.5f,
			"engine float bounds are unchanged");
}

bool testEngineLongForwardsExactBounds()
{
	g_longCalls = 0;
	EngineRandomSource source;
	source.configure({&fakeRandomLong, &fakeRandomFloat});
	const RandomLongResult result = source.nextLong(RandomRequest::longRequest(
		"RNG-ENGINE-LONG", actor(), timing(), 0, 1));
	return check(result.status == RandomStatus::Ok && result.value == 1,
			"engine long result is returned") &&
		check(g_longCalls == 1, "engine long is called once") &&
		check(g_longLower == 0 && g_longUpper == 1, "engine long bounds are unchanged");
}

bool testUnavailableCallbacks()
{
	EngineRandomSource source;
	source.configure({nullptr, nullptr});
	const RandomFloatResult floatResult = source.nextFloat(RandomRequest::floatRequest(
		"RNG-ENGINE-UNAVAILABLE-FLOAT", actor(), timing(), 0.0f, 1.0f));
	const RandomLongResult longResult = source.nextLong(RandomRequest::longRequest(
		"RNG-ENGINE-UNAVAILABLE-LONG", actor(), timing(), 0, 1));
	return check(!source.available(), "missing callbacks make source unavailable") &&
		check(floatResult.status == RandomStatus::Unavailable,
			"missing float callback fails closed") &&
		check(longResult.status == RandomStatus::Unavailable,
			"missing long callback fails closed");
}
}

int main()
{
	return testEngineFloatForwardsExactBounds() && testEngineLongForwardsExactBounds() &&
		testUnavailableCallbacks() ? 0 : 1;
}
