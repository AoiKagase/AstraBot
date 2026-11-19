#include "runtime/behavior/bot_behavior_runtime.hpp"
#include "astrabot/compat/follow_request.hpp"
#include "astrabot/compat/defend_request.hpp"
#include "astrabot/compat/defend_hiding_spots.hpp"
#include "runtime/shared/bot_runtime_support.hpp"
#include "runtime/shared/public_nav_probe.hpp"
#include <algorithm>
namespace astrabot
{
namespace metamod
{
namespace
{
bool carriesSniperRifle(const edict_t &entity)
{
	return (static_cast<unsigned>(entity.v.weapons) & compat::kSniperRifleInventoryMask) != 0;
}
constexpr int kTerroristTeam = 1;
constexpr int kCounterTerroristTeam = 2;
constexpr int kCommandRadioMenu = 1;
constexpr int kGroupRadioMenu = 2;
constexpr unsigned kVipMapPrefixLength = 3U;
constexpr int kMaximumClientSlots = 32;
std::uint32_t chatterPlacePhrase(const nav::NavSnapshot &navigation, const compat::ChatterRadioCatalog &catalog,
	std::uint16_t place)
{
	if (place && navigation.isValid())
		for (const auto &name : navigation.document()->places())
			if (name.id == place)
			{
				const auto id = catalog.nameToId(name.name);
				return id || catalog.hasPlaceBefore() ? id : compat::defaultChatterPlaceId(name.name);
			}
	return 0;
}
// Keep literal evaluation precision on x87; each macro is undefined below.
#define ASTRABOT_BOT_BEHAVIOR_RADIO_NEXT_PHRASE_GAP_SECONDS 0.1f
}
using namespace runtime_detail;

compat::RadioContactCounts BotBehaviorRuntime::livingRadioPeers(const edict_t *self, int ownTeam) const
{
	compat::RadioContactCounts counts{};
	for (int slot = 1; slot <= (std::min)(kMaximumClientSlots, services_->service_globals()->maxClients); ++slot)
	{
		auto *player = services_->service_engineFunctions()->pfnPEntityOfEntIndex(slot);
		if (!player || player == self || player->free || !(player->v.flags & FL_CLIENT) || player->serialnumber < 0 ||
			(player->v.flags & (FL_SPECTATOR | FL_DORMANT)) || player->v.deadflag != DEAD_NO || !(player->v.health > 0))
			continue;
		if (services_->observation_resolvePlayerTeam(player) == ownTeam)
			++counts.friends;
		else if (services_->observation_resolvePlayerTeam(player) == kTerroristTeam ||
			services_->observation_resolvePlayerTeam(player) == kCounterTerroristTeam)
			++counts.enemies;
	}
	return counts;
}

bool BotBehaviorRuntime::helpGoalCurrent(std::size_t index) const
{
	const auto &actor = actors_[index];
	const auto &goal = actor.helpGoal;
	return goal.active &&
		services_->service_compatibilitySurface().configuration().mode == compat::RuntimeMode::Compatibility &&
		goal.frame.mapGeneration == services_->service_lifecycle().mapGeneration() &&
		goal.frame.roundGeneration == services_->service_lifecycle().roundGeneration() &&
		actor.managedBotStateMachines.state() == compat::CompatibilityStateId::MoveTo &&
		goal.epoch == actor.managedBotStateMachines.normalStateEpoch();
}

struct BotBehaviorRuntime::ActorTaskEnvironment : compat::IHelpRequestEnvironment,
														compat::IFollowRequestEnvironment, compat::IDefendRequestEnvironment
{
	BotBehaviorRuntime &owner;
	std::size_t sender, receiver;
	std::function<bool()> sourceCurrent;
	ChatterPeerReceipt receipt;
	std::uint64_t execution, epoch;
	nav::NavSnapshot navigation;
	ActorTaskEnvironment(
		BotBehaviorRuntime &runtime, std::size_t from, std::size_t to, std::function<bool()> source)
		: owner(runtime), sender(from), receiver(to), sourceCurrent(std::move(source)),
		  receipt(runtime.snapshotChatterPeer(to)), execution(runtime.actors_[to].executionRevision),
		  epoch(runtime.actors_[to].managedBotStateMachines.normalStateEpoch()),
		  navigation(runtime.services_->service_navPublisher().snapshot())
	{
	}
	bool current() const override
	{
		if (!sourceCurrent())
			return false;
		const auto now = owner.snapshotChatterPeer(receiver);
		const auto nav = owner.services_->service_navPublisher().snapshot();
		// Speech admission is independent of execution ownership. Dead native bots
		// retain normal state and are valid recipients, although they cannot speak.
		return now.managed && receipt.entity == now.entity && receipt.actor == now.actor &&
			receipt.context.frame == now.context.frame && receipt.context.now == now.context.now &&
			receipt.context.alive == now.context.alive && receipt.serial == now.serial && !now.free &&
			receipt.flags == now.flags && receipt.name == now.name && receipt.connection == now.connection &&
			receipt.teamKnown == now.teamKnown && receipt.publicTeam == now.publicTeam &&
			receipt.entityTeam == now.entityTeam &&
			owner.receiptConnectionCurrent({receipt.actor.slot, receipt.connection}) &&
			owner.actors_[receiver].executionRevision == execution &&
			owner.actors_[receiver].managedBotStateMachines.normalStateEpoch() == epoch &&
			navigation.isValid() == nav.isValid() &&
			(!navigation.isValid() ||
				(navigation.revision() == nav.revision() && navigation.mapGeneration() == nav.mapGeneration()));
	}
	compat::RandomTimingContext timing() const
	{
		return {owner.services_->command_managedBotCommandSequences(receiver), 0U,
			owner.services_->service_managedBotFullUpdateSequences()[receiver]};
	}
	bool rogue(bool *value) override
	{
		compat::TeamworkInput input{};
		input.actor = receipt.actor;
		input.frame = receipt.context.frame;
		input.now = receipt.context.now;
		input.teamwork = static_cast<float>(owner.actors_[receiver].managedProfiles.teamwork) / 100.0f;
		const auto engine = owner.services_->service_engineFunctions();
		input.allowRoguesKnown =
			engine->pfnCVarGetPointer && engine->pfnCVarGetFloat && engine->pfnCVarGetPointer("bot_allow_rogues");
		if (!current())
			return false;
		const float enabled = readOptionalCvarFloat(engine, "bot_allow_rogues");
		if (!current())
			return false;
		input.allowRoguesKnown = input.allowRoguesKnown && std::isfinite(enabled);
		input.allowRogues = enabled != 0;
		const auto result = owner.actors_[receiver].teamworkPolicies.queryRogue(input,
			owner.services_->service_compatibilityRandomSource(), {input.actor.slot, input.actor.generation}, timing(),
			[this]
			{
				return current();
			});
		*value = result.rogue;
		return result.available && result.rogueKnown;
	}
	bool busy() override
	{
		const auto &machine = owner.actors_[receiver].managedBotStateMachines;
		return !machine.isInitialized() ||
			compat::nativeBotBusy(machine.state(), machine.task(), machine.attackOverlayActive());
	}
	bool travelDistanceToSender(float *distance) override
	{
		*distance = -1;
		if (!navigation.isValid())
			return true;
		const auto start = owner.services_->navigation_knownArea(
			receiver, receipt.actor, receipt.context.frame, navigation.revision());
		if (!start)
			return true;
		nav::NavVector point{};
		if (!senderPosition(&point))
			return false;
		const auto engine = owner.services_->service_engineFunctions();
		const auto pointCurrent = [&]()
		{
			if (!current() || owner.services_->service_engineFunctions() != engine)
				return false;
			const auto &origin = owner.services_->service_managedBotHandles()[sender].entity->v.origin;
			return origin.x == point.x && origin.y == point.y && origin.z == point.z;
		};
		PublicNavProbe probe(engine, pointCurrent);
		const auto goal = compat::findBotNavArea(navigation, point, &probe);
		if (!pointCurrent())
			return false;
		if (!goal)
			return true;
		nav::NavLadderTopology ladders;
		if (!owner.services_->navigation_ladderTopology(&ladders) || !pointCurrent())
			return false;
		nav::NavQuery query(navigation);
		query.setPathCostContext(owner.services_->navigation_managedPathCostContext(receiver));
		if (!pointCurrent())
			return false;
		const auto result = query.travelDistance(start, goal, nav::NavRouteType::Safest, ladders, distance);
		return result == nav::NavQueryResult::Found || result == nav::NavQueryResult::NoRoute;
	}
	bool senderPosition(nav::NavVector *point) override
	{
		if (!current())
			return false;
		const auto &origin = owner.services_->service_managedBotHandles()[sender].entity->v.origin;
		*point = {origin.x, origin.y, origin.z};
		return true;
	}
	bool squaredDistanceToSender(float *distance) override
	{
		nav::NavVector point{};
		if (!senderPosition(&point))
			return false;
		const auto &origin = receipt.entity->v.origin;
		const float dx = point.x - origin.x, dy = point.y - origin.y, dz = point.z - origin.z;
		*distance = dx * dx + dy * dy + dz * dz;
		return true;
	}
	std::uint16_t ownPlace() override
	{
		if (!navigation.isValid())
			return 0;
		const auto id = owner.services_->navigation_knownArea(
			receiver, receipt.actor, receipt.context.frame, navigation.revision());
		const auto area = navigation.document()->findArea(id);
		return area ? area->placeId : 0;
	}
	bool randomSpotAtPlace(std::uint16_t place, nav::NavVector *point, bool *found) override
	{
		const auto result =
			compat::chooseHelpPlaceSpot(navigation, place, owner.services_->service_compatibilityRandomSource(),
				{receipt.actor.slot, receipt.actor.generation}, timing(),
				[this]
				{
					return current();
				});
		*point = result.position;
		*found = result.found;
		return result.available;
	}
	bool moveTo(const nav::NavVector &point) override
	{
		if (!current())
			return false;
		auto &actor = owner.actors_[receiver];
		auto &machine = actor.managedBotStateMachines;
		runtime::MovementPhysicsState before{};
		before.dead = !receipt.context.alive;
		auto context = owner.makeManagedBotStateContext(receiver, before);
		context.preserveMoveToTask = true;
		actor.enemyReaction.onStateChange(
			owner.reactionContext(receiver), machine.state(), compat::CompatibilityStateId::MoveTo);
		const auto transitioned = machine.transition(compat::CompatibilityStateId::MoveTo,
			compat::StateTransitionReason::ObjectiveChanged, compat::StateTransitionId::ExplicitStateChange, context);
		epoch = machine.normalStateEpoch();
		if (transitioned != compat::StateResult::Transitioned ||
			machine.state() != compat::CompatibilityStateId::MoveTo || !current())
			return false;
		actor.managedBotCombat.stopMovement();
		actor.managedBotCombat.clearAim();
		actor.weaponActions.stopRapidFire();
		const auto generation = actor.managedBotObjectiveTargets.generation + 1U;
		actor.managedBotObjectiveTargets = {};
		actor.managedBotObjectiveActions = {};
		actor.scenarioDecisions = {};
		actor.noiseInvestigation.cancel();
		actor.investigationWalking = false;
		actor.helpGoal = {true, point, receipt.context.frame, epoch};
		auto &cache = actor.managedBotObjectiveTargets;
		cache.valid = true;
		cache.mapGeneration = receipt.context.frame.mapGeneration;
		cache.roundGeneration = receipt.context.frame.roundGeneration;
		cache.target = point;
		cache.generation = generation ? generation : 1U;
		cache.routeOverride = true;
		const auto task = machine.task();
		cache.routeType = task == compat::CompatibilityTaskId::FindTickingBomb ||
				task == compat::CompatibilityTaskId::DefuseBomb ||
				task == compat::CompatibilityTaskId::MoveToLastKnownEnemyPosition
			? nav::NavRouteType::Fastest
			: nav::NavRouteType::Safest;
		for (auto request : {NavigationRequestKind::ClearStep, NavigationRequestKind::DestroyPath,
				 NavigationRequestKind::ClearFailure, NavigationRequestKind::ResetProgress})
		{
			owner.services_->applyNavigationRequest({receiver, request});
			if (!current())
				return false;
		}
		return true;
	}
	bool followSender() override
	{
		if (!current())
			return false;
		const auto source = owner.snapshotChatterPeer(sender);
		if (!source.managed || !source.actor.isValid() || !source.entity || source.serial < 0 || !source.connection)
			return false;
		const TeammateFollow leader{
			source.actor, {source.actor.slot, static_cast<std::uint32_t>(source.serial) + 1U}, source.connection};
		auto &actor = owner.actors_[receiver];
		auto &machine = actor.managedBotStateMachines;
		runtime::MovementPhysicsState before{};
		before.dead = !receipt.context.alive;
		const auto context = owner.makeManagedBotStateContext(receiver, before);
		if (!current())
			return false;
		const auto previous = machine.state();
		const auto transitioned = machine.transition(compat::CompatibilityStateId::Follow,
			compat::StateTransitionReason::ObjectiveChanged, compat::StateTransitionId::ExplicitStateChange, context);
		epoch = machine.normalStateEpoch();
		if (transitioned != compat::StateResult::Transitioned || !current())
			return false;
		actor.scenarioPlanners.followTeammate(leader.entity, receipt.context.now);
		actor.teammateFollow = leader;
		actor.enemyReaction.onStateChange(
			owner.reactionContext(receiver), previous, compat::CompatibilityStateId::Follow);
		actor.managedBotCombat.stopMovement();
		actor.managedBotCombat.clearAim();
		actor.weaponActions.stopRapidFire();
		actor.helpGoal = {};
		actor.managedTaskEntity = {};
		actor.managedBotObjectiveTargets = {};
		actor.managedBotObjectiveActions = {};
		actor.scenarioDecisions = {};
		actor.noiseInvestigation.cancel();
		actor.investigationWalking = false;
		for (auto request : {NavigationRequestKind::ClearStep, NavigationRequestKind::DestroyPath,
				 NavigationRequestKind::ClearFailure, NavigationRequestKind::ResetProgress})
		{
			owner.services_->applyNavigationRequest({receiver, request});
			if (!current())
				return false;
		}
		return true;
	}
	nav::AreaId areaAt(const nav::NavVector &point, bool anyHeight)
	{
		PublicNavProbe probe(owner.services_->service_engineFunctions(), [this] { return current(); });
		return compat::findBotNavArea(navigation, point, &probe, anyHeight);
	}
	bool placeAt(const nav::NavVector &point, std::uint16_t *place) override
	{
		const auto id = areaAt(point, true);
		const auto *area = navigation.isValid() ? navigation.document()->findArea(id) : nullptr;
		*place = area ? area->placeId : 0;
		return current();
	}
	bool selectSpot(std::uint16_t place, nav::AreaId start, const nav::NavVector &origin,
		nav::NavVector *point, bool *found, float range = 750.0f, int retreatTeam = 0)
	{
		compat::HidingSpotOccupancy occupancy;
		const auto engine = owner.services_->service_engineFunctions();
		const auto globals = owner.services_->service_globals();
		std::vector<edict_t *> enemies;
		if (!engine->pfnPEntityOfEntIndex || !globals || (retreatTeam && !engine->pfnTraceLine))
			return false;
		for (int slot = 1; slot <= (std::min)(globals->maxClients, kMaximumClientSlots); ++slot)
		{
			auto *player = engine->pfnPEntityOfEntIndex(slot);
			if (!current())
				return false;
			if (!player || player->free || !(player->v.flags & FL_CLIENT) || (player->v.flags & FL_DORMANT) ||
				player->v.deadflag != DEAD_NO || player->v.health <= 0)
				continue;
			const int team = retreatTeam ? owner.services_->observation_resolvePlayerTeam(player) : 0;
			if (!current())
				return false;
			const auto &pos = player->v.origin;
			occupancy.players.push_back({{pos.x, pos.y, pos.z}, player == receipt.entity, team == retreatTeam});
			if (retreatTeam && team == (retreatTeam == kTerroristTeam ? kCounterTerroristTeam : kTerroristTeam))
				enemies.push_back(player);
		}
		if (engine->pfnFindEntityByString)
		{
			edict_t *previous = nullptr;
			for (int i = 0; i < globals->maxEntities; ++i)
			{
				auto *hostage = engine->pfnFindEntityByString(previous, "classname", "hostage_entity");
				if (!current())
					return false;
				if (!hostage || hostage == previous || hostage->free)
					break;
				const auto &pos = hostage->v.origin;
				occupancy.hostages.push_back({pos.x, pos.y, pos.z});
				previous = hostage;
			}
		}
		const compat::HidingSpotSearch search{navigation, occupancy, owner.services_->service_compatibilityRandomSource(),
			{receipt.actor.slot, receipt.actor.generation}, timing(), [this] { return current(); },
			carriesSniperRifle(*receipt.entity), range};
		compat::HidingSpotSelection selection;
		if (place)
			selection = search.atPlace(place);
		else
		{
			nav::NavLadderTopology ladders;
			if (!owner.services_->navigation_ladderTopology(&ladders) || !current())
				return false;
			if (retreatTeam)
			{
				selection = search.retreatNearArea(start, origin, ladders, [&](const nav::NavVector &spot)
				{
					constexpr float kHalfHumanHeight = 36.0f;
					constexpr int kIgnorePlayersAndGlass = 0x101;
					const float target[3] = {spot.x, spot.y, spot.z + kHalfHumanHeight};
					for (auto *enemy : enemies)
					{
						const auto &position = enemy->v.origin;
						const auto &offset = enemy->v.view_ofs;
						const float eye[3] = {position.x + offset.x, position.y + offset.y, position.z + offset.z};
						TraceResult trace{};
						engine->pfnTraceLine(eye, target, kIgnorePlayersAndGlass, enemy, &trace);
						if (!current() || trace.flFraction == 1.0f)
							return true;
					}
					return false;
				});
			}
			else
				selection = search.nearArea(start, origin, ladders);
		}
		*found = selection.found;
		*point = selection.position;
		return selection.available && current();
	}
	bool randomHidingSpot(std::uint16_t place, nav::NavVector *point, bool *found) override
	{
		return selectSpot(place, 0, {}, point, found);
	}
	bool drawFloat(const char *name, float low, float high, float &value) const
	{
		const auto result = owner.services_->service_compatibilityRandomSource().nextFloat(
			compat::RandomRequest::floatRequest(name, {receipt.actor.slot, receipt.actor.generation},
				timing(), low, high));
		value = result.value;
		return current() && result.status == compat::RandomStatus::Ok && std::isfinite(value) &&
			value >= low && value <= high;
	}
	bool enterHideState(const nav::NavVector &point, nav::AreaId goal, compat::CompatibilityTaskId task,
		float duration = -1.0f, bool holdForAmbush = false, bool keepFollowing = false)
	{
		if (!current())
			return false;
		auto &actor = owner.actors_[receiver];
		auto &machine = actor.managedBotStateMachines;
		const auto start = owner.services_->navigation_knownArea(receiver, receipt.actor, receipt.context.frame, navigation.revision());
		bool reachable = false;
		if (start && goal)
		{
			nav::NavLadderTopology ladders;
			if (!owner.services_->navigation_ladderTopology(&ladders) || !current())
				return false;
			nav::NavQuery query(navigation);
			query.setPathCostContext(owner.services_->navigation_managedPathCostContext(receiver));
			if (!current())
				return false;
			nav::NavCorridor corridor;
			reachable = query.buildNativeCorridor(start, goal, nav::NavRouteType::Fastest, ladders, &corridor) == nav::NavQueryResult::Found;
		}
		runtime::MovementPhysicsState before{};
		before.dead = !receipt.context.alive;
		auto context = owner.makeManagedBotStateContext(receiver, before);
		if (!current())
			return false;
		float ambushDelay = 0.0f;
		if (reachable)
		{
			float roll = 0.0f;
			if ((duration < 0.0f && !drawFloat("Hide.Duration", 30.0f, 60.0f, duration)) ||
				!drawFloat("Hide.Ambush", 0.0f, 100.0f, roll) ||
				((holdForAmbush || roll < 50.0f) && !drawFloat("Hide.HoldPositionTime", 3.0f, 10.0f, ambushDelay)))
				return false;
		}
		context.hideTask = reachable ? task : compat::CompatibilityTaskId::None;
		const auto next = reachable ? compat::CompatibilityStateId::Hide : compat::CompatibilityStateId::Idle;
		const auto previous = machine.state();
		const auto result = machine.transition(next, compat::StateTransitionReason::ObjectiveChanged,
			compat::StateTransitionId::ExplicitStateChange, context);
		epoch = machine.normalStateEpoch();
		if (result != compat::StateResult::Transitioned || !current())
			return false;
		actor.enemyReaction.onStateChange(owner.reactionContext(receiver), previous, next);
		actor.helpGoal = {};
		actor.managedBotObjectiveTargets = {};
		actor.managedBotObjectiveActions = {};
		actor.scenarioDecisions = {};
		owner.services_->applyNavigationRequest({receiver, NavigationRequestKind::DestroyPath});
		if (!current())
			return false;
		if (reachable)
		{
			compat::ScenarioTarget target{};
			target.knowledge = compat::ScenarioKnowledge::Remembered;
			target.frame = receipt.context.frame;
			target.position = {point.x, point.y, point.z};
			actor.scenarioPlanners.holdPosition(target, duration, ambushDelay, keepFollowing);
		}
		else
		{
			actor.scenarioPlanners.cancelHoldPosition();
		}
		return true;
	}
	bool retreatToCover(int team, float range, float duration, float ignoreDuration,
		bool holdForAmbush = false, bool keepFollowing = false)
	{
		if (!navigation.isValid() || !current())
			return false;
		const auto start = owner.services_->navigation_knownArea(
			receiver, receipt.actor, receipt.context.frame, navigation.revision());
		if (!start || !current())
			return false;
		const auto &body = receipt.entity->v.origin;
		nav::NavVector spot{};
		bool found = false;
		if (!selectSpot(0, start, {body.x, body.y, body.z}, &spot, &found, range, team) || !found)
			return false;
		if (duration < 0.0f && !drawFloat("Attack.RetreatHold", 3.0f, 15.0f, duration))
			return false;
		const auto goal = areaAt(spot, false);
		if (!current() || !enterHideState(spot, goal, compat::CompatibilityTaskId::None, duration, holdForAmbush, keepFollowing))
			return false;
		auto &actor = owner.actors_[receiver];
		const auto &machine = actor.managedBotStateMachines;
		actor.combatRetreat.hideEpoch = machine.state() == compat::CompatibilityStateId::Hide
			? machine.normalStateEpoch() : 0;
		if (actor.combatRetreat.hideEpoch)
			actor.enemyReaction.ignoreEnemies(owner.reactionContext(receiver), ignoreDuration);
		actor.managedBotCombat.stopMovement();
		actor.managedBotCombat.clearAim();
		actor.weaponActions.stopRapidFire();
		return true;
	}

	bool hideAtSpot(const nav::NavVector &point) override
	{
		return enterHideState(point, areaAt(point, false), compat::CompatibilityTaskId::HoldPosition);
	}
	bool hideNearPosition(const nav::NavVector &point) override
	{
		return hideNearArea(areaAt(point, false), compat::CompatibilityTaskId::HoldPosition);
	}
	bool hideNearArea(nav::AreaId start, compat::CompatibilityTaskId task, float range = 750.0f,
		float duration = -1.0f)
	{
		if (!current())
			return false;
		const auto &origin = receipt.entity->v.origin;
		const nav::NavVector self{origin.x, origin.y, origin.z};
		nav::NavVector center = self;
		if (start)
			center = nav::extentCenterOf(*navigation.document()->findArea(start));
		else
			start = owner.services_->navigation_knownArea(receiver, receipt.actor, receipt.context.frame, navigation.revision());
		if (!start)
			return enterHideState(self, 0, task, duration);
		nav::NavVector spot{};
		bool found = false;
		if (!selectSpot(0, start, center, &spot, &found, range))
			return false;
		if (!found)
			spot = self;
		// Hide(area) computes its route to the area directly beneath the chosen spot.
		return enterHideState(spot, compat::findBotNavArea(navigation, spot, nullptr), task, duration);
	}
	void affirmative() override
	{
		acknowledge(compat::ChatterPhrase::Affirmative);
	}
	void receivedMovingBomb(nav::NavVector position, int team)
	{
		auto &actor = owner.actors_[receiver];
		actor.bombStatus.rememberCarrier(position);
		bool isRogue = false;
		if (!rogue(&isRogue) || !current() || isRogue ||
			actor.managedBotStateMachines.state() != compat::CompatibilityStateId::Hunt)
			return;
		const auto living = owner.livingRadioPeers(receipt.entity, team);
		if (!current())
			return;
		const auto nearby = actor.outgoingRadioContactMemory.counts(receipt.actor, receipt.context.frame,
			receipt.context.now, living.friends, living.enemies);
		std::uint16_t place = 0;
		if (!nearby.enemies && placeAt(position, &place))
			(void)compat::respondToHelpRequest({place, -1.0f}, *this);
	}
	void receivedSiteStatus(const compat::ChatterMeme &meme)
	{
		const auto site = meme.site;
		if (!site.isValid() || site.id >= static_cast<unsigned>(owner.services_->service_globals()->maxEntities))
			return;
		const auto engine = owner.services_->service_engineFunctions();
		const auto *zone = engine->pfnPEntityOfEntIndex(static_cast<int>(site.id));
		if (!current() || !zone || zone->free || zone->serialnumber < 0 ||
			static_cast<unsigned>(zone->serialnumber) + 1U != site.generation)
			return;
		auto &actor = owner.actors_[receiver];
		if (meme.kind == compat::ChatterMemeKind::BombsitePlanted)
		{
			auto &known = actor.plantedBombsite;
			known.knowledge = compat::ScenarioKnowledge::Remembered;
			known.entity = site;
			known.frame = receipt.context.frame;
			known.position = {meme.position.x, meme.position.y, meme.position.z};
		}
		else
			actor.bombSearchMemory.clear(receipt.context.frame, site);
		if (meme.kind == compat::ChatterMemeKind::BombsitePlanted || actor.bombSearchTargets.entity == site)
			actor.bombSearchTargets = {};
		leaveTaskAndAcknowledge(compat::CompatibilityTaskId::FindTickingBomb);
	}
	void receivedHostageBeingTaken()
	{
		owner.actors_[receiver].someHostagesTaken = true;
		if (!busy())
			idleAndAcknowledge(compat::ChatterStatementTopic::MyIntention, true);
	}
	void leaveTaskAndAcknowledge(compat::CompatibilityTaskId task)
	{
		if (owner.actors_[receiver].managedBotStateMachines.task() == task)
			idleAndAcknowledge(compat::ChatterStatementTopic::Acknowledge,
				task == compat::CompatibilityTaskId::GuardBombZone);
	}
	void idleAndAcknowledge(compat::ChatterStatementTopic topic, bool cancelHold)
	{
		if (idleForObjectiveChange(cancelHold))
			acknowledge(compat::ChatterPhrase::Affirmative, topic);
	}
	bool idleForObjectiveChange(bool cancelHold)
	{
		auto &actor = owner.actors_[receiver];
		auto &machine = actor.managedBotStateMachines;
		runtime::MovementPhysicsState before{};
		before.dead = !receipt.context.alive;
		const auto context = owner.makeManagedBotStateContext(receiver, before);
		if (!current())
			return false;
		const auto previous = machine.state();
		const auto result = machine.transition(compat::CompatibilityStateId::Idle,
			compat::StateTransitionReason::ObjectiveChanged, compat::StateTransitionId::ExplicitStateChange, context);
		epoch = machine.normalStateEpoch();
		if (result != compat::StateResult::Transitioned || !current())
			return false;
		actor.enemyReaction.onStateChange(owner.reactionContext(receiver), previous, machine.state());
		if (cancelHold)
			actor.scenarioPlanners.cancelHoldPosition();
		actor.helpGoal = {};
		actor.managedBotObjectiveTargets = {};
		actor.managedBotObjectiveActions = {};
		actor.scenarioDecisions = {};
		owner.services_->applyNavigationRequest({receiver, NavigationRequestKind::DestroyPath});
		return current();
	}
	void onMyWay() override
	{
		acknowledge(compat::ChatterPhrase::OnMyWay);
	}
	void announcePlan(compat::ChatterPhrase phrase, std::uint16_t place)
	{
		if (owner.services_->service_roundOver())
			return;
		const auto delay = owner.services_->service_compatibilityRandomSource().nextFloat(compat::RandomRequest::floatRequest(
			"Chatter.AnnouncePlan", {receipt.actor.slot, receipt.actor.generation}, timing(), 2.0f, 3.0f));
		if (!current() || delay.status != compat::RandomStatus::Ok || !std::isfinite(delay.value) ||
			delay.value < 2.0f || delay.value > 3.0f)
			return;
		owner.ensureOutgoingChatterCatalog();
		if (!current())
			return;
		compat::ChatterStatementRequest plan{};
		plan.source = compat::ChatterScheduleSource::Generic;
		plan.topic = compat::ChatterStatementTopic::MyPlan;
		plan.phrase = phrase;
		plan.place = place;
		plan.criteriaPlace = chatterPlacePhrase(navigation, owner.outgoingChatterCatalog_, place);
		plan.created = receipt.context.now;
		const auto roundStart = owner.services_->service_roundFreezeGeneration() == receipt.context.frame.roundGeneration
			? owner.services_->service_roundFreezeUntil() : 0.0f;
		plan.start = roundStart + delay.value;
		plan.readyAt = plan.start;
		plan.expires = plan.created + 10.0f;
		admit(plan);
	}
	void reportHostageGuard(compat::CompatibilityTaskId task, bool atSpot, std::uint16_t place = 0)
	{
		if ((task != compat::CompatibilityTaskId::GuardHostages &&
			task != compat::CompatibilityTaskId::GuardHostageRescueZone) || !current() ||
			!receipt.context.alive || owner.services_->service_roundOver())
			return;
		auto &scheduler = owner.actors_[receiver].outgoingRadioSchedulers;
		if (!scheduler.beginHostageGuardReport(receipt.context.now))
			return;
		const bool hostage = task == compat::CompatibilityTaskId::GuardHostages;
		if (atSpot)
		{
			owner.ensureOutgoingChatterCatalog();
			acknowledge(hostage ? compat::ChatterPhrase::GuardingHostages :
				compat::ChatterPhrase::GuardingHostageEscapeZone);
		}
		else
			announcePlan(hostage ? compat::ChatterPhrase::GoingToGuardHostages :
				compat::ChatterPhrase::GoingToGuardHostageEscapeZone, place);
	}
	bool rulesOutVisibleFreeHostage(world::EntityKey following) const
	{
		const auto engine = owner.services_->service_engineFunctions();
		const auto &roster = owner.actors_[receiver].hostagePositions;
		if (!engine || !engine->pfnPEntityOfEntIndex || !engine->pfnTraceLine || roster.nextCheck < 0.0f)
			return false;
		const auto hostages = roster.entries;
		constexpr float kHostageViewHeight = 54.0f; // Three quarters of the native human hull height.
		const auto validPoint = [](const Vector &point) {
			constexpr float limit = world::WorldLimits::kMaximumCoordinate;
			return std::fabs(point.x) <= limit && std::fabs(point.y) <= limit && std::fabs(point.z) <= limit;
		};
		const auto observerOrigin = receipt.entity->v.origin;
		const auto viewOffset = receipt.entity->v.view_ofs;
		if (!validPoint(observerOrigin) || !validPoint(observerOrigin + viewOffset))
			return false;
		for (const auto &entry : hostages)
		{
			if (!entry.entity.isValid())
				break;
			if (entry.entity == following)
				continue;
			const auto *hostage = engine->pfnPEntityOfEntIndex(static_cast<int>(entry.entity.id));
			if (!current() || owner.services_->service_engineFunctions() != engine)
				return false;
			if (!hostage || hostage->free || hostage->serialnumber < 0 ||
				static_cast<unsigned>(hostage->serialnumber) + 1U != entry.entity.generation ||
				hostage->v.deadflag != DEAD_NO || !(hostage->v.health > 0.0f))
				continue;
			const auto origin = hostage->v.origin;
			const auto point = origin + Vector(0.0f, 0.0f, kHostageViewHeight);
			if (!validPoint(point))
				return false;
			// The public engine fields cannot rule out a visible hostage whose leader is unknown.
			if (owner.canSeePoint(receiver, {point.x, point.y, point.z}, receipt.context.frame, false) ||
				!current() || owner.services_->service_engineFunctions() != engine || hostage->free ||
				hostage->serialnumber < 0 || static_cast<unsigned>(hostage->serialnumber) + 1U != entry.entity.generation ||
				hostage->v.origin != origin || receipt.entity->v.origin != observerOrigin ||
				receipt.entity->v.view_ofs != viewOffset)
				return false;
		}
		return true;
	}

	const nav::NavArea *randomRescueArea()
	{
		const auto engine = owner.services_->service_engineFunctions();
		const auto globals = owner.services_->service_globals();
		if (!navigation.isValid() || !engine || !globals || !engine->pfnPEntityOfEntIndex || !engine->pfnSzFromIndex)
			return nullptr;
		constexpr std::size_t kMaximumZones = 4, kMaximumZoneAreas = 16;
		std::array<nav::NavExtent, kMaximumZones> explicitZones{}, spawnZones{};
		std::size_t explicitCount = 0, spawnCount = 0;
		const int limit = (std::min)(globals->maxEntities, 2048);
		for (int id = globals->maxClients + 1; id < limit && explicitCount < explicitZones.size(); ++id)
		{
			const auto *entity = engine->pfnPEntityOfEntIndex(id);
			if (!current())
				return nullptr;
			if (!entity || entity->free)
				continue;
			const auto serial = entity->serialnumber;
			const auto classname = entity->v.classname;
			const char *name = engine->pfnSzFromIndex(classname);
			if (!current())
				return nullptr;
			if (!name || entity->free || entity->serialnumber != serial || entity->v.classname != classname)
				continue;
			const auto origin = entity->v.origin;
			const nav::NavExtent legacy{{origin.x - 256.0f, origin.y - 256.0f, origin.z - 306.0f},
				{origin.x + 256.0f, origin.y + 256.0f, origin.z + 306.0f}};
			if (std::strcmp(name, "func_hostage_rescue") == 0)
				explicitZones[explicitCount++] = {{entity->v.absmin.x, entity->v.absmin.y, entity->v.absmin.z - 50.0f},
					{entity->v.absmax.x, entity->v.absmax.y, entity->v.absmax.z + 50.0f}};
			else if (std::strcmp(name, "info_hostage_rescue") == 0)
				explicitZones[explicitCount++] = legacy;
			else if (spawnCount < spawnZones.size() && std::strcmp(name, "info_player_start") == 0)
				spawnZones[spawnCount++] = legacy;
		}
		const auto count = explicitCount ? explicitCount : spawnCount;
		if (!count)
			return nullptr;
		auto &random = owner.services_->service_compatibilityRandomSource();
		const compat::RandomActor actor{receipt.actor.slot, receipt.actor.generation};
		const auto zone = random.nextLong(compat::RandomRequest::longRequest("Hostage.RandomRescueZone", actor, timing(),
			0, static_cast<std::int32_t>(count) - 1));
		if (!current() || zone.status != compat::RandomStatus::Ok || zone.value < 0 ||
			static_cast<unsigned>(zone.value) >= count)
			return nullptr;
		const auto &zones = explicitCount ? explicitZones : spawnZones;
		const auto *area = compat::randomBombSiteArea(*navigation.document(), zones[zone.value], kMaximumZoneAreas,
			random, actor, timing(), "Hostage.RandomRescueArea");
		return current() ? area : nullptr;
	}
	const nav::NavArea *randomSiteArea()
	{
		owner.refreshManagedObjectiveSiteRegistry();
		const auto &zones = owner.managedObjectiveSiteRegistry_;
		if (!current() || !zones.registeredCount)
			return nullptr;
		auto &random = owner.services_->service_compatibilityRandomSource();
		const compat::RandomActor actor{receipt.actor.slot, receipt.actor.generation};
		const auto zone = random.nextLong(compat::RandomRequest::longRequest("Bomb.RandomZone", actor, timing(),
			0, static_cast<std::int32_t>(zones.registeredCount) - 1));
		if (!current() || zone.status != compat::RandomStatus::Ok || zone.value < 0 ||
			static_cast<unsigned>(zone.value) >= zones.registeredCount)
			return nullptr;
		const auto *area = compat::randomBombSiteArea(*navigation.document(), zones.entries[zone.value].zone.extent,
			compat::BombSearchSite::kMaximumNavAreas, random, actor, timing());
		return current() ? area : nullptr;
	}
	void coveringFriend() override
	{
		acknowledge(compat::ChatterPhrase::CoveringFriend);
	}
	void say(const char *name, float lifetime = 3.0f, float delay = 0.0f)
	{
		owner.ensureOutgoingChatterCatalog();
		if (!current())
			return;
		if (const auto *phrase = owner.outgoingChatterCatalog_.findPhrase(name))
			admit(compat::makeSayStatement(*phrase, receipt.context.now, lifetime, delay));
	}
	void acknowledge(compat::ChatterPhrase phrase,
		compat::ChatterStatementTopic topic = compat::ChatterStatementTopic::MyIntention)
	{
		if (topic == compat::ChatterStatementTopic::MyIntention)
		{
			say(compat::ChatterRadioCatalog::name(phrase));
			return;
		}
		compat::ChatterStatementRequest request{};
		request.source = compat::ChatterScheduleSource::Generic;
		request.topic = topic;
		request.phrase = phrase;
		request.created = receipt.context.now;
		request.start = request.created;
		request.readyAt = request.created;
		request.expires = request.created + 3.0f;
		admit(request);
	}
	void reportPlantedSite()
	{
		const auto known = owner.actors_[receiver].plantedBombsite;
		if (!known.entity.isValid() || known.frame.mapGeneration != receipt.context.frame.mapGeneration ||
			known.frame.roundGeneration != receipt.context.frame.roundGeneration)
			return;
		const nav::NavVector center{known.position.x, known.position.y, known.position.z};
		std::uint16_t place = 0;
		if (!placeAt(center, &place))
			return;
		auto report = compat::plantedBombsiteReport(receipt.context.now, known.entity, place,
			chatterPlacePhrase(navigation, owner.outgoingChatterCatalog_, place));
		report.meme.position = center;
		admit(report);
	}
	void reportingIn(int team)
	{
		const auto place = ownPlace();
		if (!current())
			return;
		const auto living = owner.livingRadioPeers(receipt.entity, team);
		if (!current())
			return;
		const auto &actor = owner.actors_[receiver];
		const auto &context = receipt.context;
		const auto nearby = actor.outgoingRadioContactMemory.counts(receipt.actor, context.frame, context.now,
			living.friends, living.enemies);
		nav::NavVector bomb{};
		const auto *guarded = owner.guardedBombPosition(receiver, &bomb) ? &bomb : nullptr;
		if (!current())
			return;
		reportHostageGuard(actor.managedBotStateMachines.task(), actor.scenarioPlanners.atDefenseSpot());
		admit(compat::reportingInStatement(context.now, actor.managedBotStateMachines.attackOverlayActive(),
			nearby, actor.enemyReaction.enemyDeathTimestamp(context), context.now - actor.lastVisibleEnemyTimes,
			place, chatterPlacePhrase(navigation, owner.outgoingChatterCatalog_, place), guarded), true);
	}
	void admit(const compat::ChatterStatementRequest &request, bool reportingIn = false)
	{
		if (!current() || !receipt.context.alive)
			return;
		const auto engine = owner.services_->service_engineFunctions();
		const char *verbosity = engine->pfnCVarGetString ? engine->pfnCVarGetString("bot_chatter") : nullptr;
		const auto mode = compat::parseChatterVerbosity(verbosity);
		if (!current())
			return;
		const auto admission = owner.chatterAdmission(receiver);
		if (!current())
			return;
		auto &actor = owner.actors_[receiver];
		if (reportingIn)
		{
			compat::OutgoingRadioInput input{};
			input.admission = admission;
			input.verbosity = mode;
			input.speaking = actor.confirmedSpeech.scheduledStatement();
			input.now = receipt.context.now;
			input.roundOver = owner.services_->service_roundOver();
			actor.outgoingRadioSchedulers.reportIn(request, actor.managedBotStateMachines.task(), input,
				owner.outgoingChatterCatalog_);
			return;
		}
		actor.outgoingRadioSchedulers.statements().admit(
			request, admission, owner.outgoingChatterCatalog_, mode, actor.confirmedSpeech.scheduledStatement());
	}
};

bool BotBehaviorRuntime::updateCombatRetreat(std::size_t index, int team)
{
	auto &actor = actors_[index];
	auto &retreat = actor.combatRetreat;
	auto &machine = actor.managedBotStateMachines;
	if (!machine.attackOverlayActive() || (team != kTerroristTeam && team != kCounterTerroristTeam) ||
		services_->service_compatibilitySurface().configuration().mode != compat::RuntimeMode::Compatibility)
		return false;
	const auto update = services_->service_managedBotFullUpdateSequences()[index];
	const auto attackEpoch = machine.attackEntryEpoch();
	if (retreat.attackEpoch == attackEpoch && retreat.updateSequence == update)
		return false;

	ActorTaskEnvironment env(*this, index, index, [&] {
		return machine.attackEntryEpoch() == attackEpoch;
	});
	if (!env.current() || !env.receipt.context.alive)
		return false;
	const float now = env.receipt.context.now;
	if (retreat.attackEpoch != attackEpoch)
	{
		float pinned = 0;
		float fear = 0;
		if (!env.drawFloat("Attack.PinnedDown", 7.0f, 10.0f, pinned) ||
			!env.drawFloat("Attack.Coward", 0.0f, 100.0f, fear))
			return false;
		retreat.attackEpoch = attackEpoch;
		retreat.pinnedAfter = now + pinned;
		retreat.coward = fear > static_cast<float>(actor.managedProfiles.aggression);
	}
	retreat.updateSequence = update;
	if (now <= retreat.retryAfter)
		return false;
	const bool pinned = now > retreat.pinnedAfter;
	if (!pinned)
	{
		if (!retreat.coward && actor.managedProfiles.aggression >= 100)
			return false;
		const auto living = livingRadioPeers(env.receipt.entity, team);
		if (!env.current())
			return false;
		const auto nearby = actor.outgoingRadioContactMemory.counts(env.receipt.actor, env.receipt.context.frame,
			now, living.friends, living.enemies);
		const int excessEnemies = nearby.enemies - nearby.friends - 1;
		if (excessEnemies < (retreat.coward ? 1 : 2))
			return false;
	}

	compat::OutgoingRadioInput speech{};
	speech.now = now;
	speech.ownPlace = env.ownPlace();
	speech.ownPlacePhrase = chatterPlacePhrase(env.navigation, outgoingChatterCatalog_, speech.ownPlace);
	const auto engine = services_->service_engineFunctions();
	speech.verbosity = compat::parseChatterVerbosity(
		engine->pfnCVarGetString ? engine->pfnCVarGetString("bot_chatter") : nullptr);
	if (!env.current())
		return false;
	speech.admission = chatterAdmission(index);
	if (!env.current())
		return false;
	speech.speaking = actor.confirmedSpeech.scheduledStatement();
	actor.outgoingRadioSchedulers.reportRetreat(pinned, speech, outgoingChatterCatalog_);

	float retry = 0;
	if (!env.drawFloat("Attack.RetreatRetry", 3.0f, 15.0f, retry))
		return false;
	retreat.retryAfter = now + retry;
	return env.retreatToCover(team, 1000.0f, -1.0f, 10.0f);
}

bool BotBehaviorRuntime::respondToFallBack(std::size_t index, int team, const compat::ScenarioFallBackResponse &response)
{
	const auto engine = services_->service_engineFunctions();
	if ((team != kTerroristTeam && team != kCounterTerroristTeam) || !engine || !engine->pfnPEntityOfEntIndex)
		return false;
	const auto sender = response.sender;
	const auto senderConnection = services_->service_lifecycle().slotGeneration(sender.slot);
	const auto senderCurrent = [&] {
		const auto *human = engine->pfnPEntityOfEntIndex(static_cast<int>(sender.slot));
		return human && !human->free && (human->v.flags & FL_CLIENT) != 0 &&
			(human->v.flags & FL_FAKECLIENT) == 0 && human->serialnumber >= 0 &&
			static_cast<std::uint32_t>(human->serialnumber) == sender.generation &&
			services_->observation_resolvePlayerTeam(human) == team &&
			receiptConnectionCurrent({sender.slot, senderConnection});
	};
	ActorTaskEnvironment env(*this, index, index, senderCurrent);
	if (!env.current() || !env.receipt.context.alive || env.busy())
		return false;
	if (response.decline)
	{
		env.acknowledge(compat::ChatterPhrase::Negative, compat::ChatterStatementTopic::Acknowledge);
		return false;
	}
	if (!env.retreatToCover(team, 1000.0f, -1.0f, 10.0f, false, true))
		return false;
	env.acknowledge(compat::ChatterPhrase::Affirmative, compat::ChatterStatementTopic::Acknowledge);
	if (env.current())
	{
		auto &actor = actors_[index];
		actor.combatRetreat.fallBackEquipEpoch = actor.managedBotStateMachines.normalStateEpoch();
	}
	return true;
}

bool BotBehaviorRuntime::tryCombatAmbush(std::size_t index, int team)
{
	if (team != kTerroristTeam && team != kCounterTerroristTeam)
		return false;
	const auto &machine = actors_[index].managedBotStateMachines;
	const auto epoch = machine.attackEntryEpoch();
	ActorTaskEnvironment env(*this, index, index, [&] { return machine.attackEntryEpoch() == epoch; });
	float roll = 0.0f;
	float duration = 0.0f;
	if (!env.current() || !env.receipt.context.alive ||
		!env.drawFloat("Attack.AmbushChance", 0.0f, 100.0f, roll) || roll >= 33.3f ||
		!env.drawFloat("Attack.AmbushDuration", 3.0f, 15.0f, duration))
		return false;
	return env.retreatToCover(team, 200.0f, duration, 1.0f, true);
}

bool BotBehaviorRuntime::tryReloadCover(std::size_t index, int team, int remainingEnemies, float timeSinceEnemy)
{
	auto &actor = actors_[index];
	if (actor.managedBotStateMachines.state() == compat::CompatibilityStateId::Hide)
		return false;
	const auto receipt = reactionContext(index);
	if (!actor.outgoingRadioContactMemory.counts(receipt.actor, receipt.frame, receipt.now, 0, remainingEnemies).enemies)
		return false;
	ActorTaskEnvironment env(*this, index, index, [] { return true; });
	const float chance = 25.0f + static_cast<float>(actor.managedProfiles.skill);
	float roll = 0;
	if (!env.current() || !env.drawFloat("Reload.HideChance", 0.0f, 100.0f, roll) ||
		roll >= chance || timeSinceEnemy >= 5.0f)
		return false;
	return env.retreatToCover(team, 1000.0f, 0.0f, 10.0f);
}

bool BotBehaviorRuntime::selectHostageDefense(std::size_t index, bool rogue, const compat::ScenarioBehaviorInput &scenario)
{
	ActorTaskEnvironment env(*this, index, index, [] { return true; });
	if (!env.current() || !env.navigation.isValid() ||
		!services_->navigation_knownArea(index, env.receipt.actor, scenario.frame, env.navigation.revision()))
		return false;
	const auto &actor = actors_[index];
	const auto start = services_->service_roundFreezeGeneration() == scenario.frame.roundGeneration
		? services_->service_roundFreezeUntil() : 0.0f;
	const float elapsed = scenario.timestamp - start;
	const float safeTime = actor.enemyReaction.safeTime(scenario.aggression);
	const auto draw = [&](const char *name, float &value) {
		const auto result = services_->service_compatibilityRandomSource().nextFloat(compat::RandomRequest::floatRequest(
			name, {env.receipt.actor.slot, env.receipt.actor.generation}, env.timing(), 0.0f, 100.0f));
		value = result.value;
		return env.current() && result.status == compat::RandomStatus::Ok && value >= 0.0f && value <= 100.0f;
	};
	float roll = 0;
	bool campHostages = elapsed < safeTime;
	const auto &hostages = actor.hostagePositions;
	const bool allBeingRescued = !hostages.hasKnownPosition() &&
		std::any_of(hostages.entries.begin(), hostages.entries.end(), [](const RememberedHostage &entry) {
			return entry.alive;
		});
	if (!campHostages && !actor.someHostagesTaken && !allBeingRescued)
	{
		if (!draw("Idle.HostageCampZone", roll))
			return false;
		campHostages = roll > 100.0f * (elapsed - safeTime) / 120.0f;
	}
	const auto randomHostage = [&](nav::NavVector &position) {
		std::array<nav::NavVector, HostagePositionMemory::kMaximumHostages> known{};
		std::size_t count = 0;
		for (const auto &entry : actor.hostagePositions.entries)
			if (entry.valid)
				known[count++] = {entry.position.x, entry.position.y, entry.position.z};
		if (!count)
			return false;
		const auto random = services_->service_compatibilityRandomSource().nextLong(compat::RandomRequest::longRequest(
			"Hostage.RandomFreePosition", {env.receipt.actor.slot, env.receipt.actor.generation},
			env.timing(), 0, static_cast<std::int32_t>(count) - 1));
		if (!env.current() || random.status != compat::RandomStatus::Ok || random.value < 0 ||
			static_cast<unsigned>(random.value) >= count)
			return false;
		position = known[random.value];
		return true;
	};
	if (carriesSniperRifle(*env.receipt.entity))
	{
		if (!draw("Idle.DefenseSniperCamp", roll))
			return false;
		if (roll <= 75.0f)
		{
			nav::NavVector position{};
			if (randomHostage(position) && campHostages)
			{
				const auto area = env.areaAt(position, false);
				return env.current() && env.hideNearArea(area, compat::CompatibilityTaskId::MoveToSniperSpot, 2000.0f);
			}
			if (const auto *area = env.randomRescueArea())
				return env.hideNearArea(area->id, compat::CompatibilityTaskId::MoveToSniperSpot, 2000.0f);
		}
	}
	if (!env.current())
		return false;
	const auto peers = livingRadioPeers(env.receipt.entity, kTerroristTeam);
	if (!env.current())
		return false;
	const int morale = services_->service_excellentMorale() ? 3 : actor.morale.value;
	if (peers.friends && (rogue || roundPlantDecision_.defense == objectives::DefenseStrategy::Rush ||
		!draw("Idle.HostageHunt", roll) || roll < 70.0f + 25.0f * morale))
		return false;

	nav::NavVector position{};
	if (randomHostage(position) && campHostages)
	{
		const auto areaId = env.areaAt(position, false);
		const auto *area = env.navigation.document()->findArea(areaId);
		if (!env.current())
			return false;
		if (area)
		{
			if (!env.hideNearArea(areaId, compat::CompatibilityTaskId::GuardHostages, 750.0f + 250.0f * (morale + 3)))
				return false;
			if (draw("Idle.HostageGuardPlan", roll) && roll < 50.0f)
				env.reportHostageGuard(compat::CompatibilityTaskId::GuardHostages, false, area->placeId);
			return true;
		}
	}
	return env.current() && renewHostageRescueGuard(index);
}

bool BotBehaviorRuntime::renewHostageRescueGuard(std::size_t index)
{
	ActorTaskEnvironment env(*this, index, index, [] { return true; });
	if (!env.current())
		return false;
	const auto *area = env.randomRescueArea();
	if (!area || !env.hideNearArea(area->id, compat::CompatibilityTaskId::GuardHostageRescueZone))
		return false;
	env.reportHostageGuard(compat::CompatibilityTaskId::GuardHostageRescueZone, false);
	return true;
}

bool BotBehaviorRuntime::selectPreplantDefense(std::size_t index, bool cooperative, const compat::ScenarioBehaviorInput &scenario)
{
	const auto &actor = actors_[index];
	const auto *self = services_->service_managedBotHandles()[index].entity;
	const bool sniper = self && carriesSniperRifle(*self);
	const bool sawDrop = actor.looseBombObservation.knowledge == compat::ScenarioKnowledge::Visible;
	const bool knowsDrop = sawDrop || actor.bombStatus.status == compat::BombStatusMemory::Status::Loose;
	const float start = services_->service_roundFreezeGeneration() == scenario.frame.roundGeneration
		? services_->service_roundFreezeUntil() : 0.0f;
	const float lastSawEnemy = (std::max)(0.0f, actor.enemyReaction.lastSaw(reactionContext(index)));
	const bool mayGuard = cooperative && !knowsDrop && roundPlantDecision_.defense == objectives::DefenseStrategy::Hold &&
		(actor.enemyReaction.isSafe(scenario.timestamp - start, scenario.aggression) ||
			scenario.timestamp - lastSawEnemy > 30.0f);
	if (!sniper && !mayGuard)
		return false;
	ActorTaskEnvironment env(*this, index, index, [] { return true; });
	if (!env.current() || !env.navigation.isValid() ||
		!services_->navigation_knownArea(index, env.receipt.actor, scenario.frame, env.navigation.revision()))
		return false;
	const auto draw = [&](const char *name) {
		return services_->service_compatibilityRandomSource().nextFloat(compat::RandomRequest::floatRequest(
			name, {env.receipt.actor.slot, env.receipt.actor.generation}, env.timing(), 0.0f, 100.0f));
	};
	const auto valid = [](const compat::RandomFloatResult &r) {
		return r.status == compat::RandomStatus::Ok && std::isfinite(r.value) && r.value >= 0 && r.value <= 100;
	};
	if (sniper)
	{
		const auto roll = draw("Idle.DefenseSniperCamp");
		if (!env.current() || !valid(roll))
			return false;
		if (roll.value <= 75.0f)
		{
			nav::AreaId area = 0;
			if (knowsDrop)
			{
				const auto &pos = actor.looseBombObservation.position;
				area = env.areaAt(sawDrop ? nav::NavVector{pos.x, pos.y, pos.z} : actor.bombStatus.position, false);
			}
			else if (const auto *site = env.randomSiteArea())
				area = site->id;
			if (!env.current())
				return false;
			if (area)
				return env.hideNearArea(area, compat::CompatibilityTaskId::MoveToSniperSpot, 2000.0f);
		}
	}
	if (!mayGuard)
		return false;
	const int morale = services_->service_excellentMorale() ? 3 : actor.morale.value;
	const auto roll = draw("Idle.GuardBombsite");
	if (!env.current() || !valid(roll) || roll.value >= -34.0f * morale)
		return false;
	const auto *area = env.randomSiteArea();
	if (!area)
		return false;
	env.announcePlan(compat::ChatterPhrase::GoingToDefendBombsite, area->placeId);
	return env.current() && env.hideNearArea(area->id, compat::CompatibilityTaskId::GuardBombZone,
		500.0f + 100.0f * (morale + 3));
}

bool BotBehaviorRuntime::renewBombsiteGuard(std::size_t index)
{
	ActorTaskEnvironment env(*this, index, index, [] { return true; });
	if (!env.current() || !env.navigation.isValid())
		return false;
	refreshManagedObjectiveSiteRegistry();
	if (!env.current())
		return false;
	const auto pos = env.receipt.entity->v.origin;
	const compat::BombSearchSite *nearest = nullptr;
	float closestSquared = 1e9f;
	for (std::size_t i = 0; i < managedObjectiveSiteRegistry_.registeredCount; ++i)
	{
		const auto &zone = managedObjectiveSiteRegistry_.entries[i].zone;
		const float dx = zone.center.x - pos.x, dy = zone.center.y - pos.y, dz = zone.center.z - pos.z;
		const float squared = dx * dx + dy * dy + dz * dz;
		if (squared < closestSquared)
		{
			closestSquared = squared;
			nearest = &zone;
		}
	}
	if (!nearest)
		return false;
	const auto *area = compat::randomBombSiteArea(*env.navigation.document(), nearest->extent,
		compat::BombSearchSite::kMaximumNavAreas, services_->service_compatibilityRandomSource(),
		{env.receipt.actor.slot, env.receipt.actor.generation}, env.timing());
	return env.current() && area && env.hideNearArea(area->id, compat::CompatibilityTaskId::GuardBombZone);
}

void BotBehaviorRuntime::hideAtPlantedBombsite(std::size_t index, compat::ScenarioTarget target)
{
	const auto engine = services_->service_engineFunctions();
	const auto globals = services_->service_globals();
	const auto site = target.entity;
	const edict_t *entity = nullptr;
	const auto sameWorld = [&] {
		const auto &life = services_->service_lifecycle();
		return engine == services_->service_engineFunctions() && globals == services_->service_globals() &&
			target.frame.mapGeneration == life.mapGeneration() && target.frame.roundGeneration == life.roundGeneration();
	};
	const auto current = [&] {
		return sameWorld() && (!site.isValid() || (entity && !entity->free && entity->serialnumber >= 0 &&
			static_cast<unsigned>(entity->serialnumber) + 1U == site.generation));
	};
	ActorTaskEnvironment env(*this, index, index, current);
	if (!sameWorld())
		return;
	if (site.isValid())
	{
		if (!engine || !engine->pfnPEntityOfEntIndex || !globals || globals->maxEntities <= 0 ||
			site.id >= static_cast<unsigned>(globals->maxEntities))
			return;
		entity = engine->pfnPEntityOfEntIndex(static_cast<int>(site.id));
	}
	if (!env.current())
		return;
	const nav::NavVector point{target.position.x, target.position.y, target.position.z};
	env.hideNearArea(compat::findBotNavArea(env.navigation, point, nullptr), compat::CompatibilityTaskId::GuardBombZone);
}

void BotBehaviorRuntime::hideNearLooseBomb(std::size_t index, bool announce)
{
	const auto bomb = actors_[index].looseBombObservation;
	const auto sameDrop = [&] {
		return bomb.entity.isValid() ? looseBombDrop_.matches(bomb.entity, bomb.frame) : !looseBombDrop_.box.isValid();
	};
	ActorTaskEnvironment env(*this, index, index, sameDrop);
	if (!env.current() || !(bomb.frame == env.receipt.context.frame))
		return;
	const auto area = looseBombArea(env.navigation, [&env] { return env.current(); });
	if (!env.hideNearArea(area, compat::CompatibilityTaskId::GuardLooseBomb))
		return;
	if (announce && area)
		env.announcePlan(compat::ChatterPhrase::GoingToGuardLooseBomb, env.navigation.document()->findArea(area)->placeId);
}

void BotBehaviorRuntime::transmitTeammateRequest(
	std::size_t sender, compat::ChatterMeme meme, const std::function<bool()> &current)
{
	if ((meme.kind != compat::ChatterMemeKind::Help && meme.kind != compat::ChatterMemeKind::Follow &&
		 meme.kind != compat::ChatterMemeKind::DefendHere && meme.kind != compat::ChatterMemeKind::RequestReport &&
		 meme.kind != compat::ChatterMemeKind::BombsiteClear && meme.kind != compat::ChatterMemeKind::BombsitePlanted &&
		 meme.kind != compat::ChatterMemeKind::WhereBomb && meme.kind != compat::ChatterMemeKind::BombMoving &&
		 meme.kind != compat::ChatterMemeKind::BombLoose && meme.kind != compat::ChatterMemeKind::HostageBeingTaken &&
		 meme.kind != compat::ChatterMemeKind::WarnSniper) ||
		services_->service_compatibilitySurface().configuration().mode != compat::RuntimeMode::Compatibility)
		return;
	const auto engine = services_->service_engineFunctions();
	const auto source = snapshotChatterPeer(sender);
	const auto execution = actors_[sender].executionRevision;
	const auto sourceCurrent = [&]()
	{
		if (!current() || services_->service_engineFunctions() != engine ||
			actors_[sender].executionRevision != execution)
			return false;
		const auto live = snapshotChatterPeer(sender);
		return source.managed && live.managed && source.entity == live.entity && source.actor == live.actor &&
			source.serial == live.serial && source.connection == live.connection && source.flags == live.flags &&
			source.name == live.name && source.teamKnown == live.teamKnown && source.publicTeam == live.publicTeam &&
			source.entityTeam == live.entityTeam;
	};
	const int team = services_->observation_resolvePlayerTeam(source.entity);
	if (!sourceCurrent())
		return;
	const int maximum = (std::min)(kMaximumClientSlots, services_->service_globals()->maxClients);
	for (int slot = 1; slot <= maximum; ++slot)
	{
		if (!sourceCurrent())
			return;
		const auto receiver = static_cast<std::size_t>(slot - 1);
		if (receiver == sender || !services_->service_managedBotSlots()[receiver])
			continue;
		ActorTaskEnvironment environment(*this, sender, receiver, sourceCurrent);
		const auto &receipt = environment.receipt;
		auto *entity = engine->pfnPEntityOfEntIndex(slot);
		if (!environment.current())
		{
			if (!sourceCurrent())
				return;
			continue;
		}
		if (!entity || entity != receipt.entity || entity->free || receipt.serial < 0 ||
			!(entity->v.flags & FL_CLIENT) || (entity->v.flags & FL_DORMANT) || !entity->v.netname)
			continue;
		if (engine->pfnSzFromIndex)
		{
			const auto name = engine->pfnSzFromIndex(entity->v.netname);
			if (!environment.current())
			{
				if (!sourceCurrent())
					return;
				continue;
			}
			if (!name || !*name)
				continue;
		}
		const int recipientTeam = services_->observation_resolvePlayerTeam(entity);
		if (!environment.current())
		{
			if (!sourceCurrent())
				return;
			continue;
		}
		if (recipientTeam != team)
			continue;
		if (meme.kind == compat::ChatterMemeKind::Help)
			(void)compat::respondToHelpRequest({meme.place, 3000.0f}, environment);
		else if (meme.kind == compat::ChatterMemeKind::Follow)
			(void)compat::respondToFollowRequest(environment);
		else if (meme.kind == compat::ChatterMemeKind::DefendHere)
			(void)compat::respondToDefendRequest(meme.position, environment);
		else if (meme.kind == compat::ChatterMemeKind::BombsiteClear || meme.kind == compat::ChatterMemeKind::BombsitePlanted)
			environment.receivedSiteStatus(meme);
		else if (meme.kind == compat::ChatterMemeKind::WhereBomb)
			environment.reportPlantedSite();
		else if (meme.kind == compat::ChatterMemeKind::BombMoving)
			environment.receivedMovingBomb(meme.position, team);
		else if (meme.kind == compat::ChatterMemeKind::WarnSniper)
			actors_[receiver].sniperWarning.warnedUntil = receipt.context.now + 60.0f;
		else if (meme.kind == compat::ChatterMemeKind::HostageBeingTaken)
			environment.receivedHostageBeingTaken();
		else if (meme.kind == compat::ChatterMemeKind::BombLoose)
		{
			actors_[receiver].bombStatus.rememberLoose(meme.position);
			environment.leaveTaskAndAcknowledge(compat::CompatibilityTaskId::GuardBombZone);
		}
		else
			environment.reportingIn(team);
	}
}

compat::ChatterScheduleAdmission BotBehaviorRuntime::chatterAdmission(std::size_t index)
{
	const auto context = reactionContext(index);
	if (!context.identityValid())
		return {};
	auto &actor = actors_[index];
	actor.confirmedSpeech.synchronizeOwner(context);
	return {context, services_->service_lifecycle().slotGeneration(context.actor.slot),
		actor.confirmedSpeech.lifetimeRevision(), &actor.chatterAdmissions};
}

BotBehaviorRuntime::ChatterPeerReceipt BotBehaviorRuntime::snapshotChatterPeer(std::size_t index) const
{
	const auto &handles = services_->service_managedBotHandles();
	constexpr int kEligibilityFlags = FL_CLIENT | FL_DORMANT | FL_SPECTATOR;
	ChatterPeerReceipt receipt{};
	receipt.managed = services_->service_managedBotSlots()[index];
	if (!receipt.managed)
	{
		return receipt;
	}
	const auto &handle = handles[index];
	receipt.entity = handle.entity;
	receipt.actor = {handle.actor.slot, handle.actor.actorGeneration};
	receipt.context = reactionContext(index);
	receipt.connection = services_->service_lifecycle().slotGeneration(handle.actor.slot);
	receipt.revision = actors_[index].confirmedSpeech.revision();
	receipt.teamKnown = services_->observation_publicTeamKnown(index);
	receipt.publicTeam = services_->observation_teamInfoTeams(index);
	if (receipt.entity)
	{
		receipt.serial = receipt.entity->serialnumber;
		receipt.free = receipt.entity->free != 0;
		receipt.flags = receipt.entity->v.flags & kEligibilityFlags;
		receipt.entityTeam = receipt.entity->v.team;
		receipt.name = receipt.entity->v.netname;
	}
	receipt.queueRevision = actors_[index].outgoingRadioSchedulers.statements().revision();
	receipt.hasPending = !actors_[index].outgoingRadioSchedulers.statements().empty();
	receipt.speaking = actors_[index].confirmedSpeech.scheduledStatement();
	return receipt;
}

bool BotBehaviorRuntime::sameChatterPeer(const ChatterPeerReceipt &a, const ChatterPeerReceipt &b)
{
	const auto sameStatement = [](const compat::ChatterScheduleEntry &a, const compat::ChatterScheduleEntry &b)
	{
		return a.owner.actor == b.owner.actor && a.owner.frame == b.owner.frame &&
			a.owner.entityGeneration == b.owner.entityGeneration && a.owner.now == b.owner.now &&
			a.owner.alive == b.owner.alive && a.connection == b.connection && a.ownerRevision == b.ownerRevision &&
			a.source == b.source && a.topic == b.topic && a.created == b.created && a.start == b.start &&
			a.order == b.order && a.place == b.place && a.subject == b.subject && a.speaking == b.speaking &&
			a.firstPhrase == b.firstPhrase;
	};

	if (a.managed != b.managed)
	{
		return false;
	}
	if (!a.managed)
	{
		return true;
	}
	if (!(a.entity == b.entity && a.actor == b.actor && a.context.actor == b.context.actor &&
			a.context.frame == b.context.frame && a.context.entityGeneration == b.context.entityGeneration &&
			a.context.now == b.context.now && a.context.alive == b.context.alive && a.serial == b.serial &&
			a.free == b.free && a.flags == b.flags && a.connection == b.connection && a.revision == b.revision &&
			a.teamKnown == b.teamKnown && a.publicTeam == b.publicTeam && a.entityTeam == b.entityTeam &&
			a.name == b.name))
		return false;
	return a.queueRevision == b.queueRevision && a.hasPending == b.hasPending && sameStatement(a.speaking, b.speaking);
}

compat::ChatterScheduleEntry BotBehaviorRuntime::activeTeamChatter(std::size_t index, int team, bool *current)
{
	*current = false;
	const auto engine = services_->service_engineFunctions();
	std::array<ChatterPeerReceipt, kMaximumClientSlots> peers{};
	for (std::size_t i = 0; i < peers.size(); ++i)
	{
		peers[i] = snapshotChatterPeer(i);
	}
	const auto ownCurrent = [&]()
	{
		return sameChatterPeer(peers[index], snapshotChatterPeer(index));
	};
	compat::ChatterTeamSelection selection;
	for (std::size_t peerIndex = 0; peerIndex < peers.size(); ++peerIndex)
	{
		const auto &peer = peers[peerIndex];
		const auto &context = peer.context;
		if (!peer.managed || !context.identityValid() || !peer.entity || !(peer.flags & FL_CLIENT) ||
			(peer.flags & (FL_DORMANT | FL_SPECTATOR)) || peer.name == 0)
		{
			continue;
		}
		if (!peer.hasPending && !compat::validChatterScheduleEntry(peer.speaking))
			continue;
		const int peerTeam = services_->observation_resolvePlayerTeam(peer.entity);
		if (!ownCurrent() || !sameChatterPeer(peer, snapshotChatterPeer(peerIndex)))
		{
			return {};
		}
		bool named = true;
		if (engine->pfnSzFromIndex)
		{
			const char *name = engine->pfnSzFromIndex(peer.name);
			if (!ownCurrent() || !sameChatterPeer(peer, snapshotChatterPeer(peerIndex)))
			{
				return {};
			}
			named = name && name[0] != '\0';
		}
		if (peerTeam != team || !named)
		{
			continue;
		}
		const auto append = [&](const compat::ChatterScheduleEntry &entry)
		{
			if (!compat::validChatterScheduleEntry(entry))
			{
				return true;
			}
			if (!(entry.owner.actor == context.actor) || entry.owner.entityGeneration != context.entityGeneration ||
				entry.owner.frame.mapGeneration != context.frame.mapGeneration ||
				entry.owner.frame.roundGeneration != context.frame.roundGeneration ||
				entry.owner.frame.tick > context.frame.tick || entry.owner.now > context.now ||
				entry.connection != peer.connection ||
				(!context.alive && entry.topic != compat::ChatterStatementTopic::RoundEnd) ||
				entry.ownerRevision != actors_[peerIndex].confirmedSpeech.lifetimeRevision())
			{
				return true;
			}
			return selection.consider(entry);
		};
		// Retained speech retains its native insertion position among pending nodes.
		bool visitedSpeaker = false;
		actors_[peerIndex].outgoingRadioSchedulers.statements().visitSchedules(
			[&](const compat::ChatterScheduleEntry &entry)
			{
				if (!visitedSpeaker && compat::validChatterScheduleEntry(peer.speaking) &&
					(peer.speaking.start < entry.start ||
						(peer.speaking.start == entry.start && peer.speaking.order < entry.order)))
				{
					visitedSpeaker = true;
					if (!append(peer.speaking))
						return false;
				}
				if (compat::sameChatterStatement(entry, peer.speaking))
				{
					visitedSpeaker = true;
					return append(peer.speaking);
				}
				return append(entry);
			});
		if (!visitedSpeaker)
			append(peer.speaking);
	}
	// Exclusion is part of the receipt: later callbacks may requalify an earlier peer.
	if (!ownCurrent())
	{
		return {};
	}
	for (std::size_t i = 0; i < peers.size(); ++i)
	{
		if (!sameChatterPeer(peers[i], snapshotChatterPeer(i)))
		{
			return {};
		}
	}
	*current = true;
	return selection.finish(peers[index].context.now);
}

void BotBehaviorRuntime::reportBombPickedUp(std::size_t index)
{
	auto &actor = actors_[index];
	if (actor.bombStatus.status != compat::BombStatusMemory::Status::Loose || services_->service_roundOver())
		return;
	const auto receipt = reactionContext(index);
	if (!receipt.valid())
		return;
	const auto revision = actor.executionRevision;
	const auto &origin = services_->service_managedBotHandles()[index].entity->v.origin;
	const nav::NavVector position{origin.x, origin.y, origin.z};
	actor.bombStatus.rememberCarrier(position);
	ensureOutgoingChatterCatalog();
	if (!executionCurrent(index, receipt, revision))
		return;
	const auto engine = services_->service_engineFunctions();
	const auto mode = compat::parseChatterVerbosity(
		engine && engine->pfnCVarGetString ? engine->pfnCVarGetString("bot_chatter") : nullptr);
	if (!executionCurrent(index, receipt, revision))
		return;
	const auto report = compat::bombStatusReport(compat::ChatterPhrase::TheyPickedUpTheBomb, receipt.now, position);
	actor.outgoingRadioSchedulers.statements().admit(report, chatterAdmission(index), outgoingChatterCatalog_,
		mode, actor.confirmedSpeech.scheduledStatement());
}

void BotBehaviorRuntime::observeBombPickup(Vector origin, world::FrameIdentity frame)
{
	if (services_->service_compatibilitySurface().configuration().mode != compat::RuntimeMode::Compatibility ||
		services_->service_roundOver())
		return;
	constexpr float kHearingRange = 1000.0f;
	for (std::size_t index = 0; index < actors_.size(); ++index)
	{
		if (!services_->service_managedBotSlots()[index] ||
			actors_[index].bombStatus.status != compat::BombStatusMemory::Status::Loose)
			continue;
		const auto receipt = reactionContext(index);
		auto *self = services_->service_managedBotHandles()[index].entity;
		const auto revision = actors_[index].executionRevision;
		if (!receipt.valid() || !(receipt.frame == frame) || !self ||
			services_->observation_resolvePlayerTeam(self) != kCounterTerroristTeam || !executionCurrent(index, receipt, revision))
			continue;
		const auto offset = self->v.origin - origin;
		if (offset.x * offset.x + offset.y * offset.y + offset.z * offset.z < kHearingRange * kHearingRange)
			reportBombPickedUp(index);
	}
}

void BotBehaviorRuntime::observeHostageUsed(Vector origin, world::FrameIdentity frame, world::EntityKey following)
{
	if (services_->service_compatibilitySurface().configuration().mode != compat::RuntimeMode::Compatibility ||
		services_->service_roundOver())
		return;
	constexpr float kHearingRange = 1200.0f;
	for (std::size_t index = 0; index < actors_.size() && !services_->service_roundOver(); ++index)
	{
		const auto receipt = reactionContext(index);
		auto *self = services_->service_managedBotHandles()[index].entity;
		if (!receipt.valid() || !receipt.alive || !(receipt.frame == frame) || !self)
			continue;
		const auto offset = self->v.origin - origin;
		if (!(offset.x * offset.x + offset.y * offset.y + offset.z * offset.z <= kHearingRange * kHearingRange))
			continue;
		const auto revision = actors_[index].executionRevision;
		if (services_->observation_resolvePlayerTeam(self) != kTerroristTeam ||
			!executionCurrent(index, receipt, revision))
			continue;
		ensureOutgoingChatterCatalog();
		if (!executionCurrent(index, receipt, revision) || services_->service_roundOver())
			continue;
		ActorTaskEnvironment environment(*this, index, index, [&] { return !services_->service_roundOver(); });
		compat::ChatterStatementRequest report{};
		report.source = compat::ChatterScheduleSource::Generic;
		report.topic = compat::ChatterStatementTopic::Information;
		report.phrase = compat::ChatterPhrase::HostagesBeingTaken;
		report.created = report.start = report.readyAt = receipt.now;
		report.expires = receipt.now + 3.0f;
		report.meme.kind = compat::ChatterMemeKind::HostageBeingTaken;
		if (!environment.current())
			continue;
		environment.admit(report);
		const auto guardTask = compat::CompatibilityTaskId::GuardHostageRescueZone;
		if (!environment.current() || !environment.navigation.isValid() ||
			actors_[index].managedBotStateMachines.task() == guardTask ||
			!environment.rulesOutVisibleFreeHostage(following))
			continue;
		if (const auto *area = environment.randomRescueArea())
			environment.hideNearArea(area->id, guardTask, 500.0f);
	}
}

void BotBehaviorRuntime::reactToHostageLoss(std::size_t index, bool allGone)
{
	ActorTaskEnvironment environment(*this, index, index, [] { return true; });
	if (!environment.current())
		return;
	if (allGone && !services_->service_roundOver())
	{
		ensureOutgoingChatterCatalog();
		const float now = environment.receipt.context.now;
		compat::ChatterStatementRequest report{};
		report.source = compat::ChatterScheduleSource::Generic;
		report.topic = compat::ChatterStatementTopic::Information;
		report.phrase = compat::ChatterPhrase::HostagesTaken;
		report.created = report.start = report.readyAt = now;
		report.expires = now + 3.0f;
		environment.admit(report);
	}
	if (!environment.current())
		return;
	if (!allGone)
		actors_[index].someHostagesTaken = true;
	(void)environment.idleForObjectiveChange(true);
}

void BotBehaviorRuntime::reportLostEnemy(
	std::size_t index, world::EntityKey target, const std::function<bool()> &current)
{
	const auto engine = services_->service_engineFunctions();
	const auto globals = services_->service_globals();
	if (!engine || !engine->pfnPEntityOfEntIndex || !globals || globals->maxClients <= 0 ||
		target.id > static_cast<unsigned>(globals->maxClients) || services_->service_roundOver())
		return;
	const auto connection = services_->service_lifecycle().slotGeneration(target.id);
	const auto *victim = engine->pfnPEntityOfEntIndex(static_cast<int>(target.id));
	const auto targetCurrent = [&] {
		return current() && victim && !victim->free && (victim->v.flags & FL_CLIENT) &&
			victim->serialnumber >= 0 && static_cast<unsigned>(victim->serialnumber) + 1U == target.generation &&
			victim->v.deadflag == DEAD_NO && victim->v.health > 0.0f &&
			services_->service_lifecycle().slotGeneration(target.id) == connection;
	};
	ActorTaskEnvironment env(*this, index, index, targetCurrent);
	if (env.current() && env.receipt.context.alive)
		env.say("LostEnemy");
}

bool BotBehaviorRuntime::waitAtEndOfSafeTime(std::size_t index, const compat::ScenarioBehaviorInput &scenario,
	const std::function<bool()> &current)
{
	const auto &actor = actors_[index];
	const int morale = services_->service_excellentMorale() ? 3 : actor.morale.value;
	if (morale >= 0 || services_->service_roundOver() ||
		services_->service_roundFreezeGeneration() != scenario.frame.roundGeneration)
		return false;
	const float elapsed = scenario.timestamp - services_->service_roundFreezeUntil();
	const float remaining = actor.enemyReaction.safeTime(scenario.aggression) - elapsed;
	if (!(remaining > 0.0f && remaining < 2.0f))
		return false;

	ActorTaskEnvironment env(*this, index, index, current);
	if (!env.current() || !env.receipt.context.alive)
		return false;
	float roll = 0.0f;
	if (!env.drawFloat("Hunt.WaitingHereChance", 0.0f, 100.0f, roll) ||
		roll >= -40.0f * static_cast<float>(morale))
		return false;
	const auto kind = scenario.scenario;
	const bool onDefense = (kind == compat::PublicScenarioKind::Bomb &&
		scenario.team == objectives::TeamRole::CounterTerrorist) ||
		((kind == compat::PublicScenarioKind::Hostage || kind == compat::PublicScenarioKind::Vip) &&
			scenario.team == objectives::TeamRole::Terrorist);
	if (onDefense && (roundPlantDecision_.map != scenario.frame.mapGeneration ||
			roundPlantDecision_.round != scenario.frame.roundGeneration ||
			roundPlantDecision_.defense != objectives::DefenseStrategy::Hold))
		return false;

	float duration = 0.0f;
	if (!env.drawFloat("Hunt.WaitingHereDuration", 3.0f, 15.0f, duration))
		return false;
	const auto area = services_->navigation_knownArea(index, env.receipt.actor, scenario.frame, env.navigation.revision());
	if (!env.current())
		return false;
	const bool changed = env.hideNearArea(area, compat::CompatibilityTaskId::SeekAndDestroy, 750.0f, duration) ||
		env.idleForObjectiveChange(false);
	if (changed && env.current())
		env.say("WaitingHere");
	return changed;
}

void BotBehaviorRuntime::reportAutomaticFollow(std::size_t index, const std::function<bool()> &current)
{
	const auto engine = services_->service_engineFunctions();
	if (!engine || !engine->pfnIsCareerMatch || services_->service_roundOver())
		return;
	ActorTaskEnvironment env(*this, index, index, current);
	if (!env.current() || !env.receipt.context.alive)
		return;
	const bool career = engine->pfnIsCareerMatch() != FALSE;
	if (env.current())
		env.say(career ? "FollowingCommander" : "FollowingSir", 10.0f);
}

void BotBehaviorRuntime::observePlayerBlind(std::size_t index, world::ActorKey actor, world::FrameIdentity frame)
{
	if (index >= actors_.size() || !actor.isValid() || !frame.isValid() ||
		services_->service_compatibilitySurface().configuration().mode != compat::RuntimeMode::Compatibility)
		return;
	const auto current = [&] {
		const auto context = reactionContext(index);
		return context.actor == actor && context.frame == frame && context.alive;
	};
	ActorTaskEnvironment env(*this, index, index, current);
	float roll = 0.0f;
	if (env.current() && env.drawFloat("Chatter.BlindedChance", 0.0f, 100.0f, roll) && roll < 33.3f)
		env.say("Blinded", 1.0f);
}

void BotBehaviorRuntime::observePlayerDamage(std::size_t index, world::ActorKey actor, world::FrameIdentity frame,
	const edict_t *source, int serial, world::ActorKey sourceConnection)
{
	if (index >= actors_.size() || !actor.isValid() || !frame.isValid() || !source ||
		sourceConnection.slot == 0 || sourceConnection.slot > actors_.size() ||
		services_->service_compatibilitySurface().configuration().mode != compat::RuntimeMode::Compatibility)
		return;
	const auto sourceIndex = static_cast<std::size_t>(sourceConnection.slot - 1U);
	const auto sourceTeamKnown = services_->observation_publicTeamKnown(sourceIndex);
	const auto sourceTeamInfo = services_->observation_teamInfoTeams(sourceIndex);
	const int sourceEntityTeam = source->v.team;
	constexpr int playerFlags = FL_CLIENT | FL_FAKECLIENT | FL_SPECTATOR | FL_DORMANT;
	const int sourceFlags = source->v.flags & playerFlags;
	if ((sourceFlags & ~FL_FAKECLIENT) != FL_CLIENT)
		return;
	const auto current = [&] {
		const auto context = reactionContext(index);
		return context.actor == actor && context.frame == frame &&
			!source->free && source->serialnumber == serial && receiptConnectionCurrent(sourceConnection) &&
			services_->observation_publicTeamKnown(sourceIndex) == sourceTeamKnown &&
			services_->observation_teamInfoTeams(sourceIndex) == sourceTeamInfo && source->v.team == sourceEntityTeam &&
			(source->v.flags & playerFlags) == sourceFlags;
	};
	ActorTaskEnvironment env(*this, index, index, current);
	if (!env.current())
		return;
	const bool freeForAll = readOptionalCvarFloat(services_->service_engineFunctions(), "mp_freeforall") != 0.0f;
	if (!env.current())
		return;
	const int victimTeam = services_->observation_resolvePlayerTeam(env.receipt.entity);
	if (!env.current())
		return;
	const int sourceTeam = services_->observation_resolvePlayerTeam(source);
	if (!env.current() || (!freeForAll && (victimTeam == 0 || sourceTeam == 0)))
		return;
	if (freeForAll || sourceTeam != victimTeam)
	{
		if (source->v.deadflag == DEAD_NO && source->v.health > 0)
		{
			auto &target = actors_[index];
			const float roundStart = services_->service_roundFreezeGeneration() == frame.roundGeneration
				? services_->service_roundFreezeUntil() : 0.0f;
			const float aggression = static_cast<float>(target.managedProfiles.aggression) / 100.0f;
			auto &memory = target.enemyReaction;
			const auto &receipt = env.receipt.context;
			const world::EntityKey inflictor{sourceConnection.slot, static_cast<std::uint32_t>(serial) + 1U};
			if (!memory.recordEnemyDamage(receipt, inflictor, receipt.now - roundStart, aggression,
					sourceConnection.generation) || !receipt.alive || memory.surprised(receipt))
				return;

			compat::PublicActorState sight{};
			const bool blinded = services_->observation_publicObservations().read(actor, frame, &sight) &&
				receipt.now < sight.blindUntil;
			const world::ActorKey sourceActor{sourceConnection.slot, (std::max)(1U, sourceConnection.generation)};
			perception::VisionObservation visibility{};
			if (!blinded && (services_->observation_observationAdapter().collectVisibility(
					env.receipt.entity, source, sourceActor, frame, &visibility, true) != ObservationAdapterResult::Accepted ||
				visibility.visible))
				return;
			const auto panicCurrent = [&] {
				return env.current() && source->v.deadflag == DEAD_NO && source->v.health > 0.0f &&
					memory.recentAttacker(receipt, 3.0f) == inflictor && !memory.surprised(receipt);
			};
			if (!panicCurrent())
				return;
			const float skill = static_cast<float>(target.managedProfiles.skill) / 100.0f;
			const auto &belief = services_->observation_managedBotPerception(index).snapshot();
			const world::ActorObservation *contact = nullptr;
			if (target.managedBotStateMachines.attackOverlayActive() &&
				belief.findActor(memory.enemy(receipt), &contact) == world::ContactLookupResult::Found &&
				contact && contact->visible)
			{
				float roll = 0.0f;
				const float inexperience = 1.0f - skill;
				if (!env.drawFloat("Damage.PanicChance", 0.0f, 100.0f, roll) || !panicCurrent() ||
					!(inexperience * inexperience * 50.0f > roll))
					return;
			}

			const auto origin = env.receipt.entity->v.origin;
			const auto towards = source->v.origin - origin;
			constexpr float kRadiansPerDegree = 0.01745329251994329577f;
			const float yaw = env.receipt.entity->v.v_angle.y * kRadiansPerDegree;
			const float forwardX = std::cos(yaw), forwardY = std::sin(yaw);
			const bool skilled = skill >= 0.5f;
			const float sideRange = skilled ? 75.0f : 200.0f;
			float shift = 0.0f, height = 0.0f, delay = 0.0f;
			if (!env.drawFloat("Damage.PanicSide", -sideRange, sideRange, shift) || !panicCurrent() ||
				!env.drawFloat("Damage.PanicHeight", -50.0f, 50.0f, height) || !panicCurrent() ||
				!env.drawFloat("Damage.PanicDelay", 0.1f, 0.2f, delay) || !panicCurrent())
				return;
			float forward = -200.0f;
			float lateral = 2.0f * shift;
			if (skilled)
			{
				const float distance = std::hypot(towards.x, towards.y);
				const float along = distance > 0.0f ? (towards.x * forwardX + towards.y * forwardY) / distance : 0.0f;
				if (along > 0.7071f || along < -0.7071f)
				{
					forward = along > 0.0f ? 100.0f : -100.0f;
					lateral = shift;
				}
				else
				{
					forward = shift;
					lateral = towards.y * forwardX - towards.x * forwardY > 0.0f ? 100.0f : -100.0f;
				}
			}
			memory.startSurprise(receipt, delay);
			target.noiseLook.lookAtDamagePanic(actor, frame, receipt.now,
				{origin.x + forwardX * forward - forwardY * lateral,
					origin.y + forwardY * forward + forwardX * lateral, origin.z + height});
		}
		return;
	}
	if (sourceFlags & FL_FAKECLIENT)
		return;
	float delay = 0.0f;
	if (!env.drawFloat("Chatter.FriendlyFireDelay", 0.3f, 0.5f, delay))
		return;
	ensureOutgoingChatterCatalog();
	if (!env.current())
		return;
	if (const auto *phrase = outgoingChatterCatalog_.findPhrase("FriendlyFire"))
	{
		auto statement = compat::makeSayStatement(*phrase, env.receipt.context.now, 1.0f, delay);
		statement.topic = compat::ChatterStatementTopic::FriendlyFire;
		env.admit(statement);
	}
}

void BotBehaviorRuntime::encourageHumanDefuse(
	std::size_t index, float remaining, const std::function<bool()> &current)
{
	if (services_->service_roundOver())
		return;
	ActorTaskEnvironment env(*this, index, index, current);
	if (!env.current() || !env.receipt.context.alive)
		return;
	const auto living = livingRadioPeers(env.receipt.entity, kCounterTerroristTeam);
	if (!env.current())
		return;
	const auto &receipt = env.receipt.context;
	const auto nearby = actors_[index].outgoingRadioContactMemory.counts(
		env.receipt.actor, receipt.frame, receipt.now, living.friends, living.enemies);
	if (nearby.enemies != 0)
		return;

	const char *phrase = "WaitingForHumanToDefuseBombPanic";
	float low = 3.0f, high = 4.0f;
	if (remaining > 30.0f)
	{
		phrase = "BombsiteSecure";
		low = 10.0f;
		high = 15.0f;
	}
	else if (remaining > 11.0f)
	{
		phrase = "WaitingForHumanToDefuseBomb";
		low = 5.0f;
		high = 8.0f;
	}
	float interval = 0.0f;
	// CSBot evaluates RANDOM_FLOAT at the call site even while the shared timer is active.
	if (!env.drawFloat("Chatter.Encourage.Defuse", low, high, interval) || receipt.now <= encouragementUntil_)
		return;
	encouragementUntil_ = receipt.now + interval;
	env.say(phrase);
}

void BotBehaviorRuntime::reportRoundVictory(
	std::size_t index, int team, bool barelyDefused, const std::function<bool()> &current)
{
	ActorTaskEnvironment env(*this, index, index, current);
	if (!env.current() || !env.receipt.context.alive)
		return;
	if (barelyDefused)
	{
		env.say("BarelyDefused");
		if (!env.current())
			return;
	}
	float delay = 0.0f;
	if (!env.drawFloat("Chatter.VictoryDelay", 2.0f, 5.0f, delay))
		return;
	const auto living = livingRadioPeers(env.receipt.entity, team);
	if (!env.current())
		return;
	const float now = env.receipt.context.now;
	const float began = services_->service_roundFreezeGeneration() == env.receipt.context.frame.roundGeneration
		? services_->service_roundFreezeUntil() : 0.0f;
	const bool quick = now - began < 45.0f;
	compat::ChatterPhrase phrase = quick ? compat::ChatterPhrase::WonRoundQuickly
		: living.friends == 0 ? compat::ChatterPhrase::LastManStanding : compat::ChatterPhrase::WonRound;
	if (!quick || living.friends != 0)
	{
		float roll = 0.0f;
		if (!env.drawFloat("Chatter.VictoryChance", 0.0f, 100.0f, roll) ||
			roll >= (quick || living.friends == 0 ? 33.3f : 10.0f))
			return;
	}
	ensureOutgoingChatterCatalog();
	if (!env.current())
		return;
	compat::ChatterStatementRequest statement{};
	statement.source = compat::ChatterScheduleSource::Generic;
	statement.topic = compat::ChatterStatementTopic::Emote;
	statement.phrase = phrase;
	statement.created = now;
	statement.start = statement.readyAt = now + delay;
	statement.expires = now + 15.0f;
	env.admit(statement);
}

void BotBehaviorRuntime::discardEndedRoundSpeech(std::size_t index)
{
	const auto &speech = actors_[index].confirmedSpeech;
	const auto confirmation = speech.scheduledStatement();
	auto &queue = actors_[index].outgoingRadioSchedulers.statements();
	const auto retained = queue.handle(confirmation);
	queue.discardEndedRound();
	compat::ChatterPendingStatement node{};
	if (retained.id != 0 && !queue.read(retained, &node))
	{
		(void)actors_[index].confirmedSpeech.finishStatement(
			reactionContext(index), confirmation, speech.revision());
		stopVoiceFeedback(index);
	}
}

void BotBehaviorRuntime::finishRoundSpeech()
{
	shuffleReadyChatterBanks();
	for (std::size_t index = 0; index < actors_.size(); ++index)
		discardEndedRoundSpeech(index);
}

void BotBehaviorRuntime::updateOutgoingPublicRadio(std::size_t index, const PlantMoveChatter *plantMove)
{
	if (index >= services_->service_managedBotHandles().size() || services_->service_globals() == nullptr ||
		services_->service_engineFunctions() == nullptr ||
		services_->service_engineFunctions()->pfnPEntityOfEntIndex == nullptr)
		return;
	const auto &handle = services_->service_managedBotHandles()[index];
	if (handle.entity == nullptr || handle.entity->free)
		return;
	const auto receiptContext = reactionContext(index);
	actors_[index].confirmedSpeech.synchronizeOwner(receiptContext);
	const auto receiptRevision = actors_[index].confirmedSpeech.revision();
	const auto receiptConnection = services_->service_lifecycle().slotGeneration(receiptContext.actor.slot);
	const auto receiptTeamKnown = services_->observation_publicTeamKnown(index);
	const auto receiptTeam = services_->observation_teamInfoTeams(index);
	const int receiptEntityTeam = handle.entity->v.team;
	const auto receiptCurrent = [&]()
	{
		// Dead/spectating observers still maintain the existing public radio update.
		// Transport completion separately requires a valid living speech receipt.
		const auto current = reactionContext(index);
		return receiptContext.actor.isValid() && receiptContext.frame.isValid() &&
			receiptContext.entityGeneration != 0 && std::isfinite(receiptContext.now) && receiptContext.now >= 0 &&
			receiptContext.actor == current.actor && receiptContext.entityGeneration == current.entityGeneration &&
			receiptContext.frame == current.frame && receiptContext.now == current.now &&
			receiptContext.alive == current.alive && actors_[index].confirmedSpeech.revision() == receiptRevision &&
			services_->service_lifecycle().slotGeneration(receiptContext.actor.slot) == receiptConnection &&
			services_->observation_publicTeamKnown(index) == receiptTeamKnown &&
			services_->observation_teamInfoTeams(index) == receiptTeam && handle.entity->v.team == receiptEntityTeam;
	};
	if (!receiptCurrent())
		return;
	const char *verbosity = services_->service_engineFunctions()->pfnCVarGetString != nullptr
		? services_->service_engineFunctions()->pfnCVarGetString("bot_chatter")
		: nullptr;
	ensureOutgoingChatterCatalog();
	if (!receiptCurrent())
		return;
	if (outgoingRadioRound_ != services_->service_lifecycle().roundGeneration())
	{
		outgoingRadioRound_ = services_->service_lifecycle().roundGeneration();
		// Round recognition already cleared old nodes before current-round event admission.
		outgoingTeamLastSpeech_.fill(-1.0f);
		outgoingTeamBusyUntil_.fill(0.0f);
		outgoingTeamCurrentPhrase_.fill(compat::ChatterPhrase::None);
		outgoingTeamCurrentPlace_.fill(0);
		outgoingPlaceHistory_.reset();
		outgoingTeamFollowRadio_.fill(0);
	}
	compat::OutgoingRadioInput input{};
	input.actor = receiptContext.actor;
	input.frame = receiptContext.frame;
	input.now = receiptContext.now;
	input.alive = handle.entity->v.deadflag == DEAD_NO && handle.entity->v.health > 0.0f;
	input.team = services_->observation_resolvePlayerTeam(handle.entity);
	if (!receiptCurrent())
		return;
	if (input.team != kTerroristTeam && input.team != kCounterTerroristTeam)
		return;
	input.teamLastSpeech = outgoingTeamLastSpeech_[input.team];
	input.teamFollowRadioAt = outgoingTeamFollowRadio_[input.team];
	// Native active statements, including retained speakers, own arbitration.
	input.teamBusyUntil = 0;
	input.teamCurrentPhrase = compat::ChatterPhrase::None;
	input.teamCurrentPlace = outgoingTeamCurrentPlace_[input.team];
	const auto reportNav = services_->service_navPublisher().snapshot();
	input.navRevision = reportNav.isValid() ? reportNav.revision() : 0;
	input.roundOver = services_->service_roundOver();
	input.noiseReport = actors_[index].noiseReport;
	input.reactionSeconds = actors_[index].managedProfiles.reactionTimeSeconds;
	input.inCombat = actors_[index].managedBotStateMachines.attackOverlayActive();
	const auto living = livingRadioPeers(handle.entity, input.team);
	input.friendCount = living.friends;
	input.enemiesRemaining = living.enemies;
	if (!receiptCurrent())
		return;
	// Count the observer's current public FOV/LOS/smoke-filtered contacts. A
	// selected combat target is only one contact and cannot express outnumbering.
	world::WorldSnapshot belief;
	world::ActorKey selected{};
	world::WorldPosition selectedPosition{};
	compat::PublicActorState ownObservation{};
	const bool blind = services_->observation_publicObservations().read(input.actor, input.frame, &ownObservation) &&
		services_->service_globals()->time < ownObservation.blindUntil;
	const bool haveBelief = !blind && prepareManagedPerception(index, &belief, &selected, &selectedPosition);
	if (!receiptCurrent())
		return;
	const auto contacts = actors_[index].outgoingRadioContactMemory.update(input.actor, input.frame, input.now,
		haveBelief ? &belief : nullptr, blind, input.friendCount, input.enemiesRemaining);
	input.visibleEnemyCount = contacts.enemies;
	input.nearbyFriends = contacts.friends;
	world::ActorKey bomber{};
	nav::NavVector bomberPosition{};
	if (input.alive && haveBelief && input.team == kCounterTerroristTeam &&
		services_->service_compatibilitySurface().configuration().mode == compat::RuntimeMode::Compatibility)
	{
		for (std::size_t i = 0; i < belief.actorCount(); ++i)
		{
			const auto *contact = belief.actorAt(i);
			if (contact && contact->carryingC4 && contact->visible && contact->hasCurrentPosition() &&
				contact->relation == world::TeamRelation::Hostile && contact->actor.slot > bomber.slot)
			{
				bomber = contact->actor;
				bomberPosition = {contact->position.x, contact->position.y, contact->position.z};
			}
		}
	}
	bool reportSniper = false;
	auto &sniper = actors_[index].sniperWarning;
	if (input.alive && haveBelief && input.now > sniper.sightUntil &&
		services_->service_compatibilitySurface().configuration().mode == compat::RuntimeMode::Compatibility)
	{
		for (std::size_t i = 0; i < belief.actorCount(); ++i)
		{
			const auto *contact = belief.actorAt(i);
			if (!contact || contact->relation != world::TeamRelation::Hostile || !contact->visible ||
				!contact->hasCurrentPosition() || !contact->hasSniperRifle)
				continue;
			const bool activeSniper = services_->observation_activeSniper(contact->actor, input.frame, input.team);
			if (!receiptCurrent())
				return;
			if (activeSniper)
			{
				sniper.sightUntil = input.now + 20.0f;
				reportSniper = input.friendCount > 0 && input.now > sniper.warnedUntil;
				break;
			}
		}
	}
	const bool reportBomber = bomber.isValid() && actors_[index].bombStatus.isNewCarrierPosition(bomberPosition);
	std::uint16_t bomberPlace = 0;
	const auto loose = actors_[index].looseBombObservation;
	const bool sawLoose = input.alive && !input.roundOver && loose.frame == input.frame &&
		loose.knowledge == compat::ScenarioKnowledge::Visible;
	const nav::NavVector loosePosition{loose.position.x, loose.position.y, loose.position.z};
	std::uint16_t loosePlace = 0;
	auto discovery = actors_[index].discoveredBombsiteReport;
	const auto metadataPlace = [&](std::uint16_t place)
	{
		return chatterPlacePhrase(reportNav, outgoingChatterCatalog_, place);
	};
	if (reportNav.isValid() && reportNav.mapGeneration() == input.frame.mapGeneration)
	{
		const auto areaPlace = [&](nav::AreaId area)
		{
			const auto *definition = reportNav.document()->findArea(area);
			return definition ? definition->placeId : std::uint16_t{0};
		};
		auto ownArea = services_->navigation_knownArea(index, input.actor, input.frame, reportNav.revision());
		if (!ownArea)
			ownArea = compat::findBotNavArea(
				reportNav, {handle.entity->v.origin.x, handle.entity->v.origin.y, handle.entity->v.origin.z}, nullptr);
		input.ownPlace = areaPlace(ownArea);
		input.ownPlacePhrase = metadataPlace(input.ownPlace);
		input.noisePlacePhrase = metadataPlace(input.noiseReport.place);
		std::array<compat::ChatterVisiblePlace, kMaximumClientSlots> visiblePlaces{};
		struct PlacePeerReceipt
		{
			std::uint32_t connection;
			bool known;
			std::uint8_t team;
		};
		std::array<PlacePeerReceipt, kMaximumClientSlots> peers{};
		for (std::size_t peer = 0; peer < peers.size(); ++peer)
			peers[peer] = {services_->service_lifecycle().slotGeneration(static_cast<std::uint32_t>(peer + 1)),
				services_->observation_publicTeamKnown(peer), services_->observation_teamInfoTeams(peer)};
		const auto queryCurrent = [&]()
		{
			const auto currentNav = services_->service_navPublisher().snapshot();
			if (!receiptCurrent() || !currentNav.isValid() || currentNav.revision() != reportNav.revision())
				return false;
			for (std::size_t peer = 0; peer < peers.size(); ++peer)
				if (peers[peer].connection !=
						services_->service_lifecycle().slotGeneration(static_cast<std::uint32_t>(peer + 1)) ||
					peers[peer].known != services_->observation_publicTeamKnown(peer) ||
					peers[peer].team != services_->observation_teamInfoTeams(peer))
					return false;
			return true;
		};
		PublicNavProbe probe(services_->service_engineFunctions(), queryCurrent);
		if (reportBomber)
		{
			const auto area = compat::findBotNavArea(reportNav, bomberPosition, &probe, true);
			if (!queryCurrent())
				return;
			bomberPlace = areaPlace(area);
		}
		if (sawLoose && (reportBomber || actors_[index].bombStatus.status != compat::BombStatusMemory::Status::Loose) &&
			input.now > actors_[index].bombStatus.nextLooseReport)
		{
			const auto area = compat::findBotNavArea(reportNav, loosePosition, &probe, true);
			if (!queryCurrent())
				return;
			loosePlace = areaPlace(area);
		}
		if (discovery.meme.site.isValid())
		{
			const auto area = compat::findBotNavArea(reportNav, discovery.meme.position, &probe, true);
			if (!queryCurrent())
				return;
			discovery.place = areaPlace(area);
			discovery.criteriaPlace = metadataPlace(discovery.place);
		}
		const auto cleared = actors_[index].clearedBombsite;
		if (cleared.frame == input.frame && cleared.entity.isValid())
		{
			input.clearedBombsite = cleared.entity;
			const auto area = compat::findBotNavArea(reportNav,
				{cleared.position.x, cleared.position.y, cleared.position.z}, &probe, true);
			if (!queryCurrent())
				return;
			input.clearedBombsitePlace = areaPlace(area);
			input.clearedBombsitePhrase = metadataPlace(input.clearedBombsitePlace);
		}
		if (haveBelief && outgoingChatterCatalog_.hasPlaceBefore() &&
			actors_[index].outgoingRadioSchedulers.needsEnemyPlace(input))
			for (std::size_t i = 0; i < belief.actorCount(); ++i)
			{
				const auto *contact = belief.actorAt(i);
				if (!contact || !contact->visible || !contact->hasCurrentPosition() ||
					contact->relation != world::TeamRelation::Hostile || !contact->actor.slot ||
					contact->actor.slot > actors_.size())
					continue;
				const auto connection = services_->service_lifecycle().slotGeneration(contact->actor.slot);
				if (contact->actor.generation != (connection ? connection : 1U))
					continue;
				nav::AreaId area = 0;
				const auto peer = static_cast<std::size_t>(contact->actor.slot - 1);
				if (services_->service_managedBotSlots()[peer])
				{
					const auto &managed = services_->service_managedBotHandles()[peer];
					area = services_->navigation_knownArea(
						peer, {managed.actor.slot, managed.actor.actorGeneration}, input.frame, reportNav.revision());
				}
				else
				{
					area = compat::findBotNavArea(reportNav,{contact->position.x,contact->position.y,contact->position.z},&probe);
					if(!queryCurrent())
						return;
				}
				const float dx = contact->position.x - handle.entity->v.origin.x;
				const float dy = contact->position.y - handle.entity->v.origin.y;
				const float dz = contact->position.z - handle.entity->v.origin.z;
				// Input index is the native client-slot tie order, independent of snapshot storage.
				visiblePlaces[peer] = {areaPlace(area), dx * dx + dy * dy + dz * dz};
			}
		input.enemyPlace = compat::chooseChatterEnemyPlace(visiblePlaces.data(), visiblePlaces.size());
		input.enemyPlacePhrase = metadataPlace(input.enemyPlace);
		if(!queryCurrent())
			return;
	}
	const auto &order = services_->observation_scenarioRadio(index);
	const auto &decision = actors_[index].scenarioDecisions;
	if (order.senderHuman && static_cast<int>(order.team) == input.team)
	{
		input.humanOrderSequence = order.sequence;
		input.humanOrderReceived = order.receivedTime;
		input.humanOrderAccepted = decision.action == compat::ScenarioAction::Follow ||
			decision.action == compat::ScenarioAction::Guard || decision.action == compat::ScenarioAction::Hunt ||
			decision.action == compat::ScenarioAction::Escape;
		input.humanOrderHelp = order.command == compat::ScenarioRadioCommand::NeedBackup;
		if (order.command == compat::ScenarioRadioCommand::ReportIn || order.command == compat::ScenarioRadioCommand::TeamFallBack)
			input.humanOrderAccepted = false;
		outgoingTeamLastSpeech_[input.team] = (std::max)(outgoingTeamLastSpeech_[input.team], order.receivedTime);
	}
	const char *mapName = services_->service_engineFunctions()->pfnSzFromIndex != nullptr
		? services_->service_engineFunctions()->pfnSzFromIndex(services_->service_globals()->mapname)
		: nullptr;
	if (!receiptCurrent())
		return;
	input.vipEscaping = mapName != nullptr && std::strncmp(mapName, "as_", kVipMapPrefixLength) == 0 &&
		input.team == kCounterTerroristTeam && decision.action == compat::ScenarioAction::Escape;
	const auto &machine = actors_[index].managedBotStateMachines;
	input.plantingBomb = machine.state() == compat::CompatibilityStateId::PlantBomb;
	if (input.plantingBomb)
	{
		const auto &origin = handle.entity->v.origin;
		input.plantPosition = {origin.x, origin.y, origin.z};
	}
	if (plantMove && plantMove->epoch && input.alive && input.team == kTerroristTeam && input.friendCount > 0)
	{
		const auto epoch = machine.normalStateEpoch();
		input.plantMoveEpoch = plantMove->epoch;
		input.plantMoveAge = plantMove->age;
		input.plantCoverDue = plantMove->coverDue;
		input.plantPosition = plantMove->reportPosition;
		const auto &scheduler = actors_[index].outgoingRadioSchedulers;
		if (plantMove->age > 2.0f && scheduler.needsPlantPlanPlace(plantMove->epoch) && plantMove->hasGoal &&
			reportNav.isValid())
		{
			const auto current = [&]()
			{
				return receiptCurrent() && machine.normalStateEpoch() == epoch &&
					services_->service_navPublisher().snapshot().revision() == reportNav.revision();
			};
			PublicNavProbe probe(services_->service_engineFunctions(), current);
			const auto areaId = compat::findBotNavArea(reportNav, plantMove->goal, &probe, true);
			if (!current())
				return;
			const auto *area = reportNav.document()->findArea(areaId);
			if (area)
			{
				input.plantGoalPlace = area->placeId;
				input.plantGoalPlacePhrase = metadataPlace(area->placeId);
			}
		}
	}
	input.escortingHostages = decision.escortInferred;
	input.bombLocationUnknown = input.team == kCounterTerroristTeam && services_->observation_bombNoticeKnown(index) &&
		services_->observation_bombNoticePlanted(index) && decision.action == compat::ScenarioAction::Move &&
		decision.hasTarget && actors_[index].bombSearchTargets.knowledge != compat::ScenarioKnowledge::Unknown &&
		actors_[index].bombSearchTargets.frame.mapGeneration == input.frame.mapGeneration &&
		actors_[index].bombSearchTargets.frame.roundGeneration == input.frame.roundGeneration &&
		decision.target.entity == actors_[index].bombSearchTargets.entity;
	const auto mode = compat::parseChatterVerbosity(verbosity);
	const bool voiceMode = mode != compat::ChatterVerbosity::Radio;
	auto voice = voiceMode ? chatterBank(index) : std::shared_ptr<compat::ChatterVoiceCatalog>{};
	if (!receiptCurrent())
		return;

	input.admission = chatterAdmission(index);
	input.verbosity = mode;
	input.speaking = actors_[index].confirmedSpeech.scheduledStatement();
	actors_[index].outgoingRadioSchedulers.prepare(input, outgoingChatterCatalog_);
	if (reportBomber && bomber.generation == (std::max)(1U, services_->service_lifecycle().slotGeneration(bomber.slot)))
	{
		actors_[index].bombStatus.rememberCarrier(bomberPosition);
		const auto report = compat::bombStatusReport(compat::ChatterPhrase::SpottedBomber, input.now,
			bomberPosition, bomberPlace, metadataPlace(bomberPlace), static_cast<int>(bomber.slot));
		actors_[index].outgoingRadioSchedulers.statements().admit(
			report, input.admission, outgoingChatterCatalog_, mode, input.speaking);
	}
	if (reportSniper)
	{
		compat::ChatterStatementRequest warning{};
		warning.source = compat::ChatterScheduleSource::Generic;
		warning.topic = compat::ChatterStatementTopic::Information;
		warning.phrase = compat::ChatterPhrase::SniperWarning;
		warning.created = warning.start = warning.readyAt = input.now;
		warning.expires = input.now + 10.0f;
		warning.meme.kind = compat::ChatterMemeKind::WarnSniper;
		actors_[index].outgoingRadioSchedulers.statements().admit(
			warning, input.admission, outgoingChatterCatalog_, mode, input.speaking);
	}
	if (sawLoose && actors_[index].bombStatus.spotLooseBomb(loosePosition, input.now))
	{
		const auto report = compat::bombStatusReport(compat::ChatterPhrase::SpottedLooseBomb, input.now,
			loosePosition, loosePlace, metadataPlace(loosePlace));
		actors_[index].outgoingRadioSchedulers.statements().admit(
			report, input.admission, outgoingChatterCatalog_, mode, input.speaking);
	}
	if (discovery.meme.site.isValid())
	{
		actors_[index].discoveredBombsiteReport = {};
		if (input.alive && !input.roundOver && input.now >= discovery.created && input.now < discovery.expires)
			actors_[index].outgoingRadioSchedulers.statements().admit(
				discovery, input.admission, outgoingChatterCatalog_, mode, input.speaking);
	}
	if (input.clearedBombsite.isValid())
		actors_[index].clearedBombsite = {};
	if (input.alive && actors_[index].scenarioPlanners.takeReportRequest(input.frame))
	{
		nav::NavVector bomb{};
		const auto *guarded = guardedBombPosition(index, &bomb) ? &bomb : nullptr;
		if (!receiptCurrent())
			return;
		const auto report = compat::reportingInStatement(input.now, input.inCombat, contacts,
			actors_[index].enemyReaction.enemyDeathTimestamp(receiptContext),
			input.now - actors_[index].lastVisibleEnemyTimes, input.ownPlace, input.ownPlacePhrase, guarded);
		if (machine.task() == compat::CompatibilityTaskId::GuardHostages ||
			machine.task() == compat::CompatibilityTaskId::GuardHostageRescueZone)
		{
			ActorTaskEnvironment environment(*this, index, index, receiptCurrent);
			environment.reportHostageGuard(machine.task(), actors_[index].scenarioPlanners.atDefenseSpot());
		}
		if (receiptCurrent())
			actors_[index].outgoingRadioSchedulers.reportIn(report, machine.task(), input, outgoingChatterCatalog_);
	}
	const bool direct = actors_[index].outgoingRadioSchedulers.hasDirect();
	bool scanCurrent = true;
	compat::ChatterScheduleEntry selectedStatement{};
	if (!direct)
	{
		selectedStatement = activeTeamChatter(index, input.team, &scanCurrent);
	}
	if (!scanCurrent || !receiptCurrent())
		return;
	const auto sendRadio = [&](compat::RadioMenuChoice choice, const auto &current)
	{
		if (!current()) return false;
		// Native manager event time is recorded before client commands, whether
		// the GameDLL accepts its quota or emits any public audio receipt.
		if (choice.menu == kCommandRadioMenu && choice.slot == 5)
			outgoingTeamFollowRadio_[input.team] = input.now;
		const char *menu = choice.menu == kCommandRadioMenu ? "radio1" : choice.menu == kGroupRadioMenu ? "radio2" : "radio3";
		char slot[] = {static_cast<char>('0' + choice.slot), '\0'};
		(void)services_->command_dispatchClientCommand(handle.entity, menu, nullptr);
		if (!current()) return false;
		(void)services_->command_dispatchClientCommand(handle.entity, "menuselect", slot);
		if (!current()) return false;
		(void)services_->command_dispatchClientCommand(handle.entity, "menuselect", "10");
		return current();
	};
	const auto action = [&]()
	{
		if (!direct)
		{
			if (!(selectedStatement.owner.actor == input.actor))
				return;
			auto &queue = actors_[index].outgoingRadioSchedulers.statements();
			const auto selectedHandle = queue.handle(selectedStatement);
			compat::ChatterPendingStatement node{};
			if (!queue.read(selectedHandle, &node))
				return;
			if (node.request.phrase == compat::ChatterPhrase::HeardNoise &&
				(input.roundOver || !input.noiseReport.active || node.request.navRevision != input.navRevision ||
					node.request.created != input.noiseReport.created ||
					node.request.noiseTick != input.noiseReport.frame.tick))
			{
				queue.erase(selectedHandle);
				(void)actors_[index].confirmedSpeech.finishStatement(
					receiptContext, selectedStatement, actors_[index].confirmedSpeech.revision());
				return;
			}
			compat::ChatterPlaybackInput playback{};
			playback.now = input.now;
			playback.enemies = input.visibleEnemyCount;
			playback.nearbyFriends = input.nearbyFriends;
			playback.enemiesRemaining = input.enemiesRemaining;
			playback.friendsRemaining = input.friendCount;
			playback.emote = selectedStatement.topic == compat::ChatterStatementTopic::Emote;
			compat::ChatterStatementStep step{};
			step = queue.advance(selectedHandle, playback);
			if (step.playback.kind == compat::ChatterPlaybackKind::Finished)
			{
				const auto revision = actors_[index].confirmedSpeech.revision();
				const auto completionCurrent = [&]
				{
					return actors_[index].confirmedSpeech.canComplete(
							   receiptContext, reactionContext(index), revision) &&
						receiptConnectionCurrent({input.actor.slot, receiptConnection});
				};
				if (step.completion.natural && completionCurrent())
					transmitTeammateRequest(index, step.completion.meme, completionCurrent);
				if (!completionCurrent())
					return;
				(void)actors_[index].confirmedSpeech.finishStatement(
					receiptContext, selectedStatement, actors_[index].confirmedSpeech.revision());
				return;
			}
			if (!step.schedule.speaking ||
				!actors_[index].confirmedSpeech.beginStatement(
					receiptContext, step.schedule, actors_[index].confirmedSpeech.revision()))
				return;
			if (step.playback.kind == compat::ChatterPlaybackKind::Waiting ||
				step.playback.kind == compat::ChatterPlaybackKind::Null)
				return;
			const auto speechRevision = actors_[index].confirmedSpeech.revision();
			const auto catalogRevision = outgoingCatalogRevision_;
			const auto bankRevision = voice ? voice->revision() : 0;
			const auto partCurrent = [&]()
			{
				compat::ChatterStatementStep current{};
				return actors_[index].confirmedSpeech.canComplete(
						   receiptContext, reactionContext(index), speechRevision) &&
					receiptConnectionCurrent({input.actor.slot, receiptConnection}) &&
					outgoingCatalogRevision_ == catalogRevision && (!voice || voice->revision() == bankRevision) &&
					queue.currentPart(step.token, &current);
			};
			if (step.playback.kind == compat::ChatterPlaybackKind::Delay)
			{
				const compat::RandomTimingContext timing{services_->command_managedBotCommandSequences(index), 0U,
					services_->service_managedBotFullUpdateSequences()[index]};
				const char *site =
					step.playback.low == .1f ? "RNG-CSBOT-CHATTER-SHORT-DELAY" : "RNG-CSBOT-CHATTER-LONG-DELAY";
				const auto result =
					services_->service_compatibilityRandomSource().nextFloat(compat::RandomRequest::floatRequest(site,
						{input.actor.slot, input.actor.generation}, timing, step.playback.low, step.playback.high));
				if (!partCurrent() || result.status != compat::RandomStatus::Ok ||
					!queue.finishDelay(step.token, result.value))
					queue.cancelPart(step.token);
				return;
			}
			const auto phrase = step.playback.phrase;
			const auto *place = outgoingChatterCatalog_.place(step.playback.placePhrase);
			const auto *named = outgoingChatterCatalog_.phrase(step.playback.namedPhrase);
			const bool defined = place || named || outgoingChatterCatalog_.defined(phrase);
			if (!defined)
			{
				queue.finishPart(step.token, 0, true);
				return;
			}
			const auto choice = named ? named->choice : place ? place->choice : outgoingChatterCatalog_.lookup(phrase);
			const auto finish = [&](float duration, bool silence)
			{
				if (!partCurrent() || !queue.finishPart(step.token, duration))
					return;
				(void)actors_[index].confirmedSpeech.record(receiptContext, phrase,
					input.now + duration + ASTRABOT_BOT_BEHAVIOR_RADIO_NEXT_PHRASE_GAP_SECONDS, speechRevision);

				if (silence)
				{
					outgoingTeamLastSpeech_[input.team] = input.now;
					outgoingTeamBusyUntil_[input.team] =
						input.now + duration + ASTRABOT_BOT_BEHAVIOR_RADIO_NEXT_PHRASE_GAP_SECONDS;
					outgoingTeamCurrentPhrase_[input.team] = phrase;
					outgoingTeamCurrentPlace_[input.team] = step.schedule.place;
				}
			};
			if (voiceMode)
			{
				float duration = 0;
				bool hasSample = false, suppressed = false;
				if (voice)
					(void)sendChatterVoice(index, phrase, step.playback.enemyCount, voice, &duration,
						step.criteriaPlace, step.schedule.topic == compat::ChatterStatementTopic::RoundEnd
							? ChatterVoicePurpose::RetainedRoundEnd : ChatterVoicePurpose::Statement,
						step.playback.placePhrase, &hasSample,
						&suppressed, step.playback.namedPhrase);
				else if (step.playback.placePhrase)
					suppressed = !outgoingPlaceHistory_.allow(step.playback.placePhrase, input.now);
				if (!partCurrent())
				{
					queue.cancelPart(step.token);
					return;
				}
				if (hasSample || suppressed)
				{
					finish(duration, hasSample && !suppressed);
					return;
				}
			}
			if (!choice.menu)
			{
				// Native's defined but inaudible phrase still reserves duration0 + .1.
				finish(0, false);
				return;
			}
			(void)actors_[index].confirmedSpeech.beginSpeaking(receiptContext, phrase,
				input.now + 2.0f + ASTRABOT_BOT_BEHAVIOR_RADIO_NEXT_PHRASE_GAP_SECONDS, speechRevision);
			// Standard-radio verbosity resets silence before SendRadioMessage;
			// a missing-sample fallback resets it after the attempted dispatch.
			if (!voiceMode) outgoingTeamLastSpeech_[input.team] = input.now;
			if (!sendRadio(choice, partCurrent))
			{
				queue.cancelPart(step.token);
				return;
			}
			finish(2.0f, true);
			return;
		}

		// Direct VIP SendRadioMessage has no Chatter duration or silence reset.
		const auto output = actors_[index].outgoingRadioSchedulers.decide(input, outgoingChatterCatalog_, 0, {});
		if (output.status != compat::OutgoingRadioStatus::Ready) return;
		auto &scheduler = actors_[index].outgoingRadioSchedulers;
		scheduler.confirmSent(input, output);
		const auto consumedToken = scheduler.directToken();
		const auto directCurrent = [&]()
		{
			return receiptCurrent() && scheduler.directToken() == consumedToken;
		};
		(void)sendRadio({output.menu, output.slot}, directCurrent);
	};
	action();
	const auto currentContext = reactionContext(index);
	if (!compat::sameChatterUpdateReceipt(receiptContext, currentContext) ||
		services_->service_lifecycle().slotGeneration(receiptContext.actor.slot) != receiptConnection ||
		actors_[index].confirmedSpeech.lifetimeRevision() != input.admission.ownerRevision)
		return;
	compat::ChatterScheduleEntry teammate{};
	if (!direct)
	{
		teammate = activeTeamChatter(index, input.team, &scanCurrent);
		if (!scanCurrent)
			return;
		if (teammate.owner.actor == input.actor)
			teammate = {};
	}
	input.speaking = actors_[index].confirmedSpeech.scheduledStatement();
	const auto catalogRevision = outgoingCatalogRevision_;
	const bool mayConvert = teammate.topic == compat::ChatterStatementTopic::MyPlan;
	std::array<ChatterPeerReceipt, kMaximumClientSlots> maintenancePeers{};
	if (mayConvert)
		for (std::size_t peer = 0; peer < maintenancePeers.size(); ++peer)
			maintenancePeers[peer] = snapshotChatterPeer(peer);
	const auto maintenanceCurrent = [&]()
	{
		if (!compat::sameChatterUpdateReceipt(receiptContext, reactionContext(index)) ||
			services_->service_lifecycle().slotGeneration(receiptContext.actor.slot) != receiptConnection ||
			actors_[index].confirmedSpeech.lifetimeRevision() != input.admission.ownerRevision ||
			outgoingCatalogRevision_ != catalogRevision ||
			services_->observation_publicTeamKnown(index) != receiptTeamKnown ||
			services_->observation_teamInfoTeams(index) != receiptTeam || handle.entity->v.team != receiptEntityTeam)
			return false;
		if (mayConvert)
			for (std::size_t peer = 0; peer < maintenancePeers.size(); ++peer)
			{
				auto before = maintenancePeers[peer];
				const auto now = snapshotChatterPeer(peer);
				if (peer == index)
				{
					// Our queue can change during preceding maintenance nodes. Its
					// per-draw revision check is owned by ChatterPendingQueue.
					before.queueRevision = now.queueRevision;
					before.hasPending = now.hasPending;
				}
				if (!sameChatterPeer(before, now))
					return false;
			}
		return true;
	};
	const compat::RandomTimingContext timing{services_->command_managedBotCommandSequences(index), 0U,
		services_->service_managedBotFullUpdateSequences()[index]};
	actors_[index].outgoingRadioSchedulers.maintain(input, teammate, &services_->service_compatibilityRandomSource(),
		timing, outgoingChatterCatalog_.phraseId(compat::ChatterPhrase::AgreeWithPlan), maintenanceCurrent);
	const auto retained = actors_[index].confirmedSpeech.scheduledStatement();
	if (compat::validChatterScheduleEntry(retained) &&
		!actors_[index].outgoingRadioSchedulers.statements().handle(retained).id)
		(void)actors_[index].confirmedSpeech.finishStatement(
			reactionContext(index), retained, actors_[index].confirmedSpeech.revision());
}

} // namespace metamod
} // namespace astrabot
#undef ASTRABOT_BOT_BEHAVIOR_RADIO_NEXT_PHRASE_GAP_SECONDS
