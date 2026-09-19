#include "astrabot/compat/observation.hpp"

#include <cstdio>
#include <string>

namespace
{
bool check(bool condition, const char *message)
{
	if (!condition)
	{
		std::fprintf(stderr, "FAIL: %s\n", message);
	}
	return condition;
}

astrabot::compat::ObservationContext context(
	const char *semanticId,
	astrabot::world::ActorKey actor,
	astrabot::world::FrameIdentity frame,
	astrabot::compat::ObservationQuality quality,
	astrabot::compat::ObservationFreshness freshness,
	astrabot::compat::ObservationSource source,
	std::uint32_t delayTicks = 0U,
	astrabot::compat::ObservationTimingContext timing = {0U, 0U, 0U})
{
	astrabot::compat::ObservationContext value = {};
	value.semanticId = semanticId;
	value.actor = actor;
	value.frame = frame;
	value.quality = quality;
	value.freshness = freshness;
	value.source = source;
	value.delayTicks = delayTicks;
	value.timing = timing;
	return value;
}

bool testQualityAndFreshnessAreExplicit()
{
	const astrabot::world::ActorKey actor = {4U, 9U};
	const astrabot::world::FrameIdentity frame = {2U, 3U, 40U};
	const astrabot::compat::ObservationContext value = context(
		"OBS-PLAYER-FOV", actor, frame,
		astrabot::compat::ObservationQuality::ExactEngineApi,
		astrabot::compat::ObservationFreshness::SameTick,
		astrabot::compat::ObservationSource::PublicEdict);
	return check(value.isValid(), "observation context is valid") &&
		check(value.quality == astrabot::compat::ObservationQuality::ExactEngineApi,
			"quality is retained") &&
		check(value.freshness == astrabot::compat::ObservationFreshness::SameTick,
			"freshness is retained") &&
		check(value.source == astrabot::compat::ObservationSource::PublicEdict,
			"source is retained");
}

bool testZeroAndFalseRemainValidWhenObserved()
{
	using namespace astrabot::compat;
	const ObservationContext exact = context(
		"OBS-PLAYER-FOV", {4U, 9U}, {2U, 3U, 40U}, ObservationQuality::Exact,
		ObservationFreshness::SameTick, ObservationSource::PublicEdict);
	const ObservationValue<float> zero = {0.0f, true, exact};
	const ObservationValue<bool> falseValue = {false, true, exact};
	return check(zero.isAvailable() && zero.value == 0.0f,
			"observed zero remains available") &&
		check(falseValue.isAvailable() && !falseValue.value,
			"observed false remains available");
}

bool testUnavailableValueIsNotAvailable()
{
	using namespace astrabot::compat;
	const ObservationContext unavailable = context(
		"OBS-WEAPON-ACCURACY", {4U, 9U}, {2U, 3U, 40U},
		ObservationQuality::Unavailable, ObservationFreshness::Stale,
		ObservationSource::None);
	const ObservationValue<float> value = {0.0f, false, unavailable};
	return check(!value.isAvailable(), "unavailable value is not available") &&
		check(value.context.quality == ObservationQuality::Unavailable,
			"unavailable quality is explicit");
}

bool testDelayedValueRetainsDelayAndSource()
{
	using namespace astrabot::compat;
	const ObservationContext delayed = context(
		"OBS-PLAYER-TEAM", {4U, 9U}, {2U, 3U, 40U}, ObservationQuality::Delayed,
		ObservationFreshness::EventDrivenCached, ObservationSource::PublicGameDll, 2U);
	const ObservationValue<std::int32_t> value = {2, true, delayed};
	return check(value.isAvailable(), "delayed value remains usable") &&
		check(value.context.delayTicks == 2U, "delay is retained") &&
		check(value.context.source == ObservationSource::PublicGameDll,
			"delayed source is retained");
}

bool testActorGenerationPreventsCrossBotReuse()
{
	using namespace astrabot::compat;
	const ObservationContext actorA = context(
		"OBS-PLAYER-HEALTH", {4U, 9U}, {2U, 3U, 40U}, ObservationQuality::Exact,
		ObservationFreshness::SameTick, ObservationSource::PublicEdict);
	const ObservationContext actorB = context(
		"OBS-PLAYER-HEALTH", {5U, 3U}, {2U, 3U, 40U}, ObservationQuality::Exact,
		ObservationFreshness::SameTick, ObservationSource::PublicEdict);
	return check(!(actorA.actor == actorB.actor), "actor identity is isolated") &&
		check(actorA.actor.generation != actorB.actor.generation,
			"generation distinguishes reused state");
}

bool testLifecycleContextRejectsStaleFrame()
{
	using namespace astrabot::compat;
	const ObservationContext value = context(
		"OBS-PLAYER-HEALTH", {4U, 9U}, {2U, 3U, 40U}, ObservationQuality::Exact,
		ObservationFreshness::SameTick, ObservationSource::PublicEdict);
	return check(value.isCurrent({2U, 3U, 40U}), "same frame is current") &&
		check(!value.isCurrent({2U, 4U, 1U}), "new round rejects stale frame");
}

bool testTimingContextIsAttachedWithoutSchedulerMutation()
{
	using namespace astrabot::compat;
	const ObservationTimingContext timing = {18U, 0U, 0U};
	const ObservationContext value = context(
		"OBS-PLAYER-FOV", {4U, 9U}, {2U, 3U, 40U}, ObservationQuality::Exact,
		ObservationFreshness::CurrentFullUpdate, ObservationSource::PublicEdict, 0U,
		timing);
	return check(value.timing.commandSequence == 18U,
			"command sequence is attached") &&
		check(value.timing.upkeepSequence == 0U && value.timing.fullUpdateSequence == 0U,
			"unavailable scheduler event sequences remain zero");
}

struct TraceCollector : public astrabot::compat::IObservationTraceSink
{
	TraceCollector() : count(0U), last{} {}

	void record(const astrabot::compat::ObservationTraceRecord &value) override
	{
		++count;
		last = value;
	}

	std::uint32_t count;
	astrabot::compat::ObservationTraceRecord last;
};

bool testTraceSinkReceivesSemanticMetadata()
{
	using namespace astrabot::compat;
	const ObservationValue<float> value = {
		90.0f,
		true,
		context("OBS-PLAYER-FOV", {4U, 9U}, {2U, 3U, 40U}, ObservationQuality::Exact,
			ObservationFreshness::SameTick, ObservationSource::PublicEdict)};
	TraceCollector trace;
	emitObservationTrace(value, ObservationValueKind::Float, &trace);
	return check(trace.count == 1U, "one observation trace is emitted") &&
		check(trace.last.context.semanticId != nullptr &&
			std::string(trace.last.context.semanticId) == "OBS-PLAYER-FOV",
			"semantic ID is retained") &&
		check(trace.last.floatValue == 90.0f, "trace value is retained");
}
}

int main()
{
	return testQualityAndFreshnessAreExplicit() &&
		testZeroAndFalseRemainValidWhenObserved() &&
		testUnavailableValueIsNotAvailable() &&
		testDelayedValueRetainsDelayAndSource() &&
		testActorGenerationPreventsCrossBotReuse() &&
		testLifecycleContextRejectsStaleFrame() &&
		testTimingContextIsAttachedWithoutSchedulerMutation() &&
		testTraceSinkReceivesSemanticMetadata() ? 0 : 1;
}
