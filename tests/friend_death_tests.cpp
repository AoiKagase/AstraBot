#include "astrabot/compat/friend_death.hpp"
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>
namespace
{
struct Random : astrabot::compat::ICompatibilityRandomSource
{
	std::vector<std::string> ids;
	float ambush = 49;
	astrabot::compat::RandomFloatResult nextFloat(const astrabot::compat::RandomRequest &r) override
	{
		ids.push_back(r.semanticId);
		return {astrabot::compat::RandomStatus::Ok, r.floatLower == 10 ? 12.0f : ambush};
	}
	astrabot::compat::RandomLongResult nextLong(const astrabot::compat::RandomRequest &) override
	{
		return {astrabot::compat::RandomStatus::Unavailable, 0};
	}
};
}
int main()
{
	using namespace astrabot;
	using namespace compat;
	int failures = 0;
	const auto check = [&](bool ok, const char *message)
	{
		if (!ok)
		{
			std::printf("FAIL: %s\n", message);
			++failures;
		}
	};
	ReactionContext context{{1, 2}, {3, 4, 5}, 6, 10, true};
	FriendDeathInput in{};
	in.context = context;
	in.victimFriendly = true;
	in.killerEnemy = true;
	in.victimVisible = true;
	in.state = CompatibilityStateId::Hunt;
	Random rng;
	auto decision = decideFriendDeath(in, rng);
	check(decision.action == FriendDeathAction::Retaliate && decision.hurrySeconds == 12 && rng.ids.size() == 1,
		"retaliation precedes Hunt and draws Hurry only");
	check(rng.ids[0] == "FriendDeath.Hurry", "named Hurry draw");
	in.state = CompatibilityStateId::Hide;
	in.usingKnife = true;
	rng.ids.clear();
	decision = decideFriendDeath(in, rng);
	check(decision.action == FriendDeathAction::Retaliate && rng.ids.size() == 2 &&
			rng.ids[1] == "FriendDeath.KnifeAmbush",
		"hiding knife draws after Hurry");
	rng.ambush = 50;
	decision = decideFriendDeath(in, rng);
	check(decision.action == FriendDeathAction::None && decision.hurrySeconds == 12,
		"strict50 rejects attack but preserves Hurry");
	in.state = CompatibilityStateId::Hunt;
	in.sniperInventory = true;
	rng.ids.clear();
	decision = decideFriendDeath(in, rng);
	check(decision.action == FriendDeathAction::RethinkHunt && rng.ids.empty(),
		"inventory sniper skips retaliation then rethinks Hunt");
	in.sniperInventory = false;
	in.attacking = true;
	in.enemyKnown = true;
	in.lastSawEnemy = 9;
	check(decideFriendDeath(in, rng).action == FriendDeathAction::RethinkHunt, "Hunt wins over attacking reposition");
	in.state = CompatibilityStateId::Follow;
	in.context.now = 10.4f;
	in.lastSawEnemy = 10;
	check(decideFriendDeath(in, rng).action == FriendDeathAction::None, "strict0.4 no reposition");
	in.context.now = 10.401f;
	check(decideFriendDeath(in, rng).action == FriendDeathAction::Reposition, "greater0.4 repositions");
	in.killerEnemy = false;
	check(decideFriendDeath(in, rng).action == FriendDeathAction::Reposition,
		"world or friendly killer does not suppress tail reaction");
	in.victimFriendly = false;
	check(decideFriendDeath(in, rng).action == FriendDeathAction::None, "enemy death cannot trigger friend reaction");
	in.victimFriendly = true;
	in.attacking = false;
	in.killerEnemy = true;
	in.disposition = ReactionDisposition::SelfDefense;
	check(decideFriendDeath(in, rng).action == FriendDeathAction::None, "self defense excludes early retaliation");
	in.disposition = ReactionDisposition::Ignore;
	check(decideFriendDeath(in, rng).action == FriendDeathAction::None, "ignore excludes early retaliation");
	in.disposition = ReactionDisposition::Engage;
	in.victimVisible = false;
	check(decideFriendDeath(in, rng).action == FriendDeathAction::None, "unseen friend cannot grant killer position");
	EnemyReactionMemory memory;
	const world::ActorKey enemy{7, 8};
	const world::EntityKey entity{7, 9};
	memory.seedRetaliation(context, enemy, entity, {100, 200, 36});
	check(memory.enemy(context) == enemy && memory.source(context) == entity && memory.shouldAim(context) &&
			memory.initialAttack(context),
		"native event seed is qualified memory");
	memory.hurry(context, 12);
	memory.beginPursuit(context, true, CompatibilityStateId::Hide);
	context.now = 11;
	check(memory.ignoring(context) && memory.pursuing(context), "ignore deadline inclusive");
	context.now = 11.001f;
	check(!memory.ignoring(context), "ignore expires strictly after1");
	context.now = 12;
	memory.rememberVisible(context, enemy, entity, {300, 400, 36});
	check(memory.pursuitPosition(context).x == 100 && memory.position(context).x == 300,
		"chase goal stays fixed while visible enemy memory updates");
	context.now = 22;
	check(memory.hurrying(context), "Hurry deadline inclusive");
	context.now = 22.001f;
	check(!memory.hurrying(context), "Hurry expires after original deadline");
	context.now = 30;
	memory.seedRetaliation(context, enemy, entity, {100, 200, 36});
	memory.beginPursuit(context, true, CompatibilityStateId::MoveTo);
	check(!memory.ignoring(context), "native MoveTo exit cancels newly requested IgnoreEnemies");
	memory.beginPursuit(context, true, CompatibilityStateId::Follow);
	check(memory.ignoring(context), "Follow exit retains ignore timer");
	memory.onStateChange(context, CompatibilityStateId::MoveTo, CompatibilityStateId::Idle);
	check(
		!memory.ignoring(context) && !memory.pursuing(context), "native disposition reset cancels ignore and pursuit");
	context.now = 40;
	memory.seedRetaliation(context, enemy, entity, {100, 200, 36});
	context.now = 39;
	check(!memory.update(context) || !memory.enemy(context).isValid(), "clock rollback cancels enemy memory");
	context.now = 40;
	check(!memory.enemy(context).isValid(), "rollback cannot revive seed");
	memory.seedRetaliation(context, enemy, entity, {100, 200, 36});
	++context.entityGeneration;
	memory.update(context);
	check(!memory.enemy(context).isValid(), "observer entity reuse clears memory");
	memory.seedRetaliation(context, enemy, entity, {100, 200, 36});
	++context.frame.roundGeneration;
	memory.update(context);
	check(!memory.enemy(context).isValid(), "round clears memory");
	memory.seedRetaliation(context, enemy, entity, {100, 200, 36});
	context.alive = false;
	memory.update(context);
	context.alive = true;
	check(!memory.enemy(context).isValid(), "death/revival cannot restore memory");
	context.alive = true;
	context.now = 50;
	memory.seedRetaliation(context, enemy, entity, {100, 200, 36});
	memory.markDead(context);
	check(memory.enemyDead(context) && !memory.shouldAim(context) && memory.enemyDeathTimestamp(context) == 50,
		"public death cancels aim even if source revives before update");
	memory.rememberVisible(context, enemy, entity, {300, 400, 36});
	check(memory.enemyDead(context), "same dead receipt cannot be resurrected by stale visibility");
	memory.reset();
	memory.update(context);
	memory.adjustSafeTime(10, .5f);
	check(!memory.wellPastSafe(10, .5f) && memory.wellPastSafe(10.001f, .5f), "native strict well-past-safe boundary");
	memory.adjustSafeTime(12, .5f);
	check(memory.wellPastSafe(10.1f, .5f), "later sightings do not extend safe time");
	memory.adjustSafeTime(9, .5f);
	check(!memory.wellPastSafe(10, .5f), "sightings after safe time do not shorten the prior safe deadline");
	EnemyReactionMemory early;
	early.update(context);
	early.adjustSafeTime(.5f, .5f);
	check(early.wellPastSafe(.6f, .5f), "native early sighting preserves negative safe time");
	early.adjustSafeTime(1, .5f);
	check(early.wellPastSafe(.6f, .5f), "later sighting cannot reset negative safe time to default");
	check(memory.isSafe(7.999f, .5f) && !memory.isSafe(8.0f, .5f), "the adjusted safe deadline is strict");
	auto nextRound = context;
	++nextRound.frame.roundGeneration;
	memory.resetForLife();
	memory.update(nextRound);
	check(!memory.isSafe(8.0f, .5f), "the same bot retains the map's safe deadline on respawn");
	++nextRound.actor.generation;
	memory.update(nextRound);
	check(memory.isSafe(8.0f, .5f), "a replacement bot starts with its profile safe time");
	CompatibilityStateMachine machine(context.actor);
	StateUpdateContext state{};
	state.actor = context.actor;
	state.frame = context.frame;
	state.timestamp = context.now;
	state.fullUpdate = true;
	state.fullUpdateSequence = 1;
	state.alive = true;
	state.roundActive = true;
	(void)machine.initialize(state);
	state.moveToTask = CompatibilityTaskId::MoveToLastKnownEnemyPosition;
	(void)machine.transition(CompatibilityStateId::MoveTo, StateTransitionReason::DeathObserved,
		StateTransitionId::ExplicitStateChange, state);
	check(machine.task() == CompatibilityTaskId::MoveToLastKnownEnemyPosition,
		"MoveTo entry preserves native chase task");
	state.moveToTask = CompatibilityTaskId::None;
	(void)machine.transition(CompatibilityStateId::MoveTo, StateTransitionReason::ExplicitRequest,
		StateTransitionId::ExplicitStateChange, state);
	check(machine.task() == CompatibilityTaskId::MoveTo, "later ordinary MoveTo does not inherit chase task");
	EnemyReactionMemory separate;
	separate.seedRetaliation(context, enemy, entity, {100, 200, 36});
	separate.beginPursuit(context, false, CompatibilityStateId::Idle);
	separate.rememberVisible(context, {9, 10}, {9, 11}, {300, 400, 36});
	check(separate.pursuitEnemy(context) == enemy,
		"task entity connection remains distinct from a newly selected combat enemy");
	separate.clearEnemy(false);
	check(separate.pursuing(context) && separate.pursuitEnemy(context) == enemy,
		"clearing another enemy does not erase the live pursuit task");
	EnemyReactionMemory injury;
	check(injury.recordEnemyDamage(context, entity, 5, .5f) && injury.recentlyDamaged(context, 1) &&
		!injury.isSafe(3, .5f) && !injury.enemy(context).isValid(),
		"enemy damage cuts safe time without acquiring a visual target");
	check(!injury.recordEnemyDamage(context, entity, 5, .5f) &&
		injury.recordEnemyDamage(context, {entity.id, entity.generation + 1U}, 5, .5f),
		"the same attacker and timestamp are one injury reaction, a reused slot is distinct");
	auto oneSecondLater = context;
	oneSecondLater.now += 1.0f;
	++oneSecondLater.frame.tick;
	check(!injury.recentlyDamaged(oneSecondLater, 1), "the recent-injury window excludes one second exactly");
	++oneSecondLater.frame.roundGeneration;
	injury.update(oneSecondLater);
	check(!injury.recentlyDamaged(oneSecondLater, 1) && !injury.isSafe(3, .5f),
		"round reset clears injury while retaining learned safe time for the same bot");
	EnemyReactionMemory damageBeforeBaseRejection;
	auto deadVictim = context;
	deadVictim.alive = false;
	check(damageBeforeBaseRejection.recordEnemyDamage(deadVictim, entity, 5, .5f) &&
		!damageBeforeBaseRejection.recordEnemyDamage(deadVictim, entity, 5, .5f),
		"direct enemy damage records its timing before the base rejection of a dead victim");
	damageBeforeBaseRejection.update(context);
	check(!damageBeforeBaseRejection.isSafe(3, .5f) && !damageBeforeBaseRejection.recentlyDamaged(context, 1),
		"the dead victim's learned deadline persists without carrying damage into a live context");
	return failures == 0 ? 0 : 1;
}
