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
    const nav::query::NavSpatialIndex*, const nav::runtime::QueryRequest&) noexcept;
}
