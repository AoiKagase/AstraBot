#include "astrabot/metamod/fake_client_manager.hpp"

#include <cstddef>
#include <cstdio>

namespace astrabot
{
namespace metamod
{
	namespace
	{
		constexpr std::size_t kFirstClientSlot = 1U;
		constexpr std::size_t kLastClientSlot = 32U;
		constexpr std::size_t kKickCommandCapacity = 64U;
		constexpr std::size_t kUserIdDigitsCapacity = 16U;

		bool formatKickCommand(char *buffer, std::size_t capacity, int userId)
		{
			if (buffer == nullptr || capacity == 0U || userId < 0)
			{
				return false;
			}

			const char prefix[] = "kick #";
			std::size_t index = 0U;
			for (const char character : prefix)
			{
				if (character == '\0')
				{
					break;
				}
				if (index + 1U >= capacity)
				{
					return false;
				}
				buffer[index++] = character;
			}

			char digits[kUserIdDigitsCapacity]{};
			std::size_t digitCount = 0U;
			unsigned int remaining = static_cast<unsigned int>(userId);
			do
			{
				if (digitCount >= kUserIdDigitsCapacity)
				{
					return false;
				}
				digits[digitCount++] = static_cast<char>('0' + remaining % 10U);
				remaining /= 10U;
			} while (remaining != 0U);

			while (digitCount > 0U)
			{
				if (index + 1U >= capacity)
				{
					return false;
				}
				buffer[index++] = digits[--digitCount];
			}
			if (index + 2U > capacity)
			{
				return false;
			}
			buffer[index++] = '\n';
			buffer[index] = '\0';
			return true;
		}
	}

	FakeClientManager::FakeClientManager(
		runtime::LifecycleSession &lifecycle,
		runtime::ActorRegistry &registry)
		: lifecycle_(lifecycle),
		  registry_(registry),
		  engineFunctions_(nullptr),
		  gameDllFunctions_(nullptr),
		  managedBotCreationAllowed_(false)
	{
	}

	void FakeClientManager::configure(
		enginefuncs_t *engineFunctions,
		gamedll_funcs_t *gameDllFunctions,
		bool managedBotCreationAllowed)
	{
		engineFunctions_ = engineFunctions;
		gameDllFunctions_ = gameDllFunctions;
		managedBotCreationAllowed_ = managedBotCreationAllowed;
	}

	FakeClientResult FakeClientManager::create(const char *name, FakeClientHandle *handle)
	{
		if (name == nullptr || name[0] == '\0')
		{
			return FakeClientResult::InvalidName;
		}
		if (handle == nullptr)
		{
			return FakeClientResult::BoundaryUnavailable;
		}
		if (!managedBotCreationAllowed_)
		{
			return FakeClientResult::NativeGuardDenied;
		}
		if (!lifecycle_.isMapActive() || engineFunctions_ == nullptr ||
				gameDllFunctions_ == nullptr || gameDllFunctions_->dllapi_table == nullptr ||
				engineFunctions_->pfnCreateFakeClient == nullptr ||
				engineFunctions_->pfnIndexOfEdict == nullptr ||
				gameDllFunctions_->dllapi_table->pfnClientPutInServer == nullptr)
		{
			return FakeClientResult::BoundaryUnavailable;
		}

		edict_t *entity = engineFunctions_->pfnCreateFakeClient(name);
		if (entity == nullptr)
		{
			return FakeClientResult::CreateFailed;
		}

		const int slot = engineFunctions_->pfnIndexOfEdict(entity);
		if (slot < static_cast<int>(kFirstClientSlot) ||
				slot > static_cast<int>(kLastClientSlot))
		{
			cleanupFailedClient(entity, 0U, false);
			return FakeClientResult::InvalidSlot;
		}

		const std::uint32_t clientSlot = static_cast<std::uint32_t>(slot);
		bool connectedHere = false;
		if (lifecycle_.tokenForSlot(clientSlot).slotGeneration ==
				runtime::LifecycleSession::kInvalidGeneration)
		{
			if (!lifecycle_.connectSlot(clientSlot))
			{
				cleanupFailedClient(entity, clientSlot, false);
				return FakeClientResult::SlotBusy;
			}
			connectedHere = true;
		}

		const runtime::LifecycleToken lifecycleToken = lifecycle_.tokenForSlot(clientSlot);
		runtime::ActorId actor{};
		runtime::ActorResult actorResult = registry_.reserve(clientSlot, lifecycleToken, &actor);
		if (actorResult != runtime::ActorResult::Accepted)
		{
			cleanupFailedClient(entity, clientSlot, connectedHere);
			return mapRegistryResult(actorResult);
		}
		actorResult = registry_.beginJoin(actor);
		if (actorResult != runtime::ActorResult::Accepted)
		{
			registry_.release(actor);
			cleanupFailedClient(entity, clientSlot, connectedHere);
			return mapRegistryResult(actorResult);
		}

		gameDllFunctions_->dllapi_table->pfnClientPutInServer(entity);
		actorResult = registry_.markJoined(actor);
		if (actorResult != runtime::ActorResult::Accepted)
		{
			registry_.release(actor);
			cleanupFailedClient(entity, clientSlot, connectedHere);
			return FakeClientResult::JoinFailed;
		}
		handle->actor = actor;
		handle->entity = entity;
		return FakeClientResult::Created;
	}

