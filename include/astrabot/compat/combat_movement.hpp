#ifndef ASTRABOT_COMPAT_COMBAT_MOVEMENT_HPP
#define ASTRABOT_COMPAT_COMBAT_MOVEMENT_HPP
#include "astrabot/compat/random_source.hpp"
#include "astrabot/world/world_snapshot.hpp"
#include <cmath>
#include <array>
namespace astrabot
{
namespace compat
{
struct CombatMovementInput
{
	static constexpr int kUnknownPlanarRange = -1;
	world::ActorKey actor{}, target{};
	world::FrameIdentity frame{};
	float now = 0, skill = 0, aggression = 0, range = 0;
	float planarRange = kUnknownPlanarRange;
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
	void reset()
	{
		*this = CombatMovementPlanner();
	}

	void stop()
	{
		active_ = false;
	}

	bool active() const
	{
		return active_;
	}

	CombatMovementDecision update(const CombatMovementInput &in)
	{
		const auto entry = prepare(in);
		if (!entry.active)
			return entry;
		auto out = finish(in);
		out.entered = entry.entered;
		return out;
	}

	CombatMovementDecision prepare(const CombatMovementInput &in)
	{
		constexpr float kOutnumberedObservationSeconds = 3.0f;
		constexpr int kObserverFriendlyCount = 1;
		constexpr int kKnifeWeaponId = 29;
		constexpr int kHeGrenadeWeaponId = 4;
		constexpr int kSmokeGrenadeWeaponId = 9;
		constexpr int kFlashbangWeaponId = 25;
		constexpr int kC4WeaponId = 6;
		constexpr int kMinimumCombatRange = 325;
		constexpr int kMaximumCombatRange = 425;
		constexpr int kChanceRollMinimum = 0;
		constexpr int kChanceRollMaximum = 100;
		constexpr int kDistantCrouchRange = 750;
		constexpr int kDistantCrouchChance = 50;
		constexpr int kAggressionCrouchChanceScale = 20;
		constexpr float kOutnumberedDodgeMinimumSkill = .5f;
		constexpr int kOutnumberedDodgeChance = 100;
		constexpr int kDodgeChanceSkillScale = 80;
		CombatMovementDecision out{};
		if (!in.actor.isValid() || !in.frame.isValid() || !std::isfinite(in.now) || in.now < 0 ||
			!std::isfinite(in.skill) || in.skill < 0 || in.skill > 1 || !std::isfinite(in.aggression) ||
			in.aggression < 0 || in.aggression > 1 || !std::isfinite(in.range) || in.range < 0)
			return out;
		if (!(in.actor == actor_) || frame_.mapGeneration != in.frame.mapGeneration ||
			frame_.roundGeneration != in.frame.roundGeneration || in.now < now_)
			reset();
		else if (frame_.isValid() && in.frame.tick <= frame_.tick)
			return out;
		actor_ = in.actor;
		frame_ = in.frame;
		now_ = in.now;
		finished_ = false;
		bool outnumbered = in.outnumbered;
		if (in.knowledge)
		{
			for (std::size_t i = 0; i < in.knowledge->actorCount(); ++i)
			{
				const auto *a = in.knowledge->actorAt(i);
				if (!a || !a->visible || a->actor == in.actor || a->actor.slot == 0 || a->actor.slot > watch_.size())
					continue;
				if (a->relation == world::TeamRelation::Hostile || a->relation == world::TeamRelation::Friendly)
					watch_[a->actor.slot - 1] = {in.now, a->relation == world::TeamRelation::Hostile, true};
			}
			unsigned enemies = 0, friends = 0;
			for (const auto &w : watch_)
				if (w.known && in.now - w.seen < kOutnumberedObservationSeconds)
				{
					if (w.enemy)
						++enemies;
					else
						++friends;
				}
			outnumbered = enemies > friends + kObserverFriendlyCount;
		}
		if (!in.target.isValid() || in.weapon == kKnifeWeaponId || in.weapon == kHeGrenadeWeaponId ||
			in.weapon == kSmokeGrenadeWeaponId || in.weapon == kFlashbangWeaponId || in.weapon == kC4WeaponId)
		{
			stop();
			return out;
		}
		auto draw = [&](const char *id, float low, float high, float &value)
		{
			if (!in.random)
				return false;
			const auto r = in.random->nextFloat(
				RandomRequest::floatRequest(id, {in.actor.slot, in.actor.generation}, in.timing, low, high));
			value = r.value;
			return r.status == RandomStatus::Ok && std::isfinite(value) && value >= low && value <= high;
		};
		if (!active_ || !(target_ == in.target))
		{
			active_ = false;
			crouch_ = false;
			firstDodge_ = true;
			nextDodge_ = 0;
			dodgeState_ = kDodgeNone;
			if (combatRange_ == 0 && !draw("Combat.Range", kMinimumCombatRange, kMaximumCombatRange, combatRange_))
				return out;
			float roll = 0;
			if (!draw("Combat.CrouchChance", kChanceRollMinimum, kChanceRollMaximum, roll))
				return out;
			const float chance = in.sniper || in.range > kDistantCrouchRange
				? kDistantCrouchChance
				: kAggressionCrouchChanceScale * (1 - in.aggression);
			crouch_ = roll < chance && in.crouchVisible && in.crouchVisible(in.crouchContext);
			if (!draw("Combat.DodgeChance", kChanceRollMinimum, kChanceRollMaximum, roll))
				return out;
			dodge_ =
				roll < (in.skill > kOutnumberedDodgeMinimumSkill && outnumbered ? kOutnumberedDodgeChance
																				: kDodgeChanceSkillScale * in.skill);
			target_ = in.target;
			active_ = true;
			out.entered = true;
		}
		out.active = true;
		out.crouch = crouch_;
		return out;
	}

