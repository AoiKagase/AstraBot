#ifndef ASTRABOT_COMPAT_FRIEND_DEATH_HPP
#define ASTRABOT_COMPAT_FRIEND_DEATH_HPP

#include "astrabot/compat/random_source.hpp"
#include "astrabot/compat/state_machine.hpp"
#include <cmath>

namespace astrabot
{
namespace compat
{
struct ReactionContext
{
	world::ActorKey actor{};
	world::FrameIdentity frame{};
	std::uint32_t entityGeneration = 0;
	float now = 0;
	bool alive = false;
	bool valid() const
	{
		return actor.isValid() && frame.isValid() && entityGeneration != 0 && std::isfinite(now) && now >= 0 && alive;
	}
};

enum class ReactionDisposition
{
	Engage,
	Opportunity,
	SelfDefense,
	Ignore
};
enum class FriendDeathAction
{
	None,
	Retaliate,
	RethinkHunt,
	Reposition
};
struct FriendDeathInput
{
	ReactionContext context{};
	RandomTimingContext timing{};
	CompatibilityStateId state = CompatibilityStateId::None;
	ReactionDisposition disposition = ReactionDisposition::Engage;
	bool victimFriendly = false;
	bool killerEnemy = false;
	bool victimVisible = false;
	bool sniperInventory = false;
	bool usingKnife = false;
	bool attacking = false;
	bool enemyKnown = false;
	float lastSawEnemy = 0;
};
struct FriendDeathDecision
{
	FriendDeathAction action = FriendDeathAction::None;
	float hurrySeconds = 0;
};

inline FriendDeathAction decideFriendDeathTail(const FriendDeathInput &in)
{
	if (!in.context.valid() || !in.victimFriendly)
	{
		return FriendDeathAction::None;
	}
	if (in.state == CompatibilityStateId::Hunt)
	{
		return FriendDeathAction::RethinkHunt;
	}
	else
	{
		constexpr float kRepositionLostSightSeconds = .4f;
		if (in.attacking && in.enemyKnown && std::isfinite(in.lastSawEnemy) &&
			in.context.now - in.lastSawEnemy > kRepositionLostSightSeconds)
		{
			return FriendDeathAction::Reposition;
		}
	}
	return FriendDeathAction::None;
}

inline FriendDeathDecision decideFriendDeath(const FriendDeathInput &in, ICompatibilityRandomSource &random)
{
	FriendDeathDecision out{};
	if (!in.context.valid() || !in.victimFriendly)
	{
		return out;
	}
	if (!in.attacking &&
		(in.disposition == ReactionDisposition::Engage || in.disposition == ReactionDisposition::Opportunity) &&
		in.killerEnemy && !in.sniperInventory && in.victimVisible)
	{
		const auto hurry = random.nextFloat(RandomRequest::floatRequest(
			"FriendDeath.Hurry", {in.context.actor.slot, in.context.actor.generation}, in.timing, 10.0f, 15.0f));
		if (hurry.status != RandomStatus::Ok || !std::isfinite(hurry.value) || hurry.value < 10 || hurry.value > 15)
		{
			return out;
		}
		out.hurrySeconds = hurry.value;
		bool attack = true;
		if (in.state == CompatibilityStateId::Hide && in.usingKnife)
		{
			const auto ambush = random.nextFloat(RandomRequest::floatRequest("FriendDeath.KnifeAmbush",
				{in.context.actor.slot, in.context.actor.generation}, in.timing, 0.0f, 100.0f));
			if (ambush.status != RandomStatus::Ok || !std::isfinite(ambush.value) || ambush.value < 0 ||
				ambush.value > 100)
			{
				return out;
			}
			constexpr float kKnifeAmbushChance = 50.0f;
			attack = ambush.value < kKnifeAmbushChance;
		}
		if (attack)
		{
			out.action = FriendDeathAction::Retaliate;
			return out;
		}
	}
	out.action = decideFriendDeathTail(in);
	return out;
}

class EnemyReactionMemory
{
public:
	void reset()
	{
		*this = EnemyReactionMemory{};
	}
	void resetForLife()
	{
		// The bot learns its safe time for this map. Respawn clears enemy memory.
		const auto owner = context_;
		const float learned = safeTime_;
		const bool adjusted = safeTimeAdjusted_;
		reset();
		context_.actor = owner.actor;
		context_.entityGeneration = owner.entityGeneration;
		context_.frame.mapGeneration = owner.frame.mapGeneration;
		safeTime_ = learned;
		safeTimeAdjusted_ = adjusted;
	}

