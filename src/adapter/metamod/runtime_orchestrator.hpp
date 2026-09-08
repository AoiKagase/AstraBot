// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#pragma once

// This is an adapter-private value boundary.  It deliberately contains no
// SDK handles; the LifecycleCoordinator converts engine state to these values
// before the orchestration loop is entered.
#include "core/action_planner.hpp"
#include "core/combat.hpp"
#include "core/experience.hpp"
#include "core/p11_learning.hpp"
#include "core/tactical_planner.hpp"
#include "core/team_director.hpp"
#include "core/world_model.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace astrabot::adapter::metamod {

constexpr std::size_t kRuntimeActorCapacity = 32;
constexpr std::size_t kRuntimeExperienceEvents = 8;
constexpr std::size_t kRuntimeDiagnosticCapacity = 128;
constexpr std::uint64_t kTeamDirectorCadenceMicros = 1'000'000;
constexpr std::uint64_t kTacticalPlannerCadenceMicros = 200'000;
constexpr std::uint64_t kActionPlannerCadenceMicros = 100'000;

enum class RuntimeStage : std::uint8_t {
	None = 0,
	PerceptionPublished,
	ExperienceUpdated,
	TeamDirector,
	TacticalPlanner,
	ActionPlanner,
	Combat,
	Navigation,
};

enum class RuntimeRejectReason : std::uint8_t {
	None = 0,
	InvalidFrame,
	StaleFrame,
	InvalidActorInput,
	DuplicateActor,
	InvalidTeamInput,
	InvalidTacticalInput,
	InvalidActionInput,
	InvalidCombatInput,
	NonPrimaryActor,
	PlannerRejected,
	StaleGeneration,
	QueueDuplicate,
};

struct RuntimeFrame final {
	core::MapGeneration map{};
	core::perception::RoundGeneration round{};
	core::TickId tick{};
	std::uint64_t nowMicros{0};
	std::uint64_t elapsedMicros{0};
	// Optional because a host may not have finished BSP/NAV fingerprinting at
	// the first frame.  Experience events remain disabled until it is valid.
	core::experience::MapIdentity mapIdentity{};

	bool valid() const noexcept;
};

struct RuntimeActorInput final {
	core::PlayerId player{};
	core::BotAgentId agent{};
	// The current adapter contract executes exactly one primary bot. The
	// slot-oriented arrays below are retained for a future multi-bot mode,
	// but an input that is not explicitly marked primary is never executed.
	bool primary{false};
	core::world::WorldSnapshot world{};
	core::team::TeamSnapshot team{};
	// Missing objective observations disable team strategy for this frame.
	// Self perception/combat may continue with a neutral team decision.
	bool teamObjectiveAvailable{true};
	core::tactical::TacticalContextSeed tactical{};
	core::action::ActionPlannerInput action{};
	core::combat::CombatInput combat{};
	core::tactical::ReplanEvents tacticalEvents{};
	core::team::TeamEvents teamEvents{};
	core::action::ActionObservation actionObservation{};
	std::array<core::experience::ExperienceEvent, kRuntimeExperienceEvents> experienceEvents{};
	std::size_t experienceEventCount{0};
	std::optional<core::learning::ContextualDangerObservation> contextualDanger{};
	std::optional<core::learning::OpponentObservation> opponent{};

	bool valid(const RuntimeFrame &frame) const noexcept;
};

struct RuntimeDecision final {
	core::PlayerId player{};
	core::BotAgentId agent{};
	core::team::TeamDecision team{};
	core::tactical::TacticalDecision tactical{};
	core::action::ActionDecision action{};
	core::combat::CombatDecision combat{};
	nav::model::NavAreaId navigationGoal{};
	RuntimeRejectReason rejection{RuntimeRejectReason::None};
	bool hasNavigationGoal{false};
	bool teamExecuted{false};
	bool tacticalExecuted{false};
	bool actionExecuted{false};
	bool combatExecuted{false};
	bool executable{false};
};

struct RuntimeFrameResult final {
	std::array<RuntimeDecision, kRuntimeActorCapacity> decisions{};
	std::size_t decisionCount{0};
	std::size_t executableCount{0};
	std::size_t queuedCombatCount{0};
	std::array<RuntimeStage, 8> stageTrace{};
	std::size_t stageCount{0};
	bool accepted{false};
};

