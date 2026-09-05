// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include "nav/io/nav_reader.hpp"

#include <cstring>
#include <new>
#include <string>
#include <utility>

namespace astrabot::nav::io {

namespace {

using diagnostics::NavError;
using diagnostics::NavErrorKind;
using diagnostics::NavField;
using diagnostics::NavRecord;
using diagnostics::ReadResult;
using model::NavFileHeader;
using model::NavVersion;

constexpr std::uint32_t kNavMagic = 0xFEEDFACEU;
constexpr std::uint32_t kMinNavVersion = 1U;
constexpr std::uint32_t kMaxNavVersion = 5U;

NavError error(
    NavErrorKind kind,
    std::size_t offset,
    NavRecord record,
    NavField field) noexcept {
    return NavError{
        kind,
        static_cast<std::uint64_t>(offset),
        record,
        field,
    };
}

bool hasInternalNul(ByteView bytes) noexcept {
    if (bytes.size < 2U) {
        return false;
    }
    for (std::size_t index = 0U; index + 1U < bytes.size; ++index) {
        if (bytes.data[index] == 0U) {
            return true;
        }
    }
    return false;
}

} // namespace

diagnostics::ReadResult<NavFileHeader> NavFileReader::readHeader(
    ByteView bytes,
    const NavReadLimits& limits) noexcept {
    NavError allocationContext = error(
        NavErrorKind::AllocationFailure,
        0U,
        NavRecord::FileHeader,
        NavField::None);

    try {
        ByteReader reader(bytes);
        NavFileHeader header{};

        const std::size_t magicOffset = reader.offset();
        const ReadResult<std::uint32_t> magic = reader.readU32LE(
            NavRecord::FileHeader,
            NavField::Magic);
        if (!magic) {
            return ReadResult<NavFileHeader>::failure(magic.error);
        }
        if (*magic.value != kNavMagic) {
            return ReadResult<NavFileHeader>::failure(error(
                NavErrorKind::InvalidValue,
                magicOffset,
                NavRecord::FileHeader,
                NavField::Magic));
        }

        const std::size_t versionOffset = reader.offset();
        const ReadResult<std::uint32_t> version = reader.readU32LE(
            NavRecord::FileHeader,
            NavField::Version);
        if (!version) {
            return ReadResult<NavFileHeader>::failure(version.error);
        }
        if (*version.value < kMinNavVersion || *version.value > kMaxNavVersion) {
            return ReadResult<NavFileHeader>::failure(error(
                NavErrorKind::UnsupportedValue,
                versionOffset,
                NavRecord::FileHeader,
                NavField::Version));
        }
        header.version = static_cast<NavVersion>(*version.value);

        if (*version.value >= 4U) {
            const ReadResult<std::uint32_t> bspSize = reader.readU32LE(
                NavRecord::FileHeader,
                NavField::BspSize);
            if (!bspSize) {
                return ReadResult<NavFileHeader>::failure(bspSize.error);
            }
            header.bspSize = *bspSize.value;
        }

        if (*version.value >= 5U) {
            const std::size_t placeCountOffset = reader.offset();
            const ReadResult<std::uint16_t> placeCount = reader.readU16LE(
                NavRecord::PlaceDictionary,
                NavField::PlaceCount);
            if (!placeCount) {
                return ReadResult<NavFileHeader>::failure(placeCount.error);
            }
            if (*placeCount.value > limits.maxPlaces) {
                return ReadResult<NavFileHeader>::failure(error(
                    NavErrorKind::CountLimitExceeded,
                    placeCountOffset,
                    NavRecord::PlaceDictionary,
                    NavField::PlaceCount));
            }

            allocationContext = error(
                NavErrorKind::AllocationFailure,
                placeCountOffset,
                NavRecord::PlaceDictionary,
                NavField::PlaceCount);
            header.places.reserve(*placeCount.value);
            std::uint32_t totalPlaceBytes = 0U;

            for (std::uint16_t index = 0U; index < *placeCount.value; ++index) {
                const std::size_t placeLengthOffset = reader.offset();
                const ReadResult<std::uint16_t> placeLength = reader.readU16LE(
                    NavRecord::PlaceDictionary,
                    NavField::PlaceLength);
                if (!placeLength) {
                    return ReadResult<NavFileHeader>::failure(placeLength.error);
                }
                if (*placeLength.value < 2U) {
                    return ReadResult<NavFileHeader>::failure(error(
                        NavErrorKind::InvalidValue,
                        placeLengthOffset,
                        NavRecord::PlaceDictionary,
                        NavField::PlaceLength));
                }
                if (*placeLength.value > limits.maxPlaceBytes) {
                    return ReadResult<NavFileHeader>::failure(error(
                        NavErrorKind::CountLimitExceeded,
                        placeLengthOffset,
                        NavRecord::PlaceDictionary,
                        NavField::PlaceLength));
                }
                if (totalPlaceBytes > limits.maxTotalPlaceBytes ||
                    *placeLength.value > limits.maxTotalPlaceBytes - totalPlaceBytes) {
                    return ReadResult<NavFileHeader>::failure(error(
                        NavErrorKind::AllocationFailure,
                        placeLengthOffset,
                        NavRecord::PlaceDictionary,
                        NavField::PlaceText));
                }

                const std::size_t placeTextOffset = reader.offset();
                const ReadResult<ByteView> placeText = reader.readBytes(
                    *placeLength.value,
                    NavRecord::PlaceDictionary,
                    NavField::PlaceText);
                if (!placeText) {
                    return ReadResult<NavFileHeader>::failure(placeText.error);
                }
                if (placeText.value->data[placeText.value->size - 1U] != 0U ||
                    hasInternalNul(*placeText.value)) {
                    return ReadResult<NavFileHeader>::failure(error(
                        NavErrorKind::InvalidValue,
                        placeTextOffset,
                        NavRecord::PlaceDictionary,
                        NavField::PlaceText));
                }

                allocationContext = error(
                    NavErrorKind::AllocationFailure,
                    placeTextOffset,
                    NavRecord::PlaceDictionary,
                    NavField::PlaceText);
                const char* text = reinterpret_cast<const char*>(placeText.value->data);
                header.places.emplace_back(text, placeText.value->size - 1U);
                totalPlaceBytes += *placeLength.value;
            }
        }

        const std::size_t areaCountOffset = reader.offset();
        const ReadResult<std::uint32_t> areaCount = reader.readU32LE(
            NavRecord::FileHeader,
            NavField::AreaCount);
        if (!areaCount) {
            return ReadResult<NavFileHeader>::failure(areaCount.error);
        }
        if (*areaCount.value == 0U) {
            return ReadResult<NavFileHeader>::failure(error(
                NavErrorKind::InvalidValue,
                areaCountOffset,
                NavRecord::FileHeader,
                NavField::AreaCount));
        }
        if (*areaCount.value > limits.maxAreas) {
            return ReadResult<NavFileHeader>::failure(error(
                NavErrorKind::CountLimitExceeded,
                areaCountOffset,
                NavRecord::FileHeader,
                NavField::AreaCount));
        }
        header.areaCount = *areaCount.value;
        header.headerBytes = reader.offset();

        return ReadResult<NavFileHeader>{
            std::optional<NavFileHeader>{std::move(header)},
            NavError{},
        };
    } catch (const std::bad_alloc&) {
        return ReadResult<NavFileHeader>::failure(allocationContext);
    }
}

} // namespace astrabot::nav::io
