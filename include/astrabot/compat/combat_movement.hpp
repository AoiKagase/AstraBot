#ifndef ASTRABOT_COMPAT_COMBAT_MOVEMENT_HPP
#define ASTRABOT_COMPAT_COMBAT_MOVEMENT_HPP
#include "astrabot/compat/random_source.hpp"
#include "astrabot/world/world_snapshot.hpp"
#include <cmath>
#include <array>
namespace astrabot { namespace compat {
struct CombatMovementInput
{
	world::ActorKey actor{}, target{};
	world::FrameIdentity frame{};
	float now = 0, skill = 0, aggression = 0, range = 0;
	float planarRange = -1;
	std::uint16_t weapon = 0;
	bool sniper = false, moving = false, enemyFacing = false, outnumbered = false;
	bool (*crouchVisible)(void *) = nullptr;
	void *crouchContext = nullptr;
	ICompatibilityRandomSource *random = nullptr;
	RandomTimingContext timing{};
	const world::WorldSnapshot *knowledge = nullptr;
};
struct CombatMovementDecision
{
	bool active = false, entered = false, crouch = false, jump = false;
	float forward = 0, side = 0;
};
class CombatMovementPlanner
{
public:
	void reset() { *this = CombatMovementPlanner(); }
	void stop() { active_ = false; }
	bool active() const { return active_; }
	CombatMovementDecision update(const CombatMovementInput &in)
	{
		const auto entry = prepare(in);
		if (!entry.active) return entry;
		auto out = finish(in); out.entered = entry.entered; return out;
	}
	CombatMovementDecision prepare(const CombatMovementInput &in)
	{
		CombatMovementDecision out{};
		if (!in.actor.isValid() || !in.frame.isValid() || !std::isfinite(in.now) || in.now < 0 ||
			!std::isfinite(in.skill) || in.skill < 0 || in.skill > 1 || !std::isfinite(in.aggression) ||
			in.aggression < 0 || in.aggression > 1 || !std::isfinite(in.range) || in.range < 0) return out;
		if (!(in.actor == actor_) || frame_.mapGeneration != in.frame.mapGeneration ||
			frame_.roundGeneration != in.frame.roundGeneration || in.now < now_) reset();
		else if (frame_.isValid() && in.frame.tick <= frame_.tick) return out;
		actor_ = in.actor; frame_ = in.frame; now_ = in.now; finished_ = false;
		bool outnumbered = in.outnumbered;
		if (in.knowledge)
		{
			for (std::size_t i=0;i<in.knowledge->actorCount();++i)
			{
				const auto *a=in.knowledge->actorAt(i);
				if(!a || !a->visible || a->actor==in.actor || a->actor.slot==0 || a->actor.slot>watch_.size())continue;
				if(a->relation==world::TeamRelation::Hostile || a->relation==world::TeamRelation::Friendly)
					watch_[a->actor.slot-1]={in.now,a->relation==world::TeamRelation::Hostile,true};
			}
			unsigned enemies=0,friends=0;
			for(const auto &w:watch_)if(w.known && in.now-w.seen<3.0f){if(w.enemy)++enemies;else ++friends;}
			outnumbered=enemies>friends+1;
		}
		if (!in.target.isValid() || in.weapon == 29 || in.weapon == 4 || in.weapon == 9 || in.weapon == 25 || in.weapon == 6)
		{ stop(); return out; }
		auto draw = [&](const char *id, float low, float high, float &value) {
			if (!in.random) return false;
			const auto r = in.random->nextFloat(RandomRequest::floatRequest(id,
				{in.actor.slot,in.actor.generation},in.timing,low,high));
			value = r.value;
			return r.status == RandomStatus::Ok && std::isfinite(value) && value >= low && value <= high;
		};
		if (!active_ || !(target_ == in.target))
		{
			active_ = false; crouch_ = false; firstDodge_ = true; nextDodge_ = 0; dodgeState_ = 0;
			if (combatRange_ == 0 && !draw("Combat.Range",325,425,combatRange_)) return out;
			float roll = 0;
			if (!draw("Combat.CrouchChance",0,100,roll)) return out;
			const float chance = in.sniper || in.range > 750 ? 50 : 20 * (1 - in.aggression);
			crouch_ = roll < chance && in.crouchVisible && in.crouchVisible(in.crouchContext);
			if (!draw("Combat.DodgeChance",0,100,roll)) return out;
			dodge_ = roll < (in.skill > .5f && outnumbered ? 100 : 80 * in.skill);
			target_ = in.target; active_ = true; out.entered = true;
		}
		out.active = true; out.crouch = crouch_;
		return out;
	}
	CombatMovementDecision finish(const CombatMovementInput &in)
	{
		CombatMovementDecision out{};
		if (!active_ || finished_ || !(in.actor == actor_) || !(in.target == target_) ||
			!(in.frame == frame_) || in.now != now_) return out;
		finished_ = true; out.active = true; out.crouch = crouch_;
		if (in.sniper || crouch_ || !dodge_) return out;
		const float range = in.planarRange >= 0 ? in.planarRange : in.range;
		if (in.skill < .66f)
		{
			if (range > combatRange_ + 125) out.forward = 1;
			else if (range < combatRange_ - 125) out.forward = -1;
		}
		if (range > 2000 || !in.enemyFacing) { dodgeState_ = 0; nextDodge_ = 0; }
		else if (in.now >= nextDodge_)
		{
			auto draw = [&](const char *id, float low, float high, float &value) {
				if (!in.random) return false;
				const auto r = in.random->nextFloat(RandomRequest::floatRequest(id,
					{in.actor.slot,in.actor.generation},in.timing,low,high));
				value = r.value;
				return r.status == RandomStatus::Ok && std::isfinite(value) && value >= low && value <= high;
			};
			bool selected = false;
			for (unsigned attempt = 0; attempt < 32 && !selected; ++attempt)
			{
				float jumpRoll = 100;
				if (firstDodge_ && in.skill > .5f && !draw("Combat.JumpChance",0,100,jumpRoll))
					return {};
				const int upper = firstDodge_ && in.skill > .5f && in.moving && jumpRoll < 33.3f ? 3 : 2;
				if (!in.random) return {};
				const auto choice = in.random->nextLong(RandomRequest::longRequest("Combat.DodgeState",
					{in.actor.slot,in.actor.generation},in.timing,0,upper));
				if (choice.status != RandomStatus::Ok || choice.value < 0 || choice.value > upper) return {};
				if (!firstDodge_ && choice.value == dodgeState_) continue;
				dodgeState_ = choice.value; selected = true;
			}
			float duration = 0;
			if (!selected || !draw("Combat.DodgeDuration",.3f,1,duration)) return {};
			nextDodge_ = in.now + duration; firstDodge_ = false;
		}
		out.side = dodgeState_ == 1 ? -1.0f : dodgeState_ == 2 ? 1.0f : 0.0f;
		out.jump = dodgeState_ == 3;
		return out;
	}
private:
	struct Watch { float seen=0; bool enemy=false,known=false; };
	std::array<Watch,32> watch_{};
	world::ActorKey actor_{}, target_{};
	world::FrameIdentity frame_{};
	float now_ = 0, combatRange_ = 0, nextDodge_ = 0;
	bool active_ = false, crouch_ = false, dodge_ = false, firstDodge_ = true;
	bool finished_ = false;
	int dodgeState_ = 0;
};
} }
#endif
