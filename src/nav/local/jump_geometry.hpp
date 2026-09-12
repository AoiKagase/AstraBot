// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "nav/local/simple_jump.hpp"

namespace astrabot::nav::local {
struct JumpGeometryLimits { double preferredDistance{}, clearanceMargin{}; };
enum class JumpGeometryReason { None, InvalidInput, InvalidActor, InvalidStep,
    UnsupportedTransition, NoRoom, InvalidGeometry, HeightUnsupported };
enum class JumpLandingRegionKind : std::uint8_t { HullMargin, CentreInset, SuccessorCentreInset };
struct JumpLandingRegion {
    model::NavAreaId area{};
    model::NavExtent extent{};
    double lowX{}, highX{}, lowY{}, highY{};
    std::uint8_t cursorAdvance{1};
    JumpLandingRegionKind kind{JumpLandingRegionKind::HullMargin};
    explicit operator bool() const noexcept {
        return area.isValid() && extent.isFinite() && lowX<=highX && lowY<=highY &&
            cursorAdvance>=1 && cursorAdvance<=2;
    }
};
struct JumpLandingEnvelope {
    JumpLandingRegion target{};
    std::optional<JumpLandingRegion> successor{};
};
struct JumpGeometryResult {
    JumpGeometryReason reason{JumpGeometryReason::None};
    std::optional<JumpPlan> plan{};
    std::optional<JumpLandingEnvelope> landingEnvelope{};
    explicit operator bool() const noexcept { return reason==JumpGeometryReason::None && plan.has_value(); }
};
class JumpGeometry final {
public:
    // Uses exactly binding.step of a validated corridor. Hull-safe patches use
    // a hull-margin region; micro patches retain only a centred NAV envelope.
    // The full hull and flight remain world-query obligations, never NAV proof.
    // No external/gap/duck-jump traversal, world proof or command is inferred.
    static JumpGeometryResult derive(const corridor::Corridor&, Binding,
        const runtime::MovementSnapshot&, JumpLimits, JumpGeometryLimits, bool observedObstacle=false) noexcept;
};
}
