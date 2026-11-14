#include "runtime/behavior/bot_behavior_runtime.hpp"
#include "astrabot/compat/weapon_pickup.hpp"
#include <cmath>
#include <cstring>
namespace astrabot
{
namespace metamod
{
void BotBehaviorRuntime::observeWeaponTouch(edict_t *box, edict_t *self)
{
	// CWeaponBox::Touch invokes the reference policy only with box=self, player=other.
	if (!services_ || !box || !self || box == self || box->free || self->free || (box->v.flags & FL_CLIENT) ||
		!(box->v.flags & FL_ONGROUND) || !(self->v.flags & FL_CLIENT) || (self->v.flags & (FL_FROZEN | FL_SPECTATOR)) ||
		self->v.deadflag != DEAD_NO || self->v.health <= 0)
		return;
	const auto engine = services_->service_engineFunctions();
	const auto globals = services_->service_globals();
	const auto config = services_->service_compatibilitySurface().configuration();
	const auto &lifecycle = services_->service_lifecycle();
	if (!engine || !engine->pfnSzFromIndex || !engine->pfnIndexOfEdict || !globals || !std::isfinite(globals->time) ||
		!lifecycle.isMapActive() || config.mode != compat::RuntimeMode::Compatibility || config.botEnable <= 0 ||
		config.botStop > 0)
		return;
	const char *classname = engine->pfnSzFromIndex(box->v.classname);
	if (!classname || std::strcmp(classname, "weaponbox") != 0)
		return;
	const auto dropped = compat::primaryWeaponFromWorldModel(engine->pfnSzFromIndex(box->v.model));
	const auto current = compat::carriedPrimaryWeapon(static_cast<std::uint32_t>(self->v.weapons));
	if (!dropped || !current)
		return;
	for (std::size_t i = 0; i < actors_.size(); ++i)
	{
		const auto &handle = services_->service_managedBotHandles()[i];
		if (!services_->service_managedBotSlots()[i] || handle.entity != self)
			continue;
		if (engine->pfnIndexOfEdict(self) != static_cast<int>(handle.actor.slot) ||
			services_->service_joinControllers()[i].phase() != JoinPhase::Joined ||
			!lifecycle.isCurrent(lifecycle.tokenForSlot(handle.actor.slot)))
			return;
		constexpr int kVipScoreFlag = 4;
		const auto &score = services_->observation_publicScoreStatuses(i)[i];
		if (score.known && score.serial == static_cast<std::uint32_t>(self->serialnumber) &&
			score.frame.mapGeneration == lifecycle.mapGeneration() &&
			score.frame.roundGeneration == lifecycle.roundGeneration() && (score.flags & kVipScoreFlag))
			return;
		if (compat::shouldExchangePrimary(actors_[i].managedProfiles, static_cast<int>(current->id),
				static_cast<int>(dropped->id), globals->time - actors_[i].lastVisibleEnemyTimes))
			(void)services_->command_dispatchClientCommand(self, "drop", current->classname);
		// The original GameDLL Touch continues with its own pickup and inventory receipts.
		return;
	}
}
} // namespace metamod
} // namespace astrabot
