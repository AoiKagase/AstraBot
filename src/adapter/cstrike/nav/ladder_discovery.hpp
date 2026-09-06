// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "adapter/cstrike/nav/ladder_probe.hpp"
#include "nav/enrichment/traversal_link.hpp"
namespace astrabot::adapter::cstrike {
inline constexpr std::uint64_t ladderSourceId=0x4c4144444552ULL;
struct LadderDiscoveryLimits {
    std::uint32_t maxQueries{12288};
    std::size_t maxPassages{1024};
};
struct LadderDiscovery {
    core::MapGeneration map{};
    std::uint64_t generation{};
    nav::enrichment::NavTraversalLinkSet links{};
    // Same order as link pairs; links[2*i] up, links[2*i+1] down.
    std::vector<LadderPassage> passages{};
};
enum class LadderDiscoveryReason { None, InvalidInput, ScanFailed, ProbeFailed,
    UnlinkedCandidate, BudgetExceeded, StaleWorld, AllocationFailure };
struct LadderDiscoveryResult {
    LadderDiscoveryReason reason{LadderDiscoveryReason::InvalidInput};
    LadderScanReason scanReason{LadderScanReason::None};
    LadderProbeReason probeReason{LadderProbeReason::None};
    std::shared_ptr<const LadderDiscovery> value{};
    std::uint32_t candidates{},queries{};
    explicit operator bool() const noexcept { return reason==LadderDiscoveryReason::None && bool(value); }
};
// The caller binds the fingerprint to its own BSP stream and the NAV/index.
// Enumerates each face/exit exactly once; failed variants are not retried.
// No batch is published when a candidate is unlinked, stale or over budget.
LadderDiscoveryResult discoverLadderLinks(LadderWorld,core::MapGeneration,
    const nav::query::NavSpatialIndex&,core::MapGeneration indexMap,
    const nav::enrichment::NavMapFingerprint&,std::uint64_t generation,int maxEntities,
    LadderDiscoveryLimits={}) noexcept;
}
