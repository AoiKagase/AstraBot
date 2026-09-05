// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include "nav/io/area_reader.hpp"
#include "nav/io/byte_reader.hpp"
#include "nav/model/area_records.hpp"

#include <cassert>
#include <cstring>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace {

using astrabot::nav::io::ByteView;
using astrabot::nav::io::NavAreaReadLimits;
using astrabot::nav::io::NavAreaReader;
using astrabot::nav::diagnostics::NavErrorKind;
using astrabot::nav::diagnostics::NavField;
using astrabot::nav::model::NavAreaId;
using astrabot::nav::model::NavAreaRecord;
using astrabot::nav::model::NavExtent;
using astrabot::nav::model::NavVersion;

void appendU8(std::vector<std::uint8_t>& bytes, std::uint8_t value) {
    bytes.push_back(value);
}

void appendU32LE(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xFFU));
}

void appendF32LE(std::vector<std::uint8_t>& bytes, float value) {
    std::uint32_t raw = 0U;
    std::memcpy(&raw, &value, sizeof(raw));
    appendU32LE(bytes, raw);
}

NavAreaReadLimits normalLimits() {
    return NavAreaReadLimits{
        1U,
        2U,
        2U,
        2U,
        2U,
        2U,
        4U,
        2U,
        2U,
        2U,
        2U,
    };
}

std::vector<std::uint8_t> makeV1BasePayload() {
    std::vector<std::uint8_t> bytes{};
    appendU32LE(bytes, 7U);
    appendU8(bytes, 0x05U);
    for (float value = 1.0F; value <= 8.0F; value += 1.0F) {
        appendF32LE(bytes, value);
    }

    appendU32LE(bytes, 1U);
    appendU32LE(bytes, 10U);
    appendU32LE(bytes, 2U);
    appendU32LE(bytes, 20U);
    appendU32LE(bytes, 21U);
    appendU32LE(bytes, 0U);
    appendU32LE(bytes, 1U);
    appendU32LE(bytes, 40U);

    return bytes;
}

void assertTruncatedAt(
    const std::vector<std::uint8_t>& bytes,
    std::size_t offset,
    NavField field) {
    const auto result = NavAreaReader::read(
        ByteView{bytes.data(), offset}, NavVersion::V1, 1U, normalLimits());
    assert(!result);
    assert(!result.value.has_value());
    assert(result.error.kind == NavErrorKind::EndOfInput);
    assert(result.error.offset == offset);
    assert(result.error.field == field);
}

void testPublicAreaRecordContract() {
    const NavAreaRecord area{
        NavAreaId{7U},
        0x05U,
        NavExtent{},
        {},
        {},
        {},
        {},
        std::optional<std::uint16_t>{static_cast<std::uint16_t>(3U)},
    };
    assert(area.id == NavAreaId{7U});
    assert(area.place.has_value() && *area.place == 3U);

    const auto result = NavAreaReader::read(
        ByteView{nullptr, 0U}, NavVersion::V1, 0U, NavAreaReadLimits{});
    assert(!result);
}

void testV1AreaBaseAndConnections() {
    const std::vector<std::uint8_t> bytes = makeV1BasePayload();
    const auto result = NavAreaReader::read(
        ByteView{bytes.data(), bytes.size()}, NavVersion::V1, 1U, normalLimits());
    assert(result);
    assert(result.value->areas.size() == 1U);
    assert(result.value->bytesConsumed == bytes.size());

    const auto& area = result.value->areas.front();
    assert(area.id == NavAreaId{7U});
    assert(area.attributes == 0x05U);
    assert((area.extent.northWest == astrabot::nav::model::NavVector3{1.0F, 2.0F, 3.0F}));
    assert((area.extent.southEast == astrabot::nav::model::NavVector3{4.0F, 5.0F, 6.0F}));
    assert(area.extent.northEastZ == 7.0F);
    assert(area.extent.southWestZ == 8.0F);
    assert(area.connections[0].size() == 1U);
    assert(area.connections[0][0] == NavAreaId{10U});
    assert(area.connections[1].size() == 2U);
    assert(area.connections[1][0] == NavAreaId{20U});
    assert(area.connections[1][1] == NavAreaId{21U});
    assert(area.connections[2].empty());
    assert(area.connections[3].size() == 1U);
    assert(area.connections[3][0] == NavAreaId{40U});
}

void testV1AreaBaseTruncation() {
    const std::vector<std::uint8_t> bytes = makeV1BasePayload();
    const std::vector<std::pair<std::size_t, NavField>> fields{
        {0U, NavField::AreaId},
        {4U, NavField::Attributes},
        {5U, NavField::NorthWestExtent},
        {9U, NavField::NorthWestExtent},
        {13U, NavField::NorthWestExtent},
        {17U, NavField::SouthEastExtent},
        {21U, NavField::SouthEastExtent},
        {25U, NavField::SouthEastExtent},
        {29U, NavField::NorthEastZ},
        {33U, NavField::SouthWestZ},
        {37U, NavField::ConnectionCount},
        {41U, NavField::ConnectionAreaId},
        {45U, NavField::ConnectionCount},
        {49U, NavField::ConnectionAreaId},
        {53U, NavField::ConnectionAreaId},
        {57U, NavField::ConnectionCount},
        {61U, NavField::ConnectionCount},
        {65U, NavField::ConnectionAreaId},
    };
    for (const auto& field : fields) {
        assertTruncatedAt(bytes, field.first, field.second);
    }
}

} // namespace

int main() {
    testPublicAreaRecordContract();
    testV1AreaBaseAndConnections();
    testV1AreaBaseTruncation();
    return 0;
}
