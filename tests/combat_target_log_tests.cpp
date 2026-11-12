#include "plugin_runtime.hpp"
#include "bot_runtime_support.hpp"

namespace
{
bool testCombatTargetLog(astrabot::metamod::PluginRuntime &, enginefuncs_t &, globalvars_t &);
}
#define ASTRABOT_EXTRA_ACTOR_TEST testCombatTargetLog
#include "compat_actor_command_tests.cpp"

namespace
{
unsigned combatAttackCommands = 0U;
const std::uint32_t *combatFrameCounter = nullptr;
std::vector<std::pair<std::uint32_t, unsigned short>> combatSentInputs;
void runCombatPlayerMove(edict_t *entity, const float *angles, float forward, float side,
	float up, unsigned short buttons, byte impulse, byte msec)
{
	runPlayerMove(entity, angles, forward, side, up, buttons, impulse, msec);
	// Feed the public engine angle result back on the next full update.
	entity->v.v_angle = Vector(angles[0], angles[1], angles[2]);
	if (entity == &gEntities[0] && (buttons & IN_ATTACK) != 0U) ++combatAttackCommands;
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
	engine.pfnTraceLine = &traceObjectiveVisibility;
	engine.pfnRunPlayerMove = &runCombatPlayerMove;
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
	const auto observe = [&](bool enemyPresent) {
		combatAttackCommands = 0U;
		combatSentInputs.clear();
		gEntities[0].v.v_angle = Vector(20.0f, 30.0f, 0.0f);
		gEntities[1].v.flags = enemyPresent ? FL_CLIENT : 0;
		gEntities[1].v.team = 2;
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
			if (!enemyPresent && !check(message.find(" target=0 ") != std::string::npos,
				"combat sensor reports no target when no enemy was acquired")) return false;
			if (message.find(enemyPresent ? " target=1 " : " target=0 ") != std::string::npos)
				found = true;
		}
		if (!check(physicalSample, "detailed physical samples continue beyond the initial log budget")) return false;
		if (!check(enemyPresent ? combatAttackCommands > 0U : combatAttackCommands == 0U,
			"engine angle feedback reaches fire with a visible enemy and holds fire without one")) return false;
		if (enemyPresent && enemyDistance > 800.0f &&
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
	bool passed = observe(false) && observe(true) && observe(false);
	enemyDistance = 900.0f;
	passed = check(static_cast<astrabot::metamod::BotObservationServices &>(runtime).
		behavior_managedProfiles(0U).skill >= 50U,
		"distant cadence fixture uses a skilled profile") && observe(true) && passed;
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
