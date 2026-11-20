#include "bridge/action_adapter.hpp"

#include <cctype>
#include <cmath>
#include <array>

namespace astrabot
{
namespace metamod
{
namespace
{
bool equalsIgnoreCase(const char *left, const char *right) noexcept
{
	if (left == nullptr || right == nullptr)
	{
		return false;
	}
	while (*left != '\0' && *right != '\0')
	{
		const unsigned char leftCharacter = static_cast<unsigned char>(*left++);
		const unsigned char rightCharacter = static_cast<unsigned char>(*right++);
		if (std::tolower(leftCharacter) != std::tolower(rightCharacter))
		{
			return false;
		}
	}
	return *left == *right;
}
} // namespace

ActionDispatch ActionAdapter::translate(const ActionProposal &proposal)
{
	ActionDispatch result = {proposal.kind, proposal.viewAngles, proposal.movementButtons, nullptr, false};
	switch (proposal.kind)
	{
	case ActionKind::Fire:
		result.buttons = static_cast<std::uint16_t>(result.buttons | kAttackButton);
		break;
	case ActionKind::Plant:
	case ActionKind::PlantContinue:
		result.buttons = static_cast<std::uint16_t>(result.buttons | kAttackButton);
		result.clientCommand = kSelectC4Command;
		result.stopMovement = true;
		break;
	case ActionKind::SelectC4:
		result.clientCommand = kSelectC4Command;
		result.stopMovement = true;
		break;
	case ActionKind::Defuse:
	case ActionKind::UseEntity:
		result.buttons = static_cast<std::uint16_t>(result.buttons | kUseButton);
		result.stopMovement = true;
		break;
	case ActionKind::Reload:
		result.buttons = static_cast<std::uint16_t>(result.buttons | IN_RELOAD);
		break;
	case ActionKind::SelectPistol:
		result.clientCommand = "slot2";
		break;
	case ActionKind::SelectKnife:
		result.clientCommand = "weapon_knife";
		break;
	case ActionKind::SelectPrimary:
		result.clientCommand = "slot1";
		break;
	case ActionKind::None:
	default:
		break;
	}
	if (proposal.secondaryAttack)
	{
		result.buttons = static_cast<std::uint16_t>(result.buttons | IN_ATTACK2);
	}
	result.stopMovement = result.stopMovement || proposal.stopForWeapon;
	if (proposal.kind == ActionKind::SelectC4 || proposal.kind == ActionKind::Plant ||
		proposal.kind == ActionKind::PlantContinue)
	{
		// Zero analog speed alone does not cancel a traversal jump in RunPlayerMove.
		result.buttons = static_cast<std::uint16_t>(
			result.buttons & ~(IN_JUMP | IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT | IN_LEFT | IN_RIGHT));
	}
	return result;
}

ActionProposal ActionAdapter::forLiveDispatch(
	const ActionProposal &proposal, bool activeWeaponAvailable, const runtime::ViewAngles &movementViewAngles)
{
	if ((proposal.kind == ActionKind::Fire || proposal.secondaryAttack) && !activeWeaponAvailable)
	{
		return {ActionKind::None, movementViewAngles, proposal.movementButtons};
	}
	return proposal;
}

bool ActionAdapter::validPlantTargetBounds(const PlantTargetBounds &bounds) noexcept
{
	const float values[] = {
		bounds.minimumX, bounds.minimumY, bounds.minimumZ, bounds.maximumX, bounds.maximumY, bounds.maximumZ};
	for (const float value : values)
	{
		if (!std::isfinite(value))
		{
			return false;
		}
	}
	return bounds.minimumX <= bounds.maximumX && bounds.minimumY <= bounds.maximumY &&
		bounds.minimumZ <= bounds.maximumZ;
}

bool ActionAdapter::overlapsPlantTarget(const PlantTargetBounds &site, const PlantTargetBounds &actor) noexcept
{
	if (!validPlantTargetBounds(site) || !validPlantTargetBounds(actor))
	{
		return false;
	}
	return actor.maximumX >= site.minimumX && actor.minimumX <= site.maximumX && actor.maximumY >= site.minimumY &&
		actor.minimumY <= site.maximumY && actor.maximumZ >= site.minimumZ && actor.minimumZ <= site.maximumZ;
}

bool ActionAdapter::isBombTargetClassname(const char *classname) noexcept
{
	return equalsIgnoreCase(classname, "func_bomb_target") || isLegacyInfoBombTargetClassname(classname);
}

bool ActionAdapter::isLegacyInfoBombTargetClassname(const char *classname) noexcept
{
	return equalsIgnoreCase(classname, "info_bomb_target");
}

bool ActionAdapter::withinLegacyBombTargetRadius(const PlantTargetPoint &center, const PlantTargetPoint &actor) noexcept
{
	const float values[] = {center.x, center.y, center.z, actor.x, actor.y, actor.z};
	for (const float value : values)
	{
		if (!std::isfinite(value))
		{
			return false;
		}
	}
	const float dx = center.x - actor.x;
	const float dy = center.y - actor.y;
	const float dz = center.z - actor.z;
	const float radiusSquared = kLegacyBombTargetRadius * kLegacyBombTargetRadius;
	return dx * dx + dy * dy + dz * dz <= radiusSquared;
}

ActionKind ActionAdapter::plantActionForWeaponObservation(
	bool observationAvailable, bool active, std::uint8_t weaponId, bool attackAlreadyDispatched) noexcept
{
	if (!observationAvailable || !active || weaponId != kC4WeaponId)
	{
		return ActionKind::SelectC4;
	}
	return attackAlreadyDispatched ? ActionKind::PlantContinue : ActionKind::Plant;
}

float ActionAdapter::csBotCosine(float angle)
{
	static constexpr std::size_t tableSize = 256;
	constexpr float fullTurn = 360.0f;
	static const auto table = []
	{
		std::array<float, tableSize> samples{};
		for (std::size_t i = 0; i < samples.size(); ++i)
			samples[i] = std::cos(static_cast<float>(6.283185307179586 * i / (tableSize - 1)));
		return samples;
	}();
	if (!std::isfinite(angle))
		return 0.0f;
	float wrapped = std::fmod(angle, fullTurn);
	if (wrapped < 0.0f)
		wrapped += fullTurn;
	return table[static_cast<std::size_t>(wrapped * ((tableSize - 1) / fullTurn))];
}

runtime::ViewAngles ActionAdapter::useEntityView(float x, float y, float z)
{
	constexpr float degreesPerRadian = 57.29577951308232f;
	const float horizontal = std::hypot(x, y);
	if (horizontal == 0.0f)
		return {z > 0.0f ? -90.0f : 90.0f, 0.0f, 0.0f};
	return {-std::atan2(z, horizontal) * degreesPerRadian, std::atan2(y, x) * degreesPerRadian, 0.0f};
}

bool ActionAdapter::isFacingUseEntity(const runtime::ViewAngles &current, const runtime::ViewAngles &desired)
{
	constexpr float tolerance = 20.0f;
	return std::fabs(std::remainder(desired.yaw - current.yaw, 360.0f)) < tolerance &&
		std::fabs(std::remainder(desired.pitch - current.pitch, 360.0f)) < tolerance;
}

MovementProjection ActionAdapter::projectMovement(
	float worldDirectionX, float worldDirectionY, float speed, float viewYaw, bool compatibility)
{
	if (!std::isfinite(worldDirectionX) || !std::isfinite(worldDirectionY) || !std::isfinite(speed) ||
		!std::isfinite(viewYaw))
	{
		return {0.0f, 0.0f};
	}
	constexpr float kDegreesToRadians = 0.01745329251994329577f;
	const float yaw = viewYaw * kDegreesToRadians;
	const float forwardX = compatibility ? csBotCosine(viewYaw) : std::cos(yaw);
	const float forwardY = compatibility ? csBotCosine(viewYaw - 90.0f) : std::sin(yaw);
	const float rightX = -forwardY;
	const float rightY = forwardX;
	if (compatibility)
	{
		constexpr float kPositiveProjectionThreshold = 0.25f;
		constexpr float kNegativeProjectionThreshold = -0.25f;
		const float forward = worldDirectionX * forwardX + worldDirectionY * forwardY;
		const float side = -(worldDirectionX * rightX + worldDirectionY * rightY);
		return {forward > kPositiveProjectionThreshold	 ? speed
				: forward < kNegativeProjectionThreshold ? -speed
														 : 0.0f,
			side >= kPositiveProjectionThreshold	   ? speed
				: side <= kNegativeProjectionThreshold ? -speed
													   : 0.0f};
	}
	return {(worldDirectionX * forwardX + worldDirectionY * forwardY) * speed,
		-(worldDirectionX * rightX + worldDirectionY * rightY) * speed};
}

MovementProjection ActionAdapter::reprojectMovement(float forward, float side, float oldYaw, float newYaw)
{
// Keep literal evaluation precision on x87; each macro is undefined below.
#define ASTRABOT_ACTION_ADAPTER_DEGREES_TO_RADIANS 0.01745329251994329577f
	const float delta = (newYaw - oldYaw) * ASTRABOT_ACTION_ADAPTER_DEGREES_TO_RADIANS;
	return {forward * std::cos(delta) - side * std::sin(delta), forward * std::sin(delta) + side * std::cos(delta)};
}
#undef ASTRABOT_ACTION_ADAPTER_DEGREES_TO_RADIANS

void ActionAdapter::applyCombatMovement(runtime::BotCommand &command, const ActionProposal &proposal, float maxSpeed)
{
	if (!proposal.combatMovement || !std::isfinite(maxSpeed) || maxSpeed <= 0)
		return;
	command.movement.forward = proposal.stopForWeapon ? 0.0f : proposal.combatForward * maxSpeed;
	command.movement.side = proposal.stopForWeapon ? 0.0f : proposal.combatSide * maxSpeed;
	command.movement.buttons =
		static_cast<std::uint16_t>(command.movement.buttons & ~(IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT));
	command.movement.buttons = static_cast<std::uint16_t>(
		command.movement.buttons | movementButtons(command.movement.forward, command.movement.side));
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
