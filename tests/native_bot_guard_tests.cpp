#include "astrabot/metamod/native_bot_guard.hpp"

#include <array>
#include <cstdio>

namespace
{
	bool check(bool condition, const char *description)
	{
		if (condition)
		{
			return true;
		}

		std::fprintf(stderr, "check failed: %s\n", description);
		return false;
	}

	astrabot::metamod::NativeBotObservation cleanObservation()
	{
		astrabot::metamod::NativeBotObservation observation{};
		observation.controlsAvailable = true;
		observation.controlsWritable = true;
		observation.clientObservationAvailable = true;
		observation.botEnable = 0.0f;
		observation.botQuota = 0.0f;
		return observation;
	}
} // namespace

int main()
{
	using astrabot::metamod::NativeBotGuard;
	using astrabot::metamod::NativeBotGuardReason;
	using astrabot::metamod::NativeBotGuardState;

	const NativeBotGuard guard;
	const auto clean = guard.evaluate(cleanObservation());
	if (!check(clean.state == NativeBotGuardState::Clean, "clean controls are accepted"))
	{
		return 1;
	}
	if (!check(clean.managedBotCreationAllowed, "clean controls allow managed creation"))
	{
		return 1;
	}
	const auto suppressed = [&guard]() {
		auto observation = cleanObservation();
		observation.suppressionApplied = true;
		return guard.evaluate(observation);
	}();
	if (!check(suppressed.state == NativeBotGuardState::Suppressed,
			   "suppressed controls expose suppressed state"))
	{
		return 1;
	}

	auto missingControls = cleanObservation();
	missingControls.controlsAvailable = false;
	const auto unsupported = guard.evaluate(missingControls);
	if (!check(unsupported.state == NativeBotGuardState::Clean,
			   "missing native controls are clean when clients are observable"))
	{
		return 1;
	}
	if (!check(unsupported.reason == NativeBotGuardReason::None,
			   "missing native controls expose a clean reason"))
	{
		return 1;
	}
	if (!check(unsupported.managedBotCreationAllowed,
			   "missing native controls allow managed creation when clients are observable"))
	{
		return 1;
	}
	auto unobservableClients = cleanObservation();
	unobservableClients.controlsAvailable = false;
	unobservableClients.clientObservationAvailable = false;
	const auto unobservableDecision = guard.evaluate(unobservableClients);
	if (!check(!unobservableDecision.managedBotCreationAllowed,
			   "unobservable client slots fail closed"))
	{
		return 1;
	}

	auto activeControls = cleanObservation();
	activeControls.botEnable = 1.0f;
	const auto active = guard.evaluate(activeControls);
	if (!check(active.state == NativeBotGuardState::Conflict,
			   "enabled native controls are a conflict"))
	{
		return 1;
	}
	if (!check(active.reason == NativeBotGuardReason::NativeControlsActive,
			   "active controls expose a stable reason"))
	{
		return 1;
	}

	auto unmanaged = cleanObservation();
	unmanaged.fakeClientSlots[0U] = true;
	const auto unmanagedDecision = guard.evaluate(unmanaged);
	if (!check(unmanagedDecision.state == NativeBotGuardState::Conflict,
			   "unmanaged fake clients are a conflict"))
	{
		return 1;
	}
	if (!check(unmanagedDecision.reason == NativeBotGuardReason::UnmanagedFakeClient,
			   "unmanaged clients expose a stable reason"))
	{
		return 1;
	}
	if (!check(!unmanagedDecision.managedBotCreationAllowed, "unmanaged clients fail closed"))
	{
		return 1;
	}

	auto owned = cleanObservation();
	owned.fakeClientSlots[0U] = true;
	owned.managedClientSlots[0U] = true;
	const auto ownedDecision = guard.evaluate(owned);
	if (!check(ownedDecision.managedBotCreationAllowed, "owned fake clients remain eligible"))
	{
		return 1;
	}

	if (!check(guard.shouldBlockServerCommand("bot_add"), "bot_add is blocked"))
	{
		return 1;
	}
	if (!check(guard.shouldBlockServerCommand("bot_nav_save"), "native nav command is blocked"))
	{
		return 1;
	}
	if (!check(!guard.shouldBlockServerCommand("say"), "unrelated command is allowed"))
	{
		return 1;
	}
	if (!check(!guard.shouldBlockServerCommand("bot_custom"),
			   "unknown bot command is not blanket blocked"))
	{
		return 1;
	}
	return 0;
}
