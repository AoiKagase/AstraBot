// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include "adapter/cstrike/buy_executor.hpp"
#include "core/economy.hpp"

#include <array>
#include <cassert>

namespace {

namespace e = astrabot::core::economy;
namespace t = astrabot::core::team;
namespace c = astrabot::adapter::cstrike;
using astrabot::core::BotAgentId;
using astrabot::core::PlayerId;

PlayerId player(std::uint16_t slot) { return {slot, {1}}; }
BotAgentId agent(std::uint32_t value) { return {value}; }

e::TeamEconomySnapshot snapshot(std::size_t count = 5) {
    e::TeamEconomySnapshot result{};
    result.map = {4};
    result.round = {7};
    result.tick = {99};
    result.nowMicros = 2'000'000;
    result.roundNumber = 2;
    result.phase = e::RoundPhase::FreezeTime;
    result.team = astrabot::core::perception::Team::Terrorist;
    result.teamStrategy = t::Strategy::AttackSplit;
    result.memberCount = count;
    for (std::size_t i = 0; i < count; ++i) {
        const auto role = i == 0 ? t::Role::Entry
            : (i == 1 ? t::Role::Support
                      : (i == 2 ? t::Role::Trade : t::Role::Lurk));
        result.members[i] = {
            player(static_cast<std::uint16_t>(i + 1)),
            agent(static_cast<std::uint32_t>(i + 1)), result.team, role, 5'000,
            {}, false, false, true, true};
    }
    return result;
}

bool hasItem(const e::BuyPlan& plan, e::PurchaseItem item) {
    for (std::size_t i = 0; i < plan.requestCount; ++i) {
        if (plan.requests[i].item == item) return true;
    }
    return false;
}

const e::BuyRequest* findItem(const e::BuyPlan& plan, e::PurchaseItem item) {
    for (std::size_t i = 0; i < plan.requestCount; ++i) {
        if (plan.requests[i].item == item) return &plan.requests[i];
    }
    return nullptr;
}

void testContractsRejectInvalidAndDuplicateState() {
    auto input = snapshot();
    assert(input.valid());
    input.map = {};
    assert(!input.valid());

    input = snapshot(2);
    input.members[1].player = input.members[0].player;
    assert(!input.valid());

    e::BuyRequest request{};
    request.item = e::PurchaseItem::Rifle;
    request.fallbacks[0] = e::PurchaseItem::Rifle;
    request.fallbackCount = 1;
    assert(!request.valid());
}

void testTeamStrategiesUseMedianAndLossState() {
    e::BuyPlanner planner;
    auto input = snapshot();
    input.roundNumber = 1;
    assert(planner.chooseStrategy(input).strategy == e::BuyStrategy::PistolRound);

    input = snapshot();
    assert(planner.chooseStrategy(input).strategy == e::BuyStrategy::FullBuy);

    for (std::size_t i = 0; i < input.memberCount; ++i) input.members[i].money = 2'500;
    assert(planner.chooseStrategy(input).strategy == e::BuyStrategy::HalfBuy);

    for (std::size_t i = 0; i < input.memberCount; ++i) input.members[i].money = 2'000;
    input.lossStreak = 2;
    assert(planner.chooseStrategy(input).strategy == e::BuyStrategy::ForceBuy);

    for (std::size_t i = 0; i < input.memberCount; ++i) input.members[i].money = 800;
    assert(planner.chooseStrategy(input).strategy == e::BuyStrategy::Eco);

    for (std::size_t i = 0; i < input.memberCount; ++i) {
        input.members[i].money = 800;
        input.members[i].equipment.primary = e::WeaponKind::Rifle;
    }
    assert(planner.chooseStrategy(input).strategy == e::BuyStrategy::Save);

    input = snapshot();
    input.members[0].money = 16'000;
    for (std::size_t i = 1; i < input.memberCount; ++i) input.members[i].money = 800;
    const auto mixed = planner.chooseStrategy(input);
    assert(mixed.strategy == e::BuyStrategy::Eco);
    assert(mixed.medianMoney == 800);
}

void testRoleAwarePlanAndAffordableFallbacks() {
    e::BuyPlanner planner;
    auto input = snapshot();
    input.members[0].awpCandidate = true;
    const auto decision = planner.chooseStrategy(input);
    assert(decision.strategy == e::BuyStrategy::FullBuy);
    const auto plan = planner.buildPlan(input, 0);
    assert(plan.valid());
    assert(hasItem(plan, e::PurchaseItem::Armor));
    assert(hasItem(plan, e::PurchaseItem::Flashbang));
    const auto* awp = findItem(plan, e::PurchaseItem::Awp);
    assert(awp != nullptr);
    assert(awp->fallbackCount >= 2);
    assert(awp->fallbacks[0] == e::PurchaseItem::Rifle);
    assert(awp->fallbacks[1] == e::PurchaseItem::Galil);

    input.members[0].equipment.primary = e::WeaponKind::Rifle;
    input.members[0].equipment.secondary = e::WeaponKind::Pistol;
    input.members[0].equipment.armor = 100;
    input.members[0].equipment.flashbangs = 2;
    const auto preserved = planner.buildPlan(input, 0);
    assert(preserved.valid());
    assert(!hasItem(preserved, e::PurchaseItem::Rifle));
    assert(!hasItem(preserved, e::PurchaseItem::Pistol));
    assert(preserved.preservedPrimary && preserved.preservedSecondary);

    input = snapshot();
    input.members[0].equipment.primary = e::WeaponKind::Smg;
    const auto upgraded = planner.buildPlan(input, 0);
    assert(upgraded.valid());
    assert(hasItem(upgraded, e::PurchaseItem::Rifle));
    assert(!upgraded.preservedPrimary);
}

void testTeamKitAndUtilityCoordination() {
    e::BuyPlanner planner;
    auto input = snapshot();
    input.team = astrabot::core::perception::Team::CounterTerrorist;
    input.requestedKitCount = 1;
    input.members[0].team = input.team;
    input.members[0].role = t::Role::Defuser;
    input.members[0].canDefuse = true;
    input.members[1].team = input.team;
    input.members[1].role = t::Role::Support;
    for (std::size_t i = 2; i < input.memberCount; ++i) input.members[i].team = input.team;
    const auto team = planner.planTeam(input);
    assert(team.valid(input));
    assert(team.kitPlanCount == 1);
    assert(hasItem(team.plans[0], e::PurchaseItem::DefuseKit));
    assert(hasItem(team.plans[1], e::PurchaseItem::SmokeGrenade));
    assert(hasItem(team.plans[2], e::PurchaseItem::Flashbang));
}

void testOneEightAndSixteenMembersRemainBoundedAndDeterministic() {
    e::BuyPlanner planner;
    for (const auto count : {std::size_t{1}, std::size_t{8}, std::size_t{16}}) {
        auto input = snapshot(count);
        const auto left = planner.planTeam(input);
        const auto right = planner.planTeam(input);
        assert(left.valid(input) && right.valid(input));
        assert(left.planCount == count && right.planCount == count);
        for (std::size_t i = 0; i < count; ++i) {
            assert(left.plans[i].player == right.plans[i].player);
            assert(left.plans[i].requestCount == right.plans[i].requestCount);
        }
    }
}

class FakeBuyOperations final : public c::IBuyOperations {
public:
    e::EquipmentSnapshot inventory{};
    std::array<e::PurchaseItem, e::kMaxPurchaseAttempts> calls{};
    std::size_t callCount{0};
    bool rejectAwp{false};
    bool mutate{true};

