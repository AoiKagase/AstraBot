#ifndef ASTRABOT_COMPAT_DEFEND_REQUEST_HPP
#define ASTRABOT_COMPAT_DEFEND_REQUEST_HPP

#include "astrabot/nav/nav_model.hpp"
#include <cmath>

namespace astrabot
{
namespace compat
{
enum class DefendRequestResult
{
	Unavailable,
	Ignored,
	HideAtSpot,
	HideNearby
};

class IDefendRequestEnvironment
{
public:
	virtual ~IDefendRequestEnvironment() = default;
	virtual bool current() const = 0;
	virtual bool rogue(bool *value) = 0;
	virtual bool busy() = 0;
	virtual bool placeAt(const nav::NavVector &position, std::uint16_t *value) = 0;
	virtual bool randomHidingSpot(std::uint16_t place, nav::NavVector *position, bool *found) = 0;
	virtual bool hideAtSpot(const nav::NavVector &position) = 0;
	virtual bool hideNearPosition(const nav::NavVector &position) = 0;
	virtual void affirmative() = 0;
};

inline DefendRequestResult respondToDefendRequest(const nav::NavVector &position, IDefendRequestEnvironment &environment)
{
	using Result = DefendRequestResult;
	if (!environment.current() || !std::isfinite(position.x) || !std::isfinite(position.y) ||
		!std::isfinite(position.z))
		return Result::Unavailable;

	bool rogue = false;
	if (!environment.rogue(&rogue) || !environment.current())
		return Result::Unavailable;
	if (rogue)
		return Result::Ignored;

	const bool busy = environment.busy();
	if (!environment.current())
		return Result::Unavailable;
	if (busy)
		return Result::Ignored;

	std::uint16_t place = 0;
	if (!environment.placeAt(position, &place) || !environment.current())
		return Result::Unavailable;
	if (place)
	{
		nav::NavVector spot{};
		bool found = false;
		if (!environment.randomHidingSpot(place, &spot, &found) || !environment.current())
			return Result::Unavailable;
		if (found)
		{
			if (!environment.hideAtSpot(spot) || !environment.current())
				return Result::Unavailable;
			return Result::HideAtSpot;
		}
	}

	if (!environment.hideNearPosition(position) || !environment.current())
		return Result::Unavailable;
	environment.affirmative();
	return environment.current() ? Result::HideNearby : Result::Unavailable;
}
}
}
#endif
