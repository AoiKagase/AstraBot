#ifndef ASTRABOT_COMPAT_CHATTER_ENEMY_PLACE_HPP
#define ASTRABOT_COMPAT_CHATTER_ENEMY_PLACE_HPP
#include "astrabot/world/world_snapshot.hpp"
#include "astrabot/compat/nav_area_lookup.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
namespace astrabot
{
namespace compat
{
class ChatterKnownArea
{
public:
	void observe(world::ActorKey actor, world::FrameIdentity frame, std::uint64_t revision, std::uint32_t area)
	{
		if (!actor.isValid() || !frame.isValid() || !revision)
			return;
		if (!(actor == actor_) || frame.mapGeneration != frame_.mapGeneration ||
			frame.roundGeneration != frame_.roundGeneration || revision != revision_ || frame.tick < frame_.tick)
		{
			area_ = 0;
			current_ = 0;
			cacheArea_ = 0;
			overlaps_.clear();
		}
		if (area != current_)
		{
			cacheArea_ = 0;
			overlaps_.clear();
		}
		current_ = area;
		actor_ = actor;
		frame_ = frame;
		revision_ = revision;
		if (area)
			area_ = area;
	}
	std::uint32_t read(world::ActorKey actor, world::FrameIdentity frame, std::uint64_t revision) const
	{
		return actor == actor_ && frame.isValid() && frame.mapGeneration == frame_.mapGeneration &&
				frame.roundGeneration == frame_.roundGeneration && frame.tick >= frame_.tick && revision == revision_
			? area_
			: 0;
	}
	std::uint32_t current(world::ActorKey actor, world::FrameIdentity frame, std::uint64_t revision) const
	{
		return read(actor, frame, revision) ? current_ : 0;
	}
	void update(const nav::NavSnapshot &snapshot, world::ActorKey actor, world::FrameIdentity frame,
		const nav::NavVector &position)
	{
		if (!snapshot.isValid() || snapshot.mapGeneration() != frame.mapGeneration || !actor.isValid() ||
			!frame.isValid())
			return;
		auto area = current(actor, frame, snapshot.revision());
		if (area)
		{
			if (cacheArea_ != area)
			{
				overlaps_.clear();
				const auto *selected = snapshot.document()->findArea(area);
				if (selected)
					for (const auto &candidate : snapshot.document()->areas())
						if (candidate.id != area && botNavAreasOverlap(*selected, candidate))
							overlaps_.push_back(candidate.id);
				cacheArea_ = area;
			}
			if (!botNavAreaContains(snapshot, area, position, &overlaps_))
				area = 0;
		}
		if (!area)
			area = findBotNavArea(snapshot, position, nullptr);
		observe(actor, frame, snapshot.revision(), area);
	}
	void reset()
	{
		actor_ = {};
		frame_ = {};
		revision_ = 0;
		area_ = 0;
		current_ = 0;
		cacheArea_ = 0;
		overlaps_.clear();
	}

private:
	world::ActorKey actor_{};
	world::FrameIdentity frame_{};
	std::uint64_t revision_ = 0;
	std::uint32_t area_ = 0;
	std::uint32_t current_ = 0;
	std::uint32_t cacheArea_ = 0;
	std::vector<nav::AreaId> overlaps_;
};
struct ChatterVisiblePlace
{
	std::uint16_t place;
	float distanceSquared;
};
inline std::uint16_t chooseChatterEnemyPlace(const ChatterVisiblePlace *places, std::size_t count)
{
	constexpr std::size_t kMaximumThreats = 32;
	if (!places || count > kMaximumThreats)
		return 0;
	std::array<std::size_t, kMaximumThreats> order{};
	std::size_t valid = 0;
	for (std::size_t i = 0; i < count; ++i)
		if (places[i].place && std::isfinite(places[i].distanceSquared) && places[i].distanceSquared >= 0)
			order[valid++] = i;
	std::sort(order.begin(), order.begin() + valid,
		[&](std::size_t a, std::size_t b)
		{
			return places[a].distanceSquared < places[b].distanceSquared ||
				(places[a].distanceSquared == places[b].distanceSquared && a < b);
		});
	std::uint16_t winner = 0;
	std::size_t best = 0;
	for (std::size_t i = 0; i < valid; ++i)
	{
		std::size_t seen = 0;
		for (std::size_t j = 0; j <= i; ++j)
			if (places[order[j]].place == places[order[i]].place)
				++seen;
		if (seen > best)
		{
			winner = places[order[i]].place;
			best = seen;
		}
	}
	return winner;
}
}
}
#endif
