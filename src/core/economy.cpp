// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include "core/economy.hpp"

#include <algorithm>
#include <array>

namespace astrabot::core::economy {
namespace {

bool playingTeam(perception::Team team) noexcept {
    return team == perception::Team::Terrorist ||
           team == perception::Team::CounterTerrorist;
}

std::uint8_t utilityCount(const EquipmentSnapshot& equipment,
                          PurchaseItem item) noexcept {
    switch (item) {
    case PurchaseItem::HeGrenade:
        return equipment.heGrenades;
    case PurchaseItem::Flashbang:
        return equipment.flashbangs;
    case PurchaseItem::SmokeGrenade:
        return equipment.smokeGrenades;
    default:
        return 0;
    }
}

std::int32_t minimumAffordable(const BuyRequest& request,
                               std::int32_t money) noexcept {
    const auto cost = [&](PurchaseItem item) noexcept {
        return BuyPlanner::price(item) *
            static_cast<std::int32_t>(request.quantity);
    };
    const auto requestedCost = cost(request.item);
    if (requestedCost <= money) return requestedCost;
    for (std::size_t i = 0; i < request.fallbackCount; ++i) {
        const auto fallbackCost = cost(request.fallbacks[i]);
        if (fallbackCost <= money) return fallbackCost;
    }
    return -1;
}

void appendRequest(BuyPlan& plan, PurchaseItem item, std::uint8_t quantity,
                   std::uint8_t priority, bool mandatory, bool preserved,
                   std::array<PurchaseItem, kMaxFallbacks> fallbacks = {},
                   std::size_t fallbackCount = 0) noexcept {
    if (item == PurchaseItem::None || quantity == 0 ||
        plan.requestCount >= kMaxBuyRequests || fallbackCount > kMaxFallbacks) {
        return;
    }
    auto& request = plan.requests[plan.requestCount++];
    request.item = item;
    request.quantity = quantity;
    request.priority = priority;
    request.mandatory = mandatory;
    request.preserved = preserved;
    request.fallbacks = fallbacks;
    request.fallbackCount = fallbackCount;

    const auto cost = minimumAffordable(request, plan.remainingMoney);
    if (cost >= 0) {
        plan.expectedSpend += cost;
        plan.remainingMoney -= cost;
    }
}

void appendUtility(BuyPlan& plan, const PlayerEconomySnapshot& member,
                   PurchaseItem item, std::uint8_t desired,
                   std::uint8_t priority) noexcept {
    const auto owned = utilityCount(member.equipment, item);
    if (owned >= desired) return;
    const auto quantity = static_cast<std::uint8_t>(desired - owned);
    appendRequest(plan, item, quantity, priority, false, false);
}

bool hasGoodPrimary(const PlayerEconomySnapshot& member) noexcept {
    return member.equipment.primary == WeaponKind::Rifle ||
           member.equipment.primary == WeaponKind::Smg ||
           member.equipment.primary == WeaponKind::Sniper;
}

bool supportsHelmet(const PlayerEconomySnapshot& member) noexcept {
    return !member.equipment.helmet;
}

bool betterKitOwner(const PlayerEconomySnapshot& candidate,
                    const PlayerEconomySnapshot& current) noexcept {
    const auto candidateDefuser = candidate.role == team::Role::Defuser;
    const auto currentDefuser = current.role == team::Role::Defuser;
    if (candidateDefuser != currentDefuser) return candidateDefuser;
    if (candidate.canDefuse != current.canDefuse) return candidate.canDefuse;
    return candidate.player < current.player;
}

} // namespace

bool EquipmentSnapshot::valid() const noexcept {
    return armor <= 100 && heGrenades <= kMaxUtility &&
           flashbangs <= kMaxUtility && smokeGrenades <= kMaxUtility &&
           primaryAmmo <= 4096 && secondaryAmmo <= 4096;
}

bool PlayerEconomySnapshot::valid() const noexcept {
    return player.isValid() && agent.isValid() && playingTeam(team) &&
           role != team::Role::None && money >= 0 && money <= kMaxMoney &&
           equipment.valid();
}

bool TeamEconomySnapshot::valid() const noexcept {
    if (!map.isValid() || !round.isValid() || !tick.isValid() ||
        nowMicros == 0 || roundNumber == 0 || phase == RoundPhase::Unknown ||
        !playingTeam(team) || memberCount == 0 ||
        memberCount > members.size() || requestedKitCount > kMaxKitTarget) {
        return false;
    }
    if (requestedKitCount > memberCount) return false;
    for (std::size_t i = 0; i < memberCount; ++i) {
        const auto& member = members[i];
        if (!member.valid() || member.team != team ||
            (!member.connected && member.alive)) {
            return false;
        }
        for (std::size_t j = 0; j < i; ++j) {
            if (members[j].player == member.player ||
                members[j].agent == member.agent) {
                return false;
            }
        }
    }
    return true;
}

bool BuyRequest::valid() const noexcept {
    if (item == PurchaseItem::None || quantity == 0 ||
        fallbackCount > fallbacks.size()) {
        return false;
    }
    for (std::size_t i = 0; i < fallbackCount; ++i) {
        if (fallbacks[i] == PurchaseItem::None ||
            fallbacks[i] == item) return false;
    }
    return true;
}

bool BuyPlan::valid() const noexcept {
    if (!map.isValid() || !round.isValid() || !tick.isValid() ||
        !player.isValid() || role == team::Role::None ||
        strategy == BuyStrategy::None || requestCount > requests.size() ||
        expectedSpend < 0 || remainingMoney < 0 || !accepted) {
        return false;
    }
    for (std::size_t i = 0; i < requestCount; ++i) {
        if (!requests[i].valid()) return false;
    }
    return true;
}

bool BuyPlan::matches(const TeamEconomySnapshot& snapshot) const noexcept {
    if (!valid() || !snapshot.valid() || map != snapshot.map ||
        round != snapshot.round || tick != snapshot.tick) {
        return false;
    }
    for (std::size_t i = 0; i < snapshot.memberCount; ++i) {
        if (snapshot.members[i].player == player) return true;
    }
    return false;
}

bool TeamBuyDecision::valid(const TeamEconomySnapshot& snapshot) const noexcept {
    if (!accepted || !snapshot.valid() || strategy == BuyStrategy::None ||
        planCount != snapshot.memberCount || kitPlanCount > planCount) {
        return false;
    }
    for (std::size_t i = 0; i < planCount; ++i) {
        if (!plans[i].valid() || !plans[i].matches(snapshot) ||
            plans[i].strategy != strategy) {
            return false;
        }
    }
    return true;
}

BuyStrategyDecision BuyPlanner::chooseStrategy(
    const TeamEconomySnapshot& snapshot) const noexcept {
    BuyStrategyDecision result{};
    if (!snapshot.valid()) {
        result.reason = StrategyReason::InvalidInput;
        return result;
    }

    result.accepted = true;
    result.evaluatedMembers = snapshot.memberCount;
    if (snapshot.roundNumber == 1) {
        result.strategy = BuyStrategy::PistolRound;
        result.reason = StrategyReason::PistolRound;
        return result;
    }

    std::array<std::int32_t, team::kMaxTeamMembers> money{};
    std::size_t goodPrimary = 0;
    std::size_t halfReady = 0;
    std::size_t fullReady = 0;
    for (std::size_t i = 0; i < snapshot.memberCount; ++i) {
        const auto& member = snapshot.members[i];
        money[i] = member.money;
        if (hasGoodPrimary(member)) ++goodPrimary;
        if (member.money >= 2'500 || hasGoodPrimary(member)) ++halfReady;
        if (member.money >= 4'000 ||
            (hasGoodPrimary(member) && member.money >= 2'000)) {
            ++fullReady;
        }
    }
    std::sort(money.begin(), money.begin() +
              static_cast<std::ptrdiff_t>(snapshot.memberCount));
    result.medianMoney = money[snapshot.memberCount / 2];
    const auto majority = (snapshot.memberCount / 2U) + 1U;

    if (fullReady >= majority && result.medianMoney >= 3'000) {
        result.strategy = BuyStrategy::FullBuy;
        result.reason = StrategyReason::MajorityReady;
    } else if (snapshot.lossStreak >= 2 && halfReady >= majority &&
               result.medianMoney >= 1'800) {
        result.strategy = BuyStrategy::ForceBuy;
        result.reason = StrategyReason::LossRecovery;
    } else if (halfReady >= majority && result.medianMoney >= 1'800) {
        result.strategy = BuyStrategy::HalfBuy;
        result.reason = StrategyReason::TeamMedianBudget;
    } else if (goodPrimary >= majority && result.medianMoney < 1'800) {
        result.strategy = BuyStrategy::Save;
        result.reason = StrategyReason::CarriedEquipment;
    } else if (result.medianMoney < 1'200) {
        result.strategy = BuyStrategy::Eco;
        result.reason = StrategyReason::LowTeamBudget;
    } else {
        result.strategy = BuyStrategy::ForceBuy;
        result.reason = StrategyReason::LossRecovery;
    }
    return result;
}

std::size_t BuyPlanner::firstAwpOwner(
    const TeamEconomySnapshot& snapshot) noexcept {
    std::size_t owner = snapshot.memberCount;
    for (std::size_t i = 0; i < snapshot.memberCount; ++i) {
        const auto& candidate = snapshot.members[i];
        if (!activeMember(candidate) || !candidate.awpCandidate) continue;
        if (owner == snapshot.memberCount || candidate.player < snapshot.members[owner].player) {
            owner = i;
        }
    }
    return owner;
}

bool BuyPlanner::isKitOwner(const TeamEconomySnapshot& snapshot,
                            std::size_t memberIndex) noexcept {
    if (snapshot.team != perception::Team::CounterTerrorist ||
        snapshot.requestedKitCount == 0 || memberIndex >= snapshot.memberCount ||
        !activeMember(snapshot.members[memberIndex]) ||
        !snapshot.members[memberIndex].canDefuse) {
        return false;
    }
    std::size_t rank = 1;
    for (std::size_t i = 0; i < snapshot.memberCount; ++i) {
        const auto& candidate = snapshot.members[i];
        if (i == memberIndex || !activeMember(candidate) || !candidate.canDefuse) continue;
        if (betterKitOwner(candidate, snapshot.members[memberIndex])) ++rank;
    }
    return rank <= snapshot.requestedKitCount;
}

bool BuyPlanner::activeMember(const PlayerEconomySnapshot& member) noexcept {
    return member.connected && member.alive;
}

BuyPlan BuyPlanner::buildPlan(const TeamEconomySnapshot& snapshot,
                              std::size_t memberIndex) const noexcept {
    BuyPlan plan{};
    if (!snapshot.valid() || memberIndex >= snapshot.memberCount ||
        !activeMember(snapshot.members[memberIndex])) {
        return plan;
    }
    const auto decision = chooseStrategy(snapshot);
    if (!decision.accepted) return plan;

    const auto& member = snapshot.members[memberIndex];
    const auto awpOwner = firstAwpOwner(snapshot);
    plan.map = snapshot.map;
    plan.round = snapshot.round;
    plan.tick = snapshot.tick;
    plan.player = member.player;
    plan.strategy = decision.strategy;
    plan.role = member.role;
    plan.remainingMoney = member.money;
    plan.accepted = true;

    const bool economyBuy = decision.strategy == BuyStrategy::FullBuy ||
        decision.strategy == BuyStrategy::HalfBuy ||
        decision.strategy == BuyStrategy::ForceBuy ||
        decision.strategy == BuyStrategy::PistolRound;

    if (!economyBuy || decision.strategy == BuyStrategy::Save ||
        decision.strategy == BuyStrategy::Eco) {
        plan.preservedPrimary = member.equipment.hasUsablePrimary();
        plan.preservedSecondary = member.equipment.hasUsableSecondary();
        plan.preservedArmor = member.equipment.armor != 0;
        plan.preservedUtility = member.equipment.heGrenades != 0 ||
            member.equipment.flashbangs != 0 || member.equipment.smokeGrenades != 0;
    }

    if ((economyBuy || decision.strategy == BuyStrategy::Eco) &&
        member.equipment.armor < 100) {
        appendRequest(plan, PurchaseItem::Armor, 1, 10,
                      decision.strategy != BuyStrategy::Eco, false);
    } else if (member.equipment.armor != 0) {
        plan.preservedArmor = true;
    }

    if (economyBuy && supportsHelmet(member)) {
        appendRequest(plan, PurchaseItem::Helmet, 1, 20,
                      decision.strategy == BuyStrategy::FullBuy, false);
    } else if (member.equipment.helmet) {
        plan.preservedArmor = true;
    }

    if (economyBuy && decision.strategy != BuyStrategy::PistolRound &&
        !member.equipment.hasUsablePrimary()) {
        std::array<PurchaseItem, kMaxFallbacks> fallbacks{};
        std::size_t fallbackCount = 0;
        PurchaseItem primary = PurchaseItem::Rifle;
        if (member.awpCandidate && awpOwner == memberIndex &&
            decision.strategy == BuyStrategy::FullBuy) {
            primary = PurchaseItem::Awp;
            fallbacks[fallbackCount++] = PurchaseItem::Rifle;
        } else {
            primary = PurchaseItem::Rifle;
        }
        fallbacks[fallbackCount++] = PurchaseItem::Galil;
        fallbacks[fallbackCount++] = PurchaseItem::Smg;
        appendRequest(plan, primary, 1, 30, true, false, fallbacks,
                      fallbackCount);
    } else if (member.equipment.hasUsablePrimary()) {
        plan.preservedPrimary = true;
    }

    if (economyBuy && !member.equipment.hasUsableSecondary()) {
        appendRequest(plan, PurchaseItem::Pistol, 1, 40, false, false);
    } else if (member.equipment.hasUsableSecondary()) {
        plan.preservedSecondary = true;
    }

    if (decision.strategy != BuyStrategy::Eco &&
        decision.strategy != BuyStrategy::Save) {
        switch (member.role) {
        case team::Role::Entry:
        case team::Role::Trade:
            appendUtility(plan, member, PurchaseItem::Flashbang,
                          decision.strategy == BuyStrategy::PistolRound ? 1 : 2, 50);
            break;
        case team::Role::Support:
            appendUtility(plan, member, PurchaseItem::SmokeGrenade, 1, 50);
            appendUtility(plan, member, PurchaseItem::Flashbang, 1, 51);
            appendUtility(plan, member, PurchaseItem::HeGrenade, 1, 52);
            break;
        case team::Role::Anchor:
        case team::Role::Rotator:
        case team::Role::Defuser:
            appendUtility(plan, member, PurchaseItem::SmokeGrenade, 1, 50);
            break;
        case team::Role::Lurk:
        case team::Role::FlankWatch:
        case team::Role::Escort:
        case team::Role::None:
            break;
        }
    }

    if (isKitOwner(snapshot, memberIndex) && !member.equipment.defuseKit &&
        economyBuy) {
        appendRequest(plan, PurchaseItem::DefuseKit, 1, 55, true, false);
    } else if (member.equipment.defuseKit) {
        plan.preservedUtility = true;
    }

    if (member.equipment.hasUsablePrimary() && member.equipment.primaryAmmo < 30 &&
        economyBuy) {
        appendRequest(plan, PurchaseItem::Ammo, 1, 60, false, false);
    }
    if (member.equipment.heGrenades != 0 || member.equipment.flashbangs != 0 ||
        member.equipment.smokeGrenades != 0) {
        plan.preservedUtility = true;
    }
    return plan;
}

TeamBuyDecision BuyPlanner::planTeam(
    const TeamEconomySnapshot& snapshot) const noexcept {
    TeamBuyDecision result{};
    const auto strategy = chooseStrategy(snapshot);
    if (!strategy.accepted) return result;
    result.strategy = strategy.strategy;
    result.reason = strategy.reason;
    result.accepted = true;
    for (std::size_t i = 0; i < snapshot.memberCount; ++i) {
        result.plans[result.planCount++] = buildPlan(snapshot, i);
        if (result.plans[result.planCount - 1].valid()) {
            for (std::size_t j = 0; j < result.plans[result.planCount - 1].requestCount; ++j) {
                if (result.plans[result.planCount - 1].requests[j].item == PurchaseItem::DefuseKit) {
                    ++result.kitPlanCount;
                    break;
                }
            }
        }
    }
    if (!result.valid(snapshot)) result.accepted = false;
    return result;
}

std::int32_t BuyPlanner::price(PurchaseItem item) noexcept {
    switch (item) {
    case PurchaseItem::Rifle: return 2'500;
    case PurchaseItem::Galil: return 2'000;
    case PurchaseItem::Famas: return 2'250;
    case PurchaseItem::Smg: return 1'500;
    case PurchaseItem::Awp: return 4'750;
    case PurchaseItem::Pistol: return 600;
    case PurchaseItem::Armor: return 650;
    case PurchaseItem::Helmet: return 350;
    case PurchaseItem::DefuseKit: return 200;
    case PurchaseItem::HeGrenade: return 300;
    case PurchaseItem::Flashbang: return 200;
    case PurchaseItem::SmokeGrenade: return 300;
    case PurchaseItem::Ammo: return 80;
    case PurchaseItem::None: return 0;
    }
    return 0;
}

const char* BuyPlanner::strategyName(BuyStrategy strategy) noexcept {
    switch (strategy) {
    case BuyStrategy::None: return "none";
    case BuyStrategy::PistolRound: return "pistol-round";
    case BuyStrategy::Eco: return "eco";
    case BuyStrategy::HalfBuy: return "half-buy";
    case BuyStrategy::ForceBuy: return "force-buy";
    case BuyStrategy::FullBuy: return "full-buy";
    case BuyStrategy::Save: return "save";
    }
    return "unknown";
}

const char* BuyPlanner::strategyReasonName(StrategyReason reason) noexcept {
    switch (reason) {
    case StrategyReason::None: return "none";
    case StrategyReason::InvalidInput: return "invalid-input";
    case StrategyReason::PistolRound: return "pistol-round";
    case StrategyReason::TeamMedianBudget: return "team-median-budget";
    case StrategyReason::MajorityReady: return "majority-ready";
    case StrategyReason::LossRecovery: return "loss-recovery";
    case StrategyReason::CarriedEquipment: return "carried-equipment";
    case StrategyReason::LowTeamBudget: return "low-team-budget";
    }
    return "unknown";
}

const char* BuyPlanner::itemName(PurchaseItem item) noexcept {
    switch (item) {
    case PurchaseItem::None: return "none";
    case PurchaseItem::Rifle: return "rifle";
    case PurchaseItem::Galil: return "galil";
    case PurchaseItem::Famas: return "famas";
    case PurchaseItem::Smg: return "smg";
    case PurchaseItem::Awp: return "awp";
    case PurchaseItem::Pistol: return "pistol";
    case PurchaseItem::Armor: return "armor";
    case PurchaseItem::Helmet: return "helmet";
    case PurchaseItem::DefuseKit: return "defuse-kit";
    case PurchaseItem::HeGrenade: return "he";
    case PurchaseItem::Flashbang: return "flashbang";
    case PurchaseItem::SmokeGrenade: return "smoke";
    case PurchaseItem::Ammo: return "ammo";
    }
    return "unknown";
}

const char* BuyPlanner::failureName(BuyFailureReason reason) noexcept {
    switch (reason) {
    case BuyFailureReason::None: return "none";
    case BuyFailureReason::InvalidPlan: return "invalid-plan";
    case BuyFailureReason::StaleIdentity: return "stale-identity";
    case BuyFailureReason::NotEnoughMoney: return "not-enough-money";
    case BuyFailureReason::NotInBuyZone: return "not-in-buy-zone";
    case BuyFailureReason::BuyTimeExpired: return "buy-time-expired";
    case BuyFailureReason::ItemUnavailable: return "item-unavailable";
    case BuyFailureReason::AlreadyOwned: return "already-owned";
    case BuyFailureReason::InventoryConflict: return "inventory-conflict";
    case BuyFailureReason::CommandRejected: return "command-rejected";
    case BuyFailureReason::RetryLimit: return "retry-limit";
    case BuyFailureReason::Unknown: return "unknown";
    }
    return "unknown";
}

} // namespace astrabot::core::economy
