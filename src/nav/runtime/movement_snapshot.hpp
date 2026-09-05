// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "core/identity.hpp"
#include "nav/model/value_types.hpp"
#include <optional>

namespace astrabot::nav::runtime {
enum class ActorKind { Unknown, ManagedBot, Human };
struct HullDimensions { model::NavVector3 minimum{}, maximum{}; };
struct LadderContact {
    std::uint64_t sourceId{}, generation{}, linkId{};
    bool touching{};
};
// Supplied by a trusted host adapter; absent values mean unknown, never false/zero proof.
// elapsedUs is measured simulation delta, not a wall-clock read by Nav.
struct MovementSnapshot {
    core::BotAgentId agent{};
    core::PlayerId actor{};
    core::MapGeneration map{};
    core::TickId tick{};
    std::uint64_t elapsedUs{};
    ActorKind kind{ActorKind::Unknown};
    std::optional<model::NavVector3> position{}, velocity{}, view{};
    std::optional<bool> connected{}, alive{}, joined{}, grounded{}, ducked{};
    std::optional<HullDimensions> hull{};
    std::optional<float> speedLimit{};
    std::optional<LadderContact> ladder{};
};
} // namespace astrabot::nav::runtime
