// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "nav/io/mesh_loader.hpp"
#include "nav/query/spatial_index.hpp"
#include "nav/query/route_search.hpp"
#include <filesystem>
#include <iosfwd>
#include <optional>

namespace astrabot::tools::inspection {
struct Query {
    nav::model::NavVector3 start{}, goal{};
    double radius{}, vertical{};
};
struct Options {
    std::filesystem::path nav;
    std::optional<std::filesystem::path> bsp;
    std::optional<Query> query;
};
struct Profile {
    nav::io::NavMeshReadLimits mesh;
    nav::query::NavSpatialIndexLimits index;
    nav::query::NavGraphLimits graph;
    nav::query::NavRouteLimits route;
};
Profile compatibilityProfile() noexcept;
// Tool-local inspection seam; the CLI exposes only the fixed profile.
int run(const Options& options, const Profile& profile, std::ostream& out);
int cli(int argc, const char* const* argv, std::ostream& out);
} // namespace astrabot::tools::inspection
