// SPDX-License-Identifier: MPL-2.0
#include "adapter/metamod/runtime_input.hpp"
#include "adapter/cstrike/weapon_protocol.hpp"
#include "adapter/metamod/lifecycle.hpp"
#include <entity_state.h>
#include <algorithm>
#include <cmath>
#include <limits>

namespace astrabot::adapter::metamod {
namespace {
using WeaponClass = core::combat::WeaponSnapshot::WeaponClass;

using cstrike::protocol::weaponClass;
using cstrike::protocol::isSwitchableWeapon;

RuntimeActorStaleReason staleReason(const LifecycleCoordinator& owner,
             const RuntimeFrame& frame, core::PlayerId player,
             core::BotAgentId agent, const edict_t* entity) noexcept {
    const auto binding = owner.agents().findByPlayer(player);
    const auto* join = owner.joinState(player);
    if (!owner.registry().isMapActive()) return RuntimeActorStaleReason::MapInactive;
    if (owner.registry().mapGeneration() != frame.map)
        return RuntimeActorStaleReason::MapGenerationMismatch;
    if (owner.registry().currentTick() != frame.tick)
        return RuntimeActorStaleReason::TickMismatch;
    if (owner.round() != frame.round) return RuntimeActorStaleReason::RoundMismatch;
    if (owner.registry().currentPlayer(player.slot) != player)
        return RuntimeActorStaleReason::PlayerGenerationMismatch;
    if (!binding.isValid()) return RuntimeActorStaleReason::BindingInvalid;
    if (binding.agent != agent) return RuntimeActorStaleReason::BindingAgentMismatch;
    if (binding.map != frame.map) return RuntimeActorStaleReason::BindingMapMismatch;
    if (!entity || owner.entityFor(player) != entity)
        return RuntimeActorStaleReason::MissingEntity;
    if (entity->free) return RuntimeActorStaleReason::EntityFree;
    if (owner.removalPending(player)) return RuntimeActorStaleReason::RemovalPending;
    if (!join || join->phase() != cstrike::JoinPhase::Joined)
        return RuntimeActorStaleReason::NotJoined;
    if (entity->v.deadflag != DEAD_NO) return RuntimeActorStaleReason::Dead;
    if (!std::isfinite(entity->v.health) || entity->v.health <= 0)
        return RuntimeActorStaleReason::InvalidHealth;
    if (entity->v.iuser1 != 0) return RuntimeActorStaleReason::SpectatorState;
    if (entity->v.flags & FL_SPECTATOR) return RuntimeActorStaleReason::SpectatorFlag;
    return RuntimeActorStaleReason::None;
}

bool current(const LifecycleCoordinator& owner, const RuntimeFrame& frame,
             core::PlayerId player, core::BotAgentId agent, const edict_t* entity) noexcept {
    return staleReason(owner, frame, player, agent, entity) == RuntimeActorStaleReason::None;
}

bool readWeapon(const LifecycleCoordinator& owner, const RuntimeFrame& frame,
                core::PlayerId player, core::BotAgentId agent, DLL_FUNCTIONS* dll, edict_t* entity,
                cstrike::WeaponObservation& result,
                RuntimeInputBuildReason* failure = nullptr,
                RuntimeInputBuildStatus* status = nullptr) noexcept {
    const auto fail = [&](RuntimeInputBuildReason reason) noexcept {
        if (failure) *failure = reason;
        return false;
    };
    if (status) {
        status->updateClientDataAvailable = dll != nullptr && dll->pfnUpdateClientData != nullptr;
        status->weaponDataAvailable = dll != nullptr && dll->pfnGetWeaponData != nullptr;
    }
    if (!dll || !dll->pfnUpdateClientData)
        return fail(RuntimeInputBuildReason::MissingUpdateClientData);
    if (!dll->pfnGetWeaponData)
        return fail(RuntimeInputBuildReason::MissingWeaponData);
    clientdata_t client{};
    std::array<weapon_data_t, cstrike::protocol::kWeaponDataSlots> weapons{};
    if (status) status->updateClientDataCalled = true;
    dll->pfnUpdateClientData(entity, 1, &client);
    if (!current(owner, frame, player, agent, entity))
        return fail(RuntimeInputBuildReason::InvalidWeaponObservation);
    if (status) status->weaponDataCalled = true;
    if (!dll->pfnGetWeaponData(entity, weapons.data()) ||
        !current(owner, frame, player, agent, entity))
        return fail(RuntimeInputBuildReason::InvalidWeaponObservation);
    if (client.m_iId <= 0 || static_cast<std::size_t>(client.m_iId) >= weapons.size())
        return fail(RuntimeInputBuildReason::InvalidWeaponObservation);
    const auto& active = weapons[static_cast<std::size_t>(client.m_iId)];
    if (active.m_iId != client.m_iId || !std::isfinite(client.vuser4[1]) ||
        client.vuser4[1] < 0 || client.vuser4[1] > core::combat::kMaxAmmo ||
        std::floor(client.vuser4[1]) != client.vuser4[1] ||
        !std::isfinite(client.m_flNextAttack) || !std::isfinite(active.m_flNextPrimaryAttack))
        return fail(RuntimeInputBuildReason::InvalidWeaponObservation);
    result.map = frame.map; result.round = frame.round; result.tick = frame.tick;
    result.observedMicros = frame.nowMicros;
    result.activeWeapon = static_cast<std::uint16_t>(client.m_iId);
    result.activeClass = weaponClass(client.m_iId);
    // Non-firearms have -1 clips and unsupported P5 fire modes. They still
    // provide a current inventory, but cannot authorize firearm attacks.
    if (active.m_iClip < cstrike::protocol::kNoClip || (active.m_iClip < 0 && result.activeClass != WeaponClass::Unknown && result.activeClass != WeaponClass::Melee))
        return fail(RuntimeInputBuildReason::InvalidWeaponObservation);
    result.clipAmmo = (std::max)(0, active.m_iClip);
    result.reserveAmmo = result.activeClass == WeaponClass::Melee ? 0 : static_cast<std::int32_t>(client.vuser4[1]);
    result.reloading = active.m_fInReload != 0 || active.m_fInSpecialReload != 0;
    result.canReload = result.activeClass != WeaponClass::Unknown && result.activeClass != WeaponClass::Melee && result.reserveAmmo > 0;
    result.reloadClipThreshold = result.activeClass == WeaponClass::Melee ? 0 : 3;
    bool hasSwitchableAlternative = false;
    for (std::size_t i = 1; i < weapons.size(); ++i) {
        if (weapons[i].m_iId == 0) continue;
        if (weapons[i].m_iId != static_cast<int>(i) || result.ownedCount == result.owned.size())
            return fail(RuntimeInputBuildReason::InvalidWeaponObservation);
        result.owned[result.ownedCount++] = static_cast<std::uint16_t>(i);
        hasSwitchableAlternative = hasSwitchableAlternative || (i != static_cast<std::size_t>(client.m_iId) && isSwitchableWeapon(static_cast<int>(i)));
    }
    result.canSwitch = hasSwitchableAlternative;
    // ReGameDLL's prediction timers are relative seconds. iuser3 bit 0 is
    // CAN_SHOOT and bit 1 denotes the freeze period (despite its wire name).
    const double delay = (std::max)(0.0, static_cast<double>((std::max)(client.m_flNextAttack, active.m_flNextPrimaryAttack)));
    if (delay > 60.0 || frame.nowMicros > (std::numeric_limits<std::uint64_t>::max)() - 60'000'001)
        return fail(RuntimeInputBuildReason::InvalidWeaponObservation);
    result.primaryAttackReadyMicros = frame.nowMicros + static_cast<std::uint64_t>(std::ceil(delay * 1'000'000.0));
    if (!(client.iuser3 & cstrike::protocol::kCanShoot) ||
        (client.iuser3 & cstrike::protocol::kFreezePeriod) || result.activeClass == WeaponClass::Unknown)
        result.primaryAttackReadyMicros = frame.nowMicros + 60'000'000;
    if (!cstrike::toWeaponSnapshot(result))
        return fail(RuntimeInputBuildReason::InvalidWeaponObservation);
    return true;
}
}

bool runtimeActorReady(const LifecycleCoordinator& owner, const RuntimeFrame& frame,
    DLL_FUNCTIONS* dll, core::PlayerId player, core::combat::WeaponId expected, bool attack) noexcept {
    const auto agent = owner.agents().findByPlayer(player).agent;
    auto* entity = owner.entityFor(player);
    cstrike::WeaponObservation weapon{};
    const auto nav = owner.navConsole().runtimeState(owner, player);
    return current(owner, frame, player, agent, entity) && nav && nav->currentArea &&
        readWeapon(owner, frame, player, agent, dll, entity, weapon) &&
        weapon.activeWeapon == expected.value &&
        (!attack || (!weapon.reloading && weapon.clipAmmo > 0 &&
                     weapon.primaryAttackReadyMicros <= frame.nowMicros));
}

std::size_t buildRuntimeInputs(const LifecycleCoordinator& owner, const RuntimeFrame& frame,
    DLL_FUNCTIONS* dll, RuntimeActorInput* output, std::size_t capacity,
    RuntimeInputBuildStatus* status, std::size_t statusCapacity) noexcept {
    const auto statusFor = [&](core::PlayerId player) noexcept
        -> RuntimeInputBuildStatus* {
        if (!status || !player.isValid() || player.slot == 0 ||
            player.slot > statusCapacity)
            return nullptr;
        return &status[player.slot - 1U];
    };
    const auto initializeStatus = [&](RuntimeInputBuildStatus* target,
                                      core::PlayerId player,
                                      core::BotAgentId agent) noexcept {
        if (!target) return;
        *target = {};
        target->map = frame.map;
        target->round = frame.round;
        target->tick = frame.tick;
        target->nowMicros = frame.nowMicros;
        target->player = player;
        target->agent = agent;
    };
    if (!output || capacity == 0 || !frame.valid()) {
        if (status && statusCapacity != 0) {
            initializeStatus(status, {}, {});
            status->reason = RuntimeInputBuildReason::InvalidFrame;
        }
        return 0;
    }

    std::size_t count = 0;
    const auto clientMax = (std::min)(owner.registry().clientMax(),
                                      static_cast<std::uint16_t>(host::kMaxClientSlots));
    for (std::uint16_t slot = 1; slot <= clientMax && count < capacity; ++slot) {
        const auto player = owner.registry().currentPlayer(slot);
        const auto binding = owner.agents().findByPlayer(player);
        if (!player.isValid() || !binding.isValid() || binding.player != player ||
            binding.map != frame.map)
            continue;

        auto& input = output[count];
        input = {};
        input.player = player;
        input.agent = binding.agent;
        // Kept true for compatibility with older providers. Multi-actor
        // acceptance is determined by identity and valid(), not this flag.
        input.primary = true;
        auto* actorStatus = statusFor(player);
        initializeStatus(actorStatus, player, input.agent);
        auto* entity = owner.entityFor(player);
        if (!current(owner, frame, player, input.agent, entity)) {
            if (actorStatus) {
                actorStatus->reason = RuntimeInputBuildReason::StaleActor;
                actorStatus->staleReason =
                    staleReason(owner, frame, player, input.agent, entity);
            }
            ++count;
            continue;
        }

        const auto world = owner.world().latest(player);
        const auto nav = owner.navConsole().runtimeState(owner, player);
        if (!world) {
            if (actorStatus) actorStatus->reason = RuntimeInputBuildReason::MissingWorld;
            ++count;
            continue;
        }
        if (!nav) {
            if (actorStatus) actorStatus->reason = RuntimeInputBuildReason::MissingNav;
            ++count;
            continue;
        }
        if (actorStatus) actorStatus->currentAreaHeld = nav->currentAreaHeld;
        if (!nav->currentArea) {
            if (actorStatus) actorStatus->reason = RuntimeInputBuildReason::MissingCurrentArea;
            ++count;
            continue;
        }
        if (!nav->movement.position) {
            if (actorStatus) actorStatus->reason = RuntimeInputBuildReason::MissingPosition;
            ++count;
            continue;
        }

        cstrike::WeaponObservation weapon{};
        RuntimeInputBuildReason weaponFailure = RuntimeInputBuildReason::WeaponUnavailable;
        if (!readWeapon(owner, frame, player, input.agent, dll, entity, weapon,
                        &weaponFailure, actorStatus)) {
            if (actorStatus) actorStatus->reason = weaponFailure;
            ++count;
            continue;
        }
        if (actorStatus) {
            actorStatus->activeWeapon = weapon.activeWeapon;
            actorStatus->activeClass = weapon.activeClass;
        }

        input.world = *world;
        cstrike::CombatObservation combat{};
        combat.map = frame.map;
        combat.round = frame.round;
        combat.tick = frame.tick;
        combat.timeMicros = frame.nowMicros;
        combat.player = player;
        combat.agent = input.agent;
        combat.alive = true;
        combat.world = *world;
        combat.weapon = weapon;
        const auto* affiliation = owner.teams().find(player);
        if (!affiliation) {
            if (actorStatus) {
                actorStatus->reason = owner.teams().findBySlot(player.slot)
                    ? RuntimeInputBuildReason::TeamGenerationMismatch
                    : RuntimeInputBuildReason::MissingTeam;
            }
            ++count;
            continue;
        }
        if (affiliation->team == core::perception::Team::Unknown) {
            if (actorStatus) actorStatus->reason = RuntimeInputBuildReason::UnknownTeam;
            ++count;
            continue;
        }
        combat.team = affiliation->team;
        const auto& v = entity->v;
        combat.eye = {v.origin.x + v.view_ofs.x, v.origin.y + v.view_ofs.y,
                      v.origin.z + v.view_ofs.z};
        combat.view = {v.v_angle.x, std::remainder(v.v_angle.y, 360.0F), v.v_angle.z};
        const auto converted = cstrike::toCombatInput(combat);
        if (!converted) {
            if (actorStatus) actorStatus->reason = RuntimeInputBuildReason::CombatConversionFailed;
            ++count;
            continue;
        }
        input.combat = converted.input;

        const core::perception::Point position{v.origin.x, v.origin.y, v.origin.z};
        const float health = (std::min)(100.0F, v.health);
        RuntimeObjectiveObservation objective{};
        RuntimeEconomyObservation economy{};

        input.team.map = frame.map;
        input.team.round = frame.round;
        input.team.tick = frame.tick;
        input.team.nowMicros = frame.nowMicros;
        input.team.team = combat.team;
        input.team.objective = objective.team;
        input.teamObjectiveAvailable = objective.available;
        input.team.memberCount = 1;
        auto& member = input.team.members[0];
        member.player = player;
        member.agent = input.agent;
        member.position = position;
        member.healthPercent = health;
        member.connected = true;
        member.alive = true;
        member.team = combat.team;

        auto& self = input.tactical.self;
        self.player = player;
        self.agent = input.agent;
        self.team = combat.team;
        self.position = position;
        self.currentArea = *nav->currentArea;
        self.healthPercent = health;
        self.alive = true;
        input.tactical.objective = objective.tactical;
        input.tactical.economy = economy.tactical;
        input.tactical.navigation.roamGeneration = nav->roamGeneration;
        input.tactical.navigation.roamCandidateCount =
            (std::min)(nav->roamCandidateCount,
                       input.tactical.navigation.roamCandidates.size());
        for (std::size_t i = 0; i < input.tactical.navigation.roamCandidateCount; ++i)
            input.tactical.navigation.roamCandidates[i] = nav->roamCandidates[i];
        input.tacticalEvents.intentInvalidated = nav->roamArrived || nav->roamRejected;

        if (nav->routeExecutable && nav->goal && nav->goalPosition &&
            nav->movement.speedLimit && *nav->movement.speedLimit > 0) {
            auto& route = input.tactical.navigation.routes[0];
            route.target = {*nav->goal, *nav->goalPosition, nav->goal->value};
            route.style = nav->explicitRoute ? core::tactical::RouteStyle::Hold
                                              : core::tactical::RouteStyle::Roam;
            const auto& goal = *nav->goalPosition;
            const double eta =
                std::hypot(std::hypot(goal.x - position.x, goal.y - position.y),
                           goal.z - position.z) /
                *nav->movement.speedLimit * 1'000'000.0;
            if (std::isfinite(eta) && eta < 18446744073709551616.0) {
                route.etaMicros =
                    (std::max)(std::uint64_t{1}, static_cast<std::uint64_t>(eta));
                route.available = true;
                input.tactical.navigation.routeCount = 1;
                if (nav->explicitRoute) {
                    input.tactical.navigation.explicitRouteAvailable = true;
                    input.tactical.navigation.explicitRoute = route;
                }
            }
        }

        auto& action = input.action;
        action.map = frame.map;
        action.round = frame.round;
        action.tick = frame.tick;
        action.nowMicros = frame.nowMicros;
        action.player = player;
        action.agent = input.agent;
        action.alive = true;
        action.healthPercent = health;
        action.currentArea = *nav->currentArea;
        action.currentPosition = position;
        action.objective = objective.action;
        action.weapon.active = input.combat.weapon.active;
        action.weapon.activeClass = weapon.activeClass;
        action.weapon.clipAmmo = weapon.clipAmmo;
        action.weapon.reserveAmmo = weapon.reserveAmmo;
        action.weapon.reloading = weapon.reloading;
        action.weapon.canReload = weapon.canReload;
        action.weapon.canSwitch = weapon.canSwitch;

        const auto tactical =
            core::tactical::buildTacticalContext(*world, input.tactical);
        if (tactical.enemyCount) {
            const auto& enemy = tactical.enemies[0];
            action.enemy = {enemy.target, enemy.position,
                            frame.nowMicros - enemy.observedAgeMicros,
                            enemy.confidence, true, enemy.directVision,
                            enemy.directVision};
            input.actionObservation.enemyAppeared = enemy.directVision;
        }
        ++count;
    }
    return count;
}
}
