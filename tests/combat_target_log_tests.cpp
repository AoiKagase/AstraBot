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
int combatUserMessageId(plid_t id, const char *name, int *size)
{
	if (std::strcmp(name, "DeathMsg") == 0)
		return 103;
	if (std::strcmp(name, "ScreenFade") == 0)
		return 104;
	return siteFixtureUserMessageId(id, name, size);
}

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
float forcedNoticeFixtureRoll = -1.0f;
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
	const auto stepRifle = [&](int clip, float yaw = 0.0f)
	{
		runtime.onMessageBegin(1, 94, nullptr, &gEntities[0]);
		runtime.onWriteByte(1);
		runtime.onWriteByte(22);
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
#ifdef _MSC_VER
	std::free(scenario);
#endif
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
							 float height = 0.0f, bool damaged = false) {
			enemyBehindCover = false;
			enemy.v.origin = gEntities[0].v.origin + Vector(range, 0, height);
			enemy.v.v_angle = angles;
			enemy.v.punchangle = punch;
			runtime.beginRoundGeneration();
			stepRifle(30);
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
			stepRifle(30);
			if (damaged)
				behavior->observePlayerDamage(0, context.actor,
					{life.mapGeneration(), life.roundGeneration(), ports.service_adapterFrameCount()}, &enemy,
					enemy.serialnumber, {2U, life.slotGeneration(2U)});
			SelfDefenseTrace trace{eyeVisible, replaced};
			selfDefenseTrace = &trace;
			const auto serial = enemy.serialnumber;
			const auto deadlineDraws = attackDeadlineDraws;
			const auto action = behavior->decideManagedBotAction(0, before, {}, 0);
			selfDefenseTrace = nullptr;
			enemyBehindCover = false;
			enemy.serialnumber = serial;
			if (state.attackOverlayActive() != expectedAttack || trace.eyeChecks != expectedChecks)
				std::fprintf(stderr, "self-defense range=%.1f overlay=%d eye=%u expected=%d/%u\n",
					range, state.attackOverlayActive(), trace.eyeChecks, expectedAttack, expectedChecks);
			return check(state.attackOverlayActive() == expectedAttack && action.hasCombatAim == expectedAttack &&
					trace.eyeChecks == expectedChecks &&
					(replaced || behavior->currentEnemySource(0).id == 2U) &&
					(expectedAttack || attackDeadlineDraws == deadlineDraws) &&
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
	if (reacquireFixture)
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
