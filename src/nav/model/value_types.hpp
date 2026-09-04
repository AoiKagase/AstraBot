// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#pragma once

#include <cstdint>

namespace astrabot::nav::model {

struct NavAreaId final {
    std::uint32_t value{0};

    static constexpr NavAreaId invalid() noexcept { return {}; }
    constexpr bool isValid() const noexcept { return value != 0U; }

    friend constexpr bool operator==(NavAreaId left, NavAreaId right) noexcept {
        return left.value == right.value;
    }
    friend constexpr bool operator!=(NavAreaId left, NavAreaId right) noexcept {
        return !(left == right);
    }
    friend constexpr bool operator<(NavAreaId left, NavAreaId right) noexcept {
        return left.value < right.value;
    }
};

struct NavVector3 final {
    float x{0.0F};
    float y{0.0F};
    float z{0.0F};

    bool isFinite() const noexcept;

    friend bool operator==(const NavVector3& left, const NavVector3& right) noexcept {
        return left.x == right.x && left.y == right.y && left.z == right.z;
    }
    friend bool operator!=(const NavVector3& left, const NavVector3& right) noexcept {
        return !(left == right);
    }
};

struct NavExtent final {
    NavVector3 northWest{};
    NavVector3 southEast{};
    float northEastZ{0.0F};
    float southWestZ{0.0F};

    bool isFinite() const noexcept;

    friend bool operator==(const NavExtent& left, const NavExtent& right) noexcept {
        return left.northWest == right.northWest &&
               left.southEast == right.southEast &&
               left.northEastZ == right.northEastZ &&
               left.southWestZ == right.southWestZ;
    }
    friend bool operator!=(const NavExtent& left, const NavExtent& right) noexcept {
        return !(left == right);
    }
};

} // namespace astrabot::nav::model
