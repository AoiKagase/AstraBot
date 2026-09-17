#ifndef ASTRABOT_METAMOD_NATIVE_BOT_GUARD_HPP
#define ASTRABOT_METAMOD_NATIVE_BOT_GUARD_HPP

#include <array>
#include <cstddef>

namespace astrabot
{
	namespace metamod
	{
		enum class NativeBotGuardState
		{
			Clean,
			Suppressed,
			Conflict,
			Unsupported
		};

		enum class NativeBotGuardReason
		{
			None,
			ControlsUnavailable,
			InvalidObservation,
			NativeControlsActive,
			UnmanagedFakeClient
		};

		struct NativeBotObservation
		{
			static constexpr std::size_t kClientSlotCount = 32U;

			bool controlsAvailable;
			bool controlsWritable;
			bool clientObservationAvailable;
			bool suppressionApplied;
			float botEnable;
			float botQuota;
			std::array<bool, kClientSlotCount> fakeClientSlots;
			std::array<bool, kClientSlotCount> managedClientSlots;
		};

		struct NativeBotGuardDecision
		{
			NativeBotGuardState state;
			NativeBotGuardReason reason;
			bool managedBotCreationAllowed;
		};

		class NativeBotGuard
		{
		  public:
			NativeBotGuardDecision evaluate(const NativeBotObservation &observation) const;
			bool shouldBlockServerCommand(const char *command) const;

		  private:
			static bool isNativeServerCommand(const char *command);
		};
	} // namespace metamod
} // namespace astrabot

#endif
