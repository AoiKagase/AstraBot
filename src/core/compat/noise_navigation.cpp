#include "astrabot/compat/noise_navigation.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
namespace astrabot
{
namespace compat
{
namespace
{
nav::NavVector closest(const nav::NavArea &a, const nav::NavVector &p)
{
	const float x = (std::max)(a.extent.lo.x, (std::min)(p.x, a.extent.hi.x));
	const float y = (std::max)(a.extent.lo.y, (std::min)(p.y, a.extent.hi.y));
	return {x, y, nav::surfaceZAt(a, x, y)};
}
nav::NavVector center(const nav::NavArea &a)
{
	return closest(a, {(a.extent.lo.x + a.extent.hi.x) * .5f, (a.extent.lo.y + a.extent.hi.y) * .5f, 0});
}
}
bool NavNoisePositionResolver::resolve(
	const world::AudibleEvent &sound, world::WorldPosition *position, std::uint32_t *areaId, std::uint64_t *revision)
{
	if (!position || !areaId || !revision || !snapshot_.isValid() || !std::isfinite(sound.distance) ||
		sound.distance < 0 || !std::isfinite(sound.origin.x) || !std::isfinite(sound.origin.y) ||
		!std::isfinite(sound.origin.z))
		return false;
	const nav::NavVector source{sound.origin.x, sound.origin.y, sound.origin.z};
	const nav::NavArea *selected = nullptr;
	float highest = std::numeric_limits<float>::lowest();
	// CSBot chooses the highest overlapping floor beneath origin+5, within120 below.
	for (const auto &a : snapshot_.document()->areas())
	{
		if (source.x < a.extent.lo.x || source.x > a.extent.hi.x || source.y < a.extent.lo.y ||
			source.y > a.extent.hi.y)
			continue;
		const float z = nav::surfaceZAt(a, source.x, source.y);
		if (z <= source.z + 5 && z >= source.z - 120 && z > highest)
		{
			highest = z;
			selected = &a;
		}
	}
	if (!selected)
	{
		if (!probe_)
			return false;
		float ground = 0;
		if (!probe_->groundHeight(source, &ground) || !std::isfinite(ground))
			return false;
		const nav::NavVector from{source.x, source.y, ground + 36};
		float best = 10000.0f * 10000.0f;
		for (const auto &a : snapshot_.document()->areas())
		{
			const auto point = closest(a, from);
			const float dx = point.x - from.x, dy = point.y - from.y, dz = point.z - from.z;
			const float distance = dx * dx + dy * dy + dz * dz;
			if (distance < best && probe_->clearLine(from, {point.x, point.y, point.z + 36}))
			{
				best = distance;
				selected = &a;
			}
		}
	}
	if (!selected)
		return false;
	const float error = 400.0f * sound.distance / 2000.0f;
	const auto x = random_.nextFloat(RandomRequest::floatRequest("noise.position.x", actor_, timing_, -error, error));
	if (x.status != RandomStatus::Ok || !std::isfinite(x.value) || x.value < -error || x.value > error)
		return false;
	const auto y = random_.nextFloat(RandomRequest::floatRequest("noise.position.y", actor_, timing_, -error, error));
	if (y.status != RandomStatus::Ok || !std::isfinite(y.value) || y.value < -error || y.value > error)
		return false;
	const auto result = closest(*selected, {source.x + x.value, source.y + y.value, source.z});
	*position = {result.x, result.y, result.z};
	*areaId = selected->id;
	*revision = snapshot_.revision();
	return true;
}
float noiseTravelDistance(const nav::NavSnapshot &snapshot, nav::AreaId from, nav::AreaId to,
	const nav::NavPathCostContext &cost, nav::NavSearchStats *stats)
{
	if (!snapshot.isValid())
		return -1;
	nav::NavQuery query(snapshot);
	query.setPathCostContext(cost);
	nav::NavCorridor corridor{};
	if (query.buildCorridor(from, to, nav::NavRouteType::Fastest, &corridor, stats) != nav::NavQueryResult::Found)
		return -1;
	float distance = 0;
	for (std::size_t i = 1; i < corridor.areas.size(); ++i)
	{
		const auto a = center(*snapshot.document()->findArea(corridor.areas[i - 1]));
		const auto b = center(*snapshot.document()->findArea(corridor.areas[i]));
		const float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
		distance += std::sqrt(dx * dx + dy * dy + dz * dz);
	}
	return distance;
}
}
}
