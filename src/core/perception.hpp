// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "core/identity.hpp"
#include "core/perception_identity.hpp"
#include <array>
#include <cstddef>
#include <cstdint>

namespace astrabot::core::perception {
constexpr std::size_t kPlayerCapacity = 32;
constexpr std::size_t kCandidateCapacity = kPlayerCapacity - 1;
constexpr std::size_t kBotsPerFrame = 4;
struct Point { double x{}, y{}, z{}; };
bool finite(Point) noexcept;
struct VisionSettings {
    std::uint64_t intervalMicros{100000};
    double maxDistance{4096};
    double fullFovDegrees{90};
    bool valid() const noexcept;
};
enum class Reason : std::uint8_t {
    None, InvalidSettings, InvalidFrame, StaleIdentity, InvalidGeometry,
    OutOfRange, OutsideFov, Occluded, MissingEngine, InvalidTrace, Count
};
struct Stamp {
    BotAgentId agent{};
    PlayerId observer{};
    MapGeneration map{};
    TickId tick{};
    std::uint64_t timeMicros{};
    RoundGeneration round{1};
};
struct VisibleObservation {
    PlayerId target{};
    Point position{}; // Only the point reached by a successful trace.
};
struct ObservationBatch {
    Stamp stamp{};
    std::array<VisibleObservation, kCandidateCapacity> observations{};
    std::size_t count{};
    ObservationIdentity identity{}; // One immutable source event per published scan.
};
// Diagnostics contain no hidden positions or rejected target identities.
struct Diagnostics {
    Reason reason{Reason::None};
    std::array<std::uint32_t, static_cast<std::size_t>(Reason::Count)> reasons{};
    std::uint32_t candidates{}, traces{};
    std::uint64_t latenessMicros{}, intervalMicros{}, updates{}, deferredFrames{};
};
// Privileged synchronous input, never forwarded to a World Model/planner.
// Only ObservationBatch is a knowledge output.
struct PlayerSample {
    PlayerId player{};
    BotAgentId agent{}; // Valid only for an eligible managed observer.
    bool alive{};      // Includes connected and non-spectator eligibility.
    Point eye{}, center{}, forward{};
};
struct InputFrame {
    MapGeneration map{};
    TickId tick{};
    std::uint64_t timeMicros{};
    std::array<PlayerSample, kPlayerCapacity> players{}; // slot - 1
    RoundGeneration round{1};
};
struct SightRequest {
    Stamp stamp{};
    PlayerId target{};
    Point start{}, end{};
};
class IVisibilityQueries {
public:
    virtual ~IVisibilityQueries() = default;
    // None means proven clear; all failure paths are fail-closed.
    virtual Reason trace(const SightRequest&) noexcept = 0;
};
class Vision final {
public:
    explicit Vision(VisionSettings settings = {}) noexcept : settings_(settings) {}
    void reset() noexcept;
    void beginRound(RoundGeneration) noexcept;
    void forget(PlayerId) noexcept;
    void update(const InputFrame&, IVisibilityQueries&) noexcept;
    const ObservationBatch* latest(PlayerId) const noexcept;
    const Diagnostics* diagnostics(PlayerId) const noexcept;
    Reason frameReason() const noexcept { return frameReason_; }
    std::size_t frameUpdates() const noexcept { return frameUpdates_; }
private:
    struct State {
        PlayerId observer{};
        BotAgentId agent{};
        std::uint64_t nextMicros{};
        bool published{};
        ObservationBatch batch{};
        Diagnostics diagnostics{};
    };
    VisionSettings settings_{};
    std::array<State, kPlayerCapacity> states_{};
    std::array<Generation, kPlayerCapacity> generations_{};
    MapGeneration map_{};
    TickId tick_{};
    std::uint64_t timeMicros_{}, revision_{};
    std::size_t cursor_{}, frameUpdates_{};
    Reason frameReason_{Reason::None};
    RoundGeneration round_{1};
    std::uint64_t sequence_{};
};
} // namespace astrabot::core::perception
