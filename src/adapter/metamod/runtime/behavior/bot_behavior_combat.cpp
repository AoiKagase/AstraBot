#include "runtime/behavior/bot_behavior_runtime.hpp"
#include "runtime/shared/bot_runtime_support.hpp"
namespace astrabot
{
namespace metamod
{
using namespace runtime_detail;
namespace
{
constexpr int kProfilePercentageScale = 100;
constexpr float kSkillPercentageScale = 100.0f;
constexpr float kDuckedFeetOffset = 18.0f;
constexpr float kStandingFeetOffset = 36.0f;
// Keep literal evaluation precision on x87; each macro is undefined below.
#define ASTRABOT_BOT_BEHAVIOR_COMBAT_MAXIMUM_COMBAT_ELAPSED_SECONDS .1f
constexpr float kDefaultFieldOfView = 90.0f;
constexpr int kTraceIncludingActors = 0;
struct CrouchTraceContext
{
	enginefuncs_t *engine;
	edict_t *self;
	edict_t *enemy;
	bool ducked;
};
bool crouchTargetVisible(void *opaque)
{
	constexpr float kCrouchEyeHeightReduction = 20.0f;
	constexpr int kTraceIgnoringActors = 1;
	const auto &ctx = *static_cast<CrouchTraceContext *>(opaque);
	if (!ctx.engine || !ctx.engine->pfnTraceLine || !ctx.self || !ctx.enemy)
		return false;
	const float start[] = {ctx.self->v.origin.x, ctx.self->v.origin.y,
		ctx.self->v.origin.z - (ctx.ducked ? 0.0f : kCrouchEyeHeightReduction)};
	const float end[] = {ctx.enemy->v.origin.x + ctx.enemy->v.view_ofs.x,
		ctx.enemy->v.origin.y + ctx.enemy->v.view_ofs.y, ctx.enemy->v.origin.z + ctx.enemy->v.view_ofs.z};
	TraceResult trace{};
	ctx.engine->pfnTraceLine(start, end, kTraceIgnoringActors, ctx.self, &trace);
	return trace.flFraction >= 1.0f && trace.fStartSolid == 0 && trace.fAllSolid == 0;
}
}

void BotBehaviorRuntime::applyNativeLadderLookCommand(
	std::size_t index, runtime::BotCommand &command, bool localMovement)
{
	if (!services_ || index >= actors_.size() || !services_->service_globals())
		return;
	const auto config = services_->service_compatibilitySurface().configuration();
	const auto handle = services_->service_managedBotHandles()[index];
	const float now = services_->service_globals()->time;
	if (config.mode != compat::RuntimeMode::Compatibility || config.botEnable <= 0 || config.botStop > 0 ||
		!handle.entity || handle.entity->free || handle.entity->v.health <= 0 || handle.entity->v.deadflag != DEAD_NO ||
		(handle.entity->v.flags & (FL_FROZEN | FL_SPECTATOR)) || !std::isfinite(now) ||
		(services_->service_roundFreezeGeneration() == services_->service_lifecycle().roundGeneration() &&
			now < services_->service_roundFreezeUntil()))
		return;
	const auto receipt = reactionContext(index);
	const auto revision = actors_[index].executionRevision;
	const auto epoch = actors_[index].managedBotStateMachines.normalStateEpoch();
	nav::NativeLadderLook ladder{};
	if (!services_->navigation_nativeLadderLook(index, &ladder) || !executionCurrent(index, receipt, revision) ||
		actors_[index].managedBotStateMachines.normalStateEpoch() != epoch)
		return;
	auto &state = actors_[index];
	if (state.lastCombatTimes != now)
	{
		combat::CombatDecisionContext context{};
		context.actor = {handle.actor.slot, handle.actor.actorGeneration};
		context.frame = receipt.frame;
		context.origin = {handle.entity->v.origin.x, handle.entity->v.origin.y, handle.entity->v.origin.z};
		context.currentYaw = handle.entity->v.v_angle.y;
		context.currentPitch = handle.entity->v.v_angle.x;
		context.elapsedSeconds = state.lastCombatTimes > 0
			? (std::max)(0.0f, (std::min)(.1f, now - state.lastCombatTimes))
			: runtime::BotTimingScheduler::kCommandInterval;
		context.skill = static_cast<float>(state.managedProfiles.skill) / kSkillPercentageScale;
		compat::AimStep look{};
		if (!state.managedBotCombat.upkeepLook(context, ladder.yaw, ladder.pitchOverride ? ladder.pitch : 0.0f,
				state.managedBotStateMachines.attackOverlayActive(), &look))
			return;
		state.lastCombatTimes = now;
		const auto move = ActionAdapter::reprojectMovement(
			command.movement.forward, command.movement.side, command.viewAngles.yaw, look.yaw);
		if (!localMovement)
		{
			command.movement.forward = move.forward;
			command.movement.side = move.side;
			command.movement.buttons = static_cast<std::uint16_t>(
				(command.movement.buttons & ~(IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT)) |
				ActionAdapter::movementButtons(move.forward, move.side));
		}
		command.viewAngles = {look.pitch, look.yaw, 0};
	}
	command.synchronizeCombatView = true;
}

CombatUpkeepUpdate BotBehaviorRuntime::upkeepCombat(
	std::size_t index, const runtime::MovementPhysicsState &before, std::uint16_t cachedButtons)
{
	CombatUpkeepUpdate update{};
	if (before.dead)
	{
		clearCombatUpkeep(index);
		update.setPrimary = true;
		return update;
	}
	if (index >= actors_.size() || !services_ || !services_->service_globals() || before.dead ||
		(before.flags & FL_FROZEN) != 0 || (cachedButtons & (IN_USE | IN_RELOAD | IN_ATTACK2)) != 0)
		return update;
	auto &state = actors_[index];
	const auto tracked = state.managedBotCombat.trackedAim();
	if (!tracked.isValid() || state.weaponActions.grenadeInProgress())
		return update;
	const auto &handle = services_->service_managedBotHandles()[index];
	const auto &engine = services_->service_engineFunctions();
	const float now = services_->service_globals()->time;
	const world::FrameIdentity frame = {services_->service_lifecycle().mapGeneration(),
		services_->service_lifecycle().roundGeneration(), services_->service_adapterFrameCount()};
	if (!handle.entity || !engine || !engine->pfnPEntityOfEntIndex ||
		!(tracked.actor == world::ActorKey{handle.actor.slot, handle.actor.actorGeneration}) ||
		tracked.frame.mapGeneration != frame.mapGeneration || tracked.frame.roundGeneration != frame.roundGeneration ||
		(services_->service_roundFreezeGeneration() == frame.roundGeneration &&
			now < services_->service_roundFreezeUntil()))
		return update;
	compat::PublicActorState activeWeapon{};
	if (!services_->observation_publicObservations().read(tracked.actor, frame, &activeWeapon) ||
		!activeWeapon.weaponKnown || activeWeapon.weaponId != state.weaponActions.currentWeaponId() ||
		activeWeapon.blindUntil > now)
	{
		state.managedBotCombat.clearAim();
		state.weaponActions.stopRapidFire();
		update.setPrimary = true;
		return update;
	}
	const auto token = services_->service_lifecycle().tokenForSlot(tracked.target.slot);
	const auto generation = token.slotGeneration == 0U ? 1U : token.slotGeneration;
	edict_t *enemy = engine->pfnPEntityOfEntIndex(static_cast<int>(tracked.target.slot));
	perception::VisionObservation visible{};
	const int ownTeam = services_->observation_resolvePlayerTeam(handle.entity);
	const bool friendly = enemy && ownTeam != 0 && services_->observation_resolvePlayerTeam(enemy) == ownTeam;
	const bool validEnemy = generation == tracked.target.generation && reactionEnemyCurrent(tracked.target) && enemy && !enemy->free &&
		enemy->serialnumber >= 0 && enemy->v.deadflag == DEAD_NO && enemy->v.health > 0 && !friendly;
	const bool currentlyVisible = validEnemy &&
		services_->observation_observationAdapter().collectVisibility(
			handle.entity, enemy, tracked.target, frame, &visible,
			!(currentEnemySource(index) == world::EntityKey{tracked.target.slot,
				static_cast<std::uint32_t>(enemy->serialnumber) + 1U})) == ObservationAdapterResult::Accepted && visible.visible;
	const auto reaction = reactionContext(index);
	const bool remembered = services_->service_compatibilitySurface().configuration().mode == compat::RuntimeMode::Compatibility &&
		validEnemy && state.enemyReaction.enemy(reaction) == tracked.target &&
		state.enemyReaction.source(reaction) == world::EntityKey{tracked.target.slot,
			static_cast<std::uint32_t>(enemy->serialnumber) + 1U} && state.enemyReaction.shouldAim(reaction);
	if (!currentlyVisible && !remembered)
	{
		state.managedBotCombat.clearAim();
		state.weaponActions.stopRapidFire();
		update.setPrimary = true;
		return update;
	}
	combat::TargetBelief target{};
	target.target = tracked.target;
	target.state = currentlyVisible ? combat::TargetBeliefState::Visible : combat::TargetBeliefState::Remembered;
	target.confidence = 1.0f;
	target.position = currentlyVisible ? world::WorldPosition{enemy->v.origin.x, enemy->v.origin.y, enemy->v.origin.z}
		: state.enemyReaction.position(reaction);
	const world::WorldPosition enemyOrigin = target.position;
	compat::CompatibilityObservation weaponObservation{};
	const compat::ObservationTimingContext timing = {services_->command_managedBotCommandSequences(index), frame.tick,
		services_->service_managedBotFullUpdateSequences()[index]};
	services_->observation_observationAdapter().collectActor(
		handle.entity, tracked.actor, frame, timing, &weaponObservation);
	if (currentlyVisible)
	{
		target.position = compat::combatAimPoint(enemyOrigin,
			{handle.entity->v.origin.x, handle.entity->v.origin.y, handle.entity->v.origin.z}, visible.visibleParts,
			state.weaponActions.currentWeaponId(),
			static_cast<float>(state.managedProfiles.skill) / kProfilePercentageScale,
			before.ducked ? kDuckedFeetOffset : kStandingFeetOffset, weaponObservation.weapon.accuracy.isAvailable(),
			weaponObservation.weapon.accuracy.value);
	}
	if (currentlyVisible && engine->pfnTraceLine && ownTeam != 0)
	{
		const float eye[] = {handle.entity->v.origin.x + handle.entity->v.view_ofs.x,
			handle.entity->v.origin.y + handle.entity->v.view_ofs.y,
			handle.entity->v.origin.z + handle.entity->v.view_ofs.z};
		const float end[] = {target.position.x, target.position.y, target.position.z};
		TraceResult shot{};
		engine->pfnTraceLine(eye, end, kTraceIncludingActors, handle.entity, &shot);
		if (shot.pHit && shot.pHit != handle.entity && shot.pHit->v.deadflag == DEAD_NO &&
			services_->observation_resolvePlayerTeam(shot.pHit) == ownTeam)
		{
			state.weaponActions.stopRapidFire();
			update.setPrimary = true;
			return update;
		}
	}
	combat::CombatDecisionContext context{};
	context.actor = {handle.actor.slot, handle.actor.actorGeneration};
	context.frame = frame;
	context.origin = {handle.entity->v.origin.x, handle.entity->v.origin.y, handle.entity->v.origin.z};
	context.aimTrackingEnabled = true;
	context.currentPitch = handle.entity->v.v_angle.x;
	context.currentYaw = handle.entity->v.v_angle.y;
	context.elapsedSeconds = (std::max)(
		0.0f, (std::min)(ASTRABOT_BOT_BEHAVIOR_COMBAT_MAXIMUM_COMBAT_ELAPSED_SECONDS, now - state.lastCombatTimes));
	context.timestamp = now;
	context.skill = static_cast<float>(state.managedProfiles.skill) / kSkillPercentageScale;
	context.fieldOfView = handle.entity->v.fov > 0 ? handle.entity->v.fov : kDefaultFieldOfView;
	context.randomSource = &services_->service_compatibilityRandomSource();
	context.randomTiming = {services_->command_managedBotCommandSequences(index), frame.tick,
		services_->service_managedBotFullUpdateSequences()[index]};
	context.observerVelocity = {handle.entity->v.velocity.x, handle.entity->v.velocity.y, handle.entity->v.velocity.z};
	context.targetVelocity = currentlyVisible
		? world::WorldVelocity{enemy->v.velocity.x, enemy->v.velocity.y, enemy->v.velocity.z} : world::WorldVelocity{};
	context.predictMovement = currentlyVisible;
	context.aimErrorRange = std::sqrt((enemyOrigin.x - context.origin.x) * (enemyOrigin.x - context.origin.x) +
		(enemyOrigin.y - context.origin.y) * (enemyOrigin.y - context.origin.y) +
		(enemyOrigin.z - context.origin.z) * (enemyOrigin.z - context.origin.z));
	const auto lookReceipt = reactionContext(index);
	const auto lookExecution = state.executionRevision;
	const auto lookEpoch = state.managedBotStateMachines.normalStateEpoch();
	nav::NativeLadderLook ladder{};
	const bool ladderLook = services_->navigation_nativeLadderLook(index, &ladder);
	if (!executionCurrent(index, lookReceipt, lookExecution) ||
		state.managedBotStateMachines.normalStateEpoch() != lookEpoch)
		return update;
	if (ladderLook)
	{
		context.lookOverride = true;
		context.lookPitchOverride = ladder.pitchOverride;
		context.lookYaw = ladder.yaw;
		context.lookPitch = ladder.pitch;
	}
	combat::AimIntent aim{};
	if (state.managedBotCombat.upkeepAim(target, context, &aim))
	{
		state.lastCombatTimes = now;
		update.hasAim = true;
		update.angles = {aim.pitch, aim.yaw, 0};
		update.movementRelativeToAim = state.managedBotCombat.movementActive();
		if (!currentlyVisible)
		{
			update.setPrimary = true;
			state.weaponActions.stopRapidFire();
		}
		else if (state.weaponActions.rapidFiring())
		{
			update.setPrimary = true;
			update.primary = (cachedButtons & IN_ATTACK) == 0U &&
				(!context.lookOverride ||
					compat::combatAimOnTarget(aim.point, context.origin, aim.yaw,
						state.weaponActions.currentWeaponId() == 3 || state.weaponActions.currentWeaponId() == 18 ||
							state.weaponActions.currentWeaponId() == 13 ||
							state.weaponActions.currentWeaponId() == 24));
		}
		if (services_->service_runtimeProfiler().enabled() && gpMetaUtilFuncs && gpMetaUtilFuncs->pfnLogConsole &&
			services_->service_pluginId())
			profiledLogConsole(services_->service_runtimeProfiler(), services_->service_pluginId(),
				"combat upkeep slot=%u frame=%u full=%u now=%.3f dt=%.4f target=%u skill=%.2f "
				"enemy=(%.2f %.2f %.2f) relative=(%.2f %.2f %.2f) point=(%.2f %.2f %.2f) view=(%.2f %.2f) rapid=%d primary=%d",
				handle.actor.slot, frame.tick, services_->service_managedBotFullUpdateSequences()[index], now,
				context.elapsedSeconds, tracked.target.slot, context.skill, enemyOrigin.x, enemyOrigin.y, enemyOrigin.z,
				context.targetVelocity.x - context.observerVelocity.x,
				context.targetVelocity.y - context.observerVelocity.y,
				context.targetVelocity.z - context.observerVelocity.z, aim.point.x, aim.point.y, aim.point.z, aim.pitch,
				aim.yaw, state.weaponActions.rapidFiring() ? 1 : 0, update.setPrimary ? (update.primary ? 1 : 0) : -1);
	}
	return update;
}

bool BotBehaviorRuntime::cancelHeldGrenade(
	std::size_t index, const runtime::ViewAngles &movementAngles, ActionProposal &proposal)
{

	if (index >= services_->service_managedBotHandles().size() || !actors_[index].weaponActions.grenadeInProgress())
		return false;
	const FakeClientHandle &cancelHandle = services_->service_managedBotHandles()[index];
	compat::WeaponActionInput cancel = {};
	cancel.actor = {cancelHandle.actor.slot, cancelHandle.actor.actorGeneration};
	cancel.frame = {services_->service_lifecycle().mapGeneration(), services_->service_lifecycle().roundGeneration(),
		services_->service_adapterFrameCount()};
	cancel.timestamp = services_->service_globals() != nullptr ? services_->service_globals()->time : 0.0f;
	cancel.weaponId = actors_[index].weaponActions.currentWeaponId();
	compat::PublicActorState activeWeapon = {};
	if (services_->observation_publicObservations().read(cancel.actor, cancel.frame, &activeWeapon) &&
		activeWeapon.weaponKnown && activeWeapon.weaponId != static_cast<int>(cancel.weaponId))
	{
		cancel.weaponId = static_cast<std::uint16_t>(activeWeapon.weaponId);
		(void)actors_[index].weaponActions.update(cancel);
		return false;
	}
	const auto ammoDefinition = services_->observation_publicObservations().definition(cancel.weaponId);
	if (activeWeapon.bound && ammoDefinition.known && ammoDefinition.ammoIndex >= 0 &&
		static_cast<std::size_t>(ammoDefinition.ammoIndex) < activeWeapon.ammo.size() &&
		activeWeapon.ammoKnown[static_cast<std::size_t>(ammoDefinition.ammoIndex)])
	{
		const auto ammoIndex = static_cast<std::size_t>(ammoDefinition.ammoIndex);
		cancel.reserveKnown = true;
		cancel.reserve = static_cast<std::uint16_t>(activeWeapon.ammo[ammoIndex]);
		cancel.reserveObservationTick = activeWeapon.ammoTicks[ammoIndex];
	}
	const auto canceled = actors_[index].weaponActions.update(cancel);
	proposal.kind = canceled.primary ? ActionKind::Fire : ActionKind::None;
	proposal.stopForWeapon = canceled.stopMovement;
	if (canceled.hasRetainedGrenadeAim)
	{
		proposal.viewAngles = {canceled.grenadePitch, canceled.grenadeYaw, movementAngles.roll};
		proposal.hasCombatAim = true;
	}
	return true;
}

ActionProposal BotBehaviorRuntime::decideCombatAction(ManagedActionContext &ctx)
{
	constexpr int kP228 = 1;
	constexpr int kScout = 3;
	constexpr int kHeGrenade = 4;
	constexpr int kXm1014 = 5;
	constexpr int kMac10 = 7;
	constexpr int kAug = 8;
	constexpr int kSmokeGrenade = 9;
	constexpr int kElite = 10;
	constexpr int kFiveSeven = 11;
	constexpr int kUmp45 = 12;
	constexpr int kSg550 = 13;
	constexpr int kGalil = 14;
	constexpr int kFamas = 15;
	constexpr int kUsp = 16;
	constexpr int kGlock18 = 17;
	constexpr int kAwp = 18;
	constexpr int kMp5 = 19;
	constexpr int kM249 = 20;
	constexpr int kM3 = 21;
	constexpr int kM4a1 = 22;
	constexpr int kTmp = 23;
	constexpr int kG3sg1 = 24;
	constexpr int kFlashbang = 25;
	constexpr int kDeagle = 26;
	constexpr int kSg552 = 27;
	constexpr int kAk47 = 28;
	constexpr int kKnife = 29;
	constexpr int kP90 = 30;
	constexpr unsigned kWeaponClipCapacity = 255U;
	constexpr float kAimUpkeepRateHz = 30.0f;
	constexpr int kMovingSpeedThreshold = 10;
	constexpr float kStationarySpeedThreshold = 10.0f;
#define ASTRABOT_BOT_BEHAVIOR_COMBAT_DEGREES_TO_RADIANS .01745329251994329577f
	constexpr unsigned kPistolWeaponIds[] = {1U, 10U, 11U, 16U, 17U, 26U};
	constexpr float kKnifeAttackRange = 75.0f;
#define ASTRABOT_BOT_BEHAVIOR_COMBAT_PI 3.14159265358979323846f
	constexpr float kHalfTurnDegrees = 180.0f;
	constexpr float kMinimumKnifeRoll = 0.0f;
	constexpr float kMaximumKnifeRoll = 100.0f;
	constexpr unsigned kCombatLogFrameInterval = 16U;
	const std::size_t index = ctx.index;
	const auto &before = ctx.before;
	const auto &movementAngles = ctx.movementAngles;
	auto &proposal = ctx.proposal;
	auto &compatObservation = ctx.observation;
	const auto &handle = *ctx.handle;
	const auto &frame = ctx.frame;
	const auto &actor = ctx.actor;
	auto &snapshot = ctx.snapshot;
	auto &targetActor = ctx.targetActor;
	auto &targetPosition = ctx.targetPosition;
	const int team = ctx.team;
	combat::WeaponInventory inventory;
	combat::WeaponRecord weapon = {};
	compat::PublicActorState observedWeapon{};
	if (!services_->observation_publicObservations().read(actor, frame, &observedWeapon) || !observedWeapon.weaponKnown)
		return proposal;
	const auto &state = actors_[index].managedBotStateMachines;
	if (state.state() == compat::CompatibilityStateId::Hide &&
		actors_[index].combatRetreat.hideEpoch == state.normalStateEpoch() && reactionIgnoring(index))
		return proposal;
	const int weaponValue = observedWeapon.weaponId;
	weapon.weapon = {static_cast<std::uint16_t>(weaponValue)};
	const compat::PublicWeaponDefinition definition =
		services_->observation_publicObservations().definition(weaponValue);
	weapon.slot = static_cast<std::uint8_t>(definition.known ? definition.slot : 0);
	switch (weaponValue)
	{
	case kP228:
	case kElite:
	case kFiveSeven:
	case kUsp:
	case kGlock18:
	case kDeagle:
		weapon.kind = combat::WeaponKind::Pistol;
		break;
	case kXm1014:
	case kM3:
		weapon.kind = combat::WeaponKind::Shotgun;
		break;
	case kMac10:
	case kUmp45:
	case kMp5:
	case kTmp:
	case kP90:
		weapon.kind = combat::WeaponKind::Smg;
		break;
	case kScout:
	case kSg550:
	case kAwp:
	case kG3sg1:
		weapon.kind = combat::WeaponKind::Sniper;
		break;
	case kM249:
		weapon.kind = combat::WeaponKind::Heavy;
		break;
	case kKnife:
		weapon.kind = combat::WeaponKind::Knife;
		break;
	case kHeGrenade:
	case kSmokeGrenade:
	case kFlashbang:
		weapon.kind = combat::WeaponKind::Grenade;
		break;
	case kAug:
	case kGalil:
	case kFamas:
	case kM4a1:
	case kSg552:
	case kAk47:
		weapon.kind = combat::WeaponKind::Rifle;
		break;
	default:
		return proposal;
	}
	weapon.availability = combat::WeaponAvailability::Available;
	weapon.priority = 1U;
	const bool knife = weapon.kind == combat::WeaponKind::Knife;
	const bool reserveKnown = definition.known && definition.ammoIndex >= 0 &&
		observedWeapon.ammoKnown[static_cast<std::size_t>(definition.ammoIndex)];
	const std::uint16_t reserve = reserveKnown
		? static_cast<std::uint16_t>(observedWeapon.ammo[static_cast<std::size_t>(definition.ammoIndex)])
		: 0U;
	weapon.ammo = {static_cast<std::uint16_t>(knife ? 1 : (std::max)(0, observedWeapon.clip)), reserve,
		kWeaponClipCapacity, knife || observedWeapon.clip >= 0, reserveKnown};
	weapon.reloadState = compatObservation.weapon.reloadState.isAvailable() ? compatObservation.weapon.reloadState.value
																			: combat::ReloadState::Unknown;
	if (inventory.add(weapon) != combat::WeaponInventoryResult::Accepted)
	{
		return proposal;
	}
	combat::TargetBelief target = {};
	const world::ActorObservation *currentEnemy = nullptr;
	if (weapon.kind != combat::WeaponKind::Grenade &&
		snapshot.findActor(targetActor, &currentEnemy) == world::ContactLookupResult::Found && currentEnemy &&
		currentEnemy->visible)
		targetPosition = compat::combatAimPoint(currentEnemy->position,
			{handle.entity->v.origin.x, handle.entity->v.origin.y, handle.entity->v.origin.z},
			currentEnemy->visibleParts, static_cast<std::uint16_t>(weaponValue),
			static_cast<float>(actors_[index].managedProfiles.skill) / kProfilePercentageScale,
			before.ducked ? kDuckedFeetOffset : kStandingFeetOffset, compatObservation.weapon.accuracy.isAvailable(),
			compatObservation.weapon.accuracy.value);
	target.target = targetActor;
	target.state = currentEnemy && currentEnemy->visible
		? combat::TargetBeliefState::Visible : combat::TargetBeliefState::Remembered;
	target.position = targetPosition;
	target.confidence = 1.0f;
	target.ageTicks = 0U;
	target.friendlyFireRisk = false;
	if (services_->service_engineFunctions()->pfnTraceLine != nullptr)
	{
		const float eye[] = {handle.entity->v.origin.x + handle.entity->v.view_ofs.x,
			handle.entity->v.origin.y + handle.entity->v.view_ofs.y,
			handle.entity->v.origin.z + handle.entity->v.view_ofs.z};
		const float aim[] = {targetPosition.x, targetPosition.y, targetPosition.z};
		TraceResult trace{};
		services_->service_engineFunctions()->pfnTraceLine(eye, aim, kTraceIncludingActors, handle.entity, &trace);
		if (trace.pHit != nullptr && trace.pHit != handle.entity && trace.pHit->v.deadflag == DEAD_NO)
		{
			const int hitTeam = services_->observation_resolvePlayerTeam(trace.pHit);
			target.friendlyFireRisk = hitTeam == team && hitTeam != 0;
		}
	}
	combat::CombatDecisionContext context = {};
	context.actor = {handle.actor.slot, handle.actor.actorGeneration};
	context.frame = snapshot.identity().frame;
	context.origin = {handle.entity->v.origin.x + handle.entity->v.view_ofs.x,
		handle.entity->v.origin.y + handle.entity->v.view_ofs.y,
		handle.entity->v.origin.z + handle.entity->v.view_ofs.z};
	if (weapon.kind != combat::WeaponKind::Grenade)
		context.origin = {handle.entity->v.origin.x, handle.entity->v.origin.y, handle.entity->v.origin.z};
	context.aimTrackingEnabled = true;
	context.currentPitch = handle.entity->v.v_angle.x;
	context.currentYaw = handle.entity->v.v_angle.y;
	context.elapsedSeconds = actors_[index].lastCombatTimes > 0.0f
		? (std::max)(0.0f,
			  (std::min)(ASTRABOT_BOT_BEHAVIOR_COMBAT_MAXIMUM_COMBAT_ELAPSED_SECONDS,
				  services_->service_globals()->time - actors_[index].lastCombatTimes))
		: 1.0f / kAimUpkeepRateHz;
	context.timestamp = services_->service_globals()->time;
	context.skill = static_cast<float>(actors_[index].managedProfiles.skill) / kSkillPercentageScale;
	context.fieldOfView = handle.entity->v.fov > 0.0f ? handle.entity->v.fov : kDefaultFieldOfView;
	context.randomSource = &services_->service_compatibilityRandomSource();
	context.randomTiming = {services_->command_managedBotCommandSequences(index), 0U,
		services_->service_managedBotFullUpdateSequences()[index]};
	const world::ActorObservation *visibleEnemy = nullptr;
	if (snapshot.findActor(targetActor, &visibleEnemy) == world::ContactLookupResult::Found && visibleEnemy &&
		visibleEnemy->visible)
	{
		context.predictMovement = true;
		context.targetVelocity = visibleEnemy->velocity;
		context.observerVelocity = {
			handle.entity->v.velocity.x, handle.entity->v.velocity.y, handle.entity->v.velocity.z};
		const auto &p = visibleEnemy->position;
		context.aimErrorRange = std::sqrt((p.x - context.origin.x) * (p.x - context.origin.x) +
			(p.y - context.origin.y) * (p.y - context.origin.y) + (p.z - context.origin.z) * (p.z - context.origin.z));
	}
	const auto lookReceipt = reactionContext(index);
	const auto lookExecution = actors_[index].executionRevision;
	const auto lookEpoch = actors_[index].managedBotStateMachines.normalStateEpoch();
	nav::NativeLadderLook ladder{};
	const bool ladderLook = services_->navigation_nativeLadderLook(index, &ladder);
	if (!executionCurrent(index, lookReceipt, lookExecution) ||
		actors_[index].managedBotStateMachines.normalStateEpoch() != lookEpoch)
		return ctx.proposal;
	if (ladderLook)
	{
		context.lookOverride = true;
		context.lookPitchOverride = ladder.pitchOverride;
		context.lookYaw = ladder.yaw;
		context.lookPitch = ladder.pitch;
	}
	actors_[index].lastCombatTimes = services_->service_globals()->time;
	combat::CombatDecision decision = {};
	const combat::CombatResult result =
		actors_[index].managedBotCombat.decide(snapshot, inventory, target, context, &decision);
	compat::CombatMovementInput movementInput{};
	compat::CombatMovementDecision movementEntry{};
	CrouchTraceContext crouch{services_->service_engineFunctions(), handle.entity, nullptr, before.ducked};
	const bool combatVisible = targetActor.isValid() && currentEnemy && currentEnemy->visible;
	if (combatVisible && !reactionIgnoring(index) && weapon.kind != combat::WeaponKind::Knife && weapon.kind != combat::WeaponKind::Grenade)
	{
		auto &move = movementInput;
		move.actor = actor;
		move.target = targetActor;
		move.frame = frame;
		move.now = context.timestamp;
		move.skill = context.skill;
		move.aggression = static_cast<float>(actors_[index].managedProfiles.aggression) / kProfilePercentageScale;
		const float dx = currentEnemy->position.x - handle.entity->v.origin.x;
		const float dy = currentEnemy->position.y - handle.entity->v.origin.y;
		const float dz = currentEnemy->position.z - handle.entity->v.origin.z;
		move.range = std::sqrt(dx * dx + dy * dy + dz * dz);
		move.weapon = static_cast<std::uint16_t>(weaponValue);
		move.planarRange = std::hypot(dx, dy);
		move.sniper = weapon.kind == combat::WeaponKind::Sniper;
		move.moving = std::hypot(handle.entity->v.velocity.x, handle.entity->v.velocity.y) >= kMovingSpeedThreshold;
		edict_t *enemy = services_->service_engineFunctions()->pfnPEntityOfEntIndex
			? services_->service_engineFunctions()->pfnPEntityOfEntIndex(static_cast<int>(targetActor.slot))
			: nullptr;
		if (enemy)
		{
			const float yaw = enemy->v.v_angle.y * ASTRABOT_BOT_BEHAVIOR_COMBAT_DEGREES_TO_RADIANS;
			move.enemyFacing = -dx * std::cos(yaw) - dy * std::sin(yaw) > 0;
		}
		move.knowledge = &snapshot;
		crouch.enemy = enemy;
		move.crouchVisible = &crouchTargetVisible;
		move.crouchContext = &crouch;
		move.random = context.randomSource;
		move.timing = {services_->command_managedBotCommandSequences(index), frame.tick,
			services_->service_managedBotFullUpdateSequences()[index]};
		movementEntry = actors_[index].managedBotCombat.prepareMovement(move);
	}
	else
		actors_[index].managedBotCombat.stopMovement();
	const bool fireIntentReady = combatVisible && result == combat::CombatResult::FireIntentReady && decision.hasFire &&
		decision.hasAim && services_->service_globals()->time >= actors_[index].attackReadyTimes;
	const compat::StateUpdateContext stateContext = makeManagedBotStateContext(index, before);
	if (combatVisible && !reactionIgnoring(index))
	{
		(void)actors_[index].managedBotStateMachines.beginAttack(stateContext);
	}
	else if (actors_[index].managedBotStateMachines.attackOverlayActive() &&
		(services_->service_compatibilitySurface().configuration().mode != compat::RuntimeMode::Compatibility ||
			!actors_[index].enemyReaction.enemy(reactionContext(index)).isValid() ||
			services_->service_globals()->time - actors_[index].enemyReaction.lastSaw(reactionContext(index)) >
				(reactionDisposition(index) == compat::ReactionDisposition::Engage
					? 2.0f + 2.0f * (1.0f - static_cast<float>(actors_[index].managedProfiles.aggression) / 100.0f) +
						(weapon.kind == combat::WeaponKind::Sniper ? 3.0f : before.ducked ? 1.0f : 0.0f)
					: .1f)))
	{
		(void)actors_[index].managedBotStateMachines.stopAttack(stateContext);
	}
	const auto retreatReceipt = reactionContext(index);
	const auto retreatExecution = actors_[index].executionRevision;
	const auto retreatEpoch = state.normalStateEpoch();
	if ((targetActor.isValid() || actors_[index].enemyReaction.enemy(retreatReceipt).isValid()) &&
		updateCombatRetreat(index, team))
	{
		proposal.movementButtons = static_cast<std::uint16_t>(
			proposal.movementButtons & ~(IN_DUCK | IN_ATTACK | IN_ATTACK2 | IN_RELOAD));
		if (weapon.kind == combat::WeaponKind::Sniper)
		{
			const auto inventoryMask = static_cast<std::uint32_t>(handle.entity->v.weapons);
			for (const auto id : kPistolWeaponIds)
			{
				if ((inventoryMask & (1U << id)) != 0)
				{
					proposal.kind = ActionKind::SelectPistol;
					proposal.equipmentWeaponId = static_cast<std::uint16_t>(id);
					break;
				}
			}
		}
		return proposal;
	}
	if (!executionCurrent(index, retreatReceipt, retreatExecution) || state.normalStateEpoch() != retreatEpoch)
		return proposal;
	if (decideEquipmentAction(ctx, observedWeapon, actors_[index].managedBotStateMachines.attackOverlayActive()))
		return proposal;
	if (fireIntentReady)
	{
		proposal.kind = ActionKind::Fire;
		proposal.viewAngles = {decision.aim.pitch, decision.aim.yaw, movementAngles.roll};
	}
	else if (result == combat::CombatResult::ReloadIntentReady && decision.hasReload &&
		services_->service_compatibilitySurface().configuration().mode != compat::RuntimeMode::Compatibility)
	{
		proposal.kind = ActionKind::Reload;
	}
	if (decision.hasAim)
	{
		proposal.hasCombatAim = true;
		proposal.viewAngles = {decision.aim.pitch, decision.aim.yaw, movementAngles.roll};
	}
	compat::WeaponActionInput weaponInput{};
	weaponInput.actor = actor;
	weaponInput.frame = frame;
	weaponInput.timestamp = services_->service_globals()->time;
	weaponInput.weaponId = static_cast<std::uint16_t>(weaponValue);
	weaponInput.clipKnown = observedWeapon.clip >= 0;
	weaponInput.clip = static_cast<std::uint16_t>((std::max)(0, observedWeapon.clip));
	weaponInput.fovKnown = std::isfinite(handle.entity->v.fov);
	weaponInput.fieldOfView = handle.entity->v.fov > 0.0f ? handle.entity->v.fov : kDefaultFieldOfView;
	weaponInput.targetDistance =
		std::sqrt((targetPosition.x - context.origin.x) * (targetPosition.x - context.origin.x) +
			(targetPosition.y - context.origin.y) * (targetPosition.y - context.origin.y) +
			(targetPosition.z - context.origin.z) * (targetPosition.z - context.origin.z));
	weaponInput.reactionSeconds = actors_[index].managedProfiles.reactionTimeSeconds;
	weaponInput.targetVisible = combatVisible;
	weaponInput.aimAligned = fireIntentReady ||
		(decision.hasAim &&
			compat::combatAimOnTarget(
				decision.aim.point, context.origin, decision.aim.yaw, weapon.kind == combat::WeaponKind::Sniper));
	weaponInput.stationary =
		std::hypot(handle.entity->v.velocity.x, handle.entity->v.velocity.y) < kStationarySpeedThreshold;
	weaponInput.requestFire = weaponInput.targetVisible && weaponInput.aimAligned && !target.friendlyFireRisk &&
		services_->service_globals()->time >= actors_[index].attackReadyTimes;
	weaponInput.attacking = actors_[index].managedBotStateMachines.attackOverlayActive();
	compat::CompatibilityObservation publicWeapon = {};
	const compat::ObservationTimingContext weaponTiming = {services_->command_managedBotCommandSequences(index), 0U,
		services_->service_managedBotFullUpdateSequences()[index]};
	if (services_->observation_observationAdapter().collectActor(
			handle.entity, actor, frame, weaponTiming, &publicWeapon) == ObservationAdapterResult::Accepted)
	{
		if (publicWeapon.weapon.silencer.isAvailable())
			weaponInput.silenced = {true, publicWeapon.weapon.silencer.value};
		if (publicWeapon.weapon.reserve.isAvailable() && publicWeapon.weapon.reserve.context.delayTicks <= frame.tick)
		{
			weaponInput.reserveKnown = true;
			weaponInput.reserve = publicWeapon.weapon.reserve.value;
			weaponInput.reserveObservationTick = frame.tick - publicWeapon.weapon.reserve.context.delayTicks;
		}
		if (publicWeapon.weapon.burst.isAvailable())
			weaponInput.burstMode = {true, publicWeapon.weapon.burst.value};
		if (publicWeapon.weapon.reloadState.isAvailable() &&
			publicWeapon.weapon.reloadState.value != combat::ReloadState::Unknown)
			weaponInput.reloading = {true, publicWeapon.weapon.reloadState.value == combat::ReloadState::Reloading};
		if (publicWeapon.weapon.nextPrimaryAttack.isAvailable() &&
			publicWeapon.weapon.nextPrimaryAttack.value > services_->service_globals()->time)
			weaponInput.requestFire = false;
		if (publicWeapon.weapon.nextSecondaryAttack.isAvailable())
			weaponInput.secondaryReady = {
				true, publicWeapon.weapon.nextSecondaryAttack.value <= services_->service_globals()->time};
	}
	const std::uint32_t inventoryMask = static_cast<std::uint32_t>(handle.entity->v.weapons);
	bool havePistol = false;
	for (const unsigned id : kPistolWeaponIds)
		havePistol = havePistol || (inventoryMask & (1U << id)) != 0U;
	weaponInput.pistolAvailable = havePistol;
	if (weaponValue == kKnife && weaponInput.requestFire && weaponInput.targetDistance < kKnifeAttackRange &&
		weaponInput.timestamp > actors_[index].weaponActions.fireReadyTimestamp())
	{
		edict_t *enemy = services_->service_engineFunctions()->pfnPEntityOfEntIndex != nullptr
			? services_->service_engineFunctions()->pfnPEntityOfEntIndex(static_cast<int>(targetActor.slot))
			: nullptr;
		if (enemy != nullptr && enemy->free == 0)
		{
			const float toUsX = handle.entity->v.origin.x - enemy->v.origin.x;
			const float toUsY = handle.entity->v.origin.y - enemy->v.origin.y;
			const float pitchDegrees = enemy->v.v_angle.x + enemy->v.punchangle.x;
			const float yawDegrees = enemy->v.v_angle.y + enemy->v.punchangle.y;
			const float rollDegrees = enemy->v.v_angle.z + enemy->v.punchangle.z;
			if (std::isfinite(pitchDegrees) && std::isfinite(yawDegrees) && std::isfinite(rollDegrees) &&
				std::isfinite(toUsX) && std::isfinite(toUsY))
			{
				const float yaw = yawDegrees * ASTRABOT_BOT_BEHAVIOR_COMBAT_PI / kHalfTurnDegrees;
				const float pitch = pitchDegrees * ASTRABOT_BOT_BEHAVIOR_COMBAT_PI / kHalfTurnDegrees;
				weaponInput.enemyFacingObserver = {
					true, std::cos(pitch) * (toUsX * std::cos(yaw) + toUsY * std::sin(yaw)) > 0.0f};
			}
		}
		if (!weaponInput.enemyFacingObserver.known || weaponInput.enemyFacingObserver.value)
		{
			const auto choice = services_->service_compatibilityRandomSource().nextFloat(
				compat::RandomRequest::floatRequest("Knife.AttackChoice", {actor.slot, actor.generation},
					{services_->command_managedBotCommandSequences(index), services_->service_adapterFrameCount(),
						services_->service_managedBotFullUpdateSequences()[index]},
					kMinimumKnifeRoll, kMaximumKnifeRoll));
			weaponInput.knifeRollKnown = choice.status == compat::RandomStatus::Ok;
			weaponInput.knifeRoll = choice.value;
		}
	}
	if (compat::WeaponActionPlanner::isGrenade(static_cast<std::uint16_t>(weaponValue)) && weaponInput.targetVisible &&
		!target.friendlyFireRisk && services_->service_engineFunctions()->pfnCVarGetFloat != nullptr &&
		services_->service_engineFunctions()->pfnTraceLine != nullptr &&
		services_->service_engineFunctions()->pfnTraceHull != nullptr)
	{
		constexpr float kMaximumGrenadeLandingError = 16.0f;
#define ASTRABOT_BOT_BEHAVIOR_COMBAT_GRENADE_TRACE_STEP_SECONDS 0.05f
		constexpr int kGrenadeTraceHull = 3;
		constexpr float kLandingGroundTraceDepth = 72.0f;
#define ASTRABOT_BOT_BEHAVIOR_COMBAT_MINIMUM_LANDING_GROUND_NORMAL_Z 0.7f
		constexpr float kGrenadeAimToleranceDegrees = 1.0f;
		const auto launch = compat::solveGrenadeLaunch(context.origin,
			{handle.entity->v.velocity.x, handle.entity->v.velocity.y, handle.entity->v.velocity.z}, targetPosition,
			services_->service_engineFunctions()->pfnCVarGetFloat("sv_gravity"), kMaximumGrenadeLandingError);
		if (launch.valid)
		{
			edict_t *intendedTarget = services_->service_engineFunctions()->pfnPEntityOfEntIndex != nullptr
				? services_->service_engineFunctions()->pfnPEntityOfEntIndex(static_cast<int>(targetActor.slot))
				: nullptr;
			bool lineClear = true, hullClear = true;
			world::WorldPosition previous = context.origin;
			const unsigned steps = static_cast<unsigned>(
				std::ceil(launch.flightSeconds / ASTRABOT_BOT_BEHAVIOR_COMBAT_GRENADE_TRACE_STEP_SECONDS));
			for (unsigned step = 0U; step <= steps && lineClear && hullClear; ++step)
			{
				const auto next = step == 0U
					? launch.origin
					: compat::grenadePosition(
						  launch, launch.flightSeconds * static_cast<float>(step) / static_cast<float>(steps));
				const float start[] = {previous.x, previous.y, previous.z};
				const float end[] = {next.x, next.y, next.z};
				TraceResult line = {}, hull = {};
				services_->service_engineFunctions()->pfnTraceLine(
					start, end, kTraceIncludingActors, handle.entity, &line);
				services_->service_engineFunctions()->pfnTraceHull(
					start, end, kTraceIncludingActors, kGrenadeTraceHull, handle.entity, &hull);
				lineClear = (line.flFraction >= 1.0f || (intendedTarget != nullptr && line.pHit == intendedTarget)) &&
					line.fStartSolid == 0 && line.fAllSolid == 0;
				hullClear = (hull.flFraction >= 1.0f || (intendedTarget != nullptr && hull.pHit == intendedTarget)) &&
					hull.fStartSolid == 0 && hull.fAllSolid == 0;
				previous = next;
				if (lineClear && hullClear && intendedTarget != nullptr &&
					(line.pHit == intendedTarget || hull.pHit == intendedTarget))
					break;
			}
			const auto landing = compat::grenadePosition(launch, launch.flightSeconds);
			const float groundStart[] = {landing.x, landing.y, landing.z};
			const float groundEnd[] = {landing.x, landing.y, landing.z - kLandingGroundTraceDepth};
			TraceResult ground = {};
			services_->service_engineFunctions()->pfnTraceHull(groundStart, groundEnd, kTraceIncludingActors,
				kGrenadeTraceHull, intendedTarget != nullptr ? intendedTarget : handle.entity, &ground);
			const bool landingSafe = ground.flFraction < 1.0f && ground.fStartSolid == 0 && ground.fAllSolid == 0 &&
				ground.vecPlaneNormal.z >= ASTRABOT_BOT_BEHAVIOR_COMBAT_MINIMUM_LANDING_GROUND_NORMAL_Z;
			weaponInput.trajectory = {true, lineClear, hullClear, landingSafe};
			weaponInput.grenadeAimKnown = lineClear && hullClear && landingSafe;
			weaponInput.grenadePitch = launch.pitch - handle.entity->v.punchangle.x;
			weaponInput.grenadeYaw = launch.yaw - handle.entity->v.punchangle.y;
			weaponInput.requestGrenade = lineClear && hullClear && landingSafe &&
				services_->service_globals()->time >= actors_[index].attackReadyTimes;
			weaponInput.aimAligned = std::fabs(compat::normalizedAimAngle(launch.yaw - handle.entity->v.v_angle.y -
										 handle.entity->v.punchangle.y)) <= kGrenadeAimToleranceDegrees &&
				std::fabs(compat::normalizedAimAngle(launch.pitch - handle.entity->v.v_angle.x -
					handle.entity->v.punchangle.x)) <= kGrenadeAimToleranceDegrees;
			if (weaponInput.requestGrenade)
			{
				proposal.viewAngles = {launch.pitch - handle.entity->v.punchangle.x,
					launch.yaw - handle.entity->v.punchangle.y, movementAngles.roll};
				proposal.hasCombatAim = true;
			}
		}
	}
	const auto &cadencePoint = decision.hasAim ? decision.aim.point : targetPosition;
	const compat::FireCadenceContext cadence = {context.skill, context.randomSource,
		{services_->command_managedBotCommandSequences(index), services_->service_adapterFrameCount(),
			services_->service_managedBotFullUpdateSequences()[index]},
		std::hypot(cadencePoint.x - handle.entity->v.origin.x, cadencePoint.y - handle.entity->v.origin.y)};
	const compat::WeaponActionDecision weaponDecision = actors_[index].weaponActions.update(weaponInput, cadence);
	if (movementEntry.active)
	{
		const auto movement = actors_[index].managedBotCombat.finishMovement(movementInput);
		if (movement.active)
		{
			proposal.combatMovement = true;
			proposal.combatForward = movement.forward;
			proposal.combatSide = movement.side;
			proposal.stopForWeapon = movement.crouch || movementInput.sniper;
			proposal.movementButtons = static_cast<std::uint16_t>(proposal.movementButtons &
				~(IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT | IN_JUMP | IN_DUCK | IN_LEFT | IN_RIGHT));
			if (movement.crouch)
				proposal.movementButtons = static_cast<std::uint16_t>(proposal.movementButtons | IN_DUCK);
			if (movement.jump)
				proposal.movementButtons = static_cast<std::uint16_t>(proposal.movementButtons | IN_JUMP);
			if (movementEntry.entered)
				services_->applyNavigationRequest({index, NavigationRequestKind::DestroyPath});
			services_->applyNavigationRequest({index, NavigationRequestKind::ResetProgress});
		}
	}
	if (proposal.kind == ActionKind::Fire && !weaponDecision.primary)
	{
		proposal.kind = ActionKind::None;
	}
	if (weaponDecision.primary)
		proposal.kind = ActionKind::Fire;
	if (weaponDecision.switchToPistol && havePistol)
		proposal.kind = ActionKind::SelectPistol;
	proposal.secondaryAttack = weaponDecision.secondary;
	proposal.stopForWeapon = proposal.stopForWeapon || weaponDecision.stopMovement;
	if (weaponDecision.hasRetainedGrenadeAim)
	{
		proposal.viewAngles = {weaponDecision.grenadePitch, weaponDecision.grenadeYaw, movementAngles.roll};
		proposal.hasCombatAim = true;
	}
	if ((services_->service_adapterFrameCount() % kCombatLogFrameInterval) == 0U && gpMetaUtilFuncs != nullptr &&
		gpMetaUtilFuncs->pfnLogConsole != nullptr && services_->service_pluginId() != nullptr)
	{
		const char *coreReason = "state_gate";
		if (result == combat::CombatResult::FireIntentReady)
			coreReason = "fire_ready";
		else if (result == combat::CombatResult::ReloadIntentReady)
			coreReason = "reload_ready";
		else if (result == combat::CombatResult::FriendlyFireRisk)
			coreReason = "friendly_fire";
		else if (result == combat::CombatResult::InvalidTarget)
			coreReason = "target_invalid";
		else if (result == combat::CombatResult::InvalidObservation)
			coreReason = targetActor.isValid() ? "observation_invalid" : "no_target";
		else if (result == combat::CombatResult::NoAction)
		{
			if (decision.hasAim)
				coreReason = compat::weaponEngagementAllowed(weapon.kind, weaponInput.targetDistance) ? "aim_alignment"
																									  : "weapon_range";
			else if (weapon.reloadState == combat::ReloadState::Reloading)
				coreReason = "reloading";
			else if (!weapon.ammo.clipKnown)
				coreReason = "clip_unknown";
			else if (weapon.ammo.clip == 0U)
				coreReason = "clip_empty";
			else
				coreReason = "weapon_not_ready";
		}
		const char *weaponReason = "weapon_wait";
		if (weaponDecision.primary)
			weaponReason = "primary";
		else if (weaponDecision.secondary)
			weaponReason = "secondary";
		else if (weaponDecision.switchToPistol)
			weaponReason = "switch_pistol";
		else if (weaponDecision.releasePrimary)
			weaponReason = "primary_release";
		else if (weaponDecision.waitingForCadence)
			weaponReason = "cadence_wait";
		else if (weaponInput.reloading.known && weaponInput.reloading.value)
			weaponReason = "reloading";
		else if (weaponDecision.waitingForObservation)
			weaponReason = "observation_wait";
		else if (weaponDecision.unavailableMode)
			weaponReason = "mode_unavailable";
		else if (compat::WeaponActionPlanner::isGrenade(weaponInput.weaponId))
			weaponReason = "grenade_wait";
		else if (!weaponInput.targetVisible)
			weaponReason = "no_target";
		else if (target.friendlyFireRisk)
			weaponReason = "friendly_fire";
		else if (!weaponInput.aimAligned)
			weaponReason = "aim_alignment";
		else if (context.timestamp < actors_[index].attackReadyTimes)
			weaponReason = "attack_delay";
		else if (!weaponInput.requestFire)
			weaponReason = "attack_cooldown";
		else if (weaponDecision.stopMovement)
			weaponReason = "scope_or_stop_wait";
		else if (!weaponInput.clipKnown)
			weaponReason = "clip_unknown";
		else if (weaponInput.clip == 0U)
			weaponReason = "clip_empty";
		profiledLogConsole(services_->service_runtimeProfiler(), services_->service_pluginId(),
			"action sensor slot=%u frame=%u team=%d teamInfo=%u target=%d combatResult=%d "
			"hasAim=%d hasFire=%d core_reason=%s weapon_reason=%s weapon=%d clip=%d range=%.1f "
			"dt=%.4f current=(%.2f %.2f) aim=(%.2f %.2f) action=%d primary=%d secondary=%d "
			"now=%.3f skill=%.2f cadence_range=%.1f fire_ready_at=%.3f aim_point=(%.2f %.2f %.2f) "
			"fov=%.1f recoil_known=%d combat_move=%d attack=(%.1f %.1f) crouch=%d",
			static_cast<unsigned int>(handle.actor.slot),
			static_cast<unsigned int>(services_->service_adapterFrameCount()), static_cast<int>(handle.entity->v.team),
			static_cast<unsigned int>(services_->observation_teamInfoTeams(index)), targetActor.isValid() ? 1 : 0,
			static_cast<int>(result), decision.hasAim ? 1 : 0, decision.hasFire ? 1 : 0, coreReason, weaponReason,
			weaponValue, observedWeapon.clip, weaponInput.targetDistance, context.elapsedSeconds, context.currentPitch,
			context.currentYaw, proposal.viewAngles.pitch, proposal.viewAngles.yaw, static_cast<int>(proposal.kind),
			weaponDecision.primary ? 1 : 0, weaponDecision.secondary ? 1 : 0, context.timestamp, context.skill,
			cadence.targetRange, actors_[index].weaponActions.fireReadyTimestamp(), decision.aim.point.x,
			decision.aim.point.y, decision.aim.point.z, weaponInput.fieldOfView,
			compatObservation.weapon.accuracy.isAvailable() ? 1 : 0, proposal.combatMovement ? 1 : 0,
			proposal.combatForward, proposal.combatSide, (proposal.movementButtons & IN_DUCK) != 0 ? 1 : 0);
	}
	return proposal;
}
#undef ASTRABOT_BOT_BEHAVIOR_COMBAT_DEGREES_TO_RADIANS
#undef ASTRABOT_BOT_BEHAVIOR_COMBAT_PI
#undef ASTRABOT_BOT_BEHAVIOR_COMBAT_GRENADE_TRACE_STEP_SECONDS
#undef ASTRABOT_BOT_BEHAVIOR_COMBAT_MINIMUM_LANDING_GROUND_NORMAL_Z

} // namespace metamod
} // namespace astrabot
#undef ASTRABOT_BOT_BEHAVIOR_COMBAT_MAXIMUM_COMBAT_ELAPSED_SECONDS
