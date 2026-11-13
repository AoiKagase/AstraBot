#include "plugin/plugin_runtime.hpp"
namespace { bool testWeaponSwitch(astrabot::metamod::PluginRuntime &,enginefuncs_t &,globalvars_t &); }
#define ASTRABOT_EXTRA_ACTOR_TEST testWeaponSwitch
#define ASTRABOT_ACTOR_PROFILE_TEXT "AnyProfile { Name = \"Any Bot\" Team = any Difficulty = 1 Skill = 70 }\n"
#include "compat_actor_command_tests.cpp"
namespace {
unsigned switchRequests=0, pistolShots=0;
int activeEquipment=22;
astrabot::metamod::PluginRuntime *equipmentRuntime=nullptr;
int equipmentMessageId(plid_t id,const char *name,int *size)
{
	if(std::strcmp(name,"WeaponList")==0)return 101;
	if(std::strcmp(name,"AmmoX")==0)return 102;
	if(std::strcmp(name,"DeathMsg")==0)return 103;
	if(std::strcmp(name,"ResetHUD")==0)return 104;
	return siteFixtureUserMessageId(id,name,size);
}
void equipmentCommand(edict_t *self)
{
	if(self==&gEntities[0] && std::strcmp(astrabot::metamod::HookCommandArgv(0),"weapon_glock18")==0)
	{
		++switchRequests;activeEquipment=17;
		equipmentRuntime->onMessageBegin(1,94,nullptr,self);
		equipmentRuntime->onWriteByte(1);equipmentRuntime->onWriteByte(17);equipmentRuntime->onWriteByte(20);equipmentRuntime->onMessageEnd();
	}
	hookedGameClientCommand(self);
}
void equipmentMove(edict_t *self,const float *angles,float forward,float side,float up,unsigned short buttons,byte impulse,byte msec)
{
	if(self==&gEntities[0] && activeEquipment==17 && (buttons&IN_ATTACK)!=0)++pistolShots;
	runPlayerMove(self,angles,forward,side,up,buttons,impulse,msec);
}
bool testWeaponSwitch(astrabot::metamod::PluginRuntime &runtime,enginefuncs_t &engine,globalvars_t &globals)
{
	equipmentRuntime=&runtime;gpMetaUtilFuncs->pfnGetUserMsgID=&equipmentMessageId;
	gHookedGameDllTable.pfnClientCommand=&equipmentCommand;
	engine.pfnRunPlayerMove=&equipmentMove;engine.pfnTraceLine=&traceObjectiveVisibility;
	engine.pfnRandomFloat=&siteFixtureRandomFloat;engine.pfnRandomLong=&siteFixtureRandomLong;
	runtime.giveEnginePointers(&engine,&globals);gObjectiveEntitiesAvailable=false;gObjectiveTraceVisible=true;
	gCaptureMovementLogs=true;gProfileEnabled=true;
	gServerSetsEntityTeam=true;
	if(!check(static_cast<astrabot::metamod::BotObservationServices &>(runtime).behavior_managedProfiles(0).skill>50,
		"tactical switching fixture requires skill above 0.5"))return false;
	bool passed=true;
	for(int reserve:{30,0})
	{
		runtime.onRoundLifecycleMessage("World triggered \"Round_End\"\n");
		runtime.onRoundLifecycleMessage("World triggered \"Round_Start\"\n");
		auto &self=gEntities[0];self.v.flags=FL_CLIENT|FL_FAKECLIENT|FL_ONGROUND;
		self.v.health=100;self.v.deadflag=DEAD_NO;self.v.team=1;self.v.origin=Vector(80,32,36);
		self.v.view_ofs=Vector(0,0,17);self.v.v_angle=Vector(0,0,0);self.v.fov=90;
		self.v.solid=SOLID_SLIDEBOX;self.v.movetype=MOVETYPE_WALK;self.v.maxspeed=240;
		self.v.weapons=(1<<22)|(1<<17)|(1<<29);
		runtime.onMessageBegin(2,86,nullptr,nullptr);runtime.onWriteByte(1);runtime.onWriteString("TERRORIST");runtime.onMessageEnd();
		gEntities[1].v.flags=FL_CLIENT;gEntities[1].v.health=100;gEntities[1].v.deadflag=DEAD_NO;
		gEntities[1].v.team=2;gEntities[1].v.origin=Vector(180,32,36);
		runtime.onMessageBegin(2,86,nullptr,nullptr);runtime.onWriteByte(2);runtime.onWriteString("CT");runtime.onMessageEnd();
		activeEquipment=22;switchRequests=0;pistolShots=0;
		for(const int id:{22,17})
		{
			runtime.onMessageBegin(1,101,nullptr,&self);runtime.onWriteString(id==22?"weapon_m4a1":"weapon_glock18");
			for(int v:{id==22?4:10,90,255,255,id==22?0:1,0,id,0})runtime.onWriteByte(v);
			runtime.onMessageEnd();
		}
		runtime.onMessageBegin(1,94,nullptr,&self);runtime.onWriteByte(0);runtime.onWriteByte(17);runtime.onWriteByte(20);runtime.onMessageEnd();
		for(int ammo:{4,10}){runtime.onMessageBegin(1,102,nullptr,&self);runtime.onWriteByte(ammo);runtime.onWriteByte(ammo==4?reserve:40);runtime.onMessageEnd();}
		for(unsigned tick=0;tick<80;++tick)
		{
			runtime.onMessageBegin(1,94,nullptr,&self);runtime.onWriteByte(1);runtime.onWriteByte(activeEquipment);runtime.onWriteByte(activeEquipment==22?0:20);runtime.onMessageEnd();
			globals.time+=.125f;runtime.onStartFrame();runtime.onStartFramePost();
			if(switchRequests>0 && pistolShots>0)break;
		}
		if(switchRequests==0 || pistolShots==0)
		{
			std::fprintf(stderr,"reserve=%d selected=%u shots=%u active=%d state=%s overlay=%d\n",reserve,switchRequests,pistolShots,activeEquipment,
				astrabot::compat::toString(static_cast<astrabot::metamod::BotCommandServices &>(runtime).behavior_managedBotStateMachines(0).state()),
				static_cast<astrabot::metamod::BotCommandServices &>(runtime).behavior_managedBotStateMachines(0).attackOverlayActive()?1:0);
			for(auto i=gMovementLogs.rbegin();i!=gMovementLogs.rend();++i)if(i->find("action sensor ")==0){std::fprintf(stderr,"%s\n",i->c_str());break;}
			const auto &belief=static_cast<astrabot::metamod::BotBehaviorServices &>(runtime).observation_managedBotPerception(0).snapshot();
			std::fprintf(stderr,"reaction=%f full=%u time=%f\n",static_cast<astrabot::metamod::BotObservationServices &>(runtime).behavior_managedProfiles(0).reactionTimeSeconds,
				static_cast<astrabot::metamod::BotObservationServices &>(runtime).service_managedBotFullUpdateSequences()[0],globals.time);
			std::fprintf(stderr,"observer=%u:%u\n",belief.identity().observer.slot,belief.identity().observer.generation);
			for(std::size_t i=0;i<belief.actorCount();++i){const auto *a=belief.actorAt(i);std::fprintf(stderr,"seen slot=%u:%u relation=%d visible=%d fov=%d los=%d state=%d\n",a->actor.slot,a->actor.generation,static_cast<int>(a->relation),a->visible?1:0,a->fovPassed?1:0,a->losPassed?1:0,static_cast<int>(a->state));}
		}
		passed=check(switchRequests>0,reserve?"empty primary clip switches to pistol during combat despite remaining reserve":"totally empty primary switches to pistol")&&passed;
		passed=check(pistolShots>0,"observed pistol equip resumes actual attack input")&&passed;
		if(reserve==30 && switchRequests>0)
		{
			const auto prior=switchRequests;
			runtime.onMessageBegin(2,103,nullptr,nullptr);runtime.onWriteByte(0);runtime.onWriteByte(1);runtime.onWriteByte(0);runtime.onWriteString("world");runtime.onMessageEnd();
			runtime.onMessageBegin(1,104,nullptr,&self);runtime.onMessageEnd();
			activeEquipment=22;
			for(int ammo:{4,10}){runtime.onMessageBegin(1,102,nullptr,&self);runtime.onWriteByte(ammo);runtime.onWriteByte(40);runtime.onMessageEnd();}
			runtime.onMessageBegin(1,94,nullptr,&self);runtime.onWriteByte(0);runtime.onWriteByte(17);runtime.onWriteByte(20);runtime.onMessageEnd();
			for(unsigned tick=0;tick<16 && switchRequests==prior;++tick){runtime.onMessageBegin(1,94,nullptr,&self);runtime.onWriteByte(1);runtime.onWriteByte(activeEquipment);runtime.onWriteByte(activeEquipment==22?0:20);runtime.onMessageEnd();globals.time+=.125f;runtime.onStartFrame();runtime.onStartFramePost();}
			passed=check(switchRequests>prior,"DeathMsg and ResetHUD between full updates clear the old 5-second equip wait")&&passed;
		}
	}
	return passed;
}
}
