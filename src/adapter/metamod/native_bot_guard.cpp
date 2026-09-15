#include "astrabot/metamod/native_bot_guard.hpp"

#include <cmath>
#include <cstring>

namespace astrabot
{
namespace metamod
{
	namespace
	{
		const char *const kNativeServerCommands[] = {
			"bot_about",
			"bot_add",
			"bot_add_t",
			"bot_add_ct",
			"bot_kill",
			"bot_kick",
			"bot_knives_only",
			"bot_pistols_only",
			"bot_snipers_only",
			"bot_all_weapons",
			"entity_dump",
			"bot_nav_delete",
			"bot_nav_split",
			"bot_nav_merge",
			"bot_nav_mark",
			"bot_nav_begin_area",
			"bot_nav_end_area",
			"bot_nav_connect",
			"bot_nav_disconnect",
			"bot_nav_splice",
			"bot_nav_crouch",
			"bot_nav_jump",
			"bot_nav_precise",
			"bot_nav_no_jump",
			"bot_nav_analyze",
			"bot_nav_strip",
			"bot_nav_save",
			"bot_nav_load",
			"bot_nav_use_place",
			"bot_nav_place_floodfill",
			"bot_nav_place_pick",
			"bot_nav_toggle_place_mode",
			"bot_nav_toggle_place_painting",
			"bot_goto_mark",
			"bot_memory_usage",
			"bot_nav_mark_unnamed",
			"bot_nav_warp",
			"bot_nav_corner_select",
			"bot_nav_corner_raise",
			"bot_nav_corner_lower",
			"bot_nav_check_consistency"
		};
		const std::size_t kNativeServerCommandCount =
			sizeof(kNativeServerCommands) / sizeof(kNativeServerCommands[0]);
	}

	NativeBotGuardDecision NativeBotGuard::evaluate(
		const NativeBotObservation &observation) const
	{
		if (!observation.controlsAvailable || !observation.controlsWritable)
		{
			return {
				NativeBotGuardState::Unsupported,
				NativeBotGuardReason::ControlsUnavailable,
				false
			};
		}

		if (!std::isfinite(observation.botEnable) || !std::isfinite(observation.botQuota))
		{
			return {
				NativeBotGuardState::Unsupported,
				NativeBotGuardReason::InvalidObservation,
				false
			};
		}

		if (observation.botEnable != 0.0f || observation.botQuota != 0.0f)
		{
			return {
				NativeBotGuardState::Conflict,
				NativeBotGuardReason::NativeControlsActive,
				false
			};
		}

		for (std::size_t index = 0U; index < NativeBotObservation::kClientSlotCount; ++index)
		{
			if (observation.fakeClientSlots[index] && !observation.managedClientSlots[index])
			{
				return {
					NativeBotGuardState::Conflict,
					NativeBotGuardReason::UnmanagedFakeClient,
					false
				};
			}
		}

		return {
			observation.suppressionApplied ? NativeBotGuardState::Suppressed :
				NativeBotGuardState::Clean,
			NativeBotGuardReason::None,
			true
		};
	}

	bool NativeBotGuard::shouldBlockServerCommand(const char *command) const
	{
		return isNativeServerCommand(command);
	}

	bool NativeBotGuard::isNativeServerCommand(const char *command)
	{
		if (command == nullptr)
		{
			return false;
		}

		for (std::size_t index = 0U; index < kNativeServerCommandCount; ++index)
		{
			if (std::strcmp(command, kNativeServerCommands[index]) == 0)
			{
				return true;
			}
		}
		return false;
	}
}
}
