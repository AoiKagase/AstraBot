// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#pragma once

#include <cstdint>
#include <optional>

namespace astrabot::nav::diagnostics {

enum class NavErrorKind : std::uint8_t {
    None = 0,
    EndOfInput,
    OffsetOverflow,
    NonFiniteFloat,
    InvalidInput,
    InvalidValue,
    UnsupportedValue,
    CountLimitExceeded,
    DuplicateId,
    DanglingReference,
    InvalidGeometry,
    TrailingData,
};

enum class NavRecord : std::uint8_t {
    None = 0,
    RawInput,
    FileHeader,
    PlaceDictionary,
    Area,
    Connection,
    HidingSpot,
    Approach,
    Encounter,
    TraversalLink,
};

enum class NavField : std::uint8_t {
    None = 0,
    RawBytes,
    Magic,
    Version,
    BspSize,
    PlaceCount,
    PlaceLength,
    PlaceText,
    AreaCount,
    AreaId,
    Attributes,
    NorthWestExtent,
    SouthEastExtent,
    NorthEastZ,
    SouthWestZ,
    ConnectionCount,
    ConnectionAreaId,
    HidingSpotCount,
    HidingSpotId,
    HidingSpotFlags,
    ApproachCount,
    ApproachAreaId,
    ApproachTraversal,
    EncounterCount,
    EncounterAreaId,
    EncounterDirection,
    EncounterSpotCount,
    EncounterSpotId,
    EncounterSpotT,
    Place,
};

struct NavError final {
    NavErrorKind kind{NavErrorKind::None};
    std::uint64_t offset{0};
    NavRecord record{NavRecord::None};
    NavField field{NavField::None};

    constexpr bool isNone() const noexcept {
        return kind == NavErrorKind::None;
    }

    friend constexpr bool operator==(NavError left, NavError right) noexcept {
        return left.kind == right.kind && left.offset == right.offset &&
               left.record == right.record && left.field == right.field;
    }
    friend constexpr bool operator!=(NavError left, NavError right) noexcept {
        return !(left == right);
    }
};

template <typename T>
struct ReadResult final {
    std::optional<T> value{};
    NavError error{};

    static ReadResult success(T result) noexcept {
        return {std::optional<T>{result}, NavError{}};
    }

    static ReadResult failure(NavError reason) noexcept {
        return {std::nullopt, reason};
    }

    bool succeeded() const noexcept {
        return value.has_value() && error.isNone();
    }

    explicit operator bool() const noexcept {
        return succeeded();
    }
};

} // namespace astrabot::nav::diagnostics