struct RuntimeDiagnostic final {
	RuntimeStage stage{RuntimeStage::None};
	RuntimeRejectReason reason{RuntimeRejectReason::None};
	core::MapGeneration map{};
	core::perception::RoundGeneration round{};
	core::TickId tick{};
	core::PlayerId player{};
};

struct RuntimeDiagnostics final {
	std::array<RuntimeDiagnostic, kRuntimeDiagnosticCapacity> entries{};
	std::size_t count{0};
	std::uint64_t dropped{0};
};

class RuntimeOrchestrator final {
  public:
	const RuntimeFrameResult &run(const RuntimeFrame &frame, const RuntimeActorInput *inputs,
	                              std::size_t inputCount) noexcept;

	void reset() noexcept;
	// LifecycleCoordinator calls this at ServerActivate so map-session
	// learning is retired before the first frame is accepted.
	void beginMap(core::MapGeneration map) noexcept;
	void onDisconnect(core::PlayerId player) noexcept;
	void onDeath(core::PlayerId player) noexcept;

	// A navigation producer calls this after run() and before NavConsole's
	// movement pass.  A matching value is consumed exactly once.
	std::optional<core::combat::CombatDecision>
	takeCombatDecision(core::PlayerId player, core::BotAgentId agent, core::MapGeneration map,
	                   core::perception::RoundGeneration round, core::TickId tick) noexcept;

	const RuntimeFrameResult &result() const noexcept { return result_; }
	const RuntimeDiagnostics &diagnostics() const noexcept { return diagnostics_; }
	const core::team::TeamDirector &teamDirector() const noexcept { return team_; }
	const core::experience::ExperienceModel &experience() const noexcept {
		return experience_.model();
	}
	const core::learning::ContextualDangerModel &contextualDanger() const noexcept {
		return contextualDanger_;
	}
	const core::learning::OpponentProfileModel &opponentProfiles() const noexcept {
		return opponentProfiles_;
	}

  private:
	static std::size_t slotIndex(core::PlayerId player) noexcept;
	void addDiagnostic(RuntimeStage stage, RuntimeRejectReason reason, const RuntimeFrame &frame,
	                   core::PlayerId player) noexcept;
	void appendStage(RuntimeStage stage) noexcept;
	void clearSlot(std::size_t index) noexcept;
	void resetPlanners() noexcept;
	bool beginFrameContext(const RuntimeFrame &frame) noexcept;
	static core::action::TacticalRole roleFor(core::team::Role role) noexcept;
	const core::team::RoleAssignment *assignmentFor(core::PlayerId player) const noexcept;

	RuntimeFrameResult result_{};
	RuntimeDiagnostics diagnostics_{};
	std::array<std::optional<core::tactical::TacticalPlanner>, kRuntimeActorCapacity> tactical_{};
	std::array<std::optional<core::action::ActionPlanner>, kRuntimeActorCapacity> action_{};
	std::array<core::combat::AttackLifecycleState, kRuntimeActorCapacity> combat_{};
	std::array<RuntimeDecision, kRuntimeActorCapacity> lastDecisions_{};
	std::array<bool, kRuntimeActorCapacity> hasDecision_{};
	std::array<bool, kRuntimeActorCapacity> combatPending_{};
	std::array<std::uint64_t, kRuntimeActorCapacity> lastTacticalMicros_{};
	std::array<std::uint64_t, kRuntimeActorCapacity> lastActionMicros_{};
	core::team::TeamDirector team_{};
	core::team::TeamDecision teamDecision_{};
	core::experience::ExperiencePipeline experience_{};
	core::learning::ContextualDangerModel contextualDanger_{};
	core::learning::OpponentProfileModel opponentProfiles_{};
	core::MapGeneration map_{};
	core::perception::RoundGeneration round_{};
	core::TickId tick_{};
	std::uint64_t lastNowMicros_{0};
	bool active_{false};
	bool teamDecisionReady_{false};
	std::uint64_t teamDecisionMicros_{0};
};

} // namespace astrabot::adapter::metamod
