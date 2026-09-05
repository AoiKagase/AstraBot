// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include "nav/diagnostics/error.hpp"
#include "nav/model/value_types.hpp"
#include "nav/model/connection.hpp"

#include <cassert>
#include <limits>
#include <memory>

namespace {

using astrabot::nav::diagnostics::NavError;
using astrabot::nav::diagnostics::NavErrorKind;
using astrabot::nav::diagnostics::NavField;
using astrabot::nav::diagnostics::NavRecord;
using astrabot::nav::diagnostics::ReadResult;
using astrabot::nav::model::NavAreaId;
using astrabot::nav::model::NavExtent;
using astrabot::nav::model::NavVector3;

void testNavAreaIdValues() {
    const NavAreaId invalid = NavAreaId::invalid();
    const NavAreaId first{7};
    const NavAreaId later{9};

    assert(!invalid.isValid());
    assert(first.isValid());
    assert(first != later);
    assert(first < later);
    assert(invalid < first);
}

void testFiniteGeometry() {
    const NavVector3 finite{1.0F, -2.0F, 3.5F};
    const NavVector3 nanValue{
        std::numeric_limits<float>::quiet_NaN(), 0.0F, 0.0F};
    const NavVector3 infiniteValue{
        0.0F, std::numeric_limits<float>::infinity(), 0.0F};

    assert(finite.isFinite());
    assert(!nanValue.isFinite());
    assert(!infiniteValue.isFinite());

    const NavExtent extent{
        {-10.0F, 20.0F, 1.0F},
        {30.0F, -40.0F, 2.0F},
        3.0F,
        4.0F,
    };
    assert(extent.isFinite());

    const NavExtent invalidExtent{
        extent.northWest,
        extent.southEast,
        std::numeric_limits<float>::quiet_NaN(),
        extent.southWestZ,
    };
    assert(!invalidExtent.isFinite());
}

void testReadResultInvariants() {
    const ReadResult<std::uint32_t> success = ReadResult<std::uint32_t>::success(42U);
    assert(success.succeeded());
    assert(success.value.has_value());
    assert(*success.value == 42U);

    const NavError error{
        NavErrorKind::EndOfInput,
        12U,
        NavRecord::Area,
        NavField::AreaId,
    };
    const ReadResult<std::uint32_t> failure = ReadResult<std::uint32_t>::failure(error);
    assert(!failure.succeeded());
    assert(!failure.value.has_value());
    assert(failure.error == error);
}

} // namespace

int main() {
    auto owned = astrabot::nav::diagnostics::ReadResult<std::unique_ptr<int>>::success(
        std::make_unique<int>(42));
    assert(owned && **owned.value == 42);
    testNavAreaIdValues();
    using namespace astrabot::nav::model;
    constexpr NavConnection defaultEdge{NavAreaId{42U}};
    static_assert(defaultEdge.traversal == NavTraversalKind::Walk);
    assert(defaultEdge.target == NavAreaId{42U});
    for (const auto kind : {NavTraversalKind::Walk, NavTraversalKind::Crouch,
                            NavTraversalKind::Jump, NavTraversalKind::Ladder,
                            NavTraversalKind::Drop}) {
        const NavConnection edge{NavAreaId{43U}, kind};
        assert(isKnownTraversalKind(edge.traversal));
        assert(edge.traversal == kind && edge.target == NavAreaId{43U});
    }
    for (unsigned value = 5U; value <= 255U; ++value) {
        assert(!isKnownTraversalKind(static_cast<NavTraversalKind>(value)));
    }
    testFiniteGeometry();
    testReadResultInvariants();
    return 0;
}
