// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "fixture.hpp"
#include "check.hpp"
#include <array>
#include <algorithm>
#include <cmath>
#include <limits>

namespace evidence {
struct Patch { std::uint32_t id; float x, y, z00, z10, z01, z11; };
struct Arc { std::uint32_t from, to; unsigned direction; };
struct Scene { std::vector<Patch> patches; std::vector<Arc> arcs; };
inline Scene scene(std::uint32_t count) {
    Scene s;
    check(count > 0 && count <= 1024, "scene size");
    for (std::uint32_t i = 0; i < count; ++i) {
        const float z = float(i / 16U) * 8.f;
        s.patches.push_back({i + 1, float(i % 4U) * 3.f, float((i / 4U) % 4U) * 3.f,
                             z, z + float(i % 3U), z - float(i % 2U), z + 2});
        if (i + 1 < count) s.arcs.push_back({i + 1, i + 2, 1});
        if (i + 4 < count) s.arcs.push_back({i + 1, i + 5, 2});
        if (i % 3 == 0 && i > 0) s.arcs.push_back({i + 1, i, 0});
    }
    return s;
}
inline Fixture encode(const Scene& s) {
    Fixture f;
    f.integer(0xFEEDFACE, 4, R::FileHeader, F::Magic);
    f.integer(1, 4, R::FileHeader, F::Version);
    f.integer(static_cast<std::uint32_t>(s.patches.size()), 4, R::FileHeader, F::AreaCount);
    // Reverse wire order tests graph/index independence from input order.
    for (auto it = s.patches.rbegin(); it != s.patches.rend(); ++it) {
        const auto& p = *it;
        f.integer(p.id, 4, R::Area, F::AreaId); f.integer(0, 1, R::Area, F::Attributes);
        for (float v : {p.x, p.y, p.z00}) f.real(v, R::Area, F::NorthWestExtent);
        for (float v : {p.x + 2, p.y + 2, p.z11}) f.real(v, R::Area, F::SouthEastExtent);
        f.real(p.z10, R::Area, F::NorthEastZ); f.real(p.z01, R::Area, F::SouthWestZ);
        for (unsigned d = 0; d < 4; ++d) {
            const auto count = std::count_if(s.arcs.begin(), s.arcs.end(), [&](const Arc& a) {
                return a.from == p.id && a.direction == d;
            });
            f.integer(static_cast<std::uint32_t>(count), 4, R::Connection, F::ConnectionCount);
            for (const auto& a : s.arcs)
                if (a.from == p.id && a.direction == d)
                    f.integer(a.to, 4, R::Connection, F::ConnectionAreaId);
        }
        f.integer(0, 1, R::HidingSpot, F::HidingSpotCount);
        f.integer(0, 1, R::Approach, F::ApproachCount);
        f.integer(0, 4, R::Encounter, F::EncounterCount);
    }
    return f;
}
struct Match { std::uint32_t id; std::array<double, 3> point; double squared; };
inline std::optional<Match> linear(const Scene& s, nav::model::NavVector3 q,
                                   double radius, double vertical, bool containing) {
    std::optional<Match> best;
    for (const auto& p : s.patches) {
        const bool inside = q.x >= p.x && q.x <= p.x + 2 && q.y >= p.y && q.y <= p.y + 2;
        if (containing && !inside) continue;
        // Four explicit weights, no production projection/bounds helpers.
        const double u = std::max(0., std::min(1., (double(q.x) - p.x) / 2.));
        const double v = std::max(0., std::min(1., (double(q.y) - p.y) / 2.));
        const double x = p.x + 2 * u, y = p.y + 2 * v;
        const double z = (1-u)*(1-v)*p.z00 + u*(1-v)*p.z10 + (1-u)*v*p.z01 + u*v*p.z11;
        const double dx = x-q.x, dy = y-q.y, dz = z-q.z;
        const double squared = dx*dx + dy*dy + dz*dz;
        if (std::abs(dz) > vertical || (!containing && std::sqrt(squared) > radius)) continue;
        if (!best || squared < best->squared || (squared == best->squared && p.id < best->id))
            best = Match{p.id, {x,y,z}, squared};
    }
    return best;
}
inline double arcCost(const Patch& a, const Patch& b) {
    const double dx = double(a.x) - b.x, dy = double(a.y) - b.y;
    const double az = (double(a.z00) + a.z10 + a.z01 + a.z11) / 4;
    const double bz = (double(b.z00) + b.z10 + b.z01 + b.z11) / 4;
    return std::sqrt(dx*dx + dy*dy + (az-bz)*(az-bz));
}
inline std::vector<double> dijkstra(const Scene& s, std::size_t start) {
    // Original scene only. No production graph, heap, costs or route result.
    std::vector<double> distance(s.patches.size(), std::numeric_limits<double>::infinity());
    std::vector<bool> visited(s.patches.size());
    distance[start] = 0;
    for (std::size_t n = 0; n < s.patches.size(); ++n) {
        std::size_t selected = s.patches.size();
        for (std::size_t i = 0; i < s.patches.size(); ++i)
            if (!visited[i] && (selected == s.patches.size() || distance[i] < distance[selected]))
                selected = i;
        if (selected == s.patches.size() || !std::isfinite(distance[selected])) break;
        visited[selected] = true;
        for (const auto& arc : s.arcs) if (arc.from == s.patches[selected].id) {
            const auto to = std::find_if(s.patches.begin(), s.patches.end(),
                [&arc](const Patch& p) { return p.id == arc.to; });
            check(to != s.patches.end(), "oracle target");
            const auto i = static_cast<std::size_t>(to - s.patches.begin());
            distance[i] = std::min(distance[i], distance[selected] + arcCost(s.patches[selected], *to));
        }
    }
    return distance;
}
inline bool close(double a, double b) { return std::abs(a-b) <= 1e-10 * std::max({1., std::abs(a), std::abs(b)}); }
}
