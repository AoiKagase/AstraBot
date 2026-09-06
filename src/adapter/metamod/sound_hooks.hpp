// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "adapter/metamod/plugin_entry.hpp"
namespace astrabot::adapter::metamod {
// Installed via META_FUNCTIONS, not a new DLL export.
int soundEngineFunctionsPost(enginefuncs_t*,int*);
void installSoundPreHooks(enginefuncs_t&) noexcept;
}
