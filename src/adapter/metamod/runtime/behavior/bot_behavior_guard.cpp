#include "runtime/behavior/bot_behavior_runtime.hpp"
#include "astrabot/compat/nav_area_lookup.hpp"
#include "runtime/shared/public_nav_probe.hpp"
#include "runtime/shared/bot_runtime_support.hpp"
#include "runtime/behavior/bot_look_geometry.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
namespace astrabot
{
namespace metamod
{
namespace
{
constexpr int kScoutWeaponId = 3;
constexpr int kSg550WeaponId = 13;
constexpr int kAwpWeaponId = 18;
constexpr int kG3sg1WeaponId = 24;
constexpr int kIgnoreMonstersAndGlassTrace = 0x101;
// Keep literal evaluation precision on x87; each macro is undefined below.
#define ASTRABOT_BOT_BEHAVIOR_GUARD_DEGREES_TO_RADIANS .01745329252f
constexpr int kOpenYawDirections = 8;
constexpr float kOpenYawStepDegrees = 45.0f;
constexpr int kOpenYawRayLength = 1000;
constexpr int kContainingAreaTolerance = 120;
constexpr int kMaximumApproachPoints = 16;
constexpr int kC4WeaponId = 6;
constexpr unsigned kUnarmedInventoryMask = (1U << 6) | (1U << 4) | (1U << 9) | (1U << 25);

}
void BotBehaviorRuntime::rememberLooseBombDrop(world::EntityKey box, world::FrameIdentity frame, nav::NavVector origin)
{
	if (!looseBombDrop_.matches(box, frame))
		looseBombDrop_ = {box, frame, origin};
}

void BotBehaviorRuntime::observeLooseBombModel(edict_t *box, const char *model)
{
	const auto engine = services_->service_engineFunctions();
	const auto globals = services_->service_globals();
	if (!box || box->free || box->serialnumber < 0 || !engine || !engine->pfnIndexOfEdict ||
		!engine->pfnSzFromIndex || !globals ||
		services_->service_compatibilitySurface().configuration().mode != compat::RuntimeMode::Compatibility)
		return;
	const auto &life = services_->service_lifecycle();
	const world::FrameIdentity frame{life.mapGeneration(), life.roundGeneration(), services_->service_adapterFrameCount()};
	const auto serial = box->serialnumber;
	const auto classname = box->v.classname, modelIndex = box->v.model;
	const auto current = [&] {
		return services_->service_engineFunctions() == engine && services_->service_globals() == globals &&
			frame.mapGeneration == life.mapGeneration() &&
			frame.roundGeneration == life.roundGeneration() && !box->free && box->serialnumber == serial &&
			box->v.classname == classname && box->v.model == modelIndex;
	};
	const auto name = engine->pfnSzFromIndex(classname);
	if (!current() || !ObservationAdapter::isLooseBombBox(box, name, model))
		return;
	const auto actual = engine->pfnSzFromIndex(modelIndex);
	if (!current() || !actual || std::strcmp(actual, model) != 0)
		return;
	const int id = engine->pfnIndexOfEdict(box);
	if (!current() || id <= globals->maxClients || id >= globals->maxEntities)
		return;
	const world::EntityKey key{static_cast<unsigned>(id), static_cast<unsigned>(serial) + 1U};
	rememberLooseBombDrop(key, frame, {box->v.origin.x, box->v.origin.y, box->v.origin.z});
	(void)looseBombArea(services_->service_navPublisher().snapshot(), current);
}

nav::AreaId BotBehaviorRuntime::looseBombArea(const nav::NavSnapshot &navigation, const std::function<bool()> &current)
{
	const auto drop = looseBombDrop_;
	if (!navigation.isValid() || !drop.box.isValid() || navigation.mapGeneration() != drop.frame.mapGeneration)
		return 0;
	if (navigation.revision() == drop.navRevision)
		return drop.area;
	const auto sameDrop = [&] {
		return current() && looseBombDrop_.matches(drop.box, drop.frame) &&
			services_->service_navPublisher().snapshot().revision() == navigation.revision();
	};
	PublicNavProbe probe(services_->service_engineFunctions(), sameDrop);
	const auto area = compat::findBotNavArea(navigation, drop.origin, &probe, false);
	if (!sameDrop())
		return 0;
	looseBombDrop_.area = area;
	looseBombDrop_.navRevision = navigation.revision();
	return area;
}

bool BotBehaviorRuntime::guardedBombPosition(std::size_t index, nav::NavVector *position) const
{
	const auto &actor = actors_[index];
	if (actor.managedBotStateMachines.task() != compat::CompatibilityTaskId::GuardLooseBomb)
		return false;
	const auto drop = looseBombDrop_;
	const auto engine = services_->service_engineFunctions();
	const auto globals = services_->service_globals();
	if (!drop.box.isValid() || drop.frame.mapGeneration != services_->service_lifecycle().mapGeneration() ||
		drop.frame.roundGeneration != services_->service_lifecycle().roundGeneration() ||
		!engine || !engine->pfnPEntityOfEntIndex || !engine->pfnSzFromIndex || !globals ||
		globals->maxEntities <= 0 || drop.box.id >= static_cast<unsigned>(globals->maxEntities))
		return false;
	const auto *box = engine->pfnPEntityOfEntIndex(static_cast<int>(drop.box.id));
	const auto sameBox = [&] {
		const auto &life = services_->service_lifecycle();
		return services_->service_engineFunctions() == engine && services_->service_globals() == globals &&
			life.mapGeneration() == drop.frame.mapGeneration && life.roundGeneration() == drop.frame.roundGeneration &&
			looseBombDrop_.matches(drop.box, drop.frame) && box && !box->free && box->serialnumber >= 0 &&
			static_cast<unsigned>(box->serialnumber) + 1U == drop.box.generation;
	};
	if (!sameBox())
		return false;
	const auto classname = box->v.classname, model = box->v.model;
	const char *name = engine->pfnSzFromIndex(classname);
	if (!sameBox() || !name || std::strcmp(name, "weaponbox") != 0)
		return false;
	const char *modelName = engine->pfnSzFromIndex(model);
	if (!sameBox() || box->v.classname != classname || box->v.model != model ||
		!ObservationAdapter::isLooseBombBox(box, name, modelName))
		return false;
	const auto origin = box->v.origin;
	*position = {origin.x, origin.y, origin.z};
	return true;
}

void BotBehaviorRuntime::updateGuardLook(ManagedActionContext &ctx)
{
	auto &state = actors_[ctx.index];
	const auto &scenario = state.scenarioDecisions;
	auto *self = ctx.handle->entity;
	auto *engine = services_->service_engineFunctions();
	if (scenario.state != compat::CompatibilityStateId::Hide || !self || !engine || !engine->pfnTraceLine ||
		ctx.before.dead || (ctx.before.flags & FL_FROZEN) != 0 || state.weaponActions.grenadeInProgress())
		return;
	// A guard must have a usable weapon before settling into an armed watch.
	const auto inventory = static_cast<std::uint32_t>(self->v.weapons);
	if ((inventory & ~kUnarmedInventoryMask) == 0)
		return;
	compat::PublicActorState equipment{};
	if (services_->observation_publicObservations().read(ctx.actor, ctx.frame, &equipment))
		(void)decideEquipmentAction(ctx, equipment, false);
	const auto &weapon = services_->observation_managedBotCurrentWeapons(ctx.index);
	if (weapon.available && weapon.active && weapon.weaponId == kC4WeaponId)
	{
		const std::uint32_t primary = (1U << 3) | (1U << 5) | (1U << 7) | (1U << 8) | (1U << 12) | (1U << 13) |
			(1U << 14) | (1U << 15) | (1U << 18) | (1U << 19) | (1U << 20) | (1U << 21) | (1U << 22) | (1U << 23) |
			(1U << 24) | (1U << 27) | (1U << 28) | (1U << 30);
		const std::uint32_t pistols = (1U << 1) | (1U << 10) | (1U << 11) | (1U << 16) | (1U << 17) | (1U << 26);
		ctx.proposal.kind = (inventory & primary) != 0 ? ActionKind::SelectPrimary
			: (inventory & pistols) != 0			   ? ActionKind::SelectPistol
													   : ActionKind::SelectKnife;
	}
	if (state.noiseLook.active() || state.noiseLook.inhibited(services_->service_globals()->time))
		return;
	const auto navigation = services_->service_navPublisher().snapshot();
	const float now = services_->service_globals()->time;
	const nav::NavVector spot = {scenario.target.position.x, scenario.target.position.y, scenario.target.position.z};
	if (!state.guardLook.current(ctx.actor, ctx.frame, scenario.target.entity, spot, navigation.revision(), now))
	{
		const nav::NavVector eye = {self->v.origin.x + self->v.view_ofs.x, self->v.origin.y + self->v.view_ofs.y,
			self->v.origin.z + self->v.view_ofs.z};
		float openYaw = 0, best = 0;
		for (unsigned direction = 0; direction < kOpenYawDirections; ++direction)
		{
			const float angle = direction * kOpenYawStepDegrees * ASTRABOT_BOT_BEHAVIOR_GUARD_DEGREES_TO_RADIANS;
			const float a[] = {eye.x, eye.y, eye.z},
						b[] = {eye.x + kOpenYawRayLength * std::cos(angle), eye.y + kOpenYawRayLength * std::sin(angle),
							eye.z};
			TraceResult trace{};
			engine->pfnTraceLine(a, b, kIgnoreMonstersAndGlassTrace, self, &trace);
			if (!trace.fStartSolid && !trace.fAllSolid && trace.flFraction > best)
			{
				best = trace.flFraction;
				openYaw = direction * kOpenYawStepDegrees;
			}
		}
		std::vector<nav::NavVector> points;
		if (navigation.isValid())
		{
			nav::NavQuery query(navigation);
			nav::NavAreaMatch match{};
			const nav::NavArea *area = nullptr;
			if (query.findContaining(spot, kContainingAreaTolerance, &match) == nav::NavQueryResult::Found)
				area = navigation.document()->findArea(match.area);
			if (area)
				for (const auto &approach : area->approaches)
				{
					if (points.size() >= kMaximumApproachPoints)
						break;
					const auto *here = navigation.document()->findArea(approach.here);
					const auto *previous = navigation.document()->findArea(approach.previous);
					if (!here || !previous)
						continue;
					const auto point = compat::guardApproachPoint(*previous, *here, approach.previousToHereHow);
					nav::NavVector bend{};
					if (bendLookApproach(
							*engine, self, {self->v.origin.x, self->v.origin.y, self->v.origin.z}, point, &bend))
						points.push_back(bend);
				}
		}
		state.guardLook.begin(
			ctx.actor, ctx.frame, scenario.target.entity, spot, navigation.revision(), now, openYaw, points);
	}
	const bool sniper = weapon.available &&
		(weapon.weaponId == kScoutWeaponId || weapon.weaponId == kSg550WeaponId || weapon.weaponId == kAwpWeaponId ||
			weapon.weaponId == kG3sg1WeaponId);
	float pitch = ctx.movementAngles.pitch, yaw = ctx.movementAngles.yaw;
	if (state.guardLook.update(now, sniper,
			{self->v.origin.x + self->v.view_ofs.x, self->v.origin.y + self->v.view_ofs.y,
				self->v.origin.z + self->v.view_ofs.z},
			self->v.v_angle.x, self->v.v_angle.y, services_->service_compatibilityRandomSource(),
			{services_->command_managedBotCommandSequences(ctx.index), 0,
				services_->service_managedBotFullUpdateSequences()[ctx.index]},
			&pitch, &yaw))
	{
		ctx.proposal.viewAngles = {pitch, yaw, 0};
		ctx.proposal.hasGuardLook = true;
		ctx.proposal.movementButtons =
			static_cast<std::uint16_t>((ctx.proposal.movementButtons & ~(IN_JUMP | IN_ATTACK | IN_ATTACK2)) | IN_DUCK);
		if (services_->service_runtimeProfiler().enabled())
			runtime_detail::profiledLogConsole(services_->service_runtimeProfiler(), services_->service_pluginId(),
				"guard look slot=%u frame=%u now=%.3f view=(%.2f %.2f)", ctx.actor.slot, ctx.frame.tick, now, pitch,
				yaw);
	}
}
}
}
#undef ASTRABOT_BOT_BEHAVIOR_GUARD_DEGREES_TO_RADIANS
