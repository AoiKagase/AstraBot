#include "astrabot/compat/follow_request.hpp"
#include <cstdio>
#include <functional>
#include <limits>
#include <string>
#include <vector>
using namespace astrabot::compat;
namespace
{
struct FollowEnvironment : IFollowRequestEnvironment
{
	bool valid = true, isRogue = false, isBusy = false;
	bool rogueAvailable = true, distanceAvailable = true, followAvailable = true;
	float distance = 1000.0f;
	std::string invalidateAt;
	std::vector<std::string> calls;
	std::function<void(const std::string &)> after;
	bool current() const override
	{
		return valid;
	}
	void stage(const char *name)
	{
		calls.emplace_back(name);
		if (after)
			after(name);
		if (invalidateAt == name)
			valid = false;
	}
	bool rogue(bool *value) override
	{
		stage("rogue");
		*value = isRogue;
		return rogueAvailable;
	}
	bool busy() override
	{
		stage("busy");
		return isBusy;
	}
	bool travelDistanceToSender(float *value) override
	{
		stage("travel");
		*value = distance;
		return distanceAvailable;
	}
	bool followSender() override
	{
		stage("follow");
		return followAvailable;
	}
	void coveringFriend() override
	{
		stage("cover");
	}
};
}
int main()
{
	int failures = 0;
	const auto check = [&](bool value, const char *message)
	{
		if (!value)
		{
			std::fprintf(stderr, "%s\n", message);
			++failures;
		}
	};
	const std::vector<std::string> all{"rogue", "busy", "travel", "follow", "cover"};
	for (float distance : {0.0f, 749.0f, 1000.0f})
	{
		FollowEnvironment environment;
		environment.distance = distance;
		const auto result = respondToFollowRequest(environment);
		check(result.available && result.accepted, "Follow accepts zero/near/exact1000 without Help proximity no-op");
		check(environment.calls == all, "Follow uses Rogue -> Busy -> travel -> Follow -> immediate CoveringFriend");
	}
	for (float distance : {-1.0f, 1000.01f})
	{
		FollowEnvironment environment;
		environment.distance = distance;
		const auto result = respondToFollowRequest(environment);
		check(result.available && !result.accepted, "Follow rejects negative/unreachable and strictly over1000");
		check(environment.calls == std::vector<std::string>{"rogue", "busy", "travel"},
			"Rejected range has no Follow or speech");
	}
	{
		FollowEnvironment environment;
		environment.isRogue = true;
		const auto result = respondToFollowRequest(environment);
		check(result.available && !result.accepted && environment.calls == std::vector<std::string>{"rogue"},
			"Rogue rejects before Busy and NAV");
	}
	{
		FollowEnvironment environment;
		environment.isBusy = true;
		const auto result = respondToFollowRequest(environment);
		check(result.available && !result.accepted && environment.calls == std::vector<std::string>{"rogue", "busy"},
			"Busy rejects before sender NAV lookup");
	}
	for (float distance : {std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
	{
		FollowEnvironment environment;
		environment.distance = distance;
		const auto result = respondToFollowRequest(environment);
		check(!result.available && !result.accepted, "Nonfinite public range is unavailable, not a Follow order");
		check(environment.calls == std::vector<std::string>{"rogue", "busy", "travel"},
			"Nonfinite range stops at range provider");
	}
	for (const auto &invalid : all)
	{
		FollowEnvironment environment;
		environment.invalidateAt = invalid;
		const auto result = respondToFollowRequest(environment);
		check(!result.available && !result.accepted, "Each stale callback owner invalidates the response");
		std::vector<std::string> expected;
		for (const auto &name : all)
		{
			expected.push_back(name);
			if (name == invalid)
				break;
		}
		check(environment.calls == expected, "No later provider/action runs after a stale owner callback");
	}
	{
		FollowEnvironment environment;
		environment.valid = false;
		const auto result = respondToFollowRequest(environment);
		check(!result.available && !result.accepted && environment.calls.empty(),
			"Stale initial ownership performs no query");
	}
	for (int unavailable = 0; unavailable < 3; ++unavailable)
	{
		FollowEnvironment environment;
		environment.rogueAvailable = unavailable != 0;
		environment.distanceAvailable = unavailable != 1;
		environment.followAvailable = unavailable != 2;
		const auto result = respondToFollowRequest(environment);
		check(!result.available && !result.accepted, "Provider failure does not publish Follow acceptance");
		check(!environment.calls.empty() &&
				environment.calls.back() ==
					(unavailable == 0		   ? "rogue"
							: unavailable == 1 ? "travel"
											   : "follow"),
			"Failed provider prevents acknowledgement and later stages");
	}
	{
		FollowEnvironment environment;
		environment.distance = 0;
		environment.after = [&](const std::string &stage)
		{
			if (stage == "rogue")
				environment.distance = 1001;
		};
		const auto result = respondToFollowRequest(environment);
		check(result.available && !result.accepted && environment.calls.back() == "travel",
			"Range uses sender position after lazy Rogue callback, not an early copy");
	}
	{
		FollowEnvironment environment;
		const auto first = respondToFollowRequest(environment);
		const auto second = respondToFollowRequest(environment);
		check(first.accepted && second.accepted && environment.calls.size() == 10,
			"Repeated same-sender requests still call Follow and acknowledge; no frame-only deduplication");
	}
	return failures ? 1 : 0;
}
