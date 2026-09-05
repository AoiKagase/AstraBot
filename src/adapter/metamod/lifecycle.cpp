// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include "adapter/metamod/lifecycle.hpp"

#include <cstdint>

namespace astrabot::adapter::metamod {
namespace {

LifecycleCoordinator gCoordinator{};

void markIgnored() noexcept {
    if (gpMetaGlobals != nullptr) {
        gpMetaGlobals->mres = MRES_IGNORED;
    }
}

void copyCommandWord(
    std::array<char, 16>& destination,
    const char* source) noexcept {
    destination = {};
    if (source == nullptr) {
        return;
    }
    std::uint16_t index = 0;
    while (index + 1U < destination.size() && source[index] != '\0') {
        destination[index] = source[index];
        ++index;
    }
}

struct CommandContextGuard final {
    bool& active;
    ~CommandContextGuard() { active = false; }
};

} // namespace

void LifecycleCoordinator::configure(
    enginefuncs_t* engineFunctions,
    mutil_funcs_t* utilityFunctions,
    DLL_FUNCTIONS* gameDllFunctions,
    cstrike::UserMessageIds userMessageIds) noexcept {
    engineFunctions_ = engineFunctions;
    utilityFunctions_ = utilityFunctions;
    gameDllFunctions_ = gameDllFunctions;
    messageDecoder_.configure(userMessageIds, &LifecycleCoordinator::onMessage, this);
    fakeClient_.configure(
        engineFunctions,
        utilityFunctions,
        gameDllFunctions,
        &registry_,
        &agents_);
    movement_.configure(engineFunctions, &registry_);
    navConsole_.bindMovement(&movement_);
}

void LifecycleCoordinator::reset() noexcept {
    navConsole_.reset();
    const host::PlayerId activePlayer = fakeClient_.activePlayer();
    const bool hadActiveClient = activePlayer.isValid() &&
                                  fakeClient_.activeEntity() != nullptr;
    movement_.reset();
    // Detach must invalidate adapter state before calling the original
    // GameDLL disconnect callback.  The callback must never observe a live
    // join, decoder fragment, command context, or pending cleanup.
    messageDecoder_.reset();
    joinState_.reset();
    activeJoinEntity_ = nullptr;
    commandContextActive_ = false;
    cleanupPending_ = false;
    pendingCleanupError_ = cstrike::JoinError::None;
    commandArgv0_ = {};
    commandArgv1_ = {};
    commandArgs_ = {};
    if (hadActiveClient) {
        const bool mappingPresent = agents_.findByPlayer(activePlayer).isValid();
        (void)agents_.unbind(activePlayer);
        (void)registry_.disconnectPlayer(activePlayer);
        const bool cleaned = fakeClient_.cleanupActiveDirect(true);
        emitRemoval(
            cleaned ? debug::RemovalOutcome::Cleaned
                    : debug::RemovalOutcome::Rejected,
            cleaned ? debug::RemovalError::None
                    : debug::RemovalError::DirectCleanupFailed,
            activePlayer,
            mappingPresent,
            true);
        if (cleaned) {
            ++status_.cleanupCompletions;
        } else {
            status_.lastRemovalError = debug::RemovalError::DirectCleanupFailed;
            fakeClient_.forget(activePlayer);
        }
    }
    fakeClient_.reset();
    agents_.reset();
    registry_.reset();
    engineFunctions_ = nullptr;
    utilityFunctions_ = nullptr;
    gameDllFunctions_ = nullptr;
    status_ = {};
    traceSink_ = nullptr;
    joinTraceSink_ = nullptr;
    removalTraceSink_ = nullptr;
}

void LifecycleCoordinator::serverActivate(int clientMax) noexcept {
    const host::LifecycleResult result =
        clientMax < 1 || clientMax > host::kMaxClientSlots
            ? host::LifecycleResult::rejected(host::HostError::InvalidLifecycle)
            : registry_.activateMap(static_cast<std::uint16_t>(clientMax));
    if (result.changed()) {
        ++status_.mapActivations;
        if (status_.mapActivations > 1U) {
            ++status_.mapReplays;
        }
        movement_.resetMap();
        fakeClient_.queuePrimaryCreate();
    }
    emit(host::LifecycleEventKind::MapActivated, result);
}

void LifecycleCoordinator::serverDeactivate() noexcept {
    navConsole_.invalidate(nav::runtime::SessionReason::MapChanged);
    movement_.resetMap();
    const cstrike::JoinAction joinAction =
        joinState_.cancel(cstrike::JoinError::MapDeactivated);
    handleJoinAction(joinAction);
    joinState_.reset();
    messageDecoder_.reset();
    commandContextActive_ = false;
    cleanupPending_ = false;
    pendingCleanupError_ = cstrike::JoinError::None;
    commandArgv0_ = {};
    commandArgv1_ = {};
    commandArgs_ = {};
    activeJoinEntity_ = nullptr;
    const host::LifecycleResult result = registry_.deactivateMap();
    fakeClient_.resetMap();
    agents_.clearMappings();
    emit(host::LifecycleEventKind::MapDeactivated, result);
}

void LifecycleCoordinator::clientDisconnect(edict_t* entity) noexcept {
    if (entity == nullptr || engineFunctions_ == nullptr ||
        engineFunctions_->pfnIndexOfEdict == nullptr) {
        const host::LifecycleResult result =
            host::LifecycleResult::rejected(host::HostError::InvalidPlayer);
        emit(host::LifecycleEventKind::PlayerDisconnected, result);
        return;
    }

    const int index = engineFunctions_->pfnIndexOfEdict(entity);
    const host::PlayerId activePlayer = fakeClient_.activePlayer();
    const bool wasActiveClient =
        activePlayer.isValid() && index >= 1 &&
        index <= static_cast<int>(host::kMaxClientSlots) &&
        activePlayer.slot == static_cast<std::uint16_t>(index);
    const host::PlayerId disconnectedPlayer =
        index >= 1 && index <= static_cast<int>(host::kMaxClientSlots)
            ? registry_.currentPlayer(static_cast<std::uint16_t>(index))
            : host::PlayerId::invalid();

    // Clear all adapter-owned per-client state before acknowledging the
    // registry transition.  Keep FakeClient's mapping until that transition
    // so acknowledgeDisconnect() remains the single normal cleanup path.
    movement_.forget(disconnectedPlayer);
    if (wasActiveClient) navConsole_.invalidate(nav::runtime::SessionReason::Disconnected);
    const bool joinMatches =
        joinState_.player().isValid() && index >= 1 &&
        joinState_.player().slot == static_cast<std::uint16_t>(index);
    if (joinMatches && joinState_.active()) {
        movement_.forget(joinState_.player());
        const cstrike::JoinAction joinAction =
            joinState_.cancel(cstrike::JoinError::Disconnected);
        emitJoin(joinAction);
    }
    if (joinMatches) {
        joinState_.reset();
        activeJoinEntity_ = nullptr;
    }
    messageDecoder_.reset();
    commandContextActive_ = false;
    cleanupPending_ = false;
    pendingCleanupError_ = cstrike::JoinError::None;
    commandArgv0_ = {};
    commandArgv1_ = {};
    commandArgs_ = {};
    const host::LifecycleResult result =
        index < 1 || index > static_cast<int>(host::kMaxClientSlots)
            ? host::LifecycleResult::rejected(host::HostError::InvalidPlayer)
            : registry_.disconnectSlot(static_cast<std::uint16_t>(index));
    if (result.changed()) {
        agents_.unbind(result.event.player);
        fakeClient_.acknowledgeDisconnect(result.event.player);
        if (activeJoinEntity_ != nullptr &&
            joinState_.player() == result.event.player) {
            activeJoinEntity_ = nullptr;
        }
        if (wasActiveClient) {
            ++status_.cleanupCompletions;
            emitRemoval(
                debug::RemovalOutcome::Cleaned,
                debug::RemovalError::None,
                result.event.player,
                false,
                false);
        }
    }
    emit(host::LifecycleEventKind::PlayerDisconnected, result);
}

RemovalResult LifecycleCoordinator::removeActive() noexcept {
    const host::PlayerId player = fakeClient_.activePlayer();
    if (!player.isValid() || fakeClient_.activeEntity() == nullptr) {
        return {
            debug::RemovalOutcome::NoOp,
            debug::RemovalError::None,
            {},
            true,
            false};
    }
    if (fakeClient_.removalPending()) {
        return {
            debug::RemovalOutcome::NoOp,
            debug::RemovalError::None,
            player,
            true,
            false};
    }

    movement_.forget(player);
    navConsole_.invalidate(nav::runtime::SessionReason::Disconnected);
    if (joinState_.active()) {
        const cstrike::JoinAction action =
            joinState_.cancel(cstrike::JoinError::Disconnected);
        emitJoin(action);
    }
    messageDecoder_.reset();
    commandContextActive_ = false;
    cleanupPending_ = false;
    pendingCleanupError_ = cstrike::JoinError::None;
    commandArgv0_ = {};
    commandArgv1_ = {};
    commandArgs_ = {};

    ++status_.removalRequests;
    const RemovalResult requested = fakeClient_.requestRemoval();
    if (requested.succeeded()) {
        status_.lastRemovalError = debug::RemovalError::None;
        return requested;
    }

    status_.lastRemovalError = requested.error;
    cleanupActiveAfterRemoval(requested, player);
    return requested;
}

void LifecycleCoordinator::queuePrimaryCreate(
    cstrike::JoinRequest request) noexcept {
    fakeClient_.queuePrimaryCreate(request);
}

void LifecycleCoordinator::startFrame() noexcept {
    const host::LifecycleResult result = registry_.startFrame();
    emit(host::LifecycleEventKind::FrameStarted, result);
    if (!result.changed()) {
        return;
    }
    movement_.beginFrame();

    const FakeClientResult fakeClientResult =
        fakeClient_.processPrimaryCreate();
    if (fakeClientResult.changed ||
        fakeClientResult.error != debug::FakeClientError::None) {
        ++status_.createAttempts;
    }
    if (fakeClientResult.playerRegistration.changed()) {
        emit(
            host::LifecycleEventKind::PlayerConnected,
            fakeClientResult.playerRegistration);
    }
    if (fakeClientResult.playerRollback.changed()) {
        emit(
            host::LifecycleEventKind::PlayerDisconnected,
            fakeClientResult.playerRollback);
    }

    if (fakeClientResult.succeeded() && fakeClientResult.changed) {
        (void)requestJoin(fakeClient_.activeJoinRequest());
    }

    const cstrike::JoinAction frameAction =
        joinState_.onFrame(registry_.currentTick());
    handleJoinAction(frameAction);
    if (frameAction.kind == cstrike::JoinActionKind::SendMenuSelect) {
        const bool dispatched = dispatchMenu(frameAction.selection);
        const cstrike::JoinAction completion =
            joinState_.commandCompleted(dispatched);
        handleJoinAction(completion);
    }

    navConsole_.beforeDispatch(*this);
    const auto navTicket=navConsole_.dispatchTicket();
    const auto dispatchTick=registry_.currentTick();
    const auto movementResult=movement_.dispatchAtFrameEnd(
        joinState_.phase(),
        fakeClient_.activePlayer(),
        fakeClient_.activeEntity(),
        registry_.mapGeneration(),
        registry_.currentTick());
    navConsole_.afterDispatch(movementResult,dispatchTick,navTicket);
    navConsole_.moveFrame(*this);
}

MovementResult LifecycleCoordinator::submitCommand(
    core::PlayerId player,
    core::MapGeneration mapGeneration,
    core::TickId tick,
    const core::BotCommand& command) noexcept {
    if (joinState_.phase() != cstrike::JoinPhase::Joined) {
        return movement_.rejectIngress(
            MovementError::NotJoined,
            player,
            mapGeneration,
            tick,
            command.msec);
    }
    if (fakeClient_.activeEntity() == nullptr) {
        return movement_.rejectIngress(
            MovementError::MissingEntity,
            player,
            mapGeneration,
            tick,
            command.msec);
    }
    if (fakeClient_.activePlayer() != player) {
        return movement_.rejectIngress(
            MovementError::MappingMismatch,
            player,
            mapGeneration,
            tick,
            command.msec);
    }
    if (fakeClient_.activeEntity()->v.deadflag != DEAD_NO) {
        return movement_.rejectIngress(
            MovementError::DeadPlayer,
            player,
            mapGeneration,
            tick,
            command.msec);
    }
    return movement_.submit(player, mapGeneration, tick, command);
}

void LifecycleCoordinator::messageBegin(
    int /* messageDestination */,
    int messageType,
    const float* /* origin */,
    edict_t* recipient) noexcept {
    std::uint16_t recipientSlot = 0;
    if (recipient != nullptr && engineFunctions_ != nullptr &&
        engineFunctions_->pfnIndexOfEdict != nullptr) {
        const int index = engineFunctions_->pfnIndexOfEdict(recipient);
        if (index >= 1 && index <= static_cast<int>(host::kMaxClientSlots)) {
            recipientSlot = static_cast<std::uint16_t>(index);
        }
    }
    messageDecoder_.begin(messageType, recipientSlot);
}

void LifecycleCoordinator::messageEnd() noexcept {
    messageDecoder_.end();
}

void LifecycleCoordinator::writeByte(int value) noexcept {
    messageDecoder_.writeByte(value);
}

void LifecycleCoordinator::writeChar(int value) noexcept {
    messageDecoder_.writeChar(value);
}

void LifecycleCoordinator::writeShort(int value) noexcept {
    messageDecoder_.writeShort(value);
}

void LifecycleCoordinator::writeString(const char* value) noexcept {
    messageDecoder_.writeString(value);
}

const char* LifecycleCoordinator::commandArgs() noexcept {
    if (commandContextActive_) {
        if (gpMetaGlobals != nullptr) {
            gpMetaGlobals->mres = MRES_SUPERCEDE;
        }
        return commandArgs_.data();
    }
    markIgnored();
    return "";
}

const char* LifecycleCoordinator::commandArgv(int index) noexcept {
    if (commandContextActive_) {
        if (gpMetaGlobals != nullptr) {
            gpMetaGlobals->mres = MRES_SUPERCEDE;
        }
        if (index == 0) {
            return commandArgv0_.data();
        }
        if (index == 1) {
            return commandArgv1_.data();
        }
        return "";
    }
    markIgnored();
    return "";
}

int LifecycleCoordinator::commandArgc() noexcept {
    if (commandContextActive_) {
        if (gpMetaGlobals != nullptr) {
            gpMetaGlobals->mres = MRES_SUPERCEDE;
        }
        return 2;
    }
    markIgnored();
    return 0;
}

cstrike::JoinAction LifecycleCoordinator::requestJoin(
    cstrike::JoinRequest request) noexcept {
    const host::PlayerId player = fakeClient_.activePlayer();
    activeJoinEntity_ = fakeClient_.activeEntity();
    const cstrike::JoinAction action = joinState_.begin(
        player,
        registry_.mapGeneration(),
        request,
        registry_.currentTick());
    if (action.error == cstrike::JoinError::AlreadyJoining) {
        return action;
    }
    handleJoinAction(action);
    return action;
}

bool LifecycleCoordinator::dispatchMenuForTest(
    std::uint8_t selection) noexcept {
    return dispatchMenu(selection);
}

void LifecycleCoordinator::emit(
    host::LifecycleEventKind attemptedKind,
    const host::LifecycleResult& result,
    host::PlayerId attemptedPlayer,
    host::TickId attemptedTick) noexcept {
    debug::emitLifecycle(
        attemptedKind,
        result,
        registry_.mapGeneration(),
        attemptedPlayer,
        attemptedTick,
        traceSink_);
}

void LifecycleCoordinator::emitJoin(const cstrike::JoinAction& action) noexcept {
    if (!action.changed) {
        return;
    }
    const cstrike::JoinRequest request = joinState_.request();
    debug::emitJoin(
        {
            joinState_.phase(),
            joinState_.error(),
            joinState_.map(),
            joinState_.player(),
            request.team,
            request.classNumber,
            registry_.currentTick(),
            registry_.eventSequence(),
            joinState_.attempts(),
            action.kind != cstrike::JoinActionKind::Failed &&
                action.kind != cstrike::JoinActionKind::Cancelled,
            action.changed,
        },
        joinTraceSink_);
}

void LifecycleCoordinator::handleMessage(
    const cstrike::MessageEvent& event) noexcept {
    const cstrike::JoinAction action =
        joinState_.onMessage(event, registry_.currentTick());
    handleJoinAction(action);
}

void LifecycleCoordinator::handleJoinAction(
    const cstrike::JoinAction& action) noexcept {
    if (!action.changed) {
        return;
    }
    emitJoin(action);
    if (action.kind == cstrike::JoinActionKind::Failed) {
        if (commandContextActive_) {
            cleanupPending_ = true;
            pendingCleanupError_ = action.error;
        } else {
            cleanupFailedJoin(action.error);
        }
    } else if (action.kind == cstrike::JoinActionKind::Cancelled) {
        activeJoinEntity_ = nullptr;
        fakeClient_.forget(joinState_.player());
    }
}

void LifecycleCoordinator::cleanupActiveAfterRemoval(
    const RemovalResult& result,
    host::PlayerId player) noexcept {
    const host::BotAgentBinding mapping = agents_.findByPlayer(player);
    const bool mappingPresent = mapping.isValid();
    (void)agents_.unbind(player);
    (void)registry_.disconnectPlayer(player);
    const bool cleaned = fakeClient_.cleanupActiveDirect(true);
    if (cleaned) {
        ++status_.cleanupCompletions;
        emitRemoval(
            debug::RemovalOutcome::Cleaned,
            result.error,
            player,
            mappingPresent,
            true);
    } else {
        fakeClient_.forget(player);
        status_.lastRemovalError = debug::RemovalError::DirectCleanupFailed;
        emitRemoval(
            debug::RemovalOutcome::Rejected,
            debug::RemovalError::DirectCleanupFailed,
            player,
            mappingPresent,
            true);
    }
    activeJoinEntity_ = nullptr;
    joinState_.reset();
}

void LifecycleCoordinator::emitRemoval(
    debug::RemovalOutcome outcome,
    debug::RemovalError error,
    host::PlayerId player,
    bool mappingPresent,
    bool entityPresent) noexcept {
    debug::emitRemoval(
        {
            outcome,
            error,
            registry_.mapGeneration(),
            player,
            registry_.currentTick(),
            registry_.eventSequence(),
            mappingPresent,
            entityPresent,
        },
        removalTraceSink_);
}

void LifecycleCoordinator::cleanupFailedJoin(
    cstrike::JoinError /* error */) noexcept {
    const host::PlayerId player = joinState_.player();
    if (!player.isValid()) {
        activeJoinEntity_ = nullptr;
        cleanupPending_ = false;
        pendingCleanupError_ = cstrike::JoinError::None;
        return;
    }

    agents_.unbind(player);
    registry_.disconnectPlayer(player);
    fakeClient_.kickAndCleanup(player);
    activeJoinEntity_ = nullptr;
    cleanupPending_ = false;
    pendingCleanupError_ = cstrike::JoinError::None;
}

bool LifecycleCoordinator::dispatchMenu(std::uint8_t selection) noexcept {
    if (commandContextActive_ || activeJoinEntity_ == nullptr ||
        gameDllFunctions_ == nullptr ||
        gameDllFunctions_->pfnClientCommand == nullptr || selection == 0U ||
        selection > 9U) {
        return false;
    }

    copyCommandWord(commandArgv0_, "menuselect");
    commandArgv1_ = {};
    commandArgv1_[0] = static_cast<char>('0' + selection);
    commandArgs_ = {};
    commandArgs_[0] = commandArgv1_[0];
    commandContextActive_ = true;
    {
        CommandContextGuard guard{commandContextActive_};
        gameDllFunctions_->pfnClientCommand(activeJoinEntity_);
    }
    if (cleanupPending_) {
        cleanupFailedJoin(pendingCleanupError_);
    }
    return true;
}

void LifecycleCoordinator::onMessage(
    void* context,
    const cstrike::MessageEvent& event) noexcept {
    if (context != nullptr) {
        static_cast<LifecycleCoordinator*>(context)->handleMessage(event);
    }
}

LifecycleCoordinator& lifecycleCoordinator() noexcept {
    return gCoordinator;
}

void setLifecycleTraceSink(debug::LifecycleTraceSink sink) noexcept {
    gCoordinator.setTraceSink(sink);
}

void serverActivateHook(edict_t* entityList, int edictCount, int clientMax) {
    (void)entityList;
    (void)edictCount;
    gCoordinator.serverActivate(clientMax);
    markIgnored();
}

void serverDeactivateHook() {
    gCoordinator.serverDeactivate();
    markIgnored();
}

void clientDisconnectHook(edict_t* entity) {
    gCoordinator.clientDisconnect(entity);
    markIgnored();
}

void startFrameHook() {
    gCoordinator.startFrame();
    markIgnored();
}

void messageBeginHook(
    int messageDestination,
    int messageType,
    const float* origin,
    edict_t* recipient) {
    gCoordinator.messageBegin(messageDestination, messageType, origin, recipient);
    markIgnored();
}

void messageEndHook() {
    gCoordinator.messageEnd();
    markIgnored();
}

void writeByteHook(int value) {
    gCoordinator.writeByte(value);
    markIgnored();
}

void writeCharHook(int value) {
    gCoordinator.writeChar(value);
    markIgnored();
}

void writeShortHook(int value) {
    gCoordinator.writeShort(value);
    markIgnored();
}

void writeStringHook(const char* value) {
    gCoordinator.writeString(value);
    markIgnored();
}

const char* commandArgsHook() {
    return gCoordinator.commandArgs();
}

const char* commandArgvHook(int index) {
    return gCoordinator.commandArgv(index);
}

int commandArgcHook() {
    return gCoordinator.commandArgc();
}

} // namespace astrabot::adapter::metamod
