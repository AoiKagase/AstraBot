// SPDX-License-Identifier: MPL-2.0

#include "core/experience.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>

namespace e = astrabot::core::experience;
using astrabot::core::PlayerId;
using astrabot::core::TickId;
using astrabot::core::perception::RoundGeneration;
using astrabot::core::perception::Team;

namespace {

e::MapIdentity map() {
    e::MapIdentity value{};
    value.name = "de_astra";
    value.bspBytes = 1234;
    value.hasBspHash = true;
    value.bspHash[0] = 0x11;
    value.navFormatVersion = 5;
    value.hasNavHash = true;
    value.navHash[0] = 0x22;
    return value;
}

void zeroHashesAreRejected() {
    auto invalidBsp = map();
    invalidBsp.bspHash = {};
    assert(!invalidBsp.valid());

    auto invalidNav = map();
    invalidNav.navHash = {};
    assert(!invalidNav.valid());
}

e::ExperienceEvent event(e::ExperienceEventKind kind,
                         std::uint32_t area,
                         e::ActorKind actor = e::ActorKind::Bot) {
    e::ExperienceEvent value{};
    value.map = map();
    value.round = {1};
    value.tick = {1};
    value.timeMicros = 100;
    value.sequence = 1;
    value.kind = kind;
    value.area = area;
    value.actor = PlayerId{1, {1}};
    value.actorKind = actor;
    value.amount = 1.0;
    return value;
}

void contractsAndWeights() {
    e::ExperienceSettings settings{};
    assert(settings.valid());
    assert(std::abs(settings.weight(e::ActorKind::Human) - 1.0) < 0.00001);
    assert(std::abs(settings.weight(e::ActorKind::Bot) - 0.25) < 0.00001);
    assert(std::string(e::eventName(e::ExperienceEventKind::RoundResult)) == "RoundResult");

    e::ExperienceModel model(settings);
    assert(model.activate(map(), {1}));
    auto entered = event(e::ExperienceEventKind::AreaEntered, 10, e::ActorKind::Human);
    assert(model.apply(entered).accepted());
    auto traversal = event(e::ExperienceEventKind::BotTraversal, 10);
    assert(model.apply(traversal).accepted());
    const auto* area = model.area(10);
    assert(area != nullptr);
    assert(std::abs(area->visits - 1.0) < 0.00001);
    assert(std::abs(area->humanTraffic - 1.0) < 0.00001);
    assert(std::abs(area->botTraffic - 0.25) < 0.00001);
    assert(std::abs(area->humanVisits - 1.0) < 0.00001);
    assert(std::abs(area->botVisits - 0.25) < 0.00001);
}

void eventPipelineAndDecay() {
    e::ExperienceModel model;
    assert(model.activate(map(), {1}));
    auto damage = event(e::ExperienceEventKind::DamageReceived, 3);
    damage.team = Team::Terrorist;
    damage.amount = 40.0;
    assert(model.apply(damage).accepted());
    const auto before = model.area(3)->dangerT;
    assert(before > 9.9 && before < 10.1);

    auto round = model.beginRound({2}, 200);
    assert(round.accepted() && round.changed);
    assert(model.area(3)->dangerT < before);
    auto stale = damage;
    stale.round = {1}; stale.timeMicros = 300;
    assert(model.apply(stale).reason == e::ExperienceUpdateReason::StaleRound);

    auto result = event(e::ExperienceEventKind::RoundResult, 0);
    result.round = {2}; result.tick = {2}; result.timeMicros = 300; result.success = true;
    assert(model.apply(result).accepted());
    assert(model.totals().roundWins > 0.0);
}

void snapshotRoundTripAndMapMismatch() {
    e::ExperienceModel model;
    assert(model.activate(map(), {1}));
    auto kill = event(e::ExperienceEventKind::Kill, 9, e::ActorKind::Human);
    assert(model.apply(kill).accepted());
    const auto snapshot = model.snapshot();
    assert(snapshot.valid());

    e::ExperienceModel restored;
    assert(restored.load(snapshot));
    assert(restored.area(9) != nullptr && restored.area(9)->kills == 1.0);
    auto other = snapshot;
    other.map.name = "de_other";
    assert(!restored.load(other));
    assert(restored.map().name == "de_astra");

    e::ExperienceModel same;
    assert(same.activate(map(), {1}));
    assert(same.apply(kill).accepted());
    assert(same.snapshot().areas[0].kills == restored.snapshot().areas[0].kills);
}

std::string path() {
    return "astrabot-experience-tests.bin";
}

void persistenceAndRecovery() {
    const auto file = path();
    const auto backup = file + ".bak";
    const auto quarantine = file + ".quarantine";
    std::remove(file.c_str()); std::remove(backup.c_str()); std::remove(quarantine.c_str());

    e::ExperienceModel model;
    assert(model.activate(map(), {1}));
    assert(model.apply(event(e::ExperienceEventKind::Encounter, 7)).accepted());
    e::BinaryExperienceStore store(file);
    assert(store.save(model.snapshot()).succeeded());
    assert(store.save(model.snapshot()).succeeded());

    e::ExperienceSnapshot loaded{};

    {
        std::ofstream corrupt(file, std::ios::binary | std::ios::trunc);
        corrupt << "corrupt";
    }
    assert(store.load(map(), loaded).status == e::PersistenceStatus::RecoveredBackup);
    assert(loaded.areas.size() == 1U && loaded.areas[0].area == 7U);

    assert(store.save(model.snapshot()).succeeded());
    {
        std::fstream schema(file, std::ios::binary | std::ios::in | std::ios::out);
        schema.seekp(12, std::ios::beg);
        const char unsupported[4] = {99, 0, 0, 0};
        schema.write(unsupported, sizeof(unsupported));
    }
    assert(store.load(map(), loaded).status == e::PersistenceStatus::SchemaMismatch);
    assert(store.save(model.snapshot()).succeeded());
    e::ExperiencePipeline pipeline({}, &store);
    assert(pipeline.activate(map(), {1}));
    assert(pipeline.restore().succeeded());
    assert(pipeline.model().area(7) != nullptr);
    assert(pipeline.flush().succeeded());
    e::MapIdentity other = map(); other.name = "de_other";
    assert(store.load(other, loaded).status == e::PersistenceStatus::MapMismatch);
    assert(std::ifstream(quarantine).good());
    std::remove(file.c_str()); std::remove(backup.c_str()); std::remove(quarantine.c_str());
}

} // namespace

int main() {
    zeroHashesAreRejected();
    contractsAndWeights();
    eventPipelineAndDecay();
    snapshotRoundTripAndMapMismatch();
    persistenceAndRecovery();
}