	FakeClientResult FakeClientManager::remove(FakeClientHandle *handle)
	{
		if (handle == nullptr || handle->entity == nullptr)
		{
			return FakeClientResult::NotFound;
		}
		if (!lifecycle_.isMapActive() || engineFunctions_ == nullptr ||
				gameDllFunctions_ == nullptr || gameDllFunctions_->dllapi_table == nullptr ||
				engineFunctions_->pfnGetPlayerUserId == nullptr ||
				engineFunctions_->pfnServerCommand == nullptr ||
				engineFunctions_->pfnServerExecute == nullptr ||
				gameDllFunctions_->dllapi_table->pfnClientDisconnect == nullptr)
		{
			return FakeClientResult::BoundaryUnavailable;
		}

		const int userId = engineFunctions_->pfnGetPlayerUserId(handle->entity);
		if (userId < 0)
		{
			return FakeClientResult::InvalidEntity;
		}
		char command[kKickCommandCapacity]{};
		if (!formatKickCommand(command, sizeof(command), userId))
		{
			return FakeClientResult::CleanupFailed;
		}
		const runtime::ActorResult actorResult = registry_.beginRemoval(handle->actor);
		if (actorResult != runtime::ActorResult::Accepted)
		{
			return mapRegistryResult(actorResult);
		}

		gameDllFunctions_->dllapi_table->pfnClientDisconnect(handle->entity);
		engineFunctions_->pfnServerCommand(command);
		engineFunctions_->pfnServerExecute();
		lifecycle_.disconnectSlot(handle->actor.slot);
		if (registry_.release(handle->actor) != runtime::ActorResult::Accepted)
		{
			return FakeClientResult::CleanupFailed;
		}
		handle->actor = {0U, 0U};
		handle->entity = nullptr;
		return FakeClientResult::Removed;
	}

	void FakeClientManager::cleanupFailedClient(
		edict_t *entity,
		std::uint32_t slot,
		bool disconnectLifecycle)
	{
		if (entity != nullptr && gameDllFunctions_ != nullptr &&
				gameDllFunctions_->dllapi_table != nullptr &&
				gameDllFunctions_->dllapi_table->pfnClientDisconnect != nullptr)
		{
			gameDllFunctions_->dllapi_table->pfnClientDisconnect(entity);
		}
		if (disconnectLifecycle && slot >= kFirstClientSlot && slot <= kLastClientSlot)
		{
			lifecycle_.disconnectSlot(slot);
		}
	}

	FakeClientResult FakeClientManager::mapRegistryResult(runtime::ActorResult result) const
	{
		switch (result)
		{
		case runtime::ActorResult::Accepted:
			return FakeClientResult::Created;
		case runtime::ActorResult::InvalidSlot:
			return FakeClientResult::InvalidSlot;
		case runtime::ActorResult::SlotBusy:
			return FakeClientResult::SlotBusy;
		case runtime::ActorResult::NotFound:
			return FakeClientResult::NotFound;
		case runtime::ActorResult::InvalidState:
			return FakeClientResult::InvalidState;
		default:
			return FakeClientResult::JoinFailed;
		}
	}
}
}
