// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "core/identity.hpp"
#include <array>
#include <cstddef>

namespace astrabot::core::perception {
struct RoundGeneration {
    std::uint64_t value{};
    constexpr bool isValid() const noexcept { return value != 0; }
    friend constexpr bool operator==(RoundGeneration a, RoundGeneration b) noexcept { return a.value == b.value; }
    friend constexpr bool operator!=(RoundGeneration a, RoundGeneration b) noexcept { return !(a == b); }
};
enum class ObservationSource : std::uint8_t { Unknown, Vision, Sound, TeamReport };
// Sequence is assigned once at the source, scoped to map/round/source.
// A received copy retains this identity and its original occurrence time.
struct ObservationIdentity {
    MapGeneration map{};
    RoundGeneration round{};
    ObservationSource source{};
    std::uint64_t sequence{};
    std::uint64_t observedMicros{}, receivedMicros{};
    constexpr bool validAt(std::uint64_t now) const noexcept {
        return map.isValid() && round.isValid() && sequence != 0 &&
            (source == ObservationSource::Vision || source == ObservationSource::Sound ||
             source == ObservationSource::TeamReport) && observedMicros <= receivedMicros && receivedMicros <= now;
    }
};
enum class Team : std::uint8_t { Unknown, Terrorist, CounterTerrorist, Spectator };
enum class Relation : std::uint8_t { Self, Ally, Opponent, Unknown };
struct TeamMember { PlayerId player{}; Team team{}; };
// Geometry-free lifecycle owner. bind() registers identity, update() only changes
// a currently bound member. Retirement high-water marks prevent resurrection.
class TeamRoster final {
public:
    bool activate(MapGeneration map) noexcept {
        if (!map.isValid() || map.value <= map_.value) return false;
        map_ = map; members_ = {}; generations_ = {}; return true;
    }
    bool bind(MapGeneration map, PlayerId player) noexcept {
        if (map != map_ || !map.isValid() || !validPlayer(player)) return false;
        const auto i = static_cast<std::size_t>(player.slot - 1U);
        if (members_[i].player == player) return true;
        if (player.generation.value <= generations_[i].value) return false;
        generations_[i] = player.generation; members_[i] = {player,Team::Unknown}; return true;
    }
    bool update(MapGeneration map, PlayerId player, Team team) noexcept {
        if (map != map_ || !find(player) || !validTeam(team)) return false;
        members_[player.slot - 1U].team = team; return true;
    }
    void forget(PlayerId player) noexcept {
        if (find(player)) members_[player.slot - 1U] = {};
    }
    void clear() noexcept { members_ = {}; } // Retain map/generation high-water marks.
    const TeamMember* find(PlayerId player) const noexcept {
        if (!validPlayer(player)) return nullptr;
        const auto& member = members_[player.slot - 1U];
        return member.player == player ? &member : nullptr;
    }
    Relation relation(PlayerId observer, PlayerId target) const noexcept {
        const auto* a = find(observer); const auto* b = find(target);
        if (!a || !b) return Relation::Unknown;
        if (observer == target) return Relation::Self;
        if (!playing(a->team) || !playing(b->team)) return Relation::Unknown;
        return a->team == b->team ? Relation::Ally : Relation::Opponent;
    }
    MapGeneration map() const noexcept { return map_; }
private:
    static constexpr bool validPlayer(PlayerId p) noexcept { return p.isValid() && p.slot <= 32; }
    static constexpr bool playing(Team t) noexcept { return t == Team::Terrorist || t == Team::CounterTerrorist; }
    static constexpr bool validTeam(Team t) noexcept { return playing(t) || t == Team::Unknown || t == Team::Spectator; }
    MapGeneration map_{};
    std::array<TeamMember,32> members_{};
    std::array<Generation,32> generations_{};
};
} // namespace astrabot::core::perception
