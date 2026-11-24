#include "plugin/plugin_runtime.hpp"
#include "runtime/shared/bot_runtime_support.hpp"
#include <cstdlib>
#include <sstream>

namespace
{
bool testCombatTargetLog(astrabot::metamod::PluginRuntime &, enginefuncs_t &, globalvars_t &);
DLL_FUNCTIONS *combatGameDll = nullptr;
void captureCombatGameDll(DLL_FUNCTIONS &table) { combatGameDll = &table; }
}
#define ASTRABOT_EXTRA_ACTOR_TEST testCombatTargetLog
#define ASTRABOT_ACTOR_GAMEDLL_SETUP captureCombatGameDll
#include "compat_actor_command_tests.cpp"
#include "fixtures/noise_runtime_nav.hpp"

namespace
{
int combatUserMessageId(plid_t id, const char *name, int *size)
{
	if (std::strcmp(name, "WeaponList") == 0)
		return 105;
	if (std::strcmp(name, "DeathMsg") == 0)
		return 103;
	if (std::strcmp(name, "ScreenFade") == 0)
		return 104;
	return siteFixtureUserMessageId(id, name, size);
}

unsigned combatAttackCommands = 0U;
float combatCommandYaw = 0.0f;
float combatCommandPitch = 0.0f;
float combatLastAttackPitch = std::numeric_limits<float>::quiet_NaN();
float combatCommandForward = 0.0f, combatCommandSide = 0.0f;
unsigned combatC4Selections = 0;
unsigned grenadeAdmissionEquipRequests = 0;
astrabot::metamod::PluginRuntime *grenadeAdmissionResetRuntime = nullptr;
cvar_t *(*grenadeAdmissionOriginalPointer)(const char *) = nullptr;
float (*grenadeAdmissionOriginalFloat)(const char *) = nullptr;
cvar_t grenadeAdmissionAllowed{};
unsigned grenadeAdmissionResetCount = 0, grenadeAdmissionFloatReads = 0;
bool grenadeAdmissionInReset = false;
bool grenadeCornerTrace = false;
unsigned grenadeCornerWallTraces = 0;
unsigned grenadeCornerCase = 0;
astrabot::metamod::PluginRuntime *grenadeCornerResetRuntime = nullptr;
std::vector<Vector> grenadeCornerWallStarts;
std::vector<std::pair<unsigned, unsigned short>> grenadeCornerSent;
astrabot::compat::CompatibilityStateMachine *grenadeAdmissionAttackMachine = nullptr;
astrabot::compat::StateUpdateContext grenadeAdmissionAttackContext{};
unsigned grenadeAdmissionAttackMutations = 0;
unsigned combatMovingFireCommands = 0U;
bool combatPistolSelected = false;
bool humanFriendlyBlocks = false;
bool coverBlocksEnemySight = false;
bool forceLowPercentageRoll = false;
bool enemyBehindCover = false;
bool rejectAmbushRoll = false;
unsigned rejectedPercentageRolls = 0;
unsigned combatReloadCommands = 0;
unsigned attackDeadlineDraws = 0;
unsigned attackAimRefreshDraws = 0;
unsigned retreatTimerDraws = 0;
float forcedNoticeFixtureRoll = -1.0f;
bool shieldOwned = true, shieldClosed = false, shieldSelfReload = false, shieldEnemyReload = false;
int plantZonePrediction = 0;
bool plantZoneReplacesEntity = false;
struct EquipmentFixture
{
	int active = 6, primaryClip = 0, pistolClip = 20;
	bool refuseSwitch = false, replaceEntity = false;
	bool primaryUnknown = false;
	bool plantTail = false, dropOnC4 = false;
	bool predictionUnavailable = false, replaceOnActiveRead = false;
	Vector predictionView{};
	int attackMutation = 0;
	astrabot::compat::CompatibilityStateMachine *machine = nullptr;
	astrabot::compat::StateUpdateContext context{};
	std::vector<unsigned> commands;
};
EquipmentFixture *equipmentFixture = nullptr;
std::vector<Vector> *occludedAimTraces = nullptr;
struct SelfDefenseTrace
{
	bool eyeVisible;
	bool replaceEnemy;
	unsigned eyeChecks = 0;
};
SelfDefenseTrace *selfDefenseTrace = nullptr;
float combatRandomFloat(float low, float high)
{
	if (low == 7.0f && high == 10.0f)
		++attackDeadlineDraws;
	if (low >= .25f && low <= 1.25f && high == 1.5f)
		++attackAimRefreshDraws;
	if (low == 3.0f && high == 15.0f)
		++retreatTimerDraws;
	if (forcedNoticeFixtureRoll >= 0.0f && low == 0.0f && high == 100.0f)
		return forcedNoticeFixtureRoll;
	if (rejectAmbushRoll && low == 0.0f && high == 100.0f)
	{
		++rejectedPercentageRolls;
		return high;
	}
	if (forceLowPercentageRoll && low == 0.0f && high == 100.0f)
		return low;
	return siteFixtureRandomFloat(low, high);
}
void traceCombatVisibility(const float *start, const float *end, int ignoreMonsters, edict_t *self, TraceResult *result)
{
	traceObjectiveVisibility(start, end, ignoreMonsters, self, result);
	if (grenadeCornerTrace && self == &gEntities[0])
	{
		if (ignoreMonsters == 0x101 && std::fabs(end[2] - 36.0f) < .01f)
		{
			result->flFraction = grenadeCornerCase == 3 ? .5f : (grenadeCornerCase == 4 || grenadeCornerCase == 7) ? 1.0f :
				end[0] >= (grenadeCornerCase == 6 ? 420.0f : 200.0f) ? .5f : 1.0f;
			if (grenadeCornerResetRuntime)
			{
				auto *runtime = grenadeCornerResetRuntime;
				grenadeCornerResetRuntime = nullptr;
				runtime->beginRoundGeneration();
			}
		}
		if (ignoreMonsters == 0x100 && std::fabs(end[0] - start[0]) > 999.0f)
		{
			++grenadeCornerWallTraces;
			grenadeCornerWallStarts.emplace_back(start[0], start[1], start[2]);
			if (end[0] > start[0])
			{
				result->flFraction = .02f;
				result->vecEndPos = Vector(start[0] + 20.0f, start[1], start[2]);
			}
		}
		if (ignoreMonsters == 0x100 && std::fabs(end[1] - start[1]) > 999.0f)
		{
			++grenadeCornerWallTraces;
			grenadeCornerWallStarts.emplace_back(start[0], start[1], start[2]);
		}
	}
	if (occludedAimTraces && self == &gEntities[0] && ignoreMonsters == 0)
		occludedAimTraces->emplace_back(end[0], end[1], end[2]);
	if (enemyBehindCover && self == &gEntities[0] && (ignoreMonsters & 0xff) == 1 &&
		std::fabs(end[0] - gEntities[1].v.origin.x) <= 24.0f)
		result->flFraction = 0.0f;
	if (selfDefenseTrace && self == &gEntities[0] && ignoreMonsters == 0x101)
	{
		const auto eye = gEntities[1].v.origin + gEntities[1].v.view_ofs;
		const auto observerEye = self->v.origin + self->v.view_ofs;
		if (start[0] == observerEye.x && start[1] == observerEye.y && start[2] == observerEye.z &&
			end[0] == eye.x && end[1] == eye.y && end[2] == eye.z)
		{
			++selfDefenseTrace->eyeChecks;
			result->flFraction = selfDefenseTrace->eyeVisible ? 1.0f : .5f;
			if (selfDefenseTrace->replaceEnemy)
				++gEntities[1].serialnumber;
		}
	}
	if (coverBlocksEnemySight && ignoreMonsters == 0x101 && self == &gEntities[1])
		result->flFraction = 0.5f;
	if (humanFriendlyBlocks && ignoreMonsters == 0)
		result->pHit = &gEntities[2];
}

void combatGameClientCommand(edict_t *entity)
{
	if (entity == &gEntities[0] && std::strcmp(astrabot::metamod::HookCommandArgv(0), "weapon_c4") == 0)
		++combatC4Selections;
	if (equipmentFixture && entity == &gEntities[0])
	{
		const auto *name = astrabot::metamod::HookCommandArgv(0);
		unsigned id = std::strcmp(name, "weapon_ak47") == 0 ? 28U
			: std::strcmp(name, "weapon_glock18") == 0 ? 17U
			: equipmentFixture->plantTail && std::strcmp(name, "weapon_c4") == 0 ? 6U
			: std::strcmp(name, "weapon_knife") == 0 ? 29U : 0U;
		if (id)
		{
			equipmentFixture->commands.push_back(id);
			if (id == 6 && equipmentFixture->dropOnC4)
				entity->v.weapons &= ~(1 << 6);
			if (equipmentFixture->attackMutation && equipmentFixture->machine)
			{
				(void)equipmentFixture->machine->stopAttack(equipmentFixture->context);
				if (equipmentFixture->attackMutation == 2)
					(void)equipmentFixture->machine->beginAttack(equipmentFixture->context);
			}
			if (!equipmentFixture->refuseSwitch && (id != 6 || (entity->v.weapons & (1 << 6)) != 0))
			{
				equipmentFixture->active = static_cast<int>(id);
				auto &runtime = astrabot::metamod::PluginRuntime::instance();
				runtime.onMessageBegin(1, 94, nullptr, entity);
				runtime.onWriteByte(1);
				runtime.onWriteByte(static_cast<int>(id));
				runtime.onWriteByte(id == 28 ? equipmentFixture->primaryClip : id == 17 ? equipmentFixture->pistolClip : -1);
				runtime.onMessageEnd();
			}
			return;
		}
	}
	if (entity == &gEntities[0] && std::strcmp(astrabot::metamod::HookCommandArgv(0), "weapon_glock18") == 0)
		combatPistolSelected = true;
	hookedGameClientCommand(entity);
}

const std::uint32_t *combatFrameCounter = nullptr;
std::vector<std::pair<std::uint32_t, unsigned short>> combatSentInputs;
void runCombatPlayerMove(edict_t *entity, const float *angles, float forward, float side, float up,
	unsigned short buttons, byte impulse, byte msec)
{
	runPlayerMove(entity, angles, forward, side, up, buttons, impulse, msec);
	if (entity == &gEntities[0] && (buttons & IN_RELOAD) != 0U)
		++combatReloadCommands;
	// HLDS RunPlayerMove does not publish FakeClient v_angle; the input owner must.
	if (entity == &gEntities[0] && (buttons & IN_ATTACK) != 0U)
	{
		++combatAttackCommands;
		combatLastAttackPitch = angles[0];
	}
	if (entity == &gEntities[0])
		combatCommandYaw = angles[1];
	if (entity == &gEntities[0])
	{
		if (grenadeCornerTrace)
			grenadeCornerSent.emplace_back(astrabot::metamod::PluginRuntime::instance().snapshot().roundGeneration, buttons);
		combatCommandPitch = angles[0];
		combatCommandForward = forward;
		combatCommandSide = side;
		if ((buttons & IN_ATTACK) != 0U && std::hypot(forward, side) > .01f)
			++combatMovingFireCommands;
	}
	if (entity == &gEntities[0] && combatFrameCounter)
		combatSentInputs.push_back({*combatFrameCounter, buttons});
}

const char *grenadeAdmissionString(int index)
{
	return index == 20 ? "func_breakable" : szFromIndex(index);
}

void grenadeAdmissionClientCommand(edict_t *entity)
{
	if (entity == &gEntities[0] && std::strcmp(astrabot::metamod::HookCommandArgv(0), "slot1") == 0)
	{
		if (!grenadeAdmissionInReset)
			++grenadeAdmissionEquipRequests;
		return; // Selection is requested, but the public active weapon remains grenade.
	}
	combatGameClientCommand(entity);
}

cvar_t *grenadeAdmissionCvarPointer(const char *name)
{
	if (std::strcmp(name, "bot_allow_rifles") == 0)
	{
		if (grenadeAdmissionAttackMachine)
		{
			auto *machine = grenadeAdmissionAttackMachine;
			grenadeAdmissionAttackMachine = nullptr;
			if (machine->beginAttack(grenadeAdmissionAttackContext) == astrabot::compat::StateResult::Transitioned &&
				machine->stopAttack(grenadeAdmissionAttackContext) == astrabot::compat::StateResult::Transitioned)
				++grenadeAdmissionAttackMutations;
		}
		if (grenadeAdmissionResetRuntime)
		{
			auto *runtime = grenadeAdmissionResetRuntime;
			grenadeAdmissionResetRuntime = nullptr;
			++grenadeAdmissionResetCount;
			grenadeAdmissionInReset = true;
			runtime->beginRoundGeneration();
			grenadeAdmissionInReset = false;
		}
		return &grenadeAdmissionAllowed;
	}
	return grenadeAdmissionOriginalPointer ? grenadeAdmissionOriginalPointer(name) : nullptr;
}

float grenadeAdmissionCvarFloat(const char *name)
{
	if (std::strcmp(name, "bot_allow_rifles") == 0)
	{
		if (!grenadeAdmissionInReset)
			++grenadeAdmissionFloatReads;
		return 1.0f;
	}
	return grenadeAdmissionOriginalFloat ? grenadeAdmissionOriginalFloat(name) : 0.0f;
}

bool testCombatTargetLog(astrabot::metamod::PluginRuntime &runtime, enginefuncs_t &engine, globalvars_t &globals)
{
	gCaptureMovementLogs = true;
	gMovementLogs.clear();
	astrabot::metamod::RuntimeProfiler logProfiler;
	astrabot::metamod::runtime_detail::profiledLogConsole(logProfiler, nullptr, "action sensor target=%d", 0);
	if (!check(gMovementLogs.empty(), "normal logging suppresses detailed sensor records"))
		return false;
	astrabot::metamod::runtime_detail::importantLogConsole(logProfiler, nullptr, "join confirmed slot=%u", 1U);
	if (!check(gMovementLogs.size() == 1U && gMovementLogs[0] == "join confirmed slot=1",
			"important events remain available without detailed profiling"))
		return false;
	logProfiler.setEnabled(true, 0.0);
	astrabot::metamod::runtime_detail::profiledLogConsole(
		logProfiler, nullptr, "action sensor target=%d combatResult=%d", 0, 8);
	if (!check(gMovementLogs.size() == 2U && gMovementLogs[1] == "action sensor target=0 combatResult=8",
			"detailed mode retains the machine-readable legacy format"))
		return false;
	using astrabot::compat::NavigationStepOutcome;
	using astrabot::metamod::runtime_detail::navigationTransitionEvent;
	if (!check(std::strcmp(navigationTransitionEvent(NavigationStepOutcome::EndOfPath), "navigationComplete") == 0 &&
				std::strcmp(navigationTransitionEvent(NavigationStepOutcome::PathFailure), "navigationFailure") == 0 &&
				std::strcmp(navigationTransitionEvent(NavigationStepOutcome::ObservationUnavailable),
					"navigationTransition") == 0,
			"navigation event names distinguish completion, failure and unavailable observations"))
		return false;
	gObjectiveEntitiesAvailable = false;
	gServerSetsEntityTeam = true;
	engine.pfnRandomLong = &siteFixtureRandomLong;
	engine.pfnRandomFloat = &combatRandomFloat;
	engine.pfnTraceLine = &traceCombatVisibility;
	engine.pfnRunPlayerMove = &runCombatPlayerMove;
	gHookedGameDllTable.pfnClientCommand = &combatGameClientCommand;
	combatFrameCounter = &static_cast<astrabot::metamod::BotObservationServices &>(runtime).service_adapterFrameCount();
	gObjectiveTraceVisible = true;
	runtime.giveEnginePointers(&engine, &globals);
	gEntities[0].v.flags = FL_CLIENT | FL_FAKECLIENT | FL_ONGROUND;
	gEntities[0].v.team = 1;
	gEntities[0].v.health = 100.0f;
	gEntities[0].v.solid = SOLID_SLIDEBOX;
	gEntities[0].v.movetype = MOVETYPE_WALK;
	gEntities[0].v.maxspeed = 240.0f;
	gEntities[0].v.origin = Vector(80.0f, 32.0f, 36.0f);
	gEntities[0].v.view_ofs = Vector(0.0f, 0.0f, 17.0f);
	gEntities[0].v.fov = 90.0f;
	gEntities[0].v.weapons = 1 << 22;
	runtime.onMessageBegin(1, 94, nullptr, &gEntities[0]);
	runtime.onWriteByte(1);
	runtime.onWriteByte(22);
	runtime.onWriteByte(30);
	runtime.onMessageEnd();
	const auto stepRifle = [&](int clip, float yaw = 0.0f, int weapon = 22)
	{
		runtime.onMessageBegin(1, 94, nullptr, &gEntities[0]);
		runtime.onWriteByte(1);
		runtime.onWriteByte(weapon);
		runtime.onWriteByte(clip);
		runtime.onMessageEnd();
		gEntities[0].v.v_angle = Vector(0, yaw, 0);
		globals.time += .125f;
		runtime.onStartFrame();
		runtime.onStartFramePost();
	};
#ifdef _MSC_VER
	char *scenario = nullptr;
	std::size_t scenarioSize = 0;
	(void)_dupenv_s(&scenario, &scenarioSize, "ASTRABOT_TEST_COMBAT_SCENARIO");
#else
	const char *scenario = std::getenv("ASTRABOT_TEST_COMBAT_SCENARIO");
#endif
	const bool reacquireFixture = scenario && std::strcmp(scenario, "reacquire") == 0;
	const bool gunfireFixture = scenario && std::strcmp(scenario, "gunfire") == 0;
	const bool noticeFixture = scenario && std::strcmp(scenario, "notice") == 0;
	const bool occludedFixture = scenario && std::strcmp(scenario, "occluded") == 0;
	const bool selfDefenseFixture = scenario && std::strcmp(scenario, "self-defense") == 0;
	const bool shieldFixture = scenario && std::strcmp(scenario, "shield") == 0;
	const bool bestEquipmentFixture = scenario && std::strcmp(scenario, "equip-best") == 0;
	const bool plantTailFixture = scenario && std::strcmp(scenario, "plant-tail") == 0;
	const bool clearLookFixture = scenario && std::strcmp(scenario, "clear-look") == 0;
	const bool sharedLookFixture = scenario && std::strcmp(scenario, "shared-look") == 0;
	const bool sharedReentryFixture = scenario && std::strcmp(scenario, "shared-look-reentry") == 0;
	const bool plantEntryFixture = scenario && std::strcmp(scenario, "plant-entry") == 0;
	const bool grenadeCornerFixture = scenario && std::strncmp(scenario, "native-grenade-corner", 21) == 0;
	grenadeCornerCase = scenario && std::strstr(scenario, "-he-boundary") ? 1U :
		scenario && std::strstr(scenario, "-flash-boundary") ? 2U :
		scenario && std::strstr(scenario, "-first-hidden") ? 3U :
		scenario && std::strstr(scenario, "-all-visible") ? 4U :
		scenario && std::strstr(scenario, "-reentry") ? 5U :
		scenario && std::strstr(scenario, "-diagonal") ? 6U :
		scenario && std::strstr(scenario, "-reused-tail") ? 7U : 0U;
	const bool grenadeAdmissionFixture = scenario && std::strcmp(scenario, "native-grenade-admission") == 0;
	const bool grenadeNoiseReentryFixture = scenario && std::strcmp(scenario, "native-grenade-noise-reentry") == 0;
	const bool grenadeNoiseOrderFixture = grenadeNoiseReentryFixture || (scenario && std::strcmp(scenario, "native-grenade-noise-order") == 0);
	const bool grenadeNoiseFixture = grenadeNoiseOrderFixture || (scenario && std::strcmp(scenario, "native-grenade-noise") == 0);
	const bool huntApproachControl = scenario && std::strcmp(scenario, "hunt-approach-control") == 0;
	const bool grenadeHuntFixture = huntApproachControl || (scenario && std::strcmp(scenario, "native-grenade-hunt-admission") == 0);
	const bool grenadeMovingFixture = scenario && std::strcmp(scenario, "native-grenade-moving-admission") == 0;
	const bool grenadeOwnerFixture = scenario && std::strcmp(scenario, "native-grenade-owner-retirement") == 0;
	const bool grenadeZeroReentryFixture = scenario && std::strcmp(scenario, "native-grenade-zero-reentry") == 0;
	const bool grenadeRootReentryFixture = scenario && std::strcmp(scenario, "native-grenade-root-reentry") == 0;
	const bool grenadeRootFrameReentryFixture = scenario && std::strcmp(scenario, "native-grenade-root-frame-reentry") == 0;
	const bool grenadeZeroApproachFixture = grenadeZeroReentryFixture || grenadeRootReentryFixture ||
		grenadeRootFrameReentryFixture || (scenario && std::strcmp(scenario, "native-grenade-zero-approach") == 0);
	const bool nativeGrenadeFixture = grenadeCornerFixture || grenadeAdmissionFixture || grenadeNoiseFixture || grenadeZeroApproachFixture || grenadeHuntFixture || grenadeMovingFixture || grenadeOwnerFixture || (scenario && std::strcmp(scenario, "native-grenade") == 0);
#ifdef _MSC_VER
	std::free(scenario);
#endif
	if (nativeGrenadeFixture)
	{
		using namespace astrabot;
		// CSBot Update holds PrimaryAttack whenever a grenade is equipped,
		// before either recognized-enemy or end-of-safe-time toss selection.
		// This exercises the real full-update/RunPlayerMove path without a
		// target or any supplied ballistic trajectory receipt.
		combatGameDll->pfnUpdateClientData = nullptr;
		combatGameDll->pfnGetWeaponData = nullptr;
		runtime.setCompatibilityString("astrabot_mode", "compatibility");
		runtime.setCompatibilityFloat("bot_enable", 1.0f);
		runtime.setCompatibilityFloat("bot_stop", 0.0f);
		gEntities[1].free = true;
		gEntities[1].v.health = 0.0f;
		gEntities[1].v.deadflag = DEAD_DEAD;
		gEntities[0].v.weapons = 1 << 4;
		runtime.beginRoundGeneration();
		const auto movesBeforeGrenade = gRunPlayerMoveCount;
		stepRifle(1, 0.0f, 4);
		stepRifle(1, 0.0f, 4);
		auto &grenadePorts = static_cast<metamod::BotBehaviorServices &>(runtime);
		const auto &grenadeHandle = grenadePorts.service_managedBotHandles()[0];
		const auto &grenadeLife = grenadePorts.service_lifecycle();
		const world::ActorKey grenadeActor{grenadeHandle.actor.slot, grenadeHandle.actor.actorGeneration};
		const world::FrameIdentity grenadeFrame{grenadeLife.mapGeneration(), grenadeLife.roundGeneration(),
			grenadePorts.service_adapterFrameCount()};
		compat::PublicActorState grenadeObservation{};
		const auto &grenadeConfig = grenadePorts.service_compatibilitySurface().configuration();
		if (!check(grenadePorts.observation_publicObservations().read(grenadeActor, grenadeFrame, &grenadeObservation) &&
				grenadeObservation.weaponKnown && grenadeObservation.weaponId == 4 &&
				grenadeConfig.mode == compat::RuntimeMode::Compatibility && grenadeConfig.botEnable > 0.0f &&
				grenadeConfig.botStop == 0.0f && gEntities[0].v.deadflag == DEAD_NO && gEntities[0].v.health > 0.0f &&
				(gEntities[0].v.flags & FL_FROZEN) == 0 &&
				!(grenadePorts.service_roundFreezeGeneration() == grenadeFrame.roundGeneration &&
					globals.time < grenadePorts.service_roundFreezeUntil()) &&
				gRunPlayerMoveCount > movesBeforeGrenade && gLastRunPlayerMoveActor == 1 &&
				!static_cast<metamod::BotNavigationServices &>(runtime).behavior_managedBotStateMachines(0).attackOverlayActive(),
				"C02 control: live grenade observation and real command without an Attack overlay"))
			return false;
		if (!check((gLastRunPlayerMoveButtons & IN_ATTACK) != 0,
				"C02 equipped grenade holds PrimaryAttack without a target or ballistic receipt"))
			return false;
		static unsigned grenadeIdentityReads = 0;
		grenadeIdentityReads = 0;
		combatGameDll->pfnUpdateClientData = [](const edict_t *, int, clientdata_t *data) {
			++grenadeIdentityReads;
			data->m_iId = 0; // Available prediction explicitly reports no active weapon.
		};
		stepRifle(1, 0.0f, 4); // A stale positive CurWeapon must not override that.
		if (!check(grenadeIdentityReads > 0 && (gLastRunPlayerMoveButtons & IN_ATTACK) == 0,
				"C02 available GameDLL no-active-weapon overrides stale grenade CurWeapon"))
			return false;
		combatGameDll->pfnUpdateClientData = [](const edict_t *, int, clientdata_t *data) { data->m_iId = 4; };
		gEntities[0].v.weapons = (1 << 4) | (1 << 22);
		stepRifle(30, 0.0f, 22);
		if (!check((gLastRunPlayerMoveButtons & IN_ATTACK) != 0,
				"C02 public GameDLL grenade identity is positive even before CurWeapon updates"))
			return false;
		std::unique_ptr<metamod::BotBehaviorRuntime> movingBehavior;
		if (grenadeCornerFixture)
		{
			if (!check(loadMovingEncounterNav(runtime), "corner control: connected native route NAV loads")) return false;
			for (unsigned n = 0; n < 8; ++n) stepRifle(1, 0, 4);
			const auto nav = grenadePorts.service_navPublisher().snapshot();
			const world::FrameIdentity frame{grenadeLife.mapGeneration(), grenadeLife.roundGeneration(),
				grenadePorts.service_adapterFrameCount()};
			const float targetY = grenadeCornerCase == 6 ? 50.0f : 32.0f;
			if (!check(grenadePorts.navigation_tryFollowCoverPath(0, nav, frame, 1, 3, {450, targetY, 0}, [] { return true; }) &&
				grenadePorts.navigation_hasActivePath(0), "corner control: real current navigation owns the path")) return false;
			const auto path = grenadePorts.navigation_grenadePath(0, nav);
			if (!check(path.next == 1U && path.points.size() == 4U && path.points[1].x == 105.0f &&
				path.points[2].x == 305.0f && path.points[3].x == 450.0f,
				"corner control: native portal geometry and current next index are published without advancing the route")) return false;
			if (grenadeCornerCase == 7)
			{
				globals.time += 1.0f;
				if (!check(grenadePorts.navigation_tryFollowCoverPath(0, nav, frame, 1, 1, {80, 32, 0}, [] { return true; }),
					"C02-5 real long-to-trivial route reuses native slots")) return false;
			}
			const float roundStart = grenadePorts.service_roundFreezeGeneration() == frame.roundGeneration
				? grenadePorts.service_roundFreezeStartTime() : 0.0f;
			const float aggression = static_cast<float>(static_cast<metamod::BotNavigationServices &>(runtime).behavior_managedProfiles(0).aggression) / 100.0f;
			const float safe = 15.0f + 5.0f * aggression;
			grenadeCornerTrace = true;
			grenadeCornerWallTraces = 0;
			grenadeCornerWallStarts.clear();
			const int weapon = grenadeCornerCase == 2 ? 25 : 4;
			if (weapon == 25)
			{
				gEntities[0].v.weapons |= 1 << 25;
				combatGameDll->pfnUpdateClientData = [](const edict_t *, int, clientdata_t *data) { data->m_iId = 25; };
			}
			if (grenadeCornerCase == 1 || grenadeCornerCase == 2)
			{
				globals.time = roundStart + safe - .125f;
				stepRifle(1, 0, weapon); // Exactly the strict safe boundary, before wellPastSafe.
			}
			if (grenadeCornerCase == 1)
			{
				if (!check(grenadeCornerWallTraces == 0, "C02-3 HE refuses the ordinary safe edge before wellPastSafe")) return false;
				globals.time += 30.0f;
				stepRifle(1, 0, weapon);
				return check(grenadeCornerWallTraces == 0, "C02-3 consumed safe edge does not retry HE on a later unsafe full update");
			}
			if (grenadeCornerCase != 2)
			{
				globals.time = roundStart + 1.25f * safe + .25f;
				if (grenadeCornerCase == 5) grenadeCornerResetRuntime = &runtime;
				const auto moves = gRunPlayerMoveCount;
				grenadeCornerSent.clear();
				stepRifle(1, 0, weapon);
				if (grenadeCornerCase == 5)
				{
					std::fprintf(stderr, "corner reentry callback=%d walls=%u moves=%u->%u attack=%u round=%u->%u\n",
						grenadeCornerResetRuntime == nullptr ? 1 : 0, grenadeCornerWallTraces, moves, gRunPlayerMoveCount,
						gLastRunPlayerMoveButtons & IN_ATTACK, frame.roundGeneration, grenadeLife.roundGeneration());
					return check(grenadeCornerResetRuntime == nullptr && grenadeCornerWallTraces == 0 && gRunPlayerMoveCount == moves + 1 &&
						grenadeCornerSent.size() == 1U && grenadeCornerSent[0].first == grenadeLife.roundGeneration() &&
						grenadeCornerSent[0].second == 0U && combatCommandForward == 0.0f && combatCommandSide == 0.0f,
						"C02-3 round reentry aborts stale corner work and sends only the new generation neutral input");
				}
			}
			if (grenadeCornerCase == 3)
				return check(grenadeCornerWallTraces == 0 && (gLastRunPlayerMoveButtons & IN_ATTACK) != 0,
					"C02-3 invisible first path node cannot begin a toss");
			if (grenadeCornerCase == 7)
				return check(grenadeCornerWallTraces == 4 && !grenadeCornerWallStarts.empty() &&
					grenadeCornerWallStarts[0].x == 280.0f && grenadeCornerWallStarts[0].y == 32.0f &&
					grenadeCornerWallStarts[0].z == 46.0f,
					"C02-5 reused trivial path toss samples toward old slot2, not zero or endpoint");
			if (grenadeCornerCase == 4)
				return check(grenadeCornerWallTraces == 4 && !grenadeCornerWallStarts.empty() &&
					std::fabs(grenadeCornerWallStarts[0].x - 1.133453369140625f) < .001f &&
					std::fabs(grenadeCornerWallStarts[0].y - .080600738525390625f) < .001f &&
					grenadeCornerWallStarts[0].z == 46.0f,
					"C02-5 all-visible route samples toward the initially zero native tail");
			if (!check(grenadeCornerWallTraces == 4, "C02-3 safe edge selects the path corner and applies four wall buffers")) return false;
			// Fixed-reference Vector::NormalizeInPlace divides each component before t*dir.
			const float expectedX = grenadeCornerCase == 6 ? 404.23828125f : 180.0f;
			const float expectedY = grenadeCornerCase == 6 ? 44.319236755371094f : 32.0f;
			const float bufferedX = expectedX - 30.0f;
			if (!check(grenadeCornerWallStarts.size() == 4U && grenadeCornerWallStarts[0].x == expectedX &&
				grenadeCornerWallStarts[0].y == expectedY && grenadeCornerWallStarts[0].z == 46.0f &&
				grenadeCornerWallStarts[1].x == bufferedX && grenadeCornerWallStarts[2].x == bufferedX &&
				grenadeCornerWallStarts[3].x == bufferedX,
				"C02-3 samples at 25 units with half-height plus ten and applies wall buffers in native order")) return false;
			bool released = false;
			for (unsigned n = 0; n < 30; ++n)
			{
				stepRifle(1, 180, weapon); // Remain unaligned: strict three-second fallback releases.
				released = released || (gLastRunPlayerMoveButtons & IN_ATTACK) == 0;
			}
			grenadeCornerTrace = false;
			return check(released && grenadeCornerWallTraces == 4,
				"C02-3 path-corner toss releases through real RunPlayerMove without an enemy or repeated selection");
		}
		runtime::MovementPhysicsState movingBefore{};
		const auto makeMovingBehavior = [&] {
			auto result = std::make_unique<metamod::BotBehaviorRuntime>();
			result->bindServices(grenadePorts);
			result->onActorRemembered(0, grenadeHandle);
			compat::StateUpdateContext tick{};
			tick.actor = grenadeActor;
			tick.frame = {grenadeLife.mapGeneration(), grenadeLife.roundGeneration(), grenadePorts.service_adapterFrameCount()};
			tick.timestamp = globals.time;
			tick.fullUpdateSequence = grenadePorts.service_managedBotFullUpdateSequences()[0];
			tick.fullUpdate = tick.alive = tick.roundActive = true;
			tick.lifeAvailability = tick.roundAvailability = tick.attackObservation = compat::ObservationAvailability::Available;
			auto &state = const_cast<compat::CompatibilityStateMachine &>(result->managedBotStateMachines(0));
			state.initialize(tick);
			state.transition(compat::CompatibilityStateId::Hunt, compat::StateTransitionReason::ExplicitRequest,
				compat::StateTransitionId::ExplicitStateChange, tick);
			return result;
		};
		if (grenadeMovingFixture)
		{
			globals.time += 25.0f; // Unsafe even with the maximum unadjusted native safe time.
			movingBehavior = makeMovingBehavior();
			movingBefore.grounded = true;
			movingBefore.flags = gEntities[0].v.flags;
			movingBefore.origin = {gEntities[0].v.origin.x, gEntities[0].v.origin.y, gEntities[0].v.origin.z};
			movingBefore.velocity = {20, 0, 0};
		}
		if (grenadeOwnerFixture)
		{
			if (!check(loadInvestigationNav(runtime, true), "owner control: connected approach NAV loads")) return false;
			gMovementLogs.clear();
			gProfileEnabled = true;
			for (unsigned warmup = 0; warmup < 8; ++warmup) stepRifle(30, 0, 4);
			gProfileEnabled = false;
			const bool adopted = std::any_of(gMovementLogs.begin(), gMovementLogs.end(), [](const std::string &line) {
				return line.find("hunt approach look slot=") != std::string::npos;
			});
			if (!check(adopted, "owner control: Root adopts ordinary LOW before public enemy exposure")) return false;
		}
		auto &grenadeEnemy = gEntities[1];
		const auto grenadeAimDrawsBefore = attackAimRefreshDraws;
		runtime.onClientPutInServer(&grenadeEnemy);
		grenadeEnemy.free = huntApproachControl;
		grenadeEnemy.v.flags = FL_CLIENT | FL_ONGROUND;
		grenadeEnemy.v.health = 100.0f;
		grenadeEnemy.v.deadflag = DEAD_NO;
		grenadeEnemy.v.team = 2;
		grenadeEnemy.v.origin = gEntities[0].v.origin + Vector(320, 0, 256);
		const Vector grenadeObservedPoint = grenadeEnemy.v.origin;
		grenadeEnemy.v.view_ofs = Vector(0, 0, 19);
		runtime.onMessageBegin(MSG_ALL, 86, nullptr, nullptr);
		runtime.onWriteByte(2);
		runtime.onWriteString("CT");
		runtime.onMessageEnd();
		forcedNoticeFixtureRoll = 0.0f;
		engine.pfnTraceHull = nullptr; // Native direct-look tossing has no ballistic-hull prerequisite.
		if (grenadeAdmissionFixture)
			engine.pfnSzFromIndex = &grenadeAdmissionString;
		bool grenadeReleased = false, grenadeTurned = false;
		for (unsigned grenadeFrameIndex = 0; grenadeFrameIndex < 40; ++grenadeFrameIndex)
		{
			if (grenadeFrameIndex == 8)
			{
				grenadeEnemy.free = true;
				grenadeEnemy.v.origin = gEntities[0].v.origin + Vector(-1000, 0, -256);
			}
			stepRifle(30, 0.0f, 22); // Deliberately stale rifle message; API still reports grenade.
			if (grenadeOwnerFixture && grenadeFrameIndex == 11)
				return check((gLastRunPlayerMoveButtons & IN_ATTACK) && combatCommandPitch < -1,
					"owner control: public enemy recognition starts native grenade view and retains pin after contact loss");
			if (grenadeMovingFixture)
			{
				movingBehavior->updateManagedBotCompatibilityState(0, movingBefore, false);
				if (grenadeFrameIndex == 11)
				{
					const auto pin = movingBehavior->decideManagedBotAction(0, movingBefore, {}, 0);
					if (!check((pin.movementButtons & IN_ATTACK) && movingBehavior->upkeepGrenade(0, movingBefore).hasAim &&
							loadMovingEncounterNav(runtime), "C02 moving control: native spot/pin alive and valid directional encounter NAV")) return false;
					const auto navigation = grenadePorts.service_navPublisher().snapshot();
					const auto *middle = navigation.document()->findArea(2);
					if (!check(middle && middle->encounters.size() == 1 && navigation.findHidingSpot(42),
							"C02 moving control: real loaded middle encounter and hiding spot")) return false;
					const nav::NavHuntEncounter path{middle, &middle->encounters.front()};
					movingBefore.origin = {200, 32, 36};
					struct EncounterDraws final : compat::IRandomTraceSink
					{
						unsigned intervals = 0, points = 0;
						std::string order;
						void record(const compat::RandomTraceRecord &draw) override {
							if (std::strcmp(draw.request.semanticId, "LookAround.EncounterInterval") == 0) { ++intervals; order += 'I'; }
							if (std::strcmp(draw.request.semanticId, "LookAround.EncounterSpot") == 0) { ++points; order += 'P'; }
						}
					} draws;
					auto ordinary = makeMovingBehavior();
					metamod::HuntApproachLook ordinaryLook{};
					grenadePorts.service_compatibilityRandomSource().setTraceSink(&draws);
					const bool accepted = ordinary->updateHuntApproachLook(0, movingBefore, navigation, path, false, [] { return true; }, ordinaryLook);
					grenadePorts.service_compatibilityRandomSource().setTraceSink(nullptr);
					if (!check(accepted && draws.intervals == 1 && draws.points == 1 && draws.order == "IP" && ordinaryLook.update == metamod::HuntApproachLookUpdate::TrackEncounter,
							"C02 moving control: without native spot unsafe movement consumes both encounter draws and tracks")) return false;
					draws.intervals = draws.points = 0;
					draws.order.clear();
					const auto &nativeState = movingBehavior->managedBotStateMachines(0);
					if (!check(nativeState.state() == compat::CompatibilityStateId::Hunt && !nativeState.attackOverlayActive(),
							"C02 moving control: native candidate reaches normal Hunt without an early overlay gate")) return false;
					metamod::HuntApproachLook refused{};
					grenadePorts.service_compatibilityRandomSource().setTraceSink(&draws);
					const bool completed = movingBehavior->updateHuntApproachLook(0, movingBefore, navigation, path, false, [] { return true; }, refused);
					grenadePorts.service_compatibilityRandomSource().setTraceSink(nullptr);
					if (!check(completed && draws.intervals == 0 && draws.points == 0 && refused.update == metamod::HuntApproachLookUpdate::Unchanged,
						"C02 moving native grenade spot inhibits encounter selection before interval or candidate RNG")) return false;
					const auto oldView = gEntities[0].v.v_angle;
					const auto eye = gEntities[0].v.origin + gEntities[0].v.view_ofs;
					const auto aligned = metamod::ActionAdapter::useEntityView(grenadeObservedPoint.x - eye.x,
						grenadeObservedPoint.y - eye.y, grenadeObservedPoint.z - eye.z);
					gEntities[0].v.v_angle = Vector(aligned.pitch, aligned.yaw, 0);
					const bool retained = movingBehavior->upkeepGrenade(0, movingBefore).hasAim;
					globals.time += 3.01f; // Expire only the independent, aligned look lifetime.
					const bool finalAim = movingBehavior->upkeepGrenade(0, movingBefore).hasAim;
					const bool expired = !movingBehavior->upkeepGrenade(0, movingBefore).hasAim;
					gEntities[0].v.v_angle = oldView;
					metamod::HuntApproachLook resumed{};
					grenadePorts.service_compatibilityRandomSource().setTraceSink(&draws);
					const bool resumedUpdate = movingBehavior->updateHuntApproachLook(0, movingBefore, navigation, path, false, [] { return true; }, resumed);
					grenadePorts.service_compatibilityRandomSource().setTraceSink(nullptr);
					std::fprintf(stderr, "moving expiry retained=%d final=%d expired=%d resumed=%d interval=%u point=%u order=%s\n",
						retained, finalAim, expired, resumedUpdate, draws.intervals, draws.points, draws.order.c_str());
					return check(retained && finalAim && expired && resumedUpdate && draws.intervals == 1 && draws.points == 1 && draws.order == "IP" &&
						resumed.update == metamod::HuntApproachLookUpdate::TrackEncounter,
						"C02 moving refusal preserves native look and leaves the encounter clock/spot eligible after look expiry");
				}
			}
			if (grenadeHuntFixture && grenadeFrameIndex == 11)
			{
				if (!check((gLastRunPlayerMoveButtons & IN_ATTACK) && (huntApproachControl || combatCommandPitch < -1) &&
						loadInvestigationNav(runtime, true),
						"C02-2c2 control: retained native grenade spot/pin and connected NAV with approaches")) return false;
				stepRifle(30, 0, 4);
				stepRifle(30, 0, 4);
				const auto &state = static_cast<metamod::BotNavigationServices &>(runtime).behavior_managedBotStateMachines(0);
				const auto epoch = state.normalStateEpoch();
				if (!check(state.state() == compat::CompatibilityStateId::Hunt && !state.attackOverlayActive(),
						"C02-2c2 control: normal Hunt after movement warmup")) return false;
				struct HuntDraws final : compat::IRandomTraceSink
				{
					unsigned intervals = 0, points = 0;
					std::string order;
					void record(const compat::RandomTraceRecord &draw) override
					{
						if (std::strcmp(draw.request.semanticId, "LookAround.StateInterval") == 0) { ++intervals; order += 'I'; }
						if (std::strcmp(draw.request.semanticId, "LookAround.ApproachPoint") == 0) { ++points; order += 'P'; }
					}
				} draws;
				gMovementLogs.clear();
				gProfileEnabled = true;
				grenadePorts.service_compatibilityRandomSource().setTraceSink(&draws);
				stepRifle(30, 0, 4);
				gProfileEnabled = false;
				const bool published = std::any_of(gMovementLogs.begin(), gMovementLogs.end(), [](const std::string &line) {
					return line.find("hunt approach look slot=") != std::string::npos;
				});
				const bool firstDraws = draws.intervals == 1 && draws.points == 1;
				stepRifle(30, 0, 4);
				grenadePorts.service_compatibilityRandomSource().setTraceSink(nullptr);
				if (!check(state.state() == compat::CompatibilityStateId::Hunt && state.normalStateEpoch() == epoch && firstDraws &&
						draws.intervals == 1 && draws.points == 1 && draws.order == "IP" && (gLastRunPlayerMoveButtons & IN_ATTACK) &&
						(huntApproachControl || combatCommandPitch < -1),
						"C02-2c2 control: stationary candidate draws interval then point while native pin is held")) return false;
				return check(published == huntApproachControl, huntApproachControl
					? "C02-2c2 ordinary Hunt accepts the stationary candidate without a native grenade spot"
					: "C02-2c2 native grenade spot refuses the lower-priority stationary Hunt candidate after both draws");
			}
			if (grenadeZeroApproachFixture && grenadeFrameIndex == 11)
			{
				engine.pfnSzFromIndex = &grenadeAdmissionString;
				DLL_FUNCTIONS hooks{};
				int version = INTERFACE_VERSION;
				if (!check(runtime.provideEntityApi(&hooks, &version) && hooks.pfnKeyValue && hooks.pfnTouch,
						"C02-2c zero approach control: public material and Touch hooks available"))
					return false;
				auto &contact = gEntities[4];
				contact = {};
				contact.serialnumber = 42;
				contact.v.classname = 20;
				contact.v.takedamage = DAMAGE_YES;
				contact.v.health = 50;
				contact.v.solid = SOLID_BSP;
				// Include the touching actor on every axis, so a newly prepared
				// route cannot independently reject this admission probe.
				contact.v.absmin = gEntities[0].v.origin + Vector(-16, -16, -36);
				contact.v.absmax = gEntities[0].v.origin + Vector(40, 32, 44);
				KeyValueData material{};
				material.szClassName = const_cast<char *>("func_breakable");
				material.szKeyName = const_cast<char *>("material");
				material.szValue = const_cast<char *>("1");
				const auto probe = [&] {
					hooks.pfnKeyValue(&contact, &material);
					hooks.pfnTouch(&gEntities[0], &contact);
					runtime::BotCommand command{};
					command.viewAngles = {-12, 75, 0};
					command.movement = {120, 15, 0, IN_ATTACK, 0, 33};
					gHookedGameDllTable.pfnClientCommand = &grenadeAdmissionClientCommand;
					static_cast<metamod::BotCommandServices &>(runtime).behavior_applyObstacleCommand(0, command);
					gHookedGameDllTable.pfnClientCommand = &combatGameClientCommand;
					return command;
				};
				const auto before = probe();
				if (!check(!before.synchronizeCombatView && (gLastRunPlayerMoveButtons & IN_ATTACK) && combatCommandPitch < -1,
						"C02-2c zero approach control: native grenade spot rejects public contact before the clear"))
					return false;
				contact.free = TRUE;
				runtime::BotCommand discard{};
				static_cast<metamod::BotCommandServices &>(runtime).behavior_applyObstacleCommand(0, discard);
				if (!check(loadHearingNav(runtime), "C02-2c zero approach control: real containing NAV has zero approaches"))
					return false;
				const auto &state = static_cast<metamod::BotNavigationServices &>(runtime).behavior_managedBotStateMachines(0);
				// NAV was unavailable during the native toss setup. Both normal
				// movement warmup frames must finish before measuring lookaround.
				stepRifle(30, 0, 4);
				stepRifle(30, 0, 4);
				if (!check(state.state() == compat::CompatibilityStateId::Hunt,
						"C02-2c zero approach control: Hunt entry has completed before measuring its update"))
					return false;
				const auto epoch = state.normalStateEpoch();
				struct ApproachDraws final : compat::IRandomTraceSink
				{
					unsigned intervals = 0, points = 0;
					std::function<void()> interrupt;
					void record(const compat::RandomTraceRecord &draw) override
					{
						if (std::strcmp(draw.request.semanticId, "LookAround.StateInterval") == 0)
						{
							++intervals;
							if (interrupt) { auto callback = std::move(interrupt); interrupt = {}; callback(); }
						}
						if (std::strcmp(draw.request.semanticId, "LookAround.ApproachPoint") == 0) ++points;
					}
				} draws;
				bool restarted = false;
				const auto attackEpoch = state.attackEntryEpoch();
				unsigned recursiveCalls = 0;
				std::uint32_t recursiveFullBefore = 0, recursiveFullAfter = 0;
				unsigned recursiveMovesBefore = 0, recursiveMovesAfter = 0;
				float recursiveTimeBefore = 0, recursiveTimeAfter = 0;
				const auto movesBeforeRoot = gRunPlayerMoveCount;
				if (grenadeRootReentryFixture || grenadeRootFrameReentryFixture)
				{
					draws.interrupt = [&]
					{
						++recursiveCalls;
						recursiveTimeBefore = globals.time;
						recursiveFullBefore = grenadePorts.service_managedBotFullUpdateSequences()[0];
						recursiveMovesBefore = gRunPlayerMoveCount;
						if (grenadeRootFrameReentryFixture)
						{
							runtime.onStartFrame();
						}
						runtime.onStartFramePost();
						recursiveMovesAfter = gRunPlayerMoveCount;
						recursiveFullAfter = grenadePorts.service_managedBotFullUpdateSequences()[0];
						recursiveTimeAfter = globals.time;
					};
				}
				if (grenadeZeroReentryFixture) draws.interrupt = [&] {
					auto &machine = const_cast<compat::CompatibilityStateMachine &>(state);
					compat::StateUpdateContext context{};
					context.actor = grenadeActor;
					context.frame = {grenadeLife.mapGeneration(), grenadeLife.roundGeneration(), grenadePorts.service_adapterFrameCount()};
					context.timestamp = globals.time;
					context.fullUpdateSequence = grenadePorts.service_managedBotFullUpdateSequences()[0];
					context.fullUpdate = context.alive = context.roundActive = true;
					context.lifeAvailability = context.roundAvailability = context.attackObservation = compat::ObservationAvailability::Available;
					restarted = machine.beginAttack(context) == compat::StateResult::Transitioned &&
						machine.stopAttack(context) == compat::StateResult::Transitioned;
				};
				grenadePorts.service_compatibilityRandomSource().setTraceSink(&draws);
				stepRifle(30, 0, 4);
				if (grenadeRootReentryFixture || grenadeRootFrameReentryFixture)
				{
					const auto movesAfterRoot = gRunPlayerMoveCount;
					const bool sameState = state.state() == compat::CompatibilityStateId::Hunt &&
						state.normalStateEpoch() == epoch && state.attackEntryEpoch() == attackEpoch &&
						!state.attackOverlayActive();
					const bool admittedOnce = recursiveCalls == 1 && recursiveTimeBefore == recursiveTimeAfter &&
						recursiveFullBefore == recursiveFullAfter && recursiveMovesBefore == recursiveMovesAfter &&
						draws.intervals == 1 && draws.points == 0;
					stepRifle(30, 0, 4);
					grenadePorts.service_compatibilityRandomSource().setTraceSink(nullptr);
					std::fprintf(stderr, "Root reentry frame=%d calls=%u full=%u/%u time=%.9f/%.9f moves=%u/%u outer=%u/%u intervals=%u points=%u\n",
						grenadeRootFrameReentryFixture, recursiveCalls, recursiveFullBefore, recursiveFullAfter,
						recursiveTimeBefore, recursiveTimeAfter, recursiveMovesBefore, recursiveMovesAfter,
						movesBeforeRoot, movesAfterRoot, draws.intervals, draws.points);
					if (!check(sameState && admittedOnce,
						"same-time direct Root recursion cannot admit a second full update or command"))
					{
						return false;
					}
					return check(grenadeRootFrameReentryFixture
						? movesAfterRoot == movesBeforeRoot && draws.intervals == 2 && draws.points == 0
						: movesAfterRoot == movesBeforeRoot + 1 && draws.intervals == 1 && draws.points == 0,
						grenadeRootFrameReentryFixture
							? "new-frame same-time Root recursion aborts the old command and leaves its look clock uncommitted"
							: "same-frame same-time Root recursion preserves the single outer command and look clock");
				}
				if (grenadeZeroReentryFixture)
				{
					const bool control = restarted && state.attackEntryEpoch() == attackEpoch + 1 &&
						!state.attackOverlayActive() && state.normalStateEpoch() == epoch && draws.intervals == 1;
					if (!control)
					{
						grenadePorts.service_compatibilityRandomSource().setTraceSink(nullptr);
						return check(false, "C02-2c callback changes only Attack epoch and returns to the same Hunt");
					}
					stepRifle(30, 0, 4);
					grenadePorts.service_compatibilityRandomSource().setTraceSink(nullptr);
					return check(draws.intervals == 2 && draws.points == 0,
						"C02-2c Attack restart during interval RNG cannot commit the old look clock or clear");
				}
				grenadePorts.service_compatibilityRandomSource().setTraceSink(nullptr);
				std::fprintf(stderr, "C02 zero setup state=%d epoch_before=%llu epoch_after=%llu intervals=%u points=%u buttons=%u\n",
					static_cast<int>(state.state()), static_cast<unsigned long long>(epoch),
					static_cast<unsigned long long>(state.normalStateEpoch()), draws.intervals, draws.points,
					gLastRunPlayerMoveButtons);
				if (!check(state.state() == compat::CompatibilityStateId::Hunt && state.normalStateEpoch() == epoch &&
						draws.intervals == 1 && draws.points == 0 && (gLastRunPlayerMoveButtons & IN_ATTACK),
						"C02-2c zero approach control: same Hunt update draws interval only and retains native pin"))
					return false;
				contact.free = FALSE;
				++contact.serialnumber;
				const auto after = probe();
				return check(after.synchronizeCombatView && (after.movement.buttons & IN_ATTACK),
					"C02-2c stationary zero-approach ClearLook also clears native grenade spot without releasing the pin");
			}
			if (grenadeNoiseFixture && grenadeFrameIndex == 11)
			{
				engine.pfnRandomFloat = &hearingFixtureRandomFloat;
				const auto &noiseProfile = static_cast<metamod::BotNavigationServices &>(runtime).behavior_managedProfiles(0);
				if (!check(loadHearingNav(runtime), "C02 noise control: real NAV supports hearing localization"))
					return false;
				if (grenadeNoiseOrderFixture)
				{
					stepRifle(30, 0, 4);
					stepRifle(30, 0, 4);
					if (!check(static_cast<metamod::BotNavigationServices &>(runtime).behavior_managedBotStateMachines(0).state() ==
							compat::CompatibilityStateId::Hunt, "noise order control: Hunt entry completed before fresh sound"))
						return false;
				}
				struct NoiseDraws final : compat::IRandomTraceSink
				{
					unsigned startles = 0, intervals = 0, points = 0;
					std::function<void()> interrupt;
					void record(const compat::RandomTraceRecord &draw) override
					{
						if (std::strcmp(draw.request.semanticId, "LookAround.StateInterval") == 0) ++intervals;
						if (std::strcmp(draw.request.semanticId, "LookAround.ApproachPoint") == 0) ++points;
						if (std::strcmp(draw.request.semanticId, "NoiseLook.StartleInhibit") == 0)
						{
							++startles;
							if (interrupt) { auto callback = std::move(interrupt); interrupt = {}; callback(); }
						}
					}
				} draws;
				auto &source = gEntities[4];
				source = {};
				source.serialnumber = 44;
				source.v.origin = gEntities[0].v.origin + Vector(0, 300, 0);
				enginefuncs_t soundHooks{};
				int soundVersion = ENGINE_INTERFACE_VERSION;
				if (!check(runtime.provideEngineFunctions(&soundHooks, &soundVersion) && soundHooks.pfnEmitSound &&
						(gLastRunPlayerMoveButtons & IN_ATTACK) && combatCommandPitch < -1.0f,
						"C02 noise control: retained grenade spot and pin precede public sound"))
					return false;
				grenadePorts.service_compatibilityRandomSource().setTraceSink(&draws);
				soundHooks.pfnEmitSound(&source, 1, "weapons/ak47-1.wav", 1, 1, 0, 100);
				stepRifle(30, 0.0f, 4);
				grenadePorts.service_compatibilityRandomSource().setTraceSink(nullptr);
				const auto &heard = grenadePorts.observation_managedBotPerception(0).snapshot().noise();
				std::fprintf(stderr, "C02 noise setup now=%.9f stamp=%.9f age=%.9f reaction=%.9f active=%d localized=%d buttons=%u draws=%u\n",
					globals.time, heard.timestampSeconds, globals.time - heard.timestampSeconds,
					noiseProfile.effectiveReactionSeconds(true), heard.active ? 1 : 0,
					compat::hasNoiseLookPosition(heard) ? 1 : 0, gLastRunPlayerMoveButtons, draws.startles);
				if (!check(compat::hasNoiseLookPosition(heard) && globals.time - heard.timestampSeconds <= .5f &&
						globals.time - heard.timestampSeconds < noiseProfile.effectiveReactionSeconds(true) &&
						heard.priority == world::NoisePriority::High &&
						(gLastRunPlayerMoveButtons & IN_ATTACK),
						"C02 noise control: dangerous localized gunfire precedes ordinary reaction while grenade pin remains held"))
					return false;
				if (!check(draws.startles == 1,
						"C02 rejected dangerous noise look still consumes native startle inhibition draw while grenade holds Primary"))
					return false;
				if (grenadeNoiseOrderFixture)
				{
					std::fprintf(stderr, "noise order draws startle=%u interval=%u point=%u\n", draws.startles, draws.intervals, draws.points);
					if (!check(draws.intervals == 0 && draws.points == 0,
							"handled dangerous noise returns before stationary lookaround interval/point draws even when grenade denies its spot"))
						return false;
				}
				if (grenadeNoiseReentryFixture)
				{
					auto local = makeMovingBehavior();
					runtime::MovementPhysicsState before{};
					before.flags = gEntities[0].v.flags;
					before.origin = {gEntities[0].v.origin.x, gEntities[0].v.origin.y, gEntities[0].v.origin.z};
					const auto nav = grenadePorts.service_navPublisher().snapshot();
					bool restarted = false, innerContinued = true;
					NoiseDraws nested;
					nested.interrupt = [&] {
						auto &machine = const_cast<compat::CompatibilityStateMachine &>(local->managedBotStateMachines(0));
						compat::StateUpdateContext context{};
						context.actor = grenadeActor;
						context.frame = {grenadeLife.mapGeneration(), grenadeLife.roundGeneration(), grenadePorts.service_adapterFrameCount()};
						context.timestamp = globals.time;
						context.fullUpdateSequence = grenadePorts.service_managedBotFullUpdateSequences()[0];
						context.fullUpdate = context.alive = context.roundActive = true;
						context.lifeAvailability = context.roundAvailability = context.attackObservation = compat::ObservationAvailability::Available;
						restarted = machine.beginAttack(context) == compat::StateResult::Transitioned &&
							machine.stopAttack(context) == compat::StateResult::Transitioned;
						metamod::HuntApproachLook inner{};
						innerContinued = local->updateHuntApproachLook(0, before, nav, {}, false, [] { return true; }, inner);
					};
					grenadePorts.service_compatibilityRandomSource().setTraceSink(&nested);
					metamod::HuntApproachLook outer{};
					const bool outerContinued = local->updateHuntApproachLook(0, before, nav, {}, false, [] { return true; }, outer);
					grenadePorts.service_compatibilityRandomSource().setTraceSink(nullptr);
					if (!check(restarted && nested.startles == 1, "noise reentry control: one draw restarts Attack at identical full/time")) return false;
					return check(!innerContinued && !outerContinued,
						"invalidated in-progress same-full noise receipt aborts both nested and outer lookaround");
				}
				runtime::BotCommand refusedNoise{};
				refusedNoise.viewAngles = {-12, 75, 0};
				refusedNoise.movement = {120, 15, 0, IN_ATTACK, 0, 33};
				static_cast<metamod::BotCommandServices &>(runtime).behavior_applyNoiseLookCommand(0, refusedNoise, true);
				return check(refusedNoise.viewAngles.pitch == -12 && refusedNoise.viewAngles.yaw == 75 &&
						refusedNoise.movement.forward == 120 && refusedNoise.movement.side == 15 &&
						refusedNoise.movement.buttons == IN_ATTACK && !refusedNoise.synchronizeCombatView,
					"C02 refused noise never supplies an upkeep view or rewrites the retained pin/movement input");
			}
			if (grenadeAdmissionFixture && grenadeFrameIndex == 11)
			{
				// Three full updates after loss retire the old combat aim, while
				// the native unaligned copied spot and pin are still pending.
				DLL_FUNCTIONS contactHooks{};
				int contactVersion = INTERFACE_VERSION;
				if (!check(runtime.provideEntityApi(&contactHooks, &contactVersion) &&
						contactHooks.pfnKeyValue && contactHooks.pfnTouch &&
						(gLastRunPlayerMoveButtons & IN_ATTACK) != 0 && combatCommandPitch < -1.0f,
						"C02-2 control: public contact hooks and retained grenade look/pin are live"))
					return false;
				auto &contact = gEntities[4];
				contact = {};
				contact.serialnumber = 42;
				contact.v.classname = 20;
				contact.v.takedamage = DAMAGE_YES;
				contact.v.health = 50;
				contact.v.solid = SOLID_BSP;
				contact.v.absmin = gEntities[0].v.origin + Vector(16, 8, -36);
				contact.v.absmax = gEntities[0].v.origin + Vector(40, 32, 44);
				KeyValueData material{};
				material.szClassName = const_cast<char *>("func_breakable");
				material.szKeyName = const_cast<char *>("material");
				material.szValue = const_cast<char *>("1");
				contactHooks.pfnKeyValue(&contact, &material);
				runtime.onMessageBegin(1, 94, nullptr, &gEntities[0]);
				runtime.onWriteByte(1);
				runtime.onWriteByte(4);
				runtime.onWriteByte(1);
				runtime.onMessageEnd();
				grenadeAdmissionEquipRequests = 0;
				gHookedGameDllTable.pfnClientCommand = &grenadeAdmissionClientCommand;
				contactHooks.pfnTouch(&gEntities[0], &contact);
				runtime::BotCommand contactCommand{};
				contactCommand.viewAngles = {-12, 75, 0};
				contactCommand.movement = {120, 15, 0, IN_ATTACK, 0, 33};
				static_cast<metamod::BotCommandServices &>(runtime).behavior_applyObstacleCommand(0, contactCommand);
				gHookedGameDllTable.pfnClientCommand = &combatGameClientCommand;
				if (!check(material.fHandled == 0 && grenadeAdmissionEquipRequests == 1,
						"C02-2 control: qualified breakable contact retains its EquipBest request"))
					return false;
				if (!check(contactCommand.viewAngles.pitch == -12 && contactCommand.viewAngles.yaw == 75 &&
						contactCommand.movement.forward == 120 && contactCommand.movement.side == 15 &&
						contactCommand.movement.buttons == IN_ATTACK && !contactCommand.synchronizeCombatView,
						"C02-2 uninterruptable grenade spot rejects breakable aim and preserves its pin input"))
					return false;
				auto &contactMachine = const_cast<compat::CompatibilityStateMachine &>(
					static_cast<metamod::BotCommandServices &>(runtime).behavior_managedBotStateMachines(0));
				compat::StateUpdateContext contactContext{};
				contactContext.actor = grenadeActor;
				contactContext.frame = {grenadeLife.mapGeneration(), grenadeLife.roundGeneration(), grenadePorts.service_adapterFrameCount()};
				contactContext.timestamp = globals.time;
				contactContext.fullUpdateSequence = grenadePorts.service_managedBotFullUpdateSequences()[0];
				contactContext.fullUpdate = contactContext.alive = contactContext.roundActive = true;
				contactContext.lifeAvailability = contactContext.roundAvailability = compat::ObservationAvailability::Available;
				if (!check(contactMachine.transition(compat::CompatibilityStateId::Hide, compat::StateTransitionReason::ExplicitRequest,
						compat::StateTransitionId::ExplicitStateChange, contactContext) == compat::StateResult::Transitioned &&
						contactMachine.transition(compat::CompatibilityStateId::Idle, compat::StateTransitionReason::ExplicitRequest,
							compat::StateTransitionId::ExplicitStateChange, contactContext) == compat::StateResult::Transitioned,
						"C02-2 control: explicit Clear occurs after a denied contact"))
					return false;
				contactCommand = {};
				contactCommand.viewAngles = {-12, 75, 0};
				contactCommand.movement = {120, 15, 0, IN_ATTACK, 0, 33};
				static_cast<metamod::BotCommandServices &>(runtime).behavior_applyObstacleCommand(0, contactCommand);
				if (!check(contactCommand.viewAngles.pitch == -12 && contactCommand.viewAngles.yaw == 75 &&
						!contactCommand.synchronizeCombatView && contactCommand.movement.buttons == IN_ATTACK,
						"C02-2 denied contact cannot resurrect after grenade Clear"))
					return false;
				contactHooks.pfnTouch(&gEntities[0], &contact);
				static_cast<metamod::BotCommandServices &>(runtime).behavior_applyObstacleCommand(0, contactCommand);
				if (!check(contactCommand.synchronizeCombatView &&
						(contactCommand.viewAngles.pitch != -12 || contactCommand.viewAngles.yaw != 75) &&
						(contactCommand.movement.buttons & IN_ATTACK) != 0,
						"C02-2 a fresh Touch is admitted after Clear while the independent pin remains pending"))
					return false;
				grenadeEnemy.free = false; // A queued recognition may attempt begin while still waiting.
			}
			if (grenadeAdmissionFixture && grenadeFrameIndex == 12)
			{
				runtime::BotCommand retainedContact{};
				retainedContact.viewAngles = {-12, 75, 0};
				retainedContact.movement = {120, 15, 0, IN_ATTACK, 0, 33};
				static_cast<metamod::BotCommandServices &>(runtime).behavior_applyObstacleCommand(0, retainedContact);
				if (!check(retainedContact.synchronizeCombatView &&
						(retainedContact.viewAngles.pitch != -12 || retainedContact.viewAngles.yaw != 75),
						"C02-2 pending throw does not revoke the fresh post-Clear contact"))
					return false;
				gEntities[4].free = TRUE;
				grenadeEnemy.free = true;
			}
			if (grenadeFrameIndex == 6)
			{
				const auto fullBeforeLight = grenadePorts.service_managedBotFullUpdateSequences()[0];
				globals.time += .034f;
				runtime.onStartFrame();
				runtime.onStartFramePost();
				if (!check(grenadePorts.service_managedBotFullUpdateSequences()[0] == fullBeforeLight,
						"C02 light pin control: actual upkeep with no full Update"))
					return false;
				if (!check((gLastRunPlayerMoveButtons & IN_ATTACK) != 0,
						"C02 light upkeep preserves the full Update grenade pin input after recognition"))
					return false;
			}
			if (!check(!static_cast<metamod::BotNavigationServices &>(runtime).behavior_managedBotStateMachines(0).attackOverlayActive(),
					"C02 recognized grenade threat does not enter rifle Attack from stale CurWeapon"))
				return false;
			if ((gLastRunPlayerMoveButtons & IN_ATTACK) == 0)
			{
				if (!check(grenadeFrameIndex > 20, "C02 unaligned toss does not release before its native countdown"))
					return false;
				grenadeReleased = true;
			}
			grenadeTurned = grenadeTurned || (grenadeFrameIndex > 8 && combatCommandPitch < -1.0f);
		}
		forcedNoticeFixtureRoll = -1.0f;
		if (!check(attackAimRefreshDraws == grenadeAimDrawsBefore,
				"C02 grenade recognition does not create a Combat aim-offset clock or RNG draw"))
			return false;
		if (!check(grenadeReleased && grenadeTurned,
				"C02 recognized toss retains its original look and releases after target loss without a ballistic provider"))
			return false;
		runtime.beginRoundGeneration();
		for (unsigned settle = 0; settle < 16; ++settle)
			stepRifle(1, 0.0f, 4);
		if (!check(std::fabs(combatCommandPitch) < 5.0f,
				"C02 clear control: previous native look was reset and shared momentum settled"))
			return false;
		grenadeEnemy.free = false;
		grenadeEnemy.v.origin = gEntities[0].v.origin + Vector(320, 0, 256);
		const auto clearTarget = grenadeEnemy.v.origin;
		forcedNoticeFixtureRoll = 0.0f;
		bool pendingLookObserved = false;
		for (unsigned prime = 0; prime < 24; ++prime)
		{
			stepRifle(1, 0.0f, 4);
			if (combatCommandPitch < -5.0f)
			{
				pendingLookObserved = true;
				break;
			}
		}
		if (!check(pendingLookObserved && (gLastRunPlayerMoveButtons & IN_ATTACK) != 0,
				"C02 clear control: recognized unaligned throw has a live look and pending pin"))
			return false;
		// Let the retained combat aim from recognition retire before testing
		// the grenade spot. A negative command pitch alone is not that proof.
		grenadeEnemy.free = true;
		stepRifle(1, 0.0f, 4);
		stepRifle(1, 0.0f, 4);
		auto &clearMachine = const_cast<compat::CompatibilityStateMachine &>(
			static_cast<metamod::BotNavigationServices &>(runtime).behavior_managedBotStateMachines(0));
		compat::StateUpdateContext clearContext{};
		clearContext.actor = grenadeActor;
		clearContext.frame = {grenadeLife.mapGeneration(), grenadeLife.roundGeneration(), grenadePorts.service_adapterFrameCount()};
		clearContext.timestamp = globals.time;
		clearContext.fullUpdateSequence = grenadePorts.service_managedBotFullUpdateSequences()[0];
		clearContext.fullUpdate = true;
		clearContext.lifeAvailability = clearContext.roundAvailability = compat::ObservationAvailability::Available;
		clearContext.alive = clearContext.roundActive = true;
		if (!check(clearMachine.transition(compat::CompatibilityStateId::Hide, compat::StateTransitionReason::ExplicitRequest,
				compat::StateTransitionId::ExplicitStateChange, clearContext) == compat::StateResult::Transitioned &&
				clearMachine.transition(compat::CompatibilityStateId::Idle, compat::StateTransitionReason::ExplicitRequest,
				compat::StateTransitionId::ExplicitStateChange, clearContext) == compat::StateResult::Transitioned,
				"C02 clear control: Hide exit emits native ClearLookTarget"))
			return false;
		grenadeEnemy.free = true;
		const auto clearEye = gEntities[0].v.origin + gEntities[0].v.view_ofs;
		const auto clearAligned = metamod::ActionAdapter::useEntityView(
			clearTarget.x - clearEye.x, clearTarget.y - clearEye.y, clearTarget.z - clearEye.z);
		gEntities[0].v.v_angle = Vector(clearAligned.pitch, clearAligned.yaw, 0);
		globals.time += .125f;
		runtime.onStartFrame();
		runtime.onStartFramePost();
		if (!check((gLastRunPlayerMoveButtons & IN_ATTACK) != 0,
				"C02 explicit ClearLook cancels alignment release while retaining the pending toss timer"))
			return false;
		bool clearedTimerReleased = false;
		for (unsigned afterClear = 0; afterClear < 30; ++afterClear)
		{
			stepRifle(1, 0.0f, 4);
			clearedTimerReleased = clearedTimerReleased || (gLastRunPlayerMoveButtons & IN_ATTACK) == 0;
		}
		forcedNoticeFixtureRoll = -1.0f;
		if (!check(clearedTimerReleased, "C02 ClearLook preserves the independent native toss deadline"))
			return false;
		if (grenadeAdmissionFixture)
		{
			globals.time += 6.0f; // The prior local equip throttle is definitely elapsed.
			DLL_FUNCTIONS contactHooks{};
			int contactVersion = INTERFACE_VERSION;
			if (!check(runtime.provideEntityApi(&contactHooks, &contactVersion) && contactHooks.pfnKeyValue && contactHooks.pfnTouch,
					"C02-2 reentry control: public contact hooks remain available"))
				return false;
			auto &contact = gEntities[4];
			contact.free = FALSE;
			KeyValueData material{};
			material.szClassName = const_cast<char *>("func_breakable");
			material.szKeyName = const_cast<char *>("material");
			material.szValue = const_cast<char *>("1");
			contactHooks.pfnKeyValue(&contact, &material);
			const auto publishGrenade = [&] {
				runtime.onMessageBegin(1, 94, nullptr, &gEntities[0]);
				runtime.onWriteByte(1); runtime.onWriteByte(4); runtime.onWriteByte(1); runtime.onMessageEnd();
			};
			publishGrenade();
			grenadeAdmissionAllowed.name = const_cast<char *>("bot_allow_rifles");
			grenadeAdmissionAllowed.string = const_cast<char *>("1");
			grenadeAdmissionAllowed.value = 1;
			grenadeAdmissionOriginalPointer = engine.pfnCVarGetPointer;
			grenadeAdmissionOriginalFloat = engine.pfnCVarGetFloat;
			engine.pfnCVarGetPointer = &grenadeAdmissionCvarPointer;
			engine.pfnCVarGetFloat = &grenadeAdmissionCvarFloat;
			grenadeAdmissionEquipRequests = grenadeAdmissionResetCount = grenadeAdmissionFloatReads = 0;
			grenadeAdmissionResetRuntime = &runtime;
			gHookedGameDllTable.pfnClientCommand = &grenadeAdmissionClientCommand;
			contactHooks.pfnTouch(&gEntities[0], &contact);
			runtime::BotCommand interruptedContact{};
			static_cast<metamod::BotCommandServices &>(runtime).behavior_applyObstacleCommand(0, interruptedContact);
			engine.pfnCVarGetPointer = grenadeAdmissionOriginalPointer;
			engine.pfnCVarGetFloat = grenadeAdmissionOriginalFloat;
			gHookedGameDllTable.pfnClientCommand = &combatGameClientCommand;
			if (!check(grenadeAdmissionResetCount == 1 && grenadeAdmissionFloatReads == 0 && grenadeAdmissionEquipRequests == 0,
					"C02-2 reset during weapon allowance stops old lookups and equipment dispatch"))
				return false;
			globals.time += 4.0f; // Past round-freeze, but still inside an incorrectly consumed five-second clock.
			publishGrenade();
			contactHooks.pfnKeyValue(&contact, &material);
			contactHooks.pfnTouch(&gEntities[0], &contact);
			gHookedGameDllTable.pfnClientCommand = &grenadeAdmissionClientCommand;
			runtime::BotCommand freshContact{};
			static_cast<metamod::BotCommandServices &>(runtime).behavior_applyObstacleCommand(0, freshContact);
			gHookedGameDllTable.pfnClientCommand = &combatGameClientCommand;
			if (!check(grenadeAdmissionEquipRequests == 1,
					"C02-2 aborted allowance lookup cannot consume the fresh actor equipment clock"))
				return false;
			globals.time += 6.0f;
			grenadeAdmissionAttackContext = clearContext;
			grenadeAdmissionAttackContext.actor = {
				grenadePorts.service_managedBotHandles()[0].actor.slot,
				grenadePorts.service_managedBotHandles()[0].actor.actorGeneration};
			grenadeAdmissionAttackContext.frame = {grenadeLife.mapGeneration(), grenadeLife.roundGeneration(),
				grenadePorts.service_adapterFrameCount()};
			grenadeAdmissionAttackContext.timestamp = globals.time;
			grenadeAdmissionAttackContext.fullUpdateSequence = grenadePorts.service_managedBotFullUpdateSequences()[0];
			grenadeAdmissionAttackContext.attackObservation = compat::ObservationAvailability::Available;
			const auto normalBeforeAttack = clearMachine.normalStateEpoch();
			const auto attackBefore = clearMachine.attackEntryEpoch();
			grenadeAdmissionAttackMachine = &clearMachine;
			grenadeAdmissionEquipRequests = grenadeAdmissionFloatReads = grenadeAdmissionAttackMutations = 0;
			publishGrenade();
			contactHooks.pfnTouch(&gEntities[0], &contact);
			engine.pfnCVarGetPointer = &grenadeAdmissionCvarPointer;
			engine.pfnCVarGetFloat = &grenadeAdmissionCvarFloat;
			gHookedGameDllTable.pfnClientCommand = &grenadeAdmissionClientCommand;
			runtime::BotCommand attackInterruptedContact{};
			static_cast<metamod::BotCommandServices &>(runtime).behavior_applyObstacleCommand(0, attackInterruptedContact);
			engine.pfnCVarGetPointer = grenadeAdmissionOriginalPointer;
			engine.pfnCVarGetFloat = grenadeAdmissionOriginalFloat;
			gHookedGameDllTable.pfnClientCommand = &combatGameClientCommand;
			grenadeAdmissionAttackMachine = nullptr;
			if (!check(grenadeAdmissionAttackMutations == 1 && !clearMachine.attackOverlayActive() &&
					clearMachine.normalStateEpoch() == normalBeforeAttack && clearMachine.attackEntryEpoch() == attackBefore + 1,
					"C02-2 attack callback control changes only the attack epoch and returns to the same overlay state"))
				return false;
			if (!check(grenadeAdmissionFloatReads == 0 && grenadeAdmissionEquipRequests == 0,
					"C02-2 attack restart during weapon allowance stops stale reads and equipment dispatch"))
				return false;
			contact.free = TRUE;
		}
		return true;
	}
	if (plantEntryFixture)
	{
		using namespace astrabot;
		using namespace astrabot::compat;
		// This first control exercises the documented geometry fallback, not a
		// weapon-only prediction stub inherited from an earlier fixture.
		combatGameDll->pfnUpdateClientData = nullptr;
		globals.mapname = 6; // The public map identity is de_test, a bomb scenario.
		const char *navPath = "astrabot_plant_entry.nav";
		if (!check(writeJumpNavigation(navPath, true), "plant entry NAV written"))
			return false;
		const metamod::NavLoadRequest load{navPath, "astrabot_plant_entry", runtime.snapshot().mapGeneration,
			false, 0U, false, 0U};
		const auto loaded = runtime.loadNavigationFile(&load);
		std::remove(navPath);
		if (!check(loaded == metamod::NavLoadResult::Loaded, "plant entry has native Idle's valid NAV prerequisite"))
			return false;
		gObjectiveEntitiesAvailable = true;
		gPlantedBomb.free = 1;
		gBombSite.free = 0;
		gBombSite.v.classname = 1;
		gBombSite.v.absmin = Vector(0, 0, -4);
		gBombSite.v.absmax = Vector(128, 96, 80);
		gBombSite.v.origin = Vector(64, 48, 38);
		globals.maxEntities = 8;
		gEntities[0].v.weapons = 1 << 6;
		gEntities[0].v.mins = Vector(-16, -16, -36);
		gEntities[0].v.maxs = Vector(16, 16, 36);
		gEntities[0].v.absmin = gEntities[0].v.origin + gEntities[0].v.mins;
		gEntities[0].v.absmax = gEntities[0].v.origin + gEntities[0].v.maxs;
		globals.time += 4.0f; // Hunt's native unset-contact clock is zero; its safe boundary is strictly >3.
		stepRifle(1, 0, 6);
		auto &ports = static_cast<metamod::BotBehaviorServices &>(runtime);
		auto behavior = std::make_unique<metamod::BotBehaviorRuntime>();
		behavior->bindServices(ports);
		const auto &handle = ports.service_managedBotHandles()[0];
		behavior->onActorRemembered(0, handle);
		auto &machine = const_cast<CompatibilityStateMachine &>(behavior->managedBotStateMachines(0));
		StateUpdateContext start{};
		start.actor = {handle.actor.slot, handle.actor.actorGeneration};
		start.frame = {ports.service_lifecycle().mapGeneration(), ports.service_lifecycle().roundGeneration(),
			ports.service_adapterFrameCount()};
		start.timestamp = globals.time - .125f;
		start.fullUpdateSequence = ports.service_managedBotFullUpdateSequences()[0] - 1U;
		start.fullUpdate = start.alive = start.roundActive = true;
		start.lifeAvailability = start.roundAvailability = start.attackObservation = ObservationAvailability::Available;
		machine.initialize(start);
		machine.transition(CompatibilityStateId::Idle, StateTransitionReason::ExplicitRequest,
			StateTransitionId::ExplicitStateChange, start);
		runtime::MovementPhysicsState before{};
		before.grounded = true;
		before.origin = {gEntities[0].v.origin.x, gEntities[0].v.origin.y, gEntities[0].v.origin.z};
		struct EntryTrace final : IStateTraceSink
		{
			unsigned huntEntries = 0, plantEntries = 0, plantUpdates = 0;
			CompatibilityStateMachine *machine = nullptr;
			StateUpdateContext callbackContext{};
			bool exactTargetReentry = false;
			int lifecycleReset = 0;
			StateResult lifecycleResult = StateResult::NoOp;
			struct RejectedGuard final : IStateTransitionExecution { bool current() const override { return false; } } rejected;
			void record(const StateTraceRecord &record) override
			{
				if (record.event == StateTraceEvent::OnEnter && record.callbackState == CompatibilityStateId::Hunt)
					++huntEntries;
				if (record.event == StateTraceEvent::OnEnter && record.callbackState == CompatibilityStateId::PlantBomb)
					++plantEntries;
				if (record.event == StateTraceEvent::OnUpdate && record.callbackState == CompatibilityStateId::PlantBomb)
					++plantUpdates;
				if (exactTargetReentry && record.event == StateTraceEvent::OnUpdate && record.callbackState == CompatibilityStateId::Idle)
				{
					exactTargetReentry = false;
					machine->transition(CompatibilityStateId::PlantBomb, StateTransitionReason::ExplicitRequest,
						StateTransitionId::ExplicitStateChange, callbackContext);
				}
				if (lifecycleReset && record.event == StateTraceEvent::OnUpdate && record.callbackState == CompatibilityStateId::Idle)
				{
					const int kind = lifecycleReset;
					lifecycleReset = 0;
					callbackContext.transitionExecution = &rejected;
					lifecycleResult = kind == 1 ? machine->onRoundReset(callbackContext) : machine->onDeath(callbackContext);
				}
			}
		} entryTrace;
		machine.setTraceSink(&entryTrace);
		const auto idleEpoch = machine.normalStateEpoch();
		behavior->updateManagedBotCompatibilityState(0, before, false);
		if (machine.state() != CompatibilityStateId::PlantBomb)
			std::fprintf(stderr, "plant-entry result=%u site=%d nav=%d\n", static_cast<unsigned>(machine.state()),
				behavior->managedActorAtBombSite(0) ? 1 : 0, ports.service_navPublisher().snapshot().isValid() ? 1 : 0);
		if (!check(machine.state() == CompatibilityStateId::PlantBomb && entryTrace.huntEntries == 0U &&
				machine.normalStateEpoch() == idleEpoch + 1U && behavior->nativePlantEntryStopsMovement(0),
				"native Idle plants directly with one epoch and no Hunt entry, even with cold registry"))
		{
			std::fprintf(stderr, "plant-entry exact state=%u hunt=%u epoch=%llu/%llu hold=%d map=%d/%s roundOver=%d site=%d\n",
				static_cast<unsigned>(machine.state()), entryTrace.huntEntries,
				static_cast<unsigned long long>(machine.normalStateEpoch()), static_cast<unsigned long long>(idleEpoch),
				behavior->nativePlantEntryStopsMovement(0) ? 1 : 0, static_cast<int>(globals.mapname),
				engine.pfnSzFromIndex ? engine.pfnSzFromIndex(globals.mapname) : "unavailable",
				ports.service_roundOver() ? 1 : 0, behavior->managedActorAtBombSite(0) ? 1 : 0);
			return false;
		}
		nav::NavVector target{};
		const auto rngBeforeLateTarget = gSiteFixtureSeed;
		if (!check(!behavior->buildManagedObjectiveTarget(0, &target, nullptr, nullptr) &&
				gSiteFixtureSeed == rngBeforeLateTarget,
				"native Plant entry does not build a late site target or consume its RNG"))
			return false;
		globals.time += .125f;
		++const_cast<std::uint32_t &>(ports.service_managedBotFullUpdateSequences()[0]);
		behavior->updateManagedBotCompatibilityState(0, before, false);
		const auto nextPlantSeed = gSiteFixtureSeed;
		if (!check(machine.state() == CompatibilityStateId::PlantBomb && !behavior->nativePlantEntryStopsMovement(0) &&
				!behavior->buildManagedObjectiveTarget(0, &target, nullptr, nullptr) && gSiteFixtureSeed == nextPlantSeed &&
				!ports.navigation_hasActivePath(0),
				"the next ordinary Plant full update also creates no site target, path or selection RNG"))
			return false;
		const auto runEntry = [&](CompatibilityStateId initial, int api, int icon, bool warm,
			CompatibilityStateId expected, bool replace = false, bool exactTarget = false, int reset = 0, bool buy = false) {
			plantZonePrediction = api;
			plantZoneReplacesEntity = replace;
			combatGameDll->pfnUpdateClientData = nullptr;
			if (api >= 0)
				combatGameDll->pfnUpdateClientData = [](const edict_t *, int, clientdata_t *data) {
					data->m_iId = 6;
					data->iuser3 = plantZonePrediction ? 1 << 2 : 0;
					if (plantZoneReplacesEntity) ++gEntities[0].serialnumber;
				};
			auto &publicStore = const_cast<PublicObservationStore &>(ports.observation_publicObservations());
			publicStore.resetHud(start.actor, start.frame);
			if (icon >= 0) publicStore.statusIcon(start.actor, start.frame, "c4", icon);
			auto next = std::make_unique<metamod::BotBehaviorRuntime>();
			next->bindServices(ports);
			next->onActorRemembered(0, handle);
			auto &stateMachine = const_cast<CompatibilityStateMachine &>(next->managedBotStateMachines(0));
			stateMachine.initialize(start);
			stateMachine.transition(initial, StateTransitionReason::ExplicitRequest,
				StateTransitionId::ExplicitStateChange, start);
			EntryTrace trace;
			trace.machine = &stateMachine;
			trace.callbackContext = start;
			trace.callbackContext.timestamp = globals.time;
			trace.callbackContext.fullUpdateSequence = ports.service_managedBotFullUpdateSequences()[0];
			trace.exactTargetReentry = exactTarget;
			trace.lifecycleReset = reset;
			stateMachine.setTraceSink(&trace);
			if (warm && !check(next->managedActorAtBombSite(0), "warm geometry registry sees this real site"))
				return false;
			const auto epoch = stateMachine.normalStateEpoch();
			const auto serial = gEntities[0].serialnumber;
			const auto getCvarPointer = engine.pfnCVarGetPointer;
			const auto getCvarValue = engine.pfnCVarGetFloat;
			if (buy)
			{
				publicStore.statusIcon(start.actor, start.frame, "buyzone", 1);
				engine.pfnCVarGetPointer = [](const char *name) -> cvar_t * {
					static cvar_t buyTime{};
					return std::strcmp(name, "mp_buytime") == 0 ? &buyTime : nullptr;
				};
				engine.pfnCVarGetFloat = [](const char *name) { return std::strcmp(name, "mp_buytime") == 0 ? 1.0f : 0.0f; };
			}
			metamod::BotBehaviorRuntime::PlantMoveChatter outgoing{};
			next->updateManagedBotCompatibilityState(0, before, false, &outgoing);
			engine.pfnCVarGetPointer = getCvarPointer;
			engine.pfnCVarGetFloat = getCvarValue;
			plantZoneReplacesEntity = false;
			gEntities[0].serialnumber = serial;
			if (buy)
				return check(stateMachine.state() == CompatibilityStateId::Buy && stateMachine.normalStateEpoch() == epoch + 1U &&
					stateMachine.stateTimestamp() == globals.time && !next->nativePlantEntryStopsMovement(0) &&
					outgoing.epoch == 0U && !outgoing.coverDue,
					"higher-priority Buy completes its timestamp and cancels Plant admission, hold and chatter");
			if (replace)
				return check(stateMachine.state() == initial && stateMachine.normalStateEpoch() == epoch &&
					!next->nativePlantEntryStopsMovement(0) && outgoing.epoch == 0U && !outgoing.coverDue,
					"zone callback replacement cannot authorize the old Plant decision or chatter");
			if (exactTarget)
				return check(stateMachine.state() == CompatibilityStateId::PlantBomb && stateMachine.normalStateEpoch() == epoch + 1U &&
					trace.plantEntries == 1U && trace.plantUpdates == 0U && !next->nativePlantEntryStopsMovement(0) &&
					outgoing.epoch == 0U && !outgoing.coverDue,
					"exact-target OnUpdate reentry stops the old update and cannot authorize its Plant receipt");
			if (reset)
				return check(trace.lifecycleResult == StateResult::Transitioned && stateMachine.state() == CompatibilityStateId::Idle &&
					stateMachine.normalStateEpoch() == epoch + 1U && trace.plantEntries == 0U && !next->nativePlantEntryStopsMovement(0) &&
					outgoing.epoch == 0U && !outgoing.coverDue,
					"forced lifecycle reset discards the old guard and completes while the old Plant entry is rejected");
			if (!check(stateMachine.state() == expected, "public zone and state-specific Plant entry match the native branch"))
			{
				std::fprintf(stderr, "entry-case initial=%u api=%d icon=%d warm=%d expected=%u actual=%u epoch=%llu/%llu\n",
					static_cast<unsigned>(initial), api, icon, warm ? 1 : 0, static_cast<unsigned>(expected),
					static_cast<unsigned>(stateMachine.state()), static_cast<unsigned long long>(stateMachine.normalStateEpoch()),
					static_cast<unsigned long long>(epoch));
				return false;
			}
			if (expected == CompatibilityStateId::PlantBomb ||
				((initial == CompatibilityStateId::Hunt || initial == CompatibilityStateId::Follow) && expected == CompatibilityStateId::Idle))
			{
				if (!check(stateMachine.normalStateEpoch() == epoch + 1U && trace.huntEntries == 0U &&
					next->nativePlantEntryStopsMovement(0), "native entry/yield has exactly one transition and stops that full update"))
					return false;
				const auto seed = gSiteFixtureSeed;
				if (!check(!next->buildManagedObjectiveTarget(0, &target, nullptr, nullptr) && gSiteFixtureSeed == seed,
					"yielded entry cannot perform late target selection"))
					return false;
			}
			return true;
		};
		const bool entries = runEntry(CompatibilityStateId::Idle, -1, 1, false, CompatibilityStateId::PlantBomb) &&
			runEntry(CompatibilityStateId::Idle, -1, -1, true, CompatibilityStateId::PlantBomb) &&
			runEntry(CompatibilityStateId::Idle, 1, 1, false, CompatibilityStateId::PlantBomb) &&
			runEntry(CompatibilityStateId::Idle, 0, 2, false, CompatibilityStateId::Hunt) &&
			runEntry(CompatibilityStateId::Hunt, 1, 1, false, CompatibilityStateId::Idle) &&
			runEntry(CompatibilityStateId::Follow, 1, 1, false, CompatibilityStateId::Idle) &&
			runEntry(CompatibilityStateId::Idle, 1, 1, false, CompatibilityStateId::Idle, true) &&
			runEntry(CompatibilityStateId::Idle, 1, 1, false, CompatibilityStateId::PlantBomb, false, true) &&
			runEntry(CompatibilityStateId::Idle, 1, 1, false, CompatibilityStateId::Idle, false, false, 1) &&
			runEntry(CompatibilityStateId::Idle, 1, 1, false, CompatibilityStateId::Idle, false, false, 2) &&
			runEntry(CompatibilityStateId::Idle, 1, 1, false, CompatibilityStateId::Buy, false, false, 0, true);
		if (!entries) return false;
		auto &enemy = gEntities[1];
		enemy.free = false;
		enemy.v.flags = FL_CLIENT | FL_ONGROUND;
		enemy.v.health = 100;
		enemy.v.deadflag = DEAD_NO;
		enemy.v.team = 2;
		enemy.v.origin = gEntities[0].v.origin + Vector(800, 0, 0);
		enemy.v.velocity = Vector(250, 0, 0);
		enemy.v.v_angle = Vector(0, 0, 0); // Far and looking away: no objective self-defense Attack.
		runtime.onClientPutInServer(&enemy);
		runtime.onMessageBegin(MSG_ALL, 86, nullptr, nullptr);
		runtime.onWriteByte(2);
		runtime.onWriteString("CT");
		runtime.onMessageEnd();
		combatGameDll->pfnUpdateClientData = nullptr;
		const auto contactControl = [&](bool recognized) {
			stepRifle(1, 0, 6);
			auto next = std::make_unique<metamod::BotBehaviorRuntime>();
			next->bindServices(ports);
			next->onActorRemembered(0, handle);
			auto profile = static_cast<metamod::BotNavigationServices &>(runtime).behavior_managedProfiles(0);
			// Stay below the native ring's wraparound clamp; .5s needs four samples.
			profile.csbotReactionSeconds = recognized ? .1f : .5f;
			next->assignProfile(0, profile);
			auto &state = const_cast<CompatibilityStateMachine &>(next->managedBotStateMachines(0));
			auto current = start;
			current.frame.tick = ports.service_adapterFrameCount();
			current.timestamp = globals.time - .125f;
			current.fullUpdateSequence = ports.service_managedBotFullUpdateSequences()[0] - 1U;
			state.initialize(current);
			state.transition(CompatibilityStateId::PlantBomb, StateTransitionReason::ExplicitRequest,
				StateTransitionId::ExplicitStateChange, current);
			next->updateManagedBotCompatibilityState(0, before, false);
			stepRifle(1, 0, 6);
			next->updateManagedBotCompatibilityState(0, before, false);
			world::WorldPosition point{};
			const bool aiming = next->bombObjectiveEnemyAim(0, {}, &point);
			if (aiming != recognized || state.attackOverlayActive())
				std::fprintf(stderr, "contact-control recognized=%d aiming=%d attack=%d state=%u time=%.3f\n",
					recognized, aiming, state.attackOverlayActive(), static_cast<unsigned>(state.state()), globals.time);
			if (!check(aiming == recognized && !state.attackOverlayActive(),
				"contact control distinguishes a delayed candidate from a real recognized enemy")) return false;
			stepRifle(1, 0, 6);
			current.frame.tick = ports.service_adapterFrameCount();
			current.timestamp = globals.time - .125f;
			current.fullUpdateSequence = ports.service_managedBotFullUpdateSequences()[0] - 1U;
			state.transition(CompatibilityStateId::Hunt, StateTransitionReason::ExplicitRequest,
				StateTransitionId::ExplicitStateChange, current);
			next->updateManagedBotCompatibilityState(0, before, false);
			if (state.state() != (recognized ? CompatibilityStateId::Hunt : CompatibilityStateId::Idle))
				std::fprintf(stderr, "contact-yield recognized=%d state=%u hold=%d now=%.3f\n", recognized,
					static_cast<unsigned>(state.state()), next->nativePlantEntryStopsMovement(0), globals.time);
			return check(state.state() == (recognized ? CompatibilityStateId::Hunt : CompatibilityStateId::Idle),
				"only real enemy contact postpones Hunt's safe-at-site yield; a fresh candidate does not");
		};
		if (!contactControl(false) || !contactControl(true)) return false;
		enemy.v.flags |= FL_NOTARGET;
		const auto timeoutEntryControl = [&](bool invalidateEntry) {
			const bool savedProfile = gProfileEnabled;
			gProfileEnabled = true;
			const auto savedBody = gEntities[0].v;
			const auto savedSite = gBombSite;
			gEntities[0].v.origin = Vector(112, 32, 36);
			gEntities[0].v.velocity = Vector(0, 0, 0);
			gEntities[0].v.absmin = gEntities[0].v.origin + gEntities[0].v.mins;
			gEntities[0].v.absmax = gEntities[0].v.origin + gEntities[0].v.maxs;
			gEntities[0].v.weapons = (1 << 6) | (1 << 28);
			gBombSite.v.origin = Vector(224, 32, 0);
			gBombSite.v.mins = Vector(-8, -8, -4);
			gBombSite.v.maxs = Vector(8, 8, 4);
			gBombSite.v.absmin = Vector(216, 24, -4);
			gBombSite.v.absmax = Vector(232, 40, 4);
			const char *path = "astrabot_plant_timeout.nav";
			if (!check(writeTwoSiteNavigation(path, true), "Plant timeout has a real four-area site route")) return false;
			const metamod::NavLoadRequest request{path, "astrabot_plant_timeout", runtime.snapshot().mapGeneration,
				false, 0U, false, 0U};
			const auto loaded = runtime.loadNavigationFile(&request);
			std::remove(path);
			if (!check(loaded == metamod::NavLoadResult::Loaded, "Plant timeout NAV is loaded")) return false;
			plantZonePrediction = 0;
			combatGameDll->pfnUpdateClientData = [](const edict_t *, int, clientdata_t *data) {
				data->m_iId = 28;
				data->iuser3 = plantZonePrediction ? 1 << 2 : 0;
			};
			globals.time += 31;
			auto &liveState = const_cast<CompatibilityStateMachine &>(
				static_cast<metamod::BotCommandServices &>(runtime).behavior_managedBotStateMachines(0));
			auto context = start;
			context.frame.tick = ports.service_adapterFrameCount();
			context.timestamp = globals.time;
			context.fullUpdateSequence = ports.service_managedBotFullUpdateSequences()[0];
			context.moveToTask = CompatibilityTaskId::PlantBomb;
			(void)liveState.stopAttack(context);
			(void)liveState.transition(CompatibilityStateId::Idle, StateTransitionReason::ExplicitRequest,
				StateTransitionId::ExplicitStateChange, context);
			(void)liveState.transition(CompatibilityStateId::MoveTo, StateTransitionReason::ExplicitRequest,
				StateTransitionId::ExplicitStateChange, context);
			for (auto kind : {metamod::NavigationRequestKind::ResetMovement, metamod::NavigationRequestKind::ClearStep})
				ports.applyNavigationRequest({0, kind});
			struct EntryObservationTrace final : IObservationTraceSink
			{
				CompatibilityStateMachine *machine = nullptr;
				metamod::BotBehaviorServices *ports = nullptr;
				StateUpdateContext context{};
				bool enabled = false, fired = false;
				void record(const ObservationTraceRecord &) override
				{
					if (!enabled || fired || machine->state() != CompatibilityStateId::PlantBomb) return;
					fired = true;
					context.timestamp = ports->service_globals()->time;
					context.frame.tick = ports->service_adapterFrameCount();
					context.fullUpdateSequence = ports->service_managedBotFullUpdateSequences()[0];
					machine->transition(CompatibilityStateId::Hunt, StateTransitionReason::ExplicitRequest,
						StateTransitionId::ExplicitStateChange, context);
				}
			} actionTrace;
			actionTrace.machine = &liveState;
			actionTrace.ports = &ports;
			actionTrace.context = context;
			actionTrace.enabled = invalidateEntry;
			auto &observation = const_cast<metamod::ObservationAdapter &>(ports.observation_observationAdapter());
			observation.setTraceSink(&actionTrace);
			const float began = globals.time;
			bool sawProgress = false, entered = false, timedFailure = false, keptInput = false, nextStopped = false;
			bool invalidatedInputDropped = false;
			for (unsigned tick = 0; tick < 80; ++tick)
			{
				// Local motion prevents the velocity-stuck monitor from ending the path;
				// remaining within area2 leaves the independent corridor deadline running.
				gEntities[0].v.origin.x = 112.0f + static_cast<float>(tick % 2U) * 10.0f;
				gEntities[0].v.absmin = gEntities[0].v.origin + gEntities[0].v.mins;
				gEntities[0].v.absmax = gEntities[0].v.origin + gEntities[0].v.maxs;
				plantZonePrediction = globals.time - began >= 4.5f;
				const auto previous = liveState.state();
				const auto selectedBefore = combatC4Selections;
				const auto sentBefore = combatSentInputs.size();
				stepRifle(1, 30, 28);
				sawProgress |= liveState.state() == CompatibilityStateId::MoveTo;
				if (actionTrace.fired)
				{
					invalidatedInputDropped = liveState.state() == CompatibilityStateId::Hunt &&
						combatSentInputs.size() == sentBefore;
					break;
				}
				if (previous == CompatibilityStateId::MoveTo && liveState.state() == CompatibilityStateId::PlantBomb)
				{
					entered = true;
					const auto &steps = static_cast<metamod::BotCommandServices &>(runtime).navigation_managedNavigationSteps(0);
					const auto prefix = std::string("profile navigationStep bot_id=1 frame=") +
						std::to_string(ports.service_adapterFrameCount()) + " ";
					const auto reason = std::string(" failure_reason=") +
						std::to_string(static_cast<unsigned>(runtime::NavFailureReason::MovementNotProduced)) + " ";
					timedFailure = steps.outcome == NavigationStepOutcome::PathFailure && globals.time - began > 5 &&
						std::any_of(gMovementLogs.begin(), gMovementLogs.end(), [&](const std::string &log) {
							return log.find(prefix) == 0 && log.find(reason) != std::string::npos;
						});
					keptInput =
						combatSentInputs.size() == sentBefore + 1 &&
						std::hypot(combatCommandForward, combatCommandSide) > 10 &&
						(combatSentInputs.back().second & (IN_DUCK | IN_ATTACK)) == IN_DUCK &&
						combatC4Selections == selectedBefore;
					stepRifle(1, 30, 28);
					nextStopped = std::hypot(combatCommandForward, combatCommandSide) == 0 &&
						(combatSentInputs.back().second & (IN_DUCK | IN_JUMP)) == IN_DUCK &&
						combatC4Selections == selectedBefore + 1;
					break;
				}
			}
			observation.setTraceSink(nullptr);
			combatGameDll->pfnUpdateClientData = nullptr;
			gEntities[0].v = savedBody;
			gBombSite = savedSite;
			gProfileEnabled = savedProfile;
			if (invalidateEntry)
				return check(sawProgress && actionTrace.fired, "the Plant action observation callback changes the same actor's normal state") &&
					check(invalidatedInputDropped, "a callback-invalidated Plant entry does not dispatch its stale NAV command");
			return check(sawProgress && entered, "the full runtime reaches Plant only after a stationary MoveTo path times out") &&
				check(timedFailure, "the entry follows a real path failure after the strict five-second timeout") &&
				check(keptInput, "full-frame timeout Plant entry dispatches its produced NAV input plus crouch before selecting C4") &&
				check(nextStopped, "the next full runtime Plant update stops and selects C4 exactly once");
		};
		if (!timeoutEntryControl(false)) return false;
		const auto moveControl = [&](int mutation) {
		stepRifle(1, 0, 6);
		auto moving = std::make_unique<metamod::BotBehaviorRuntime>();
		moving->bindServices(ports);
		moving->onActorRemembered(0, handle);
		auto &moveState = const_cast<CompatibilityStateMachine &>(moving->managedBotStateMachines(0));
		auto moveContext = start;
		moveContext.frame.tick = ports.service_adapterFrameCount();
		moveContext.timestamp = globals.time - .125f;
		moveContext.fullUpdateSequence = ports.service_managedBotFullUpdateSequences()[0] - 1U;
		moveContext.moveToTask = CompatibilityTaskId::PlantBomb;
		moveState.initialize(moveContext);
		moveState.transition(CompatibilityStateId::MoveTo, StateTransitionReason::ExplicitRequest,
			StateTransitionId::ExplicitStateChange, moveContext);
		EntryTrace moveTrace;
		moveState.setTraceSink(&moveTrace);
		if (!check(moving->buildManagedObjectiveTarget(0, &target, nullptr, nullptr),
			"MoveTo control has a selected plant target and completed approach phase")) return false;
		moving->updateManagedBotCompatibilityState(0, before, false);
		if (!check(moveState.state() == CompatibilityStateId::MoveTo,
			"being inside a plant zone with a finished phase cannot preempt the MoveTo navigation step")) return false;
		runtime::NavRoamDecision path{};
		path.pathSequence = 1;
		path.goalPresent = true;
		nav::LocomotionIntent intent{};
		intent.speed = 240;
		if (!moving->finishManagedBotNavigation(0, before, runtime::NavRoamResult::IntentReady, path, &intent, {}) ||
			!check(moveState.state() == CompatibilityStateId::MoveTo && intent.speed == 240,
				"a progressing path retains MoveTo and its movement inside the bomb zone")) return false;
		stepRifle(1, 0, 6);
		globals.time += .01f; // Reserve command time after the runtime fixture's own dispatch.
		moving->updateManagedBotCompatibilityState(0, before, false);
		const auto serial = gEntities[0].serialnumber;
		const auto weapons = gEntities[0].v.weapons;
		plantZonePrediction = mutation;
		if (mutation)
			combatGameDll->pfnUpdateClientData = [](const edict_t *, int, clientdata_t *data) {
				data->m_iId = 6;
				data->iuser3 = 1 << 2;
				if (plantZonePrediction == 1) ++gEntities[0].serialnumber;
				else gEntities[0].v.weapons &= ~(1 << 6);
			};
		path.failureReason = runtime::NavFailureReason::MovementNotProduced;
		const bool accepted = moving->finishManagedBotNavigation(0, before, runtime::NavRoamResult::ReplanRequired, path, &intent, {});
		combatGameDll->pfnUpdateClientData = nullptr;
		gEntities[0].serialnumber = serial;
		gEntities[0].v.weapons = weapons;
		if (mutation == 1)
			return check(!accepted && moveState.state() == CompatibilityStateId::MoveTo && intent.speed == 240,
				"post-path zone callback replacement rejects the old completion instead of treating it as outside-zone Idle");
		if (mutation == 2)
			return check(accepted && moveState.state() == CompatibilityStateId::Idle,
				"post-path possession is read after the zone callback can drop C4 on the same actor");
		if (!accepted) return false;
		const auto plantEntryAction = moving->decideManagedBotAction(0, before, {}, IN_FORWARD | IN_JUMP);
		const auto yaw = gEntities[0].v.v_angle.y * 0.017453292519943295f;
		if (!check(plantEntryAction.kind == metamod::ActionKind::None && moveTrace.plantEntries == 1U &&
			moveTrace.plantUpdates == 0U && plantEntryAction.hasUseEntityLook &&
			std::fabs(plantEntryAction.useEntityLookTarget.x - (before.origin.x + 10 * std::cos(yaw))) < .0001f &&
			std::fabs(plantEntryAction.useEntityLookTarget.y - (before.origin.y + 10 * std::sin(yaw))) < .0001f &&
			plantEntryAction.useEntityLookTarget.z == before.origin.z - 18,
			"navigation-owned Plant entry constructs the crouched floor look before the next upkeep")) return false;
		auto commands = std::make_unique<metamod::BotCommandRuntime>();
		auto &commandPorts = static_cast<metamod::BotCommandServices &>(runtime);
		commands->bindServices(commandPorts);
		const_cast<std::uint32_t &>(commands->managedBotCommandSequences(0)) = ports.command_managedBotCommandSequences(0);
		auto commandHandle = handle;
		const auto token = ports.service_lifecycle().tokenForSlot(handle.actor.slot);
		metamod::NavigationUpdate movement{};
		movement.locomotionIntent = intent;
		movement.locomotionIntent.direction = {1, 0, 0};
		movement.locomotionIntent.traversal = nav::TraversalAction::Jump;
		movement.observation.plantApproach = true;
		movement.roamResult = runtime::NavRoamResult::ReplanRequired;
		movement.roamDecision = path;
		movement.roamDecision.currentArea = movement.roamDecision.goalArea = 1;
		const auto &nearGoal = commandPorts.behavior_managedBotObjectiveTargets(0);
		const float goalDx = nearGoal.target.x - before.origin.x, goalDy = nearGoal.target.y - before.origin.y;
		if (!check(nearGoal.valid && goalDx * goalDx + goalDy * goalDy < 200.0f * 200.0f,
			"the final-area command control has a valid nearby goal which normally reduces walking speed")) return false;
		auto draft = commands->beginMovementCommand(0, commandHandle, token, before, movement);
		const auto count = combatSentInputs.size();
		commands->finishMovementCommand(0, commandHandle, token, before, movement, draft, plantEntryAction);
		if (!check(combatSentInputs.size() == count + 1U,
			"the terminal-entry command fixture actually dispatches one RunPlayerMove")) return false;
		if (!check((gLastRunPlayerMoveButtons & (IN_JUMP | IN_DUCK)) == (IN_JUMP | IN_DUCK) &&
			(gLastRunPlayerMoveButtons & IN_ATTACK) == 0 &&
			std::hypot(gLastRunPlayerMoveForward, gLastRunPlayerMoveSide) > 10,
			"MoveTo terminal Plant entry adds crouch while its existing NAV movement and jump reach actual RunPlayerMove"))
			return false;
		if (!check(std::fabs(std::hypot(gLastRunPlayerMoveForward, gLastRunPlayerMoveSide) - commandHandle.entity->v.maxspeed) < .01f,
			"Plant entry after the path deadline retains native Run even in the old goal's final area")) return false;
		if (!check(moveState.state() == CompatibilityStateId::PlantBomb && moveState.stateTimestamp() == globals.time &&
			intent.speed == 240,
			"the evaluated terminal path enters Plant with a completed timestamp without erasing its existing navigation intent"))
			return false;
		stepRifle(1, 0, 6);
		moving->updateManagedBotCompatibilityState(0, before, false);
		const auto plantUpdateAction = moving->decideManagedBotAction(0, before, {}, IN_FORWARD | IN_JUMP);
		return check(plantUpdateAction.stopForWeapon && !plantUpdateAction.runAfterPlantTimeout &&
			(plantUpdateAction.movementButtons & (IN_FORWARD | IN_JUMP)) == 0 &&
			(plantUpdateAction.movementButtons & IN_DUCK) != 0 && moveTrace.plantUpdates == 1,
			"the following Plant full update consumes the NAV-entry receipt and stops forward/jump while crouching");
		};
		const auto idleAfterBuyControl = [&] {
			auto &commandPorts = static_cast<metamod::BotCommandServices &>(runtime);
			auto &liveState = const_cast<CompatibilityStateMachine &>(commandPorts.behavior_managedBotStateMachines(0));
			auto context = start;
			context.timestamp = globals.time;
			context.frame.tick = ports.service_adapterFrameCount();
			context.fullUpdateSequence = ports.service_managedBotFullUpdateSequences()[0];
			(void)liveState.stopAttack(context);
			(void)liveState.transition(CompatibilityStateId::Buy, StateTransitionReason::ExplicitRequest,
				StateTransitionId::ExplicitStateChange, context);
			auto &goal = const_cast<metamod::ManagedObjectiveTargetCache &>(commandPorts.behavior_managedBotObjectiveTargets(0));
			goal = {};
			gEntities[0].v.weapons = (1 << 6) | (1 << 28);
			plantZonePrediction = 1;
			combatGameDll->pfnUpdateClientData = [](const edict_t *, int, clientdata_t *data) {
				data->m_iId = 28;
				data->iuser3 = 1 << 2;
			};
			for (auto kind : {metamod::NavigationRequestKind::ResetMovement, metamod::NavigationRequestKind::ClearStep})
				ports.applyNavigationRequest({0, kind});
			const auto sentBefore = combatSentInputs.size();
			stepRifle(1, 30, 28);
			const auto &step = commandPorts.navigation_managedNavigationSteps(0);
			const bool idle = liveState.state() == CompatibilityStateId::Idle;
			const bool noPath = step.pathSequence == 0U && !ports.navigation_hasActivePath(0);
			const bool noMovement = combatSentInputs.size() == sentBefore + 1 &&
				std::hypot(combatCommandForward, combatCommandSide) == 0;
			const bool noTarget = !goal.valid && goal.generation == 0U;
			stepRifle(1, 30, 28);
			const bool nextPlants = liveState.state() == CompatibilityStateId::PlantBomb;
			combatGameDll->pfnUpdateClientData = nullptr;
			return check(idle, "completed Buy enters Idle before the carrier's next normal update") &&
				check(nextPlants, "the following Idle update admits Plant directly through the public bomb-zone observation") &&
				check(noPath && noMovement, "Idle after Buy does not prepare a path or dispatch navigation movement before Plant admission") &&
				check(noTarget, "Idle's neutral action does not create an unused legacy Plant target after NAV is held");
		};
		return moveControl(2) && moveControl(1) && moveControl(0) && timeoutEntryControl(true) && idleAfterBuyControl();
	}
	if (clearLookFixture || sharedLookFixture)
	{
		using namespace astrabot;
		using namespace astrabot::compat;
		auto &ports = static_cast<metamod::BotCommandServices &>(runtime);
		auto &behavior = static_cast<metamod::BotBehaviorServices &>(runtime);
		stepRifle(30);
		auto handle = ports.service_managedBotHandles()[0];
		auto command = std::make_unique<metamod::BotCommandRuntime>();
		command->bindServices(ports);
		const_cast<std::uint32_t &>(command->managedBotCommandSequences(0)) = behavior.command_managedBotCommandSequences(0);
		auto &machine = const_cast<CompatibilityStateMachine &>(ports.behavior_managedBotStateMachines(0));
		const auto state = [&](CompatibilityStateId next) {
			StateUpdateContext tick{};
			tick.actor = {handle.actor.slot, handle.actor.actorGeneration};
			tick.frame = {ports.service_lifecycle().mapGeneration(), ports.service_lifecycle().roundGeneration(), ports.service_adapterFrameCount()};
			tick.timestamp = globals.time;
			tick.fullUpdate = tick.alive = tick.roundActive = true;
			tick.lifeAvailability = tick.roundAvailability = tick.attackObservation = ObservationAvailability::Available;
			machine.transition(next, StateTransitionReason::ExplicitRequest, StateTransitionId::ExplicitStateChange, tick);
		};
		runtime::MovementPhysicsState before{};
		before.grounded = true;
		before.origin = {gEntities[0].v.origin.x, gEntities[0].v.origin.y, gEntities[0].v.origin.z};
		auto token = ports.service_lifecycle().tokenForSlot(handle.actor.slot);
		runtime::ViewAngles expected{30, 45, 0};
		AimSpringState spring{};
		nav::NavVector aheadDirection{1, 0, 0};
		gEntities[0].v.v_angle = Vector(expected.pitch, expected.yaw, 0);
		const auto send = [&](bool priority, bool terminal, bool guard = false, bool aim = false) {
			globals.time += .05f;
			const auto calls = combatSentInputs.size();
			metamod::NavigationUpdate movement{};
			movement.roamResult = runtime::NavRoamResult::IntentReady;
			movement.locomotionIntent.speed = priority || guard || aim ? 0.0f : 221.0f;
			movement.locomotionIntent.direction = terminal ? nav::NavVector{-1, 0, 0} : aheadDirection;
			auto draft = command->beginMovementCommand(0, handle, token, before, movement);
			metamod::ActionProposal action{metamod::ActionKind::None, expected, 0};
			action.stopForWeapon = true;
			action.hasUseEntityLook = priority;
			action.hasGuardLook = guard;
			action.hasCombatAim = aim;
			const auto eye = handle.entity->v.origin + handle.entity->v.view_ofs;
			action.useEntityLookTarget = {eye.x, eye.y + 200.0f, eye.z + 100.0f};
			action.preserveUpkeepView = terminal;
			command->finishMovementCommand(0, handle, token, before, movement, draft, action);
			return check(combatSentInputs.size() == calls + 1U && std::fabs(combatCommandYaw - expected.yaw) < .0001f &&
				std::fabs(combatCommandPitch - expected.pitch) < .0001f,
				"a new priority or terminal full retains the already-applied upkeep angle");
		};
		const auto light = [&](float yaw, float pitch, const char *label, float drift = 2.0f) {
			globals.time += .05f;
			const auto goalYaw = yaw + drift * metamod::ActionAdapter::csBotCosine(33.0f * globals.time);
			const auto goalPitch = pitch + drift * metamod::ActionAdapter::csBotCosine(13.0f * globals.time - 90.0f);
			const auto reference = trackAim(expected.yaw, expected.pitch, goalYaw, goalPitch,
				runtime::BotTimingScheduler::kCommandInterval, 1.0f, &spring, false);
			expected = {reference.pitch, reference.yaw, 0};
			const auto calls = combatSentInputs.size();
			command->advanceUpkeepLook(0, handle);
			const auto receipt = command->executeManagedBotCommand(0, handle);
			return check(receipt.result == runtime::DispatchResult::Dispatched && combatSentInputs.size() == calls + 1U &&
				std::fabs(combatCommandYaw - expected.yaw) < .0001f &&
				std::fabs(combatCommandPitch - expected.pitch) < .0001f, label);
		};
		if (sharedLookFixture)
		{
			state(CompatibilityStateId::Hunt);
			if (!send(false, false) || !light(0, 0, "NAV supplies momentum before Guard"))
				return false;
			state(CompatibilityStateId::Hide);
			globals.time += .05f;
			auto draft = command->beginNeutralCommand(0, handle, before);
			metamod::ActionProposal guard{metamod::ActionKind::None, {0, 90, 0}, 0};
			guard.hasGuardLook = true;
			command->finishNeutralCommand(0, handle, before, draft, guard);
			command->executeManagedBotCommand(0, handle);
			if (!check(std::fabs(combatCommandYaw - expected.yaw) < .0001f,
				"Guard publishes a goal without replacing the preceding Upkeep view"))
				return false;
			if (!light(90, 0, "Guard integrates its goal with the NAV yaw/pitch velocities"))
				return false;
			const auto yaw = combatCommandYaw, pitch = combatCommandPitch;
			command->advanceUpkeepLook(0, handle);
			if (!check(handle.entity->v.v_angle.y == yaw && handle.entity->v.v_angle.x == pitch,
				"Guard cannot integrate twice at one command timestamp"))
				return false;
			combat::CombatController combatOwner;
			combat::CombatDecisionContext aimContext{};
			aimContext.actor = {handle.actor.slot, handle.actor.actorGeneration};
			aimContext.frame = {ports.service_lifecycle().mapGeneration(), ports.service_lifecycle().roundGeneration(),
				ports.service_adapterFrameCount()};
			aimContext.origin = {before.origin.x, before.origin.y, before.origin.z};
			aimContext.skill = 1.0f;
			aimContext.elapsedSeconds = .09f; // A valid multi-step legacy interval; shared native integration is exactly 1/30.
			aimContext.sharedLookStep = [&](float time, float yaw, float pitch, float goalYaw, float goalPitch, bool attack, unsigned priority) {
				return command->stepSharedLook(0, time, yaw, pitch, goalYaw, goalPitch, attack, priority);
			};
			const auto combatStep = [&](float goalYaw, float goalPitch, bool attacking, bool ladder) {
				globals.time += .05f;
				aimContext.timestamp = globals.time;
				aimContext.currentYaw = expected.yaw;
				aimContext.currentPitch = expected.pitch;
				aimContext.lookOverride = ladder;
				const auto reference = trackAim(expected.yaw, expected.pitch, goalYaw, goalPitch,
					runtime::BotTimingScheduler::kCommandInterval, 1, &spring, attacking);
				expected = {reference.pitch, reference.yaw, 0};
				AimStep look{};
				if (!combatOwner.upkeepLook(aimContext, goalYaw, goalPitch, attacking, &look))
				{
					std::fprintf(stderr, "shared look upkeep rejected: valid=%d elapsed=%.12f yaw=%.9f pitch=%.9f goal=(%.9f %.9f) time=%.9f\n",
						aimContext.isValid() ? 1 : 0, aimContext.elapsedSeconds, aimContext.currentYaw,
						aimContext.currentPitch, goalYaw, goalPitch, aimContext.timestamp);
					return false;
				}
				metamod::CombatUpkeepUpdate update{};
				update.hasAim = true;
				update.angles = {look.pitch, look.yaw, 0};
				const auto templateValid = command->managedBotCommandTemplateValid(0);
				const auto sent = combatSentInputs.size();
				command->applyCombatUpkeep(0, update);
				const auto combatReceipt = command->executeManagedBotCommand(0, handle);
				if (std::fabs(combatCommandYaw - expected.yaw) >= .0001f ||
					std::fabs(combatCommandPitch - expected.pitch) >= .0001f)
					std::fprintf(stderr, "shared-look valid=%d receipt=%d sent=%u goal=(%.6f %.6f) expected=(%.9f %.9f) controller=(%.9f %.9f) command=(%.9f %.9f) spring=(%.9f %.9f)\n",
						templateValid ? 1 : 0, static_cast<int>(combatReceipt.result), static_cast<unsigned>(combatSentInputs.size() - sent),
						goalYaw, goalPitch, expected.yaw, expected.pitch, look.yaw, look.pitch, combatCommandYaw, combatCommandPitch,
						command->lookSpring(0).yawVelocity, command->lookSpring(0).pitchVelocity);
				return check(std::fabs(combatCommandYaw - expected.yaw) < .0001f &&
					std::fabs(combatCommandPitch - expected.pitch) < .0001f,
					"Combat and ladder use the command actor's momentum and exactly one native step");
			};
			if (!combatStep(-90, 20, true, false) || !combatStep(60, -60, true, true))
				return false;
			AimStep duplicate{};
			if (!combatOwner.upkeepLook(aimContext, -120, 50, true, &duplicate) ||
				!check(duplicate.yaw == expected.yaw && duplicate.pitch == expected.pitch,
					"a later Combat goal at the same timestamp cannot add another integration"))
				return false;
			state(CompatibilityStateId::UseEntity);
			if (!send(true, false) || !light(90, -26.565052f,
				"UseEntity inherits Guard momentum without a reset"))
				return false;
			state(CompatibilityStateId::Idle);
			if (!send(false, false) || !light(0, 0,
				"Guard to UseEntity to default retains one spring through all owners"))
				return false;
			// Higher-priority selection replaces the provisional result from its saved
			// pre-Upkeep spring; it must not integrate the provisional angle again.
			globals.time += .05f;
			const auto wanted = trackAim(expected.yaw, expected.pitch, 120, -40,
				runtime::BotTimingScheduler::kCommandInterval, 1, &spring, false);
			command->stepSharedLook(0, globals.time, expected.yaw, expected.pitch, -150, 50, false, 0U);
			const auto selected = command->stepSharedLook(0, globals.time, -70, 30, 120, -40, false, 7U);
			if (!check(selected.yaw == wanted.yaw && selected.pitch == wanted.pitch,
				"late obstacle priority selects one integration from the original spring and angle"))
				return false;
			expected = {selected.pitch, selected.yaw, 0};
			gEntities[0].v.v_angle = Vector(expected.pitch, expected.yaw, 0);
			command->resetManagedBotCommandTemplate(0);
			return light(0, 0, "discarding a command template preserves the actor's shared angular velocity");
		}
		state(CompatibilityStateId::Hunt);
		if (!send(false, false) || !light(0, 0, "normal NAV establishes a nonzero angular velocity"))
			return false;
		state(CompatibilityStateId::PlantBomb);
		if (!send(true, false) || !light(90, -26.565052f,
				"priority look inherits the real NAV yaw/pitch velocities"))
			return false;
		state(CompatibilityStateId::Hide);
		if (!(send(false, true) && light(0, 0,
			"ClearLook resumes retained Ahead0 with priority velocities on the next light, ignoring terminal NAV180") &&
			light(0, 0, "resumed normal upkeep reaches the real cached light command exactly once")))
			return false;
		state(CompatibilityStateId::Hunt);
		if (!light(0, 0, "normal state change without a new path preserves the released default owner"))
			return false;
		// A round reset keeps the native nonzero Ahead, but resets angular velocities.
		command->resetActorExecutionState(0);
		spring = {};
		aheadDirection = {0, 1, 0};
		if (!send(false, false) || !light(90, 0, "a genuine next path supplies Ahead90"))
			return false;
		runtime.beginRoundGeneration();
		token = ports.service_lifecycle().tokenForSlot(handle.actor.slot);
		command->resetActorExecutionState(0);
		spring = {};
		state(CompatibilityStateId::PlantBomb);
		if (!send(true, false) || !light(90, -26.565052f, "round reset starts priority with zero angular velocities"))
			return false;
		state(CompatibilityStateId::Idle);
		if (!send(false, true) || !light(90, 0, "round reset preserves nonzero native Ahead90"))
			return false;
		command->onActorRemoved(0);
		spring = {};
		const_cast<std::uint32_t &>(command->managedBotCommandSequences(0)) = 1000U;
		state(CompatibilityStateId::UseEntity);
		if (!send(true, false) || !light(90, -26.565052f, "new actor has no inherited angular momentum"))
			return false;
		state(CompatibilityStateId::Idle);
		if (!send(false, false) || !light(0, 0, "UseEntity release resumes a new actor's Ahead0 without a Plant terminal flag"))
			return false;
		for (const bool guard : {true, false})
		{
			if (!send(false, false, guard, !guard))
				return false;
			globals.time += .05f;
			const auto calls = combatSentInputs.size();
			command->advanceUpkeepLook(0, handle);
			const auto receipt = command->executeManagedBotCommand(0, handle);
			if (!check(receipt.result == runtime::DispatchResult::Dispatched && combatSentInputs.size() == calls + 1U &&
				std::fabs(combatCommandYaw - expected.yaw) < .0001f &&
				std::fabs(combatCommandPitch - expected.pitch) < .0001f,
				"a newly accepted Guard or Aim owner cancels default resume before the next light"))
				return false;
			state(CompatibilityStateId::DefuseBomb);
			if (!send(true, false) || !light(90, -26.565052f, "Defuse priority uses the retained noncombat spring"))
				return false;
			state(CompatibilityStateId::Idle);
			if (!send(false, false) || !light(0, 0, "Defuse ClearLook releases the priority owner without a Plant terminal flag"))
				return false;
		}
		runtime.onMessageBegin(1, 94, nullptr, &gEntities[0]);
		runtime.onWriteByte(1);
		runtime.onWriteByte(18);
		runtime.onWriteByte(10);
		runtime.onMessageEnd();
		gEntities[0].v.fov = 40.0f;
		if (!light(0, 0, "scoped sniper default goal uses half-degree drift", 0.5f))
			return false;
		gEntities[0].v.fov = 90.0f;
		if (!light(0, 0, "unscoped sniper default goal restores two-degree drift"))
			return false;
		runtime.setCompatibilityString("astrabot_mode", "enhanced");
		command->resetActorExecutionState(0);
		spring = {};
		state(CompatibilityStateId::UseEntity);
		gEntities[0].v.fov = 40.0f;
		const bool enhancedUnchanged = send(true, false) &&
			light(90, -26.565052f, "Enhanced scoped priority look retains its existing two-degree drift");
		runtime.setCompatibilityString("astrabot_mode", "compatibility");
		if (!enhancedUnchanged)
			return false;
		gEntities[0].v.fov = 90.0f;
		command->resetActorExecutionState(0);
		spring = {};
		aheadDirection = {3, -2, 0};
		state(CompatibilityStateId::Hunt);
		if (!send(false, false))
			return false;
		state(CompatibilityStateId::UseEntity);
		if (!send(true, false) || !light(90, -26.565052f, "noncardinal path then priority preserves its true spring"))
			return false;
		state(CompatibilityStateId::Idle);
		if (!(send(false, false) && light(326, 0,
			"negative noncardinal path Ahead uses engine floor(double atan2) then adds 360"))) return false;
		auto coldSequence = command->managedBotCommandSequences(0);
		for (const auto normalState : {CompatibilityStateId::Idle, CompatibilityStateId::Buy})
		{
			state(normalState);
			auto cold = std::make_unique<metamod::BotCommandRuntime>();
			cold->bindServices(ports);
			const_cast<std::uint32_t &>(cold->managedBotCommandSequences(0)) = coldSequence;
			gEntities[0].v.v_angle = Vector(20, 75, 0);
			globals.time += .05f;
			AimSpringState coldSpring{};
			const auto coldGoalYaw = 2.0f * metamod::ActionAdapter::csBotCosine(33.0f * globals.time);
			const auto coldGoalPitch = 2.0f * metamod::ActionAdapter::csBotCosine(13.0f * globals.time - 90.0f);
			auto coldExpected = trackAim(75, 20, coldGoalYaw, coldGoalPitch,
				runtime::BotTimingScheduler::kCommandInterval, 1.0f, &coldSpring, false);
			cold->advanceUpkeepLook(0, handle);
			if (!check(std::fabs(gEntities[0].v.v_angle.y - coldExpected.yaw) < .0001f &&
				std::fabs(gEntities[0].v.v_angle.x - coldExpected.pitch) < .0001f,
				"ordinary Idle and Buy supply retained Ahead0 on their first upkeep without a prior ClearLook")) return false;
			cold->advanceUpkeepLook(0, handle);
			if (!check(std::fabs(gEntities[0].v.v_angle.y - coldExpected.yaw) < .0001f &&
				std::fabs(gEntities[0].v.v_angle.x - coldExpected.pitch) < .0001f,
				"ordinary default upkeep consumes the spring only once at the same timestamp")) return false;
			const auto neutralFull = [&] {
				metamod::NavigationUpdate movement{};
				auto draft = cold->beginMovementCommand(0, handle, token, before, movement);
				metamod::ActionProposal action{metamod::ActionKind::None, {coldExpected.pitch, coldExpected.yaw, 0}, 0};
				action.stopForWeapon = true;
				const auto count = combatSentInputs.size();
				cold->finishMovementCommand(0, handle, token, before, movement, draft, action);
				return check(combatSentInputs.size() == count + 1U,
					"ordinary default-look fixture actually dispatches its neutral full command");
			};
			unsigned coldLight = 0U;
			const auto cachedLight = [&] {
				++coldLight;
				globals.time += .05f;
				coldExpected = trackAim(coldExpected.yaw, coldExpected.pitch,
					2.0f * metamod::ActionAdapter::csBotCosine(33.0f * globals.time),
					2.0f * metamod::ActionAdapter::csBotCosine(13.0f * globals.time - 90.0f),
					runtime::BotTimingScheduler::kCommandInterval, 1.0f, &coldSpring, false);
				const auto count = combatSentInputs.size();
				cold->advanceUpkeepLook(0, handle);
				const auto receipt = cold->executeManagedBotCommand(0, handle);
				const bool correct = receipt.result == runtime::DispatchResult::Dispatched &&
					combatSentInputs.size() == count + 1U && std::hypot(combatCommandForward, combatCommandSide) == 0 &&
					std::fabs(combatCommandYaw - coldExpected.yaw) < .0001f &&
					std::fabs(combatCommandPitch - coldExpected.pitch) < .0001f;
				if (!correct) std::fprintf(stderr, "cold-look state=%u light=%u receipt=%u count=%zu/%zu command=(%.6f %.6f) expected=(%.6f %.6f) entity=(%.6f %.6f) move=(%.3f %.3f)\n",
					static_cast<unsigned>(normalState), coldLight, static_cast<unsigned>(receipt.result), combatSentInputs.size(), count,
					combatCommandPitch, combatCommandYaw, coldExpected.pitch, coldExpected.yaw,
					gEntities[0].v.v_angle.x, gEntities[0].v.v_angle.y, combatCommandForward, combatCommandSide);
				return check(correct,
					"ordinary Idle and Buy deliver their continuously advanced default look through cached RunPlayerMove");
			};
			if (!neutralFull() || !cachedLight()) return false;
			const auto flags = gEntities[0].v.flags;
			gEntities[0].v.flags |= FL_FROZEN;
			globals.time += .05f;
			cold->advanceUpkeepLook(0, handle);
			if (!check(std::fabs(gEntities[0].v.v_angle.y - coldExpected.yaw) < .0001f &&
				std::fabs(gEntities[0].v.v_angle.x - coldExpected.pitch) < .0001f,
				"ordinary default upkeep holds its angle and spring while explicitly frozen")) return false;
			gEntities[0].v.flags = flags;
			if (!cachedLight()) return false;
			const auto serial = gEntities[0].serialnumber;
			++gEntities[0].serialnumber;
			globals.time += .05f;
			cold->advanceUpkeepLook(0, handle);
			gEntities[0].serialnumber = serial;
			globals.time += .05f;
			cold->advanceUpkeepLook(0, handle);
			if (!check(std::fabs(gEntities[0].v.v_angle.y - coldExpected.yaw) < .0001f &&
				std::fabs(gEntities[0].v.v_angle.x - coldExpected.pitch) < .0001f,
				"an invalidated default receipt cannot cold-start again merely because the old serial is restored")) return false;
			if (!neutralFull() || !cachedLight()) return false;
			state(CompatibilityStateId::MoveTo);
			globals.time += .05f;
			metamod::NavigationUpdate ladder{};
			ladder.locomotionIntent.nativeLadderControl = true;
			auto ladderDraft = cold->beginMovementCommand(0, handle, token, before, ladder);
			metamod::ActionProposal ladderAction{metamod::ActionKind::None,
				{coldExpected.pitch, coldExpected.yaw, 0}, 0};
			const auto count = combatSentInputs.size();
			cold->finishMovementCommand(0, handle, token, before, ladder, ladderDraft, ladderAction);
			if (!check(combatSentInputs.size() == count + 1U, "native ladder owner is accepted by an actual full dispatch")) return false;
			const auto ladderView = gEntities[0].v.v_angle;
			globals.time += .05f;
			cold->advanceUpkeepLook(0, handle);
			if (!check(gEntities[0].v.v_angle.x == ladderView.x && gEntities[0].v.v_angle.y == ladderView.y,
				"accepting native ladder control stops the old default look before the next upkeep")) return false;
			coldSequence = cold->managedBotCommandSequences(0);
		}
		return true;
	}
	if (plantTailFixture)
	{
		using namespace astrabot;
		using namespace astrabot::compat;
		auto &ports = static_cast<metamod::BotBehaviorServices &>(runtime);
		EquipmentFixture fixture{};
		equipmentFixture = &fixture;
		combatGameDll->pfnUpdateClientData = [](const edict_t *entity, int, clientdata_t *data) {
			data->m_iId = equipmentFixture->predictionUnavailable ? 0 : equipmentFixture->active;
			equipmentFixture->predictionView = entity->v.v_angle;
			if (equipmentFixture->replaceOnActiveRead) ++gEntities[0].serialnumber;
		};
		combatGameDll->pfnGetWeaponData = [](edict_t *entity, weapon_data_t *data) {
			data[6].m_iId = 6; data[6].m_iClip = 1;
			data[28].m_iId = 28; data[28].m_iClip = 30;
			data[17].m_iId = 17; data[17].m_iClip = 20;
			if (equipmentFixture->replaceEntity) ++entity->serialnumber;
			return 1;
		};
		metamod::ActionProposal exitProposal{};
		const auto run = [&](int active, bool carrying, float age, bool dropOnSelect, bool replace, bool staleFull,
			int actual = -1, bool unavailable = false, bool activeReentry = false, bool unpublished = false) {
			fixture = {};
			fixture.plantTail = true;
			fixture.primaryClip = 30;
			fixture.active = active;
			gEntities[0].v.weapons = (1 << 28) | (1 << 17) | (1 << 29) | (carrying ? 1 << 6 : 0);
			runtime.beginRoundGeneration();
			stepRifle(active == 6 ? 1 : 30, 0, active);
			auto behavior = std::make_unique<metamod::BotBehaviorRuntime>();
			behavior->bindServices(ports);
			const auto &handle = ports.service_managedBotHandles()[0];
			behavior->onActorRemembered(0, handle);
			auto &machine = const_cast<CompatibilityStateMachine &>(behavior->managedBotStateMachines(0));
			const auto &life = ports.service_lifecycle();
			StateUpdateContext start{};
			start.actor = {handle.actor.slot, handle.actor.actorGeneration};
			start.frame = {life.mapGeneration(), life.roundGeneration(), ports.service_adapterFrameCount()};
			start.timestamp = globals.time - age;
			start.fullUpdateSequence = ports.service_managedBotFullUpdateSequences()[0] - 1U;
			start.fullUpdate = start.alive = start.roundActive = true;
			start.lifeAvailability = start.roundAvailability = start.attackObservation = ObservationAvailability::Available;
			machine.initialize(start);
			machine.transition(CompatibilityStateId::PlantBomb, StateTransitionReason::ExplicitRequest,
				StateTransitionId::ExplicitStateChange, start);
			fixture.commands.clear();
			fixture.dropOnC4 = dropOnSelect;
			fixture.replaceEntity = replace;
			fixture.predictionUnavailable = unavailable;
			fixture.replaceOnActiveRead = activeReentry;
			if (actual >= 0) fixture.active = actual;
			if (unpublished)
			{
				const_cast<PublicObservationStore &>(ports.observation_publicObservations()).resetHud(start.actor, start.frame);
				fixture.refuseSwitch = true;
			}
			gEntities[0].v.v_angle = Vector(8, 67, 0);
			if (unpublished)
				globals.time += .05f; // A fresh dispatch opportunity after stepRifle consumed the previous one.
			runtime::MovementPhysicsState before{};
			before.grounded = before.ducked = true;
			before.origin = {gEntities[0].v.origin.x, gEntities[0].v.origin.y, gEntities[0].v.origin.z};
			const auto originalSerial = gEntities[0].serialnumber;
			behavior->updateManagedBotCompatibilityState(0, before, false);
			auto &sequence = const_cast<std::uint32_t &>(ports.service_managedBotFullUpdateSequences()[0]);
			if (staleFull) ++sequence;
			const auto proposal = behavior->decideManagedBotAction(0, before, {-30, 180, 0}, IN_DUCK);
			if (staleFull) --sequence;
			const bool lost = !carrying || dropOnSelect;
			const bool exited = lost || age > 5;
			const bool invalid = replace || activeReentry;
			const int effective = unavailable || actual < 0 ? active : actual;
			const auto expected = invalid ? CompatibilityStateId::PlantBomb : lost ? CompatibilityStateId::Hide
				: exited ? CompatibilityStateId::Idle : CompatibilityStateId::PlantBomb;
			const bool attack = proposal.kind == metamod::ActionKind::Fire;
			const bool result = check(machine.state() == expected &&
				(invalid || staleFull ? !attack : attack == (effective == 6)) &&
				(invalid || staleFull || ((proposal.movementButtons & IN_DUCK) != 0) == !exited) &&
				(!exited || invalid || !proposal.hasUseEntityLook) &&
				(invalid || staleFull || (proposal.preserveUpkeepView == exited &&
					(!exited || (proposal.viewAngles.pitch == 8 && proposal.viewAngles.yaw == 67)))),
				"Plant terminal input, standing posture and floor-look cancellation use the same valid full update");
			if (!result) return false;
			if (unpublished)
			{
				exitProposal = proposal;
				const auto &commands = static_cast<metamod::BotCommandServices &>(runtime);
				if (!check(!commands.observation_hasObservedActiveWeapon(0) && proposal.plantPrimaryInput,
					"current prediction supplies native PrimaryAttack while CurWeapon remains unpublished"))
					return false;
			}
			if (!invalid && !staleFull)
			{
				std::vector<unsigned> expectedCommands;
				if (effective != 6) expectedCommands.push_back(6);
				if (exited) expectedCommands.push_back(28);
				if (!check(fixture.commands == expectedCommands, "SelectC4 precedes OnExit EquipBest and is never replayed after it"))
					return false;
			}
			gEntities[0].serialnumber = originalSerial;
			return true;
		};
		const bool result = run(6, true, 5.0f, false, false, false) && run(6, true, 5.125f, false, false, false) &&
			run(6, false, 5.125f, false, false, false) && run(28, true, 5.125f, false, false, false) &&
			run(28, true, 2.0f, true, false, false) && run(6, true, 5.125f, false, true, false) &&
			run(6, true, 5.125f, false, false, true) &&
			run(6, false, 5.125f, false, false, false, 28) &&
			run(28, true, 5.125f, false, false, false, 6) &&
			run(6, true, 5.125f, false, false, false, 28, true) &&
			run(6, true, 5.125f, false, false, false, 6, false, true) &&
			run(6, true, 5.125f, false, false, false, 6, false, false, true);
		if (result)
		{
			auto command = std::make_unique<metamod::BotCommandRuntime>();
			auto &commandPorts = static_cast<metamod::BotCommandServices &>(runtime);
			command->bindServices(commandPorts);
			auto handle = ports.service_managedBotHandles()[0];
			const auto token = ports.service_lifecycle().tokenForSlot(handle.actor.slot);
			auto &machine = const_cast<CompatibilityStateMachine &>(commandPorts.behavior_managedBotStateMachines(0));
			StateUpdateContext context{};
			context.actor = {handle.actor.slot, handle.actor.actorGeneration};
			context.frame = {ports.service_lifecycle().mapGeneration(), ports.service_lifecycle().roundGeneration(), ports.service_adapterFrameCount()};
			context.timestamp = globals.time;
			context.fullUpdate = context.alive = context.roundActive = true;
			context.lifeAvailability = context.roundAvailability = ObservationAvailability::Available;
			machine.transition(CompatibilityStateId::Hide, StateTransitionReason::ExplicitRequest,
				StateTransitionId::ExplicitStateChange, context);
			const_cast<std::uint32_t &>(command->managedBotCommandSequences(0)) = ports.command_managedBotCommandSequences(0);
			runtime::MovementPhysicsState before{};
			before.grounded = true;
			metamod::NavigationUpdate movement{};
			movement.locomotionIntent.speed = 221;
			movement.locomotionIntent.direction = {-1, 0, 0};
			movement.roamResult = runtime::NavRoamResult::IntentReady;
			for (bool terminal : {true, false})
			{
				if (!terminal)
				{
					globals.time += .125f;
					++const_cast<std::uint32_t &>(ports.service_managedBotFullUpdateSequences()[0]);
				}
				const auto sentBefore = combatSentInputs.size();
				auto draft = command->beginMovementCommand(0, handle, token, before, movement);
				draft.command.viewAngles = {0, 180, 0};
				draft.command.synchronizeCombatView = true;
				metamod::ActionProposal proposal = terminal ? exitProposal : metamod::ActionProposal{metamod::ActionKind::None, {0, 180, 0}, 0};
				command->finishMovementCommand(0, handle, token, before, movement, draft, proposal);
				if (!check(combatSentInputs.size() == sentBefore + 1U && combatCommandYaw == (terminal ? 67.0f : 180.0f) &&
						(!terminal || combatCommandPitch == 8.0f),
					"actual command dispatch preserves terminal upkeep yaw over NAV180 and releases it for the next decision"))
					return false;
			}
		}
		equipmentFixture = nullptr;
		return result;
	}
	if (bestEquipmentFixture)
	{
		using namespace astrabot;
		using namespace astrabot::compat;
		auto &ports = static_cast<metamod::BotBehaviorServices &>(runtime);
		gpMetaUtilFuncs->pfnGetUserMsgID = &combatUserMessageId;
		runtime.onMessageBegin(1, 105, nullptr, &gEntities[0]);
		runtime.onWriteString("weapon_ak47");
		for (int value : {2, 90, 255, 255, 0, 0, 28, 32}) runtime.onWriteByte(value);
		runtime.onMessageEnd();
		if (!check(ports.observation_publicObservations().definition(28).flags == 32,
				"WeaponList preserves the public underwater flag"))
			return false;
		EquipmentFixture fixture{};
		equipmentFixture = &fixture;
		combatGameDll->pfnUpdateClientData = [](const edict_t *, int, clientdata_t *data) {
			data->m_iId = equipmentFixture->active;
		};
		combatGameDll->pfnGetWeaponData = [](edict_t *entity, weapon_data_t *data) {
			data[6].m_iId = 6; data[6].m_iClip = 1;
			data[28].m_iId = equipmentFixture->primaryUnknown ? 0 : 28; data[28].m_iClip = equipmentFixture->primaryClip;
			data[17].m_iId = 17; data[17].m_iClip = equipmentFixture->pistolClip;
			if (equipmentFixture->replaceEntity) ++entity->serialnumber;
			return 1;
		};
		const auto run = [&](int primary, int pistol, bool refuse, bool replace, unsigned expected, int mutation = 0, bool unknown = false) {
			fixture = {};
			fixture.primaryClip = primary;
			fixture.pistolClip = pistol;
			fixture.refuseSwitch = refuse;
			fixture.primaryUnknown = unknown;
			gEntities[0].v.weapons = (1 << 6) | (1 << 28) | (1 << 17) | (1 << 29);
			runtime.beginRoundGeneration();
			stepRifle(1, 0, 6);
			auto behavior = std::make_unique<metamod::BotBehaviorRuntime>();
			behavior->bindServices(ports);
			const auto &handle = ports.service_managedBotHandles()[0];
			behavior->onActorRemembered(0, handle);
			auto &state = const_cast<CompatibilityStateMachine &>(behavior->managedBotStateMachines(0));
			StateUpdateContext context{};
			const auto &life = ports.service_lifecycle();
			context.actor = {handle.actor.slot, handle.actor.actorGeneration};
			context.frame = {life.mapGeneration(), life.roundGeneration(), ports.service_adapterFrameCount()};
			context.timestamp = globals.time;
			context.fullUpdateSequence = ports.service_managedBotFullUpdateSequences()[0];
			context.fullUpdate = true;
			context.lifeAvailability = context.roundAvailability = context.attackObservation = ObservationAvailability::Available;
			context.alive = context.roundActive = true;
			state.initialize(context);
			(void)state.transition(CompatibilityStateId::PlantBomb, StateTransitionReason::ExplicitRequest,
				StateTransitionId::ExplicitStateChange, context);
			(void)state.beginAttack(context);
			fixture.machine = &state;
			fixture.context = context;
			fixture.attackMutation = mutation;
			runtime::MovementPhysicsState before{};
			before.grounded = true;
			before.origin = {gEntities[0].v.origin.x, gEntities[0].v.origin.y, gEntities[0].v.origin.z};
			fixture.commands.clear();
			fixture.replaceEntity = replace;
			const auto serial = gEntities[0].serialnumber;
			const auto action = behavior->decideManagedBotAction(0, before, {}, 0);
			fixture.replaceEntity = false;
			gEntities[0].serialnumber = serial;
			if (!check(expected ? fixture.commands == std::vector<unsigned>{expected} : fixture.commands.empty(),
					"Plant Attack uses public inactive ammo and discards a replaced actor receipt"))
				return false;
			if (mutation)
				return check(!action.hasCombatAim && action.kind == metamod::ActionKind::None,
					"Attack stop or re-entry during dispatch discards the old action context");
			if (refuse)
			{
				const float first = globals.time;
				for (float elapsed : {4.999f, 5.0f})
				{
					globals.time = first + elapsed;
					runtime.onStartFrame(); runtime.onStartFramePost();
					context.timestamp = globals.time;
					context.frame.tick = ports.service_adapterFrameCount();
					context.fullUpdateSequence = ports.service_managedBotFullUpdateSequences()[0];
					if (!state.attackOverlayActive()) (void)state.beginAttack(context);
					fixture.commands.clear();
					(void)behavior->decideManagedBotAction(0, before, {}, 0);
					if (!check(elapsed < 5 ? fixture.commands.empty() : fixture.commands == std::vector<unsigned>{28},
							"a refused real command still throttles DoEquip until exactly five seconds"))
						return false;
				}
			}
			return true;
		};
		const bool passed = run(0, 20, false, false, 17) && run(0, 0, false, false, 29) &&
			run(30, 20, true, false, 28) && run(30, 20, false, true, 0) &&
			run(30, 20, false, false, 28, 1) && run(30, 20, false, false, 28, 2) &&
			run(0, 20, false, false, 0, 0, true);
		equipmentFixture = nullptr;
		return passed;
	}
	if (shieldFixture)
	{
		using namespace astrabot;
		using namespace astrabot::compat;
		auto &ports = static_cast<metamod::BotBehaviorServices &>(runtime);
		auto &enemy = gEntities[1];
		runtime.onClientPutInServer(&enemy);
		enemy.free = false;
		enemy.v.flags = FL_CLIENT | FL_ONGROUND;
		enemy.v.deadflag = DEAD_NO;
		enemy.v.health = 100;
		enemy.v.team = 2;
		enemy.v.origin = gEntities[0].v.origin + Vector(320, 0, 0);
		enemy.v.velocity = Vector(250, 0, 0);
		enemy.v.view_ofs = Vector(0, 0, 19);
		enemy.v.v_angle = Vector(0, 180, 0);
		runtime.onMessageBegin(MSG_ALL, 86, nullptr, nullptr);
		runtime.onWriteByte(2);
		runtime.onWriteString("CT");
		runtime.onMessageEnd();
		auto &game = *combatGameDll;
		game.pfnUpdateClientData = [](const edict_t *entity, int, clientdata_t *client) {
			client->m_iId = 22;
			client->iuser3 = entity == &gEntities[0] && shieldOwned ? 1 << 3 : 0;
		};
		game.pfnGetWeaponData = [](edict_t *entity, weapon_data_t *weapons) {
			weapons[22].m_iId = 22;
			weapons[22].m_iWeaponState = entity == &gEntities[0] && shieldClosed ? 1 << 5 : 0;
			weapons[22].m_fInReload = entity == &gEntities[0] ? shieldSelfReload : shieldEnemyReload;
			return 1;
		};
		runtime.beginRoundGeneration();
		stepRifle(30);
		auto behavior = std::make_unique<metamod::BotBehaviorRuntime>();
		behavior->bindServices(ports);
		behavior->onActorRemembered(0, ports.service_managedBotHandles()[0]);
		auto profile = static_cast<metamod::BotNavigationServices &>(runtime).behavior_managedProfiles(0);
		profile.csbotReactionSeconds = .1f;
		behavior->assignProfile(0, profile);
		auto &machine = const_cast<CompatibilityStateMachine &>(behavior->managedBotStateMachines(0));
		StateUpdateContext context{};
		const auto &handle = ports.service_managedBotHandles()[0];
		const auto &life = ports.service_lifecycle();
		context.actor = {handle.actor.slot, handle.actor.actorGeneration};
		context.frame = {life.mapGeneration(), life.roundGeneration(), ports.service_adapterFrameCount()};
		context.timestamp = globals.time;
		context.fullUpdateSequence = ports.service_managedBotFullUpdateSequences()[0];
		context.fullUpdate = true;
		context.lifeAvailability = context.roundAvailability = ObservationAvailability::Available;
		context.attackObservation = ObservationAvailability::Available;
		context.alive = context.roundActive = true;
		machine.initialize(context);
		(void)machine.transition(CompatibilityStateId::Hunt, StateTransitionReason::ExplicitRequest,
			StateTransitionId::ExplicitStateChange, context);
		runtime::MovementPhysicsState before{};
		before.grounded = true;
		before.origin = {gEntities[0].v.origin.x, gEntities[0].v.origin.y, gEntities[0].v.origin.z};
		const auto action = [&] {
			const auto trace = selfDefenseTrace;
			selfDefenseTrace = nullptr;
			stepRifle(30);
			selfDefenseTrace = trace;
			if (trace)
				trace->eyeChecks = 0;
			return behavior->decideManagedBotAction(0, before, {}, 0);
		};
		(void)action();
		SelfDefenseTrace eye{true, false};
		selfDefenseTrace = &eye;
		const auto close = action();
		if (!check(behavior->managedBotStateMachines(0).attackOverlayActive() && close.secondaryAttack && eye.eyeChecks > 0,
				"recognized facing enemy closes an owned open shield through the normal combat path"))
			return false;
		shieldClosed = true;
		shieldSelfReload = true;
		eye.eyeChecks = 0;
		const auto recognizedReload = action();
		if (!check(recognizedReload.secondaryAttack && eye.eyeChecks == 0,
				"actual self reload opens the shield without querying enemy eyes"))
			return false;
		shieldSelfReload = false;
		shieldEnemyReload = true;
		const auto beforeRecognition = action();
		if (!check(!beforeRecognition.secondaryAttack,
				"current enemy reload does not replace the delayed recognized sample"))
			return false;
		eye.eyeChecks = 0;
		if (!check(action().secondaryAttack && eye.eyeChecks == 0,
				"recognized reload opens the shield without querying enemy eyes"))
			return false;
		shieldEnemyReload = false;
		(void)action();
		(void)action();
		shieldOwned = false;
		eye.eyeChecks = 0;
		(void)action();
		selfDefenseTrace = nullptr;
		if (!check(eye.eyeChecks == 0, "no shield avoids the shield eye query"))
			return false;
		shieldOwned = true;
		shieldClosed = false;
		eye.replaceEnemy = true;
		selfDefenseTrace = &eye;
		const auto serial = enemy.serialnumber;
		const auto replaced = action();
		selfDefenseTrace = nullptr;
		enemy.serialnumber = serial;
		if (!check(eye.eyeChecks == 1 && !replaced.secondaryAttack,
				"edict replacement in the eye callback discards the shield command"))
			return false;
		shieldClosed = true;
		enemy.v.health = 0;
		enemy.v.deadflag = DEAD_DEAD;
		const auto stopped = action();
		return check(!machine.attackOverlayActive() && stopped.secondaryAttack,
			"Attack exit requests an open shield after losing the enemy");
	}
	if (selfDefenseFixture)
	{
		using namespace astrabot;
		using namespace astrabot::compat;
		auto &ports = static_cast<metamod::BotBehaviorServices &>(runtime);
		auto &enemy = gEntities[1];
		runtime.onClientPutInServer(&enemy);
		enemy.free = false;
		enemy.v.flags = FL_CLIENT | FL_ONGROUND;
		enemy.v.deadflag = DEAD_NO;
		enemy.v.health = 100;
		enemy.v.team = 2;
		enemy.v.velocity = Vector(250, 0, 0);
		enemy.v.view_ofs = Vector(0, 0, 19);
		runtime.onMessageBegin(MSG_ALL, 86, nullptr, nullptr);
		runtime.onWriteByte(2);
		runtime.onWriteString("CT");
		runtime.onMessageEnd();
		const auto run = [&](float range, Vector angles, Vector punch, bool bodyVisible, bool eyeVisible,
							 bool replaced, bool expectedAttack, unsigned expectedChecks, const char *description,
							 float height = 0.0f, bool damaged = false, bool grenade = false) {
			enemyBehindCover = false;
			enemy.v.origin = gEntities[0].v.origin + Vector(range, 0, height);
			enemy.v.v_angle = angles;
			enemy.v.punchangle = punch;
			runtime.beginRoundGeneration();
			stepRifle(30, 0.0f, grenade ? 4 : 22);
			auto behavior = std::make_unique<metamod::BotBehaviorRuntime>();
			const auto &handle = ports.service_managedBotHandles()[0];
			behavior->bindServices(ports);
			behavior->onActorRemembered(0, handle);
			auto profile = static_cast<metamod::BotNavigationServices &>(runtime).behavior_managedProfiles(0);
			profile.csbotReactionSeconds = .1f;
			behavior->assignProfile(0, profile);
			auto &state = const_cast<CompatibilityStateMachine &>(behavior->managedBotStateMachines(0));
			auto &scenario = const_cast<ScenarioBehaviorDecision &>(behavior->scenarioDecisions(0));
			scenario.escortInferred = true;
			StateUpdateContext context{};
			context.actor = {handle.actor.slot, handle.actor.actorGeneration};
			const auto &life = ports.service_lifecycle();
			context.frame = {life.mapGeneration(), life.roundGeneration(), ports.service_adapterFrameCount()};
			context.timestamp = globals.time;
			context.fullUpdateSequence = ports.service_managedBotFullUpdateSequences()[0];
			context.fullUpdate = true;
			context.lifeAvailability = context.roundAvailability = ObservationAvailability::Available;
			context.attackObservation = ObservationAvailability::Available;
			context.alive = context.roundActive = true;
			state.initialize(context);
			if (!check(state.transition(CompatibilityStateId::MoveTo, StateTransitionReason::ExplicitRequest,
					StateTransitionId::ExplicitStateChange, context) == StateResult::Transitioned,
					"escort action starts in MoveTo"))
				return false;
			runtime::MovementPhysicsState before{};
			before.grounded = true;
			before.origin = {gEntities[0].v.origin.x, gEntities[0].v.origin.y, gEntities[0].v.origin.z};
			(void)behavior->decideManagedBotAction(0, before, {}, 0);
			if (!check(!state.attackOverlayActive(), "one sample is waiting in the reaction queue"))
				return false;
			enemyBehindCover = !bodyVisible;
			stepRifle(30, 0.0f, grenade ? 4 : 22);
			if (damaged)
				behavior->observePlayerDamage(0, context.actor,
					{life.mapGeneration(), life.roundGeneration(), ports.service_adapterFrameCount()}, &enemy,
					enemy.serialnumber, {2U, life.slotGeneration(2U)});
			SelfDefenseTrace trace{eyeVisible, replaced};
			selfDefenseTrace = &trace;
			const auto serial = enemy.serialnumber;
			const auto deadlineDraws = attackDeadlineDraws;
			const auto aimRefreshDraws = attackAimRefreshDraws;
			const auto action = behavior->decideManagedBotAction(0, before, {}, 0);
			selfDefenseTrace = nullptr;
			enemyBehindCover = false;
			enemy.serialnumber = serial;
			if (state.attackOverlayActive() != expectedAttack || trace.eyeChecks != expectedChecks)
				std::fprintf(stderr, "self-defense range=%.1f overlay=%d eye=%u expected=%d/%u\n",
					range, state.attackOverlayActive(), trace.eyeChecks, expectedAttack, expectedChecks);
			return check(state.attackOverlayActive() == expectedAttack && (grenade || action.hasCombatAim == expectedAttack) &&
					trace.eyeChecks == expectedChecks &&
					(replaced || behavior->currentEnemySource(0).id == 2U) &&
					(expectedAttack || attackDeadlineDraws == deadlineDraws) &&
					(!grenade || attackAimRefreshDraws == aimRefreshDraws + (expectedAttack ? 1U : 0U)) &&
					(bodyVisible || action.kind != metamod::ActionKind::Fire), description);
		};
		const Vector away(0, 0, 0), facing(0, 180, 0), none(0, 0, 0);
		return run(950, away, none, true, true, false, false, 0, "visible and facing away cannot start ranged self-defense") &&
			run(749, away, none, true, false, false, true, 0, "an enemy strictly inside 750 can trigger self-defense") &&
			run(750, away, none, true, true, false, false, 0, "exactly 750 needs facing and a clear eye ray") &&
			run(950, facing, none, true, false, false, false, 1, "visible body alone does not pass the eye test") &&
			run(950, facing, none, false, false, false, false, 1, "a delayed threat with blocked eyes remains acquired") &&
			run(950, facing, none, false, true, false, true, 1, "delayed hidden threat with an open eye ray starts without firing") &&
			run(950, Vector(0, 145, 0), Vector(0, 35, 0), true, true, false, true, 1, "punch yaw contributes to facing") &&
			run(950, Vector(35, 180, 0), none, true, true, false, false, 0, "pitch attenuates the enemy forward vector") &&
			run(950, facing, none, true, true, false, false, 0, "height attenuates the full 3D displacement", 600.0f) &&
			run(749, facing, none, true, false, false, true, 1, "the eye ray precedes even the close distance fallback") &&
			run(950, away, none, true, true, false, true, 0, "recent damage still permits retaliation", 0.0f, true) &&
			run(128, away, none, true, true, false, false, 0,
				"grenade recognition does not enter Attack or draw an entry aim offset", 0.0f, false, true) &&
			run(128, away, none, true, true, false, true, 0,
				"recent damage permits Attack and the entry aim draw while holding a grenade", 0.0f, true, true) &&
			run(950, facing, none, true, true, true, false, 1, "edict replacement during the eye trace cancels attack");
	}
	if (occludedFixture)
	{
		using namespace astrabot;
		auto &observation = static_cast<metamod::BotObservationServices &>(runtime);
		auto &behavior = static_cast<metamod::BotBehaviorServices &>(runtime);
		auto &navigation = static_cast<metamod::BotNavigationServices &>(runtime);
		auto &profile = const_cast<compat::ProfileRecord &>(navigation.behavior_managedProfiles(0U));
		auto &state = const_cast<compat::CompatibilityStateMachine &>(navigation.behavior_managedBotStateMachines(0U));
		profile.csbotReactionSeconds = .3f;
		profile.attackDelaySeconds = 0.0f;
		gpMetaUtilFuncs->pfnGetUserMsgID = &combatUserMessageId;
		auto &enemy = gEntities[1];
		runtime.onClientPutInServer(&enemy);
		enemy.free = false;
		const Vector exposed(260, 32, 36), acquired(260, 212, 36), later(260, -148, 36);
		enemy.v.origin = exposed;
		enemy.v.flags = FL_CLIENT | FL_NOTARGET;
		enemy.v.health = 100;
		enemy.v.deadflag = DEAD_NO;
		enemy.v.velocity = Vector(250, 0, 0);
		enemy.v.team = 2;
		runtime.onMessageBegin(MSG_ALL, 86, nullptr, nullptr);
		runtime.onWriteByte(2);
		runtime.onWriteString("CT");
		runtime.onMessageEnd();
		for (int frame = 0; frame < 8; ++frame)
			stepRifle(30);
		enemy.v.flags &= ~FL_NOTARGET;
		for (int frame = 0; frame < 3; ++frame)
			stepRifle(30);
		if (!check(!state.attackOverlayActive(), "three exposed samples are still waiting for the fourth full update"))
			return false;
		enemyBehindCover = true;
		rejectAmbushRoll = true;
		enemy.v.origin = acquired;
		std::vector<Vector> traces;
		const auto usesPoint = [&](const Vector &expected) {
			return std::any_of(traces.begin(), traces.end(), [&](const Vector &point) {
				return point.x == expected.x && point.y == expected.y && point.z == expected.z;
			});
		};
		occludedAimTraces = &traces;
		stepRifle(30);
		const bool aimedAtAcquisition = usesPoint(acquired) && !usesPoint(exposed);
		const auto target = observation.behavior_currentEnemySource(0U);
		const auto epoch = state.attackEntryEpoch();
		const world::ActorObservation *contact = nullptr;
		const auto &snapshot = behavior.observation_managedBotPerception(0U).snapshot();
		const bool masked = snapshot.findActor({2U, 1U}, &contact) == world::ContactLookupResult::Found &&
			contact && !contact->visible && contact->position.x == 0 && contact->position.y == 0 && contact->position.z == 0;
		enemy.v.origin = later;
		const auto fullUpdate = observation.service_managedBotFullUpdateSequences()[0];
		const auto commands = combatSentInputs.size();
		globals.time += .04f;
		runtime.onStartFrame();
		runtime.onStartFramePost();
		const bool lightAim = observation.service_managedBotFullUpdateSequences()[0] == fullUpdate &&
			combatSentInputs.size() > commands && combatCommandYaw > 0.0f;
		traces.clear();
		stepRifle(30);
		occludedAimTraces = nullptr;
		if (!check(state.attackOverlayActive() && state.attackEntryEpoch() == epoch && target.id == 2U && masked &&
			aimedAtAcquisition && lightAim && usesPoint(acquired) && !usesPoint(later) && !usesPoint(exposed) &&
			combatAttackCommands == 0U,
			"the first full and later full/light updates aim at acquisition origin B while hidden observations and fire stay gated"))
			return false;

		using namespace astrabot::compat;
		const auto &life = observation.service_lifecycle();
		StateUpdateContext stateContext{};
		stateContext.actor = state.actor();
		stateContext.frame = {life.mapGeneration(), life.roundGeneration(), observation.service_adapterFrameCount()};
		stateContext.timestamp = globals.time;
		stateContext.fullUpdateSequence = observation.service_managedBotFullUpdateSequences()[0];
		stateContext.fullUpdate = true;
		stateContext.lifeAvailability = stateContext.roundAvailability = ObservationAvailability::Available;
		stateContext.attackObservation = ObservationAvailability::Available;
		stateContext.alive = stateContext.roundActive = true;
		if (!check(state.stopAttack(stateContext) == StateResult::Transitioned, "stop Attack before the last visible ring sample"))
			return false;
		traces.clear();
		occludedAimTraces = &traces;
		stepRifle(30);
		occludedAimTraces = nullptr;
		return check(state.attackOverlayActive() && state.attackEntryEpoch() == epoch + 1U &&
			usesPoint(later) && !usesPoint(acquired) && combatAttackCommands == 0U,
			"a separate Attack entry on the same hidden enemy seeds their current origin without firing");
	}
	if (noticeFixture)
	{
		using namespace astrabot;
		auto &behavior = static_cast<metamod::BotBehaviorServices &>(runtime);
		auto &navigation = static_cast<metamod::BotNavigationServices &>(runtime);
		auto &profile = const_cast<compat::ProfileRecord &>(navigation.behavior_managedProfiles(0U));
		profile.skill = 25;
		const auto &state = navigation.behavior_managedBotStateMachines(0U);
		struct NoticeTrace : compat::IRandomTraceSink
		{
			void record(const compat::RandomTraceRecord &draw) override
			{
				if (std::strcmp(draw.request.semanticId, "Vision.Notice") == 0)
					draws.push_back(draw);
			}
			std::vector<compat::RandomTraceRecord> draws;
		} trace;
		const auto tick = [&](float yaw = 61.5f, float modelPitch = 0.0f)
		{
			gEntities[0].v.angles = Vector(modelPitch, yaw, 0.0f);
			stepRifle(30, yaw);
		};
		auto &enemy = gEntities[1];
		enemy.free = false;
		enemy.v.origin = Vector(260.0f, 32.0f, 36.0f);
		enemy.v.flags = FL_CLIENT | FL_NOTARGET;
		enemy.v.health = 100.0f;
		enemy.v.deadflag = DEAD_NO;
		enemy.v.team = 2;
		enemy.v.velocity = Vector(0.0f, 0.0f, 0.0f);
		runtime.onMessageBegin(MSG_ALL, 86, nullptr, nullptr);
		runtime.onWriteByte(2);
		runtime.onWriteString("CT");
		runtime.onMessageEnd();
		gpMetaUtilFuncs->pfnGetUserMsgID = &combatUserMessageId;
		tick();
		behavior.service_compatibilityRandomSource().setTraceSink(&trace);
		enemy.v.flags &= ~FL_NOTARGET;

		forcedNoticeFixtureRoll = 10.0f;
		for (int frame = 0; frame < 4; ++frame)
			tick();
		bool passed = check(trace.draws.size() == 4U && !state.attackOverlayActive(),
			"a stationary side-only enemy at the FOV edge fails four high draws");
		for (const auto &draw : trace.draws)
			passed = check(draw.request.floatLower == 0.0f && draw.request.floatUpper == 100.0f,
				"notice uses the native random float range") && passed;

		forcedNoticeFixtureRoll = 6.25f;
		tick();
		passed = check(trace.draws.size() == 5U && !state.attackOverlayActive(),
			"the notice threshold consumes a draw") && passed;
		forcedNoticeFixtureRoll = 4.0f;
		for (int frame = 0; frame < 5; ++frame)
			tick();
		passed = check(trace.draws.size() == 6U && state.attackOverlayActive(),
			"one low draw passes and the previous queue candidate skips subsequent draws") && passed;

		tick(90.0f);
		const auto &snapshot = behavior.observation_managedBotPerception(0U).snapshot();
		const world::ActorObservation *contact = nullptr;
		passed = check(snapshot.findActor({2U, 1U}, &contact) == world::ContactLookupResult::Found &&
				contact && contact->visible && contact->visibleInFovParts == world::VisibleNone,
			"a tracked enemy keeps line of sight while it is wholly outside the candidate FOV") && passed;
		forcedNoticeFixtureRoll = 10.0f;
		tick();
		passed = check(trace.draws.size() == 7U, "an empty FOV sample forces a new draw on return") && passed;
		enemy.v.velocity = Vector(0.0f, 201.0f, 0.0f);
		tick();
		passed = check(trace.draws.size() == 7U, "a running enemy skips the draw even after an empty sample") && passed;

		tick(90.0f);
		enemy.v.velocity = Vector(0.0f, 0.0f, 0.0f);
		runtime.onMessageBegin(MSG_ONE, 104, nullptr, &gEntities[0]);
		for (int value : {2048, 0, 0})
			runtime.onWriteShort(value);
		for (int field = 0; field < 4; ++field)
			runtime.onWriteByte(255);
		runtime.onMessageEnd();
		for (int frame = 0; frame < 3; ++frame)
			tick();
		passed = check(trace.draws.size() == 7U, "blinded full updates do not draw for an exposed enemy") && passed;
		tick();
		passed = check(trace.draws.size() == 8U, "the first post-flash update draws again") && passed;
		tick(61.5f, 30.0f);
		contact = nullptr;
		passed = check(behavior.observation_managedBotPerception(0U).snapshot().findActor(
				{2U, 1U}, &contact) == world::ContactLookupResult::Found && contact &&
				contact->visibleInFovParts == world::VisibleNone && trace.draws.size() == 8U,
			"model pitch removes the edge part from the threat queue without a notice draw") && passed;
		behavior.service_compatibilityRandomSource().setTraceSink(nullptr);
		return passed;
	}
	if (reacquireFixture || sharedReentryFixture)
	{
		auto &navigation = static_cast<astrabot::metamod::BotNavigationServices &>(runtime);
		auto &observation = static_cast<astrabot::metamod::BotObservationServices &>(runtime);
		auto &state = const_cast<astrabot::compat::CompatibilityStateMachine &>(navigation.behavior_managedBotStateMachines(0U));
		auto &profile = const_cast<astrabot::compat::ProfileRecord &>(navigation.behavior_managedProfiles(0U));
		profile.attackDelaySeconds = 1.5f;
		gpMetaUtilFuncs->pfnGetUserMsgID = &combatUserMessageId;
		const auto prepareEnemy = [&](std::size_t index, float x)
		{
			auto &enemy = gEntities[index];
			enemy.free = false;
			enemy.v.origin = Vector(x, 32, 36);
			enemy.v.velocity = Vector(250, 0, 0);
			enemy.v.flags = FL_CLIENT;
			enemy.v.health = 100;
			enemy.v.deadflag = DEAD_NO;
			enemy.v.team = 2;
			runtime.onMessageBegin(2, 86, nullptr, nullptr);
			runtime.onWriteByte(static_cast<int>(index + 1));
			runtime.onWriteString("CT");
			runtime.onMessageEnd();
		};
		runtime.onClientPutInServer(&gEntities[1]);
		prepareEnemy(1, 260);
		for (unsigned frame = 0; frame < 32U && combatAttackCommands == 0; ++frame)
			stepRifle(30);
		if (!check(combatAttackCommands > 0 && state.attackOverlayActive(),
				"the first acquisition clears the profile attack delay and fires"))
			return false;
		if (sharedReentryFixture)
		{
			using namespace astrabot;
			auto &ports = static_cast<metamod::BotBehaviorServices &>(runtime);
			struct ResetAimRound final : compat::IRandomTraceSink
			{
				metamod::PluginRuntime *runtime = nullptr;
				bool fired = false;
				compat::AimSpringState atRoundChange{};
				void record(const compat::RandomTraceRecord &draw) override
				{
					if (fired || std::strcmp(draw.request.semanticId, "AimOffsetX") != 0)
						return;
					fired = true;
					runtime->beginRoundGeneration();
					atRoundChange = static_cast<metamod::BotBehaviorServices &>(*runtime).command_lookSpring(0);
				}
			} trace;
			trace.runtime = &runtime;
			const auto round = runtime.snapshot().roundGeneration;
			ports.service_compatibilityRandomSource().setTraceSink(&trace);
			for (unsigned frame = 0; frame < 64 && !trace.fired; ++frame)
				stepRifle(30, 30.0f); // Keep the enemy in view while stale integration has nonzero yaw error.
			ports.service_compatibilityRandomSource().setTraceSink(nullptr);
			const auto spring = ports.command_lookSpring(0);
			if (spring.yawVelocity != trace.atRoundChange.yawVelocity || spring.pitchVelocity != trace.atRoundChange.pitchVelocity)
				std::fprintf(stderr, "stale shared look after RNG round change: before=(%.9f %.9f) after=(%.9f %.9f)\n",
					trace.atRoundChange.yawVelocity, trace.atRoundChange.pitchVelocity, spring.yawVelocity, spring.pitchVelocity);
			return check(trace.fired && runtime.snapshot().roundGeneration != round,
				"tracked Combat AimOffsetX synchronously changes the real runtime round receipt") &&
				check(spring.yawVelocity == trace.atRoundChange.yawVelocity && spring.pitchVelocity == trace.atRoundChange.pitchVelocity,
					"interrupted old Combat cannot modify shared angular velocity after the round receipt changes");
		}
		const auto attackEpoch = state.attackEntryEpoch();
		enemyBehindCover = true;
		rejectAmbushRoll = true;
		combatAttackCommands = 0;
		// Allow a cached command to finish before counting the fully occluded period.
		stepRifle(30);
		combatAttackCommands = 0;
		for (unsigned frame = 0; frame < 5U; ++frame)
			stepRifle(30);
		if (!check(combatAttackCommands == 0 && state.attackOverlayActive() && state.attackEntryEpoch() == attackEpoch,
				"concealment drains recognition without ending the attack or firing"))
			return false;
		enemyBehindCover = false;
		for (unsigned frame = 0; frame < 3U; ++frame)
			stepRifle(30);
		if (!check(combatAttackCommands == 0, "reacquisition still honors the sight delay"))
			return false;
		for (unsigned frame = 0; frame < 3U; ++frame)
			stepRifle(30);
		if (!check(combatAttackCommands > 0 && state.attackEntryEpoch() == attackEpoch,
				"same-target reacquisition resumes fire without restarting the profile attack delay"))
			return false;

		const auto waitForFire = [&](const char *message)
		{
			for (unsigned frame = 0; frame < 11U; ++frame)
				stepRifle(30);
			if (!check(combatAttackCommands == 0, message))
				return false;
			for (unsigned frame = 0; frame < 24U && combatAttackCommands == 0; ++frame)
				stepRifle(30);
			return check(combatAttackCommands > 0, "the new target can fire after its acquisition delay");
		};
		const auto waitForAcquisition = [&]()
		{
			combatAttackCommands = 0;
			for (unsigned frame = 0; frame < 12U && !observation.behavior_currentEnemySource(0).isValid(); ++frame)
				stepRifle(30);
			return check(observation.behavior_currentEnemySource(0).id == 3U && combatAttackCommands == 0,
				"the current life of the enemy is acquired before any shot");
		};
		const auto checkCachedFire = [&](const char *message)
		{
			const auto fullUpdate = observation.service_managedBotFullUpdateSequences()[0];
			const auto commands = combatSentInputs.size();
			combatAttackCommands = 0;
			globals.time += .04f;
			runtime.onStartFrame();
			runtime.onStartFramePost();
			return check(observation.service_managedBotFullUpdateSequences()[0] == fullUpdate &&
					combatSentInputs.size() > commands && combatAttackCommands == 0,
				message);
		};
		// A closer opponent replaces the current target without leaving Attack.
		runtime.onClientPutInServer(&gEntities[2]);
		prepareEnemy(2, 160);
		for (unsigned frame = 0; frame < 12U && observation.behavior_currentEnemySource(0).id != 3U; ++frame)
			stepRifle(30);
		if (!check(observation.behavior_currentEnemySource(0).id == 3U && state.attackEntryEpoch() == attackEpoch,
				"another enemy is acquired during the same attack"))
			return false;
		combatAttackCommands = 0;
		if (!waitForFire("switching targets while attacking restarts the profile delay"))
			return false;

		gEntities[1].v.flags = gEntities[2].v.flags = 0;
		gEntities[1].v.health = gEntities[2].v.health = 0;
		for (unsigned frame = 0; frame < 10U; ++frame)
			stepRifle(30);
		if (!check(!observation.behavior_currentEnemySource(0).isValid(), "losing both contacts clears the retained enemy"))
			return false;
		prepareEnemy(2, 160);
		if (!waitForAcquisition() || !waitForFire("a new acquisition after complete loss restarts the profile delay"))
			return false;

		const auto &life = observation.service_lifecycle();
		gEntities[2].v.deadflag = DEAD_DEAD;
		gEntities[2].v.health = 0;
		runtime.onMessageBegin(MSG_ALL, 103, nullptr, nullptr);
		runtime.onWriteByte(0);
		runtime.onWriteByte(3);
		runtime.onWriteByte(0);
		runtime.onWriteString("world");
		runtime.onMessageEnd();
		if (!check(!observation.behavior_currentEnemySource(0).isValid(), "DeathMsg invalidates the retained enemy"))
			return false;
		prepareEnemy(2, 160);
		// No full update observes the dead entity. The death notification invalidates this life.
		if (!checkCachedFire("respawn between full updates cannot reuse the previous life's fire command") ||
			!waitForAcquisition() || !waitForFire("a new life on the same edict restarts the profile delay"))
			return false;

		const auto oldConnection = life.slotGeneration(3U);
		runtime.onClientDisconnect(&gEntities[2]);
		runtime.onClientPutInServer(&gEntities[2]);
		prepareEnemy(2, 160);
		if (!check(life.slotGeneration(3U) != oldConnection, "the fixture advances the connection generation"))
			return false;
		if (!waitForAcquisition() || !waitForFire("a changed connection generation restarts the profile delay"))
			return false;

		using namespace astrabot::compat;
		StateUpdateContext stateContext{};
		stateContext.actor = state.actor();
		stateContext.frame = {life.mapGeneration(), life.roundGeneration(), observation.service_adapterFrameCount()};
		stateContext.timestamp = globals.time;
		stateContext.fullUpdateSequence = observation.service_managedBotFullUpdateSequences()[0];
		stateContext.fullUpdate = true;
		stateContext.lifeAvailability = stateContext.roundAvailability = ObservationAvailability::Available;
		stateContext.attackObservation = ObservationAvailability::Available;
		stateContext.transitionRequest = stateContext.rngAvailability = ObservationAvailability::Unavailable;
		stateContext.alive = stateContext.roundActive = true;
		if (!check(state.transition(CompatibilityStateId::Follow, StateTransitionReason::ObjectiveChanged,
				StateTransitionId::ExplicitStateChange, stateContext) == StateResult::Transitioned &&
				state.beginAttack(stateContext) == StateResult::Transitioned &&
				state.stopAttack(stateContext) == StateResult::Transitioned && state.state() == CompatibilityStateId::Idle,
				"stopping Attack from Follow enters Idle"))
			return false;
		return checkCachedFire("Idle cancels cached fire between full updates") && waitForAcquisition() &&
			waitForFire("Idle discards the acquisition even when the same enemy is still visible");
	}
	if (gunfireFixture)
	{
		const char *gunfireNav = "astrabot_combat_gunfire.nav";
		if (!writeTwoSiteNavigation(gunfireNav, true))
			return false;
		const astrabot::metamod::NavLoadRequest load{
			gunfireNav, "astrabot_combat_gunfire", runtime.snapshot().mapGeneration, false, 0U, false, 0U};
		const auto loaded = runtime.loadNavigationFile(&load);
		std::remove(gunfireNav);
		if (!check(loaded == astrabot::metamod::NavLoadResult::Loaded, "gunfire fixture supplies a NAV for noise intake"))
			return false;
		const auto &state = static_cast<astrabot::metamod::BotNavigationServices &>(runtime)
			.behavior_managedBotStateMachines(0U);
		const auto prepareEnemy = [&](unsigned index, float x)
		{
			auto &enemy = gEntities[index];
			enemy.free = false;
			enemy.serialnumber = static_cast<int>(30 + index);
			enemy.v.origin = Vector(x, 32, 36);
			enemy.v.flags = FL_CLIENT;
			enemy.v.health = 100;
			enemy.v.deadflag = DEAD_NO;
			enemy.v.team = 2;
			runtime.onMessageBegin(2, 86, nullptr, nullptr);
			runtime.onWriteByte(static_cast<int>(index + 1));
			runtime.onWriteString("CT");
			runtime.onMessageEnd();
		};
		prepareEnemy(1, 320);
		for (unsigned frame = 0; frame < 16U && !state.attackOverlayActive(); ++frame)
			stepRifle(30);
		if (!check(state.attackOverlayActive(), "gunfire fixture first acquires a visible enemy"))
			return false;
		enemyBehindCover = true;
		rejectAmbushRoll = true;
		for (unsigned frame = 0; frame < 5U; ++frame)
			stepRifle(30);
		rejectAmbushRoll = false;
		if (!check(rejectedPercentageRolls == 1 && state.attackOverlayActive(),
				"the attack remains active after over half a second without sight of its target"))
			return false;

		prepareEnemy(2, 0);
		runtime.onEmitSound(&gEntities[2], "weapons/ak47-1.wav", 1, 1);
		for (unsigned frame = 0; frame < 2U && state.attackOverlayActive(); ++frame)
			stepRifle(30);
		const auto &noise = static_cast<astrabot::metamod::BotBehaviorServices &>(runtime)
			.observation_managedBotPerception(0U).snapshot().noise();
		if (!noise.active || noise.priority != astrabot::world::NoisePriority::High)
			std::fprintf(stderr, "threatening gunfire was not published as high-priority noise (active=%d, priority=%d)\n",
				noise.active, static_cast<int>(noise.priority));
		return check(noise.active && noise.priority == astrabot::world::NoisePriority::High &&
				!state.attackOverlayActive() && (gLastRunPlayerMoveButtons & IN_ATTACK) == 0,
			"fresh threatening gunfire closer than the hidden target ends the attack");
	}
	gCaptureMovementLogs = true;
	gProfileEnabled = true;
	float enemyDistance = 100.0f;
	const auto observe = [&](bool enemyPresent, const char *publicTeam = "CT")
	{
		const bool hostile = enemyPresent && std::strcmp(publicTeam, "CT") == 0;
		combatAttackCommands = 0U;
		combatMovingFireCommands = 0U;
		combatLastAttackPitch = std::numeric_limits<float>::quiet_NaN();
		combatSentInputs.clear();
		gEntities[0].v.v_angle = Vector(20.0f, 30.0f, 0.0f);
		gEntities[1].v.flags = enemyPresent ? FL_CLIENT : 0;
		// Real CS clients publish their team in TeamInfo, not necessarily entvars.
		gEntities[1].v.team = 0;
		runtime.onMessageBegin(2, 86, nullptr, nullptr);
		runtime.onWriteByte(2);
		runtime.onWriteString(publicTeam);
		runtime.onMessageEnd();
		gEntities[1].v.health = enemyPresent ? 100.0f : 0.0f;
		gEntities[1].v.deadflag = enemyPresent ? DEAD_NO : DEAD_DEAD;
		gEntities[1].v.origin = Vector(80.0f + enemyDistance, 32.0f, 36.0f);
		gMovementLogs.clear();
		for (unsigned frame = 0; frame < 128U; ++frame)
		{
			runtime.onMessageBegin(1, 94, nullptr, &gEntities[0]);
			runtime.onWriteByte(1);
			runtime.onWriteByte(22);
			runtime.onWriteByte(30);
			runtime.onMessageEnd();
			globals.time += 0.125f;
			runtime.onStartFrame();
			runtime.onStartFramePost();
		}
		bool found = false;
		bool physicalSample = false;
		bool cadenceWait = false;
		for (const auto &message : gMovementLogs)
		{
			physicalSample = physicalSample || message.find("movement physics ") == 0U;
			if ((message.find("profile stage=") == 0U || message.find("profile pathSearchByCaller.") == 0U) &&
				!check(message.find(" calls=0 ") == std::string::npos,
					"sparse profiling omits zero-call aggregate records"))
				return false;
			if (message.find("action sensor slot=1 ") != 0U || message.find(" combatResult=") == std::string::npos)
				continue;
			cadenceWait = cadenceWait || message.find(" weapon_reason=cadence_wait ") != std::string::npos;
			if (message.find(" weapon_reason=cadence_wait ") != std::string::npos)
			{
				const auto frameKey = message.find(" frame=");
				if (!check(frameKey != std::string::npos, "cadence record identifies the dispatched frame"))
					return false;
				const auto frame = static_cast<std::uint32_t>(std::stoul(message.substr(frameKey + 7U)));
				bool sent = false;
				for (const auto &command : combatSentInputs)
					if (command.first == frame)
					{
						sent = true;
						if (!check((command.second & IN_ATTACK) == 0U,
								"cadence wait clears IN_ATTACK in the actual engine command"))
							return false;
					}
				if (!check(sent, "cadence wait reaches engine dispatch"))
					return false;
			}
			if (!check(message.find(" core_reason=") != std::string::npos &&
						message.find(" weapon_reason=") != std::string::npos &&
						message.find(" dt=") != std::string::npos,
					"combat diagnostics distinguish Core waiting from final weapon gates"))
				return false;
			if (message.find(" hasAim=1 ") != std::string::npos)
			{
				const auto pointKey = message.find(" aim_point=("), rangeKey = message.find(" cadence_range=");
				if (!check(pointKey != std::string::npos && rangeKey != std::string::npos,
						"cadence record contains its actual aim point"))
					return false;
				float x = 0, y = 0, z = 0;
				std::istringstream point(message.substr(pointKey + 12U));
				point >> x >> y >> z;
				const float actual = std::stof(message.substr(rangeKey + 15U));
				const float expected = std::hypot(x - gEntities[0].v.origin.x, y - gEntities[0].v.origin.y);
				if (!check(!point.fail() && std::fabs(actual - expected) < .2f,
						"fire cadence uses aim error and prediction rather than raw enemy distance"))
					return false;
			}
			if (!hostile &&
				!check(message.find(" target=0 ") != std::string::npos,
					"combat sensor reports no target when no enemy was acquired"))
				return false;
			if (message.find(hostile ? " target=1 " : " target=0 ") != std::string::npos)
				found = true;
		}
		if (!check(physicalSample, "detailed physical samples continue beyond the initial log budget"))
			return false;
		if (!check(hostile && !humanFriendlyBlocks ? combatAttackCommands > 0U : combatAttackCommands == 0U,
				"engine angle feedback reaches fire with a visible enemy and holds fire without one"))
			return false;
		if (hostile && !humanFriendlyBlocks && enemyDistance == 100.0f &&
			!check(
				std::isfinite(combatLastAttackPitch) && combatLastAttackPitch > 0.0f,
				"low-skill rifle's actual unobstructed shot uses the reference chest aim instead of visible head"))
			return false;
		if (hostile && enemyDistance > 800.0f &&
			!check(cadenceWait, "skilled distant rifle alternates fire and reference cadence waits"))
			return false;
		if (!found)
		{
			for (const auto &message : gMovementLogs)
				if (message.find("action sensor ") == 0U || message.find("profile currentWeapon ") == 0U)
					std::fprintf(stderr, "%s\n", message.c_str());
		}
		return check(found,
			enemyPresent ? "combat sensor reports an acquired visible enemy"
						 : "no-enemy case exercises the combat-result log");
	};
	bool passed = observe(false);
	const auto deadlinesBefore = attackDeadlineDraws;
	const auto retriesBefore = retreatTimerDraws;
	passed = observe(true) && passed;
	passed = check(attackDeadlineDraws == deadlinesBefore + 1 && retreatTimerDraws > retriesBefore,
		"A sustained attack sets its pinned deadline once and schedules a retreat attempt even without a safe spot") && passed;
	passed = observe(false) && observe(true, "TERRORIST") && observe(true, "SPECTATOR") && passed;
	gEntities[2].v.flags = FL_CLIENT;
	gEntities[2].v.team = 0;
	gEntities[2].v.health = 100;
	gEntities[2].v.deadflag = DEAD_NO;
	gEntities[2].v.origin = Vector(130, 32, 36);
	runtime.onMessageBegin(2, 86, nullptr, nullptr);
	runtime.onWriteByte(3);
	runtime.onWriteString("TERRORIST");
	runtime.onMessageEnd();
	humanFriendlyBlocks = true;
	passed = observe(true) && passed;
	humanFriendlyBlocks = false;
	gEntities[2].v.health = 0;
	gEntities[2].v.flags = 0;
	enemyDistance = 900.0f;
	passed =
		check(
			static_cast<astrabot::metamod::BotObservationServices &>(runtime).behavior_managedProfiles(0U).skill >= 50,
			"distant cadence fixture uses a skilled profile") &&
		observe(true) && passed;
	bool upkeepTurned = false;
	for (unsigned step = 0; step < 60U; ++step)
	{
		auto &ports = static_cast<astrabot::metamod::BotObservationServices &>(runtime);
		const auto fullBefore = ports.service_managedBotFullUpdateSequences()[0];
		const float yawBefore = combatCommandYaw;
		gEntities[1].v.velocity = Vector(0.0f, 250.0f, 0.0f);
		gEntities[1].v.origin.y += 250.0f / 30.0f;
		globals.time += 1.0f / 30.0f;
		runtime.onStartFrame();
		runtime.onStartFramePost();
		if (ports.service_managedBotFullUpdateSequences()[0] == fullBefore &&
			std::fabs(combatCommandYaw - yawBefore) > 0.001f)
			upkeepTurned = true;
	}
	passed = check(upkeepTurned, "combat upkeep tracks a moving target between full decisions") && passed;
	bool blockedBetweenFull = false;
	for (unsigned step = 0; step < 40U && !blockedBetweenFull; ++step)
	{
		auto &ports = static_cast<astrabot::metamod::BotObservationServices &>(runtime);
		const auto full = ports.service_managedBotFullUpdateSequences()[0];
		combatAttackCommands = 0;
		globals.time += .04f;
		runtime.onStartFrame();
		runtime.onStartFramePost();
		if (ports.service_managedBotFullUpdateSequences()[0] == full || combatAttackCommands == 0)
			continue;
		humanFriendlyBlocks = true;
		gEntities[2].v.flags = FL_CLIENT;
		gEntities[2].v.health = 100;
		const auto firingFull = ports.service_managedBotFullUpdateSequences()[0];
		combatAttackCommands = 0;
		globals.time += .04f;
		runtime.onStartFrame();
		runtime.onStartFramePost();
		blockedBetweenFull = ports.service_managedBotFullUpdateSequences()[0] == firingFull;
		passed =
			check(combatAttackCommands == 0, "upkeep clears cached fire when a human teammate enters the shot line") &&
			passed;
		humanFriendlyBlocks = false;
		gEntities[2].v.flags = 0;
		gEntities[2].v.health = 0;
	}
	passed = check(blockedBetweenFull, "friendly obstruction exercises a frame between full decisions") && passed;
	gEntities[1].v.velocity = Vector(0.0f, 0.0f, 0.0f);
	gEntities[0].v.weapons = (1 << 18) | (1 << 17);
	gEntities[1].v.origin = Vector(180, 32, 36);
	for (unsigned step = 0; step < 64; ++step)
	{
		runtime.onMessageBegin(1, 94, nullptr, &gEntities[0]);
		runtime.onWriteByte(1);
		runtime.onWriteByte(18);
		runtime.onWriteByte(10);
		runtime.onMessageEnd();
		globals.time += .125f;
		runtime.onStartFrame();
		runtime.onStartFramePost();
	}
	passed = check(combatPistolSelected,
				 "pistol switching dispatches a GameDLL weapon classname instead of a client-only slot command") &&
		passed;
	// Exhausted initial sampling must not suppress a new diagnostic session.
	gProfileEnabled = false;
	globals.time += 0.125f;
	runtime.onStartFrame();
	runtime.onStartFramePost();
	gMovementLogs.clear();
	gProfileEnabled = true;
	for (unsigned frame = 0; frame < 8U; ++frame)
	{
		globals.time += 0.125f;
		runtime.onStartFrame();
		runtime.onStartFramePost();
	}
	unsigned restartedSamples = 0U;
	bool availabilityReplayed = false;
	bool captureStarted = false;
	for (const auto &message : gMovementLogs)
	{
		if (message.find("movement physics ") == 0U)
			++restartedSamples;
		availabilityReplayed = availabilityReplayed || message.find("movement unavailable ") == 0U;
		captureStarted = captureStarted || message.find("profile capture enabled=1 ") == 0U;
	}
	const bool restartPassed = check(restartedSamples >= 8U && availabilityReplayed && captureStarted,
		"off/on refreshes physical sampling and replays the availability diagnostic");
	gCaptureMovementLogs = false;
	gProfileEnabled = false;
	const char *retreatNav = "astrabot_combat_retreat.nav";
	if (!writeTwoSiteNavigation(retreatNav, true, true))
		return false;
	const astrabot::metamod::NavLoadRequest load{
		retreatNav, "astrabot_combat_retreat", runtime.snapshot().mapGeneration, false, 0U, false, 0U};
	const auto loaded = runtime.loadNavigationFile(&load);
	std::remove(retreatNav);
	if (!check(loaded == astrabot::metamod::NavLoadResult::Loaded, "retreat fixture loads connected NAV cover"))
		return false;
	auto &services = static_cast<astrabot::metamod::BotNavigationServices &>(runtime);
	const auto &state = services.behavior_managedBotStateMachines(0U);
	gEntities[0].v.origin = Vector(80, 32, 36);
	gEntities[1].v.origin = Vector(180, 32, 36);
	coverBlocksEnemySight = true;
	bool movedToCover = false;
	for (unsigned step = 0; step < 160U; ++step)
	{
		runtime.onMessageBegin(1, 94, nullptr, &gEntities[0]);
		runtime.onWriteByte(1);
		runtime.onWriteByte(18);
		runtime.onWriteByte(10);
		runtime.onMessageEnd();
		gEntities[0].v.v_angle = Vector(0, 0, 0);
		globals.time += .125f;
		runtime.onStartFrame();
		runtime.onStartFramePost();
		const auto &decision = services.behavior_scenarioDecisions(0U);
		if (state.state() == astrabot::compat::CompatibilityStateId::Hide && !state.attackOverlayActive() &&
			decision.hasTarget && decision.target.position.x == 32 && decision.target.position.y == 50)
		{
			movedToCover = true;
			break;
		}
	}
	passed = check(movedToCover, "eligible cover sends the attacking bot to Hide with the selected NAV target") &&
		passed && restartPassed;
	gEntities[0].v.weapons = (1 << 22);
	gEntities[1].v.health = 0;
	gEntities[1].v.deadflag = DEAD_DEAD;
	for (unsigned frame = 0; frame < 136U; ++frame)
		stepRifle(30);
	// Start the reload encounter with a fresh round and a live opponent.
	runtime.onRoundLifecycleMessage("World triggered \"Round_Start\"");
	gEntities[1].v.health = 100;
	gEntities[1].v.deadflag = DEAD_NO;
	for (unsigned frame = 0; frame < 16U && !state.attackOverlayActive(); ++frame)
	{
		// Keep the intended encounter pose while the bot reacquires its target.
		gEntities[0].v.origin = Vector(80, 32, 36);
		gEntities[0].v.v_angle = gEntities[0].v.angles = Vector(0, 0, 0);
		stepRifle(30);
	}
	if (!check(state.attackOverlayActive(), "empty-magazine fixture first reacquires the live enemy"))
		return false;
	forceLowPercentageRoll = true;
	const auto reloadBefore = combatReloadCommands;
	for (unsigned frame = 0; frame < 4U; ++frame)
		stepRifle(0);
	forceLowPercentageRoll = false;
	coverBlocksEnemySight = false;
	const auto &reloadGoal = services.behavior_scenarioDecisions(0U);
	passed = check(combatReloadCommands > reloadBefore &&
			state.state() == astrabot::compat::CompatibilityStateId::Hide && !state.attackOverlayActive() &&
			reloadGoal.hasTarget && reloadGoal.target.position.x == 32 && reloadGoal.target.position.y == 50,
		"an empty magazine dispatches IN_RELOAD and sends the recently engaged bot to safe cover") && passed;
	gEntities[1].v.health = 0;
	gEntities[1].v.deadflag = DEAD_DEAD;
	for (unsigned frame = 0; frame < 4U; ++frame)
		stepRifle(30);
	passed = check(state.state() != astrabot::compat::CompatibilityStateId::Hide,
		"after the refill is observed, zero-duration reload cover can end") && passed;

	gEntities[0].v.origin = Vector(80, 32, 36);
	gEntities[1].v.health = 100;
	gEntities[1].v.deadflag = DEAD_NO;
	for (unsigned frame = 0; frame < 12U && !state.attackOverlayActive(); ++frame)
	{
		gEntities[0].v.origin = Vector(80, 32, 36);
		gEntities[0].v.v_angle = gEntities[0].v.angles = Vector(0, 0, 0);
		stepRifle(30);
	}
	if (!check(state.attackOverlayActive(), "lost-contact fixture begins with a visible enemy in an active attack"))
		return false;
	coverBlocksEnemySight = true;
	enemyBehindCover = true;
	rejectAmbushRoll = true;
	for (unsigned frame = 0; frame < 5U; ++frame)
		stepRifle(30);
	rejectAmbushRoll = false;
	passed = check(rejectedPercentageRolls == 1 && state.attackOverlayActive(),
		"a rejected chance is tried once per loss of sight and retains the attack") && passed;

	enemyBehindCover = false;
	for (unsigned frame = 0; frame < 5U; ++frame)
		stepRifle(30);
	enemyBehindCover = true;
	forceLowPercentageRoll = true;
	const auto durationDraws = retreatTimerDraws;
	for (unsigned frame = 0; frame < 5U; ++frame)
		stepRifle(30);
	coverBlocksEnemySight = false;
	enemyBehindCover = false;
	forceLowPercentageRoll = false;
	const auto &ambushGoal = services.behavior_scenarioDecisions(0U);
	passed = check(retreatTimerDraws == durationDraws + 1 && !state.attackOverlayActive() &&
			state.state() == astrabot::compat::CompatibilityStateId::Hide && ambushGoal.hasTarget &&
			ambushGoal.target.position.x == 32 && ambushGoal.target.position.y == 50 &&
			(gLastRunPlayerMoveButtons & IN_ATTACK) == 0,
		"after seeing the enemy again, the next loss may start a new ambush in nearby cover") && passed;

	return passed;
}
}
