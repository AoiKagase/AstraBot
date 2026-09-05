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

LifecycleCoordinator::ClientState* LifecycleCoordinator::findClient(core::PlayerId player) noexcept {
    if(!player.isValid()) return nullptr;
    for(auto& client:clients_) if(client.fake.activePlayer()==player) return &client;
    return nullptr;
}
const LifecycleCoordinator::ClientState* LifecycleCoordinator::findClient(core::PlayerId player) const noexcept {
    if(!player.isValid()) return nullptr;
    for(const auto& client:clients_) if(client.fake.activePlayer()==player) return &client;
    return nullptr;
}
edict_t* LifecycleCoordinator::entityFor(core::PlayerId player) const noexcept {
    const auto* client=findClient(player);
    return client ? client->fake.entityFor(player):nullptr;
}
core::PlayerId LifecycleCoordinator::playerForEntity(edict_t* entity) const noexcept {
    if(!entity) return {};
    for(const auto& client:clients_) {
        const auto player=client.fake.activePlayer();
        if(player.isValid() && client.fake.entityFor(player)==entity) return player;
    }
    return {};
}
const cstrike::JoinState* LifecycleCoordinator::joinState(core::PlayerId player) const noexcept {
    const auto* client=findClient(player);
    return client && client->join.player()==player ? &client->join:nullptr;
}
void LifecycleCoordinator::configure(enginefuncs_t* engine,mutil_funcs_t* utility,
    DLL_FUNCTIONS* game,cstrike::UserMessageIds ids) noexcept {
    engineFunctions_=engine; utilityFunctions_=utility; gameDllFunctions_=game;
    messageDecoder_.configure(ids,&LifecycleCoordinator::onMessage,this);
    activeDecoder_=&messageDecoder_;
    for(auto& client:clients_) {
        client.fake.configure(engine,utility,game,&registry_,&agents_);
        client.decoder.configure(ids,&LifecycleCoordinator::onMessage,this);
    }
    movement_.configure(engine,&registry_);
    navConsole_.bindMovement(&movement_);
}
void LifecycleCoordinator::reset() noexcept {
    navConsole_.reset(); movement_.reset();
    commandContextActive_=false; commandPlayer_={};
    commandArgv0_={}; commandArgv1_={}; commandArgs_={};
    messageDecoder_.reset(); activeDecoder_=&messageDecoder_;
    // Retire every actor's portable state before any external disconnect callback.
    for(auto& client:clients_) {
        client.join.reset(); client.decoder.reset(); client.cleanupPending=false;
        client.cleanupError=cstrike::JoinError::None;
        const auto player=client.fake.activePlayer();
        if(player.isValid()) { (void)agents_.unbind(player); (void)registry_.disconnectPlayer(player); }
    }
    for(auto& client:clients_) {
        const auto player=client.fake.activePlayer();
        if(player.isValid()) (void)client.fake.cleanupActiveDirect(true);
        client.fake.reset();
    }
    agents_.reset(); registry_.reset();
    engineFunctions_=nullptr; utilityFunctions_=nullptr; gameDllFunctions_=nullptr;
    status_={}; traceSink_=nullptr; joinTraceSink_=nullptr; removalTraceSink_=nullptr;
}
void LifecycleCoordinator::serverActivate(int clientMax) noexcept {
    const auto result=clientMax<1 || clientMax>host::kMaxClientSlots
        ? host::LifecycleResult::rejected(host::HostError::InvalidLifecycle)
        : registry_.activateMap(static_cast<std::uint16_t>(clientMax));
    if(result.changed()) {
        ++status_.mapActivations;
        if(status_.mapActivations>1) ++status_.mapReplays;
        movement_.resetMap(); clients_[0].fake.queuePrimaryCreate();
    }
    emit(host::LifecycleEventKind::MapActivated,result);
}
void LifecycleCoordinator::serverDeactivate() noexcept {
    navConsole_.invalidate(nav::runtime::SessionReason::MapChanged); movement_.resetMap();
    for(auto& client:clients_) {
        const auto action=client.join.cancel(cstrike::JoinError::MapDeactivated);
        emitJoin(client,action);
        client.join.reset(); client.decoder.reset(); client.cleanupPending=false;
        client.cleanupError=cstrike::JoinError::None;
    }
    messageDecoder_.reset(); activeDecoder_=&messageDecoder_;
    commandContextActive_=false; commandPlayer_={};
    commandArgv0_={}; commandArgv1_={}; commandArgs_={};
    const auto result=registry_.deactivateMap();
    for(auto& client:clients_) client.fake.resetMap();
    agents_.clearMappings();
    emit(host::LifecycleEventKind::MapDeactivated,result);
}
void LifecycleCoordinator::clientDisconnect(edict_t* entity) noexcept {
    if(!entity || !engineFunctions_ || !engineFunctions_->pfnIndexOfEdict) {
        emit(host::LifecycleEventKind::PlayerDisconnected,host::LifecycleResult::rejected(host::HostError::InvalidPlayer)); return;
    }
    const int index=engineFunctions_->pfnIndexOfEdict(entity);
    if(index<1 || index>host::kMaxClientSlots) {
        emit(host::LifecycleEventKind::PlayerDisconnected,host::LifecycleResult::rejected(host::HostError::InvalidPlayer)); return;
    }
    const auto player=registry_.currentPlayer(static_cast<std::uint16_t>(index));
    auto* client=findClient(player);
    // A reused edict or a foreign entity must not disconnect the mapped actor.
    if(client && client->fake.entityFor(player)!=entity) {
        emit(host::LifecycleEventKind::PlayerDisconnected,host::LifecycleResult::rejected(host::HostError::InvalidPlayer)); return;
    }
    movement_.forget(player);
    if(client) {
        if(client==&clients_[0]) navConsole_.invalidate(nav::runtime::SessionReason::Disconnected);
        emitJoin(*client,client->join.cancel(cstrike::JoinError::Disconnected));
        client->join.reset(); client->decoder.reset(); client->cleanupPending=false;
        client->cleanupError=cstrike::JoinError::None;
    }
    if(commandPlayer_==player) {
        commandContextActive_=false; commandPlayer_={};
        commandArgv0_={}; commandArgv1_={}; commandArgs_={};
    }
    const auto result=registry_.disconnectSlot(static_cast<std::uint16_t>(index));
    if(result.changed()) {
        (void)agents_.unbind(player);
        if(client) {
            client->fake.acknowledgeDisconnect(player); ++status_.cleanupCompletions;
            emitRemoval(debug::RemovalOutcome::Cleaned,debug::RemovalError::None,player,false,false);
        }
    }
    emit(host::LifecycleEventKind::PlayerDisconnected,result);
}
FakeClientResult LifecycleCoordinator::createBot(const char* name,cstrike::JoinRequest request) noexcept {
    if(commandContextActive_) return {debug::FakeClientError::Reentrant,{}};
    for(const auto& client:clients_) if(client.fake.operationActive()) return {debug::FakeClientError::Reentrant,{}};
    for(auto& client:clients_) {
        if(client.fake.activePlayer().isValid()) continue;
        client.fake.resetMap(); client.join.reset(); client.decoder.reset();
        client.cleanupPending=false; client.cleanupError=cstrike::JoinError::None;
        const auto result=client.fake.create(name,request);
        ++status_.createAttempts;
        if(result.playerRegistration.changed()) emit(host::LifecycleEventKind::PlayerConnected,result.playerRegistration);
        if(result.playerRollback.changed()) emit(host::LifecycleEventKind::PlayerDisconnected,result.playerRollback);
        if(result.succeeded()) (void)requestJoin(result.player,request);
        return result;
    }
    return {debug::FakeClientError::AlreadyCreated,{}};
}
RemovalResult LifecycleCoordinator::removeActive() noexcept { return remove(clients_[0].fake.activePlayer()); }
RemovalResult LifecycleCoordinator::remove(core::PlayerId player) noexcept {
    auto* client=findClient(player);
    if(!client) return {debug::RemovalOutcome::NoOp,debug::RemovalError::None,{},true,false};
    if(client->fake.removalPending()) return {debug::RemovalOutcome::NoOp,debug::RemovalError::None,player,true,false};
    movement_.forget(player);
    if(client==&clients_[0]) navConsole_.invalidate(nav::runtime::SessionReason::Disconnected);
    emitJoin(*client,client->join.cancel(cstrike::JoinError::Disconnected));
    client->decoder.reset(); client->cleanupPending=false; client->cleanupError=cstrike::JoinError::None;
    if(commandPlayer_==player) {
        commandContextActive_=false; commandPlayer_={}; commandArgv0_={}; commandArgv1_={}; commandArgs_={};
    }
    ++status_.removalRequests;
    const auto result=client->fake.requestRemoval();
    status_.lastRemovalError=result.error;
    if(!result.succeeded()) cleanupActiveAfterRemoval(*client,result,player);
    return result;
}
void LifecycleCoordinator::queuePrimaryCreate(cstrike::JoinRequest request) noexcept { clients_[0].fake.queuePrimaryCreate(request); }
void LifecycleCoordinator::startFrame() noexcept {
    const auto result=registry_.startFrame(); emit(host::LifecycleEventKind::FrameStarted,result);
    if(!result.changed()) return;
    movement_.beginFrame();
    const auto map=registry_.mapGeneration(); const auto tick=registry_.currentTick();
    const auto created=clients_[0].fake.processPrimaryCreate();
    if(created.changed || created.error!=debug::FakeClientError::None) ++status_.createAttempts;
    if(created.playerRegistration.changed()) emit(host::LifecycleEventKind::PlayerConnected,created.playerRegistration);
    if(created.playerRollback.changed()) emit(host::LifecycleEventKind::PlayerDisconnected,created.playerRollback);
    if(created.succeeded() && created.changed) (void)requestJoin(created.player,clients_[0].fake.activeJoinRequest());
    for(auto& client:clients_) {
        if(!registry_.isMapActive() || registry_.mapGeneration()!=map || registry_.currentTick()!=tick) return;
        const auto player=client.fake.activePlayer();
        if(!player.isValid()) continue;
        const auto action=client.join.onFrame(tick); handleJoinAction(client,action);
        if(action.kind==cstrike::JoinActionKind::SendMenuSelect && client.fake.activePlayer()==player) {
            const bool dispatched=dispatchMenu(client,action.selection);
            if(client.join.player()==player) handleJoinAction(client,client.join.commandCompleted(dispatched));
        }
    }
    navConsole_.beforeDispatch(*this);
    const auto ticket=navConsole_.dispatchTicket();
    for(auto& client:clients_) {
        if(!registry_.isMapActive() || registry_.mapGeneration()!=map || registry_.currentTick()!=tick) return;
        const auto player=client.fake.activePlayer(); if(!player.isValid()) continue;
        const auto moved=movement_.dispatchAtFrameEnd(client.join.phase(),player,client.fake.entityFor(player),map,tick);
        if(&client==&clients_[0]) navConsole_.afterDispatch(moved,tick,ticket);
    }
    navConsole_.moveFrame(*this);
}
MovementResult LifecycleCoordinator::submitCommand(core::PlayerId player,core::MapGeneration map,
    core::TickId tick,const core::BotCommand& command) noexcept {
    const auto* client=findClient(player);
    const auto reject=[&](MovementError error) { return movement_.rejectIngress(error,player,map,tick,command.msec); };
    if(!client) return reject(MovementError::MappingMismatch);
    if(client->join.player()!=player || client->join.phase()!=cstrike::JoinPhase::Joined) return reject(MovementError::NotJoined);
    auto* entity=client->fake.entityFor(player);
    if(!entity) return reject(MovementError::MissingEntity);
    if(entity->v.deadflag!=DEAD_NO) return reject(MovementError::DeadPlayer);
    return movement_.submit(player,map,tick,command);
}
void LifecycleCoordinator::messageBegin(int,int messageType,const float*,edict_t* recipient) noexcept {
    messageMap_=registry_.mapGeneration();
    for(std::uint16_t i=1;i<=host::kMaxClientSlots;++i) messagePlayers_[i-1U]=registry_.currentPlayer(i);
    std::uint16_t slot=0;
    if(recipient && engineFunctions_ && engineFunctions_->pfnIndexOfEdict) {
        const int index=engineFunctions_->pfnIndexOfEdict(recipient);
        if(index>=1 && index<=host::kMaxClientSlots) slot=static_cast<std::uint16_t>(index);
    }
    auto* client=findClient(registry_.currentPlayer(slot));
    // Per-client decoders preserve interleaved ShowMenu fragments. Broadcast
    // TeamInfo uses the shared decoder and is routed by its playerSlot at end.
    const bool matched=client && client->fake.entityFor(client->fake.activePlayer())==recipient;
    activeDecoder_=matched ? &client->decoder:&messageDecoder_;
    if(recipient && !matched) slot=0;
    activeDecoder_->begin(messageType,slot);
}
void LifecycleCoordinator::messageEnd() noexcept { if(activeDecoder_) activeDecoder_->end(); }
void LifecycleCoordinator::writeByte(int v) noexcept { if(activeDecoder_) activeDecoder_->writeByte(v); }
void LifecycleCoordinator::writeChar(int v) noexcept { if(activeDecoder_) activeDecoder_->writeChar(v); }
void LifecycleCoordinator::writeShort(int v) noexcept { if(activeDecoder_) activeDecoder_->writeShort(v); }
void LifecycleCoordinator::writeString(const char* v) noexcept { if(activeDecoder_) activeDecoder_->writeString(v); }
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

