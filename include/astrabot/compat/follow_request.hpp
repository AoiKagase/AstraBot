#ifndef ASTRABOT_COMPAT_FOLLOW_REQUEST_HPP
#define ASTRABOT_COMPAT_FOLLOW_REQUEST_HPP
#include <cmath>
namespace astrabot
{
namespace compat
{
struct FollowRequestDecision
{
	bool available = false;
	bool accepted = false;
};
class IFollowRequestEnvironment
{
public:
	virtual ~IFollowRequestEnvironment() = default;
	virtual bool current() const = 0;
	virtual bool rogue(bool *value) = 0;
	virtual bool busy() = 0;
	virtual bool travelDistanceToSender(float *distance) = 0;
	virtual bool followSender() = 0;
	virtual void coveringFriend() = 0;
};
inline FollowRequestDecision respondToFollowRequest(IFollowRequestEnvironment &environment)
{
	if (!environment.current())
		return {};
	bool rogue = false;
	if (!environment.rogue(&rogue) || !environment.current())
		return {};
	if (rogue)
		return {true, false};
	const bool busy = environment.busy();
	if (!environment.current())
		return {};
	if (busy)
		return {true, false};
	float distance = -1;
	if (!environment.travelDistanceToSender(&distance) || !environment.current() || !std::isfinite(distance))
		return {};
	if (distance < 0 || distance > 1000.0f)
		return {true, false};
	if (!environment.followSender() || !environment.current())
		return {};
	environment.coveringFriend();
	if (!environment.current())
		return {};
	return {true, true};
}
}
}
#endif
