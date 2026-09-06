// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "core/world_model.hpp"
#include "nav/query/graph.hpp"
#include "nav/query/spatial_index.hpp"
namespace astrabot::nav::query {
// Built at NAV publication, never from a movement session or an engine callback.
class DistributionTopology final {
public:
    static std::shared_ptr<const DistributionTopology> build(core::MapGeneration,
        std::shared_ptr<const NavGraph>,std::shared_ptr<const NavSpatialIndex>) noexcept;
private:
    friend class DistributionModel;
    DistributionTopology() = default;
    core::MapGeneration map{};
    std::shared_ptr<const NavGraph> graph{};
    std::shared_ptr<const NavSpatialIndex> spatial{};
    std::vector<std::size_t> offsets{};
    std::vector<std::uint32_t> targets{}; // Unique, increasing target IDs within each area.
};
struct DistributionDiagnostics {
    std::uint64_t completed{}, resets{}, unavailable{}, expiredJobs{};
    std::size_t frameConnections{}, frameMappings{}, frameVisits{}, pending{};
    std::uint64_t maxDelayMicros{};
};
class DistributionModel final {
public:
    static constexpr std::uint64_t stepMicros = 200000;
    static constexpr std::size_t connectionsPerFrame = 256, mappingsPerFrame = 32;
    void reset() noexcept;
    void update(core::world::WorldModel&,std::shared_ptr<const DistributionTopology>) noexcept;
    const DistributionDiagnostics& diagnostics() const noexcept { return diagnostics_; }
private:
    struct Job {
        core::PlayerId observer{}, target{};
        core::perception::ObservationIdentity identity{};
        core::world::PositionDistribution input{}, output{};
        std::array<std::size_t,32> edge{}, end{};
        std::array<double,32> share{};
        std::size_t stay{};
        std::uint32_t pendingArea{};
        double pendingMass{};
        bool active{}, present{}, mapped{};
    };
    void start(Job&,const core::world::PositionDistribution&) noexcept;
    bool progress(Job&,std::size_t) noexcept;
    static void retain(core::world::PositionDistribution&,std::uint32_t,double) noexcept;
    std::array<Job,32*32> jobs_{};
    std::shared_ptr<const DistributionTopology> topology_{};
    core::MapGeneration map_{};
    core::perception::RoundGeneration round_{};
    std::uint64_t revision_{}, time_{};
    std::size_t cursor_{};
    DistributionDiagnostics diagnostics_{};
};
}
