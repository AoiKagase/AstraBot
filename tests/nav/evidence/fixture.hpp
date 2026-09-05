// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "nav/io/mesh_loader.hpp"
#include <cassert>
#include <cstring>
#include <vector>

namespace evidence {
namespace nav = astrabot::nav;
using K = nav::diagnostics::NavErrorKind;
using F = nav::diagnostics::NavField;
using R = nav::diagnostics::NavRecord;
using Bytes = std::vector<std::uint8_t>;
struct Span { std::size_t offset, width; R record; F field; bool floating; };
struct Fixture {
    Bytes bytes;
    std::vector<Span> spans; // Independent encoder field boundaries, never reader output.
    void integer(std::uint32_t value, std::size_t width, R record, F field,
                 bool floating = false) {
        spans.push_back({bytes.size(), width, record, field, floating});
        for (std::size_t i = 0; i < width; ++i)
            bytes.push_back(static_cast<std::uint8_t>(value >> (8U * i)));
    }
    void real(float value, R record, F field) {
        std::uint32_t bits = 0;
        static_assert(sizeof(bits) == sizeof(value));
        std::memcpy(&bits, &value, sizeof(bits));
        integer(bits, 4, record, field, true);
    }
};
inline nav::io::NavMeshReadLimits limits() {
    return {65536, {128, 64, 256, 16384},
            {128, 128, 64, 64, 64, 64, 4096, 4096, 4096, 4096, 4096}, 4194304};
}
inline auto load(const Bytes& b, nav::io::NavMeshReadLimits cap = limits()) {
    return nav::io::NavMeshLoader::load({b.data(), b.size()}, cap);
}
inline void set(Bytes& bytes, const Span& s, std::uint32_t value) {
    assert(s.width <= 4 && s.offset + s.width <= bytes.size());
    for (std::size_t i = 0; i < s.width; ++i)
        bytes[s.offset + i] = static_cast<std::uint8_t>(value >> (8U * i));
}
inline Fixture fixture(unsigned version, bool full) {
    assert(version >= 1 && version <= 5);
    Fixture f;
    f.integer(0xFEEDFACE, 4, R::FileHeader, F::Magic);
    f.integer(version, 4, R::FileHeader, F::Version);
    if (version >= 4) f.integer(123, 4, R::FileHeader, F::BspSize);
    if (version == 5) {
        f.integer(full ? 1U : 0U, 2, R::PlaceDictionary, F::PlaceCount);
        if (full) {
            f.integer(3, 2, R::PlaceDictionary, F::PlaceLength);
            f.spans.push_back({f.bytes.size(), 3, R::PlaceDictionary, F::PlaceText, false});
            f.bytes.insert(f.bytes.end(), {0xFF, 0x41, 0});
        }
    }
    f.integer(full ? 2U : 1U, 4, R::FileHeader, F::AreaCount);
    for (unsigned id = 1; id <= (full ? 2U : 1U); ++id) {
        const float x = id == 1 ? 0.f : 3.f, z = id == 1 ? 0.f : 4.f;
        f.integer(id, 4, R::Area, F::AreaId);
        f.integer(0xFF, 1, R::Area, F::Attributes);
        for (float v : {x, 0.f, z}) f.real(v, R::Area, F::NorthWestExtent);
        for (float v : {x + 2, 2.f, z}) f.real(v, R::Area, F::SouthEastExtent);
        f.real(z, R::Area, F::NorthEastZ); f.real(z, R::Area, F::SouthWestZ);
        for (unsigned direction = 0; direction < 4; ++direction) {
            f.integer(full ? 1U : 0U, 4, R::Connection, F::ConnectionCount);
            if (full) f.integer(3U - id, 4, R::Connection, F::ConnectionAreaId);
        }
        f.integer(full ? 1U : 0U, 1, R::HidingSpot, F::HidingSpotCount);
        if (full) {
            if (version >= 2) f.integer(100 + id, 4, R::HidingSpot, F::HidingSpotId);
            for (float v : {x + 1, 1.f, z}) f.real(v, R::HidingSpot, F::RawBytes);
            if (version >= 2) f.integer(0xFE, 1, R::HidingSpot, F::HidingSpotFlags);
        }
        f.integer(full ? 1U : 0U, 1, R::Approach, F::ApproachCount);
        if (full) {
            f.integer(id, 4, R::Approach, F::ApproachAreaId);
            f.integer(id, 4, R::Approach, F::ApproachAreaId);
            f.integer(0xFE, 1, R::Approach, F::ApproachTraversal);
            f.integer(id, 4, R::Approach, F::ApproachAreaId);
            f.integer(0xFF, 1, R::Approach, F::ApproachTraversal);
        }
        f.integer(full ? 1U : 0U, 4, R::Encounter, F::EncounterCount);
        if (full && version <= 2) {
            f.integer(0, 4, R::Encounter, F::EncounterAreaId);
            f.integer(999, 4, R::Encounter, F::EncounterAreaId);
            for (unsigned i = 0; i < 6; ++i) f.real(0, R::Encounter, F::RawBytes);
            f.integer(1, 1, R::Encounter, F::EncounterSpotCount);
            for (float v : {1.f, 2.f, 3.f, .5f}) f.real(v, R::Encounter, F::RawBytes);
        } else if (full) {
            f.integer(id, 4, R::Encounter, F::EncounterAreaId);
            f.integer(0, 1, R::Encounter, F::EncounterDirection);
            f.integer(id, 4, R::Encounter, F::EncounterAreaId);
            f.integer(3, 1, R::Encounter, F::EncounterDirection);
            f.integer(1, 1, R::Encounter, F::EncounterSpotCount);
            f.integer(100 + id, 4, R::Encounter, F::EncounterSpotId);
            f.integer(255, 1, R::Encounter, F::EncounterSpotT);
        }
        if (version == 5) f.integer(full ? 1U : 0U, 2, R::Area, F::Place);
    }
    return f;
}
} // namespace evidence
