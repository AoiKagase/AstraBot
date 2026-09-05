// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "nav/io/mesh_loader.hpp"
#include <array>
#include <cassert>
#include <cstring>

namespace route_test {
namespace model = astrabot::nav::model;
struct Area {
    std::uint32_t id;
    model::NavExtent extent;
    std::array<std::vector<std::uint32_t>, 4> targets{};
};
inline std::shared_ptr<const model::NavMeshSnapshot> snapshot(const std::vector<Area> &areas) {
    std::vector<std::uint8_t> bytes;
    const auto append = [&bytes](std::uint32_t n) {
        for (unsigned i = 0; i < 4; ++i)
            bytes.push_back(static_cast<std::uint8_t>(n >> (8 * i)));
    };
    append(0xFEEDFACE);
    append(1);
    assert(areas.size() <= 1000);
    append(static_cast<std::uint32_t>(areas.size()));
    for (const auto &area : areas) {
        append(area.id);
        bytes.push_back(0);
        const auto &e = area.extent;
        for (float f : {e.northWest.x, e.northWest.y, e.northWest.z, e.southEast.x,
                        e.southEast.y, e.southEast.z, e.northEastZ, e.southWestZ}) {
            std::uint32_t bits;
            static_assert(sizeof(bits) == sizeof(f));
            std::memcpy(&bits, &f, sizeof(bits));
            append(bits);
        }
        for (const auto &targets : area.targets) {
            assert(targets.size() <= 1000);
            append(static_cast<std::uint32_t>(targets.size()));
            for (auto id : targets)
                append(id);
        }
        bytes.push_back(0); // hiding count
        bytes.push_back(0); // approach count
        append(0);         // legacy encounter count
    }
    auto r = astrabot::nav::io::NavMeshLoader::load(
        {bytes.data(), bytes.size()},
        {1000000, {1000, 0, 0, 0}, {1000, 1000, 0, 0, 0, 0, 4000, 0, 0, 0, 0}, 1000000});
    assert(r);
    return *r.value;
}
} // namespace route_test
