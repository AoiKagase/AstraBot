// SPDX-License-Identifier: MPL-2.0
#include "../../tools/nav_inspection.hpp"
#include "evidence/fixture.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace fs = std::filesystem;
namespace inspect = astrabot::tools::inspection;
using evidence::Bytes;
void check(bool value) { if (!value) throw std::runtime_error("inspection assertion failed"); }
void contains(const std::string& text, const std::string& part) {
    if (text.find(part) == std::string::npos) throw std::runtime_error("missing: " + part + "\n" + text);
}
void write(const fs::path& path, const Bytes& bytes) {
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    check(static_cast<bool>(out));
}
Bytes read(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    return Bytes(std::istreambuf_iterator<char>(in), {});
}
int main(int argc, char** argv) {
    try {
        check(argc == 2);
        const fs::path directory = argv[1];
        fs::create_directories(directory);
        const auto nav = directory / "sample.nav";
        const auto bsp = directory / "sample.bsp";
        write(bsp, Bytes(123, 0));
        inspect::Options options;
        options.nav = nav;
        const auto profile = inspect::compatibilityProfile();
        for (unsigned v = 1; v <= 5; ++v) {
            const auto bytes = evidence::fixture(v, true).bytes;
            write(nav, bytes);
            write(directory / ("v" + std::to_string(v) + ".nav"), bytes);
            std::ostringstream out;
            check(inspect::run(options, profile, out) == 0);
            contains(out.str(), "version=" + std::to_string(v));
            contains(out.str(), "areas=2\n");
            contains(out.str(), "connections=8\n");
            contains(out.str(), "hiding_spots=2\n");
            contains(out.str(), "approaches=2\n");
            contains(out.str(), v <= 2 ? "encounters_retained=0\n" : "encounters_retained=2\n");
            contains(out.str(), "bytes_consumed=" + std::to_string(bytes.size()));
            contains(out.str(), "compatibility=NotYetValidated");
            check(read(nav) == bytes);
            std::ostringstream repeat;
            check(inspect::run(options, profile, repeat) == 0 && repeat.str() == out.str());
        }
        options.bsp = bsp;
        std::ostringstream matched;
        check(inspect::run(options, profile, matched) == 0);
        contains(matched.str(), "bsp_size_comparison=Match");
        write(bsp, Bytes(122, 0));
        std::ostringstream mismatch;
        check(inspect::run(options, profile, mismatch) != 0);
        contains(mismatch.str(), "bsp_size_comparison=Mismatch");
        check(read(bsp) == Bytes(122, 0));
        options.bsp.reset();
        options.query = inspect::Query{{1, 1, 0}, {4, 1, 4}, 10, 10};
        std::ostringstream route;
        check(inspect::run(options, profile, route) == 0);
        contains(route.str(), "route_status=Complete");
        contains(route.str(), "route_areas=1,2");
        contains(route.str(), "route_total=5\n");
        contains(route.str(), "edge=1,2,0");
        auto limited = profile;
        limited.route.maxExpansions = 0;
        std::ostringstream expansion;
        check(inspect::run(options, limited, expansion) != 0);
        contains(expansion.str(), "route_status=ExpansionLimit");
        contains(expansion.str(), "route_areas=\n");
        options.query->goal = options.query->start;
        std::ostringstream same;
        check(inspect::run(options, profile, same) == 0);
        contains(same.str(), "route_areas=1\n");
        contains(same.str(), "route_total=0\n");
        options.query->start = {-1, 1, 0};
        std::ostringstream nearest;
        check(inspect::run(options, profile, nearest) == 0);
        contains(nearest.str(), "start_method=nearestGeometry");
        contains(nearest.str(), "query_semantics=geometry-only");
        options.query->radius = 0;
        std::ostringstream missingArea;
        check(inspect::run(options, profile, missingArea) != 0);
        contains(missingArea.str(), "start_match=None");
        options.query.reset();
        const auto good = read(nav);
        auto zeroId = evidence::fixture(5, true);
        for (const auto& span : zeroId.spans) {
            if (span.field == evidence::F::HidingSpotId) {
                evidence::set(zeroId.bytes, span, 0);
                write(nav, zeroId.bytes);
                std::ostringstream out;
                check(inspect::run(options, profile, out) != 0);
                contains(out.str(), "record=HidingSpot\n");
                contains(out.str(), "field=HidingSpotId\n");
                contains(out.str(), "offset=" + std::to_string(span.offset));
                break;
            }
        }
        const auto oversized = directory / "oversized.nav";
        { std::ofstream large(oversized, std::ios::binary);
          large.seekp(64 * 1024 * 1024); large.put(0); check(static_cast<bool>(large)); }
        options.nav = oversized;
        std::ostringstream oversizedReport;
        check(inspect::run(options, profile, oversizedReport) != 0);
        contains(oversizedReport.str(), "stage=input");
        contains(oversizedReport.str(), "kind=CountLimitExceeded");
        options.nav = nav;
        for (auto corrupt : {Bytes{}, Bytes(good.begin(), good.end() - 1)}) {
            write(nav, corrupt);
            std::ostringstream out;
            check(inspect::run(options, profile, out) != 0);
            contains(out.str(), "stage=load");
            contains(out.str(), "kind=EndOfInput");
            check(read(nav) == corrupt);
        }
        auto trailing = good; trailing.push_back(0); write(nav, trailing);
        std::ostringstream tail;
        check(inspect::run(options, profile, tail) != 0);
        contains(tail.str(), "kind=TrailingData");
        auto badMagic = good; badMagic[0] = 0; write(nav, badMagic);
        std::ostringstream magic;
        check(inspect::run(options, profile, magic) != 0);
        contains(magic.str(), "field=Magic\n");
        write(nav, good);
        limited = profile; limited.mesh.maxInputBytes = good.size() - 1;
        std::ostringstream size;
        check(inspect::run(options, limited, size) != 0);
        contains(size.str(), "stage=input"); contains(size.str(), "kind=CountLimitExceeded");
        limited.mesh.maxInputBytes = good.size();
        std::ostringstream exact;
        check(inspect::run(options, limited, exact) == 0);
        limited = profile; limited.route.maxWorkingBytes = 1;
        options.query = inspect::Query{{1, 1, 0}, {4, 1, 4}, 10, 10};
        std::ostringstream workingBytes;
        check(inspect::run(options, limited, workingBytes) != 0);
        contains(workingBytes.str(), "stage=route\n");
        contains(workingBytes.str(), "kind=CountLimitExceeded\n");
        options.query.reset();
        for (int stage = 0; stage < 3; ++stage) {
            limited = profile;
            if (stage == 0) limited.mesh.maxSnapshotBytes = 1;
            if (stage == 1) limited.index.maxIndexBytes = 1;
            if (stage == 2) limited.graph.maxGraphBytes = 1;
            std::ostringstream out;
            check(inspect::run(options, limited, out) != 0);
            contains(out.str(), "kind=CountLimitExceeded");
        }
        options.nav = directory / "missing.nav";
        std::ostringstream missing;
        check(inspect::run(options, profile, missing) != 0);
        check(!fs::exists(options.nav));
        options.nav = nav;
        // Original directed v1 fixture: remove outgoing edges only from area 2.
        auto directed = evidence::fixture(1, true);
        std::size_t secondAreaOffset = 0;
        unsigned areaIds = 0;
        for (const auto& span : directed.spans) {
            if (span.record == evidence::R::Area && span.field == evidence::F::AreaId && ++areaIds == 2)
                secondAreaOffset = span.offset;
        }
        check(secondAreaOffset != 0);
        for (auto it = directed.spans.rbegin(); it != directed.spans.rend(); ++it) {
            if (it->record == evidence::R::Connection && it->field == evidence::F::ConnectionCount &&
                it->offset > secondAreaOffset) {
                evidence::set(directed.bytes, *it, 0);
                directed.bytes.erase(directed.bytes.begin() + static_cast<std::ptrdiff_t>(it->offset + 4),
                                     directed.bytes.begin() + static_cast<std::ptrdiff_t>(it->offset + 8));
            }
        }
        write(nav, directed.bytes);
        write(directory / "directed.nav", directed.bytes);
        options.query = inspect::Query{{4, 1, 4}, {1, 1, 0}, 10, 10};
        std::ostringstream unreachable;
        check(inspect::run(options, profile, unreachable) == 0);
        contains(unreachable.str(), "route_status=Unreachable");
        contains(unreachable.str(), "route_areas=\n");
        std::cout << "inspection tests passed\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
