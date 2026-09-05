// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include "nav/io/area_reader.hpp"

#include <new>

namespace astrabot::nav::io {

using diagnostics::NavError;
using diagnostics::NavErrorKind;
using diagnostics::NavField;
using diagnostics::NavRecord;
using diagnostics::ReadResult;

ReadResult<NavAreaBlock> NavAreaReader::read(
    ByteView bytes,
    model::NavVersion,
    std::uint32_t areaCount,
    const NavAreaReadLimits& limits) noexcept {
    if (areaCount == 0U) {
        return ReadResult<NavAreaBlock>::failure(
            NavError{NavErrorKind::InvalidValue, 0U, NavRecord::Area, NavField::AreaCount});
    }
    if (areaCount > limits.maxAreas) {
        return ReadResult<NavAreaBlock>::failure(
            NavError{NavErrorKind::CountLimitExceeded, 0U, NavRecord::Area, NavField::AreaCount});
    }

    try {
        NavAreaBlock block{};
        block.areas.reserve(areaCount);
        ByteReader reader(bytes);
        const ReadResult<std::uint32_t> areaId = reader.readU32LE(
            NavRecord::Area, NavField::AreaId);
        if (!areaId) {
            return ReadResult<NavAreaBlock>::failure(areaId.error);
        }
        return ReadResult<NavAreaBlock>::failure(
            NavError{NavErrorKind::EndOfInput, reader.offset(), NavRecord::Area, NavField::Attributes});
    } catch (const std::bad_alloc&) {
        return ReadResult<NavAreaBlock>::failure(
            NavError{NavErrorKind::AllocationFailure, 0U, NavRecord::Area, NavField::AreaCount});
    }
}

} // namespace astrabot::nav::io
