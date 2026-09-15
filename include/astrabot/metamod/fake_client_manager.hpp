#ifndef ASTRABOT_METAMOD_FAKE_CLIENT_MANAGER_HPP
#define ASTRABOT_METAMOD_FAKE_CLIENT_MANAGER_HPP

#include "astrabot/metamod/abi_contract.hpp"
#include "astrabot/runtime/actor_registry.hpp"

#include <cstdint>

namespace astrabot
{
namespace metamod
{
	enum class FakeClientResult
	{
		Created,
		Removed,
		Killed,
		NativeGuardDenied,
		BoundaryUnavailable,
		InvalidName,
		CreateFailed,
		InvalidEntity,
		InvalidSlot,
		SlotBusy,
		JoinFailed,
		NotFound,
		InvalidState,
		CleanupFailed
	};

	struct FakeClientHandle
	{
		runtime::ActorId actor;
		edict_t *entity;
	};

	class FakeClientManager
	{
	public:
		FakeClientManager(runtime::LifecycleSession &lifecycle, runtime::ActorRegistry &registry);

		void configure(
			enginefuncs_t *engineFunctions,
			gamedll_funcs_t *gameDllFunctions,
			bool managedBotCreationAllowed);
		FakeClientResult create(const char *name, FakeClientHandle *handle);
		FakeClientResult remove(FakeClientHandle *handle);
		FakeClientResult kill(FakeClientHandle *handle);

	private:
		void cleanupFailedClient(
			edict_t *entity,
			std::uint32_t slot,
			bool disconnectLifecycle);
		FakeClientResult mapRegistryResult(runtime::ActorResult result) const;

		runtime::LifecycleSession &lifecycle_;
		runtime::ActorRegistry &registry_;
		enginefuncs_t *engineFunctions_;
		gamedll_funcs_t *gameDllFunctions_;
		bool managedBotCreationAllowed_;
	};
}
}

#endif
