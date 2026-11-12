#include "astrabot/compat/buy_controller.hpp"
#include <cmath>
#include <array>

namespace astrabot { namespace compat {
namespace {
enum class Group { Pistol, Shotgun, Smg, Rifle, Sniper, MachineGun, Grenade };
struct Item { int id, price; const char *alias; ProfileTeam team; Group group; bool preferred; };
const Item stock[] = {
 {21,1700,"m3",ProfileTeam::Any,Group::Shotgun,false}, {5,3000,"xm1014",ProfileTeam::Any,Group::Shotgun,false},
 {23,1250,"tmp",ProfileTeam::CounterTerrorist,Group::Smg,false}, {7,1400,"mac10",ProfileTeam::Terrorist,Group::Smg,false},
 {19,1500,"mp5",ProfileTeam::Any,Group::Smg,false}, {12,1700,"ump45",ProfileTeam::Any,Group::Smg,false}, {30,2350,"p90",ProfileTeam::Any,Group::Smg,false},
 {15,2250,"famas",ProfileTeam::CounterTerrorist,Group::Rifle,true}, {14,2000,"galil",ProfileTeam::Terrorist,Group::Rifle,true},
 {28,2500,"ak47",ProfileTeam::Terrorist,Group::Rifle,true}, {27,3500,"sg552",ProfileTeam::Terrorist,Group::Rifle,true}, {3,2750,"scout",ProfileTeam::Any,Group::Sniper,false},
 {22,3100,"m4a1",ProfileTeam::CounterTerrorist,Group::Rifle,true}, {8,3500,"aug",ProfileTeam::CounterTerrorist,Group::Rifle,false},
 {13,4200,"sg550",ProfileTeam::CounterTerrorist,Group::Sniper,true},
 {18,4750,"awp",ProfileTeam::Any,Group::Sniper,true}, {24,5000,"g3sg1",ProfileTeam::Terrorist,Group::Sniper,true},
 {20,5750,"m249",ProfileTeam::Any,Group::MachineGun,false},
 {1,600,"p228",ProfileTeam::Any,Group::Pistol,true}, {26,650,"deagle",ProfileTeam::Any,Group::Pistol,true},
 {11,750,"fn57",ProfileTeam::CounterTerrorist,Group::Pistol,true}, {10,800,"elites",ProfileTeam::Terrorist,Group::Pistol,true},
 {16,500,"usp",ProfileTeam::CounterTerrorist,Group::Pistol,false}, {17,400,"glock",ProfileTeam::Terrorist,Group::Pistol,false},
 {4,300,"hegren",ProfileTeam::Any,Group::Grenade,false}, {9,300,"sgren",ProfileTeam::Any,Group::Grenade,false}, {25,200,"flash",ProfileTeam::Any,Group::Grenade,false}
};
bool owns(const BuyContext &c,int id) { return id>0 && id<32 && (c.weaponMask & (1U<<id))!=0; }
bool primary(Group g) { return g!=Group::Pistol && g!=Group::Grenade; }
bool allowed(const Item &w,const BuyContext &c) {
 if (w.team!=ProfileTeam::Any && w.team!=c.team) return false;
 switch(w.group) { case Group::Pistol:return c.allowPistols; case Group::Shotgun:return c.allowShotguns;
 case Group::Smg:return c.allowSubMachineGuns; case Group::Rifle:return c.allowRifles; case Group::Sniper:return c.allowSnipers;
 case Group::MachineGun:return c.allowMachineGuns; case Group::Grenade:return c.allowGrenades; }
 return false;
}
bool draw(const BuyContext &c,const char *name,float chance,bool &value) {
 if (!c.random) return false;
 const auto r=c.random->nextFloat(RandomRequest::floatRequest(name,c.randomActor,c.randomTiming,0,100));
 if(r.status!=RandomStatus::Ok || !std::isfinite(r.value) || r.value<0 || r.value>100) return false;
 value=r.value<chance; return true;
}
const Item *choose(const BuyContext &c,const Item *const *candidates,std::size_t count,const char *name) {
 if(count==0 || !c.random) return nullptr;
 const auto r=c.random->nextLong(RandomRequest::longRequest(name,c.randomActor,c.randomTiming,0,static_cast<int>(count)-1));
 return r.status==RandomStatus::Ok && r.value>=0 && static_cast<std::size_t>(r.value)<count ? candidates[r.value] : nullptr;
}
}
void BuyController::reset() { *this=BuyController(); }
BuyDecision BuyController::update(const BuyContext &c,const ProfileRecord &profile) {
 if(generation_!=c.generation || round_!=c.round) { reset(); generation_=c.generation; round_=c.round; }
 if(!c.alive || !c.inBuyZone || !c.buyTimeOpen || !std::isfinite(c.nowSeconds) || !std::isfinite(c.stateEnteredSeconds)) return {BuyStatus::Unavailable,nullptr,0};
 if(!c.moneyKnown || !c.weaponMaskKnown || c.money<0) return {BuyStatus::UnknownObservation,nullptr,0};
 bool hasPrimary=false,hasPistol=false,hasGrenade=false;
 for(const auto &w:stock) if(owns(c,w.id)) { hasPrimary=hasPrimary || primary(w.group); hasPistol=hasPistol || w.group==Group::Pistol; hasGrenade=hasGrenade || w.group==Group::Grenade; }
 if(!initialized_) {
  if(initializationStep_==0) {
   if(c.team==ProfileTeam::CounterTerrorist && c.bombScenario && !c.career && c.allowDefuseKit && !draw(c,"buy.defuse_kit",50,buyKit_)) return {BuyStatus::Unavailable,nullptr,0};
   ++initializationStep_;
  }
  if(initializationStep_==1) {
   if(c.team==ProfileTeam::CounterTerrorist && !hasPrimary && c.allowShield && c.money>2500 && !draw(c,"buy.shield",c.money<4000?33.3f:10.0f,buyShield_)) return {BuyStatus::Unavailable,nullptr,0};
   ++initializationStep_;
  }
  if(initializationStep_==2) {
   if(c.allowGrenades && !draw(c,"buy.grenade",33.3f,buyGrenade_)) return {BuyStatus::Unavailable,nullptr,0};
   ++initializationStep_;
  }
  if(initializationStep_==3 && c.allowPistols) {
   if(!hasPistol) buyPistol_=true;
   else if(owns(c,c.team==ProfileTeam::Terrorist?17:16)) {
    const bool pistolOnly=!c.allowShotguns && !c.allowSubMachineGuns && !c.allowRifles && !c.allowMachineGuns && !c.allowShield && !c.allowSnipers;
    if(!draw(c,"buy.pistol",pistolOnly || c.money<1000?75.0f:33.3f,buyPistol_)) return {BuyStatus::Unavailable,nullptr,0};
   }
  }
  initialized_=true;
 }
 if(c.nowSeconds-c.stateEnteredSeconds<2.0) return {BuyStatus::WaitingReceipt,nullptr,0};
 if(complete_) return {BuyStatus::Complete,nullptr,0};
	if(receiptFailed_) return {BuyStatus::ReceiptTimeout,nullptr,0};
 if(pendingWeapon_ || pendingEquipment_) {
  const bool uncertainEquipment = pendingEquipment_ < 0;
  const int equipmentReceipt = uncertainEquipment ? -pendingEquipment_ : pendingEquipment_;
  bool receipt=pendingWeapon_ && owns(c,pendingWeapon_);
  if(equipmentReceipt==1) receipt=c.armorKnown && c.armor>=100;
  if(equipmentReceipt==2) receipt=c.armorKnown && c.armor>=100 && c.helmetKnown && c.hasHelmet;
  if(equipmentReceipt==3) receipt=c.defuseKitKnown && c.hasDefuseKit;
  if(equipmentReceipt==4) receipt=c.ammoKnown && !c.needsPrimaryAmmo;
  if(equipmentReceipt==5) receipt=c.ammoKnown && !c.needsSecondaryAmmo;
  if(equipmentReceipt==6) receipt=c.shieldKnown && c.hasShield;
  if(!receipt && c.nowSeconds-sentAt_<0.2) return {BuyStatus::WaitingReceipt,nullptr,pendingWeapon_};
  if(!receipt && uncertainEquipment) {
   // Unknown equipment permits one authoritative GameDLL attempt. A missing
   // receipt is not success: report it once and skip only this equipment so
   // later purchases are not indefinitely blocked by absent HUD messages.
   pendingWeapon_=0; pendingEquipment_=0; retries_=0; ++stage_;
   return {BuyStatus::ReceiptTimeout,nullptr,0};
  }
  if(!receipt && retries_>=3 && stage_!=0) { receiptFailed_=true; return {BuyStatus::ReceiptTimeout,nullptr,0}; }
  if(receipt) { if(stage_==0) stage_=1; else ++stage_; retries_=0; }
  pendingWeapon_=0; pendingEquipment_=0;
 }
 const auto equipment=[this,&c](const char *alias,int receipt,bool uncertain=false)->BuyDecision {
  pendingEquipment_=uncertain ? -receipt : receipt; sentAt_=c.nowSeconds; ++retries_; return {BuyStatus::Command,alias,0};
 };
 const auto weapon=[this,&c](const Item &item)->BuyDecision {
  pendingWeapon_=item.id; sentAt_=c.nowSeconds; ++retries_; return {BuyStatus::Command,item.alias,item.id};
 };
 for(int guard=0;guard<9;++guard) {
  if(stage_==0) {
   if(hasPrimary || (c.shieldKnown && c.hasShield)) { stage_=1; continue; }
   while(preferenceIndex_<profile.weaponPreferenceCount && preferenceIndex_<16) {
    if(preferenceAttempts_>=2) { ++preferenceIndex_; preferenceAttempts_=0; continue; }
    if(profile.weaponPreferences[preferenceIndex_]==99 && c.team==ProfileTeam::CounterTerrorist && c.allowShield && c.money>=2200) {
     ++preferenceAttempts_; return equipment("shield",6);
    }
    const Item *preferred=nullptr;
    for(const auto &w:stock) if(w.id==profile.weaponPreferences[preferenceIndex_] && allowed(w,c)) { preferred=&w; break; }
    if(!preferred || !primary(preferred->group) || c.money<preferred->price) { ++preferenceIndex_; preferenceAttempts_=0; continue; }
    ++preferenceAttempts_; return weapon(*preferred);
   }
   if(primaryAttempts_++>=6) { stage_=1; continue; }
   if(buyShield_ && c.money>=2200) return equipment("shield",6);
   bool wantSniper=false;
   if(!draw(c,"buy.want_sniper",50,wantSniper)) return {BuyStatus::Unavailable,nullptr,0};
   std::array<const Item*,32> candidates={}; std::size_t count=0;
   for(const auto &w:stock) if(primary(w.group) && allowed(w,c) && (w.group!=Group::Sniper || wantSniper) && c.money>=w.price) candidates[count++]=&w;
   if(count==0) { stage_=1; continue; }
   if(primaryAttempts_==1 && profile.difficulty>=2) {
    std::array<const Item*,32> preferred={}; std::size_t preferredCount=0;
    for(std::size_t i=0;i<count;++i) if(candidates[i]->preferred) preferred[preferredCount++]=candidates[i];
    if(preferredCount) { candidates=preferred; count=preferredCount; }
   }
   const Item *selected=choose(c,candidates.data(),count,"buy.primary");
   if(!selected) return {BuyStatus::Unavailable,nullptr,0};
   return weapon(*selected);
  }
  if(stage_==1) { if(c.ammoKnown && c.needsPrimaryAmmo && hasPrimary && c.money>=60) return equipment("primammo",4); ++stage_; continue; }
  if(stage_==2) {
   if(c.allowArmor && (!c.helmetKnown || !c.hasHelmet) && c.money>=1000)
    return equipment("vesthelm",2,!c.armorKnown || !c.helmetKnown);
   if(c.allowArmor && (!c.armorKnown || c.armor<100) && c.money>=650)
    return equipment("vest",1,!c.armorKnown);
   ++stage_; continue;
  }
  if(stage_==3) {
   if(c.allowPistols && buyPistol_) {
    const Item *selected=nullptr;
    for(std::size_t i=0;i<profile.weaponPreferenceCount && i<16 && !selected;++i)
     for(const auto &w:stock) if(w.group==Group::Pistol && w.id==profile.weaponPreferences[i] && allowed(w,c) && c.money>=w.price) { selected=&w; break; }
    if(!selected) {
     std::array<const Item*,3> candidates={}; std::size_t count=0;
     for(const auto &w:stock) if(w.group==Group::Pistol && w.preferred && allowed(w,c) && c.money>=w.price && count<candidates.size()) candidates[count++]=&w;
     if(count) selected=choose(c,candidates.data(),count,"buy.secondary");
     if(count && !selected) return {BuyStatus::Unavailable,nullptr,0};
    }
    if(selected && !owns(c,selected->id)) return weapon(*selected);
   }
   ++stage_; continue;
  }
  if(stage_==4) { if(c.allowPistols && c.ammoKnown && c.needsSecondaryAmmo && c.money>=20) return equipment("secammo",5); ++stage_; continue; }
  if(stage_==5) {
   if(buyGrenade_ && !hasGrenade && c.money>=300) {
    if(!c.random) return {BuyStatus::Unavailable,nullptr,0};
    const auto r=c.random->nextFloat(RandomRequest::floatRequest("buy.grenade_type",c.randomActor,c.randomTiming,0,100));
    if(r.status!=RandomStatus::Ok || !std::isfinite(r.value) || r.value<0 || r.value>100) return {BuyStatus::Unavailable,nullptr,0};
    const int id=r.value<10?9:(c.teamAllBots && r.value<35?25:4);
    for(const auto &w:stock) if(w.id==id) return weapon(w);
   }
   ++stage_; continue;
  }
  if(stage_==6) {
   if(buyKit_ && (!c.defuseKitKnown || !c.hasDefuseKit) && c.money>=200)
    return equipment("defuser",3,!c.defuseKitKnown);
   ++stage_; continue;
  }
  complete_=true; return {BuyStatus::Complete,nullptr,0};
 }
 return {BuyStatus::Unavailable,nullptr,0};
}
} }
