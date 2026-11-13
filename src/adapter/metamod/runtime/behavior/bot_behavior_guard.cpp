#include "runtime/behavior/bot_behavior_runtime.hpp"
#include "runtime/shared/bot_runtime_support.hpp"
#include <algorithm>
#include <cmath>
namespace astrabot { namespace metamod {
namespace {
bool clearRay(enginefuncs_t &engine, edict_t *self, const nav::NavVector &from,
	const nav::NavVector &to, TraceResult *result)
{
	const float a[]={from.x,from.y,from.z},b[]={to.x,to.y,to.z};
	*result={};engine.pfnTraceLine(a,b,1,self,result);
	return result->flFraction>=1 && !result->fStartSolid && !result->fAllSolid;
}
bool bendApproach(enginefuncs_t &engine,edict_t *self,const nav::NavVector &eye,
	const nav::NavVector &point,nav::NavVector *bend)
{
	const nav::NavVector chest={point.x,point.y,point.z+36};TraceResult trace{};
	if(clearRay(engine,self,eye,chest,&trace)){*bend=point;return true;}
	const float length=std::hypot(point.x-eye.x,point.y-eye.y);
	if(!std::isfinite(length) || length<1 || length>8192)return false;
	const float start=std::atan2(point.y-eye.y,point.x-eye.x);
	for(int angle=10;angle<=135;angle+=10)for(int side=0;side<2;++side)
	{
		const float yaw=start+(side?angle:-angle)*.01745329252f;
		const nav::NavVector rotated={eye.x+length*std::cos(yaw),eye.y+length*std::sin(yaw),point.z};
		const nav::NavVector rotatedChest={rotated.x,rotated.y,rotated.z+36};
		clearRay(engine,self,eye,rotatedChest,&trace);
		if(trace.fStartSolid || trace.fAllSolid)continue;
		const float dz=rotated.z-eye.z,rayLength=std::sqrt(length*length+dz*dz);
		const float visible=rayLength*std::max(0.0f,std::min(1.0f,trace.flFraction));
		for(float distance=50;distance<=visible;distance+=50)
		{
			const float t=distance/rayLength;
			nav::NavVector ray={eye.x+t*(rotated.x-eye.x),eye.y+t*(rotated.y-eye.y),eye.z+t*dz};
			if(!clearRay(engine,self,ray,chest,&trace))continue;
			TraceResult ground{};
			if(!clearRay(engine,self,{ray.x,ray.y,ray.z+36},{ray.x,ray.y,ray.z-1000},&ground) &&
				!ground.fStartSolid && !ground.fAllSolid) ray.z=ground.vecEndPos.z;
			else ray.z=point.z;
			*bend=ray;return true;
		}
	}
	return false;
}
}
void BotBehaviorRuntime::updateGuardLook(ManagedActionContext &ctx)
{
	auto &state=actors_[ctx.index];const auto &scenario=state.scenarioDecisions;
	auto *self=ctx.handle->entity;auto *engine=services_->service_engineFunctions();
	if(scenario.state!=compat::CompatibilityStateId::Hide || !self || !engine || !engine->pfnTraceLine ||
		ctx.before.dead || (ctx.before.flags&FL_FROZEN)!=0 || state.weaponActions.grenadeInProgress())return;
	// A guard must have a usable weapon before settling into an armed watch.
	const auto inventory=static_cast<std::uint32_t>(self->v.weapons);
	if((inventory & ~((1U<<6)|(1U<<4)|(1U<<9)|(1U<<25)))==0)return;
	compat::PublicActorState equipment{};
	if(services_->observation_publicObservations().read(ctx.actor,ctx.frame,&equipment))
		(void)decideEquipmentAction(ctx,equipment,false);
	const auto &weapon=services_->observation_managedBotCurrentWeapons(ctx.index);
	if(weapon.available && weapon.active && weapon.weaponId==6)
	{
		const std::uint32_t primary=(1U<<3)|(1U<<5)|(1U<<7)|(1U<<8)|(1U<<12)|(1U<<13)|(1U<<14)|(1U<<15)|
			(1U<<18)|(1U<<19)|(1U<<20)|(1U<<21)|(1U<<22)|(1U<<23)|(1U<<24)|(1U<<27)|(1U<<28)|(1U<<30);
		const std::uint32_t pistols=(1U<<1)|(1U<<10)|(1U<<11)|(1U<<16)|(1U<<17)|(1U<<26);
		ctx.proposal.kind=(inventory&primary)!=0 ? ActionKind::SelectPrimary :
			(inventory&pistols)!=0 ? ActionKind::SelectPistol : ActionKind::SelectKnife;
	}
	const auto navigation=services_->service_navPublisher().snapshot();
	const float now=services_->service_globals()->time;
	const nav::NavVector spot={scenario.target.position.x,scenario.target.position.y,scenario.target.position.z};
	if(!state.guardLook.current(ctx.actor,ctx.frame,scenario.target.entity,spot,navigation.revision(),now))
	{
		const nav::NavVector eye={self->v.origin.x+self->v.view_ofs.x,self->v.origin.y+self->v.view_ofs.y,self->v.origin.z+self->v.view_ofs.z};
		float openYaw=0,best=0;
		for(unsigned direction=0;direction<8;++direction)
		{
			const float angle=direction*45.0f*.01745329252f;
			const float a[]={eye.x,eye.y,eye.z},b[]={eye.x+1000*std::cos(angle),eye.y+1000*std::sin(angle),eye.z};
			TraceResult trace{};engine->pfnTraceLine(a,b,0x101,self,&trace);
			if(!trace.fStartSolid && !trace.fAllSolid && trace.flFraction>best){best=trace.flFraction;openYaw=direction*45.0f;}
		}
		std::vector<nav::NavVector> points;
		if(navigation.isValid())
		{
			nav::NavQuery query(navigation);nav::NavAreaMatch match{};const nav::NavArea *area=nullptr;
			if(query.findContaining(spot,120,&match)==nav::NavQueryResult::Found)area=navigation.document()->findArea(match.area);
			if(area)for(const auto &approach:area->approaches)
			{
				if(points.size()>=16)break;
				const auto *here=navigation.document()->findArea(approach.here);
				const auto *previous=navigation.document()->findArea(approach.previous);
				if(!here || !previous)continue;
				const auto point=compat::guardApproachPoint(*previous,*here,approach.previousToHereHow);
				nav::NavVector bend{};
				if(bendApproach(*engine,self,{self->v.origin.x,self->v.origin.y,self->v.origin.z},point,&bend))points.push_back(bend);
			}
		}
		state.guardLook.begin(ctx.actor,ctx.frame,scenario.target.entity,spot,navigation.revision(),now,openYaw,points);
	}
	const bool sniper=weapon.available && (weapon.weaponId==3 || weapon.weaponId==13 || weapon.weaponId==18 || weapon.weaponId==24);
	float pitch=ctx.movementAngles.pitch,yaw=ctx.movementAngles.yaw;
	if(state.guardLook.update(now,sniper,{self->v.origin.x+self->v.view_ofs.x,self->v.origin.y+self->v.view_ofs.y,self->v.origin.z+self->v.view_ofs.z},
		self->v.v_angle.x,self->v.v_angle.y,services_->service_compatibilityRandomSource(),
		{services_->command_managedBotCommandSequences(ctx.index),0,services_->service_managedBotFullUpdateSequences()[ctx.index]},&pitch,&yaw))
	{
		ctx.proposal.viewAngles={pitch,yaw,0};ctx.proposal.hasGuardLook=true;
		ctx.proposal.movementButtons=static_cast<std::uint16_t>((ctx.proposal.movementButtons&~(IN_JUMP|IN_ATTACK|IN_ATTACK2))|IN_DUCK);
		if(services_->service_runtimeProfiler().enabled())runtime_detail::profiledLogConsole(
			services_->service_runtimeProfiler(),services_->service_pluginId(),
			"guard look slot=%u frame=%u now=%.3f view=(%.2f %.2f)",ctx.actor.slot,ctx.frame.tick,now,pitch,yaw);
	}
}
} }
