// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 AstraBot contributors.

#include "core/experience.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <utility>

namespace astrabot::core::experience {
namespace {

constexpr std::size_t kMaxFileBytes = 16U * 1024U * 1024U;
constexpr std::size_t kHeaderBytes = 8U + 4U + 4U + 8U + 8U;
constexpr char kMagic[] = "ABEXPER1";

bool finiteNonNegative(double value) noexcept {
    return std::isfinite(value) && value >= 0.0;
}

bool validHash(const ContentHash& hash) noexcept {
    return std::any_of(hash.begin(), hash.end(), [](std::uint8_t value) {
        return value != 0U;
    });
}

std::uint64_t checksum(const std::vector<std::uint8_t>& bytes) noexcept {
    std::uint64_t result = 1469598103934665603ULL;
    for (const auto value : bytes) {
        result ^= value;
        result *= 1099511628211ULL;
    }
    return result;
}

void putU32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    for (unsigned i = 0; i < 4U; ++i) {
        out.push_back(static_cast<std::uint8_t>(value >> (8U * i)));
    }
}

void putU64(std::vector<std::uint8_t>& out, std::uint64_t value) {
    for (unsigned i = 0; i < 8U; ++i) {
        out.push_back(static_cast<std::uint8_t>(value >> (8U * i)));
    }
}

void putDouble(std::vector<std::uint8_t>& out, double value) {
    std::uint64_t bits{};
    static_assert(sizeof(bits) == sizeof(value));
    std::memcpy(&bits, &value, sizeof(bits));
    putU64(out, bits);
}

bool putString(std::vector<std::uint8_t>& out, const std::string& value) {
    if (value.size() > kMapNameLimit) return false;
    putU32(out, static_cast<std::uint32_t>(value.size()));
    out.insert(out.end(), value.begin(), value.end());
    return true;
}

class Reader final {
public:
    explicit Reader(const std::vector<std::uint8_t>& bytes) noexcept
        : bytes_(bytes) {}

