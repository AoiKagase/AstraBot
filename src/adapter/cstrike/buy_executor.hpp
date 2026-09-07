// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#pragma once

#include "core/economy.hpp"

#include <cstddef>
#include <cstdint>

namespace astrabot::adapter::cstrike {

// The adapter translates this value request to a CS command/menu operation.
// No engine pointer or command string crosses the Core boundary.
enum class DispatchStatus : std::uint8_t {
    Sent = 0,
    NotEnoughMoney,
    NotInBuyZone,
    BuyTimeExpired,
    ItemUnavailable,
    AlreadyOwned,
    InventoryConflict,
    Rejected,
};

class IBuyOperations {
public:
    virtual ~IBuyOperations() = default;
    virtual DispatchStatus dispatch(core::economy::PurchaseItem item) noexcept = 0;
    virtual core::economy::EquipmentSnapshot observeInventory() const noexcept = 0;
};

struct PurchaseContext final {
    core::MapGeneration map{};
    core::perception::RoundGeneration round{};
    core::TickId tick{};
    core::PlayerId player{};
    core::economy::EquipmentSnapshot inventory{};
    bool inBuyZone{false};
    bool buyTimeOpen{false};
    bool liveRound{false};
};

struct PurchaseExecutionResult final {
    core::economy::BuyFailureReason failure{
        core::economy::BuyFailureReason::None};
    std::size_t attempts{0};
    std::size_t fulfilledRequests{0};
    bool accepted{false};
    bool completed{false};
};

class BuyExecutionAdapter final {
public:
    PurchaseExecutionResult execute(
        const core::economy::BuyPlan& plan, const PurchaseContext& context,
        IBuyOperations& operations) const noexcept;

private:
    static bool satisfied(const core::economy::EquipmentSnapshot& before,
                          const core::economy::EquipmentSnapshot& after,
                          core::economy::PurchaseItem item) noexcept;
    static bool alreadyOwned(const core::economy::EquipmentSnapshot& inventory,
                             core::economy::PurchaseItem item) noexcept;
    static core::economy::BuyFailureReason mapFailure(
        DispatchStatus status) noexcept;
};

} // namespace astrabot::adapter::cstrike
