// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "core/perception.hpp"

namespace astrabot::adapter::cstrike {
struct VisualEffectSettings {
    double smokeRadius{115};
    std::uint64_t smokeMicros{22000000};
    bool valid() const noexcept;
};
struct VisualEffectDiagnostics {
    std::uint64_t smokeAccepted{}, flashAccepted{}, expired{}, rejected{}, duplicates{}, overflow{};
    std::size_t regions{};
    bool overflowActive{}, clockInvalid{};
};
// Privileged geometry, SDK-free for testing; never exported in a WorldSnapshot.
class VisualEffects final {
public:
    explicit VisualEffects(VisualEffectSettings settings = {}) noexcept : settings_(settings) {}
    void reset() noexcept;
    void invalidate() noexcept { ready_ = false; diagnostics_.clockInvalid = true; ++diagnostics_.rejected; ++revision_; }
    void reject() noexcept { ++diagnostics_.rejected; }
    bool advance(core::MapGeneration,core::perception::RoundGeneration,std::uint64_t) noexcept;
    bool smoke(core::perception::Point) noexcept;
    bool flash(core::PlayerId,std::uint64_t durationMicros) noexcept;
    void forget(core::PlayerId) noexcept;
    core::perception::Reason blocked(core::PlayerId,core::perception::Point,core::perception::Point) const noexcept;
    const VisualEffectDiagnostics& diagnostics() const noexcept { return diagnostics_; }
    std::uint64_t revision() const noexcept { return revision_; }
private:
    struct Smoke { core::perception::Point center{}; std::uint64_t start{}, end{}; };
    struct Flash { core::PlayerId player{}; std::uint64_t start{}, end{}; };
    void clear() noexcept;
    VisualEffectSettings settings_{};
    std::array<Smoke,32> smoke_{};
    std::array<Flash,32> flash_{};
    core::MapGeneration map_{};
    core::perception::RoundGeneration round_{};
    std::uint64_t time_{}, overflowUntil_{}, revision_{};
    bool ready_{};
    VisualEffectDiagnostics diagnostics_{};
};
}
