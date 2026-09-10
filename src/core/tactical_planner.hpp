// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#pragma once

#include "core/identity.hpp"
#include "core/perception.hpp"
#include "core/world_model.hpp"
#include "nav/model/value_types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace astrabot::core::tactical {

constexpr std::size_t kMaxTacticalRoutes = 16;
constexpr std::size_t kMaxTacticalRoamCandidates = 16;
constexpr std::size_t kTacticalRoamHistory = 4;
constexpr std::size_t kMaxTacticalTeammates = perception::kPlayerCapacity - 1;
constexpr std::size_t kMaxTacticalEnemies = perception::kCandidateCapacity;
constexpr std::uint64_t kDefaultReplanIntervalMicros = 1'000'000;
constexpr std::uint64_t kDefaultIntentHorizonMicros = 10'000'000;
constexpr std::uint64_t kDefaultSaveTimeMicros = 8'000'000;
constexpr float kDefaultSaveHealthPercent = 25.0F;
constexpr std::int32_t kDefaultValuableWeapon = 40;

enum class IntentType : std::uint8_t {
    None = 0,
    AttackSite,
    DefendSite,
    Rotate,
    Retake,
    Save,
    Flank,
    Lurk,
    Support,
    Entry,
    Trade,
    Hold,
    Escort,
    Roam,
};

enum class RouteStyle : std::uint8_t {
    None = 0,
    Direct,
    Safe,
    Fast,
    Rotate,
    Retake,
    Flank,
    Lurk,
    Support,
    Escort,
    Roam,
    Hold,
};

enum class RolePreference : std::uint8_t {
    Any = 0,
    Entry,
    Support,
    Anchor,
    Sniper,
    Escort,
};

enum class Urgency : std::uint8_t {
    None = 0,
    Low,
    Normal,
    High,
    Critical,
};

enum class Reason : std::uint8_t {
    None = 0,
    Initial,
    AttackObjective,
    DefendObjective,
    HoldCurrentArea,
    EnemyConcentration,
    EnemySighting,
    BombPlanted,
    BombDropped,
    RetakeUnavailable,
    SaveWeapon,
    EntryLost,
    TeammateDeath,
    ObjectiveTransition,
    RouteBlocked,
    IntentInvalidated,
    AutonomousRoam,
    Periodic,
    InvalidContext,
};

enum class Validity : std::uint8_t {
    Invalid = 0,
    Valid,
    Expired,
    Superseded,
    Unreachable,
    StaleContext,
};

struct TargetArea final {
    nav::model::NavAreaId area{};
    perception::Point position{};
    std::uint32_t stableId{0};

    bool valid() const noexcept;
};

struct TacticalRoute final {
    TargetArea target{};
    RouteStyle style{RouteStyle::None};
    std::uint64_t etaMicros{0};
    double danger{0.0};
    bool available{false};

    bool valid() const noexcept;
};

struct SelfState final {
    PlayerId player{};
    BotAgentId agent{};
    perception::Team team{perception::Team::Unknown};
    RolePreference role{RolePreference::Any};
    perception::Point position{};
    nav::model::NavAreaId currentArea{};
    float healthPercent{0.0F};
    std::int32_t weaponValue{0};
    bool alive{false};

    bool valid() const noexcept;
};

struct TeammateState final {
    PlayerId player{};
    RolePreference role{RolePreference::Any};
    nav::model::NavAreaId area{};
    bool alive{false};
    bool objectiveCarrier{false};

    bool valid() const noexcept;
};

// This is deliberately a value-level belief. It may only contain locations
// obtained from World Model observations or reports, never engine truth.
struct EnemyBelief final {
    PlayerId target{};
    nav::model::NavAreaId area{};
    perception::Point position{};
    double confidence{0.0};
    std::uint64_t observedAgeMicros{0};
    bool confirmed{false};
    bool directVision{false};

    bool valid(std::uint64_t nowMicros,
               std::uint64_t maxAgeMicros) const noexcept;
};

enum class ObjectiveKind : std::uint8_t {
    None = 0,
    Bomb,
    Hostage,
};

enum class BombState : std::uint8_t {
    None = 0,
    Carried,
    Dropped,
    Planted,
    Defused,
    Exploded,
};

struct ObjectiveState final {
    ObjectiveKind kind{ObjectiveKind::None};
    BombState bomb{BombState::None};
    TargetArea target{};
    std::uint64_t remainingMicros{0};
    std::uint64_t retakeTimeMicros{0};
    bool canAttack{false};
    bool canDefend{false};
    bool retakeFeasible{false};
    bool canEscort{false};

    bool valid() const noexcept;
};

struct EconomySummary final {
    bool available{false};
    std::int32_t weaponValue{0};
    bool lowFunds{false};

    bool valid() const noexcept;
};

