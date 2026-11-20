#include "runtime/behavior/bot_behavior_runtime.hpp"

namespace astrabot
{
namespace metamod
{
void BotBehaviorRuntime::updateBehaviorPriorities(std::size_t index, const runtime::MovementPhysicsState &before,
	compat::StateUpdateContext &context, compat::StateBehaviorInput &stateBehavior, bool wasAtDefenseSpot)
{
	auto &machine = actors_[index].managedBotStateMachines;
	const bool buyingRequested = shouldBuy(index, context);
	if (buyingRequested || machine.state() == compat::CompatibilityStateId::Buy)
	{
		context.requestedState =
			buyingRequested ? compat::CompatibilityStateId::Buy : compat::CompatibilityStateId::Idle;
		context.requestTransition = machine.state() != context.requestedState;
		context.transitionId = compat::StateTransitionId::ExplicitStateChange;
		context.transitionReason = compat::StateTransitionReason::ObjectiveChanged;
	}
	else if (!context.requestTransition && !machine.attackOverlayActive())
	{
		updateNoiseInvestigation(index, before, context, stateBehavior, wasAtDefenseSpot);
	}
}
} // namespace metamod
} // namespace astrabot
