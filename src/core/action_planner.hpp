// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#pragma once

#include "core/combat.hpp"
#include "core/identity.hpp"
#include "core/perception.hpp"
#include "nav/model/value_types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace astrabot::core::action {

constexpr std::size_t kMaxWeaponPickupCandidates = 16;
constexpr std::size_t kMaxNavigationCandidates = 16;
constexpr std::size_t kMaxActionCandidates = 16;
constexpr std::uint64_t kDefaultCandidateAgeMicros = 2'000'000;
constexpr std::uint64_t kDefaultActionTimeoutMicros = 3'000'000;
constexpr std::uint64_t kDefaultPeekExposureMicros = 500'000;
constexpr std::uint64_t kDefaultObjectiveUrgencyMicros = 15'000'000;
constexpr std::int32_t kDefaultActionSwitchMargin = 8;
constexpr std::int32_t kDefaultMinimumPickupUtility = 12;

enum class ActionType : std::uint8_t {
    NoAction = 0,
    Engage,
    Reload,
    TakeCover,
    Retreat,
    Peek,
    Hold,
    FollowCurrentIntent,
    Plant,
    Defuse,
    GuardBomb,
    RecoverBomb,
    RescueHostage,
    EscortHostage,
    AcquireWeapon,
};

enum class TacticalRole : std::uint8_t {
    Entry = 0,
    Support,
    Sniper,
    Anchor,
    Escort,
    Unknown,
};

enum class Personality : std::uint8_t {
    Cautious = 0,
    Balanced,
    Aggressive,
    Supportive,
};

enum class BombState : std::uint8_t {
    None = 0,
    Carried,
    Dropped,
    Planted,
    Defused,
    Exploded,
};

enum class ObjectiveKind : std::uint8_t {
    None = 0,
    Bomb,
    Hostage,
};

struct ObjectiveSnapshot final {
    ObjectiveKind kind{ObjectiveKind::None};
    BombState bomb{BombState::None};
    std::uint64_t remainingMicros{0};
    std::uint64_t estimatedActionMicros{0};
    bool hasDefuseKit{false};
    bool canPlant{false};
    bool canDefuse{false};
    bool canGuardBomb{false};
    bool canRecoverBomb{false};
    bool canRescueHostage{false};
    bool canEscortHostage{false};

    bool valid() const noexcept;
    bool urgent(std::uint64_t thresholdMicros) const noexcept;
};

// Enemy knowledge is a World Model product. It contains only observed or
// explicitly inferred value-level facts; no entity pointer or hidden position.
struct EnemyBelief final {
    PlayerId target{};
    perception::Point lastKnownPosition{};
    std::uint64_t observedMicros{0};
    double confidence{0.0};
    bool known{false};
    bool directVision{false};
    bool pressure{false};

    bool valid(std::uint64_t nowMicros) const noexcept;
};

struct WeaponState final {
    combat::WeaponId active{};
    combat::WeaponSnapshot::WeaponClass activeClass{
        combat::WeaponSnapshot::WeaponClass::Unknown};
    std::int32_t clipAmmo{0};
    std::int32_t reserveAmmo{0};
    bool reloading{false};
    bool canReload{false};
    bool canSwitch{false};
    bool secondaryUsable{false};

    bool valid() const noexcept;
    std::int32_t totalAmmo() const noexcept;
};

struct NavigationCandidate final {
    nav::model::NavAreaId area{};
    perception::Point position{};
    double distance{0.0};
    double routeCost{0.0};
    double exposure{0.0};
    bool safe{false};
    std::uint32_t stableId{0};

    bool valid() const noexcept;
};

enum class WeaponObservationError : std::uint8_t {
    None = 0,
    InvalidReference,
    InvalidWeapon,
    InvalidIdentity,
    StaleMap,
    StaleRound,
    StaleTime,
    StaleTick,
    NonFinitePosition,
    InvalidArea,
    InvalidAmmo,
    InvalidConfidence,
};

struct DroppedWeaponObservation final {
    std::uint32_t reference{0};
    combat::WeaponId weapon{};
    combat::WeaponSnapshot::WeaponClass weaponClass{
        combat::WeaponSnapshot::WeaponClass::Unknown};
    std::int32_t estimatedClipAmmo{0};
    std::int32_t estimatedReserveAmmo{0};
    perception::Point position{};
    nav::model::NavAreaId area{};
    MapGeneration map{};
    perception::RoundGeneration round{};
    TickId observedTick{};
    std::uint64_t observedMicros{0};
    double confidence{0.0};
    bool present{true};

    WeaponObservationError validate(MapGeneration currentMap,
                                    perception::RoundGeneration currentRound,
                                    TickId currentTick,
                                    std::uint64_t nowMicros,
                                    std::uint64_t maxAgeMicros) const noexcept;
};

