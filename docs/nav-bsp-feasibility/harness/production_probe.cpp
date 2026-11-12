// Diagnostic harness: compile the CURRENT unmodified production NAV sources.
// stdout is JSON. No files, game entities, NAV data or product state are modified.
#include "astrabot/nav/legacy_nav_reader.hpp"
#include "astrabot/nav/nav_query.hpp"
#include "astrabot/nav/nav_snapshot.hpp"
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>
#include <vector>

using namespace astrabot::nav;
struct Digest
{
    std::uint64_t h = 14695981039346656037ULL;
    void u32(std::uint32_t v)
    {
        for (unsigned i = 0; i < 4; ++i) { h = (h ^ ((v >> (i * 8)) & 255U)) * 1099511628211ULL; }
    }
    void f32(float f) { std::uint32_t v; std::memcpy(&v, &f, sizeof(v)); u32(v); }
    void vec(const NavVector &v) { f32(v.x); f32(v.y); f32(v.z); }
    std::string area(const NavArea &a)
    {
        u32(a.id); u32(a.attributes); vec(a.extent.lo); vec(a.extent.hi);
        f32(a.northEastZ); f32(a.southWestZ); u32(a.placeId);
        for (const auto &ids : a.connections) { u32(static_cast<std::uint32_t>(ids.size())); for (auto id : ids) u32(id); }
        u32(static_cast<std::uint32_t>(a.hidingSpots.size()));
        for (const auto &s : a.hidingSpots) { u32(s.id); vec(s.position); u32(s.flags); }
        u32(static_cast<std::uint32_t>(a.approaches.size()));
        for (const auto &p : a.approaches) { u32(p.here); u32(p.previous); u32(p.previousToHereHow); u32(p.next); u32(p.hereToNextHow); }
        u32(static_cast<std::uint32_t>(a.encounters.size()));
        for (const auto &e : a.encounters)
        {
            u32(e.fromArea); u32(e.fromDirection); u32(e.toArea); u32(e.toDirection);
            u32(static_cast<std::uint32_t>(e.spots.size()));
            for (const auto &s : e.spots) { u32(s.hidingSpotId); u32(static_cast<std::uint32_t>(std::lround(s.t * 255.0f))); }
        }
        std::ostringstream out; out << std::hex << std::setfill('0') << std::setw(16) << h; return out.str();
    }
};

int main(int argc, char **argv)
{
    if (argc != 2 && argc != 3) return 1;
    std::ifstream input(argv[1], std::ios::binary);
    if (!input) return 2;
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(input)), {});
    NavDocument document;
    auto result = LegacyNavReader().read(bytes.data(), bytes.size(), &document);
    std::cout << "{\"read_result\":" << static_cast<int>(result);
    if (result != NavReadResult::Loaded) { std::cout << "}\n"; return 3; }
    std::cout << ",\"version\":" << document.sourceIdentity().sourceVersion
              << ",\"bsp_size\":" << document.sourceIdentity().bspSize << ",\"places\":[";
    bool first = true;
    for (const auto &p : document.places())
    {
        if (!first) std::cout << ',';
        first = false; std::cout << std::quoted(p.name);
    }
    std::cout << "],\"area_digests\":{"; first = true;
    for (const auto &a : document.areas())
    {
        if (!first) std::cout << ',';
        first = false; std::cout << '"' << a.id << "\":\"" << Digest().area(a) << '"';
    }
    std::cout << '}';
    if (argc == 3)
    {
        NavSnapshotPublisher publisher;
        if (publisher.publish(&document, 1) != NavSnapshotResult::Published) return 4;
        const auto snapshot = publisher.snapshot(); NavQuery query(snapshot);
        std::ifstream points(argv[2]); if (!points) return 5;
        AreaId id; NavVector point = {}; float predicted;
        first = true; std::cout << ",\"point_queries\":[" << std::setprecision(9);
        while (points >> id >> point.x >> point.y >> point.z >> predicted)
        {
            const auto area = document.findArea(id); if (area == nullptr) return 6;
            if (!first) std::cout << ','; first = false;
            NavAreaMatch m16 = {}, m64 = {};
            const auto r16 = query.findContaining(point, 16, &m16);
            const auto r64 = query.findContaining(point, 64, &m64);
            std::cout << "{\"source_area\":" << id << ",\"x\":" << point.x << ",\"y\":" << point.y
                      << ",\"physical_z\":" << point.z << ",\"predicted_nav_z\":" << predicted
                      << ",\"production_nav_z\":" << surfaceZAt(*area, point.x, point.y)
                      << ",\"result16\":" << static_cast<int>(r16) << ",\"match16\":" << m16.area
                      << ",\"result64\":" << static_cast<int>(r64) << ",\"match64\":" << m64.area << '}';
        }
        std::cout << ']';
    }
    std::cout << "}\n";
    return 0;
}