    bool u8(std::uint8_t& value) noexcept {
        if (offset_ >= bytes_.size()) return false;
        value = bytes_[offset_++];
        return true;
    }
    bool u32(std::uint32_t& value) noexcept {
        if (bytes_.size() - offset_ < 4U) return false;
        value = static_cast<std::uint32_t>(bytes_[offset_]) |
                (static_cast<std::uint32_t>(bytes_[offset_ + 1U]) << 8U) |
                (static_cast<std::uint32_t>(bytes_[offset_ + 2U]) << 16U) |
                (static_cast<std::uint32_t>(bytes_[offset_ + 3U]) << 24U);
        offset_ += 4U;
        return true;
    }
    bool u64(std::uint64_t& value) noexcept {
        if (bytes_.size() - offset_ < 8U) return false;
        value = 0;
        for (unsigned i = 0; i < 8U; ++i) {
            value |= static_cast<std::uint64_t>(bytes_[offset_ + i]) << (8U * i);
        }
        offset_ += 8U;
        return true;
    }
    bool doubleValue(double& value) noexcept {
        std::uint64_t bits{};
        if (!u64(bits)) return false;
        std::memcpy(&value, &bits, sizeof(value));
        return true;
    }
    bool bytes(std::uint8_t* value, std::size_t count) noexcept {
        if (bytes_.size() - offset_ < count) return false;
        std::copy_n(bytes_.data() + offset_, count, value);
        offset_ += count;
        return true;
    }
    bool string(std::string& value) {
        std::uint32_t size{};
        if (!u32(size) || size > kMapNameLimit || bytes_.size() - offset_ < size) {
            return false;
        }
        value.assign(reinterpret_cast<const char*>(bytes_.data() + offset_), size);
        offset_ += size;
        return true;
    }
    std::size_t remaining() const noexcept { return bytes_.size() - offset_; }

private:
    const std::vector<std::uint8_t>& bytes_;
    std::size_t offset_{0};
};

void putMap(std::vector<std::uint8_t>& out, const MapIdentity& map) {
    (void)putString(out, map.name);
    putU64(out, map.bspBytes);
    out.push_back(static_cast<std::uint8_t>(map.hasBspHash));
    out.insert(out.end(), map.bspHash.begin(), map.bspHash.end());
    putU32(out, map.navFormatVersion);
    out.push_back(static_cast<std::uint8_t>(map.hasNavHash));
    out.insert(out.end(), map.navHash.begin(), map.navHash.end());
}

bool readMap(Reader& reader, MapIdentity& map) noexcept {
    std::uint8_t bspFlag{};
    std::uint8_t navFlag{};
    if (!reader.string(map.name) || !reader.u64(map.bspBytes) ||
        !reader.u8(bspFlag) || bspFlag > 1U || !reader.bytes(map.bspHash.data(), map.bspHash.size()) ||
        !reader.u32(map.navFormatVersion) || !reader.u8(navFlag) || navFlag > 1U ||
        !reader.bytes(map.navHash.data(), map.navHash.size())) {
        return false;
    }
    map.hasBspHash = bspFlag != 0U;
    map.hasNavHash = navFlag != 0U;
    return map.valid() && (!map.hasBspHash || map.bspBytes != 0U);
}

void putArea(std::vector<std::uint8_t>& out, const AreaExperience& area) {
    putU32(out, area.area);
    putDouble(out, area.visits);
    putDouble(out, area.dangerT);
    putDouble(out, area.dangerCT);
    putDouble(out, area.encounterRate);
    putDouble(out, area.deathRate);
    putDouble(out, area.grenadeThreat);
    putDouble(out, area.sniperThreat);
    putDouble(out, area.pushSuccess);
    putDouble(out, area.retakeSuccess);
    putDouble(out, area.humanTraffic);
    putDouble(out, area.botTraffic);
    putDouble(out, area.humanVisits);
    putDouble(out, area.botVisits);
    putDouble(out, area.damageDealt);
    putDouble(out, area.damageReceived);
    putDouble(out, area.kills);
    putDouble(out, area.deaths);
    putDouble(out, area.attackSuccess);
    putDouble(out, area.attackFailure);
}

bool readArea(Reader& reader, AreaExperience& area) noexcept {
    return reader.u32(area.area) && reader.doubleValue(area.visits) &&
           reader.doubleValue(area.dangerT) && reader.doubleValue(area.dangerCT) &&
           reader.doubleValue(area.encounterRate) && reader.doubleValue(area.deathRate) &&
           reader.doubleValue(area.grenadeThreat) && reader.doubleValue(area.sniperThreat) &&
           reader.doubleValue(area.pushSuccess) && reader.doubleValue(area.retakeSuccess) &&
           reader.doubleValue(area.humanTraffic) && reader.doubleValue(area.botTraffic) &&
           reader.doubleValue(area.humanVisits) && reader.doubleValue(area.botVisits) &&
           reader.doubleValue(area.damageDealt) && reader.doubleValue(area.damageReceived) &&
           reader.doubleValue(area.kills) && reader.doubleValue(area.deaths) &&
           reader.doubleValue(area.attackSuccess) && reader.doubleValue(area.attackFailure) &&
           area.valid();
}

bool readLegacyArea(Reader& reader, AreaExperience& area) noexcept {
    if (!reader.u32(area.area)) return false;
    if (!reader.doubleValue(area.visits) || !reader.doubleValue(area.dangerT) ||
        !reader.doubleValue(area.dangerCT) || !reader.doubleValue(area.encounterRate) ||
        !reader.doubleValue(area.deathRate) || !reader.doubleValue(area.grenadeThreat) ||
        !reader.doubleValue(area.sniperThreat) || !reader.doubleValue(area.pushSuccess) ||
        !reader.doubleValue(area.retakeSuccess) || !reader.doubleValue(area.humanTraffic) ||
        !reader.doubleValue(area.botTraffic)) {
        return false;
    }
    area.humanVisits = area.humanTraffic;
    area.botVisits = area.botTraffic;
    return area.valid();
}

std::vector<std::uint8_t> encode(const ExperienceSnapshot& snapshot) {
    std::vector<std::uint8_t> payload;
    payload.reserve(128U + snapshot.areas.size() * 164U);
    putMap(payload, snapshot.map);
    putU64(payload, snapshot.round.value);
    putU64(payload, snapshot.timeMicros);
    putDouble(payload, snapshot.totals.damageDealt);
    putDouble(payload, snapshot.totals.damageReceived);
    putDouble(payload, snapshot.totals.kills);
    putDouble(payload, snapshot.totals.deaths);
    putDouble(payload, snapshot.totals.roundWins);
    putDouble(payload, snapshot.totals.roundLosses);
    putU32(payload, static_cast<std::uint32_t>(snapshot.areas.size()));
    for (const auto& area : snapshot.areas) putArea(payload, area);
    return payload;
}

PersistenceStatus decode(const std::vector<std::uint8_t>& bytes,
                         const MapIdentity& expected,
                         ExperienceSnapshot& snapshot) {
    if (bytes.size() < kHeaderBytes ||
        !std::equal(std::begin(kMagic), std::end(kMagic) - 1, bytes.begin())) {
        return PersistenceStatus::Corrupt;
    }
    Reader header(bytes);
    std::uint8_t magic[8]{};
    std::uint32_t format{};
    std::uint32_t schema{};
    std::uint64_t payloadBytes{};
    std::uint64_t expectedChecksum{};
    if (!header.bytes(magic, sizeof(magic)) || !header.u32(format) ||
        !header.u32(schema) || !header.u64(payloadBytes) ||
        !header.u64(expectedChecksum) || format != kCurrentFormatVersion) {
        return format == kCurrentFormatVersion ? PersistenceStatus::Corrupt
                                                : PersistenceStatus::UnsupportedVersion;
    }
    if (schema != 1U && schema != kCurrentSchemaVersion) return PersistenceStatus::SchemaMismatch;
    if (payloadBytes > kMaxFileBytes || header.remaining() != payloadBytes) {
        return PersistenceStatus::Corrupt;
    }
    std::vector<std::uint8_t> payload(static_cast<std::size_t>(payloadBytes));
    if (!header.bytes(payload.data(), payload.size()) || checksum(payload) != expectedChecksum) {
        return PersistenceStatus::Corrupt;
    }
    Reader reader(payload);
    ExperienceSnapshot decoded{};
    if (!readMap(reader, decoded.map) || !reader.u64(decoded.round.value) ||
        !reader.u64(decoded.timeMicros) || !reader.doubleValue(decoded.totals.damageDealt) ||
        !reader.doubleValue(decoded.totals.damageReceived) || !reader.doubleValue(decoded.totals.kills) ||
        !reader.doubleValue(decoded.totals.deaths) || !reader.doubleValue(decoded.totals.roundWins) ||
        !reader.doubleValue(decoded.totals.roundLosses)) {
        return PersistenceStatus::Corrupt;
    }
    std::uint32_t areaCount{};
    if (!reader.u32(areaCount) || areaCount > kMaxExperienceAreas) {
        return PersistenceStatus::Corrupt;
    }
    try {
        decoded.areas.reserve(areaCount);
        for (std::uint32_t i = 0; i < areaCount; ++i) {
            AreaExperience area{};
            const bool read = schema == 1U ? readLegacyArea(reader, area) : readArea(reader, area);
            if (!read) return PersistenceStatus::Corrupt;
            if (!decoded.areas.empty() && decoded.areas.back().area >= area.area) {
                return PersistenceStatus::Corrupt;
            }
            decoded.areas.push_back(area);
        }
    } catch (...) {
        return PersistenceStatus::Corrupt;
    }
    if (reader.remaining() != 0U || !decoded.valid()) return PersistenceStatus::Corrupt;
    if (decoded.map != expected) return PersistenceStatus::MapMismatch;
    snapshot = std::move(decoded);
    return schema == 1U ? PersistenceStatus::Migrated : PersistenceStatus::Ok;
}

bool writeBytes(const std::string& path, const std::vector<std::uint8_t>& payload) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) return false;
    output.write(reinterpret_cast<const char*>(payload.data()),
                 static_cast<std::streamsize>(payload.size()));
    output.flush();
    return output.good();
}

