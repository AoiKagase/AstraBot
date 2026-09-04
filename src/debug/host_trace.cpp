// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include "debug/host_trace.hpp"

namespace astrabot::debug {
namespace {

constexpr char kAttachedIdentityLine[] =
    "astrabot version=0.1.0 adapter=metamod-p interface=5:13 outcome=attached";

} // namespace

const char* attachedIdentityLine() noexcept {
    return kAttachedIdentityLine;
}

void emitAttached(TraceSink sink) noexcept {
    if (sink != nullptr) {
        sink(kAttachedIdentityLine);
    }
}

} // namespace astrabot::debug