struct WeaponPickupCandidate final {
    DroppedWeaponObservation observation{};
    double distance{0.0};
    double routeCost{0.0};
    double routeDanger{0.0};
    double exposure{0.0};
    std::uint64_t timeRequiredMicros{0};
    bool safeRoute{false};
};

enum class WeaponAcquisitionFailure : std::uint8_t {
    None = 0,
    InvalidObservation,
    StaleObservation,
    CandidateUnavailable,
    NoRoute,
    NoTime,
    ObjectiveUrgent,
    SecondarySufficient,
    InsufficientUtility,
    CapacityExceeded,
};

struct WeaponPickupEvaluation final {
    std::uint32_t reference{0};
    combat::WeaponId candidateWeapon{};
    combat::WeaponSnapshot::WeaponClass candidateClass{
        combat::WeaponSnapshot::WeaponClass::Unknown};
    std::int32_t upgradeValue{0};
    std::int32_t ammoNeed{0};
    std::int32_t tacticalFit{0};
    std::int32_t roleFit{0};
    std::int32_t travelRisk{0};
    std::int32_t exposureRisk{0};
    std::int32_t objectiveDelay{0};
    std::int32_t swapCost{0};
    std::int32_t totalUtility{0};
    WeaponAcquisitionFailure failure{WeaponAcquisitionFailure::None};
    bool actionable{false};

    friend bool operator==(const WeaponPickupEvaluation& left,
                           const WeaponPickupEvaluation& right) noexcept;
    friend bool operator!=(const WeaponPickupEvaluation& left,
                           const WeaponPickupEvaluation& right) noexcept {
        return !(left == right);
    }
};

struct WeaponAcquisitionIntent final {
    std::uint32_t reference{0};
    combat::WeaponId weapon{};
    nav::model::NavAreaId targetArea{};
    perception::Point targetPosition{};
    double maximumRouteCost{0.0};
    std::uint64_t startedMicros{0};
};

enum class WeaponAcquisitionState : std::uint8_t {
    None = 0,
    Evaluate,
    Selected,
    Commit,
    Navigate,
    Approach,
    Acquire,
    Verify,
    Complete,
    Aborted,
};

struct WeaponAcquisitionResult final {
    WeaponAcquisitionState state{WeaponAcquisitionState::None};
    WeaponAcquisitionFailure failure{WeaponAcquisitionFailure::None};
    WeaponAcquisitionIntent intent{};
    WeaponPickupEvaluation evaluation{};
    bool terminal{false};
};

struct ActionPrecondition final {
    ActionType action{ActionType::NoAction};
    bool validSituation{false};
    bool alive{false};
    bool knownEnemy{false};
    bool directThreat{false};
    bool safeRoute{false};
    bool enoughTime{false};
    bool hasReserveAmmo{false};
    bool canReload{false};
    bool objectiveAvailable{false};
    bool candidateAvailable{false};
    bool secondarySufficient{false};
    bool satisfied{false};

    bool validFor(ActionType requested) const noexcept;
};

struct ActionScore final {
    std::int32_t base{0};
    std::int32_t threat{0};
    std::int32_t ammunition{0};
    std::int32_t objective{0};
    std::int32_t navigation{0};
    std::int32_t role{0};
    std::int32_t urgency{0};
    std::int32_t risk{0};
    std::int32_t total{0};

    void recompute() noexcept;
};

enum class ActionAbortReason : std::uint8_t {
    None = 0,
    InvalidInput,
    Dead,
    Replaced,
    TimedOut,
    EnemyAppeared,
    ThreatChanged,
    ObjectiveUrgent,
    CandidateGone,
    CandidatePickedUp,
    RouteUnsafe,
    InsufficientTime,
    BetterActionCritical,
    NoSafeRoute,
    NoTarget,
    Completed,
};

struct ActionIntent final {
    ActionType action{ActionType::NoAction};
    PlayerId target{};
    nav::model::NavAreaId targetArea{};
    perception::Point targetPosition{};
    nav::model::NavAreaId returnArea{};
    perception::Point returnPosition{};
    combat::WeaponId weapon{};
    std::uint32_t candidateReference{0};
    std::uint64_t createdMicros{0};
    std::uint64_t expiresMicros{0};
};

struct ActionDecision final {
    ActionIntent intent{};
    ActionScore score{};
    ActionPrecondition precondition{};
    WeaponPickupEvaluation pickup{};
    WeaponAcquisitionIntent acquisition{};
    ActionAbortReason transitionReason{ActionAbortReason::None};
    std::size_t evaluatedCandidates{0};
    bool hasPickupEvaluation{false};
    bool hasAcquisition{false};
    bool accepted{false};
    bool changed{false};
};

