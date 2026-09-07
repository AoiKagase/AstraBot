// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include "core/combat.hpp"

#include <cmath>

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
    if (ownedCount == 0 || ownedCount > owned.size()) {
        return {WeaponValidationError::InvalidInventory};
    }
    if (clipAmmo < 0 || clipAmmo > kMaxAmmo || reserveAmmo < 0 || reserveAmmo > kMaxAmmo) {
        return {WeaponValidationError::ImpossibleAmmo};
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
        left.ownedCount != right.ownedCount || left.clipAmmo != right.clipAmmo ||
        left.reserveAmmo != right.reserveAmmo || left.reloading != right.reloading ||
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
        if ((buttons & static_cast<ButtonMask>(Button::Attack)) == 0U) {
            return {DecisionValidation::Error::MissingAttackButton};
        }
    } else if (fireMode.has_value()) {
        return {DecisionValidation::Error::UnexpectedFireMode};
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

} // namespace astrabot::core::combat
