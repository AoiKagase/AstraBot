// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "core/sound_memory.hpp"

namespace astrabot::adapter::cstrike {
// Adapter-only interpretation of engine names and exact coordinates.
core::perception::SoundKind soundSample(const char*) noexcept;
bool quantizeSound(core::perception::Point,core::perception::SoundRegion&) noexcept;
bool soundAudible(core::perception::Point source,core::perception::Point listener,double volume,double attenuation) noexcept;
enum class EventKind : std::uint8_t { Unknown, Gunshot, Smoke };
enum class EventBinding : std::uint8_t { Registered, Duplicate, Unsupported, Invalid, Conflict, Full };
class EventCatalog final {
public:
    static constexpr std::size_t capacity = 128;
    void reset() noexcept { entries_ = {}; count_ = 0; }
    EventBinding bind(std::uint16_t,const char*) noexcept;
    EventKind find(std::uint16_t) const noexcept;
    std::size_t size() const noexcept { return count_; }
private:
    struct Entry { std::uint16_t index{}; EventKind kind{}; bool conflict{}; std::array<char,64> name{}; };
    std::array<Entry,capacity> entries_{};
    std::size_t count_{};
};
} // namespace astrabot::adapter::cstrike
