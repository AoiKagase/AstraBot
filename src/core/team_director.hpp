// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#pragma once

#include "core/identity.hpp"
#include "core/perception.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace astrabot::core::team {

constexpr std::size_t kMaxTeamMembers = perception::kPlayerCapacity;
constexpr std::size_t kMaxSharedObservations = perception::kCandidateCapacity;
constexpr std::size_t kMaxTacticalProposals = perception::kCandidateCapacity;

enum class Role : std::uint8_t {
    None = 0,
    Entry,
    Trade,
    Support,
    Lurk,
    FlankWatch,
    Anchor,
    Rotator,
    Defuser,
    Escort,
};

enum class Strategy : std::uint8_t {
    None = 0,
    AttackSplit,
    DefenseSplit,
    RetakeGroup,
    EscortPriority,
    DefusePriority,
};

enum class ObjectivePhase : std::uint8_t {
    None = 0,
    Attack,
    Defense,
    Retake,
    Escort,
    Defuse,
    Carried,
    Dropped,
    Planted,
};

enum class PlanIntent : std::uint8_t {
    None = 0,
    Attack,
    Defend,
    Rotate,
    Retake,
    Lurk,
    Support,
    Entry,
    Hold,
    Escort,
    Defuse,
};

enum class Personality : std::uint8_t {
    Balanced = 0,
    Cautious,
    Aggressive,
    Supportive,
};

enum class SharedObservationSource : std::uint8_t {
    Unknown = 0,
    DirectVision,
    TeammateReport,
};

enum class ReassignmentReason : std::uint8_t {
    None = 0,
    Initial,
    BotDied,
    BombDropped,
    DefuserDied,
    ObjectiveTransition,
    BotDisconnected,
    InputChanged,
    InvalidInput,
};

struct TeamObjective final {
    ObjectivePhase phase{ObjectivePhase::None};
    PlayerId carrier{};
    std::uint32_t site{0};
    std::uint64_t remainingMicros{0};
    bool known{false};

    bool valid() const noexcept;
};

struct TacticalPlan final {
    PlanIntent intent{PlanIntent::None};
    std::uint32_t targetArea{0};

    bool valid() const noexcept;
};

// This is an input observation, not an engine roster. Disconnected members
// remain representable so that a TeamDirector can retire their assignment.
struct TeamMemberSnapshot final {
    PlayerId player{};
    BotAgentId agent{};
    perception::Point position{};
    float healthPercent{0.0F};
    std::int32_t weaponValue{0};
    Personality personality{Personality::Balanced};
    TacticalPlan plan{};
    bool defuseCapable{false};
    bool carryingObjective{false};
    bool connected{false};
    bool alive{false};

    bool valid() const noexcept;
};

// Only explicit, value-level observations may cross the team boundary. The
// type deliberately has no engine entity handle, hidden position, or trace.
struct SharedObservation final {
    PlayerId observer{};
    PlayerId subject{};
    perception::Point position{};
    std::uint64_t observedMicros{0};
    float confidence{0.0F};
    SharedObservationSource source{SharedObservationSource::Unknown};

    bool valid(std::uint64_t nowMicros) const noexcept;
};

// A proposal is advisory and must identify the explicit evidence that made it
// shareable. The director never turns a proposal into a private certainty.
struct TacticalProposal final {
    BotAgentId proposer{};
    PlanIntent intent{PlanIntent::None};
    std::uint32_t targetArea{0};
    std::uint8_t priority{0};
    bool basedOnSharedEvidence{false};

    bool valid() const noexcept;
};

struct TeamEvents final {
    bool botDied{false};
    bool bombDropped{false};
    bool defuserDied{false};
    bool objectiveTransition{false};
    bool botDisconnected{false};

    bool any() const noexcept;
};

struct TeamSnapshot final {
    MapGeneration map{};
    perception::RoundGeneration round{};
    TickId tick{};
    std::uint64_t nowMicros{0};
    TeamObjective objective{};
    std::array<TeamMemberSnapshot, kMaxTeamMembers> members{};
    std::size_t memberCount{0};
    std::array<SharedObservation, kMaxSharedObservations> observations{};
    std::size_t observationCount{0};
    std::array<TacticalProposal, kMaxTacticalProposals> proposals{};
    std::size_t proposalCount{0};

    bool valid() const noexcept;
};

struct RoleAssignment final {
    PlayerId player{};
    BotAgentId agent{};
    Role role{Role::None};

    bool valid() const noexcept;
};

struct SharedTeamState final {
    TeamObjective objective{};
    std::array<SharedObservation, kMaxSharedObservations> observations{};
    std::size_t observationCount{0};
    std::array<TacticalProposal, kMaxTacticalProposals> proposals{};
    std::size_t proposalCount{0};
    std::array<RoleAssignment, kMaxTeamMembers> assignments{};
    std::size_t assignmentCount{0};

    bool valid(std::uint64_t nowMicros) const noexcept;
};

struct TeamDecision final {
    Strategy strategy{Strategy::None};
    SharedTeamState shared{};
    ReassignmentReason reassignment{ReassignmentReason::None};
    std::uint64_t generation{0};
    std::size_t evaluatedMembers{0};
    bool accepted{false};
    bool changed{false};
    bool reassigned{false};
};

class TeamDirector final {
public:
    TeamDecision update(const TeamSnapshot& snapshot,
                        const TeamEvents& events = {}) noexcept;
    void reset() noexcept;

    bool active() const noexcept { return active_; }
    Strategy strategy() const noexcept { return strategy_; }
    const SharedTeamState& state() const noexcept { return state_; }
    std::uint64_t generation() const noexcept { return generation_; }

private:
    static Strategy chooseStrategy(const TeamSnapshot& snapshot) noexcept;
    static Role roleForSlot(Strategy strategy, std::size_t slot) noexcept;
    static int roleScore(Role role, const TeamMemberSnapshot& member,
                         const TeamSnapshot& snapshot, double frontline) noexcept;
    static ReassignmentReason reasonFor(const TeamEvents& events) noexcept;

    Strategy strategy_{Strategy::None};
    SharedTeamState state_{};
    std::uint64_t generation_{0};
    bool active_{false};
};

const char* roleName(Role role) noexcept;
const char* strategyName(Strategy strategy) noexcept;
const char* objectivePhaseName(ObjectivePhase phase) noexcept;
const char* planIntentName(PlanIntent intent) noexcept;
const char* reassignmentReasonName(ReassignmentReason reason) noexcept;

} // namespace astrabot::core::team
