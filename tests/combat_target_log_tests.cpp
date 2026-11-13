#include "plugin_runtime.hpp"
#include "bot_runtime_support.hpp"
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
void traceCombatVisibility(const float *start, const float *end, int ignoreMonsters,
	edict_t *self, TraceResult *result)
{
	traceObjectiveVisibility(start, end, ignoreMonsters, self, result);
	if (humanFriendlyBlocks && ignoreMonsters == 0) result->pHit = &gEntities[2];
}
void combatGameClientCommand(edict_t *entity)
{
	if(entity==&gEntities[0] && std::strcmp(astrabot::metamod::HookCommandArgv(0),"weapon_glock18")==0)
		combatPistolSelected=true;
	hookedGameClientCommand(entity);
}
const std::uint32_t *combatFrameCounter = nullptr;
std::vector<std::pair<std::uint32_t, unsigned short>> combatSentInputs;
void runCombatPlayerMove(edict_t *entity, const float *angles, float forward, float side,
	float up, unsigned short buttons, byte impulse, byte msec)
{
	runPlayerMove(entity, angles, forward, side, up, buttons, impulse, msec);
	// HLDS RunPlayerMove does not publish FakeClient v_angle; the input owner must.
	if (entity == &gEntities[0] && (buttons & IN_ATTACK) != 0U) ++combatAttackCommands;
	if (entity == &gEntities[0]) combatCommandYaw = angles[1];
	if (entity == &gEntities[0])
	{
		combatCommandPitch = angles[0];
		if ((buttons & IN_ATTACK) != 0U && std::hypot(forward, side) > .01f) ++combatMovingFireCommands;
	}
	if (entity == &gEntities[0] && combatFrameCounter)
		combatSentInputs.push_back({*combatFrameCounter, buttons});
}

