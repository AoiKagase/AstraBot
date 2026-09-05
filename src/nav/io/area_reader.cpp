// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include "nav/io/area_reader.hpp"

#include <new>
#include <stdexcept>
#include <utility>

namespace astrabot::nav::io {

using diagnostics::NavError;
using diagnostics::NavErrorKind;
using diagnostics::NavField;
using diagnostics::NavRecord;
using diagnostics::ReadResult;

ReadResult<NavAreaBlock> NavAreaReader::read(
    ByteView bytes,
    model::NavVersion version,
    std::uint32_t areaCount,
    const NavAreaReadLimits& limits) noexcept {
    return readTracked(bytes,version,areaCount,limits,nullptr);
}
ReadResult<NavAreaBlock> NavAreaReader::readTracked(
    ByteView bytes, model::NavVersion version, std::uint32_t areaCount,
    const NavAreaReadLimits& limits, detail::DecodeContext* context) noexcept {
    if (areaCount == 0U) {
        return ReadResult<NavAreaBlock>::failure(
            NavError{NavErrorKind::InvalidValue, 0U, NavRecord::Area, NavField::AreaCount});
    }
    if (areaCount > limits.maxAreas) {
        return ReadResult<NavAreaBlock>::failure(
            NavError{NavErrorKind::CountLimitExceeded, 0U, NavRecord::Area, NavField::AreaCount});
    }

    NavError allocationContext{
        NavErrorKind::AllocationFailure, 0U, NavRecord::Area, NavField::AreaCount};
    try {
        NavAreaBlock block{};
        block.areas.reserve(areaCount);
        ByteReader reader(bytes,context);

        std::uint64_t totalConnections = 0U;
        std::uint64_t totalHidingSpots = 0U;
        std::uint64_t totalApproaches = 0U;
        std::uint64_t totalEncounters = 0U;
        std::uint64_t totalEncounterSpots = 0U;
        for (std::uint32_t areaIndex = 0U; areaIndex < areaCount; ++areaIndex) {
            model::NavAreaRecord area{};

            const ReadResult<std::uint32_t> areaId = reader.readU32LE(
                NavRecord::Area, NavField::AreaId);
            if (!areaId) {
                return ReadResult<NavAreaBlock>::failure(areaId.error);
            }
            area.id = model::NavAreaId{*areaId.value};

            const ReadResult<std::uint8_t> attributes = reader.readU8(
                NavRecord::Area, NavField::Attributes);
            if (!attributes) {
                return ReadResult<NavAreaBlock>::failure(attributes.error);
            }
            area.attributes = *attributes.value;

            const ReadResult<float> northWestX = reader.readF32LE(
                NavRecord::Area, NavField::NorthWestExtent);
            if (!northWestX) {
                return ReadResult<NavAreaBlock>::failure(northWestX.error);
            }
            area.extent.northWest.x = *northWestX.value;

            const ReadResult<float> northWestY = reader.readF32LE(
                NavRecord::Area, NavField::NorthWestExtent);
            if (!northWestY) {
                return ReadResult<NavAreaBlock>::failure(northWestY.error);
            }
            area.extent.northWest.y = *northWestY.value;

            const ReadResult<float> northWestZ = reader.readF32LE(
                NavRecord::Area, NavField::NorthWestExtent);
            if (!northWestZ) {
                return ReadResult<NavAreaBlock>::failure(northWestZ.error);
            }
            area.extent.northWest.z = *northWestZ.value;

            const ReadResult<float> southEastX = reader.readF32LE(
                NavRecord::Area, NavField::SouthEastExtent);
            if (!southEastX) {
                return ReadResult<NavAreaBlock>::failure(southEastX.error);
            }
            area.extent.southEast.x = *southEastX.value;

            const ReadResult<float> southEastY = reader.readF32LE(
                NavRecord::Area, NavField::SouthEastExtent);
            if (!southEastY) {
                return ReadResult<NavAreaBlock>::failure(southEastY.error);
            }
            area.extent.southEast.y = *southEastY.value;

            const ReadResult<float> southEastZ = reader.readF32LE(
                NavRecord::Area, NavField::SouthEastExtent);
            if (!southEastZ) {
                return ReadResult<NavAreaBlock>::failure(southEastZ.error);
            }
            area.extent.southEast.z = *southEastZ.value;

            const ReadResult<float> northEastZ = reader.readF32LE(
                NavRecord::Area, NavField::NorthEastZ);
            if (!northEastZ) {
                return ReadResult<NavAreaBlock>::failure(northEastZ.error);
            }
            area.extent.northEastZ = *northEastZ.value;

            const ReadResult<float> southWestZ = reader.readF32LE(
                NavRecord::Area, NavField::SouthWestZ);
            if (!southWestZ) {
                return ReadResult<NavAreaBlock>::failure(southWestZ.error);
            }
            area.extent.southWestZ = *southWestZ.value;

            for (std::size_t direction = 0U; direction < area.connections.size(); ++direction) {
                const std::size_t countOffset = reader.offset();
                const ReadResult<std::uint32_t> connectionCount = reader.readU32LE(
                    NavRecord::Connection, NavField::ConnectionCount);
                if (!connectionCount) {
                    return ReadResult<NavAreaBlock>::failure(connectionCount.error);
                }
                if (*connectionCount.value > limits.maxConnectionsPerDirection) {
                    return ReadResult<NavAreaBlock>::failure(
                        NavError{NavErrorKind::CountLimitExceeded,
                                 static_cast<std::uint64_t>(countOffset),
                                 NavRecord::Connection,
                                 NavField::ConnectionCount});
                }
                if (totalConnections + *connectionCount.value > limits.maxTotalConnections) {
                    return ReadResult<NavAreaBlock>::failure(
                        NavError{NavErrorKind::CountLimitExceeded,
                                 static_cast<std::uint64_t>(countOffset),
                                 NavRecord::Connection,
                                 NavField::ConnectionCount});
                }

                allocationContext = NavError{
                    NavErrorKind::AllocationFailure,
                    static_cast<std::uint64_t>(countOffset),
                    NavRecord::Connection,
                    NavField::ConnectionCount};
                area.connections[direction].reserve(*connectionCount.value);
                totalConnections += *connectionCount.value;
                for (std::uint32_t connectionIndex = 0U;
                     connectionIndex < *connectionCount.value;
                     ++connectionIndex) {
                    const ReadResult<std::uint32_t> connectionId = reader.readU32LE(
                        NavRecord::Connection, NavField::ConnectionAreaId);
                    if (!connectionId) {
                        return ReadResult<NavAreaBlock>::failure(connectionId.error);
                    }
                    area.connections[direction].push_back(
                        model::NavConnection{model::NavAreaId{*connectionId.value}});
                }
            }

            const std::size_t hidingCountOffset = reader.offset();
            const ReadResult<std::uint8_t> hidingCount = reader.readU8(
                NavRecord::HidingSpot, NavField::HidingSpotCount);
            if (!hidingCount) {
                return ReadResult<NavAreaBlock>::failure(hidingCount.error);
            }
            if (*hidingCount.value > limits.maxHidingSpotsPerArea ||
                totalHidingSpots + *hidingCount.value > limits.maxTotalHidingSpots) {
                return ReadResult<NavAreaBlock>::failure(
                    NavError{NavErrorKind::CountLimitExceeded,
                             static_cast<std::uint64_t>(hidingCountOffset),
                             NavRecord::HidingSpot,
                             NavField::HidingSpotCount});
            }
            allocationContext = NavError{
                NavErrorKind::AllocationFailure,
                static_cast<std::uint64_t>(hidingCountOffset),
                NavRecord::HidingSpot,
                NavField::HidingSpotCount};
            area.hidingSpots.reserve(*hidingCount.value);
            totalHidingSpots += *hidingCount.value;
            for (std::uint8_t hidingIndex = 0U;
                 hidingIndex < *hidingCount.value;
                 ++hidingIndex) {
                model::NavHidingSpot hidingSpot{};
                if (version != model::NavVersion::V1) {
                    const ReadResult<std::uint32_t> hidingId = reader.readU32LE(
                        NavRecord::HidingSpot, NavField::HidingSpotId);
                    if (!hidingId) {
                        return ReadResult<NavAreaBlock>::failure(hidingId.error);
                    }
                    hidingSpot.id = *hidingId.value;
                }

                const ReadResult<float> hidingX = reader.readF32LE(
                    NavRecord::HidingSpot, NavField::RawBytes);
                if (!hidingX) {
                    return ReadResult<NavAreaBlock>::failure(hidingX.error);
                }
                hidingSpot.position.x = *hidingX.value;
                const ReadResult<float> hidingY = reader.readF32LE(
                    NavRecord::HidingSpot, NavField::RawBytes);
                if (!hidingY) {
                    return ReadResult<NavAreaBlock>::failure(hidingY.error);
                }
                hidingSpot.position.y = *hidingY.value;
                const ReadResult<float> hidingZ = reader.readF32LE(
                    NavRecord::HidingSpot, NavField::RawBytes);
                if (!hidingZ) {
                    return ReadResult<NavAreaBlock>::failure(hidingZ.error);
                }
                hidingSpot.position.z = *hidingZ.value;

                if (version != model::NavVersion::V1) {
                    const ReadResult<std::uint8_t> hidingFlags = reader.readU8(
                        NavRecord::HidingSpot, NavField::HidingSpotFlags);
                    if (!hidingFlags) {
                        return ReadResult<NavAreaBlock>::failure(hidingFlags.error);
                    }
                    hidingSpot.flags = *hidingFlags.value;
                }
                area.hidingSpots.push_back(std::move(hidingSpot));
            }

            const std::size_t approachCountOffset = reader.offset();
            const ReadResult<std::uint8_t> approachCount = reader.readU8(
                NavRecord::Approach, NavField::ApproachCount);
            if (!approachCount) {
                return ReadResult<NavAreaBlock>::failure(approachCount.error);
            }
            if (*approachCount.value > limits.maxApproachesPerArea ||
                totalApproaches + *approachCount.value > limits.maxTotalApproaches) {
                return ReadResult<NavAreaBlock>::failure(
                    NavError{NavErrorKind::CountLimitExceeded,
                             static_cast<std::uint64_t>(approachCountOffset),
                             NavRecord::Approach,
                             NavField::ApproachCount});
            }
            allocationContext = NavError{
                NavErrorKind::AllocationFailure,
                static_cast<std::uint64_t>(approachCountOffset),
                NavRecord::Approach,
                NavField::ApproachCount};
            area.approaches.reserve(*approachCount.value);
            totalApproaches += *approachCount.value;
            for (std::uint8_t approachIndex = 0U;
                 approachIndex < *approachCount.value;
                 ++approachIndex) {
                model::NavApproachRecord approach{};
                const ReadResult<std::uint32_t> here = reader.readU32LE(
                    NavRecord::Approach, NavField::ApproachAreaId);
                if (!here) {
                    return ReadResult<NavAreaBlock>::failure(here.error);
                }
                approach.here = model::NavAreaId{*here.value};
                const ReadResult<std::uint32_t> previous = reader.readU32LE(
                    NavRecord::Approach, NavField::ApproachAreaId);
                if (!previous) {
                    return ReadResult<NavAreaBlock>::failure(previous.error);
                }
                approach.previous = model::NavAreaId{*previous.value};
                const ReadResult<std::uint8_t> previousToHere = reader.readU8(
                    NavRecord::Approach, NavField::ApproachTraversal);
                if (!previousToHere) {
                    return ReadResult<NavAreaBlock>::failure(previousToHere.error);
                }
                approach.previousToHereHow = *previousToHere.value;
                const ReadResult<std::uint32_t> next = reader.readU32LE(
                    NavRecord::Approach, NavField::ApproachAreaId);
                if (!next) {
                    return ReadResult<NavAreaBlock>::failure(next.error);
                }
                approach.next = model::NavAreaId{*next.value};
                const ReadResult<std::uint8_t> hereToNext = reader.readU8(
                    NavRecord::Approach, NavField::ApproachTraversal);
                if (!hereToNext) {
                    return ReadResult<NavAreaBlock>::failure(hereToNext.error);
                }
                approach.hereToNextHow = *hereToNext.value;
                area.approaches.push_back(std::move(approach));
            }

            const std::size_t encounterCountOffset = reader.offset();
            const ReadResult<std::uint32_t> encounterCount = reader.readU32LE(
                NavRecord::Encounter, NavField::EncounterCount);
            if (!encounterCount) {
                return ReadResult<NavAreaBlock>::failure(encounterCount.error);
            }
            if (*encounterCount.value > limits.maxEncountersPerArea ||
                totalEncounters + *encounterCount.value > limits.maxTotalEncounters) {
                return ReadResult<NavAreaBlock>::failure(
                    NavError{NavErrorKind::CountLimitExceeded,
                             static_cast<std::uint64_t>(encounterCountOffset),
                             NavRecord::Encounter,
                             NavField::EncounterCount});
            }
            if (version != model::NavVersion::V1 &&
                version != model::NavVersion::V2) {
                allocationContext = NavError{
                    NavErrorKind::AllocationFailure,
                    static_cast<std::uint64_t>(encounterCountOffset),
                    NavRecord::Encounter,
                    NavField::EncounterCount};
                area.encounters.reserve(*encounterCount.value);
            }
            totalEncounters += *encounterCount.value;

            const auto readEncounterVector = [&reader]() -> ReadResult<model::NavVector3> {
                model::NavVector3 vector{};
                const ReadResult<float> x = reader.readF32LE(
                    NavRecord::Encounter, NavField::RawBytes);
                if (!x) {
                    return ReadResult<model::NavVector3>::failure(x.error);
                }
                vector.x = *x.value;
                const ReadResult<float> y = reader.readF32LE(
                    NavRecord::Encounter, NavField::RawBytes);
                if (!y) {
                    return ReadResult<model::NavVector3>::failure(y.error);
                }
                vector.y = *y.value;
                const ReadResult<float> z = reader.readF32LE(
                    NavRecord::Encounter, NavField::RawBytes);
                if (!z) {
                    return ReadResult<model::NavVector3>::failure(z.error);
                }
                vector.z = *z.value;
                return ReadResult<model::NavVector3>::success(vector);
            };

            for (std::uint32_t encounterIndex = 0U;
                 encounterIndex < *encounterCount.value;
                 ++encounterIndex) {
                if (version == model::NavVersion::V1 ||
                    version == model::NavVersion::V2) {
                    const ReadResult<std::uint32_t> from = reader.readU32LE(
                        NavRecord::Encounter, NavField::EncounterAreaId);
                    if (!from) {
                        return ReadResult<NavAreaBlock>::failure(from.error);
                    }
                    const ReadResult<std::uint32_t> to = reader.readU32LE(
                        NavRecord::Encounter, NavField::EncounterAreaId);
                    if (!to) {
                        return ReadResult<NavAreaBlock>::failure(to.error);
                    }
                    const ReadResult<model::NavVector3> fromEndpoint = readEncounterVector();
                    if (!fromEndpoint) {
                        return ReadResult<NavAreaBlock>::failure(fromEndpoint.error);
                    }
                    const ReadResult<model::NavVector3> toEndpoint = readEncounterVector();
                    if (!toEndpoint) {
                        return ReadResult<NavAreaBlock>::failure(toEndpoint.error);
                    }

                    const std::size_t spotCountOffset = reader.offset();
                    const ReadResult<std::uint8_t> spotCount = reader.readU8(
                        NavRecord::Encounter, NavField::EncounterSpotCount);
                    if (!spotCount) {
                        return ReadResult<NavAreaBlock>::failure(spotCount.error);
                    }
                    if (*spotCount.value > limits.maxEncounterSpotsPerPath ||
                        totalEncounterSpots + *spotCount.value > limits.maxTotalEncounterSpots) {
                        return ReadResult<NavAreaBlock>::failure(
                            NavError{NavErrorKind::CountLimitExceeded,
                                     static_cast<std::uint64_t>(spotCountOffset),
                                     NavRecord::Encounter,
                                     NavField::EncounterSpotCount});
                    }
                    totalEncounterSpots += *spotCount.value;
                    for (std::uint8_t spotIndex = 0U;
                         spotIndex < *spotCount.value;
                         ++spotIndex) {
                        const ReadResult<model::NavVector3> spotPosition = readEncounterVector();
                        if (!spotPosition) {
                            return ReadResult<NavAreaBlock>::failure(spotPosition.error);
                        }
                        const ReadResult<float> spotT = reader.readF32LE(
                            NavRecord::Encounter, NavField::RawBytes);
                        if (!spotT) {
                            return ReadResult<NavAreaBlock>::failure(spotT.error);
                        }
                    }
                    continue;
                }

                model::NavEncounterRecord encounter{};
                const ReadResult<std::uint32_t> from = reader.readU32LE(
                    NavRecord::Encounter, NavField::EncounterAreaId);
                if (!from) {
                    return ReadResult<NavAreaBlock>::failure(from.error);
                }
                encounter.from = model::NavAreaId{*from.value};
                const ReadResult<std::uint8_t> fromDirection = reader.readU8(
                    NavRecord::Encounter, NavField::EncounterDirection);
                if (!fromDirection) {
                    return ReadResult<NavAreaBlock>::failure(fromDirection.error);
                }
                encounter.fromDirection = *fromDirection.value;
                const ReadResult<std::uint32_t> to = reader.readU32LE(
                    NavRecord::Encounter, NavField::EncounterAreaId);
                if (!to) {
                    return ReadResult<NavAreaBlock>::failure(to.error);
                }
                encounter.to = model::NavAreaId{*to.value};
                const ReadResult<std::uint8_t> toDirection = reader.readU8(
                    NavRecord::Encounter, NavField::EncounterDirection);
                if (!toDirection) {
                    return ReadResult<NavAreaBlock>::failure(toDirection.error);
                }
                encounter.toDirection = *toDirection.value;

                const std::size_t spotCountOffset = reader.offset();
                const ReadResult<std::uint8_t> spotCount = reader.readU8(
                    NavRecord::Encounter, NavField::EncounterSpotCount);
                if (!spotCount) {
                    return ReadResult<NavAreaBlock>::failure(spotCount.error);
                }
                if (*spotCount.value > limits.maxEncounterSpotsPerPath ||
                    totalEncounterSpots + *spotCount.value > limits.maxTotalEncounterSpots) {
                    return ReadResult<NavAreaBlock>::failure(
                        NavError{NavErrorKind::CountLimitExceeded,
                                 static_cast<std::uint64_t>(spotCountOffset),
                                 NavRecord::Encounter,
                                 NavField::EncounterSpotCount});
                }
                allocationContext = NavError{
                    NavErrorKind::AllocationFailure,
                    static_cast<std::uint64_t>(spotCountOffset),
                    NavRecord::Encounter,
                    NavField::EncounterSpotCount};
                encounter.spots.reserve(*spotCount.value);
                totalEncounterSpots += *spotCount.value;
                for (std::uint8_t spotIndex = 0U;
                     spotIndex < *spotCount.value;
                     ++spotIndex) {
                    model::NavEncounterSpot spot{};
                    const ReadResult<std::uint32_t> spotId = reader.readU32LE(
                        NavRecord::Encounter, NavField::EncounterSpotId);
                    if (!spotId) {
                        return ReadResult<NavAreaBlock>::failure(spotId.error);
                    }
                    spot.hidingSpotId = *spotId.value;
                    const ReadResult<std::uint8_t> spotT = reader.readU8(
                        NavRecord::Encounter, NavField::EncounterSpotT);
                    if (!spotT) {
                        return ReadResult<NavAreaBlock>::failure(spotT.error);
                    }
                    spot.t = *spotT.value;
                    encounter.spots.push_back(spot);
                }
                area.encounters.push_back(std::move(encounter));
            }

            if (version == model::NavVersion::V5) {
                const ReadResult<std::uint16_t> place = reader.readU16LE(
                    NavRecord::Area, NavField::Place);
                if (!place) {
                    return ReadResult<NavAreaBlock>::failure(place.error);
                }
                area.place = *place.value;
            }

            block.areas.push_back(std::move(area));
        }
        block.bytesConsumed = reader.offset();
        return ReadResult<NavAreaBlock>::success(std::move(block));
    } catch (const std::bad_alloc&) {
        return ReadResult<NavAreaBlock>::failure(allocationContext);
    } catch (const std::length_error&) {
        return ReadResult<NavAreaBlock>::failure(allocationContext);
    }
}

} // namespace astrabot::nav::io
