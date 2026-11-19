#ifndef ASTRABOT_ADAPTER_PUBLIC_NAV_PROBE_HPP
#define ASTRABOT_ADAPTER_PUBLIC_NAV_PROBE_HPP
#include "astrabot/metamod/abi_contract.hpp"
#include "astrabot/compat/nav_area_lookup.hpp"
#include <cmath>
#include <cstring>
#include <functional>
#include <utility>
namespace astrabot
{
namespace metamod
{
class PublicNavProbe final : public compat::INoiseNavProbe
{
public:
	explicit PublicNavProbe(enginefuncs_t *engine, std::function<bool()> receipt = {})
		: engine_(engine), receipt_(std::move(receipt))
	{
	}
	bool available() const override
	{
		return available_ && current();
	}
	bool groundHeight(const nav::NavVector &position, float *height) override
	{
		if (!engine_ || !engine_->pfnTraceLine || !current())
			return unavailable();
		return compat::findBotGroundHeight(
			[&](float offset, std::uintptr_t ignored, compat::NavGroundTrace *observation)
			{
				if (!current())
					return unavailable();
				const float from[] = {position.x, position.y, position.z + offset};
			// Preserve the reference literal expression before storing public float coordinates.
#define ASTRABOT_PUBLIC_NAV_GROUND_DEPTH 9999.9f
				const float to[] = {position.x, position.y, position.z - ASTRABOT_PUBLIC_NAV_GROUND_DEPTH};
#undef ASTRABOT_PUBLIC_NAV_GROUND_DEPTH
				TraceResult trace{};
				engine_->pfnTraceLine(from, to, 1, reinterpret_cast<edict_t *>(ignored), &trace);
				if (!current() || !std::isfinite(trace.flFraction) || trace.flFraction < 0 || trace.flFraction > 1)
					return unavailable();
				if (!std::isfinite(trace.vecEndPos.z))
					return unavailable();
				observation->ground = trace.vecEndPos.z;
				observation->startSolid = trace.fStartSolid != 0;
				if (trace.flFraction != 1 && trace.pHit)
				{
					if (trace.pHit->free || !engine_->pfnSzFromIndex)
						return unavailable();
					const char *name = engine_->pfnSzFromIndex(trace.pHit->v.classname);
					if (!current() || !name)
						return unavailable();
					observation->walkable = std::strcmp(name, "func_door") == 0 ||
						std::strcmp(name, "func_door_rotating") == 0 ||
						(std::strcmp(name, "func_breakable") == 0 && trace.pHit->v.takedamage == DAMAGE_YES);
					observation->entity = reinterpret_cast<std::uintptr_t>(trace.pHit);
				}
				return true;
			},
			height);
	}
	bool clearLine(const nav::NavVector &from, const nav::NavVector &to) override
	{
		if (!engine_ || !engine_->pfnTraceLine || !current())
			return unavailable();
		const float start[] = {from.x, from.y, from.z}, end[] = {to.x, to.y, to.z};
		TraceResult trace{};
		engine_->pfnTraceLine(start, end, 0x101, nullptr, &trace);
		if (!current() || !std::isfinite(trace.flFraction) || trace.flFraction < 0 || trace.flFraction > 1)
			return unavailable();
		return trace.flFraction == 1;
	}

private:
	bool unavailable()
	{
		available_ = false;
		return false;
	}
	bool available_ = true;
	bool current() const
	{
		return !receipt_ || receipt_();
	}
	enginefuncs_t *engine_;
	std::function<bool()> receipt_;
};
}
}
#endif
