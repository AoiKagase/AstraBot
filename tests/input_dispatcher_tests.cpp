#include "astrabot/metamod/input_dispatcher.hpp"

#include <cstdio>

namespace
{
	edict_t *gDispatchedEntity = nullptr;
	float gDispatchedAngles[3]{};
	float gDispatchedForward = 0.0f;
	float gDispatchedSide = 0.0f;
	float gDispatchedUp = 0.0f;
	unsigned short gDispatchedButtons = 0U;
	byte gDispatchedImpulse = 0U;
	byte gDispatchedMsec = 0U;
	int gDispatchCount = 0;

	bool check(bool condition, const char *description)
	{
		if (condition)
		{
			return true;
		}

		std::fprintf(stderr, "check failed: %s\n", description);
		return false;
	}

	void runPlayerMove(
		edict_t *entity,
		const float *viewAngles,
		float forward,
		float side,
		float up,
		unsigned short buttons,
		byte impulse,
		byte msec)
	{
		gDispatchedEntity = entity;
		gDispatchedAngles[0] = viewAngles[0];
		gDispatchedAngles[1] = viewAngles[1];
		gDispatchedAngles[2] = viewAngles[2];
		gDispatchedForward = forward;
		gDispatchedSide = side;
		gDispatchedUp = up;
		gDispatchedButtons = buttons;
		gDispatchedImpulse = impulse;
		gDispatchedMsec = msec;
		++gDispatchCount;
	}

	astrabot::runtime::ActorId joinActor(
		astrabot::runtime::LifecycleSession &lifecycle,
		astrabot::runtime::ActorRegistry &registry,
		std::uint32_t slot)
	{
		if (!lifecycle.connectSlot(slot))
		{
			return {};
		}
		const auto token = lifecycle.tokenForSlot(slot);
		astrabot::runtime::ActorId actor{};
		if (registry.reserve(slot, token, &actor) != astrabot::runtime::ActorResult::Accepted ||
				registry.beginJoin(actor) != astrabot::runtime::ActorResult::Accepted ||
				registry.markJoined(actor) != astrabot::runtime::ActorResult::Accepted)
		{
			return {};
		}
		return actor;
	}

	astrabot::runtime::BotCommand commandFor(
		const astrabot::runtime::ActorId &actor,
		const astrabot::runtime::LifecycleToken &token,
		std::uint32_t sequence)
	{
		astrabot::runtime::BotCommand command{};
		command.actor = actor;
		command.lifecycle = token;
		command.sequence = sequence;
		command.issueFrame = 10U;
		command.viewAngles = {1.0f, 2.0f, 3.0f};
		command.movement = {100.0f, 20.0f, 0.0f, 4U, 5U, 10U};
		return command;
	}
}

int main()
{
	using astrabot::metamod::InputDispatcher;
	using astrabot::runtime::ActorRegistry;
	using astrabot::runtime::DispatchResult;
	using astrabot::runtime::LifecycleSession;

	LifecycleSession lifecycle;
	ActorRegistry registry;
	if (!check(lifecycle.activateMap(), "map activation succeeds"))
	{
		return 1;
	}
	const auto firstActor = joinActor(lifecycle, registry, 1U);
	if (!check(firstActor.actorGeneration != 0U, "first actor is joined"))
	{
		return 1;
	}
	edict_t firstEntity{};
	enginefuncs_t engineFunctions{};
	engineFunctions.pfnRunPlayerMove = &runPlayerMove;
	InputDispatcher dispatcher(lifecycle, registry);
	dispatcher.configure(&engineFunctions);
	if (!check(dispatcher.bindActor(firstActor, &firstEntity), "first actor is bound"))
	{
		return 1;
	}
	const auto firstToken = lifecycle.tokenForSlot(1U);
	if (!check(dispatcher.enqueue(commandFor(firstActor, firstToken, 1U)) ==
			astrabot::runtime::QueueResult::Accepted, "first input is queued"))
	{
		return 1;
	}
	const auto firstReceipt = dispatcher.dispatchNext(firstActor, 20U);
	if (!check(firstReceipt.result == DispatchResult::Dispatched,
			"first input is dispatched"))
	{
		return 1;
	}
	if (!check(gDispatchCount == 1 && gDispatchedEntity == &firstEntity,
			"engine receives first actor entity once"))
	{
		return 1;
	}
	if (!check(gDispatchedAngles[0] == 1.0f && gDispatchedAngles[1] == 2.0f &&
			gDispatchedAngles[2] == 3.0f && gDispatchedForward == 100.0f &&
			gDispatchedSide == 20.0f && gDispatchedButtons == 4U &&
			gDispatchedImpulse == 5U && gDispatchedMsec == 10U,
			"engine receives converted command values"))
	{
		return 1;
	}

	const auto queuedToken = lifecycle.tokenForSlot(1U);
	if (!check(dispatcher.enqueue(commandFor(firstActor, queuedToken, 2U)) ==
			astrabot::runtime::QueueResult::Accepted, "stale candidate is queued"))
	{
		return 1;
	}
	if (!check(lifecycle.beginRound(), "round transition succeeds"))
	{
		return 1;
	}
	const auto staleReceipt = dispatcher.dispatchNext(firstActor, 21U);
	if (!check(staleReceipt.result == DispatchResult::StaleActor,
			"stale queued input is rejected"))
	{
		return 1;
	}
	if (!check(gDispatchCount == 1, "stale input does not call engine"))
	{
		return 1;
	}
	const auto secondActor = joinActor(lifecycle, registry, 2U);
	edict_t secondEntity{};
	if (!check(dispatcher.bindActor(secondActor, &secondEntity), "second actor is bound"))
	{
		return 1;
	}
	const auto secondToken = lifecycle.tokenForSlot(2U);
	if (!check(dispatcher.enqueue(commandFor(secondActor, secondToken, 1U)) ==
			astrabot::runtime::QueueResult::Accepted, "current input is queued"))
	{
		return 1;
	}
	dispatcher.configure(nullptr);
	const auto unavailableReceipt = dispatcher.dispatchNext(secondActor, 22U);
	if (!check(unavailableReceipt.result == DispatchResult::EngineUnavailable,
			"missing engine is reported"))
	{
		return 1;
	}
	if (!check(gDispatchCount == 1, "missing engine does not call engine"))
	{
		return 1;
	}

	dispatcher.configure(&engineFunctions);
	if (!check(dispatcher.enqueue(commandFor(secondActor, secondToken, 2U)) ==
			astrabot::runtime::QueueResult::Accepted, "second input is queued"))
	{
		return 1;
	}
	if (!check(dispatcher.dispatchNext(firstActor, 23U).result == DispatchResult::NoCommand,
			"first actor cannot consume second input"))
	{
		return 1;
	}
	if (!check(dispatcher.dispatchNext(secondActor, 23U).result == DispatchResult::Dispatched,
			"second actor consumes its own input"))
	{
		return 1;
	}
	return 0;
}
