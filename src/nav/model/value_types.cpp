// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include "nav/model/value_types.hpp"

#include <cmath>

namespace astrabot::nav::model {

bool NavVector3::isFinite() const noexcept {
    return std::isfinite(x) && std::isfinite(y) && std::isfinite(z);
}

bool NavExtent::isFinite() const noexcept {
    return northWest.isFinite() && southEast.isFinite() &&
           std::isfinite(northEastZ) && std::isfinite(southWestZ);
}

} // namespace astrabot::nav::model
