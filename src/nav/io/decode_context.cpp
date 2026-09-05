// SPDX-License-Identifier: MPL-2.0
#include "nav/io/decode_context.hpp"
#include "nav/model/area_records.hpp"
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
namespace astrabot::nav::detail {
using namespace diagnostics;
NavError DecodeContext::charge(std::size_t count, std::size_t width, NavError at) noexcept {
    const auto cap = std::numeric_limits<std::size_t>::max();
    if (width != 0 && count > (cap - used) / width) {
        at.kind = NavErrorKind::OffsetOverflow;
        return at;
    }
    const auto amount = count * width;
    if (used > maximum || amount > maximum - used) {
        at.kind = NavErrorKind::CountLimitExceeded;
        return at;
    }
    used += amount;
    return {};
}
NavError DecodeContext::observe(std::size_t offset, NavRecord record, NavField field,
                                std::uint32_t value) noexcept {
    NavError at{NavErrorKind::None, static_cast<std::uint64_t>(base), record, field};
    if (offset > std::numeric_limits<std::size_t>::max() - base) {
        at.kind = NavErrorKind::OffsetOverflow;
        return at;
    }
    at.offset = static_cast<std::uint64_t>(base + offset);
    if (field == NavField::Version)
        version = value;
    std::size_t width = 0;
    switch (field) {
    case NavField::AreaCount:
        width = sizeof(model::NavAreaRecord);
        break;
    case NavField::PlaceCount:
        width = sizeof(std::string);
        break;
    case NavField::PlaceLength:
        width = 1;
        break;
    case NavField::ConnectionCount:
        width = sizeof(model::NavConnection);
        break;
    case NavField::HidingSpotCount:
        width = sizeof(model::NavHidingSpot);
        break;
    case NavField::ApproachCount:
        width = sizeof(model::NavApproachRecord);
        break;
    case NavField::EncounterCount:
        if (version >= 3)
            width = sizeof(model::NavEncounterRecord);
        break;
    case NavField::EncounterSpotCount:
        if (version >= 3)
            width = sizeof(model::NavEncounterSpot);
        break;
    default:
        break;
    }
    if (width != 0) {
        auto error = charge(value, width, at);
        if (!error.isNone())
            return error;
    }
    // Skip non-semantic header/count/raw fields, including discarded legacy data.
    const bool keep = field == NavField::AreaId || field == NavField::NorthWestExtent ||
                      field == NavField::SouthEastExtent || field == NavField::ConnectionCount ||
                      field == NavField::ConnectionAreaId || field == NavField::HidingSpotId ||
                      field == NavField::ApproachAreaId ||
                      (field == NavField::EncounterAreaId && version >= 3) ||
                      field == NavField::EncounterDirection || field == NavField::EncounterSpotId ||
                      field == NavField::Place;
    if (keep) {
        try {
            fields.push_back({at, value});
        } catch (const std::bad_alloc &) {
            at.kind = NavErrorKind::AllocationFailure;
            return at;
        } catch (const std::length_error &) {
            at.kind = NavErrorKind::AllocationFailure;
            return at;
        }
    }
    return {};
}
} // namespace astrabot::nav::detail
