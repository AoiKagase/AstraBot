#ifndef ASTRABOT_ADAPTER_METAMOD_ACTION_ADAPTER_HPP
#define ASTRABOT_ADAPTER_METAMOD_ACTION_ADAPTER_HPP

#include "astrabot/runtime/bot_command.hpp"

#include <in_buttons.h>

#include <cstdint>

namespace astrabot
{
namespace metamod
{
enum class ActionKind : std::uint8_t
{
	None,
	Fire,
	Reload,
	Plant,
	Defuse
};

struct ActionProposal
{
	ActionKind kind;
	runtime::ViewAngles viewAngles;
	std::uint16_t movementButtons;
};

struct ActionDispatch
{
	ActionKind kind;
	runtime::ViewAngles viewAngles;
	std::uint16_t buttons;
	const char *clientCommand;
	bool stopMovement;
};

struct MovementProjection
{
	float forward;
	float side;
};

class ActionAdapter
{
  public:
	static constexpr std::uint16_t kAttackButton = static_cast<std::uint16_t>(IN_ATTACK);
	static constexpr std::uint16_t kUseButton = static_cast<std::uint16_t>(IN_USE);
	static constexpr std::uint16_t kForwardButton = static_cast<std::uint16_t>(IN_FORWARD);
	static constexpr std::uint16_t kBackButton = static_cast<std::uint16_t>(IN_BACK);
	static constexpr std::uint16_t kMoveLeftButton = static_cast<std::uint16_t>(IN_MOVELEFT);
	static constexpr std::uint16_t kMoveRightButton = static_cast<std::uint16_t>(IN_MOVERIGHT);
	static constexpr const char *kReloadCommand = "reload";
	static constexpr const char *kSelectC4Command = "use weapon_c4";

	static ActionDispatch translate(const ActionProposal &proposal);
	static ActionProposal forLiveDispatch(
		const ActionProposal &proposal,
		bool activeWeaponAvailable,
		const runtime::ViewAngles &movementViewAngles);
	static MovementProjection projectMovement(
		float worldDirectionX,
		float worldDirectionY,
		float speed,
		float viewYaw);
	static std::uint16_t movementButtons(float forward, float side);
};
} // namespace metamod
} // namespace astrabot

#endif
