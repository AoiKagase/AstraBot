// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "nav/model/connection.hpp"
namespace astrabot::nav::local {
enum class ConstraintReason { None, UnknownAttributes, PreciseUnsupported,
    ConflictingJump, DuckJumpUnsupported, UnsupportedTraversal };
struct TraversalConstraints {
    model::NavTraversalKind kind{model::NavTraversalKind::Walk};
    bool noJump{};
    ConstraintReason reason{ConstraintReason::None};
    explicit operator bool() const noexcept { return reason==ConstraintReason::None; }
};
// NAV v1-v5 hints from the manifest-pinned nav.h; interpretation only, never
// rewriting serialized attributes or treating an area hint as a jump button.
inline TraversalConstraints constraints(model::NavTraversalKind edge,std::uint8_t source,std::uint8_t target) noexcept {
    const auto hints=static_cast<unsigned>(source|target);
    TraversalConstraints out; out.noJump=(hints&8U)!=0;
    if(hints&~15U) { out.reason=ConstraintReason::UnknownAttributes; return out; }
    if(hints&4U) { out.reason=ConstraintReason::PreciseUnsupported; return out; }
    if(edge!=model::NavTraversalKind::Walk && edge!=model::NavTraversalKind::Crouch && edge!=model::NavTraversalKind::Jump) {
        out.reason=ConstraintReason::UnsupportedTraversal; return out;
    }
    const bool duck=(hints&1U)!=0 || edge==model::NavTraversalKind::Crouch;
    const bool jump=(hints&2U)!=0 || edge==model::NavTraversalKind::Jump;
    if(jump && out.noJump) { out.reason=ConstraintReason::ConflictingJump; return out; }
    if(jump && duck) { out.reason=ConstraintReason::DuckJumpUnsupported; return out; }
    out.kind=jump ? model::NavTraversalKind::Jump:duck ? model::NavTraversalKind::Crouch:model::NavTraversalKind::Walk;
    return out;
}
}
