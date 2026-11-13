#ifndef ASTRABOT_ADAPTER_BOT_NAVIGATION_RUNTIME_HPP
#define ASTRABOT_ADAPTER_BOT_NAVIGATION_RUNTIME_HPP
#include "runtime/shared/bot_runtime_services.hpp"
#include <limits>
namespace astrabot
{
namespace metamod
{
class BotNavigationRuntime
{
public:
	void bindServices(BotNavigationServices &services)
	{
		services_ = &services;
	}

	void logGoalAssignmentDiagnostic(std::size_t index, const runtime::MovementPhysicsState &before,
		const runtime::NavRoamDecision &decision, const nav::LocomotionIntent &intent, runtime::NavRoamResult result,
		const nav::GroundLookaheadObservation &ground, const runtime::GroundLookaheadRoute &probeRoute);
	void logMovementDiagnostic(
		std::size_t index, const char *reason, const runtime::NavRoamDecision *decision = nullptr);
	void logMovementReversalDiagnostic(std::size_t index, const runtime::MovementPhysicsState &before,
		const runtime::NavRoamDecision &decision, const nav::LocomotionIntent &intent);
	void logTraversalDiagnostic(std::size_t index, const runtime::MovementPhysicsState &before,
		const runtime::NavRoamDecision &decision, const nav::LocomotionIntent &intent, std::uint16_t buttons,
		const nav::LocomotionObservation &locomotion);
	void logTraversalOutcome(std::size_t index, const runtime::MovementPhysicsState &after,
		const runtime::NavRoamDecision &decision, const runtime::LandingDamageObservation &landingFeedback);
	nav::NavPathCostContext managedPathCostContext(std::size_t index);
	void recordMovementPhysicsSample(
		std::size_t index, const runtime::MovementPhysicsState &before, const runtime::CommandReceipt &receipt);
	const ManagedNavigationFailure &managedNavigationFailures(std::size_t index) const
	{
		return actors_[index].managedNavigationFailures;
	}

	const compat::NavigationStep &managedNavigationSteps(std::size_t index) const
	{
		return actors_[index].managedNavigationSteps;
	}

	const std::uint8_t &movementDiagnosticSamples(std::size_t index) const
	{
		return actors_[index].movementDiagnosticSamples;
	}

	const runtime::MovementPhysicsSample &movementPhysicsSamples(std::size_t index) const
	{
		return actors_[index].movementPhysicsSamples;
	}

	void onActorRemoved(std::size_t index);

	void resetSharedExecutionState();

	void resetActorExecutionState(std::size_t index);

	void beginMovementUpdate(
		const compat::CvarSnapshot &configuration, const nav::NavSnapshot &navigation, bool movementUnavailable);

	void observeFullUpdateMotion(std::size_t index, const FakeClientHandle &handle,
		const runtime::MovementPhysicsState &before, const compat::CvarSnapshot &configuration);

	MovementPreparation prepareMovement(std::size_t index, FakeClientHandle &handle,
		const runtime::MovementPhysicsState &before, bool movementUnavailable);

	void completePreparation(std::size_t index, MovementPreparationCompletion completion);
	NavigationUpdate updateNavigation(std::size_t index, const FakeClientHandle &handle,
		const runtime::MovementPhysicsState &before, const nav::NavSnapshot &navigation,
		bool performanceDisablePathSearch_);

	void recordNavigationUpdate(
		std::size_t index, const runtime::MovementPhysicsState &before, NavigationUpdate &movement);

	void resetRoundDeathReceipts();

	void resetMapDanger();

	void resetAvailabilityDiagnostic();
	void restartLoggingDiagnostics();

	void resetCreatedActorRoute(std::size_t slotIndex);

	void resetCreatedActorSampling(std::size_t slotIndex);

	void resetCreatedActorReadiness(std::size_t slotIndex);

	void observePublicDeath(std::size_t victim);

	void observeResetHud(std::size_t index);

	void applyBehaviorRequest(const NavigationRequest &request);

	void noteMovementDiagnosticSample(std::size_t index);

private:
	void fill_movementReversalDiagnostics(const MovementReversalDiagnosticState &value)
	{
		for (auto &actor : actors_)
			actor.movementReversalDiagnostics = value;
	}

	void fill_movementWasAirborne(const bool &value)
	{
		for (auto &actor : actors_)
			actor.movementWasAirborne = value;
	}

	void fill_movementDispatchFrames(const std::uint32_t &value)
	{
		for (auto &actor : actors_)
			actor.movementDispatchFrames = value;
	}

