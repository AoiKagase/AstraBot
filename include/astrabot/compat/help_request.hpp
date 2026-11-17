#ifndef ASTRABOT_COMPAT_HELP_REQUEST_HPP
#define ASTRABOT_COMPAT_HELP_REQUEST_HPP
#include "astrabot/compat/random_source.hpp"
#include "astrabot/compat/state_machine.hpp"
#include "astrabot/nav/nav_query.hpp"
#include <functional>
#include <cmath>
namespace astrabot
{
namespace compat
{
inline bool nativeBotBusy(CompatibilityStateId state, CompatibilityTaskId task, bool attacking)
{
	return attacking || state == CompatibilityStateId::Buy || state == CompatibilityStateId::DefuseBomb ||
		task == CompatibilityTaskId::PlantBomb || task == CompatibilityTaskId::RescueHostages ||
		task == CompatibilityTaskId::MoveToSniperSpot || task == CompatibilityTaskId::Sniping;
}
enum class RetainedHelpTaskAction
{
	Continue,
	Idle,
	Repath
};
struct RetainedHelpTaskInput
{
	CompatibilityTaskId task = CompatibilityTaskId::None;
	bool bombScenario = false, counterTerrorist = false;
	bool bombPlantedKnown = false, bombPlanted = false, bombLocated = false, bombVisible = false, otherDefuser = false;
	bool taskLifeKnown = false, taskAlive = false;
	bool hostageFollowingKnown = false, hostageFollowing = false, hostagePositionKnown = false;
	nav::NavVector hostagePosition{}, goal{};
};
inline RetainedHelpTaskAction updateRetainedHelpTask(const RetainedHelpTaskInput &in)
{
	if (in.task == CompatibilityTaskId::MoveToLastKnownEnemyPosition && in.taskLifeKnown && !in.taskAlive)
		return RetainedHelpTaskAction::Idle;
	if (in.bombScenario)
	{
		if (in.task == CompatibilityTaskId::FindTickingBomb)
		{
			if ((in.bombPlantedKnown && !in.bombPlanted) || in.bombLocated)
				return RetainedHelpTaskAction::Idle;
		}
		else if (in.counterTerrorist && in.bombPlantedKnown && in.bombPlanted)
		{
			if (in.task != CompatibilityTaskId::DefuseBomb || (in.bombVisible && in.otherDefuser))
				return RetainedHelpTaskAction::Idle;
		}
	}
	if (in.task == CompatibilityTaskId::CollectHostages)
	{
		if ((in.taskLifeKnown && !in.taskAlive) || (in.hostageFollowingKnown && in.hostageFollowing))
			return RetainedHelpTaskAction::Idle;
		if (in.hostagePositionKnown && std::isfinite(in.hostagePosition.x) && std::isfinite(in.hostagePosition.y) &&
			std::isfinite(in.hostagePosition.z))
		{
			const float x = in.hostagePosition.x - in.goal.x, y = in.hostagePosition.y - in.goal.y,
						z = in.hostagePosition.z - in.goal.z;
			if (x * x + y * y + z * z > 75.0f * 75.0f)
				return RetainedHelpTaskAction::Repath;
		}
	}
	return RetainedHelpTaskAction::Continue;
}
struct HelpRequest
{
	std::uint16_t place = 0;
	float maximumRange = 3000.0f;
};
struct HelpRequestDecision
{
	bool available = false;
	bool accepted = false;
	bool moved = false;
	nav::NavVector target{};
};
class IHelpRequestEnvironment
{
public:
	virtual ~IHelpRequestEnvironment() = default;
	virtual bool current() const = 0;
	virtual bool rogue(bool *value) = 0;
	virtual bool busy() = 0;
	virtual bool travelDistanceToSender(float *distance) = 0;
	virtual bool squaredDistanceToSender(float *distance) = 0;
	virtual bool senderPosition(nav::NavVector *position) = 0;
	virtual std::uint16_t ownPlace() = 0;
	virtual bool randomSpotAtPlace(std::uint16_t place, nav::NavVector *position, bool *found) = 0;
	virtual bool moveTo(const nav::NavVector &position) = 0;
	virtual void onMyWay() = 0;
};
inline HelpRequestDecision respondToHelpRequest(HelpRequest request, IHelpRequestEnvironment &environment)
{
	if (!environment.current() || !std::isfinite(request.maximumRange))
		return {};
	bool rogue = false;
	if (!environment.rogue(&rogue) || !environment.current())
		return {};
	if (rogue)
		return {true, false, false, {}};
	const bool busy = environment.busy();
	if (!environment.current())
		return {};
	if (busy)
		return {true, false, false, {}};
	if (request.maximumRange > 0)
	{
		float distance = -1;
		if (!environment.travelDistanceToSender(&distance) || !environment.current() || !std::isfinite(distance))
			return {};
		if (distance < 0 || distance > request.maximumRange)
			return {true, false, false, {}};
	}
	nav::NavVector target{};
	if (!request.place)
	{
		float squaredDistance = 0;
		if (!environment.squaredDistanceToSender(&squaredDistance) || !environment.current() ||
			!std::isfinite(squaredDistance) || squaredDistance < 0)
			return {};
		if (squaredDistance < 750.0f * 750.0f)
			return {true, true, false, {}};
		if (!environment.senderPosition(&target) || !environment.current())
			return {};
	}
	else
	{
		const auto place = environment.ownPlace();
		if (!environment.current())
			return {};
		if (place == request.place)
			return {true, true, false, {}};
		bool found = false;
		if (!environment.randomSpotAtPlace(request.place, &target, &found) || !environment.current())
			return {};
		if (!found && (!environment.senderPosition(&target) || !environment.current()))
			return {};
	}
	if (!std::isfinite(target.x) || !std::isfinite(target.y) || !std::isfinite(target.z))
		return {};
	if (!environment.moveTo(target) || !environment.current())
		return {};
	environment.onMyWay();
	if (!environment.current())
		return {};
	return {true, true, true, target};
}
struct HelpPlaceSpot
{
	bool available = false;
	bool found = false;
	nav::NavVector position{};
};
inline HelpPlaceSpot chooseHelpPlaceSpot(nav::NavSnapshot navigation, std::uint16_t place,
	ICompatibilityRandomSource &random, RandomActor actor, RandomTimingContext timing,
	std::function<bool()> current = {})
{
	if (current && !current())
		return {};
	const auto *document = navigation.document();
	if (!document)
		return {true, false, {}};
	std::int32_t count = 0;
	nav::AreaId first = 0;
	for (const auto &area : document->areas())
		if (area.placeId == place)
		{
			if (!first)
				first = area.id;
			++count;
		}
	if (!count)
		return {true, false, {}};
	const auto draw = random.nextLong(RandomRequest::longRequest("RNG-CSBOT-HELP-PLACE", actor, timing, 0, count - 1));
	if ((current && !current()) || draw.status != RandomStatus::Ok || draw.value < 0 || draw.value >= count)
		return {};
	// The pinned native iterator never decrements its random index. Keep that observable quirk.
	if (draw.value != 0)
		return {true, false, {}};
	return {true, true, nav::extentCenterOf(*document->findArea(first))};
}
}
}
#endif
