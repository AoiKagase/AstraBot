// SPDX-License-Identifier: MPL-2.0
#include "nav/io/decode_context.hpp"
#include "nav/io/mesh_loader.hpp"
#include "evidence/fixture.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <type_traits>
#include <vector>

namespace {
std::size_t failAfter = std::numeric_limits<std::size_t>::max();
void *allocate(std::size_t n) {
    if (failAfter != std::numeric_limits<std::size_t>::max()) {
        if (failAfter == 0)
            throw std::bad_alloc{};
        --failAfter;
    }
    auto p = std::malloc(n ? n : 1);
    if (!p)
        throw std::bad_alloc{};
    return p;
}
} // namespace
#ifdef _MSC_VER
_Ret_notnull_ _Post_writable_byte_size_(n)
#endif
void *operator new(std::size_t n) { return allocate(n); }
#ifdef _MSC_VER
_Ret_notnull_ _Post_writable_byte_size_(n)
#endif
void *operator new[](std::size_t n) {
    return allocate(n);
}
void operator delete(void *p) noexcept { std::free(p); }
void operator delete[](void *p) noexcept { std::free(p); }
void operator delete(void *p, std::size_t) noexcept { std::free(p); }
void operator delete[](void *p, std::size_t) noexcept { std::free(p); }

