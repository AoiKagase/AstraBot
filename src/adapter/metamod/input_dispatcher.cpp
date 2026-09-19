#include "astrabot/metamod/input_dispatcher.hpp"

namespace astrabot
{
namespace metamod
{
	InputDispatcher::InputDispatcher(
		runtime::LifecycleSession &lifecycle,
		runtime::ActorRegistry &registry)
		: lifecycle_(lifecycle),
		  registry_(registry),
		  queue_(lifecycle, registry),
		  engineFunctions_(nullptr),
		  boundActors_(),
		  boundEntities_()
	{
	}

	void InputDispatcher::configure(enginefuncs_t *engineFunctions)
	{
		engineFunctions_ = engineFunctions;
	}

	void InputDispatcher::reset()
	{
		queue_.clear();
		boundActors_.fill({0U, 0U});
		boundEntities_.fill(nullptr);
		engineFunctions_ = nullptr;
	}

	runtime::QueueResult InputDispatcher::enqueue(const runtime::BotCommand &command)
	{
		return queue_.enqueue(command);
	}

	bool InputDispatcher::bindActor(const runtime::ActorId &actor, edict_t *entity)
	{
		if (entity == nullptr || !isSlotValid(actor.slot))
		{
			return false;
		}

		const runtime::LifecycleToken token = lifecycle_.tokenForSlot(actor.slot);
		const runtime::ActorState actorState = registry_.state(actor);
		if (!lifecycle_.isCurrent(token) ||
				(actorState != runtime::ActorState::Joining &&
				 actorState != runtime::ActorState::Joined))
		{
			return false;
		}

		const std::size_t index = slotIndex(actor.slot);
		if (boundEntities_[index] != nullptr && !sameActor(boundActors_[index], actor))
		{
			return false;
		}
		boundActors_[index] = actor;
		boundEntities_[index] = entity;
		return true;
	}

	void InputDispatcher::unbindActor(const runtime::ActorId &actor)
	{
		if (!isSlotValid(actor.slot))
		{
			return;
		}

		const std::size_t index = slotIndex(actor.slot);
		if (boundEntities_[index] != nullptr && sameActor(boundActors_[index], actor))
		{
			boundActors_[index] = {0U, 0U};
			boundEntities_[index] = nullptr;
			queue_.clearActor(actor);
		}
	}

	runtime::CommandReceipt InputDispatcher::dispatchNext(
		const runtime::ActorId &actor,
		std::uint32_t dispatchFrame)
	{
		runtime::CommandReceipt receipt = {
			actor,
			0U,
			dispatchFrame,
			runtime::DispatchResult::NoCommand
		};
		runtime::BotCommand command{};
		const runtime::QueueResult queueResult = queue_.dequeueForActor(actor, &command);
		if (queueResult == runtime::QueueResult::Empty ||
				queueResult == runtime::QueueResult::NoCommand)
		{
			return receipt;
		}
		if (queueResult != runtime::QueueResult::Accepted)
		{
			receipt.result = runtime::DispatchResult::InvalidCommand;
			return receipt;
		}

		receipt.sequence = command.sequence;
		const runtime::LifecycleToken currentToken = lifecycle_.tokenForSlot(actor.slot);
		if (!lifecycle_.isCurrent(currentToken) ||
				!registry_.isCurrent(actor, currentToken) ||
				command.lifecycle.mapGeneration != currentToken.mapGeneration ||
				command.lifecycle.roundGeneration != currentToken.roundGeneration ||
				command.lifecycle.slotGeneration != currentToken.slotGeneration)
		{
			receipt.result = runtime::DispatchResult::StaleActor;
			return receipt;
		}
		if (!command.isValid())
		{
			receipt.result = runtime::DispatchResult::InvalidCommand;
			return receipt;
		}
		if (!isSlotValid(actor.slot) || engineFunctions_ == nullptr ||
				engineFunctions_->pfnRunPlayerMove == nullptr)
		{
			receipt.result = runtime::DispatchResult::EngineUnavailable;
			return receipt;
		}

		const std::size_t index = slotIndex(actor.slot);
		if (boundEntities_[index] == nullptr || !sameActor(boundActors_[index], actor))
		{
			receipt.result = runtime::DispatchResult::StaleActor;
			return receipt;
		}

		const float viewAngles[3] = {
			command.viewAngles.pitch,
			command.viewAngles.yaw,
			command.viewAngles.roll
		};
		boundEntities_[index]->v.button = command.movement.buttons;
		boundEntities_[index]->v.impulse = command.movement.impulse;
		engineFunctions_->pfnRunPlayerMove(
				boundEntities_[index],
				viewAngles,
				command.movement.forward,
				command.movement.side,
				command.movement.up,
				command.movement.buttons,
				command.movement.impulse,
				command.movement.msec);
		receipt.result = runtime::DispatchResult::Dispatched;
		return receipt;
	}

	bool InputDispatcher::isSlotValid(std::uint32_t slot)
	{
		return slot >= runtime::LifecycleSession::kFirstClientSlot &&
			slot <= runtime::LifecycleSession::kLastClientSlot;
	}

	std::size_t InputDispatcher::slotIndex(std::uint32_t slot)
	{
		return static_cast<std::size_t>(slot - runtime::LifecycleSession::kFirstClientSlot);
	}

	bool InputDispatcher::sameActor(
		const runtime::ActorId &left,
		const runtime::ActorId &right)
	{
		return left.slot == right.slot && left.actorGeneration == right.actorGeneration;
	}
}
}
