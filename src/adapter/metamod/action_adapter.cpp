#include "action_adapter.hpp"

#include <cmath>

namespace astrabot
{
namespace metamod
{
ActionDispatch ActionAdapter::translate(const ActionProposal &proposal)
{
	ActionDispatch result = {
		proposal.kind, proposal.viewAngles, proposal.movementButtons, nullptr, false};
	switch (proposal.kind)
	{
	case ActionKind::Fire:
	case ActionKind::Plant:
		result.buttons = static_cast<std::uint16_t>(result.buttons | kAttackButton);
		result.clientCommand = kSelectC4Command;
		result.stopMovement = proposal.kind == ActionKind::Plant;
		break;
	case ActionKind::Defuse:
		result.buttons = static_cast<std::uint16_t>(result.buttons | kUseButton);
		result.stopMovement = true;
		break;
	case ActionKind::Reload:
		result.clientCommand = kReloadCommand;
		break;
	case ActionKind::None:
	default:
		break;
	}
	return result;
}

ActionProposal ActionAdapter::forLiveDispatch(
	const ActionProposal &proposal,
	bool activeWeaponAvailable,
	const runtime::ViewAngles &movementViewAngles)
{
	if (proposal.kind == ActionKind::Fire && !activeWeaponAvailable)
	{
		return {ActionKind::None, movementViewAngles, proposal.movementButtons};
	}
	return proposal;
}

MovementProjection ActionAdapter::projectMovement(
	float worldDirectionX,
	float worldDirectionY,
	float speed,
	float viewYaw)
{
	if (!std::isfinite(worldDirectionX) || !std::isfinite(worldDirectionY) ||
			!std::isfinite(speed) || !std::isfinite(viewYaw))
	{
		return {0.0f, 0.0f};
	}
	constexpr float kDegreesToRadians = 0.01745329251994329577f;
	const float yaw = viewYaw * kDegreesToRadians;
	const float forwardX = std::cos(yaw);
	const float forwardY = std::sin(yaw);
	const float rightX = -forwardY;
	const float rightY = forwardX;
	return {
		(worldDirectionX * forwardX + worldDirectionY * forwardY) * speed,
		-(worldDirectionX * rightX + worldDirectionY * rightY) * speed};
}

std::uint16_t ActionAdapter::movementButtons(float forward, float side)
{
	constexpr float kInputEpsilon = 0.01f;
	std::uint16_t buttons = 0U;
	if (std::isfinite(forward) && forward > kInputEpsilon)
	{
		buttons = static_cast<std::uint16_t>(buttons | kForwardButton);
	}
	else if (std::isfinite(forward) && forward < -kInputEpsilon)
	{
		buttons = static_cast<std::uint16_t>(buttons | kBackButton);
	}
	if (std::isfinite(side) && side > kInputEpsilon)
	{
		buttons = static_cast<std::uint16_t>(buttons | kMoveRightButton);
	}
	else if (std::isfinite(side) && side < -kInputEpsilon)
	{
		buttons = static_cast<std::uint16_t>(buttons | kMoveLeftButton);
	}
	return buttons;
}
} // namespace metamod
} // namespace astrabot
