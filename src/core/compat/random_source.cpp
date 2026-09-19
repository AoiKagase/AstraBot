#include "astrabot/compat/random_source.hpp"

namespace astrabot
{
namespace compat
{
namespace
{
bool sameActor(const RandomActor &left, const RandomActor &right)
{
	return left.slot == right.slot && left.generation == right.generation;
}

bool sameTiming(const RandomTimingContext &left, const RandomTimingContext &right)
{
	return left.commandSequence == right.commandSequence &&
		left.upkeepSequence == right.upkeepSequence &&
		left.fullUpdateSequence == right.fullUpdateSequence;
}

bool sameSite(const RandomTapeEntry &entry, const RandomRequest &request)
{
	return !entry.requireSemanticId ||
		(entry.semanticId == (request.semanticId == nullptr ? "" : request.semanticId));
}

bool sameRequest(const RandomTapeEntry &entry, const RandomRequest &request)
{
	if (entry.request.type != request.type || !sameActor(entry.request.actor, request.actor) ||
		!sameTiming(entry.request.timing, request.timing) || !sameSite(entry, request))
	{
		return false;
	}
	if (request.type == RandomType::Float)
	{
		return entry.request.floatLower == request.floatLower &&
			entry.request.floatUpper == request.floatUpper;
	}
	return entry.request.longLower == request.longLower &&
		entry.request.longUpper == request.longUpper;
}
}

RandomRequest RandomRequest::floatRequest(
	const char *semanticId,
	RandomActor actor,
	RandomTimingContext timing,
	float lower,
	float upper)
{
	RandomRequest request = {};
	request.type = RandomType::Float;
	request.semanticId = semanticId;
	request.actor = actor;
	request.timing = timing;
	request.floatLower = lower;
	request.floatUpper = upper;
	return request;
}

RandomRequest RandomRequest::longRequest(
	const char *semanticId,
	RandomActor actor,
	RandomTimingContext timing,
	std::int32_t lower,
	std::int32_t upper)
{
	RandomRequest request = {};
	request.type = RandomType::Long;
	request.semanticId = semanticId;
	request.actor = actor;
	request.timing = timing;
	request.longLower = lower;
	request.longUpper = upper;
	return request;
}

RandomTapeEntry RandomTapeEntry::floatEntry(
	const char *semanticId,
	RandomActor actor,
	RandomTimingContext timing,
	float lower,
	float upper,
	float value)
{
	RandomTapeEntry entry = {};
	entry.request = RandomRequest::floatRequest(semanticId, actor, timing, lower, upper);
	entry.floatValue = value;
	entry.longValue = 0;
	entry.requireSemanticId = semanticId != nullptr;
	entry.semanticId = semanticId == nullptr ? "" : semanticId;
	return entry;
}

RandomTapeEntry RandomTapeEntry::longEntry(
	const char *semanticId,
	RandomActor actor,
	RandomTimingContext timing,
	std::int32_t lower,
	std::int32_t upper,
	std::int32_t value)
{
	RandomTapeEntry entry = {};
	entry.request = RandomRequest::longRequest(semanticId, actor, timing, lower, upper);
	entry.floatValue = 0.0f;
	entry.longValue = value;
	entry.requireSemanticId = semanticId != nullptr;
	entry.semanticId = semanticId == nullptr ? "" : semanticId;
	return entry;
}

ScriptedRandomSource::ScriptedRandomSource(const std::vector<RandomTapeEntry> &tape)
	: tape_(tape), position_(0U), failed_(false), sequence_(0U), traceSink_(nullptr)
{
}

RandomFloatResult ScriptedRandomSource::nextFloat(const RandomRequest &request)
{
	if (request.type != RandomType::Float)
	{
		failed_ = true;
		return {RandomStatus::InvalidRequest, 0.0f};
	}
	if (position_ >= tape_.size())
	{
		failed_ = true;
		return {RandomStatus::TapeExhausted, 0.0f};
	}
	const RandomTapeEntry &entry = tape_[position_];
	if (!sameRequest(entry, request))
	{
		failed_ = true;
		return {RandomStatus::RequestMismatch, 0.0f};
	}
	const float value = entry.floatValue;
	++position_;
	++sequence_;
	if (traceSink_ != nullptr)
	{
		RandomTraceRecord record = {};
		record.sequence = sequence_;
		record.request = request;
		record.floatResult = value;
		traceSink_->record(record);
	}
	return {RandomStatus::Ok, value};
}

RandomLongResult ScriptedRandomSource::nextLong(const RandomRequest &request)
{
	if (request.type != RandomType::Long)
	{
		failed_ = true;
		return {RandomStatus::InvalidRequest, 0};
	}
	if (position_ >= tape_.size())
	{
		failed_ = true;
		return {RandomStatus::TapeExhausted, 0};
	}
	const RandomTapeEntry &entry = tape_[position_];
	if (!sameRequest(entry, request))
	{
		failed_ = true;
		return {RandomStatus::RequestMismatch, 0};
	}
	const std::int32_t value = entry.longValue;
	++position_;
	++sequence_;
	if (traceSink_ != nullptr)
	{
		RandomTraceRecord record = {};
		record.sequence = sequence_;
		record.request = request;
		record.longResult = value;
		traceSink_->record(record);
	}
	return {RandomStatus::Ok, value};
}

void ScriptedRandomSource::setTraceSink(IRandomTraceSink *sink)
{
	traceSink_ = sink;
}

bool ScriptedRandomSource::verifyComplete() const
{
	return !failed_ && position_ == tape_.size();
}

bool ScriptedRandomSource::failed() const
{
	return failed_;
}

std::size_t ScriptedRandomSource::position() const
{
	return position_;
}
}
}
