// SPDX-License-Identifier: MPL-2.0
// Included in the fake-engine fixture namespace.
namespace p403 {
namespace p = astrabot::core::perception;
void roundEvent(enginefuncs_t& hooks,int destination=MSG_SPEC,int first=0,int second=0,edict_t* recipient=nullptr) {
    hooks.pfnMessageBegin(destination,14,nullptr,recipient);
    hooks.pfnWriteByte(first); hooks.pfnWriteByte(second); hooks.pfnMessageEnd();
}
void reentrantRound() { p401::extraTrace = nullptr; roundEvent(*gEngineHooks); }
void reentrantTeam() { p401::extraTrace = nullptr; sendTeamInfo(*gEngineHooks,13,32,"SPECTATOR"); }
void teamsAndRounds() {
    using namespace astrabot;
    using namespace p401;
    Fixture fixture; enginefuncs_t hooks{}; setup(fixture,hooks,1);
    auto& owner = adapter::metamod::lifecycleCoordinator();
    const auto observer = owner.fakeClient().activePlayer();
    const auto target = owner.registry().currentPlayer(32);
    assert(owner.teams().relation(observer,target) == p::Relation::Unknown);
    assert(owner.teams().relation(observer,observer) == p::Relation::Self);
    sendTeamInfo(hooks,13,32,"TERRORIST");
    assert(owner.teams().relation(observer,target) == p::Relation::Ally);
    step(fixture,0.2f); step(fixture,0.2f);
    const auto latest = [&]() { return owner.vision().memory().latest(observer); };
    assert(latest() && latest()->count == 1); // Allies are still remembered.
    const auto sameTeamSequence = latest()->memories[0].identity.sequence;
    sendTeamInfo(hooks,13,32,"TERRORIST");
    assert(latest()->count == 1 && latest()->memories[0].identity.sequence == sameTeamSequence);
    sendTeamInfo(hooks,13,32,"CT");
    assert(owner.teams().relation(observer,target) == p::Relation::Opponent);
    assert(latest()->count == 0); // Geometry-free affiliation invalidation.
    step(fixture,0.2f); assert(latest()->count == 1);
    sendTeamInfo(hooks,13,32,"unexpected");
    assert(owner.teams().relation(observer,target) == p::Relation::Unknown);
    human.v.health = 0;
    sendTeamInfo(hooks,13,32,"CT");
    assert(owner.teams().find(target)->team == p::Team::CounterTerrorist); // Dead humans remain in roster.
    human.v.health = 100; step(fixture,0.2f); assert(latest()->count == 1);
    const auto old = *owner.vision().observations().latest(observer);
    const auto prior = owner.round();
    assert(owner.perceptionIdentityDiagnostics().roundNotificationAvailable);
    roundEvent(hooks); assert(owner.round().value == prior.value+1 && !latest());
    roundEvent(hooks); assert(owner.round().value == prior.value+1);
    assert(owner.perceptionIdentityDiagnostics().duplicateRounds == 1);
    step(fixture,0); roundEvent(hooks); // Same time in a different host frame.
    assert(owner.round().value == prior.value+1);
    roundEvent(hooks,MSG_ONE,0,0,&fixture.entity);
    roundEvent(hooks,MSG_SPEC,0,100);
    roundEvent(hooks,MSG_SPEC,-256,0);
    assert(owner.round().value == prior.value+1);
    mode = 1; step(fixture,0.2f); assert(latest() && latest()->count == 0);
    assert(latest()->stamp.round == owner.round());
    mode = 0; step(fixture,0.2f); assert(latest()->count == 1);
    const auto& identity = latest()->memories[0].identity;
    assert(identity.round == owner.round() && identity.round != old.identity.round);
    assert(identity.source == p::ObservationSource::Vision && identity.validAt(latest()->stamp.timeMicros));
    assert(identity.observedMicros == latest()->memories[0].lastSeenMicros);
    const auto sequence = identity.sequence;
    step(fixture,0.001f); assert(latest()->memories[0].identity.sequence == sequence);
    extraTrace = &reentrantRound; step(fixture,0.2f);
    assert(owner.round().value == prior.value+2 && !latest());
    mode = 1; step(fixture,0.2f); assert(latest() && latest()->count == 0);
    mode = 0; step(fixture,0.2f); assert(latest()->count == 1);
    extraTrace = &reentrantTeam; step(fixture,0.2f); assert(latest()->count == 0);
    step(fixture,0.2f); assert(latest()->count == 0);
    sendTeamInfo(hooks,13,32,"CT"); step(fixture,0.2f); assert(latest()->count == 1);
    // Slot reuse during a TeamInfo message cannot apply an old affiliation.
    hooks.pfnMessageBegin(MSG_ALL,13,nullptr,nullptr); hooks.pfnWriteByte(32);
    ++human.serialnumber; hooks.pfnWriteString("TERRORIST"); hooks.pfnMessageEnd();
    assert(!owner.teams().find(target) && latest()->count == 0);
    step(fixture,0.2f);
    const auto replacement = owner.registry().currentPlayer(32);
    assert(replacement != target && !owner.teams().find(target));
    assert(owner.teams().find(replacement)->team == p::Team::Unknown);
    owner.clientDisconnect(&human);
    assert(!owner.teams().find(replacement));
    sendTeamInfo(hooks,13,32,"CT"); // Still-present disconnected serial is not a new connection.
    assert(!owner.registry().currentPlayer(32).isValid());
    human.free = 1;
    human.free = 0; ++human.serialnumber; human.v.health = 0; human.v.flags |= FL_SPECTATOR;
    sendTeamInfo(hooks,13,32,"SPECTATOR"); // Join before any alive/vision frame.
    const auto spectator = owner.registry().currentPlayer(32);
    assert(spectator.isValid() && spectator != replacement);
    assert(owner.teams().find(spectator)->team == p::Team::Spectator);
    assert(owner.teams().relation(observer,spectator) == p::Relation::Unknown);
    const auto knownRound = owner.round();
    hooks.pfnMessageBegin(MSG_ONE,15,nullptr,&fixture.entity); hooks.pfnMessageEnd(); // Unregistered ResetHUD-like notification.
    assert(owner.round() == knownRound);
    owner.configure(&fixture.engine,&fixture.utility,&fixture.dll,{11,12,13,0},&fixture.engineGlobals);
    assert(!owner.perceptionIdentityDiagnostics().roundNotificationAvailable);
    roundEvent(hooks); assert(owner.round() == knownRound);
    owner.serverDeactivate(); assert(!owner.teams().find(observer) && !latest());
    detach();
}
void run() { teamsAndRounds(); }
} // namespace p403
