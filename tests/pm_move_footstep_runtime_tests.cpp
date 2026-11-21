#include "plugin/plugin_runtime.hpp"
#include "bridge/movement_sound_bridge.hpp"
namespace
{
bool testMovementSounds(astrabot::metamod::PluginRuntime &, enginefuncs_t &, globalvars_t &);
}
#define ASTRABOT_EXTRA_ACTOR_TEST testMovementSounds
#define ASTRABOT_ACTOR_PROFILE_TEXT \
	"Any { Name = \"PM Sound Bot\" Team = any Difficulty = 1 Skill = 70 ReactionTime = 0.125 }\n"
#include "compat_actor_command_tests.cpp"
#include "fixtures/noise_runtime_nav.hpp"
#include <memory>
namespace
{
unsigned movementAudioCalls = 0;
unsigned movementPistolCommands = 0;
bool echoMovementEmit = false;
bool shrinkMovementClients = false;
edict_t *movementSource = nullptr;
astrabot::metamod::PluginRuntime *movementRuntime = nullptr;

void movementAudio(int channel, const char *sample, float volume, float attenuation, int flags, int pitch)
{
	++movementAudioCalls;
	if (echoMovementEmit)
		astrabot::metamod::HookEmitSound(movementSource, channel, sample, volume, attenuation, flags, pitch);
}

void movementCommand(edict_t *self)
{
	if (self == &gEntities[0] && std::strcmp(astrabot::metamod::HookCommandArgv(0), "weapon_glock18") == 0)
	{
		++movementPistolCommands;
		movementRuntime->onMessageBegin(MSG_ONE, 94, nullptr, self);
		movementRuntime->onWriteByte(1);
		movementRuntime->onWriteByte(17);
		movementRuntime->onWriteByte(20);
		movementRuntime->onMessageEnd();
	}
	hookedGameClientCommand(self);
}

int movementMessageId(plid_t id, const char *name, int *size)
{
	if (std::strcmp(name, "WeaponList") == 0)
		return 101;
	if (std::strcmp(name, "AmmoX") == 0)
		return 102;
	return siteFixtureUserMessageId(id, name, size);
}

void movementTrace(const float *, const float *end, int, edict_t *, TraceResult *trace)
{
	*trace = {};
	trace->flFraction = 0;
	trace->vecEndPos = Vector(end[0], end[1], end[2]);
}

bool testMovementSounds(astrabot::metamod::PluginRuntime &runtime, enginefuncs_t &engine, globalvars_t &globals)
{
	using namespace astrabot;
	DLL_FUNCTIONS pre{}, post{};
	int version = INTERFACE_VERSION;
	if (!check(runtime.provideEntityApi(&pre, &version) && runtime.provideEntityApiPost(&post, &version) &&
			pre.pfnPM_Move && post.pfnPM_Move, "registered PM_Move pre/post hooks observe movement-only sound"))
		return false;
	movementRuntime = &runtime;
	engine.pfnRandomFloat = &hearingFixtureRandomFloat;
	engine.pfnTraceLine = &movementTrace;
	gpMetaUtilFuncs->pfnGetUserMsgID = &movementMessageId;
	gHookedGameDllTable.pfnClientCommand = &movementCommand;
	runtime.giveEnginePointers(&engine, &globals);
	gObjectiveEntitiesAvailable = false;
	setCvarFloat("bot_auto_vacate", 0);
	globals.maxClients = 5;
	for (unsigned i = 1; i < 4; ++i)
	{
		gEntities[i].v.health = 0;
		gEntities[i].v.deadflag = DEAD_DEAD;
	}
	auto &self = gEntities[0];
	self.v.flags = FL_CLIENT | FL_FAKECLIENT | FL_ONGROUND;
	self.v.health = 100;
	self.v.deadflag = DEAD_NO;
	self.v.origin = Vector(0, 0, 36);
	self.v.view_ofs = Vector(0, 0, 17);
	self.v.weapons = (1 << 22) | (1 << 18) | (1 << 17) | (1 << 29);
	auto &source = gEntities[4];
	source = {};
	source.serialnumber = 0;
	source.v.flags = FL_CLIENT | FL_ONGROUND;
	source.v.team = 2;
	source.v.health = 100;
	source.v.deadflag = DEAD_NO;
	source.v.origin = Vector(5000, 0, 36);
	movementSource = &source;
	runtime.onMessageBegin(MSG_ALL, 86, nullptr, nullptr);
	runtime.onWriteByte(5);
	runtime.onWriteString("CT");
	runtime.onMessageEnd();
	if (!check(loadHearingNav(runtime), "movement hearing loads valid NAV"))
		return false;
	auto &services = static_cast<metamod::BotBehaviorServices &>(runtime);
	world::WorldSnapshot belief;
	const auto step = [&](float dt)
	{
		globals.time += dt;
		runtime.onStartFrame();
		runtime.onStartFramePost();
		services.observation_readManagedWorldSnapshot(0, &belief);
	};
	const auto currentWeapon = [&](int id)
	{
		runtime.onMessageBegin(MSG_ONE, 94, nullptr, &self);
		runtime.onWriteByte(1);
		runtime.onWriteByte(id);
		runtime.onWriteByte(10);
		runtime.onMessageEnd();
	};
	const auto round = [&]()
	{
		runtime.beginRoundGeneration();
		currentWeapon(22);
		step(.125f);
	};
	std::unique_ptr<playermove_t> ownedMove(new playermove_t{});
	auto &move = *ownedMove;
	const auto prepareMove = [&]()
	{
		move.player_index = 4;
		move.server = TRUE;
		move.runfuncs = TRUE;
		move.dead = FALSE;
		move.deadflag = DEAD_NO;
		move.spectator = 0;
		move.origin[0] = 500;
		move.origin[1] = 0;
		move.origin[2] = 36;
		move.PM_PlaySound = &movementAudio;
	};
	const auto emitMove = [&](int server, const char *sample = "player/pl_metal1.wav", int flags = 0)
	{
		pre.pfnPM_Move(&move, server);
		if (shrinkMovementClients)
			globals.maxClients = 4;
		move.PM_PlaySound(4, sample, .5f, .8f, flags, 100);
		globals.maxClients = 5;
		post.pfnPM_Move(&move, server);
	};
	round();
	prepareMove();
	unsigned calls = movementAudioCalls;
	emitMove(TRUE);
	step(.2f);
	if (!check(movementAudioCalls == calls + 1 && move.PM_PlaySound == &movementAudio &&
			belief.noise().active && belief.noise().priority == world::NoisePriority::Low &&
			belief.noise().position.x == 500 && source.v.origin.x == 5000,
			"PM-only footstep uses live movement origin, restores callback, forwards audio without editing edict"))
		return false;
	round();
	prepareMove();
	echoMovementEmit = true;
	emitMove(TRUE);
	echoMovementEmit = false;
	step(.2f);
	if (!check(belief.soundCount() == 1 && belief.soundAt(0)->origin.x == 500,
			"scoped EmitSound duplicate shares movement origin and one receipt"))
		return false;

	round();
	prepareMove();
	emitMove(TRUE);
	move.origin[0] = 600;
	emitMove(TRUE);
	self.v.origin.x = -601;
	++source.serialnumber;
	step(.2f);
	self.v.origin.x = 0;
	--source.serialnumber;
	if (!check(belief.noise().active && belief.noise().position.x == 500 &&
			belief.soundCount() == 2 && belief.soundAt(0)->distance == 500 && belief.soundAt(1)->distance == 600,
			"the nearer same-frame receipt wins even if the listener moves away and the source edict changes"))
		return false;

	round();
	prepareMove();
	move.origin[0] = 1100;
	emitMove(TRUE);
	self.v.origin.x = 500;
	step(.2f);
	self.v.origin.x = 0;
	if (!check(!belief.noise().active && belief.soundCount() == 0,
			"a listener entering range later cannot hear a past footstep at the strict distance boundary"))
		return false;

	for (unsigned variant = 0; variant < 14; ++variant)
	{
		round();
		prepareMove();
		int server = TRUE;
		const char *sample = "player/pl_metal1.wav";
		int flags = 0;
		if (variant == 0)
			server = FALSE;
		if (variant == 1)
			move.runfuncs = FALSE;
		if (variant == 2)
			move.player_index = -1;
		if (variant == 3)
			move.player_index = globals.maxClients;
		if (variant == 4)
			move.dead = TRUE;
		if (variant == 5)
			move.spectator = 1;
		if (variant == 6)
			sample = "player/pl_jump1.wav";
		if (variant == 7)
			flags = 1 << 5;
		if (variant == 8)
			move.origin[0] = std::numeric_limits<float>::quiet_NaN();
		if (variant == 9)
			source.free = TRUE;
		if (variant == 10)
			source.v.health = 0;
		if (variant == 11)
			source.v.flags |= FL_SPECTATOR;
		if (variant == 12)
			source.serialnumber = -1;
		shrinkMovementClients = variant == 13;
		calls = movementAudioCalls;
		emitMove(server, sample, flags);
		shrinkMovementClients = false;
		source.free = FALSE;
		source.v.health = 100;
		source.v.flags = FL_CLIENT | FL_ONGROUND;
		source.serialnumber = 0;
		step(.2f);
		if (belief.noise().active)
			std::fprintf(stderr, "unexpected PM noise variant=%u\n", variant);
		if (!check(!belief.noise().active && movementAudioCalls == calls + 1 && move.PM_PlaySound == &movementAudio,
				"rejected movement observation preserves original audio and callback"))
			return false;
	}
	round();
	prepareMove();
	pre.pfnPM_Move(&move, TRUE);
	++source.serialnumber;
	move.PM_PlaySound(4, "player/pl_step1.wav", 1, 1, 0, 100);
	post.pfnPM_Move(&move, TRUE);
	step(.2f);
	if (!check(!belief.noise().active, "source serial reuse between pre and sound cannot create hearing"))
		return false;
	source.serialnumber = 0;
	round();
	prepareMove();
	pre.pfnPM_Move(&move, TRUE);
	if (!check(!runtime.detach(PT_ANYTIME, PNL_NULL) && move.PM_PlaySound != &movementAudio,
			"active movement scope refuses unload without altering the live callback"))
		return false;
	runtime.beginRoundGeneration();
	if (!check(move.PM_PlaySound == &movementAudio && !runtime.detach(PT_ANYTIME, PNL_NULL),
			"round reset restores callback but retains active scope lifetime until post"))
		return false;
	move.PM_PlaySound(4, "player/pl_step1.wav", 1, 1, 0, 100);
	post.pfnPM_Move(&move, TRUE);
	step(.2f);
	if (!check(!belief.noise().active, "round-changed movement cannot emit stale hearing"))
		return false;
	round();
	prepareMove();
	pre.pfnPM_Move(&move, TRUE);
	const float beforeClockChange = globals.time;
	globals.time -= 1;
	move.PM_PlaySound(4, "player/pl_step1.wav", 1, 1, 0, 100);
	globals.time = beforeClockChange;
	post.pfnPM_Move(&move, TRUE);
	step(.2f);
	if (!check(!belief.noise().active, "clock rollback during movement rejects an obsolete scope"))
		return false;
	// AWP and usable pistol are public observations, not synthetic inventory certainty.
	round();
	for (int id : {18, 17})
	{
		runtime.onMessageBegin(MSG_ONE, 101, nullptr, &self);
		runtime.onWriteString(id == 18 ? "weapon_awp" : "weapon_glock18");
		for (int v : {id == 18 ? 4 : 10, 90, 255, 255, id == 18 ? 0 : 1, 0, id, 0})
			runtime.onWriteByte(v);
		runtime.onMessageEnd();
	}
	runtime.onMessageBegin(MSG_ONE, 94, nullptr, &self);
	runtime.onWriteByte(0);
	runtime.onWriteByte(17);
	runtime.onWriteByte(20);
	runtime.onMessageEnd();
	currentWeapon(18);
	prepareMove();
	move.origin[0] = 200;
	const auto selections = movementPistolCommands;
	emitMove(TRUE);
	step(.125f);
	if (!check(movementPistolCommands == selections + 1 && source.v.origin.x == 5000,
			"PM-only close footstep reaches actual sniper-to-pistol command from live origin"))
		return false;
	prepareMove();
	pre.pfnPM_Move(&move, TRUE);
	runtime.onServerDeactivate();
	const bool restored = move.PM_PlaySound == &movementAudio;
	post.pfnPM_Move(&move, TRUE);
	return check(restored, "map deactivation restores a callback before cancelling movement capture");
}
}