std::vector<std::uint8_t> encodeFile(const ExperienceSnapshot& snapshot) {
    const auto payload = encode(snapshot);
    std::vector<std::uint8_t> result;
    result.reserve(kHeaderBytes + payload.size());
    result.insert(result.end(), std::begin(kMagic), std::end(kMagic) - 1);
    putU32(result, kCurrentFormatVersion);
    putU32(result, kCurrentSchemaVersion);
    putU64(result, static_cast<std::uint64_t>(payload.size()));
    putU64(result, checksum(payload));
    result.insert(result.end(), payload.begin(), payload.end());
    return result;
}

} // namespace

bool MapIdentity::valid() const noexcept {
    return !name.empty() && name.size() <= kMapNameLimit &&
           name.find('\0') == std::string::npos &&
           (!hasBspHash || validHash(bspHash)) &&
           (!hasNavHash || validHash(navHash));
}

bool MapIdentity::sameMap(const MapIdentity& other) const noexcept {
    return valid() && other.valid() && name == other.name && bspBytes == other.bspBytes &&
           hasBspHash == other.hasBspHash && (!hasBspHash || bspHash == other.bspHash) &&
           navFormatVersion == other.navFormatVersion && hasNavHash == other.hasNavHash &&
           (!hasNavHash || navHash == other.navHash);
}

