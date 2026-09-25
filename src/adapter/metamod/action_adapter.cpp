#include "action_adapter.hpp"

#include <cctype>
#include <cmath>

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
	ActionDispatch result = {
		proposal.kind, proposal.viewAngles, proposal.movementButtons, nullptr, false};
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

bool ActionAdapter::validPlantTargetBounds(const PlantTargetBounds &bounds) noexcept
{
	const float values[] = {bounds.minimumX, bounds.minimumY, bounds.minimumZ,
		bounds.maximumX, bounds.maximumY, bounds.maximumZ};
	for (const float value : values)
	{
		if (!std::isfinite(value))
		{
			return false;
		}
	}
	return bounds.minimumX <= bounds.maximumX &&
		bounds.minimumY <= bounds.maximumY &&
		bounds.minimumZ <= bounds.maximumZ;
}

bool ActionAdapter::overlapsPlantTarget(
	const PlantTargetBounds &site,
	const PlantTargetBounds &actor) noexcept
{
	if (!validPlantTargetBounds(site) || !validPlantTargetBounds(actor))
	{
		return false;
	}
	return actor.maximumX >= site.minimumX && actor.minimumX <= site.maximumX &&
		actor.maximumY >= site.minimumY && actor.minimumY <= site.maximumY &&
		actor.maximumZ >= site.minimumZ && actor.minimumZ <= site.maximumZ;
}

bool ActionAdapter::plantAttemptExpired(float elapsedSeconds) noexcept
{
	return std::isfinite(elapsedSeconds) &&
		elapsedSeconds >= kPlantAttemptTimeoutSeconds;
}

bool ActionAdapter::isBombTargetClassname(const char *classname) noexcept
{
	return equalsIgnoreCase(classname, "func_bomb_target") ||
		isLegacyInfoBombTargetClassname(classname);
}

bool ActionAdapter::isLegacyInfoBombTargetClassname(const char *classname) noexcept
{
	return equalsIgnoreCase(classname, "info_bomb_target");
}

bool ActionAdapter::canBeginPlantObjective(
	bool hasPlantAssignment,
	bool carryingBomb,
	bool isTerrorist,
	bool selectedTargetAvailable) noexcept
{
	return hasPlantAssignment && carryingBomb && isTerrorist &&
		selectedTargetAvailable;
}

bool ActionAdapter::withinLegacyBombTargetRadius(
	const PlantTargetPoint &center,
	const PlantTargetPoint &actor) noexcept
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

PlantAttemptResult ActionAdapter::evaluatePlantAttempt(
	PlantAttemptState &state,
	bool actorOverlapsSite,
	bool plantedConfirmed,
	float now) noexcept
{
	if (plantedConfirmed)
	{
		state.active = false;
		state.startedAt = 0.0f;
		state.retryAfter = 0.0f;
		return PlantAttemptResult::Confirmed;
	}
	if (!std::isfinite(now))
	{
		return PlantAttemptResult::InvalidTime;
	}
	if (!actorOverlapsSite)
	{
		state.active = false;
		state.startedAt = 0.0f;
		return PlantAttemptResult::OutsideSite;
	}
	if (state.active)
	{
		const float elapsed = now - state.startedAt;
		if (!std::isfinite(elapsed) || elapsed < 0.0f)
		{
			state.active = false;
			state.startedAt = 0.0f;
		}
		else if (plantAttemptExpired(elapsed))
		{
			state.active = false;
			state.startedAt = 0.0f;
			state.retryAfter = now + kPlantAttemptRetrySeconds;
			return PlantAttemptResult::TimedOut;
		}
		else
		{
			return PlantAttemptResult::Continuing;
		}
	}
	if (!std::isfinite(state.retryAfter))
	{
		state.retryAfter = now;
	}
	return now < state.retryAfter
		? PlantAttemptResult::RetryPending
		: PlantAttemptResult::Ready;
}

void ActionAdapter::recordPlantAttemptDispatched(
	PlantAttemptState &state,
	float now) noexcept
{
	if (state.active || !std::isfinite(now) || now < state.retryAfter)
	{
		return;
	}
	state.active = true;
	state.startedAt = now;
}

ActionKind ActionAdapter::plantActionForWeaponObservation(
	bool observationAvailable,
	bool active,
	std::uint8_t weaponId,
	bool attackAlreadyDispatched) noexcept
{
	if (!observationAvailable || !active || weaponId != kC4WeaponId)
	{
		return ActionKind::SelectC4;
	}
	return attackAlreadyDispatched ? ActionKind::PlantContinue : ActionKind::Plant;
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
