#ifndef ASTRABOT_RUNTIME_COMMAND_QUEUE_HPP
#define ASTRABOT_RUNTIME_COMMAND_QUEUE_HPP

#include "astrabot/runtime/bot_command.hpp"

#include <array>
#include <cstddef>

namespace astrabot
{
namespace runtime
{
	enum class QueueResult
	{
		Accepted,
		Empty,
		NoCommand,
		Full,
		InvalidCommand,
		StaleActor,
		SequenceRejected,
		InvalidOutput
	};

	class CommandQueue
	{
	public:
		static constexpr std::size_t kCapacity = 64U;

		CommandQueue(LifecycleSession &lifecycle, ActorRegistry &registry);

		QueueResult enqueue(const BotCommand &command);
		QueueResult dequeueForActor(const ActorId &actor, BotCommand *command);
		void clearActor(const ActorId &actor);
		void clear();
		std::size_t size() const;

	private:
		static bool sameActor(const ActorId &left, const ActorId &right);
		static bool isSlotValid(std::uint32_t slot);
		std::size_t slotIndex(std::uint32_t slot) const;
		bool isSequenceAccepted(const ActorId &actor, std::uint32_t sequence) const;
		void recordSequence(const ActorId &actor, std::uint32_t sequence);

		LifecycleSession &lifecycle_;
		ActorRegistry &registry_;
		std::array<BotCommand, kCapacity> commands_;
		std::size_t commandCount_;
		std::array<ActorId, LifecycleSession::kClientSlotCount> sequenceActors_;
		std::array<std::uint32_t, LifecycleSession::kClientSlotCount> lastSequences_;
		std::array<bool, LifecycleSession::kClientSlotCount> sequenceInitialized_;
	};
}
}

#endif
