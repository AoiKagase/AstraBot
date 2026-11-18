#include "bridge/action_adapter.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

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

bool testFireAndObjectiveButtons()
{
	using astrabot::metamod::ActionAdapter;
	using astrabot::metamod::ActionKind;
	using astrabot::metamod::ActionProposal;
	using astrabot::runtime::ViewAngles;
	using astrabot::runtime::ViewAngles;

	const ViewAngles aim = {12.0f, 90.0f, 0.0f};
	const ActionProposal fire = {ActionKind::Fire, aim, 2U};
	const auto fireDispatch = ActionAdapter::translate(fire);
	if (!check(fireDispatch.buttons == (2U | ActionAdapter::kAttackButton) && fireDispatch.viewAngles.yaw == aim.yaw,
			"fire preserves movement buttons and adds IN_ATTACK"))
	{
		return false;
	}
	if (!check(fireDispatch.clientCommand == nullptr, "generic Fire does not issue a C4 selection command"))
	{
		return false;
	}

	const ActionProposal defuse = {ActionKind::Defuse, aim, 4U};
	const auto defuseDispatch = ActionAdapter::translate(defuse);
	if (!check(
			defuseDispatch.buttons == (4U | ActionAdapter::kUseButton) && defuseDispatch.viewAngles.pitch == aim.pitch,
			"defuse preserves movement buttons and adds IN_USE"))
	{
		return false;
	}

	const ActionProposal plant = {ActionKind::Plant, aim, 8U};
	const auto plantDispatch = ActionAdapter::translate(plant);
	return check(
		plantDispatch.buttons == ActionAdapter::kAttackButton, "plant uses the primary attack input at the bombsite");
}

bool testReloadAndNoOp()
{
	using astrabot::metamod::ActionAdapter;
	using astrabot::metamod::ActionKind;
	using astrabot::metamod::ActionProposal;
	const auto knife = ActionAdapter::translate({ActionKind::SelectKnife, {}, 8U});
	if (!check(knife.clientCommand != nullptr && std::strcmp(knife.clientCommand, "weapon_knife") == 0 &&
				knife.buttons == 8U && !knife.stopMovement,
			"bomb escape selects knife through the public command while preserving movement"))
		return false;

	const ActionProposal reload = {ActionKind::Reload, {}, 0U};
	const auto reloadDispatch = ActionAdapter::translate(reload);
	if (!check(reloadDispatch.clientCommand == nullptr &&
				reloadDispatch.buttons == IN_RELOAD && !reloadDispatch.stopMovement,
			"reload uses the native player-move button and allows movement"))
	{
		return false;
	}

	const ActionProposal none = {ActionKind::None, {}, 3U};
	const auto noOp = ActionAdapter::translate(none);
	return check(noOp.buttons == 3U && noOp.clientCommand == nullptr, "no action does not add input or command");
}

bool testLiveFireRequiresWeaponBoundary()
{
	using astrabot::metamod::ActionAdapter;
	using astrabot::metamod::ActionKind;
	using astrabot::metamod::ActionProposal;
	using astrabot::runtime::ViewAngles;

	const ActionProposal fire = {ActionKind::Fire, {}, 0U};
	const ViewAngles movementAngles = {0.0f, 15.0f, 0.0f};
	const auto suppressed = ActionAdapter::forLiveDispatch(fire, false, movementAngles);
	if (!check(suppressed.kind == ActionKind::None && suppressed.movementButtons == 0U &&
				suppressed.viewAngles.yaw == movementAngles.yaw,
			"suppressed Fire restores the locomotion view"))
	{
		return false;
	}
	const auto ready = ActionAdapter::forLiveDispatch(fire, true, movementAngles);
	return check(
		ready.kind == ActionKind::Fire, "live Fire remains available when an active weapon boundary is present");
}

bool testActionStopsMovementForObjectiveUse()
{
	using astrabot::metamod::ActionAdapter;
	using astrabot::metamod::ActionKind;
	using astrabot::metamod::ActionProposal;

	const ActionProposal plant = {ActionKind::Plant, {0.0f, 90.0f, 0.0f}, 8U};
	const ActionProposal defuse = {ActionKind::Defuse, {0.0f, -90.0f, 0.0f}, 4U};
	const auto plantDispatch = ActionAdapter::translate(plant);
	const auto defuseDispatch = ActionAdapter::translate(defuse);
	return check(plantDispatch.stopMovement && defuseDispatch.stopMovement && plantDispatch.clientCommand != nullptr &&
				   std::strcmp(plantDispatch.clientCommand, ActionAdapter::kSelectC4Command) == 0,
			   "plant and defuse hold the bot in place") &&
		check(!ActionAdapter::translate({ActionKind::Fire, {}, 0U}).stopMovement, "Fire preserves locomotion");
}

