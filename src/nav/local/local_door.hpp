// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "nav/local/terrain_sampler.hpp"
#include "nav/local/walk.hpp"

namespace astrabot::nav::local {
// Bounded door handling for the locomotion controller. The caller invokes it
// only after a grounded movement probe reports a blocked segment.
class LocalDoor final {
public:
    static constexpr std::uint64_t timeoutUs=3'000'000;

    std::optional<WalkDecision> update(const runtime::MovementSnapshot&,
        Binding, model::NavVector3 desiredTarget,
        const query::NavSpatialIndex&, core::MapGeneration,
        GroundProbeLimits, runtime::IWorldQueries&, std::uint64_t nowUs,
        std::uint32_t& used, std::uint32_t maximum) noexcept;
    void reset() noexcept;

private:
    std::optional<DoorWait> wait_{};
    Binding binding_{};
    std::uint64_t doorId_{};
    bool passive_{},contactSent_{};
};
}
