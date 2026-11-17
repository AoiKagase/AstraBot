#ifndef ASTRABOT_ADAPTER_PUBLIC_NATIVE_LADDER_PROBE_HPP
#define ASTRABOT_ADAPTER_PUBLIC_NATIVE_LADDER_PROBE_HPP
#include "runtime/shared/public_nav_probe.hpp"
#include "astrabot/nav/native_ladder.hpp"
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>
namespace astrabot
{
namespace metamod
{
class PublicNativeLadderProbe final : public nav::INativeLadderProbe
{
public:
	PublicNativeLadderProbe(enginefuncs_t *engine, std::function<bool()> receipt)
		: engine_(engine), receipt_(std::move(receipt)), entity_(engine ? engine->pfnPEntityOfEntIndex : nullptr),
		  index_(engine ? engine->pfnIndexOfEdict : nullptr), name_(engine ? engine->pfnSzFromIndex : nullptr),
		  trace_(engine ? engine->pfnTraceLine : nullptr)
	{
	}
	bool current() const override
	{
		if (!engine_ || !receipt_ || !receipt_() || !entity_ || !index_ || !name_ || !trace_ ||
			engine_->pfnPEntityOfEntIndex != entity_ || engine_->pfnIndexOfEdict != index_ ||
			engine_->pfnSzFromIndex != name_ || engine_->pfnTraceLine != trace_)
			return false;
		return !source_ ||
			(!source_->free && source_->serialnumber == serial_ && source_->v.classname == classname_ &&
				source_->v.absmin.x == bounds_.lo.x && source_->v.absmin.y == bounds_.lo.y &&
				source_->v.absmin.z == bounds_.lo.z && source_->v.absmax.x == bounds_.hi.x &&
				source_->v.absmax.y == bounds_.hi.y && source_->v.absmax.z == bounds_.hi.z);
	}
	bool trace(
		const nav::NavLadder &ladder, const nav::NavVector &from, const nav::NavVector &to, float *fraction) override
	{
		if (!fraction)
			return false;
		*fraction = 0;
		if (!current() || !finite(from) || !finite(to) || !ladder.entityIndex ||
			ladder.entityIndex > static_cast<std::uint32_t>((std::numeric_limits<int>::max)()) ||
			!ladder.entityGeneration)
			return false;
		auto *entity = entity_(static_cast<int>(ladder.entityIndex));
		if (!current() || !matches(entity, ladder))
			return false;
		const int actual = index_(entity);
		if (!current() || actual != static_cast<int>(ladder.entityIndex) || !matches(entity, ladder))
			return false;
		source_ = entity;
		serial_ = entity->serialnumber;
		classname_ = entity->v.classname;
		bounds_ = {{entity->v.absmin.x, entity->v.absmin.y, entity->v.absmin.z},
			{entity->v.absmax.x, entity->v.absmax.y, entity->v.absmax.z}};
		if (!current())
			return false;
		const auto classname = name_(classname_);
		if (!current() || !classname || std::strcmp(classname, "func_ladder") != 0)
			return false;
		const float a[] = {from.x, from.y, from.z}, b[] = {to.x, to.y, to.z};
		TraceResult result{};
		trace_(a, b, 1, source_, &result);
		if (!current() || !std::isfinite(result.flFraction) || result.flFraction < 0 || result.flFraction > 1)
			return false;
		*fraction = result.flFraction;
		return true;
	}

private:
	static bool finite(const nav::NavVector &point)
	{
		return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
	}
	static bool matches(const edict_t *entity, const nav::NavLadder &ladder)
	{
		return entity && !entity->free && entity->serialnumber >= 0 &&
			static_cast<std::uint32_t>(entity->serialnumber) + 1U == ladder.entityGeneration;
	}
	enginefuncs_t *engine_ = nullptr;
	std::function<bool()> receipt_;
	decltype(enginefuncs_t::pfnPEntityOfEntIndex) entity_ = nullptr;
	decltype(enginefuncs_t::pfnIndexOfEdict) index_ = nullptr;
	decltype(enginefuncs_t::pfnSzFromIndex) name_ = nullptr;
	decltype(enginefuncs_t::pfnTraceLine) trace_ = nullptr;
	edict_t *source_ = nullptr;
	int serial_ = -1;
	string_t classname_ = 0;
	nav::NavExtent bounds_{};
};
}
}
#endif
