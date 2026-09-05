// SPDX-License-Identifier: MPL-2.0
#include "nav_inspection.hpp"
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace astrabot::tools::inspection {
namespace {
namespace fs = std::filesystem;
using namespace nav;
using namespace diagnostics;
constexpr std::size_t mib = 1024 * 1024;
const char* kindName(NavErrorKind kind) {
    switch (kind) {
#define K(name) case NavErrorKind::name: return #name;
    K(None) K(EndOfInput) K(OffsetOverflow) K(NonFiniteFloat) K(InvalidInput)
    K(InvalidValue) K(UnsupportedValue) K(CountLimitExceeded) K(DuplicateId)
    K(DanglingReference) K(InvalidGeometry) K(TrailingData) K(AllocationFailure) K(PolicyFailure)
#undef K
    }
    return "Unknown";
}
const char* recordName(NavRecord value) {
    switch (value) {
    case NavRecord::None: return "None";
    case NavRecord::RawInput: return "RawInput";
    case NavRecord::FileHeader: return "FileHeader";
    case NavRecord::PlaceDictionary: return "PlaceDictionary";
    case NavRecord::Area: return "Area";
    case NavRecord::Connection: return "Connection";
    case NavRecord::HidingSpot: return "HidingSpot";
    case NavRecord::Approach: return "Approach";
    case NavRecord::Encounter: return "Encounter";
    case NavRecord::TraversalLink: return "TraversalLink";
    case NavRecord::Graph: return "Graph";
    case NavRecord::Route: return "Route";
    }
    return "Unknown";
}
const char* fieldName(NavField value) {
    switch (value) {
    case NavField::None: return "None";
    case NavField::RawBytes: return "RawBytes";
    case NavField::Magic: return "Magic";
    case NavField::Version: return "Version";
    case NavField::BspSize: return "BspSize";
    case NavField::PlaceCount: return "PlaceCount";
    case NavField::PlaceLength: return "PlaceLength";
    case NavField::PlaceText: return "PlaceText";
    case NavField::AreaCount: return "AreaCount";
    case NavField::AreaId: return "AreaId";
    case NavField::Attributes: return "Attributes";
    case NavField::NorthWestExtent: return "NorthWestExtent";
    case NavField::SouthEastExtent: return "SouthEastExtent";
    case NavField::NorthEastZ: return "NorthEastZ";
    case NavField::SouthWestZ: return "SouthWestZ";
    case NavField::ConnectionCount: return "ConnectionCount";
    case NavField::ConnectionAreaId: return "ConnectionAreaId";
    case NavField::HidingSpotCount: return "HidingSpotCount";
    case NavField::HidingSpotId: return "HidingSpotId";
    case NavField::HidingSpotFlags: return "HidingSpotFlags";
    case NavField::ApproachCount: return "ApproachCount";
    case NavField::ApproachAreaId: return "ApproachAreaId";
    case NavField::ApproachTraversal: return "ApproachTraversal";
    case NavField::EncounterCount: return "EncounterCount";
    case NavField::EncounterAreaId: return "EncounterAreaId";
    case NavField::EncounterDirection: return "EncounterDirection";
    case NavField::EncounterSpotCount: return "EncounterSpotCount";
    case NavField::EncounterSpotId: return "EncounterSpotId";
    case NavField::EncounterSpotT: return "EncounterSpotT";
    case NavField::Place: return "Place";
    case NavField::GraphBytes: return "GraphBytes";
    case NavField::GraphTraversal: return "GraphTraversal";
    case NavField::RouteStart: return "RouteStart";
    case NavField::RouteGoal: return "RouteGoal";
    case NavField::RouteCost: return "RouteCost";
    case NavField::RouteHeuristic: return "RouteHeuristic";
    case NavField::RouteBytes: return "RouteBytes";
    case NavField::LinkCount: return "LinkCount";
    case NavField::LinkWorkingBytes: return "LinkWorkingBytes";
    case NavField::LinkFingerprint: return "LinkFingerprint";
    case NavField::LinkSourceId: return "LinkSourceId";
    case NavField::LinkGeneration: return "LinkGeneration";
    case NavField::LinkId: return "LinkId";
    case NavField::LinkFrom: return "LinkFrom";
    case NavField::LinkTo: return "LinkTo";
    case NavField::LinkTraversal: return "LinkTraversal";
    case NavField::LinkDirection: return "LinkDirection";
    case NavField::LinkEntry: return "LinkEntry";
    case NavField::LinkExit: return "LinkExit";
    case NavField::LinkCost: return "LinkCost";
    case NavField::LinkGenerationConflict: return "LinkGenerationConflict";
    case NavField::LinkConflict: return "LinkConflict";
    }
    return "Unknown";
}
int error(std::ostream& out, const char* stage, NavError reason) {
    out << "stage=" << stage << "\nkind=" << kindName(reason.kind)
        << "\nrecord=" << recordName(reason.record)
        << "\nfield=" << fieldName(reason.field)
        << "\noffset=" << reason.offset << '\n';
    return 1;
}
int failure(std::ostream& out, const char* stage, NavErrorKind kind) {
    return error(out, stage, {kind, 0, NavRecord::RawInput, NavField::RawBytes});
}
void limits(std::ostream& out, const Profile& p) {
#define L(field) out << "limit." #field "=" << p.field << '\n';
    L(mesh.maxInputBytes) L(mesh.header.maxAreas) L(mesh.header.maxPlaces)
    L(mesh.header.maxPlaceBytes) L(mesh.header.maxTotalPlaceBytes)
    L(mesh.areas.maxAreas) L(mesh.areas.maxConnectionsPerDirection)
    L(mesh.areas.maxHidingSpotsPerArea) L(mesh.areas.maxApproachesPerArea)
    L(mesh.areas.maxEncountersPerArea) L(mesh.areas.maxEncounterSpotsPerPath)
    L(mesh.areas.maxTotalConnections) L(mesh.areas.maxTotalHidingSpots)
    L(mesh.areas.maxTotalApproaches) L(mesh.areas.maxTotalEncounters)
    L(mesh.areas.maxTotalEncounterSpots) L(mesh.maxSnapshotBytes)
    L(index.maxAreas) L(index.maxNodes) L(index.maxIndexBytes)
    L(graph.maxAreas) L(graph.maxEdges) L(graph.maxGraphBytes)
    L(route.maxExpansions) L(route.maxWorkingBytes)
#undef L
}
bool fileLength(const fs::path& path, std::uintmax_t& length) {
    std::error_code ec;
    if (!fs::is_regular_file(path, ec) || ec) return false;
    length = fs::file_size(path, ec);
    return !ec;
}
void vector(std::ostream& out, model::NavVector3 v) { out << v.x << ',' << v.y << ',' << v.z; }
void metadata(std::ostream& out, const model::NavMeshSnapshot& mesh, std::size_t bytes) {
    const auto& header = mesh.header();
    out << "version=" << static_cast<unsigned>(header.version)
        << "\nheader_bytes=" << header.headerBytes << "\nbytes_consumed=" << bytes
        << "\nareas=" << mesh.areas().size() << "\nplaces=" << header.places.size() << '\n';
    out << "nav_bsp_size=";
    if (header.bspSize) out << *header.bspSize; else out << "Absent";
    out << '\n';
    constexpr char hex[] = "0123456789abcdef";
    for (std::size_t i = 0; i < header.places.size(); ++i) {
        const auto& place = header.places[i];
        out << "place=" << i + 1 << ",wire_length=" << place.size() + 1 << ",hex=";
        for (char c : place) { const auto b = static_cast<unsigned char>(c); out << hex[b >> 4] << hex[b & 15]; }
        out << "00\n";
    }
    std::uint64_t connections = 0, hiding = 0, approaches = 0, encounters = 0, spots = 0;
    for (const auto& area : mesh.areas()) {
        out << "area=" << area.id.value << ",attributes=" << static_cast<unsigned>(area.attributes)
            << ",nw="; vector(out, area.extent.northWest);
        out << ",se="; vector(out, area.extent.southEast);
        out << ",ne_z=" << area.extent.northEastZ << ",sw_z=" << area.extent.southWestZ << ",place=";
        if (area.place) out << *area.place; else out << "Absent";
        out << '\n';
        for (std::size_t d = 0; d < 4; ++d) {
            connections += area.connections[d].size();
            out << "connections_from=" << area.id.value << ",direction=" << d << ",targets=";
            bool first = true;
            for (const auto& edge : area.connections[d]) {
                if (!first) out << ',';
                out << edge.target.value; first = false;
            }
            out << '\n';
        }
        hiding += area.hidingSpots.size(); approaches += area.approaches.size();
        encounters += area.encounters.size();
        for (const auto& encounter : area.encounters) spots += encounter.spots.size();
    }
    out << "connections=" << connections << "\nhiding_spots=" << hiding
        << "\napproaches=" << approaches << "\nencounters_retained=" << encounters
        << "\nencounter_spots_retained=" << spots << "\nlegacy_encounters="
        << (header.version <= model::NavVersion::V2 ? "DiscardedNotCounted" : "NotApplicable") << '\n';
}
std::optional<model::NavAreaId> match(std::ostream& out, const char* label,
    const query::NavSpatialIndex& index, model::NavVector3 point, const Query& q) {
    auto result = index.containing(point, q.vertical);
    const char* method = "containing";
    if (result && !*result.value) {
        method = "nearestGeometry";
        result = index.nearestGeometry(point, {q.radius, q.vertical});
    }
    out << label << "_position="; vector(out, point);
    out << '\n' << label << "_method=" << method << '\n';
    if (!result) { error(out, label, result.error); return {}; }
    if (!*result.value) { out << label << "_match=None\n"; return {}; }
    const auto& hit = **result.value;
    out << label << "_match=" << hit.areaId.value << '\n' << label << "_distance_squared="
        << hit.distanceSquared << '\n' << label << "_projected=" << hit.projectedPoint.x << ','
        << hit.projectedPoint.y << ',' << hit.projectedPoint.z << '\n';
    return hit.areaId;
}
void costs(std::ostream& out, const query::NavCostComponents& c) {
    out << c.distance << ',' << c.traversal << ',' << c.danger << ',' << c.experience;
}
int routes(std::ostream& out, const Options& options, const Profile& profile,
           const query::NavSpatialIndex& index, const query::NavGraph& graph) {
    if (!options.query) { out << "query=NotRequested\n"; return 0; }
    const auto& q = *options.query;
    out << "query_semantics=geometry-only\nquery_radius=" << q.radius
        << "\nquery_vertical=" << q.vertical << "\nallow_partial=false\n";
    const auto start = match(out, "start", index, q.start, q);
    const auto goal = match(out, "goal", index, q.goal, q);
    if (!start || !goal) return failure(out, "query", NavErrorKind::InvalidValue);
    const auto route = query::NavRouteSearch::search(graph, {*start, *goal, profile.route, false});
    if (!route) return error(out, "route", route.error);
    const auto& r = *route.value;
    const char* status = r.status == query::NavRouteStatus::Complete ? "Complete" :
        r.status == query::NavRouteStatus::Unreachable ? "Unreachable" : "ExpansionLimit";
    out << "route_status=" << status << "\nroute_areas=";
    for (std::size_t i = 0; i < r.areas.size(); ++i) { if (i) out << ','; out << r.areas[i].value; }
    out << "\nroute_total=" << r.total << "\nroute_components="; costs(out, r.components); out << '\n';
    for (const auto& step : r.steps) {
        out << "edge=" << step.edge.source.value << ',' << step.edge.target.value << ','
            << static_cast<unsigned>(step.edge.direction) << ",traversal="
            << static_cast<unsigned>(step.edge.traversal) << ",external=false,total=" << step.total << ",components=";
        costs(out, step.components); out << '\n';
    }
    out << "expansions=" << r.metrics.expansions << "\nexamined_edges=" << r.metrics.examinedEdges
        << "\nrelaxations=" << r.metrics.relaxations << "\nreopens=" << r.metrics.reopens
        << "\npeak_open=" << r.metrics.peakOpen << '\n';
    return r.status == query::NavRouteStatus::ExpansionLimit ? 1 : 0;
}
bool number(const char* text, double& value) {
    std::istringstream in(text); in.imbue(std::locale::classic()); in >> std::noskipws >> value;
    return in && in.peek() == std::char_traits<char>::eof() && std::isfinite(value);
}
bool point(const char* const* text, model::NavVector3& value) {
    double v[3]{};
    for (int i = 0; i < 3; ++i)
        if (!number(text[i], v[i]) || std::abs(v[i]) > std::numeric_limits<float>::max()) return false;
    value = {static_cast<float>(v[0]), static_cast<float>(v[1]), static_cast<float>(v[2])};
    return true;
}
int usage(std::ostream& out, bool help) {
    if (!help) failure(out, "cli", NavErrorKind::InvalidInput);
    out << "usage: astrabot_nav_inspect --nav PATH [--bsp PATH] [--profile compatibility-v1]\n"
        << "       [--start X Y Z --goal X Y Z --radius R --vertical V]\n"
        << "Queries are geometry-only. R and V must be finite and nonnegative.\n";
    return help ? 0 : 2;
}
} // namespace
Profile compatibilityProfile() noexcept {
    return {{64 * mib, {100000, 65535, 65535, 8 * mib},
        {100000, 4096, 255, 255, 65536, 255, 1000000, 1000000, 1000000, 1000000, 1000000}, 256 * mib},
        {100000, 199999, 256 * mib}, {100000, 1000000, 256 * mib}, {100000, 256 * mib}};
}
int run(const Options& options, const Profile& profile, std::ostream& out) {
    out.imbue(std::locale::classic()); out << std::setprecision(std::numeric_limits<double>::max_digits10);
    out << "report=astrabot.nav-inspection.v1\ncompatibility=NotYetValidated\n"
        << "compatibility_note=IndependentExpectedEvidenceRequired\n";
    limits(out, profile);
    try {
        std::uintmax_t length = 0;
        if (!fileLength(options.nav, length)) return failure(out, "input", NavErrorKind::InvalidInput);
        out << "input_bytes=" << length << '\n';
        if (length > profile.mesh.maxInputBytes) return failure(out, "input", NavErrorKind::CountLimitExceeded);
        if (length > static_cast<std::uintmax_t>(std::numeric_limits<std::streamsize>::max()) ||
            length > std::numeric_limits<std::size_t>::max()) return failure(out, "input", NavErrorKind::OffsetOverflow);
        std::ifstream file(options.nav, std::ios::binary);
        if (!file) return failure(out, "input", NavErrorKind::InvalidInput);
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
        if (length != 0) file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(length));
        if (!file || file.gcount() != static_cast<std::streamsize>(length))
            return failure(out, "input", NavErrorKind::EndOfInput);
        if (file.peek() != std::char_traits<char>::eof() || file.bad())
            return failure(out, "input", NavErrorKind::InvalidInput);
        const auto loaded = io::NavMeshLoader::load({bytes.data(), bytes.size()}, profile.mesh);
        if (!loaded) return error(out, "load", loaded.error);
        const auto& mesh = **loaded.value;
        metadata(out, mesh, bytes.size());
        bool bspMismatch = false;
        if (options.bsp) {
            std::uintmax_t bspBytes = 0;
            if (!fileLength(*options.bsp, bspBytes)) return failure(out, "bsp", NavErrorKind::InvalidInput);
            out << "bsp_bytes=" << bspBytes << "\nbsp_size_comparison=";
            if (!mesh.header().bspSize) out << "AbsentInVersion";
            else { bspMismatch = bspBytes != *mesh.header().bspSize; out << (bspMismatch ? "Mismatch" : "Match"); }
            out << '\n';
        } else out << "bsp_size_comparison=Unverified\n";
        const auto index = query::NavSpatialIndex::build(*loaded.value, profile.index);
        if (!index) return error(out, "index", index.error);
        const auto graph = query::NavGraph::build(*loaded.value, profile.graph);
        if (!graph) return error(out, "graph", graph.error);
        out << "graph_areas=" << (*graph.value)->areaCount() << "\ngraph_edges=" << (*graph.value)->edgeCount()
            << "\ngraph_logical_bytes=" << (*graph.value)->logicalBytes() << '\n';
        const auto code = routes(out, options, profile, **index.value, **graph.value);
        if (bspMismatch) return error(out, "bsp",
            {NavErrorKind::InvalidValue, 8, NavRecord::FileHeader, NavField::BspSize});
        return out && code == 0 ? 0 : 1;
    } catch (const std::bad_alloc&) { return failure(out, "inspection", NavErrorKind::AllocationFailure); }
      catch (const std::length_error&) { return failure(out, "inspection", NavErrorKind::AllocationFailure); }
      catch (const fs::filesystem_error&) { return failure(out, "input", NavErrorKind::InvalidInput); }
}
int cli(int argc, const char* const* argv, std::ostream& out) {
    try {
        if (argc == 2 && std::string_view(argv[1]) == "--help") return usage(out, true);
        Options options; Query q; unsigned seen = 0;
        for (int i = 1; i < argc; ++i) {
            const std::string_view arg = argv[i];
            unsigned flag = 0; int count = 1;
            if (arg == "--nav") flag = 1;
            else if (arg == "--bsp") flag = 2;
            else if (arg == "--profile") flag = 4;
            else if (arg == "--start") { flag = 8; count = 3; }
            else if (arg == "--goal") { flag = 16; count = 3; }
            else if (arg == "--radius") flag = 32;
            else if (arg == "--vertical") flag = 64;
            if (!flag || (seen & flag) || argc - i - 1 < count) return usage(out, false);
            seen |= flag;
            if (flag == 1 || flag == 2) {
                if (!argv[i + 1][0] || std::string_view(argv[i + 1]).substr(0, 2) == "--") return usage(out, false);
                if (flag == 1) options.nav = fs::path(argv[i + 1]); else options.bsp = fs::path(argv[i + 1]);
            } else if (flag == 4) {
                if (std::string_view(argv[i + 1]) != "compatibility-v1") return usage(out, false);
            } else if (flag == 8 || flag == 16) {
                if (!point(argv + i + 1, flag == 8 ? q.start : q.goal)) return usage(out, false);
            } else if (!number(argv[i + 1], flag == 32 ? q.radius : q.vertical) ||
                       (flag == 32 ? q.radius : q.vertical) < 0) return usage(out, false);
            i += count;
        }
        if (!(seen & 1) || ((seen & 120) != 0 && (seen & 120) != 120)) return usage(out, false);
        if (seen & 120) options.query = q;
        return run(options, compatibilityProfile(), out);
    } catch (const std::exception&) { return failure(out, "cli", NavErrorKind::InvalidInput); }
}
} // namespace astrabot::tools::inspection
