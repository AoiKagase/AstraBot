#ifndef ASTRABOT_RUNTIME_LIFECYCLE_HPP
#define ASTRABOT_RUNTIME_LIFECYCLE_HPP

#include <array>
#include <cstdint>

namespace astrabot
{
namespace runtime
{
	using LifecycleGeneration = std::uint32_t;

	struct LifecycleToken
	{
		LifecycleGeneration mapGeneration;
		LifecycleGeneration roundGeneration;
		LifecycleGeneration slotGeneration;
		std::uint32_t slot;
	};

	class LifecycleSession
	{
	public:
		using Generation = LifecycleGeneration;

		static constexpr std::uint32_t kFirstClientSlot = 1U;
		static constexpr std::uint32_t kLastClientSlot = 32U;
		static constexpr std::size_t kClientSlotCount =
			kLastClientSlot - kFirstClientSlot + 1U;
		static constexpr Generation kInvalidGeneration = 0U;

		LifecycleSession();

		bool activateMap();
		bool beginRound();
		bool observeFrame(std::uint32_t frameCount, float timeSeconds);
		void deactivateMap();
		bool connectSlot(std::uint32_t slot);
		bool disconnectSlot(std::uint32_t slot);

		bool isMapActive() const;
		Generation mapGeneration() const;
		Generation roundGeneration() const;
		Generation slotGeneration(std::uint32_t slot) const;
		LifecycleToken tokenForSlot(std::uint32_t slot) const;
		bool isCurrent(const LifecycleToken &token) const;

	private:
		static bool advanceGeneration(Generation &generation);
		static bool isSlotValid(std::uint32_t slot);
		static std::size_t slotIndex(std::uint32_t slot);

		Generation mapGeneration_;
		Generation roundGeneration_;
		std::array<Generation, kClientSlotCount> slotGenerations_;
		std::array<bool, kClientSlotCount> slotConnected_;
		bool mapActive_;
		bool hasFrameObservation_;
		std::uint32_t lastFrameCount_;
		float lastTimeSeconds_;
	};
}
}

#endif
