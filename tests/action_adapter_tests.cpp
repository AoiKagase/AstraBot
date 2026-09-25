#include "action_adapter.hpp"

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
	if (!check(fireDispatch.buttons == (2U | ActionAdapter::kAttackButton) &&
				fireDispatch.viewAngles.yaw == aim.yaw,
			"fire preserves movement buttons and adds IN_ATTACK"))
	{
		return false;
	}
	if (!check(fireDispatch.clientCommand == nullptr,
			"generic Fire does not issue a C4 selection command"))
	{
		return false;
	}

	const ActionProposal defuse = {ActionKind::Defuse, aim, 4U};
	const auto defuseDispatch = ActionAdapter::translate(defuse);
	if (!check(defuseDispatch.buttons == (4U | ActionAdapter::kUseButton) &&
					defuseDispatch.viewAngles.pitch == aim.pitch,
				"defuse preserves movement buttons and adds IN_USE"))
	{
		return false;
	}

	const ActionProposal plant = {ActionKind::Plant, aim, 8U};
	const auto plantDispatch = ActionAdapter::translate(plant);
	return check(plantDispatch.buttons == (8U | ActionAdapter::kAttackButton),
				"plant uses the primary attack input at the bombsite");
}

bool testReloadAndNoOp()
{
	using astrabot::metamod::ActionAdapter;
	using astrabot::metamod::ActionKind;
	using astrabot::metamod::ActionProposal;

	const ActionProposal reload = {ActionKind::Reload, {}, 0U};
	const auto reloadDispatch = ActionAdapter::translate(reload);
	if (!check(reloadDispatch.clientCommand == ActionAdapter::kReloadCommand &&
				reloadDispatch.buttons == 0U,
			"reload uses the public reload command without synthetic button success"))
	{
		return false;
	}

	const ActionProposal none = {ActionKind::None, {}, 3U};
	const auto noOp = ActionAdapter::translate(none);
	return check(noOp.buttons == 3U && noOp.clientCommand == nullptr,
				"no action does not add input or command");
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
	if (!check(suppressed.kind == ActionKind::None &&
				suppressed.movementButtons == 0U &&
				suppressed.viewAngles.yaw == movementAngles.yaw,
			"suppressed Fire restores the locomotion view"))
	{
		return false;
	}
	const auto ready = ActionAdapter::forLiveDispatch(fire, true, movementAngles);
	return check(ready.kind == ActionKind::Fire,
		"live Fire remains available when an active weapon boundary is present");
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
	return check(plantDispatch.stopMovement && defuseDispatch.stopMovement &&
				plantDispatch.clientCommand == ActionAdapter::kSelectC4Command,
			"plant and defuse hold the bot in place") &&
		check(!ActionAdapter::translate({ActionKind::Fire, {}, 0U}).stopMovement,
			"Fire preserves locomotion");
}

bool testWorldMovementProjectionIgnoresAimTarget()
{
	using astrabot::metamod::ActionAdapter;

	const auto forward = ActionAdapter::projectMovement(1.0f, 0.0f, 100.0f, 90.0f);
	const auto left = ActionAdapter::projectMovement(0.0f, 1.0f, 100.0f, 0.0f);
	return check(std::fabs(forward.forward) < 0.01f &&
				std::fabs(forward.side - 100.0f) < 0.01f,
			"world east remains east when aiming north") &&
		check(std::fabs(left.forward) < 0.01f &&
				std::fabs(left.side + 100.0f) < 0.01f,
			"world north projects to left strafe when aiming east");
}

bool testMovementButtonsFollowProjectedInput()
{
	using astrabot::metamod::ActionAdapter;

	const auto forward = ActionAdapter::movementButtons(100.0f, 0.0f);
	const auto back = ActionAdapter::movementButtons(-100.0f, 0.0f);
	const auto left = ActionAdapter::movementButtons(0.0f, -100.0f);
	const auto right = ActionAdapter::movementButtons(0.0f, 100.0f);
	return check((forward & ActionAdapter::kForwardButton) != 0U &&
				(forward & ActionAdapter::kBackButton) == 0U,
			"forward analog input carries IN_FORWARD") &&
		check((back & ActionAdapter::kBackButton) != 0U &&
				(back & ActionAdapter::kForwardButton) == 0U,
			"backward analog input carries IN_BACK") &&
		check((left & ActionAdapter::kMoveLeftButton) != 0U &&
			(right & ActionAdapter::kMoveRightButton) != 0U,
			"side analog input carries the matching strafe button");
}
} // namespace

bool testPlantContinuationReselectsC4()
{
	using astrabot::metamod::ActionAdapter;
	using astrabot::metamod::ActionKind;
	using astrabot::metamod::ActionProposal;
	const ActionProposal continuation = {
		ActionKind::PlantContinue, {0.0f, 45.0f, 0.0f},
		ActionAdapter::kForwardButton};
	const auto dispatch = ActionAdapter::translate(continuation);
	return check(dispatch.kind == ActionKind::PlantContinue &&
			dispatch.stopMovement &&
			(dispatch.buttons & ActionAdapter::kAttackButton) != 0U &&
			dispatch.clientCommand == ActionAdapter::kSelectC4Command,
		"plant continuation reselects C4 while holding attack");
}

