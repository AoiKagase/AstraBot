// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#pragma once

#include "nav/enrichment/traversal_link.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace astrabot::nav::query {
struct AdaptiveTraversalExperience;
}

namespace astrabot::nav::enrichment {

enum class TraversalActor : std::uint8_t {
    Human = 0,
    Bot,
};

struct TraversalObservation final {
    NavMapFingerprint fingerprint{};
    model::NavAreaId from{};
    model::NavAreaId to{};
    NavLinkPoint entry{};
    NavLinkPoint exit{};
    model::NavTraversalKind traversal{model::NavTraversalKind::Jump};
    NavLinkDirection direction{NavLinkDirection::Forward};
    double additionalCost{0.0};
    std::uint64_t sequence{0};
    bool hasNormalConnection{false};
    bool succeeded{false};
    TraversalActor actor{TraversalActor::Human};

    bool valid() const noexcept;
};

struct TraversalLearningSettings final {
    std::size_t maxCandidates{64};
    double maxDistance{256.0};
    std::uint32_t minHumanAttempts{3};
    double minHumanSuccessRate{0.6};

    bool valid() const noexcept;
};

enum class TraversalLearningReason : std::uint8_t {
    None = 0,
    Accepted,
    InvalidObservation,
    WrongMap,
    NonDiscovery,
    CapacityExceeded,
    StaleSequence,
    LinkConflict,
};

struct TraversalLearningUpdate final {
    TraversalLearningReason reason{TraversalLearningReason::None};
    std::uint64_t linkId{0};
    bool changed{false};

    constexpr bool accepted() const noexcept {
        return reason == TraversalLearningReason::Accepted;
    }
};

// Bounded, round-local learning. It proposes external links only; it never
// edits NavMesh records or emits a link before human evidence reaches the gate.
class TraversalLearningModel final {
public:
    explicit TraversalLearningModel(TraversalLearningSettings settings = {}) noexcept
        : settings_(settings) {}

    bool activate(const NavMapFingerprint& fingerprint,
                  std::uint64_t sourceId,
                  std::uint64_t generation) noexcept;
    void reset() noexcept;
    TraversalLearningUpdate observe(const TraversalObservation& observation) noexcept;

    bool active() const noexcept { return active_; }
    std::size_t candidateCount() const noexcept { return candidates_.size(); }
    const TraversalLearningSettings& settings() const noexcept { return settings_; }
    NavTraversalLinkSet activeEnrichment() const;
    bool experience(std::uint64_t linkId,
                    query::AdaptiveTraversalExperience& result) const noexcept;

private:
    struct Candidate final {
        NavTraversalLink link{};
        double humanAttempts{0.0};
        double humanSuccess{0.0};
        double botAttempts{0.0};
        double botSuccess{0.0};
    };

    Candidate* find(std::uint64_t linkId) noexcept;
    const Candidate* find(std::uint64_t linkId) const noexcept;
    bool eligible(const Candidate& candidate) const noexcept;

    TraversalLearningSettings settings_{};
    NavMapFingerprint fingerprint_{};
    std::uint64_t sourceId_{0};
    std::uint64_t generation_{0};
    std::uint64_t lastSequence_{0};
    std::vector<Candidate> candidates_{};
    bool active_{false};
};

} // namespace astrabot::nav::enrichment
