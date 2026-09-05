// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "nav/model/value_types.hpp"
namespace astrabot::nav::query {
struct NavQueryPoint final {
    double x{0}, y{0}, z{0};
};
// Preconditions: finite point and validated, nondegenerate extent.
// This is an XY projection, not the closest point on a sloped surface.
NavQueryPoint projectToArea(const model::NavExtent &extent, model::NavVector3 point) noexcept;
bool containsXY(const model::NavExtent &extent, model::NavVector3 point) noexcept;
double squaredDistance(NavQueryPoint projected, model::NavVector3 point) noexcept;
} // namespace astrabot::nav::query
