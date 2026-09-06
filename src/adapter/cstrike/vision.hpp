// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "adapter/metamod/plugin_entry.hpp"
#include "core/perception.hpp"

namespace astrabot::adapter::metamod { class LifecycleCoordinator; }
namespace astrabot::adapter::cstrike {
// Engine pointers/serials and unobserved geometry stay inside this adapter.
class VisionAdapter final {
public:
    void reset() noexcept;
    void forget(core::PlayerId) noexcept;
    void frame(metamod::LifecycleCoordinator&, enginefuncs_t*, float engineTime) noexcept;
    const core::perception::Vision& observations() const noexcept { return vision_; }
    core::perception::Reason error() const noexcept { return error_; }
private:
    struct EntityBinding {
        edict_t* entity{};
        int serial{};
        core::PlayerId player{};
    };
    std::array<EntityBinding, core::perception::kPlayerCapacity> roster_{};
    core::perception::Vision vision_{};
    core::MapGeneration map_{};
    core::perception::Reason error_{core::perception::Reason::None};
};
}