bool testWorldMovementProjectionIgnoresAimTarget()
{
	using astrabot::metamod::ActionAdapter;

	const auto fixed = ActionAdapter::projectMovement(0.8f, 0.6f, 180.0f, 0.0f, true);
	if (!check(fixed.forward == 180.0f && fixed.side == -180.0f,
			"Compatibility movement uses reference directional inputs, not continuous projection"))
		return false;
	const auto forward = ActionAdapter::projectMovement(1.0f, 0.0f, 100.0f, 90.0f);
	const auto left = ActionAdapter::projectMovement(0.0f, 1.0f, 100.0f, 0.0f);
	return check(std::fabs(forward.forward) < 0.01f && std::fabs(forward.side - 100.0f) < 0.01f,
			   "world east remains east when aiming north") &&
		check(std::fabs(left.forward) < 0.01f && std::fabs(left.side + 100.0f) < 0.01f,
			"world north projects to left strafe when aiming east");
}

bool testMovementButtonsFollowProjectedInput()
{
	using astrabot::metamod::ActionAdapter;

	const auto forward = ActionAdapter::movementButtons(100.0f, 0.0f);
	const auto back = ActionAdapter::movementButtons(-100.0f, 0.0f);
	const auto left = ActionAdapter::movementButtons(0.0f, -100.0f);
	const auto right = ActionAdapter::movementButtons(0.0f, 100.0f);
	return check((forward & ActionAdapter::kForwardButton) != 0U && (forward & ActionAdapter::kBackButton) == 0U,
			   "forward analog input carries IN_FORWARD") &&
		check((back & ActionAdapter::kBackButton) != 0U && (back & ActionAdapter::kForwardButton) == 0U,
			"backward analog input carries IN_BACK") &&
		check((left & ActionAdapter::kMoveLeftButton) != 0U && (right & ActionAdapter::kMoveRightButton) != 0U,
			"side analog input carries the matching strafe button");
}
} // namespace

bool testPlantContinuationReselectsC4()
{
	using astrabot::metamod::ActionAdapter;
	using astrabot::metamod::ActionKind;
	using astrabot::metamod::ActionProposal;
	const ActionProposal continuation = {ActionKind::PlantContinue, {0.0f, 45.0f, 0.0f}, ActionAdapter::kForwardButton};
	const auto dispatch = ActionAdapter::translate(continuation);
	return check(dispatch.kind == ActionKind::PlantContinue && dispatch.stopMovement &&
			(dispatch.buttons & ActionAdapter::kAttackButton) != 0U && dispatch.clientCommand != nullptr &&
			std::strcmp(dispatch.clientCommand, ActionAdapter::kSelectC4Command) == 0,
		"plant continuation reselects C4 while holding attack");
}

bool testPlantAttackWaitsForObservedC4Selection()
{
	using astrabot::metamod::ActionAdapter;
	using astrabot::metamod::ActionKind;

	const ActionKind noWeaponObservation = ActionAdapter::plantActionForWeaponObservation(false, false, 0U, false);
	const auto selectDispatch = ActionAdapter::translate({noWeaponObservation, {}, 0U});
	if (!check(noWeaponObservation == ActionKind::SelectC4 &&
				(selectDispatch.buttons & ActionAdapter::kAttackButton) == 0U &&
				selectDispatch.clientCommand != nullptr && std::strcmp(selectDispatch.clientCommand, "weapon_c4") == 0,
			"unknown active weapon requests ZBot SelectItem without attacking"))
	{
		return false;
	}
	if (!check(ActionAdapter::plantActionForWeaponObservation(true, true, ActionAdapter::kC4WeaponId, false) ==
				ActionKind::Plant,
			"primary attack starts only after CurWeapon reports active C4"))
	{
		return false;
	}
	const auto plantDispatch = ActionAdapter::translate({ActionKind::Plant, {}, 0U});
	if (!check((plantDispatch.buttons & ActionAdapter::kAttackButton) != 0U && plantDispatch.clientCommand != nullptr &&
				std::strcmp(plantDispatch.clientCommand, "weapon_c4") == 0,
			"active C4 is attacked and reselected like ZBot"))
	{
		return false;
	}
	return check(ActionAdapter::plantActionForWeaponObservation(true, true, ActionAdapter::kC4WeaponId, true) ==
				ActionKind::PlantContinue &&
			ActionAdapter::plantActionForWeaponObservation(true, true, 7U, false) == ActionKind::SelectC4,
		"continued attack requires current C4 observation");
}

