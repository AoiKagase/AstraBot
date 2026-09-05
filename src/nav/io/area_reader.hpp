// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#pragma once

#include "nav/diagnostics/error.hpp"
#include "nav/io/byte_reader.hpp"
#include "nav/model/area_records.hpp"
#include "nav/model/file_header.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace astrabot::nav::io {

struct NavAreaReadLimits final {
    std::uint32_t maxAreas{0};
    std::uint32_t maxConnectionsPerDirection{0};
    std::uint32_t maxHidingSpotsPerArea{0};
    std::uint32_t maxApproachesPerArea{0};
    std::uint32_t maxEncountersPerArea{0};
    std::uint32_t maxEncounterSpotsPerPath{0};
    std::uint32_t maxTotalConnections{0};
    std::uint32_t maxTotalHidingSpots{0};
    std::uint32_t maxTotalApproaches{0};
    std::uint32_t maxTotalEncounters{0};
    std::uint32_t maxTotalEncounterSpots{0};
};

struct NavAreaBlock final {
    std::vector<model::NavAreaRecord> areas{};
    std::size_t bytesConsumed{0};
};

class NavAreaReader final {
public:
    static diagnostics::ReadResult<NavAreaBlock> read(
        ByteView bytes,
        model::NavVersion version,
        std::uint32_t areaCount,
        const NavAreaReadLimits& limits) noexcept;
private:
    friend class NavMeshLoader;
    static diagnostics::ReadResult<NavAreaBlock> readTracked(
        ByteView bytes, model::NavVersion version, std::uint32_t areaCount,
        const NavAreaReadLimits& limits, detail::DecodeContext* context) noexcept;
};

} // namespace astrabot::nav::io
