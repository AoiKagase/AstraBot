#include "runtime/behavior/bot_behavior_runtime.hpp"
#include "runtime/shared/bot_runtime_support.hpp"
#include "runtime/shared/public_nav_probe.hpp"
#include "runtime/shared/public_nav_probe.hpp"
namespace astrabot
{
namespace metamod
{
namespace
{
constexpr int kTerroristTeam = 1;
constexpr int kCounterTerroristTeam = 2;
constexpr int kCommandRadioMenu = 1;
constexpr int kGroupRadioMenu = 2;
constexpr unsigned kVipMapPrefixLength = 3U;
constexpr int kMaximumClientSlots = 32;
// Keep literal evaluation precision on x87; each macro is undefined below.
#define ASTRABOT_BOT_BEHAVIOR_RADIO_NEXT_PHRASE_GAP_SECONDS 0.1f
}
using namespace runtime_detail;

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

struct BotBehaviorRuntime::HelpEnvironment : compat::IHelpRequestEnvironment
{
	BotBehaviorRuntime &owner;
	std::size_t sender, receiver;
	std::function<bool()> sourceCurrent;
	ChatterPeerReceipt receipt;
	std::uint64_t execution, epoch;
	nav::NavSnapshot navigation;
	HelpEnvironment(BotBehaviorRuntime &runtime, std::size_t from, std::size_t to, std::function<bool()> source)
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
	void onMyWay() override
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
		compat::ChatterStatementRequest request{};
		request.source = compat::ChatterScheduleSource::Generic;
		request.topic = compat::ChatterStatementTopic::MyIntention;
		request.phrase = compat::ChatterPhrase::OnMyWay;
		request.created = receipt.context.now;
		request.start = request.created;
		request.readyAt = request.created;
		request.expires = request.created + 3.0f;
		auto &actor = owner.actors_[receiver];
		actor.outgoingRadioSchedulers.statements().admit(
			request, admission, owner.outgoingChatterCatalog_, mode, actor.confirmedSpeech.scheduledStatement());
	}
};

void BotBehaviorRuntime::transmitHelp(
	std::size_t sender, compat::ChatterMeme meme, const std::function<bool()> &current)
{
	if (meme.kind != compat::ChatterMemeKind::Help ||
		services_->service_compatibilitySurface().configuration().mode != compat::RuntimeMode::Compatibility)
		return;
	const auto engine = services_->service_engineFunctions();
	const int team = services_->observation_resolvePlayerTeam(services_->service_managedBotHandles()[sender].entity);
	if (!current())
		return;
	const int maximum = (std::min)(kMaximumClientSlots, services_->service_globals()->maxClients);
	for (int slot = 1; slot <= maximum; ++slot)
	{
		if (!current())
			return;
		const auto receiver = static_cast<std::size_t>(slot - 1);
		if (receiver == sender || !services_->service_managedBotSlots()[receiver])
			continue;
		HelpEnvironment environment(*this, sender, receiver, current);
		const auto &receipt = environment.receipt;
		auto *entity = engine->pfnPEntityOfEntIndex(slot);
		if (!environment.current())
		{
			if (!current())
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
				if (!current())
					return;
				continue;
			}
			if (!name || !*name)
				continue;
		}
		const int recipientTeam = services_->observation_resolvePlayerTeam(entity);
		if (!environment.current())
		{
			if (!current())
				return;
			continue;
		}
		if (recipientTeam != team)
			continue;
		(void)compat::respondToHelpRequest({meme.place, 3000.0f}, environment);
	}
}

compat::ChatterScheduleAdmission BotBehaviorRuntime::chatterAdmission(std::size_t index)
{
	const auto context = reactionContext(index);
	if (!context.valid())
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
		if (!peer.managed || !context.valid() || !peer.entity || !(peer.flags & FL_CLIENT) ||
			(peer.flags & FL_DORMANT) || peer.name == 0)
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

void BotBehaviorRuntime::updateOutgoingPublicRadio(std::size_t index)
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
	for (int slot = 1; slot <= (std::min)(kMaximumClientSlots, services_->service_globals()->maxClients); ++slot)
	{
		edict_t *player = services_->service_engineFunctions()->pfnPEntityOfEntIndex(slot);
		if (player == nullptr || player == handle.entity || player->free || (player->v.flags & FL_CLIENT) == 0 ||
			player->serialnumber < 0 || (player->v.flags & (FL_SPECTATOR | FL_DORMANT)) != 0 ||
			player->v.deadflag != DEAD_NO || !(player->v.health > 0))
			continue;
		if (services_->observation_resolvePlayerTeam(player) == input.team)
		{
			++input.friendCount;
		}
		else if (services_->observation_resolvePlayerTeam(player) == kTerroristTeam ||
			services_->observation_resolvePlayerTeam(player) == kCounterTerroristTeam)
			++input.enemiesRemaining;
	}
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
	if (reportNav.isValid() && reportNav.mapGeneration() == input.frame.mapGeneration)
	{
		const auto metadataPlace = [&](std::uint16_t place)
		{
			for (const auto &name : reportNav.document()->places())
				if (name.id == place)
				{
					const auto id = outgoingChatterCatalog_.nameToId(name.name);
					return id || outgoingChatterCatalog_.hasPlaceBefore() ? id
																		  : compat::defaultChatterPlaceId(name.name);
				}
			return 0U;
		};
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
		outgoingTeamLastSpeech_[input.team] = (std::max)(outgoingTeamLastSpeech_[input.team], order.receivedTime);
	}
	const char *mapName = services_->service_engineFunctions()->pfnSzFromIndex != nullptr
		? services_->service_engineFunctions()->pfnSzFromIndex(services_->service_globals()->mapname)
		: nullptr;
	if (!receiptCurrent())
		return;
	input.vipEscaping = mapName != nullptr && std::strncmp(mapName, "as_", kVipMapPrefixLength) == 0 &&
		input.team == kCounterTerroristTeam && decision.action == compat::ScenarioAction::Escape;
	input.plantingBomb = actors_[index].managedBotStateMachines.state() == compat::CompatibilityStateId::PlantBomb;
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
					transmitHelp(index, step.completion.meme, completionCurrent);
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
			const bool defined = step.playback.placePhrase ? place != nullptr : outgoingChatterCatalog_.defined(phrase);
			if (!defined)
			{
				queue.finishPart(step.token, 0, true);
				return;
			}
			const auto choice = place ? place->choice : outgoingChatterCatalog_.lookup(phrase);
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
						step.criteriaPlace, ChatterVoicePurpose::Statement, step.playback.placePhrase, &hasSample,
						&suppressed);
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
	if (!compat::sameChatterReceipt(receiptContext, currentContext) ||
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
		if (!compat::sameChatterReceipt(receiptContext, reactionContext(index)) ||
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
