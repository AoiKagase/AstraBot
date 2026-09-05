// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include "nav/io/nav_reader.hpp"

#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

namespace {

using astrabot::nav::diagnostics::NavErrorKind;
using astrabot::nav::diagnostics::NavField;
using astrabot::nav::diagnostics::NavRecord;
using astrabot::nav::io::ByteView;
using astrabot::nav::io::NavFileReader;
using astrabot::nav::io::NavReadLimits;
using astrabot::nav::model::NavVersion;

constexpr std::uint32_t kMagic = 0xFEEDFACEU;

void appendU16(std::vector<std::uint8_t>& bytes, std::uint16_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
}

void appendU32(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xFFU));
}

void appendPlace(std::vector<std::uint8_t>& bytes, const std::string& text) {
    appendU16(bytes, static_cast<std::uint16_t>(text.size() + 1U));
    bytes.insert(bytes.end(), text.begin(), text.end());
    bytes.push_back(0U);
}

std::vector<std::uint8_t> makeHeader(
    std::uint32_t version,
    std::uint32_t areaCount = 1U,
    const std::vector<std::string>& places = {},
    std::uint32_t bspSize = 0x12345678U) {
    std::vector<std::uint8_t> bytes;
    appendU32(bytes, kMagic);
    appendU32(bytes, version);
    if (version >= 4U) {
        appendU32(bytes, bspSize);
    }
    if (version >= 5U) {
        appendU16(bytes, static_cast<std::uint16_t>(places.size()));
        for (const std::string& place : places) {
            appendPlace(bytes, place);
        }
    }
    appendU32(bytes, areaCount);
    return bytes;
}

NavReadLimits normalLimits() {
    return NavReadLimits{4U, 4U, 16U, 32U};
}

void testMinimalVersionsAndOptionalFields() {
    for (std::uint32_t version = 1U; version <= 5U; ++version) {
        const std::vector<std::string> places = version == 5U
            ? std::vector<std::string>{"MID", "\xFF\x01"}
            : std::vector<std::string>{};
        const std::vector<std::uint8_t> bytes = makeHeader(version, 1U, places);
        const auto result = NavFileReader::readHeader(ByteView{bytes.data(), bytes.size()}, normalLimits());

        assert(result);
        assert(result.value->version == static_cast<NavVersion>(version));
        assert(result.value->areaCount == 1U);
        if (version >= 4U) {
            assert(result.value->bspSize.has_value());
            assert(*result.value->bspSize == 0x12345678U);
        } else {
            assert(!result.value->bspSize.has_value());
        }
        if (version == 5U) {
            assert(result.value->places == places);
        } else {
            assert(result.value->places.empty());
        }
        assert(result.value->headerBytes == bytes.size());
    }
}

void testHeaderStopsBeforeAreaPayload() {
    std::vector<std::uint8_t> bytes = makeHeader(5U, 1U, {"A"});
    bytes.push_back(0xCCU);
    const auto result = NavFileReader::readHeader(ByteView{bytes.data(), bytes.size()}, normalLimits());

    assert(result);
    assert(result.value->headerBytes == bytes.size() - 1U);
    assert(bytes[result.value->headerBytes] == 0xCCU);
}

void testWrongMagicAndUnsupportedVersions() {
    std::vector<std::uint8_t> wrongMagic = makeHeader(1U);
    wrongMagic[0] = 0U;
    const auto magicResult = NavFileReader::readHeader(
        ByteView{wrongMagic.data(), wrongMagic.size()}, normalLimits());
    assert(!magicResult);
    assert(magicResult.error.kind == NavErrorKind::InvalidValue);
    assert(magicResult.error.offset == 0U);
    assert(magicResult.error.record == NavRecord::FileHeader);
    assert(magicResult.error.field == NavField::Magic);

    for (const std::uint32_t version : {0U, 6U}) {
        const std::vector<std::uint8_t> bytes = makeHeader(version);
        const auto result = NavFileReader::readHeader(
            ByteView{bytes.data(), bytes.size()}, normalLimits());
        assert(!result);
        assert(result.error.kind == NavErrorKind::UnsupportedValue);
        assert(result.error.offset == 4U);
        assert(result.error.record == NavRecord::FileHeader);
        assert(result.error.field == NavField::Version);
    }
}

void testTruncationReportsFieldOffset() {
    const std::vector<std::uint8_t> bytes = makeHeader(1U);
    for (std::size_t length = 0U; length < bytes.size(); ++length) {
        const auto result = NavFileReader::readHeader(
            ByteView{bytes.data(), length}, normalLimits());
        assert(!result);
        assert(result.error.kind == NavErrorKind::EndOfInput);
        assert(result.error.offset == (length < 4U ? 0U : length < 8U ? 4U : 8U));
        assert(!result.value.has_value());
    }
}

void assertTruncatedField(
    const std::vector<std::uint8_t>& bytes,
    std::size_t length,
    std::uint64_t offset,
    NavField field) {
    const std::vector<std::uint8_t> truncated(bytes.begin(), bytes.begin() + length);
    const auto result = NavFileReader::readHeader(
        ByteView{truncated.data(), truncated.size()}, normalLimits());
    assert(!result);
    assert(result.error.kind == NavErrorKind::EndOfInput);
    assert(result.error.offset == offset);
    assert(result.error.field == field);
    assert(!result.value.has_value());
}