bool testPlantAttackWaitsForObservedC4Selection()
{
	using astrabot::metamod::ActionAdapter;
	using astrabot::metamod::ActionKind;

	const ActionKind noWeaponObservation = ActionAdapter::plantActionForWeaponObservation(
		false, false, 0U, false);
	const auto selectDispatch = ActionAdapter::translate(
		{noWeaponObservation, {}, 0U});
	if (!check(noWeaponObservation == ActionKind::SelectC4 &&
			(selectDispatch.buttons & ActionAdapter::kAttackButton) == 0U &&
			selectDispatch.clientCommand != nullptr &&
			std::strcmp(selectDispatch.clientCommand, "weapon_c4") == 0,
			"unknown active weapon requests ZBot SelectItem without attacking"))
	{
		return false;
	}
	if (!check(ActionAdapter::plantActionForWeaponObservation(
			true, true, ActionAdapter::kC4WeaponId, false) == ActionKind::Plant,
			"primary attack starts only after CurWeapon reports active C4"))
	{
		return false;
	}
	const auto plantDispatch = ActionAdapter::translate({ActionKind::Plant, {}, 0U});
	if (!check((plantDispatch.buttons & ActionAdapter::kAttackButton) != 0U &&
			plantDispatch.clientCommand != nullptr &&
			std::strcmp(plantDispatch.clientCommand, "weapon_c4") == 0,
			"active C4 is attacked and reselected like ZBot"))
	{
		return false;
	}
	return check(ActionAdapter::plantActionForWeaponObservation(
			true, true, ActionAdapter::kC4WeaponId, true) == ActionKind::PlantContinue &&
			ActionAdapter::plantActionForWeaponObservation(true, true, 7U, false) ==
				ActionKind::SelectC4,
			"continued attack requires current C4 observation");
}

bool testPlantObjectiveAcceptsAnySelectedBombTargetClass()
{
	using astrabot::metamod::ActionAdapter;
	return check(ActionAdapter::isBombTargetClassname("func_bomb_target") &&
			ActionAdapter::isBombTargetClassname("INFO_BOMB_TARGET") &&
			!ActionAdapter::isBombTargetClassname("info_target"),
			"plant target classifier accepts both public BombTarget classnames") &&
		check(ActionAdapter::canBeginPlantObjective(true, true, true, true),
			"selected info-only site permits a C4-carrying Terrorist to plant") &&
		check(!ActionAdapter::canBeginPlantObjective(true, true, true, false),
			"plant action requires a target selected by the shared BombTarget selector");
}

bool testPlantZoneBoundsAndTimeout()
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
		check(!ActionAdapter::overlapsPlantTarget(site, invalid),
			"invalid site bounds fail closed") &&
		check(!ActionAdapter::plantAttemptExpired(4.99f),
			"plant attempt is live before ZBot timeout") &&
		check(ActionAdapter::plantAttemptExpired(5.0f),
			"plant attempt times out at five seconds") &&
		check(!ActionAdapter::plantAttemptExpired(
			std::numeric_limits<float>::quiet_NaN()),
			"nonfinite plant elapsed time does not trigger timeout");
}

bool testPlantAttemptRequiresSiteAndTimesOutAfterDeliveredInput()
{
	using astrabot::metamod::ActionAdapter;
	using astrabot::metamod::PlantAttemptResult;
	using astrabot::metamod::PlantAttemptState;
	PlantAttemptState state{};
	if (!check(ActionAdapter::evaluatePlantAttempt(state, false, false, 10.0f) ==
			PlantAttemptResult::OutsideSite,
			"plant attempt is suppressed outside selected target bounds"))
	{
		return false;
	}
	if (!check(ActionAdapter::evaluatePlantAttempt(state, true, false, 10.0f) ==
			PlantAttemptResult::Ready,
			"plant attempt may start after entering selected target bounds"))
	{
		return false;
	}
	ActionAdapter::recordPlantAttemptDispatched(state, 10.0f);
	if (!check(ActionAdapter::evaluatePlantAttempt(state, true, false, 14.99f) ==
			PlantAttemptResult::Continuing,
			"delivered attack input keeps the plant attempt active for five seconds"))
	{
		return false;
	}
	if (!check(ActionAdapter::evaluatePlantAttempt(state, true, false, 15.0f) ==
			PlantAttemptResult::TimedOut && !state.active,
			"unconfirmed plant times out and releases the action state"))
	{
		return false;
	}
	if (!check(ActionAdapter::evaluatePlantAttempt(state, true, false, 15.5f) ==
			PlantAttemptResult::RetryPending,
			"timed out plant waits before replanning an attempt"))
	{
		return false;
	}
	if (!check(ActionAdapter::evaluatePlantAttempt(state, true, false, 16.0f) ==
			PlantAttemptResult::Ready,
			"plant attempt can restart after its retry delay"))
	{
		return false;
	}
	return check(ActionAdapter::evaluatePlantAttempt(state, true, true, 16.0f) ==
			PlantAttemptResult::Confirmed && !state.active,
			"observed planted bomb clears the attack attempt");
}

int main()
{
	return testFireAndObjectiveButtons() && testReloadAndNoOp() &&
		testLiveFireRequiresWeaponBoundary() &&
		testActionStopsMovementForObjectiveUse() &&
			testPlantContinuationReselectsC4() &&
			testPlantAttackWaitsForObservedC4Selection() &&
			testPlantObjectiveAcceptsAnySelectedBombTargetClass() &&
			testPlantZoneBoundsAndTimeout() &&
			testPlantAttemptRequiresSiteAndTimesOutAfterDeliveredInput() &&
		testWorldMovementProjectionIgnoresAimTarget() &&
		testMovementButtonsFollowProjectedInput() ? 0 : 1;
}
