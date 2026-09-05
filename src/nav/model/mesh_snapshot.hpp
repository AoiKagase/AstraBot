// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "nav/model/area_records.hpp"
#include "nav/model/file_header.hpp"
#include <utility>
namespace astrabot::nav::detail {
class NavMeshBuilder;
}
namespace astrabot::nav::model {
class NavMeshSnapshot final {
  public:
    const NavFileHeader &header() const noexcept { return header_; }
    const std::vector<NavAreaRecord> &areas() const noexcept { return areas_; }

  private:
    friend class astrabot::nav::detail::NavMeshBuilder;
    NavMeshSnapshot(NavFileHeader header, std::vector<NavAreaRecord> areas) noexcept
        : header_(std::move(header)), areas_(std::move(areas)) {}
    NavFileHeader header_;
    std::vector<NavAreaRecord> areas_;
};
} // namespace astrabot::nav::model