struct NavigationState final {
    std::array<TacticalRoute, kMaxTacticalRoutes> routes{};
    std::size_t routeCount{0};
    TacticalRoute explicitRoute{};
    bool explicitRouteAvailable{false};
    std::array<TargetArea, kMaxTacticalRoamCandidates> roamCandidates{};
    std::size_t roamCandidateCount{0};
    std::uint64_t roamGeneration{0};

    bool valid() const noexcept;
};

struct TacticalContext final {
    MapGeneration map{};
    perception::RoundGeneration round{};
    TickId tick{};
    std::uint64_t nowMicros{0};
    SelfState self{};
    std::array<TeammateState, kMaxTacticalTeammates> teammates{};
    std::size_t teammateCount{0};
    std::array<EnemyBelief, kMaxTacticalEnemies> enemies{};
    std::size_t enemyCount{0};
    ObjectiveState objective{};
    EconomySummary economy{};
    NavigationState navigation{};

    bool valid() const noexcept;
};

// The adapter/host supplies only current self/objective/navigation values.
// buildTacticalContext adds roster and observed beliefs from World Model.
struct TacticalContextSeed final {
    SelfState self{};
    ObjectiveState objective{};
    EconomySummary economy{};
    NavigationState navigation{};
};

TacticalContext buildTacticalContext(const world::WorldSnapshot& snapshot,
                                     const TacticalContextSeed& seed) noexcept;

enum class ReplanTrigger : std::uint8_t {
    None = 0,
    Periodic,
    EnemySighting,
    BombPlanted,
    BombDropped,
    TeammateDeath,
    ObjectiveTransition,
    RouteBlocked,
    IntentInvalidated,
};

struct ReplanEvents final {
    bool enemySighting{false};
    bool bombPlanted{false};
    bool bombDropped{false};
    bool teammateDeath{false};
    bool entryPlayerDied{false};
    bool objectiveTransition{false};
    bool routeBlocked{false};
    bool intentInvalidated{false};

    bool any() const noexcept;
};

struct TacticalPlannerSettings final {
    std::uint64_t replanIntervalMicros{kDefaultReplanIntervalMicros};
    std::uint64_t intentHorizonMicros{kDefaultIntentHorizonMicros};
    std::uint64_t saveTimeMicros{kDefaultSaveTimeMicros};
    std::uint64_t enemyFreshnessMicros{5'000'000};
    float saveHealthPercent{kDefaultSaveHealthPercent};
    std::int32_t valuableWeaponValue{kDefaultValuableWeapon};
    std::size_t maxEnemies{kMaxTacticalEnemies};
    std::size_t maxTeammates{kMaxTacticalTeammates};

    bool valid() const noexcept;
};

struct TacticalIntent final {
    IntentType type{IntentType::None};
    TargetArea target{};
    RouteStyle route{RouteStyle::None};
    RolePreference role{RolePreference::Any};
    PlayerId targetPlayer{};
    Urgency urgency{Urgency::None};
    Reason reason{Reason::None};
    Validity validity{Validity::Invalid};
    std::uint64_t createdMicros{0};
    std::uint64_t expiresMicros{0};
    std::uint64_t generation{0};
};

struct TacticalDecision final {
    TacticalIntent intent{};
    ReplanTrigger trigger{ReplanTrigger::None};
    std::size_t evaluatedEnemies{0};
    std::size_t evaluatedTeammates{0};
    bool accepted{false};
    bool changed{false};
    bool replanned{false};
};

class TacticalPlanner final {
public:
    explicit TacticalPlanner(TacticalPlannerSettings settings = {}) noexcept
        : settings_(settings) {}

    void reset() noexcept;
    bool needsReplan(const TacticalContext& context,
                     const ReplanEvents& events = {}) const noexcept;
    TacticalDecision plan(const TacticalContext& context,
                          const ReplanEvents& events = {}) noexcept;

    bool active() const noexcept { return active_; }
    const TacticalIntent& currentIntent() const noexcept { return current_; }
    const TacticalPlannerSettings& settings() const noexcept { return settings_; }

private:
    TacticalDecision invalidDecision(const TacticalContext& context,
                                     Reason reason) noexcept;
    TacticalIntent choose(const TacticalContext& context,
                          ReplanTrigger trigger) const noexcept;
    bool currentStillValid(const TacticalContext& context) const noexcept;
    void activate(TacticalIntent intent) noexcept;
    bool wasRecentRoam(const TargetArea& target) const noexcept;
    void rememberRoam(const TargetArea& target) noexcept;

    TacticalPlannerSettings settings_{};
    TacticalIntent current_{};
    std::uint64_t lastPlanMicros_{0};
    std::uint64_t generation_{0};
    std::array<TargetArea, kTacticalRoamHistory> recentRoam_{};
    std::size_t recentRoamCount_{0};
    bool active_{false};
};

const char* intentName(IntentType intent) noexcept;
const char* routeStyleName(RouteStyle route) noexcept;
const char* reasonName(Reason reason) noexcept;
const char* validityName(Validity validity) noexcept;

} // namespace astrabot::core::tactical
