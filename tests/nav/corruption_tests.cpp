// SPDX-License-Identifier: MPL-2.0
#include "evidence/fixture.hpp"
#include <cassert>
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif
#include "nav/query/route_search.hpp"
using namespace evidence;
namespace {
std::size_t rejectedCases = 0;
void bad(const Bytes& bytes, K kind, const Span& span) {
    ++rejectedCases;
    const auto r = load(bytes);
    const nav::diagnostics::NavError expected{kind, span.offset, span.record, span.field};
    if (r || r.value || r.error != expected) {
        std::fprintf(stderr, "expected %u/%zu/%u/%u got %u/%llu/%u/%u\n",
                     unsigned(kind), span.offset, unsigned(span.record), unsigned(span.field),
                     unsigned(r.error.kind), static_cast<unsigned long long>(r.error.offset),
                     unsigned(r.error.record), unsigned(r.error.field));
        std::abort();
    }
}
void values(const Fixture& f, unsigned v, bool full) {
    const auto r = load(f.bytes);
    assert(r);
    const auto& h = (*r.value)->header();
    assert(unsigned(h.version) == v && h.areaCount == (full ? 2U : 1U));
    assert(h.bspSize.has_value() == (v >= 4));
    if (h.bspSize) assert(*h.bspSize == 123);
    assert(h.headerBytes == (v == 5 ? (full ? 23U : 18U) : v == 4 ? 16U : 12U));
    assert(h.places.size() == (v == 5 && full ? 1U : 0U));
    if (!h.places.empty()) assert(h.places[0] == std::string("\xFF" "A", 2));
    const auto& areas = (*r.value)->areas();
    assert(areas.size() == h.areaCount);
    for (std::size_t i = 0; i < areas.size(); ++i) {
        const auto& a = areas[i];
        const float x = i == 0 ? 0.f : 3.f, z = i == 0 ? 0.f : 4.f;
        assert(a.id.value == i + 1 && a.attributes == 255);
        assert((a.extent.northWest == nav::model::NavVector3{x, 0, z}));
        assert((a.extent.southEast == nav::model::NavVector3{x + 2, 2, z}));
        assert(a.extent.northEastZ == z && a.extent.southWestZ == z);
        for (const auto& c : a.connections) {
            assert(c.size() == (full ? 1U : 0U));
            if (full) {
                assert(c[0].target.value == 2 - i);
                assert(c[0].traversal == nav::model::NavTraversalKind::Walk);
            }
        }
        assert(a.hidingSpots.size() == (full ? 1U : 0U));
        assert(a.approaches.size() == (full ? 1U : 0U));
        assert(a.encounters.size() == (full && v >= 3 ? 1U : 0U));
        if (full) {
            const auto& s = a.hidingSpots[0];
            assert((s.position == nav::model::NavVector3{x + 1, 1, z}));
            assert(s.id.has_value() == (v >= 2) && s.flags.has_value() == (v >= 2));
            if (v >= 2) assert(*s.id == 101 + i && *s.flags == 254);
            const auto& p = a.approaches[0];
            assert(p.here == a.id && p.previous == a.id && p.next == a.id);
            assert(p.previousToHereHow == 254 && p.hereToNextHow == 255);
            if (v >= 3) {
                const auto& e = a.encounters[0];
                assert(e.from == a.id && e.to == a.id);
                assert(e.fromDirection == 0 && e.toDirection == 3 && e.spots.size() == 1);
                assert(e.spots[0].hidingSpotId == 101 + i && e.spots[0].t == 255);
            }
        }
        assert(a.place.has_value() == (v == 5));
        if (a.place) assert(*a.place == (full ? 1 : 0));
    }
    const auto graph = nav::query::NavGraph::build(*r.value, {128, 4096, 4194304});
    assert(graph && (*graph.value)->edgeCount() == (full ? 8U : 0U));
    const auto route = nav::query::NavRouteSearch::search(
        **graph.value, {{1}, {full ? 2U : 1U}, {128, 4194304}, false});
    assert(route && route.value->status == nav::query::NavRouteStatus::Complete);
    assert(route.value->total == (full ? 5 : 0));
    assert(route.value->areas.size() == (full ? 2U : 1U));
    if (full) assert(route.value->steps[0].edge.direction == 0);
}
void corruptions(const Fixture& f, unsigned version) {
    // All byte prefixes, including inside a scalar/string, must point at that field.
    for (const auto& s : f.spans) {
        for (std::size_t n = s.offset; n < s.offset + s.width; ++n)
            bad(Bytes(f.bytes.begin(), f.bytes.begin() + static_cast<std::ptrdiff_t>(n)),
                K::EndOfInput, s);
        auto b = f.bytes;
        if (s.floating) {
            set(b, s, 0x7FC00000); bad(b, K::NonFiniteFloat, s);
            set(b, s, 0x7F800000); bad(b, K::NonFiniteFloat, s);
            continue;
        }
        K kind = K::None; std::uint32_t replacement = 0;
        switch (s.field) {
        case F::Magic: case F::AreaId: case F::HidingSpotId:
        case F::ApproachAreaId: kind = K::InvalidValue; break;
        case F::Version: replacement = 6; kind = K::UnsupportedValue; break;
        case F::AreaCount: case F::ConnectionCount: case F::HidingSpotCount:
        case F::ApproachCount: case F::EncounterCount: case F::EncounterSpotCount:
        case F::PlaceCount: replacement = 255; kind = K::CountLimitExceeded; break;
        case F::PlaceLength: kind = K::InvalidValue; break;
        case F::ConnectionAreaId: case F::EncounterSpotId:
            replacement = 999; kind = K::DanglingReference; break;
        case F::EncounterAreaId:
            if (version >= 3) { replacement = 999; kind = K::DanglingReference; }
            break;
        case F::EncounterDirection: replacement = 4; kind = K::UnsupportedValue; break;
        case F::Place: replacement = 2; kind = K::InvalidValue; break;
        case F::PlaceText:
            b[s.offset] = 0; bad(b, K::InvalidValue, s);
            b = f.bytes; b[s.offset + s.width - 1] = 1; bad(b, K::InvalidValue, s);
            break;
        default: break;
        }
        if (kind != K::None) {
            set(b, s, replacement);
            // Removing area 2 makes the earlier area-1 connection unresolved.
            // Semantic diagnostics select file offset, not definition order.
            const auto earlier = std::find_if(f.spans.begin(), f.spans.end(),
                [&s](const Span& p) {
                    return p.offset < s.offset && p.field == F::ConnectionAreaId;
                });
            if (s.field == F::AreaId && earlier != f.spans.end())
                bad(b, K::DanglingReference, *earlier);
            else bad(b, kind, s);
        }
    }
    auto b = f.bytes; b.push_back(0);
    bad(b, K::TrailingData, {f.bytes.size(), 1, R::RawInput, F::RawBytes, false});
}
void boundariesAndPrecedence() {
    const auto f = fixture(1, false);
    auto bytes = f.bytes;
    const Span version{4, 4, R::FileHeader, F::Version, false};
    set(bytes, version, 0); bad(bytes, K::UnsupportedValue, version);
    bytes = f.bytes;
    const Span count{8, 4, R::FileHeader, F::AreaCount, false};
    set(bytes, count, 0); bad(bytes, K::InvalidValue, count);
    bytes = f.bytes;
    const Span id{12, 4, R::Area, F::AreaId, false};
    const Span eastX{29, 4, R::Area, F::SouthEastExtent, true};
    set(bytes, eastX, 0); bad(bytes, K::InvalidGeometry, eastX);
    set(bytes, id, 0); bad(bytes, K::InvalidValue, id);
    bytes.push_back(1); bad(bytes, K::InvalidValue, id); // semantic before trailing
    bytes.resize(70); // decode after invalid semantics must win
    bad(bytes, K::EndOfInput, {67, 4, R::Encounter, F::EncounterCount, false});
    auto cap = limits();
    cap.maxInputBytes = f.bytes.size(); assert(load(f.bytes, cap));
    --cap.maxInputBytes;
    auto r = load(f.bytes, cap);
    assert(!r.value && r.error.kind == K::CountLimitExceeded && r.error.offset == 0);
    bytes.assign(65536, 0);
    bad(bytes, K::InvalidValue, {0, 4, R::FileHeader, F::Magic, false});
    bytes.push_back(0);
    bad(bytes, K::CountLimitExceeded, {0, 1, R::RawInput, F::RawBytes, false});
    cap = limits(); cap.maxSnapshotBytes = sizeof(nav::model::NavMeshSnapshot) + sizeof(nav::model::NavAreaRecord);
    assert(load(f.bytes, cap)); --cap.maxSnapshotBytes;
    r = load(f.bytes, cap); assert(!r.value && r.error.kind == K::CountLimitExceeded);

    // Exact maximum Place count, length (including NUL) and aggregate simultaneously.
    Fixture places;
    places.integer(0xFEEDFACE, 4, R::FileHeader, F::Magic);
    places.integer(5, 4, R::FileHeader, F::Version);
    places.integer(123, 4, R::FileHeader, F::BspSize);
    places.integer(64, 2, R::PlaceDictionary, F::PlaceCount);
    for (unsigned n = 0; n < 64; ++n) {
        places.integer(256, 2, R::PlaceDictionary, F::PlaceLength);
        places.bytes.insert(places.bytes.end(), 255, 0xFF); places.bytes.push_back(0);
    }
    places.integer(1, 4, R::FileHeader, F::AreaCount);
    const auto minimum = fixture(5, false);
    places.bytes.insert(places.bytes.end(), minimum.bytes.begin() + 18, minimum.bytes.end());
    assert(load(places.bytes));
    for (unsigned which = 0; which < 3; ++which) {
        cap = limits();
        if (which == 0) --cap.header.maxPlaces;
        if (which == 1) --cap.header.maxPlaceBytes;
        if (which == 2) --cap.header.maxTotalPlaceBytes;
        r = load(places.bytes, cap);
        assert(!r.value && r.error.kind == K::CountLimitExceeded);
    }
    const auto full = fixture(5, true);
    for (unsigned length : {0U, 1U, 257U}) {
        bytes = full.bytes; const Span s{14, 2, R::PlaceDictionary, F::PlaceLength, false};
        set(bytes, s, length); bad(bytes, length > 256 ? K::CountLimitExceeded : K::InvalidValue, s);
    }
}
}
int main(int argc, char** argv) {
#ifdef _MSC_VER
    _set_error_mode(_OUT_TO_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    if (argc > 2) return 2;
    boundariesAndPrecedence();
    if (argc == 2) std::filesystem::create_directories(argv[1]);
    for (unsigned v = 1; v <= 5; ++v) {
        for (bool full : {false, true}) {
            const auto f = evidence::fixture(v, full);
            values(f, v, full);
            corruptions(f, v);
            if (argc == 2) {
                const auto path = std::filesystem::path(argv[1]) /
                    ("v" + std::to_string(v) + (full ? "-full.nav" : "-minimal.nav"));
                std::ofstream out(path, std::ios::binary);
                out.write(reinterpret_cast<const char*>(f.bytes.data()),
                          static_cast<std::streamsize>(f.bytes.size()));
                assert(out);
            }
            std::printf("v%u %s bytes=%zu exact-prefixes=%zu fields=%zu OK\n",
                        v, full ? "full" : "minimal", f.bytes.size(), f.bytes.size(), f.spans.size());
        }
    }
    std::printf("exact rejected cases=%zu plus limit-boundary checks OK\n", rejectedCases);
}
