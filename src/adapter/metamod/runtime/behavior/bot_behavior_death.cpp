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
	const int victimSerial = victim->serialnumber;
	const int killerSerial = killer->serialnumber;
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
		const int selfSerial = self->serialnumber;
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
		if (in.nearbyEnemies > 1 || killer == self)
			continue;
		if (self->serialnumber != selfSerial || victim->serialnumber != victimSerial ||
			killer->serialnumber != killerSerial || self->free || victim->free || killer->free ||
			frame.mapGeneration != services_->service_lifecycle().mapGeneration() ||
			frame.roundGeneration != services_->service_lifecycle().roundGeneration() || globals->time != in.now)
			continue;
		if (!killVictimVisible(index, victim, frame))
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
		praise.allowedPhrases = allowedKillPraisePhrases(mode);
		if (!services_->service_lifecycle().isMapActive() ||
			frame.mapGeneration != services_->service_lifecycle().mapGeneration() ||
			frame.roundGeneration != services_->service_lifecycle().roundGeneration() ||
			frame.tick != services_->service_adapterFrameCount() || globals->time != in.now || self->free ||
			victim->free || killer->free || self->serialnumber != selfSerial || victim->serialnumber != victimSerial ||
			killer->serialnumber != killerSerial || services_->service_managedBotHandles()[index].entity != self ||
			services_->service_managedBotHandles()[index].actor.actorGeneration != in.actor.generation)
			continue;
		actors_[index].killPraise.observe(praise, services_->service_compatibilityRandomSource());
	}
}
bool BotBehaviorRuntime::killVictimVisible(
	std::size_t index, const edict_t *victim, const world::FrameIdentity &frame) const
{
	if (!services_ || index >= actors_.size() || !victim)
		return false;
	const auto engine = services_->service_engineFunctions();
	const auto globals = services_->service_globals();
	const auto &handle = services_->service_managedBotHandles()[index];
	auto *self = handle.entity;
	if (!engine || !engine->pfnTraceLine || !globals || !self)
		return false;
	compat::PublicActorState observed{};
	if (services_->observation_publicObservations().read(
			{handle.actor.slot, handle.actor.actorGeneration}, frame, &observed) &&
		globals->time < observed.blindUntil)
		return false;
	const world::WorldPosition eye{self->v.origin.x + self->v.view_ofs.x, self->v.origin.y + self->v.view_ofs.y,
		self->v.origin.z + self->v.view_ofs.z};
	const world::WorldPosition point{victim->v.origin.x, victim->v.origin.y, victim->v.origin.z};
	constexpr float kMaximumCoordinate = world::WorldLimits::kMaximumCoordinate;
	for (float value : {eye.x, eye.y, eye.z, point.x, point.y, point.z, self->v.origin.x, self->v.origin.y})
		if (!std::isfinite(value) || std::fabs(value) > kMaximumCoordinate)
			return false;
	if (!compat::killPraiseViewCone(
			point.x - self->v.origin.x, point.y - self->v.origin.y, self->v.angles.x, self->v.angles.y) ||
		services_->observation_smokeObservations().blocks(eye, point, globals->time))
		return false;
	const float from[]{eye.x, eye.y, eye.z};
	const float to[]{point.x, point.y, point.z};
	TraceResult result{};
	constexpr int kIgnoreActorsAndGlass = 0x101;
	engine->pfnTraceLine(from, to, kIgnoreActorsAndGlass, self, &result);
	return result.flFraction == 1.0f;
}

}
}
