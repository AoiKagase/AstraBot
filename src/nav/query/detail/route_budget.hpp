// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "nav/diagnostics/error.hpp"
#include "nav/model/connection.hpp"
#include <cstddef>
#include <limits>

namespace astrabot::nav::query::detail {
inline diagnostics::NavError graphError(diagnostics::NavErrorKind kind,
                                        diagnostics::NavField field = diagnostics::NavField::GraphBytes) noexcept {
    return {kind, 0, diagnostics::NavRecord::Graph, field};
}
// Logical storage only. Failure leaves used unchanged. Check arithmetic before caps.
inline diagnostics::NavError charge(std::size_t &used, std::size_t count,
                                    std::size_t elementSize, std::size_t cap) noexcept {
    if (elementSize != 0 && count > (std::numeric_limits<std::size_t>::max() - used) / elementSize)
        return graphError(diagnostics::NavErrorKind::OffsetOverflow);
    const auto total = used + count * elementSize;
    if (total > cap)
        return graphError(diagnostics::NavErrorKind::CountLimitExceeded);
    used = total;
    return {};
}
inline diagnostics::NavError validateEdge(model::NavTraversalKind traversal) noexcept {
    if (!model::isKnownTraversalKind(traversal))
        return graphError(diagnostics::NavErrorKind::UnsupportedValue,
                          diagnostics::NavField::GraphTraversal);
    return {};
}
} // namespace astrabot::nav::query::detail
