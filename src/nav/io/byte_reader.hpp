// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#pragma once

#include "nav/diagnostics/error.hpp"

#include <cstddef>
#include <cstdint>

namespace astrabot::nav::detail { struct DecodeContext; }
namespace astrabot::nav::io {

using diagnostics::NavField;
using diagnostics::NavRecord;
using diagnostics::ReadResult;

struct ByteView final {
    const std::uint8_t* data{nullptr};
    std::size_t size{0};

    constexpr bool isValid() const noexcept {
        return data != nullptr || size == 0U;
    }
};

class ByteReader final {
public:
    explicit ByteReader(ByteView bytes) noexcept;

    std::size_t offset() const noexcept { return offset_; }
    std::size_t remaining() const noexcept { return bytes_.size - offset_; }
    bool atEnd() const noexcept { return offset_ == bytes_.size; }

    ReadResult<std::uint8_t> readU8(
        NavRecord record = NavRecord::RawInput,
        NavField field = NavField::RawBytes) noexcept;
    ReadResult<std::uint16_t> readU16LE(
        NavRecord record = NavRecord::RawInput,
        NavField field = NavField::RawBytes) noexcept;
    ReadResult<std::uint32_t> readU32LE(
        NavRecord record = NavRecord::RawInput,
        NavField field = NavField::RawBytes) noexcept;
    ReadResult<float> readF32LE(
        NavRecord record = NavRecord::RawInput,
        NavField field = NavField::RawBytes) noexcept;
    ReadResult<ByteView> readBytes(
        std::size_t length,
        NavRecord record = NavRecord::RawInput,
        NavField field = NavField::RawBytes) noexcept;

private:
    friend class NavFileReader;
    friend class NavAreaReader;
    ByteReader(ByteView bytes, detail::DecodeContext* context) noexcept;
    ByteView bytes_{};
    std::size_t offset_{0};
    detail::DecodeContext* context_{nullptr};
    diagnostics::NavError observe(NavRecord record, NavField field, std::uint32_t value) noexcept;

    ReadResult<ByteView> availableBytes(
        std::size_t length,
        NavRecord record,
        NavField field) const noexcept;
    static std::uint16_t decodeU16LE(ByteView bytes) noexcept;
    static std::uint32_t decodeU32LE(ByteView bytes) noexcept;
    void consume(std::size_t length) noexcept { offset_ += length; }
};

} // namespace astrabot::nav::io
