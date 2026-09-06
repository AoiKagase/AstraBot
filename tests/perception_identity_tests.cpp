// SPDX-License-Identifier: MPL-2.0
#include "core/perception_identity.hpp"
#include <cassert>

namespace p = astrabot::core::perception;
using astrabot::core::PlayerId;
int main() {
    p::TeamRoster roster;
    const PlayerId human{1,{1}}, bot{2,{1}}, replacement{1,{2}};
    assert(!roster.bind({},human));
    assert(roster.activate({1}));
    assert(roster.bind({1},human) && roster.bind({1},bot));
    assert(roster.relation(human,human) == p::Relation::Self);
    assert(roster.relation(human,bot) == p::Relation::Unknown);
    assert(roster.update({1},bot,p::Team::Terrorist));
    assert(roster.relation(human,bot) == p::Relation::Unknown);
    assert(roster.update({1},human,p::Team::Terrorist));
    assert(roster.relation(human,bot) == p::Relation::Ally);
    assert(roster.update({1},human,p::Team::CounterTerrorist));
    assert(roster.relation(human,bot) == p::Relation::Opponent);
    assert(roster.update({1},human,p::Team::Spectator));
    assert(roster.relation(human,bot) == p::Relation::Unknown);
    roster.forget(human);
    assert(!roster.bind({1},human) && !roster.update({1},human,p::Team::Terrorist));
    assert(roster.bind({1},replacement));
    roster.forget(human); // Old disconnect cannot retire a replacement.
    assert(roster.find(replacement) && roster.find(replacement)->team == p::Team::Unknown);
    assert(!roster.update({1},human,p::Team::Terrorist));
    assert(!roster.update({1},replacement,static_cast<p::Team>(255)));
    assert(!roster.bind({1},{33,{1}}));
    roster.clear();
    assert(!roster.find(bot) && !roster.bind({1},bot));
    assert(!roster.activate({1}) && roster.activate({2}));
    assert(!roster.bind({1},replacement) && roster.bind({2},human));
    assert(roster.relation(human,bot) == p::Relation::Unknown);

    p::ObservationIdentity observation{{2},{1},p::ObservationSource::Vision,1,100,200};
    assert(observation.validAt(200));
    assert(observation.observedMicros == 100); // Delayed receipt retains the original time.
    assert(!observation.validAt(199));
    observation.receivedMicros = 99; assert(!observation.validAt(200));
    observation.receivedMicros = 200; observation.sequence = 0; assert(!observation.validAt(200));
    observation.sequence = 1; observation.round = {}; assert(!observation.validAt(200));
    observation.round = {1}; observation.source = static_cast<p::ObservationSource>(255);
    assert(!observation.validAt(200));
}
