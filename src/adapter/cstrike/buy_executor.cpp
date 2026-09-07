// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include "adapter/cstrike/buy_executor.hpp"

namespace astrabot::adapter::cstrike {
namespace {

std::uint8_t utilityCount(const core::economy::EquipmentSnapshot& inventory,
                          core::economy::PurchaseItem item) noexcept {
    using core::economy::PurchaseItem;
    switch (item) {
    case PurchaseItem::HeGrenade:
        return inventory.heGrenades;
    case PurchaseItem::Flashbang:
        return inventory.flashbangs;
    case PurchaseItem::SmokeGrenade:
        return inventory.smokeGrenades;
    default:
        return 0;
    }
}

} // namespace

bool BuyExecutionAdapter::satisfied(
    const core::economy::EquipmentSnapshot& before,
    const core::economy::EquipmentSnapshot& after,
    core::economy::PurchaseItem item) noexcept {
    using core::economy::PurchaseItem;
    switch (item) {
    case PurchaseItem::Rifle:
    case PurchaseItem::Galil:
    case PurchaseItem::Famas:
    case PurchaseItem::Smg:
        return after.hasUsablePrimary() &&
               (before.primary == core::economy::WeaponKind::None ||
                before.primary != after.primary);
    case PurchaseItem::Awp:
        return after.primary == core::economy::WeaponKind::Sniper;
    case PurchaseItem::Pistol:
        return after.hasUsableSecondary() &&
               (before.secondary == core::economy::WeaponKind::None ||
                before.secondary != after.secondary);
    case PurchaseItem::Armor:
        return after.armor > before.armor;
    case PurchaseItem::Helmet:
        return after.helmet;
    case PurchaseItem::DefuseKit:
        return after.defuseKit;
    case PurchaseItem::HeGrenade:
    case PurchaseItem::Flashbang:
    case PurchaseItem::SmokeGrenade:
        return utilityCount(after, item) > utilityCount(before, item);
    case PurchaseItem::Ammo:
        return after.primaryAmmo > before.primaryAmmo ||
               after.secondaryAmmo > before.secondaryAmmo;
    case PurchaseItem::None:
        return false;
    }
    return false;
}

bool BuyExecutionAdapter::alreadyOwned(
    const core::economy::EquipmentSnapshot& inventory,
    core::economy::PurchaseItem item) noexcept {
    using core::economy::PurchaseItem;
    switch (item) {
    case PurchaseItem::Rifle:
    case PurchaseItem::Galil:
    case PurchaseItem::Famas:
    case PurchaseItem::Smg:
        return inventory.hasUsablePrimary();
    case PurchaseItem::Awp:
        return inventory.primary == core::economy::WeaponKind::Sniper;
    case PurchaseItem::Pistol:
        return inventory.hasUsableSecondary();
    case PurchaseItem::Armor:
        return inventory.armor >= 100;
    case PurchaseItem::Helmet:
        return inventory.helmet;
    case PurchaseItem::DefuseKit:
        return inventory.defuseKit;
    case PurchaseItem::HeGrenade:
    case PurchaseItem::Flashbang:
    case PurchaseItem::SmokeGrenade:
        return utilityCount(inventory, item) >= core::economy::kMaxUtility;
    case PurchaseItem::Ammo:
    case PurchaseItem::None:
        return false;
    }
    return false;
}

core::economy::BuyFailureReason BuyExecutionAdapter::mapFailure(
    DispatchStatus status) noexcept {
    using Reason = core::economy::BuyFailureReason;
    switch (status) {
    case DispatchStatus::Sent:
        return Reason::None;
    case DispatchStatus::NotEnoughMoney:
        return Reason::NotEnoughMoney;
    case DispatchStatus::NotInBuyZone:
        return Reason::NotInBuyZone;
    case DispatchStatus::BuyTimeExpired:
        return Reason::BuyTimeExpired;
    case DispatchStatus::ItemUnavailable:
        return Reason::ItemUnavailable;
    case DispatchStatus::AlreadyOwned:
        return Reason::AlreadyOwned;
    case DispatchStatus::InventoryConflict:
        return Reason::InventoryConflict;
    case DispatchStatus::Rejected:
        return Reason::CommandRejected;
    }
    return Reason::Unknown;
}

PurchaseExecutionResult BuyExecutionAdapter::execute(
    const core::economy::BuyPlan& plan, const PurchaseContext& context,
    IBuyOperations& operations) const noexcept {
    PurchaseExecutionResult result{};
    if (!plan.valid() || !context.map.isValid() || !context.round.isValid() ||
        !context.tick.isValid() || !context.player.isValid() ||
        !context.inventory.valid() || plan.map != context.map ||
        plan.round != context.round || plan.tick != context.tick ||
        plan.player != context.player) {
        result.failure = plan.valid() ? core::economy::BuyFailureReason::StaleIdentity
                                      : core::economy::BuyFailureReason::InvalidPlan;
        return result;
    }
    result.accepted = true;
    if (!context.inBuyZone) {
        result.failure = core::economy::BuyFailureReason::NotInBuyZone;
        return result;
    }
    if (!context.buyTimeOpen) {
        result.failure = core::economy::BuyFailureReason::BuyTimeExpired;
        return result;
    }
    if (context.liveRound) {
        result.failure = core::economy::BuyFailureReason::BuyTimeExpired;
        return result;
    }

    auto inventory = context.inventory;
    for (std::size_t requestIndex = 0; requestIndex < plan.requestCount;
         ++requestIndex) {
        const auto& request = plan.requests[requestIndex];
        bool fulfilled = true;
        for (std::uint8_t quantity = 0; quantity < request.quantity; ++quantity) {
            bool unitFulfilled = false;
            for (std::size_t candidateIndex = 0;
                 candidateIndex <= request.fallbackCount; ++candidateIndex) {
                if (result.attempts >= core::economy::kMaxPurchaseAttempts) {
                    result.failure = core::economy::BuyFailureReason::RetryLimit;
                    return result;
                }
                const auto item = candidateIndex == 0
                    ? request.item : request.fallbacks[candidateIndex - 1];
                if (alreadyOwned(inventory, item)) {
                    unitFulfilled = true;
                    break;
                }
                const auto before = inventory;
                const auto status = operations.dispatch(item);
                ++result.attempts;
                if (status == DispatchStatus::Sent) {
                    inventory = operations.observeInventory();
                    if (!inventory.valid()) {
                        result.failure = core::economy::BuyFailureReason::InventoryConflict;
                        continue;
                    }
                    if (satisfied(before, inventory, item)) {
                        unitFulfilled = true;
                        result.failure = core::economy::BuyFailureReason::None;
                        break;
                    }
                    result.failure = core::economy::BuyFailureReason::InventoryConflict;
                } else {
                    result.failure = mapFailure(status);
                    if (status == DispatchStatus::NotInBuyZone ||
                        status == DispatchStatus::BuyTimeExpired) {
                        return result;
                    }
                }
            }
            if (!unitFulfilled) {
                fulfilled = false;
                break;
            }
        }
        if (fulfilled) {
            ++result.fulfilledRequests;
        } else if (request.mandatory) {
            result.completed = false;
            return result;
        }
    }
    result.completed = true;
    return result;
}

} // namespace astrabot::adapter::cstrike
