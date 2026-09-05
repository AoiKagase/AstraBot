// SPDX-License-Identifier: MPL-2.0
#include "nav/runtime/route_session.hpp"
#include "route_fixture.hpp"
#include "nav/enrichment/traversal_link.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace astrabot;
using namespace nav;
using namespace runtime;
void check(bool yes) { if (!yes) throw std::runtime_error("route session check failed"); }
struct FakeQueries final : IWorldQueries {
    unsigned calls{};
    QueryRequest last{};
    QueryError failure{QueryError::None};
    bool stale{}, unknown{}, throwing{};
    unsigned staleField{};
    model::NavAreaId current{1};
    WorldQueryResult query(const QueryRequest& request) override {
        ++calls; last=request;
        if (throwing) throw std::runtime_error("query failure");
        WorldQueryResult result;
        result.stamp=request.stamp; result.kind=request.kind; result.error=failure;
        if (stale) ++result.stamp.routeGeneration;
        switch (staleField) {
        case 1: result.stamp.agent={99}; break;
        case 2: result.stamp.actor.generation={99}; break;
        case 3: result.stamp.map={99}; break;
        case 4: result.stamp.tick={99}; break;
        case 5: ++result.stamp.ordinal; break;
        case 6: result.kind=QueryKind::Floor; break;
        default: break;
        }
        if (!unknown) result.ground=GroundedAreaObservation{current, FloorObservation{0, {0,0,1}, true}};
        return result;
    }
};
MovementSnapshot snapshot() {
    MovementSnapshot s;
    s.agent={1}; s.actor={1,{1}}; s.map={1}; s.tick={1}; s.kind=ActorKind::ManagedBot;
    s.connected=true; s.alive=true; s.joined=true; s.grounded=true; s.position=model::NavVector3{1,1,0};
    return s;
}
NavigationSnapshot graph(bool connected=true) {
    route_test::Area a{1,{{0,0,0},{2,2,0},0,0}}, b{2,{{3,0,0},{5,2,0},0,0}}, c{3,{{6,0,0},{8,2,0},0,0}};
    if (connected) { a.targets[1]={2}; b.targets[1]={3}; }
    const auto built=query::NavGraph::build(route_test::snapshot({a,b,c}), {3,2,1000000});
    check(static_cast<bool>(built)); return {{1},*built.value};
}
void externalOwnership() {
    const auto s=snapshot(); FakeQueries port;
    RouteSession session(s.agent,s.actor,s.map);
    RouteOptions options; options.limits={3,1000000};
    std::weak_ptr<const model::NavMeshSnapshot> weak;
    {
        auto mesh=route_test::snapshot({
            {1,{{0,0,0},{2,2,0},0,0}}, {2,{{0,0,10},{2,2,10},10,10}}});
        weak=mesh;
        enrichment::NavMapFingerprint fingerprint{}; fingerprint[0]=42;
        enrichment::NavTraversalLinkSet links{fingerprint, {
            {7,8,9,{1},{2},{1,1,0},{1,1,10},model::NavTraversalKind::Ladder,
             enrichment::NavLinkDirection::Up,2}}};
        const auto built=query::NavGraph::compose(mesh,fingerprint,links,{3,3,1000000},{3,1000000});
        check(static_cast<bool>(built));
        check(session.request(s,{2},{{1},*built.value},port,options).accepted);
        links.links[0].linkId=999;
    }
    check(!weak.expired()); // Session retains graph and mesh after caller releases them.
    const auto saved=session.trace();
    check(session.cancel().count==1 && weak.expired());
    const auto& edge=saved.route->steps[0].edge;
    check(edge.external && edge.external->sourceId==7 && edge.external->generation==8 &&
          edge.external->linkId==9 && edge.external->entry.z==0 && edge.external->exit.z==10);
    check(saved.route->total==12 && edge.traversal==model::NavTraversalKind::Ladder);
}
int main() {
    try {
        externalOwnership();
        auto s=snapshot(); auto g=graph(); FakeQueries port;
        RouteSession session(s.agent,s.actor,s.map);
        RouteOptions options; options.limits={3,1000000};
        auto update=session.request(s,{3},g,port,options);
        check(update.accepted && update.count==1 && session.executable());
        check(session.trace().state==SessionState::Ready && session.trace().currentArea==model::NavAreaId{1});
        check(session.trace().route->areas==std::vector<model::NavAreaId>({{1},{2},{3}}));
        check(session.trace().route->steps[0].edge.direction==1 && session.trace().route->total==6);
        check(port.calls==1 && port.last.stamp.ordinal==1 && port.last.stamp.routeGeneration==1);
        const auto retained=session.trace();
        update=session.request(s,{1},g,port,options);
        check(update.count==2 && update.events[0].reason==SessionReason::GoalReplaced);
        check(update.events[0].terminal && update.events[0].route==retained.route);
        check(update.events[1].routeGeneration==2 && session.executable());
        check(session.trace().route->areas==std::vector<model::NavAreaId>({{1}})); // Ready, never arrival.
        auto bad=s; bad.kind=ActorKind::Human;
        update=session.request(bad,{2},g,port,options);
        check(!update.accepted && update.reason==SessionReason::InvalidActor && update.count==0);
        check(session.trace().routeGeneration==2);
        update=session.request(s,{999},g,port,options);
        check(!update.accepted && update.reason==SessionReason::InvalidGoal && !session.executable());
        check(port.calls==2);
        update=session.request(s,{3},graph(false),port,options);
        check(update.reason==SessionReason::Unreachable && !session.executable());
        check(session.trace().route->areas.empty());
        options.limits.maxExpansions=1;
        update=session.request(s,{3},g,port,options);
        check(update.reason==SessionReason::ExpansionLimit && !session.executable() && session.trace().route->areas.empty());
        options.diagnosticPartial=true;
        update=session.request(s,{3},g,port,options);
        check(update.reason==SessionReason::ExpansionLimit && !session.executable() && !session.trace().route->areas.empty());
        options.limits={3,1000000}; options.diagnosticPartial=false;
        options.maxWorldQueries=0;
        const auto calls=port.calls;
        update=session.request(s,{3},g,port,options);
        check(update.reason==SessionReason::QueryBudgetExceeded && port.calls==calls);
        options.maxWorldQueries=1;
        port.stale=true;
        check(session.request(s,{3},g,port,options).reason==SessionReason::StaleQuery);
        port.stale=false; port.unknown=true;
        check(session.request(s,{3},g,port,options).reason==SessionReason::UnknownGround);
        port.unknown=false; port.throwing=true;
        check(session.request(s,{3},g,port,options).reason==SessionReason::QueryFailed);
        port.throwing=false;
        for (unsigned field=1; field<=6; ++field) {
            port.staleField=field;
            check(session.request(s,{3},g,port,options).reason==SessionReason::StaleQuery);
        }
        port.staleField=0;
        port.failure=QueryError::BudgetExceeded;
        check(session.request(s,{3},g,port,options).reason==SessionReason::QueryBudgetExceeded);
        port.failure=QueryError::Unavailable;
        check(session.request(s,{3},g,port,options).reason==SessionReason::QueryFailed);
        port.failure=QueryError::None;
        port.current={999};
        check(session.request(s,{3},g,port,options).reason==SessionReason::NoCurrentArea);
        port.current={1};
        auto unknown=s; unknown.alive.reset();
        check(session.request(unknown,{3},g,port,options).reason==SessionReason::InvalidSnapshot);
        unknown=s; unknown.position->x=std::numeric_limits<float>::quiet_NaN();
        check(session.request(unknown,{3},g,port,options).reason==SessionReason::InvalidSnapshot);
        check(session.request(s,{3},{{1},{}},port,options).reason==SessionReason::MissingGraph);
        check(session.request(s,{3},{{2},g.graph},port,options).reason==SessionReason::MapChanged);
        check(session.request(s,{3},g,port,options).accepted);
        auto next=s; next.tick={2}; next.alive=false;
        update=session.observe(next);
        check(update.count==1 && update.events[0].terminal && update.reason==SessionReason::Dead);
        check(!session.executable() && session.observe(next).count==0 && session.cancel().count==0);
        s.tick={3}; check(session.request(s,{3},g,port,options).accepted);
        next=s; next.tick={4}; next.map={2};
        check(session.observe(next).reason==SessionReason::MapChanged && !session.executable());
        s.tick={5}; check(session.request(s,{3},g,port,options).reason==SessionReason::MapChanged);
        session=RouteSession(s.agent,s.actor,s.map);
        check(session.request(s,{3},g,port,options).accepted);
        next=s; next.tick={6}; next.actor.generation={2};
        check(session.observe(next).reason==SessionReason::ActorChanged);
        s.tick={7}; check(session.request(s,{3},g,port,options).reason==SessionReason::ActorChanged);
        session=RouteSession(s.agent,s.actor,s.map);
        check(session.request(s,{3},g,port,options).accepted);
        next=s; next.tick={8}; next.connected=false;
        check(session.observe(next).reason==SessionReason::Disconnected);
        s.tick={9}; check(session.request(s,{3},g,port,options).accepted);
        next=s; next.tick={10}; check(session.observe(next).accepted);
        check(session.observe(s).reason==SessionReason::StaleSnapshot && session.executable());
        auto staleActor=s; staleActor.actor.generation={99};
        check(session.observe(staleActor).reason==SessionReason::StaleSnapshot && session.executable());
        session.cancel();
        g.graph.reset();
        check(retained.route->areas.back()==model::NavAreaId{3} && retained.route->total==6);
        // Independent actors do not share request ordinals or cancellation.
        auto other=snapshot(); other.agent={2}; other.actor={2,{1}};
        RouteSession second(other.agent,other.actor,other.map); FakeQueries otherPort;
        check(second.request(other,{2},graph(),otherPort,options).accepted);
        check(second.trace().routeGeneration==1 && otherPort.last.stamp.actor==other.actor);
        std::cout << "route session contracts passed\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
