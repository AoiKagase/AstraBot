#include "plugin/plugin_runtime.hpp"
#include "runtime/shared/bot_runtime_support.hpp"
#include <cstdlib>
#include <sstream>

namespace
{
bool testCombatTargetLog(astrabot::metamod::PluginRuntime &, enginefuncs_t &, globalvars_t &);
}
#define ASTRABOT_EXTRA_ACTOR_TEST testCombatTargetLog
#include "compat_actor_command_tests.cpp"

namespace
{
unsigned combatAttackCommands = 0U;
float combatCommandYaw = 0.0f;
float combatCommandPitch = 0.0f;
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
unsigned retreatTimerDraws = 0;
float combatRandomFloat(float low, float high)
{
	if (low == 7.0f && high == 10.0f)
		++attackDeadlineDraws;
	if (low == 3.0f && high == 15.0f)
		++retreatTimerDraws;
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
	if (enemyBehindCover && self == &gEntities[0] && ignoreMonsters == 1 &&
		(std::fabs(end[0] - gEntities[1].v.origin.x) <= 24.0f ||
			(gEntities[2].v.health > 0 && std::fabs(end[0] - gEntities[2].v.origin.x) <= 24.0f)))
		result->flFraction = 0.0f;
	if (coverBlocksEnemySight && ignoreMonsters == 0x101 && self == &gEntities[1])
		result->flFraction = 0.5f;
	if (humanFriendlyBlocks && ignoreMonsters == 0)
		result->pHit = &gEntities[2];
}

void combatGameClientCommand(edict_t *entity)
{
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
		++combatAttackCommands;
	if (entity == &gEntities[0])
		combatCommandYaw = angles[1];
	if (entity == &gEntities[0])
	{
		combatCommandPitch = angles[0];
		if ((buttons & IN_ATTACK) != 0U && std::hypot(forward, side) > .01f)
			++combatMovingFireCommands;
	}
	if (entity == &gEntities[0] && combatFrameCounter)
		combatSentInputs.push_back({*combatFrameCounter, buttons});
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
	const auto stepRifle = [&](int clip)
	{
		runtime.onMessageBegin(1, 94, nullptr, &gEntities[0]);
		runtime.onWriteByte(1);
		runtime.onWriteByte(22);
		runtime.onWriteByte(clip);
		runtime.onMessageEnd();
		gEntities[0].v.v_angle = Vector(0, 0, 0);
		globals.time += .125f;
		runtime.onStartFrame();
		runtime.onStartFramePost();
	};
#ifdef _MSC_VER
	char *gunfireMode = nullptr;
	std::size_t gunfireModeSize = 0;
	const bool gunfireFixture = _dupenv_s(&gunfireMode, &gunfireModeSize, "ASTRABOT_TEST_COMBAT_GUNFIRE") == 0 &&
		gunfireMode != nullptr;
	std::free(gunfireMode);
#else
	const bool gunfireFixture = std::getenv("ASTRABOT_TEST_COMBAT_GUNFIRE") != nullptr;
#endif
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
		if (hostile && enemyDistance == 100.0f &&
			!check(
				combatCommandPitch > 0.0f, "low-skill rifle selects the reference chest aim instead of visible head"))
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
	for (unsigned frame = 0; frame < 12U; ++frame)
		stepRifle(30);
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
