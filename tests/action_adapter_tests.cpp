#include "action_adapter.hpp"

#include <cmath>
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

int main()
{
	return testFireAndObjectiveButtons() && testReloadAndNoOp() &&
		testLiveFireRequiresWeaponBoundary() &&
		testActionStopsMovementForObjectiveUse() &&
		testWorldMovementProjectionIgnoresAimTarget() &&
		testMovementButtonsFollowProjectedInput() ? 0 : 1;
}
