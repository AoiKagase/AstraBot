// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#pragma once

#include "core/action_planner.hpp"
#include "core/combat.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace astrabot::core::learning {

constexpr std::size_t kMaxContextualDangerEntries = 256;
constexpr std::size_t kMaxOpponentProfiles = 32;

enum class ApproachDirection : std::uint8_t {
    Unknown = 0,
    North,
    East,
    South,
    West,
    Up,
    Down,
};

struct ContextualDangerKey final {
    std::uint32_t area{0};
    perception::Team team{perception::Team::Unknown};
    ApproachDirection approach{ApproachDirection::Unknown};
    combat::WeaponSnapshot::WeaponClass enemyWeaponClass{
        combat::WeaponSnapshot::WeaponClass::Unknown};
    std::uint32_t likelyEnemyArea{0};

    bool valid() const noexcept;
    friend bool operator<(const ContextualDangerKey& left,
                          const ContextualDangerKey& right) noexcept;
};

struct ContextualDangerObservation final {
    ContextualDangerKey key{};
    MapGeneration map{};
    double risk{0.0};
    double weight{1.0};
    std::uint64_t timeMicros{0};

    bool valid() const noexcept;
};

struct ContextualDangerRecord final {
    ContextualDangerKey key{};
    double risk{0.0};
    double observations{0.0};
    std::uint64_t lastTimeMicros{0};

    bool valid() const noexcept;
};

enum class ContextualDangerUpdateReason : std::uint8_t {
    None = 0,
    Accepted,
    InvalidObservation,
    WrongMap,
    StaleObservation,
    CapacityExceeded,
};

struct ContextualDangerUpdate final {
    ContextualDangerUpdateReason reason{ContextualDangerUpdateReason::None};
    bool changed{false};

    constexpr bool accepted() const noexcept {
        return reason == ContextualDangerUpdateReason::Accepted;
    }
};

class ContextualDangerModel final {
public:
    // Contextual danger is tactical state for one active map session. It is
    // deliberately separate from the map-identified persistent experience.
    bool beginMap(MapGeneration map) noexcept;
    void reset() noexcept;
    ContextualDangerUpdate observe(const ContextualDangerObservation& observation) noexcept;
    double risk(const ContextualDangerKey& key) const noexcept;
    const ContextualDangerRecord* find(const ContextualDangerKey& key) const noexcept;
    std::size_t size() const noexcept { return records_.size(); }
    MapGeneration map() const noexcept { return map_; }
    const std::vector<ContextualDangerRecord>& records() const noexcept { return records_; }

private:
    std::vector<ContextualDangerRecord> records_{};
    MapGeneration map_{};
};

struct OpponentObservation final {
    PlayerId player{};
    MapGeneration map{};
    perception::RoundGeneration round{};
    TickId tick{};
    std::uint32_t area{0};
    combat::WeaponSnapshot::WeaponClass weapon{
        combat::WeaponSnapshot::WeaponClass::Unknown};
    double aggression{0.0};
    double rushProbability{0.0};
    double campProbability{0.0};
    double rotationSpeed{0.0};

    bool valid() const noexcept;
};

struct OpponentProfile final {
    PlayerId player{};
    MapGeneration map{};
    double aggression{0.0};
    double rushProbability{0.0};
    double campProbability{0.0};
    std::uint32_t preferredArea{0};
    combat::WeaponSnapshot::WeaponClass preferredWeapon{
        combat::WeaponSnapshot::WeaponClass::Unknown};
    double rotationSpeed{0.0};
    std::uint32_t observations{0};

    bool valid() const noexcept;
};

enum class OpponentProfileUpdateReason : std::uint8_t {
    None = 0,
    Accepted,
    InvalidObservation,
    WrongMap,
    WrongRound,
    RetiredGeneration,
    CapacityExceeded,
};

struct OpponentProfileUpdate final {
    OpponentProfileUpdateReason reason{OpponentProfileUpdateReason::None};
    bool changed{false};

    constexpr bool accepted() const noexcept {
        return reason == OpponentProfileUpdateReason::Accepted;
    }
};

class OpponentProfileModel final {
public:
    // Profiles live for the active map session only. Persistent Experience is
    // intentionally a separate model and is not part of this lifecycle.
    bool beginMap(MapGeneration map) noexcept;
    bool beginRound(perception::RoundGeneration round) noexcept;
    void reset() noexcept;
    void forget(PlayerId player) noexcept;
    OpponentProfileUpdate observe(const OpponentObservation& observation) noexcept;
    const OpponentProfile* find(PlayerId player) const noexcept;
    std::size_t size() const noexcept { return count_; }
    MapGeneration map() const noexcept { return map_; }
    perception::RoundGeneration round() const noexcept { return round_; }

private:
    std::array<OpponentProfile, kMaxOpponentProfiles> profiles_{};
    std::array<Generation, kMaxOpponentProfiles> retiredGenerations_{};
    MapGeneration map_{};
    perception::RoundGeneration round_{};
    std::size_t count_{0};
};

enum class WallbangReason : std::uint8_t {
    None = 0,
    Accepted,
    InvalidInput,
    AnonymousSource,
    StaleBelief,
    LowConfidence,
    InsufficientPenetration,
    FriendlyFireRisk,
    InsufficientAmmo,
    LowExpectedDamage,
};

struct WallbangInput final {
    action::EnemyBelief belief{};
    perception::ObservationSource source{perception::ObservationSource::Unknown};
    std::uint64_t nowMicros{0};
    std::uint64_t maxAgeMicros{2'000'000};
    double minimumConfidence{0.25};
    double wallThickness{0.0};
    double materialLoss{1.0};
    double weaponPenetration{0.0};
    double friendlyFireRisk{0.0};
    double expectedDamage{0.0};
    std::int32_t ammoCost{1};
    std::int32_t availableAmmo{0};
};

struct WallbangPlan final {
    combat::FireMode mode{combat::FireMode::Wallbang};
    PlayerId target{};
    perception::Point aimPoint{};
    double penetrationFactor{0.0};
    double expectedDamage{0.0};
    double utility{0.0};
    WallbangReason reason{WallbangReason::None};
    bool accepted{false};
};

WallbangPlan planWallbang(const WallbangInput& input) noexcept;

enum class SuppressiveFireReason : std::uint8_t {
    None = 0,
    Accepted,
    InvalidInput,
    LowProbability,
    FriendlyFireRisk,
    InsufficientAmmo,
};

struct SuppressiveFireInput final {
    std::uint32_t regionArea{0};
    perception::Point region{};
    double regionRadius{0.0};
    double probability{0.0};
    double minimumProbability{0.5};
    double expectedDamage{0.0};
    double friendlyFireRisk{0.0};
    std::int32_t ammoCost{1};
    std::int32_t availableAmmo{0};
};

struct SuppressiveFirePlan final {
    combat::FireMode mode{combat::FireMode::SuppressiveFire};
    std::uint32_t regionArea{0};
    perception::Point region{};
    double probability{0.0};
    double utility{0.0};
    SuppressiveFireReason reason{SuppressiveFireReason::None};
    bool accepted{false};
};

SuppressiveFirePlan planSuppressiveFire(const SuppressiveFireInput& input) noexcept;

} // namespace astrabot::core::learning
