// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include "adapter/metamod/console_debug.hpp"

#include "adapter/cstrike/join_state.hpp"
#include "adapter/metamod/lifecycle.hpp"

#include <cstdio>
#include <cstring>

#ifdef snprintf
#undef snprintf
#endif

namespace astrabot::adapter::metamod {
namespace {

const char* lifecycleKindName(host::LifecycleEventKind kind) noexcept {
    switch (kind) {
    case host::LifecycleEventKind::None: return "None";
    case host::LifecycleEventKind::MapActivated: return "MapActivated";
    case host::LifecycleEventKind::MapDeactivated: return "MapDeactivated";
    case host::LifecycleEventKind::PlayerConnected: return "PlayerConnected";
    case host::LifecycleEventKind::PlayerDisconnected: return "PlayerDisconnected";
    case host::LifecycleEventKind::FrameStarted: return "FrameStarted";
    }
    return "Unknown";
}

const char* hostErrorName(host::HostError error) noexcept {
    switch (error) {
    case host::HostError::None: return "None";
    case host::HostError::InvalidLifecycle: return "InvalidLifecycle";
    case host::HostError::InvalidPlayer: return "InvalidPlayer";
    case host::HostError::StalePlayerGeneration: return "StalePlayerGeneration";
    case host::HostError::NotMapActive: return "NotMapActive";
    case host::HostError::AlreadyConnected: return "AlreadyConnected";
    case host::HostError::NotConnected: return "NotConnected";
    case host::HostError::StaleTick: return "StaleTick";
    case host::HostError::DuplicateTick: return "DuplicateTick";
    case host::HostError::InvalidCommand: return "InvalidCommand";
    case host::HostError::FrameNotStarted: return "FrameNotStarted";
    case host::HostError::Rejected: return "Rejected";
    case host::HostError::Unsupported: return "Unsupported";
    }
    return "Unknown";
}

const char* fakeClientStageName(debug::FakeClientStage stage) noexcept {
    switch (stage) {
    case debug::FakeClientStage::None: return "None";
    case debug::FakeClientStage::Requested: return "Requested";
    case debug::FakeClientStage::Allocated: return "Allocated";
    case debug::FakeClientStage::PlayerFactory: return "PlayerFactory";
    case debug::FakeClientStage::Metadata: return "Metadata";
    case debug::FakeClientStage::Connected: return "Connected";
    case debug::FakeClientStage::PutInServer: return "PutInServer";
    case debug::FakeClientStage::Published: return "Published";
    case debug::FakeClientStage::Rejected: return "Rejected";
    case debug::FakeClientStage::RolledBack: return "RolledBack";
    }
    return "Unknown";
}

const char* fakeClientErrorName(debug::FakeClientError error) noexcept {
    switch (error) {
    case debug::FakeClientError::None: return "None";
    case debug::FakeClientError::NotConfigured: return "NotConfigured";
    case debug::FakeClientError::MissingFunction: return "MissingFunction";
    case debug::FakeClientError::NotMapActive: return "NotMapActive";
    case debug::FakeClientError::AlreadyCreated: return "AlreadyCreated";
    case debug::FakeClientError::Reentrant: return "Reentrant";
    case debug::FakeClientError::InvalidName: return "InvalidName";
    case debug::FakeClientError::CreateFailed: return "CreateFailed";
    case debug::FakeClientError::InvalidEntity: return "InvalidEntity";
    case debug::FakeClientError::InvalidSlot: return "InvalidSlot";
    case debug::FakeClientError::SlotOccupied: return "SlotOccupied";
    case debug::FakeClientError::PlayerFactoryFailed: return "PlayerFactoryFailed";
    case debug::FakeClientError::InfoBufferFailed: return "InfoBufferFailed";
    case debug::FakeClientError::ConnectRejected: return "ConnectRejected";
    case debug::FakeClientError::PlayerRegistrationFailed: return "PlayerRegistrationFailed";
    case debug::FakeClientError::AgentBindingFailed: return "AgentBindingFailed";
    }
    return "Unknown";
}

const char* joinPhaseName(cstrike::JoinPhase phase) noexcept {
    switch (phase) {
    case cstrike::JoinPhase::Idle: return "Idle";
    case cstrike::JoinPhase::WaitingTeamMenu: return "WaitingTeamMenu";
    case cstrike::JoinPhase::TeamCommandPending: return "TeamCommandPending";
    case cstrike::JoinPhase::WaitingClassMenu: return "WaitingClassMenu";
    case cstrike::JoinPhase::ClassCommandPending: return "ClassCommandPending";
    case cstrike::JoinPhase::WaitingConfirmation: return "WaitingConfirmation";
    case cstrike::JoinPhase::Joined: return "Joined";
    case cstrike::JoinPhase::Failed: return "Failed";
    case cstrike::JoinPhase::Cancelled: return "Cancelled";
    }
    return "Unknown";
}

const char* joinErrorName(cstrike::JoinError error) noexcept {
    switch (error) {
    case cstrike::JoinError::None: return "None";
    case cstrike::JoinError::InvalidRequest: return "InvalidRequest";
    case cstrike::JoinError::AlreadyJoining: return "AlreadyJoining";
    case cstrike::JoinError::InvalidPlayer: return "InvalidPlayer";
    case cstrike::JoinError::InvalidMap: return "InvalidMap";
    case cstrike::JoinError::MenuOptionUnavailable: return "MenuOptionUnavailable";
    case cstrike::JoinError::WrongTeam: return "WrongTeam";
    case cstrike::JoinError::CommandAttemptsExhausted: return "CommandAttemptsExhausted";
    case cstrike::JoinError::Timeout: return "Timeout";
    case cstrike::JoinError::CommandDispatchFailed: return "CommandDispatchFailed";
    case cstrike::JoinError::CommandContextReentrant: return "CommandContextReentrant";
    case cstrike::JoinError::Disconnected: return "Disconnected";
    case cstrike::JoinError::MapDeactivated: return "MapDeactivated";
    case cstrike::JoinError::KickFailed: return "KickFailed";
    case cstrike::JoinError::MessageUnavailable: return "MessageUnavailable";
    case cstrike::JoinError::GameDllProgressUnavailable: return "GameDllProgressUnavailable";
    }
    return "Unknown";
}

const char* removalOutcomeName(debug::RemovalOutcome outcome) noexcept {
    switch (outcome) {
    case debug::RemovalOutcome::None: return "None";
    case debug::RemovalOutcome::NoOp: return "NoOp";
    case debug::RemovalOutcome::KickQueued: return "KickQueued";
    case debug::RemovalOutcome::Cleaned: return "Cleaned";
    case debug::RemovalOutcome::Rejected: return "Rejected";
    }
    return "Unknown";
}

const char* removalErrorName(debug::RemovalError error) noexcept {
    switch (error) {
    case debug::RemovalError::None: return "None";
    case debug::RemovalError::NotConfigured: return "NotConfigured";
    case debug::RemovalError::NoActiveClient: return "NoActiveClient";
    case debug::RemovalError::AlreadyPending: return "AlreadyPending";
    case debug::RemovalError::InvalidUserId: return "InvalidUserId";
    case debug::RemovalError::KickUnavailable: return "KickUnavailable";
    case debug::RemovalError::CommandBuildFailed: return "CommandBuildFailed";
    case debug::RemovalError::DirectCleanupFailed: return "DirectCleanupFailed";
    }
    return "Unknown";
}

const char* teamName(cstrike::Team team) noexcept {
    switch (team) {
    case cstrike::Team::Terrorist: return "T";
    case cstrike::Team::CounterTerrorist: return "CT";
    }
    return "Unknown";
}

const char* movementSourceName(debug::MovementTraceSource source) noexcept {
    switch(source) {
    case debug::MovementTraceSource::None: return "None";
    case debug::MovementTraceSource::Join: return "Join";
    case debug::MovementTraceSource::Command: return "Command";
    case debug::MovementTraceSource::Idle: return "Idle";
    case debug::MovementTraceSource::Dead: return "Dead";
    }
    return "Unknown";
}

const char* runtimeInputReasonName(RuntimeInputBuildReason reason) noexcept {
    switch(reason) {
    case RuntimeInputBuildReason::None: return "None";
    case RuntimeInputBuildReason::InvalidFrame: return "InvalidFrame";
    case RuntimeInputBuildReason::MissingPrimary: return "MissingPrimary";
    case RuntimeInputBuildReason::StaleActor: return "StaleActor";
    case RuntimeInputBuildReason::MissingWorld: return "MissingWorld";
    case RuntimeInputBuildReason::MissingNav: return "MissingNav";
    case RuntimeInputBuildReason::MissingCurrentArea: return "MissingCurrentArea";
    case RuntimeInputBuildReason::MissingPosition: return "MissingPosition";
    case RuntimeInputBuildReason::WeaponUnavailable: return "WeaponUnavailable";
    case RuntimeInputBuildReason::MissingUpdateClientData: return "MissingUpdateClientData";
    case RuntimeInputBuildReason::MissingWeaponData: return "MissingWeaponData";
    case RuntimeInputBuildReason::InvalidWeaponObservation: return "InvalidWeaponObservation";
    case RuntimeInputBuildReason::MissingTeam: return "MissingTeam";
    case RuntimeInputBuildReason::TeamGenerationMismatch: return "TeamGenerationMismatch";
    case RuntimeInputBuildReason::UnknownTeam: return "UnknownTeam";
    case RuntimeInputBuildReason::CombatConversionFailed: return "CombatConversionFailed";
    }
    return "Unknown";
}

const char* runtimeActorStaleReasonName(RuntimeActorStaleReason reason) noexcept {
    switch (reason) {
    case RuntimeActorStaleReason::None: return "None";
    case RuntimeActorStaleReason::MapInactive: return "MapInactive";
    case RuntimeActorStaleReason::MapGenerationMismatch: return "MapGenerationMismatch";
    case RuntimeActorStaleReason::TickMismatch: return "TickMismatch";
    case RuntimeActorStaleReason::RoundMismatch: return "RoundMismatch";
    case RuntimeActorStaleReason::PlayerGenerationMismatch: return "PlayerGenerationMismatch";
    case RuntimeActorStaleReason::BindingInvalid: return "BindingInvalid";
    case RuntimeActorStaleReason::BindingAgentMismatch: return "BindingAgentMismatch";
    case RuntimeActorStaleReason::BindingMapMismatch: return "BindingMapMismatch";
    case RuntimeActorStaleReason::MissingEntity: return "MissingEntity";
    case RuntimeActorStaleReason::EntityFree: return "EntityFree";
    case RuntimeActorStaleReason::RemovalPending: return "RemovalPending";
    case RuntimeActorStaleReason::NotJoined: return "NotJoined";
    case RuntimeActorStaleReason::Dead: return "Dead";
    case RuntimeActorStaleReason::InvalidHealth: return "InvalidHealth";
    case RuntimeActorStaleReason::SpectatorState: return "SpectatorState";
    case RuntimeActorStaleReason::SpectatorFlag: return "SpectatorFlag";
    }
    return "Unknown";
}

const char* runtimeNavigationResultName(
    cstrike::RuntimeNavigationApplyResult result) noexcept {
    switch (result) {
    case cstrike::RuntimeNavigationApplyResult::None: return "None";
    case cstrike::RuntimeNavigationApplyResult::Applied: return "Applied";
    case cstrike::RuntimeNavigationApplyResult::Unchanged: return "Unchanged";
    case cstrike::RuntimeNavigationApplyResult::Rejected: return "Rejected";
    }
    return "Unknown";
}

const char* runtimeNavigationReasonName(cstrike::RuntimeNavigationApplyReason reason) noexcept {
    switch (reason) {
    case cstrike::RuntimeNavigationApplyReason::None: return "None";
    case cstrike::RuntimeNavigationApplyReason::NoExecutableGoal: return "NoExecutableGoal";
    case cstrike::RuntimeNavigationApplyReason::InvalidIdentity: return "InvalidIdentity";
    case cstrike::RuntimeNavigationApplyReason::RequestReentrant: return "RequestReentrant";
    case cstrike::RuntimeNavigationApplyReason::MapInactive: return "MapInactive";
    case cstrike::RuntimeNavigationApplyReason::StampMismatch: return "StampMismatch";
    case cstrike::RuntimeNavigationApplyReason::ActorUnavailable: return "ActorUnavailable";
    case cstrike::RuntimeNavigationApplyReason::ActorStateInvalid: return "ActorStateInvalid";
    case cstrike::RuntimeNavigationApplyReason::RouteRejected: return "RouteRejected";
    }
    return "Unknown";
}
} // namespace

ConsoleDebug& ConsoleDebug::instance() noexcept {
    static ConsoleDebug value{};
    return value;
}

void ConsoleDebug::configure(
    enginefuncs_t* engine,
    mutil_funcs_t* utility,
    LifecycleCoordinator* lifecycle) noexcept {
    reset();
    engine_ = engine;
    utility_ = utility;
    lifecycle_ = lifecycle;
    if (engine_ == nullptr || utility_ == nullptr || lifecycle_ == nullptr ||
        engine_->pfnAddServerCommand == nullptr ||
        engine_->pfnCmd_Argc == nullptr || engine_->pfnCmd_Argv == nullptr) {
        return;
    }

    static char commandName[] = "astrabot_debug";
    engine_->pfnAddServerCommand(commandName, &ConsoleDebug::command);
    static char addBotCommandName[] = "astrabot_addbot";
    engine_->pfnAddServerCommand(addBotCommandName, &ConsoleDebug::addBotCommand);
    lifecycle_->setTraceSink(&ConsoleDebug::lifecycleSink);
    lifecycle_->setFakeClientTraceSink(&ConsoleDebug::fakeClientSink);
    lifecycle_->setJoinTraceSink(&ConsoleDebug::joinSink);
    lifecycle_->setRemovalTraceSink(&ConsoleDebug::removalSink);
    lifecycle_->setMovementTraceSink(&ConsoleDebug::movementSink);
}

void ConsoleDebug::reset() noexcept {
    if (lifecycle_ != nullptr) {
        lifecycle_->setTraceSink(nullptr);
        lifecycle_->setFakeClientTraceSink(nullptr);
        lifecycle_->setJoinTraceSink(nullptr);
        lifecycle_->setRemovalTraceSink(nullptr);
        lifecycle_->setMovementTraceSink(nullptr);
    }
    engine_ = nullptr;
    utility_ = nullptr;
    lifecycle_ = nullptr;
    enabled_ = false;
    nextBotOrdinal_ = 1;
    lastMovementLogCall_.fill(0);
    lastMovementSource_.fill(debug::MovementTraceSource::None);
}

void ConsoleDebug::command() {
    auto& self = instance();
    if (self.engine_ == nullptr || self.engine_->pfnCmd_Argc == nullptr ||
        self.engine_->pfnCmd_Argv == nullptr) {
        self.commandLine("[ASTRABOT][DEBUG][COMMAND] error=Unavailable");
        return;
    }

    const int argc = self.engine_->pfnCmd_Argc();
    if (argc == 1) {
        self.commandLine(self.enabled_
                             ? "[ASTRABOT][DEBUG][COMMAND] state=on"
                             : "[ASTRABOT][DEBUG][COMMAND] state=off");
        return;
    }
    if (argc != 2) {
        self.commandLine(
            "[ASTRABOT][DEBUG][COMMAND] error=InvalidArguments expected=0|1");
        return;
    }

    const char* value = self.engine_->pfnCmd_Argv(1);
    if (value == nullptr || (std::strcmp(value, "0") != 0 &&
                             std::strcmp(value, "1") != 0)) {
        self.commandLine(
            "[ASTRABOT][DEBUG][COMMAND] error=InvalidArguments expected=0|1");
        return;
    }

    self.enabled_ = value[0] == '1';
    self.commandLine(self.enabled_
                         ? "[ASTRABOT][DEBUG][COMMAND] enabled=1"
                         : "[ASTRABOT][DEBUG][COMMAND] enabled=0");
}

void ConsoleDebug::addBotCommand() {
    auto& self = instance();
    if (self.lifecycle_ == nullptr || self.engine_ == nullptr ||
        self.engine_->pfnCmd_Argc == nullptr ||
        self.engine_->pfnCmd_Argv == nullptr) {
        self.commandLine(
            "[ASTRABOT][DEBUG][COMMAND] addbot error=Unavailable");
        return;
    }

    const int argc = self.engine_->pfnCmd_Argc();
    if (argc > 2) {
        self.commandLine(
            "[ASTRABOT][DEBUG][COMMAND] addbot error=InvalidArguments expected=count");
        return;
    }

    std::uint32_t requested = 1;
    if (argc == 2) {
        const char* value = self.engine_->pfnCmd_Argv(1);
        if (value == nullptr || value[0] == '\0') {
            self.commandLine(
                "[ASTRABOT][DEBUG][COMMAND] addbot error=InvalidArguments expected=count");
            return;
        }
        requested = 0;
        for (std::size_t index = 0; value[index] != '\0'; ++index) {
            if (value[index] < '0' || value[index] > '9' || requested > 32U) {
                self.commandLine(
                    "[ASTRABOT][DEBUG][COMMAND] addbot error=InvalidArguments expected=count");
                return;
            }
            requested = requested * 10U +
                        static_cast<std::uint32_t>(value[index] - '0');
            if (requested > 32U) {
                self.commandLine(
                    "[ASTRABOT][DEBUG][COMMAND] addbot error=InvalidArguments expected=1..32");
                return;
            }
        }
        if (requested == 0U) {
            self.commandLine(
                "[ASTRABOT][DEBUG][COMMAND] addbot error=InvalidArguments expected=1..32");
            return;
        }
    }

    std::uint32_t created = 0;
    const char* firstError = "None";
    for (std::uint32_t index = 0; index < requested; ++index) {
        char name[32]{};
        const auto terroristCount =
            self.lifecycle_->managedTeamCount(cstrike::Team::Terrorist);
        const auto counterTerroristCount =
            self.lifecycle_->managedTeamCount(cstrike::Team::CounterTerrorist);
        const auto team = terroristCount <= counterTerroristCount
                              ? cstrike::Team::Terrorist
                              : cstrike::Team::CounterTerrorist;
        const auto classNumber = static_cast<std::uint8_t>(
            ((self.nextBotOrdinal_ - 1U) % 4U) + 1U);
        std::snprintf(name, sizeof(name), "AstraBot-%u", self.nextBotOrdinal_);
        const auto result = self.lifecycle_->createBot(
            name, {team, classNumber});
        if (!result.succeeded()) {
            firstError = fakeClientErrorName(result.error);
            break;
        }
        ++created;
        ++self.nextBotOrdinal_;
    }

    char lineBuffer[256]{};
    std::snprintf(
        lineBuffer, sizeof(lineBuffer),
        "[ASTRABOT][DEBUG][COMMAND] addbot requested=%u created=%u error=%s",
        unsigned(requested), unsigned(created), firstError);
    self.commandLine(lineBuffer);
}

void ConsoleDebug::line(const char* text) noexcept {
    if (!enabled_ || text == nullptr || utility_ == nullptr ||
        utility_->pfnLogConsole == nullptr) {
        return;
    }
    utility_->pfnLogConsole(PLID, "%s", text);
}

void ConsoleDebug::commandLine(const char* text) noexcept {
    if (text == nullptr || utility_ == nullptr || utility_->pfnLogConsole == nullptr) {
        return;
    }
    utility_->pfnLogConsole(PLID, "%s", text);
}

void ConsoleDebug::lifecycleTrace(const debug::LifecycleTrace& trace) noexcept {
    if (trace.kind == host::LifecycleEventKind::FrameStarted) {
        return;
    }
    char lineBuffer[320]{};
    std::snprintf(
        lineBuffer,
        sizeof(lineBuffer),
        "[ASTRABOT][DEBUG][LIFECYCLE] kind=%s error=%s map=%u player=%u:%u tick=%llu seq=%llu accepted=%u changed=%u",
        lifecycleKindName(trace.kind), hostErrorName(trace.error),
        unsigned(trace.map.value), unsigned(trace.slot),
        unsigned(trace.playerGeneration.value),
        static_cast<unsigned long long>(trace.tick.value),
        static_cast<unsigned long long>(trace.sequence),
        unsigned(trace.accepted), unsigned(trace.changed));
    line(lineBuffer);
}

void ConsoleDebug::fakeClientTrace(const debug::FakeClientTrace& trace) noexcept {
    char lineBuffer[320]{};
    std::snprintf(
        lineBuffer,
        sizeof(lineBuffer),
        "[ASTRABOT][DEBUG][FAKECLIENT] stage=%s error=%s map=%u player=%u:%u agent=%u seq=%llu accepted=%u changed=%u",
        fakeClientStageName(trace.stage), fakeClientErrorName(trace.error),
        unsigned(trace.map.value), unsigned(trace.slot),
        unsigned(trace.playerGeneration.value), unsigned(trace.agent.value),
        static_cast<unsigned long long>(trace.sequence),
        unsigned(trace.accepted), unsigned(trace.changed));
    line(lineBuffer);
}

void ConsoleDebug::joinTrace(const debug::JoinTrace& trace) noexcept {
    char lineBuffer[384]{};
    std::snprintf(
        lineBuffer,
        sizeof(lineBuffer),
        "[ASTRABOT][DEBUG][JOIN] phase=%s error=%s map=%u player=%u:%u team=%s class=%u tick=%llu seq=%llu attempts=%u accepted=%u changed=%u",
        joinPhaseName(trace.phase), joinErrorName(trace.error),
        unsigned(trace.map.value), unsigned(trace.player.slot),
        unsigned(trace.player.generation.value), teamName(trace.team),
        unsigned(trace.classNumber), static_cast<unsigned long long>(trace.tick.value),
        static_cast<unsigned long long>(trace.sequence), unsigned(trace.attempts),
        unsigned(trace.accepted), unsigned(trace.changed));
    line(lineBuffer);
}

void ConsoleDebug::removalTrace(const debug::RemovalTrace& trace) noexcept {
    char lineBuffer[320]{};
    std::snprintf(
        lineBuffer,
        sizeof(lineBuffer),
        "[ASTRABOT][DEBUG][REMOVAL] outcome=%s error=%s map=%u player=%u:%u tick=%llu seq=%llu mapping=%u entity=%u",
        removalOutcomeName(trace.outcome), removalErrorName(trace.error),
        unsigned(trace.map.value), unsigned(trace.player.slot),
        unsigned(trace.player.generation.value),
        static_cast<unsigned long long>(trace.tick.value),
        static_cast<unsigned long long>(trace.sequence),
        unsigned(trace.mappingPresent), unsigned(trace.entityPresent));
    line(lineBuffer);
}

void ConsoleDebug::movementTrace(const debug::MovementTrace& trace) noexcept {
    if(!enabled_ || !trace.player.isValid() ||
       trace.player.slot>host::kMaxClientSlots || lifecycle_==nullptr) return;
    const auto index=trace.player.slot-1U;
    auto& last=lastMovementLogCall_[index];
    auto& lastSource=lastMovementSource_[index];
    const bool sourceChanged=trace.source!=lastSource;
    if(!trace.engineCall && trace.outcome != debug::MovementTraceOutcome::Rejected) return;
    if(!sourceChanged && trace.callCount!=0 && trace.callCount!=1 && trace.callCount<last+512) return;
    last=trace.callCount;
    lastSource=trace.source;
    const auto* entity=lifecycle_->entityFor(trace.player);
    const auto* join=lifecycle_->joinState(trace.player);
    const auto& runtime=lifecycle_->runtimeInputBuildStatus();
    const auto& nav=lifecycle_->navConsole().runtimeNavigationStatus(trace.player);
    const auto* decision=lifecycle_->runtimeOrchestrator().decision(trace.player);
    const bool inputMatch=runtime.player==trace.player && runtime.agent==trace.agent;
    const bool spectator=entity && (entity->v.iuser1!=0 || (entity->v.flags&FL_SPECTATOR));
    const bool spawned=entity && entity->v.deadflag==DEAD_NO && entity->v.health>0 && !spectator;
    char lineBuffer[768]{};
    std::snprintf(lineBuffer,sizeof(lineBuffer),
        "[ASTRABOT][DEBUG][MOVEMENT] map=%u round=%llu actor=%u:%u agent=%u outcome=%u error=%u input_tick=%llu dispatch_tick=%llu calls=%llu source=%s msec=%u phase=%s spawned=%u x=%.3f y=%.3f z=%.3f vx=%.3f vy=%.3f vz=%.3f movetype=%d solid=%d onground=%u spectator=%u runtime_map=%u runtime_round=%llu runtime_tick=%llu input_actor=%u:%u input_agent=%u input_match=%u runtime=%s stale=%s held_area=%u weapon=%u weapon_class=%u decision_map=%u decision_round=%llu decision_tick=%llu runtime_reject=%u nav=%s nav_reason=%s nav_map=%u nav_round=%llu nav_tick=%llu nav_decision_tick=%llu",
        unsigned(trace.map.value),
        static_cast<unsigned long long>(lifecycle_->round().value),
        unsigned(trace.player.slot),unsigned(trace.player.generation.value),
        unsigned(trace.agent.value),unsigned(trace.outcome),unsigned(trace.error),
        static_cast<unsigned long long>(trace.commandTick.value),
        static_cast<unsigned long long>(trace.dispatchTick.value),
        static_cast<unsigned long long>(trace.callCount),movementSourceName(trace.source),
        unsigned(trace.engineMsec),join ? joinPhaseName(join->phase()):"None",unsigned(spawned),
        entity ? double(entity->v.origin.x):0.0,entity ? double(entity->v.origin.y):0.0,
        entity ? double(entity->v.origin.z):0.0,entity ? double(entity->v.velocity.x):0.0,
        entity ? double(entity->v.velocity.y):0.0,entity ? double(entity->v.velocity.z):0.0,
        entity ? entity->v.movetype:0,entity ? entity->v.solid:0,
        unsigned(entity && (entity->v.flags&FL_ONGROUND)),unsigned(spectator),
        unsigned(runtime.map.value),
        static_cast<unsigned long long>(runtime.round.value),
        static_cast<unsigned long long>(runtime.tick.value),
        unsigned(runtime.player.slot),unsigned(runtime.player.generation.value),
        unsigned(runtime.agent.value),unsigned(inputMatch),
        runtimeInputReasonName(runtime.reason),runtimeActorStaleReasonName(runtime.staleReason),
        unsigned(runtime.currentAreaHeld),unsigned(runtime.activeWeapon),
        unsigned(runtime.activeClass),
        unsigned(decision ? decision->team.shared.map.value : 0U),
        static_cast<unsigned long long>(decision ? decision->team.shared.round.value : 0U),
        static_cast<unsigned long long>(decision ? decision->team.shared.tick.value : 0U),
        unsigned(decision ? decision->rejection : RuntimeRejectReason::None),
        runtimeNavigationResultName(nav.result),runtimeNavigationReasonName(nav.reason),
        unsigned(nav.map.value),
        static_cast<unsigned long long>(nav.round.value),
        static_cast<unsigned long long>(nav.tick.value),
        static_cast<unsigned long long>(nav.decisionTick.value));
    line(lineBuffer);
}

void ConsoleDebug::lifecycleSink(const debug::LifecycleTrace& trace) noexcept {
    instance().lifecycleTrace(trace);
}

void ConsoleDebug::fakeClientSink(const debug::FakeClientTrace& trace) noexcept {
    instance().fakeClientTrace(trace);
}

void ConsoleDebug::joinSink(const debug::JoinTrace& trace) noexcept {
    instance().joinTrace(trace);
}

void ConsoleDebug::removalSink(const debug::RemovalTrace& trace) noexcept {
    instance().removalTrace(trace);
}

void ConsoleDebug::movementSink(const debug::MovementTrace& trace) noexcept {
    instance().movementTrace(trace);
}

} // namespace astrabot::adapter::metamod
