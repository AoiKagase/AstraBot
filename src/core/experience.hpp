// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#pragma once

#include "core/identity.hpp"
#include "core/perception_identity.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace astrabot::core::experience {

constexpr std::size_t kMapNameLimit = 64;
constexpr std::size_t kMaxExperienceAreas = 4096;
constexpr std::size_t kContentHashBytes = 32;
constexpr std::uint32_t kCurrentFormatVersion = 1;
constexpr std::uint32_t kCurrentSchemaVersion = 2;

using ContentHash = std::array<std::uint8_t, kContentHashBytes>;

struct MapIdentity final {
    std::string name{};
    std::uint64_t bspBytes{0};
    bool hasBspHash{false};
    ContentHash bspHash{};
    std::uint32_t navFormatVersion{0};
    bool hasNavHash{false};
    ContentHash navHash{};

    bool valid() const noexcept;
    bool sameMap(const MapIdentity& other) const noexcept;

    friend bool operator==(const MapIdentity& left,
                           const MapIdentity& right) noexcept {
        return left.sameMap(right);
    }
    friend bool operator!=(const MapIdentity& left,
                           const MapIdentity& right) noexcept {
        return !(left == right);
    }
};

enum class ActorKind : std::uint8_t {
    Unknown = 0,
    Human,
    Bot,
    AstraBot = Bot,
    ManagedBot = Bot,
    OtherBot,
};

enum class ExperienceEventKind : std::uint8_t {
    AreaEntered = 0,
    DamageDealt,
    DamageReceived,
    Kill,
    Death,
    Encounter,
    GrenadeExplosion,
    Plant,
    Defuse,
    AttackSuccess,
    AttackFailure,
    RetakeSuccess,
    RoundResult,
    HumanTraversal,
    BotTraversal,
};

const char* eventName(ExperienceEventKind kind) noexcept;

struct ExperienceEvent final {
    MapIdentity map{};
    perception::RoundGeneration round{};
    TickId tick{};
    std::uint64_t timeMicros{0};
    std::uint64_t sequence{0};
    ExperienceEventKind kind{ExperienceEventKind::AreaEntered};
    std::uint32_t area{0};
    PlayerId actor{};
    ActorKind actorKind{ActorKind::Unknown};
    perception::Team team{perception::Team::Unknown};
    double amount{1.0};
    bool success{false};

    bool requiresArea() const noexcept;
    bool valid() const noexcept;
};

struct ExperienceSettings final {
    double humanWeight{1.0};
    double botWeight{0.25};
    double otherBotWeight{0.25};
    double unknownWeight{0.0};
    std::uint32_t decayNumerator{9};
    std::uint32_t decayDenominator{10};
    std::uint32_t maxDecayRounds{256};

    bool valid() const noexcept;
    double weight(ActorKind kind) const noexcept;
};

struct AreaExperience final {
    std::uint32_t area{0};
    double visits{0.0};
    double dangerT{0.0};
    double dangerCT{0.0};
    double encounterRate{0.0};
    double deathRate{0.0};
    double grenadeThreat{0.0};
    double sniperThreat{0.0};
    double pushSuccess{0.0};
    double retakeSuccess{0.0};
    double humanTraffic{0.0};
    double botTraffic{0.0};
    double humanVisits{0.0};
    double botVisits{0.0};
    double damageDealt{0.0};
    double damageReceived{0.0};
    double kills{0.0};
    double deaths{0.0};
    double attackSuccess{0.0};
    double attackFailure{0.0};

    bool valid() const noexcept;
    void decay(double factor) noexcept;
};

struct ExperienceTotals final {
    double damageDealt{0.0};
    double damageReceived{0.0};
    double kills{0.0};
    double deaths{0.0};
    double roundWins{0.0};
    double roundLosses{0.0};

    bool valid() const noexcept;
    void decay(double factor) noexcept;
};

struct ExperienceSnapshot final {
    MapIdentity map{};
    perception::RoundGeneration round{};
    std::uint64_t timeMicros{0};
    ExperienceTotals totals{};
    std::vector<AreaExperience> areas{};

    bool valid() const noexcept;
};

enum class ExperienceUpdateReason : std::uint8_t {
    None = 0,
    Accepted,
    Inactive,
    InvalidSettings,
    InvalidEvent,
    MapMismatch,
    StaleRound,
    StaleTime,
    InvalidArea,
    CapacityExceeded,
    AllocationFailure,
};

struct ExperienceUpdateResult final {
    ExperienceUpdateReason reason{ExperienceUpdateReason::None};
    std::uint32_t area{0};
    bool changed{false};