cstrike::JoinAction LifecycleCoordinator::requestJoin(cstrike::JoinRequest request) noexcept {
    return requestJoin(clients_[0].fake.activePlayer(),request);
}
cstrike::JoinAction LifecycleCoordinator::requestJoin(core::PlayerId player,cstrike::JoinRequest request) noexcept {
    auto* client=findClient(player);
    if(!client || !client->fake.entityFor(player)) return cstrike::JoinAction::failed(cstrike::JoinError::InvalidPlayer);
    const auto action=client->join.begin(player,registry_.mapGeneration(),request,registry_.currentTick());
    if(action.error!=cstrike::JoinError::AlreadyJoining) handleJoinAction(*client,action);
    return action;
}
bool LifecycleCoordinator::dispatchMenuForTest(std::uint8_t selection) noexcept { return dispatchMenu(clients_[0],selection); }
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

void LifecycleCoordinator::emitJoin(const ClientState& client,const cstrike::JoinAction& action) noexcept {
    if(!action.changed) return;
    const auto& join=client.join; const auto request=join.request();
    debug::emitJoin({join.phase(),join.error(),join.map(),join.player(),request.team,request.classNumber,
        registry_.currentTick(),registry_.eventSequence(),join.attempts(),
        action.kind!=cstrike::JoinActionKind::Failed && action.kind!=cstrike::JoinActionKind::Cancelled,
        action.changed},joinTraceSink_);
}
void LifecycleCoordinator::handleMessage(const cstrike::MessageEvent& event) noexcept {
    const auto slot=event.kind==cstrike::MessageKind::TeamInfo ? event.playerSlot:event.recipientSlot;
    if(slot<1 || slot>host::kMaxClientSlots || messageMap_!=registry_.mapGeneration() ||
       messagePlayers_[slot-1U]!=registry_.currentPlayer(slot)) return;
    auto* client=findClient(registry_.currentPlayer(slot));
    if(!client || !client->fake.entityFor(client->fake.activePlayer())) return;
    handleJoinAction(*client,client->join.onMessage(event,registry_.currentTick()));
}
void LifecycleCoordinator::handleJoinAction(ClientState& client,const cstrike::JoinAction& action) noexcept {
    if(!action.changed) return;
    emitJoin(client,action);
    if(action.kind==cstrike::JoinActionKind::Failed) {
        if(commandContextActive_) { client.cleanupPending=true; client.cleanupError=action.error; }
        else cleanupFailedJoin(client,action.error);
    }
}
void LifecycleCoordinator::cleanupActiveAfterRemoval(ClientState& client,const RemovalResult& result,host::PlayerId player) noexcept {
    const bool mapping=agents_.findByPlayer(player).isValid();
    (void)agents_.unbind(player); (void)registry_.disconnectPlayer(player);
    const bool cleaned=client.fake.cleanupActiveDirect(true);
    if(cleaned) ++status_.cleanupCompletions;
    else { client.fake.forget(player); status_.lastRemovalError=debug::RemovalError::DirectCleanupFailed; }
    emitRemoval(cleaned ? debug::RemovalOutcome::Cleaned:debug::RemovalOutcome::Rejected,
        cleaned ? result.error:debug::RemovalError::DirectCleanupFailed,player,mapping,true);
    client.join.reset(); client.decoder.reset();
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

void LifecycleCoordinator::cleanupFailedJoin(ClientState& client,cstrike::JoinError) noexcept {
    const auto player=client.join.player();
    if(player.isValid()) {
        movement_.forget(player); (void)agents_.unbind(player); (void)registry_.disconnectPlayer(player);
        (void)client.fake.kickAndCleanup(player);
    }
    client.cleanupPending=false; client.cleanupError=cstrike::JoinError::None;
}
bool LifecycleCoordinator::dispatchMenu(ClientState& client,std::uint8_t selection) noexcept {
    const auto player=client.join.player(); auto* entity=client.fake.entityFor(player);
    if(commandContextActive_ || !entity || !gameDllFunctions_ || !gameDllFunctions_->pfnClientCommand ||
       selection==0 || selection>9) return false;
    copyCommandWord(commandArgv0_,"menuselect");
    commandArgv1_={}; commandArgv1_[0]=static_cast<char>('0'+selection);
    commandArgs_={}; commandArgs_[0]=commandArgv1_[0]; commandPlayer_=player; commandContextActive_=true;
    { CommandContextGuard guard{commandContextActive_}; gameDllFunctions_->pfnClientCommand(entity); }
    commandPlayer_={};
    for(auto& pending:clients_) if(pending.cleanupPending) cleanupFailedJoin(pending,pending.cleanupError);
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
