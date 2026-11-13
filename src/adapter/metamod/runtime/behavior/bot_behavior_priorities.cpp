#include "runtime/behavior/bot_behavior_runtime.hpp"

namespace astrabot
{
namespace metamod
{
void BotBehaviorRuntime::updateBehaviorPriorities(std::size_t index, const runtime::MovementPhysicsState &before,
	compat::StateUpdateContext &context, compat::StateBehaviorInput &stateBehavior)
{
	auto &machine = actors_[index].managedBotStateMachines;
	const bool buyingRequested = shouldBuy(index, context);
	constexpr float kInvestigatedNoiseRadius = 50.0f;
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
		const auto &belief = services_->observation_managedBotPerception(index).snapshot();
		const auto &noise = belief.noise();
		stateBehavior.noiseAvailable = belief.isValid() &&
			belief.identity().frame.mapGeneration == context.frame.mapGeneration &&
			belief.identity().frame.roundGeneration == context.frame.roundGeneration && noise.active && noise.ready &&
			noise.timestampTick > actors_[index].investigatedNoiseTicks;
		if (stateBehavior.noiseAvailable)
		{
			const float dx = before.origin.x - noise.position.x;
			const float dy = before.origin.y - noise.position.y;
			const float dz = before.origin.z - noise.position.z;
			if (dx * dx + dy * dy + dz * dz <= kInvestigatedNoiseRadius * kInvestigatedNoiseRadius)
			{
				actors_[index].investigatedNoiseTicks = noise.timestampTick;
				stateBehavior.noiseAvailable = false;
				stateBehavior.goalReached = true;
			}
			else if (machine.state() == compat::CompatibilityStateId::Hunt ||
				machine.state() == compat::CompatibilityStateId::Idle)
			{
				context.requestedState = compat::CompatibilityStateId::InvestigateNoise;
				context.requestTransition = true;
				context.transitionId = compat::StateTransitionId::ExplicitStateChange;
				context.transitionReason = compat::StateTransitionReason::ObjectiveChanged;
			}
		}
	}
}
} // namespace metamod
} // namespace astrabot
