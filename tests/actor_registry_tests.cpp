#include "astrabot/runtime/actor_registry.hpp"

#include <cstdio>

namespace
{
	bool check(bool condition, const char *description)
	{
		if (condition)
		{
			return true;
		}

		std::fprintf(stderr, "check failed: %s\n", description);
		return false;
	}

	astrabot::runtime::LifecycleToken tokenForSlot(
		std::uint32_t slot,
		std::uint32_t slotGeneration)
	{
		return {1U, 1U, slotGeneration, slot};
	}
}

int main()
{
	using astrabot::runtime::ActorId;
	using astrabot::runtime::ActorRegistry;
	using astrabot::runtime::ActorResult;
	using astrabot::runtime::ActorState;

	ActorRegistry registry;
	ActorId firstActor{};
	const auto firstToken = tokenForSlot(1U, 1U);
	if (!check(registry.reserve(1U, firstToken, &firstActor) == ActorResult::Accepted,
			"first actor reservation succeeds"))
	{
		return 1;
	}
	if (!check(firstActor.slot == 1U && firstActor.actorGeneration != 0U,
			"first actor has stable identity"))
	{
		return 1;
	}
	if (!check(registry.state(firstActor) == ActorState::Reserved,
			"reservation enters reserved state"))
	{
		return 1;
	}
	if (!check(registry.reserve(1U, firstToken, nullptr) == ActorResult::SlotBusy,
			"duplicate reservation is rejected"))
	{
		return 1;
	}
	if (!check(registry.beginJoin(firstActor) == ActorResult::Accepted,
			"join transition succeeds"))
	{
		return 1;
	}
	if (!check(registry.markJoined(firstActor) == ActorResult::Accepted,
			"joined transition succeeds"))
	{
		return 1;
	}
	if (!check(registry.state(firstActor) == ActorState::Joined,
			"actor reaches joined state"))
	{
		return 1;
	}
	if (!check(registry.isCurrent(firstActor, firstToken),
			"current actor and lifecycle token are accepted"))
	{
		return 1;
	}
	if (!check(registry.beginRemoval(firstActor) == ActorResult::Accepted,
			"removal transition succeeds"))
	{
		return 1;
	}
	if (!check(registry.release(firstActor) == ActorResult::Accepted,
			"actor release succeeds"))
	{
		return 1;
	}
	if (!check(!registry.isCurrent(firstActor, firstToken),
			"released actor is stale"))
	{
		return 1;
	}
	if (!check(registry.release(firstActor) == ActorResult::NotFound,
			"repeated release is harmless"))
	{
		return 1;
	}

	ActorId reusedActor{};
	const auto reusedToken = tokenForSlot(1U, 2U);
	if (!check(registry.reserve(1U, reusedToken, &reusedActor) == ActorResult::Accepted,
			"slot reuse reservation succeeds"))
	{
		return 1;
	}
	if (!check(reusedActor.actorGeneration != firstActor.actorGeneration,
			"slot reuse advances actor generation"))
	{
		return 1;
	}
	if (!check(!registry.isCurrent(firstActor, reusedToken),
			"old actor cannot address reused slot"))
	{
		return 1;
	}
	if (!check(registry.reserve(0U, reusedToken, nullptr) == ActorResult::InvalidSlot,
			"zero slot is rejected"))
	{
		return 1;
	}
	if (!check(registry.reserve(33U, reusedToken, nullptr) == ActorResult::InvalidSlot,
			"out of range slot is rejected"))
	{
		return 1;
	}
	const astrabot::runtime::LifecycleToken invalidToken{};
	if (!check(registry.reserve(2U, invalidToken, nullptr) == ActorResult::InvalidToken,
			"invalid lifecycle token is rejected"))
	{
		return 1;
	}
	return 0;
}