bool testCombatTargetLog(astrabot::metamod::PluginRuntime &runtime, enginefuncs_t &engine,
	globalvars_t &globals)
{
	gCaptureMovementLogs = true;
	gMovementLogs.clear();
	astrabot::metamod::RuntimeProfiler logProfiler;
	astrabot::metamod::runtime_detail::profiledLogConsole(logProfiler, nullptr,
		"action sensor target=%d", 0);
	if (!check(gMovementLogs.empty(), "normal logging suppresses detailed sensor records")) return false;
	astrabot::metamod::runtime_detail::importantLogConsole(logProfiler, nullptr,
		"join confirmed slot=%u", 1U);
	if (!check(gMovementLogs.size() == 1U && gMovementLogs[0] == "join confirmed slot=1",
		"important events remain available without detailed profiling")) return false;
	logProfiler.setEnabled(true, 0.0);
	astrabot::metamod::runtime_detail::profiledLogConsole(logProfiler, nullptr,
		"action sensor target=%d combatResult=%d", 0, 8);
	if (!check(gMovementLogs.size() == 2U &&
		gMovementLogs[1] == "action sensor target=0 combatResult=8",
		"detailed mode retains the machine-readable legacy format")) return false;
	using astrabot::compat::NavigationStepOutcome;
	using astrabot::metamod::runtime_detail::navigationTransitionEvent;
	if (!check(std::strcmp(navigationTransitionEvent(NavigationStepOutcome::EndOfPath), "navigationComplete") == 0 &&
		std::strcmp(navigationTransitionEvent(NavigationStepOutcome::PathFailure), "navigationFailure") == 0 &&
		std::strcmp(navigationTransitionEvent(NavigationStepOutcome::ObservationUnavailable), "navigationTransition") == 0,
		"navigation event names distinguish completion, failure and unavailable observations")) return false;
	gObjectiveEntitiesAvailable = false;
	gServerSetsEntityTeam = true;
	engine.pfnRandomLong = &siteFixtureRandomLong;
	engine.pfnRandomFloat = &siteFixtureRandomFloat;
	engine.pfnTraceLine = &traceCombatVisibility;
	engine.pfnRunPlayerMove = &runCombatPlayerMove;
	gHookedGameDllTable.pfnClientCommand = &combatGameClientCommand;
	combatFrameCounter = &static_cast<astrabot::metamod::BotObservationServices &>(runtime).
		service_adapterFrameCount();
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
	runtime.onWriteByte(1); runtime.onWriteByte(22); runtime.onWriteByte(30);
	runtime.onMessageEnd();
	gCaptureMovementLogs = true;
	gProfileEnabled = true;
	float enemyDistance = 100.0f;
	const auto observe = [&](bool enemyPresent, const char *publicTeam = "CT") {
		const bool hostile = enemyPresent && std::strcmp(publicTeam, "CT") == 0;
		combatAttackCommands = 0U;
		combatMovingFireCommands = 0U;
		combatSentInputs.clear();
		gEntities[0].v.v_angle = Vector(20.0f, 30.0f, 0.0f);
		gEntities[1].v.flags = enemyPresent ? FL_CLIENT : 0;
		// Real CS clients publish their team in TeamInfo, not necessarily entvars.
		gEntities[1].v.team = 0;
		runtime.onMessageBegin(2, 86, nullptr, nullptr);
		runtime.onWriteByte(2); runtime.onWriteString(publicTeam); runtime.onMessageEnd();
		gEntities[1].v.health = enemyPresent ? 100.0f : 0.0f;
		gEntities[1].v.deadflag = enemyPresent ? DEAD_NO : DEAD_DEAD;
		gEntities[1].v.origin = Vector(80.0f + enemyDistance, 32.0f, 36.0f);
		gMovementLogs.clear();
		for (unsigned frame = 0; frame < 128U; ++frame)
		{
			runtime.onMessageBegin(1, 94, nullptr, &gEntities[0]);
			runtime.onWriteByte(1); runtime.onWriteByte(22); runtime.onWriteByte(30);
			runtime.onMessageEnd();
			globals.time += 0.125f;
			runtime.onStartFrame(); runtime.onStartFramePost();
		}
		bool found = false;
		bool physicalSample = false;
		bool cadenceWait = false;
		for (const auto &message : gMovementLogs)
		{
			physicalSample = physicalSample || message.find("movement physics ") == 0U;
			if ((message.find("profile stage=") == 0U || message.find("profile pathSearchByCaller.") == 0U) &&
				!check(message.find(" calls=0 ") == std::string::npos,
					"sparse profiling omits zero-call aggregate records")) return false;
			if (message.find("action sensor slot=1 ") != 0U ||
				message.find(" combatResult=") == std::string::npos) continue;
			cadenceWait = cadenceWait || message.find(" weapon_reason=cadence_wait ") != std::string::npos;
			if (message.find(" weapon_reason=cadence_wait ") != std::string::npos)
			{
				const auto frameKey = message.find(" frame=");
				if (!check(frameKey != std::string::npos,
					"cadence record identifies the dispatched frame")) return false;
				const auto frame = static_cast<std::uint32_t>(std::stoul(message.substr(frameKey + 7U)));
				bool sent = false;
				for (const auto &command : combatSentInputs)
					if (command.first == frame)
					{
						sent = true;
						if (!check((command.second & IN_ATTACK) == 0U,
							"cadence wait clears IN_ATTACK in the actual engine command")) return false;
					}
				if (!check(sent, "cadence wait reaches engine dispatch")) return false;
			}
			if (!check(message.find(" core_reason=") != std::string::npos &&
				message.find(" weapon_reason=") != std::string::npos &&
				message.find(" dt=") != std::string::npos,
				"combat diagnostics distinguish Core waiting from final weapon gates")) return false;
			if (message.find(" hasAim=1 ") != std::string::npos)
			{
				const auto pointKey=message.find(" aim_point=("), rangeKey=message.find(" cadence_range=");
				if(!check(pointKey!=std::string::npos&&rangeKey!=std::string::npos,"cadence record contains its actual aim point"))return false;
				float x=0,y=0,z=0;std::istringstream point(message.substr(pointKey+12U));point>>x>>y>>z;
				const float actual=std::stof(message.substr(rangeKey+15U));
				const float expected=std::hypot(x-gEntities[0].v.origin.x,y-gEntities[0].v.origin.y);
				if(!check(!point.fail()&&std::fabs(actual-expected)<.2f,
					"fire cadence uses aim error and prediction rather than raw enemy distance"))return false;
			}
			if (!hostile && !check(message.find(" target=0 ") != std::string::npos,
				"combat sensor reports no target when no enemy was acquired")) return false;
			if (message.find(hostile ? " target=1 " : " target=0 ") != std::string::npos)
				found = true;
		}
		if (!check(physicalSample, "detailed physical samples continue beyond the initial log budget")) return false;
		if (!check(hostile && !humanFriendlyBlocks ? combatAttackCommands > 0U : combatAttackCommands == 0U,
			"engine angle feedback reaches fire with a visible enemy and holds fire without one")) return false;
		if (hostile && enemyDistance == 100.0f &&
			!check(combatCommandPitch > 0.0f, "low-skill rifle selects the reference chest aim instead of visible head")) return false;
		if (hostile && enemyDistance > 800.0f &&
			!check(cadenceWait, "skilled distant rifle alternates fire and reference cadence waits")) return false;
		if (!found)
		{
			for (const auto &message : gMovementLogs)
				if (message.find("action sensor ") == 0U || message.find("profile currentWeapon ") == 0U)
					std::fprintf(stderr, "%s\n", message.c_str());
		}
		return check(found, enemyPresent ? "combat sensor reports an acquired visible enemy" :
			"no-enemy case exercises the combat-result log");
	};
	bool passed = observe(false) && observe(true) && observe(false) &&
		observe(true, "TERRORIST") && observe(true, "SPECTATOR");
	gEntities[2].v.flags = FL_CLIENT; gEntities[2].v.team = 0;
	gEntities[2].v.health = 100; gEntities[2].v.deadflag = DEAD_NO;
	gEntities[2].v.origin = Vector(130,32,36);
	runtime.onMessageBegin(2,86,nullptr,nullptr);
	runtime.onWriteByte(3); runtime.onWriteString("TERRORIST"); runtime.onMessageEnd();
	humanFriendlyBlocks = true;
	passed = observe(true) && passed;
	humanFriendlyBlocks = false; gEntities[2].v.health = 0; gEntities[2].v.flags = 0;
	enemyDistance = 900.0f;
	passed = check(static_cast<astrabot::metamod::BotObservationServices &>(runtime).
		behavior_managedProfiles(0U).skill >= 50U,
		"distant cadence fixture uses a skilled profile") && observe(true) && passed;
	bool upkeepTurned = false;
	for (unsigned step = 0; step < 60U; ++step)
	{
		auto &ports = static_cast<astrabot::metamod::BotObservationServices &>(runtime);
		const auto fullBefore = ports.service_managedBotFullUpdateSequences()[0];
		const float yawBefore = combatCommandYaw;
		gEntities[1].v.velocity = Vector(0.0f, 250.0f, 0.0f);
		gEntities[1].v.origin.y += 250.0f / 30.0f;
		globals.time += 1.0f / 30.0f;
		runtime.onStartFrame(); runtime.onStartFramePost();
		if (ports.service_managedBotFullUpdateSequences()[0] == fullBefore &&
			std::fabs(combatCommandYaw - yawBefore) > 0.001f) upkeepTurned = true;
	}
	passed = check(upkeepTurned, "combat upkeep tracks a moving target between full decisions") && passed;
	bool blockedBetweenFull = false;
	for (unsigned step = 0; step < 40U && !blockedBetweenFull; ++step)
	{
		auto &ports = static_cast<astrabot::metamod::BotObservationServices &>(runtime);
		const auto full = ports.service_managedBotFullUpdateSequences()[0];
		combatAttackCommands = 0;
		globals.time += .04f; runtime.onStartFrame(); runtime.onStartFramePost();
		if (ports.service_managedBotFullUpdateSequences()[0] == full || combatAttackCommands == 0) continue;
		humanFriendlyBlocks = true;
		gEntities[2].v.flags = FL_CLIENT; gEntities[2].v.health = 100;
		const auto firingFull = ports.service_managedBotFullUpdateSequences()[0];
		combatAttackCommands = 0;
		globals.time += .04f; runtime.onStartFrame(); runtime.onStartFramePost();
		blockedBetweenFull = ports.service_managedBotFullUpdateSequences()[0] == firingFull;
		passed = check(combatAttackCommands == 0,
			"upkeep clears cached fire when a human teammate enters the shot line") && passed;
		humanFriendlyBlocks = false; gEntities[2].v.flags = 0; gEntities[2].v.health = 0;
	}
	passed = check(blockedBetweenFull, "friendly obstruction exercises a frame between full decisions") && passed;
	gEntities[1].v.velocity = Vector(0.0f, 0.0f, 0.0f);
	gEntities[0].v.weapons = (1<<18)|(1<<17);gEntities[1].v.origin=Vector(180,32,36);
	for(unsigned step=0;step<64;++step)
	{
		runtime.onMessageBegin(1,94,nullptr,&gEntities[0]);
		runtime.onWriteByte(1);runtime.onWriteByte(18);runtime.onWriteByte(10);runtime.onMessageEnd();
		globals.time+=.125f;runtime.onStartFrame();runtime.onStartFramePost();
	}
	passed=check(combatPistolSelected,"pistol switching dispatches a GameDLL weapon classname instead of a client-only slot command")&&passed;
	// Exhausted initial sampling must not suppress a new diagnostic session.
	gProfileEnabled = false;
	globals.time += 0.125f;
	runtime.onStartFrame(); runtime.onStartFramePost();
	gMovementLogs.clear();
	gProfileEnabled = true;
	for (unsigned frame = 0; frame < 8U; ++frame)
	{
		globals.time += 0.125f;
		runtime.onStartFrame(); runtime.onStartFramePost();
	}
	unsigned restartedSamples = 0U;
	bool availabilityReplayed = false;
	bool captureStarted = false;
	for (const auto &message : gMovementLogs)
	{
		if (message.find("movement physics ") == 0U) ++restartedSamples;
		availabilityReplayed = availabilityReplayed || message.find("movement unavailable ") == 0U;
		captureStarted = captureStarted || message.find("profile capture enabled=1 ") == 0U;
	}
	const bool restartPassed = check(restartedSamples >= 8U && availabilityReplayed && captureStarted,
		"off/on refreshes physical sampling and replays the availability diagnostic");
	gCaptureMovementLogs = false;
	gProfileEnabled = false;
	return passed && restartPassed;
}
}
