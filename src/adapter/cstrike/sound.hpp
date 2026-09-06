// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "adapter/metamod/plugin_entry.hpp"
#include "adapter/cstrike/sound_profile.hpp"

namespace astrabot::adapter::metamod { class LifecycleCoordinator; }
namespace astrabot::adapter::cstrike {
struct SoundAdapterDiagnostics {
    std::uint64_t captured{}, processed{}, overflow{}, duplicates{}, invalid{}, unknown{}, expired{}, retired{}, reentrant{};
    std::uint64_t eventBindings{}, eventRejected{}, recipientRejected{}, hookRejected{};
    std::size_t queued{}, frameEvents{}, frameAudienceChecks{};
};
// Exact emitter/listener positions and engine identity stay in this adapter.
class SoundAdapter final {
public:
    explicit SoundAdapter(core::world::SoundMemoryModel& memory) noexcept : memory_(memory) {}
    static constexpr std::size_t queueCapacity = 256, eventsPerFrame = 32;
    void reset() noexcept;
    void beginMap(core::MapGeneration) noexcept;
    void beginRound(core::perception::RoundGeneration) noexcept;
    void forget(core::PlayerId) noexcept;
    void precache(int type,const char* name,std::uint16_t index) noexcept;
    void emit(metamod::LifecycleCoordinator&,float time,const edict_t* emitter,const float* origin,
        int channel,const char* sample,float volume,float attenuation,int flags,int pitch,bool ambient) noexcept;
    void playback(metamod::LifecycleCoordinator&,float time,int flags,const edict_t* invoker,
        std::uint16_t event,float delay,const float* origin) noexcept;
    void frame(metamod::LifecycleCoordinator&,float time) noexcept;
    const core::world::SoundMemoryModel& memory() const noexcept { return memory_; }
    const SoundAdapterDiagnostics& diagnostics() const noexcept { return diagnostics_; }
    const EventCatalog& events() const noexcept { return events_; }
    std::uint64_t revision() const noexcept { return revision_; }
    void rejectHook() noexcept { ++diagnostics_.hookRejected; }
private:
    struct Listener {
        core::PlayerId player{}; core::BotAgentId agent{};
        core::perception::Point position{};
        std::uint64_t epoch{};
    };
    struct Key {
        std::uint64_t time{}; std::uintptr_t emitter{}; int serial{}, channel{}, pitch{};
        std::uint16_t event{}; bool ambient{};
        std::array<char,96> sample{};
        core::perception::Point source{};
        double volume{}, attenuation{};
    };
    struct Pending {
        core::perception::ObservationIdentity identity{};
        core::perception::SoundKind kind{};
        core::perception::Point source{};
        double volume{}, attenuation{};
        std::array<Listener,core::perception::kPlayerCapacity> audience{};
    };
    bool synchronize(metamod::LifecycleCoordinator&) noexcept;
    bool timestamp(float,std::uint64_t&) noexcept;
    void capture(metamod::LifecycleCoordinator&,float,Key,core::perception::SoundKind) noexcept;
    void clearQueue() noexcept;
    EventCatalog events_{};
    core::world::SoundMemoryModel& memory_;
    core::MapGeneration map_{};
    core::perception::RoundGeneration round_{1};
    std::array<Listener,core::perception::kPlayerCapacity> listeners_{};
    std::array<std::uint64_t,core::perception::kPlayerCapacity> epochs_{};
    std::array<Pending,queueCapacity> queue_{};
    std::array<Key,queueCapacity> keys_{};
    std::size_t head_{}, count_{}, keyCount_{}, keyCursor_{};
    std::uint64_t sequence_{}, timeHighWater_{}, revision_{};
    SoundAdapterDiagnostics diagnostics_{};
    bool capturing_{};
};
} // namespace astrabot::adapter::cstrike
