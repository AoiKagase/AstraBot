// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#pragma once

#include "core/identity.hpp"
#include "core/perception_identity.hpp"
#include "core/team_director.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace astrabot::core::economy {

constexpr std::size_t kMaxBuyRequests = 16;
constexpr std::size_t kMaxFallbacks = 3;
constexpr std::int32_t kMaxMoney = 16'000;
constexpr std::uint8_t kMaxUtility = 4;
constexpr std::uint8_t kMaxKitTarget = 4;
constexpr std::size_t kMaxPurchaseAttempts = 32;

enum class RoundPhase : std::uint8_t {
    Unknown = 0,
    FreezeTime,
    Live,
    PostRound,
};

enum class BuyStrategy : std::uint8_t {
    None = 0,
    PistolRound,
    Eco,
    HalfBuy,
    ForceBuy,
    FullBuy,
    Save,
};

enum class StrategyReason : std::uint8_t {
    None = 0,
    InvalidInput,
    PistolRound,
    TeamMedianBudget,
    MajorityReady,
    LossRecovery,
    CarriedEquipment,
    LowTeamBudget,
};

enum class WeaponKind : std::uint8_t {
    None = 0,
    Rifle,
    Smg,
    Sniper,
    Pistol,
};

enum class PurchaseItem : std::uint8_t {
    None = 0,
    Rifle,
    Galil,
    Famas,
    Smg,
    Awp,
    Pistol,
    Armor,
    Helmet,
    DefuseKit,
    HeGrenade,
    Flashbang,
    SmokeGrenade,
    Ammo,
};

enum class BuyFailureReason : std::uint8_t {
    None = 0,
    InvalidPlan,
    StaleIdentity,
    NotEnoughMoney,
    NotInBuyZone,
    BuyTimeExpired,
    ItemUnavailable,
    AlreadyOwned,
    InventoryConflict,
    CommandRejected,
    RetryLimit,
    Unknown,
};

struct EquipmentSnapshot final {
    WeaponKind primary{WeaponKind::None};
    WeaponKind secondary{WeaponKind::None};
    std::uint8_t armor{0};
    bool helmet{false};
    bool defuseKit{false};
    std::uint8_t heGrenades{0};
    std::uint8_t flashbangs{0};
    std::uint8_t smokeGrenades{0};
    std::uint16_t primaryAmmo{0};
    std::uint16_t secondaryAmmo{0};

    bool valid() const noexcept;
    bool hasUsablePrimary() const noexcept { return primary != WeaponKind::None; }
    bool hasUsableSecondary() const noexcept {
        return secondary != WeaponKind::None;
    }
};

struct PlayerEconomySnapshot final {
    PlayerId player{};
    BotAgentId agent{};
    perception::Team team{perception::Team::Unknown};
    team::Role role{team::Role::None};
    std::int32_t money{0};
    EquipmentSnapshot equipment{};
    bool awpCandidate{false};
    bool canDefuse{false};
    bool connected{false};
    bool alive{false};

    bool valid() const noexcept;
};

struct TeamEconomySnapshot final {
    MapGeneration map{};
    perception::RoundGeneration round{};
    TickId tick{};
    std::uint64_t nowMicros{0};
    std::uint32_t roundNumber{0};
    RoundPhase phase{RoundPhase::Unknown};
    perception::Team team{perception::Team::Unknown};
    team::Strategy teamStrategy{team::Strategy::None};
    std::uint8_t lossStreak{0};
    std::uint16_t lossBonus{0};
    std::uint8_t requestedKitCount{1};
    std::array<PlayerEconomySnapshot, team::kMaxTeamMembers> members{};
    std::size_t memberCount{0};

    bool valid() const noexcept;
};

struct BuyRequest final {
    PurchaseItem item{PurchaseItem::None};
    std::uint8_t quantity{1};
    std::uint8_t priority{0};
    bool mandatory{false};
    bool preserved{false};
    std::array<PurchaseItem, kMaxFallbacks> fallbacks{};
    std::size_t fallbackCount{0};

    bool valid() const noexcept;
};

struct BuyStrategyDecision final {
    BuyStrategy strategy{BuyStrategy::None};
    StrategyReason reason{StrategyReason::None};
    std::int32_t medianMoney{0};
    std::size_t evaluatedMembers{0};
    bool accepted{false};
};

struct BuyPlan final {
    MapGeneration map{};
    perception::RoundGeneration round{};
    TickId tick{};
    PlayerId player{};
    BuyStrategy strategy{BuyStrategy::None};
    team::Role role{team::Role::None};
    std::array<BuyRequest, kMaxBuyRequests> requests{};
    std::size_t requestCount{0};
    std::int32_t expectedSpend{0};
    std::int32_t remainingMoney{0};
    bool preservedPrimary{false};
    bool preservedSecondary{false};
    bool preservedArmor{false};
    bool preservedUtility{false};
    bool accepted{false};

    bool valid() const noexcept;
    bool matches(const TeamEconomySnapshot& snapshot) const noexcept;
};

struct TeamBuyDecision final {
    BuyStrategy strategy{BuyStrategy::None};
    StrategyReason reason{StrategyReason::None};
    std::array<BuyPlan, team::kMaxTeamMembers> plans{};
    std::size_t planCount{0};
    std::size_t kitPlanCount{0};
    bool accepted{false};

    bool valid(const TeamEconomySnapshot& snapshot) const noexcept;
};

class BuyPlanner final {
public:
    BuyStrategyDecision chooseStrategy(
        const TeamEconomySnapshot& snapshot) const noexcept;
    BuyPlan buildPlan(const TeamEconomySnapshot& snapshot,
                      std::size_t memberIndex) const noexcept;
    TeamBuyDecision planTeam(const TeamEconomySnapshot& snapshot) const noexcept;

    static std::int32_t price(PurchaseItem item) noexcept;
    static const char* strategyName(BuyStrategy strategy) noexcept;
    static const char* strategyReasonName(StrategyReason reason) noexcept;
    static const char* itemName(PurchaseItem item) noexcept;
    static const char* failureName(BuyFailureReason reason) noexcept;

private:
    static std::size_t firstAwpOwner(const TeamEconomySnapshot& snapshot) noexcept;
    static bool isKitOwner(const TeamEconomySnapshot& snapshot,
                           std::size_t memberIndex) noexcept;
    static bool activeMember(const PlayerEconomySnapshot& member) noexcept;
};

} // namespace astrabot::core::economy
