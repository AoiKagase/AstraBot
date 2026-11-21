#include "plugin/plugin_runtime.hpp"
#include <entity_state.h>
#include <weaponinfo.h>
namespace
{
void setupGunshot(enginefuncs_t &, globalvars_t &);
void setupGunshotGameDll(DLL_FUNCTIONS &);
bool testGunshot(astrabot::metamod::PluginRuntime &, enginefuncs_t &, globalvars_t &);
}
#define ASTRABOT_EXTRA_ACTOR_TEST testGunshot
#define ASTRABOT_ACTOR_ENGINE_SETUP setupGunshot
#define ASTRABOT_ACTOR_GAMEDLL_SETUP setupGunshotGameDll
#define ASTRABOT_ACTOR_PROFILE_TEXT                                                                                    \
	"Any { Name = \"Hearing Bot\" Team = any Difficulty = 1 Skill = 70 ReactionTime = 0.125 }\n"
#include "compat_actor_command_tests.cpp"
#include "fixtures/noise_runtime_nav.hpp"
namespace
{
int nativeActiveWeapon = 0;
bool nativeSilenced = false;

void setupGunshotGameDll(DLL_FUNCTIONS &dll)
{
	dll.pfnUpdateClientData = [](const edict_t *, int sendWeapons, clientdata_t *client) {
		if (sendWeapons)
			client->m_iId = nativeActiveWeapon;
	};
	dll.pfnGetWeaponData = [](edict_t *, weapon_data_t *weapons) {
		weapons[nativeActiveWeapon].m_iId = nativeActiveWeapon;
		weapons[nativeActiveWeapon].m_iWeaponState = nativeSilenced ? 1 << 2 : 0;
		return 1;
	};
}

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
	const float runStartedAt = globals.time;
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
	// The fake engine records commands but does not move the edict. Advance
	// along the approach in normal updates until the independent run timer expires.
	while (globals.time <= runStartedAt + 5.0f)
	{
		self.v.origin.y += 6.0f;
		step(.12f);
	}
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

bool testNativeBotEvents(astrabot::metamod::PluginRuntime &runtime, enginefuncs_t &engine, globalvars_t &globals)
{
	using namespace astrabot;
	using metamod::NativeBotEvent;
	auto &enemy = gEntities[4];
	enemy.v.origin = Vector(0, 500, 36);
	auto &self = gEntities[0];
	self.v.origin = Vector(0, 0, 36);
	self.v.v_angle = self.v.angles = Vector(0, 0, 0);
	engine.pfnPvEntPrivateData = [](edict_t *entity) -> void * { return entity; };
	runtime.giveEnginePointers(&engine, &globals);

	auto defuse = std::make_unique<metamod::BotObservationRuntime>();
	defuse->bindServices(runtime);
	defuse->configureEngine();
	defuse->notifyTeamInfo(5, "CT");
	defuse->observeGameDllBotEvent(NativeBotEvent::DefuseStarted, &enemy);
	if (!check(defuse->nativeDefuserSlot() == 5U, "accepted GameDLL defuse binds a live CT"))
		return false;
	++enemy.serialnumber;
	const bool newEntityRejected = defuse->nativeDefuserSlot() == 0U;
	--enemy.serialnumber;
	defuse->observeGameDllBotEvent(NativeBotEvent::DefuseEnded, nullptr);
	if (!check(newEntityRejected && defuse->nativeDefuserSlot() == 0U,
			"a reused edict and a senderless abort both invalidate accepted defuse"))
		return false;

	const auto test = [&](NativeBotEvent kind, int weapon, float expectedRange, world::NoisePriority priority) {
		auto observer = std::make_unique<metamod::BotObservationRuntime>();
		observer->bindServices(runtime);
		observer->configureEngine();
		observer->notifyTeamInfo(1, "TERRORIST");
		observer->notifyTeamInfo(5, "CT");
		nativeActiveWeapon = weapon;
		if (kind == NativeBotEvent::FiredOnEmpty)
			observer->onEmitSound(&enemy, "weapons/dryfire_pistol.wav", .8f, 1.0f);
		observer->observeGameDllBotEvent(kind, &enemy);
		// The public sound for the same animation must neither promote nor duplicate the native receipt.
		observer->onEmitSound(&enemy, "weapons/ak47_clipin.wav", 1.0f, 1.0f);
		world::WorldSnapshot snapshot;
		metamod::ThreatObservation threats;
		if (!observer->collectManagedPerception(0, &snapshot, &threats))
			return false;
		if (expectedRange == 0)
			return snapshot.soundCount() == 0;
		const auto *sound = snapshot.soundAt(0);
		const bool matched = snapshot.soundCount() == 1 && sound && sound->priority == priority &&
			sound->hearingRange == expectedRange && sound->timestampSeconds == globals.time;
		if (!matched)
			std::fprintf(stderr, "native receipt weapon=%d count=%zu priority=%d range=%g timestamp=%g now=%g\n",
				weapon, snapshot.soundCount(), sound ? static_cast<int>(sound->priority) : -1,
				sound ? sound->hearingRange : 0, sound ? sound->timestampSeconds : 0, globals.time);
		return matched;
	};
	const auto low = world::NoisePriority::Low;
	const auto high = world::NoisePriority::High;
	{
		auto observer = std::make_unique<metamod::BotObservationRuntime>();
		observer->bindServices(runtime);
		observer->configureEngine();
		observer->notifyTeamInfo(1, "TERRORIST");
		observer->notifyTeamInfo(5, "CT");
		nativeActiveWeapon = 22;
		const float firedAt = globals.time;
		observer->observeGameDllBotEvent(NativeBotEvent::Fired, &enemy);
		globals.time += .01f;
		enemy.v.origin.y = 250.0f;
		observer->observeGameDllBotEvent(NativeBotEvent::Footstep, &enemy);
		world::WorldSnapshot snapshot;
		metamod::ThreatObservation threats;
		const bool collected = observer->collectManagedPerception(0, &snapshot, &threats);
		enemy.v.origin.y = 500.0f;
		if (!check(collected && snapshot.soundCount() == 2 && snapshot.noise().active &&
				snapshot.noise().kind == world::SoundKind::WeaponFire && snapshot.noise().priority == high &&
				snapshot.noise().timestampSeconds == firedAt,
				"an authoritative shot received before a nearer footstep retains its high-priority memory"))
			return false;
	}
	const auto testFlash = [&](float distance, bool friendly, bool dead, bool ownerKnown, bool heard) {
		auto observer = std::make_unique<metamod::BotObservationRuntime>();
		observer->bindServices(runtime);
		observer->configureEngine();
		observer->notifyTeamInfo(1, "TERRORIST");
		observer->notifyTeamInfo(5, friendly ? "TERRORIST" : "CT");
		const auto origin = Vector(0, distance, 36);
		enemy.v.origin = origin;
		const auto health = enemy.v.health;
		const auto deadflag = enemy.v.deadflag;
		if (dead)
		{
			enemy.v.health = 0;
			enemy.v.deadflag = DEAD_DEAD;
		}
		observer->observeGameDllBotEvent(NativeBotEvent::FlashbangExploded, ownerKnown ? &enemy : nullptr);
		enemy.v.health = health;
		enemy.v.deadflag = deadflag;
		// Neither the later source movement nor the later ear position
		// should change which listener received the explosion event.
		enemy.v.origin = Vector(0, 500, 36);
		self.v.origin = Vector(0.0f, heard ? -1200.0f : 800.0f, 36.0f);
		world::WorldSnapshot snapshot;
		metamod::ThreatObservation threats;
		const bool collected = observer->collectManagedPerception(0, &snapshot, &threats);
		self.v.origin = Vector(0, 0, 36);
		if (!collected || snapshot.soundCount() != (heard ? 1U : 0U))
			return false;
		const auto *sound = snapshot.soundAt(0);
		return !heard || (sound && sound->kind == world::SoundKind::Grenade && sound->priority == low &&
			sound->source.slot == 5 && sound->distance == distance && sound->hearingRange == 1000.0f &&
			sound->origin.y == origin.y && sound->timestampSeconds == globals.time);
	};
	if (!check(testFlash(999, false, false, true, true), "native flash uses the live enemy's event-time position") ||
		!check(testFlash(1000, false, false, true, false), "native flash has a strict 1000 unit hearing limit") ||
		!check(testFlash(500, true, false, true, false), "native flash ignores a teammate's throw") ||
		!check(testFlash(500, false, true, true, false), "native flash ignores a dead thrower") ||
		!check(testFlash(500, false, false, false, false), "native flash with no thrower gives no hearing receipt"))
		return false;
	{
		auto observer = std::make_unique<metamod::BotObservationRuntime>();
		observer->bindServices(runtime);
		observer->configureEngine();
		observer->notifyTeamInfo(1, "TERRORIST");
		edict_t grenade{};
		grenade.v.origin = Vector(0, 90, 36);
		observer->onEmitSound(&grenade, "weapons/flashbang-1.wav", .55f, 1.0f);
		world::WorldSnapshot snapshot;
		metamod::ThreatObservation threats;
		if (!check(observer->collectManagedPerception(0, &snapshot, &threats) && snapshot.soundCount() == 1 &&
				snapshot.soundAt(0)->priority == world::NoisePriority::Medium &&
				snapshot.soundAt(0)->origin.y == 90.0f,
			"stock GameDLL fallback hears the public sample at its emitter"))
			return false;
	}
	if (!check(test(NativeBotEvent::Reloaded, 28, 1100, low), "native reload has the low-priority CSBot radius") ||
		!check(test(NativeBotEvent::FiredOnEmpty, 28, 1100, low), "native dry fire has the low-priority radius") ||
		!check(test(NativeBotEvent::Fired, 29, 1000, high), "native knife attack has the short high-priority radius") ||
		!check(test(NativeBotEvent::Fired, 4, 0, low), "native grenade throw is acoustically silent"))
		return false;
	nativeSilenced = true;
	if (!check(test(NativeBotEvent::Fired, 22, 1000, high), "a native shot reads the current M4A1 silencer state"))
		return false;
	nativeSilenced = false;
	return check(test(NativeBotEvent::Fired, 22, 2000, high), "an unsilenced native M4A1 uses the normal radius");
}

bool testHostageRescueCall(astrabot::metamod::PluginRuntime &runtime, enginefuncs_t &engine)
{
	using namespace astrabot;
	const auto originalStringLookup = engine.pfnSzFromIndex;
	engine.pfnSzFromIndex = [](int index) { return index == 78 ? "hostage_entity" : szFromIndex(index); };
	edict_t hostage{};
	hostage.v.classname = 78;
	hostage.v.origin = gEntities[0].v.origin + Vector(0, 400, 0);
	const auto audible = [&](const char *sample) {
		auto observer = std::make_unique<metamod::BotObservationRuntime>();
		observer->bindServices(runtime);
		observer->configureEngine();
		observer->notifyTeamInfo(1, "CT");
		observer->onEmitSound(&hostage, sample, 1.0f, 1.0f);
		world::WorldSnapshot snapshot;
		metamod::ThreatObservation threats;
		return observer->collectManagedPerception(0, &snapshot, &threats)
			? static_cast<int>(snapshot.soundCount())
			: -1;
	};
	bool passed = true;
	for (const char *sample : {"hostage/hwavect/helpme1.wav", "hostage/hwavect/helpme2.wav", "hostage/hwavect/hey1.wav",
			 "hostage/hwavect/hey2.wav", "hostage/hwavect/overhere1.wav"})
		passed = check(audible(sample) == 0, "CSBot rescue call must not become CT investigation noise") && passed;
	passed = check(audible("hostage/hwavect/custom.wav") == 1, "unrelated samples remain audible") && passed;
	hostage.v.classname = 1;
	passed = check(audible("hostage/hwavect/helpme1.wav") == 1, "the same sample from a non-hostage remains audible") &&
		passed;
	engine.pfnSzFromIndex = originalStringLookup;
	return passed;
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
	if (!testNativeBotEvents(runtime, engine, globals))
		return false;
	if (!testHostageRescueCall(runtime, engine))
		return false;
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
