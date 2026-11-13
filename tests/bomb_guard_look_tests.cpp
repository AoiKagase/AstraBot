#include "plugin/plugin_runtime.hpp"
#include "astrabot/compat/guard_look.hpp"
namespace { bool testBombGuardLook(astrabot::metamod::PluginRuntime &,enginefuncs_t &,globalvars_t &); }
#define ASTRABOT_EXTRA_ACTOR_TEST testBombGuardLook
#include "compat_actor_command_tests.cpp"
namespace {
bool testGuardPolicy()
{
	using namespace astrabot;
	nav::NavArea previous{},here{};previous.extent={{0,0,0},{100,100,0}};
	here.extent={{100,20,0},{200,80,0}};
	const auto portal=compat::guardApproachPoint(previous,here,1);
	if(!check(portal.x==100 && portal.y==50,"guard approach uses the portal midpoint without walking inset"))return false;
	compat::ScriptedRandomSource random({compat::RandomTapeEntry::floatEntry("LookAround.StateInterval",{1,1},{},1,2,1.5f),
		compat::RandomTapeEntry::longEntry("LookAround.ApproachPoint",{1,1},{},0,1,1)});
	compat::GuardLookPlanner planner;float pitch=0,yaw=0;
	planner.begin({1,1},{1,1,1},{9,1},{0,0,0},1,10,0,{{100,0,0},{0,100,0}});
	if(!planner.update(10,false,{0,0,36},pitch,yaw,random,{},&pitch,&yaw))return false;
	for(unsigned i=1;i<10;++i)if(!planner.update(10+.1f*i,false,{0,0,36},pitch,yaw,random,{},&pitch,&yaw))return false;
	if(!check(random.verifyComplete() && std::fabs(yaw-90)<.01f,"ordinary guard selects an approach and retains it until the 1-2 second timer"))return false;
	if(!check(!planner.current({1,1},{1,2,1},{9,1},{0,0,0},1,11),"guard watch cannot survive a round change"))return false;
	compat::ScriptedRandomSource slow({compat::RandomTapeEntry::floatEntry("LookAround.StateInterval",{1,1},{},5,10,7),
		compat::RandomTapeEntry::longEntry("LookAround.ApproachPoint",{1,1},{},0,0,0)});
	planner.begin({1,1},{1,1,1},{9,1},{0,0,0},1,10,0,{{100,0,0}});
	if(!check(planner.update(10,true,{0,0,36},0,0,slow,{},&pitch,&yaw) && slow.verifyComplete(),"sniper watch uses the reference 5-10 second timer"))return false;
	planner.reset();return check(!planner.current({1,1},{1,1,1},{9,1},{0,0,0},1,10),"leaving guard clears its retained watch");
}
bool guardSelectedPrimary=false;
void guardGameCommand(edict_t *entity)
{
	if(entity==&gEntities[0] && std::strcmp(astrabot::metamod::HookCommandArgv(0),"weapon_m4a1")==0)guardSelectedPrimary=true;
	hookedGameClientCommand(entity);
}
void traceGuardWall(const float *start,const float *end,int flags,edict_t *self,TraceResult *result)
{
	traceObjectiveVisibility(start,end,flags,self,result);
	const float dx=end[0]-start[0],dy=end[1]-start[1];
	if(std::hypot(dx,dy)>900) result->flFraction = dy>900 ? 1.0f : .05f;
}
bool testBombGuardLook(astrabot::metamod::PluginRuntime &runtime,enginefuncs_t &engine,globalvars_t &globals)
{
	if(!testGuardPolicy())return false;
	const char *path="astrabot_guard_look.nav";
	if(!writeTwoSiteNavigation(path,true,true))return false;
	const astrabot::metamod::NavLoadRequest load={path,"astrabot_guard_look",runtime.snapshot().mapGeneration,false,0,false,0};
	const auto loaded=runtime.loadNavigationFile(&load);std::remove(path);
	if(!check(loaded==astrabot::metamod::NavLoadResult::Loaded,"guard NAV loaded"))return false;
	gObjectiveEntitiesAvailable=true;gObjectiveTraceVisible=true;gCaptureMovementLogs=true;gProfileEnabled=true;
	engine.pfnTraceLine=&traceGuardWall;engine.pfnRandomFloat=&siteFixtureRandomFloat;engine.pfnRandomLong=&siteFixtureRandomLong;
	runtime.giveEnginePointers(&engine,&globals);globals.mapname=6;globals.maxEntities=8;
	for(unsigned i=1;i<5;++i){gEntities[i].v.health=0;gEntities[i].v.flags=0;}
	auto &self=gEntities[0];self.v.flags=FL_CLIENT|FL_FAKECLIENT|FL_ONGROUND;
	self.v.health=100;self.v.team=1;self.v.solid=SOLID_SLIDEBOX;self.v.movetype=MOVETYPE_WALK;
	self.v.mins=Vector(-16,-16,-36);self.v.maxs=Vector(16,16,36);self.v.maxspeed=240;
	self.v.origin=Vector(80,32,36);self.v.view_ofs=Vector(0,0,17);self.v.v_angle=Vector(0,180,0);self.v.weapons=1<<22;
	gPlantedBomb={};gPlantedBomb.v.classname=2;gPlantedBomb.v.model=3;gPlantedBomb.serialnumber=5;
	gPlantedBomb.v.origin=Vector(224,32,36);gPlantedBomb.v.dmgtime=globals.time+100;
	runtime.onMessageBegin(MSG_ALL,98,nullptr,nullptr);
	runtime.onWriteByte(4);runtime.onWriteString("#Bomb_Planted");runtime.onMessageEnd();
	const auto tick=[&](){runtime.onMessageBegin(1,94,nullptr,&self);runtime.onWriteByte(1);runtime.onWriteByte(22);runtime.onWriteByte(30);runtime.onMessageEnd();
		self.v.absmin=self.v.origin+self.v.mins;self.v.absmax=self.v.origin+self.v.maxs;
		globals.time+=.1f;runtime.onStartFrame();runtime.onStartFramePost();};
	gMovementLogs.clear();for(unsigned i=0;i<24;++i)tick();
	float coverX=0;
	for(const auto &s:gMovementLogs)if(s.find("state=STATE-HIDE")!=std::string::npos){
		if(s.find("goal_position=(32.0 50.0 0.0)")!=std::string::npos)coverX=32;
		if(s.find("goal_position=(224.0 50.0 0.0)")!=std::string::npos)coverX=224;}
	if(!check(coverX!=0,"planted bomb chooses cover"))return false;
	self.v.origin=Vector(coverX,50,36);self.v.v_angle=Vector(0,180,0);
	for(unsigned i=0;i<24;++i)tick();
	bool passed=check(std::fabs(self.v.v_angle.y-90)<1,"guard turns away from the wall toward the longest open ray") &&
		check(gLastRunPlayerMoveForward==0 && gLastRunPlayerMoveSide==0,"guard watches without leaving cover");
	gHookedGameDllTable.pfnClientCommand=&guardGameCommand;self.v.weapons=(1<<22)|(1<<6);
	for(unsigned i=0;i<8;++i){runtime.onMessageBegin(1,94,nullptr,&self);runtime.onWriteByte(1);runtime.onWriteByte(6);runtime.onWriteByte(1);runtime.onMessageEnd();
		globals.time+=.1f;runtime.onStartFrame();runtime.onStartFramePost();}
	passed=check(guardSelectedPrimary,"post-plant guard equips a usable gun through its GameDLL classname") && passed;
	gEntities[1].v.flags=FL_CLIENT;gEntities[1].v.team=0;gEntities[1].v.health=100;gEntities[1].v.deadflag=DEAD_NO;
	gEntities[1].v.origin=self.v.origin+Vector(0,100,0);
	runtime.onMessageBegin(MSG_ALL,86,nullptr,nullptr);runtime.onWriteByte(2);runtime.onWriteString("CT");runtime.onMessageEnd();
	bool attacked=false;for(unsigned i=0;i<32;++i){tick();attacked=attacked || (gLastRunPlayerMoveButtons&IN_ATTACK)!=0;}
	return check(attacked,"a visible human enemy interrupts post-plant guard and receives attack input") && passed;
}
}
