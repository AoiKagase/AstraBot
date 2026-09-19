#include "engine_random_source.hpp"

namespace astrabot
{
namespace compat
{
EngineRandomSource::EngineRandomSource()
	: callbacks_({nullptr, nullptr}), sequence_(0U), traceSink_(nullptr)
{
}

void EngineRandomSource::configure(const EngineRandomCallbacks &callbacks)
{
	callbacks_ = callbacks;
}

bool EngineRandomSource::available() const
{
	return callbacks_.randomLong != nullptr && callbacks_.randomFloat != nullptr;
}

void EngineRandomSource::setTraceSink(IRandomTraceSink *sink)
{
	traceSink_ = sink;
}

RandomFloatResult EngineRandomSource::nextFloat(const RandomRequest &request)
{
	if (request.type != RandomType::Float)
	{
		return {RandomStatus::InvalidRequest, 0.0f};
	}
	if (callbacks_.randomFloat == nullptr)
	{
		return {RandomStatus::Unavailable, 0.0f};
	}
	const float value = callbacks_.randomFloat(request.floatLower, request.floatUpper);
	if (traceSink_ != nullptr)
	{
		RandomTraceRecord record = {};
		record.sequence = ++sequence_;
		record.request = request;
		record.floatResult = value;
		traceSink_->record(record);
	}
	return {RandomStatus::Ok, value};
}

RandomLongResult EngineRandomSource::nextLong(const RandomRequest &request)
{
	if (request.type != RandomType::Long)
	{
		return {RandomStatus::InvalidRequest, 0};
	}
	if (callbacks_.randomLong == nullptr)
	{
		return {RandomStatus::Unavailable, 0};
	}
	const std::int32_t value = callbacks_.randomLong(request.longLower, request.longUpper);
	if (traceSink_ != nullptr)
	{
		RandomTraceRecord record = {};
		record.sequence = ++sequence_;
		record.request = request;
		record.longResult = value;
		traceSink_->record(record);
	}
	return {RandomStatus::Ok, value};
}
}
}
