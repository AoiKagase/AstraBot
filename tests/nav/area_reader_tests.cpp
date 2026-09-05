// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include "nav/io/area_reader.hpp"
#include "nav/io/byte_reader.hpp"
#include "nav/model/area_records.hpp"

#include <cassert>
#include <cstdint>
#include <optional>

namespace {

using astrabot::nav::io::ByteView;
using astrabot::nav::io::NavAreaReadLimits;
using astrabot::nav::io::NavAreaReader;
using astrabot::nav::model::NavAreaId;
using astrabot::nav::model::NavAreaRecord;
using astrabot::nav::model::NavExtent;
using astrabot::nav::model::NavVersion;

void testPublicAreaRecordContract() {
    const NavAreaRecord area{
        NavAreaId{7U},
        0x05U,
        NavExtent{},
        {},
        {},
        {},
        {},
        std::optional<std::uint16_t>{static_cast<std::uint16_t>(3U)},
    };
    assert(area.id == NavAreaId{7U});
    assert(area.place.has_value() && *area.place == 3U);

    const auto result = NavAreaReader::read(
        ByteView{nullptr, 0U}, NavVersion::V1, 0U, NavAreaReadLimits{});
    assert(!result);
}

} // namespace

int main() {
    testPublicAreaRecordContract();
    return 0;
}