void testOptionalFieldTruncationReportsFieldOffset() {
    const std::vector<std::uint8_t> v4 = makeHeader(4U);
    assertTruncatedField(v4, 8U, 8U, NavField::BspSize);

    const std::vector<std::uint8_t> v5WithoutPlaces = makeHeader(5U);
    assertTruncatedField(v5WithoutPlaces, 12U, 12U, NavField::PlaceCount);
    assertTruncatedField(v5WithoutPlaces, 14U, 14U, NavField::AreaCount);

    const std::vector<std::uint8_t> v5WithPlace = makeHeader(5U, 1U, {"A"});
    assertTruncatedField(v5WithPlace, 14U, 14U, NavField::PlaceLength);
    assertTruncatedField(v5WithPlace, 16U, 16U, NavField::PlaceText);
    assertTruncatedField(v5WithPlace, 18U, 18U, NavField::AreaCount);
}

void testAreaCountLimits() {
    const std::vector<std::uint8_t> zeroAreas = makeHeader(1U, 0U);
    const auto zeroResult = NavFileReader::readHeader(
        ByteView{zeroAreas.data(), zeroAreas.size()}, normalLimits());
    assert(!zeroResult);
    assert(zeroResult.error.kind == NavErrorKind::InvalidValue);
    assert(zeroResult.error.offset == 8U);
    assert(zeroResult.error.field == NavField::AreaCount);

    const std::vector<std::uint8_t> tooManyAreas = makeHeader(1U, 5U);
    const auto limitResult = NavFileReader::readHeader(
        ByteView{tooManyAreas.data(), tooManyAreas.size()}, normalLimits());
    assert(!limitResult);
    assert(limitResult.error.kind == NavErrorKind::CountLimitExceeded);
    assert(limitResult.error.offset == 8U);
    assert(limitResult.error.field == NavField::AreaCount);
}

void testPlaceValidationAndLimits() {
    const std::vector<std::uint8_t> tooManyPlaces = makeHeader(5U, 1U, {"A", "B"});
    const auto placeCountResult = NavFileReader::readHeader(
        ByteView{tooManyPlaces.data(), tooManyPlaces.size()},
        NavReadLimits{4U, 1U, 16U, 32U});
    assert(!placeCountResult);
    assert(placeCountResult.error.kind == NavErrorKind::CountLimitExceeded);
    assert(placeCountResult.error.offset == 12U);
    assert(placeCountResult.error.field == NavField::PlaceCount);

    std::vector<std::uint8_t> zeroLength = makeHeader(5U, 1U, {""});
    zeroLength[14] = 0U;
    zeroLength[15] = 0U;
    const auto zeroLengthResult = NavFileReader::readHeader(
        ByteView{zeroLength.data(), zeroLength.size()}, normalLimits());
    assert(!zeroLengthResult);
    assert(zeroLengthResult.error.kind == NavErrorKind::InvalidValue);
    assert(zeroLengthResult.error.offset == 14U);
    assert(zeroLengthResult.error.field == NavField::PlaceLength);

    std::vector<std::uint8_t> unterminated = makeHeader(5U, 1U, {"A"});
    unterminated[17] = 0x42U;
    const auto unterminatedResult = NavFileReader::readHeader(
        ByteView{unterminated.data(), unterminated.size()}, normalLimits());
    assert(!unterminatedResult);
    assert(unterminatedResult.error.kind == NavErrorKind::InvalidValue);
    assert(unterminatedResult.error.offset == 16U);
    assert(unterminatedResult.error.field == NavField::PlaceText);

    const std::string embeddedPlace("A\0", 2U);
    const std::vector<std::uint8_t> embeddedNul = makeHeader(
        5U,
        1U,
        {embeddedPlace});
    const auto embeddedNulResult = NavFileReader::readHeader(
        ByteView{embeddedNul.data(), embeddedNul.size()}, normalLimits());
    assert(!embeddedNulResult);
    assert(embeddedNulResult.error.kind == NavErrorKind::InvalidValue);
    assert(embeddedNulResult.error.offset == 16U);
    assert(embeddedNulResult.error.field == NavField::PlaceText);

    const std::vector<std::uint8_t> tooLong = makeHeader(
        5U,
        1U,
        {std::string(16U, '1')});
    const auto tooLongResult = NavFileReader::readHeader(
        ByteView{tooLong.data(), tooLong.size()}, normalLimits());
    assert(!tooLongResult);
    assert(tooLongResult.error.kind == NavErrorKind::CountLimitExceeded);
    assert(tooLongResult.error.offset == 14U);
    assert(tooLongResult.error.field == NavField::PlaceLength);
}

void testAllocationBudgetRejectsPartialHeader() {
    const std::vector<std::uint8_t> bytes = makeHeader(5U, 1U, {"AA", "BB"});
    const auto result = NavFileReader::readHeader(
        ByteView{bytes.data(), bytes.size()},
        NavReadLimits{4U, 4U, 16U, 3U});
    assert(!result);
    assert(result.error.kind == NavErrorKind::AllocationFailure);
    assert(!result.value.has_value());
}

} // namespace

int main() {
    testMinimalVersionsAndOptionalFields();
    testHeaderStopsBeforeAreaPayload();
    testWrongMagicAndUnsupportedVersions();
    testTruncationReportsFieldOffset();
    testOptionalFieldTruncationReportsFieldOffset();
    testAreaCountLimits();
    testPlaceValidationAndLimits();
    testAllocationBudgetRejectsPartialHeader();
    return 0;
}
