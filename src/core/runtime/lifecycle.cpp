#include "astrabot/runtime/lifecycle.hpp"

#include <cmath>
#include <limits>

namespace astrabot
{
namespace runtime
{
	LifecycleSession::LifecycleSession()
		: mapGeneration_(kInvalidGeneration),
		  roundGeneration_(kInvalidGeneration),
		  slotGenerations_(),
		  slotConnected_(),
		  mapActive_(false),
		  hasFrameObservation_(false),
		  lastFrameCount_(0U),
		  lastTimeSeconds_(0.0f)
	{
	}

	bool LifecycleSession::activateMap()
	{
		if (mapActive_)
		{
			return false;
		}

		Generation nextMapGeneration = mapGeneration_;
		Generation nextRoundGeneration = roundGeneration_;
		if (!advanceGeneration(nextMapGeneration) ||
				!advanceGeneration(nextRoundGeneration))
		{
			return false;
		}

		mapGeneration_ = nextMapGeneration;
		roundGeneration_ = nextRoundGeneration;
		mapActive_ = true;
		hasFrameObservation_ = false;
		for (std::size_t index = 0U; index < kClientSlotCount; ++index)
		{
			slotConnected_[index] = false;
		}
		return true;
	}

	bool LifecycleSession::beginRound()
	{
		if (!mapActive_)
		{
			return false;
		}

		Generation nextRoundGeneration = roundGeneration_;
		if (!advanceGeneration(nextRoundGeneration))
		{
			return false;
		}

		roundGeneration_ = nextRoundGeneration;
		hasFrameObservation_ = false;
		return true;
	}

	bool LifecycleSession::observeFrame(std::uint32_t frameCount, float timeSeconds)
	{
		if (!mapActive_ || !std::isfinite(timeSeconds))
		{
			return false;
		}

		const bool frameReset = hasFrameObservation_ &&
			(frameCount < lastFrameCount_ || timeSeconds < lastTimeSeconds_);
		if (frameReset && !beginRound())
		{
			return false;
		}

		lastFrameCount_ = frameCount;
		lastTimeSeconds_ = timeSeconds;
		hasFrameObservation_ = true;
		return true;
	}

	void LifecycleSession::deactivateMap()
	{
		mapActive_ = false;
		hasFrameObservation_ = false;
		for (std::size_t index = 0U; index < kClientSlotCount; ++index)
		{
			slotConnected_[index] = false;
		}
	}

	bool LifecycleSession::connectSlot(std::uint32_t slot)
	{
		if (!mapActive_ || !isSlotValid(slot))
		{
			return false;
		}

		const std::size_t index = slotIndex(slot);
		if (slotConnected_[index])
		{
			return false;
		}
		if (!advanceGeneration(slotGenerations_[index]))
		{
			return false;
		}

		slotConnected_[index] = true;
		return true;
	}

	bool LifecycleSession::disconnectSlot(std::uint32_t slot)
	{
		if (!mapActive_ || !isSlotValid(slot))
		{
			return false;
		}

		const std::size_t index = slotIndex(slot);
		if (!slotConnected_[index])
		{
			return false;
		}
		if (!advanceGeneration(slotGenerations_[index]))
		{
			return false;
		}

		slotConnected_[index] = false;
		return true;
	}

	bool LifecycleSession::isMapActive() const
	{
		return mapActive_;
	}

	LifecycleSession::Generation LifecycleSession::mapGeneration() const
	{
		return mapGeneration_;
	}

	LifecycleSession::Generation LifecycleSession::roundGeneration() const
	{
		return roundGeneration_;
	}

	LifecycleSession::Generation LifecycleSession::slotGeneration(std::uint32_t slot) const
	{
		if (!isSlotValid(slot))
		{
			return kInvalidGeneration;
		}

		return slotGenerations_[slotIndex(slot)];
	}

	LifecycleToken LifecycleSession::tokenForSlot(std::uint32_t slot) const
	{
		LifecycleToken token = {
			kInvalidGeneration,
			kInvalidGeneration,
			kInvalidGeneration,
			slot
		};
		if (!mapActive_ || !isSlotValid(slot) || !slotConnected_[slotIndex(slot)])
		{
			return token;
		}

		token.mapGeneration = mapGeneration_;
		token.roundGeneration = roundGeneration_;
		token.slotGeneration = slotGenerations_[slotIndex(slot)];
		return token;
	}

	bool LifecycleSession::isCurrent(const LifecycleToken &token) const
	{
		if (!mapActive_ || !isSlotValid(token.slot))
		{
			return false;
		}

		const std::size_t index = slotIndex(token.slot);
		return slotConnected_[index] &&
			token.mapGeneration == mapGeneration_ &&
			token.roundGeneration == roundGeneration_ &&
			token.slotGeneration == slotGenerations_[index] &&
			token.mapGeneration != kInvalidGeneration &&
			token.roundGeneration != kInvalidGeneration &&
			token.slotGeneration != kInvalidGeneration;
	}

	bool LifecycleSession::advanceGeneration(Generation &generation)
	{
		if (generation == std::numeric_limits<Generation>::max())
		{
			return false;
		}

		++generation;
		return generation != kInvalidGeneration;
	}

	bool LifecycleSession::isSlotValid(std::uint32_t slot)
	{
		return slot >= kFirstClientSlot && slot <= kLastClientSlot;
	}

	std::size_t LifecycleSession::slotIndex(std::uint32_t slot)
	{
		return static_cast<std::size_t>(slot - kFirstClientSlot);
	}
}
}
