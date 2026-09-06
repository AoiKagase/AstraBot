// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "core/command.hpp"
#include "nav/model/value_types.hpp"
#include <optional>
namespace astrabot::nav::local {
struct LadderAirPhysics {
    double gravity{},airAcceleration{},friction{},maximumSpeed{},maximumVelocity{};
};
struct LadderAirStep {
    model::NavVector3 displacement{},velocity{};
};
// Standard standing CS profile, measured cardinal/vertical ladder face and
// actual command buttons. Caller must prove selected model contact/face and
// on-floor point solidity; this does not perform collision handling or dispatch.
std::optional<model::NavVector3> ladderVelocity(const core::BotCommand&,
    model::NavVector3 outwardNormal,double maximumSpeed,bool floorSolid) noexcept;
// One airborne WALK frame with no water/basevelocity/contact/collision. Command
// duration is the actual rounded motor msec, at most120. Host must separately
// sweep the displacement and verify the supplied current physics parameters.
std::optional<LadderAirStep> ladderAirStep(const core::BotCommand&,
    model::NavVector3 velocity,LadderAirPhysics) noexcept;
// Jump while touching a verified ladder in the air switches to WALK and
// replaces velocity with270 along the face normal, then runs air physics.
// Requires current airborne contact; grounded jumps need ground friction/move.
std::optional<LadderAirStep> ladderJumpAirStep(const core::BotCommand&,
    model::NavVector3 outwardNormal,LadderAirPhysics) noexcept;
}
