// SPDX-License-Identifier: MPL-2.0
// Included after the fake-engine and perception fixtures.
namespace p12_execution_test {
using namespace astrabot;

void trace(const float* start, const float* end, int flags, edict_t* observer,
           TraceResult* result) {
    if (flags) captureGround(start, end, flags, observer, result);
    else p401::trace(start, end, flags, observer, result);
}

void run() {
    Fixture fixture;
    enginefuncs_t hooks{};
    p401::setup(fixture, hooks, 2);
    auto& owner = adapter::metamod::lifecycleCoordinator();
    auto& console = owner.navConsole();
    owner.setMovementClockForTest(&navNow);
    // Drive runtime decisions explicitly below while retaining real NAV frames.
    owner.setRuntimeInputProvider([](void*, const adapter::metamod::LifecycleCoordinator&,
        adapter::metamod::RuntimeFrame&, adapter::metamod::RuntimeActorInput*,
        std::size_t) noexcept -> std::size_t { return 0; }, nullptr);
    fixture.engine.pfnTraceLine = &trace;
    gGroundMissing = gInvalidateDuringGround = gInvalidateDuringHull = false;
    gHullMode = 0;
    gSteeringMode = -1;
    gStairHeight = 0;
    gSimulateNav = false;
    gSimulateCrouch = gDoorActive = false;
    gDoorOpenAtUs = 0;
    gInvalidateFromActor = nullptr;
    fixture.entity.v.origin = Vector(50, 50, 36);
    fixture.secondEntity.v.origin = Vector(50, 50, 36);
    const auto first = owner.registry().currentPlayer(1);
    const auto second = owner.registry().currentPlayer(2);
    assert(first.isValid() && second.isValid() && first != second);

    // All areas fit a standing hull. The 100-unit gap is above the 32-unit
    // micro-transit allowance, so search succeeds but corridor construction
    // fails even when small boundary gaps can become legal Drop transitions.
    route_test::Area a{1,{{0,0,0},{100,100,0},0,0}},
        b{2,{{200,0,0},{300,100,0},0,0}},
        c{3,{{0,100,0},{100,200,0},0,0}},
        d{4,{{300,0,0},{400,100,0},0,0}};
    a.targets[1] = {2};
    a.targets[2] = {3};
    b.targets[1] = {4};
    assert(console.publish(owner.registry().mapGeneration(),
                           route_test::snapshot({a,b,c,d})).isNone());
    navFrame(fixture, 100'000);

    const auto apply = [&](core::PlayerId player, nav::model::NavAreaId goal) {
        const auto state = console.runtimeState(owner, player);
        assert(state && state->movement.agent.isValid());
        adapter::metamod::RuntimeDecision decision{};
        decision.player = player;
        decision.agent = state->movement.agent;
        decision.executable = true;
        decision.hasNavigationGoal = true;
        decision.navigationGoal = goal;
        decision.team.shared.map = owner.registry().mapGeneration();
        decision.team.shared.round = owner.round();
        decision.team.shared.tick = owner.registry().currentTick();
        decision.tactical.intent.type = core::tactical::IntentType::Roam;
        console.applyRuntimeNavigation(owner, decision);
        return console.runtimeNavigationStatus(player);
    };
    using Result = adapter::cstrike::RuntimeNavigationApplyResult;
    using Reason = adapter::cstrike::RuntimeNavigationApplyReason;
    using State = nav::runtime::ExecutionState;
    using Failure = nav::runtime::ExecutionFailure;
    const auto rejected = apply(first, {2});
    assert(rejected.result == Result::Rejected && rejected.reason == Reason::RouteRejected);
    const auto failed = console.runtimeState(owner, first);
    assert(failed && failed->execution == State::Failed);
    assert(failed->executionFailure == Failure::Corridor && !failed->routeExecutable);
    assert(failed->failedEdge && failed->failedEdge->source.value == 1 &&
           failed->failedEdge->target.value == 2 && failed->retryAtUs != 0);
    const auto motion = console.motionTrace(first);
    assert(motion && motion->portalReason == nav::corridor::PortalFailureReason::BoundaryMismatch);
    assert(console.trace(first) && console.trace(first)->state == nav::runtime::SessionState::Ready);
    assert(apply(first, {2}).result == Result::Rejected); // Never Unchanged after failure.
    assert(console.runtimeState(owner, first)->routeGeneration == failed->routeGeneration);
    assert(!console.runtimeState(owner, first)->routeExecutable);
    assert(apply(first, {3}).result == Result::Rejected); // Global 250 ms search backoff.

    // One actor's failed goal/backoff cannot prevent another actor from moving.
    assert(apply(second, {3}).result == Result::Applied);
    assert(console.runtimeState(owner, second)->routeExecutable);
    assert(console.runtimeState(owner, first)->execution == State::Failed);
    for (unsigned i = 0; i < 3; ++i) navFrame(fixture, 100'000);
    assert(apply(first, {2}).result == Result::Rejected); // Still inside the 2 s goal cooldown.
    assert(apply(first, {3}).result == Result::Applied);
    assert(console.runtimeState(owner, first)->routeExecutable);
    assert(console.runtimeState(owner, first)->execution == State::Running);
    assert(apply(first, {2}).result == Result::Rejected);
    assert(console.runtimeState(owner, first)->goal == nav::model::NavAreaId{3});

    // A different goal behind the same bad edge must be filtered at search,
    // including after a successful intervening goal. The other actor can
    // still search that edge and independently discover its corridor failure.
    assert(apply(first, {4}).result == Result::Rejected);
    assert(console.runtimeState(owner, first)->executionFailure == Failure::Search);
    assert(apply(second, {4}).result == Result::Rejected);
    assert(console.runtimeState(owner, second)->executionFailure == Failure::Corridor);
    assert(!console.runtimeState(owner, second)->routeExecutable);

    const auto oldRound = owner.round();
    p403::roundEvent(hooks);
    assert(owner.round().value == oldRound.value + 1);
    for (const auto player : {first, second}) {
        const auto reset = console.runtimeState(owner, player);
        assert(reset && reset->execution == State::Idle && !reset->routeExecutable);
        assert(reset->executionFailure == Failure::None && !reset->failedEdge && reset->retryAtUs == 0);
    }
    // Immediate fresh search proves both the goal cooldown and edge exclusion
    // were cleared by the lifecycle event; geometry still rejects execution.
    assert(apply(first, {2}).result == Result::Rejected);
    assert(console.runtimeState(owner, first)->executionFailure == Failure::Corridor);
    assert(console.trace(first) && console.trace(first)->state == nav::runtime::SessionState::Ready);
    detach();
}
}
