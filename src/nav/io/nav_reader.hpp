// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#pragma once

#include "nav/io/byte_reader.hpp"
#include "nav/model/file_header.hpp"

#include <cstdint>

namespace astrabot::nav::io {

struct NavReadLimits final {
    std::uint32_t maxAreas{0};
    std::uint16_t maxPlaces{0};
    std::uint16_t maxPlaceBytes{0};
    std::uint32_t maxTotalPlaceBytes{0};
};

class NavFileReader final {
public:
    static diagnostics::ReadResult<model::NavFileHeader> readHeader(
        ByteView bytes,
        const NavReadLimits& limits) noexcept;
};

} // namespace astrabot::nav::io
