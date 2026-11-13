#include "astrabot/compat/weapon_equipment.hpp"
#include "astrabot/compat/public_observation.hpp"
#include <cstdio>
using namespace astrabot;
namespace {
bool check(bool value,const char *message){if(!value)std::fprintf(stderr,"%s\n",message);return value;}
compat::EquipmentInput input()
{
	compat::EquipmentInput in{};in.actor={1,1};in.frame={1,1,1};in.now=10;in.skill=.9f;
	in.currentWeapon=22;in.weaponObservationTick=1;in.primary=true;in.clipKnown=true;in.reserveKnown=true;
	in.reserve=30;in.attacking=true;in.pistolWeapon=17;in.pistolAmmo=compat::EquipmentAmmo::Usable;in.hasKnife=true;
	return in;
}
bool testConditions()
{
	for(unsigned variant=0;variant<13;++variant)
	{
		auto in=input();unsigned expected=17;
		switch(variant){
		case 0:in.skill=.5f;expected=0;break;
		case 1:in.skill=.51f;break;
		case 2:in.attacking=false;expected=0;break;
		case 3:in.attacking=false;in.skill=.1f;in.reserve=0;break;
		case 4:in.reserveKnown=false;break;
		case 5:in.allowPistols=false;expected=0;break;
		case 6:in.clipKnown=false;expected=0;break;
		case 7:in.reloading=true;expected=0;break;
		case 8:in.defusing=true;expected=0;break;
		case 9:in.pistolWeapon=0;in.reserve=0;expected=29;break;
		case 10:in.pistolAmmo=compat::EquipmentAmmo::Unknown;in.reserve=0;expected=0;break;
		case 11:in.pistolAmmo=compat::EquipmentAmmo::Empty;in.reserve=0;expected=29;break;
		case 12:in.clip=1;expected=0;break;
		}
		compat::WeaponEquipmentPlanner planner;const auto d=planner.update(in);
		if(!check(d.selectWeapon==expected,"equipment condition matrix: skill, combat, reserve, exclusions and unknown ammo"))return false;
	}
	return true;
}
bool testReceiptAndClock()
{
	auto in=input();compat::WeaponEquipmentPlanner planner;
	if(!check(planner.update(in).selectWeapon==17,"first eligible switch is immediate"))return false;
	planner.dispatched(in.actor,in.frame,in.now,17,true);
	in.frame.tick=2;in.now=14.999f;
	if(!check(planner.update(in).ownsAction,"5-second interval suppresses reload as well as another switch"))return false;
	in.frame.tick=3;in.now=15;
	if(!check(planner.update(in).selectWeapon==17,"5-second exact boundary permits retry without receipt"))return false;
	planner.dispatched(in.actor,in.frame,in.now,17,true);
	in.frame.tick=4;in.now=15.1f;in.currentWeapon=17;in.primary=false;in.weaponObservationTick=2;
	if(!check(planner.update(in).ownsAction,"old weapon observation is not a new equip receipt"))return false;
	in.frame.tick=5;in.weaponObservationTick=4;
	if(!check(!planner.update(in).ownsAction,"fresh CurWeapon releases the pending selection"))return false;
	in=input();planner.reset();planner.dispatched(in.actor,in.frame,in.now,17,false);
	in.frame.tick=2;in.now=10.1f;
	if(!check(planner.update(in).selectWeapon==0,"failed transport has bounded retry and does not assert weapon success"))return false;
	in.frame.tick=3;in.now=15;
	if(!check(planner.update(in).selectWeapon==17,"failed transport can retry at the deadline"))return false;
	for(unsigned variant=0;variant<4;++variant)
	{
		in=input();planner.reset();planner.dispatched(in.actor,in.frame,in.now,17,true);
		in.frame.tick=2;in.now=10.1f;
		if(variant==0)in.actor.generation++;
		if(variant==1)in.frame.mapGeneration++;
		if(variant==2)in.frame.roundGeneration++;
		if(variant==3)in.now=1;
		if(!check(planner.update(in).selectWeapon==17,"actor/map/round/time restart clears old equip state"))return false;
	}
	return true;
}
bool testPublicClipHistory()
{
	compat::PublicObservationStore store;compat::PublicActorState state{};
	world::ActorKey actor{1,1};world::FrameIdentity frame{1,1,1};store.bind(actor,frame);
	store.currentWeapon(actor,frame,1,22,0);store.currentWeapon(actor,frame,0,17,20);
	if(!check(store.read(actor,frame,&state)&&state.weaponId==22&&state.clip==0&&state.weaponClipKnown[17]&&state.weaponClips[17]==20,
		"inactive CurWeapon records pistol clip without changing active primary"))return false;
	store.currentWeapon(actor,frame,0,17,-1);
	if(!check(store.read(actor,frame,&state)&&!state.weaponClipKnown[17],"unknown clip invalidates the cached certainty"))return false;
	frame.roundGeneration++;store.bind(actor,frame);
	if(!check(store.read(actor,frame,&state)&&!state.weaponClipKnown[17],"new round cannot reuse earlier pistol clip"))return false;
	store.currentWeapon(actor,frame,0,17,20);store.resetSlot(1);
	return check(!store.read(actor,frame,&state),"removed actor cannot reuse ammo history");
}
}
int main(){return testConditions()&&testReceiptAndClock()&&testPublicClipHistory()?0:1;}
