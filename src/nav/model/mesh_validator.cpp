// SPDX-License-Identifier: MPL-2.0
#include "nav/model/mesh_validator.hpp"
#include <cstring>
#include <set>
namespace astrabot::nav::detail {
diagnostics::NavError validateMesh(const model::NavFileHeader &header,
                                   const std::vector<model::NavAreaRecord> &areas,
                                   const DecodeContext &context) {
    using namespace diagnostics;
    std::set<std::uint32_t> allAreas, allHiding, seenAreas, seenHiding;
    for (const auto &area : areas) {
        allAreas.insert(area.id.value);
        for (const auto &spot : area.hidingSpots)
            if (spot.id)
                allHiding.insert(*spot.id);
    }
    std::uint32_t owner = 0;
    std::set<std::uint32_t> connections;
    float nw[3]{};
    std::size_t nwIndex = 0, seIndex = 0;
    for (const auto &stamp : context.fields) {
        auto at = stamp.location;
        const auto v = stamp.value;
        switch (at.field) {
        case NavField::AreaId:
            owner = v;
            nwIndex = 0;
            seIndex = 0;
            if (v == 0)
                at.kind = NavErrorKind::InvalidValue;
            else if (!seenAreas.insert(v).second)
                at.kind = NavErrorKind::DuplicateId;
            break;
        case NavField::HidingSpotId:
            if (!seenHiding.insert(v).second)
                at.kind = NavErrorKind::DuplicateId;
            break;
        case NavField::NorthWestExtent:
            std::memcpy(&nw[nwIndex++], &v, sizeof(float));
            break;
        case NavField::SouthEastExtent: {
            float f;
            std::memcpy(&f, &v, sizeof(float));
            if (seIndex < 2 && !(nw[seIndex] < f))
                at.kind = NavErrorKind::InvalidGeometry;
            ++seIndex;
            break;
        }
        case NavField::ConnectionCount:
            connections.clear();
            break;
        case NavField::ConnectionAreaId:
            if (v == 0 || v == owner)
                at.kind = NavErrorKind::InvalidValue;
            else if (!allAreas.count(v))
                at.kind = NavErrorKind::DanglingReference;
            else if (!connections.insert(v).second)
                at.kind = NavErrorKind::InvalidValue;
            break;
        case NavField::ApproachAreaId:
            if (v != 0 && !allAreas.count(v))
                at.kind = NavErrorKind::DanglingReference;
            break;
        case NavField::EncounterAreaId:
            if (v == 0)
                at.kind = NavErrorKind::InvalidValue;
            else if (!allAreas.count(v))
                at.kind = NavErrorKind::DanglingReference;
            break;
        case NavField::EncounterSpotId:
            if (!allHiding.count(v))
                at.kind = NavErrorKind::DanglingReference;
            break;
        case NavField::EncounterDirection:
            if (v > 3)
                at.kind = NavErrorKind::UnsupportedValue;
            break;
        case NavField::Place:
            if (v > header.places.size())
                at.kind = NavErrorKind::InvalidValue;
            break;
        default:
            break;
        }
        if (!at.isNone())
            return at;
    }
    return {};
}
} // namespace astrabot::nav::detail
