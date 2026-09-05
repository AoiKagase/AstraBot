// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "adapter/metamod/plugin_entry.hpp"
#include "nav/runtime/world_query.hpp"
#include "nav/query/spatial_index.hpp"

namespace astrabot::adapter::cstrike {
// Caller validates actor/map/tick and borrows the matching live entity only for
// this synchronous call. Budget and ordinal/freshness checks belong to the
// portable query owner. No entity or engine pointer escapes into the result.
nav::runtime::WorldQueryResult queryNavWorld(enginefuncs_t*, edict_t*,
    const nav::query::NavSpatialIndex*, const nav::runtime::QueryRequest&, int maxEntities=0) noexcept;
// Public-entvars-only selection proof; at most 33 sphere enumeration calls
// (32 candidates plus the terminator). Unknown competing entities reject Use.
// Also used immediately before dispatching a queued Use press.
std::optional<nav::model::NavVector3> doorUseView(enginefuncs_t*, edict_t*,
    std::uint64_t doorId, int maxEntities) noexcept;
}