const char* eventName(ExperienceEventKind kind) noexcept {
    switch (kind) {
    case ExperienceEventKind::AreaEntered: return "AreaEntered";
    case ExperienceEventKind::DamageDealt: return "DamageDealt";
    case ExperienceEventKind::DamageReceived: return "DamageReceived";
    case ExperienceEventKind::Kill: return "Kill";
    case ExperienceEventKind::Death: return "Death";
    case ExperienceEventKind::Encounter: return "Encounter";
    case ExperienceEventKind::GrenadeExplosion: return "GrenadeExplosion";
    case ExperienceEventKind::Plant: return "Plant";
    case ExperienceEventKind::Defuse: return "Defuse";
    case ExperienceEventKind::AttackSuccess: return "AttackSuccess";
    case ExperienceEventKind::AttackFailure: return "AttackFailure";
    case ExperienceEventKind::RetakeSuccess: return "RetakeSuccess";
    case ExperienceEventKind::RoundResult: return "RoundResult";
    case ExperienceEventKind::HumanTraversal: return "HumanTraversal";
    case ExperienceEventKind::BotTraversal: return "BotTraversal";
    }
    return "Unknown";
}

bool ExperienceEvent::requiresArea() const noexcept {
    return kind != ExperienceEventKind::RoundResult;
}

bool ExperienceEvent::valid() const noexcept {
    const auto eventValue = static_cast<std::uint8_t>(kind);
    const bool kindValid = eventValue <= static_cast<std::uint8_t>(ExperienceEventKind::BotTraversal);
    const bool actorKindValid = actorKind == ActorKind::Unknown ||
        actorKind == ActorKind::Human || actorKind == ActorKind::Bot ||
        actorKind == ActorKind::OtherBot;
    const bool actorRequired = kind != ExperienceEventKind::RoundResult;
    return map.valid() && round.isValid() && tick.isValid() && kindValid && actorKindValid &&
           (!requiresArea() || area != 0U) &&
           (!actorRequired || actor.isValid()) && std::isfinite(amount) && amount >= 0.0 &&
           amount <= 1'000'000.0 &&
           (team == perception::Team::Unknown || team == perception::Team::Terrorist ||
            team == perception::Team::CounterTerrorist || team == perception::Team::Spectator);
}

bool ExperienceSettings::valid() const noexcept {
    return finiteNonNegative(humanWeight) && finiteNonNegative(botWeight) &&
           finiteNonNegative(otherBotWeight) && finiteNonNegative(unknownWeight) &&
           humanWeight <= 100.0 && botWeight <= 100.0 && otherBotWeight <= 100.0 &&
           unknownWeight <= 100.0 && decayDenominator != 0U &&
           decayNumerator <= decayDenominator && maxDecayRounds != 0U;
}

