#include "astrabot/runtime/command_queue.hpp"

namespace astrabot
{
namespace runtime
{
	CommandQueue::CommandQueue(LifecycleSession &lifecycle, ActorRegistry &registry)
		: lifecycle_(lifecycle),
		  registry_(registry),
		  commands_(),
		  commandCount_(0U),
		  sequenceActors_(),
		  lastSequences_(),
		  sequenceInitialized_()
	{
	}

	QueueResult CommandQueue::enqueue(const BotCommand &command)
	{
		if (!command.isValid())
		{
			return QueueResult::InvalidCommand;
		}
		if (!lifecycle_.isCurrent(command.lifecycle) ||
				!registry_.isCurrent(command.actor, command.lifecycle))
		{
			return QueueResult::StaleActor;
		}
		if (commandCount_ >= kCapacity)
		{
			return QueueResult::Full;
		}
		if (!isSequenceAccepted(command.actor, command.sequence))
		{
			return QueueResult::SequenceRejected;
		}

		commands_[commandCount_] = command;
		++commandCount_;
		recordSequence(command.actor, command.sequence);
		return QueueResult::Accepted;
	}

	QueueResult CommandQueue::dequeueForActor(const ActorId &actor, BotCommand *command)
	{
		if (command == nullptr)
		{
			return QueueResult::InvalidOutput;
		}
		if (commandCount_ == 0U)
		{
			return QueueResult::Empty;
		}

		std::size_t foundIndex = commandCount_;
		for (std::size_t index = 0U; index < commandCount_; ++index)
		{
			if (sameActor(commands_[index].actor, actor))
			{
				foundIndex = index;
				break;
			}
		}
		if (foundIndex == commandCount_)
		{
			return QueueResult::NoCommand;
		}

		*command = commands_[foundIndex];
		for (std::size_t index = foundIndex; index + 1U < commandCount_; ++index)
		{
			commands_[index] = commands_[index + 1U];
		}
		--commandCount_;
		return QueueResult::Accepted;
	}

	void CommandQueue::clearActor(const ActorId &actor)
	{
		std::size_t keptCount = 0U;
		for (std::size_t index = 0U; index < commandCount_; ++index)
		{
			if (!sameActor(commands_[index].actor, actor))
			{
				commands_[keptCount] = commands_[index];
				++keptCount;
			}
		}
		commandCount_ = keptCount;
	}

	void CommandQueue::clear()
	{
		commandCount_ = 0U;
		sequenceActors_.fill({0U, 0U});
		lastSequences_.fill(0U);
		sequenceInitialized_.fill(false);
	}

	std::size_t CommandQueue::size() const
	{
		return commandCount_;
	}

	bool CommandQueue::sameActor(const ActorId &left, const ActorId &right)
	{
		return left.slot == right.slot && left.actorGeneration == right.actorGeneration;
	}

	bool CommandQueue::isSlotValid(std::uint32_t slot)
	{
		return slot >= LifecycleSession::kFirstClientSlot &&
			slot <= LifecycleSession::kLastClientSlot;
	}

	std::size_t CommandQueue::slotIndex(std::uint32_t slot) const
	{
		return static_cast<std::size_t>(slot - LifecycleSession::kFirstClientSlot);
	}

	bool CommandQueue::isSequenceAccepted(
		const ActorId &actor,
		std::uint32_t sequence) const
	{
		if (!isSlotValid(actor.slot))
		{
			return false;
		}

		const std::size_t index = slotIndex(actor.slot);
		if (!sequenceInitialized_[index] || !sameActor(sequenceActors_[index], actor))
		{
			return true;
		}
		return sequence > lastSequences_[index];
	}

	void CommandQueue::recordSequence(const ActorId &actor, std::uint32_t sequence)
	{
		const std::size_t index = slotIndex(actor.slot);
		sequenceActors_[index] = actor;
		lastSequences_[index] = sequence;
		sequenceInitialized_[index] = true;
	}
}
}
