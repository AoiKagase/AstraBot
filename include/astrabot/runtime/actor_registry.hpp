#ifndef ASTRABOT_RUNTIME_ACTOR_REGISTRY_HPP
#define ASTRABOT_RUNTIME_ACTOR_REGISTRY_HPP

#include "astrabot/runtime/lifecycle.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace astrabot
{
namespace runtime
{
	struct ActorId
	{
		std::uint32_t slot;
		LifecycleGeneration actorGeneration;
	};

	enum class ActorState
	{
		Vacant,
		Reserved,
		Joining,
		Joined,
		Removing
	};

	enum class ActorResult
	{
		Accepted,
		InvalidSlot,
		InvalidToken,
		InvalidOutput,
		SlotBusy,
		NotFound,
		InvalidState,
		StaleToken,
		GenerationExhausted
	};

	class ActorRegistry
	{
	public:
		static constexpr std::size_t kCapacity = LifecycleSession::kClientSlotCount;

		ActorRegistry();

		ActorResult reserve(
			std::uint32_t slot,
			const LifecycleToken &lifecycleToken,
			ActorId *actor);
		ActorResult beginJoin(const ActorId &actor);
		ActorResult markJoined(const ActorId &actor);
		ActorResult beginRemoval(const ActorId &actor);
		ActorResult release(const ActorId &actor);

		bool isCurrent(const ActorId &actor, const LifecycleToken &lifecycleToken) const;
		ActorState state(const ActorId &actor) const;
		ActorId actorForSlot(std::uint32_t slot) const;

	private:
		struct Record
		{
			ActorId actor;
			LifecycleToken lifecycleToken;
			ActorState state;
		};

		static bool isSlotValid(std::uint32_t slot);
		static std::size_t slotIndex(std::uint32_t slot);
		static bool isTokenValid(std::uint32_t slot, const LifecycleToken &token);
		static bool sameActor(const ActorId &left, const ActorId &right);
		ActorResult findRecord(const ActorId &actor, Record **record);
		ActorResult findRecord(const ActorId &actor, const Record **record) const;

		std::array<LifecycleGeneration, kCapacity> actorGenerations_;
		std::array<Record, kCapacity> records_;
	};
}
}

#endif
