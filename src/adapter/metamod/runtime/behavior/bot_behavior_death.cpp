#include "runtime/behavior/bot_behavior_runtime.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace astrabot
{
namespace metamod
{
void BotBehaviorRuntime::observePlayerDeath(
	int victimSlot, int killerSlot, edict_t *recipient, const world::FrameIdentity &receiptFrame)
{
	const auto frame = receiptFrame;
	constexpr int kMaximumClientSlots = 32;
	constexpr int kTerroristTeam = 1;
	constexpr int kCounterTerroristTeam = 2;
	if (!services_)
		return;
	const auto engine = services_->service_engineFunctions();
	const auto globals = services_->service_globals();
	if (!engine || !globals || !engine->pfnPEntityOfEntIndex || !std::isfinite(globals->time) ||
		!services_->service_lifecycle().isMapActive() || !frame.isValid() ||
		frame.mapGeneration != services_->service_lifecycle().mapGeneration() ||
		frame.roundGeneration != services_->service_lifecycle().roundGeneration() ||
		frame.tick != services_->service_adapterFrameCount())
		return;
	const int maximum = (std::min)(kMaximumClientSlots, globals->maxClients);
	if (victimSlot < 1 || victimSlot > maximum || killerSlot < 0 || killerSlot > maximum)
		return;
	const auto &life = services_->service_lifecycle();
	const world::ActorKey victimConnection{static_cast<std::uint32_t>(victimSlot),
		life.slotGeneration(static_cast<std::uint32_t>(victimSlot))};
	const world::ActorKey killerConnection{static_cast<std::uint32_t>(killerSlot),
		life.slotGeneration(static_cast<std::uint32_t>(killerSlot))};
	struct ObserverReceipt
	{
		edict_t *entity = nullptr;
		int serial = -1;
		std::uint32_t actorGeneration = 0;
		world::ActorKey connection{};
		compat::ChatterScheduleAdmission admission{};
	};
	std::array<ObserverReceipt, kMaximumClientSlots> observers{};
	for (std::size_t index = 0; index < observers.size(); ++index)
	{
		const auto &handle = services_->service_managedBotHandles()[index];
		if (services_->service_managedBotSlots()[index] && handle.entity)
		{
			observers[index] = {handle.entity, handle.entity->serialnumber, handle.actor.actorGeneration,
				{static_cast<std::uint32_t>(index + 1), life.slotGeneration(static_cast<std::uint32_t>(index + 1))},
				chatterAdmission(index)};
		}
	}
	const float receiptTime = globals->time;
	auto *victim = engine->pfnPEntityOfEntIndex(victimSlot);
	auto *killer = killerSlot > 0 ? engine->pfnPEntityOfEntIndex(killerSlot) : nullptr;
	const auto player = [](const edict_t *entity)
	{
		return entity && !entity->free && entity->serialnumber >= 0 && (entity->v.flags & FL_CLIENT) != 0 &&
			(entity->v.flags & (FL_SPECTATOR | FL_DORMANT)) == 0;
	};
	if (!player(victim))
		return;
	const int victimSerial = victim->serialnumber;
	const int killerSerial = killer ? killer->serialnumber : -1;
	const int victimTeam = services_->observation_resolvePlayerTeam(victim);
	const int killerTeam = player(killer) ? services_->observation_resolvePlayerTeam(killer) : 0;
	if (victimTeam != kTerroristTeam && victimTeam != kCounterTerroristTeam)
		return;
	std::array<int, kCounterTerroristTeam + 1> alive{};
	for (int slot = 1; slot <= maximum; ++slot)
	{
		const auto *entity = engine->pfnPEntityOfEntIndex(slot);
		if (slot == victimSlot || !player(entity) || entity->v.deadflag != DEAD_NO || !(entity->v.health > 0))
			continue;
		const int team = services_->observation_resolvePlayerTeam(entity);
		if (team == kTerroristTeam || team == kCounterTerroristTeam)
			++alive[team];
	}
	const char *mode = engine->pfnCVarGetString ? engine->pfnCVarGetString("bot_chatter") : nullptr;
	const auto verbosity = compat::parseChatterVerbosity(mode);
	for (std::size_t index = 0; index < actors_.size() && index < static_cast<std::size_t>(maximum); ++index)
	{
		const auto &handle = services_->service_managedBotHandles()[index];
		const auto &original = observers[index];
		auto *self = handle.entity;
		const auto observerCurrent = [&]()
		{
			return self && original.entity == self && original.serial == self->serialnumber &&
				handle.actor.actorGeneration == original.actorGeneration &&
				receiptConnectionCurrent(original.connection) &&
				compat::sameChatterReceipt(original.admission.owner, reactionContext(index)) &&
				original.admission.ownerRevision == actors_[index].confirmedSpeech.lifetimeRevision();
		};
		if (!observerCurrent() || !services_->service_managedBotSlots()[index] || !player(self) || (recipient && recipient != self) ||
			index + 1 == static_cast<std::size_t>(victimSlot) || handle.actor.actorGeneration == 0 ||
			self->v.deadflag != DEAD_NO || !(self->v.health > 0) ||
			engine->pfnPEntityOfEntIndex(static_cast<int>(index + 1)) != self)
			continue;
		if (!receiptConnectionCurrent(victimConnection) || !receiptConnectionCurrent(killerConnection) ||
			victim->free || victim->serialnumber != victimSerial ||
			(killer && (killer->free || killer->serialnumber != killerSerial)) || globals->time != receiptTime ||
			frame.mapGeneration != services_->service_lifecycle().mapGeneration() ||
			frame.roundGeneration != services_->service_lifecycle().roundGeneration())
		{
			continue;
		}
		const int ownTeam = services_->observation_resolvePlayerTeam(self);
		if (!observerCurrent() || !receiptConnectionCurrent(victimConnection) || !receiptConnectionCurrent(killerConnection))
			continue;
		if (ownTeam == victimTeam)
		{
			observeFriendDeath(index, victim, player(killer) ? killer : nullptr, frame, victimConnection, killerConnection);
			continue;
		}
		if (!player(killer) || ownTeam != killerTeam || victimTeam == killerTeam)
		{
			continue;
		}
		const int selfSerial = self->serialnumber;
		compat::EnemyCountDeathInput in{};
		in.actor = {handle.actor.slot, handle.actor.actorGeneration};
		in.frame = frame;
		in.entityGeneration = static_cast<std::uint32_t>(self->serialnumber) + 1U;
		in.now = globals->time;
		in.alive = true;
		in.victimEnemy = true;
		in.killerFriendly = true;
		in.enabled = true;
		in.nearbyEnemies =
			actors_[index]
				.outgoingRadioContactMemory.counts(in.actor, in.frame, in.now, alive[killerTeam] - 1, alive[victimTeam])
				.enemies;
		in.timing = {services_->command_managedBotCommandSequences(index), 0U,
			services_->service_managedBotFullUpdateSequences()[index]};
		in.admission = original.admission;
		const auto remaining =
			compat::prepareRemainingEnemyStatement(in, services_->service_compatibilityRandomSource());
		if (!observerCurrent())
			continue;
		if (remaining.source != compat::ChatterScheduleSource::None)
		{
			ensureOutgoingChatterCatalog();
			if (!observerCurrent())
				continue;
			actors_[index].outgoingRadioSchedulers.statements().admit(remaining, original.admission,
				outgoingChatterCatalog_, verbosity, actors_[index].confirmedSpeech.scheduledStatement());
		}
		if (in.nearbyEnemies > 1 || killer == self)
			continue;
		if (self->serialnumber != selfSerial || victim->serialnumber != victimSerial ||
			killer->serialnumber != killerSerial || self->free || victim->free || killer->free ||
			frame.mapGeneration != services_->service_lifecycle().mapGeneration() ||
			frame.roundGeneration != services_->service_lifecycle().roundGeneration() || globals->time != in.now)
			continue;
		if (!canSeeEntityOrigin(index, victim, frame))
			continue;
		compat::KillPraiseDeathInput praise{};
		praise.actor = in.actor;
		praise.frame = frame;
		praise.entityGeneration = in.entityGeneration;
		praise.timing = in.timing;
		praise.now = in.now;
		praise.nearbyEnemies = in.nearbyEnemies;
		praise.alive = true;
		praise.victimEnemy = true;
		praise.killerFriendly = true;
		praise.killerBot = (killer->v.flags & FL_FAKECLIENT) != 0;
		praise.victimVisible = true;
		praise.careerKnown = engine->pfnIsCareerMatch != nullptr;
		praise.career = !praise.killerBot && praise.careerKnown && engine->pfnIsCareerMatch() != FALSE;
		if (!services_->service_lifecycle().isMapActive() ||
			frame.mapGeneration != services_->service_lifecycle().mapGeneration() ||
			frame.roundGeneration != services_->service_lifecycle().roundGeneration() ||
			frame.tick != services_->service_adapterFrameCount() || globals->time != in.now || self->free ||
			victim->free || killer->free || self->serialnumber != selfSerial || victim->serialnumber != victimSerial ||
			killer->serialnumber != killerSerial || services_->service_managedBotHandles()[index].entity != self ||
			services_->service_managedBotHandles()[index].actor.actorGeneration != in.actor.generation)
			continue;
		praise.admission = original.admission;
		const auto praiseStatement =
			compat::prepareKillPraiseStatement(praise, services_->service_compatibilityRandomSource());
		if (!observerCurrent())
			continue;
		if (praiseStatement.source != compat::ChatterScheduleSource::None)
		{
			ensureOutgoingChatterCatalog();
			if (!observerCurrent())
				continue;
			actors_[index].outgoingRadioSchedulers.statements().admit(praiseStatement, original.admission,
				outgoingChatterCatalog_, verbosity, actors_[index].confirmedSpeech.scheduledStatement());
		}
	}
}
}
}
