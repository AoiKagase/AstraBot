// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "adapter/metamod/plugin_entry.hpp"
#include "nav/local/walk.hpp"

namespace astrabot::adapter::cstrike {
inline constexpr nav::local::WalkJumpLimits jumpLimits{
    {120,100,180,16,16,5,96,32,4,21,2000000,200000,1500000,1500000},
    {80,1},{21,8,0.125,2,18}};
// Standard CS/ReGameDLL public physics model. Private API/hook replacements of
// jump impulse or movement are outside this profile; no private data is read.
std::optional<nav::local::JumpPhysics> standardJumpPhysics(enginefuncs_t*,edict_t*,
    nav::local::Binding,core::TickId) noexcept;
}
