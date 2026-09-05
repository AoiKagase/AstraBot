// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "nav/local/simple_jump.hpp"

namespace astrabot::nav::local {
struct JumpGeometryLimits { double preferredDistance{}, clearanceMargin{}; };
enum class JumpGeometryReason { None, InvalidInput, InvalidActor, InvalidStep,
    UnsupportedTransition, NoRoom, InvalidGeometry, HeightUnsupported };
struct JumpGeometryResult {
    JumpGeometryReason reason{JumpGeometryReason::None};
    std::optional<JumpPlan> plan{};
    explicit operator bool() const noexcept { return reason==JumpGeometryReason::None && plan.has_value(); }
};
class JumpGeometry final {
public:
    // Uses exactly binding.step of a validated corridor. The full takeoff and
    // landing circles (plus actual hull and explicit margin) must fit their areas.
    // NAV-interpolated standing origins are only candidates for world queries.
    // No external/gap/duck-jump traversal, world proof or command is inferred.
    static JumpGeometryResult derive(const corridor::Corridor&, Binding,
        const runtime::MovementSnapshot&, JumpLimits, JumpGeometryLimits) noexcept;
};
}