double ExperienceSettings::weight(ActorKind kind) const noexcept {
    switch (kind) {
    case ActorKind::Human: return humanWeight;
    case ActorKind::Bot: return botWeight;
    case ActorKind::OtherBot: return otherBotWeight;
    case ActorKind::Unknown: return unknownWeight;
    }
    return 0.0;
}

bool AreaExperience::valid() const noexcept {
    if (area == 0U) return false;
    const std::array values{visits, dangerT, dangerCT, encounterRate, deathRate,
                            grenadeThreat, sniperThreat, pushSuccess, retakeSuccess,
                            humanTraffic, botTraffic, humanVisits, botVisits,
                            damageDealt, damageReceived, kills, deaths,
                            attackSuccess, attackFailure};
    return std::all_of(values.begin(), values.end(), finiteNonNegative);
}

void AreaExperience::decay(double factor) noexcept {
    visits *= factor; dangerT *= factor; dangerCT *= factor;
    encounterRate *= factor; deathRate *= factor; grenadeThreat *= factor;
    sniperThreat *= factor; pushSuccess *= factor; retakeSuccess *= factor;
    humanTraffic *= factor; botTraffic *= factor; humanVisits *= factor;
    botVisits *= factor; damageDealt *= factor; damageReceived *= factor;
    kills *= factor; deaths *= factor; attackSuccess *= factor;
    attackFailure *= factor;
}

bool ExperienceTotals::valid() const noexcept {
    return finiteNonNegative(damageDealt) && finiteNonNegative(damageReceived) &&
           finiteNonNegative(kills) && finiteNonNegative(deaths) &&
           finiteNonNegative(roundWins) && finiteNonNegative(roundLosses);
}

void ExperienceTotals::decay(double factor) noexcept {
    damageDealt *= factor; damageReceived *= factor; kills *= factor;
    deaths *= factor; roundWins *= factor; roundLosses *= factor;
}

bool ExperienceSnapshot::valid() const noexcept {
    if (!map.valid() || !round.isValid() || !totals.valid() ||
        areas.size() > kMaxExperienceAreas) return false;
    for (std::size_t i = 0; i < areas.size(); ++i) {
        if (!areas[i].valid() || (i != 0U && areas[i - 1U].area >= areas[i].area)) return false;
    }
    return true;
}

bool ExperienceModel::activate(const MapIdentity& map,
                               perception::RoundGeneration round) noexcept {
    if (!settings_.valid() || !map.valid() || !round.isValid()) return false;
    try {
        map_ = map; round_ = round; timeMicros_ = 0U; totals_ = {}; areas_.clear(); active_ = true;
        return true;
    } catch (...) {
        reset();
        return false;
    }
}

void ExperienceModel::reset() noexcept {
    map_ = {}; round_ = {}; timeMicros_ = 0U; totals_ = {}; areas_.clear(); active_ = false;
}

ExperienceUpdateResult ExperienceModel::applyDecayTo(perception::RoundGeneration round,
                                                     std::uint64_t timeMicros) noexcept {
    if (round.value < round_.value) return {ExperienceUpdateReason::StaleRound, 0U, false};
    const auto gap = round.value - round_.value;
    const auto steps = (std::min)(gap, static_cast<std::uint64_t>(settings_.maxDecayRounds));
    const double factor = static_cast<double>(settings_.decayNumerator) /
                          static_cast<double>(settings_.decayDenominator);
    for (std::uint64_t i = 0; i < steps; ++i) {
        for (auto& area : areas_) area.decay(factor);
        totals_.decay(factor);
    }
    round_ = round;
    timeMicros_ = timeMicros;
    return {ExperienceUpdateReason::Accepted, 0U, steps != 0U};
}

ExperienceUpdateResult ExperienceModel::beginRound(perception::RoundGeneration round,
                                                   std::uint64_t timeMicros) noexcept {
    if (!active_) return {ExperienceUpdateReason::Inactive, 0U, false};
    if (!round.isValid()) return {ExperienceUpdateReason::InvalidEvent, 0U, false};
    if (timeMicros < timeMicros_) return {ExperienceUpdateReason::StaleTime, 0U, false};
    return applyDecayTo(round, timeMicros);
}

