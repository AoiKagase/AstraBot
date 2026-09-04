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
    queuePrimaryCreate({cstrike::Team::Terrorist, 1});
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
    resetMap();
}

void FakeClientCoordinator::resetMap() noexcept {
    primaryQueued_ = false;
    attempted_ = false;
    operationActive_ = false;
    activeEntity_ = nullptr;
    activePlayer_ = {};
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
        cleanup(entity, false);
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

void FakeClientCoordinator::forget(host::PlayerId player) noexcept {
    if (player.isValid() && activePlayer_ == player) {
        activeEntity_ = nullptr;
        activePlayer_ = {};
    }
}

bool FakeClientCoordinator::kickAndCleanup(host::PlayerId player) noexcept {
    if (!player.isValid() || activePlayer_ != player || activeEntity_ == nullptr) {
        return false;
    }

    edict_t* entity = activeEntity_;
    const bool kicked = issueKick(entity);
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
    if (connected && gameDllFunctions_ != nullptr &&
        gameDllFunctions_->pfnClientDisconnect != nullptr) {
        gameDllFunctions_->pfnClientDisconnect(entity);
    }
    if (engineFunctions_ != nullptr && engineFunctions_->pfnRemoveEntity != nullptr) {
        engineFunctions_->pfnRemoveEntity(entity);
    }
}

bool FakeClientCoordinator::issueKick(edict_t* entity) noexcept {
    if (entity == nullptr || engineFunctions_ == nullptr ||
        engineFunctions_->pfnGetPlayerUserId == nullptr ||
        engineFunctions_->pfnServerCommand == nullptr ||
        engineFunctions_->pfnServerExecute == nullptr) {
        return false;
    }

    const int userId = engineFunctions_->pfnGetPlayerUserId(entity);
    if (userId <= 0) {
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

} // namespace astrabot::adapter::metamod
