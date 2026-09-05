// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "nav/diagnostics/error.hpp"
#include <cstddef>
#include <vector>
namespace astrabot::nav::detail {
struct FieldStamp final {
    diagnostics::NavError location{};
    std::uint32_t value{0};
};
// Internal, per-load instrumentation. Never attached to a published snapshot.
struct DecodeContext final {
    std::size_t maximum{0};
    std::size_t used{0};
    std::size_t base{0};
    std::uint32_t version{1};
    std::vector<FieldStamp> fields{};
    diagnostics::NavError charge(std::size_t count, std::size_t width,
                                 diagnostics::NavError at) noexcept;
    diagnostics::NavError observe(std::size_t offset, diagnostics::NavRecord record,
                                  diagnostics::NavField field, std::uint32_t value) noexcept;
};
} // namespace astrabot::nav::detail
