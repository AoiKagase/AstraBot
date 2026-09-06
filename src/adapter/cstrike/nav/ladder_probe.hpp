// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "adapter/cstrike/nav/ladder_scanner.hpp"
#include "nav/query/spatial_index.hpp"

namespace astrabot::adapter::cstrike {
enum class LadderFace { MinX, MaxX, MinY, MaxY };
enum class LadderExit { SameFace, AcrossTop };
enum class LadderProbeReason { None, InvalidInput, Unavailable, StaleMap,
    StaleEntity, BudgetExceeded, InvalidTrace, NoFace, NoSupport, NoArea, Blocked };
struct LadderWorld {
    enginefuncs_t* engine{};
    const void* context{};
    core::MapGeneration (*currentMap)(const void*) noexcept = nullptr;
};
struct LadderEndpoint {
    nav::model::NavVector3 origin{};
    nav::model::NavAreaId area{};
};
struct LadderPassage {
    core::MapGeneration map{};
    std::uint64_t entityId{};
    LadderCandidate candidate{};
    int modelIndex{},modelName{};
    LadderFace face{};
    LadderExit exit{};
    nav::model::NavVector3 normal{}, lowContact{}, highContact{};
    LadderEndpoint bottom{}, top{};
    nav::model::NavVector3 mount{}, dismount{};
};
struct LadderProbeResult {
    LadderProbeReason reason{LadderProbeReason::InvalidInput};
    std::optional<LadderPassage> passage{};
    std::uint32_t queries{};
    explicit operator bool() const noexcept { return reason==LadderProbeReason::None && passage.has_value(); }
};
// One explicit face/exit variant; no retries or automatic side selection.
// Standard standing GoldSrc hull only. Twelve traces maximum, no dynamic allocation.
// The evidence is synchronous and must be revalidated before later motion.
LadderProbeResult inspectLadderPassage(LadderWorld,core::MapGeneration,
    const LadderCandidate&,LadderFace,LadderExit,const nav::query::NavSpatialIndex&,
    core::MapGeneration indexMap,int maxEntities,std::uint32_t maxQueries=12) noexcept;
// Identity/model/bounds freshness only; does not renew support/clearance proof.
bool ladderPassageCurrent(LadderWorld,const LadderPassage&,int maxEntities) noexcept;
}