AreaExperience* ExperienceModel::findArea(std::uint32_t area) noexcept {
    const auto it = std::lower_bound(areas_.begin(), areas_.end(), area,
        [](const AreaExperience& value, std::uint32_t id) { return value.area < id; });
    return it != areas_.end() && it->area == area ? &*it : nullptr;
}

const AreaExperience* ExperienceModel::findArea(std::uint32_t area) const noexcept {
    const auto it = std::lower_bound(areas_.begin(), areas_.end(), area,
        [](const AreaExperience& value, std::uint32_t id) { return value.area < id; });
    return it != areas_.end() && it->area == area ? &*it : nullptr;
}

AreaExperience* ExperienceModel::ensureArea(std::uint32_t area) noexcept {
    if (auto* existing = findArea(area)) return existing;
    if (areas_.size() >= kMaxExperienceAreas) return nullptr;
    try {
        const auto it = std::lower_bound(areas_.begin(), areas_.end(), area,
            [](const AreaExperience& value, std::uint32_t id) { return value.area < id; });
        AreaExperience value{}; value.area = area;
        return &*areas_.insert(it, value);
    } catch (...) {
        return nullptr;
    }
}

ExperienceUpdateResult ExperienceModel::updateArea(const ExperienceEvent& event,
                                                   AreaExperience& value,
                                                   double weight) noexcept {
    const double amount = event.amount == 0.0 ? 1.0 : event.amount;
    switch (event.kind) {
    case ExperienceEventKind::AreaEntered:
        value.visits += weight;
        break;
    case ExperienceEventKind::DamageDealt:
        value.damageDealt += amount * weight; totals_.damageDealt += amount * weight;
        break;
    case ExperienceEventKind::DamageReceived:
        value.damageReceived += amount * weight; totals_.damageReceived += amount * weight;
        if (event.team == perception::Team::Terrorist) value.dangerT += amount * weight;
        if (event.team == perception::Team::CounterTerrorist) value.dangerCT += amount * weight;
        break;
    case ExperienceEventKind::Kill:
        value.kills += weight; totals_.kills += weight;
        break;
    case ExperienceEventKind::Death:
        value.deaths += weight; value.deathRate += weight; totals_.deaths += weight;
        break;
    case ExperienceEventKind::Encounter:
        value.encounterRate += weight;
        break;
    case ExperienceEventKind::GrenadeExplosion:
        value.grenadeThreat += amount * weight;
        break;
    case ExperienceEventKind::Plant:
        if (event.success) value.pushSuccess += weight;
        break;
    case ExperienceEventKind::Defuse:
        if (event.success) value.retakeSuccess += weight;
        break;
    case ExperienceEventKind::AttackSuccess:
        value.attackSuccess += weight; value.pushSuccess += weight;
        break;
    case ExperienceEventKind::AttackFailure:
        value.attackFailure += weight;
        break;
    case ExperienceEventKind::RetakeSuccess:
        value.retakeSuccess += weight;
        break;
    case ExperienceEventKind::HumanTraversal:
    case ExperienceEventKind::BotTraversal:
        break;
    case ExperienceEventKind::RoundResult:
        return {ExperienceUpdateReason::InvalidArea, 0U, false};
    }
    if (event.actorKind == ActorKind::Human) {
        value.humanTraffic += weight;
        value.humanVisits += weight;
    } else if (event.actorKind == ActorKind::Bot || event.actorKind == ActorKind::OtherBot) {
        value.botTraffic += weight;
        value.botVisits += weight;
    }
    return {ExperienceUpdateReason::Accepted, value.area, weight != 0.0};
}