	CombatMovementDecision finish(const CombatMovementInput &in)
	{
		constexpr int kDodgeLeft = 1;
		constexpr int kDodgeRight = 2;
		constexpr int kDodgeJump = 3;
// Keep literal evaluation precision on x87; each macro is undefined below.
#define ASTRABOT_COMBAT_MOVEMENT_RANGE_ADJUSTMENT_MAXIMUM_SKILL .66f
		constexpr int kCombatRangeTolerance = 125;
		constexpr int kMaximumDodgeRange = 2000;
		constexpr int kMaximumDodgeSelectionAttempts = 32;
		constexpr int kChanceRollMinimum = 0;
		constexpr int kChanceRollMaximum = 100;
		constexpr float kJumpMinimumSkill = .5f;
#define ASTRABOT_COMBAT_MOVEMENT_JUMP_CHANCE 33.3f
#define ASTRABOT_COMBAT_MOVEMENT_MINIMUM_DODGE_SECONDS .3f
		constexpr int kMaximumDodgeSeconds = 1;
		CombatMovementDecision out{};
		if (!active_ || finished_ || !(in.actor == actor_) || !(in.target == target_) || !(in.frame == frame_) ||
			in.now != now_)
			return out;
		finished_ = true;
		out.active = true;
		out.crouch = crouch_;
		if (in.sniper || crouch_ || !dodge_)
			return out;
		const float range = in.planarRange >= 0 ? in.planarRange : in.range;
		if (in.skill < ASTRABOT_COMBAT_MOVEMENT_RANGE_ADJUSTMENT_MAXIMUM_SKILL)
		{
			if (range > combatRange_ + kCombatRangeTolerance)
				out.forward = 1;
			else if (range < combatRange_ - kCombatRangeTolerance)
				out.forward = -1;
		}
		if (range > kMaximumDodgeRange || !in.enemyFacing)
		{
			dodgeState_ = kDodgeNone;
			nextDodge_ = 0;
		}
		else if (in.now >= nextDodge_)
		{
			auto draw = [&](const char *id, float low, float high, float &value)
			{
				if (!in.random)
					return false;
				const auto r = in.random->nextFloat(
					RandomRequest::floatRequest(id, {in.actor.slot, in.actor.generation}, in.timing, low, high));
				value = r.value;
				return r.status == RandomStatus::Ok && std::isfinite(value) && value >= low && value <= high;
			};
			bool selected = false;
			for (unsigned attempt = 0; attempt < kMaximumDodgeSelectionAttempts && !selected; ++attempt)
			{
				float jumpRoll = kChanceRollMaximum;
				if (firstDodge_ && in.skill > kJumpMinimumSkill &&
					!draw("Combat.JumpChance", kChanceRollMinimum, kChanceRollMaximum, jumpRoll))
					return {};
				const int upper = firstDodge_ && in.skill > kJumpMinimumSkill && in.moving &&
						jumpRoll < ASTRABOT_COMBAT_MOVEMENT_JUMP_CHANCE
					? kDodgeJump
					: kDodgeRight;
				if (!in.random)
					return {};
				const auto choice = in.random->nextLong(RandomRequest::longRequest(
					"Combat.DodgeState", {in.actor.slot, in.actor.generation}, in.timing, kDodgeNone, upper));
				if (choice.status != RandomStatus::Ok || choice.value < kDodgeNone || choice.value > upper)
					return {};
				if (!firstDodge_ && choice.value == dodgeState_)
					continue;
				dodgeState_ = choice.value;
				selected = true;
			}
			float duration = 0;
			if (!selected ||
				!draw("Combat.DodgeDuration", ASTRABOT_COMBAT_MOVEMENT_MINIMUM_DODGE_SECONDS, kMaximumDodgeSeconds,
					duration))
				return {};
			nextDodge_ = in.now + duration;
			firstDodge_ = false;
		}
		out.side = dodgeState_ == kDodgeLeft ? -1.0f : dodgeState_ == kDodgeRight ? 1.0f : 0.0f;
		out.jump = dodgeState_ == kDodgeJump;
		return out;
	}
#undef ASTRABOT_COMBAT_MOVEMENT_RANGE_ADJUSTMENT_MAXIMUM_SKILL
#undef ASTRABOT_COMBAT_MOVEMENT_JUMP_CHANCE
#undef ASTRABOT_COMBAT_MOVEMENT_MINIMUM_DODGE_SECONDS

private:
	static constexpr int kDodgeNone = 0;
	static constexpr int kWatchedActorCapacity = 32;
	struct Watch
	{
		float seen = 0;
		bool enemy = false, known = false;
	};
	std::array<Watch, kWatchedActorCapacity> watch_{};
	world::ActorKey actor_{}, target_{};
	world::FrameIdentity frame_{};
	float now_ = 0, combatRange_ = 0, nextDodge_ = 0;
	bool active_ = false, crouch_ = false, dodge_ = false, firstDodge_ = true;
	bool finished_ = false;
	int dodgeState_ = kDodgeNone;
};
}
}
#endif
