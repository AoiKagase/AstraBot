#ifndef ASTRABOT_ADAPTER_BOT_COMMAND_RUNTIME_HPP
#define ASTRABOT_ADAPTER_BOT_COMMAND_RUNTIME_HPP
#include "runtime/shared/bot_runtime_services.hpp"
#include <limits>
namespace astrabot
{
namespace metamod
{
class BotCommandRuntime
{
public:
	void bindServices(BotCommandServices &services)
	{
		services_ = &services;
	}

	void applyPlayerAvoidance(std::size_t index, runtime::BotCommand *command, float desiredSpeed);
	runtime::CommandReceipt dispatchJoinHeartbeat(
		std::size_t index, FakeClientHandle &handle, std::uint8_t milliseconds);
	bool dispatchManagedActionCommand(
		std::size_t index, FakeClientHandle &handle, const ActionProposal &proposal, const ActionDispatch &dispatch);
	MovementCommandDraft beginDirectNeutralCommand(
		std::size_t index, FakeClientHandle &handle, std::uint8_t milliseconds);
	runtime::CommandReceipt finishDirectNeutralCommand(std::size_t index, FakeClientHandle &handle,
		std::uint8_t milliseconds, MovementCommandDraft draft, const ActionProposal &requestedAction);
	runtime::CommandReceipt executeManagedBotCommand(std::size_t index, FakeClientHandle &handle);
	void onTouch(edict_t *entity, edict_t *other);
	MovementCommandDraft beginNeutralCommand(
		std::size_t index, FakeClientHandle &handle, const runtime::MovementPhysicsState &before);
	void finishNeutralCommand(std::size_t index, FakeClientHandle &handle, const runtime::MovementPhysicsState &before,
		MovementCommandDraft draft, const ActionProposal &requestedAction);
	void resetManagedBotCommandTemplate(std::size_t index);
	const std::uint32_t &managedBotCommandSequences(std::size_t index) const
	{
		return actors_[index].managedBotCommandSequences;
	}

	const runtime::BotCommand &managedBotCommandTemplates(std::size_t index) const
	{
		return actors_[index].managedBotCommandTemplates;
	}

	const bool &managedBotCommandTemplateValid(std::size_t index) const
	{
		return actors_[index].managedBotCommandTemplateValid;
	}

	void onActorRemoved(std::size_t index);

	void onActorRemembered(std::size_t index);

	void resetSharedExecutionState();

	void resetActorExecutionState(std::size_t index);

	MovementCommandDraft beginMovementCommand(std::size_t index, FakeClientHandle &handle,
		const runtime::LifecycleToken &token, const runtime::MovementPhysicsState &before, NavigationUpdate &movement);
	void finishMovementCommand(std::size_t index, FakeClientHandle &handle, const runtime::LifecycleToken &token,
		const runtime::MovementPhysicsState &before, NavigationUpdate &movement, MovementCommandDraft draft,
		const ActionProposal &requestedAction);

	void resetCreatedActorCommand(std::size_t slotIndex);

	void resetCreatedActorGateDiagnostic(std::size_t slotIndex);

	void resetJumpCrouch(std::size_t index);

	bool dispatchClientCommand(edict_t *entity, const char *name, const char *argument);

	void resetGateDiagnostics();

	void stabilizeFrozenActors();
	void applyCombatUpkeep(std::size_t index, const CombatUpkeepUpdate &update);
	runtime::QueueResult enqueue(const runtime::BotCommand &command);
	runtime::CommandReceipt dispatchInput(const runtime::ActorId &actor, std::uint32_t dispatchFrame);

private:
	void fill_movementGateDiagnosticTimes(const float &value)
	{
		for (auto &actor : actors_)
			actor.movementGateDiagnosticTimes = value;
	}

	void fill_managedBotCommandSequences(const std::uint32_t &value)
	{
		for (auto &actor : actors_)
			actor.managedBotCommandSequences = value;
	}

	void reset_managedCommandDiagnosticUpdates()
	{
		for (auto &actor : actors_)
			actor.managedCommandDiagnosticUpdates = {};
	}

	void reset_managedMoveSpeeds()
	{
		for (auto &actor : actors_)
			actor.managedMoveSpeeds = {};
	}

	struct ActorState
	{
		runtime::PlayerAvoidance playerAvoidance{};
		float managedMoveSpeeds{};
		std::uint32_t managedCommandDiagnosticUpdates{};
		std::uint32_t managedBotCommandSequences{};
		runtime::JumpCrouchSequencer managedBotJumpCrouch{};
		runtime::BotCommand managedBotCommandTemplates{};
		bool managedBotCommandTemplateValid{};
		bool managedBotActionStopsMovement{};
		float movementGateDiagnosticTimes = std::numeric_limits<float>::quiet_NaN();
	};
	std::array<ActorState, NativeBotObservation::kClientSlotCount> actors_{};
	BotCommandServices *services_ = nullptr;
};
} // namespace metamod
} // namespace astrabot
#endif
