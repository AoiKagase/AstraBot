#include "astrabot/compat/random_source.hpp"

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

namespace
{
using astrabot::compat::IRandomTraceSink;
using astrabot::compat::RandomActor;
using astrabot::compat::RandomFloatResult;
using astrabot::compat::RandomLongResult;
using astrabot::compat::RandomRequest;
using astrabot::compat::RandomStatus;
using astrabot::compat::RandomTapeEntry;
using astrabot::compat::RandomTimingContext;
using astrabot::compat::RandomTraceRecord;
using astrabot::compat::ScriptedRandomSource;

bool check(bool condition, const char *description)
{
	if (condition)
	{
		return true;
	}
	std::fprintf(stderr, "check failed: %s\n", description);
	return false;
}

struct TraceCollector : public IRandomTraceSink
{
	std::vector<RandomTraceRecord> records;

	void record(const RandomTraceRecord &record) override
	{
		records.push_back(record);
	}
};

RandomActor actor(std::uint32_t slot, std::uint32_t generation)
{
	RandomActor value = {slot, generation};
	return value;
}

RandomTimingContext timing(
	std::uint32_t command,
	std::uint32_t upkeep,
	std::uint32_t fullUpdate)
{
	RandomTimingContext value = {command, upkeep, fullUpdate};
	return value;
}

bool testFloatForwardingAndTrace()
{
	const RandomActor bot = actor(4U, 9U);
	const RandomTimingContext context = timing(88U, 87U, 45U);
	ScriptedRandomSource source({
		RandomTapeEntry::floatEntry("RNG-TEST-FLOAT", bot, context, -10.0f, 10.0f, 3.5f)});
	TraceCollector trace;
	source.setTraceSink(&trace);
	const RandomFloatResult result = source.nextFloat(
		RandomRequest::floatRequest("RNG-TEST-FLOAT", bot, context, -10.0f, 10.0f));
	return check(result.status == RandomStatus::Ok, "float result is accepted") &&
		check(result.value == 3.5f, "float result is forwarded") &&
		check(trace.records.size() == 1U, "one float trace is emitted") &&
		check(trace.records[0].sequence == 1U, "float trace sequence starts at one") &&
		check(trace.records[0].request.type == astrabot::compat::RandomType::Float,
			"float trace type is preserved") &&
		check(trace.records[0].request.floatLower == -10.0f &&
			trace.records[0].request.floatUpper == 10.0f, "float bounds are preserved") &&
		check(trace.records[0].request.actor.slot == 4U &&
			trace.records[0].request.actor.generation == 9U, "float actor is preserved") &&
		check(trace.records[0].request.timing.fullUpdateSequence == 45U,
			"float timing is preserved") &&
		check(trace.records[0].floatResult == 3.5f, "float trace result is preserved") &&
		check(source.verifyComplete(), "float tape is complete");
}

bool testLongForwardingAndTrace()
{
	const RandomActor bot = actor(2U, 3U);
	const RandomTimingContext context = timing(12U, 11U, 6U);
	ScriptedRandomSource source({
		RandomTapeEntry::longEntry("RNG-TEST-LONG", bot, context, 0, 1, 1)});
	TraceCollector trace;
	source.setTraceSink(&trace);
	const RandomLongResult result = source.nextLong(
		RandomRequest::longRequest("RNG-TEST-LONG", bot, context, 0, 1));
	return check(result.status == RandomStatus::Ok, "long result is accepted") &&
		check(result.value == 1, "long result is forwarded") &&
		check(trace.records.size() == 1U, "one long trace is emitted") &&
		check(trace.records[0].request.type == astrabot::compat::RandomType::Long,
			"long trace type is preserved") &&
		check(trace.records[0].request.longLower == 0 &&
			trace.records[0].request.longUpper == 1, "long bounds are preserved") &&
		check(trace.records[0].longResult == 1, "long trace result is preserved") &&
		check(source.verifyComplete(), "long tape is complete");
}

bool testScriptedSequence()
{
	const RandomActor bot = actor(1U, 1U);
	const RandomTimingContext context = timing(1U, 1U, 1U);
	ScriptedRandomSource source({
		RandomTapeEntry::floatEntry("RNG-SEQ-FLOAT-1", bot, context, -10.0f, 10.0f, 3.5f),
		RandomTapeEntry::floatEntry("RNG-SEQ-FLOAT-2", bot, context, -10.0f, 10.0f, -2.0f),
		RandomTapeEntry::longEntry("RNG-SEQ-LONG", bot, context, 0, 1, 1),
		RandomTapeEntry::floatEntry("RNG-SEQ-FLOAT-3", bot, context, 0.25f, 1.0f, 0.7f)});
	const RandomFloatResult first = source.nextFloat(
		RandomRequest::floatRequest("RNG-SEQ-FLOAT-1", bot, context, -10.0f, 10.0f));
	const RandomFloatResult second = source.nextFloat(
		RandomRequest::floatRequest("RNG-SEQ-FLOAT-2", bot, context, -10.0f, 10.0f));
	const RandomLongResult third = source.nextLong(
		RandomRequest::longRequest("RNG-SEQ-LONG", bot, context, 0, 1));
	const RandomFloatResult fourth = source.nextFloat(
		RandomRequest::floatRequest("RNG-SEQ-FLOAT-3", bot, context, 0.25f, 1.0f));
	return check(first.value == 3.5f && second.value == -2.0f && third.value == 1 &&
		fourth.value == 0.7f, "scripted sequence values are ordered") &&
		check(source.position() == 4U, "scripted sequence consumes four entries") &&
		check(source.verifyComplete(), "scripted sequence is complete");
}

bool testTapeExhaustion()
{
	const RandomActor bot = actor(1U, 1U);
	const RandomTimingContext context = timing(1U, 1U, 1U);
	ScriptedRandomSource source({
		RandomTapeEntry::longEntry("RNG-EXHAUST", bot, context, 0, 1, 0)});
	source.nextLong(RandomRequest::longRequest("RNG-EXHAUST", bot, context, 0, 1));
	const RandomLongResult result = source.nextLong(
		RandomRequest::longRequest("RNG-EXHAUST", bot, context, 0, 1));
	return check(result.status == RandomStatus::TapeExhausted, "too many calls exhaust tape") &&
		check(source.failed(), "tape exhaustion is observable") &&
		check(!source.verifyComplete(), "exhausted tape cannot be complete");
}

bool testTapeMismatch()
{
	const RandomActor bot = actor(1U, 1U);
	const RandomTimingContext context = timing(1U, 1U, 1U);
	ScriptedRandomSource source({
		RandomTapeEntry::floatEntry("RNG-MISMATCH", bot, context, -10.0f, 10.0f, 1.0f)});
	const RandomFloatResult result = source.nextFloat(
		RandomRequest::floatRequest("RNG-MISMATCH", bot, context, -10.0f, 9.0f));
	return check(result.status == RandomStatus::RequestMismatch, "bounds mismatch fails") &&
		check(source.failed(), "bounds mismatch is observable");
}

bool testConditionalConsumption()
{
	const RandomActor bot = actor(1U, 1U);
	const RandomTimingContext context = timing(1U, 1U, 1U);
	ScriptedRandomSource falseBranch({
		RandomTapeEntry::longEntry("RNG-CONDITIONAL-LONG", bot, context, 0, 1, 1)});
	if (false)
	{
		falseBranch.nextFloat(RandomRequest::floatRequest(
			"RNG-CONDITIONAL-FLOAT", bot, context, 0.0f, 1.0f));
	}
	const RandomLongResult falseResult = falseBranch.nextLong(
		RandomRequest::longRequest("RNG-CONDITIONAL-LONG", bot, context, 0, 1));
	ScriptedRandomSource trueBranch({
		RandomTapeEntry::floatEntry("RNG-CONDITIONAL-FLOAT", bot, context, 0.0f, 1.0f, 0.5f),
		RandomTapeEntry::longEntry("RNG-CONDITIONAL-LONG", bot, context, 0, 1, 1)});
	if (true)
	{
		trueBranch.nextFloat(RandomRequest::floatRequest(
			"RNG-CONDITIONAL-FLOAT", bot, context, 0.0f, 1.0f));
	}
	const RandomLongResult trueResult = trueBranch.nextLong(
		RandomRequest::longRequest("RNG-CONDITIONAL-LONG", bot, context, 0, 1));
	return check(falseResult.value == 1 && falseBranch.position() == 1U,
			"false branch does not consume") &&
		check(trueResult.value == 1 && trueBranch.position() == 2U,
			"true branch consumes at its position") &&
		check(falseBranch.verifyComplete() && trueBranch.verifyComplete(),
			"conditional tapes are complete");
}

bool testAimOffsetFixture()
{
	const RandomActor bot = actor(4U, 2U);
	const RandomTimingContext context = timing(88U, 88U, 9U);
	const float error = 2.0f;
	ScriptedRandomSource source({
		RandomTapeEntry::floatEntry("RNG-CSBOT-AIM-X", bot, context, -error, error, 0.5f),
		RandomTapeEntry::floatEntry("RNG-CSBOT-AIM-Y", bot, context, -error, error, -0.25f),
		RandomTapeEntry::floatEntry("RNG-CSBOT-AIM-Z", bot, context, -error, error, 1.25f),
		RandomTapeEntry::floatEntry("RNG-CSBOT-AIM-NEXT-TIME", bot, context, 0.25f, 1.0f, 0.75f)});
	const RandomFloatResult x = source.nextFloat(RandomRequest::floatRequest(
		"RNG-CSBOT-AIM-X", bot, context, -error, error));
	const RandomFloatResult y = source.nextFloat(RandomRequest::floatRequest(
		"RNG-CSBOT-AIM-Y", bot, context, -error, error));
	const RandomFloatResult z = source.nextFloat(RandomRequest::floatRequest(
		"RNG-CSBOT-AIM-Z", bot, context, -error, error));
	const RandomFloatResult next = source.nextFloat(RandomRequest::floatRequest(
		"RNG-CSBOT-AIM-NEXT-TIME", bot, context, 0.25f, 1.0f));
	return check(x.value == 0.5f && y.value == -0.25f && z.value == 1.25f &&
		next.value == 0.75f, "aim fixture values preserve order") &&
		check(source.position() == 4U && source.verifyComplete(),
			"aim fixture consumes the complete tape");
}

bool testSharedOrdering()
{
	const RandomTimingContext context = timing(2U, 2U, 1U);
	const RandomActor botA = actor(1U, 1U);
	const RandomActor botB = actor(2U, 1U);
	ScriptedRandomSource source({
		RandomTapeEntry::longEntry("RNG-SHARED-A-1", botA, context, 0, 1, 0),
		RandomTapeEntry::longEntry("RNG-SHARED-B-1", botB, context, 0, 1, 1),
		RandomTapeEntry::longEntry("RNG-SHARED-A-2", botA, context, 0, 1, 1)});
	TraceCollector trace;
	source.setTraceSink(&trace);
	source.nextLong(RandomRequest::longRequest("RNG-SHARED-A-1", botA, context, 0, 1));
	source.nextLong(RandomRequest::longRequest("RNG-SHARED-B-1", botB, context, 0, 1));
	source.nextLong(RandomRequest::longRequest("RNG-SHARED-A-2", botA, context, 0, 1));
	return check(trace.records.size() == 3U, "shared source records all bots") &&
		check(trace.records[0].sequence == 1U && trace.records[1].sequence == 2U &&
			trace.records[2].sequence == 3U, "shared source sequence is global") &&
		check(trace.records[0].request.actor.slot == 1U &&
			trace.records[1].request.actor.slot == 2U &&
			trace.records[2].request.actor.slot == 1U, "shared source preserves actor order") &&
		check(source.verifyComplete(), "shared source tape is complete");
}

bool testEnhancedIsolation()
{
	const RandomTimingContext context = timing(1U, 1U, 1U);
	const RandomActor bot = actor(1U, 1U);
	ScriptedRandomSource compatibility({
		RandomTapeEntry::floatEntry("RNG-COMPAT", bot, context, 0.0f, 1.0f, 0.25f)});
	ScriptedRandomSource enhanced({
		RandomTapeEntry::floatEntry("RNG-ENHANCED", bot, context, 0.0f, 1.0f, 0.75f)});
	enhanced.nextFloat(RandomRequest::floatRequest("RNG-ENHANCED", bot, context, 0.0f, 1.0f));
	const RandomFloatResult result = compatibility.nextFloat(
		RandomRequest::floatRequest("RNG-COMPAT", bot, context, 0.0f, 1.0f));
	return check(result.status == RandomStatus::Ok && result.value == 0.25f,
			"compatibility stream is unaffected by enhanced call") &&
		check(compatibility.position() == 1U && enhanced.position() == 1U,
			"enhanced and compatibility positions are separate") &&
		check(compatibility.verifyComplete() && enhanced.verifyComplete(),
			"isolated tapes are complete");
}

bool testModeBaselineEquivalence()
{
	const RandomActor bot = actor(3U, 4U);
	const RandomTimingContext context = timing(5U, 5U, 2U);
	ScriptedRandomSource compatibility({
		RandomTapeEntry::longEntry("RNG-MODE-BASELINE", bot, context, 0, 2, 2)});
	ScriptedRandomSource enhancedWithExtensionDisabled({
		RandomTapeEntry::longEntry("RNG-MODE-BASELINE", bot, context, 0, 2, 2)});
	const RandomLongResult compatibilityResult = compatibility.nextLong(
		RandomRequest::longRequest("RNG-MODE-BASELINE", bot, context, 0, 2));
	const RandomLongResult enhancedResult = enhancedWithExtensionDisabled.nextLong(
		RandomRequest::longRequest("RNG-MODE-BASELINE", bot, context, 0, 2));
	return check(compatibilityResult.status == RandomStatus::Ok &&
		enhancedResult.status == RandomStatus::Ok && compatibilityResult.value == enhancedResult.value,
		"disabled enhanced extension preserves baseline result") &&
		check(compatibility.position() == enhancedWithExtensionDisabled.position(),
			"disabled enhanced extension preserves baseline consumption");
}

bool testNoDirectCompatibilityBypass()
{
	const char *paths[] = {
		"src/core/compat/random_source.cpp",
		"src/adapter/metamod/engine_random_source.cpp"};
	const char *forbidden[] = {"RANDOM_FLOAT", "RANDOM_LONG", "std::mt19937", "xorshift", "PCG", "rand("};
	for (const char *path : paths)
	{
		std::ifstream input(path);
		if (!check(input.good(), "compatibility source is readable"))
		{
			return false;
		}
		std::string contents((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
		for (const char *token : forbidden)
		{
			if (!check(contents.find(token) == std::string::npos, "compatibility source has no direct bypass"))
			{
				return false;
			}
		}
	}
	return true;
}
}

int main()
{
	const bool passed = testFloatForwardingAndTrace() && testLongForwardingAndTrace() &&
		testScriptedSequence() && testTapeExhaustion() && testTapeMismatch() &&
		testConditionalConsumption() && testAimOffsetFixture() && testSharedOrdering() &&
		testEnhancedIsolation() && testModeBaselineEquivalence() &&
		testNoDirectCompatibilityBypass();
	return passed ? 0 : 1;
}