    constexpr bool accepted() const noexcept {
        return reason == ExperienceUpdateReason::Accepted;
    }
};

class ExperienceModel final {
public:
    explicit ExperienceModel(ExperienceSettings settings = {}) noexcept
        : settings_(settings) {}

    bool activate(const MapIdentity& map,
                  perception::RoundGeneration round) noexcept;
    void reset() noexcept;
    ExperienceUpdateResult beginRound(perception::RoundGeneration round,
                                      std::uint64_t timeMicros) noexcept;
    ExperienceUpdateResult apply(const ExperienceEvent& event);
    bool load(const ExperienceSnapshot& snapshot) noexcept;

    bool active() const noexcept { return active_; }
    const MapIdentity& map() const noexcept { return map_; }
    perception::RoundGeneration round() const noexcept { return round_; }
    std::uint64_t timeMicros() const noexcept { return timeMicros_; }
    std::size_t areaCount() const noexcept { return areas_.size(); }
    const AreaExperience* area(std::uint32_t area) const noexcept;
    const ExperienceTotals& totals() const noexcept { return totals_; }
    ExperienceSnapshot snapshot() const;
    const ExperienceSettings& settings() const noexcept { return settings_; }

private:
    ExperienceUpdateResult applyDecayTo(perception::RoundGeneration round,
                                        std::uint64_t timeMicros) noexcept;
    AreaExperience* findArea(std::uint32_t area) noexcept;
    const AreaExperience* findArea(std::uint32_t area) const noexcept;
    AreaExperience* ensureArea(std::uint32_t area) noexcept;
    ExperienceUpdateResult updateArea(const ExperienceEvent& event,
                                      AreaExperience& value,
                                      double weight) noexcept;

    ExperienceSettings settings_{};
    MapIdentity map_{};
    perception::RoundGeneration round_{};
    std::uint64_t timeMicros_{0};
    ExperienceTotals totals_{};
    std::vector<AreaExperience> areas_{};
    bool active_{false};
};

enum class PersistenceStatus : std::uint8_t {
    None = 0,
    Ok,
    NotFound,
    Migrated,
    RecoveredBackup,
    InvalidPath,
    IoFailure,
    Corrupt,
    UnsupportedVersion,
    SchemaMismatch,
    MapMismatch,
    QuarantineFailure,
};

struct PersistenceResult final {
    PersistenceStatus status{PersistenceStatus::None};
    bool recovered{false};

    constexpr bool succeeded() const noexcept {
        return status == PersistenceStatus::Ok || status == PersistenceStatus::Migrated ||
               status == PersistenceStatus::RecoveredBackup;
    }
};

class IExperiencePersistence {
public:
    virtual ~IExperiencePersistence() = default;
    virtual PersistenceResult load(const MapIdentity& map,
                                   ExperienceSnapshot& snapshot) = 0;
    virtual PersistenceResult save(const ExperienceSnapshot& snapshot) = 0;
};

// A bounded, little-endian, versioned binary store. The store is deliberately
// behind IExperiencePersistence so an engine adapter never owns a file format.
class BinaryExperienceStore final : public IExperiencePersistence {
public:
    explicit BinaryExperienceStore(std::string path) noexcept
        : path_(std::move(path)) {}

    PersistenceResult load(const MapIdentity& map,
                           ExperienceSnapshot& snapshot) override;
    PersistenceResult save(const ExperienceSnapshot& snapshot) override;

    const std::string& path() const noexcept { return path_; }
    static const char* statusName(PersistenceStatus status) noexcept;

private:
    PersistenceResult readFile(const std::string& path,
                               const MapIdentity& expected,
                               ExperienceSnapshot& snapshot) const;
    PersistenceResult quarantine(const MapIdentity& expected);
    std::string path_{};
};

class ExperiencePipeline final {
public:
    ExperiencePipeline(ExperienceSettings settings,
                        IExperiencePersistence* persistence) noexcept
        : model_(settings), persistence_(persistence) {}

    bool activate(const MapIdentity& map,
                  perception::RoundGeneration round) noexcept {
        return model_.activate(map, round);
    }
    PersistenceResult restore();
    ExperienceUpdateResult submit(const ExperienceEvent& event) {
        return model_.apply(event);
    }
    PersistenceResult flush();

    ExperienceModel& model() noexcept { return model_; }
    const ExperienceModel& model() const noexcept { return model_; }

private:
    ExperienceModel model_;
    IExperiencePersistence* persistence_{nullptr};
};

} // namespace astrabot::core::experience
