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

    appendU8(bytes, 0U);
    appendU8(bytes, 0U);
    return bytes;
}

void assertTruncatedAt(
    const std::vector<std::uint8_t>& bytes,
    NavVersion version,
    std::size_t offset,
    NavField field) {
    const auto result = NavAreaReader::read(
        ByteView{bytes.data(), offset}, version, 1U, normalLimits());
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
        assertTruncatedAt(bytes, NavVersion::V1, field.first, field.second);
    }
}

std::vector<std::uint8_t> makeV1HidingApproachPayload() {
    std::vector<std::uint8_t> bytes = makeV1BasePayload();
    bytes.resize(bytes.size() - 2U);
    appendU8(bytes, 1U);
    appendF32LE(bytes, 9.0F);
    appendF32LE(bytes, 10.0F);
    appendF32LE(bytes, 11.0F);
    appendU8(bytes, 1U);
    appendU32LE(bytes, 7U);
    appendU32LE(bytes, 6U);
    appendU8(bytes, 0xA1U);
    appendU32LE(bytes, 8U);
    appendU8(bytes, 0xB2U);
    return bytes;
}

std::vector<std::uint8_t> makeV2HidingPayload() {
    std::vector<std::uint8_t> bytes = makeV1BasePayload();
    bytes.resize(bytes.size() - 2U);
    appendU8(bytes, 1U);
    appendU32LE(bytes, 101U);
    appendF32LE(bytes, 1.5F);
    appendF32LE(bytes, 2.5F);
    appendF32LE(bytes, 3.5F);
    appendU8(bytes, 0x03U);
    appendU8(bytes, 0U);
    return bytes;
}

void testV1HidingAndApproach() {
    const std::vector<std::uint8_t> bytes = makeV1HidingApproachPayload();
    const auto result = NavAreaReader::read(
        ByteView{bytes.data(), bytes.size()}, NavVersion::V1, 1U, normalLimits());
    assert(result);
    assert(result.value->bytesConsumed == bytes.size());
    const auto& area = result.value->areas.front();
    assert(area.hidingSpots.size() == 1U);
    assert(!area.hidingSpots[0].id.has_value());
    assert(!area.hidingSpots[0].flags.has_value());
    assert((area.hidingSpots[0].position == astrabot::nav::model::NavVector3{9.0F, 10.0F, 11.0F}));
    assert(area.approaches.size() == 1U);
    assert(area.approaches[0].here == NavAreaId{7U});
    assert(area.approaches[0].previous == NavAreaId{6U});
    assert(area.approaches[0].previousToHereHow == 0xA1U);
    assert(area.approaches[0].next == NavAreaId{8U});
    assert(area.approaches[0].hereToNextHow == 0xB2U);
}

void testV2Hiding() {
    const std::vector<std::uint8_t> bytes = makeV2HidingPayload();
    const auto result = NavAreaReader::read(
        ByteView{bytes.data(), bytes.size()}, NavVersion::V2, 1U, normalLimits());
    assert(result);
    assert(result.value->bytesConsumed == bytes.size());
    const auto& hiding = result.value->areas.front().hidingSpots.front();
    assert(hiding.id.has_value() && *hiding.id == 101U);
    assert((hiding.position == astrabot::nav::model::NavVector3{1.5F, 2.5F, 3.5F}));
    assert(hiding.flags.has_value() && *hiding.flags == 0x03U);
    assert(result.value->areas.front().approaches.empty());
}

void testHidingAndApproachTruncation() {
    const std::vector<std::uint8_t> v1 = makeV1HidingApproachPayload();
    const std::vector<std::pair<std::size_t, NavField>> v1Fields{
        {69U, NavField::HidingSpotCount},
        {70U, NavField::RawBytes},
        {74U, NavField::RawBytes},
        {78U, NavField::RawBytes},
        {82U, NavField::ApproachCount},
        {83U, NavField::ApproachAreaId},
        {87U, NavField::ApproachAreaId},
        {91U, NavField::ApproachTraversal},
        {92U, NavField::ApproachAreaId},
        {96U, NavField::ApproachTraversal},
    };
    for (const auto& field : v1Fields) {
        assertTruncatedAt(v1, NavVersion::V1, field.first, field.second);
    }

    const std::vector<std::uint8_t> v2 = makeV2HidingPayload();
    const std::vector<std::pair<std::size_t, NavField>> v2Fields{
        {69U, NavField::HidingSpotCount},
        {70U, NavField::HidingSpotId},
        {74U, NavField::RawBytes},
        {78U, NavField::RawBytes},
        {82U, NavField::RawBytes},
        {86U, NavField::HidingSpotFlags},
        {87U, NavField::ApproachCount},
    };
    for (const auto& field : v2Fields) {
        assertTruncatedAt(v2, NavVersion::V2, field.first, field.second);
    }
}

} // namespace

int main() {
    testPublicAreaRecordContract();
    testV1AreaBaseAndConnections();
    testV1AreaBaseTruncation();
    testV1HidingAndApproach();
    testV2Hiding();
    testHidingAndApproachTruncation();
    return 0;
}
