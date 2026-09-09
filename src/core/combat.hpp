// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#pragma once

#include "core/command.hpp"
#include "core/perception.hpp"
#include "core/world_model.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace astrabot::core::combat {

constexpr std::size_t kMaxOwnedWeapons = 16;
constexpr std::int32_t kMaxAmmo = 4096;
constexpr std::uint64_t kMaxReactionDelayMicros = 10'000'000;
constexpr float kMaxDifficultyErrorDegrees = 180.0F;
constexpr std::uint8_t kMaxDecisionQuality = 100;

struct WeaponId {
    std::uint16_t value{0};

    static constexpr WeaponId invalid() noexcept { return {}; }
    constexpr bool isValid() const noexcept { return value != 0; }

    friend constexpr bool operator==(WeaponId left, WeaponId right) noexcept {
        return left.value == right.value;
    }
    friend constexpr bool operator!=(WeaponId left, WeaponId right) noexcept {
        return !(left == right);
    }
    friend constexpr bool operator<(WeaponId left, WeaponId right) noexcept {
        return left.value < right.value;
    }
};

enum class WeaponValidationError : std::uint8_t {
    None = 0,
    InvalidIdentity,
    InvalidTimestamp,
    InvalidActiveWeapon,
    InvalidInventory,
    DuplicateWeapon,
    ImpossibleAmmo,
    InvalidReloadThreshold,
    InvalidWeaponClass,
};

struct WeaponValidation {
    WeaponValidationError error{WeaponValidationError::None};

    constexpr bool isValid() const noexcept {
        return error == WeaponValidationError::None;
    }
    constexpr explicit operator bool() const noexcept { return isValid(); }
};

struct WeaponSnapshot {
    MapGeneration map{};
    perception::RoundGeneration round{};
    TickId tick{};
    std::uint64_t observedMicros{0};

    WeaponId active{};
    enum class WeaponClass : std::uint8_t {
        Unknown = 0,
        Rifle,
        SMG,
        Pistol,
        Sniper,
        Shotgun,
        MachineGun,
        Melee,
    };
    WeaponClass activeClass{WeaponClass::Unknown};
    std::array<WeaponId, kMaxOwnedWeapons> owned{};
    std::size_t ownedCount{0};
    std::int32_t clipAmmo{0};
    std::int32_t reserveAmmo{0};
    // Zero means empty-only reload policy. Non-zero values request reload
    // when the active clip is at or below this configured threshold.
    std::int32_t reloadClipThreshold{0};
    bool reloading{false};
    bool canReload{false};
    bool canSwitch{false};
    std::uint64_t primaryAttackReadyMicros{0};

    WeaponValidation validate() const noexcept;
    bool owns(WeaponId weapon) const noexcept;
    bool hasAmmunition() const noexcept { return clipAmmo > 0 || reserveAmmo > 0; }

    friend bool operator==(const WeaponSnapshot& left, const WeaponSnapshot& right) noexcept;
    friend bool operator!=(const WeaponSnapshot& left, const WeaponSnapshot& right) noexcept {
        return !(left == right);
    }
};

struct DifficultySettings {
    std::uint64_t reactionDelayMicros{0};
    float observationErrorDegrees{0.0F};
    float predictionErrorDegrees{0.0F};
    float aimNoiseDegrees{0.0F};
    std::uint8_t decisionQuality{100};

    bool valid() const noexcept;
};

enum class CombatAction : std::uint8_t {
    NoOp = 0,
    Track,
    Fire,
    Reload,
    SwitchWeapon,
};

// DirectFire is the only P5 fire mode. The other values are deliberately
// representable in the value contract, but are not executable by P5.
enum class FireMode : std::uint8_t {
    DirectFire = 0,
    Wallbang,
    SuppressiveFire,
};

constexpr std::uint8_t kMaxBurstShots = 8;

// A FirePlan is carried by one Fire decision. It deliberately describes the
// cadence instead of collapsing future tap/burst/automatic behavior into a
// persistent attack boolean.
enum class FirePattern : std::uint8_t {
    Tap = 0,
    Burst,
    FullAuto,
};

struct FirePlan {
    FirePattern pattern{FirePattern::Tap};
    std::uint8_t burstShots{1}; // 0 means no fixed burst count for FullAuto.

    static constexpr FirePlan tap() noexcept { return {FirePattern::Tap, 1}; }
    static constexpr FirePlan burst(std::uint8_t shots) noexcept {
        return {FirePattern::Burst, shots};
    }
    static constexpr FirePlan fullAuto() noexcept { return {FirePattern::FullAuto, 0}; }

    bool valid() const noexcept;

    friend constexpr bool operator==(FirePlan left, FirePlan right) noexcept {
        return left.pattern == right.pattern && left.burstShots == right.burstShots;
    }
    friend constexpr bool operator!=(FirePlan left, FirePlan right) noexcept {
        return !(left == right);
    }
};

enum class CombatReason : std::uint8_t {
    None = 0,
    Accepted,
    InvalidInput,
    InvalidActor,
    InvalidMap,
    InvalidRound,
    InvalidTick,
    InvalidWorldSnapshot,
    StaleInput,
    StaleWeapon,
    NonFinitePose,
    ViewOutOfRange,
    InvalidWeapon,
    ImpossibleAmmo,
    InvalidDifficulty,
    Dead,
    NoTarget,
    UnknownRelation,
    Ally,
    StaleTarget,
    AnonymousSound,
    UnsupportedFireMode,
    NoUsableWeapon,
    Reloading,
    EmptyClip,
    Cooldown,
    ReactionDelay,
    HostRejected,
    InvalidVisibility,
    DuplicateAttack,
    DuplicateAction,
};