using namespace astrabot::nav;
namespace {
void u32(std::vector<std::uint8_t> &b, std::uint32_t v) {
    for (unsigned i = 0; i < 4; ++i)
        b.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
}
void f32(std::vector<std::uint8_t> &b, float f) {
    std::uint32_t v;
    std::memcpy(&v, &f, 4);
    u32(b, v);
}
void set(std::vector<std::uint8_t> &b, std::size_t o, std::uint32_t v) {
    for (unsigned i = 0; i < 4; ++i)
        b[o + i] = static_cast<std::uint8_t>(v >> (8 * i));
}
io::NavMeshReadLimits limits() {
    return {4096, {8, 8, 32, 128}, {8, 8, 8, 8, 8, 8, 32, 32, 32, 32, 32}, 65536};
}
std::vector<std::uint8_t> fixture(unsigned version) {
    std::vector<std::uint8_t> b;
    u32(b, 0xFEEDFACE);
    u32(b, version);
    if (version >= 4)
        u32(b, 123);
    if (version == 5) {
        b.insert(b.end(), {1, 0, 2, 0, 0xFF, 0});
    }
    u32(b, 1);
    u32(b, 7);
    b.push_back(0xFF);
    for (float f : {0.f, 0.f, 10.f, 20.f, 20.f, -10.f, 5.f, 6.f})
        f32(b, f);
    for (unsigned i = 0; i < 4; ++i)
        u32(b, 0);
    b.push_back(0);
    b.push_back(0);
    u32(b, 0);
    if (version == 5) {
        b.push_back(1);
        b.push_back(0);
    }
    return b;
}
auto load(const std::vector<std::uint8_t> &b, io::NavMeshReadLimits l = limits()) {
    return io::NavMeshLoader::load({b.data(), b.size()}, l);
}
void bad(const std::vector<std::uint8_t> &b, diagnostics::NavErrorKind k, std::uint64_t offset,
         diagnostics::NavField field) {
    auto r = load(b);
    assert(!r && !r.value);
    if (r.error.kind != k || r.error.offset != offset || r.error.field != field)
        std::fprintf(stderr, "expected %u/%llu/%u, got %u/%llu/%u\n", unsigned(k),
                     static_cast<unsigned long long>(offset), unsigned(field),
                     unsigned(r.error.kind), static_cast<unsigned long long>(r.error.offset),
                     unsigned(r.error.field));
    assert(r.error.kind == k);
    assert(r.error.offset == offset);
    assert(r.error.field == field);
}

void traversalDecodeAndBudget() {
    using K = diagnostics::NavErrorKind;
    for (unsigned version = 1; version <= 5; ++version) {
        auto b = fixture(version);
        const std::size_t header = version == 5 ? 22U : version == 4 ? 16U : 12U;
        const auto areaBytes = b.size() - header;
        // One directed connection in each cardinal bucket, all to area 8.
        for (std::size_t direction = 4; direction-- > 0;) {
            const auto count = header + 37U + direction * 4U;
            set(b, count, 1);
            b.insert(b.begin() + static_cast<std::ptrdiff_t>(count + 4U), {8, 0, 0, 0});
        }
        const auto secondStart = b.size();
        auto second = fixture(version);
        b.insert(b.end(), second.end() - static_cast<std::ptrdiff_t>(areaBytes), second.end());
        set(b, header - 4U, 2);
        set(b, secondStart, 8);
        auto l = limits();
        l.maxSnapshotBytes = sizeof(model::NavMeshSnapshot) + 2 * sizeof(model::NavAreaRecord) +
                             4 * sizeof(model::NavConnection);
        if (version == 5)
            l.maxSnapshotBytes += sizeof(std::string) + 2;
        auto result = load(b, l);
        assert(result);
        const auto& areas = (*result.value)->areas();
        for (const auto& direction : areas[0].connections) {
            assert(direction.size() == 1);
            assert(direction[0].target == model::NavAreaId{8});
            assert(direction[0].traversal == model::NavTraversalKind::Walk);
        }
        for (const auto& direction : areas[1].connections)
            assert(direction.empty()); // No inferred reverse edges.
        assert(areas[0].attributes == 0xFF); // Raw bytes do not override Walk.
        --l.maxSnapshotBytes;
        auto rejected = load(b, l);
        assert(!rejected && !rejected.value && rejected.error.kind == K::CountLimitExceeded);
        bool success = false;
        for (std::size_t n = 0; n < 256; ++n) {
            failAfter = n;
            auto attempt = load(b);
            failAfter = std::numeric_limits<std::size_t>::max();
            if (attempt) { success = true; break; }
            assert(!attempt.value && attempt.error.kind == K::AllocationFailure);
        }
        assert(success);
        assert(areas[0].connections[0][0].target == model::NavAreaId{8});
    }
}

void zeroIdentityCompatibility() {
    using K = diagnostics::NavErrorKind;
    using F = diagnostics::NavField;
    for (unsigned version = 1; version <= 5; ++version) {
        auto fixture = evidence::fixture(version, true);
        for (const auto& span : fixture.spans) {
            if (span.field == F::ApproachAreaId)
                evidence::set(fixture.bytes, span, 0);
            if (span.field == F::HidingSpotId && fixture.bytes[span.offset] == 101)
                evidence::set(fixture.bytes, span, 0);
            if (span.field == F::EncounterSpotId && fixture.bytes[span.offset] == 101)
                evidence::set(fixture.bytes, span, 0);
        }
        const auto result = evidence::load(fixture.bytes);
        if (!result) {
            std::fprintf(stderr, "v%u zero identity/reference rejected: kind=%u field=%u offset=%llu\n",
                version, unsigned(result.error.kind), unsigned(result.error.field),
                static_cast<unsigned long long>(result.error.offset));
            std::exit(1);
        }
        const auto& area = (*result.value)->areas().front();
        assert(area.hidingSpots.front().id.has_value() == (version >= 2));
        if (version >= 2) assert(*area.hidingSpots.front().id == 0);
        assert(area.approaches.front().here.value == 0);
        assert(area.approaches.front().previous.value == 0);
        assert(area.approaches.front().next.value == 0);
        if (version >= 3) assert(area.encounters.front().spots.front().hidingSpotId == 0);
        for (const auto& span : fixture.spans) {
            if (span.field == F::HidingSpotId && fixture.bytes[span.offset] == 102) {
                auto duplicate = fixture.bytes;
                evidence::set(duplicate, span, 0);
                bad(duplicate, K::DuplicateId, span.offset, F::HidingSpotId);
            }
            if (span.field == F::ApproachAreaId) {
                auto missing = fixture.bytes;
                evidence::set(missing, span, 999);
                bad(missing, K::DanglingReference, span.offset, F::ApproachAreaId);
            }
        }
        if (version >= 3) {
            auto absent = evidence::fixture(version, true);
            for (const auto& span : absent.spans) {
                if (span.field == F::EncounterSpotId) {
                    auto bytes = absent.bytes;
                    evidence::set(bytes, span, 0);
                    bad(bytes, K::DanglingReference, span.offset, F::EncounterSpotId);
                }
            }
        }
    }
}

void tacticalAndConnections() {
    using K = diagnostics::NavErrorKind;
    using F = diagnostics::NavField;
    // V3 one area: hiding 101, owner-referencing approach, owner encounter.
    auto b = fixture(3);
    b.resize(65);
    b.push_back(1);
    u32(b, 101);
    f32(b, 1);
    f32(b, 2);
    f32(b, 3);
    b.push_back(0xFF);
    b.push_back(1);
    u32(b, 7);
    u32(b, 7);
    b.push_back(0xFF);
    u32(b, 7);
    b.push_back(0xFF);
    u32(b, 1);
    u32(b, 7);
    b.push_back(0);
    u32(b, 7);
    b.push_back(3);
    b.push_back(1);
    u32(b, 101);
    b.push_back(255);
    assert(b.size() == 118);
    assert(load(b));
    const auto original = b;
    for (auto o : {84U, 88U, 93U, 102U, 107U}) {
        b = original;
        set(b, o, 99);
        bad(b, K::DanglingReference, o, o < 100 ? F::ApproachAreaId : F::EncounterAreaId);
        set(b, o, 0);
        if (o < 100) assert(load(b));
        else bad(b, K::InvalidValue, o, F::EncounterAreaId);
    }
    b = original;
    set(b, 113, 999);
    bad(b, K::DanglingReference, 113, F::EncounterSpotId);
    b = original;
    b[106] = 4;
    bad(b, K::UnsupportedValue, 106, F::EncounterDirection);
    b = original;
    b[111] = 255;
    bad(b, K::UnsupportedValue, 111, F::EncounterDirection);
    b = original;
    set(b, 66, 0);
    bad(b, K::DanglingReference, 113, F::EncounterSpotId);
    // Exact logical budget, including each nested object, succeeds at equality.
    auto l = limits();
    l.maxSnapshotBytes = sizeof(model::NavMeshSnapshot) + sizeof(model::NavAreaRecord) +
                         sizeof(model::NavHidingSpot) + sizeof(model::NavApproachRecord) +
                         sizeof(model::NavEncounterRecord) + sizeof(model::NavEncounterSpot);
    assert(load(original, l));
    --l.maxSnapshotBytes;
    assert(load(original, l).error.kind == K::CountLimitExceeded);
    // Every allocation in decoding, validation, object and control-block construction.
    auto retained = load(original);
    assert(retained);
    bool reachedSuccess = false;
    for (std::size_t n = 0; n < 256; ++n) {
        failAfter = n;
        auto r = load(original);
        failAfter = std::numeric_limits<std::size_t>::max();
        assert((*retained.value)->areas()[0].hidingSpots[0].id == 101U);
        if (r) {
            reachedSuccess = true;
            break;
        }
        assert(!r.value && r.error.kind == K::AllocationFailure);
    }
    assert(reachedSuccess);
    // Two valid areas with the same hiding ID: error at the second definition.
    b = original;
    b.insert(b.end(), original.begin() + 12, original.end());
    set(b, 8, 2);
    set(b, 118, 8);
    bad(b, K::DuplicateId, 172, F::HidingSpotId);
    // The first reference is invalid even though a later definition is duplicate.
    set(b, 84, 999);
    bad(b, K::DanglingReference, 84, F::ApproachAreaId);
    // Connections to area 8 in N and E are legal.
    b = fixture(1);
    b.resize(49);
    u32(b, 1);
    u32(b, 8);
    u32(b, 1);
    u32(b, 8);
    u32(b, 0);
    u32(b, 0);
    b.push_back(0);
    b.push_back(0);
    u32(b, 0);
    auto a = fixture(1);
    b.insert(b.end(), a.begin() + 12, a.end());
    set(b, 8, 2);
    set(b, 79, 8);
    assert(load(b));
    auto connected = b;
    set(b, 53, 7);
    bad(b, K::InvalidValue, 53, F::ConnectionAreaId);
    b = connected;
    set(b, 53, 999);
    bad(b, K::DanglingReference, 53, F::ConnectionAreaId);
    b = connected;
    b.insert(b.begin() + 57, {8, 0, 0, 0});
    set(b, 49, 2);
    bad(b, K::InvalidValue, 57, F::ConnectionAreaId);
}

void legacyAndLimits() {
    using K = diagnostics::NavErrorKind;
    for (unsigned v : {1U, 2U}) {
        auto b = fixture(v);
        set(b, 67, 1);
        u32(b, 0);
        u32(b, 999); // Discarded legacy endpoints are not retained references.
        for (unsigned i = 0; i < 6; ++i)
            f32(b, 0);
        b.push_back(0);
        assert(load(b));
    }
    auto b = fixture(5);
    auto l = limits();
    l.header.maxTotalPlaceBytes = 1;
    assert(load(b, l).error.kind == K::CountLimitExceeded);
    l = limits();
    l.maxSnapshotBytes =
        sizeof(model::NavMeshSnapshot) + sizeof(model::NavAreaRecord) + sizeof(std::string) + 2;
    assert(load(b, l));
    --l.maxSnapshotBytes;
    assert(load(b, l).error.kind == K::CountLimitExceeded);
    detail::DecodeContext context;
    context.maximum = std::numeric_limits<std::size_t>::max();
    assert(context.charge(context.maximum, 2, {}).kind == K::OffsetOverflow);
    assert(context.used == 0);
    assert(context.charge(1, 1, {}).isNone());
    assert(context.charge(context.maximum, 1, {}).kind == K::OffsetOverflow);
    // Long opaque Place bytes exercise heap-backed strings and publication failure.
    b = fixture(5);
    b[14] = 26;
    b.insert(b.begin() + 17, 24, 0xFE);
    assert(load(b));
    bool success = false;
    for (std::size_t n = 0; n < 256; ++n) {
        failAfter = n;
        auto r = load(b);
        failAfter = std::numeric_limits<std::size_t>::max();
        if (r) {
            assert((*r.value)->header().places[0].size() == 25);
            success = true;
            break;
        }
        assert(!r.value && r.error.kind == K::AllocationFailure);
    }
    assert(success);
    // Every truncation of a valid minimal file must fail without publication.
    for (unsigned v = 1; v <= 5; ++v) {
        b = fixture(v);
        for (std::size_t end = 0; end < b.size(); ++end) {
            auto r = io::NavMeshLoader::load({b.data(), end}, limits());
            assert(!r && !r.value && r.error.kind == K::EndOfInput);
        }
    }
    using F = diagnostics::NavField;
    b = fixture(1);
    set(b, 17, 0x7FC00000);
    bad(b, K::NonFiniteFloat, 17, F::NorthWestExtent);
    b = fixture(1);
    set(b, 33, 0);
    bad(b, K::InvalidGeometry, 33, F::SouthEastExtent);
    b = fixture(5);
    b[81] = 0;
    assert(load(b));
    b.erase(b.begin() + 14, b.begin() + 18);
    b[12] = 0;
    b[77] = 0;
    assert(load(b));
    b[77] = 1;
    bad(b, K::InvalidValue, 77, F::Place);
}
} // namespace
int main() {
    using K = diagnostics::NavErrorKind;
    using F = diagnostics::NavField;
    for (unsigned v = 1; v <= 5; ++v) {
        auto b = fixture(v);
        auto a = load(b);
        auto c = load(b);
        assert(a && c && *a.value && *c.value);
        assert(a.value->get() != c.value->get());
        assert((*a.value)->areas()[0].id.value == 7);
        assert((*a.value)->areas()[0].attributes == 0xFF);
        assert((*a.value)->areas()[0].extent == (*c.value)->areas()[0].extent);
        assert((*a.value)->header().bspSize.has_value() == (v >= 4));
        b.clear();
        b.shrink_to_fit();
        assert((*a.value)->areas()[0].extent.southEast.z == -10.f);
        if (v == 5)
            assert(static_cast<unsigned char>((*a.value)->header().places[0][0]) == 0xFF);
    }
    static_assert(std::is_same_v<decltype(std::declval<const model::NavMeshSnapshot &>().areas()),
                                 const std::vector<model::NavAreaRecord> &>);
    auto b = fixture(1);
    set(b, 12, 0);
    bad(b, K::InvalidValue, 12, F::AreaId);
    b = fixture(1);
    set(b, 29, 0);
    bad(b, K::InvalidGeometry, 29, F::SouthEastExtent);
    b = fixture(1);
    b.push_back(0);
    bad(b, K::TrailingData, 71, F::RawBytes);
    set(b, 12, 0);
    bad(b, K::InvalidValue, 12, F::AreaId);
    b.pop_back();
    b.pop_back();
    bad(b, K::EndOfInput, 67, F::EncounterCount);
    b = fixture(1);
    auto second = fixture(1);
    b.insert(b.end(), second.begin() + 12, second.end());
    set(b, 8, 2);
    bad(b, K::DuplicateId, 71, F::AreaId);
    b = fixture(5);
    b[81] = 2;
    bad(b, K::InvalidValue, 81, F::Place);
    b = fixture(1);
    auto l = limits();
    l.maxInputBytes = b.size() - 1;
    assert(load(b, l).error.kind == K::CountLimitExceeded);
    l = limits();
    l.maxSnapshotBytes = 0;
    assert(load(b, l).error.kind == K::CountLimitExceeded);
    l = limits();
    l.areas.maxAreas = 0;
    assert(load(b, l).error.kind == K::CountLimitExceeded);
    assert(io::NavMeshLoader::load({nullptr, 1}, limits()).error.kind == K::InvalidInput);
    zeroIdentityCompatibility();
    traversalDecodeAndBudget();
    tacticalAndConnections();
    legacyAndLimits();
}
