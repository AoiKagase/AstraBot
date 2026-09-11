// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "adapter/metamod/plugin_entry.hpp"
#include "nav/local/walk.hpp"

namespace astrabot::adapter::cstrike {
enum class JumpPhysicsReason : std::uint8_t {
    None, MissingHost, MissingActor, InvalidTick, MoveType, Water,
    WaterJump, BaseVelocity, MovingSupport, MissingGravity,
    InvalidGravity, InvalidJumpHeight, NonCanonicalHull, TicketStale,
    BindingChanged, GravityChanged, ImpulseChanged,
    CrouchMultiplierChanged, PostureIncompatible
};
enum class JumpActorPosture : std::uint8_t {
    Unknown, Standing, Crouching, Transition, Invalid
};
struct JumpPhysicsAssessment {
    JumpPhysicsReason reason{JumpPhysicsReason::None};
    JumpActorPosture posture{JumpActorPosture::Unknown};
    std::optional<nav::local::JumpPhysics> physics{};
    std::optional<nav::runtime::HullDimensions> actorHull{};
    nav::model::NavVector3 baseVelocity{};
    int moveType{};
    int waterLevel{};
    int flags{};
    bool movingSupport{};
    explicit operator bool() const noexcept {
        return reason==JumpPhysicsReason::None && physics.has_value();
    }
};
const char* jumpPhysicsReasonName(JumpPhysicsReason) noexcept;
const char* jumpActorPostureName(JumpActorPosture) noexcept;
inline constexpr nav::local::WalkJumpLimits jumpLimits{
    {120,100,180,16,16,5,96,1,4,21,2000000,200000,1500000,1500000},
    {80,1},{21,8,0.125,2,18}};
// Standard CS/ReGameDLL public physics model. Private API/hook replacements of
// jump impulse or movement are outside this profile; no private data is read.
JumpPhysicsAssessment assessStandardJumpPhysics(enginefuncs_t*,edict_t*,
    nav::local::Binding,core::TickId) noexcept;
JumpPhysicsReason validateJumpPhysicsTicket(const nav::local::JumpPhysics&,
    const nav::local::JumpPhysics&,nav::local::Binding,core::TickId) noexcept;
std::optional<nav::local::JumpPhysics> standardJumpPhysics(enginefuncs_t*,edict_t*,
    nav::local::Binding,core::TickId) noexcept;
}
