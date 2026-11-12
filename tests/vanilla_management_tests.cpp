#include "astrabot/compat/profile_catalog.hpp"
#include "astrabot/compat/bot_configuration.hpp"
#include "astrabot/compat/buy_controller.hpp"
#include <cstdio>
#include <cstring>

class RecoveringRandom : public astrabot::compat::ICompatibilityRandomSource
{
public:
 int calls=0; bool valid=true;
 astrabot::compat::RandomFloatResult nextFloat(const astrabot::compat::RandomRequest &r) override {
  using namespace astrabot::compat; ++calls;
  const char *expected=calls==1?"buy.defuse_kit":"buy.grenade";
  if(std::strcmp(r.semanticId,expected)!=0) valid=false;
  return {calls==2?RandomStatus::Unavailable:RandomStatus::Ok,90};
 }
 astrabot::compat::RandomLongResult nextLong(const astrabot::compat::RandomRequest &) override { valid=false; return {astrabot::compat::RandomStatus::Unavailable,0}; }
};

int main()
{
 using namespace astrabot::compat;
 int failures = 0;
 const auto check = [&failures](bool ok, const char *name) { if (!ok) { ++failures; std::fprintf(stderr, "FAIL: %s\n", name); } };
 ProfileCatalog profiles;
 ProfileRecord first = {}; std::memcpy(first.name, "first", 6); first.team=ProfileTeam::Any; first.difficulty=1; first.skill=50; first.aggression=50;
 ProfileRecord second=first; std::memcpy(second.name,"second",7); profiles.add(first); profiles.add(second);
 const char *used[] = {"FIRST"}; ProfileRecord selected = {};
 ScriptedRandomSource random({RandomTapeEntry::longEntry("profile.select_unused", {}, {}, 0, 0, 0)});
 check(profiles.selectUnused(ProfileTeam::CounterTerrorist,1,used,1,random,{}, {},&selected)==ProfileSelectionResult::Selected && std::strcmp(selected.name,"second")==0,"unused wildcard profile selected with shared random");
 check(random.verifyComplete(),"profile random consumed once");
 const char *allUsed[]={"first","second"};
 check(profiles.selectUnused(ProfileTeam::Any,1,allUsed,2,random,{}, {},&selected)==ProfileSelectionResult::NoMatch,"exhausted profiles do not reuse names or consume random");
 CvarSnapshot settings = {}; settings.botEnable=1; settings.botQuota=4; settings.botQuotaMode=QuotaMode::Fill; settings.botAutoVacate=true;
 QuotaPopulation population = {}; population.maxClients=8; population.totalClients=6; population.humanClients=3; population.activeHumanClients=3; population.managedBotClients=3;
 check(calculateDesiredBotCount(settings,population)==1,"fill quota includes humans");
 settings.botQuotaMode=QuotaMode::Match; settings.botQuota=2;
 check(calculateDesiredBotCount(settings,population)==4,"match quota bounded by vacant slot");
 settings.botJoinAfterPlayer=true; population.activeHumanClients=0;
 check(calculateDesiredBotCount(settings,population)==0,"join after player waits for active human");
 settings.botJoinAfterPlayer=2; settings.botQuotaMode=QuotaMode::Normal;
 check(calculateDesiredBotCount(settings,population)==2,"join after spectator accepts connected human");
 settings.botJoinAfterPlayer=0; settings.botJoinDelay=10;
 check(calculateDesiredBotCount(settings,population)==0,"map join delay suppresses automatic bots");
 settings.botJoinDelay=0; settings.botQuotaMode=QuotaMode::Fill; population.lateJoinBlocked=true; population.managedBotClients=2;
 check(calculateDesiredBotCount(settings,population)==2,"late join rule holds current population");
 first.weaponPreferences[0]=22; first.weaponPreferenceCount=1;
 BuyController buyer; BuyContext context = {}; context.nowSeconds=3; context.generation=1; context.round=1; context.alive=true; context.inBuyZone=true; context.buyTimeOpen=true; context.team=ProfileTeam::CounterTerrorist; context.allowRifles=true; context.money=5000; context.weaponMaskKnown=true;
 BuyDecision buy=buyer.update(context,first);
 check(buy.status==BuyStatus::UnknownObservation,"unknown money prevents invented purchases");
 context.moneyKnown=true; buy=buyer.update(context,first);
 check(buy.status==BuyStatus::Command && buy.weaponId==22 && std::strcmp(buy.command,"m4a1")==0,"CT purchases affordable permitted primary");
 context.nowSeconds=3.1; check(buyer.update(context,first).status==BuyStatus::WaitingReceipt,"command not treated as purchase success");
 context.weaponMask |= (1u<<22); context.nowSeconds=3.2;
 check(buyer.update(context,first).status==BuyStatus::Complete,"observed inventory acknowledges purchase");
 context.generation=2; context.weaponMask=0; context.nowSeconds=4;
 check(buyer.update(context,first).status==BuyStatus::Command,"slot generation resets pending state");
 context.weaponMask=(1u<<22); context.nowSeconds=4.2; context.allowArmor=true;
 context.armorKnown=true; context.armor=0; context.helmetKnown=true; context.hasHelmet=false;
 buy=buyer.update(context,first);
 check(buy.status==BuyStatus::Command && std::strcmp(buy.command,"vesthelm")==0,"confirmed primary proceeds to armor");
 context.armor=100; context.hasHelmet=true; context.nowSeconds=4.4;
 check(buyer.update(context,first).status==BuyStatus::Complete,"armor receipt completes equipment stage");
 BuyController ammoBuyer; context.allowArmor=false; context.ammoKnown=true; context.needsPrimaryAmmo=true;
 buy=ammoBuyer.update(context,first);
 check(buy.status==BuyStatus::Command && std::strcmp(buy.command,"primammo")==0,"observed low ammunition purchases primary ammo");
 context.needsPrimaryAmmo=false; context.nowSeconds=4.6;
 check(ammoBuyer.update(context,first).status==BuyStatus::Complete,"ammunition receipt completes ammunition stage");
 BuyController equipped; BuyContext equippedContext = {}; equippedContext.nowSeconds=3; equippedContext.generation=1; equippedContext.round=1;
 equippedContext.alive=true; equippedContext.inBuyZone=true; equippedContext.buyTimeOpen=true; equippedContext.team=ProfileTeam::CounterTerrorist;
 equippedContext.moneyKnown=true; equippedContext.money=10000; equippedContext.weaponMaskKnown=true; equippedContext.weaponMask=1u<<22;
 equippedContext.allowDefuseKit=true; equippedContext.bombScenario=true; equippedContext.defuseKitKnown=true; equippedContext.allowGrenades=true; equippedContext.teamAllBots=true;
 ScriptedRandomSource equipmentRandom({
 RandomTapeEntry::floatEntry("buy.defuse_kit", {}, {}, 0,100,10),
 RandomTapeEntry::floatEntry("buy.grenade", {}, {}, 0,100,10),
 RandomTapeEntry::floatEntry("buy.grenade_type", {}, {}, 0,100,20)});
 equippedContext.random=&equipmentRandom;
 buy=equipped.update(equippedContext,first);
 check(buy.status==BuyStatus::Command && buy.weaponId==25,"all bot team may buy flash grenade from shared random");
 equippedContext.weaponMask |= 1u<<25; equippedContext.nowSeconds=3.3;
 buy=equipped.update(equippedContext,first);
 check(buy.status==BuyStatus::Command && std::strcmp(buy.command,"defuser")==0,"kit follows grenade receipt");
 equippedContext.hasDefuseKit=true; equippedContext.nowSeconds=3.6;
 check(equipped.update(equippedContext,first).status==BuyStatus::Complete && equipmentRandom.verifyComplete(),"full equipment cycle consumes exact chance sequence");
 ProfileRecord multi=first; std::memcpy(multi.name,"multi",6); multi.difficulty=4; multi.difficultyMask=3;
 ProfileCatalog masks; masks.add(multi);
 ScriptedRandomSource maskRandom({RandomTapeEntry::longEntry("profile.select_unused", {}, {}, 0,0,0)});
 check(masks.selectUnused(ProfileTeam::Any,1,nullptr,0,maskRandom,{}, {},&selected)==ProfileSelectionResult::Selected,"multi difficulty profile selected from allowed mask");
 BuyController recoveryBuyer; RecoveringRandom recovery; equippedContext.random=&recovery; equippedContext.hasDefuseKit=false; equippedContext.weaponMask=1u<<22;
 check(recoveryBuyer.update(equippedContext,first).status==BuyStatus::Unavailable,"unavailable entry draw is explicit");
 check(recoveryBuyer.update(equippedContext,first).status==BuyStatus::Complete && recovery.valid && recovery.calls==3,"retry resumes after completed random draws");
 BuyController failedReceipt; BuyContext refused=context; refused.allowArmor=true; refused.armor=0; refused.hasHelmet=false; refused.ammoKnown=false; refused.nowSeconds=10;
 check(failedReceipt.update(refused,first).status==BuyStatus::Command,"armor purchase request sent");
 refused.nowSeconds=10.3; failedReceipt.update(refused,first);
 refused.nowSeconds=10.6; failedReceipt.update(refused,first);
 refused.nowSeconds=10.9;
 check(failedReceipt.update(refused,first).status==BuyStatus::ReceiptTimeout,"unacknowledged purchases time out explicitly");
 refused.nowSeconds=11.2;
 check(failedReceipt.update(refused,first).status==BuyStatus::ReceiptTimeout,"timeout is not converted into success on next update");
 // Unknown equipment is a bounded request, never invented absence. A missed
 // armor receipt must not prevent a later defuse-kit attempt.
 BuyController unknownBuyer; BuyContext uncertain={};
 uncertain.generation=1; uncertain.round=1; uncertain.nowSeconds=3;
 uncertain.alive=true; uncertain.inBuyZone=true; uncertain.buyTimeOpen=true;
 uncertain.team=ProfileTeam::CounterTerrorist; uncertain.moneyKnown=true;
 uncertain.money=5000; uncertain.weaponMaskKnown=true; uncertain.weaponMask=1U<<22;
 uncertain.allowArmor=true; uncertain.armorKnown=true; uncertain.armor=100;
 uncertain.allowDefuseKit=true; uncertain.bombScenario=true;
 ScriptedRandomSource unknownRandom({RandomTapeEntry::floatEntry("buy.defuse_kit",{},{},0,100,10)});
 uncertain.random=&unknownRandom;
 buy=unknownBuyer.update(uncertain,first);
 check(buy.status==BuyStatus::Command && std::strcmp(buy.command,"vesthelm")==0,
  "unknown helmet permits one authoritative vesthelm request even with full armor");
 check(!uncertain.helmetKnown && !uncertain.defuseKitKnown,"unknown equipment knowledge stays unknown after request");
 uncertain.nowSeconds=3.1;
 check(unknownBuyer.update(uncertain,first).status==BuyStatus::WaitingReceipt,"unknown equipment waits for receipt");
 uncertain.nowSeconds=3.3;
 check(unknownBuyer.update(uncertain,first).status==BuyStatus::ReceiptTimeout,"unknown armor reports bounded unconfirmed attempt");
 uncertain.nowSeconds=3.4; buy=unknownBuyer.update(uncertain,first);
 check(buy.status==BuyStatus::Command && std::strcmp(buy.command,"defuser")==0,
  "unconfirmed armor does not block subsequent unknown defuse-kit request");
 uncertain.defuseKitKnown=true; uncertain.hasDefuseKit=true; uncertain.nowSeconds=3.5;
 check(unknownBuyer.update(uncertain,first).status==BuyStatus::Complete && unknownRandom.verifyComplete(),
  "observed kit receipt completes after unconfirmed armor without extra random draws");
 BuyController unconfirmedKit; uncertain.allowArmor=false; uncertain.defuseKitKnown=false;
 uncertain.hasDefuseKit=false; uncertain.nowSeconds=10;
 ScriptedRandomSource kitRandom({RandomTapeEntry::floatEntry("buy.defuse_kit",{},{},0,100,10)});
 uncertain.random=&kitRandom; buy=unconfirmedKit.update(uncertain,first);
 check(buy.status==BuyStatus::Command && std::strcmp(buy.command,"defuser")==0,"unknown kit requests purchase once");
 uncertain.nowSeconds=10.3;
 check(unconfirmedKit.update(uncertain,first).status==BuyStatus::ReceiptTimeout,"unconfirmed kit reports timeout");
 uncertain.nowSeconds=10.6;
 check(unconfirmedKit.update(uncertain,first).status==BuyStatus::Complete && !uncertain.defuseKitKnown,
  "unconfirmed kit stage is skipped without claiming ownership or retrying indefinitely");
 BuyController observedArmor; uncertain.allowArmor=true; uncertain.armorKnown=false;
 uncertain.helmetKnown=false; uncertain.allowDefuseKit=false; uncertain.random=nullptr; uncertain.nowSeconds=20;
 buy=observedArmor.update(uncertain,first);
 check(buy.status==BuyStatus::Command && std::strcmp(buy.command,"vesthelm")==0,"unknown armor amount permits one purchase request");
 uncertain.armorKnown=true; uncertain.armor=100; uncertain.helmetKnown=true; uncertain.hasHelmet=true; uncertain.nowSeconds=20.1;
 check(observedArmor.update(uncertain,first).status==BuyStatus::Complete,"armor and helmet receipts confirm unknown-start purchase");
 return failures==0?0:1;
}
