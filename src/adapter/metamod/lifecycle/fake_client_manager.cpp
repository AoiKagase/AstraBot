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
		} // namespace

		FakeClientManager::FakeClientManager(runtime::LifecycleSession &lifecycle,
											 runtime::ActorRegistry &registry)
			: lifecycle_(lifecycle), registry_(registry), engineFunctions_(nullptr),
			  gameDllFunctions_(nullptr), metaUtils_(nullptr), pluginId_(nullptr),
			  managedBotCreationAllowed_(false)
		{
		}

		void FakeClientManager::configure(enginefuncs_t *engineFunctions,
										  gamedll_funcs_t *gameDllFunctions,
										  bool managedBotCreationAllowed, mutil_funcs_t *metaUtils,
										  plid_t pluginId)
		{
			engineFunctions_ = engineFunctions;
			gameDllFunctions_ = gameDllFunctions;
			metaUtils_ = metaUtils;
			pluginId_ = pluginId;
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
				engineFunctions_->pfnGetInfoKeyBuffer == nullptr ||
				engineFunctions_->pfnSetClientKeyValue == nullptr ||
				gameDllFunctions_->dllapi_table->pfnClientConnect == nullptr ||
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
		const string_t netname = entity->v.netname;
		entity->v = entvars_t{};
		entity->v.pContainingEntity = entity;
		entity->v.netname = netname;
		entity->v.flags = FL_CLIENT | FL_FAKECLIENT;
		if (entity->pvPrivateData != nullptr)
		{
			if (engineFunctions_->pfnFreeEntPrivateData == nullptr)
			{
				cleanupFailedClient(entity, clientSlot, false);
				return FakeClientResult::BoundaryUnavailable;
			}
			engineFunctions_->pfnFreeEntPrivateData(entity);
			entity->pvPrivateData = nullptr;
		}
		entity->v.frags = 0.0f;
		if (metaUtils_ != nullptr && metaUtils_->pfnCallGameEntity != nullptr &&
				!metaUtils_->pfnCallGameEntity(pluginId_, "player", &entity->v))
			{
				cleanupFailedClient(entity, clientSlot, false);
				return FakeClientResult::JoinFailed;
			}
			if (engineFunctions_->pfnGetInfoKeyBuffer != nullptr &&
				engineFunctions_->pfnSetClientKeyValue != nullptr)
			{
				char *infoBuffer = engineFunctions_->pfnGetInfoKeyBuffer(entity);
				if (infoBuffer == nullptr)
				{
					cleanupFailedClient(entity, clientSlot, false);
					return FakeClientResult::InvalidEntity;
				}
				char vguiMenusKey[] = "_vgui_menus";
				char vguiMenusValue[] = "0";
				char autoHelpKey[] = "_ah";
				char autoHelpValue[] = "0";
				char botKey[] = "*bot";
				char botValue[] = "1";
				char modelKey[] = "model";
				char modelValue[] = "";
				char rateKey[] = "rate";
				char rateValue[] = "3500.000000";
				char updateRateKey[] = "cl_updaterate";
				char updateRateValue[] = "20";
				char trackerKey[] = "tracker";
				char trackerValue[] = "0";
				char downloadMaxKey[] = "cl_dlmax";
				char downloadMaxValue[] = "128";
				char leftHandKey[] = "lefthand";
				char leftHandValue[] = "1";
				char friendsKey[] = "friends";
				char friendsValue[] = "0";
				char dmKey[] = "dm";
				char dmValue[] = "0";
				char autoHelpClientKey[] = "ah";
				char autoHelpClientValue[] = "1";
				engineFunctions_->pfnSetClientKeyValue(slot, infoBuffer, modelKey, modelValue);
				engineFunctions_->pfnSetClientKeyValue(slot, infoBuffer, rateKey, rateValue);
				engineFunctions_->pfnSetClientKeyValue(
						slot, infoBuffer, updateRateKey, updateRateValue);
				engineFunctions_->pfnSetClientKeyValue(slot, infoBuffer, trackerKey, trackerValue);
				engineFunctions_->pfnSetClientKeyValue(
						slot, infoBuffer, downloadMaxKey, downloadMaxValue);
				engineFunctions_->pfnSetClientKeyValue(
						slot, infoBuffer, leftHandKey, leftHandValue);
				engineFunctions_->pfnSetClientKeyValue(slot, infoBuffer, friendsKey, friendsValue);
				engineFunctions_->pfnSetClientKeyValue(slot, infoBuffer, dmKey, dmValue);
				engineFunctions_->pfnSetClientKeyValue(
						slot, infoBuffer, autoHelpClientKey, autoHelpClientValue);
				engineFunctions_->pfnSetClientKeyValue(slot, infoBuffer, vguiMenusKey,
															   vguiMenusValue);
				engineFunctions_->pfnSetClientKeyValue(slot, infoBuffer, autoHelpKey, autoHelpValue);
				engineFunctions_->pfnSetClientKeyValue(slot, infoBuffer, botKey, botValue);
			}
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
			runtime::ActorResult actorResult =
				registry_.reserve(clientSlot, lifecycleToken, &actor);
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

		char rejectReason[128] = {};
		if (gameDllFunctions_->dllapi_table->pfnClientConnect(
				entity, name, "127.0.0.1", rejectReason) == 0)
		{
			registry_.release(actor);
			cleanupFailedClient(entity, clientSlot, connectedHere);
			return FakeClientResult::JoinFailed;
		}

			gameDllFunctions_->dllapi_table->pfnClientPutInServer(entity);
			entity->v.flags |= (FL_CLIENT | FL_FAKECLIENT);
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

		const runtime::ActorId actor = handle->actor;
		edict_t *const entity = handle->entity;
		const int userId = engineFunctions_->pfnGetPlayerUserId(entity);
			if (userId < 0)
			{
				return FakeClientResult::InvalidEntity;
			}
			char command[kKickCommandCapacity]{};
			if (!formatKickCommand(command, sizeof(command), userId))
			{
				return FakeClientResult::CleanupFailed;
			}
		const runtime::ActorResult actorResult = registry_.beginRemoval(actor);
			if (actorResult != runtime::ActorResult::Accepted)
			{
				return mapRegistryResult(actorResult);
			}

		gameDllFunctions_->dllapi_table->pfnClientDisconnect(entity);
		engineFunctions_->pfnServerCommand(command);
		engineFunctions_->pfnServerExecute();
		lifecycle_.disconnectSlot(actor.slot);
		if (registry_.release(actor) != runtime::ActorResult::Accepted)
			{
				return FakeClientResult::CleanupFailed;
			}
			handle->actor = {0U, 0U};
			handle->entity = nullptr;
			return FakeClientResult::Removed;
		}

		FakeClientResult FakeClientManager::kill(FakeClientHandle *handle)
		{
			if (handle == nullptr || handle->entity == nullptr)
			{
				return FakeClientResult::NotFound;
			}
			if (!lifecycle_.isMapActive() || gameDllFunctions_ == nullptr ||
				gameDllFunctions_->dllapi_table == nullptr ||
				gameDllFunctions_->dllapi_table->pfnClientKill == nullptr)
			{
				return FakeClientResult::BoundaryUnavailable;
			}
			const runtime::ActorState actorState = registry_.state(handle->actor);
			if (actorState != runtime::ActorState::Joining &&
					actorState != runtime::ActorState::Joined)
			{
				return FakeClientResult::NotFound;
			}
			gameDllFunctions_->dllapi_table->pfnClientKill(handle->entity);
			return FakeClientResult::Killed;
		}

		void FakeClientManager::cleanupFailedClient(edict_t *entity, std::uint32_t slot,
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
	} // namespace metamod
} // namespace astrabot
