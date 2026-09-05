// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "nav/query/route_types.hpp"

namespace astrabot::nav::query {
class NavRouteSearch final {
  public:
    static diagnostics::ReadResult<NavRouteResult>
    search(const NavGraph &, const NavRouteRequest &, NavRoutePolicy = {}) noexcept;
};
} // namespace astrabot::nav::query