bool testPlantObjectiveAcceptsAnySelectedBombTargetClass()
{
	using astrabot::metamod::ActionAdapter;
	using astrabot::metamod::PlantTargetPoint;
	const PlantTargetPoint infoTargetCenter = {100.0f, -50.0f, 32.0f};
	const PlantTargetPoint atLegacyRadius = {356.0f, -50.0f, 32.0f};
	const PlantTargetPoint outsideLegacyRadius = {356.1f, -50.0f, 32.0f};
	return check(ActionAdapter::isBombTargetClassname("func_bomb_target") &&
				   ActionAdapter::isBombTargetClassname("INFO_BOMB_TARGET") &&
				   !ActionAdapter::isBombTargetClassname("info_target"),
			   "plant target classifier accepts both public BombTarget classnames") &&
		check(ActionAdapter::isLegacyInfoBombTargetClassname("INFO_BOMB_TARGET") &&
				!ActionAdapter::isLegacyInfoBombTargetClassname("func_bomb_target"),
			"legacy info radius classifier is case-insensitive") &&
		check(ActionAdapter::withinLegacyBombTargetRadius(infoTargetCenter, atLegacyRadius) &&
				!ActionAdapter::withinLegacyBombTargetRadius(infoTargetCenter, outsideLegacyRadius),
			"info_bomb_target plant region follows the public 256-unit boundary");
}

bool testPlantZoneBounds()
{
	using astrabot::metamod::ActionAdapter;
	using astrabot::metamod::PlantTargetBounds;
	const PlantTargetBounds site = {-100.0f, -100.0f, 0.0f, 100.0f, 100.0f, 64.0f};
	const PlantTargetBounds inside = {-4.0f, -4.0f, 32.0f, 4.0f, 4.0f, 68.0f};
	const PlantTargetBounds outside = {101.0f, -4.0f, 32.0f, 110.0f, 4.0f, 68.0f};
	PlantTargetBounds invalid = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
	invalid.maximumX = std::numeric_limits<float>::quiet_NaN();
	return check(ActionAdapter::overlapsPlantTarget(site, inside),
			   "standing player hull overlaps selected bomb-target bounds") &&
		check(!ActionAdapter::overlapsPlantTarget(site, outside),
			"plant attempt is blocked outside the selected bomb-target bounds") &&
		check(!ActionAdapter::overlapsPlantTarget(site, invalid), "invalid site bounds fail closed");
}

int main()
{
	const auto rotated = astrabot::metamod::ActionAdapter::reprojectMovement(100, 0, 0, 90);
	if (!check(std::fabs(rotated.forward) < .001f && std::fabs(rotated.side - 100) < .001f,
			"upkeep aim change preserves world movement direction"))
		return 1;
	for (const auto kind : {astrabot::metamod::ActionKind::SelectC4, astrabot::metamod::ActionKind::Plant,
			 astrabot::metamod::ActionKind::PlantContinue})
	{
		const auto dispatch = astrabot::metamod::ActionAdapter::translate(
			{kind, {}, static_cast<std::uint16_t>(IN_JUMP | IN_FORWARD | IN_MOVELEFT | IN_DUCK)});
		if (!check((dispatch.buttons & (IN_JUMP | IN_FORWARD | IN_MOVELEFT)) == 0U &&
					(dispatch.buttons & IN_DUCK) != 0U && dispatch.stopMovement,
				"C4 selection and planting cancel traversal input while preserving crouch"))
			return 1;
	}
	return testFireAndObjectiveButtons() && testReloadAndNoOp() && testLiveFireRequiresWeaponBoundary() &&
			testActionStopsMovementForObjectiveUse() && testPlantContinuationReselectsC4() &&
			testPlantAttackWaitsForObservedC4Selection() && testPlantObjectiveAcceptsAnySelectedBombTargetClass() &&
			testPlantZoneBounds() && testWorldMovementProjectionIgnoresAimTarget() &&
			testMovementButtonsFollowProjectedInput()
		? 0
		: 1;
}
