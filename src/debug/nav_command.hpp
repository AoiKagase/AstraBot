// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "nav/runtime/route_session.hpp"
#include <string_view>
namespace astrabot::debug {
using NavLineSink = void(*)(void*, const char*) noexcept;
std::optional<nav::model::NavAreaId> parseNavGoal(std::string_view text) noexcept;
// At most 66 fixed-size lines; excess selected edges are explicitly counted.
void printNavTrace(const nav::runtime::DecisionTrace&, NavLineSink, void*) noexcept;
}