    c::DispatchStatus dispatch(e::PurchaseItem item) noexcept override {
        if (callCount < calls.size()) calls[callCount++] = item;
        if (rejectAwp && item == e::PurchaseItem::Awp) {
            return c::DispatchStatus::NotEnoughMoney;
        }
        if (!mutate) return c::DispatchStatus::Sent;
        switch (item) {
        case e::PurchaseItem::Rifle:
        case e::PurchaseItem::Galil:
        case e::PurchaseItem::Famas:
        case e::PurchaseItem::Smg:
            inventory.primary = e::WeaponKind::Rifle;
            break;
        case e::PurchaseItem::Awp:
            inventory.primary = e::WeaponKind::Sniper;
            break;
        case e::PurchaseItem::Pistol:
            inventory.secondary = e::WeaponKind::Pistol;
            break;
        case e::PurchaseItem::Armor:
            inventory.armor = 100;
            break;
        case e::PurchaseItem::Helmet:
            inventory.helmet = true;
            break;
        case e::PurchaseItem::DefuseKit:
            inventory.defuseKit = true;
            break;
        case e::PurchaseItem::HeGrenade:
            ++inventory.heGrenades;
            break;
        case e::PurchaseItem::Flashbang:
            ++inventory.flashbangs;
            break;
        case e::PurchaseItem::SmokeGrenade:
            ++inventory.smokeGrenades;
            break;
        case e::PurchaseItem::Ammo:
            inventory.primaryAmmo = 90;
            break;
        case e::PurchaseItem::None:
            return c::DispatchStatus::Rejected;
        }
        return c::DispatchStatus::Sent;
    }

