#include "plugin/plugin_runtime.hpp"
namespace
{
bool testFriendDeath(astrabot::metamod::PluginRuntime &, enginefuncs_t &, globalvars_t &);
void setupFriendDeath(enginefuncs_t &, globalvars_t &);
}
#define ASTRABOT_EXTRA_ACTOR_TEST testFriendDeath
#define ASTRABOT_ACTOR_ENGINE_SETUP setupFriendDeath
#define ASTRABOT_ACTOR_PROFILE_TEXT                                                                                    \
	"Any { Name = \"Reaction Bot\" Team = any Difficulty = 1 Skill = 70 Aggression = 50 ReactionTime = 0.125 }\n"
#include "compat_actor_command_tests.cpp"
#include "fixtures/noise_runtime_nav.hpp"
namespace
{
bool friendVisible = true;
bool killerVisible = false;
bool objectiveVisible = false;
bool captureLostEnemy = false;
bool lostEnemyAudioMessage = false;
unsigned lostEnemySamples = 0;
std::vector<std::string> reactionDraws;
struct ReactionTrace : astrabot::compat::IRandomTraceSink
{
	void record(const astrabot::compat::RandomTraceRecord &r) override
	{
		if (std::strncmp(r.request.semanticId, "FriendDeath.", 12) == 0)
		{
			reactionDraws.push_back(r.request.semanticId);
		}
	}
};
ReactionTrace reactionTrace;
float ambushRoll = 0;
bool reuseVictimOnHurry = false;
astrabot::metamod::PluginRuntime *receiptRuntime = nullptr;
int receiptReconnectSlot = 0;
bool receiptObserverRecreated = false;
enum class ReceiptMutation
{
	None,
	Hurry,
	Visibility,
	Chatter
};
ReceiptMutation receiptMutation = ReceiptMutation::None;
void mutateReceipt(ReceiptMutation point)
{
	if (receiptMutation != point || !receiptRuntime || receiptReconnectSlot == 0)
	{
		return;
	}
	const int slot = receiptReconnectSlot;
	receiptMutation = ReceiptMutation::None;
	receiptRuntime->onClientDisconnect(&gEntities[slot - 1]);
	if (slot == 1)
	{
		gCreateCount = 0;
		astrabot::metamod::FakeClientHandle handle{};
		receiptObserverRecreated =
			receiptRuntime->createFakeClient("Reaction Bot", &handle) == astrabot::metamod::FakeClientResult::Created;
	}
	else
	{
		receiptRuntime->onClientPutInServer(&gEntities[slot - 1]);
	}
}
float reactionRandom(float low, float high)
{
	if (low == 10 && high == 15)
	{
		mutateReceipt(ReceiptMutation::Hurry);
		if (reuseVictimOnHurry)
		{
			++gEntities[1].serialnumber;
		}
		return 12;
	}
	if (low == 0 && high == 100)
	{
		return ambushRoll;
	}
	return hearingFixtureRandomFloat(low, high);
}
const char *reactionCvar(const char *name)
{
	if (std::strcmp(name, "bot_chatter") == 0)
	{
		mutateReceipt(ReceiptMutation::Chatter);
		return captureLostEnemy ? "normal" : "off";
	}
	const auto *v = getCvar(name);
	return v && v->string ? v->string : "";
}
int reactionMessageId(plid_t p, const char *name, int *size)
{
	if (std::strcmp(name, "SendAudio") == 0)
		return 101;
	return std::strcmp(name, "DeathMsg") == 0  ? 103
		: std::strcmp(name, "ScreenFade") == 0 ? 104
											   : siteFixtureUserMessageId(p, name, size);
}
void reactionTraceLine(const float *, const float *end, int, edict_t *, TraceResult *trace)
{
	mutateReceipt(ReceiptMutation::Visibility);
	*trace = {};
	trace->vecEndPos = Vector(end[0], end[1], end[2]);
	trace->flFraction =
		((objectiveVisible && end[0] == 50) || (end[0] < 0 ? friendVisible : killerVisible)) ? 1.0f : 0.0f;
}
void setupFriendDeath(enginefuncs_t &engine, globalvars_t &)
{
	engine.pfnCVarGetString = &reactionCvar;
	engine.pfnRandomFloat = &reactionRandom;
	engine.pfnLoadFileForMe = [](char *name, int *length) -> unsigned char * {
		static unsigned char bank[] = "Chatter LostEnemy test/lost_enemy.wav End";
		const bool matches = std::strcmp(name, "BotChatter.db") == 0;
		*length = matches ? static_cast<int>(sizeof(bank) - 1U) : 0;
		return matches ? bank : nullptr;
	};
	engine.pfnFreeFile = [](void *) {};
	engine.pfnGetFileSize = [](char *) { return 100; };
	engine.pfnGetApproxWavePlayLen = [](const char *) -> unsigned int { return 200; };
	engine.pfnMessageBegin = [](int, int type, const float *, edict_t *recipient) {
		lostEnemyAudioMessage = type == 101 && recipient == &gEntities[4];
	};
	engine.pfnWriteByte = [](int) {};
	engine.pfnWriteShort = [](int) {};
	engine.pfnWriteString = [](const char *value) {
		if (lostEnemyAudioMessage && std::strcmp(value, "test/lost_enemy.wav") == 0)
			++lostEnemySamples;
	};
	engine.pfnMessageEnd = [] { lostEnemyAudioMessage = false; };
	gpMetaUtilFuncs->pfnGetUserMsgID = &reactionMessageId;
}
bool testFriendDeath(astrabot::metamod::PluginRuntime &runtime, enginefuncs_t &engine, globalvars_t &globals)
{
	using namespace astrabot;
	engine.pfnTraceLine = &reactionTraceLine;
	runtime.giveEnginePointers(&engine, &globals);
	setCvarFloat("bot_auto_vacate", 0);
	globals.maxClients = 5;
	gObjectiveEntitiesAvailable = false;
	for (unsigned i = 1; i < 5; ++i)
	{
		auto &p = gEntities[i];
		p.serialnumber = static_cast<int>(i);
		p.free = false;
		p.v.flags = FL_CLIENT | FL_ONGROUND;
		p.v.health = 0;
		p.v.deadflag = DEAD_DEAD;
	}
	auto &self = gEntities[0];
	self.v.flags = FL_CLIENT | FL_FAKECLIENT | FL_ONGROUND;
	self.v.health = 100;
	self.v.deadflag = DEAD_NO;
	self.v.origin = Vector(0, 0, 36);
	self.v.view_ofs = Vector(0, 0, 17);
	self.v.maxspeed = 240;
	self.v.weapons = 1 << 28;
	auto &victim = gEntities[1];
	victim.v.origin = Vector(-500, 0, 36);
	victim.v.team = 1;
	auto &killer = gEntities[2];
	killer.v.origin = Vector(1000, 0, 36);
	killer.v.team = 2;
	killer.v.health = 100;
	killer.v.deadflag = DEAD_NO;
	enginefuncs_t hooks{};
	int version = ENGINE_INTERFACE_VERSION;
	if (!runtime.provideEngineFunctions(&hooks, &version))
	{
		return false;
	}
	const auto team = [&](int slot, const char *name)
	{
		hooks.pfnMessageBegin(MSG_ALL, 86, nullptr, nullptr);
		hooks.pfnWriteByte(slot);
		hooks.pfnWriteString(name);
		hooks.pfnMessageEnd();
	};
	team(2, "TERRORIST");
	team(3, "CT");
	const auto weapon = [&](int id)
	{
		hooks.pfnMessageBegin(MSG_ONE, 94, nullptr, &self);
		hooks.pfnWriteByte(1);
		hooks.pfnWriteByte(id);
		hooks.pfnWriteByte(30);
		hooks.pfnMessageEnd();
	};
	const auto step = [&](float dt)
	{
		globals.time += dt;
		runtime.onStartFrame();
		runtime.onStartFramePost();
	};
	auto &services = static_cast<metamod::BotBehaviorServices &>(runtime);
	auto &navigation = static_cast<metamod::BotNavigationServices &>(runtime);
	services.service_compatibilityRandomSource().setTraceSink(&reactionTrace);
	if (!check(loadHearingNav(runtime), "friend reaction fixture loads real NAV"))
	{
		return false;
	}
	runtime.beginRoundGeneration();
	weapon(28);
	step(.12f);
	step(.12f);
	self.v.angles = Vector(0, 0, 0);
	self.v.v_angle = self.v.angles;
	reactionDraws.clear();
	hooks.pfnMessageBegin(MSG_ALL, 103, nullptr, nullptr);
	hooks.pfnWriteByte(3);
	hooks.pfnWriteByte(2);
	hooks.pfnWriteByte(0);
	hooks.pfnWriteString("ak47");
	hooks.pfnMessageEnd();
	if (!check(navigation.behavior_managedBotStateMachines(0).attackOverlayActive(),
			"witnessed rear friend death immediately attacks hidden killer"))
	{
		return false;
	}
	if (!check(reactionDraws.size() == 1 && reactionDraws[0] == "FriendDeath.Hurry",
			"native Hurry draw only before direct attack"))
	{
		return false;
	}
	killer.v.origin.x = 2000;
	step(.12f);
	world::WorldSnapshot belief;
	services.observation_readManagedWorldSnapshot(0, &belief);
	const auto &machine = navigation.behavior_managedBotStateMachines(0);
	nav::NavVector target{};
	if (!check(machine.state() == compat::CompatibilityStateId::MoveTo &&
				machine.task() == compat::CompatibilityTaskId::MoveToLastKnownEnemyPosition &&
				navigation.behavior_buildManagedObjectiveTarget(0, &target, nullptr) && target.x == 1000,
			"unseen killer pursuit uses immutable event-seeded position"))
	{
		return false;
	}
	for (std::size_t i = 0; i < belief.actorCount(); ++i)
	{
		const auto *contact = belief.actorAt(i);
		if (contact->actor.slot == 3 &&
			!check(
				!contact->visible && contact->position.x == 0, "event exception never becomes visible world contact"))
		{
			return false;
		}
	}
	if (!check((gLastRunPlayerMoveButtons & IN_ATTACK) == 0, "hidden event position alone cannot grant fire"))
	{
		return false;
	}
	const auto selectState = [&](compat::CompatibilityStateId state, bool attack = false,
		compat::CompatibilityTaskId hideTask = compat::CompatibilityTaskId::None)
	{
		auto &machine = const_cast<compat::CompatibilityStateMachine &>(navigation.behavior_managedBotStateMachines(0));
		compat::StateUpdateContext context{};
		const auto &life = services.service_lifecycle();
		const auto &handle = services.service_managedBotHandles()[0];
		context.actor = {handle.actor.slot, handle.actor.actorGeneration};
		context.frame = {life.mapGeneration(), life.roundGeneration(), services.service_adapterFrameCount()};
		context.timestamp = globals.time;
		context.fullUpdate = true;
		context.fullUpdateSequence = services.service_managedBotFullUpdateSequences()[0];
		context.alive = true;
		context.roundActive = true;
		context.hideTask = hideTask;
		(void)machine.transition(state, compat::StateTransitionReason::ExplicitRequest,
			compat::StateTransitionId::ExplicitStateChange, context);
		if (attack)
		{
			(void)machine.beginAttack(context);
		}
	};
	const auto death = [&](int killerSlot = 3)
	{
		hooks.pfnMessageBegin(MSG_ALL, 103, nullptr, nullptr);
		hooks.pfnWriteByte(killerSlot);
		hooks.pfnWriteByte(2);
		hooks.pfnWriteByte(0);
		hooks.pfnWriteString("ak47");
		hooks.pfnMessageEnd();
	};
	const auto begin = [&]()
	{
		runtime.beginRoundGeneration();
		self.v.health = 100;
		self.v.deadflag = DEAD_NO;
		self.v.origin = Vector(0, 0, 36);
		self.v.flags = FL_CLIENT | FL_FAKECLIENT | FL_ONGROUND;
		self.v.maxspeed = 240;
		self.v.weapons = 1 << 28;
		self.v.angles = Vector(0, 0, 0);
		self.v.v_angle = self.v.angles;
		killer.v.origin = Vector(1000, 0, 36);
		killer.v.health = 100;
		killer.v.deadflag = DEAD_NO;
		friendVisible = true;
		killerVisible = false;
		ambushRoll = 0;
		reuseVictimOnHurry = false;
		team(1, "TERRORIST");
		team(2, "TERRORIST");
		team(3, "CT");
		weapon(28);
		step(.12f);
		step(.12f);
		selectState(compat::CompatibilityStateId::Hunt);
		reactionDraws.clear();
	};
	begin();
	friendVisible = false;
	death(0);
	if (!check(navigation.behavior_managedBotStateMachines(0).state() == compat::CompatibilityStateId::Idle &&
				reactionDraws.empty(),
			"world-killed unseen teammate still rethinks Hunt without RNG"))
	{
		return false;
	}
	begin();
	selectState(compat::CompatibilityStateId::Hunt, true);
	death(0);
	if (!check(navigation.behavior_managedBotStateMachines(0).state() == compat::CompatibilityStateId::Idle &&
				!navigation.behavior_managedBotStateMachines(0).attackOverlayActive(),
			"Hunt death branch wins over attack overlay"))
	{
		return false;
	}
	begin();
	selectState(compat::CompatibilityStateId::Hide);
	weapon(29);
	self.v.weapons = (1 << 18) | (1 << 29);
	death();
	if (!check(!navigation.behavior_managedBotStateMachines(0).attackOverlayActive() && reactionDraws.empty(),
			"sniper in inventory excludes retaliation even while using knife"))
	{
		return false;
	}
	begin();
	selectState(compat::CompatibilityStateId::Hide);
	weapon(29);
	self.v.weapons = 1 << 29;
	ambushRoll = 50;
	death();
	if (!check(!navigation.behavior_managedBotStateMachines(0).attackOverlayActive() && reactionDraws.size() == 2 &&
				reactionDraws[0] == "FriendDeath.Hurry" && reactionDraws[1] == "FriendDeath.KnifeAmbush",
			"hiding knife rolls after Hurry and strict50 keeps ambush"))
	{
		return false;
	}
	begin();
	selectState(compat::CompatibilityStateId::Hide);
	weapon(29);
	self.v.weapons = 1 << 29;
	ambushRoll = 49;
	death();
	if (!check(navigation.behavior_managedBotStateMachines(0).attackOverlayActive() && reactionDraws.size() == 2,
			"hiding knife below50 retaliates"))
	{
		return false;
	}
	begin();
	const auto actorKey = world::ActorKey{services.service_managedBotHandles()[0].actor.slot,
		services.service_managedBotHandles()[0].actor.actorGeneration};
	static_cast<metamod::BotCommandServices &>(runtime).observation_observeReloadDispatched(actorKey,
		{services.service_lifecycle().mapGeneration(), services.service_lifecycle().roundGeneration(),
			services.service_adapterFrameCount()},
		globals.time);
	death();
	if (!check(!navigation.behavior_managedBotStateMachines(0).attackOverlayActive() &&
				navigation.behavior_managedBotStateMachines(0).state() == compat::CompatibilityStateId::Hunt &&
				reactionDraws.size() == 1,
			"reloading Attack rejects but preserves native early return instead of Hunt rethink"))
	{
		return false;
	}
	begin();
	selectState(compat::CompatibilityStateId::DefuseBomb);
	death();
	if (!check(!navigation.behavior_managedBotStateMachines(0).attackOverlayActive() && reactionDraws.empty(),
			"defuse self-defense disposition excludes death retaliation"))
	{
		return false;
	}
	begin();
	selectState(compat::CompatibilityStateId::Hide);
	hooks.pfnMessageBegin(MSG_ONE, 104, nullptr, &self);
	hooks.pfnWriteShort(4096);
	hooks.pfnWriteShort(0);
	hooks.pfnWriteShort(0);
	for (int i = 0; i < 4; ++i)
	{
		hooks.pfnWriteByte(255);
	}
	hooks.pfnMessageEnd();
	death();
	if (!check(!navigation.behavior_managedBotStateMachines(0).attackOverlayActive() && reactionDraws.empty(),
			"public flash blocks witnessed-friend point visibility"))
	{
		return false;
	}
	step(1.1f);
	begin();
	selectState(compat::CompatibilityStateId::Hide);
	gEntities[4].v.origin = Vector(-250, 0, 36);
	hooks.pfnEmitSound(&gEntities[4], 1, "weapons/sg_explode.wav", 1, 1, 0, 100);
	death();
	if (!check(!navigation.behavior_managedBotStateMachines(0).attackOverlayActive() && reactionDraws.empty(),
			"public smoke blocks witnessed-friend point visibility"))
	{
		return false;
	}
	step(4.1f);
	begin();
	reuseVictimOnHurry = true;
	death();
	if (!check(!navigation.behavior_managedBotStateMachines(0).attackOverlayActive() && reactionDraws.size() == 1,
			"victim reused during Hurry RNG cannot transfer a native position exception"))
	{
		return false;
	}
	begin();
	// Acquire the enemy while holding position so the death event can use the retained contact.
	selectState(compat::CompatibilityStateId::Hide, false, compat::CompatibilityTaskId::HoldPosition);
	killerVisible = true;
	for (int i = 0; i < 6; ++i)
	{
		self.v.angles = Vector(0, 0, 0);
		self.v.v_angle = self.v.angles;
		step(.12f);
	}
	if (!check(navigation.behavior_managedBotStateMachines(0).attackOverlayActive() &&
				navigation.behavior_managedBotStateMachines(0).state() == compat::CompatibilityStateId::Hide &&
				static_cast<metamod::BotObservationServices &>(runtime).behavior_currentEnemySource(0).id == 3U,
			"real perception acquires the enemy from Hide before the lost-sight death reaction"))
	{
		return false;
	}
	killerVisible = false;
	step(.5f);
	if (!check(navigation.behavior_managedBotStateMachines(0).attackOverlayActive(),
			"temporary loss of sight retains native attack for0.4second reaction"))
	{
		return false;
	}
	reactionDraws.clear();
	death(0);
	if (!check(navigation.behavior_managedBotStateMachines(0).task() ==
					compat::CompatibilityTaskId::MoveToLastKnownEnemyPosition &&
				!navigation.behavior_managedBotStateMachines(0).attackOverlayActive() && reactionDraws.empty(),
			"world-killed teammate causes stale attack to MoveTo last known enemy"))
	{
		return false;
	}
	killerVisible = true;
	step(.4f);
	if (!check(!navigation.behavior_managedBotStateMachines(0).attackOverlayActive() &&
				navigation.behavior_managedBotStateMachines(0).task() ==
					compat::CompatibilityTaskId::MoveToLastKnownEnemyPosition,
			"IgnoreEnemies keeps movement without re-entering attack while enemy becomes visible"))
	{
		return false;
	}
	if (!check((gLastRunPlayerMoveButtons & IN_ATTACK) != 0 &&
				std::hypot(gLastRunPlayerMoveForward, gLastRunPlayerMoveSide) > 0,
			"native IgnoreEnemies permits visible-target firing while following the chase path"))
	{
		return false;
	}
	step(.7f);
	if (!check(navigation.behavior_managedBotStateMachines(0).attackOverlayActive(),
			"native ignore window expires and visible enemy can re-engage"))
	{
		return false;
	}
	self.v.angles = Vector(0, 180, 0);
	self.v.v_angle = self.v.angles;
	auto &unknown = gEntities[3];
	unknown.v.flags = FL_CLIENT | FL_ONGROUND;
	unknown.v.deadflag = DEAD_NO;
	unknown.v.health = 100;
	unknown.v.origin = Vector(1200, 0, 36);
	team(4, "CT");
	step(.12f);
	services.observation_readManagedWorldSnapshot(0, &belief);
	bool acquiredBehind = false;
	bool unknownBehind = false;
	for (std::size_t i = 0; i < belief.actorCount(); ++i)
	{
		const auto *contact = belief.actorAt(i);
		if (contact->actor.slot == 3)
		{
			acquiredBehind = contact->visible;
		}
		if (contact->actor.slot == 4)
		{
			unknownBehind = contact->visible;
		}
	}
	if (!check(acquiredBehind && !unknownBehind,
			"only the acquired enemy bypasses FOV; unknown contacts retain the normal cone"))
	{
		return false;
	}
	begin();
	death();
	step(.12f);
	++killer.serialnumber;
	step(.12f);
	if (!check(navigation.behavior_managedBotStateMachines(0).state() != compat::CompatibilityStateId::MoveTo &&
				!static_cast<metamod::BotObservationServices &>(runtime).behavior_currentEnemySource(0).isValid(),
			"reused enemy task entity cancels pursuit without inheriting replacement position"))
	{
		return false;
	}
	begin();
	team(1, "CT");
	team(2, "CT");
	team(3, "TERRORIST");
	death();
	hooks.pfnMessageBegin(MSG_ALL, 98, nullptr, nullptr);
	hooks.pfnWriteByte(4);
	hooks.pfnWriteString("#Bomb_Planted");
	hooks.pfnMessageEnd();
	step(.12f);
	if (!check(navigation.behavior_managedBotStateMachines(0).task() !=
				compat::CompatibilityTaskId::MoveToLastKnownEnemyPosition,
			"CT planted-bomb notice cancels enemy pursuit as native MoveTo does"))
	{
		return false;
	}
	begin();
	death();
	if (!check(static_cast<metamod::BotObservationServices &>(runtime).behavior_currentEnemySource(0).isValid(),
			"death seed exists before lifecycle cancellation"))
	{
		return false;
	}
	hooks.pfnMessageBegin(MSG_ALL, 103, nullptr, nullptr);
	hooks.pfnWriteByte(3);
	hooks.pfnWriteByte(1);
	hooks.pfnWriteByte(0);
	hooks.pfnWriteString("ak47");
	hooks.pfnMessageEnd();
	if (!check(!static_cast<metamod::BotObservationServices &>(runtime).behavior_currentEnemySource(0).isValid(),
			"observer public death clears receipt before possible same-frame revival"))
	{
		return false;
	}
	begin();
	death();
	runtime.beginRoundGeneration();
	if (!check(!static_cast<metamod::BotObservationServices &>(runtime).behavior_currentEnemySource(0).isValid(),
			"round transition cancels seeded enemy"))
	{
		return false;
	}
	begin();
	death();
	const float timeBeforeRollback = globals.time;
	globals.time -= 1;
	step(.12f);
	step(.12f);
	if (!check(!static_cast<metamod::BotObservationServices &>(runtime).behavior_currentEnemySource(0).isValid(),
			"clock rollback cannot revive event seed"))
	{
		return false;
	}
	globals.time = timeBeforeRollback;
	auto &listener = gEntities[4];
	const auto originalListener = listener.v;
	const auto originalListenerName = gEntityNames[4];
	const auto startLostEnemyCapture = [&] {
		captureLostEnemy = true;
		lostEnemySamples = 0;
		gEntityNames[4] = "Pursuit listener";
		listener.v.netname = 104;
		listener.v.team = 1;
		listener.v.health = 100.0f;
		listener.v.deadflag = DEAD_NO;
		listener.v.origin = Vector(-700, 0, 36);
		team(5, "TERRORIST");
	};
	const auto stopLostEnemyCapture = [&] {
		captureLostEnemy = false;
		listener.v = originalListener;
		gEntityNames[4] = originalListenerName;
	};
	const auto letChatterPlay = [&] {
		for (int i = 0; i < 16; ++i)
			step(.125f);
	};
	begin();
	startLostEnemyCapture();
	death();
	const auto &pursuit = navigation.behavior_managedBotStateMachines(0);
	for (int i = 0; i < 8 && pursuit.task() != compat::CompatibilityTaskId::MoveToLastKnownEnemyPosition; ++i)
		step(.12f);
	if (!check(pursuit.task() == compat::CompatibilityTaskId::MoveToLastKnownEnemyPosition && lostEnemySamples == 0,
			"the chase starts and has not yet reported LostEnemy"))
		return false;
	self.v.origin = Vector(1000, 0, 36);
	step(.12f);
	step(.12f);
	if (!check(navigation.behavior_managedBotStateMachines(0).task() !=
				compat::CompatibilityTaskId::MoveToLastKnownEnemyPosition,
			"actual navigation arrival finishes pursuit"))
	{
		return false;
	}
	letChatterPlay();
	if (!check(lostEnemySamples == 1, "a chase that reaches its destination says LostEnemy once"))
		return false;
	stopLostEnemyCapture();
	begin();
	startLostEnemyCapture();
	death();
	step(.12f);
	killer.v.health = 0.0f;
	killer.v.deadflag = DEAD_DEAD;
	letChatterPlay();
	if (!check(lostEnemySamples == 0, "the enemy dying ends the chase without LostEnemy"))
		return false;
	stopLostEnemyCapture();
	const auto staleAttack = [&](compat::CompatibilityStateId from)
	{
		begin();
		killerVisible = true;
		for (int i = 0; i < 6; ++i)
		{
			self.v.angles = Vector(0, 0, 0);
			self.v.v_angle = self.v.angles;
			step(.12f);
		}
		selectState(from, true);
		killerVisible = false;
		globals.time += .5f;
		death(0);
		killerVisible = true;
		step(.12f);
		return navigation.behavior_managedBotStateMachines(0).attackOverlayActive();
	};
	if (!check(staleAttack(compat::CompatibilityStateId::MoveTo),
			"MoveTo exit cancels new IgnoreEnemies before visible re-engagement") ||
		!check(staleAttack(compat::CompatibilityStateId::Follow),
			"attacking Follow nested Idle exit cancels new IgnoreEnemies"))
	{
		return false;
	}
	begin();
	self.v.angles = Vector(0, 90, 0);
	self.v.v_angle = self.v.angles;
	death();
	step(.12f);
	const auto cachedFull = services.service_managedBotFullUpdateSequences()[0];
	const auto cachedCommand = services.command_managedBotCommandSequences(0);
	const float rememberedYaw = gLastRunPlayerMoveYaw;
	killer.v.origin = Vector(-2000, -2000, 36);
	step(.04f);
	if (!check(services.service_managedBotFullUpdateSequences()[0] == cachedFull &&
				services.command_managedBotCommandSequences(0) > cachedCommand &&
				gLastRunPlayerMoveYaw < rememberedYaw && gLastRunPlayerMoveYaw >= 0 &&
				(gLastRunPlayerMoveButtons & IN_ATTACK) == 0,
			"cached command keeps aiming at remembered point without hidden-position refresh or firing"))
	{
		return false;
	}
	begin();
	const char *brokenPath = "astrabot_friend_death_broken.nav";
	if (!check(writeTwoSiteNavigation(brokenPath, false), "writes disconnected navigation fixture"))
	{
		return false;
	}
	const metamod::NavLoadRequest brokenRequest = {
		brokenPath, "astrabot_friend_death_broken", runtime.snapshot().mapGeneration, false, 0, false, 0};
	const auto brokenLoaded = runtime.loadNavigationFile(&brokenRequest);
	std::remove(brokenPath);
	if (!check(brokenLoaded == metamod::NavLoadResult::Loaded, "loads actual disconnected NAV"))
	{
		return false;
	}
	self.v.origin = Vector(16, 32, 36);
	killer.v.origin = Vector(224, 32, 36);
	startLostEnemyCapture();
	death();
	step(.12f);
	step(.12f);
	if (!check(navigation.behavior_managedBotStateMachines(0).task() !=
				compat::CompatibilityTaskId::MoveToLastKnownEnemyPosition,
			"actual disconnected route failure finishes pursuit"))
		return false;
	letChatterPlay();
	if (!check(lostEnemySamples == 1, "a failed pursuit path with a living enemy says LostEnemy once") ||
		!check(loadHearingNav(runtime), "restores hearing NAV after route failure"))
		return false;
	stopLostEnemyCapture();
	begin();
	gObjectiveEntitiesAvailable = true;
	globals.maxEntities = 8;
	globals.mapname = 6;
	gPlantedBomb = {};
	gPlantedBomb.v.classname = 2;
	gPlantedBomb.v.model = 3;
	gPlantedBomb.serialnumber = 9;
	gPlantedBomb.v.origin = Vector(50, 0, 36);
	gPlantedBomb.v.dmgtime = globals.time + 100;
	objectiveVisible = true;
	hooks.pfnMessageBegin(MSG_ALL, 98, nullptr, nullptr);
	hooks.pfnWriteByte(4);
	hooks.pfnWriteString("#Bomb_Planted");
	hooks.pfnMessageEnd();
	step(.12f);
	step(.12f);
	const auto guardBeforePursuit = navigation.behavior_scenarioDecisions(0);
	if (!check(guardBeforePursuit.action == compat::ScenarioAction::Guard && guardBeforePursuit.hasTarget &&
				guardBeforePursuit.target.inUseRange,
			"actual known planted-bomb scenario selects arrived Guard"))
	{
		return false;
	}
	killerVisible = false;
	selectState(compat::CompatibilityStateId::Hide);
	death();
	step(.12f);
	if (!check(navigation.behavior_managedBotStateMachines(0).task() ==
					compat::CompatibilityTaskId::MoveToLastKnownEnemyPosition &&
				std::hypot(gLastRunPlayerMoveForward, gLastRunPlayerMoveSide) > 0 &&
				(gLastRunPlayerMoveButtons & IN_USE) == 0,
			"ordinary planted-bomb Guard cannot stop delivered pursuit movement"))
	{
		return false;
	}
	gObjectiveEntitiesAvailable = false;
	objectiveVisible = false;
	gPlantedBomb = {};
	bool receiptsSafe = true;
	receiptRuntime = &runtime;
	for (const auto point : {ReceiptMutation::Hurry, ReceiptMutation::Visibility, ReceiptMutation::Chatter})
	{
		for (int slot : {2, 3})
		{
			begin();
			runtime.onClientPutInServer(&victim);
			runtime.onClientPutInServer(&killer);
			team(2, "TERRORIST");
			team(3, "CT");
			receiptReconnectSlot = slot;
			receiptMutation = point;
			death();
			receiptsSafe = check(!navigation.behavior_managedBotStateMachines(0).attackOverlayActive() &&
								   !static_cast<metamod::BotObservationServices &>(runtime)
										.behavior_currentEnemySource(0)
										.isValid(),
							   "same-serial reconnect during callback cannot inherit death receipt") &&
				receiptsSafe;
		}
	}
	begin();
	receiptReconnectSlot = 1;
	receiptMutation = ReceiptMutation::Chatter;
	hooks.pfnMessageBegin(MSG_ONE, 103, nullptr, &self);
	hooks.pfnWriteByte(3);
	hooks.pfnWriteByte(2);
	hooks.pfnWriteByte(0);
	hooks.pfnWriteString("ak47");
	hooks.pfnMessageEnd();
	receiptsSafe =
		check(receiptObserverRecreated && !navigation.behavior_managedBotStateMachines(0).attackOverlayActive() &&
				!static_cast<metamod::BotObservationServices &>(runtime).behavior_currentEnemySource(0).isValid(),
			"targeted receipt cannot transfer to recreated same-serial managed observer") &&
		receiptsSafe;
	receiptRuntime = nullptr;
	receiptReconnectSlot = 0;
	if (!receiptsSafe)
	{
		return false;
	}
	begin();
	runtime.onClientPutInServer(&killer);
	team(3, "CT");
	death();
	if (!check(static_cast<metamod::BotObservationServices &>(runtime).behavior_currentEnemySource(0).isValid(),
			"connected killer establishes qualified event enemy"))
	{
		return false;
	}
	runtime.onClientDisconnect(&killer);
	if (!check(!static_cast<metamod::BotObservationServices &>(runtime).behavior_currentEnemySource(0).isValid(),
			"disconnect invalidates enemy connection even before edict serial changes"))
	{
		return false;
	}
	runtime.onClientPutInServer(&killer);
	team(3, "CT");
	begin();
	death();
	runtime.onServerDeactivate();
	return check(!static_cast<metamod::BotObservationServices &>(runtime).behavior_currentEnemySource(0).isValid(),
		"map deactivation cancels event seed");
}
}
