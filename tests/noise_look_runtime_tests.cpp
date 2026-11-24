#include "plugin/plugin_runtime.hpp"
namespace
{
bool testNoiseLook(astrabot::metamod::PluginRuntime &, enginefuncs_t &, globalvars_t &);
}
#define ASTRABOT_EXTRA_ACTOR_TEST testNoiseLook
#define ASTRABOT_ACTOR_PROFILE_TEXT                                                                                    \
	"Any { Name = \"Noise Bot\" Team = any Difficulty = 1 Skill = 70 ReactionTime = 0.125 }\n"
#include "compat_actor_command_tests.cpp"
#include "fixtures/noise_runtime_nav.hpp"
#include "runtime/behavior/bot_behavior_runtime.hpp"
namespace
{
bool noiseRayBlocked = false;
bool noiseApproachVisible = false;
bool noiseEnemyVisible = false;
unsigned noiseApproachRays = 0;
unsigned noiseGlassIgnoringRays = 0;
void noiseLookTrace(const float *start, const float *end, int flags, edict_t *, TraceResult *trace)
{
	*trace = {};
	trace->flFraction = noiseRayBlocked && !(noiseApproachVisible && flags != 0x101) ? 0.0f : 1.0f;
	if (noiseEnemyVisible && flags == 0x101 && end[1] > 100.0f)
		trace->flFraction = 1.0f;
	if (noiseApproachVisible && flags == 1 && std::fabs(start[0] - 32) < .01f && std::fabs(start[1] - 32) < .01f &&
		std::fabs(end[0] - 192) < .01f && std::fabs(end[1] - 32) < .01f && std::fabs(end[2] - 36) < .01f)
		++noiseApproachRays;
	trace->vecEndPos = Vector(end[0], end[1], end[2]);
	if (flags == 0x101)
		++noiseGlassIgnoringRays;
}
bool writeNoiseApproachNav(const char *path)
{
	std::vector<std::uint8_t> bytes;
	const auto u32 = [&](std::uint32_t value)
	{
		for (unsigned shift = 0; shift < 32; shift += 8)
			bytes.push_back(static_cast<std::uint8_t>(value >> shift));
	};
	const auto f32 = [&](float value)
	{
		std::uint32_t bits = 0;
		std::memcpy(&bits, &value, sizeof(bits));
		u32(bits);
	};
	u32(0xFEEDFACEU);
	u32(1);
	u32(4);
	for (unsigned id = 1; id <= 4; ++id)
	{
		u32(id);
		bytes.push_back(0);
		const float low = static_cast<float>(id - 1) * 64.0f;
		for (float value : {low, 0.0f, 0.0f, low + 64.0f, 64.0f, 0.0f, 0.0f, 0.0f})
			f32(value);
		for (unsigned direction = 0; direction < 4; ++direction)
		{
			const bool east = direction == 1 && id < 4, west = direction == 3 && id > 1;
			u32(east || west ? 1U : 0U);
			if (east || west)
				u32(east ? id + 1 : id - 1);
		}
		bytes.push_back(0);
		bytes.push_back(id == 1 ? 1 : 0);
		if (id == 1)
		{
			u32(4);
			u32(3);
			bytes.push_back(1);
			u32(0);
			bytes.push_back(0);
		}
		u32(0);
	}
	auto *file = openFixture(path, "wb");
	if (!file)
		return false;
	const auto written = std::fwrite(bytes.data(), 1, bytes.size(), file);
	const int closed = std::fclose(file);
	return written == bytes.size() && closed == 0;
}
bool testNoiseLook(astrabot::metamod::PluginRuntime &runtime, enginefuncs_t &engine, globalvars_t &globals)
{
	if (!check(loadHearingNav(runtime), "hearing fixture supplies real NAV"))
		return false;
	engine.pfnRandomFloat = &hearingFixtureRandomFloat;
	engine.pfnTraceLine = &noiseLookTrace;
	runtime.giveEnginePointers(&engine, &globals);
	gObjectiveEntitiesAvailable = false;
	for (unsigned i = 1; i < 4; ++i)
	{
		gEntities[i].v.deadflag = DEAD_DEAD;
		gEntities[i].v.health = 0;
	}
	auto &self = gEntities[0];
	self.v.flags = FL_CLIENT | FL_FAKECLIENT | FL_ONGROUND;
	self.v.health = 100;
	self.v.deadflag = DEAD_NO;
	self.v.origin = Vector(0, 0, 36);
	self.v.view_ofs = Vector(0, 0, 17);
	self.v.v_angle = Vector(0, 0, 0);
	self.v.angles = self.v.v_angle;
	self.v.fov = 90;
	self.v.weapons = 1 << 22;
	auto &source = gEntities[4];
	source = {};
	source.serialnumber = 44;
	source.v.origin = Vector(0, 300, 0);
	enginefuncs_t hooks{};
	int version = ENGINE_INTERFACE_VERSION;
	if (!check(runtime.provideEngineFunctions(&hooks, &version) && hooks.pfnEmitSound,
			"test uses the actual public EmitSound callback"))
		return false;
	const auto emit = [&](const char *sample)
	{
		hooks.pfnEmitSound(&source, 1, sample, 1.0f, 1.0f, 0, 100);
	};
	const auto step = [&](float dt)
	{
		globals.time += dt;
		runtime.onStartFrame();
		runtime.onStartFramePost();
	};
	const auto round = [&]()
	{
		runtime.beginRoundGeneration();
		self.v.v_angle = Vector(0, 0, 0);
		self.v.angles = self.v.v_angle;
	};
	round();
	step(.12f);
	hooks.pfnEmitSound(&source, 1, "player/pl_step1.wav", 1.0f, 1.0f, 1 << 5, 100);
	step(.2f);
	if (!check(std::fabs(self.v.v_angle.y) < .01f, "stopping a sound is not a fresh auditory event"))
		return false;
	round();
	step(.12f);
	emit("player/pl_step1.wav");
	step(.08f);
	if (!check(std::fabs(self.v.v_angle.y) < .01f, "ordinary heard noise waits for profile reaction time"))
		return false;
	step(.08f);
	if (!check(self.v.v_angle.y > 0 && self.v.v_angle.y < 90 &&
				(gLastRunPlayerMoveButtons & (IN_ATTACK | IN_ATTACK2)) == 0,
			"recent heard sound smoothly turns RunPlayerMove view without inventing fire"))
		return false;
	const float firstYaw = self.v.v_angle.y;
	step(.04f);
	if (!check(self.v.v_angle.y > firstYaw && self.v.v_angle.y - firstYaw < 12,
			"view continues at command cadence between Full Updates"))
		return false;
	astrabot::runtime::BotCommand projected{};
	projected.viewAngles = {0, 0, 0};
	projected.movement = {120, 15, 0, 0, 0, 33};
	auto &commands = static_cast<astrabot::metamod::BotCommandServices &>(runtime);
	commands.behavior_applyNoiseLookCommand(0, projected);
	const float radians = projected.viewAngles.yaw * 0.017453292519943295f;
	const float worldX = projected.movement.forward * std::cos(radians) + projected.movement.side * std::sin(radians);
	const float worldY = projected.movement.forward * std::sin(radians) - projected.movement.side * std::cos(radians);
	if (!check(projected.synchronizeCombatView && std::fabs(worldX - 120) < .001f && std::fabs(worldY + 15) < .001f,
			"auditory view change preserves the supplied world movement direction"))
		return false;
	astrabot::runtime::BotCommand usingEntity{};
	usingEntity.viewAngles = {12, -30, 0};
	usingEntity.movement.buttons = IN_USE;
	commands.behavior_applyNoiseLookCommand(0, usingEntity);
	if (!check(usingEntity.viewAngles.pitch == 12 && usingEntity.viewAngles.yaw == -30,
			"objective USE retains its view ownership"))
		return false;
	if (!check(noiseGlassIgnoringRays > 0, "direct auditory look ignores glass without applying visual FOV"))
		return false;
	for (unsigned retainedCase = 0; retainedCase < 3; ++retainedCase)
	{
		using namespace astrabot;
		if (retainedCase != 0)
		{
			if (!check(retainedCase == 1 ? loadInvestigationNav(runtime, true) : loadMovingEncounterNav(runtime),
					"retained control: real approach or encounter NAV loads")) return false;
			round();
			globals.time += 25.0f; // Beyond the safe movement interval.
			self.v.origin = retainedCase == 1 ? Vector(0, 64, 36) : Vector(200, 32, 36);
			source.v.origin = self.v.origin + (retainedCase == 1 ? Vector(0, 300, 0) : Vector(300, 0, 0));
			step(.12f);
			emit("player/pl_step1.wav");
			step(.2f);
		}
		auto &ports = static_cast<metamod::BotBehaviorServices &>(runtime);
		metamod::BotBehaviorRuntime retained;
		retained.bindServices(ports);
		const auto &handle = ports.service_managedBotHandles()[0];
		retained.onActorRemembered(0, handle);
		retained.assignProfile(0, static_cast<metamod::BotNavigationServices &>(runtime).behavior_managedProfiles(0));
		compat::StateUpdateContext context{};
		context.actor = {handle.actor.slot, handle.actor.actorGeneration};
		context.frame = {ports.service_lifecycle().mapGeneration(), ports.service_lifecycle().roundGeneration(), ports.service_adapterFrameCount()};
		context.timestamp = globals.time;
		context.fullUpdateSequence = ports.service_managedBotFullUpdateSequences()[0];
		context.fullUpdate = context.alive = context.roundActive = true;
		context.lifeAvailability = context.roundAvailability = context.attackObservation = compat::ObservationAvailability::Available;
		auto &state = const_cast<compat::CompatibilityStateMachine &>(retained.managedBotStateMachines(0));
		state.initialize(context);
		state.transition(compat::CompatibilityStateId::Hunt, compat::StateTransitionReason::ExplicitRequest,
			compat::StateTransitionId::ExplicitStateChange, context);
		runtime::BotCommand heardCommand{};
		retained.applyNoiseLookCommand(0, heardCommand, false);
		if (!check(heardCommand.synchronizeCombatView, "retained control: public footstep supplies a local auditory look")) {
			std::fprintf(stderr, "retained case=%u now=%.6f noise=%.6f\n", retainedCase, globals.time,
				ports.observation_managedBotPerception(0).snapshot().noise().timestampSeconds);
			return false;
		}
		globals.time += .4f; // Older than fresh-noise window, inside retained look lifetime.
		struct Draws final : compat::IRandomTraceSink {
			unsigned intervals = 0, points = 0, encounters = 0;
			void record(const compat::RandomTraceRecord &draw) override {
				if (std::strcmp(draw.request.semanticId, "LookAround.StateInterval") == 0) ++intervals;
				if (std::strcmp(draw.request.semanticId, "LookAround.ApproachPoint") == 0) ++points;
				if (std::strcmp(draw.request.semanticId, "LookAround.EncounterInterval") == 0 ||
					std::strcmp(draw.request.semanticId, "LookAround.EncounterSpot") == 0) ++encounters;
			}
		} draws;
		runtime::MovementPhysicsState stationary{};
		stationary.flags = self.v.flags;
		stationary.origin = {self.v.origin.x, self.v.origin.y, self.v.origin.z};
		const auto nav = ports.service_navPublisher().snapshot();
		if (!check(retained.refreshHuntApproaches(0, stationary, nav), "retained control: approach refresh completes")) return false;
		nav::NavHuntEncounter path{};
		if (retainedCase == 2)
		{
			stationary.velocity = {20, 0, 0};
			const auto *middle = nav.document()->findArea(2);
			if (!check(middle && middle->encounters.size() == 1 && nav.findHidingSpot(42), "retained control: valid encounter and spot")) return false;
			path = {middle, &middle->encounters.front()};
		}
		metamod::HuntApproachLook look{};
		ports.service_compatibilityRandomSource().setTraceSink(&draws);
		const bool completed = retained.updateHuntApproachLook(0, stationary, nav, path, false, [] { return true; }, look);
		ports.service_compatibilityRandomSource().setTraceSink(nullptr);
		if (!check(completed && draws.intervals == (retainedCase == 2 ? 0U : 1U) &&
				draws.points == (retainedCase == 1 ? 1U : 0U) && draws.encounters == 0 &&
				look.update == metamod::HuntApproachLookUpdate::Clear,
				"retained noise selects stationary LOW before denial but blocks moving selection")) return false;
		runtime::BotCommand cleared{};
		retained.applyNoiseLookCommand(0, cleared, true);
		if (!check(cleared.synchronizeCombatView == (retainedCase != 0),
				"only zero-approach Clear retires retained auditory view; denied LOW preserves it")) return false;
		if (retainedCase != 0)
		{
			metamod::BotBehaviorRuntime ordinary;
			ordinary.bindServices(ports);
			ordinary.onActorRemembered(0, handle);
			ordinary.assignProfile(0, static_cast<metamod::BotNavigationServices &>(runtime).behavior_managedProfiles(0));
			context.timestamp = globals.time;
			auto &ordinaryState = const_cast<compat::CompatibilityStateMachine &>(ordinary.managedBotStateMachines(0));
			ordinaryState.initialize(context);
			ordinaryState.transition(compat::CompatibilityStateId::Hunt, compat::StateTransitionReason::ExplicitRequest,
				compat::StateTransitionId::ExplicitStateChange, context);
			if (!check(ordinary.refreshHuntApproaches(0, stationary, nav), "ordinary control: approaches refresh")) return false;
			draws = {};
			metamod::HuntApproachLook accepted{};
			ports.service_compatibilityRandomSource().setTraceSink(&draws);
			const bool ordinaryCompleted = ordinary.updateHuntApproachLook(0, stationary, nav, path, false, [] { return true; }, accepted);
			ports.service_compatibilityRandomSource().setTraceSink(nullptr);
			if (!check(ordinaryCompleted && (retainedCase == 1
					? draws.intervals == 1 && draws.points == 1 && accepted.update == metamod::HuntApproachLookUpdate::Track
					: draws.encounters == 2 && accepted.update == metamod::HuntApproachLookUpdate::TrackEncounter),
					"without retained Noise the same stationary/moving route selects and adopts LOW")) return false;
		}
	}
	if (!check(loadHearingNav(runtime), "restore simple hearing NAV after retained controls")) return false;
	self.v.origin = Vector(0, 0, 36);
	source.v.origin = Vector(0, 300, 0);
	round();
	noiseRayBlocked = true;
	emit("player/pl_step1.wav");
	step(.2f);
	if (!check(std::fabs(self.v.v_angle.y) < .01f,
			"blocked sound without a NAV approach does not fabricate a visible aim point"))
		return false;
	noiseRayBlocked = false;
	round();
	self.v.flags |= FL_FROZEN;
	emit("player/pl_step1.wav");
	step(.2f);
	if (!check(std::fabs(self.v.v_angle.y) < .01f, "frozen command rejects auditory look"))
		return false;
	self.v.flags &= ~FL_FROZEN;
	round();
	emit("player/pl_step1.wav");
	step(.75f);
	if (!check(std::fabs(self.v.v_angle.y) < .01f,
			"low-FPS delayed delivery cannot renew a sound older than half a second"))
		return false;
	round();
	step(.12f);
	emit("weapons/ak47_clipin.wav");
	emit("weapons/ak47-1.wav");
	step(.12f);
	if (!check(self.v.v_angle.y > 0, "same-frame reload cannot suppress the immediate high-priority shot startle"))
		return false;
	// A real visible hostile takes over from an auditory look on the next update.
	runtime.onMessageBegin(MSG_ONE, 94, nullptr, &self);
	runtime.onWriteByte(1);
	runtime.onWriteByte(22);
	runtime.onWriteByte(30);
	runtime.onMessageEnd();
	auto &enemy = gEntities[1];
	enemy.serialnumber = 2;
	enemy.v.flags = FL_CLIENT | FL_ONGROUND;
	enemy.v.health = 100;
	enemy.v.deadflag = DEAD_NO;
	enemy.v.team = 2;
	enemy.v.origin = Vector(300, 0, 36);
	enemy.v.velocity = self.v.velocity = Vector(0, 0, 0);
	self.v.v_angle = Vector(0, 0, 0);
	self.v.angles = self.v.v_angle;
	{
		auto &views = static_cast<astrabot::metamod::BotBehaviorServices &>(runtime);
		auto &navigation = static_cast<astrabot::metamod::BotNavigationServices &>(runtime);
		for (unsigned i = 0; i < 32 && !navigation.behavior_managedBotStateMachines(0).attackOverlayActive(); ++i)
			step(.08f);
		const auto &belief = views.observation_managedBotPerception(0).snapshot();
		const astrabot::world::ActorObservation *visibleEnemy = nullptr;
		for (std::size_t i = 0; i < belief.actorCount(); ++i)
			if (belief.actorAt(i)->visible && belief.actorAt(i)->relation == astrabot::world::TeamRelation::Hostile)
				visibleEnemy = belief.actorAt(i);
		if (!check(visibleEnemy && navigation.behavior_managedBotStateMachines(0).attackOverlayActive(),
				"a real visible hostile is recognized and accepted by Attack before checking look precedence"))
			return false;
		const auto combatView = self.v.v_angle;
		auto enemySpring = views.command_lookSpring(0), sideNoiseSpring = enemySpring;
		const auto enemyPoint = astrabot::compat::combatAimPoint({enemy.v.origin.x, enemy.v.origin.y, enemy.v.origin.z},
			{self.v.origin.x, self.v.origin.y, self.v.origin.z}, visibleEnemy->visibleParts, 22,
			static_cast<float>(navigation.behavior_managedProfiles(0).skill) / 100.0f,
			(self.v.flags & FL_DUCKING) ? 18.0f : 36.0f, false, 0);
		const auto enemyGoal = astrabot::metamod::ActionAdapter::useEntityView(enemyPoint.x - self.v.origin.x,
			enemyPoint.y - self.v.origin.y, enemyPoint.z - self.v.origin.z);
		const auto expectedEnemy = astrabot::compat::trackAim(combatView.y, combatView.x, enemyGoal.yaw, enemyGoal.pitch,
			astrabot::runtime::BotTimingScheduler::kCommandInterval, 1, &enemySpring, true);
		const auto wrongNoise = astrabot::compat::trackAim(combatView.y, combatView.x, 90, 0,
			astrabot::runtime::BotTimingScheduler::kCommandInterval, 1, &sideNoiseSpring, true);
		const auto combatFull = views.service_managedBotFullUpdateSequences()[0];
		const auto combatCommand = views.command_managedBotCommandSequences(0);
		step(.04f);
		const auto selectedSpring = views.command_lookSpring(0);
		if (!check(views.service_managedBotFullUpdateSequences()[0] == combatFull &&
				views.command_managedBotCommandSequences(0) > combatCommand &&
				std::fabs(gLastRunPlayerMoveYaw - expectedEnemy.yaw) < .0001f &&
				std::fabs(self.v.v_angle.y - expectedEnemy.yaw) < .0001f &&
				std::fabs(self.v.v_angle.x - expectedEnemy.pitch) < .0001f &&
				std::fabs(selectedSpring.yawVelocity - enemySpring.yawVelocity) < .0001f &&
				std::fabs(selectedSpring.pitchVelocity - enemySpring.pitchVelocity) < .0001f &&
				std::fabs(astrabot::compat::normalizedAimAngle(wrongNoise.yaw - expectedEnemy.yaw)) > .001f,
				"visible enemy owns the cached shared spring even while inherited noise momentum is still nonzero"))
		{
			std::fprintf(stderr, "enemy shared aim expected=(%.9f %.9f) actual=(%.9f %.9f) spring_expected=(%.9f %.9f) spring_actual=(%.9f %.9f)\n",
				expectedEnemy.yaw, expectedEnemy.pitch, self.v.v_angle.y, self.v.v_angle.x,
				enemySpring.yawVelocity, enemySpring.pitchVelocity, selectedSpring.yawVelocity, selectedSpring.pitchVelocity);
			return false;
		}
	}
	enemy.v.deadflag = DEAD_DEAD;
	enemy.v.health = 0;
	round();
	step(.12f);
	emit("weapons/c4_beep1.wav");
	step(.2f);
	if (!check(std::fabs(self.v.v_angle.y) < .01f, "objective beep is not classified as an enemy sound look"))
		return false;
	round();
	step(.12f);
	emit("player/pl_step1.wav");
	step(.2f);
	const auto beforeRound = globals.time;
	round();
	step(.04f);
	if (!check(globals.time > beforeRound && std::fabs(self.v.v_angle.y) < .01f,
			"round reset discards pending auditory look"))
		return false;
	round();
	step(.12f);
	emit("player/pl_step1.wav");
	step(.2f);
	self.v.deadflag = DEAD_DEAD;
	self.v.health = 0;
	astrabot::runtime::BotCommand dead{};
	dead.viewAngles = {11, -25, 0};
	commands.behavior_applyNoiseLookCommand(0, dead);
	if (!check(dead.viewAngles.pitch == 11 && dead.viewAngles.yaw == -25,
			"death prevents retained auditory look from modifying commands"))
		return false;
	self.v.deadflag = DEAD_NO;
	self.v.health = 100;
	const char *navPath = "astrabot_noise_approach.nav";
	if (!check(writeNoiseApproachNav(navPath), "auditory NAV fixture writes"))
		return false;
	const astrabot::metamod::NavLoadRequest load = {
		navPath, "astrabot_noise_approach", runtime.snapshot().mapGeneration, false, 0, false, 0};
	const auto loaded = runtime.loadNavigationFile(&load);
	std::remove(navPath);
	if (!check(loaded == astrabot::metamod::NavLoadResult::Loaded, "auditory NAV approach fixture loads"))
		return false;
	round();
	self.v.origin = Vector(32, 32, 36);
	self.v.v_angle = Vector(0, 90, 0);
	self.v.angles = self.v.v_angle;
	step(.12f);
	noiseRayBlocked = true;
	noiseApproachVisible = true;
	noiseApproachRays = 0;
	source.v.origin = Vector(224, 32, 0); // The heard point belongs to area4; its route is occluded.
	emit("player/pl_step1.wav");
	const auto approachBefore = self.v.v_angle;
	auto approachSpring = static_cast<astrabot::metamod::BotBehaviorServices &>(runtime).command_lookSpring(0);
	const auto approachEye = self.v.origin + self.v.view_ofs;
	const auto approachGoal = astrabot::metamod::ActionAdapter::useEntityView(
		192.0f - approachEye.x, 32.0f - approachEye.y, 36.0f - approachEye.z);
	const auto approachLook = astrabot::compat::trackAim(approachBefore.y, approachBefore.x, approachGoal.yaw,
		approachGoal.pitch, astrabot::runtime::BotTimingScheduler::kCommandInterval, 1, &approachSpring, false);
	step(.2f);
	const auto actualApproachSpring = static_cast<astrabot::metamod::BotBehaviorServices &>(runtime).command_lookSpring(0);
	if (!check(noiseApproachRays == 1 && std::fabs(self.v.v_angle.y - approachLook.yaw) < .0001f &&
			std::fabs(self.v.v_angle.x - approachLook.pitch) < .0001f &&
			std::fabs(actualApproachSpring.yawVelocity - approachSpring.yawVelocity) < .0001f &&
			std::fabs(actualApproachSpring.pitchVelocity - approachSpring.pitchVelocity) < .0001f,
			"occluded sound follows the visible NAV approach with retained native spring momentum"))
	{
		std::fprintf(stderr, "approach shared aim rays=%u expected=(%.9f %.9f) actual=(%.9f %.9f)\n",
			noiseApproachRays, approachLook.yaw, approachLook.pitch, self.v.v_angle.y, self.v.v_angle.x);
		return false;
	}
	const auto cachedApproachLook = astrabot::compat::trackAim(approachLook.yaw, approachLook.pitch, approachGoal.yaw,
		approachGoal.pitch, astrabot::runtime::BotTimingScheduler::kCommandInterval, 1, &approachSpring, false);
	step(.16f);
	if (!check(noiseApproachRays == 1 && std::fabs(self.v.v_angle.y - cachedApproachLook.yaw) < .0001f &&
			std::fabs(self.v.v_angle.x - cachedApproachLook.pitch) < .0001f,
			"unchanged sound, pose and NAV reuse the bend and advance one native spring step"))
		return false;

	round();
	self.v.v_angle = Vector(0, 90, 0);
	self.v.angles = self.v.v_angle;
	auto &navigation = static_cast<astrabot::metamod::BotNavigationServices &>(runtime);
	auto &profile = const_cast<astrabot::compat::ProfileRecord &>(navigation.behavior_managedProfiles(0));
	profile.csbotReactionSeconds = .3f;
	step(.12f);
	enemy.v.origin = Vector(32, 150, 36);
	enemy.v.velocity = Vector(0, 230, 0);
	enemy.v.deadflag = DEAD_NO;
	enemy.v.health = 100;
	noiseEnemyVisible = true;
	noiseApproachRays = 0;
	emit("player/pl_step1.wav");
	step(.18f);
	auto &views = static_cast<astrabot::metamod::BotBehaviorServices &>(runtime);
	auto &observations = static_cast<astrabot::metamod::BotObservationServices &>(runtime);
	const auto &snapshot = views.observation_managedBotPerception(0).snapshot();
	const astrabot::world::ActorObservation *visibleEnemy = nullptr;
	if (!check(snapshot.findActor({2U, 1U}, &visibleEnemy) == astrabot::world::ContactLookupResult::Found &&
			visibleEnemy && visibleEnemy->visible && !observations.behavior_currentEnemySource(0).isValid(),
			"one visible enemy sample precedes delayed acquisition"))
		return false;
	noiseEnemyVisible = false;
	step(.14f);
	visibleEnemy = nullptr;
	if (!check(snapshot.findActor({2U, 1U}, &visibleEnemy) == astrabot::world::ContactLookupResult::Found &&
			visibleEnemy && !visibleEnemy->visible && !observations.behavior_currentEnemySource(0).isValid(),
			"the candidate is occluded and remains unacquired at the first eligible noise update"))
		return false;
	if (!check(noiseApproachRays == 1, "an unacquired vision candidate cannot bypass the occluded footstep approach"))
		return false;

	using namespace astrabot;
	const compat::NoiseEnemyContact unseen{};
	const compat::NoiseEnemyContact atTimeZero{{2U, 1U}, {2U, 3U}, {0, 0, 750}, 0.0f};
	if (!check(!unseen.allowsDirectLook(false, false, 0.0f, {}) &&
			atTimeZero.allowsDirectLook(false, false, 4.999f, {0, 0, 1}) &&
			!atTimeZero.allowsDirectLook(false, false, 4.999f, {}) &&
			!atTimeZero.allowsDirectLook(false, false, 5.0f, {0, 0, 1}) &&
			!atTimeZero.allowsDirectLook(false, true, 1.0f, {0, 0, 1}) &&
			unseen.allowsDirectLook(true, true, 5.0f, {}),
			"recent contact distinguishes time zero, strict 3D range and age, arrival at Hide, and clear LOS"))
		return false;

	runtime.onMessageBegin(MSG_ONE, 94, nullptr, &self);
	runtime.onWriteByte(1);
	runtime.onWriteByte(22);
	runtime.onWriteByte(30);
	runtime.onMessageEnd();
	noiseEnemyVisible = true;
	auto &machine = const_cast<compat::CompatibilityStateMachine &>(navigation.behavior_managedBotStateMachines(0));
	for (int frame = 0; frame < 4 && !machine.attackOverlayActive(); ++frame)
	{
		self.v.angles = Vector(0, 90, 0);
		self.v.v_angle = self.v.angles;
		step(.125f);
	}
	if (!check(machine.attackOverlayActive() && observations.behavior_currentEnemySource(0).id == 2U,
			"positive control actually enters Attack against the visible enemy"))
		return false;

	enemy.v.deadflag = DEAD_DEAD;
	enemy.v.health = 0;
	noiseEnemyVisible = false;
	const auto &life = observations.service_lifecycle();
	compat::StateUpdateContext attackContext{};
	attackContext.actor = machine.actor();
	attackContext.frame = {life.mapGeneration(), life.roundGeneration(), observations.service_adapterFrameCount()};
	attackContext.timestamp = globals.time;
	attackContext.fullUpdateSequence = observations.service_managedBotFullUpdateSequences()[0];
	attackContext.fullUpdate = true;
	attackContext.lifeAvailability = attackContext.roundAvailability = compat::ObservationAvailability::Available;
	attackContext.attackObservation = compat::ObservationAvailability::Available;
	attackContext.alive = attackContext.roundActive = true;
	if (!check(machine.stopAttack(attackContext) == compat::StateResult::Transitioned,
			"normal noise look resumes after Attack stops"))
		return false;
	step(.125f);
	profile.csbotReactionSeconds = .125f;
	self.v.angles = Vector(0, 90, 0);
	self.v.v_angle = self.v.angles;
	noiseApproachRays = 0;
	emit("player/pl_step1.wav");
	step(.2f);
	if (!check(noiseApproachRays == 0 && self.v.v_angle.y < 90.0f &&
			!machine.attackOverlayActive() && (gLastRunPlayerMoveButtons & (IN_ATTACK | IN_ATTACK2)) == 0,
			"recent acquired contact looks directly at the occluded footstep after combat stops"))
		return false;

	// Exercise the late command consumer, outside Hunt/Guard preparation. A
	// lost abort result here would submit the pre-callback movement draft.
	if (!check(loadHearingNav(runtime), "late noise: hearing NAV loads")) return false;
	noiseRayBlocked = noiseApproachVisible = false;
	for (unsigned mutation = 0; mutation < 4; ++mutation)
	{
		round();
		globals.time += 1.0f;
		self.v.origin = Vector(0, 0, 36);
		source.v.origin = Vector(0, 300, 0);
		emit("weapons/ak47-1.wav");
		// Drive the real observation/command phases separately so no early
		// Hunt/Guard decision can consume the sound before late dispatch.
		++const_cast<std::uint32_t &>(observations.service_managedBotFullUpdateSequences()[0]);
		world::WorldSnapshot lateSnapshot{};
		metamod::ThreatObservation threats{};
		if (!check(static_cast<metamod::BotBehaviorServices &>(runtime).
			observation_collectManagedPerception(0, &lateSnapshot, &threats), "late noise: public sound perception collected"))
			return false;
		const auto &heard = static_cast<metamod::BotBehaviorServices &>(runtime).
			observation_managedBotPerception(0).snapshot().noise();
		if (!check(compat::hasNoiseLookPosition(heard) && heard.priority == world::NoisePriority::High,
			"late noise: localized dangerous gunfire is real input")) return false;
		attackContext.actor = machine.actor();
		attackContext.frame = {life.mapGeneration(), life.roundGeneration(), observations.service_adapterFrameCount()};
		attackContext.timestamp = globals.time;
		attackContext.fullUpdateSequence = observations.service_managedBotFullUpdateSequences()[0];
		machine.transition(compat::CompatibilityStateId::MoveTo, compat::StateTransitionReason::ExplicitRequest,
			compat::StateTransitionId::ExplicitStateChange, attackContext);
		metamod::BotCommandRuntime late;
		late.bindServices(commands);
		late.onActorRemembered(0);
		const_cast<std::uint32_t &>(late.managedBotCommandSequences(0)) =
			static_cast<metamod::BotBehaviorServices &>(runtime).command_managedBotCommandSequences(0) + mutation;
		auto handle = commands.service_managedBotHandles()[0];
		runtime::MovementPhysicsState before{};
		auto draft = late.beginNeutralCommand(0, handle, before);
		metamod::ActionProposal proposal{};
		late.finishNeutralCommand(0, handle, before, draft, proposal);
		struct Interrupt final : compat::IRandomTraceSink
		{
			std::function<void()> callback;
			unsigned startles = 0;
			void record(const compat::RandomTraceRecord &draw) override
			{
				if (std::strcmp(draw.request.semanticId, "NoiseLook.StartleInhibit") != 0) return;
				++startles;
				if (callback) { auto once = std::move(callback); callback = {}; once(); }
			}
		} interrupt;
		bool changed = mutation == 0;
		interrupt.callback = [&] {
			if (mutation == 1)
				changed = machine.beginAttack(attackContext) == compat::StateResult::Transitioned &&
					machine.stopAttack(attackContext) == compat::StateResult::Transitioned;
			if (mutation == 2)
				changed = machine.transition(compat::CompatibilityStateId::Idle, compat::StateTransitionReason::ExplicitRequest,
					compat::StateTransitionId::ExplicitStateChange, attackContext) == compat::StateResult::Transitioned;
			if (mutation == 3) { runtime.beginRoundGeneration(); changed = true; }
		};
		const int sent = gRunPlayerMoveCount;
		observations.service_compatibilityRandomSource().setTraceSink(&interrupt);
		const auto receipt = late.executeManagedBotCommand(0, handle);
		observations.service_compatibilityRandomSource().setTraceSink(nullptr);
		if (!check(interrupt.startles == 1 && changed, "late noise: callback reached and requested mutation completed")) return false;
		if (!check(mutation == 0 ? receipt.result == runtime::DispatchResult::Dispatched && gRunPlayerMoveCount == sent + 1
			: receipt.result == runtime::DispatchResult::NoCommand && gRunPlayerMoveCount == sent,
			"late noise: invalidated callback aborts old RunPlayerMove; unchanged execution dispatches"))
		{
			std::fprintf(stderr, "late noise mutation=%u result=%u moves=%d/%d\n", mutation,
				static_cast<unsigned>(receipt.result), gRunPlayerMoveCount, sent);
			return false;
		}
	}
	return true;
}
}
