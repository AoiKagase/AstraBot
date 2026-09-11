// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include "adapter/metamod/console_debug.hpp"

#include "adapter/cstrike/join_state.hpp"
#include "adapter/metamod/lifecycle.hpp"

#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>

#ifdef snprintf
#undef snprintf
#endif

namespace astrabot::adapter::metamod {
namespace {

const char* runtimeFireReasonName(core::combat::CombatReason reason) noexcept {
    switch (reason) {
    case core::combat::CombatReason::None: return "None";
    case core::combat::CombatReason::Accepted: return "Accepted";
    case core::combat::CombatReason::InvalidInput: return "InvalidInput";
    case core::combat::CombatReason::InvalidActor: return "InvalidActor";
    case core::combat::CombatReason::InvalidMap: return "InvalidMap";
    case core::combat::CombatReason::InvalidRound: return "InvalidRound";
    case core::combat::CombatReason::InvalidTick: return "InvalidTick";
    case core::combat::CombatReason::InvalidWorldSnapshot: return "InvalidWorldSnapshot";
    case core::combat::CombatReason::StaleInput: return "StaleInput";
    case core::combat::CombatReason::StaleWeapon: return "StaleWeapon";
    case core::combat::CombatReason::NonFinitePose: return "NonFinitePose";
    case core::combat::CombatReason::ViewOutOfRange: return "ViewOutOfRange";
    case core::combat::CombatReason::InvalidWeapon: return "InvalidWeapon";
    case core::combat::CombatReason::ImpossibleAmmo: return "ImpossibleAmmo";
    case core::combat::CombatReason::InvalidDifficulty: return "InvalidDifficulty";
    case core::combat::CombatReason::Dead: return "Dead";
    case core::combat::CombatReason::NoTarget: return "NoTarget";
    case core::combat::CombatReason::UnknownRelation: return "UnknownRelation";
    case core::combat::CombatReason::Ally: return "Ally";
    case core::combat::CombatReason::StaleTarget: return "StaleTarget";
    case core::combat::CombatReason::AnonymousSound: return "AnonymousSound";
    case core::combat::CombatReason::UnsupportedFireMode: return "UnsupportedFireMode";
    case core::combat::CombatReason::NoUsableWeapon: return "NoUsableWeapon";
    case core::combat::CombatReason::Reloading: return "Reloading";
    case core::combat::CombatReason::EmptyClip: return "EmptyClip";
    case core::combat::CombatReason::Cooldown: return "Cooldown";
    case core::combat::CombatReason::ReactionDelay: return "ReactionDelay";
    case core::combat::CombatReason::HostRejected: return "HostRejected";
    case core::combat::CombatReason::InvalidVisibility: return "InvalidVisibility";
    case core::combat::CombatReason::DuplicateAttack: return "DuplicateAttack";
    case core::combat::CombatReason::DuplicateAction: return "DuplicateAction";
    }
    return "Unknown";
}

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

const char* runtimeInputValidationReasonName(
    RuntimeInputValidationReason reason) noexcept {
	switch (reason) {
	case RuntimeInputValidationReason::None: return "None";
	case RuntimeInputValidationReason::InvalidFrame: return "InvalidFrame";
	case RuntimeInputValidationReason::InvalidActor: return "InvalidActor";
	case RuntimeInputValidationReason::InvalidAgent: return "InvalidAgent";
	case RuntimeInputValidationReason::TeamStampMismatch: return "TeamStampMismatch";
	case RuntimeInputValidationReason::InvalidTeam: return "InvalidTeam";
	case RuntimeInputValidationReason::WorldStampMismatch: return "WorldStampMismatch";
	case RuntimeInputValidationReason::MissingWorld: return "MissingWorld";
	case RuntimeInputValidationReason::ActionStampMismatch: return "ActionStampMismatch";
	case RuntimeInputValidationReason::ActionIdentityMismatch: return "ActionIdentityMismatch";
	case RuntimeInputValidationReason::InvalidAction: return "InvalidAction";
	case RuntimeInputValidationReason::CombatStampMismatch: return "CombatStampMismatch";
	case RuntimeInputValidationReason::CombatIdentityMismatch:
		return "CombatIdentityMismatch";
	case RuntimeInputValidationReason::InvalidCombat: return "InvalidCombat";
	case RuntimeInputValidationReason::TacticalStampMismatch:
		return "TacticalStampMismatch";
	case RuntimeInputValidationReason::TacticalIdentityMismatch:
		return "TacticalIdentityMismatch";
	case RuntimeInputValidationReason::InvalidTacticalContext:
		return "InvalidTacticalContext";
	case RuntimeInputValidationReason::InvalidOptionalObservation:
		return "InvalidOptionalObservation";
	case RuntimeInputValidationReason::InvalidExperienceEvent:
		return "InvalidExperienceEvent";
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
    debugLevel_ = 0;
    nextBotOrdinal_ = 1;
    lastMovementLogCall_.fill(0);
    lastMovementSource_.fill(debug::MovementTraceSource::None);
    lastMovementOutcome_.fill(debug::MovementTraceOutcome::None);
    lastMovementError_.fill(debug::MovementTraceError::None);
    lastMovementMap_.fill({});
    lastMovementPlayer_.fill({});
    lastMovementAgent_.fill({});
    physicalWindowUs_.fill(0);
    physicalDispatches_.fill(0);
    physicalNonZeroInputs_.fill(0);
    physicalSuppressed_.fill(0);
    physicalWindowActive_.fill(false);
    physicalStartX_.fill(0.0F);
    physicalStartY_.fill(0.0F);
    physicalStartZ_.fill(0.0F);
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
        self.commandLine(self.debugLevel_ != 0
                             ? "[ASTRABOT][DEBUG][COMMAND] state=on"
                             : "[ASTRABOT][DEBUG][COMMAND] state=off");
        return;
    }
    if (argc != 2) {
        self.commandLine(
            "[ASTRABOT][DEBUG][COMMAND] error=InvalidArguments expected=0|1|2");
        return;
    }

    const char* value = self.engine_->pfnCmd_Argv(1);
    if (value == nullptr || (std::strcmp(value, "0") != 0 &&
                             std::strcmp(value, "1") != 0 &&
                             std::strcmp(value, "2") != 0)) {
        self.commandLine(
            "[ASTRABOT][DEBUG][COMMAND] error=InvalidArguments expected=0|1|2");
        return;
    }

    self.debugLevel_ = static_cast<std::uint32_t>(value[0] - '0');
    if (self.debugLevel_ == 0) {
        self.commandLine("[ASTRABOT][DEBUG][COMMAND] enabled=0");
    } else if (self.debugLevel_ == 1) {
        self.commandLine("[ASTRABOT][DEBUG][COMMAND] enabled=1");
    } else {
        self.commandLine("[ASTRABOT][DEBUG][COMMAND] enabled=2 nav=1");
    }
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
    if (debugLevel_ == 0 || text == nullptr || utility_ == nullptr ||
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
    char lineBuffer[512]{};
    std::snprintf(
        lineBuffer,
        sizeof(lineBuffer),
        "[ASTRABOT][DEBUG][JOIN] phase=%s error=%s map=%u player=%u:%u team=%s class=%u tick=%llu seq=%llu attempts=%u accepted=%u changed=%u observed_team=%u model=%d team_info=%u class_done=%u post_class_frame=%u entity=%u alive=%u",
        joinPhaseName(trace.phase), joinErrorName(trace.error),
        unsigned(trace.map.value), unsigned(trace.player.slot),
        unsigned(trace.player.generation.value), teamName(trace.team),
        unsigned(trace.classNumber), static_cast<unsigned long long>(trace.tick.value),
        static_cast<unsigned long long>(trace.sequence), unsigned(trace.attempts),
        unsigned(trace.accepted), unsigned(trace.changed), unsigned(trace.observedTeam),
        int(trace.modelIndex), unsigned(trace.teamInfoReceived),
        unsigned(trace.classSelectionCompleted), unsigned(trace.postClassFrameAdvanced),
        unsigned(trace.entityPresent), unsigned(trace.alive));
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

void ConsoleDebug::runtimeCorrelationTrace(core::PlayerId player) noexcept {
    if (debugLevel_ == 0 || !player.isValid() ||
        player.slot > host::kMaxClientSlots || lifecycle_ == nullptr) {
        return;
    }

    const auto& correlation = lifecycle_->runtimeCorrelation(player);
    const auto& input = lifecycle_->runtimeInputBuildStatus(player);
    const bool inputStampMatch = correlation.player == player &&
        correlation.agent.isValid() && correlation.inputTick.isValid() &&
        input.player == player && input.agent == correlation.agent &&
        input.map == correlation.map && input.round == correlation.round &&
        input.tick == correlation.inputTick;
    const auto* candidate = lifecycle_->runtimeOrchestrator().decision(player);
    const bool decisionStampMatch = inputStampMatch && candidate != nullptr &&
        candidate->player == player && candidate->agent == correlation.agent &&
        candidate->team.shared.map == correlation.map &&
        candidate->team.shared.round == correlation.round &&
        candidate->team.shared.tick == correlation.inputTick;
    const auto& result = lifecycle_->runtimeResult();
    const auto decisionMap = decisionStampMatch ? candidate->team.shared.map : core::MapGeneration{};
    const auto decisionRound = decisionStampMatch ? candidate->team.shared.round : core::perception::RoundGeneration{};
    const auto decisionTick = decisionStampMatch ? candidate->team.shared.tick : core::TickId{};
    const auto decisionReject = decisionStampMatch ? candidate->rejection : RuntimeRejectReason::None;
    // This snapshot exposes perception and authorization even when NAV emits
    // no command. Queue/dispatch remain separate evidence in the correlation.
    if (decisionStampMatch) {
        const auto& combat = candidate->combat;
        char combatLine[768]{};
        std::snprintf(combatLine, sizeof(combatLine),
            "[ASTRABOT][DEBUG][COMBAT] kind=Runtime map=%u round=%llu tick=%llu actor=%u:%u agent=%u known_enemies=%zu vision_memories=%zu target=%u:%u source=%u age_us=%llu confidence=%.3f action=%u fire_reason=%s attack_authorized=%u executable=%u tactical_ran=%u action_ran=%u view_pitch=%.2f view_yaw=%.2f",
            unsigned(correlation.map.value),
            static_cast<unsigned long long>(correlation.round.value),
            static_cast<unsigned long long>(correlation.inputTick.value),
            unsigned(player.slot), unsigned(player.generation.value),
            unsigned(correlation.agent.value), candidate->knownEnemyCount,
            candidate->directEnemyCount, unsigned(combat.target.slot),
            unsigned(combat.target.generation.value), unsigned(combat.source),
            static_cast<unsigned long long>(combat.targetAgeMicros), combat.confidence,
            unsigned(combat.action), runtimeFireReasonName(combat.reason),
            unsigned(combat.hasAttackInput()), unsigned(candidate->executable),
            unsigned(candidate->tacticalExecuted), unsigned(candidate->actionExecuted),
            static_cast<double>(combat.view.pitch), static_cast<double>(combat.view.yaw));
        line(combatLine);
    }
    if (const auto* health = lifecycle_->runtimeHealth(player)) {
        char healthLine[512]{};
        std::snprintf(healthLine, sizeof(healthLine),
            "[ASTRABOT][DEBUG][COMBAT] kind=SelfObservation map=%u round=%llu tick=%llu actor=%u:%u agent=%u serial=%d health=%.2f dead=%u health_loss_observed=%.2f deaths_observed=%llu respawns_observed=%llu attribution=Unknown",
            unsigned(health->frame.map.value),
            static_cast<unsigned long long>(health->frame.round.value),
            static_cast<unsigned long long>(health->frame.tick.value),
            unsigned(player.slot), unsigned(player.generation.value),
            unsigned(health->agent.value), health->serial, double(health->health),
            unsigned(health->dead), health->observedHealthLoss,
            static_cast<unsigned long long>(health->deaths),
            static_cast<unsigned long long>(health->respawns));
        line(healthLine);
    }
    const auto intent = decisionStampMatch ? correlation.intent : core::tactical::IntentType::None;
    const auto route = decisionStampMatch ? correlation.route : core::tactical::RouteStyle::None;
    const auto reason = decisionStampMatch ? correlation.reason : core::tactical::Reason::None;
    const auto goal = decisionStampMatch ? correlation.roamGoal : nav::model::NavAreaId{};
    const auto navResult = inputStampMatch ? correlation.navResult : cstrike::RuntimeNavigationApplyResult::None;
    const auto navReason = inputStampMatch ? correlation.navReason : cstrike::RuntimeNavigationApplyReason::None;
    const auto queueOutcome = inputStampMatch ? correlation.queueOutcome : MovementOutcome::None;
    const auto queueError = inputStampMatch ? correlation.queueError : MovementError::None;
    const auto dispatchOutcome = inputStampMatch ? correlation.dispatchOutcome : MovementOutcome::None;
    const auto dispatchError = inputStampMatch ? correlation.dispatchError : MovementError::None;
    const auto* entity = lifecycle_->entityFor(player);
    const double originX = entity != nullptr ? entity->v.origin.x : 0.0;
    const double originY = entity != nullptr ? entity->v.origin.y : 0.0;
    const double originZ = entity != nullptr ? entity->v.origin.z : 0.0;
    const double velocityX = entity != nullptr ? entity->v.velocity.x : 0.0;
    const double velocityY = entity != nullptr ? entity->v.velocity.y : 0.0;
    const double velocityZ = entity != nullptr ? entity->v.velocity.z : 0.0;
    const unsigned onGround = entity != nullptr && (entity->v.flags & FL_ONGROUND) ? 1U : 0U;
    char lineBuffer[2048]{};
    std::snprintf(
        lineBuffer, sizeof(lineBuffer),
        "[ASTRABOT][DEBUG][MOVEMENT] kind=Correlation correlated=%u map=%u round=%llu tick=%llu actor=%u:%u agent=%u input_reason=%s stale=%s validation=%s current_area_held=%u current_area=%u elapsed_us=%llu frame_delta_us=%llu weapon=%u weapon_class=%u update_client_data=%u weapon_data=%u update_called=%u weapon_called=%u accepted=%zu decision=%u decision_map=%u decision_round=%llu decision_tick=%llu runtime_reject=%u nav=%s nav_reason=%s nav_map=%u nav_round=%llu nav_tick=%llu nav_decision_tick=%llu queue=%u queue_error=%u queue_tick=%llu dispatch=%u dispatch_error=%u dispatch_command_tick=%llu dispatch_tick=%llu source=%s command_f=%.3f command_s=%.3f command_u=%.3f buttons=%u impulse=%u msec=%u origin=%.2f,%.2f,%.2f velocity=%.2f,%.2f,%.2f onground=%u intent=%s route=%s reason=%s roam_goal=%u roam_candidates=%zu roam_generation=%llu",
        unsigned(inputStampMatch),
        unsigned(correlation.map.value),
        static_cast<unsigned long long>(correlation.round.value),
        static_cast<unsigned long long>(correlation.inputTick.value),
        unsigned(player.slot), unsigned(player.generation.value),
        unsigned(correlation.agent.value),
        runtimeInputReasonName(inputStampMatch ? correlation.inputReason : RuntimeInputBuildReason::None),
        runtimeActorStaleReasonName(inputStampMatch ? correlation.staleReason : RuntimeActorStaleReason::None),
        runtimeInputValidationReasonName(inputStampMatch ? correlation.validation : RuntimeInputValidationReason::None),
        unsigned(inputStampMatch && correlation.currentAreaHeld),
        unsigned(inputStampMatch ? correlation.currentArea.value : 0U),
        static_cast<unsigned long long>(inputStampMatch ? correlation.elapsedUs : 0U),
        static_cast<unsigned long long>(inputStampMatch ? correlation.frameDeltaUs : 0U),
        unsigned(inputStampMatch ? input.activeWeapon : 0U),
        unsigned(inputStampMatch ? input.activeClass : core::combat::WeaponSnapshot::WeaponClass::Unknown),
        unsigned(inputStampMatch && input.updateClientDataAvailable),
        unsigned(inputStampMatch && input.weaponDataAvailable),
        unsigned(inputStampMatch && input.updateClientDataCalled),
        unsigned(inputStampMatch && input.weaponDataCalled),
        inputStampMatch ? result.acceptedActorCount : 0U,
        unsigned(decisionStampMatch), unsigned(decisionMap.value),
        static_cast<unsigned long long>(decisionRound.value),
        static_cast<unsigned long long>(decisionTick.value),
        unsigned(decisionReject), runtimeNavigationResultName(navResult),
        runtimeNavigationReasonName(navReason), unsigned(inputStampMatch ? correlation.map.value : 0U),
        static_cast<unsigned long long>(inputStampMatch ? correlation.round.value : 0U),
        static_cast<unsigned long long>(inputStampMatch ? correlation.inputTick.value : 0U),
        static_cast<unsigned long long>(inputStampMatch ? correlation.decisionTick.value : 0U),
        unsigned(queueOutcome), unsigned(queueError),
        static_cast<unsigned long long>(inputStampMatch ? correlation.queueTick.value : 0U),
        unsigned(dispatchOutcome), unsigned(dispatchError),
        static_cast<unsigned long long>(inputStampMatch ? correlation.dispatchCommandTick.value : 0U),
        static_cast<unsigned long long>(inputStampMatch ? correlation.dispatchTick.value : 0U),
        movementSourceName(inputStampMatch ? correlation.source : debug::MovementTraceSource::None),
        inputStampMatch ? double(correlation.forward) : 0.0,
        inputStampMatch ? double(correlation.side) : 0.0,
        inputStampMatch ? double(correlation.up) : 0.0,
        unsigned(inputStampMatch ? correlation.buttons : 0U),
        unsigned(inputStampMatch ? correlation.impulse : 0U),
        unsigned(inputStampMatch ? correlation.msec : 0U),
        originX, originY, originZ, velocityX, velocityY, velocityZ, onGround,
        core::tactical::intentName(intent), core::tactical::routeStyleName(route),
        core::tactical::reasonName(reason), unsigned(goal.value),
        inputStampMatch ? correlation.roamCandidateCount : 0U,
        static_cast<unsigned long long>(inputStampMatch ? correlation.roamGeneration : 0U));
    line(lineBuffer);
    if (inputStampMatch && correlation.roamCandidateCount == 0U) {
        char filterLine[512]{};
        std::snprintf(filterLine, sizeof(filterLine),
            "[ASTRABOT][DEBUG][NAV] kind=RoamCandidateFilter actor=%u:%u candidates=0 capacity=%u invalid=%u occupied=%u cooling=%u rejected=%u recent=%u missing=%u hull=%u",
            unsigned(player.slot), unsigned(player.generation.value),
            correlation.roamExcludedCapacity, correlation.roamExcludedInvalid,
            correlation.roamExcludedOccupied, correlation.roamExcludedCooling,
            correlation.roamExcludedRejected, correlation.roamExcludedRecent,
            correlation.roamExcludedMissing, correlation.roamExcludedHull);
        line(filterLine);
    }
    if (inputStampMatch) {
        const auto count=(std::min)(correlation.roamExclusionCount,
                                    correlation.roamExclusionSamples.size());
        for (std::size_t i=0; i<count; ++i) {
            const auto& sample=correlation.roamExclusionSamples[i];
            char sampleLine[384]{};
            std::snprintf(
                sampleLine, sizeof(sampleLine),
                "[ASTRABOT][DEBUG][NAV] kind=RoamCandidateExclusion actor=%u:%u sample=%u area=%u reason=%u remaining_us=%llu owner=%u:%u",
                unsigned(player.slot), unsigned(player.generation.value),
                unsigned(i), unsigned(sample.area.value), unsigned(sample.reason),
                static_cast<unsigned long long>(sample.remainingUs),
                unsigned(sample.owner.slot), unsigned(sample.owner.generation.value));
            line(sampleLine);
        }
    }
}

void ConsoleDebug::movementTrace(const debug::MovementTrace& trace) noexcept {
    if(debugLevel_ == 0 || !trace.player.isValid() ||
       trace.player.slot>host::kMaxClientSlots || lifecycle_==nullptr) return;
    const auto index=trace.player.slot-1U;
    auto& last=lastMovementLogCall_[index];
    auto& lastSource=lastMovementSource_[index];
    auto& lastOutcome=lastMovementOutcome_[index];
    auto& lastError=lastMovementError_[index];
    const bool sourceChanged=trace.source!=lastSource;
    if(!trace.engineCall && trace.outcome != debug::MovementTraceOutcome::Rejected) return;
    const bool actorChanged = trace.map != lastMovementMap_[index] ||
        trace.player != lastMovementPlayer_[index] || trace.agent != lastMovementAgent_[index];
    if (actorChanged) {
        physicalWindowUs_[index] = 0;
        physicalDispatches_[index] = 0;
        physicalNonZeroInputs_[index] = 0;
        physicalSuppressed_[index] = 0;
        physicalWindowActive_[index] = false;
    }
    if (debugLevel_ >= 2 && trace.engineCall && trace.physical.valid) {
        if (physicalWindowActive_[index])
            ++physicalSuppressed_[index];
        else {
            physicalWindowActive_[index] = true;
            physicalStartX_[index] = trace.physical.beforeOriginX;
            physicalStartY_[index] = trace.physical.beforeOriginY;
            physicalStartZ_[index] = trace.physical.beforeOriginZ;
        }
        physicalWindowUs_[index] += trace.frameDeltaUs;
        ++physicalDispatches_[index];
        if (trace.forward != 0.0F || trace.side != 0.0F || trace.up != 0.0F ||
            trace.buttons != 0U)
            ++physicalNonZeroInputs_[index];
    }
    const bool physicalReady = debugLevel_ >= 2 && trace.engineCall &&
        trace.physical.valid && physicalWindowUs_[index] >= 1'000'000U;
    const bool rejectionChanged = trace.outcome == debug::MovementTraceOutcome::Rejected &&
        (trace.outcome != lastOutcome || trace.error != lastError || sourceChanged || actorChanged);
    if(trace.outcome == debug::MovementTraceOutcome::Rejected && !rejectionChanged) return;
    if(!sourceChanged && trace.outcome != debug::MovementTraceOutcome::Rejected &&
       trace.callCount!=0 && trace.callCount!=1 && trace.callCount<last+512 &&
       !physicalReady) return;
    last=trace.callCount;
    lastSource=trace.source;
    lastOutcome=trace.outcome;
    lastError=trace.error;
    lastMovementMap_[index]=trace.map;
    lastMovementPlayer_[index]=trace.player;
    lastMovementAgent_[index]=trace.agent;
    const auto* entity=lifecycle_->entityFor(trace.player);
    const auto* join=lifecycle_->joinState(trace.player);
    const auto& runtime=lifecycle_->runtimeInputBuildStatus(trace.player);
    const auto& correlation=lifecycle_->runtimeCorrelation(trace.player);
    const bool correlationMatch=correlation.player==trace.player &&
        correlation.agent==trace.agent && correlation.map==trace.map &&
        correlation.round==lifecycle_->round() && correlation.inputTick.isValid() &&
        correlation.inputTick==trace.commandTick;
    const bool runtimeStatusMatch=correlationMatch && runtime.player==trace.player &&
        runtime.agent==trace.agent && runtime.map==correlation.map &&
        runtime.round==correlation.round && runtime.tick==correlation.inputTick;
    const auto runtimeMap=correlationMatch ? correlation.map : core::MapGeneration{};
    const auto runtimeRound=correlationMatch ? correlation.round : core::perception::RoundGeneration{};
    const auto runtimeTick=correlationMatch ? correlation.inputTick : core::TickId{};
    const auto runtimePlayer=correlationMatch ? correlation.player : core::PlayerId{};
    const auto runtimeAgent=correlationMatch ? correlation.agent : core::BotAgentId{};
    const auto runtimeReason=correlationMatch ? correlation.inputReason : RuntimeInputBuildReason::None;
    const auto runtimeStale=correlationMatch ? correlation.staleReason : RuntimeActorStaleReason::None;
    const auto runtimeValidation=correlationMatch ? correlation.validation : RuntimeInputValidationReason::None;
    const bool runtimeHeldArea=runtimeStatusMatch && correlation.currentAreaHeld;
    const auto binding=lifecycle_->agents().findByPlayer(trace.player);
    const bool managed=binding.isValid() && binding.player==trace.player && binding.map==trace.map;
    const bool connected=lifecycle_->registry().isConnected(trace.player.slot) && lifecycle_->registry().currentPlayer(trace.player.slot)==trace.player;
    const bool removal=lifecycle_->removalPending(trace.player);
    const auto* latestDecision=lifecycle_->runtimeOrchestrator().decision(trace.player);
    const bool decisionMatch=correlationMatch && latestDecision!=nullptr &&
        latestDecision->player==correlation.player &&
        latestDecision->agent==correlation.agent &&
        latestDecision->team.shared.map==correlation.map &&
        latestDecision->team.shared.round==correlation.round &&
        latestDecision->team.shared.tick==correlation.inputTick;
    const auto* decision=decisionMatch ? latestDecision : nullptr;
    const auto navResult=correlationMatch ? correlation.navResult : cstrike::RuntimeNavigationApplyResult::None;
    const auto navReason=correlationMatch ? correlation.navReason : cstrike::RuntimeNavigationApplyReason::None;
    const auto navMap=correlationMatch ? correlation.map : core::MapGeneration{};
    const auto navRound=correlationMatch ? correlation.round : core::perception::RoundGeneration{};
    const auto navTick=correlationMatch ? correlation.inputTick : core::TickId{};
    const auto navDecisionTick=correlationMatch ? correlation.decisionTick : core::TickId{};
    const bool inputMatch=runtimePlayer==trace.player && runtimeAgent==trace.agent;
    const bool spectator=entity && (entity->v.iuser1!=0 || (entity->v.flags&FL_SPECTATOR));
    const bool spawned=entity && entity->v.deadflag==DEAD_NO && entity->v.health>0 && !spectator;
    char lineBuffer[2048]{};
    std::snprintf(lineBuffer,sizeof(lineBuffer),
        "[ASTRABOT][DEBUG][MOVEMENT] map=%u round=%llu actor=%u:%u agent=%u outcome=%u error=%u input_tick=%llu dispatch_tick=%llu calls=%llu source=%s msec=%u serial=%u command_f=%.3f command_s=%.3f command_u=%.3f buttons=%u impulse=%u managed=%u connected=%u removal=%u phase=%s spawned=%u x=%.3f y=%.3f z=%.3f vx=%.3f vy=%.3f vz=%.3f movetype=%d solid=%d onground=%u spectator=%u runtime_map=%u runtime_round=%llu runtime_tick=%llu input_actor=%u:%u input_agent=%u input_match=%u runtime_validation=%s runtime=%s stale=%s held_area=%u weapon=%u weapon_class=%u decision_map=%u decision_round=%llu decision_tick=%llu runtime_reject=%u nav=%s nav_reason=%s nav_map=%u nav_round=%llu nav_tick=%llu nav_decision_tick=%llu accepted=%zu nonprimary_rejected=%zu intent=%s route=%s reason=%s roam_goal=%u roam_candidates=%zu roam_generation=%llu",
        unsigned(trace.map.value),
        static_cast<unsigned long long>(lifecycle_->round().value),
        unsigned(trace.player.slot),unsigned(trace.player.generation.value),
        unsigned(trace.agent.value),unsigned(trace.outcome),unsigned(trace.error),
        static_cast<unsigned long long>(trace.commandTick.value),
        static_cast<unsigned long long>(trace.dispatchTick.value),
        static_cast<unsigned long long>(trace.callCount),movementSourceName(trace.source),
        unsigned(trace.engineMsec),unsigned(trace.edictSerial),
        double(trace.forward),double(trace.side),double(trace.up),
        unsigned(trace.buttons),unsigned(trace.impulse),unsigned(managed),
        unsigned(connected),unsigned(removal),join ? joinPhaseName(join->phase()):"None",unsigned(spawned),
        entity ? double(entity->v.origin.x):0.0,entity ? double(entity->v.origin.y):0.0,
        entity ? double(entity->v.origin.z):0.0,entity ? double(entity->v.velocity.x):0.0,
        entity ? double(entity->v.velocity.y):0.0,entity ? double(entity->v.velocity.z):0.0,
        entity ? entity->v.movetype:0,entity ? entity->v.solid:0,
        unsigned(entity && (entity->v.flags&FL_ONGROUND)),unsigned(spectator),
        unsigned(runtimeMap.value),
        static_cast<unsigned long long>(runtimeRound.value),
        static_cast<unsigned long long>(runtimeTick.value),
        unsigned(runtimePlayer.slot),unsigned(runtimePlayer.generation.value),
        unsigned(runtimeAgent.value),unsigned(inputMatch),
        runtimeInputValidationReasonName(runtimeValidation),
        runtimeInputReasonName(runtimeReason),runtimeActorStaleReasonName(runtimeStale),
        unsigned(runtimeHeldArea),unsigned(runtimeStatusMatch ? runtime.activeWeapon : 0U),
        unsigned(runtimeStatusMatch ? runtime.activeClass : core::combat::WeaponSnapshot::WeaponClass::Unknown),
        unsigned(decision ? decision->team.shared.map.value : 0U),
        static_cast<unsigned long long>(decision ? decision->team.shared.round.value : 0U),
        static_cast<unsigned long long>(decision ? decision->team.shared.tick.value : 0U),
        unsigned(decision ? decision->rejection : RuntimeRejectReason::None),
        runtimeNavigationResultName(navResult),runtimeNavigationReasonName(navReason),
        unsigned(navMap.value),
        static_cast<unsigned long long>(navRound.value),
        static_cast<unsigned long long>(navTick.value),
        static_cast<unsigned long long>(navDecisionTick.value),
        correlationMatch ? lifecycle_->runtimeResult().acceptedActorCount : 0U,
        correlationMatch ? lifecycle_->runtimeResult().nonPrimaryRejectedCount : 0U,
        core::tactical::intentName(decision ? decision->tactical.intent.type :
                                   core::tactical::IntentType::None),
        core::tactical::routeStyleName(decision ? decision->tactical.intent.route :
                                       core::tactical::RouteStyle::None),
        core::tactical::reasonName(decision ? decision->tactical.intent.reason :
                                   core::tactical::Reason::None),
        unsigned(decision && decision->tactical.intent.type ==
                         core::tactical::IntentType::Roam
                     ? decision->tactical.intent.target.area.value
                     : 0U),
        decision ? decision->roamCandidateCount : 0U,
        static_cast<unsigned long long>(decision ? decision->roamGeneration : 0U));
    line(lineBuffer);
    if (debugLevel_ >= 2 && trace.engineCall && trace.physical.valid &&
        physicalWindowUs_[index] >= 1'000'000U) {
        const double dx = static_cast<double>(trace.physical.afterOriginX) -
            static_cast<double>(physicalStartX_[index]);
        const double dy = static_cast<double>(trace.physical.afterOriginY) -
            static_cast<double>(physicalStartY_[index]);
        const double dz = static_cast<double>(trace.physical.afterOriginZ) -
            static_cast<double>(physicalStartZ_[index]);
        const double horizontal = std::sqrt(dx * dx + dy * dy);
        const bool noProgress = physicalNonZeroInputs_[index] != 0U &&
            horizontal < 1.0;
        char physicalLine[1024]{};
        std::snprintf(
            physicalLine,
            sizeof(physicalLine),
            "[ASTRABOT][DEBUG][MOVEMENT] kind=DispatchObservation map=%u actor=%u:%u agent=%u sequence=%llu command_tick=%llu dispatch_tick=%llu window_us=%llu dispatches=%llu nonzero_inputs=%llu suppressed=%llu physical_valid=%u before_origin=%.3f,%.3f,%.3f after_origin=%.3f,%.3f,%.3f displacement=%.3f,%.3f,%.3f horizontal_displacement=%.3f no_progress=%u before_velocity=%.3f,%.3f,%.3f after_velocity=%.3f,%.3f,%.3f onground=%u->%u source=%s command=%.3f,%.3f,%.3f buttons=%u msec=%u",
            unsigned(trace.map.value), unsigned(trace.player.slot),
            unsigned(trace.player.generation.value), unsigned(trace.agent.value),
            static_cast<unsigned long long>(trace.callCount),
            static_cast<unsigned long long>(trace.commandTick.value),
            static_cast<unsigned long long>(trace.dispatchTick.value),
            static_cast<unsigned long long>(physicalWindowUs_[index]),
            static_cast<unsigned long long>(physicalDispatches_[index]),
            static_cast<unsigned long long>(physicalNonZeroInputs_[index]),
            static_cast<unsigned long long>(physicalSuppressed_[index]),
            1U,
            static_cast<float>(physicalStartX_[index]),
            static_cast<float>(physicalStartY_[index]),
            static_cast<float>(physicalStartZ_[index]),
            trace.physical.afterOriginX, trace.physical.afterOriginY,
            trace.physical.afterOriginZ, dx, dy, dz, horizontal,
            unsigned(noProgress), trace.physical.beforeVelocityX,
            trace.physical.beforeVelocityY, trace.physical.beforeVelocityZ,
            trace.physical.afterVelocityX, trace.physical.afterVelocityY,
            trace.physical.afterVelocityZ, unsigned(trace.physical.beforeOnGround),
            unsigned(trace.physical.afterOnGround), movementSourceName(trace.source),
            double(trace.forward), double(trace.side), double(trace.up),
            unsigned(trace.buttons), unsigned(trace.engineMsec));
        line(physicalLine);
        physicalWindowUs_[index] = 0;
        physicalDispatches_[index] = 0;
        physicalNonZeroInputs_[index] = 0;
        physicalSuppressed_[index] = 0;
        physicalWindowActive_[index] = false;
    }
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
