// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include "host/game_host.hpp"

#include <type_traits>

namespace astrabot::host {

static_assert(std::is_trivially_copyable_v<LifecycleEvent>);
static_assert(std::is_trivially_copyable_v<LifecycleResult>);
static_assert(std::is_trivially_copyable_v<SimulationTime>);
static_assert(std::is_trivially_copyable_v<CommandResult>);

} // namespace astrabot::host
