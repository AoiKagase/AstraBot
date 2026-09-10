// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include "adapter/metamod/runtime_orchestrator.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace astrabot::adapter::metamod {
namespace {

bool sameStamp(const core::world::WorldSnapshot &snapshot, const RuntimeFrame &frame) noexcept {
	return snapshot.stamp.map == frame.map && snapshot.stamp.round == frame.round &&
	       snapshot.stamp.tick == frame.tick && snapshot.stamp.timeMicros == frame.nowMicros;
}

bool actionObservationEvent(const core::action::ActionObservation &observation) noexcept {
	return observation.actionComplete || observation.enemyAppeared || observation.threatChanged ||
	       observation.objectiveUrgent || observation.candidatePickedUp || !observation.routeSafe ||
	       !observation.enoughTime || observation.betterActionCritical;
}

bool validExperienceMap(const core::experience::MapIdentity &map) noexcept {
	return !map.name.empty() && map.valid();
}

bool validOptionalMapIdentity(const core::experience::MapIdentity &map) noexcept {
	const bool absent = map.name.empty() && map.bspBytes == 0U && !map.hasBspHash &&
	                    map.navFormatVersion == 0U && !map.hasNavHash;
	return absent || map.valid();
}

} // namespace

bool RuntimeFrame::valid() const noexcept {
	if (!map.isValid() || !round.isValid() || !tick.isValid())
		return false;
	return validOptionalMapIdentity(mapIdentity);
}

bool RuntimeActorInput::valid(const RuntimeFrame &frame) const noexcept {
	if (!frame.valid() || !player.isValid() || player.slot > core::perception::kPlayerCapacity ||
	    !agent.isValid()) {
		return false;
	}
	auto checkedTeam = team;
	if (!teamObjectiveAvailable) {
		if (tactical.objective.kind != core::tactical::ObjectiveKind::None ||
		    action.objective.kind != core::action::ObjectiveKind::None) return false;
		// Validate the roster/stamp independently without manufacturing an
		// objective observation for TeamDirector.
		checkedTeam.objective = {};
		checkedTeam.objective.known = true;
	}
	if (team.map != frame.map || team.round != frame.round || team.tick != frame.tick ||
	    team.nowMicros != frame.nowMicros || !checkedTeam.valid()) {
		return false;
	}
	if (world.stamp.map != frame.map || world.stamp.round != frame.round ||
	    world.stamp.tick != frame.tick || world.stamp.timeMicros != frame.nowMicros ||
	    !world.visual || !world.sounds || !sameStamp(world, frame)) {
		return false;
	}
	if (action.map != frame.map || action.round != frame.round || action.tick != frame.tick ||
	    action.nowMicros != frame.nowMicros || action.player != player || action.agent != agent ||
	    !action.valid()) {
		return false;
	}
	if (combat.map != frame.map || combat.round != frame.round || combat.tick != frame.tick ||
	    combat.timeMicros != frame.nowMicros || combat.player != player || combat.agent != agent ||
	    !combat.validate()) {
		return false;
	}
	if (contextualDanger && contextualDanger->map != frame.map) {
		return false;
	}
	if (opponent && (opponent->map != frame.map || opponent->round != frame.round ||
	                 opponent->tick != frame.tick)) {
		return false;
	}
	const auto context = core::tactical::buildTacticalContext(world, tactical);
	if (context.map != frame.map || context.round != frame.round || context.tick != frame.tick ||
	    context.nowMicros != frame.nowMicros || context.self.player != player ||
	    context.self.agent != agent || !context.valid()) {
		return false;
	}
	if (experienceEventCount > experienceEvents.size())
		return false;
	for (std::size_t i = 0; i < experienceEventCount; ++i) {
		const auto &event = experienceEvents[i];
		if (!event.valid() || !validExperienceMap(frame.mapIdentity) ||
		    event.map != frame.mapIdentity || event.round != frame.round ||
		    event.tick.value > frame.tick.value || event.timeMicros > frame.nowMicros) {
			return false;
		}
	}
	if (contextualDanger) {
		if (!contextualDanger->valid() || contextualDanger->map != frame.map ||
		    contextualDanger->timeMicros > frame.nowMicros) {
			return false;
		}
	}
	if (opponent) {
		if (!opponent->valid() || opponent->map != frame.map || opponent->round != frame.round ||
		    opponent->tick.value > frame.tick.value) {
			return false;
		}
	}
	return true;
}

