// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "nav/local/primitive.hpp"
#include "nav/runtime/world_query.hpp"

namespace astrabot::nav::local {
enum class BlockerAction { Neutral, Yield, InspectAvoidance, ReinspectPassage, Replan, Aborted };
enum class BlockerReason { None, InvalidInput, InvalidBinding, StaleTick,
    InvalidObservation, Unavailable, TimedOut, Cancelled };
struct BlockerLimits {
    std::uint64_t factLifetimeUs{}, yieldUs{}, timeoutUs{};
};
struct BlockerFeedback {
    Binding binding{};
    runtime::QueryStamp requested{};
    runtime::WorldQueryResult observed{};
    std::uint64_t nowUs{}; // Accumulated monotonic simulation time, never frame delta.
};
struct BlockerDecision {
    BlockerAction action{BlockerAction::Neutral};
    BlockerReason reason{BlockerReason::None};
    bool accepted{}, terminalEvent{};
};
// One route/portal-bound attempt with one expiring fact, no mesh mutation or
// allocation. Advice never authorizes translation: the caller must inspect the
// actual side segment or recheck the passage. Clear requires a fresh clearance
// result, not the absence of a blocker. Reobservations, replacements and fact
// expiry cannot reset the attempt deadline. The owner aborts on invalidation.
class BlockerWait final {
public:
    BlockerWait(Binding binding, BlockerLimits limits) noexcept
        : binding_(binding), limits_(limits) {}
    BlockerDecision update(const BlockerFeedback&) noexcept;
    // Trusted controller has just completed full ground/segment inspection.
    // This does not issue or fabricate a world-query stamp and never moves.
    BlockerDecision clear(Binding, core::TickId, std::uint64_t nowUs) noexcept;
    BlockerDecision abort() noexcept;
    std::optional<runtime::BlockerObservation> fact(std::uint64_t nowUs) const noexcept;
private:
    Binding binding_{};
    BlockerLimits limits_{};
    core::TickId tick_{};
    std::uint64_t startedUs_{}, lastUs_{}, observedUs_{};
    bool started_{}, terminal_{};
    std::optional<runtime::BlockerObservation> fact_{};
    BlockerDecision finish(BlockerAction, BlockerReason) noexcept;
    std::optional<BlockerDecision> begin(Binding, core::TickId, std::uint64_t) noexcept;
};
}
