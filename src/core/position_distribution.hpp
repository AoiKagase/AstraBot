// SPDX-License-Identifier: MPL-2.0
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
namespace astrabot::core::world {
struct AreaWeight { std::uint32_t area{}; double weight{}; };
struct PositionDistribution {
    std::array<AreaWeight,32> areas{};
    std::size_t count{};
    double unknownMass{};
    std::uint64_t navRevision{}, updatedMicros{}, delayMicros{};
    bool available{};
};
}
