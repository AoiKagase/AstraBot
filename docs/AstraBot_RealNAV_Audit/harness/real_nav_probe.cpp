// Diagnostic only: unmodified production reader/query/controller, no engine physics.
#include "astrabot/nav/legacy_nav_reader.hpp"
#include "astrabot/runtime/nav_roam_controller.hpp"
#include <fstream>
#include <iterator>
#include <cstdio>
#include <stdexcept>
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include <cmath>
using namespace astrabot;
static nav::NavVector center(const nav::NavArea &a)
{
 return {(a.extent.lo.x+a.extent.hi.x)/2,(a.extent.lo.y+a.extent.hi.y)/2,(a.extent.lo.z+a.northEastZ+a.southWestZ+a.extent.hi.z)/4};
}
static runtime::NavRoamObservation obs(nav::NavVector p, nav::NavVector goal)
{
 runtime::NavRoamObservation o={};o.actor={1,1};o.frame={1,1,1};
 o.locomotion.position=p;o.locomotion.standingClearance=72;o.locomotion.crouchingClearance=36;o.locomotion.grounded=true;
 o.hasObjectiveTarget=true;o.objectiveTarget=goal;o.collectPathStats=true;return o;
}
int main(int argc,char **argv)
{
 try {
 if(argc!=3) throw std::runtime_error("usage: real_nav_probe NAV OUTPUT_JSON");
 std::ifstream f(argv[1],std::ios::binary);if(!f)throw std::runtime_error("NAV open");
 std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(f)),{});
 nav::LegacyNavReader reader;nav::NavDocument d;auto result=reader.read(bytes.data(),bytes.size(),&d);
 std::printf("READER result=%d bytes=%zu areas=%zu places=%zu version=%u bspSize=%u contentHash=%u\n",int(result),bytes.size(),d.areaCount(),d.placeCount(),d.sourceIdentity().sourceVersion,d.sourceIdentity().bspSize,d.sourceIdentity().contentHash);
 if(result!=nav::NavReadResult::Loaded) return 2;
 nav::NavSnapshotPublisher pub;if(pub.publish(&d,1)!=nav::NavSnapshotResult::Published)return 3;
 auto s=pub.snapshot();nav::NavQuery q(s);std::ofstream j(argv[2]);j.precision(9);j<<"{\"areas\":[";bool first=true;
 std::map<unsigned,unsigned> attrs,hows;unsigned edges=0;
 for(auto &a:d.areas()){
  ++attrs[a.attributes];if(!first)j<<',';first=false;
  j<<"{\"id\":"<<a.id<<",\"attributes\":"<<unsigned(a.attributes)<<",\"lo\":["<<a.extent.lo.x<<','<<a.extent.lo.y<<','<<a.extent.lo.z<<"],\"hi\":["<<a.extent.hi.x<<','<<a.extent.hi.y<<','<<a.extent.hi.z<<"],\"ne\":"<<a.northEastZ<<",\"sw\":"<<a.southWestZ<<",\"place\":"<<a.placeId<<",\"links\":[";
  std::vector<nav::NavDirectedLink> ls;q.outgoingLinks(a.id,&ls);bool fl=true;
  for(auto &l:ls){++edges;++hows[l.how];if(!fl)j<<',';fl=false;j<<"["<<l.toArea<<','<<unsigned(l.direction)<<','<<unsigned(l.how)<<"]";}
  j<<"]}";
 }
 j<<"],\"places\":[";first=true;for(auto &p:d.places()){if(!first)j<<',';first=false;j<<"{\"id\":"<<p.id<<",\"name\":\""<<p.name<<"\"}";}j<<"]}\n";
 std::printf("GRAPH edges=%u attrs",edges);for(auto &v:attrs)std::printf(" %u:%u",v.first,v.second);std::printf(" hows");for(auto &v:hows)std::printf(" %u:%u",v.first,v.second);std::puts("");
 for(auto pair:std::vector<std::pair<unsigned,unsigned>>{{41,90},{41,1438},{5,1520},{41,54},{46,119},{46,54},{1505,1653},{1505,286},{619,1445},{120,320},{320,210}}){
  nav::NavCorridor c;nav::NavSearchStats st={};auto r=q.buildCorridor(pair.first,pair.second,nav::NavRouteType::Fastest,&c,&st);
  std::printf("ROUTE %u->%u result=%d length=%zu cost=%.3f expanded=%u enqueues=%u usec=%llu areas=",pair.first,pair.second,int(r),c.areas.size(),c.cost,st.expandedUniqueAreas,st.enqueueCount,(unsigned long long)st.totalUsec);
  for(auto id:c.areas)std::printf("%u,",id);std::puts("");
  if(r!=nav::NavQueryResult::Found) continue;
  auto o=obs(center(*d.findArea(pair.first)),center(*d.findArea(pair.second)));
  nav::LocomotionController loc({32,20,64,16,100,8});loc.start(c);nav::NavPathFollower fol;fol.start(c);runtime::NavRoamController rc;
  for(int it=0;it<3;++it){nav::LocomotionIntent i={};nav::NavVector p={};nav::AreaId ta=0;auto fr=fol.update(s,o.locomotion.position,20,64,&p,&ta);auto lr=loc.update(s,o.locomotion,&i);
   std::printf(" HOLD %d feet=(%.3f,%.3f,%.3f) follow=%d idx=%zu target=%u local=(%.3f,%.3f,%.3f) locomotion=%d intentArea=%u action=%d dir=(%.3f,%.3f,%.3f)\n",it,o.locomotion.position.x,o.locomotion.position.y,o.locomotion.position.z,int(fr),fol.currentIndex(),ta,p.x,p.y,p.z,int(lr),i.targetArea,int(i.traversal),i.direction.x,i.direction.y,i.direction.z);
  }
  runtime::NavRoamDecision dc={};nav::LocomotionIntent i={};auto rr=rc.update(s,o,&i,&dc);
  std::printf(" RUNTIME result=%d current=%u goal=%u locomotion=%d actualIntentArea=%u action=%d path=",int(rr),dc.currentArea,dc.goalArea,int(dc.locomotionResult),i.targetArea,int(i.traversal));for(auto id:dc.selectedPath)std::printf("%u,",id);std::puts("");
 }
 nav::NavCorridor c;nav::NavSearchStats st={};nav::NavQuery limited(s,{256,256});auto r=limited.buildCorridor(41,90,nav::NavRouteType::Fastest,&c,&st);std::printf("EXPLICIT_BUDGET_256 result=%d expanded=%u enqueues=%u\n",int(r),st.expandedUniqueAreas,st.enqueueCount);
 return 0;
 }catch(const std::exception &e){std::fprintf(stderr,"ERROR %s\n",e.what());return 4;}
}
