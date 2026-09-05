// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "nav/io/area_reader.hpp"
#include "nav/io/nav_reader.hpp"
#include "nav/model/mesh_snapshot.hpp"
#include <memory>
namespace astrabot::nav::io {
struct NavMeshReadLimits final {
    std::size_t maxInputBytes{0};
    NavReadLimits header{};
    NavAreaReadLimits areas{};
    std::size_t maxSnapshotBytes{0};
};
class NavMeshLoader final {
  public:
    static diagnostics::ReadResult<std::shared_ptr<const model::NavMeshSnapshot>>
    load(ByteView bytes, const NavMeshReadLimits &limits) noexcept;
};
} // namespace astrabot::nav::io
