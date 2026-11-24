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
	compat::ScriptedRandomSource empty(
		{compat::RandomTapeEntry::floatEntry("LookAround.StateInterval", {1, 1}, {}, 1, 2, 1.5f)});
	planner.begin({1, 1}, {1, 1, 1}, {9, 1}, {0, 0, 0}, 1, 10, 90, {});
	pitch = 13;
	yaw = -37;
	const bool emptyGoal = planner.update(10, false, {0, 0, 36}, 0, 0, empty, {}, &pitch, &yaw, true);
	if (!check(!emptyGoal && empty.verifyComplete() && pitch == 13 && yaw == -37,
			"compatibility zero-approach watch consumes its interval but supplies no open-ray fallback goal"))
		return false;
	compat::ScriptedRandomSource denied(
		{compat::RandomTapeEntry::floatEntry("LookAround.StateInterval", {1, 1}, {}, 1, 2, 1.5f),
			compat::RandomTapeEntry::longEntry("LookAround.ApproachPoint", {1, 1}, {}, 0, 0, 0)});
	planner.begin({1, 1}, {1, 1, 1}, {9, 1}, {}, 1, 10, 90, {{100, 0, 0}});
	bool checkedAfterDraws = false;
	const auto refused = planner.selectGoal(10, false, {0, 0, 36}, denied, {}, [&] {
		checkedAfterDraws = denied.verifyComplete();
		return false;
	});
	if (!check(checkedAfterDraws && refused == compat::GuardLookSelection::Unchanged &&
			planner.selectGoal(10.1f, false, {0, 0, 36}, denied, {}) == compat::GuardLookSelection::Unchanged && denied.verifyComplete(),
			"Guard LOW is denied after both draws and cannot reappear before a new selection"))
		return false;
	for (bool atPoint : {false, true})
	{
		std::vector<compat::RandomTapeEntry> tape{
			compat::RandomTapeEntry::floatEntry("LookAround.StateInterval", {1, 1}, {}, 1, 2, 1.5f)};
		if (atPoint) tape.push_back(compat::RandomTapeEntry::longEntry("LookAround.ApproachPoint", {1, 1}, {}, 0, 0, 0));
		compat::ScriptedRandomSource interrupted(tape);
		struct Restart final : compat::IRandomTraceSink {
			compat::GuardLookPlanner *planner = nullptr;
			bool atPoint = false, fired = false;
			void record(const compat::RandomTraceRecord &draw) override {
				if (std::strcmp(draw.request.semanticId, atPoint ? "LookAround.ApproachPoint" : "LookAround.StateInterval") != 0) return;
				fired = true;
				planner->begin({2, 1}, {1, 1, 1}, {10, 1}, {}, 1, 10, 45, {{0, 100, 0}});
			}
		} restart;
		restart.planner = &planner;
		restart.atPoint = atPoint;
		planner.begin({1, 1}, {1, 1, 1}, {9, 1}, {}, 1, 10, 0, {{100, 0, 0}});
		interrupted.setTraceSink(&restart);
		const auto aborted = planner.selectGoal(10, false, {0, 0, 36}, interrupted, {});
		compat::ScriptedRandomSource replacement({
			compat::RandomTapeEntry::floatEntry("LookAround.StateInterval", {2, 1}, {}, 1, 2, 1.5f),
			compat::RandomTapeEntry::longEntry("LookAround.ApproachPoint", {2, 1}, {}, 0, 0, 0)});
		if (!check(restart.fired && interrupted.verifyComplete() && aborted == compat::GuardLookSelection::Aborted &&
				planner.aheadYaw() == 45 && planner.selectGoal(10, false, {0, 0, 36}, replacement, {}) == compat::GuardLookSelection::Track &&
				replacement.verifyComplete() && std::fabs(planner.desiredYaw() - 90) < .01f,
				"reentrant interval/point callbacks preserve the replacement Guard goal and due deadline")) return false;
	}
	planner.reset();
	return check(
		!planner.current({1, 1}, {1, 1, 1}, {9, 1}, {0, 0, 0}, 1, 10), "leaving guard clears its retained watch");
}

