// SPDX-License-Identifier: MPL-2.0
// Included inside fake_client_tests.cpp's fixture namespace; no production seams.
struct P308HostResult {
    std::uint64_t frames{},elapsedUs{},totalQueries{},replans{},setupQueries{},discoveryQueries{};
    unsigned maxQueries{};
    std::vector<std::string> trace;
    static constexpr std::size_t traceLimit=32768;
    void event(const std::string& value) { assert(trace.size()<traceLimit); trace.push_back(value); }
    void marker(const std::string& value) {
        event("{\"type\":\"marker\",\"actor\":1,\"tick\":"+std::to_string(frames)+",\"data\":\""+value+"\"}");
    }
};

P308HostResult p308HostRun(unsigned actors,std::uint64_t us,unsigned mode,bool mapChange) {
    using namespace astrabot;
    P308HostResult result;
    const bool ladder=mode>=15 && mode<=17;
    gNavClockUs=1000000; gFrozenActor=nullptr; gFreezeNav=false;
    Fixture fixture{}; fixture.multiClient=true;
    enginefuncs_t hooks{};
    core::MapGeneration retiredMap{};
    std::vector<core::PlayerId> retiredPlayers;
    // The first epoch deliberately retires a queued command; the next epoch
    // reuses the same edict slots with fresh registry/map identities.
    for(unsigned epoch=0;epoch<(mapChange ? 2U:1U);++epoch) {
        fixture.matrixSlot=1;
        for(unsigned slot=1;slot<=actors;++slot) { fixture.matrixEntity(slot)->free=slot==1 ? 0:1; if(epoch) ++fixture.matrixEntity(slot)->serialnumber; }
        fixture.engine.pfnPEntityOfEntIndex=&captureDoorEntity;
        gDoorActive=false; gDoorOpenAtUs=0; gSteeringMode=-1; gSimulateCrouch=gCrouchCeiling=false;
        gSimulateJump=gMissJump=gSimulateLadder=false;
        gMatrixPlayer=gPlayerObstacle=false;
        if(mode>=5 && mode<=7) configureDoor(fixture);
        if(mode==12 || mode==13) fixture.engine.pfnCVarGetPointer=&captureJumpCvar;
        if(ladder) {
            fixture.engineGlobals.maxEntities=128;
            (void)configureLadderMotionFixture(fixture.engine,&fixture.entity,{},mode>=16);
        }
        prepareNavWalk(fixture,hooks,us,epoch!=0);
        result.frames+=2; result.elapsedUs+=2*us;
        auto& owner=adapter::metamod::lifecycleCoordinator(); auto& console=owner.navConsole();
        std::vector<core::PlayerId> players{owner.fakeClient().activePlayer()};
        for(unsigned slot=2;slot<=actors;++slot) {
            fixture.matrixSlot=slot;
            fixture.matrixEntity(slot)->free=0; // FakeClient allocation begins here.
            const auto created=owner.createBot("AstraBot-Matrix",{adapter::cstrike::Team::Terrorist,1});
            assert(created.succeeded()); players.push_back(created.player);
            auto* entity=fixture.matrixEntity(slot);
            sendVguiMenu(hooks,11,entity,2,1); navFrame(fixture,us);
            sendVguiMenu(hooks,11,entity,26,1); navFrame(fixture,us);
            sendTeamInfo(hooks,13,static_cast<int>(slot),"TERRORIST");
            result.frames+=2; result.elapsedUs+=2*us;
            assert(owner.joinState(created.player)->phase()==adapter::cstrike::JoinPhase::Joined);
        }
        if(epoch) {
            assert(owner.registry().mapGeneration()!=retiredMap);
            for(unsigned i=0;i<actors;++i) {
                assert(players[i].generation!=retiredPlayers[i].generation && !owner.entityFor(retiredPlayers[i]));
                assert(!console.trace(retiredPlayers[i]));
            }
            result.marker("fresh-map:new-identities");
        }
        std::vector<route_test::Area> areas;
        for(unsigned i=0;i<actors;++i) {
            const float y=static_cast<float>(i*200);
            route_test::Area a{i*2+1,{{0,y,0},{100,y+100,0},0,0}},
                b{i*2+2,{{100,y,0},{200,y+100,0},0,0}};
            a.targets[1]={i*2+2}; areas.push_back(a); areas.push_back(b);
            auto* entity=fixture.matrixEntity(i+1); entity->v=fixture.entity.v;
            entity->v.origin=Vector(mode==1 ? 20+static_cast<float>(i)*4:50,y+50,36);
        }
        if(mode==8 || mode==9) {
            gStairHeight=16; areas[1].extent={{100,0,16},{200,100,16},16,16};
            areas[1].targets[3]={1};
            if(mode==9) fixture.entity.v.origin=Vector(150,50,52);
        }
        if(mode==10) { gSteeringMode=0; fixture.entity.v.origin.y=49; }
        if(mode==11) {
            areas[1].attributes=1; areas[1].targets[1]={3};
            areas.push_back({3,{{200,0,0},{300,100,0},0,0}});
            gSimulateCrouch=gCrouchCeiling=true;
        }
        if(mode>=5 && mode<=7) { fixture.entity.v.view_ofs=Vector(0,0,28); gDoorLocked=mode==7; if(mode==6) { gNavDoor.v.spawnflags=0; gNavDoor.v.solid=SOLID_BSP; } }
        if(mode==12 || mode==13) {
            areas[1].attributes=2; gSimulateJump=true; gMissJump=mode==13;
            gJumpGravity.value=800; gJumpHeight.value=45;
            fixture.entity.v.movetype=MOVETYPE_WALK; fixture.entity.v.velocity=Vector(0,0,0);
        }
        if(mode==18 || mode==19) {
            fixture.engineGlobals.maxEntities=128;
            assert(owner.registry().registerPlayer(2));
            gNavPlayer={}; gNavPlayer.v.flags=FL_CLIENT; gNavPlayer.v.solid=SOLID_SLIDEBOX; gNavPlayer.serialnumber=23;
            gMatrixPlayer=gPlayerObstacle=true; gSteeringMode=mode==18 ? 1:2;
        }
        if(mode==20) areas[0].targets[1].clear();
        if(ladder) {
            const auto mesh=configureLadderMotionFixture(fixture.engine,&fixture.entity,owner.registry().mapGeneration(),mode>=16);
            fixture.entity.v.origin=Vector(-49,32,mode>=16 ? 164.0f:36.0f); fixture.entity.v.velocity=Vector(0,0,0);
            fixture.entity.v.movetype=MOVETYPE_WALK; fixture.entity.v.friction=1;
            assert(console.publish(owner.registry().mapGeneration(),mesh).isNone());
            std::istringstream bsp("abc");
            const adapter::cstrike::LadderWorld world{&fixture.engine,&owner,[](const void* context) noexcept {
                return static_cast<const adapter::metamod::LifecycleCoordinator*>(context)->registry().mapGeneration(); }};
            const auto discoveryStart=ladderMotionQueryCount();
            assert(console.publishLadders(bsp,world,owner.registry().mapGeneration(),128));
            result.discoveryQueries+=ladderMotionQueryCount()-discoveryStart;
            gSimulateLadder=true;
        } else assert(console.publish(owner.registry().mapGeneration(),route_test::snapshot(areas)).isNone());
        const auto selector=[](core::PlayerId player) { return std::to_string(player.slot)+":"+std::to_string(player.generation.value); };
        bool diagnostic=mode==20;
        const auto goal=[&](unsigned i) {
            const auto target=std::to_string(mode==9 || (ladder && mode>=16) ? 1:mode==11 ? 3:i*2+2),actor=selector(players[i]);
            const auto& start=fixture.matrixEntity(i+1)->v.origin;
            std::ostringstream input;
            input<<"{\"type\":\"input\",\"actor\":"<<i+1<<",\"tick\":"<<result.frames
                 <<",\"map\":"<<owner.registry().mapGeneration().value<<",\"start\":["<<start.x<<','<<start.y<<','<<start.z
                 <<"],\"goalArea\":"<<target<<'}';
            result.event(input.str());
            const auto before=ladder ? ladderMotionQueryCount():static_cast<unsigned>(gHullCalls);
            runNav({"astrabot_goto",target.c_str(),actor.c_str()});
            result.setupQueries+=(ladder ? ladderMotionQueryCount():static_cast<unsigned>(gHullCalls))-before;
            assert(console.trace(players[i])->state==(diagnostic ? nav::runtime::SessionState::Failed:nav::runtime::SessionState::Ready));
        };
        gActorHullCalls.clear(); gClientMoves.clear(); gNavMoves.clear();
        for(unsigned i=0;i<actors;++i) goal(i);
        if(mode==20) {
            assert(console.trace(players.front())->reason==nav::runtime::SessionReason::Unreachable);
            const auto moves=gClientMoves.size();
            for(unsigned n=0;n<3;++n) { navFrame(fixture,us); ++result.frames; result.elapsedUs+=us; }
            assert(gClientMoves.size()==moves); result.marker("unreachable:no-dispatch");
            if(mapChange && epoch==0) {
                retiredMap=owner.registry().mapGeneration(); retiredPlayers=players;
                owner.serverDeactivate(); navFrame(fixture,us); ++result.frames; result.elapsedUs+=us;
                assert(gClientMoves.size()==moves && !console.trace(players.front()));
                result.marker("map-invalidated:no-old-dispatch"); gSimulateNav=false; continue;
            }
            areas[0].targets[1]={2};
            assert(console.publish(owner.registry().mapGeneration(),route_test::snapshot(areas)).isNone());
            diagnostic=false; goal(0);
        }
        if(mode==2 || mode==14) gFrozenActor=fixture.matrixEntity(1);
        std::vector<std::uint64_t> sequences(actors),decisionTimes(actors),routeGenerations(actors,1);
        for(unsigned i=0;i<actors;++i) routeGenerations[i]=console.trace(players[i])->routeGeneration;
        std::vector<unsigned> arrivals(actors);
        const auto frame=[&] {
            const auto oldCalls=gActorHullCalls;
            const auto oldLadder=ladderMotionQueryCount();
            const auto oldMoves=gClientMoves.size();
            navFrame(fixture,us); ++result.frames; result.elapsedUs+=us;
            std::vector<unsigned> dispatches(actors);
            for(std::size_t n=oldMoves;n<gClientMoves.size();++n) {
                auto* entity=gClientMoves[n].first;
                const auto player=owner.playerForEntity(entity);
                assert(player.slot>=1 && player.slot<=actors && player==players[player.slot-1]);
                assert(++dispatches[player.slot-1]<=1);
            }
            for(unsigned i=0;i<actors;++i) {
                const auto* entity=fixture.matrixEntity(i+1);
                const auto old=oldCalls.find(const_cast<edict_t*>(entity));
                const unsigned before=old==oldCalls.end() ? 0:old->second;
                const unsigned queries=ladder ? ladderMotionQueryCount()-oldLadder:gActorHullCalls[const_cast<edict_t*>(entity)]-before;
                assert(queries<=21); result.maxQueries=(std::max)(result.maxQueries,queries); result.totalQueries+=queries;
                const auto* t=console.motionTrace(players[i]); assert(t);
                assert(console.motionHistoryCount(players[i])<=128);
                for(std::size_t n=0;n<console.motionHistoryCount(players[i]);++n)
                    assert(console.motionHistory(players[i],n)->decision.binding.actor==players[i]);
                if(t->decision.tick==owner.registry().currentTick()) {
                    assert(t->decision.queries<=21 && t->decision.samples<=4);
                    // Goto/replan stamps are also observations. Allow their
                    // startup slots while bounding the sustained 25 Hz rate.
                    if(queries) ++decisionTimes[i];
                    assert(decisionTimes[i]<=result.elapsedUs/40000+4+2*result.replans);
                }
                if(t->decision.binding.routeGeneration>routeGenerations[i]) {
                    result.replans+=t->decision.binding.routeGeneration-routeGenerations[i];
                    routeGenerations[i]=t->decision.binding.routeGeneration;
                }
                if(t->sequence!=sequences[i] || queries || dispatches[i]) {
                    sequences[i]=t->sequence;
                    std::ostringstream line;
                    line<<epoch<<':'<<result.frames<<':'<<i<<':'<<unsigned(t->event)<<':'
                        <<unsigned(t->decision.state)<<':'<<unsigned(t->decision.reason)<<':'
                        <<unsigned(t->decision.recovery.state)<<':'<<t->decision.recovery.attempts<<':'
                        <<queries<<':'<<dispatches[i]<<':'<<t->queued<<':'<<t->dispatched<<':'
                        <<t->command.movement.forward<<':'<<t->command.movement.side<<':'<<t->command.buttons<<':'
                        <<unsigned(t->command.msec)<<':'<<entity->v.origin.x<<':'<<entity->v.origin.y;
                    if(t->selectedEdge) line<<':'<<t->selectedEdge->source.value<<':'<<t->selectedEdge->target.value;
                    if(t->decision.target) line<<':'<<t->decision.target->origin.x<<':'<<t->decision.target->origin.y;
                    result.event("{\"type\":\"frame\",\"actor\":"+std::to_string(i+1)+",\"tick\":"+
                        std::to_string(result.frames)+",\"data\":\""+line.str()+"\"}");
                }
                assert(std::abs(entity->v.origin.y-(ladder ? 32.0f:i*200+50.0f))<((mode==18 || mode==19) ? 50.01f:20.01f));
                if(!arrivals[i] && t->decision.state==nav::local::WalkState::Arrived) arrivals[i]=static_cast<unsigned>(result.frames);
            }
        };
        frame();
        if(mapChange && epoch==0) {
            if(mode>=5 && mode<=14) {
                bool active=false;
                for(unsigned n=0;n<500 && !active;++n) {
                    frame(); const auto& d=console.motionTrace(players.front())->decision;
                    active=mode==11 ? (fixture.entity.v.flags&FL_DUCKING)!=0:
                        (mode==12 || mode==13) ? (console.motionTrace(players.front())->command.buttons&IN_JUMP)!=0:
                        mode<=7 ? d.doorState==nav::local::DoorWaitState::Waiting:
                        console.motionTrace(players.front())->dispatched>0;
                }
                assert(active);
            }
            if(ladder) {
                bool climbing=false;
                for(unsigned n=0;n<500 && !climbing;++n) {
                    frame(); const auto state=console.motionTrace(players.front())->decision.ladderState;
                    climbing=state==nav::local::LadderState::ClimbUp || state==nav::local::LadderState::ClimbDown;
                }
                assert(climbing);
            }
            if(mode==18 || mode==19) {
                for(unsigned n=0;n<500 && !console.motionTrace(players.front())->decision.blocker;++n) frame();
                assert(console.motionTrace(players.front())->decision.blocker);
            }
            const auto allPending=[&] {
                bool pending=true;
                for(const auto player:players) {
                    const auto* trace=console.motionTrace(player); assert(trace);
                    assert(trace->decision.state==nav::local::WalkState::Running);
                    pending=pending && trace->event==adapter::cstrike::MotionEvent::Queued &&
                        trace->commandTick==owner.registry().currentTick();
                }
                return pending;
            };
            // Semantic checkpoints can occur after dispatch but before the next
            // intent is queued. Retire real current-frame pending work for every
            // actor, rather than relying on the cumulative queued counter.
            for(unsigned n=0;n<500 && !allPending();++n) frame();
            assert(allPending());
            const auto oldMap=owner.registry().mapGeneration();
            retiredMap=oldMap; retiredPlayers=players;
            owner.serverDeactivate(); const auto moves=gClientMoves.size();
            navFrame(fixture,us); ++result.frames; result.elapsedUs+=us;
            assert(gClientMoves.size()==moves);
            for(const auto player:players) assert(!owner.entityFor(player) && !console.trace(player));
            if(ladder) assert(!console.ladders());
            assert(owner.registry().mapGeneration()!=oldMap || !owner.registry().isMapActive());
            result.marker("map-invalidated:no-old-dispatch");
            gFrozenActor=nullptr; gSimulateNav=false; continue;
        }
        if(mode==3) { const auto actor=selector(players.front()); runNav({"astrabot_nav_cancel",actor.c_str()}); }
        bool finished=false,ladderFault=false;
        for(unsigned n=0;n<1000;++n) {
            if(mode==14 && n*us>=700000) gFrozenActor=nullptr;
            if(mode==18 && n*us>=300000) gSteeringMode=-1;
            if(mode==17 && !ladderFault && (console.motionTrace(players.front())->command.buttons&IN_JUMP)) {
                ladderFault=true; invalidateLadderMotionFixture(false);
            }
            frame(); finished=true;
            for(unsigned i=0;i<actors;++i) {
                const auto state=console.motionTrace(players[i])->decision.state;
                const auto expected=(i==0 && mode==2) || mode==7 || mode==13 || mode==17 || mode==19 ? nav::local::WalkState::Failed:
                    i==0 && mode==3 ? nav::local::WalkState::Aborted:nav::local::WalkState::Arrived;
                if(state!=expected) finished=false;
                if(i==0 && mode==2 && console.motionTrace(players[i])->decision.recovery.state!=nav::local::RecoveryState::Aborted) finished=false;
            }
            if(finished) break;
            assert(result.elapsedUs<=40000000);
        }
        if(!finished) std::fprintf(stderr,"P3-08 host nonterminal mode=%u actors=%u frame_us=%llu map_change=%u state=%u reason=%u\n",mode,actors,
            static_cast<unsigned long long>(us),unsigned(mapChange),unsigned(console.motionTrace(players.front())->decision.state),
            unsigned(console.motionTrace(players.front())->decision.reason));
        assert(finished);
        if(mode==19) assert(console.motionTrace(players.front())->decision.reason==nav::local::WalkReason::DynamicBlocked &&
            console.motionTrace(players.front())->decision.blockerReason==nav::local::BlockerReason::TimedOut);
        if(mode==17) assert(ladderFault);
        if(mode==13) {
            const auto& decision=console.motionTrace(players.front())->decision;
            const auto presses=std::count_if(gNavMoves.begin(),gNavMoves.end(),[](const auto& command) {
                return (command.buttons&IN_JUMP)!=0;
            });
            assert(presses==1 && decision.reason==nav::local::WalkReason::JumpFailed &&
                decision.jumpReason==nav::local::JumpReason::TakeoffTimeout);
        }
        if(mode==5) assert(gDoorUses>0 && gDoorOpen);
        if(mode==6) assert(gTouchContacts>0 && gDoorOpen);
        if(mode==7) assert(!gDoorOpen && gDoorUses>0 && console.motionTrace(players.front())->decision.doorReason==nav::local::DoorWaitReason::TimedOut);
        if(mode==11) assert(!(fixture.entity.v.flags&FL_DUCKING) && fixture.entity.v.origin.x>216);
        if(mode==1 && actors>1) assert(arrivals.front()>arrivals.back());
        if(mode==2) {
            const auto* t=console.motionTrace(players.front());
            assert(t->decision.recovery.state==nav::local::RecoveryState::Aborted && t->decision.recovery.attempts==1);
            for(unsigned i=1;i<actors;++i) assert(arrivals[i]>0 && arrivals[i]<result.frames);
        }
        if(mode==4) {
            const auto stale=players.front(); const auto oldSelector=selector(stale);
            owner.clientDisconnect(fixture.matrixEntity(1)); fixture.matrixSlot=1; ++fixture.entity.serialnumber;
            const auto fresh=owner.createBot("AstraBot-Reused",{adapter::cstrike::Team::Terrorist,1});
            assert(fresh.succeeded() && fresh.player.generation!=stale.generation); players[0]=fresh.player;
            runNav({"astrabot_goto","2",oldSelector.c_str()}); assert(!console.trace(fresh.player));
            sendVguiMenu(hooks,11,&fixture.entity,2,1); navFrame(fixture,us);
            sendVguiMenu(hooks,11,&fixture.entity,26,1); navFrame(fixture,us);
            result.frames+=2; result.elapsedUs+=2*us;
            sendTeamInfo(hooks,13,1,"TERRORIST"); fixture.entity.v.origin=Vector(50,50,36);
            sequences[0]=decisionTimes[0]=0; routeGenerations[0]=1; goal(0);
            for(unsigned n=0;n<1000 && console.motionTrace(fresh.player)->decision.state!=nav::local::WalkState::Arrived;++n) frame();
            assert(console.motionTrace(fresh.player)->decision.state==nav::local::WalkState::Arrived);
            assert(!console.motionTrace(stale)); result.marker("slot-reused:stale-selector-rejected:fresh-arrived");
        }
        for(unsigned i=0;i<actors;++i) {
            const auto state=console.motionTrace(players[i])->decision.state;
            const char* outcome=state==nav::local::WalkState::Arrived ? "Arrived":state==nav::local::WalkState::Aborted ? "Aborted":"Failed";
            assert(state!=nav::local::WalkState::Running);
            result.event("{\"type\":\"terminal\",\"actor\":"+std::to_string(i+1)+",\"tick\":"+
                std::to_string(result.frames)+",\"outcome\":\""+outcome+"\"}");
        }
        gFrozenActor=nullptr; gSimulateNav=false; detach();
    }
    gDoorActive=false; gDoorOpenAtUs=0; gSteeringMode=-1; gSimulateCrouch=gCrouchCeiling=false;
    gSimulateJump=gMissJump=gSimulateLadder=false; gStairHeight=0;
    gMatrixPlayer=gPlayerObstacle=false;
    return result;
}

