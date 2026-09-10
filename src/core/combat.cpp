// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include "core/combat.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace astrabot::core::combat {
namespace {

bool isFinitePoint(const perception::Point& point) noexcept {
    return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
}

bool isFiniteView(const ViewAngles& view) noexcept {
    return std::isfinite(view.pitch) && std::isfinite(view.yaw) && std::isfinite(view.roll);
}

bool inRange(const ViewAngles& view) noexcept {
    return view.pitch >= kMinPitch && view.pitch <= kMaxPitch &&
           view.yaw >= kMinYaw && view.yaw <= kMaxYaw &&
           view.roll >= kMinRoll && view.roll <= kMaxRoll;
}

bool validPlayer(PlayerId player) noexcept {
    return player.isValid() && player.slot <= 32;
}

bool sameStamp(const perception::Stamp& left, const perception::Stamp& right) noexcept {
    return left.agent == right.agent && left.observer == right.observer &&
           left.map == right.map && left.round == right.round &&
           left.tick == right.tick && left.timeMicros == right.timeMicros;
}

bool sameStamp(const world::WorldSnapshot& snapshot, const CombatInput& input) noexcept {
    const perception::Stamp expected{input.agent, input.player, input.map, input.tick,
                                    input.timeMicros, input.round};
    return sameStamp(snapshot.stamp, expected);
}

struct TargetCandidate {
    PlayerId target{};
    perception::ObservationSource source{perception::ObservationSource::Unknown};
    std::uint64_t observedMicros{0};
    std::uint64_t ageMicros{0};
    double confidence{0.0};
    double angularError{0.0};
};

struct RejectionFlags {
    bool unknownRelation{false};
    bool ally{false};
    bool stale{false};
    bool anonymousSound{false};
};

bool validCandidatePlayer(PlayerId player) noexcept {
    return player.isValid() && player.slot <= perception::kPlayerCapacity;
}

double shortestAngle(double delta) noexcept {
    while (delta > 180.0) delta -= 360.0;
    while (delta < -180.0) delta += 360.0;
    return delta;
}

bool calculateAngularError(const perception::Point& eye, const perception::Point& target,
                           const ViewAngles& view, double& error) noexcept {
    if (!isFinitePoint(target)) return false;
    const double dx = target.x - eye.x;
    const double dy = target.y - eye.y;
    const double dz = target.z - eye.z;
    const double horizontal = std::hypot(dx, dy);
    const double distance = std::hypot(horizontal, dz);
    if (!std::isfinite(distance) || distance <= 0.0) return false;

    constexpr double kDegreesPerRadian = 57.29577951308232;
    const double desiredPitch = std::atan2(-dz, horizontal) * kDegreesPerRadian;
    const double desiredYaw = std::atan2(dy, dx) * kDegreesPerRadian;
    const double pitchDelta = desiredPitch - static_cast<double>(view.pitch);
    const double yawDelta = shortestAngle(desiredYaw - static_cast<double>(view.yaw));
    error = std::hypot(pitchDelta, yawDelta);
    return std::isfinite(error);
}

bool calculateAim(const perception::Point& eye, const perception::Point& target,
                  const ViewAngles& current, ViewAngles& result) noexcept {
    if (!isFinitePoint(target)) return false;
    const double dx = target.x - eye.x;
    const double dy = target.y - eye.y;
    const double dz = target.z - eye.z;
    const double horizontal = std::hypot(dx, dy);
    const double distance = std::hypot(horizontal, dz);
    if (!std::isfinite(distance) || distance <= 0.0) return false;

    constexpr double kDegreesPerRadian = 57.29577951308232;
    const double desiredPitch = std::atan2(-dz, horizontal) * kDegreesPerRadian;
    const double desiredYaw = std::atan2(dy, dx) * kDegreesPerRadian;
    const double shortestYaw = static_cast<double>(current.yaw) +
                               shortestAngle(desiredYaw - static_cast<double>(current.yaw));
    result.pitch = static_cast<float>(std::clamp(desiredPitch, static_cast<double>(kMinPitch),
                                                  static_cast<double>(kMaxPitch)));
    result.yaw = static_cast<float>(shortestAngle(shortestYaw));
    result.roll = current.roll;
    return isFiniteView(result) && inRange(result);
}

constexpr std::uint64_t kViewScanPeriodMicros = 4'000'000;

ViewAngles scanView(const CombatInput& input) noexcept {
    ViewAngles result = input.view;
    const auto phase = (input.timeMicros % kViewScanPeriodMicros +
                        (static_cast<std::uint64_t>(input.agent.value) * 500'000U) %
                            kViewScanPeriodMicros) % kViewScanPeriodMicros;
    const double yaw = -180.0 +
                       360.0 * static_cast<double>(phase) /
                           static_cast<double>(kViewScanPeriodMicros);
    result.yaw = static_cast<float>(yaw);
    return result;
}

void preserveWorldMovement(const ViewAngles& from, const ViewAngles& to,
                           BotCommand& command) noexcept {
    constexpr double radiansPerDegree = 3.14159265358979323846 / 180.0;
    const double delta = (static_cast<double>(from.yaw) -
                         static_cast<double>(to.yaw)) * radiansPerDegree;
    const double cosine = std::cos(delta);
    const double sine = std::sin(delta);
    const double forward = static_cast<double>(command.movement.forward);
    const double side = static_cast<double>(command.movement.side);
    const double rotatedForward = forward * cosine + side * sine;
    const double rotatedSide = -forward * sine + side * cosine;
    if (std::isfinite(rotatedForward) && std::isfinite(rotatedSide) &&
        std::abs(rotatedForward) <= static_cast<double>(kMaxMovement) + 0.001 &&
        std::abs(rotatedSide) <= static_cast<double>(kMaxMovement) + 0.001) {
        command.movement.forward = static_cast<float>(rotatedForward);
        command.movement.side = static_cast<float>(rotatedSide);
    }
}

bool resolveTargetPoint(const CombatInput& input, const CombatDecision& selection,
                        perception::Point& point, std::uint64_t& observedMicros) noexcept {
    if (!selection.target.isValid()) return false;
    if (selection.source == perception::ObservationSource::Vision) {
        for (std::size_t i = 0; i < input.world.visual->count; ++i) {
            const auto& memory = input.world.visual->memories[i];
            if (memory.target == selection.target && memory.identity.source == selection.source) {
                point = memory.lastKnownPosition;
                observedMicros = memory.identity.observedMicros;
                return isFinitePoint(point);
            }
        }
    } else if (selection.source == perception::ObservationSource::TeamReport &&
               input.world.reports != nullptr) {
        for (std::size_t i = 0; i < input.world.reports->count; ++i) {
            const auto& report = input.world.reports->reports[i].report;
            if (report.target == selection.target &&
                report.identity.source == selection.source) {
                point = report.position;
                observedMicros = report.origin.observedMicros;
                return isFinitePoint(point);
            }
        }
    }
    return false;
}

std::uint64_t mix(std::uint64_t value) noexcept {
    value ^= value >> 30U;
    value *= UINT64_C(0xbf58476d1ce4e5b9);
    value ^= value >> 27U;
    value *= UINT64_C(0x94d049bb133111eb);
    return value ^ (value >> 31U);
}

std::uint64_t aimSeed(const CombatInput& input, const CombatDecision& selection,
                      std::uint64_t stream) noexcept {
    std::uint64_t seed = UINT64_C(0x9e3779b97f4a7c15) ^ stream;
    seed = mix(seed ^ input.agent.value);
    seed = mix(seed ^ input.map.value);
    seed = mix(seed ^ input.round.value);
    seed = mix(seed ^ input.tick.value);
    seed = mix(seed ^ (static_cast<std::uint64_t>(selection.target.slot) << 32U));
    seed = mix(seed ^ selection.target.generation.value);
    seed = mix(seed ^ static_cast<std::uint64_t>(selection.source));
    return seed;
}

double signedUnit(std::uint64_t seed) noexcept {
    constexpr double kUnitScale = 1.0 / 9007199254740992.0;
    return static_cast<double>(mix(seed) >> 11U) * kUnitScale * 2.0 - 1.0;
}

double effectiveError(float configured, std::uint8_t quality) noexcept {
    const double difficulty = static_cast<double>(100U - quality) / 100.0;
    return static_cast<double>(configured) * difficulty;
}

std::uint64_t reactionReadyAt(std::uint64_t observedMicros,
                              std::uint64_t delayMicros) noexcept {
    const auto maximum = (std::numeric_limits<std::uint64_t>::max)();
    if (observedMicros > maximum - delayMicros) return maximum;
    return observedMicros + delayMicros;
}

bool validVisualIdentity(const perception::ObservationIdentity& identity,
                         const CombatInput& input) noexcept {
    return identity.source == perception::ObservationSource::Vision &&
           identity.map == input.map && identity.round == input.round &&
           identity.validAt(input.timeMicros);
}

bool validReportIdentity(const world::TeamReport& report, const CombatInput& input) noexcept {
    return report.origin.source == perception::ObservationSource::Vision &&
           report.origin.map == input.map && report.origin.round == input.round &&
           report.origin.validAt(input.timeMicros) &&
           report.identity.source == perception::ObservationSource::TeamReport &&
           report.identity.map == input.map && report.identity.round == input.round &&
           report.identity.validAt(input.timeMicros) &&
           report.identity.observedMicros == report.origin.observedMicros &&
           report.origin.receivedMicros <= report.sentMicros &&
           report.sentMicros <= report.identity.receivedMicros &&
           report.sentMicros <= input.timeMicros;
}

bool validConfidence(double confidence, double maximum) noexcept {
    return std::isfinite(confidence) && confidence > 0.0 && confidence <= maximum;
}

int sourceRank(perception::ObservationSource source) noexcept {
    return source == perception::ObservationSource::Vision ? 0 : 1;
}

bool betterCandidate(const TargetCandidate& left, const TargetCandidate& right) noexcept {
    if (sourceRank(left.source) != sourceRank(right.source)) {
        return sourceRank(left.source) < sourceRank(right.source);
    }
    if (left.confidence != right.confidence) return left.confidence > right.confidence;
    if (left.observedMicros != right.observedMicros) return left.observedMicros > right.observedMicros;
    if (left.angularError != right.angularError) return left.angularError < right.angularError;
    return left.target < right.target;
}

void noteRelation(RejectionFlags& flags, perception::Relation relation) noexcept {
    if (relation == perception::Relation::Unknown) flags.unknownRelation = true;
    else if (relation == perception::Relation::Self || relation == perception::Relation::Ally) flags.ally = true;
}

CombatReason noTargetReason(const RejectionFlags& flags) noexcept {
    if (flags.unknownRelation) return CombatReason::UnknownRelation;
    if (flags.ally) return CombatReason::Ally;
    if (flags.stale) return CombatReason::StaleTarget;
    if (flags.anonymousSound) return CombatReason::AnonymousSound;
    return CombatReason::NoTarget;
}

CombatDecision validNoOp(const CombatInput& input, CombatReason reason) noexcept {
    auto decision = CombatDecision::noOp(input.tick, reason);
    decision.view = input.view;
    decision.validUntilMicros = input.timeMicros;
    return decision;
}

CombatDecision suppressedTrack(const CombatInput& input,
                               const CombatDecision& aim,
                               CombatReason reason) noexcept {
    auto decision = aim;
    decision.action = CombatAction::Track;
    decision.fireMode.reset();
    decision.firePlan.reset();
    decision.buttons = 0;
    decision.selectedWeapon = {};
    decision.reason = reason;
    decision.inputTick = input.tick;
    decision.validUntilMicros = input.timeMicros;
    return decision;
}

const world::VisualMemory* currentVisualFor(
    const CombatInput& input, PlayerId target) noexcept {
    if (input.world.visual == nullptr || !validCandidatePlayer(target) ||
        input.world.visual->count > input.world.visual->memories.size()) {
        return nullptr;
    }

    const auto& visual = *input.world.visual;
    if (visual.stamp.agent != input.agent ||
        visual.stamp.observer != input.player ||
        visual.stamp.map != input.map || visual.stamp.round != input.round ||
        visual.stamp.tick != input.tick ||
        visual.stamp.timeMicros != input.timeMicros) {
        return nullptr;
    }

    for (std::size_t i = 0; i < visual.count; ++i) {
        const auto& memory = visual.memories[i];
        if (memory.target != target ||
            !validVisualIdentity(memory.identity, input) ||
            memory.identity.observedMicros != visual.stamp.timeMicros ||
            memory.lastSeenMicros != memory.identity.observedMicros ||
            !isFinitePoint(memory.lastKnownPosition) ||
            !validConfidence(memory.confidence, 1.0)) {
            continue;
        }
        return &memory;
    }
    return nullptr;
}

bool inspectVisual(const CombatInput& input, const world::VisualMemory& memory,
                   TargetCandidate& candidate, RejectionFlags& flags) noexcept {
    if (!validCandidatePlayer(memory.target)) {
        flags.stale = true;
        return false;
    }
    const auto relation = input.world.relation(input.team, memory.target);
    if (relation != perception::Relation::Opponent) {
        noteRelation(flags, relation);
        return false;
    }
    if (!validVisualIdentity(memory.identity, input) ||
        memory.lastSeenMicros != memory.identity.observedMicros ||
        memory.lastSeenMicros > input.timeMicros ||
        !validConfidence(memory.confidence, 1.0) ||
        !calculateAngularError(input.eye, memory.lastKnownPosition, input.view, candidate.angularError)) {
        flags.stale = true;
        return false;
    }
    candidate.target = memory.target;
    candidate.source = perception::ObservationSource::Vision;
    candidate.observedMicros = memory.identity.observedMicros;
    candidate.ageMicros = input.timeMicros - candidate.observedMicros;
    candidate.confidence = memory.confidence;
    return true;
}

bool inspectReport(const CombatInput& input, const world::ReportMemory& memory,
                   TargetCandidate& candidate, RejectionFlags& flags) noexcept {
    const auto& report = memory.report;
    if (!validCandidatePlayer(report.reporter) || !validCandidatePlayer(report.target) ||
        report.reporter == report.target || report.reporter == report.receiver ||
        report.receiver != input.player ||
        report.receiver == report.target) {
        flags.stale = true;
        return false;
    }
    const auto reporterRelation = input.world.relation(input.team, report.reporter);
    if (reporterRelation != perception::Relation::Ally) {
        noteRelation(flags, reporterRelation);
        if (reporterRelation == perception::Relation::Opponent) flags.stale = true;
        return false;
    }
    const auto relation = input.world.relation(input.team, report.target);
    if (relation != perception::Relation::Opponent) {
        noteRelation(flags, relation);
        return false;
    }
    if (!validReportIdentity(report, input) ||
        !validConfidence(memory.confidence, 0.5) ||
        !calculateAngularError(input.eye, report.position, input.view, candidate.angularError)) {
        flags.stale = true;
        return false;
    }
    candidate.target = report.target;
    candidate.source = perception::ObservationSource::TeamReport;
    candidate.observedMicros = report.origin.observedMicros;
    candidate.ageMicros = input.timeMicros - candidate.observedMicros;
    candidate.confidence = memory.confidence;
    return true;
}

constexpr double kMediumCombatRange = 512.0;
constexpr double kLongCombatRange = 1024.0;
constexpr double kPoorAimErrorDegrees = 8.0;
constexpr std::uint8_t kFullAutoQualityThreshold = 60;
constexpr std::uint8_t kMaxFullAutoShots = kMaxBurstShots;
constexpr std::uint64_t kBurstPauseMicros = 100'000;

using WeaponClass = WeaponSnapshot::WeaponClass;

bool currentVisionThreat(const CombatInput& input,
                         const CombatDecision& aim) noexcept {
    if (aim.action != CombatAction::Track ||
        aim.source != perception::ObservationSource::Vision || !aim.target.isValid()) {
        return false;
    }
    const auto* visual = currentVisualFor(input, aim.target);
    return visual != nullptr && input.world.relation(input.team, aim.target) ==
                                      perception::Relation::Opponent;
}

bool targetDistanceAndAimError(const CombatInput& input, PlayerId target,
                               double& distance, double& aimError) noexcept {
    const auto* visual = currentVisualFor(input, target);
    if (visual == nullptr) return false;

    const double dx = visual->lastKnownPosition.x - input.eye.x;
    const double dy = visual->lastKnownPosition.y - input.eye.y;
    const double dz = visual->lastKnownPosition.z - input.eye.z;
    distance = std::hypot(std::hypot(dx, dy), dz);
    if (!std::isfinite(distance) || distance <= 0.0 ||
        !calculateAngularError(input.eye, visual->lastKnownPosition, input.view, aimError)) {
        return false;
    }
    return true;
}

FirePlan boundedBurst(std::uint8_t requested, std::int32_t clipAmmo) noexcept {
    const auto available = static_cast<std::uint8_t>(std::min(
        std::max(clipAmmo, 0), static_cast<std::int32_t>(kMaxBurstShots)));
    const auto shots = std::min(requested, available);
    return shots >= 2U ? FirePlan::burst(shots) : FirePlan::tap();
}

FirePlan chooseFirePlan(const CombatInput& input, PlayerId target) noexcept {
    double distance = 0.0;
    double aimError = 0.0;
    if (!targetDistanceAndAimError(input, target, distance, aimError) ||
        input.weapon.clipAmmo <= 0) {
        return FirePlan::tap();
    }

    const auto quality = input.difficulty.decisionQuality;
    const bool poorAim = aimError > kPoorAimErrorDegrees;
    switch (input.weapon.activeClass) {
    case WeaponClass::Rifle:
    case WeaponClass::MachineGun:
        if (distance >= kLongCombatRange || poorAim) return FirePlan::tap();
        if (distance >= kMediumCombatRange || quality < kFullAutoQualityThreshold) {
            return boundedBurst(3, input.weapon.clipAmmo);
        }
        return input.weapon.clipAmmo >= 2 ? FirePlan::fullAuto() : FirePlan::tap();
    case WeaponClass::SMG:
        if (distance >= kLongCombatRange || poorAim) return FirePlan::tap();
        if (distance >= kMediumCombatRange || quality < kFullAutoQualityThreshold) {
            return boundedBurst(5, input.weapon.clipAmmo);
        }
        return input.weapon.clipAmmo >= 2 ? FirePlan::fullAuto() : FirePlan::tap();
    case WeaponClass::Pistol:
    case WeaponClass::Shotgun:
    case WeaponClass::Melee:
    case WeaponClass::Sniper:
    case WeaponClass::Unknown:
        return FirePlan::tap();
    }
    return FirePlan::tap();
}

bool isSwitchableWeaponId(WeaponId weapon) noexcept {
    switch (weapon.value) {
    case 1: case 3: case 5: case 7: case 8: case 10: case 11:
    case 12: case 13: case 14: case 15: case 16: case 17: case 18:
    case 19: case 20: case 21: case 22: case 23: case 24: case 26:
    case 27: case 28: case 29: case 30:
        return true;
    default:
        return false;
    }
}

WeaponId preferredSwitchWeapon(const WeaponSnapshot& weapon) noexcept {
    WeaponId selected{};
    for (std::size_t i = 0; i < weapon.ownedCount; ++i) {
        const auto candidate = weapon.owned[i];
        if (!candidate.isValid() || candidate == weapon.active) continue;
        if (!isSwitchableWeaponId(candidate)) continue;
        if (!selected.isValid() || candidate < selected) selected = candidate;
    }
    return selected;
}

CombatDecision acceptedReload(const CombatInput& input) noexcept {
    auto decision = validNoOp(input, CombatReason::Accepted);
    decision.action = CombatAction::Reload;
    decision.buttons = static_cast<ButtonMask>(Button::Reload);
    return decision;
}

CombatDecision acceptedSwitch(const CombatInput& input, WeaponId weapon) noexcept {
    auto decision = validNoOp(input, CombatReason::Accepted);
    decision.action = CombatAction::SwitchWeapon;
    decision.selectedWeapon = weapon;
    return decision;
}

void clearCadence(AttackLifecycleState& state) noexcept {
    state.cadenceTarget = {};
    state.cadenceWeapon = {};
    state.cadencePlan = FirePlan::tap();
    state.cadenceShotsFired = 0;
    state.cadencePauseUntilMicros = 0;
    state.cadenceActive = false;
    state.attackHeld = false;
}

void clearReaction(AttackLifecycleState& state) noexcept {
    state.reactionTarget = {};
    state.reactionStartedMicros = 0;
    state.reactionActive = false;
}

void markAction(AttackLifecycleState& state, const CombatInput& input,
                CombatAction action) noexcept {
    state.map = input.map;
    state.round = input.round;
    state.player = input.player;
    state.agent = input.agent;
    state.lastActionTick = input.tick;
    state.lastAction = action;
    state.initialized = true;
}

std::uint8_t cadenceLimit(const FirePlan& plan) noexcept {
    switch (plan.pattern) {
    case FirePattern::Burst:
        return plan.burstShots;
    case FirePattern::FullAuto:
        return kMaxFullAutoShots;
    case FirePattern::Tap:
        return 1;
    }
    return 1;
}

std::optional<CombatDecision> planWeaponAction(
    const CombatInput& input, const CombatDecision& aim,
    AttackLifecycleState& state) noexcept {
    if (input.weapon.reloading) {
        clearCadence(state);
        clearReaction(state);
        if (aim.action == CombatAction::Track && aim.validateForP5()) {
            return suppressedTrack(input, aim, CombatReason::Reloading);
        }
        return validNoOp(input, CombatReason::Reloading);
    }

    const bool threat = currentVisionThreat(input, aim);
    if (input.weapon.clipAmmo <= input.weapon.reloadClipThreshold &&
        input.weapon.reserveAmmo > 0 && input.weapon.canReload &&
        (input.weapon.clipAmmo == 0 || !threat)) {
        clearCadence(state);
        clearReaction(state);
        return acceptedReload(input);
    }

    if (input.weapon.clipAmmo <= 0 && input.weapon.reserveAmmo <= 0 &&
        input.weapon.activeClass != WeaponClass::Melee) {
        const auto candidate = preferredSwitchWeapon(input.weapon);
        if (input.weapon.canSwitch && candidate.isValid()) {
            clearCadence(state);
            clearReaction(state);
            return acceptedSwitch(input, candidate);
        }
        clearCadence(state);
        clearReaction(state);
        return validNoOp(input, CombatReason::NoUsableWeapon);
    }

    if (input.weapon.clipAmmo <= 0 && input.weapon.reserveAmmo > 0 &&
        !input.weapon.canReload) {
        const auto candidate = preferredSwitchWeapon(input.weapon);
        if (input.weapon.canSwitch && candidate.isValid()) {
            clearCadence(state);
            clearReaction(state);
            return acceptedSwitch(input, candidate);
        }
    }
    return std::nullopt;
}

CombatReason rejectionReason(CombatInputError error) noexcept {
    switch (error) {
    case CombatInputError::None:
        return CombatReason::None;
    case CombatInputError::InvalidActor:
        return CombatReason::InvalidActor;
    case CombatInputError::InvalidMap:
        return CombatReason::InvalidMap;
    case CombatInputError::InvalidRound:
        return CombatReason::InvalidRound;
    case CombatInputError::InvalidTick:
        return CombatReason::InvalidTick;
    case CombatInputError::InvalidWorldSnapshot:
        return CombatReason::InvalidWorldSnapshot;
    case CombatInputError::StaleWorldSnapshot:
        return CombatReason::StaleInput;
    case CombatInputError::NonFinitePose:
        return CombatReason::NonFinitePose;
    case CombatInputError::ViewOutOfRange:
        return CombatReason::ViewOutOfRange;
    case CombatInputError::StaleWeapon:
        return CombatReason::StaleWeapon;
    case CombatInputError::InvalidWeapon:
        return CombatReason::InvalidWeapon;
    case CombatInputError::ImpossibleAmmo:
        return CombatReason::ImpossibleAmmo;
    case CombatInputError::InvalidDifficulty:
        return CombatReason::InvalidDifficulty;
    }
    return CombatReason::InvalidInput;
}

} // namespace

WeaponValidation WeaponSnapshot::validate() const noexcept {
    if (!map.isValid() || !round.isValid() || !tick.isValid()) {
        return {WeaponValidationError::InvalidIdentity};
    }
    if (!active.isValid()) {
        return {WeaponValidationError::InvalidActiveWeapon};
    }
    if (static_cast<std::uint8_t>(activeClass) >
        static_cast<std::uint8_t>(WeaponClass::Melee)) {
        return {WeaponValidationError::InvalidWeaponClass};
    }
    if (ownedCount == 0 || ownedCount > owned.size()) {
        return {WeaponValidationError::InvalidInventory};
    }
    if (clipAmmo < 0 || clipAmmo > kMaxAmmo || reserveAmmo < 0 || reserveAmmo > kMaxAmmo) {
        return {WeaponValidationError::ImpossibleAmmo};
    }
    if (reloadClipThreshold < 0 || reloadClipThreshold > kMaxAmmo) {
        return {WeaponValidationError::InvalidReloadThreshold};
    }

    bool activeOwned = false;
    for (std::size_t i = 0; i < ownedCount; ++i) {
        if (!owned[i].isValid()) {
            return {WeaponValidationError::InvalidInventory};
        }
        if (owned[i] == active) {
            activeOwned = true;
        }
        for (std::size_t j = 0; j < i; ++j) {
            if (owned[j] == owned[i]) {
                return {WeaponValidationError::DuplicateWeapon};
            }
        }
    }
    if (!activeOwned) {
        return {WeaponValidationError::InvalidActiveWeapon};
    }
    return {};
}

bool WeaponSnapshot::owns(WeaponId weapon) const noexcept {
    if (!weapon.isValid() || ownedCount > owned.size()) {
        return false;
    }
    for (std::size_t i = 0; i < ownedCount; ++i) {
        if (owned[i] == weapon) {
            return true;
        }
    }
    return false;
}

bool operator==(const WeaponSnapshot& left, const WeaponSnapshot& right) noexcept {
    if (left.map != right.map || left.round != right.round || left.tick != right.tick ||
        left.observedMicros != right.observedMicros || left.active != right.active ||
        left.activeClass != right.activeClass ||
        left.ownedCount != right.ownedCount || left.clipAmmo != right.clipAmmo ||
        left.reserveAmmo != right.reserveAmmo ||
        left.reloadClipThreshold != right.reloadClipThreshold ||
        left.reloading != right.reloading ||
        left.canReload != right.canReload || left.canSwitch != right.canSwitch ||
        left.primaryAttackReadyMicros != right.primaryAttackReadyMicros) {
        return false;
    }
    for (std::size_t i = 0; i < left.ownedCount && i < left.owned.size(); ++i) {
        if (left.owned[i] != right.owned[i]) {
            return false;
        }
    }
    return true;
}

bool DifficultySettings::valid() const noexcept {
    return reactionDelayMicros <= kMaxReactionDelayMicros &&
           std::isfinite(observationErrorDegrees) &&
           std::isfinite(predictionErrorDegrees) &&
           std::isfinite(aimNoiseDegrees) &&
           observationErrorDegrees >= 0.0F &&
           observationErrorDegrees <= kMaxDifficultyErrorDegrees &&
           predictionErrorDegrees >= 0.0F &&
           predictionErrorDegrees <= kMaxDifficultyErrorDegrees &&
           aimNoiseDegrees >= 0.0F && aimNoiseDegrees <= kMaxDifficultyErrorDegrees &&
           decisionQuality <= kMaxDecisionQuality;
}

bool FirePlan::valid() const noexcept {
    switch (pattern) {
    case FirePattern::Tap:
        return burstShots == 1;
    case FirePattern::Burst:
        return burstShots >= 2 && burstShots <= kMaxBurstShots;
    case FirePattern::FullAuto:
        return burstShots == 0;
    }
    return false;
}

CombatDecision CombatDecision::noOp(TickId tick, CombatReason decisionReason) noexcept {
    CombatDecision decision{};
    decision.reason = decisionReason;
    decision.inputTick = tick;
    return decision;
}

DecisionValidation CombatDecision::validate() const noexcept {
    if (!inputTick.isValid()) {
        return {DecisionValidation::Error::InvalidTick};
    }
    if (!isFiniteView(view)) {
        return {DecisionValidation::Error::NonFiniteView};
    }
    if (!inRange(view)) {
        return {DecisionValidation::Error::ViewOutOfRange};
    }
    if ((buttons & ~kKnownButtonMask) != 0U) {
        return {DecisionValidation::Error::UnknownButtons};
    }
    if (!std::isfinite(confidence) || confidence < 0.0 || confidence > 1.0) {
        return {DecisionValidation::Error::InvalidKnowledge};
    }
    if (action == CombatAction::Fire) {
        if (!target.isValid()) {
            return {DecisionValidation::Error::InvalidTarget};
        }
        if (!fireMode.has_value()) {
            return {DecisionValidation::Error::MissingFireMode};
        }
        if (!firePlan.has_value()) {
            return {DecisionValidation::Error::MissingFirePlan};
        }
        if (!firePlan->valid()) {
            return {DecisionValidation::Error::InvalidFirePlan};
        }
        if ((buttons & static_cast<ButtonMask>(Button::Attack)) == 0U) {
            return {DecisionValidation::Error::MissingAttackButton};
        }
        if ((buttons & static_cast<ButtonMask>(Button::Reload)) != 0U) {
            return {DecisionValidation::Error::UnexpectedReloadButton};
        }
    } else {
        if ((buttons & static_cast<ButtonMask>(Button::Attack)) != 0U) {
            return {DecisionValidation::Error::UnexpectedAttackButton};
        }
        if (action == CombatAction::Reload &&
            (buttons & static_cast<ButtonMask>(Button::Reload)) == 0U) {
            return {DecisionValidation::Error::MissingReloadButton};
        }
        if (action != CombatAction::Reload &&
            (buttons & static_cast<ButtonMask>(Button::Reload)) != 0U) {
            return {DecisionValidation::Error::UnexpectedReloadButton};
        }
        if (fireMode.has_value()) {
            return {DecisionValidation::Error::UnexpectedFireMode};
        }
        if (firePlan.has_value()) {
            return {DecisionValidation::Error::UnexpectedFirePlan};
        }
    }
    if (action == CombatAction::SwitchWeapon && !selectedWeapon.isValid()) {
        return {DecisionValidation::Error::InvalidSelectedWeapon};
    }
    return {};
}

DecisionValidation CombatDecision::validateForP5() const noexcept {
    const auto structural = validate();
    if (!structural || (action == CombatAction::Fire && *fireMode != FireMode::DirectFire)) {
        if (!structural) {
            return structural;
        }
        return {DecisionValidation::Error::UnsupportedFireMode};
    }
    return {};
}

bool CombatDecision::hasAttackInput() const noexcept {
    return action == CombatAction::Fire &&
           (buttons & static_cast<ButtonMask>(Button::Attack)) != 0U;
}

CombatInputValidation CombatInput::validate() const noexcept {
    if (!validPlayer(player) || !agent.isValid()) {
        return {CombatInputError::InvalidActor};
    }
    if (!map.isValid()) {
        return {CombatInputError::InvalidMap};
    }
    if (!round.isValid()) {
        return {CombatInputError::InvalidRound};
    }
    if (!tick.isValid()) {
        return {CombatInputError::InvalidTick};
    }
    if (world.visual == nullptr || world.sounds == nullptr) {
        return {CombatInputError::InvalidWorldSnapshot};
    }
    if (!sameStamp(world, *this)) {
        return {CombatInputError::StaleWorldSnapshot};
    }
    if (!sameStamp(world.visual->stamp, world.stamp) ||
        !sameStamp(world.sounds->stamp, world.stamp)) {
        return {CombatInputError::StaleWorldSnapshot};
    }
    if (world.visual->count > world.visual->memories.size() ||
        world.sounds->count > world.sounds->sounds.size()) {
        return {CombatInputError::InvalidWorldSnapshot};
    }
    if (world.reports != nullptr && world.reports->count > world.reports->reports.size()) {
        return {CombatInputError::InvalidWorldSnapshot};
    }
    if (world.reports != nullptr && !sameStamp(world.reports->stamp, world.stamp)) {
        return {CombatInputError::StaleWorldSnapshot};
    }
    if (!isFinitePoint(eye) || !isFiniteView(view)) {
        return {CombatInputError::NonFinitePose};
    }
    if (!inRange(view)) {
        return {CombatInputError::ViewOutOfRange};
    }
    const auto weaponValidation = weapon.validate();
    if (!weaponValidation) {
        switch (weaponValidation.error) {
        case WeaponValidationError::InvalidIdentity:
            return {CombatInputError::StaleWeapon};
        case WeaponValidationError::ImpossibleAmmo:
            return {CombatInputError::ImpossibleAmmo};
        case WeaponValidationError::None:
            break;
        default:
            return {CombatInputError::InvalidWeapon};
        }
    }
    if (weapon.map != map || weapon.round != round || weapon.tick != tick ||
        weapon.observedMicros != timeMicros) {
        return {CombatInputError::StaleWeapon};
    }
    if (!difficulty.valid()) {
        return {CombatInputError::InvalidDifficulty};
    }
    return {};
}

CombatDecision CombatInput::reject() const noexcept {
    const auto validation = validate();
    return CombatDecision::noOp(tick, rejectionReason(validation.error));
}

CombatDecision selectTarget(const CombatInput& input) noexcept {
    const auto validation = input.validate();
    if (!validation) return input.reject();
    if (!input.alive) return validNoOp(input, CombatReason::Dead);

    TargetCandidate best{};
    bool selected = false;
    RejectionFlags flags{};
    for (std::size_t i = 0; i < input.world.visual->count; ++i) {
        TargetCandidate candidate{};
        if (inspectVisual(input, input.world.visual->memories[i], candidate, flags) &&
            (!selected || betterCandidate(candidate, best))) {
            best = candidate;
            selected = true;
        }
    }
    if (input.world.reports != nullptr) {
        for (std::size_t i = 0; i < input.world.reports->count; ++i) {
            TargetCandidate candidate{};
            if (inspectReport(input, input.world.reports->reports[i], candidate, flags) &&
                (!selected || betterCandidate(candidate, best))) {
                best = candidate;
                selected = true;
            }
        }
    }
    if (selected) {
        CombatDecision decision{};
        decision.action = CombatAction::Track;
        decision.target = best.target;
        decision.view = input.view;
        decision.source = best.source;
        decision.targetAgeMicros = best.ageMicros;
        decision.confidence = best.confidence;
        decision.reason = CombatReason::Accepted;
        decision.inputTick = input.tick;
        decision.validUntilMicros = input.timeMicros;
        return decision;
    }

    flags.anonymousSound = input.world.sounds->count != 0;
    return validNoOp(input, noTargetReason(flags));
}

CombatDecision aimTarget(const CombatInput& input) noexcept {
    auto decision = selectTarget(input);
    if (decision.action != CombatAction::Track) {
        if (input.validate()) decision.view = scanView(input);
        return decision;
    }

    perception::Point targetPoint{};
    std::uint64_t observedMicros = 0;
    if (!resolveTargetPoint(input, decision, targetPoint, observedMicros)) {
        return validNoOp(input, CombatReason::StaleTarget);
    }

    ViewAngles view{};
    if (!calculateAim(input.eye, targetPoint, input.view, view)) {
        return validNoOp(input, CombatReason::StaleTarget);
    }

    const double observationError =
        effectiveError(input.difficulty.observationErrorDegrees, input.difficulty.decisionQuality) *
        signedUnit(aimSeed(input, decision, UINT64_C(0x6f62736572766174)));
    const double predictionError =
        effectiveError(input.difficulty.predictionErrorDegrees, input.difficulty.decisionQuality) *
        signedUnit(aimSeed(input, decision, UINT64_C(0x70726564696374)));
    const double aimNoise =
        effectiveError(input.difficulty.aimNoiseDegrees, input.difficulty.decisionQuality) *
        signedUnit(aimSeed(input, decision, UINT64_C(0x61696d2d6e6f6973)));
    const double totalError = observationError + predictionError + aimNoise;
    view.pitch = static_cast<float>(std::clamp(
        static_cast<double>(view.pitch) + totalError, static_cast<double>(kMinPitch),
        static_cast<double>(kMaxPitch)));
    view.yaw = static_cast<float>(std::clamp(
        shortestAngle(static_cast<double>(view.yaw) + totalError),
        static_cast<double>(kMinYaw), static_cast<double>(kMaxYaw)));
    if (!isFiniteView(view) || !inRange(view)) {
        return validNoOp(input, CombatReason::StaleTarget);
    }

    decision.view = view;
    decision.validUntilMicros = input.timeMicros;
    if (input.timeMicros < reactionReadyAt(observedMicros, input.difficulty.reactionDelayMicros)) {
        decision.reason = CombatReason::ReactionDelay;
    }
    return decision;
}

FireAuthorization authorizeFire(const CombatInput& input,
                                const CombatDecision& aim,
                                AttackLifecycleState previous) noexcept {
    FireAuthorization result{};
    result.decision = validNoOp(input, CombatReason::InvalidInput);
    result.nextState = previous.sameContext(input) ? previous : AttackLifecycleState{};

    const auto validation = input.validate();
    if (!validation) {
        result.decision = input.reject();
        return result;
    }
    if (!input.alive) {
        clearCadence(result.nextState);
        clearReaction(result.nextState);
        result.decision = validNoOp(input, CombatReason::Dead);
        return result;
    }
    if (result.nextState.initialized &&
        result.nextState.lastActionTick == input.tick) {
        const auto duplicateReason = result.nextState.lastAction == CombatAction::Fire
                                          ? CombatReason::DuplicateAttack
                                          : CombatReason::DuplicateAction;
        clearCadence(result.nextState);
        clearReaction(result.nextState);
        result.decision = validNoOp(input, duplicateReason);
        return result;
    }

    if (const auto weaponAction = planWeaponAction(input, aim, result.nextState);
        weaponAction.has_value()) {
        result.decision = *weaponAction;
        if (result.decision.action == CombatAction::Reload ||
            result.decision.action == CombatAction::SwitchWeapon) {
            if (!result.decision.validateForP5()) {
                clearCadence(result.nextState);
                result.decision = validNoOp(input, CombatReason::InvalidInput);
                return result;
            }
            markAction(result.nextState, input, result.decision.action);
        }
        return result;
    }

    if (aim.action == CombatAction::NoOp) {
        clearCadence(result.nextState);
        clearReaction(result.nextState);
        result.decision = aim;
        return result;
    }
    if (aim.action != CombatAction::Track || aim.inputTick != input.tick ||
        aim.validUntilMicros != input.timeMicros ||
        !aim.validateForP5()) {
        clearCadence(result.nextState);
        clearReaction(result.nextState);
        result.decision = validNoOp(input, CombatReason::InvalidInput);
        return result;
    }
    if ((aim.reason != CombatReason::Accepted &&
         aim.reason != CombatReason::ReactionDelay) ||
        aim.source != perception::ObservationSource::Vision ||
        !aim.target.isValid()) {
        clearCadence(result.nextState);
        clearReaction(result.nextState);
        result.decision = validNoOp(input, CombatReason::InvalidVisibility);
        return result;
    }

    const auto* visual = currentVisualFor(input, aim.target);
    if (visual == nullptr) {
        clearCadence(result.nextState);
        clearReaction(result.nextState);
        result.decision = validNoOp(input, CombatReason::InvalidVisibility);
        return result;
    }
    const auto relation = input.world.relation(input.team, aim.target);
    if (relation == perception::Relation::Unknown) {
        clearCadence(result.nextState);
        clearReaction(result.nextState);
        result.decision = validNoOp(input, CombatReason::UnknownRelation);
        return result;
    }
    if (relation != perception::Relation::Opponent) {
        clearCadence(result.nextState);
        clearReaction(result.nextState);
        result.decision = validNoOp(input, CombatReason::Ally);
        return result;
    }
    if (!result.nextState.reactionActive ||
        result.nextState.reactionTarget != aim.target) {
        result.nextState.map = input.map;
        result.nextState.round = input.round;
        result.nextState.player = input.player;
        result.nextState.agent = input.agent;
        result.nextState.initialized = true;
        result.nextState.reactionTarget = aim.target;
        result.nextState.reactionStartedMicros = visual->identity.observedMicros;
        result.nextState.reactionActive = true;
    }
    const auto reactionReady = reactionReadyAt(
        result.nextState.reactionStartedMicros,
        input.difficulty.reactionDelayMicros);
    if (input.timeMicros < reactionReady) {
        clearCadence(result.nextState);
        result.decision = suppressedTrack(input, aim, CombatReason::ReactionDelay);
        return result;
    }
    if (input.weapon.clipAmmo <= 0 && input.weapon.activeClass != WeaponClass::Melee) {
        clearCadence(result.nextState);
        clearReaction(result.nextState);
        result.decision = suppressedTrack(input, aim, CombatReason::EmptyClip);
        return result;
    }
    if (input.weapon.primaryAttackReadyMicros > input.timeMicros) {
        result.decision = suppressedTrack(input, aim, CombatReason::Cooldown);
        return result;
    }

    if (result.nextState.cadenceActive &&
        (result.nextState.cadenceTarget != aim.target ||
         result.nextState.cadenceWeapon != input.weapon.active)) {
        clearCadence(result.nextState);
        clearReaction(result.nextState);
    }
    if (result.nextState.cadenceActive &&
        result.nextState.cadencePauseUntilMicros > input.timeMicros) {
        result.decision = suppressedTrack(input, aim, CombatReason::Cooldown);
        return result;
    }
    if (result.nextState.cadenceActive &&
        result.nextState.cadencePauseUntilMicros != 0 &&
        result.nextState.cadencePauseUntilMicros <= input.timeMicros) {
        clearCadence(result.nextState);
        clearReaction(result.nextState);
    }

    FirePlan plan = result.nextState.cadenceActive
                        ? result.nextState.cadencePlan
                        : chooseFirePlan(input, aim.target);
    if (!plan.valid()) {
        clearCadence(result.nextState);
        plan = FirePlan::tap();
    }
    if (result.nextState.cadenceActive && plan.pattern == FirePattern::Burst &&
        result.nextState.cadenceShotsFired >= plan.burstShots) {
        clearCadence(result.nextState);
        plan = chooseFirePlan(input, aim.target);
    }
    if (result.nextState.cadenceActive && plan.pattern == FirePattern::Burst) {
        const auto remaining = static_cast<std::uint8_t>(
            plan.burstShots - result.nextState.cadenceShotsFired);
        if (input.weapon.clipAmmo < remaining) {
            clearCadence(result.nextState);
            plan = boundedBurst(remaining, input.weapon.clipAmmo);
        }
    }

    auto decision = aim;
    decision.action = CombatAction::Fire;
    decision.fireMode = FireMode::DirectFire;
    decision.firePlan = plan;
    decision.buttons = static_cast<ButtonMask>(Button::Attack);
    decision.selectedWeapon = input.weapon.active;
    decision.source = perception::ObservationSource::Vision;
    decision.targetAgeMicros =
        input.timeMicros - visual->identity.observedMicros;
    decision.confidence = visual->confidence;
    decision.reason = CombatReason::Accepted;
    decision.inputTick = input.tick;
    decision.validUntilMicros = input.timeMicros;
    if (!decision.validateForP5()) {
        clearCadence(result.nextState);
        result.decision = validNoOp(input, CombatReason::InvalidInput);
        return result;
    }

    result.decision = decision;
    markAction(result.nextState, input, CombatAction::Fire);
    result.nextState.lastFireTick = input.tick;
    result.nextState.lastFireMicros = input.timeMicros;
    if (plan.pattern == FirePattern::Burst || plan.pattern == FirePattern::FullAuto) {
        if (!result.nextState.cadenceActive) {
            result.nextState.cadenceTarget = aim.target;
            result.nextState.cadenceWeapon = input.weapon.active;
            result.nextState.cadencePlan = plan;
            result.nextState.cadenceShotsFired = 0;
            result.nextState.cadencePauseUntilMicros = 0;
            result.nextState.cadenceActive = true;
        }
        ++result.nextState.cadenceShotsFired;
        if (result.nextState.cadenceShotsFired >= cadenceLimit(plan)) {
            const auto maximum = (std::numeric_limits<std::uint64_t>::max)();
            result.nextState.cadencePauseUntilMicros =
                input.timeMicros > maximum - kBurstPauseMicros
                    ? maximum
                    : input.timeMicros + kBurstPauseMicros;
        }
    } else {
        clearCadence(result.nextState);
    }
    return result;
}

CommandCompositionResult composeCommand(
    const CombatDecision& combat, const BotCommand& navigation) noexcept {
    const auto decisionValidation = combat.validateForP5();
    if (!decisionValidation) {
        return {{}, CommandCompositionError::InvalidDecision, false};
    }
    if (!navigation.validate()) {
        return {{}, CommandCompositionError::InvalidNavigationCommand, false};
    }

    constexpr ButtonMask combatButtons =
        static_cast<ButtonMask>(Button::Attack) |
        static_cast<ButtonMask>(Button::Reload);
    BotCommand command = navigation;
    preserveWorldMovement(navigation.view, combat.view, command);
    command.view = combat.view;
    command.buttons = (navigation.buttons & ~combatButtons) |
                      (combat.buttons & combatButtons);
    command.weaponSelect = combat.action == CombatAction::SwitchWeapon
                               ? combat.selectedWeapon.value
                               : kNoWeaponSelection;
    if (!command.validate()) {
        return {{}, CommandCompositionError::InvalidNavigationCommand, false};
    }
    return {command, CommandCompositionError::None, true};
}

} // namespace astrabot::core::combat