ExperienceUpdateResult ExperienceModel::apply(const ExperienceEvent& event) {
    if (!active_) return {ExperienceUpdateReason::Inactive, event.area, false};
    if (!settings_.valid()) return {ExperienceUpdateReason::InvalidSettings, event.area, false};
    if (!event.valid()) return {ExperienceUpdateReason::InvalidEvent, event.area, false};
    if (event.map != map_) return {ExperienceUpdateReason::MapMismatch, event.area, false};
    if (event.round.value < round_.value) return {ExperienceUpdateReason::StaleRound, event.area, false};
    if (event.timeMicros < timeMicros_) return {ExperienceUpdateReason::StaleTime, event.area, false};
    if (event.kind == ExperienceEventKind::HumanTraversal && event.actorKind != ActorKind::Human) {
        return {ExperienceUpdateReason::InvalidEvent, event.area, false};
    }
    if (event.kind == ExperienceEventKind::BotTraversal &&
        event.actorKind != ActorKind::Bot && event.actorKind != ActorKind::OtherBot) {
        return {ExperienceUpdateReason::InvalidEvent, event.area, false};
    }
    if (event.round.value > round_.value) {
        const auto decay = applyDecayTo(event.round, event.timeMicros);
        if (decay.reason != ExperienceUpdateReason::Accepted) return decay;
    } else {
        timeMicros_ = event.timeMicros;
    }
    if (event.kind == ExperienceEventKind::RoundResult) {
        const double weight = event.actorKind == ActorKind::Unknown ? 1.0 : settings_.weight(event.actorKind);
        if (event.success) totals_.roundWins += weight;
        else totals_.roundLosses += weight;
        return {ExperienceUpdateReason::Accepted, 0U, weight != 0.0};
    }
    auto* value = ensureArea(event.area);
    if (!value) {
        return {areas_.size() >= kMaxExperienceAreas ? ExperienceUpdateReason::CapacityExceeded
                                                      : ExperienceUpdateReason::AllocationFailure,
                event.area, false};
    }
    const double weight = settings_.weight(event.actorKind);
    if (event.kind == ExperienceEventKind::HumanTraversal) {
        value->humanTraffic += weight; value->humanVisits += weight;
        return {ExperienceUpdateReason::Accepted, event.area, weight != 0.0};
    }
    if (event.kind == ExperienceEventKind::BotTraversal) {
        value->botTraffic += weight; value->botVisits += weight;
        return {ExperienceUpdateReason::Accepted, event.area, weight != 0.0};
    }
    return updateArea(event, *value, weight);
}

bool ExperienceModel::load(const ExperienceSnapshot& snapshot) noexcept {
    if (!settings_.valid() || !snapshot.valid()) return false;
    if (active_ && snapshot.map != map_) return false;
    try {
        map_ = snapshot.map; round_ = snapshot.round; timeMicros_ = snapshot.timeMicros;
        totals_ = snapshot.totals; areas_ = snapshot.areas; active_ = true;
        return true;
    } catch (...) {
        return false;
    }
}

const AreaExperience* ExperienceModel::area(std::uint32_t area) const noexcept {
    return findArea(area);
}

ExperienceSnapshot ExperienceModel::snapshot() const {
    ExperienceSnapshot result{};
    result.map = map_; result.round = round_; result.timeMicros = timeMicros_;
    result.totals = totals_; result.areas = areas_;
    return result;
}

PersistenceResult BinaryExperienceStore::readFile(const std::string& path,
                                                  const MapIdentity& expected,
                                                  ExperienceSnapshot& snapshot) const {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) return {PersistenceStatus::NotFound, false};
    const auto end = input.tellg();
    if (end < 0 || static_cast<std::uint64_t>(end) > kMaxFileBytes) {
        return {PersistenceStatus::Corrupt, false};
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(end));
    input.seekg(0, std::ios::beg);
    if (!bytes.empty()) {
        input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
    if (!input.good() && !input.eof()) return {PersistenceStatus::IoFailure, false};
    try {
        return {decode(bytes, expected, snapshot), false};
    } catch (...) {
        return {PersistenceStatus::IoFailure, false};
    }
}

PersistenceResult BinaryExperienceStore::quarantine(const MapIdentity& expected) {
    const std::string quarantinePath = path_ + ".quarantine";
    std::ifstream existing(quarantinePath, std::ios::binary);
    if (existing) return {PersistenceStatus::QuarantineFailure, false};
    if (std::rename(path_.c_str(), quarantinePath.c_str()) != 0) {
        return {PersistenceStatus::QuarantineFailure, false};
    }
    (void)expected;
    return {PersistenceStatus::MapMismatch, false};
}