int runP308HostMatrix(const char* path) {
    const char* names[]={"host-simultaneous","host-staggered","host-blocked-actor","host-cancel","host-slot-reuse",
        "host-door-use","host-door-touch","host-door-timeout","host-stairs-up","host-stairs-down","host-narrow",
        "host-crouch-release","host-jump","host-jump-failed","host-stuck-transient","host-ladder-up","host-ladder-down","host-ladder-failed-dismount",
        "host-player-transient","host-player-permanent","host-unreachable"};
    const char* outcomes[]={"Arrived","Arrived","FailedWithPeerArrival","CancelledWithPeerArrival","ReusedAndArrived",
        "Arrived","Arrived","Failed","Arrived","Arrived","Arrived","Arrived","Arrived","Failed","Arrived","Arrived","Arrived","Failed","Arrived","Failed","UnreachableThenArrived"};
    std::ofstream out(path); if(!out) return 2;
    out<<"{\"schemaVersion\":1,\"producer\":\"adapter\",\"results\":[";
    bool first=true;
    for(unsigned mode=0;mode<21;++mode) for(unsigned actors:{1U,8U,16U})
        for(std::uint64_t us:{8000U,16000U,100000U}) for(bool changed:{false,true}) {
            if(mode>=5 && actors!=1) continue;
            std::fprintf(stderr,"case=%s actors=%u frameUs=%llu mapChange=%u seed=308\n",names[mode],actors,
                static_cast<unsigned long long>(us),unsigned(changed));
            const auto one=p308HostRun(actors,us,mode,changed),two=p308HostRun(actors,us,mode,changed);
            if(one.trace!=two.trace) {
                std::size_t index=0;
                while(index<one.trace.size() && index<two.trace.size() && one.trace[index]==two.trace[index]) ++index;
                std::fprintf(stderr,"first divergent event=%zu\nfirst=%s\nsecond=%s\n",index,
                    index<one.trace.size() ? one.trace[index].c_str():"<end>",
                    index<two.trace.size() ? two.trace[index].c_str():"<end>");
            }
            assert(one.trace==two.trace && one.frames==two.frames && one.totalQueries==two.totalQueries && one.replans==two.replans &&
                one.setupQueries==two.setupQueries && one.discoveryQueries==two.discoveryQueries && one.maxQueries==two.maxQueries);
            if(!first) out<<','; first=false;
            out<<"{\"scenario\":\""<<names[mode]<<"\",\"frameUs\":"<<us<<",\"actors\":"<<actors
                <<",\"variant\":\""<<(changed ? "map-change":"clean")<<"\",\"seed\":308,\"outcome\":\""<<outcomes[mode]
                <<"\",\"expectedOutcome\":\""<<outcomes[mode]<<"\",\"frames\":"<<one.frames<<",\"elapsedUs\":"<<one.elapsedUs
                <<",\"maxQueriesPerActorFrame\":"<<one.maxQueries<<",\"totalQueries\":"<<one.totalQueries<<",\"replans\":"<<one.replans
                <<",\"setupQueries\":"<<one.setupQueries<<",\"discoveryQueries\":"<<one.discoveryQueries
                <<",\"traceLimit\":"<<P308HostResult::traceLimit<<",\"traceTruncated\":false,\"replayEqual\":true,\"trace\":[";
            for(std::size_t i=0;i<one.trace.size();++i) { if(i) out<<','; out<<one.trace[i]; }
            out<<"]}";
        }
    out<<"]}\n"; return out ? 0:2;
}
