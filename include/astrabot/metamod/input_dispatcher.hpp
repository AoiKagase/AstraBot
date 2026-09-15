#ifndef ASTRABOT_METAMOD_INPUT_DISPATCHER_HPP
#define ASTRABOT_METAMOD_INPUT_DISPATCHER_HPP

#include "astrabot/metamod/abi_contract.hpp"
#include "astrabot/runtime/command_queue.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace astrabot
{
namespace metamod
{
	class InputDispatcher
	{
	public:
		InputDispatcher(runtime::LifecycleSession &lifecycle, runtime::ActorRegistry &registry);

		void configure(enginefuncs_t *engineFunctions);
		void reset();
		runtime::QueueResult enqueue(const runtime::BotCommand &command);
		bool bindActor(const runtime::ActorId &actor, edict_t *entity);
		void unbindActor(const runtime::ActorId &actor);
		runtime::CommandReceipt dispatchNext(
			const runtime::ActorId &actor,
			std::uint32_t dispatchFrame);

	private:
		static bool isSlotValid(std::uint32_t slot);
		static std::size_t slotIndex(std::uint32_t slot);
		static bool sameActor(const runtime::ActorId &left, const runtime::ActorId &right);

		runtime::LifecycleSession &lifecycle_;
		runtime::ActorRegistry &registry_;
		runtime::CommandQueue queue_;
		enginefuncs_t *engineFunctions_;
		std::array<runtime::ActorId, runtime::LifecycleSession::kClientSlotCount> boundActors_;
		std::array<edict_t *, runtime::LifecycleSession::kClientSlotCount> boundEntities_;
	};
}
}

#endif
