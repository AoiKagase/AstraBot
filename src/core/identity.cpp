// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include "core/identity.hpp"

#include <type_traits>

namespace astrabot::core {

static_assert(std::is_trivially_copyable_v<Generation>);
static_assert(std::is_trivially_copyable_v<MapGeneration>);
static_assert(std::is_trivially_copyable_v<PlayerId>);
static_assert(std::is_trivially_copyable_v<BotAgentId>);
static_assert(std::is_trivially_copyable_v<TickId>);

} // namespace astrabot::core