	void fill_movementTraversalPending(const PendingTraversalDiagnostic &value)
	{
		for (auto &actor : actors_)
			actor.movementTraversalPending = value;
	}

	void fill_movementTraversalDiagnosticSeconds(const std::uint32_t &value)
	{
		for (auto &actor : actors_)
			actor.movementTraversalDiagnosticSeconds = value;
	}

	void fill_movementGoalDiagnosticSeconds(const std::uint32_t &value)
	{
		for (auto &actor : actors_)
			actor.movementGoalDiagnosticSeconds = value;
	}

	void fill_movementReadyLogged(const bool &value)
	{
		for (auto &actor : actors_)
			actor.movementReadyLogged = value;
	}

	void fill_movementPhysicsSamples(const runtime::MovementPhysicsSample &value)
	{
		for (auto &actor : actors_)
			actor.movementPhysicsSamples = value;
	}

	void fill_movementGroundLookaheadRoutes(const runtime::GroundLookaheadRoute &value)
	{
		for (auto &actor : actors_)
			actor.movementGroundLookaheadRoutes = value;
	}

	void fill_movementWarmupFrames(const std::uint8_t &value)
	{
		for (auto &actor : actors_)
			actor.movementWarmupFrames = value;
	}

	void fill_movementSettledDeadFrames(const std::uint32_t &value)
	{
		for (auto &actor : actors_)
			actor.movementSettledDeadFrames = value;
	}

	void fill_movementLastDeadFrames(const std::uint32_t &value)
	{
		for (auto &actor : actors_)
			actor.movementLastDeadFrames = value;
	}

	void fill_movementResumeFrames(const std::uint32_t &value)
	{
		for (auto &actor : actors_)
			actor.movementResumeFrames = value;
	}

	void fill_movementDiagnosticUnavailable(const bool &value)
	{
		for (auto &actor : actors_)
			actor.movementDiagnosticUnavailable = value;
	}

	void fill_movementDiagnosticAttempts(const bool &value)
	{
		for (auto &actor : actors_)
			actor.movementDiagnosticAttempts = value;
	}

	void fill_movementDiagnosticSamples(const std::uint8_t &value)
	{
		for (auto &actor : actors_)
			actor.movementDiagnosticSamples = value;
	}

	void reset_deathDangerRecorded()
	{
		for (auto &actor : actors_)
			actor.deathDangerRecorded = {};
	}

	void reset_managedNavigationSteps()
	{
		for (auto &actor : actors_)
			actor.managedNavigationSteps = {};
	}

	void reset_managedNavigationFailures()
	{
		for (auto &actor : actors_)
			actor.managedNavigationFailures = {};
	}

	struct ActorState
	{
		runtime::NavRoamController managedBotMovement{};
		ManagedNavigationFailure managedNavigationFailures{};
		compat::NavigationStep managedNavigationSteps{};
		bool deathDangerRecorded{};
		std::uint8_t movementDiagnosticSamples{};
		bool movementDiagnosticAttempts{};
		bool movementDiagnosticUnavailable{};
		std::uint32_t movementResumeFrames{};
		std::uint32_t movementLastDeadFrames{};
		std::uint32_t movementSettledDeadFrames{};
		std::uint8_t movementWarmupFrames{};
		runtime::GroundLookaheadRoute movementGroundLookaheadRoutes{};
		runtime::MovementPhysicsSample movementPhysicsSamples{};
		bool movementReadyLogged{};
		std::uint32_t movementGoalDiagnosticSeconds{};
		std::uint32_t movementTraversalDiagnosticSeconds{};
		PendingTraversalDiagnostic movementTraversalPending{};
		std::uint32_t movementDispatchFrames{};
		bool movementWasAirborne{};
		runtime::FallLandingTracker movementFallLandingTrackers{};
		MovementReversalDiagnosticState movementReversalDiagnostics{};
	};
	std::array<ActorState, NativeBotObservation::kClientSlotCount> actors_{};
	BotNavigationServices *services_ = nullptr;
	runtime::NavAreaVisitHistory managedBotAreaVisitHistory_{};
	runtime::NavRoamReservationBoard managedBotRoamReservationBoard_{};
	nav::DangerMemory dangerMemory_{};
	std::uint32_t movementDiagnosticRound_{};
	bool movementDiagnosticGlobal_{};
};
} // namespace metamod
} // namespace astrabot
#endif
