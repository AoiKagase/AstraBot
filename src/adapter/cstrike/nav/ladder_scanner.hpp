// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "adapter/metamod/plugin_entry.hpp"
#include "nav/runtime/movement_snapshot.hpp"
#include <array>

namespace astrabot::adapter::cstrike {
// Candidates only: AABB geometry is not evidence of facing, contact or exits.
// No engine pointers escape. Consumers must revalidate map and entity identity
// before tracing or publishing traversal links.
struct LadderCandidate {
    std::uint64_t entityId{}; // serial in upper 32 bits, nonzero edict index below
    nav::model::NavVector3 minimum{}, maximum{};
};
enum class LadderScanReason { None, InvalidInput, Unavailable, EntityLimit,
    CandidateLimit, InvalidEntity, UnsupportedGeometry };
struct LadderCandidates {
    core::MapGeneration map{};
    std::array<LadderCandidate,128> values{};
    std::size_t count{};
};
struct LadderScanResult {
    LadderScanReason reason{LadderScanReason::InvalidInput};
    LadderCandidates candidates{};
    std::uint32_t inspected{};
    explicit operator bool() const noexcept { return reason==LadderScanReason::None; }
};
// Fixed maximum 8192 slots / 128 ladders; caller may tighten either limit.
// maxEntities is the engine's exclusive slot bound (includes the world slot).
// Empty discovery is success. Any failure discards the whole candidate batch.
LadderScanResult scanLadderCandidates(enginefuncs_t*,core::MapGeneration,int maxEntities,
    std::uint32_t maxSlots=8192,std::size_t maxCandidates=128) noexcept;
}