std::size_t RuntimeOrchestrator::slotIndex(core::PlayerId player) noexcept {
	if (!player.isValid() || player.slot == 0 || player.slot > kRuntimeActorCapacity) {
		return kRuntimeActorCapacity;
	}
	return static_cast<std::size_t>(player.slot - 1U);
}

void RuntimeOrchestrator::appendStage(RuntimeStage stage) noexcept {
	if (result_.stageCount >= result_.stageTrace.size())
		return;
	if (result_.stageCount != 0 && result_.stageTrace[result_.stageCount - 1U] == stage) {
		return;
	}
	result_.stageTrace[result_.stageCount++] = stage;
}

void RuntimeOrchestrator::addDiagnostic(RuntimeStage stage, RuntimeRejectReason reason,
                                        const RuntimeFrame &frame, core::PlayerId player,
                                        core::BotAgentId agent) noexcept {
	RuntimeDiagnostic value{};
	value.stage = stage;
	value.reason = reason;
	value.map = frame.map;
	value.round = frame.round;
	value.tick = frame.tick;
	value.player = player;
	value.agent = agent;
	if (diagnostics_.count < diagnostics_.entries.size()) {
		diagnostics_.entries[diagnostics_.count++] = value;
		return;
	}
	diagnostics_.entries[diagnostics_.dropped % diagnostics_.entries.size()] = value;
	++diagnostics_.dropped;
}

void RuntimeOrchestrator::clearSlot(std::size_t index) noexcept {
	if (index >= kRuntimeActorCapacity)
		return;
	tactical_[index].reset();
	tactical_[index].emplace();
	action_[index].reset();
	action_[index].emplace();
	combat_[index] = {};
	lastDecisions_[index] = {};
	hasDecision_[index] = false;
	combatPending_[index] = false;
	lastTacticalMicros_[index] = 0;
	lastActionMicros_[index] = 0;
	actorIdentity_[index] = {};
}

void RuntimeOrchestrator::resetPlanners() noexcept {
	team_.reset();
	teamDecision_ = {};
	teamDecisionReady_ = false;
	teamDecisionMicros_ = 0;
	actorIdentity_.fill({});
	for (std::size_t i = 0; i < kRuntimeActorCapacity; ++i)
		clearSlot(i);
}

bool RuntimeOrchestrator::beginFrameContext(const RuntimeFrame &frame) noexcept {
	if (!frame.valid())
		return false;

	if (!active_) {
		resetPlanners();
		map_ = frame.map;
		round_ = frame.round;
		tick_ = {};
		lastNowMicros_ = frame.nowMicros;
		if (contextualDanger_.map() != frame.map) {
			contextualDanger_.reset();
			opponentProfiles_.reset();
			if (!contextualDanger_.beginMap(frame.map) || !opponentProfiles_.beginMap(frame.map)) {
				return false;
			}
		}
		if (!opponentProfiles_.beginRound(frame.round)) {
			return false;
		}
		experience_.reset();
		if (validExperienceMap(frame.mapIdentity) &&
		    !experience_.activate(frame.mapIdentity, frame.round)) {
			return false;
		}
		active_ = true;
		tick_ = frame.tick;
		return true;
	}

	const bool mapChanged = frame.map != map_;
	const bool roundChanged = frame.round != round_;
	if ((!mapChanged && frame.map.value < map_.value) ||
	    (mapChanged && frame.map.value < map_.value) ||
	    (!mapChanged && frame.round.value < round_.value) ||
	    (!mapChanged && !roundChanged && frame.tick.value <= tick_.value) ||
	    (!mapChanged && frame.nowMicros < lastNowMicros_)) {
		return false;
	}
	if (!mapChanged && experience_.active() && validExperienceMap(frame.mapIdentity) &&
	    frame.mapIdentity != experience_.map()) {
		return false;
	}

	if (mapChanged) {
		resetPlanners();
		contextualDanger_.reset();
		opponentProfiles_.reset();
		experience_.reset();
		if (!contextualDanger_.beginMap(frame.map) || !opponentProfiles_.beginMap(frame.map)) {
            return false;
        }
        if (!opponentProfiles_.beginRound(frame.round)) {
            return false;
        }
        if (validExperienceMap(frame.mapIdentity) &&
		    !experience_.activate(frame.mapIdentity, frame.round)) {
			return false;
		}
	} else if (roundChanged) {
		resetPlanners();
		if (experience_.active()) {
			const auto update = experience_.beginRound(frame.round, frame.nowMicros);
			if (update.reason != core::experience::ExperienceUpdateReason::Accepted) {
				return false;
			}
		}
		if (!opponentProfiles_.beginRound(frame.round))
			return false;
	}

	map_ = frame.map;
	round_ = frame.round;
	tick_ = frame.tick;
	lastNowMicros_ = frame.nowMicros;
	return true;
}

