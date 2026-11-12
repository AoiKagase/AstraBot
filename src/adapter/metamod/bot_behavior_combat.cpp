#include "bot_behavior_runtime.hpp"
#include "bot_runtime_support.hpp"
namespace astrabot
{
namespace metamod
{
using namespace runtime_detail;

bool BotBehaviorRuntime::cancelHeldGrenade(std::size_t index,
										   const runtime::ViewAngles &movementAngles,
										   ActionProposal &proposal)
{

	if (index >= services_->service_managedBotHandles().size() ||
		!actors_[index].weaponActions.grenadeInProgress())
		return false;
	const FakeClientHandle &cancelHandle = services_->service_managedBotHandles()[index];
	compat::WeaponActionInput cancel = {};
	cancel.actor = {cancelHandle.actor.slot, cancelHandle.actor.actorGeneration};
	cancel.frame = {services_->service_lifecycle().mapGeneration(),
					services_->service_lifecycle().roundGeneration(),
					services_->service_adapterFrameCount()};
	cancel.timestamp =
		services_->service_globals() != nullptr ? services_->service_globals()->time : 0.0f;
	cancel.weaponId = actors_[index].weaponActions.currentWeaponId();
	compat::PublicActorState activeWeapon = {};
	if (services_->observation_publicObservations().read(cancel.actor, cancel.frame,
														 &activeWeapon) &&
		activeWeapon.weaponKnown && activeWeapon.weaponId != static_cast<int>(cancel.weaponId))
	{
		actors_[index].weaponActions.reset();
		return false;
	}
	const auto ammoDefinition =
		services_->observation_publicObservations().definition(cancel.weaponId);
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
		proposal.viewAngles = {canceled.grenadePitch, canceled.grenadeYaw, movementAngles.roll};
	return true;
}

ActionProposal BotBehaviorRuntime::decideCombatAction(ManagedActionContext &ctx)
{
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
	if (!services_->observation_publicObservations().read(actor, frame, &observedWeapon) ||
		!observedWeapon.weaponKnown)
		return proposal;
	const int weaponValue = observedWeapon.weaponId;
	weapon.weapon = {static_cast<std::uint16_t>(weaponValue)};
	const compat::PublicWeaponDefinition definition =
		services_->observation_publicObservations().definition(weaponValue);
	weapon.slot = static_cast<std::uint8_t>(definition.known ? definition.slot : 0);
	switch (weaponValue)
	{
	case 1:
	case 10:
	case 11:
	case 16:
	case 17:
	case 26:
		weapon.kind = combat::WeaponKind::Pistol;
		break;
	case 5:
	case 21:
		weapon.kind = combat::WeaponKind::Shotgun;
		break;
	case 7:
	case 12:
	case 19:
	case 23:
	case 30:
		weapon.kind = combat::WeaponKind::Smg;
		break;
	case 3:
	case 13:
	case 18:
	case 24:
		weapon.kind = combat::WeaponKind::Sniper;
		break;
	case 20:
		weapon.kind = combat::WeaponKind::Heavy;
		break;
	case 29:
		weapon.kind = combat::WeaponKind::Knife;
		break;
	case 4:
	case 9:
	case 25:
		weapon.kind = combat::WeaponKind::Grenade;
		break;
	case 8:
	case 14:
	case 15:
	case 22:
	case 27:
	case 28:
		weapon.kind = combat::WeaponKind::Rifle;
		break;
	default:
		return proposal;
	}
	weapon.availability = combat::WeaponAvailability::Available;
	weapon.priority = 1U;
	const bool knife = weapon.kind == combat::WeaponKind::Knife;
	const bool reserveKnown =
		definition.known && definition.ammoIndex >= 0 &&
		observedWeapon.ammoKnown[static_cast<std::size_t>(definition.ammoIndex)];
	const std::uint16_t reserve =
		reserveKnown ? static_cast<std::uint16_t>(
						   observedWeapon.ammo[static_cast<std::size_t>(definition.ammoIndex)])
					 : 0U;
	weapon.ammo = {static_cast<std::uint16_t>(knife ? 1 : (std::max)(0, observedWeapon.clip)),
				   reserve, 255U, knife || observedWeapon.clip >= 0, reserveKnown};
	weapon.reloadState = compatObservation.weapon.reloadState.isAvailable()
							 ? compatObservation.weapon.reloadState.value
							 : combat::ReloadState::Unknown;
	if (inventory.add(weapon) != combat::WeaponInventoryResult::Accepted)
	{
		return proposal;
	}
	combat::TargetBelief target = {};
	target.target = targetActor;
	target.state = combat::TargetBeliefState::Visible;
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
		services_->service_engineFunctions()->pfnTraceLine(eye, aim, 0, handle.entity, &trace);
		if (trace.pHit != nullptr && trace.pHit != handle.entity &&
			trace.pHit->v.deadflag == DEAD_NO)
		{
			int hitTeam = static_cast<int>(trace.pHit->v.team);
			for (std::size_t slot = 0U; slot < services_->service_managedBotHandles().size();
				 ++slot)
				if (services_->service_managedBotHandles()[slot].entity == trace.pHit &&
					services_->observation_managedBotTeamNumbers(slot) != 0U)
					hitTeam = services_->observation_managedBotTeamNumbers(slot);
			target.friendlyFireRisk = hitTeam == team && hitTeam != 0;
		}
	}
	combat::CombatDecisionContext context = {};
	context.actor = {handle.actor.slot, handle.actor.actorGeneration};
	context.frame = snapshot.identity().frame;
	context.origin = {handle.entity->v.origin.x + handle.entity->v.view_ofs.x,
					  handle.entity->v.origin.y + handle.entity->v.view_ofs.y,
					  handle.entity->v.origin.z + handle.entity->v.view_ofs.z};
	context.aimTrackingEnabled = true;
	context.currentPitch = handle.entity->v.v_angle.x;
	context.currentYaw = handle.entity->v.v_angle.y;
	context.elapsedSeconds =
		actors_[index].lastCombatTimes > 0.0f
			? (std::max)(0.0f, (std::min)(0.1f, services_->service_globals()->time -
													actors_[index].lastCombatTimes))
			: 1.0f / 30.0f;
	context.timestamp = services_->service_globals()->time;
	context.skill = static_cast<float>(actors_[index].managedProfiles.skill) / 100.0f;
	context.fieldOfView = handle.entity->v.fov > 0.0f ? handle.entity->v.fov : 90.0f;
	context.randomSource = &services_->service_compatibilityRandomSource();
	context.randomTiming = {services_->command_managedBotCommandSequences(index), 0U,
							services_->service_managedBotFullUpdateSequences()[index]};
	actors_[index].lastCombatTimes = services_->service_globals()->time;
	combat::CombatDecision decision = {};
	const combat::CombatResult result =
		actors_[index].managedBotCombat.decide(snapshot, inventory, target, context, &decision);
	const bool fireIntentReady =
		result == combat::CombatResult::FireIntentReady && decision.hasFire && decision.hasAim &&
		services_->service_globals()->time >= actors_[index].attackReadyTimes;
	const compat::StateUpdateContext stateContext = makeManagedBotStateContext(index, before);
	if (fireIntentReady)
	{
		(void)actors_[index].managedBotStateMachines.beginAttack(stateContext);
	}
	else if (actors_[index].managedBotStateMachines.attackOverlayActive())
	{
		(void)actors_[index].managedBotStateMachines.stopAttack(stateContext);
	}
	if (fireIntentReady)
	{
		proposal.kind = ActionKind::Fire;
		proposal.viewAngles = {decision.aim.pitch, decision.aim.yaw, movementAngles.roll};
	}
	else if (result == combat::CombatResult::ReloadIntentReady && decision.hasReload)
	{
		proposal.kind = ActionKind::Reload;
	}
	if (decision.hasAim)
	{
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
	weaponInput.fieldOfView = handle.entity->v.fov > 0.0f ? handle.entity->v.fov : 90.0f;
	weaponInput.targetDistance =
		std::sqrt((targetPosition.x - context.origin.x) * (targetPosition.x - context.origin.x) +
				  (targetPosition.y - context.origin.y) * (targetPosition.y - context.origin.y) +
				  (targetPosition.z - context.origin.z) * (targetPosition.z - context.origin.z));
	weaponInput.reactionSeconds = actors_[index].managedProfiles.reactionTimeSeconds;
	weaponInput.targetVisible = targetActor.isValid();
	const float weaponDx = targetPosition.x - context.origin.x;
	const float weaponDy = targetPosition.y - context.origin.y;
	const float weaponDz = targetPosition.z - context.origin.z;
	const float desiredYaw = std::atan2(weaponDy, weaponDx) * 180.0f / 3.14159265358979323846f;
	const float desiredPitch =
		-std::atan2(weaponDz, std::hypot(weaponDx, weaponDy)) * 180.0f / 3.14159265358979323846f;
	weaponInput.aimAligned =
		fireIntentReady ||
		(decision.hasAim &&
		 std::fabs(compat::normalizedAimAngle(desiredYaw - handle.entity->v.v_angle.y)) <= 1.0f &&
		 std::fabs(compat::normalizedAimAngle(desiredPitch - handle.entity->v.v_angle.x)) <= 1.0f);
	weaponInput.stationary =
		std::hypot(handle.entity->v.velocity.x, handle.entity->v.velocity.y) < 10.0f;
	weaponInput.requestFire = weaponInput.targetVisible && weaponInput.aimAligned &&
							  !target.friendlyFireRisk &&
							  services_->service_globals()->time >= actors_[index].attackReadyTimes;
	weaponInput.attacking = actors_[index].managedBotStateMachines.attackOverlayActive();
	compat::CompatibilityObservation publicWeapon = {};
	const compat::ObservationTimingContext weaponTiming = {
		services_->command_managedBotCommandSequences(index), 0U,
		services_->service_managedBotFullUpdateSequences()[index]};
	if (services_->observation_observationAdapter().collectActor(handle.entity, actor, frame,
																 weaponTiming, &publicWeapon) ==
		ObservationAdapterResult::Accepted)
	{
		if (publicWeapon.weapon.silencer.isAvailable())
			weaponInput.silenced = {true, publicWeapon.weapon.silencer.value};
		if (publicWeapon.weapon.reserve.isAvailable() &&
			publicWeapon.weapon.reserve.context.delayTicks <= frame.tick)
		{
			weaponInput.reserveKnown = true;
			weaponInput.reserve = publicWeapon.weapon.reserve.value;
			weaponInput.reserveObservationTick =
				frame.tick - publicWeapon.weapon.reserve.context.delayTicks;
		}
		if (publicWeapon.weapon.burst.isAvailable())
			weaponInput.burstMode = {true, publicWeapon.weapon.burst.value};
		if (publicWeapon.weapon.reloadState.isAvailable() &&
			publicWeapon.weapon.reloadState.value != combat::ReloadState::Unknown)
			weaponInput.reloading = {true, publicWeapon.weapon.reloadState.value ==
											   combat::ReloadState::Reloading};
		if (publicWeapon.weapon.nextPrimaryAttack.isAvailable() &&
			publicWeapon.weapon.nextPrimaryAttack.value > services_->service_globals()->time)
			weaponInput.requestFire = false;
		if (publicWeapon.weapon.nextSecondaryAttack.isAvailable())
			weaponInput.secondaryReady = {true, publicWeapon.weapon.nextSecondaryAttack.value <=
													services_->service_globals()->time};
	}
	const std::uint32_t inventoryMask = static_cast<std::uint32_t>(handle.entity->v.weapons);
	bool havePistol = false;
	for (const unsigned id : {1U, 10U, 11U, 16U, 17U, 26U})
		havePistol = havePistol || (inventoryMask & (1U << id)) != 0U;
	if (weaponValue == 29 && weaponInput.requestFire && weaponInput.targetDistance < 75.0f)
	{
		edict_t *enemy = services_->service_engineFunctions()->pfnPEntityOfEntIndex != nullptr
							 ? services_->service_engineFunctions()->pfnPEntityOfEntIndex(
								   static_cast<int>(targetActor.slot))
							 : nullptr;
		if (enemy != nullptr && enemy->free == 0)
		{
			const float toUsX = handle.entity->v.origin.x - enemy->v.origin.x;
			const float toUsY = handle.entity->v.origin.y - enemy->v.origin.y;
			const float pitchDegrees = enemy->v.v_angle.x + enemy->v.punchangle.x;
			const float yawDegrees = enemy->v.v_angle.y + enemy->v.punchangle.y;
			const float rollDegrees = enemy->v.v_angle.z + enemy->v.punchangle.z;
			if (std::isfinite(pitchDegrees) && std::isfinite(yawDegrees) &&
				std::isfinite(rollDegrees) && std::isfinite(toUsX) && std::isfinite(toUsY))
			{
				const float yaw = yawDegrees * 3.14159265358979323846f / 180.0f;
				const float pitch = pitchDegrees * 3.14159265358979323846f / 180.0f;
				weaponInput.enemyFacingObserver = {
					true, std::cos(pitch) * (toUsX * std::cos(yaw) + toUsY * std::sin(yaw)) > 0.0f};
			}
		}
		if (!weaponInput.enemyFacingObserver.known || weaponInput.enemyFacingObserver.value)
		{
			const auto choice = services_->service_compatibilityRandomSource().nextFloat(
				compat::RandomRequest::floatRequest(
					"Knife.AttackChoice", {actor.slot, actor.generation},
					{services_->command_managedBotCommandSequences(index),
					 services_->service_adapterFrameCount(),
					 services_->service_managedBotFullUpdateSequences()[index]},
					0.0f, 100.0f));
			weaponInput.knifeRollKnown = choice.status == compat::RandomStatus::Ok;
			weaponInput.knifeRoll = choice.value;
		}
	}
	if (compat::WeaponActionPlanner::isGrenade(static_cast<std::uint16_t>(weaponValue)) &&
		weaponInput.targetVisible && !target.friendlyFireRisk &&
		services_->service_engineFunctions()->pfnCVarGetFloat != nullptr &&
		services_->service_engineFunctions()->pfnTraceLine != nullptr &&
		services_->service_engineFunctions()->pfnTraceHull != nullptr)
	{
		const auto launch = compat::solveGrenadeLaunch(
			context.origin,
			{handle.entity->v.velocity.x, handle.entity->v.velocity.y, handle.entity->v.velocity.z},
			targetPosition, services_->service_engineFunctions()->pfnCVarGetFloat("sv_gravity"),
			16.0f);
		if (launch.valid)
		{
			edict_t *intendedTarget =
				services_->service_engineFunctions()->pfnPEntityOfEntIndex != nullptr
					? services_->service_engineFunctions()->pfnPEntityOfEntIndex(
						  static_cast<int>(targetActor.slot))
					: nullptr;
			bool lineClear = true, hullClear = true;
			world::WorldPosition previous = context.origin;
			const unsigned steps = static_cast<unsigned>(std::ceil(launch.flightSeconds / 0.05f));
			for (unsigned step = 0U; step <= steps && lineClear && hullClear; ++step)
			{
				const auto next =
					step == 0U ? launch.origin
							   : compat::grenadePosition(launch, launch.flightSeconds *
																	 static_cast<float>(step) /
																	 static_cast<float>(steps));
				const float start[] = {previous.x, previous.y, previous.z};
				const float end[] = {next.x, next.y, next.z};
				TraceResult line = {}, hull = {};
				services_->service_engineFunctions()->pfnTraceLine(start, end, 0, handle.entity,
																   &line);
				services_->service_engineFunctions()->pfnTraceHull(start, end, 0, 3, handle.entity,
																   &hull);
				lineClear = (line.flFraction >= 1.0f ||
							 (intendedTarget != nullptr && line.pHit == intendedTarget)) &&
							line.fStartSolid == 0 && line.fAllSolid == 0;
				hullClear = (hull.flFraction >= 1.0f ||
							 (intendedTarget != nullptr && hull.pHit == intendedTarget)) &&
							hull.fStartSolid == 0 && hull.fAllSolid == 0;
				previous = next;
				if (lineClear && hullClear && intendedTarget != nullptr &&
					(line.pHit == intendedTarget || hull.pHit == intendedTarget))
					break;
			}
			const auto landing = compat::grenadePosition(launch, launch.flightSeconds);
			const float groundStart[] = {landing.x, landing.y, landing.z};
			const float groundEnd[] = {landing.x, landing.y, landing.z - 72.0f};
			TraceResult ground = {};
			services_->service_engineFunctions()->pfnTraceHull(
				groundStart, groundEnd, 0, 3,
				intendedTarget != nullptr ? intendedTarget : handle.entity, &ground);
			const bool landingSafe = ground.flFraction < 1.0f && ground.fStartSolid == 0 &&
									 ground.fAllSolid == 0 && ground.vecPlaneNormal.z >= 0.7f;
			weaponInput.trajectory = {true, lineClear, hullClear, landingSafe};
			weaponInput.grenadeAimKnown = lineClear && hullClear && landingSafe;
			weaponInput.grenadePitch = launch.pitch - handle.entity->v.punchangle.x;
			weaponInput.grenadeYaw = launch.yaw - handle.entity->v.punchangle.y;
			weaponInput.requestGrenade =
				lineClear && hullClear && landingSafe &&
				services_->service_globals()->time >= actors_[index].attackReadyTimes;
			weaponInput.aimAligned =
				std::fabs(compat::normalizedAimAngle(launch.yaw - handle.entity->v.v_angle.y -
													 handle.entity->v.punchangle.y)) <= 1.0f &&
				std::fabs(compat::normalizedAimAngle(launch.pitch - handle.entity->v.v_angle.x -
													 handle.entity->v.punchangle.x)) <= 1.0f;
			if (weaponInput.requestGrenade)
				proposal.viewAngles = {launch.pitch - handle.entity->v.punchangle.x,
									   launch.yaw - handle.entity->v.punchangle.y,
									   movementAngles.roll};
		}
	}
	const compat::WeaponActionDecision weaponDecision =
		actors_[index].weaponActions.update(weaponInput);
	if (proposal.kind == ActionKind::Fire && !weaponDecision.primary)
	{
		proposal.kind = ActionKind::None;
	}
	if (weaponDecision.primary)
		proposal.kind = ActionKind::Fire;
	if (weaponDecision.switchToPistol && havePistol)
		proposal.kind = ActionKind::SelectPistol;
	proposal.secondaryAttack = weaponDecision.secondary;
	proposal.stopForWeapon = weaponDecision.stopMovement;
	if (weaponDecision.hasRetainedGrenadeAim)
		proposal.viewAngles = {weaponDecision.grenadePitch, weaponDecision.grenadeYaw,
							   movementAngles.roll};
	if ((services_->service_adapterFrameCount() % 16U) == 0U && gpMetaUtilFuncs != nullptr &&
		gpMetaUtilFuncs->pfnLogConsole != nullptr && services_->service_pluginId() != nullptr)
	{
		const char *coreReason = "state_gate";
		if (result == combat::CombatResult::FireIntentReady) coreReason = "fire_ready";
		else if (result == combat::CombatResult::ReloadIntentReady) coreReason = "reload_ready";
		else if (result == combat::CombatResult::FriendlyFireRisk) coreReason = "friendly_fire";
		else if (result == combat::CombatResult::InvalidTarget) coreReason = "target_invalid";
		else if (result == combat::CombatResult::InvalidObservation)
			coreReason = targetActor.isValid() ? "observation_invalid" : "no_target";
		else if (result == combat::CombatResult::NoAction)
		{
			if (decision.hasAim)
				coreReason = compat::weaponEngagementAllowed(weapon.kind, weaponInput.targetDistance)
					? "aim_alignment" : "weapon_range";
			else if (weapon.reloadState == combat::ReloadState::Reloading) coreReason = "reloading";
			else if (!weapon.ammo.clipKnown) coreReason = "clip_unknown";
			else if (weapon.ammo.clip == 0U) coreReason = "clip_empty";
			else coreReason = "weapon_not_ready";
		}
		const char *weaponReason = "weapon_wait";
		if (weaponDecision.primary) weaponReason = "primary";
		else if (weaponDecision.secondary) weaponReason = "secondary";
		else if (weaponDecision.switchToPistol) weaponReason = "switch_pistol";
		else if (weaponDecision.releasePrimary) weaponReason = "primary_release";
		else if (weaponInput.reloading.known && weaponInput.reloading.value) weaponReason = "reloading";
		else if (weaponDecision.waitingForObservation) weaponReason = "observation_wait";
		else if (weaponDecision.unavailableMode) weaponReason = "mode_unavailable";
		else if (compat::WeaponActionPlanner::isGrenade(weaponInput.weaponId)) weaponReason = "grenade_wait";
		else if (!weaponInput.targetVisible) weaponReason = "no_target";
		else if (target.friendlyFireRisk) weaponReason = "friendly_fire";
		else if (!weaponInput.aimAligned) weaponReason = "aim_alignment";
		else if (context.timestamp < actors_[index].attackReadyTimes) weaponReason = "attack_delay";
		else if (!weaponInput.requestFire) weaponReason = "attack_cooldown";
		else if (weaponDecision.stopMovement) weaponReason = "scope_or_stop_wait";
		else if (!weaponInput.clipKnown) weaponReason = "clip_unknown";
		else if (weaponInput.clip == 0U) weaponReason = "clip_empty";
		profiledLogConsole(services_->service_runtimeProfiler(), services_->service_pluginId(),
			"action sensor slot=%u frame=%u team=%d teamInfo=%u target=%d combatResult=%d "
			"hasAim=%d hasFire=%d core_reason=%s weapon_reason=%s weapon=%d clip=%d range=%.1f "
			"dt=%.4f current=(%.2f %.2f) aim=(%.2f %.2f) action=%d primary=%d secondary=%d",
			static_cast<unsigned int>(handle.actor.slot),
			static_cast<unsigned int>(services_->service_adapterFrameCount()),
			static_cast<int>(handle.entity->v.team),
			static_cast<unsigned int>(services_->observation_managedBotTeamNumbers(index)),
			targetActor.isValid() ? 1 : 0, static_cast<int>(result), decision.hasAim ? 1 : 0,
			decision.hasFire ? 1 : 0, coreReason, weaponReason, weaponValue, observedWeapon.clip,
			weaponInput.targetDistance, context.elapsedSeconds, context.currentPitch, context.currentYaw,
			proposal.viewAngles.pitch, proposal.viewAngles.yaw, static_cast<int>(proposal.kind),
			weaponDecision.primary ? 1 : 0, weaponDecision.secondary ? 1 : 0);
	}
	return proposal;
}

} // namespace metamod
} // namespace astrabot
