// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "core/visual_memory.hpp"
namespace astrabot::core::world {
enum class ReportReason : std::uint8_t { None, NotReady, InvalidTime, InvalidActor, NoDirectSight, TooOld, NoAllies, Duplicate, QueueFull, Stale, Expired, Exhausted, Count };
const char* reportReasonName(ReportReason) noexcept;
struct ReportResult { ReportReason reason{}; std::size_t recipients{}; bool accepted() const noexcept { return reason==ReportReason::None && recipients>0; } };
struct TeamReport {
    PlayerId reporter{}, receiver{}, target{};
    perception::Point position{};
    perception::ObservationIdentity origin{}, identity{};
    std::uint64_t sentMicros{};
};
struct ReportMemory { TeamReport report{}; double confidence{}; };
struct ReportSnapshot { perception::Stamp stamp{}; std::array<ReportMemory,31> reports{}; std::size_t count{}; };
struct ReportDiagnostics {
    ReportReason reason{};
    std::array<std::uint64_t,static_cast<std::size_t>(ReportReason::Count)> rejected{};
    std::uint64_t sent{}, delivered{}, retired{}, overflow{};
    std::size_t queued{}, frameDelivered{}, frameVisits{};
};
class TeamReportModel final {
public:
    static constexpr std::uint64_t freshnessMicros=500000, retentionMicros=5000000;
    static constexpr std::size_t queueCapacity=256, deliveriesPerFrame=32;
    void reset() noexcept;
    bool advance(const MemoryFrame&,const perception::TeamRoster&) noexcept;
    void invalidate() noexcept;
    void forget(PlayerId) noexcept;
    void beginRound(perception::RoundGeneration) noexcept;
    ReportResult send(const VisualMemoryModel&,PlayerId,PlayerId,std::uint64_t,
        const std::array<bool,32>&,const perception::TeamRoster&) noexcept;
    void deliver() noexcept;
    const ReportSnapshot* latest(PlayerId) const noexcept;
    const ReportDiagnostics& diagnostics() const noexcept { return diagnostics_; }
private:
    struct Participant { MemoryPlayer value{}; perception::Team team{}; std::uint64_t epoch{}; };
    struct Pending { TeamReport report{}; std::uint64_t reporterEpoch{}, receiverEpoch{}, targetEpoch{}; };
    ReportResult reject(ReportReason) noexcept;
    bool eligible(PlayerId,bool managed=false) const noexcept;
    bool allies(PlayerId,PlayerId) const noexcept;
    bool current(const TeamReport&) const noexcept;
    void prune(bool) noexcept;
    void clearQueue() noexcept;
    MemoryFrame frame_{};
    std::array<Participant,32> participants_{};
    std::array<Generation,32> generations_{};
    std::array<ReportSnapshot,32> snapshots_{};
    struct Epochs { std::uint64_t reporter{}, receiver{}, target{}; };
    std::array<std::array<Epochs,31>,32> retainedEpochs_{};
    std::array<std::array<std::uint64_t,32>,32> sentOrigins_{};
    std::array<Pending,queueCapacity> queue_{};
    std::size_t head_{}, count_{};
    std::uint64_t sequence_{}, timeHighWater_{};
    bool ready_{};
    ReportDiagnostics diagnostics_{};
};
}
