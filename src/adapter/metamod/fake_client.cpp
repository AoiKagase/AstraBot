// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include "adapter/metamod/fake_client.hpp"

#include <limits>

namespace astrabot::adapter::metamod {
namespace {

constexpr std::uint16_t kFakeNameCapacity = 32;
constexpr char kDefaultFakeName[] = "AstraBot";
constexpr char kLoopbackAddress[] = "127.0.0.1";

struct OperationGuard final {
    bool& active;
    ~OperationGuard() { active = false; }
};

} // namespace

void FakeClientCoordinator::queuePrimaryCreate() noexcept {
    if (primaryQueued_) {
        return;
    }
    // Keep the last explicitly selected request across map teardown.  The
    // member is initialized to the backwards-compatible T/1 default.
    primaryQueued_ = true;
}

void FakeClientCoordinator::queuePrimaryCreate(
    cstrike::JoinRequest request) noexcept {
    primaryJoinRequest_ = request;
    primaryQueued_ = true;
}

void FakeClientCoordinator::configure(
    enginefuncs_t* engineFunctions,
    mutil_funcs_t* utilityFunctions,
    DLL_FUNCTIONS* gameDllFunctions,
    host::PlayerRegistry* players,
    host::BotAgentRegistry* agents) noexcept {
    engineFunctions_ = engineFunctions;
    utilityFunctions_ = utilityFunctions;
    gameDllFunctions_ = gameDllFunctions;
    players_ = players;
    agents_ = agents;
}

void FakeClientCoordinator::reset() noexcept {
    engineFunctions_ = nullptr;
    utilityFunctions_ = nullptr;
    gameDllFunctions_ = nullptr;
    players_ = nullptr;
    agents_ = nullptr;
    traceSink_ = nullptr;
    removalTraceSink_ = nullptr;
    resetMap();
}

void FakeClientCoordinator::resetMap() noexcept {
    primaryQueued_ = false;
    attempted_ = false;
    operationActive_ = false;
    removalPending_ = false;
    activeEntity_ = nullptr;
    activePlayer_ = {};
    activeMap_ = {}; activeSerial_=0;
}

FakeClientResult FakeClientCoordinator::processPrimaryCreate() noexcept {
    if (!primaryQueued_) {
        return FakeClientResult::noOp();
    }
    primaryQueued_ = false;
    return create(kDefaultFakeName, primaryJoinRequest_);
}

FakeClientResult FakeClientCoordinator::create(const char* name) noexcept {
    return create(name, {cstrike::Team::Terrorist, 1});
}

FakeClientResult FakeClientCoordinator::create(
    const char* name,
    cstrike::JoinRequest request) noexcept {
    activeJoinRequest_ = request;
    if (players_ == nullptr || !players_->isMapActive()) {
        return rejected(debug::FakeClientError::NotMapActive);
    }
    if (attempted_) {
        return rejected(debug::FakeClientError::AlreadyCreated);
    }
    if (operationActive_) {
        return rejected(debug::FakeClientError::Reentrant);
    }
    attempted_ = true;
    operationActive_ = true;
    OperationGuard guard{operationActive_};

    if (!configured()) {
        return rejected(debug::FakeClientError::NotConfigured);
    }

    char nameBuffer[kFakeNameCapacity]{};
    if (!copyName(name, nameBuffer, kFakeNameCapacity)) {
        return rejected(debug::FakeClientError::InvalidName);
    }

    trace(debug::FakeClientStage::Requested, debug::FakeClientError::None);
    edict_t* entity = engineFunctions_->pfnCreateFakeClient(nameBuffer);
    if (entity == nullptr) {
        return rejected(debug::FakeClientError::CreateFailed,
                        debug::FakeClientStage::Rejected);
    }
    trace(debug::FakeClientStage::Allocated, debug::FakeClientError::None);

    const int index = engineFunctions_->pfnIndexOfEdict(entity);
    if (index < 1 || index > static_cast<int>(players_->clientMax()) ||
        index > static_cast<int>(host::kMaxClientSlots)) {
        cleanup(entity, false);
        return rejected(debug::FakeClientError::InvalidSlot,
                        debug::FakeClientStage::RolledBack);
    }
    const auto slot = static_cast<std::uint16_t>(index);
    if (players_->isConnected(slot)) {
        // The engine returned an occupied slot; ownership was never acquired.
        // Removing this edict could destroy another managed or human player.
        return rejected(debug::FakeClientError::SlotOccupied,
                        debug::FakeClientStage::RolledBack);
    }

    if (!utilityFunctions_->pfnCallGameEntity(
            PLID, "player", &entity->v)) {
        cleanup(entity, false);
        return rejected(debug::FakeClientError::PlayerFactoryFailed,
                        debug::FakeClientStage::RolledBack);
    }
    trace(debug::FakeClientStage::PlayerFactory, debug::FakeClientError::None);

    char* infoBuffer = engineFunctions_->pfnGetInfoKeyBuffer(entity);
    if (infoBuffer == nullptr) {
        cleanup(entity, false);
        return rejected(debug::FakeClientError::InfoBufferFailed,
                        debug::FakeClientStage::RolledBack);
    }
    char vguiKey[] = "_vgui_menus";
    char vguiValue[] = "0";
    char ahKey[] = "_ah";
    char ahValue[] = "0";
    char botKey[] = "*bot";
    char botValue[] = "1";
    engineFunctions_->pfnSetClientKeyValue(
        index, infoBuffer, vguiKey, vguiValue);
    engineFunctions_->pfnSetClientKeyValue(index, infoBuffer, ahKey, ahValue);
    engineFunctions_->pfnSetClientKeyValue(index, infoBuffer, botKey, botValue);
    trace(debug::FakeClientStage::Metadata, debug::FakeClientError::None);

    char rejectReason[128]{};
    if (!gameDllFunctions_->pfnClientConnect(
            entity, nameBuffer, kLoopbackAddress, rejectReason)) {
        cleanup(entity, false);
        return rejected(debug::FakeClientError::ConnectRejected,
                        debug::FakeClientStage::RolledBack);
    }
    trace(debug::FakeClientStage::Connected, debug::FakeClientError::None);

    gameDllFunctions_->pfnClientPutInServer(entity);
    trace(debug::FakeClientStage::PutInServer, debug::FakeClientError::None);

    const host::LifecycleResult registration = players_->registerPlayer(slot);
    if (!registration) {
        cleanup(entity, true);
        return rejected(debug::FakeClientError::PlayerRegistrationFailed,
                        debug::FakeClientStage::RolledBack);
    }

    const host::BotAgentResult binding =
        agents_->bind(registration.event.player, registration.event.map);
    if (!binding) {
        const host::LifecycleResult rollback =
            players_->disconnectPlayer(registration.event.player);
        cleanup(entity, true);
        FakeClientResult result = rejected(
            debug::FakeClientError::AgentBindingFailed,
            debug::FakeClientStage::RolledBack,
            registration.event.player);
        result.playerRegistration = registration;
        result.playerRollback = rollback;
        return result;
    }

    FakeClientResult result{};
    result.player = registration.event.player;
    result.agent = binding.binding.agent;
    result.playerRegistration = registration;
    result.accepted = true;
    result.changed = true;
    activeEntity_ = entity;
    activePlayer_ = result.player;
    activeMap_=registration.event.map; activeSerial_=entity->serialnumber;
    trace(
        debug::FakeClientStage::Published,
        debug::FakeClientError::None,
        result.player,
        result.agent,
        true,
        true);
    return result;
}

void FakeClientCoordinator::trace(
    debug::FakeClientStage stage,
    debug::FakeClientError error,
    host::PlayerId player,
    core::BotAgentId agent,
    bool accepted,
    bool changed) noexcept {
    const host::MapGeneration map =
        players_ == nullptr ? host::MapGeneration::invalid() :
                              players_->mapGeneration();
    const host::EventSequence sequence =
        players_ != nullptr && player.isValid() ? players_->eventSequence() : 0;
    debug::emitFakeClient({
                              stage,
                              error,
                              map,
                              player.slot,
                              player.generation,
                              agent,
                              sequence,
                              accepted,
                              changed,
                          },
                          traceSink_);
}

FakeClientResult FakeClientCoordinator::rejected(
    debug::FakeClientError error,
    debug::FakeClientStage stage,
    host::PlayerId player) noexcept {
    trace(stage, error, player);
    return {error, player};
}

bool FakeClientCoordinator::configured() const noexcept {
    return engineFunctions_ != nullptr && utilityFunctions_ != nullptr &&
           gameDllFunctions_ != nullptr && players_ != nullptr &&
           agents_ != nullptr && engineFunctions_->pfnCreateFakeClient != nullptr &&
           engineFunctions_->pfnIndexOfEdict != nullptr &&
           engineFunctions_->pfnGetInfoKeyBuffer != nullptr &&
           engineFunctions_->pfnSetClientKeyValue != nullptr &&
           engineFunctions_->pfnRemoveEntity != nullptr &&
           utilityFunctions_->pfnCallGameEntity != nullptr &&
           gameDllFunctions_->pfnClientConnect != nullptr &&
           gameDllFunctions_->pfnClientPutInServer != nullptr &&
           gameDllFunctions_->pfnClientDisconnect != nullptr;
}

bool FakeClientCoordinator::sameEntity() const noexcept {
    auto* entity=activeEntity_; auto* engine=engineFunctions_;
    const auto player=activePlayer_; const auto map=activeMap_; const auto serial=activeSerial_;
    if(!entity || entity->free || entity->serialnumber!=serial || !engine || !engine->pfnIndexOfEdict ||
       engine->pfnIndexOfEdict(entity)!=player.slot ||
       (engine->pfnPEntityOfEntIndex && engine->pfnPEntityOfEntIndex(player.slot)!=entity)) return false;
    return activeEntity_==entity && activePlayer_==player && activeMap_==map && activeSerial_==serial &&
        !entity->free && entity->serialnumber==serial;
}
edict_t* FakeClientCoordinator::entityFor(host::PlayerId player) const noexcept {
    if(!player.isValid() || player!=activePlayer_ || !players_ || !players_->isMapActive() ||
       players_->mapGeneration()!=activeMap_ || players_->currentPlayer(player.slot)!=player || !sameEntity()) return nullptr;
    return players_ && players_->isMapActive() && players_->mapGeneration()==activeMap_ &&
        players_->currentPlayer(player.slot)==player ? activeEntity_:nullptr;
}
void FakeClientCoordinator::forget(host::PlayerId player) noexcept {
    if (player.isValid() && activePlayer_ == player) {
        activeEntity_ = nullptr; activeMap_={}; activeSerial_=0;
        activePlayer_ = {};
        removalPending_ = false;
        attempted_ = false;
    }
}

RemovalResult FakeClientCoordinator::requestRemoval() noexcept {
    if (activeEntity_ == nullptr || !activePlayer_.isValid()) {
        return {
            debug::RemovalOutcome::NoOp,
            debug::RemovalError::None,
            {},
            true,
            false};
    }
    if (removalPending_) {
        return {
            debug::RemovalOutcome::NoOp,
            debug::RemovalError::None,
            activePlayer_,
            true,
            false};
    }
    if(!sameEntity()) return {debug::RemovalOutcome::Rejected,debug::RemovalError::DirectCleanupFailed,activePlayer_,false,false};
    if (!configured()) {
        emitRemoval(
            debug::RemovalOutcome::Rejected,
            debug::RemovalError::NotConfigured,
            activePlayer_,
            true,
            true);
        return {
            debug::RemovalOutcome::Rejected,
            debug::RemovalError::NotConfigured,
            activePlayer_,
            false,
            false};
    }

    const auto requestedPlayer=activePlayer_;
    removalPending_=true; // Publish before synchronous ServerExecute can disconnect it.
    debug::RemovalError error = debug::RemovalError::None;
    if (!issueKick(activeEntity_, error)) {
        if(activePlayer_==requestedPlayer) removalPending_=false;
        emitRemoval(
            debug::RemovalOutcome::Rejected,
            error,
            activePlayer_,
            true,
            true);
        return {
            debug::RemovalOutcome::Rejected,
            error,
            activePlayer_,
            false,
            false};
    }

    emitRemoval(
        debug::RemovalOutcome::KickQueued,
        debug::RemovalError::None,
        requestedPlayer,
        true,
        true);
    return {
        debug::RemovalOutcome::KickQueued,
        debug::RemovalError::None,
        requestedPlayer,
        true,
        true};
}

bool FakeClientCoordinator::cleanupActiveDirect(bool connected) noexcept {
    if (!sameEntity() || !activePlayer_.isValid()) {
        return false;
    }
    if (engineFunctions_ == nullptr || engineFunctions_->pfnRemoveEntity == nullptr ||
        (connected &&
         (gameDllFunctions_ == nullptr ||
          gameDllFunctions_->pfnClientDisconnect == nullptr))) {
        return false;
    }
    const host::PlayerId player = activePlayer_;
    cleanup(activeEntity_, connected);
    acknowledgeDisconnect(player);
    return true;
}

void FakeClientCoordinator::acknowledgeDisconnect(
    host::PlayerId player) noexcept {
    if (player.isValid() && activePlayer_ == player) {
        activeEntity_ = nullptr; activeMap_={}; activeSerial_=0;
        activePlayer_ = {};
        removalPending_ = false;
        attempted_ = false;
    }
}

bool FakeClientCoordinator::kickAndCleanup(host::PlayerId player) noexcept {
    if (!player.isValid() || activePlayer_ != player || !sameEntity()) {
        return false;
    }

    edict_t* entity = activeEntity_;
    debug::RemovalError error = debug::RemovalError::None;
    const bool kicked = issueKick(entity, error);
    if (!kicked) {
        cleanup(entity, true);
    }
    forget(player);
    return kicked;
}

bool FakeClientCoordinator::copyName(
    const char* source, char* destination, std::uint16_t capacity) noexcept {
    if (source == nullptr || destination == nullptr || capacity < 2U) {
        return false;
    }
    std::uint16_t length = 0;
    while (length + 1U < capacity && source[length] != '\0') {
        const unsigned char character =
            static_cast<unsigned char>(source[length]);
        if (character < 0x21U || character > 0x7EU) {
            return false;
        }
        ++length;
    }
    if (length == 0U ||
        ((length + 1U >= capacity) && source[length] != '\0')) {
        return false;
    }
    for (std::uint16_t index = 0; index <= length; ++index) {
        destination[index] = source[index];
    }
    return true;
}

void FakeClientCoordinator::cleanup(edict_t* entity, bool connected) noexcept {
    if (entity == nullptr) {
        return;
    }
    const auto serial=entity->serialnumber;
    if (connected && gameDllFunctions_ != nullptr &&
        gameDllFunctions_->pfnClientDisconnect != nullptr) {
        gameDllFunctions_->pfnClientDisconnect(entity);
    }
    if (!entity->free && entity->serialnumber==serial && engineFunctions_ != nullptr && engineFunctions_->pfnRemoveEntity != nullptr) {
        engineFunctions_->pfnRemoveEntity(entity);
    }
}

bool FakeClientCoordinator::issueKick(
    edict_t* entity,
    debug::RemovalError& error) noexcept {
    error = debug::RemovalError::None;
    if (entity == nullptr || engineFunctions_ == nullptr ||
        engineFunctions_->pfnGetPlayerUserId == nullptr ||
        engineFunctions_->pfnServerCommand == nullptr ||
        engineFunctions_->pfnServerExecute == nullptr) {
        error = debug::RemovalError::KickUnavailable;
        return false;
    }

    const int userId = engineFunctions_->pfnGetPlayerUserId(entity);
    if (userId <= 0) {
        error = debug::RemovalError::InvalidUserId;
        return false;
    }

    char command[32]{};
    const char prefix[] = "kick #";
    std::uint16_t length = 0;
    for (const char character : prefix) {
        if (character == '\0') {
            break;
        }
        command[length++] = character;
    }

    char digits[12]{};
    std::uint16_t digitCount = 0;
    int value = userId;
    while (value > 0 && digitCount < sizeof(digits)) {
        digits[digitCount++] = static_cast<char>('0' + (value % 10));
        value /= 10;
    }
    if (digitCount == 0U || length + digitCount + 2U >= sizeof(command)) {
        error = debug::RemovalError::CommandBuildFailed;
        return false;
    }
    while (digitCount > 0U) {
        command[length++] = digits[--digitCount];
    }
    command[length++] = '\n';
    command[length] = '\0';
    engineFunctions_->pfnServerCommand(command);
    engineFunctions_->pfnServerExecute();
    return true;
}

void FakeClientCoordinator::emitRemoval(
    debug::RemovalOutcome outcome,
    debug::RemovalError error,
    host::PlayerId player,
    bool mappingPresent,
    bool entityPresent) noexcept {
    const host::MapGeneration map =
        players_ == nullptr ? host::MapGeneration::invalid() :
                              players_->mapGeneration();
    const host::TickId tick =
        players_ == nullptr ? host::TickId::invalid() : players_->currentTick();
    const host::EventSequence sequence =
        players_ == nullptr ? 0 : players_->eventSequence();
    debug::emitRemoval(
        {
            outcome,
            error,
            map,
            player,
            tick,
            sequence,
            mappingPresent,
            entityPresent,
        },
        removalTraceSink_);
}

} // namespace astrabot::adapter::metamod
