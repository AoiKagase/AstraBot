#ifndef ASTRABOT_ADAPTER_PUBLIC_NAV_LADDERS_HPP
#define ASTRABOT_ADAPTER_PUBLIC_NAV_LADDERS_HPP
#include "runtime/shared/public_nav_probe.hpp"
#include "astrabot/compat/nav_ladders.hpp"
#include <new>
namespace astrabot
{
namespace metamod
{
// Own a read-only runtime graph, independently of serialized NAV and ground locomotion.
class PublicNavLadderCache
{
public:
	bool read(enginefuncs_t *engine, const nav::NavSnapshot &navigation, const std::function<bool()> &receipt,
		nav::NavLadderTopology *out)
	{
		if (!out)
			return false;
		*out = {};
		const auto request = ++request_;
		if (!engine || !navigation.isValid() || !engine->pfnFindEntityByString || !engine->pfnTraceLine ||
			!engine->pfnIndexOfEdict)
			return false;
		const auto find = engine->pfnFindEntityByString;
		const auto trace = engine->pfnTraceLine;
		const auto index = engine->pfnIndexOfEdict;
		const auto name = engine->pfnSzFromIndex;
		struct EntityReceipt
		{
			edict_t *entity;
			int serial;
			nav::NavExtent bounds;
		};
		std::vector<EntityReceipt> entities;
		const auto current = [&]
		{
			if ((receipt && !receipt()) || request != request_ || engine->pfnFindEntityByString != find ||
				engine->pfnTraceLine != trace || engine->pfnIndexOfEdict != index || engine->pfnSzFromIndex != name)
				return false;
			for (const auto &saved : entities)
			{
				const auto *entity = saved.entity;
				if (entity->free || entity->serialnumber != saved.serial || entity->v.absmin.x != saved.bounds.lo.x ||
					entity->v.absmin.y != saved.bounds.lo.y || entity->v.absmin.z != saved.bounds.lo.z ||
					entity->v.absmax.x != saved.bounds.hi.x || entity->v.absmax.y != saved.bounds.hi.y ||
					entity->v.absmax.z != saved.bounds.hi.z)
					return false;
			}
			return true;
		};
		if (!current())
			return false;
		try
		{
			if (cachedEngine_ == engine && cachedFind_ == find && cachedTrace_ == trace && cachedIndex_ == index &&
				cachedName_ == name && cachedNavigation_.document() == navigation.document() &&
				cached_.navRevision == navigation.revision() && cached_.mapGeneration == navigation.mapGeneration())
			{
				auto output = cached_;
				*out = std::move(output);
				return true;
			}
			std::vector<compat::PublicLadderBounds> bounds;
			edict_t *after = nullptr;
			while (true)
			{
				if (!current())
					return false;
				auto *entity = find(after, "classname", "func_ladder");
				if (!current())
					return false;
				if (!entity)
					break;
				for (const auto &saved : entities)
					if (saved.entity == entity)
						return false;
				const nav::NavExtent extent{{entity->v.absmin.x, entity->v.absmin.y, entity->v.absmin.z},
					{entity->v.absmax.x, entity->v.absmax.y, entity->v.absmax.z}};
				entities.push_back({entity, entity->serialnumber, extent});
				const int entityIndex = index(entity);
				if (!current())
					return false;
				if (entityIndex == 0)
					break; // GoldSrc's null-entity/world sentinel.
				if (entityIndex < 0 || bounds.size() >= nav::NavLimits::kMaximumAreas)
					return false;

				bounds.push_back({reinterpret_cast<std::uintptr_t>(entity), extent,
					static_cast<std::uint32_t>(entityIndex), static_cast<std::uint32_t>(entities.back().serial) + 1U});
				after = entity;
			}
			class Probe final : public compat::ILadderNavProbe
			{
			public:
				Probe(enginefuncs_t *engine, std::function<bool()> receipt)
					: engine_(engine), receipt_(std::move(receipt)), ground_(engine, receipt_)
				{
				}
				bool available() const override
				{
					return ground_.available();
				}
				bool groundHeight(const nav::NavVector &position, float *height) override
				{
					return ground_.groundHeight(position, height);
				}
				bool clearLine(const nav::NavVector &from, const nav::NavVector &to) override
				{
					return ground_.clearLine(from, to);
				}
				bool traceLadder(const nav::NavVector &from, const nav::NavVector &to, std::uintptr_t ignored,
					compat::LadderTrace *result) override
				{
					if (!receipt_())
						return false;
					const float start[] = {from.x, from.y, from.z}, end[] = {to.x, to.y, to.z};
					TraceResult trace{};
					engine_->pfnTraceLine(start, end, 1, reinterpret_cast<edict_t *>(ignored), &trace);
					if (!receipt_())
						return false;
					result->fraction = trace.flFraction;
					result->startSolid = trace.fStartSolid != 0;
					return true;
				}

			private:
				enginefuncs_t *engine_;
				std::function<bool()> receipt_;
				PublicNavProbe ground_;
			} probe(engine, current);
			nav::NavLadderTopology candidate;
			if (!compat::buildBotNavLadders(navigation, bounds, probe, &candidate) || !current())
				return false;
			// Prepare all allocating copies before publishing cache identity.
			auto output = candidate;
			cached_ = std::move(candidate);
			cachedNavigation_ = navigation;
			cachedEngine_ = engine;
			cachedFind_ = find;
			cachedTrace_ = trace;
			cachedIndex_ = index;
			cachedName_ = name;
			*out = std::move(output);
			return true;
		}
		catch (const std::bad_alloc &)
		{
			return false;
		}
	}

private:
	std::uint64_t request_ = 0;
	nav::NavSnapshot cachedNavigation_;
	nav::NavLadderTopology cached_;
	enginefuncs_t *cachedEngine_ = nullptr;
	decltype(enginefuncs_t::pfnFindEntityByString) cachedFind_ = nullptr;
	decltype(enginefuncs_t::pfnTraceLine) cachedTrace_ = nullptr;
	decltype(enginefuncs_t::pfnIndexOfEdict) cachedIndex_ = nullptr;
	decltype(enginefuncs_t::pfnSzFromIndex) cachedName_ = nullptr;
};
}
}
#endif
