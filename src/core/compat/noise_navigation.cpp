#include "astrabot/compat/noise_navigation.hpp"
#include "astrabot/compat/nav_area_lookup.hpp"
#include <algorithm>
#include <cmath>
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
}
bool NavNoisePositionResolver::resolve(
	const world::AudibleEvent &sound, world::WorldPosition *position, std::uint32_t *areaId, std::uint64_t *revision)
{
	if (!position || !areaId || !revision || !snapshot_.isValid() || !std::isfinite(sound.distance) ||
		sound.distance < 0 || !std::isfinite(sound.origin.x) || !std::isfinite(sound.origin.y) ||
		!std::isfinite(sound.origin.z))
		return false;
	const nav::NavVector source{sound.origin.x, sound.origin.y, sound.origin.z};
	const auto area = findBotNavArea(snapshot_, source, probe_);
	const auto *selected = snapshot_.document()->findArea(area);
	if (!selected || (probe_ && !probe_->available()))
		return false;
	const float error = 400.0f * sound.distance / 2000.0f;
	const auto x = random_.nextFloat(RandomRequest::floatRequest("noise.position.x", actor_, timing_, -error, error));
	if (x.status != RandomStatus::Ok || !std::isfinite(x.value) || x.value < -error || x.value > error ||
		(probe_ && !probe_->available()))
		return false;
	const auto y = random_.nextFloat(RandomRequest::floatRequest("noise.position.y", actor_, timing_, -error, error));
	if (y.status != RandomStatus::Ok || !std::isfinite(y.value) || y.value < -error || y.value > error ||
		(probe_ && !probe_->available()))
		return false;
	const auto result = closest(*selected, {source.x + x.value, source.y + y.value, source.z});
	*position = {result.x, result.y, result.z};
	*areaId = selected->id;
	*revision = snapshot_.revision();
	return true;
}
float noiseTravelDistance(const nav::NavSnapshot &snapshot, nav::AreaId from, nav::AreaId to,
	const nav::NavPathCostContext &cost, const nav::NavLadderTopology &ladders)
{
	nav::NavQuery query(snapshot);
	query.setPathCostContext(cost);
	float distance = -1.0f;
	return query.travelDistance(from, to, nav::NavRouteType::Safest, ladders, &distance) == nav::NavQueryResult::Found
		? distance : -1.0f;
}
}
}
