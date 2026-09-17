#include "astrabot/runtime/actor_registry.hpp"

#include <limits>

namespace astrabot
{
namespace runtime
{
	ActorRegistry::ActorRegistry()
		: actorGenerations_(),
		  records_()
	{
		for (std::size_t index = 0U; index < kCapacity; ++index)
		{
			records_[index] = {
				{0U, 0U},
				{0U, 0U, 0U, 0U},
				ActorState::Vacant
			};
		}
	}

	ActorResult ActorRegistry::reserve(
		std::uint32_t slot,
		const LifecycleToken &lifecycleToken,
		ActorId *actor)
	{
		if (!isSlotValid(slot))
		{
			return ActorResult::InvalidSlot;
		}
		if (!isTokenValid(slot, lifecycleToken))
		{
			return ActorResult::InvalidToken;
		}

		Record &record = records_[slotIndex(slot)];
		if (record.state != ActorState::Vacant)
		{
			return ActorResult::SlotBusy;
		}
		if (actor == nullptr)
		{
			return ActorResult::InvalidOutput;
		}
		if (actorGenerations_[slotIndex(slot)] ==
				(std::numeric_limits<LifecycleGeneration>::max)())
		{
			return ActorResult::GenerationExhausted;
		}

		++actorGenerations_[slotIndex(slot)];
		record.actor = {slot, actorGenerations_[slotIndex(slot)]};
		record.lifecycleToken = lifecycleToken;
		record.state = ActorState::Reserved;
		*actor = record.actor;
		return ActorResult::Accepted;
	}

	ActorResult ActorRegistry::beginJoin(const ActorId &actor)
	{
		Record *record = nullptr;
		const ActorResult result = findRecord(actor, &record);
		if (result != ActorResult::Accepted)
		{
			return result;
		}
		if (record->state != ActorState::Reserved)
		{
			return ActorResult::InvalidState;
		}

		record->state = ActorState::Joining;
		return ActorResult::Accepted;
	}

	ActorResult ActorRegistry::markJoined(const ActorId &actor)
	{
		Record *record = nullptr;
		const ActorResult result = findRecord(actor, &record);
		if (result != ActorResult::Accepted)
		{
			return result;
		}
		if (record->state != ActorState::Joining)
		{
			return ActorResult::InvalidState;
		}

		record->state = ActorState::Joined;
		return ActorResult::Accepted;
	}

	ActorResult ActorRegistry::beginRemoval(const ActorId &actor)
	{
		Record *record = nullptr;
		const ActorResult result = findRecord(actor, &record);
		if (result != ActorResult::Accepted)
		{
			return result;
		}
		if (record->state != ActorState::Joining && record->state != ActorState::Joined)
		{
			return ActorResult::InvalidState;
		}

		record->state = ActorState::Removing;
		return ActorResult::Accepted;
	}

	ActorResult ActorRegistry::release(const ActorId &actor)
	{
		Record *record = nullptr;
		const ActorResult result = findRecord(actor, &record);
		if (result != ActorResult::Accepted)
		{
			return result;
		}
		if (record->state != ActorState::Reserved &&
				record->state != ActorState::Joining &&
				record->state != ActorState::Removing)
		{
			return ActorResult::InvalidState;
		}

		record->state = ActorState::Vacant;
		return ActorResult::Accepted;
	}

	bool ActorRegistry::isCurrent(
		const ActorId &actor,
		const LifecycleToken &lifecycleToken) const
	{
		const Record *record = nullptr;
		if (findRecord(actor, &record) != ActorResult::Accepted)
		{
			return false;
		}

		return record->state == ActorState::Joined &&
			sameActor(record->actor, actor) &&
			record->lifecycleToken.mapGeneration == lifecycleToken.mapGeneration &&
			record->lifecycleToken.roundGeneration == lifecycleToken.roundGeneration &&
			record->lifecycleToken.slotGeneration == lifecycleToken.slotGeneration &&
			record->lifecycleToken.slot == lifecycleToken.slot &&
			isTokenValid(actor.slot, lifecycleToken);
	}

	ActorState ActorRegistry::state(const ActorId &actor) const
	{
		const Record *record = nullptr;
		if (findRecord(actor, &record) != ActorResult::Accepted)
		{
			return ActorState::Vacant;
		}
		return record->state;
	}

	ActorId ActorRegistry::actorForSlot(std::uint32_t slot) const
	{
		if (!isSlotValid(slot))
		{
			return {0U, 0U};
		}

		const Record &record = records_[slotIndex(slot)];
		if (record.state == ActorState::Vacant)
		{
			return {0U, 0U};
		}
		return record.actor;
	}

	bool ActorRegistry::isSlotValid(std::uint32_t slot)
	{
		return slot >= LifecycleSession::kFirstClientSlot &&
			slot <= LifecycleSession::kLastClientSlot;
	}

	std::size_t ActorRegistry::slotIndex(std::uint32_t slot)
	{
		return static_cast<std::size_t>(slot - LifecycleSession::kFirstClientSlot);
	}

	bool ActorRegistry::isTokenValid(std::uint32_t slot, const LifecycleToken &token)
	{
		return token.slot == slot &&
			token.mapGeneration != LifecycleSession::kInvalidGeneration &&
			token.roundGeneration != LifecycleSession::kInvalidGeneration &&
			token.slotGeneration != LifecycleSession::kInvalidGeneration;
	}

	bool ActorRegistry::sameActor(const ActorId &left, const ActorId &right)
	{
		return left.slot == right.slot && left.actorGeneration == right.actorGeneration;
	}

	ActorResult ActorRegistry::findRecord(const ActorId &actor, Record **record)
	{
		if (record == nullptr || !isSlotValid(actor.slot))
		{
			return !isSlotValid(actor.slot) ? ActorResult::InvalidSlot : ActorResult::InvalidOutput;
		}

		Record &candidate = records_[slotIndex(actor.slot)];
		if (candidate.state == ActorState::Vacant || !sameActor(candidate.actor, actor))
		{
			return ActorResult::NotFound;
		}
		*record = &candidate;
		return ActorResult::Accepted;
	}

	ActorResult ActorRegistry::findRecord(const ActorId &actor, const Record **record) const
	{
		if (record == nullptr || !isSlotValid(actor.slot))
		{
			return !isSlotValid(actor.slot) ? ActorResult::InvalidSlot : ActorResult::InvalidOutput;
		}

		const Record &candidate = records_[slotIndex(actor.slot)];
		if (candidate.state == ActorState::Vacant || !sameActor(candidate.actor, actor))
		{
			return ActorResult::NotFound;
		}
		*record = &candidate;
		return ActorResult::Accepted;
	}
}
}
