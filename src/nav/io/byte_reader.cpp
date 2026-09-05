// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include "nav/io/byte_reader.hpp"
#include "nav/io/decode_context.hpp"

#include <cmath>
#include <cstring>
#include <limits>

namespace astrabot::nav::io {

using diagnostics::NavError;
using diagnostics::NavErrorKind;

ByteReader::ByteReader(ByteView bytes) noexcept : bytes_(bytes) {}
ByteReader::ByteReader(ByteView bytes, detail::DecodeContext* context) noexcept
    : bytes_(bytes), context_(context) {}
NavError ByteReader::observe(NavRecord record, NavField field, std::uint32_t value) noexcept {
    if (!context_) return {};
    auto result=context_->observe(offset_,record,field,value);
    if (!result.isNone()) result.offset=static_cast<std::uint64_t>(offset_);
    return result;
}

ReadResult<ByteView> ByteReader::availableBytes(
    std::size_t length,
    NavRecord record,
    NavField field) const noexcept {
    if (!bytes_.isValid()) {
        return ReadResult<ByteView>::failure(
            NavError{NavErrorKind::InvalidInput, static_cast<std::uint64_t>(offset_), record, field});
    }

    if (offset_ > std::numeric_limits<std::size_t>::max() - length) {
        return ReadResult<ByteView>::failure(
            NavError{NavErrorKind::OffsetOverflow, static_cast<std::uint64_t>(offset_), record, field});
    }

    if (length > bytes_.size - offset_) {
        return ReadResult<ByteView>::failure(
            NavError{NavErrorKind::EndOfInput, static_cast<std::uint64_t>(offset_), record, field});
    }

    const std::uint8_t* start = bytes_.data;
    if (length != 0U) {
        start += offset_;
    }
    return ReadResult<ByteView>::success(ByteView{start, length});
}

ReadResult<ByteView> ByteReader::readBytes(
    std::size_t length,
    NavRecord record,
    NavField field) noexcept {
    const ReadResult<ByteView> result = availableBytes(length, record, field);
    if (!result) {
        return result;
    }

    consume(length);
    return result;
}

ReadResult<std::uint8_t> ByteReader::readU8(
    NavRecord record,
    NavField field) noexcept {
    const ReadResult<ByteView> bytes = availableBytes(1U, record, field);
    if (!bytes) {
        return ReadResult<std::uint8_t>::failure(bytes.error);
    }

    const std::uint8_t value = bytes.value->data[0];
    const auto error = observe(record,field,value);
    if (!error.isNone()) return ReadResult<std::uint8_t>::failure(error);
    consume(1U);
    return ReadResult<std::uint8_t>::success(value);
}

std::uint16_t ByteReader::decodeU16LE(ByteView bytes) noexcept {
    return static_cast<std::uint16_t>(bytes.data[0]) |
           static_cast<std::uint16_t>(
               static_cast<std::uint16_t>(bytes.data[1]) << 8U);
}

std::uint32_t ByteReader::decodeU32LE(ByteView bytes) noexcept {
    return static_cast<std::uint32_t>(bytes.data[0]) |
           (static_cast<std::uint32_t>(bytes.data[1]) << 8U) |
           (static_cast<std::uint32_t>(bytes.data[2]) << 16U) |
           (static_cast<std::uint32_t>(bytes.data[3]) << 24U);
}

ReadResult<std::uint16_t> ByteReader::readU16LE(
    NavRecord record,
    NavField field) noexcept {
    const ReadResult<ByteView> bytes = availableBytes(2U, record, field);
    if (!bytes) {
        return ReadResult<std::uint16_t>::failure(bytes.error);
    }

    const std::uint16_t value = decodeU16LE(*bytes.value);
    const auto error = observe(record,field,value);
    if (!error.isNone()) return ReadResult<std::uint16_t>::failure(error);
    consume(2U);
    return ReadResult<std::uint16_t>::success(value);
}

ReadResult<std::uint32_t> ByteReader::readU32LE(
    NavRecord record,
    NavField field) noexcept {
    const ReadResult<ByteView> bytes = availableBytes(4U, record, field);
    if (!bytes) {
        return ReadResult<std::uint32_t>::failure(bytes.error);
    }

    const std::uint32_t value = decodeU32LE(*bytes.value);
    const auto error = observe(record,field,value);
    if (!error.isNone()) return ReadResult<std::uint32_t>::failure(error);
    consume(4U);
    return ReadResult<std::uint32_t>::success(value);
}

ReadResult<float> ByteReader::readF32LE(
    NavRecord record,
    NavField field) noexcept {
    const ReadResult<ByteView> bytes = availableBytes(4U, record, field);
    if (!bytes) {
        return ReadResult<float>::failure(bytes.error);
    }

    const std::uint32_t raw = decodeU32LE(*bytes.value);
    float value = 0.0F;
    std::memcpy(&value, &raw, sizeof(value));
    if (!std::isfinite(value)) {
        return ReadResult<float>::failure(
            NavError{NavErrorKind::NonFiniteFloat, static_cast<std::uint64_t>(offset_), record, field});
    }

    const auto error = observe(record,field,raw);
    if (!error.isNone()) return ReadResult<float>::failure(error);
    consume(4U);
    return ReadResult<float>::success(value);
}

} // namespace astrabot::nav::io
