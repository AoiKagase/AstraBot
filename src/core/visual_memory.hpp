// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "core/perception.hpp"

namespace astrabot::core::world {
struct MemorySettings {
    std::uint64_t retentionMicros{5000000};
    bool valid() const noexcept { return retentionMicros != 0; }
};
// Identity/eligibility only. Never pass privileged PlayerSample geometry here.
struct MemoryPlayer { PlayerId player{}; BotAgentId agent{}; bool eligible{}; };
struct MemoryFrame {
    MapGeneration map{};
    TickId tick{};
    std::uint64_t timeMicros{};
    std::array<MemoryPlayer, perception::kPlayerCapacity> players{};
    perception::RoundGeneration round{1};
};
struct VisualMemory {
    PlayerId target{};
    perception::Point lastKnownPosition{};
    std::uint64_t lastSeenMicros{};
    double confidence{};
    perception::ObservationIdentity identity{};
};
struct MemorySnapshot {
    perception::Stamp stamp{}; // Owner/map and most recent decay frame.
    std::array<VisualMemory, perception::kCandidateCapacity> memories{};
    std::size_t count{};
};
enum class MemoryReason : std::uint8_t {
    None, InvalidSettings, InvalidFrame, StaleIdentity, InvalidBatch, DuplicateBatch,
    StaleBatch, MissingEngine, RoundChanged
};
struct MemoryDiagnostics {
    MemoryReason reason{};
    std::uint64_t frames{}, updates{}, expired{}, retired{}, rejected{};
    std::size_t frameVisits{}; // Entries visited by the most recent decay pass (<= 32*31).
    std::size_t frameObservations{}; // Accepted observations since that advance.
};
class VisualMemoryModel final {
public:
    explicit VisualMemoryModel(MemorySettings settings = {}) noexcept : settings_(settings) {}
    void reset() noexcept;
    void beginRound(perception::RoundGeneration) noexcept;
    // Clears knowledge while retaining temporal/generation high-water marks.
    void invalidate(MemoryReason) noexcept;
    void forget(PlayerId) noexcept;
    bool advance(const MemoryFrame&) noexcept;
    // Only a validated publication from the current frame may refresh memory.
    bool observe(const perception::ObservationBatch&) noexcept;
    // Borrowed read-only view, valid until the next model mutation.
    const MemorySnapshot* latest(PlayerId) const noexcept;
    const MemoryDiagnostics& diagnostics() const noexcept { return diagnostics_; }
private:
    struct State { MemorySnapshot snapshot{}; TickId consumed{}; std::uint64_t sequence{}; };
    void reject(MemoryReason) noexcept;
    MemorySettings settings_{};
    MemoryFrame frame_{};
    std::array<State, perception::kPlayerCapacity> states_{};
    std::array<Generation, perception::kPlayerCapacity> generations_{};
    MemoryDiagnostics diagnostics_{};
    bool ready_{};
};
} // namespace astrabot::core::world
