// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#pragma once

#include "nav/model/value_types.hpp"
#include "nav/model/connection.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace astrabot::nav::model {

enum class NavCardinalDirection : std::uint8_t {
    North = 0,
    East,
    South,
    West,
};

struct NavHidingSpot final {
    // v1 has no serialized ID; v2+ may contain any uint32 ID, including zero.
    std::optional<std::uint32_t> id{};
    NavVector3 position{};
    std::optional<std::uint8_t> flags{};
};

struct NavApproachRecord final {
    // Zero is an absent reference for each approach endpoint, not an area ID.
    NavAreaId here{};
    NavAreaId previous{};
    NavAreaId next{};
    std::uint8_t previousToHereHow{0};
    std::uint8_t hereToNextHow{0};
};

struct NavEncounterSpot final {
    // Zero refers to the hiding spot with ID zero; it is not an absent reference.
    std::uint32_t hidingSpotId{0};
    std::uint8_t t{0};
};

struct NavEncounterRecord final {
    NavAreaId from{};
    NavAreaId to{};
    std::uint8_t fromDirection{0};
    std::uint8_t toDirection{0};
    std::vector<NavEncounterSpot> spots{};
};

struct NavAreaRecord final {
    NavAreaId id{};
    std::uint8_t attributes{0};
    NavExtent extent{};
    std::array<std::vector<NavConnection>, 4> connections{};
    std::vector<NavHidingSpot> hidingSpots{};
    std::vector<NavApproachRecord> approaches{};
    std::vector<NavEncounterRecord> encounters{};
    std::optional<std::uint16_t> place{};
};

} // namespace astrabot::nav::model
