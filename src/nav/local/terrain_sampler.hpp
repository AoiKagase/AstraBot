// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "nav/local/ground_probe.hpp"

namespace astrabot::nav::local
{
// Physical floor candidates, footprint support and passage are independent
// evidence. NAV containment identifies each supported sample; route adjacency
// belongs to PathFollower. A failed query never means a physical obstruction.
class TerrainSampler final
{
public:
	static ProbeResult locate(const runtime::MovementSnapshot&, std::uint64_t, const query::NavSpatialIndex&,
							  core::MapGeneration, runtime::IWorldQueries&, GroundProbeLimits) noexcept;
	static ProbeResult inspect(const runtime::MovementSnapshot&, std::uint64_t, model::NavAreaId, float x, float y,
							   const query::NavSpatialIndex&, core::MapGeneration, runtime::IWorldQueries&,
							   GroundProbeLimits) noexcept;
};
} // namespace astrabot::nav::local
