// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "core/sound_memory.hpp"
#include "core/position_distribution.hpp"
#include <optional>

namespace astrabot::core::world {
enum class WorldReason : std::uint8_t { None, InvalidFrame, NotCollecting, QueueFull, Duplicate, Conflict, VisualRejected, SoundRejected, Count };
struct WorldDiagnostics {
    WorldReason reason{};
    std::array<std::uint64_t,static_cast<std::size_t>(WorldReason::Count)> rejected{};
    std::uint64_t publications{}, accepted{}, maxDelayMicros{};
    std::size_t staged{}, frameProcessed{};
};
struct SourceQueueDiagnostics { std::size_t soundPending{}; std::uint64_t soundOverflow{}; };
struct WorldSnapshot {
    perception::Stamp stamp{};
    const MemorySnapshot* visual{};
    const SoundSnapshot* sounds{};
    std::uint64_t oldestVisualAgeMicros{}, oldestSoundAgeMicros{}, maxReceiptDelayMicros{};
    SourceQueueDiagnostics queues{};
    // Parallel to visual->memories; null means no matching completed distribution.
    std::array<const PositionDistribution*,perception::kCandidateCapacity> distributions{};
};
// Single owner of canonical memories. Snapshots borrow these memories until the
// next mutation; they contain no privileged engine geometry or sound emitter ID.
class WorldModel final {
public:
    static constexpr std::size_t visionCapacity = 32, soundCapacity = 32*32;
    explicit WorldModel(MemorySettings visual = {},SoundSettings sound = {}) noexcept : visual_(visual),sounds_(sound) {}
    WorldModel(const WorldModel&) = delete;
    WorldModel& operator=(const WorldModel&) = delete;
    void reset() noexcept;
    void beginUpdate() noexcept; // Retire the prior publication before callbacks.
    bool advance(const MemoryFrame&) noexcept; // Invalidation and decay before staging.
    bool stage(const perception::ObservationBatch&) noexcept;
    bool stage(PlayerId,const perception::SoundObservation&) noexcept;
    bool publish(SourceQueueDiagnostics = {}) noexcept;
    bool collectingAt(MapGeneration map,perception::RoundGeneration round,TickId tick,std::uint64_t time) const noexcept {
        return collecting_ && frame_.map == map && frame_.round == round && frame_.tick == tick && frame_.timeMicros == time;
    }
    void forget(PlayerId) noexcept;
    void beginRound(perception::RoundGeneration) noexcept;
    std::optional<WorldSnapshot> latest(PlayerId) const noexcept;
    const MemoryFrame* publishedFrame() const noexcept { return published_ ? &frame_ : nullptr; }
    bool setDistribution(PlayerId,PlayerId,const perception::ObservationIdentity&,const PositionDistribution&) noexcept;
    void clearDistributions() noexcept;
    const WorldDiagnostics& diagnostics() const noexcept { return diagnostics_; }
    const VisualMemoryModel& visual() const noexcept { return visual_; }
    const SoundMemoryModel& sounds() const noexcept { return sounds_; }
    // Wiring for privileged adapters; LifecycleCoordinator exposes only const WorldModel.
    VisualMemoryModel& visualReducer() noexcept { return visual_; }
    SoundMemoryModel& soundReducer() noexcept { return sounds_; }
private:
    struct SoundInput { PlayerId receiver{}; perception::SoundObservation sound{}; };
    struct InputRef { std::uint16_t index{}; bool visual{}; };
    bool reject(WorldReason) noexcept;
    const perception::ObservationIdentity& identity(InputRef) const noexcept;
    PlayerId receiver(InputRef) const noexcept;
    bool equal(InputRef,InputRef) const noexcept;
    VisualMemoryModel visual_{};
    SoundMemoryModel sounds_{};
    struct DistributionEntry { PlayerId observer{}, target{}; perception::ObservationIdentity identity{}; PositionDistribution value{}; };
    std::array<std::array<DistributionEntry,32>,32> distributions_{};
    MemoryFrame frame_{};
    std::array<perception::ObservationBatch,visionCapacity> visionInputs_{};
    std::array<SoundInput,soundCapacity> soundInputs_{};
    std::array<InputRef,visionCapacity+soundCapacity> order_{};
    std::size_t visionCount_{}, soundCount_{};
    bool collecting_{}, published_{};
    SourceQueueDiagnostics queues_{};
    WorldDiagnostics diagnostics_{};
};
}
