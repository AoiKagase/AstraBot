// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#pragma once

namespace astrabot::debug {

using TraceSink = void (*)(const char* line) noexcept;

const char* attachedIdentityLine() noexcept;
void emitAttached(TraceSink sink) noexcept;

} // namespace astrabot::debug
