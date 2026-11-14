#include "plugin/plugin_runtime.hpp"
namespace
{
void setupGunshot(enginefuncs_t &, globalvars_t &);
bool testGunshot(astrabot::metamod::PluginRuntime &, enginefuncs_t &, globalvars_t &);
}
#define ASTRABOT_EXTRA_ACTOR_TEST testGunshot
#define ASTRABOT_ACTOR_ENGINE_SETUP setupGunshot
#define ASTRABOT_ACTOR_PROFILE_TEXT                                                                                    \
	"Any { Name = \"Hearing Bot\" Team = any Difficulty = 1 Skill = 70 ReactionTime = 0.125 }\n"
#include "compat_actor_command_tests.cpp"
#include "fixtures/noise_runtime_nav.hpp"
namespace
{
bool precacheGun(unsigned short index, const char *script, bool overridden = false, int type = 1)
{
	enginefuncs_t post{};
	int version = ENGINE_INTERFACE_VERSION;
	if (!astrabot::metamod::PluginRuntime::instance().provideEngineFunctionsPost(&post, &version) ||
		!post.pfnPrecacheEvent)
		return false;
	unsigned short original = overridden ? 60000 : index;
	gpMetaGlobals->orig_ret = &original;
	gpMetaGlobals->override_ret = overridden ? &index : nullptr;
	gpMetaGlobals->status = overridden ? MRES_OVERRIDE : MRES_IGNORED;
	post.pfnPrecacheEvent(type, script);
	bool ignored = gpMetaGlobals->mres == MRES_IGNORED;
	gpMetaGlobals->orig_ret = nullptr;
	gpMetaGlobals->override_ret = nullptr;
	gpMetaGlobals->status = MRES_IGNORED;
	return ignored;
}
void setupGunshot(enginefuncs_t &, globalvars_t &)
{
	// Weapon precaching occurs before ServerActivate in an actual GameDLL.
	precacheGun(17, "events/ak47.sc");
	precacheGun(18, "events/awp.sc", true);
	precacheGun(19, "events/m4a1.sc");
	precacheGun(20, "events/usp.sc");
	precacheGun(21, "events/tmp.sc");
}
void gunTrace(const float *, const float *end, int flags, edict_t *, TraceResult *trace)
{
	*trace = {};
	trace->flFraction = flags == 0x101 ? 1.0f : 0.0f;
	trace->vecEndPos = Vector(end[0], end[1], end[2]);
}
bool knifeSight = true;
void knifeTrace(const float *, const float *end, int, edict_t *, TraceResult *trace)
{
	*trace = {};
	trace->flFraction = knifeSight ? 1.0f : 0.0f;
	trace->vecEndPos = Vector(end[0], end[1], end[2]);
}

bool testKnifeRun(astrabot::metamod::PluginRuntime &runtime, enginefuncs_t &engine, globalvars_t &globals)
{
	using namespace astrabot;
	auto &self = gEntities[0];
	auto &source = gEntities[4];
	runtime.beginRoundGeneration();
	engine.pfnTraceLine = &knifeTrace;
	knifeSight = true;
	runtime.giveEnginePointers(&engine, &globals);
	self.v.v_angle = Vector(0, 90, 0);
	self.v.angles = self.v.v_angle;
	self.v.maxspeed = 240;
	source.v.origin = Vector(0, 500, 36);
	source.v.team = 2;
	enginefuncs_t hooks{};
	int version = ENGINE_INTERFACE_VERSION;
	if (!runtime.provideEngineFunctions(&hooks, &version))
	{
		return false;
	}
	const auto weapon = [&](int id)
	{
		hooks.pfnMessageBegin(MSG_ONE, 94, nullptr, &self);
		hooks.pfnWriteByte(1);
		hooks.pfnWriteByte(id);
		hooks.pfnWriteByte(1);
		hooks.pfnMessageEnd();
	};
	const auto step = [&](float dt)
	{
		globals.time += dt;
		runtime.onStartFrame();
		runtime.onStartFramePost();
	};
	const auto fire = [&](const edict_t *entity, int flags = 1, float delay = 0, unsigned short index = 17)
	{
		float zero[3]{};
		hooks.pfnPlaybackEvent(flags, entity, index, delay, zero, zero, 0, 0, 0, 0, 0, 0);
	};
	weapon(29);
	for (int i = 0; i < 6; ++i)
	{
		self.v.v_angle = Vector(0, 90, 0);
		self.v.angles = self.v.v_angle;
		step(.12f);
	}
	auto &nav = static_cast<metamod::BotNavigationServices &>(runtime);
	auto &commands = static_cast<metamod::BotCommandServices &>(runtime);
	if (!check(nav.behavior_managedBotStateMachines(0).attackOverlayActive(),
			"knife fixture recognizes enemy through real perception"))
	{
		return false;
	}
	knifeSight = false;
	source.v.origin = Vector(0, 10000, 36);
	fire(&source);
	if (!check(commands.behavior_forcedRunning(0), "current enemy shot forces knife run outside hearing and sight"))
	{
		return false;
	}
	const float started = globals.time;
	globals.time = started + 5;
	if (!check(commands.behavior_forcedRunning(0), "runtime preserves native exact five-second boundary"))
	{
		return false;
	}
	globals.time += .01f;
	if (!check(!commands.behavior_forcedRunning(0), "runtime expires after five seconds"))
	{
		return false;
	}
	fire(&source, 1 << 4);
	fire(&source, 1 << 6);
	fire(&source, 1, .1f);
	fire(&source, 1, 0, 999);
	hooks.pfnEmitSound(&source, 1, "weapons/ak47_clipin.wav", 1, 1, 0, 100);
	if (!check(!commands.behavior_forcedRunning(0),
			"private delayed unknown playback and reload samples do not force run"))
	{
		return false;
	}
	weapon(28);
	fire(&source);
	if (!check(!commands.behavior_forcedRunning(0), "rifle-equipped observer ignores enemy firing reaction"))
	{
		return false;
	}
	weapon(29);
	fire(&self);
	if (!check(!commands.behavior_forcedRunning(0), "self is not the current enemy"))
	{
		return false;
	}
	++source.serialnumber;
	fire(&source);
	if (!check(!commands.behavior_forcedRunning(0), "reused enemy slot cannot trigger old enemy reaction"))
	{
		return false;
	}
	--source.serialnumber;
	hooks.pfnEmitSound(&source, 1, "weapons/dryfire_rifle.wav", .8f, 1, 0, 100);
	if (!check(commands.behavior_forcedRunning(0), "exact standard dry fire starts run without audibility gate"))
	{
		return false;
	}
	weapon(28);
	if (!check(commands.behavior_forcedRunning(0), "weapon switch does not cancel started native run timer"))
	{
		return false;
	}
	static_cast<metamod::BotObservationServices &>(runtime).behavior_observePublicDeath(0);
	if (!check(!commands.behavior_forcedRunning(0), "public death cancels even before the next full update"))
	{
		return false;
	}
	runtime.beginRoundGeneration();
	if (!check(!commands.behavior_forcedRunning(0), "round clears run timer and enemy identity"))
	{
		return false;
	}
	weapon(29);
	fire(&source);
	if (!check(!commands.behavior_forcedRunning(0), "unrecognized enemy cannot restart timer after round reset"))
	{
		return false;
	}
	if (!check(loadInvestigationNav(runtime), "knife run uses actual investigation movement fixture"))
	{
		return false;
	}
	step(30);
	knifeSight = true;
	source.v.origin = Vector(0, 500, 36);
	for (int i = 0; i < 6; ++i)
	{
		self.v.v_angle = Vector(0, 90, 0);
		self.v.angles = self.v.v_angle;
		step(.12f);
	}
	knifeSight = false;
	step(.3f);
	step(.3f);
	// Isolate the run override from Attack ownership. Native knife Attack keeps
	// pursuing after sight loss; losing vision does not authorize sound investigation.
	auto &machine = const_cast<compat::CompatibilityStateMachine &>(nav.behavior_managedBotStateMachines(0));
	auto &services = static_cast<metamod::BotBehaviorServices &>(runtime);
	const auto &life = services.service_lifecycle();
	const auto &handle = services.service_managedBotHandles()[0];
	compat::StateUpdateContext stopped{};
	stopped.actor = {handle.actor.slot, handle.actor.actorGeneration};
	stopped.frame = {life.mapGeneration(), life.roundGeneration(), services.service_adapterFrameCount()};
	stopped.timestamp = globals.time;
	stopped.fullUpdate = true;
	stopped.fullUpdateSequence = services.service_managedBotFullUpdateSequences()[0];
	stopped.alive = true;
	stopped.roundActive = true;
	(void)machine.stopAttack(stopped);
	if (!check(!machine.attackOverlayActive(), "run fixture explicitly releases Attack before noise investigation"))
	{
		return false;
	}
	hooks.pfnEmitSound(&source, 1, "weapons/ak47-1.wav", 1, 1, 0, 100);
	step(.12f); // Publish before reaction; defer the next decision beyond the existing path throttle.
	step(.6f);
	for (int i = 0; i < 12; ++i)
	{
		step(.12f);
		if (nav.behavior_noiseInvestigationWalking(0))
		{
			break;
		}
	}
	if (!check(nav.behavior_noiseInvestigationWalking(0) && commands.behavior_forcedRunning(0) &&
				std::hypot(gLastRunPlayerMoveForward, gLastRunPlayerMoveSide) > 96.01f,
			"forced run overrides actual full-update noise walking speed"))
	{
		return false;
	}
	step(.04f);
	if (!check(std::hypot(gLastRunPlayerMoveForward, gLastRunPlayerMoveSide) > 96.01f,
			"cached command preserves forced running speed"))
	{
		return false;
	}
	// The fake engine does not move the edict. Reset its unrelated stuck-progress window.
	globals.time += 5.1f;
	static_cast<metamod::BotBehaviorServices &>(runtime).applyNavigationRequest(
		{0, metamod::NavigationRequestKind::ResetProgress});
	step(.12f);
	if (!check(!commands.behavior_forcedRunning(0) && nav.behavior_noiseInvestigationWalking(0) &&
				std::hypot(gLastRunPlayerMoveForward, gLastRunPlayerMoveSide) > 0 &&
				std::hypot(gLastRunPlayerMoveForward, gLastRunPlayerMoveSide) <= 96.01f,
			"expiry restores quiet walking on the actual command path"))
	{
		return false;
	}
	const auto commandBefore =
		static_cast<metamod::BotObservationServices &>(runtime).command_managedBotCommandSequences(0);
	const auto fullBefore =
		static_cast<metamod::BotBehaviorServices &>(runtime).service_managedBotFullUpdateSequences()[0];
	hooks.pfnEmitSound(&source, 1, "weapons/dryfire_pistol.wav", .8f, 1, 0, 100);
	step(.04f);
	if (!check(static_cast<metamod::BotBehaviorServices &>(runtime).service_managedBotFullUpdateSequences()[0] ==
					fullBefore &&
				static_cast<metamod::BotObservationServices &>(runtime).command_managedBotCommandSequences(0) >
					commandBefore &&
				commands.behavior_forcedRunning(0) &&
				std::hypot(gLastRunPlayerMoveForward, gLastRunPlayerMoveSide) > 96.01f,
			"enemy firing immediately restores an already cached walking command without a full update"))
	{
		return false;
	}
	engine.pfnTraceLine = &gunTrace;
	return true;
}

bool testGunshot(astrabot::metamod::PluginRuntime &runtime, enginefuncs_t &engine, globalvars_t &globals)
{
	if (!check(loadHearingNav(runtime), "hearing fixture supplies real NAV"))
		return false;
	engine.pfnRandomFloat = &hearingFixtureRandomFloat;
	using namespace astrabot;
	engine.pfnTraceLine = &gunTrace;
	runtime.giveEnginePointers(&engine, &globals);
	gObjectiveEntitiesAvailable = false;
	for (unsigned i = 1; i < 4; ++i)
	{
		gEntities[i].v.deadflag = DEAD_DEAD;
		gEntities[i].v.health = 0;
	}
	auto &self = gEntities[0];
	self.v.flags = FL_CLIENT | FL_FAKECLIENT | FL_ONGROUND;
	self.v.deadflag = DEAD_NO;
	self.v.health = 100;
	self.v.origin = Vector(0, 0, 36);
	self.v.view_ofs = Vector(0, 0, 17);
	self.v.fov = 90;
	auto &source = gEntities[4];
	source = {};
	source.serialnumber = 44;
	source.v.flags = FL_CLIENT | FL_ONGROUND;
	source.v.deadflag = DEAD_NO;
	source.v.health = 100;
	source.v.origin = Vector(0, 500, 36);
	runtime.onMessageBegin(MSG_ALL, 86, nullptr, nullptr);
	runtime.onWriteByte(5);
	runtime.onWriteString("CT");
	runtime.onMessageEnd();
	enginefuncs_t hooks{};
	int version = ENGINE_INTERFACE_VERSION;
	if (!check(runtime.provideEngineFunctions(&hooks, &version) && hooks.pfnPlaybackEvent && hooks.pfnEmitSound,
			"real public firing hook table"))
		return false;
	auto &services = static_cast<metamod::BotBehaviorServices &>(runtime);
	world::WorldSnapshot belief;
	const auto step = [&](float dt)
	{
		globals.time += dt;
		runtime.onStartFrame();
		runtime.onStartFramePost();
		return services.observation_readManagedWorldSnapshot(0, &belief);
	};
	const auto round = [&]()
	{
		runtime.beginRoundGeneration();
		self.v.v_angle = Vector(0, 0, 0);
		self.v.angles = self.v.v_angle;
		return step(.12f);
	};
	const auto fire = [&](unsigned short index, int b1 = 0, int b2 = 0, int flags = 1, float delay = 0,
						  const edict_t *invoker = nullptr)
	{
		float zero[3]{};
		hooks.pfnPlaybackEvent(flags, invoker ? invoker : &source, index, delay, zero, zero, 0, 0, 0, 0, b1, b2);
		return gpMetaGlobals->mres == MRES_IGNORED;
	};
	round();
	float shotTime = globals.time;
	if (!check(fire(17) && step(.04f), "native FEV_NOTHOST shot observed without replacing playback"))
		return false;
	// Wait for publication, retaining the original receipt time rather than renewing at delivery.
	step(.04f);
	step(.04f); // Full Update at 0.12 seconds; still below the 0.125-second reaction delay.
	if (!check(belief.noise().active && !belief.noise().ready && belief.noise().timestampSeconds == shotTime &&
				belief.noise().priority == world::NoisePriority::High && belief.noise().source.slot == 0 &&
				belief.noise().position.y == 500,
			"precached pre-activation shot reaches hidden-source noise memory"))
		return false;
	step(.12f);
	if (!check(belief.noise().ready && belief.noise().timestampSeconds == shotTime,
			"profile reaction uses real seconds in runtime"))
		return false;
	round();
	fire(17);
	hooks.pfnEmitSound(&source, 1, "weapons/ak47-1.wav", 1, 1, 0, 100);
	step(.12f);
	if (!check(belief.soundCount() == 1, "PlaybackEvent then EmitSound is one auditory receipt"))
		return false;
	round();
	hooks.pfnEmitSound(&source, 1, "weapons/ak47_clipin.wav", 1, 1, 0, 100);
	fire(17);
	step(.12f);
	if (!check(belief.soundCount() == 1 && belief.noise().priority == world::NoisePriority::High,
			"incidental weapon sample cannot suppress same-frame shot"))
		return false;
	source.v.origin = Vector(0, 1500, 36);
	for (auto index : {19, 20, 21})
	{
		round();
		fire(static_cast<unsigned short>(index), index == 19 ? 1 : 0, index == 20 ? 1 : 0);
		step(.12f);
		if (!check(!belief.noise().active, "quiet shot outside 1000 units is inaudible"))
			return false;
	}
	round();
	fire(19, 0, 1);
	step(.12f);
	if (!check(belief.noise().active, "unsilenced M4A1 uses 2000-unit radius and bparam1 only"))
		return false;
	source.v.origin = Vector(0, 10000, 36);
	round();
	fire(18);
	step(.12f);
	if (!check(belief.noise().active && belief.noise().distance > 9000,
			"overridden AWP event index reaches beyond generic hearing cap"))
		return false;
	round();
	fire(60000);
	step(.12f);
	if (!check(!belief.noise().active, "superseded original precache result is not mapped"))
		return false;
	source.v.origin = Vector(0, 500, 36);
	const auto savedSerial = source.serialnumber;
	source.serialnumber = -1;
	round();
	fire(17);
	step(.12f);
	const bool serialOk =
		check(!belief.noise().active, "invalid source generation cannot turn a player into anonymous world gunfire");
	source.serialnumber = savedSerial;
	round();
	fire(17, 0, 0, 1 << 4);
	step(.12f);
	const bool hostOnlyOk = check(!belief.noise().active, "host-only playback is not enemy audible gunfire");
	if (!serialOk || !hostOnlyOk)
		return false;
	round();
	fire(17, 0, 0, 1 << 6);
	fire(17, 0, 0, 1, .1f);
	fire(999);
	step(.12f);
	if (!check(!belief.noise().active, "client-only delayed and unmapped events fail closed"))
		return false;
	round();
	fire(17, 0, 0, 1, 0, &self);
	step(.12f);
	if (!check(!belief.noise().active, "self gunshot ignored"))
		return false;
	runtime.onMessageBegin(MSG_ALL, 86, nullptr, nullptr);
	runtime.onWriteByte(5);
	runtime.onWriteString("TERRORIST");
	runtime.onMessageEnd();
	round();
	fire(17);
	step(.12f);
	if (!check(!belief.noise().active, "friendly gunshot ignored"))
		return false;
	runtime.onMessageBegin(MSG_ALL, 86, nullptr, nullptr);
	runtime.onWriteByte(5);
	runtime.onWriteString("CT");
	runtime.onMessageEnd();
	round();
	fire(17);
	++source.serialnumber;
	step(.12f);
	if (!check(!belief.noise().active, "source slot reuse cannot deliver previous player shot"))
		return false;
	round();
	fire(17);
	step(20.1f);
	if (!check(!belief.noise().active, "delayed publication cannot renew expired gunshot"))
		return false;
	if (!testKnifeRun(runtime, engine, globals))
	{
		return false;
	}
	runtime.onServerDeactivate();
	resetEntities(); // GameDLL destroys the previous map edicts before precaching the next map.
	gCreateCount = 0;
	gServerSetsEntityTeam = true;
	gBotQuota = 0;
	if (!check(precacheGun(27, "events/ak47.sc"), "next map precaches before activation"))
		return false;
	runtime.onServerActivate(nullptr, 0, 5);
	if (!check(loadHearingNav(runtime), "next map supplies its own NAV"))
		return false;
	runtime.setCompatibilityFloat("bot_enable", 1.0f);
	runtime.setCompatibilityFloat("bot_quota", 0.0f);
	const auto addResult = runtime.executeCompatibilityCommand(request("bot_add_t"));
	if (!check(addResult == metamod::CompatibilityCommandResult::Handled,
			"next map creates observer through public command"))
		return false;
	self.v.flags = FL_CLIENT | FL_FAKECLIENT | FL_ONGROUND;
	self.v.health = 100;
	self.v.deadflag = DEAD_NO;
	self.v.solid = SOLID_SLIDEBOX;
	self.v.movetype = MOVETYPE_WALK;
	self.v.origin = Vector(0, 0, 36);
	source.v.flags = FL_CLIENT | FL_ONGROUND;
	source.v.health = 100;
	source.v.deadflag = DEAD_NO;
	source.v.origin = Vector(0, 500, 36);
	source.v.team = 2;
	for (int i = 0; i < 12; ++i)
		step(.12f);
	round();
	fire(17);
	step(.12f);
	if (!check(!belief.noise().active, "deactivation drops previous map event indices"))
		return false;
	fire(27);
	step(.12f);
	return check(belief.noise().active, "activation preserves newly precached map event indices");
}
}
