#include "plugin/plugin_runtime.hpp"
#include "astrabot/compat/guard_look.hpp"
namespace
{
bool testBombGuardLook(astrabot::metamod::PluginRuntime &, enginefuncs_t &, globalvars_t &);
}
#define ASTRABOT_EXTRA_ACTOR_TEST testBombGuardLook
#include "compat_actor_command_tests.cpp"
namespace
{
bool testGuardPolicy()
{
	using namespace astrabot;
	if (!check(!compat::approachViewPositionChanged({0, 0, 0}, {30, 0, 40}) &&
			!compat::approachViewPositionChanged({170, 40, 20}, {170, 40, 69.99f}) &&
			compat::approachViewPositionChanged({170, 40, 20}, {170, 40, 70.01f}),
			"the approach scan requires strictly more than 50 units in three dimensions"))
		return false;
	nav::NavArea previous{}, here{};
	previous.extent = {{0, 0, 0}, {100, 100, 0}};
	here.extent = {{100, 20, 0}, {200, 80, 0}};
	const auto portal = compat::guardApproachPoint(previous, here, 1);
	if (!check(portal.x == 100 && portal.y == 50, "guard approach uses the portal midpoint without walking inset"))
		return false;
	compat::ScriptedRandomSource random(
		{compat::RandomTapeEntry::floatEntry("LookAround.StateInterval", {1, 1}, {}, 1, 2, 1.5f),
			compat::RandomTapeEntry::longEntry("LookAround.ApproachPoint", {1, 1}, {}, 0, 1, 1)});
	compat::GuardLookPlanner planner;
	float pitch = 0, yaw = 0;
	planner.begin({1, 1}, {1, 1, 1}, {9, 1}, {0, 0, 0}, 1, 10, 0, {{100, 0, 0}, {0, 100, 0}});
	if (!planner.update(10, false, {0, 0, 36}, pitch, yaw, random, {}, &pitch, &yaw))
		return false;
	for (unsigned i = 1; i < 10; ++i)
		if (!planner.update(10 + .1f * i, false, {0, 0, 36}, pitch, yaw, random, {}, &pitch, &yaw))
			return false;
	if (!check(random.verifyComplete() && std::fabs(yaw - 90) < .01f,
			"ordinary guard selects an approach and retains it until the 1-2 second timer"))
		return false;
	if (!check(
			!planner.current({1, 1}, {1, 2, 1}, {9, 1}, {0, 0, 0}, 1, 11), "guard watch cannot survive a round change"))
		return false;
	compat::ScriptedRandomSource slow(
		{compat::RandomTapeEntry::floatEntry("LookAround.StateInterval", {1, 1}, {}, 5, 10, 7),
			compat::RandomTapeEntry::longEntry("LookAround.ApproachPoint", {1, 1}, {}, 0, 0, 0)});
	planner.begin({1, 1}, {1, 1, 1}, {9, 1}, {0, 0, 0}, 1, 10, 0, {{100, 0, 0}});
	if (!check(planner.update(10, true, {0, 0, 36}, 0, 0, slow, {}, &pitch, &yaw) && slow.verifyComplete(),
			"sniper watch uses the reference 5-10 second timer"))
		return false;
	planner.reset();
	return check(
		!planner.current({1, 1}, {1, 1, 1}, {9, 1}, {0, 0, 0}, 1, 10), "leaving guard clears its retained watch");
}

bool guardSelectedPrimary = false;
void guardGameCommand(edict_t *entity)
{
	if (entity == &gEntities[0] && std::strcmp(astrabot::metamod::HookCommandArgv(0), "weapon_m4a1") == 0)
		guardSelectedPrimary = true;
	hookedGameClientCommand(entity);
}

void traceGuardWall(const float *start, const float *end, int flags, edict_t *self, TraceResult *result)
{
	traceObjectiveVisibility(start, end, flags, self, result);
	const float dx = end[0] - start[0], dy = end[1] - start[1];
	if (std::hypot(dx, dy) > 900)
		result->flFraction = dy > 900 ? 1.0f : .05f;
}

bool testBombGuardLook(astrabot::metamod::PluginRuntime &runtime, enginefuncs_t &engine, globalvars_t &globals)
{
	if (!testGuardPolicy())
		return false;
	const char *path = "astrabot_guard_look.nav";
	if (!writeTwoSiteNavigation(path, true, true))
		return false;
	const astrabot::metamod::NavLoadRequest load = {
		path, "astrabot_guard_look", runtime.snapshot().mapGeneration, false, 0, false, 0};
	const auto loaded = runtime.loadNavigationFile(&load);
	std::remove(path);
	if (!check(loaded == astrabot::metamod::NavLoadResult::Loaded, "guard NAV loaded"))
		return false;
	gObjectiveEntitiesAvailable = true;
	gObjectiveTraceVisible = true;
	gCaptureMovementLogs = true;
	gProfileEnabled = true;
	engine.pfnTraceLine = &traceGuardWall;
	engine.pfnRandomFloat = &siteFixtureRandomFloat;
	engine.pfnRandomLong = &siteFixtureRandomLong;
	runtime.giveEnginePointers(&engine, &globals);
	globals.mapname = 6;
	globals.maxEntities = 8;
	for (unsigned i = 1; i < 5; ++i)
	{
		gEntities[i].v.health = 0;
		gEntities[i].v.flags = 0;
	}
	auto &self = gEntities[0];
	self.v.flags = FL_CLIENT | FL_FAKECLIENT | FL_ONGROUND;
	self.v.health = 100;
	self.v.team = 1;
	self.v.solid = SOLID_SLIDEBOX;
	self.v.movetype = MOVETYPE_WALK;
	self.v.mins = Vector(-16, -16, -36);
	self.v.maxs = Vector(16, 16, 36);
	self.v.maxspeed = 240;
	self.v.origin = Vector(80, 32, 36);
	self.v.view_ofs = Vector(0, 0, 17);
	self.v.v_angle = Vector(0, 180, 0);
	self.v.weapons = 1 << 22;
	gPlantedBomb = {};
	gPlantedBomb.v.classname = 2;
	gPlantedBomb.v.model = 3;
	gPlantedBomb.serialnumber = 5;
	gPlantedBomb.v.origin = Vector(224, 32, 36);
	gPlantedBomb.v.dmgtime = globals.time + 100;
	runtime.onMessageBegin(MSG_ALL, 98, nullptr, nullptr);
	runtime.onWriteByte(4);
	runtime.onWriteString("#Bomb_Planted");
	runtime.onMessageEnd();
	const auto tick = [&]()
	{
		runtime.onMessageBegin(1, 94, nullptr, &self);
		runtime.onWriteByte(1);
		runtime.onWriteByte(22);
		runtime.onWriteByte(30);
		runtime.onMessageEnd();
		self.v.absmin = self.v.origin + self.v.mins;
		self.v.absmax = self.v.origin + self.v.maxs;
		globals.time += .1f;
		runtime.onStartFrame();
		runtime.onStartFramePost();
	};
	gMovementLogs.clear();
	for (unsigned i = 0; i < 24; ++i)
		tick();
	float coverX = 0;
	for (const auto &s : gMovementLogs)
		if (s.find("state=STATE-HIDE") != std::string::npos)
		{
			if (s.find("goal_position=(32.0 50.0 0.0)") != std::string::npos)
				coverX = 32;
			if (s.find("goal_position=(224.0 50.0 0.0)") != std::string::npos)
				coverX = 224;
		}
	if (!check(coverX != 0, "planted bomb chooses cover"))
		return false;
	self.v.origin = Vector(coverX, 50, 36);
	self.v.v_angle = Vector(0, 180, 0);
	bool sharedGuardTrajectory = true;
	for (unsigned i = 0; i < 24; ++i)
	{
		const float now = globals.time + .1f;
		auto spring = static_cast<astrabot::metamod::BotBehaviorServices &>(runtime).command_lookSpring(0);
		const auto expected = astrabot::compat::trackAim(self.v.v_angle.y, self.v.v_angle.x,
			90.0f + 2.0f * astrabot::metamod::ActionAdapter::csBotCosine(33.0f * now),
			2.0f * astrabot::metamod::ActionAdapter::csBotCosine(13.0f * now - 90.0f),
			astrabot::runtime::BotTimingScheduler::kCommandInterval, 1.0f, &spring, false);
		tick();
		if (i >= 16 && (std::fabs(self.v.v_angle.y - expected.yaw) > .0001f ||
			std::fabs(self.v.v_angle.x - expected.pitch) > .0001f))
		{
			std::fprintf(stderr, "Guard upkeep %.6f expected=(%.9f %.9f) actual=(%.9f %.9f)\n",
				now, expected.yaw, expected.pitch, self.v.v_angle.y, self.v.v_angle.x);
			sharedGuardTrajectory = false;
		}
	}
	bool passed =
		check(sharedGuardTrajectory, "guard uses one shared native spring step toward the open ray plus drift on every upkeep") &&
		check(gLastRunPlayerMoveForward == 0 && gLastRunPlayerMoveSide == 0, "guard watches without leaving cover");
	struct NoiseOrderDraws final : astrabot::compat::IRandomTraceSink
	{
		unsigned startles = 0, intervals = 0, points = 0;
		std::function<void()> interrupt;
		void record(const astrabot::compat::RandomTraceRecord &draw) override
		{
			if (std::strcmp(draw.request.semanticId, "NoiseLook.StartleInhibit") == 0)
			{
				++startles;
				if (interrupt) { auto callback = std::move(interrupt); interrupt = {}; callback(); }
			}
			if (std::strcmp(draw.request.semanticId, "LookAround.StateInterval") == 0) ++intervals;
			if (std::strcmp(draw.request.semanticId, "LookAround.ApproachPoint") == 0) ++points;
		}
	} noiseDraws;
	enginefuncs_t soundHooks{};
	int soundVersion = ENGINE_INTERFACE_VERSION;
	if (!check(runtime.provideEngineFunctions(&soundHooks, &soundVersion) && soundHooks.pfnEmitSound,
			"guard noise order uses the public sound callback")) return false;
	// The stationary deadline is due before the sound, with no intervening
	// update that could spend its interval draw.
	globals.time += 2.1f;
	auto &soundSource = gEntities[4];
	soundSource = {};
	soundSource.serialnumber = 44;
	soundSource.v.origin = self.v.origin + Vector(0, 300, 0);
	auto &guardPorts = static_cast<astrabot::metamod::BotBehaviorServices &>(runtime);
	guardPorts.service_compatibilityRandomSource().setTraceSink(&noiseDraws);
	soundHooks.pfnEmitSound(&soundSource, 1, "weapons/ak47-1.wav", 1, 1, 0, 100);
	tick();
	guardPorts.service_compatibilityRandomSource().setTraceSink(nullptr);
	std::fprintf(stderr, "guard noise order startle=%u interval=%u point=%u buttons=%u\n",
		noiseDraws.startles, noiseDraws.intervals, noiseDraws.points, gLastRunPlayerMoveButtons);
	if (!check(noiseDraws.startles == 1 && noiseDraws.intervals == 0 && noiseDraws.points == 0,
			"guard handles fresh dangerous noise before stationary lookaround draws")) return false;
	if (!check((gLastRunPlayerMoveButtons & IN_DUCK) != 0 && gLastRunPlayerMoveForward == 0 && gLastRunPlayerMoveSide == 0,
			"handled noise leaves Hide crouch and holding-position behavior intact")) return false;
	globals.time += 4.1f;
	bool restartedAttack = false;
	noiseDraws.interrupt = [&] {
		auto &machine = const_cast<astrabot::compat::CompatibilityStateMachine &>(
			static_cast<astrabot::metamod::BotNavigationServices &>(runtime).behavior_managedBotStateMachines(0));
		const auto &life = guardPorts.service_lifecycle();
		const auto &handle = guardPorts.service_managedBotHandles()[0];
		astrabot::compat::StateUpdateContext context{};
		context.actor = {handle.actor.slot, handle.actor.actorGeneration};
		context.frame = {life.mapGeneration(), life.roundGeneration(), guardPorts.service_adapterFrameCount()};
		context.timestamp = globals.time;
		context.fullUpdateSequence = guardPorts.service_managedBotFullUpdateSequences()[0];
		context.fullUpdate = context.alive = context.roundActive = true;
		context.lifeAvailability = context.roundAvailability = context.attackObservation = astrabot::compat::ObservationAvailability::Available;
		restartedAttack = machine.beginAttack(context) == astrabot::compat::StateResult::Transitioned &&
			machine.stopAttack(context) == astrabot::compat::StateResult::Transitioned;
	};
	guardPorts.service_compatibilityRandomSource().setTraceSink(&noiseDraws);
	soundHooks.pfnEmitSound(&soundSource, 1, "weapons/ak47-1.wav", 1, 1, 0, 100);
	const auto beforeReentryMove = gRunPlayerMoveCount;
	tick();
	guardPorts.service_compatibilityRandomSource().setTraceSink(nullptr);
	if (!check(restartedAttack, "guard noise callback changes Attack epoch and returns to Hide")) return false;
	if (!check(gRunPlayerMoveCount == beforeReentryMove,
			"guard noise reentry discards the old command draft before RunPlayerMove")) return false;
	gHookedGameDllTable.pfnClientCommand = &guardGameCommand;
	self.v.weapons = (1 << 22) | (1 << 6);
	for (unsigned i = 0; i < 8; ++i)
	{
		runtime.onMessageBegin(1, 94, nullptr, &self);
		runtime.onWriteByte(1);
		runtime.onWriteByte(6);
		runtime.onWriteByte(1);
		runtime.onMessageEnd();
		globals.time += .1f;
		runtime.onStartFrame();
		runtime.onStartFramePost();
	}
	passed =
		check(guardSelectedPrimary, "post-plant guard equips a usable gun through its GameDLL classname") && passed;
	gEntities[1].v.flags = FL_CLIENT;
	gEntities[1].v.team = 0;
	gEntities[1].v.health = 100;
	gEntities[1].v.deadflag = DEAD_NO;
	gEntities[1].v.origin = self.v.origin + Vector(0, 100, 0);
	runtime.onMessageBegin(MSG_ALL, 86, nullptr, nullptr);
	runtime.onWriteByte(2);
	runtime.onWriteString("CT");
	runtime.onMessageEnd();
	bool attacked = false;
	for (unsigned i = 0; i < 32; ++i)
	{
		tick();
		attacked = attacked || (gLastRunPlayerMoveButtons & IN_ATTACK) != 0;
	}
	if (!check(attacked, "a visible human enemy interrupts post-plant guard and receives attack input") || !passed)
		return false;

	using namespace astrabot;
	gEntities[1].v.health = 0.0f;
	gEntities[1].v.deadflag = DEAD_DEAD;
	self.v.weapons = 1 << 22;
	self.v.origin = Vector(80, 32, 36);
	runtime.onMessageBegin(MSG_ALL, 86, nullptr, nullptr);
	runtime.onWriteByte(1);
	runtime.onWriteString("TERRORIST");
	runtime.onMessageEnd();
	gPlantedBomb.free = true;
	runtime.onRoundLifecycleMessage("World triggered \"Round_End\"\n");
	runtime.onRoundLifecycleMessage("World triggered \"Round_Start\"\n");
	const auto &navigation = static_cast<metamod::BotNavigationServices &>(runtime);
	const auto &machine = navigation.behavior_managedBotStateMachines(0);
	for (int i = 0; i < 32 && machine.state() != compat::CompatibilityStateId::Hunt; ++i)
		tick();
	if (!check(machine.state() == compat::CompatibilityStateId::Hunt, "the recovery fixture starts from Hunt"))
		return false;

	gPlantedBomb.free = false;
	++gPlantedBomb.serialnumber;
	gPlantedBomb.v.classname = 4;
	gPlantedBomb.v.model = 5;
	gPlantedBomb.v.origin = Vector(224, 32, 2);
	runtime.onMessageBegin(MSG_ONE, 95, nullptr, &self);
	runtime.onWriteCoord(180);
	runtime.onWriteCoord(32);
	runtime.onWriteCoord(2);
	runtime.onWriteByte(0);
	runtime.onMessageEnd();
	for (int i = 0; i < 8 && machine.state() != compat::CompatibilityStateId::FetchBomb; ++i)
	{
		self.v.angles = self.v.v_angle = Vector(0, 180, 0);
		tick();
	}
	if (!check(machine.state() == compat::CompatibilityStateId::FetchBomb &&
			machine.task() == compat::CompatibilityTaskId::SeekAndDestroy &&
			gLastRunPlayerMoveForward == 0.0f && gLastRunPlayerMoveSide == 0.0f,
		"the targeted T radar notice enters FetchBomb behind the observer with neutral entry movement"))
		return false;

	// Let any previous Hunt attempt expire without another full update.
	globals.time += 0.65f;
	gMovementLogs.clear();
	tick();
	const auto &movement = static_cast<metamod::BotCommandServices &>(runtime);
	const auto fetchPath = movement.navigation_managedNavigationSteps(0).pathSequence;
	bool usedBoxOrigin = false;
	for (const auto &line : gMovementLogs)
		if (line.find("STATE-FETCH") != std::string::npos &&
			line.find("goal_position=(224.0 32.0 2.0)") != std::string::npos)
			usedBoxOrigin = true;
	if (!check(machine.state() == compat::CompatibilityStateId::FetchBomb && fetchPath != 0U &&
			usedBoxOrigin && (gLastRunPlayerMoveForward != 0.0f || gLastRunPlayerMoveSide != 0.0f),
		"FetchBomb plans and moves toward the current box origin when the radar coordinate differs"))
		return false;

	gPlantedBomb.v.origin.x = 248;
	for (int i = 0; i < 3; ++i)
		tick();
	if (!check(machine.state() == compat::CompatibilityStateId::FetchBomb &&
			movement.navigation_managedNavigationSteps(0).pathSequence == fetchPath,
		"the same FetchBomb path survives a change in the loose box position"))
	{
		for (const auto &line : gMovementLogs)
			if (line.find("navigationStep") != std::string::npos)
				std::fprintf(stderr, "%s\n", line.c_str());
		return false;
	}

	gPlantedBomb.free = true;
	for (int i = 0; i < 3 && machine.state() == compat::CompatibilityStateId::FetchBomb; ++i)
		tick();
	return check(machine.state() == compat::CompatibilityStateId::Idle &&
			gLastRunPlayerMoveForward == 0.0f && gLastRunPlayerMoveSide == 0.0f,
		"another player's pickup returns FetchBomb to Idle with neutral movement");
}
}
