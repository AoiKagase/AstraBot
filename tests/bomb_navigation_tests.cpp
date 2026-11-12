#include "astrabot/compat/bomb_search.hpp"
#include <cmath>
#include <cstdio>

namespace
{
astrabot::nav::NavArea area(unsigned int id, float center, float width = 150.0f)
{
	astrabot::nav::NavArea result{};
	result.id = id;
	result.extent = {{center-width*0.5f,0,0},{center+width*0.5f,width,0}};
	return result;
}
}

int main()
{
	using namespace astrabot;
	using namespace compat;
	int failures = 0;
	const auto check = [&failures](bool ok, const char *message)
	{ if (!ok) { ++failures; std::fprintf(stderr,"FAIL: %s\n",message); } };
	nav::NavDocument document;
	for (unsigned int i=1;i<=33;++i) document.addArea(area(i,float(i)*200.0f));
	document.addArea(area(100U,100000.0f,10.0f));
	ScriptedRandomSource escape({RandomTapeEntry::longEntry("Bomb.EscapeArea",{},{},0,31,0)});
	nav::AreaId goal = 0U;
	check(selectBombEscapeArea(document,{0,0,0},escape,{},{},&goal) && goal==33U && escape.verifyComplete(),
		"32-candidate replacement selects farthest cheap area without reachability filtering or small-area inclusion");
	nav::NavDocument tied;
	for (unsigned int i=1;i<=33;++i) tied.addArea(area(i,200.0f));
	ScriptedRandomSource tie({RandomTapeEntry::longEntry("Bomb.EscapeArea",{},{},0,31,0)});
	check(selectBombEscapeArea(tied,{0,0,0},tie,{},{},&goal) && goal==1U,"equal cost does not replace existing candidate");
	nav::NavDocument small;
	small.addArea(area(1U,0,10)); small.addArea(area(2U,100,10));
	ScriptedRandomSource fallback({RandomTapeEntry::longEntry("Bomb.EscapeArea",{},{},0,1,1)});
	check(selectBombEscapeArea(small,{0,0,0},fallback,{},{},&goal) && goal==2U,"small-area fallback selects from all NAV areas");
	nav::NavDocument travel;
	travel.setSourceIdentity({5U,100U,200U});
	travel.addArea(area(1U,100.0f,200.0f));
	nav::NavSnapshotPublisher publisher;
	publisher.publish(&travel,1U);
	float seconds = 99.0f;
	check(estimateBombTravel(publisher.snapshot(),{10,10,0},{150,10,0},100,10000,&seconds) && std::fabs(seconds-1.4f)<0.0001f,
		"same-area travel estimate retains point distance rather than zero corridor length");
	seconds=99;
	check(!estimateBombTravel(publisher.snapshot(),{10,10,0},{150,10,0},0,10000,&seconds) && seconds==99,
		"unknown public movement speed does not manufacture travel time");
	std::vector<BombSearchSite> sites(2U);
	sites[0].entity={10U,1U}; sites[0].travel=3000;
	sites[1].entity={11U,1U}; sites[1].travel=4000;
	BombSiteSearchMemory search;
	ScriptedRandomSource order({RandomTapeEntry::longEntry("Bomb.SearchOrder",{},{},0,1,1),
		RandomTapeEntry::longEntry("Bomb.SearchOrder",{},{},1,1,1)});
	std::size_t selected=99;
	check(search.choose({1,1,1},sites,order,{},{},&selected) && selected==1 && order.verifyComplete(),
		"distant sites use round shuffled order");
	search.clear(sites[1].entity);
	sites[0].travel=1000;
	check(search.choose({1,1,2},sites,order,{},{},&selected) && selected==0,"nearest uncleared site within 2000 takes precedence");
	search.clear(sites[0].entity);
	check(search.choose({1,1,3},sites,order,{},{},&selected) && selected==1,"all-cleared sites restart shuffled search");
	ScriptedRandomSource nextRound({RandomTapeEntry::longEntry("Bomb.SearchOrder",{},{},0,1,0),
		RandomTapeEntry::longEntry("Bomb.SearchOrder",{},{},1,1,1)});
	check(search.choose({1,2,1},sites,nextRound,{},{},&selected) && selected==0 && nextRound.verifyComplete(),"new round resets clear state and shuffles once");
	BombSearchSite zone{};
	zone.entity={12,1}; zone.extent={{50,60,-50},{150,160,50}};
	ScriptedRandomSource zoneRandom({RandomTapeEntry::longEntry("Bomb.SearchZoneArea",{},{},0,0,0)});
	nav::NavVector point{};
	check(bombSitePoint(travel,zone,16,zoneRandom,{},{},&point) && point.x==100 && point.y==110 && point.z==0,
		"brush site selects overlapping NAV/zone midpoint and NAV surface height");
	zone.legacy=true; zone.center={250,250,0}; zone.extent={{-6,-6,-306},{506,506,306}};
	ScriptedRandomSource legacyRandom({RandomTapeEntry::longEntry("Bomb.SearchZoneArea",{},{},0,0,0)});
	check(bombSitePoint(travel,zone,16,legacyRandom,{},{},&point) && point.x==200 && point.y==200,
		"legacy site uses closest NAV point to site center");
	return failures==0?0:1;
}
