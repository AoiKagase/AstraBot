// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "core/visual_memory.hpp"

namespace astrabot::core::perception {
enum class SoundKind : std::uint8_t { Unknown, Footstep, Gunshot, Explosion };
// A 256-unit grid cell, not an emitter position or a player identity. Its box is
// [cell*256, (cell+1)*256) on each axis; compute endpoints in double precision.
struct SoundRegion {
    std::int32_t x{}, y{}, z{};
    static constexpr double width = 256.0;
};
struct SoundObservation {
    ObservationIdentity identity{};
    SoundKind kind{};
    SoundRegion region{};
};
} // namespace astrabot::core::perception

namespace astrabot::core::world {
constexpr std::size_t kSoundsPerObserver = 16;
struct SoundMemory {
    perception::SoundObservation observation{};
    double confidence{};
};
struct SoundSnapshot {
    perception::Stamp stamp{};
    std::array<SoundMemory,kSoundsPerObserver> sounds{};
    std::size_t count{};
};
struct SoundSettings {
    std::uint64_t retentionMicros{3000000};
    bool valid() const noexcept { return retentionMicros != 0; }
};
enum class SoundReason : std::uint8_t {
    None, InvalidSettings, InvalidFrame, InvalidObservation, StaleIdentity,
    StaleObservation, DuplicateObservation, ExpiredObservation, RoundChanged
};
struct SoundDiagnostics {
    SoundReason reason{};
    std::uint64_t updates{}, expired{}, retired{}, rejected{}, evicted{};
    std::size_t frameVisits{}, frameObservations{};
};
// Owns anonymous sounds only; never joins them to visual/person memories.
class SoundMemoryModel final {
public:
    explicit SoundMemoryModel(SoundSettings settings = {}) noexcept : settings_(settings) {}
    void reset() noexcept;
    void invalidate(SoundReason) noexcept;
    void beginRound(perception::RoundGeneration) noexcept;
    void forget(PlayerId) noexcept; // Receiver retirement; there is no emitter ID.
    bool advance(const MemoryFrame&) noexcept;
    bool observe(PlayerId observer, const perception::SoundObservation&) noexcept;
    const SoundSnapshot* latest(PlayerId) const noexcept;
    const SoundDiagnostics& diagnostics() const noexcept { return diagnostics_; }
private:
    struct State {
        SoundSnapshot snapshot{};
        std::uint64_t sequence{}, observedMicros{};
    };
    bool reject(SoundReason) noexcept;
    void clear() noexcept;
    SoundSettings settings_{};
    MemoryFrame frame_{};
    std::array<State,perception::kPlayerCapacity> states_{};
    std::array<Generation,perception::kPlayerCapacity> generations_{};
    SoundDiagnostics diagnostics_{};
    bool ready_{};
};
} // namespace astrabot::core::world
