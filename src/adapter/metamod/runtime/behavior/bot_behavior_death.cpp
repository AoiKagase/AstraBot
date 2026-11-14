#include "runtime/behavior/bot_behavior_runtime.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace astrabot
{
namespace metamod
{
void BotBehaviorRuntime::observePlayerDeath(
	int victimSlot, int killerSlot, edict_t *recipient, const world::FrameIdentity &frame)
{
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
	if (victimSlot < 1 || victimSlot > maximum || killerSlot < 1 || killerSlot > maximum)
		return;
	auto *victim = engine->pfnPEntityOfEntIndex(victimSlot);
	auto *killer = engine->pfnPEntityOfEntIndex(killerSlot);
	const auto player = [](const edict_t *entity)
	{
		return entity && !entity->free && entity->serialnumber >= 0 && (entity->v.flags & FL_CLIENT) != 0 &&
			(entity->v.flags & (FL_SPECTATOR | FL_DORMANT)) == 0;
	};
	if (!player(victim) || !player(killer))
		return;
	const int victimTeam = services_->observation_resolvePlayerTeam(victim);
	const int killerTeam = services_->observation_resolvePlayerTeam(killer);
	if ((victimTeam != kTerroristTeam && victimTeam != kCounterTerroristTeam) ||
		(killerTeam != kTerroristTeam && killerTeam != kCounterTerroristTeam) || victimTeam == killerTeam)
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
	const bool enabled = !mode || (std::strcmp(mode, "off") != 0 && std::strcmp(mode, "minimal") != 0);
	for (std::size_t index = 0; index < actors_.size() && index < static_cast<std::size_t>(maximum); ++index)
	{
		const auto &handle = services_->service_managedBotHandles()[index];
		auto *self = handle.entity;
		if (!services_->service_managedBotSlots()[index] || !player(self) || (recipient && recipient != self) ||
			index + 1 == static_cast<std::size_t>(victimSlot) || handle.actor.actorGeneration == 0 ||
			self->v.deadflag != DEAD_NO || !(self->v.health > 0) ||
			engine->pfnPEntityOfEntIndex(static_cast<int>(index + 1)) != self ||
			services_->observation_resolvePlayerTeam(self) != killerTeam)
			continue;
		compat::EnemyCountDeathInput in{};
		in.actor = {handle.actor.slot, handle.actor.actorGeneration};
		in.frame = frame;
		in.entityGeneration = static_cast<std::uint32_t>(self->serialnumber) + 1U;
		in.now = globals->time;
		in.alive = true;
		in.victimEnemy = true;
		in.killerFriendly = true;
		in.enabled = enabled;
		in.nearbyEnemies =
			actors_[index]
				.outgoingRadioContactMemory.counts(in.actor, in.frame, in.now, alive[killerTeam] - 1, alive[victimTeam])
				.enemies;
		in.timing = {services_->command_managedBotCommandSequences(index), 0U,
			services_->service_managedBotFullUpdateSequences()[index]};
		actors_[index].enemyCountChatter.observe(in, services_->service_compatibilityRandomSource());
	}
}
}
}
