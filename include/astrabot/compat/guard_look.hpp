#ifndef ASTRABOT_COMPAT_GUARD_LOOK_HPP
#define ASTRABOT_COMPAT_GUARD_LOOK_HPP
#include "astrabot/compat/random_source.hpp"
#include "astrabot/nav/nav_query.hpp"
#include "astrabot/world/world_snapshot.hpp"
#include <algorithm>
#include <cmath>
#include <vector>
namespace astrabot { namespace compat {
inline nav::NavVector guardApproachPoint(const nav::NavArea &previous, const nav::NavArea &here, unsigned how)
{
	nav::NavVector point={(here.extent.lo.x+here.extent.hi.x)*.5f,(here.extent.lo.y+here.extent.hi.y)*.5f,0};
	const auto clamp=[](float value,float lo,float hi){return std::max(lo,std::min(hi,value));};
	if(how==0 || how==2)
	{
		point.y=how==0 ? previous.extent.lo.y : previous.extent.hi.y;
		point.x=.5f*(clamp(std::max(previous.extent.lo.x,here.extent.lo.x),previous.extent.lo.x,previous.extent.hi.x)+
			clamp(std::min(previous.extent.hi.x,here.extent.hi.x),previous.extent.lo.x,previous.extent.hi.x));
	}
	else if(how==1 || how==3)
	{
		point.x=how==3 ? previous.extent.lo.x : previous.extent.hi.x;
		point.y=.5f*(clamp(std::max(previous.extent.lo.y,here.extent.lo.y),previous.extent.lo.y,previous.extent.hi.y)+
			clamp(std::min(previous.extent.hi.y,here.extent.hi.y),previous.extent.lo.y,previous.extent.hi.y));
	}
	point.z=nav::surfaceZAt(here,point.x,point.y);return point;
}
// Private behavior state: geometry is supplied by the adapter, input by its owner.
class GuardLookPlanner
{
public:
	void reset() { *this = GuardLookPlanner{}; }
	bool current(world::ActorKey actor, world::FrameIdentity frame, world::EntityKey bomb,
		const nav::NavVector &spot, std::uint64_t revision, float now) const
	{
		return active_ && actor == actor_ && frame.mapGeneration == frame_.mapGeneration &&
			frame.roundGeneration == frame_.roundGeneration && bomb == bomb_ && revision == revision_ &&
			std::hypot(spot.x-spot_.x,spot.y-spot_.y)<1 && std::fabs(spot.z-spot_.z)<1 && now>=lastTime_;
	}
	void begin(world::ActorKey actor, world::FrameIdentity frame, world::EntityKey bomb,
		const nav::NavVector &spot, std::uint64_t revision, float now, float openYaw,
		const std::vector<nav::NavVector> &approaches)
	{
		reset();active_=true;actor_=actor;frame_=frame;bomb_=bomb;spot_=spot;revision_=revision;
		lastTime_=now;nextTime_=now;desiredYaw_=openYaw;approaches_=approaches;
	}
	bool update(float now, bool sniper, const nav::NavVector &eye, float currentPitch, float currentYaw,
		ICompatibilityRandomSource &random, RandomTimingContext timing, float *pitch, float *yaw)
	{
		if(!active_ || !std::isfinite(now) || now<lastTime_ || !pitch || !yaw)return false;
		if(now>=nextTime_)
		{
			const auto interval=random.nextFloat(RandomRequest::floatRequest("LookAround.StateInterval",
				{actor_.slot,actor_.generation},timing,sniper?5.0f:1.0f,sniper?10.0f:2.0f));
			if(interval.status!=RandomStatus::Ok || !std::isfinite(interval.value) ||
				interval.value<(sniper?5.0f:1.0f) || interval.value>(sniper?10.0f:2.0f))return false;
			nextTime_=now+interval.value;
			if(!approaches_.empty())
			{
				const auto which=random.nextLong(RandomRequest::longRequest("LookAround.ApproachPoint",
					{actor_.slot,actor_.generation},timing,0,static_cast<std::int32_t>(approaches_.size()-1)));
				if(which.status!=RandomStatus::Ok || which.value<0 || static_cast<std::size_t>(which.value)>=approaches_.size())return false;
				const auto &p=approaches_[which.value];const float dx=p.x-eye.x,dy=p.y-eye.y;
				const float range=std::hypot(dx,dy);
				if(range>=1){desiredYaw_=std::atan2(dy,dx)*57.295779513f;desiredPitch_=-std::atan2(p.z+36-eye.z,range)*57.295779513f;}
			}
		}
		const float dt=std::max(0.0f,std::min(.1f,now-lastTime_));lastTime_=now;
		const float step=240.0f*dt;
		*pitch=currentPitch+std::max(-step,std::min(step,normalize(desiredPitch_-currentPitch)));
		*yaw=normalize(currentYaw+std::max(-step,std::min(step,normalize(desiredYaw_-currentYaw))));
		return true;
	}
private:
	static float normalize(float value){return std::remainder(value,360.0f);}
	bool active_=false;
	world::ActorKey actor_{};world::FrameIdentity frame_{};world::EntityKey bomb_{};
	nav::NavVector spot_{};std::uint64_t revision_=0;
	float nextTime_=0,lastTime_=0,desiredYaw_=0,desiredPitch_=0;
	std::vector<nav::NavVector> approaches_;
};
} }
#endif
