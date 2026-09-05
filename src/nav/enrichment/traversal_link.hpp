// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "nav/model/connection.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace astrabot::nav::enrichment {
// Caller-supplied BSP SHA-256. Equality does not authenticate its snapshot binding.
using NavMapFingerprint = std::array<std::uint8_t, 32>;
struct NavLinkPoint { double x{0}, y{0}, z{0}; };
enum class NavLinkDirection : std::uint8_t { Forward, Up, Down };
struct NavTraversalLink {
    std::uint64_t sourceId{0}, generation{0}, linkId{0};
    model::NavAreaId from{}, to{};
    NavLinkPoint entry{}, exit{};
    model::NavTraversalKind traversal{model::NavTraversalKind::Walk};
    NavLinkDirection direction{NavLinkDirection::Forward};
    double additionalCost{0};
};
struct NavTraversalLinkSet {
    NavMapFingerprint fingerprint{};
    std::vector<NavTraversalLink> links;
};
// Logical temporary array bytes, not allocator overhead or sort stack.
// Zero allows no links/bytes. Empty link sets require no temporary arrays.
struct NavEnrichmentLimits { std::size_t maxLinks{0}, maxWorkingBytes{0}; };
} // namespace astrabot::nav::enrichment
