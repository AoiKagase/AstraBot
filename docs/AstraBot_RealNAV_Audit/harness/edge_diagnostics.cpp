// NAV-only read-only probe. Positions below are diagnostic inputs, not recorded live feet.
#include "astrabot/nav/legacy_nav_reader.hpp"
#include "astrabot/runtime/nav_roam_controller.hpp"
#include <fstream>
#include <iterator>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <vector>
#include <stdexcept>
using namespace astrabot;
static float h(const nav::NavArea &a,float x,float y){
 float u=(x-a.extent.lo.x)/(a.extent.hi.x-a.extent.lo.x),v=(y-a.extent.lo.y)/(a.extent.hi.y-a.extent.lo.y);
 u=std::max(0.0f,std::min(1.0f,u));v=std::max(0.0f,std::min(1.0f,v));
 return (1-u)*(1-v)*a.extent.lo.z+u*(1-v)*a.northEastZ+(1-u)*v*a.southWestZ+u*v*a.extent.hi.z;
}
static nav::NavVector center(const nav::NavArea &a){float x=(a.extent.lo.x+a.extent.hi.x)/2,y=(a.extent.lo.y+a.extent.hi.y)/2;return{x,y,h(a,x,y)};}
static runtime::NavRoamObservation observation(nav::NavVector p,nav::NavVector goal,unsigned tick){
 runtime::NavRoamObservation o={};o.actor={1,1};o.frame={1,1,tick};o.locomotion.position=p;o.locomotion.standingClearance=72;o.locomotion.crouchingClearance=36;o.locomotion.grounded=true;o.hasObjectiveTarget=true;o.objectiveTarget=goal;return o;
}
int main(int argc,char **argv){
 try{
 if(argc!=3)throw std::runtime_error("usage: edge_diagnostics NAV OUTPUT_CSV");
 std::ifstream f(argv[1],std::ios::binary);if(!f)throw std::runtime_error("NAV open");std::vector<std::uint8_t>bytes((std::istreambuf_iterator<char>(f)),{});nav::LegacyNavReader rd;nav::NavDocument d;
 if(rd.read(bytes.data(),bytes.size(),&d)!=nav::NavReadResult::Loaded)throw std::runtime_error("NAV parse");nav::NavSnapshotPublisher p;p.publish(&d,1);auto s=p.snapshot();nav::NavQuery q(s);
 std::ofstream csv(argv[2]);csv.precision(9);csv<<"from,to,dir,how,fromattr,toattr,center_current,follow_target,locomotion,action,average_delta,shared_xy,shared_mid_height_delta,from_width,from_height,to_width,to_height\n";
 unsigned steps=0,jumpsRejected=0,edges=0;
 for(auto&a:d.areas()){
  std::vector<nav::NavDirectedLink> ls;q.outgoingLinks(a.id,&ls);
  for(auto &l:ls){auto &b=*d.findArea(l.toArea);nav::NavCorridor c={};c.navRevision=s.revision();c.mapGeneration=s.mapGeneration();c.areas={a.id,b.id};c.links={l};nav::LocomotionController lc({32,20,64,16,100,8});lc.start(c);nav::LocomotionIntent in={};auto ob=observation(center(a),center(b),1);auto lr=lc.update(s,ob.locomotion,&in);nav::NavAreaMatch m={};q.findContaining(ob.locomotion.position,64,&m);nav::NavPathFollower ff;ff.start(c);nav::NavVector t={};nav::AreaId tid=0;ff.update(s,ob.locomotion.position,20,64,&t,&tid);
  float x0=std::max(a.extent.lo.x,b.extent.lo.x),x1=std::min(a.extent.hi.x,b.extent.hi.x),y0=std::max(a.extent.lo.y,b.extent.lo.y),y1=std::min(a.extent.hi.y,b.extent.hi.y);
  bool shared=x0<=x1&&y0<=y1;float midDelta=shared?h(b,(x0+x1)/2,(y0+y1)/2)-h(a,(x0+x1)/2,(y0+y1)/2):99999;
  float ad=(b.northEastZ+b.southWestZ-a.northEastZ-a.southWestZ)/2;
  csv<<a.id<<','<<b.id<<','<<int(l.direction)<<','<<int(l.how)<<','<<int(a.attributes)<<','<<int(b.attributes)<<','<<m.area<<','<<tid<<','<<int(lr)<<','<<int(in.traversal)<<','<<ad<<','<<shared<<','<<midDelta<<','<<a.extent.hi.x-a.extent.lo.x<<','<<a.extent.hi.y-a.extent.lo.y<<','<<b.extent.hi.x-b.extent.lo.x<<','<<b.extent.hi.y-b.extent.lo.y<<'\n';
  ++edges;if(lr==nav::LocomotionResult::StepTooHigh){++steps;if(b.attributes&2)++jumpsRejected;}
 }
 }
 printf("EDGE_SCAN sampled=%u StepTooHigh=%u with_destination_jump=%u (center inputs; not live traversability verdict)\n",edges,steps,jumpsRejected);
 // A gap on an actual descending link: retain controller state across imposed observations.
 runtime::NavRoamController ctl;auto goal=center(*d.findArea(54));unsigned tick=0;
 for(auto pt:std::vector<nav::NavVector>{{-1575,-175,128.03125f},{-1575,-151,128.03125f},{-1575,-149,128.03125f},{-1575,-140,128.03125f},{-1575,-136,128.03125f},{-1575,-124,128.03125f},{-1575,-124,85.0f},{-1575,-124,60.0f}}){
  nav::NavAreaMatch cm={},nm={};auto cr=q.findContaining(pt,64,&cm),nr=q.findNearest(pt,10000,&nm);runtime::NavRoamDecision dc={};nav::LocomotionIntent in={};auto ob=observation(pt,goal,++tick);ob.airborne=tick>=3;ob.locomotion.grounded=!ob.airborne;auto rr=ctl.update(s,ob,&in,&dc);
  printf("GAP feet=(%.3f,%.3f,%.3f) containing_result=%d containing=%u nearest=%u nearestXYZ=(%.3f,%.3f,%.3f) runtime=%d stage=%d decision_current=%u recovery=%u intent_target=%u dir=(%.6f,%.6f,%.6f) speed=%.1f action=%d\n",pt.x,pt.y,pt.z,int(cr),cm.area,nm.area,nm.closestPoint.x,nm.closestPoint.y,nm.closestPoint.z,int(rr),int(dc.stage),dc.currentArea,dc.recoveryArea,in.targetArea,in.direction.x,in.direction.y,in.direction.z,in.speed,int(in.traversal));
 }
 for(auto pair:std::vector<std::pair<unsigned,unsigned>>{{1778,619},{1779,619},{619,93},{12,1784},{8,1094},{18,607},{46,119},{119,46}}){
  auto a=d.findArea(pair.first),b=d.findArea(pair.second);nav::NavCorridor c={};auto qr=q.buildCorridor(a->id,b->id,&c);nav::LocomotionController lc({32,20,64,16,100,8});lc.start(c);auto ob=observation(center(*a),center(*b),1);nav::LocomotionIntent in={};auto lr=lc.update(s,ob.locomotion,&in);
  printf("TARGET_EDGE %u->%u query=%d pathlen=%zu loco=%d target=%u action=%d destattr=%u average_delta=%.6f\n",a->id,b->id,int(qr),c.areas.size(),int(lr),in.targetArea,int(in.traversal),b->attributes,(b->northEastZ+b->southWestZ-a->northEastZ-a->southWestZ)/2);
 }
 for(auto pt:std::vector<nav::NavVector>{{-1187.5f,1250,-32},{512.5f,2075,-126.3433f},{325,1810,96.03125f}}){
  nav::NavAreaMatch cm={},nm={};auto cr=q.findContaining(pt,64,&cm),nr=q.findNearest(pt,10000,&nm);runtime::NavRoamController ct;runtime::NavRoamDecision dc={};nav::LocomotionIntent in={};auto ob=observation(pt,center(*d.findArea(1653)),1);auto rr=ct.update(s,ob,&in,&dc);
  printf("RECOVERY feet=(%.3f,%.3f,%.3f) containing_result=%d containing=%u nearest=%u nearestXYZ=(%.3f,%.3f,%.3f) runtime=%d stage=%d recovery=%u intent_current=%u dir=(%.6f,%.6f,%.6f)\n",pt.x,pt.y,pt.z,int(cr),cm.area,nm.area,nm.closestPoint.x,nm.closestPoint.y,nm.closestPoint.z,int(rr),int(dc.stage),dc.recoveryArea,in.currentArea,in.direction.x,in.direction.y,in.direction.z);
 }
 return 0;
 }catch(const std::exception&e){fprintf(stderr,"ERROR %s\n",e.what());return 2;}
}
