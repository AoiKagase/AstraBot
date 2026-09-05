// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#pragma once

#include "nav/model/value_types.hpp"

namespace astrabot::nav::model {

// Runtime graph vocabulary, not wire values or a stable serialized/public ABI.
// Area attribute bits and approach "how" bytes are separate source metadata.
enum class NavTraversalKind : std::uint8_t {
    Walk,
    Crouch,
    Jump,
    Ladder,
    Drop,
};

// Consumers of externally supplied enrichment must reject unknown kinds.
// Never normalize an unsupported value to Walk.
constexpr bool isKnownTraversalKind(NavTraversalKind kind) noexcept {
    switch (kind) {
    case NavTraversalKind::Walk:
    case NavTraversalKind::Crouch:
    case NavTraversalKind::Jump:
    case NavTraversalKind::Ladder:
    case NavTraversalKind::Drop:
        return true;
    default:
        return false;
    }
}

struct NavConnection final {
    NavAreaId target{};
    // Default interpretation of cardinal adjacency; not a claim that the file
    // encodes Walk, or that per-frame movement is safe without local probes.
    NavTraversalKind traversal{NavTraversalKind::Walk};
};

} // namespace astrabot::nav::model
