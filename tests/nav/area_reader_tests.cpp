// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include "nav/io/area_reader.hpp"
#include "nav/io/byte_reader.hpp"
#include "nav/model/area_records.hpp"

#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <new>
#include <optional>
#include <utility>
#include <vector>

namespace {

constexpr std::size_t kTestAllocationFailureThreshold = 16U * 1024U * 1024U;

void* allocateForAreaReaderTest(std::size_t size) {
    if (size > kTestAllocationFailureThreshold) {
        throw std::bad_alloc{};
    }
    void* result = std::malloc(size == 0U ? 1U : size);
    if (result == nullptr) {
        throw std::bad_alloc{};
    }
    return result;
}

} // namespace

_Ret_notnull_ _Post_writable_byte_size_(size)
void* operator new(std::size_t size) {
    return allocateForAreaReaderTest(size);
}

_Ret_notnull_ _Post_writable_byte_size_(size)
void* operator new[](std::size_t size) {
    return allocateForAreaReaderTest(size);
}

void operator delete(void* pointer) noexcept {
    std::free(pointer);
}

void operator delete[](void* pointer) noexcept {
    std::free(pointer);
}

void operator delete(void* pointer, std::size_t) noexcept {
    std::free(pointer);
}

void operator delete[](void* pointer, std::size_t) noexcept {
    std::free(pointer);
}

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

void appendU16LE(std::vector<std::uint8_t>& bytes, std::uint16_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
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

void overwriteU32LE(
    std::vector<std::uint8_t>& bytes,
    std::size_t offset,
    std::uint32_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value & 0xFFU);
    bytes[offset + 1U] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
    bytes[offset + 2U] = static_cast<std::uint8_t>((value >> 16U) & 0xFFU);
    bytes[offset + 3U] = static_cast<std::uint8_t>((value >> 24U) & 0xFFU);
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
    appendU32LE(bytes, 0U);
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
    assert(area.connections[0][0].target == NavAreaId{10U});
    assert(area.connections[1].size() == 2U);
    assert(area.connections[1][0].target == NavAreaId{20U});
    assert(area.connections[1][1].target == NavAreaId{21U});
    assert(area.connections[2].empty());
    assert(area.connections[3].size() == 1U);
    assert(area.connections[3][0].target == NavAreaId{40U});
    for (const auto& direction : area.connections)
        for (const auto& edge : direction)
            assert(edge.traversal == astrabot::nav::model::NavTraversalKind::Walk);
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
    bytes.resize(bytes.size() - 6U);
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
    appendU32LE(bytes, 0U);
    return bytes;
}

