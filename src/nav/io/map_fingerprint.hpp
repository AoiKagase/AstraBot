// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "nav/enrichment/traversal_link.hpp"
#include <istream>
#include <optional>
namespace astrabot::nav::io {
enum class FingerprintReason { None, InvalidLimit, InputFailure, SizeLimit };
struct MapFingerprintResult {
    FingerprintReason reason{FingerprintReason::InputFailure};
    std::optional<enrichment::NavMapFingerprint> fingerprint{};
    std::uint64_t bytes{};
    explicit operator bool() const noexcept { return reason==FingerprintReason::None && fingerprint.has_value(); }
};
// SHA-256 of bytes from current position to EOF. Read-only, fixed working memory.
// Hard cap512 MiB; caller supplies a tighter/equal cap. This identifies the file
// stream, not the authenticity or in-memory state of a running engine's BSP.
MapFingerprintResult fingerprintMap(std::istream&,std::uint64_t maxBytes) noexcept;
}