	bool update(const ReactionContext &context)
	{
		if (!matches(context))
		{
			if (context.actor.isValid() && context.actor == context_.actor &&
				context.entityGeneration == context_.entityGeneration && context.frame.mapGeneration == context_.frame.mapGeneration)
				resetForLife();
			else
				reset();
		}
		if (!context.valid())
			return false;
		context_ = context;
		return true;
	}

	void seedRetaliation(
		const ReactionContext &context, world::ActorKey enemy, world::EntityKey source, world::WorldPosition point)
	{
		if (!remember(context, enemy, source, point))
		{
			return;
		}
		initialAttack_ = true;
		eventActive_ = true;
	}

	void rememberVisible(
		const ReactionContext &context, world::ActorKey enemy, world::EntityKey source, world::WorldPosition point)
	{
		if (remember(context, enemy, source, point))
		{
			initialAttack_ = false;
		}
	}

	void hurry(const ReactionContext &context, float seconds)
	{
		if (update(context) && std::isfinite(seconds) && seconds > 0)
		{
			hurryUntil_ = context.now + seconds;
			eventActive_ = true;
		}
	}

	void beginPursuit(const ReactionContext &context, bool ignore, CompatibilityStateId from)
	{
		if (!update(context) || !enemy_.isValid())
		{
			return;
		}
		pursuing_ = true;
		eventActive_ = true;
		initialAttack_ = false;
		pursuitPosition_ = position_;
		pursuitSource_ = source_;
		pursuitEnemy_ = enemy_;
		++pursuitGeneration_;
		if (pursuitGeneration_ == 0)
		{
			++pursuitGeneration_;
		}
		constexpr float kIgnoreEnemiesSeconds = 1.0f;
		if (ignore)
		{
			ignoreUntil_ = context.now + kIgnoreEnemiesSeconds;
		}
		onStateChange(context, from, CompatibilityStateId::MoveTo);
	}

	void onStateChange(const ReactionContext &context, CompatibilityStateId from, CompatibilityStateId to)
	{
		if (!update(context))
		{
			return;
		}
		if (from == CompatibilityStateId::MoveTo || from == CompatibilityStateId::PlantBomb ||
			from == CompatibilityStateId::DefuseBomb || to == CompatibilityStateId::Idle ||
			to == CompatibilityStateId::Hunt || to == CompatibilityStateId::PlantBomb ||
			to == CompatibilityStateId::DefuseBomb)
		{
			ignoreUntil_ = -1;
		}
		if (to != CompatibilityStateId::MoveTo)
		{
			pursuing_ = false;
		}
	}

	void ignoreEnemies(const ReactionContext &context, float duration)
	{
		if (update(context))
			ignoreUntil_ = context.now + duration;
	}

	void cancelIgnore()
	{
		ignoreUntil_ = -1;
	}

	void markDead(const ReactionContext &context)
	{
		if (!matches(context) || !enemy_.isValid())
			return;
		if (!enemyDead_)
			enemyDeathTimestamp_ = context.now;
		enemyDead_ = true;
		eventActive_ = true;
	}

	float enemyDeathTimestamp(const ReactionContext &context) const
	{
		return matches(context) ? enemyDeathTimestamp_ : 0.0f;
	}

	bool enemyDead(const ReactionContext &context) const
	{
		return matches(context) && enemyDead_;
	}

	void adjustSafeTime(float elapsedRound, float aggression)
	{
		if (std::isfinite(elapsedRound) && std::isfinite(aggression) && aggression >= 0 && aggression <= 1)
		{
			const float initial = 15.0f + 5.0f * aggression;
			if (!safeTimeAdjusted_)
			{
				safeTime_ = initial;
				safeTimeAdjusted_ = true;
			}
			if (safeTime_ > elapsedRound)
			{
				safeTime_ = elapsedRound - 2.0f;
			}
		}
	}

	bool wellPastSafe(float elapsedRound, float aggression) const
	{
		return std::isfinite(elapsedRound) &&
			elapsedRound > 1.25f * (safeTimeAdjusted_ ? safeTime_ : 15.0f + 5.0f * aggression);
	}
	float safeTime(float aggression) const
	{
		return safeTimeAdjusted_ ? safeTime_ : 15.0f + 5.0f * aggression;
	}
	bool isSafe(float elapsedRound, float aggression) const
	{
		return std::isfinite(elapsedRound) && elapsedRound < safeTime(aggression);
	}