std::vector<std::uint8_t> makeV2HidingPayload() {
    std::vector<std::uint8_t> bytes = makeV1BasePayload();
    bytes.resize(bytes.size() - 6U);
    appendU8(bytes, 1U);
    appendU32LE(bytes, 101U);
    appendF32LE(bytes, 1.5F);
    appendF32LE(bytes, 2.5F);
    appendF32LE(bytes, 3.5F);
    appendU8(bytes, 0x03U);
    appendU8(bytes, 0U);
    appendU32LE(bytes, 0U);
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

std::vector<std::uint8_t> makeV1LegacyEncounterPayload() {
    std::vector<std::uint8_t> bytes = makeV1BasePayload();
    bytes.resize(bytes.size() - 4U);
    appendU32LE(bytes, 1U);
    appendU32LE(bytes, 7U);
    appendU32LE(bytes, 6U);
    for (float value = 1.0F; value <= 6.0F; value += 1.0F) {
        appendF32LE(bytes, value);
    }
    appendU8(bytes, 1U);
    appendF32LE(bytes, 12.0F);
    appendF32LE(bytes, 13.0F);
    appendF32LE(bytes, 14.0F);
    appendF32LE(bytes, 0.5F);
    appendU8(bytes, 0xEEU);
    return bytes;
}

std::vector<std::uint8_t> makeV3EncounterPayload() {
    std::vector<std::uint8_t> bytes = makeV1BasePayload();
    bytes.resize(bytes.size() - 4U);
    appendU32LE(bytes, 1U);
    appendU32LE(bytes, 7U);
    appendU8(bytes, 1U);
    appendU32LE(bytes, 8U);
    appendU8(bytes, 2U);
    appendU8(bytes, 2U);
    appendU32LE(bytes, 201U);
    appendU8(bytes, 0x11U);
    appendU32LE(bytes, 202U);
    appendU8(bytes, 0x22U);
    return bytes;
}

std::vector<std::uint8_t> makeV4EmptyEncounterPayload() {
    std::vector<std::uint8_t> bytes = makeV1BasePayload();
    return bytes;
}

std::vector<std::uint8_t> makeV5PlacePayload() {
    std::vector<std::uint8_t> bytes = makeV4EmptyEncounterPayload();
    appendU16LE(bytes, 4U);
    return bytes;
}

void testV1LegacyEncounterIsConsumedButNotPublished() {
    const std::vector<std::uint8_t> bytes = makeV1LegacyEncounterPayload();
    const auto result = NavAreaReader::read(
        ByteView{bytes.data(), bytes.size()}, NavVersion::V1, 1U, normalLimits());
    assert(result);
    assert(result.value->bytesConsumed == bytes.size() - 1U);
    assert(result.value->areas.front().encounters.empty());
    assert(bytes[result.value->bytesConsumed] == 0xEEU);
}

void testV3Encounter() {
    const std::vector<std::uint8_t> bytes = makeV3EncounterPayload();
    const auto result = NavAreaReader::read(
        ByteView{bytes.data(), bytes.size()}, NavVersion::V3, 1U, normalLimits());
    assert(result);
    assert(result.value->bytesConsumed == bytes.size());
    const auto& encounter = result.value->areas.front().encounters.front();
    assert(encounter.from == NavAreaId{7U});
    assert(encounter.fromDirection == 1U);
    assert(encounter.to == NavAreaId{8U});
    assert(encounter.toDirection == 2U);
    assert(encounter.spots.size() == 2U);
    assert(encounter.spots[0].hidingSpotId == 201U);
    assert(encounter.spots[0].t == 0x11U);
    assert(encounter.spots[1].hidingSpotId == 202U);
    assert(encounter.spots[1].t == 0x22U);
}

void testV4AndV5PlaceEntries() {
    const std::vector<std::uint8_t> v4 = makeV4EmptyEncounterPayload();
    const auto v4Result = NavAreaReader::read(
        ByteView{v4.data(), v4.size()}, NavVersion::V4, 1U, normalLimits());
    assert(v4Result);
    assert(!v4Result.value->areas.front().place.has_value());
    assert(v4Result.value->bytesConsumed == v4.size());

    const std::vector<std::uint8_t> v5 = makeV5PlacePayload();
    const auto v5Result = NavAreaReader::read(
        ByteView{v5.data(), v5.size()}, NavVersion::V5, 1U, normalLimits());
    assert(v5Result);
    assert(v5Result.value->areas.front().place.has_value());
    assert(*v5Result.value->areas.front().place == 4U);
    assert(v5Result.value->bytesConsumed == v5.size());
}

void testEncounterAndPlaceTruncation() {
    const std::vector<std::uint8_t> legacy = makeV1LegacyEncounterPayload();
    const std::vector<std::pair<std::size_t, NavField>> legacyFields{
        {71U, NavField::EncounterCount},
        {75U, NavField::EncounterAreaId},
        {79U, NavField::EncounterAreaId},
        {83U, NavField::RawBytes},
        {87U, NavField::RawBytes},
        {91U, NavField::RawBytes},
        {95U, NavField::RawBytes},
        {99U, NavField::RawBytes},
        {103U, NavField::RawBytes},
        {107U, NavField::EncounterSpotCount},
        {108U, NavField::RawBytes},
        {112U, NavField::RawBytes},
        {116U, NavField::RawBytes},
        {120U, NavField::RawBytes},
    };
    for (const auto& field : legacyFields) {
        assertTruncatedAt(legacy, NavVersion::V1, field.first, field.second);
    }

    const std::vector<std::uint8_t> modern = makeV3EncounterPayload();
    const std::vector<std::pair<std::size_t, NavField>> modernFields{
        {71U, NavField::EncounterCount},
        {75U, NavField::EncounterAreaId},
        {79U, NavField::EncounterDirection},
        {80U, NavField::EncounterAreaId},
        {84U, NavField::EncounterDirection},
        {85U, NavField::EncounterSpotCount},
        {86U, NavField::EncounterSpotId},
        {90U, NavField::EncounterSpotT},
        {91U, NavField::EncounterSpotId},
        {95U, NavField::EncounterSpotT},
    };
    for (const auto& field : modernFields) {
        assertTruncatedAt(modern, NavVersion::V3, field.first, field.second);
    }

    const std::vector<std::uint8_t> place = makeV5PlacePayload();
    assertTruncatedAt(place, NavVersion::V5, 75U, NavField::Place);
}

void testEncounterLimits() {
    const std::vector<std::uint8_t> modern = makeV3EncounterPayload();
    NavAreaReadLimits noAreaEncounters = normalLimits();
    noAreaEncounters.maxEncountersPerArea = 0U;
    auto result = NavAreaReader::read(
        ByteView{modern.data(), modern.size()}, NavVersion::V3, 1U, noAreaEncounters);
    assert(!result);
    assert(result.error.kind == NavErrorKind::CountLimitExceeded);
    assert(result.error.offset == 71U);
    assert(result.error.field == NavField::EncounterCount);

    NavAreaReadLimits noTotalEncounters = normalLimits();
    noTotalEncounters.maxTotalEncounters = 0U;
    result = NavAreaReader::read(
        ByteView{modern.data(), modern.size()}, NavVersion::V3, 1U, noTotalEncounters);
    assert(!result);
    assert(result.error.kind == NavErrorKind::CountLimitExceeded);
    assert(result.error.offset == 71U);
    assert(result.error.field == NavField::EncounterCount);

    NavAreaReadLimits oneSpot = normalLimits();
    oneSpot.maxEncounterSpotsPerPath = 1U;
    result = NavAreaReader::read(
        ByteView{modern.data(), modern.size()}, NavVersion::V3, 1U, oneSpot);
    assert(!result);
    assert(result.error.kind == NavErrorKind::CountLimitExceeded);
    assert(result.error.offset == 85U);
    assert(result.error.field == NavField::EncounterSpotCount);

    NavAreaReadLimits oneTotalSpot = normalLimits();
    oneTotalSpot.maxTotalEncounterSpots = 1U;
    result = NavAreaReader::read(
        ByteView{modern.data(), modern.size()}, NavVersion::V3, 1U, oneTotalSpot);
    assert(!result);
    assert(result.error.kind == NavErrorKind::CountLimitExceeded);
    assert(result.error.offset == 85U);
    assert(result.error.field == NavField::EncounterSpotCount);
}

void testGlobalLimitsAndTransactionalFailures() {
    const std::vector<std::uint8_t> base = makeV1BasePayload();

    const auto zeroArea = NavAreaReader::read(
        ByteView{base.data(), base.size()}, NavVersion::V1, 0U, normalLimits());
    assert(!zeroArea);
    assert(!zeroArea.value.has_value());
    assert(zeroArea.error.kind == NavErrorKind::InvalidValue);
    assert(zeroArea.error.offset == 0U);
    assert(zeroArea.error.field == NavField::AreaCount);

    const auto tooManyAreas = NavAreaReader::read(
        ByteView{base.data(), base.size()}, NavVersion::V1, 2U, normalLimits());
    assert(!tooManyAreas);
    assert(!tooManyAreas.value.has_value());
    assert(tooManyAreas.error.kind == NavErrorKind::CountLimitExceeded);
    assert(tooManyAreas.error.offset == 0U);
    assert(tooManyAreas.error.field == NavField::AreaCount);

    NavAreaReadLimits noConnections = normalLimits();
    noConnections.maxConnectionsPerDirection = 0U;
    auto result = NavAreaReader::read(
        ByteView{base.data(), base.size()}, NavVersion::V1, 1U, noConnections);
    assert(!result);
    assert(result.error.kind == NavErrorKind::CountLimitExceeded);
    assert(result.error.offset == 37U);
    assert(result.error.field == NavField::ConnectionCount);

    NavAreaReadLimits noTotalConnections = normalLimits();
    noTotalConnections.maxTotalConnections = 0U;
    result = NavAreaReader::read(
        ByteView{base.data(), base.size()}, NavVersion::V1, 1U, noTotalConnections);
    assert(!result);
    assert(result.error.kind == NavErrorKind::CountLimitExceeded);
    assert(result.error.offset == 37U);
    assert(result.error.field == NavField::ConnectionCount);

    const std::vector<std::uint8_t> hiding = makeV1HidingApproachPayload();
    NavAreaReadLimits noHiding = normalLimits();
    noHiding.maxHidingSpotsPerArea = 0U;
    result = NavAreaReader::read(
        ByteView{hiding.data(), hiding.size()}, NavVersion::V1, 1U, noHiding);
    assert(!result);
    assert(result.error.kind == NavErrorKind::CountLimitExceeded);
    assert(result.error.offset == 69U);
    assert(result.error.field == NavField::HidingSpotCount);

    NavAreaReadLimits noTotalHiding = normalLimits();
    noTotalHiding.maxTotalHidingSpots = 0U;
    result = NavAreaReader::read(
        ByteView{hiding.data(), hiding.size()}, NavVersion::V1, 1U, noTotalHiding);
    assert(!result);
    assert(result.error.kind == NavErrorKind::CountLimitExceeded);
    assert(result.error.offset == 69U);
    assert(result.error.field == NavField::HidingSpotCount);

    NavAreaReadLimits noApproaches = normalLimits();
    noApproaches.maxApproachesPerArea = 0U;
    result = NavAreaReader::read(
        ByteView{hiding.data(), hiding.size()}, NavVersion::V1, 1U, noApproaches);
    assert(!result);
    assert(result.error.kind == NavErrorKind::CountLimitExceeded);
    assert(result.error.offset == 82U);
    assert(result.error.field == NavField::ApproachCount);

    NavAreaReadLimits noTotalApproaches = normalLimits();
    noTotalApproaches.maxTotalApproaches = 0U;
    result = NavAreaReader::read(
        ByteView{hiding.data(), hiding.size()}, NavVersion::V1, 1U, noTotalApproaches);
    assert(!result);
    assert(result.error.kind == NavErrorKind::CountLimitExceeded);
    assert(result.error.offset == 82U);
    assert(result.error.field == NavField::ApproachCount);

    const auto invalidBytes = NavAreaReader::read(
        ByteView{nullptr, 1U}, NavVersion::V1, 1U, normalLimits());
    assert(!invalidBytes);
    assert(!invalidBytes.value.has_value());
    assert(invalidBytes.error.kind == NavErrorKind::InvalidInput);
    assert(invalidBytes.error.offset == 0U);
    assert(invalidBytes.error.field == NavField::AreaId);

    std::vector<std::uint8_t> nonFinite = base;
    overwriteU32LE(nonFinite, 5U, 0x7FC00000U);
    const auto nonFiniteResult = NavAreaReader::read(
        ByteView{nonFinite.data(), nonFinite.size()}, NavVersion::V1, 1U, normalLimits());
    assert(!nonFiniteResult);
    assert(!nonFiniteResult.value.has_value());
    assert(nonFiniteResult.error.kind == NavErrorKind::NonFiniteFloat);
    assert(nonFiniteResult.error.offset == 5U);
    assert(nonFiniteResult.error.field == NavField::NorthWestExtent);

    const auto secondAreaFailure = NavAreaReader::read(
        ByteView{base.data(), base.size()}, NavVersion::V1, 2U,
        NavAreaReadLimits{
            2U, 2U, 2U, 2U, 2U, 2U, 8U, 4U, 4U, 4U, 4U,
        });
    assert(!secondAreaFailure);
    assert(!secondAreaFailure.value.has_value());
    assert(secondAreaFailure.error.kind == NavErrorKind::EndOfInput);
    assert(secondAreaFailure.error.offset == base.size());
    assert(secondAreaFailure.error.record == astrabot::nav::diagnostics::NavRecord::Area);
    assert(secondAreaFailure.error.field == NavField::AreaId);
    std::vector<std::uint8_t> allocationFailurePayload = base;
    overwriteU32LE(allocationFailurePayload, 37U, 5000000U);
    NavAreaReadLimits allocationLimits = normalLimits();
    allocationLimits.maxConnectionsPerDirection = 5000000U;
    allocationLimits.maxTotalConnections = 5000000U;
    const auto allocationResult = NavAreaReader::read(
        ByteView{allocationFailurePayload.data(), allocationFailurePayload.size()},
        NavVersion::V1, 1U, allocationLimits);
    assert(!allocationResult);
    assert(!allocationResult.value.has_value());
    assert(allocationResult.error.kind == NavErrorKind::AllocationFailure);
    assert(allocationResult.error.offset == 37U);
    assert(allocationResult.error.field == NavField::ConnectionCount);
}

} // namespace

int main() {
    testPublicAreaRecordContract();
    testV1AreaBaseAndConnections();
    testV1AreaBaseTruncation();
    testV1HidingAndApproach();
    testV2Hiding();
    testHidingAndApproachTruncation();
    testV1LegacyEncounterIsConsumedButNotPublished();
    testV3Encounter();
    testV4AndV5PlaceEntries();
    testEncounterAndPlaceTruncation();
    testEncounterLimits();
    testGlobalLimitsAndTransactionalFailures();
    return 0;
}