enum class CombatInputError : std::uint8_t {
    None = 0,
    InvalidActor,
    InvalidMap,
    InvalidRound,
    InvalidTick,
    InvalidWorldSnapshot,
    StaleWorldSnapshot,
    NonFinitePose,
    ViewOutOfRange,
    StaleWeapon,
    InvalidWeapon,
    ImpossibleAmmo,
    InvalidDifficulty,
};

struct CombatInputValidation {
    CombatInputError error{CombatInputError::None};

    constexpr bool isValid() const noexcept {
        return error == CombatInputError::None;
    }
    constexpr explicit operator bool() const noexcept { return isValid(); }
};

struct DecisionValidation {
    enum class Error : std::uint8_t {
        None = 0,
        InvalidTick,
        NonFiniteView,
        ViewOutOfRange,
        UnknownButtons,
        InvalidTarget,
        MissingFireMode,
        MissingFirePlan,
        UnexpectedFireMode,
        UnexpectedFirePlan,
        InvalidFirePlan,
        MissingAttackButton,
        MissingReloadButton,
        UnexpectedAttackButton,
        UnexpectedReloadButton,
        InvalidSelectedWeapon,
        InvalidKnowledge,
        UnsupportedFireMode,
    };

    Error error{Error::None};

    constexpr bool isValid() const noexcept { return error == Error::None; }
    constexpr explicit operator bool() const noexcept { return isValid(); }
};

struct CombatDecision {
    CombatAction action{CombatAction::NoOp};
    PlayerId target{};
    std::optional<FireMode> fireMode{};
    std::optional<FirePlan> firePlan{};
    ViewAngles view{};
    ButtonMask buttons{0};
    WeaponId selectedWeapon{};
    perception::ObservationSource source{perception::ObservationSource::Unknown};
    std::uint64_t targetAgeMicros{0};
    double confidence{0.0};
    CombatReason reason{CombatReason::None};
    TickId inputTick{};
    std::uint64_t validUntilMicros{0};

    static CombatDecision noOp(TickId tick, CombatReason reason) noexcept;
    DecisionValidation validate() const noexcept;
    DecisionValidation validateForP5() const noexcept;
    bool hasAttackInput() const noexcept;
};

struct CombatInput {
    MapGeneration map{};
    perception::RoundGeneration round{};
    TickId tick{};
    std::uint64_t timeMicros{0};
    PlayerId player{};
    BotAgentId agent{};
    bool alive{false};
    perception::Team team{perception::Team::Unknown};
    perception::Point eye{};
    ViewAngles view{};
    world::WorldSnapshot world{};
    WeaponSnapshot weapon{};
    DifficultySettings difficulty{};

    CombatInputValidation validate() const noexcept;
    CombatDecision reject() const noexcept;
};

enum class CommandCompositionError : std::uint8_t {
    None = 0,
    InvalidDecision,
    InvalidNavigationCommand,
};

struct CommandCompositionResult {
    BotCommand command{};
    CommandCompositionError error{CommandCompositionError::None};
    bool accepted{false};

    constexpr explicit operator bool() const noexcept {
        return accepted && error == CommandCompositionError::None;
    }
};

struct AttackLifecycleState {
    MapGeneration map{};
    perception::RoundGeneration round{};
    PlayerId player{};
    BotAgentId agent{};
    TickId lastFireTick{};
    std::uint64_t lastFireMicros{0};
    TickId lastActionTick{};
    CombatAction lastAction{CombatAction::NoOp};
    PlayerId cadenceTarget{};
    WeaponId cadenceWeapon{};
    FirePlan cadencePlan{};
    std::uint8_t cadenceShotsFired{0};
    std::uint64_t cadencePauseUntilMicros{0};
    PlayerId reactionTarget{};
    std::uint64_t reactionStartedMicros{0};
    bool reactionActive{false};
    bool cadenceActive{false};
    bool attackHeld{false};
    bool initialized{false};

    constexpr bool sameContext(const CombatInput& input) const noexcept {
        return initialized && map == input.map && round == input.round &&
               player == input.player && agent == input.agent;
    }
};

struct FireAuthorization {
    CombatDecision decision{};
    AttackLifecycleState nextState{};
};

// Selects at most one currently usable opponent for the bounded combat
// pipeline. P5-02 only emits Track or NoOp; it never authorizes firing.
CombatDecision selectTarget(const CombatInput& input) noexcept;

// Calculates a bounded, deterministic view toward the selected target. P5-03
// only tracks; it never emits an attack input or a fire mode.
CombatDecision aimTarget(const CombatInput& input) noexcept;

// Authorizes one deterministic combat action from a P5-03 aim decision.
// The lifecycle state is explicit so repeated input frames cannot create
// duplicate attacks, reloads, or weapon switches through hidden shared state.
FireAuthorization authorizeFire(
    const CombatInput& input,
    const CombatDecision& aim,
    AttackLifecycleState previous) noexcept;

// Compose the two owners of one host command. Navigation supplies movement,
// movement buttons, impulse, and duration; combat supplies view, attack or
// reload, and a value-level weapon selection. The navigation command's
// combat-owned fields are discarded so stale attack state cannot survive.
CommandCompositionResult composeCommand(
    const CombatDecision& combat,
    const BotCommand& navigation) noexcept;

} // namespace astrabot::core::combat