core::action::TacticalRole RuntimeOrchestrator::roleFor(core::team::Role role) noexcept {
	switch (role) {
	case core::team::Role::Entry:
		return core::action::TacticalRole::Entry;
	case core::team::Role::Support:
		return core::action::TacticalRole::Support;
	case core::team::Role::Anchor:
		return core::action::TacticalRole::Anchor;
	case core::team::Role::FlankWatch:
	case core::team::Role::Lurk:
		return core::action::TacticalRole::Sniper;
	case core::team::Role::Escort:
		return core::action::TacticalRole::Escort;
	case core::team::Role::Trade:
		return core::action::TacticalRole::Entry;
	case core::team::Role::Rotator:
	case core::team::Role::Defuser:
	case core::team::Role::None:
		return core::action::TacticalRole::Unknown;
	}
	return core::action::TacticalRole::Unknown;
}

const core::team::RoleAssignment *
RuntimeOrchestrator::assignmentFor(core::PlayerId player) const noexcept {
	const auto &state = team_.state();
	const auto limit = (std::min)(state.assignmentCount, state.assignments.size());
	for (std::size_t i = 0; i < limit; ++i) {
		if (state.assignments[i].player == player)
			return &state.assignments[i];
	}
	return nullptr;
}

const RuntimeFrameResult &RuntimeOrchestrator::run(const RuntimeFrame &frame,
                                                   const RuntimeActorInput *inputs,
                                                   std::size_t inputCount) noexcept {
	result_ = {};
	// A combat value belongs to exactly one run/tick. Any value not consumed
	// by that tick's navigation pass is discarded before the next frame,
	// including stale-frame and generation-mismatch paths.
	combatPending_ = {};
	if (!beginFrameContext(frame)) {
		addDiagnostic(RuntimeStage::None,
		              frame.valid() ? RuntimeRejectReason::StaleFrame
		                            : RuntimeRejectReason::InvalidFrame,
		              frame, {});
		return result_;
	}
	result_.accepted = true;
	appendStage(RuntimeStage::PerceptionPublished);

	std::array<const RuntimeActorInput *, kRuntimeActorCapacity> ordered{};
	std::array<bool, kRuntimeActorCapacity> seen{};
	const auto limit = (std::min)(inputCount, kRuntimeActorCapacity);
	if (inputs == nullptr && limit != 0) {
		addDiagnostic(RuntimeStage::PerceptionPublished, RuntimeRejectReason::InvalidActorInput,
		              frame, {});
		result_.accepted = false;
		return result_;
	}
	if (inputCount > kRuntimeActorCapacity) {
		addDiagnostic(RuntimeStage::PerceptionPublished, RuntimeRejectReason::InvalidActorInput,
		              frame, {});
	}
	auto appendRejected = [&](const RuntimeActorInput &input, RuntimeRejectReason reason) noexcept {
		if (reason == RuntimeRejectReason::NonPrimaryActor)
			++result_.nonPrimaryRejectedCount;
		addDiagnostic(RuntimeStage::PerceptionPublished, reason, frame, input.player, input.agent);
		if (result_.decisionCount >= result_.decisions.size())
			return;
		auto &decision = result_.decisions[result_.decisionCount++];
		decision.player = input.player;
		decision.agent = input.agent;
		decision.rejection = reason;
	};
	for (std::size_t i = 0; i < limit; ++i) {
		const auto &input = inputs[i];
		const auto index = slotIndex(input.player);
		if (index >= kRuntimeActorCapacity) {
			appendRejected(input, RuntimeRejectReason::InvalidActorInput);
			continue;
		}
		bool duplicateAgent = false;
		if (input.agent.isValid()) {
			for (const auto *existing : ordered) {
				if (existing != nullptr && existing->agent == input.agent) {
					duplicateAgent = true;
					break;
				}
			}
		}
		if (seen[index] || duplicateAgent) {
			appendRejected(input, RuntimeRejectReason::DuplicateActor);
			continue;
		}
		// All valid managed actors are accepted. The primary flag is retained
		// for older providers and is not an execution gate.
	seen[index] = true;
	if (actorIdentity_[index].isValid() && actorIdentity_[index] != input.player)
		clearSlot(index);
		if (!input.valid(frame)) {
			if (!actorIdentity_[index].isValid() || actorIdentity_[index] == input.player)
				clearSlot(index);
			appendRejected(input, RuntimeRejectReason::InvalidActorInput);
			continue;
		}
		ordered[index] = &input;
		++result_.acceptedActorCount;
		actorIdentity_[index] = input.player;
	}

	appendStage(RuntimeStage::ExperienceUpdated);
	for (std::size_t index = 0; index < ordered.size(); ++index) {
		const auto *input = ordered[index];
		if (!input)
			continue;
		for (std::size_t i = 0; i < input->experienceEventCount; ++i) {
			if (!experience_.active())
				break;
			try {
				(void)experience_.submit(input->experienceEvents[i]);
			} catch (...) {
				addDiagnostic(RuntimeStage::ExperienceUpdated,
				              RuntimeRejectReason::InvalidActorInput, frame, input->player,
				              input->agent);
				break;
			}
		}
		if (input->contextualDanger) {
			(void)contextualDanger_.observe(*input->contextualDanger);
		}
		if (input->opponent)
			(void)opponentProfiles_.observe(*input->opponent);
	}

	const RuntimeActorInput *teamInput = nullptr;
	for (const auto *input : ordered) {
		if (input) {
			teamInput = input;
			break;
		}
	}
	appendStage(RuntimeStage::TeamDirector);
	bool teamExecuted = false;
	if (teamInput && !teamInput->teamObjectiveAvailable) {
		// Unknown objective input is a neutral team decision, not a reason to
		// erase each actor's tactical planner or Roam history.
		team_.reset();
		teamDecision_ = {};
		teamDecision_.shared.map = frame.map;
		teamDecision_.shared.round = frame.round;
		teamDecision_.shared.tick = frame.tick;
		teamDecision_.shared.nowMicros = frame.nowMicros;
		teamDecision_.accepted = true;
		teamDecisionReady_ = true;
		teamDecisionMicros_ = 0;
	} else if (teamInput) {
		const bool eventDriven = teamInput->teamEvents.any();
		const auto maximum = (std::numeric_limits<std::uint64_t>::max)();
		const bool cadence =
		    !teamDecisionReady_ || team_.strategy() == core::team::Strategy::None ||
		    frame.nowMicros >= (teamDecisionMicros_ > maximum - kTeamDirectorCadenceMicros
		                            ? maximum
		                            : teamDecisionMicros_ + kTeamDirectorCadenceMicros);
		if (cadence || eventDriven) {
			teamExecuted = true;
			teamDecision_ = team_.update(teamInput->team, teamInput->teamEvents);
			if (teamDecision_.accepted) {
				teamDecisionReady_ = true;
				teamDecisionMicros_ = frame.nowMicros;
			} else {
				addDiagnostic(RuntimeStage::TeamDirector, RuntimeRejectReason::InvalidTeamInput,
				              frame, teamInput->player, teamInput->agent);
			}
		}
	}

	appendStage(RuntimeStage::TacticalPlanner);
	appendStage(RuntimeStage::ActionPlanner);
	appendStage(RuntimeStage::Combat);
	appendStage(RuntimeStage::Navigation);
	for (std::size_t index = 0; index < ordered.size(); ++index) {
		const auto *input = ordered[index];
		if (!input)
			continue;
		if (!teamDecisionReady_) {
			clearSlot(index);
			if (result_.decisionCount >= result_.decisions.size())
				continue;
			auto &decision = result_.decisions[result_.decisionCount++];
			decision.player = input->player;
			decision.agent = input->agent;
			decision.rejection = RuntimeRejectReason::InvalidTeamInput;
			addDiagnostic(RuntimeStage::TeamDirector, RuntimeRejectReason::InvalidTeamInput, frame,
			              input->player, input->agent);
			continue;
		}

        RuntimeDecision decision{};
        decision.player = input->player;
        decision.agent = input->agent;
        decision.team = teamDecision_;
        // Team strategy is cadence-cached, but the value delivered to NAV
        // belongs to this input frame. Preserve the cached objective and
        // assignments while stamping the transport identity for this tick.
        decision.team.shared.map = frame.map;
        decision.team.shared.round = frame.round;
        decision.team.shared.tick = frame.tick;
		decision.team.shared.nowMicros = frame.nowMicros;
        decision.teamExecuted = teamExecuted;
		auto tacticalSeed = input->tactical;
		if (const auto *assignment = assignmentFor(input->player)) {
			if (tacticalSeed.self.role == core::tactical::RolePreference::Any) {
				switch (assignment->role) {
				case core::team::Role::Entry:
					tacticalSeed.self.role = core::tactical::RolePreference::Entry;
					break;
				case core::team::Role::Support:
					tacticalSeed.self.role = core::tactical::RolePreference::Support;
					break;
				case core::team::Role::Anchor:
					tacticalSeed.self.role = core::tactical::RolePreference::Anchor;
					break;
				case core::team::Role::Escort:
					tacticalSeed.self.role = core::tactical::RolePreference::Escort;
					break;
				default:
					break;
				}
			}
		}
		const auto context = core::tactical::buildTacticalContext(input->world, tacticalSeed);
		decision.roamCandidateCount = context.navigation.roamCandidateCount;
		decision.roamGeneration = context.navigation.roamGeneration;
		if (!context.valid()) {
			clearSlot(index);
			decision.rejection = RuntimeRejectReason::InvalidTacticalInput;
			addDiagnostic(RuntimeStage::TacticalPlanner, decision.rejection, frame, input->player,
			              input->agent);
			if (result_.decisionCount < result_.decisions.size())
				result_.decisions[result_.decisionCount++] = decision;
			continue;
		}

		const auto maximum = (std::numeric_limits<std::uint64_t>::max)();
		const bool tacticalDue =
		    !hasDecision_[index] || input->tacticalEvents.any() ||
		    tactical_[index]->needsReplan(context, input->tacticalEvents) ||
		    frame.nowMicros >= (lastTacticalMicros_[index] > maximum - kTacticalPlannerCadenceMicros
		                            ? maximum
		                            : lastTacticalMicros_[index] + kTacticalPlannerCadenceMicros);
		if (tacticalDue) {
			decision.tactical = tactical_[index]->plan(context, input->tacticalEvents);
			decision.tacticalExecuted = true;
			lastTacticalMicros_[index] = frame.nowMicros;
		} else {
			decision.tactical = lastDecisions_[index].tactical;
		}
		if (!decision.tactical.accepted) {
			decision.rejection = RuntimeRejectReason::PlannerRejected;
			addDiagnostic(RuntimeStage::TacticalPlanner, decision.rejection, frame, input->player,
			              input->agent);
		}

		auto actionInput = input->action;
		if (const auto *assignment = assignmentFor(input->player)) {
			if (actionInput.role == core::action::TacticalRole::Unknown)
				actionInput.role = roleFor(assignment->role);
		}
		const bool actionDue =
		    !hasDecision_[index] || actionObservationEvent(input->actionObservation) ||
		    frame.nowMicros >= (lastActionMicros_[index] > maximum - kActionPlannerCadenceMicros
		                            ? maximum
		                            : lastActionMicros_[index] + kActionPlannerCadenceMicros);
		if (actionDue) {
			if (action_[index]->active())
				(void)action_[index]->update(actionInput, input->actionObservation);
			decision.action = action_[index]->decide(actionInput);
			decision.actionExecuted = true;
			lastActionMicros_[index] = frame.nowMicros;
		} else {
			decision.action = lastDecisions_[index].action;
		}
		if (!decision.action.accepted && decision.rejection == RuntimeRejectReason::None) {
			decision.rejection = RuntimeRejectReason::PlannerRejected;
			addDiagnostic(RuntimeStage::ActionPlanner, decision.rejection, frame, input->player,
			              input->agent);
		}

		const auto aim = core::combat::aimTarget(input->combat);
		const auto authorization = core::combat::authorizeFire(input->combat, aim, combat_[index]);
		combat_[index] = authorization.nextState;
		decision.combat = authorization.decision;
		decision.combatExecuted = true;
		if (!decision.combat.validateForP5()) {
			decision.rejection = RuntimeRejectReason::InvalidCombatInput;
			addDiagnostic(RuntimeStage::Combat, decision.rejection, frame, input->player,
			              input->agent);
		}
		if (decision.action.intent.targetArea.isValid()) {
			decision.navigationGoal = decision.action.intent.targetArea;
			decision.hasNavigationGoal = true;
		} else if (decision.tactical.intent.target.area.isValid() &&
		           !(decision.tactical.intent.type == core::tactical::IntentType::Hold &&
		             decision.tactical.intent.reason == core::tactical::Reason::HoldCurrentArea &&
		             decision.tactical.intent.target.area == input->tactical.self.currentArea)) {
			decision.navigationGoal = decision.tactical.intent.target.area;
			decision.hasNavigationGoal = true;
		}
		decision.executable = decision.rejection == RuntimeRejectReason::None &&
		                      decision.tactical.accepted && decision.action.accepted &&
		                      decision.combat.validateForP5();
		combatPending_[index] = decision.executable;
		if (combatPending_[index])
			++result_.queuedCombatCount;
		if (decision.executable)
			++result_.executableCount;

		lastDecisions_[index] = decision;
		hasDecision_[index] = decision.tactical.accepted && decision.action.accepted;
		if (result_.decisionCount < result_.decisions.size())
			result_.decisions[result_.decisionCount++] = decision;
	}
	return result_;
}

