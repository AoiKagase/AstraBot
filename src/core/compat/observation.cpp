#include "astrabot/compat/observation.hpp"

namespace astrabot
{
namespace compat
{
bool isAvailableQuality(ObservationQuality quality)
{
	return quality != ObservationQuality::Unavailable &&
		quality != ObservationQuality::NotYetImplemented &&
		quality != ObservationQuality::NotRequired &&
		quality != ObservationQuality::Unknown;
}

bool ObservationContext::isValid() const
{
	return semanticId != nullptr && semanticId[0] != '\0' && actor.isValid() &&
		frame.isValid();
}

bool ObservationContext::isCurrent(const world::FrameIdentity &frameValue) const
{
	return isValid() && frame == frameValue;
}

const char *toString(ObservationQuality quality)
{
	switch (quality)
	{
	case ObservationQuality::Exact: return "EXACT";
	case ObservationQuality::ExactEngineApi: return "EXACT_ENGINE_API";
	case ObservationQuality::ExactGameApi: return "EXACT_GAME_API";
	case ObservationQuality::Delayed: return "DELAYED";
	case ObservationQuality::Inferred: return "INFERRED";
	case ObservationQuality::Approximated: return "APPROXIMATED";
	case ObservationQuality::Unavailable: return "UNAVAILABLE";
	case ObservationQuality::NotYetImplemented: return "NOT_YET_IMPLEMENTED";
	case ObservationQuality::NotRequired: return "NOT_REQUIRED";
	case ObservationQuality::Unknown: return "UNKNOWN";
	default: return "UNKNOWN";
	}
}

const char *toString(ObservationFreshness freshness)
{
	switch (freshness)
	{
	case ObservationFreshness::SameTick: return "same_tick";
	case ObservationFreshness::CurrentFullUpdate: return "current_full_update";
	case ObservationFreshness::EventDrivenCached: return "event_driven_cached";
	case ObservationFreshness::Stale: return "stale";
	default: return "stale";
	}
}

const char *toString(ObservationSource source)
{
	switch (source)
	{
	case ObservationSource::PublicEdict: return "public_edict";
	case ObservationSource::PublicEngineApi: return "public_engine_api";
	case ObservationSource::PublicGameDll: return "public_gamedll";
	case ObservationSource::AdapterCache: return "adapter_cache";
	case ObservationSource::Fixture: return "fixture";
	case ObservationSource::Synthetic: return "synthetic";
	case ObservationSource::None: return "none";
	default: return "none";
	}
}

bool CompatibilityObservation::isValid() const
{
	return actor.isValid() && frame.isValid();
}

namespace
{
ObservationTraceRecord baseTrace(
	const ObservationContext &context,
	ObservationValueKind kind)
{
	ObservationTraceRecord record = {};
	record.context = context;
	record.kind = kind;
	return record;
}
}

void emitObservationTrace(
	const ObservationValue<float> &value,
	ObservationValueKind kind,
	IObservationTraceSink *sink)
{
	if (sink == nullptr || !value.isAvailable())
	{
		return;
	}
	ObservationTraceRecord record = baseTrace(value.context, kind);
	record.floatValue = value.value;
	sink->record(record);
}

void emitObservationTrace(
	const ObservationValue<std::int32_t> &value,
	ObservationValueKind kind,
	IObservationTraceSink *sink)
{
	if (sink == nullptr || !value.isAvailable())
	{
		return;
	}
	ObservationTraceRecord record = baseTrace(value.context, kind);
	record.integerValue = value.value;
	sink->record(record);
}

void emitObservationTrace(
	const ObservationValue<bool> &value,
	ObservationValueKind kind,
	IObservationTraceSink *sink)
{
	if (sink == nullptr || !value.isAvailable())
	{
		return;
	}
	ObservationTraceRecord record = baseTrace(value.context, kind);
	record.booleanValue = value.value;
	sink->record(record);
}

void emitObservationTrace(
	const ObservationValue<world::WorldVector> &value,
	ObservationValueKind kind,
	IObservationTraceSink *sink)
{
	if (sink == nullptr || !value.isAvailable())
	{
		return;
	}
	ObservationTraceRecord record = baseTrace(value.context, kind);
	record.vectorValue = value.value;
	sink->record(record);
}
}
}
