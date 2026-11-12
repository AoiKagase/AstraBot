// Regression proposals. They run unmodified product classes; no BSP/physics claims.
#include "astrabot/nav/legacy_nav_reader.hpp"
#include "astrabot/runtime/nav_roam_controller.hpp"
#include "action_adapter.hpp"
#include <fstream>
#include <iterator>
#include <string>
#include <stdexcept>
#include <cstdio>
#include <cmath>
#include <utility>
#include <vector>
using namespace astrabot;
static nav::NavVector center(const nav::NavArea &a)
{
 return {(a.extent.lo.x+a.extent.hi.x)*0.5f,(a.extent.lo.y+a.extent.hi.y)*0.5f,
 (a.extent.lo.z+a.northEastZ+a.southWestZ+a.extent.hi.z)*0.25f};
}
static runtime::NavRoamObservation observe(nav::NavVector pos,nav::NavVector goal,unsigned frame=1)
{
 runtime::NavRoamObservation o={};o.actor={1,1};o.frame={1,1,frame};
	o.locomotion.position=pos;o.locomotion.standingClearance=72;o.locomotion.crouchingClearance=36;o.locomotion.grounded=true;
	o.locomotion.safeDropHeightAvailable=true;
	o.locomotion.maximumSafeDropHeight=(500.0f*500.0f)/(2.0f*800.0f);
 o.hasObjectiveTarget=true;o.objectiveTarget=goal;return o;
}
static int expect(bool condition,const char *message)
{
 std::printf("%s: %s\n",condition?"PASS":"FAIL",message);return condition?0:1;
}
int main(int argc,char **argv)
{
 try {
 if(argc!=3)throw std::runtime_error("usage: real_nav_regressions NAV CASE");
 std::ifstream f(argv[1],std::ios::binary);if(!f)throw std::runtime_error("NAV open");std::vector<std::uint8_t>b((std::istreambuf_iterator<char>(f)),{});
 nav::LegacyNavReader rd;nav::NavDocument d;
 if(rd.read(b.data(),b.size(),&d)!=nav::NavReadResult::Loaded)throw std::runtime_error("NAV parse");
 nav::NavSnapshotPublisher pub;if(pub.publish(&d,1)!=nav::NavSnapshotResult::Published)throw std::runtime_error("publication");
 auto s=pub.snapshot();nav::NavQuery q(s);std::string name=argv[2];
 if(name=="baseline") {
  int errors=0;
  for(auto pair:std::vector<std::pair<unsigned,unsigned>>{{41,54},{41,90},{41,1438},{5,1520},{46,119}}){
   nav::NavCorridor c={};nav::NavSearchStats st={};auto r=q.buildCorridor(pair.first,pair.second,nav::NavRouteType::Fastest,&c,&st);
   printf("route=%u->%u result=%d areas=%zu expanded=%u enqueues=%u\n",pair.first,pair.second,int(r),c.areas.size(),st.expandedUniqueAreas,st.enqueueCount);
   errors+=expect(r==nav::NavQueryResult::Found&&!c.areas.empty()&&c.areas.front()==pair.first&&c.areas.back()==pair.second,"raw A* reaches requested goal");
  }
  nav::NavQuery cap(s,{256,256});nav::NavCorridor c={};auto r=cap.buildCorridor(41,90,&c);
  errors+=expect(r==nav::NavQueryResult::ResourceLimit,"explicit diagnostic 256 budget remains a limit");return errors?1:0;
 }
 if(name=="slope_entry"||name=="slope_exit"||name=="jump_gate") {
  unsigned from=name=="slope_entry"?1778:name=="slope_exit"?619:275;
  unsigned to=name=="slope_exit"?93:name=="jump_gate"?642:619;
  nav::NavCorridor c={};if(q.buildCorridor(from,to,&c)!=nav::NavQueryResult::Found||c.areas.size()!=2)throw std::runtime_error("expected direct real NAV edge");
  nav::LocomotionController lc({32,20,64,16,100,8});lc.start(c);auto o=observe(center(*d.findArea(from)),center(*d.findArea(to)));nav::LocomotionIntent i={};auto r=lc.update(s,o.locomotion,&i);
  printf("edge=%u->%u loco=%d targetAttr=%u action=%d feet=(%.6f,%.6f,%.6f)\n",from,to,int(r),d.findArea(to)->attributes,int(i.traversal),o.locomotion.position.x,o.locomotion.position.y,o.locomotion.position.z);
  if(name=="jump_gate")return expect(r!=nav::LocomotionResult::StepTooHigh,"Jump-marked transition is not rejected solely by the ordinary step gate; not a physical jump-success assertion");
  return expect(r==nav::LocomotionResult::IntentReady,"continuous real NAV slope edge must not be rejected as an isolated step");
 }
 if(name=="gap_policy") {
  runtime::NavRoamController objectiveController;runtime::NavRoamDecision objectiveDecision={};nav::LocomotionIntent objectiveIntent={};
  auto objectiveObservation=observe(center(*d.findArea(41)),center(*d.findArea(54)));
  auto objectiveResult=objectiveController.update(s,objectiveObservation,&objectiveIntent,&objectiveDecision);
  if(objectiveResult!=runtime::NavRoamResult::IntentReady||objectiveDecision.goalArea!=54||objectiveDecision.currentArea!=41||objectiveDecision.pathResult!=nav::NavQueryResult::Found)throw std::runtime_error("C4 objective route from area41 to site area54 did not start");
  runtime::NavRoamController siteController;runtime::NavRoamDecision siteDecision={};nav::LocomotionIntent siteIntent={};
  auto siteObservation=observe(center(*d.findArea(41)),center(*d.findArea(54)));
  auto siteResult=siteController.update(s,siteObservation,&siteIntent,&siteDecision);
  if(siteResult!=runtime::NavRoamResult::IntentReady||siteDecision.goalArea!=54||siteDecision.currentArea!=41||siteDecision.pathResult!=nav::NavQueryResult::Found){std::fprintf(stderr,"C4 objective path result=%d path=%d goal=%u current=%u failure=%d\n",int(siteResult),int(siteDecision.pathResult),siteDecision.goalArea,siteDecision.currentArea,int(siteDecision.failureReason));throw std::runtime_error("C4 objective corridor failed from area41 to BombTarget area54");}
  runtime::NavRoamController rc;runtime::NavRoamDecision dc={};nav::LocomotionIntent i={};
  const nav::NavArea *from=d.findArea(46),*to=d.findArea(119);
  if(from==nullptr||to==nullptr)throw std::runtime_error("gap fixture areas missing");
  std::fprintf(stderr,"areas from=%u bounds=(%.1f,%.1f)-(%.1f,%.1f) z=(%.1f,%.1f) to=%u bounds=(%.1f,%.1f)-(%.1f,%.1f) z=(%.1f,%.1f)\\n",from->id,from->extent.lo.x,from->extent.lo.y,from->extent.hi.x,from->extent.hi.y,from->southWestZ,from->northEastZ,to->id,to->extent.lo.x,to->extent.lo.y,to->extent.hi.x,to->extent.hi.y,to->southWestZ,to->northEastZ);
  nav::NavCorridor siteRoute={};
  if(q.buildCorridor(46,54,&siteRoute)!=nav::NavQueryResult::Found||siteRoute.links.empty())throw std::runtime_error("site route missing gap link");
  nav::NavCorridor alternateSiteRoute={};nav::NavSearchStats alternateStats={};auto alternateResult=q.buildAlternativeCorridor(siteRoute,siteRoute.links.front(),&alternateSiteRoute,&alternateStats);
  std::fprintf(stderr,"site route first=%u->%u/how%u total=%zu altResult=%d altFirst=%u->%u areas=%zu searches=%u failed=%u\\n",siteRoute.links.front().fromArea,siteRoute.links.front().toArea,siteRoute.links.front().how,siteRoute.areas.size(),int(alternateResult),alternateSiteRoute.links.empty()?0U:alternateSiteRoute.links.front().fromArea,alternateSiteRoute.links.empty()?0U:alternateSiteRoute.links.front().toArea,alternateSiteRoute.areas.size(),alternateStats.searchCalls,alternateStats.failureCount);
  const nav::NavDirectedLink link=siteRoute.links.front();
  const std::uint8_t reverse=link.direction<nav::NavArea::kDirectionCount?static_cast<std::uint8_t>((link.direction+2U)%nav::NavArea::kDirectionCount):nav::NavArea::kDirectionCount;
  const nav::NavVector startPosition={-1575,-151,128.03125f};
  const nav::NavVector landingPortal=nav::portalSteeringPointForLink(*from,*to,startPosition,link.direction);
  const nav::NavVector launchPortal=nav::portalSteeringPointForLink(*to,*from,landingPortal,reverse);
  auto o=observe(startPosition,center(*d.findArea(54)));
  auto r=rc.update(s,o,&i,&dc);
  if(r!=runtime::NavRoamResult::IntentReady||i.targetArea!=119||i.traversal==nav::TraversalAction::Jump){std::fprintf(stderr,"gap approach result=%d target=%u traversal=%d current=%u goal=%u stage=%d failure=%d locomotion=%d link=%u->%u dir=%u how=%u launch=(%.1f,%.1f,%.1f) pos=(%.1f,%.1f,%.1f)\n",int(r),i.targetArea,int(i.traversal),dc.currentArea,dc.goalArea,int(dc.stage),int(dc.failureReason),int(dc.locomotionResult),dc.linkFromArea,dc.linkToArea,dc.linkDirection,dc.linkHow,launchPortal.x,launchPortal.y,launchPortal.z,o.locomotion.position.x,o.locomotion.position.y,o.locomotion.position.z);throw std::runtime_error("safe gap must approach or drop without Jump");}
  o.frame.tick=2;o.locomotion.position=launchPortal;
  r=rc.update(s,o,&i,&dc);
  if(r!=runtime::NavRoamResult::IntentReady||i.targetArea!=119||i.traversal==nav::TraversalAction::Jump)throw std::runtime_error("safe gap movement must not press Jump");
  o.frame.tick=3;o.locomotion.position=landingPortal;o.locomotion.grounded=true;o.airborne=false;o.landingConfirmed=true;o.hasLandingDamage=true;o.landingDamage=0.0f;
  r=rc.update(s,o,&i,&dc);
  if(r!=runtime::NavRoamResult::ReplanRequired||dc.locomotionResult!=nav::LocomotionResult::TargetReached||dc.currentArea!=119)throw std::runtime_error("safe gap landing was not confirmed in the destination area");
  o.frame.tick=4;o.landingConfirmed=false;o.hasLandingDamage=false;
  r=rc.update(s,o,&i,&dc);
  if(r!=runtime::NavRoamResult::IntentReady||dc.currentArea!=119)throw std::runtime_error("corridor did not continue from the landed area");
  return 0;
 }
 if(name=="recovery_projection") {
  runtime::NavRoamController rc;runtime::NavRoamDecision dc={};nav::LocomotionIntent i={};auto o=observe({-1187.5f,1250,-32},center(*d.findArea(1653)));
  auto r=rc.update(s,o,&i,&dc);auto m=metamod::ActionAdapter::projectMovement(i.direction.x,i.direction.y,i.speed,0);
  printf("result=%d stage=%d recovery=%u dir=(%.6f,%.6f,%.6f) projectedForward=%.6f projectedSide=%.6f\n",int(r),int(dc.stage),dc.recoveryArea,i.direction.x,i.direction.y,i.direction.z,m.forward,m.side);
  return expect(!(r==runtime::NavRoamResult::IntentReady&&dc.stage==runtime::NavRoamStage::OffMeshRecovery&&i.traversal==nav::TraversalAction::Walk&&std::fabs(i.direction.z)>0.9f&&m.forward==0&&m.side==0),"vertical-only off-mesh recovery must not report actionable walking progress when its execution projection is zero");
 }
 if(name=="goal_floor") {
  runtime::NavRoamController rc;runtime::NavRoamDecision dc={};nav::LocomotionIntent i={};auto o=observe(center(*d.findArea(5)),center(*d.findArea(1520)));
  auto r=rc.update(s,o,&i,&dc);nav::NavAreaMatch cm={},nm={};q.findContaining(o.objectiveTarget,64,&cm);q.findNearest(o.objectiveTarget,10000,&nm);
  printf("requested=(%.6f,%.6f,%.6f) containing=%u nearest=%u runtime=%d selectedPathEnd=%u\n",o.objectiveTarget.x,o.objectiveTarget.y,o.objectiveTarget.z,cm.area,nm.area,int(r),dc.selectedPath.empty()?0:dc.selectedPath.back());
  return expect(r==runtime::NavRoamResult::IntentReady&&!dc.selectedPath.empty()&&dc.selectedPath.back()==1520,"a runtime 3D goal on area1520 must not be silently reassigned to area351 above it");
 }
 throw std::runtime_error("unknown case");
 }catch(const std::exception&e){fprintf(stderr,"ERROR: %s\n",e.what());return 2;}
}
