// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "nav/enrichment/traversal_link.hpp"
#include "nav/model/mesh_snapshot.hpp"
#include "nav/query/detail/route_budget.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <tuple>

namespace astrabot::nav::enrichment::detail {
using Error = diagnostics::NavError;
using K = diagnostics::NavErrorKind;
using F = diagnostics::NavField;
inline Error linkError(K kind, F field) noexcept {
    return {kind, 0, diagnostics::NavRecord::TraversalLink, field};
}
inline auto identity(const NavTraversalLink &l) noexcept {
    return std::tie(l.sourceId, l.generation, l.linkId);
}
inline bool samePayload(const NavTraversalLink &a, const NavTraversalLink &b) noexcept {
    return std::tie(a.from, a.to, a.entry.x, a.entry.y, a.entry.z,
                    a.exit.x, a.exit.y, a.exit.z, a.traversal, a.direction, a.additionalCost) ==
           std::tie(b.from, b.to, b.entry.x, b.entry.y, b.entry.z,
                    b.exit.x, b.exit.y, b.exit.z, b.traversal, b.direction, b.additionalCost);
}
inline Error workingBudget(std::size_t areas, std::size_t links,
                           const NavEnrichmentLimits &limits) noexcept {
    if (links > limits.maxLinks) return linkError(K::CountLimitExceeded, F::LinkCount);
    if (links == 0) return {};
    std::size_t bytes = 0;
    const auto maximum = std::numeric_limits<std::size_t>::max();
    auto e = query::detail::charge(bytes, areas, sizeof(std::size_t), maximum);
    if (e.isNone()) e = query::detail::charge(bytes, links, sizeof(std::size_t), maximum);
    if (!e.isNone()) return linkError(e.kind, F::LinkWorkingBytes);
    if (bytes > limits.maxWorkingBytes)
        return linkError(K::CountLimitExceeded, F::LinkWorkingBytes);
    return {};
}
inline bool finite(NavLinkPoint p) noexcept {
    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}
// Called only after graph and working budgets are checked. Arrays die before
// final graph storage is allocated. Throws allocation failures to compose.
inline Error validate(const model::NavMeshSnapshot &snapshot, const NavMapFingerprint &expected,
                      const NavTraversalLinkSet &set) {
    if (expected != set.fingerprint) return linkError(K::InvalidValue, F::LinkFingerprint);
    if (set.links.empty()) return {};
    const auto &areas = snapshot.areas();
    std::vector<std::size_t> areaOrder(areas.size());
    std::iota(areaOrder.begin(), areaOrder.end(), std::size_t{0});
    std::sort(areaOrder.begin(), areaOrder.end(), [&areas](auto a, auto b) {
        return areas[a].id < areas[b].id;
    });
    const auto exists = [&areas, &areaOrder](model::NavAreaId id) {
        const auto it = std::lower_bound(areaOrder.begin(), areaOrder.end(), id,
            [&areas](auto index, auto key) { return areas[index].id < key; });
        return it != areaOrder.end() && areas[*it].id == id;
    };
    for (const auto &l : set.links) {
        if (!l.sourceId) return linkError(K::InvalidValue, F::LinkSourceId);
        if (!l.generation) return linkError(K::InvalidValue, F::LinkGeneration);
        if (!l.linkId) return linkError(K::InvalidValue, F::LinkId);
        if (!l.from.value) return linkError(K::InvalidValue, F::LinkFrom);
        if (!exists(l.from)) return linkError(K::DanglingReference, F::LinkFrom);
        if (!l.to.value) return linkError(K::InvalidValue, F::LinkTo);
        if (!exists(l.to)) return linkError(K::DanglingReference, F::LinkTo);
        if (l.from == l.to) return linkError(K::InvalidValue, F::LinkTo);
        if (!model::isKnownTraversalKind(l.traversal))
            return linkError(K::UnsupportedValue, F::LinkTraversal);
        if (l.direction != NavLinkDirection::Forward && l.direction != NavLinkDirection::Up &&
            l.direction != NavLinkDirection::Down)
            return linkError(K::UnsupportedValue, F::LinkDirection);
        if (!finite(l.entry)) return linkError(K::InvalidGeometry, F::LinkEntry);
        if (!finite(l.exit)) return linkError(K::InvalidGeometry, F::LinkExit);
        if ((l.direction == NavLinkDirection::Up && !(l.exit.z > l.entry.z)) ||
            (l.direction == NavLinkDirection::Down && !(l.exit.z < l.entry.z)))
            return linkError(K::InvalidGeometry, F::LinkDirection);
        if (!std::isfinite(l.additionalCost) || l.additionalCost < 0)
            return linkError(K::InvalidValue, F::LinkCost);
    }
    std::vector<std::size_t> order(set.links.size());
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::sort(order.begin(), order.end(), [&set](auto a, auto b) {
        return identity(set.links[a]) < identity(set.links[b]);
    });
    // Source groups first, then key groups; conflict beats identical duplicates
    // in one key group regardless of the unspecified ordering of equal keys.
    for (std::size_t begin = 0; begin < order.size();) {
        const auto &first = set.links[order[begin]];
        auto end = begin + 1;
        while (end < order.size() && set.links[order[end]].sourceId == first.sourceId) ++end;
        if (set.links[order[end - 1]].generation != first.generation)
            return linkError(K::InvalidValue, F::LinkGenerationConflict);
        for (auto key = begin; key < end;) {
            const auto &base = set.links[order[key]];
            auto next = key + 1;
            while (next < end && identity(set.links[order[next]]) == identity(base)) {
                if (!samePayload(base, set.links[order[next]]))
                    return linkError(K::InvalidValue, F::LinkConflict);
                ++next;
            }
            if (next > key + 1) return linkError(K::DuplicateId, F::LinkId);
            key = next;
        }
        begin = end;
    }
    return {};
}
} // namespace astrabot::nav::enrichment::detail
