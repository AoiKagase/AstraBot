// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include "core/combat.hpp"

#include <cassert>
#include <cmath>

namespace
{

namespace c = astrabot::core::combat;
using astrabot::core::BotCommand;
using astrabot::core::Button;
using astrabot::core::ButtonMask;
using astrabot::core::PlayerId;

constexpr PlayerId kTarget{2, {1}};

c::CombatDecision decision(c::CombatAction action)
{
	c::CombatDecision value{};
	value.action = action;
	value.target = action == c::CombatAction::Fire ? kTarget : PlayerId{};
	value.view = {12.0F, -45.0F, 0.0F};
	value.confidence = 1.0;
	value.reason = c::CombatReason::Accepted;
	value.inputTick = {7};
	if (action == c::CombatAction::Fire)
	{
		value.fireMode = c::FireMode::DirectFire;
		value.firePlan = c::FirePlan::tap();
		value.buttons = static_cast<ButtonMask>(Button::Attack);
		value.selectedWeapon = {5};
	}
	else if (action == c::CombatAction::Reload)
	{
		value.buttons = static_cast<ButtonMask>(Button::Reload);
	}
	return value;
}

BotCommand navigationCommand()
{
	auto value = BotCommand::neutral(16);
	value.view = {-10.0F, 10.0F, 0.0F};
	value.movement = {125.0F, -40.0F, 5.0F};
	value.buttons = static_cast<ButtonMask>(Button::Forward) | static_cast<ButtonMask>(Button::Jump) |
					static_cast<ButtonMask>(Button::Attack) | static_cast<ButtonMask>(Button::Reload);
	value.weaponSelect = 99;
	value.impulse = 4;
	return value;
}

void testMovementAndCombatOwnersRemainSeparate()
{
	const auto navigation = navigationCommand();
	const auto composed = c::composeCommand(decision(c::CombatAction::Fire), navigation);
	assert(composed);
	assert(composed.command.view == decision(c::CombatAction::Fire).view);
	const double radians = (static_cast<double>(navigation.view.yaw) - static_cast<double>(composed.command.view.yaw)) *
						   3.14159265358979323846 / 180.0;
	const double cosine = std::cos(radians);
	const double sine = std::sin(radians);
	assert(std::abs(composed.command.movement.forward - static_cast<float>(navigation.movement.forward * cosine +
																		   navigation.movement.side * sine)) < 0.001F);
	assert(std::abs(composed.command.movement.side - static_cast<float>(-navigation.movement.forward * sine +
																		navigation.movement.side * cosine)) < 0.001F);
	assert(composed.command.impulse == navigation.impulse);
	assert(composed.command.msec == navigation.msec);
	assert(composed.command.weaponSelect == 0);
	assert((composed.command.buttons & static_cast<ButtonMask>(Button::Forward)) != 0U);
	assert((composed.command.buttons & static_cast<ButtonMask>(Button::Jump)) != 0U);
	assert((composed.command.buttons & static_cast<ButtonMask>(Button::Attack)) != 0U);
	assert((composed.command.buttons & static_cast<ButtonMask>(Button::Reload)) == 0U);

	const auto reload = c::composeCommand(decision(c::CombatAction::Reload), navigation);
	assert(reload);
	assert((reload.command.buttons & static_cast<ButtonMask>(Button::Attack)) == 0U);
	assert((reload.command.buttons & static_cast<ButtonMask>(Button::Reload)) != 0U);

	auto switchDecision = decision(c::CombatAction::SwitchWeapon);
	switchDecision.selectedWeapon = {7};
	const auto switched = c::composeCommand(switchDecision, navigation);
	assert(switched);
	assert(switched.command.weaponSelect == 7);
	assert((switched.command.buttons & static_cast<ButtonMask>(Button::Attack)) == 0U);
	assert((switched.command.buttons & static_cast<ButtonMask>(Button::Reload)) == 0U);
}

void testInvalidInputsFailClosedAndAreDeterministic()
{
	const auto navigation = navigationCommand();
	auto invalidDecision = decision(c::CombatAction::Fire);
	invalidDecision.firePlan.reset();
	const auto rejectedDecision = c::composeCommand(invalidDecision, navigation);
	assert(!rejectedDecision);
	assert(rejectedDecision.error == c::CommandCompositionError::InvalidDecision);

	auto invalidNavigation = navigation;
	invalidNavigation.msec = 0;
	const auto rejectedNavigation = c::composeCommand(decision(c::CombatAction::Track), invalidNavigation);
	assert(!rejectedNavigation);
	assert(rejectedNavigation.error == c::CommandCompositionError::InvalidNavigationCommand);

	auto switchDecision = decision(c::CombatAction::SwitchWeapon);
	switchDecision.selectedWeapon = {7};
	const auto first = c::composeCommand(switchDecision, navigation);
	const auto second = c::composeCommand(switchDecision, navigation);
	assert(first && second);
	assert(first.command == second.command);
}

void testNoTargetScanOwnsOnlyStationaryValidView()
{
	auto scan = decision(c::CombatAction::NoOp);
	scan.reason = c::CombatReason::NoTarget;
	scan.view = {0.0F, -90.0F, 0.0F};

	auto stationary = BotCommand::neutral(16);
	stationary.view = {0.0F, 25.0F, 0.0F};
	const auto searching = c::composeCommand(scan, stationary);
	assert(searching && searching.command.view == scan.view);

	scan.reason = c::CombatReason::AnonymousSound;
	assert(c::composeCommand(scan, stationary).command.view == scan.view);

	auto moving = stationary;
	moving.movement.forward = 100.0F;
	assert(c::composeCommand(scan, moving).command.view == moving.view);

	auto usingDoor = stationary;
	usingDoor.buttons = static_cast<ButtonMask>(Button::Use);
	assert(c::composeCommand(scan, usingDoor).command.view == usingDoor.view);
	assert((c::composeCommand(scan, usingDoor).command.buttons & static_cast<ButtonMask>(Button::Use)) != 0U);

	scan.reason = c::CombatReason::InvalidInput;
	assert(c::composeCommand(scan, stationary).command.view == stationary.view);
}

void testDirectVisionCombatHoldsNavigation()
{
	auto combat = decision(c::CombatAction::Fire);
	combat.source = astrabot::core::perception::ObservationSource::Vision;
	const auto composed = c::composeCommand(combat, navigationCommand());
	assert(composed);
	assert(composed.command.movement == astrabot::core::Movement{});
	const auto movementButtons = static_cast<ButtonMask>(Button::Jump) | static_cast<ButtonMask>(Button::Forward) |
								 static_cast<ButtonMask>(Button::Back) | static_cast<ButtonMask>(Button::Use) |
								 static_cast<ButtonMask>(Button::MoveLeft) | static_cast<ButtonMask>(Button::MoveRight);
	assert((composed.command.buttons & movementButtons) == 0U);
	assert((composed.command.buttons & static_cast<ButtonMask>(Button::Duck)) != 0U);
	assert((composed.command.buttons & static_cast<ButtonMask>(Button::Attack)) != 0U);
}

} // namespace

int main()
{
	testMovementAndCombatOwnersRemainSeparate();
	testInvalidInputsFailClosedAndAreDeterministic();
	testNoTargetScanOwnsOnlyStationaryValidView();
	testDirectVisionCombatHoldsNavigation();
	return 0;
}
