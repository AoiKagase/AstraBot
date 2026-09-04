// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include "nav/io/byte_reader.hpp"

#include <cassert>
#include <cstdint>
#include <limits>

namespace {

using astrabot::nav::diagnostics::NavErrorKind;
using astrabot::nav::diagnostics::NavField;
using astrabot::nav::diagnostics::NavRecord;
using astrabot::nav::io::ByteReader;
using astrabot::nav::io::ByteView;

void testLittleEndianReads() {
    const std::uint8_t bytes[] = {
        0x7FU,
        0x34U,
        0x12U,
        0x78U,
        0x56U,
        0x34U,
        0x12U,
        0x00U,
        0x00U,
        0x80U,
        0x3FU,
    };
    ByteReader reader(ByteView{bytes, sizeof(bytes)});

    const auto byte = reader.readU8(NavRecord::Area, NavField::Attributes);
    const auto word = reader.readU16LE(NavRecord::Area, NavField::AreaId);
    const auto dword = reader.readU32LE(NavRecord::Area, NavField::AreaId);
    const auto real = reader.readF32LE(NavRecord::Area, NavField::NorthEastZ);

    assert(byte && *byte.value == 0x7FU);
    assert(word && *word.value == 0x1234U);
    assert(dword && *dword.value == 0x12345678U);
    assert(real && *real.value == 1.0F);
    assert(reader.offset() == sizeof(bytes));
    assert(reader.remaining() == 0U);
}

void testExactEofAndTruncation() {
    const std::uint8_t bytes[] = {0x01U, 0x02U, 0x03U, 0x04U};
    ByteReader exact(ByteView{bytes, sizeof(bytes)});
    assert(exact.readBytes(sizeof(bytes)));
    const auto eof = exact.readU8(NavRecord::Area, NavField::AreaId);
    assert(!eof);
    assert(eof.error.kind == NavErrorKind::EndOfInput);
    assert(eof.error.offset == sizeof(bytes));
    assert(exact.offset() == sizeof(bytes));

    for (std::size_t length = 0U; length < sizeof(std::uint16_t); ++length) {
        ByteReader reader(ByteView{bytes, length});
        const auto result = reader.readU16LE(NavRecord::Area, NavField::AreaId);
        assert(!result);
        assert(result.error.kind == NavErrorKind::EndOfInput);
        assert(result.error.offset == 0U);
        assert(reader.offset() == 0U);
    }

    for (std::size_t length = 0U; length < sizeof(std::uint32_t); ++length) {
        ByteReader reader(ByteView{bytes, length});
        const auto result = reader.readU32LE(NavRecord::Area, NavField::AreaId);
        assert(!result);
        assert(result.error.kind == NavErrorKind::EndOfInput);
        assert(result.error.offset == 0U);
        assert(reader.offset() == 0U);
    }
}

void testRangeOverflow() {
    const std::uint8_t byte = 0x01U;
    ByteReader reader(ByteView{
        &byte,
        std::numeric_limits<std::size_t>::max(),
    });
    assert(reader.readBytes(1U));

    const auto overflow = reader.readBytes(
        std::numeric_limits<std::size_t>::max(),
        NavRecord::Area,
        NavField::RawBytes);
    assert(!overflow);
    assert(overflow.error.kind == NavErrorKind::OffsetOverflow);
    assert(overflow.error.offset == 1U);
    assert(reader.offset() == 1U);
}

void testNonFiniteFloatDoesNotAdvance() {
    const std::uint8_t nanBytes[] = {0x00U, 0x00U, 0xC0U, 0x7FU};
    ByteReader reader(ByteView{nanBytes, sizeof(nanBytes)});
    const auto result = reader.readF32LE(
        NavRecord::Area,
        NavField::NorthEastZ);

    assert(!result);
    assert(result.error.kind == NavErrorKind::NonFiniteFloat);
    assert(result.error.offset == 0U);
    assert(result.error.record == NavRecord::Area);
    assert(result.error.field == NavField::NorthEastZ);
    assert(reader.offset() == 0U);
}

void testDeterministicErrors() {
    const std::uint8_t bytes[] = {0x01U};
    ByteReader first(ByteView{bytes, sizeof(bytes)});
    ByteReader second(ByteView{bytes, sizeof(bytes)});

    const auto firstResult = first.readU32LE(
        NavRecord::Connection,
        NavField::ConnectionAreaId);
    const auto secondResult = second.readU32LE(
        NavRecord::Connection,
        NavField::ConnectionAreaId);

    assert(!firstResult);
    assert(!secondResult);
    assert(firstResult.error == secondResult.error);
}

} // namespace

int main() {
    testLittleEndianReads();
    testExactEofAndTruncation();
    testRangeOverflow();
    testNonFiniteFloatDoesNotAdvance();
    testDeterministicErrors();
    return 0;
}
