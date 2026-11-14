#include "plugin/plugin_runtime.hpp"
namespace
{
bool testNoiseLook(astrabot::metamod::PluginRuntime &, enginefuncs_t &, globalvars_t &);
}
#define ASTRABOT_EXTRA_ACTOR_TEST testNoiseLook
#define ASTRABOT_ACTOR_PROFILE_TEXT                                                                                    \
	"Any { Name = \"Noise Bot\" Team = any Difficulty = 1 Skill = 70 ReactionTime = 0.125 }\n"
#include "compat_actor_command_tests.cpp"
namespace
{
bool noiseRayBlocked = false;
bool noiseApproachVisible = false;
unsigned noiseApproachRays = 0;
unsigned noiseGlassIgnoringRays = 0;
void noiseLookTrace(const float *start, const float *end, int flags, edict_t *, TraceResult *trace)
{
	*trace = {};
	trace->flFraction = noiseRayBlocked && !(noiseApproachVisible && flags != 0x101) ? 0.0f : 1.0f;
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
	self.v.v_angle = Vector(0, 0, 0);
	self.v.angles = self.v.v_angle;
	for (unsigned i = 0; i < 4; ++i)
		step(.08f);
	if (std::fabs(self.v.v_angle.y) >= 5)
	{
		auto &views = static_cast<astrabot::metamod::BotBehaviorServices &>(runtime);
		const auto &belief = views.observation_managedBotPerception(0).snapshot();
		unsigned visible = 0;
		for (std::size_t i = 0; i < belief.actorCount(); ++i)
		{
			const auto *contact = belief.actorAt(i);
			if (contact && contact->visible && contact->relation == astrabot::world::TeamRelation::Hostile)
				++visible;
		}
		std::fprintf(stderr, "noise precedence yaw=%g visible=%u activeWeapon=%d\n", self.v.v_angle.y, visible,
			views.observation_managedBotCurrentWeapons(0).available ? 1 : 0);
	}
	if (!check(std::fabs(self.v.v_angle.y) < 5, "visible enemy aim is never displaced by the remembered side noise"))
		return false;
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
	emit("player/pl_step1.wav");
	step(.2f);
	if (!check(noiseApproachRays == 1 && self.v.v_angle.y > 0 && self.v.v_angle.y < 90,
			"occluded sound uses the nearest visible NAV approach instead of aiming through the wall"))
		return false;
	step(.16f);
	return check(noiseApproachRays == 1, "unchanged sound, pose and NAV reuse the validated bend result");
}
}