	void clearEnemy(bool cancelPursuit = true)
	{
		enemy_ = {};
		source_ = {};
		enemyDead_ = false;
		if (cancelPursuit)
		{
			pursuing_ = false;
		}
		initialAttack_ = false;
	}

	world::ActorKey enemy(const ReactionContext &context) const
	{
		return matches(context) ? enemy_ : world::ActorKey{};
	}
	world::EntityKey source(const ReactionContext &context) const
	{
		return matches(context) ? source_ : world::EntityKey{};
	}
	world::WorldPosition position(const ReactionContext &context) const
	{
		return matches(context) ? position_ : world::WorldPosition{};
	}
	float lastSaw(const ReactionContext &context) const
	{
		return matches(context) ? lastSaw_ : -1;
	}
	bool initialAttack(const ReactionContext &context) const
	{
		return matches(context) && initialAttack_;
	}
	bool eventActive(const ReactionContext &context) const
	{
		return matches(context) &&
			(pursuing_ || initialAttack_ || enemyDead_ || (eventActive_ && enemy_.isValid()) || hurrying(context) ||
				ignoring(context));
	}
	bool shouldAim(const ReactionContext &context) const
	{
		constexpr float kRecentEnemyAimSeconds = 3.0f;
		return !enemyDead(context) && enemy(context).isValid() && context.now - lastSaw_ < kRecentEnemyAimSeconds;
	}
	bool hurrying(const ReactionContext &context) const
	{
		return matches(context) && hurryUntil_ >= 0 && context.now <= hurryUntil_;
	}
	bool ignoring(const ReactionContext &context) const
	{
		return matches(context) && ignoreUntil_ >= 0 && context.now <= ignoreUntil_;
	}
	bool pursuing(const ReactionContext &context) const
	{
		return matches(context) && pursuing_;
	}
	world::WorldPosition pursuitPosition(const ReactionContext &context) const
	{
		return pursuing(context) ? pursuitPosition_ : world::WorldPosition{};
	}
	world::ActorKey pursuitEnemy(const ReactionContext &context) const
	{
		return pursuing(context) ? pursuitEnemy_ : world::ActorKey{};
	}
	world::EntityKey pursuitSource(const ReactionContext &context) const
	{
		return pursuing(context) ? pursuitSource_ : world::EntityKey{};
	}
	std::uint32_t pursuitGeneration() const
	{
		return pursuitGeneration_;
	}

private:
	bool matches(const ReactionContext &context) const
	{
		return context.valid() && context_.valid() && context.actor == context_.actor &&
			context.entityGeneration == context_.entityGeneration &&
			context.frame.mapGeneration == context_.frame.mapGeneration &&
			context.frame.roundGeneration == context_.frame.roundGeneration &&
			context.frame.tick >= context_.frame.tick && context.now >= context_.now;
	}
	bool remember(
		const ReactionContext &context, world::ActorKey enemy, world::EntityKey source, world::WorldPosition point)
	{
		if (!update(context) || !enemy.isValid() || enemy == context.actor || !source.isValid() ||
			enemy.slot != source.id || (enemyDead_ && source_ == source))
		{
			return false;
		}
		for (float value : {point.x, point.y, point.z})
		{
			if (!std::isfinite(value) || std::fabs(value) > world::WorldLimits::kMaximumCoordinate)
			{
				return false;
			}
		}
		enemy_ = enemy;
		source_ = source;
		enemyDead_ = false;
		enemyDeathTimestamp_ = 0.0f;
		position_ = point;
		lastSaw_ = context.now;
		return true;
	}
	ReactionContext context_{};
	world::ActorKey enemy_{};
	world::EntityKey source_{};
	world::WorldPosition position_{};
	world::WorldPosition pursuitPosition_{};
	world::EntityKey pursuitSource_{};
	world::ActorKey pursuitEnemy_{};
	float safeTime_ = -1;
	float lastSaw_ = -1;
	float enemyDeathTimestamp_ = 0.0f;
	float hurryUntil_ = -1;
	float ignoreUntil_ = -1;
	std::uint32_t pursuitGeneration_ = 0;
	bool safeTimeAdjusted_ = false;
	bool enemyDead_ = false;
	bool initialAttack_ = false;
	bool eventActive_ = false;
	bool pursuing_ = false;
};
}
}
#endif