bool guardSelectedPrimary = false;
std::function<void()> guardRayInterrupt;
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
	{
		result->flFraction = dy > 900 ? 1.0f : .05f;
		if (guardRayInterrupt) { auto callback = std::move(guardRayInterrupt); guardRayInterrupt = {}; callback(); }
	}
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
	unsigned activeWeapon = 22;
	const auto tick = [&]()
	{
		runtime.onMessageBegin(1, 94, nullptr, &self);
		runtime.onWriteByte(1);
		runtime.onWriteByte(activeWeapon);
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
	bool rayRestarted = false;
	guardRayInterrupt = [&] {
		auto &ports = static_cast<astrabot::metamod::BotBehaviorServices &>(runtime);
		auto &state = const_cast<astrabot::compat::CompatibilityStateMachine &>(
			static_cast<astrabot::metamod::BotNavigationServices &>(runtime).behavior_managedBotStateMachines(0));
		const auto &life = ports.service_lifecycle();
		astrabot::compat::StateUpdateContext context{};
		context.actor = state.actor();
		context.frame = {life.mapGeneration(), life.roundGeneration(), ports.service_adapterFrameCount()};
		context.timestamp = globals.time;
		context.fullUpdateSequence = ports.service_managedBotFullUpdateSequences()[0];
		context.fullUpdate = context.alive = context.roundActive = true;
		context.lifeAvailability = context.roundAvailability = context.attackObservation = astrabot::compat::ObservationAvailability::Available;
		rayRestarted = state.beginAttack(context) == astrabot::compat::StateResult::Transitioned &&
			state.stopAttack(context) == astrabot::compat::StateResult::Transitioned;
	};
	const auto movesBeforeRay = gRunPlayerMoveCount;
	tick();
	if (!check(rayRestarted && gRunPlayerMoveCount == movesBeforeRay,
			"Guard geometry callback changing Attack epoch aborts the stale Root command")) return false;
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
		unsigned startles = 0, intervals = 0, points = 0, durations = 0;
		bool interruptInterval = false;
		std::function<void()> interrupt;
		void record(const astrabot::compat::RandomTraceRecord &draw) override
		{
			if (std::strcmp(draw.request.semanticId, "NoiseLook.Duration") == 0) ++durations;
			if (std::strcmp(draw.request.semanticId, "NoiseLook.StartleInhibit") == 0)
			{
				++startles;
				if (interrupt) { auto callback = std::move(interrupt); interrupt = {}; callback(); }
			}
			if (std::strcmp(draw.request.semanticId, "LookAround.StateInterval") == 0) {
				++intervals;
				if (interruptInterval && interrupt) { auto callback = std::move(interrupt); interrupt = {}; callback(); }
			}
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
	const auto restartGuardAttack = [&] {
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
	noiseDraws.interrupt = restartGuardAttack;
	guardPorts.service_compatibilityRandomSource().setTraceSink(&noiseDraws);
	soundHooks.pfnEmitSound(&soundSource, 1, "weapons/ak47-1.wav", 1, 1, 0, 100);
	const auto beforeReentryMove = gRunPlayerMoveCount;
	tick();
	guardPorts.service_compatibilityRandomSource().setTraceSink(nullptr);
	if (!check(restartedAttack, "guard noise callback changes Attack epoch and returns to Hide")) return false;
	if (!check(gRunPlayerMoveCount == beforeReentryMove,
			"guard noise reentry discards the old command draft before RunPlayerMove")) return false;
	globals.time += 4.1f;
	restartedAttack = false;
	noiseDraws.interruptInterval = true;
	noiseDraws.interrupt = restartGuardAttack;
	const auto beforeIntervalMove = gRunPlayerMoveCount;
	guardPorts.service_compatibilityRandomSource().setTraceSink(&noiseDraws);
	tick();
	guardPorts.service_compatibilityRandomSource().setTraceSink(nullptr);
	if (!check(restartedAttack && gRunPlayerMoveCount == beforeIntervalMove,
			"Guard stationary interval callback changing Attack epoch aborts the stale Root command")) return false;
	for (bool withCandidates : {false, true})
	{
	if (withCandidates)
	{
		if (!check(writeTwoSiteNavigation(path, true, true, nullptr, true), "Guard candidate NAV writes")) return false;
		const auto candidateLoaded = runtime.loadNavigationFile(&load);
		std::remove(path);
		if (!check(candidateLoaded == astrabot::metamod::NavLoadResult::Loaded, "Guard candidate NAV loads")) return false;
		tick();
	}
	globals.time += 2.1f;
	noiseDraws.interruptInterval = false;
	noiseDraws.interrupt = {};
	const auto durationsBefore = noiseDraws.durations;
	soundSource.v.origin = Vector(coverX == 32 ? 224.0f : 32.0f, 50, 36);
	guardPorts.service_compatibilityRandomSource().setTraceSink(&noiseDraws);
	soundHooks.pfnEmitSound(&soundSource, 1, "player/pl_step1.wav", 1, 1, 0, 100);
	for (unsigned i = 0; i < 4; ++i) {
		self.v.v_angle = Vector(0, coverX == 32 ? 180.0f : 0.0f, 0); // Face away from the in-NAV sound.
		tick();
	}
	guardPorts.service_compatibilityRandomSource().setTraceSink(nullptr);
	globals.time += 2.1f;
	astrabot::runtime::BotCommand retainedNoise{};
	auto &commandPorts = static_cast<astrabot::metamod::BotCommandServices &>(runtime);
	commandPorts.behavior_applyNoiseLookCommand(0, retainedNoise, true);
	std::fprintf(stderr, "Guard ordinary control durations=%u before=%u activeView=%d now=%.6f heard=%.6f reaction=%.6f\n",
		noiseDraws.durations, durationsBefore, retainedNoise.synchronizeCombatView ? 1 : 0, globals.time,
		guardPorts.observation_managedBotPerception(0).snapshot().noise().timestampSeconds,
		static_cast<astrabot::metamod::BotNavigationServices &>(runtime).behavior_managedProfiles(0).effectiveReactionSeconds(true));
	if (!check(noiseDraws.durations > durationsBefore && retainedNoise.synchronizeCombatView,
			"Guard zero control: public ordinary footstep is retained after freshness expires")) return false;
	const auto intervalsBeforeClear = noiseDraws.intervals;
	const auto pointsBeforeAdmission = noiseDraws.points;
	gMovementLogs.clear();
	guardPorts.service_compatibilityRandomSource().setTraceSink(&noiseDraws);
	tick();
	guardPorts.service_compatibilityRandomSource().setTraceSink(nullptr);
	astrabot::runtime::BotCommand afterClear{};
	commandPorts.behavior_applyNoiseLookCommand(0, afterClear, true);
	const bool publishedLow = std::any_of(gMovementLogs.begin(), gMovementLogs.end(), [](const std::string &line) {
		return line.find("guard look slot=") != std::string::npos;
	});
	if (!check(noiseDraws.intervals == intervalsBeforeClear + 1 &&
			noiseDraws.points == pointsBeforeAdmission + (withCandidates ? 1U : 0U) &&
			afterClear.synchronizeCombatView == withCandidates && !publishedLow &&
			(gLastRunPlayerMoveButtons & IN_DUCK) != 0,
			"Guard selects before Noise admission: zero clears, positive refuses LOW and retains Noise/crouch")) return false;
	}
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
	activeWeapon = 4;
	self.v.weapons = 1 << 4;
	gEntities[1].v.origin = self.v.origin + Vector(300, 0, 128);
	for (unsigned i = 0; i < 2; ++i) {
		self.v.angles = self.v.v_angle = Vector(0, 0, 0);
		tick();
	}
	gEntities[1].v.health = 0;
	gEntities[1].v.deadflag = DEAD_DEAD;
	for (unsigned i = 0; i < 4; ++i) {
		self.v.angles = self.v.v_angle = Vector(0, 0, 0);
		tick();
	}
	auto &nativeGuardState = const_cast<astrabot::compat::CompatibilityStateMachine &>(
		static_cast<astrabot::metamod::BotNavigationServices &>(runtime).behavior_managedBotStateMachines(0));
	if (nativeGuardState.attackOverlayActive())
	{
		const auto &life = guardPorts.service_lifecycle();
		astrabot::compat::StateUpdateContext endAttack{};
		endAttack.actor = nativeGuardState.actor();
		endAttack.frame = {life.mapGeneration(), life.roundGeneration(), guardPorts.service_adapterFrameCount()};
		endAttack.timestamp = globals.time;
		endAttack.fullUpdateSequence = guardPorts.service_managedBotFullUpdateSequences()[0];
		endAttack.fullUpdate = endAttack.alive = endAttack.roundActive = true;
		endAttack.lifeAvailability = endAttack.roundAvailability = endAttack.attackObservation = astrabot::compat::ObservationAvailability::Available;
		if (!check(nativeGuardState.stopAttack(endAttack) == astrabot::compat::StateResult::Transitioned,
				"Native Guard control explicitly ends the retained Attack overlay through normal state API")) return false;
		self.v.angles = self.v.v_angle = Vector(0, 0, 0);
		tick();
	}
	std::fprintf(stderr, "Guard Native setup state=%d attack=%d weapon=%u buttons=%u pitch=%.6f now=%.6f\n",
		static_cast<int>(nativeGuardState.state()), nativeGuardState.attackOverlayActive() ? 1 : 0,
		guardPorts.observation_managedBotCurrentWeapons(0).weaponId, gLastRunPlayerMoveButtons, self.v.v_angle.x, globals.time);
	if (!check(nativeGuardState.state() == astrabot::compat::CompatibilityStateId::Hide &&
			!nativeGuardState.attackOverlayActive() && (gLastRunPlayerMoveButtons & IN_ATTACK) != 0 && self.v.v_angle.x < -1,
			"Guard Native control: grenade pin and elevated copied aim persist after hostile death outside Attack")) return false;
	globals.time += 2.0f;
	self.v.angles = self.v.v_angle = Vector(0, 0, 0);
	const auto nativeIntervals = noiseDraws.intervals, nativePoints = noiseDraws.points;
	gMovementLogs.clear();
	guardPorts.service_compatibilityRandomSource().setTraceSink(&noiseDraws);
	tick();
	guardPorts.service_compatibilityRandomSource().setTraceSink(nullptr);
	const bool nativeLow = std::any_of(gMovementLogs.begin(), gMovementLogs.end(), [](const std::string &line) {
		return line.find("guard look slot=") != std::string::npos;
	});
	std::fprintf(stderr, "Guard Native admission intervals=%u points=%u low=%d buttons=%u pitch=%.6f now=%.6f\n",
		noiseDraws.intervals - nativeIntervals, noiseDraws.points - nativePoints, nativeLow ? 1 : 0,
		gLastRunPlayerMoveButtons, self.v.v_angle.x, globals.time);
	if (!check(noiseDraws.intervals == nativeIntervals + 1 && noiseDraws.points == nativePoints + 1 &&
			!nativeLow && (gLastRunPlayerMoveButtons & IN_ATTACK) != 0 && self.v.v_angle.x < -1,
			"Guard Native spot refuses LOW after both draws and preserves pin/elevated aim")) return false;
	// Change only the approach geometry. The pending throw and its look owner
	// remain live; the new Guard selection has no candidates and must Clear.
	if (!check(writeTwoSiteNavigation(path, true, true), "Guard Native zero NAV writes")) return false;
	const auto zeroLoaded = runtime.loadNavigationFile(&load);
	std::remove(path);
	if (!check(zeroLoaded == astrabot::metamod::NavLoadResult::Loaded, "Guard Native zero NAV loads")) return false;
	// Reloading NAV invalidates the public scenario target. Resolve it before
	// placing this movement-less fixture at the newly selected hiding spot.
	tick();
	const auto &zeroScenario = static_cast<astrabot::metamod::BotNavigationServices &>(runtime).behavior_scenarioDecisions(0);
	if (!check(zeroScenario.hasTarget && zeroScenario.action == astrabot::compat::ScenarioAction::Guard,
			"Guard zero fixture resolves its new hiding target")) return false;
	// Pin lifetime is independent of look ownership. Prove the old Native
	// spot is still supplying a light update immediately before the Clear.
	const auto fullBeforeNativeControl = guardPorts.service_managedBotFullUpdateSequences()[0];
	globals.time += .034f;
	const auto delta = gEntities[1].v.origin - (self.v.origin + self.v.view_ofs);
	const auto nativeGoal = astrabot::metamod::ActionAdapter::useEntityView(delta.x, delta.y, delta.z);
	auto nativeSpring = guardPorts.command_lookSpring(0);
	const auto expectedNative = astrabot::compat::trackAim(self.v.v_angle.y, self.v.v_angle.x,
		nativeGoal.yaw + 2.0f * astrabot::metamod::ActionAdapter::csBotCosine(33.0f * globals.time),
		nativeGoal.pitch + 2.0f * astrabot::metamod::ActionAdapter::csBotCosine(13.0f * globals.time - 90.0f),
		astrabot::runtime::BotTimingScheduler::kCommandInterval, 1.0f, &nativeSpring, false);
	runtime.onStartFrame();
	runtime.onStartFramePost();
	if (!check(guardPorts.service_managedBotFullUpdateSequences()[0] == fullBeforeNativeControl &&
			std::fabs(self.v.v_angle.x - expectedNative.pitch) < .0001f &&
			std::fabs(self.v.v_angle.y - expectedNative.yaw) < .0001f,
			"Guard zero control: Native copied spot survives NAV preparation until the measured Clear")) return false;
	self.v.origin = Vector(zeroScenario.target.position.x, zeroScenario.target.position.y, 36);
	const auto zeroIntervals = noiseDraws.intervals, zeroPoints = noiseDraws.points;
	self.v.angles = self.v.v_angle = Vector(0, 0, 0);
	guardPorts.service_compatibilityRandomSource().setTraceSink(&noiseDraws);
	tick();
	guardPorts.service_compatibilityRandomSource().setTraceSink(nullptr);
	std::fprintf(stderr, "Guard Native zero intervals=%u points=%u buttons=%u state=%d now=%.6f\n",
		noiseDraws.intervals - zeroIntervals, noiseDraws.points - zeroPoints, gLastRunPlayerMoveButtons,
		static_cast<int>(nativeGuardState.state()), globals.time);
	if (!check(noiseDraws.intervals == zeroIntervals + 1 && noiseDraws.points == zeroPoints &&
			(gLastRunPlayerMoveButtons & IN_ATTACK) != 0,
			"Guard zero candidates consume only interval and preserve the independent pending pin")) return false;
	const auto fullBeforeClearUpkeep = guardPorts.service_managedBotFullUpdateSequences()[0];
	globals.time += .034f;
	auto clearSpring = guardPorts.command_lookSpring(0);
	const auto clearAhead = astrabot::compat::trackAim(self.v.v_angle.y, self.v.v_angle.x,
		90.0f + 2.0f * astrabot::metamod::ActionAdapter::csBotCosine(33.0f * globals.time),
		2.0f * astrabot::metamod::ActionAdapter::csBotCosine(13.0f * globals.time - 90.0f),
		astrabot::runtime::BotTimingScheduler::kCommandInterval, 1.0f, &clearSpring, false);
	runtime.onStartFrame();
	runtime.onStartFramePost();
	if (!check(guardPorts.service_managedBotFullUpdateSequences()[0] == fullBeforeClearUpkeep &&
			std::fabs(self.v.v_angle.x - clearAhead.pitch) < .0001f &&
			std::fabs(self.v.v_angle.y - clearAhead.yaw) < .0001f && (gLastRunPlayerMoveButtons & IN_ATTACK) != 0,
			"Guard zero Clear resumes Ahead on light upkeep without resurrecting Native/LOW or dropping the pin")) return false;
	activeWeapon = 22;

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
