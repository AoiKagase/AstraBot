#include "astrabot/runtime/command_queue.hpp"

#include <cstdio>
#include <limits>

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

	astrabot::runtime::LifecycleToken connectSlot(
		astrabot::runtime::LifecycleSession &lifecycle,
		std::uint32_t slot)
	{
		if (lifecycle.connectSlot(slot))
		{
			return lifecycle.tokenForSlot(slot);
		}
		return {};
	}

	astrabot::runtime::ActorId joinActor(
		astrabot::runtime::ActorRegistry &registry,
		std::uint32_t slot,
		const astrabot::runtime::LifecycleToken &token)
	{
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
		command.viewAngles = {0.0f, 90.0f, 0.0f};
		command.movement = {100.0f, 0.0f, 0.0f, 0U, 0U, 10U};
		return command;
	}
}

int main()
{
	using astrabot::runtime::ActorRegistry;
	using astrabot::runtime::BotCommand;
	using astrabot::runtime::CommandQueue;
	using astrabot::runtime::LifecycleSession;
	using astrabot::runtime::QueueResult;

	LifecycleSession lifecycle;
	ActorRegistry registry;
	if (!check(lifecycle.activateMap(), "map activation succeeds"))
	{
		return 1;
	}
	const auto firstToken = connectSlot(lifecycle, 1U);
	const auto secondToken = connectSlot(lifecycle, 2U);
	const auto firstActor = joinActor(registry, 1U, firstToken);
	const auto secondActor = joinActor(registry, 2U, secondToken);
	if (!check(firstActor.actorGeneration != 0U && secondActor.actorGeneration != 0U,
			"two actors are joined"))
	{
		return 1;
	}

	CommandQueue queue(lifecycle, registry);
	const BotCommand firstCommand = commandFor(firstActor, firstToken, 1U);
	BotCommand secondCommand = commandFor(secondActor, secondToken, 1U);
	BotCommand thirdCommand = commandFor(firstActor, firstToken, 2U);
	if (!check(queue.enqueue(firstCommand) == QueueResult::Accepted,
			"first command is accepted"))
	{
		return 1;
	}
	if (!check(queue.enqueue(secondCommand) == QueueResult::Accepted,
			"second actor command is accepted"))
	{
		return 1;
	}
	if (!check(queue.enqueue(thirdCommand) == QueueResult::Accepted,
			"second command for first actor is accepted"))
	{
		return 1;
	}
	BotCommand dequeued{};
	if (!check(queue.dequeueForActor(firstActor, &dequeued) == QueueResult::Accepted &&
			dequeued.sequence == 1U, "first actor receives its first command"))
	{
		return 1;
	}
	if (!check(queue.dequeueForActor(firstActor, &dequeued) == QueueResult::Accepted &&
			dequeued.sequence == 2U, "first actor receives its second command"))
	{
		return 1;
	}
	if (!check(queue.dequeueForActor(secondActor, &dequeued) == QueueResult::Accepted &&
			dequeued.actor.slot == 2U, "second actor receives its own command"))
	{
		return 1;
	}
	if (!check(queue.dequeueForActor(firstActor, &dequeued) == QueueResult::Empty,
			"empty queue is reported"))
	{
		return 1;
	}

	const BotCommand sequenceBaseline = commandFor(firstActor, firstToken, 3U);
	if (!check(queue.enqueue(sequenceBaseline) == QueueResult::Accepted,
			"sequence baseline is accepted"))
	{
		return 1;
	}
	if (!check(queue.enqueue(sequenceBaseline) == QueueResult::SequenceRejected,
			"duplicate sequence is rejected"))
	{
		return 1;
	}
	BotCommand invalidCommand = sequenceBaseline;
	invalidCommand.sequence = 4U;
	invalidCommand.viewAngles.pitch = std::numeric_limits<float>::quiet_NaN();
	if (!check(queue.enqueue(invalidCommand) == QueueResult::InvalidCommand,
			"non-finite command is rejected"))
	{
		return 1;
	}
	if (!check(queue.dequeueForActor(firstActor, &dequeued) == QueueResult::Accepted,
			"valid command remains after invalid command"))
	{
		return 1;
	}

	if (!check(lifecycle.beginRound(), "round transition succeeds"))
	{
		return 1;
	}
	if (!check(queue.enqueue(thirdCommand) == QueueResult::StaleActor,
			"stale lifecycle command is rejected"))
	{
		return 1;
	}
	queue.clearActor(firstActor);
	if (!check(queue.size() == 0U, "actor clear removes pending commands"))
	{
		return 1;
	}
	return 0;
}