void RuntimeOrchestrator::reset() noexcept {
	result_ = {};
	diagnostics_ = {};
	resetPlanners();
	experience_.reset();
	contextualDanger_.reset();
	opponentProfiles_.reset();
	map_ = {};
	round_ = {};
	tick_ = {};
	lastNowMicros_ = 0;
	active_ = false;
}

void RuntimeOrchestrator::beginMap(core::MapGeneration map) noexcept {
	reset();
	if (!map.isValid() || !contextualDanger_.beginMap(map) || !opponentProfiles_.beginMap(map)) {
		reset();
		return;
	}
	map_ = map;
}

void RuntimeOrchestrator::onDisconnect(core::PlayerId player) noexcept {
	const auto index = slotIndex(player);
	if (index < kRuntimeActorCapacity &&
	    (!actorIdentity_[index].isValid() || actorIdentity_[index] == player))
		clearSlot(index);
	opponentProfiles_.forget(player);
}

void RuntimeOrchestrator::onDeath(core::PlayerId player) noexcept {
	const auto index = slotIndex(player);
	if (index < kRuntimeActorCapacity &&
	    (!actorIdentity_[index].isValid() || actorIdentity_[index] == player))
		clearSlot(index);
}

void RuntimeOrchestrator::onInputUnavailable(core::PlayerId player) noexcept {
	const auto index = slotIndex(player);
	if (index < kRuntimeActorCapacity &&
	    (!actorIdentity_[index].isValid() || actorIdentity_[index] == player))
		clearSlot(index);
}

std::optional<core::combat::CombatDecision> RuntimeOrchestrator::takeCombatDecision(
    core::PlayerId player, core::BotAgentId agent, core::MapGeneration map,
    core::perception::RoundGeneration round, core::TickId tick) noexcept {
	const auto index = slotIndex(player);
	if (!active_ || index >= kRuntimeActorCapacity || !combatPending_[index] || map != map_ ||
	    round != round_ || tick != tick_ || lastDecisions_[index].player != player ||
	    lastDecisions_[index].agent != agent || !lastDecisions_[index].combat.validateForP5()) {
		return std::nullopt;
	}
	combatPending_[index] = false;
	return lastDecisions_[index].combat;
}

} // namespace astrabot::adapter::metamod