struct ActionObservation final {
    bool actionComplete{false};
    bool enemyAppeared{false};
    bool threatChanged{false};
    bool objectiveUrgent{false};
    bool candidatePresent{true};
    bool candidatePickedUp{false};
    bool routeSafe{true};
    bool enoughTime{true};
    bool betterActionCritical{false};
};

enum class ActionResultState : std::uint8_t {
    None = 0,
    Running,
    Complete,
    Aborted,
    Rejected,
};

struct ActionResult final {
    ActionResultState state{ActionResultState::None};
    ActionType action{ActionType::NoAction};
    ActionAbortReason reason{ActionAbortReason::None};
    std::uint64_t elapsedMicros{0};
    bool terminal{false};
};

struct ActionPlannerSettings final {
    std::uint64_t maxCandidateAgeMicros{kDefaultCandidateAgeMicros};
    std::uint64_t actionTimeoutMicros{kDefaultActionTimeoutMicros};
    std::uint64_t peekExposureMicros{kDefaultPeekExposureMicros};
    std::uint64_t objectiveUrgencyMicros{kDefaultObjectiveUrgencyMicros};
    std::uint32_t maxWeaponCandidates{
        static_cast<std::uint32_t>(kMaxWeaponPickupCandidates)};
    std::uint32_t maxNavigationCandidates{
        static_cast<std::uint32_t>(kMaxNavigationCandidates)};
    std::uint32_t maxEvaluations{64};
    std::int32_t actionSwitchMargin{kDefaultActionSwitchMargin};
    std::int32_t minimumPickupUtility{kDefaultMinimumPickupUtility};
    float retreatHealthPercent{30.0F};
    std::int32_t reloadClipThreshold{3};

    bool valid() const noexcept;
};

struct ActionPlannerInput final {
    MapGeneration map{};
    perception::RoundGeneration round{};
    TickId tick{};
    std::uint64_t nowMicros{0};
    PlayerId player{};
    BotAgentId agent{};
    bool alive{false};
    float healthPercent{0.0F};
    TacticalRole role{TacticalRole::Unknown};
    Personality personality{Personality::Balanced};
    std::uint32_t teammateCount{0};
    nav::model::NavAreaId currentArea{};
    perception::Point currentPosition{};
    EnemyBelief enemy{};
    WeaponState weapon{};
    ObjectiveSnapshot objective{};
    std::array<NavigationCandidate, kMaxNavigationCandidates> navigation{};
    std::size_t navigationCount{0};
    std::array<WeaponPickupCandidate, kMaxWeaponPickupCandidates> weapons{};
    std::size_t weaponCount{0};

    bool valid(const ActionPlannerSettings& settings = {}) const noexcept;
};

struct WeaponSelectionResult final {
    WeaponPickupEvaluation evaluation{};
    std::size_t candidateIndex{0};
    std::size_t evaluatedCandidates{0};
    bool found{false};
};

WeaponPickupEvaluation evaluateWeaponPickup(
    const ActionPlannerInput& input,
    const WeaponPickupCandidate& candidate,
    const ActionPlannerSettings& settings = {}) noexcept;

std::int32_t estimateCurrentWeaponUtility(
    const ActionPlannerInput& input) noexcept;

WeaponSelectionResult selectWeaponPickup(
    const ActionPlannerInput& input,
    const ActionPlannerSettings& settings = {}) noexcept;

class ActionPlanner final {
public:
    explicit ActionPlanner(ActionPlannerSettings settings = {}) noexcept
        : settings_(settings) {}

    void reset() noexcept;
    ActionDecision decide(const ActionPlannerInput& input) noexcept;
    ActionResult update(const ActionPlannerInput& input,
                        const ActionObservation& observation) noexcept;

    bool active() const noexcept { return active_; }
    const ActionIntent& currentIntent() const noexcept { return current_; }
    const ActionScore& currentScore() const noexcept { return currentScore_; }
    const ActionPlannerSettings& settings() const noexcept { return settings_; }

private:
    ActionDecision invalidDecision(const ActionPlannerInput& input,
                                   ActionAbortReason reason) noexcept;
    ActionDecision holdDecision(const ActionPlannerInput& input) noexcept;
    bool currentStillValid(const ActionPlannerInput& input) const noexcept;
    bool currentCandidatePresent(const ActionPlannerInput& input) const noexcept;
    void activate(const ActionDecision& decision) noexcept;
    void clear(ActionAbortReason reason = ActionAbortReason::None) noexcept;

    ActionPlannerSettings settings_{};
    ActionIntent current_{};
    ActionScore currentScore_{};
    ActionPrecondition currentPrecondition_{};
    std::uint64_t activeSinceMicros_{0};
    ActionAbortReason lastAbortReason_{ActionAbortReason::None};
    bool active_{false};
};

const char* actionName(ActionType action) noexcept;
const char* actionAbortReasonName(ActionAbortReason reason) noexcept;

} // namespace astrabot::core::action