    e::EquipmentSnapshot observeInventory() const noexcept override {
        return inventory;
    }
};

e::BuyPlan simplePlan(std::size_t fallbackCount) {
    e::BuyPlan plan{};
    plan.map = {4};
    plan.round = {7};
    plan.tick = {99};
    plan.player = player(1);
    plan.strategy = e::BuyStrategy::FullBuy;
    plan.role = t::Role::Entry;
    plan.accepted = true;
    plan.requestCount = 1;
    plan.requests[0].item = e::PurchaseItem::Awp;
    plan.requests[0].quantity = 1;
    plan.requests[0].mandatory = true;
    plan.requests[0].fallbacks[0] = e::PurchaseItem::Rifle;
    plan.requests[0].fallbackCount = fallbackCount;
    return plan;
}

c::PurchaseContext context(const e::EquipmentSnapshot& inventory = {}) {
    return {{4}, {7}, {99}, player(1), inventory, true, true, false};
}

void testExecutionVerifiesInventoryAndUsesBoundedFallback() {
    c::BuyExecutionAdapter executor;
    auto plan = simplePlan(1);
    assert(plan.valid());
    FakeBuyOperations operations;
    operations.rejectAwp = true;
    const auto result = executor.execute(plan, context(), operations);
    assert(result.accepted && result.completed);
    assert(result.attempts == 2 && result.fulfilledRequests == 1);
    assert(operations.calls[0] == e::PurchaseItem::Awp);
    assert(operations.calls[1] == e::PurchaseItem::Rifle);

    e::EquipmentSnapshot smgInventory{};
    smgInventory.primary = e::WeaponKind::Smg;
    auto upgradePlan = simplePlan(0);
    upgradePlan.requests[0].item = e::PurchaseItem::Rifle;
    FakeBuyOperations upgradeOperations;
    upgradeOperations.inventory = smgInventory;
    const auto upgradeResult = executor.execute(
        upgradePlan, context(smgInventory), upgradeOperations);
    assert(upgradeResult.accepted && upgradeResult.completed);
    assert(upgradeResult.attempts == 1);
    assert(upgradeOperations.calls[0] == e::PurchaseItem::Rifle);

    auto stale = context();
    stale.round = {8};
    const auto rejected = executor.execute(plan, stale, operations);
    assert(!rejected.accepted);
    assert(rejected.failure == e::BuyFailureReason::StaleIdentity);

    auto expired = context();
    expired.buyTimeOpen = false;
    const auto expiredResult = executor.execute(plan, expired, operations);
    assert(expiredResult.accepted && !expiredResult.completed);
    assert(expiredResult.failure == e::BuyFailureReason::BuyTimeExpired);

    auto bounded = simplePlan(0);
    bounded.requestCount = e::kMaxBuyRequests;
    for (std::size_t i = 0; i < bounded.requestCount; ++i) {
        bounded.requests[i].item = e::PurchaseItem::Armor;
        bounded.requests[i].quantity = 1;
        bounded.requests[i].mandatory = false;
        bounded.requests[i].fallbacks[0] = e::PurchaseItem::Galil;
        bounded.requests[i].fallbacks[1] = e::PurchaseItem::Smg;
        bounded.requests[i].fallbackCount = 2;
    }
    FakeBuyOperations noInventoryChange;
    noInventoryChange.mutate = false;
    const auto boundedResult = executor.execute(bounded, context(), noInventoryChange);
    assert(boundedResult.attempts == e::kMaxPurchaseAttempts);
    assert(boundedResult.failure == e::BuyFailureReason::RetryLimit);
}

} // namespace

int main() {
    testContractsRejectInvalidAndDuplicateState();
    testTeamStrategiesUseMedianAndLossState();
    testRoleAwarePlanAndAffordableFallbacks();
    testTeamKitAndUtilityCoordination();
    testOneEightAndSixteenMembersRemainBoundedAndDeterministic();
    testExecutionVerifiesInventoryAndUsesBoundedFallback();
    return 0;
}