PersistenceResult BinaryExperienceStore::load(const MapIdentity& map,
                                              ExperienceSnapshot& snapshot) {
    if (path_.empty() || !map.valid()) return {PersistenceStatus::InvalidPath, false};
    auto result = readFile(path_, map, snapshot);
    if (result.status == PersistenceStatus::Ok) return result;
    if (result.status == PersistenceStatus::MapMismatch) return quarantine(map);
    if (result.status != PersistenceStatus::Corrupt && result.status != PersistenceStatus::IoFailure &&
        result.status != PersistenceStatus::NotFound) return result;

    const auto backupPath = path_ + ".bak";
    auto backup = readFile(backupPath, map, snapshot);
    if (backup.status == PersistenceStatus::Ok) {
        backup.status = PersistenceStatus::RecoveredBackup; backup.recovered = true; return backup;
    }
    if (backup.status == PersistenceStatus::MapMismatch) {
        return {PersistenceStatus::MapMismatch, false};
    }
    return result.status == PersistenceStatus::NotFound ? backup : result;
}

PersistenceResult BinaryExperienceStore::save(const ExperienceSnapshot& snapshot) {
    if (path_.empty() || !snapshot.valid()) return {PersistenceStatus::InvalidPath, false};
    std::vector<std::uint8_t> bytes;
    try {
        bytes = encodeFile(snapshot);
    } catch (...) {
        return {PersistenceStatus::IoFailure, false};
    }
    const auto temporaryPath = path_ + ".tmp";
    const auto backupPath = path_ + ".bak";
    if (!writeBytes(temporaryPath, bytes)) {
        std::remove(temporaryPath.c_str());
        return {PersistenceStatus::IoFailure, false};
    }
    std::ifstream existing(path_, std::ios::binary);
    const bool hadExisting = static_cast<bool>(existing);
    existing.close();
    if (hadExisting) {
        std::remove(backupPath.c_str());
        if (std::rename(path_.c_str(), backupPath.c_str()) != 0) {
            std::remove(temporaryPath.c_str());
            return {PersistenceStatus::IoFailure, false};
        }
    }
    if (std::rename(temporaryPath.c_str(), path_.c_str()) != 0) {
        std::remove(temporaryPath.c_str());
        if (hadExisting) (void)std::rename(backupPath.c_str(), path_.c_str());
        return {PersistenceStatus::IoFailure, false};
    }
    return {PersistenceStatus::Ok, false};
}

const char* BinaryExperienceStore::statusName(PersistenceStatus status) noexcept {
    switch (status) {
    case PersistenceStatus::None: return "None";
    case PersistenceStatus::Ok: return "Ok";
    case PersistenceStatus::NotFound: return "NotFound";
    case PersistenceStatus::Migrated: return "Migrated";
    case PersistenceStatus::RecoveredBackup: return "RecoveredBackup";
    case PersistenceStatus::InvalidPath: return "InvalidPath";
    case PersistenceStatus::IoFailure: return "IoFailure";
    case PersistenceStatus::Corrupt: return "Corrupt";
    case PersistenceStatus::UnsupportedVersion: return "UnsupportedVersion";
    case PersistenceStatus::SchemaMismatch: return "SchemaMismatch";
    case PersistenceStatus::MapMismatch: return "MapMismatch";
    case PersistenceStatus::QuarantineFailure: return "QuarantineFailure";
    }
    return "Unknown";
}

PersistenceResult ExperiencePipeline::restore() {
    if (!persistence_ || !model_.active()) return {PersistenceStatus::InvalidPath, false};
    ExperienceSnapshot snapshot{};
    const auto result = persistence_->load(model_.map(), snapshot);
    if (result.succeeded() && !model_.load(snapshot)) return {PersistenceStatus::MapMismatch, false};
    if (result.status == PersistenceStatus::Migrated) {
        const auto migrated = flush();
        if (!migrated.succeeded()) return migrated;
        return {PersistenceStatus::Ok, false};
    }
    return result;
}

PersistenceResult ExperiencePipeline::flush() {
    if (!persistence_ || !model_.active()) return {PersistenceStatus::InvalidPath, false};
    return persistence_->save(model_.snapshot());
}

} // namespace astrabot::core::experience
