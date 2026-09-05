// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace astrabot::nav::model {

enum class NavVersion : std::uint32_t {
    V1 = 1,
    V2 = 2,
    V3 = 3,
    V4 = 4,
    V5 = 5,
};

struct NavFileHeader final {
    NavVersion version{NavVersion::V1};
    std::optional<std::uint32_t> bspSize{};
    std::vector<std::string> places{};
    std::uint32_t areaCount{0};
    std::size_t headerBytes{0};
};

} // namespace astrabot::nav::model
