#include "plugin/plugin_runtime.hpp"
namespace
{
void setupInterrupt(enginefuncs_t &, globalvars_t &);
bool testInterrupt(astrabot::metamod::PluginRuntime &, enginefuncs_t &, globalvars_t &);
}
#define ASTRABOT_EXTRA_ACTOR_TEST testInterrupt
#define ASTRABOT_ACTOR_ENGINE_SETUP setupInterrupt
#define ASTRABOT_ACTOR_PROFILE_TEXT                                                                                    \
	"Any { Name = \"Interrupt Speaker\" Team = any Difficulty = 1 Skill = 70 VoiceBank = interrupt.db VoicePitch = 107 }\n"
#include "compat_actor_command_tests.cpp"
#include "fixtures/noise_runtime_nav.hpp"
#include <cstdlib>
namespace
{
const char *interruptMode = "off";
bool interruptDefaultMissing = false;
bool interruptSampleMissing = false;
unsigned interruptWaveProbes = 0;
bool interruptDeathDuringStatement = false;
bool interruptResetDuringPain = false;
bool interruptRoundDuringPain = false;
bool interruptDeathAfterFeedback = false;
bool interruptRefreshDuringStop = false;
bool interruptRefreshDuringIndex = false;
bool interruptVip = false;
unsigned interruptIndexMutations = 0;
unsigned interruptDirectOpens = 0;
unsigned interruptDirectSends = 0;
globalvars_t *interruptGlobals = nullptr;

void queueFreshInterrupt(bool newRound)
{
	auto &runtime = astrabot::metamod::PluginRuntime::instance();
	if (newRound)
	{
		runtime.beginRoundGeneration();
	}
	else
	{
		static_cast<astrabot::metamod::BotObservationServices &>(runtime).behavior_observePublicDeath(0);
	}
	gEntities[0].v.health = 100;
	gEntities[0].v.deadflag = DEAD_NO;
	runtime.onMessageBegin(MSG_ONE, 103, nullptr, &gEntities[0]);
	runtime.onWriteByte(0);
	runtime.onWriteByte(2);
	runtime.onWriteByte(0);
	runtime.onWriteString("world");
	runtime.onMessageEnd();
}
int interruptIndex(const edict_t *entity)
{
	if (interruptRefreshDuringIndex && entity == &gEntities[0])
	{
		interruptRefreshDuringIndex = false;
		++interruptIndexMutations;
		queueFreshInterrupt(false);
	}
	return indexOfEdict(entity);
}
const char *interruptSzFromIndex(int index)
{
	if (interruptVip && index == 10001)
	{
		return "as_interrupt";
	}
	if (interruptVip && index == 10002)
	{
		return "func_vip_safetyzone";
	}
	return szFromIndex(index);
}
void interruptRadioCommand(edict_t *self)
{
	const char *command = astrabot::metamod::HookCommandArgv(0);
	if (std::strcmp(command, "radio1") == 0)
	{
		++interruptDirectOpens;
		return;
	}
	if (std::strcmp(command, "menuselect") == 0)
	{
		if (std::strcmp(astrabot::metamod::HookCommandArgv(1), "5") == 0)
		{
			++interruptDirectSends;
			auto &runtime = astrabot::metamod::PluginRuntime::instance();
			runtime.onMessageBegin(MSG_ONE, 101, nullptr, self);
			runtime.onWriteByte(1);
			runtime.onWriteString("%!MRAD_FOLLOWME");
			runtime.onWriteShort(100);
			runtime.onMessageEnd();
		}
		return;
	}
	hookedGameClientCommand(self);
}
struct InterruptPacket
{
	int type = 0;
	int destination = 0;
	edict_t *recipient = nullptr;
	std::vector<int> numbers;
	std::string sample;
};
std::vector<InterruptPacket> interruptPackets;
std::vector<InterruptPacket> interruptStack;
const char *interruptCvar(const char *name)
{
	if (std::strcmp(name, "bot_chatter") == 0)
	{
		return interruptMode;
	}
	const auto *value = getCvar(name);
	return value && value->string ? value->string : "";
}
int interruptCareer()
{
	return 1;
}
unsigned char *interruptLoad(char *name, int *length)
{
	const char *text = nullptr;
	if (std::strcmp(name, "BotChatter.db") == 0)
	{
		text = interruptDefaultMissing
			? "Chatter CommanderDown Important down.wav End Chatter Affirmative ack.wav End Chatter OnMyWay ack.wav End"
			: "Chatter CommanderDown Important down.wav End Chatter pain Radio EVENT_RADIO_NEED_BACKUP pain.wav End Chatter Affirmative ack.wav End Chatter OnMyWay ack.wav End";
	}
	if (std::strcmp(name, "interrupt.db") == 0)
	{
		text =
			"BaseDir radio/test/ Chatter CommanderDown Count Many Place UNDEFINED down.wav End Chatter Affirmative ack.wav End Chatter OnMyWay ack.wav End Chatter pain Place UNDEFINED Count Many wrong_place.wav Place ANY Count 0 wrong_count.wav Count Many pain.wav End";
	}
	if (!text)
	{
		*length = 0;
		return nullptr;
	}
	*length = static_cast<int>(std::strlen(text));
	auto *bytes = new unsigned char[static_cast<std::size_t>(*length) + 1];
	std::memcpy(bytes, text, static_cast<std::size_t>(*length) + 1);
	return bytes;
}
void interruptFree(void *bytes)
{
	delete[] static_cast<unsigned char *>(bytes);
}
int interruptSize(char *path)
{
	return interruptSampleMissing && std::strcmp(path, "sound/radio/test/pain.wav") == 0 ? 0 : 100;
}
unsigned interruptWave(const char *)
{
	++interruptWaveProbes;
	return 1000;
}
int interruptMessageId(plid_t plugin, const char *name, int *size)
{
	if (interruptRefreshDuringStop && std::strcmp(name, "BotVoice") == 0)
	{
		interruptRefreshDuringStop = false;
		queueFreshInterrupt(true);
		interruptGlobals->time += .12f;
		auto &runtime = astrabot::metamod::PluginRuntime::instance();
		runtime.onStartFrame();
		runtime.onStartFramePost();
	}
	if (std::strcmp(name, "ScoreAttrib") == 0)
	{
		return 104;
	}
	if (std::strcmp(name, "DeathMsg") == 0)
		return 103;
	if (std::strcmp(name, "SendAudio") == 0)
		return 101;
	if (std::strcmp(name, "BotVoice") == 0)
		return 102;
	return siteFixtureUserMessageId(plugin, name, size);
}
void interruptBegin(int destination, int type, const float *, edict_t *recipient)
{
	InterruptPacket packet;
	packet.type = type;
	packet.destination = destination;
	packet.recipient = recipient;
	interruptStack.push_back(packet);
}
void interruptNumber(int value)
{
	if (!interruptStack.empty())
		interruptStack.back().numbers.push_back(value);
}
void interruptString(const char *sample)
{
	if (!interruptStack.empty())
	{
		interruptStack.back().sample = sample ? sample : "";
	}
	if (interruptDeathAfterFeedback && sample && std::strcmp(sample, "radio/test/down.wav") == 0 &&
		!interruptStack.empty() && interruptStack.back().recipient == &gEntities[1])
	{
		interruptDeathAfterFeedback = false;
		auto &runtime = astrabot::metamod::PluginRuntime::instance();
		gEntities[0].v.health = 0;
		gEntities[0].v.deadflag = DEAD_DEAD;
		runtime.onMessageBegin(MSG_ALL, 103, nullptr, nullptr);
		runtime.onWriteByte(0);
		runtime.onWriteByte(1);
		runtime.onWriteByte(0);
		runtime.onWriteString("world");
		runtime.onMessageEnd();
	}
	if (interruptDeathDuringStatement && sample && std::strcmp(sample, "radio/test/down.wav") == 0)
	{
		interruptDeathDuringStatement = false;
		auto &runtime = astrabot::metamod::PluginRuntime::instance();
		gEntities[0].v.health = 0;
		gEntities[0].v.deadflag = DEAD_DEAD;
		runtime.onMessageBegin(MSG_ALL, 103, nullptr, nullptr);
		runtime.onWriteByte(0);
		runtime.onWriteByte(1);
		runtime.onWriteByte(0);
		runtime.onWriteString("world");
		runtime.onMessageEnd();
	}
	if (interruptResetDuringPain && sample && std::strcmp(sample, "radio/test/pain.wav") == 0)
	{
		interruptResetDuringPain = false;
		auto &runtime = astrabot::metamod::PluginRuntime::instance();
		static_cast<astrabot::metamod::BotObservationServices &>(runtime).behavior_observePublicDeath(0);
		gEntities[0].v.health = 100;
		gEntities[0].v.deadflag = DEAD_NO;
		runtime.onMessageBegin(MSG_ONE, 103, nullptr, &gEntities[0]);
		runtime.onWriteByte(0);
		runtime.onWriteByte(2);
		runtime.onWriteByte(0);
		runtime.onWriteString("world");
		runtime.onMessageEnd();
	}
	if (interruptRoundDuringPain && sample && std::strcmp(sample, "radio/test/pain.wav") == 0)
	{
		interruptRoundDuringPain = false;
		astrabot::metamod::PluginRuntime::instance().beginRoundGeneration();
	}
}
void interruptEnd()
{
	if (!interruptStack.empty())
	{
		interruptPackets.push_back(interruptStack.back());
		interruptStack.pop_back();
	}
}
void interruptVisibility(const float *, const float *end, int, edict_t *, TraceResult *trace)
{
	*trace = {};
	trace->vecEndPos = Vector(end[0], end[1], end[2]);
	trace->flFraction = 0;
}
void setupInterrupt(enginefuncs_t &engine, globalvars_t &globals)
{
	interruptGlobals = &globals;
#ifdef _MSC_VER
	char *value = nullptr;
	std::size_t size = 0;
	if (_dupenv_s(&value, &size, "ASTRABOT_TEST_DEATH_DEFAULT_MISSING") == 0)
	{
		interruptDefaultMissing = value && std::strcmp(value, "1") == 0;
		interruptSampleMissing = value && std::strcmp(value, "2") == 0;
	}
	std::free(value);
#else
	const char *value = std::getenv("ASTRABOT_TEST_DEATH_DEFAULT_MISSING");
	interruptDefaultMissing = value && std::strcmp(value, "1") == 0;
	interruptSampleMissing = value && std::strcmp(value, "2") == 0;
#endif
	engine.pfnCVarGetString = &interruptCvar;
	engine.pfnIsCareerMatch = &interruptCareer;
	engine.pfnLoadFileForMe = &interruptLoad;
	engine.pfnFreeFile = &interruptFree;
	engine.pfnGetFileSize = &interruptSize;
	engine.pfnGetApproxWavePlayLen = &interruptWave;
	engine.pfnMessageBegin = &interruptBegin;
	engine.pfnWriteByte = &interruptNumber;
	engine.pfnWriteShort = &interruptNumber;
	engine.pfnWriteString = &interruptString;
	engine.pfnMessageEnd = &interruptEnd;
	gpMetaUtilFuncs->pfnGetUserMsgID = &interruptMessageId;
}
bool testInterrupt(astrabot::metamod::PluginRuntime &runtime, enginefuncs_t &engine, globalvars_t &globals)
{
	using namespace astrabot;
	setupInterrupt(engine, globals);
	engine.pfnTraceLine = &interruptVisibility;
	engine.pfnIndexOfEdict = &interruptIndex;
	engine.pfnSzFromIndex = &interruptSzFromIndex;
	engine.pfnRandomFloat = &hearingFixtureRandomFloat;
	runtime.giveEnginePointers(&engine, &globals);
	setCvarFloat("bot_auto_vacate", 0);
	setCvarFloat("bot_allow_rogues", 0);
	globals.maxClients = 5;
	gObjectiveEntitiesAvailable = false;
	auto &self = gEntities[0];
	for (unsigned i = 0; i < 5; ++i)
	{
		auto &player = gEntities[i];
		player.free = false;
		if (i != 0)
		{
			player.serialnumber = static_cast<int>(i);
		}
		player.v.flags = FL_CLIENT | FL_ONGROUND;
		player.v.health = 100;
		player.v.deadflag = DEAD_NO;
		player.v.origin = Vector(-500, 0, 36);
	}
	self.v.flags |= FL_FAKECLIENT;
	self.v.origin = Vector(0, 0, 36);
	self.v.maxspeed = 240;
	enginefuncs_t hooks{};
	int version = ENGINE_INTERFACE_VERSION;
	if (!check(runtime.provideEngineFunctions(&hooks, &version), "registered interruption hooks"))
		return false;
	const auto team = [&](int slot, const char *name)
	{
		hooks.pfnMessageBegin(MSG_ALL, 86, nullptr, nullptr);
		hooks.pfnWriteByte(slot);
		hooks.pfnWriteString(name);
		hooks.pfnMessageEnd();
	};
	const auto step = [&](float seconds)
	{
		globals.time += seconds;
		runtime.onStartFrame();
		runtime.onStartFramePost();
	};
	const auto death = [&](int victim, int destination = MSG_ALL, edict_t *recipient = nullptr, int headshot = 0)
	{
		hooks.pfnMessageBegin(destination, 103, nullptr, recipient);
		hooks.pfnWriteByte(0);
		hooks.pfnWriteByte(victim);
		hooks.pfnWriteByte(headshot);
		hooks.pfnWriteString("world");
		hooks.pfnMessageEnd();
	};
	const auto begin = [&]()
	{
		runtime.beginRoundGeneration();
		interruptMode = "normal";
		self.v.flags = FL_CLIENT | FL_FAKECLIENT | FL_ONGROUND;
		self.v.health = 100;
		self.v.deadflag = DEAD_NO;
		self.v.weapons = 1 << 28;
		self.v.origin = Vector(0, 0, 36);
		for (int slot = 1; slot <= 5; ++slot)
			team(slot, slot == 4 ? "CT" : "TERRORIST");
		gEntities[1].v.health = 0;
		gEntities[1].v.deadflag = DEAD_DEAD;
		hooks.pfnMessageBegin(MSG_ONE, 94, nullptr, &self);
		hooks.pfnWriteByte(1);
		hooks.pfnWriteByte(28);
		hooks.pfnWriteByte(30);
		hooks.pfnMessageEnd();
		step(.12f);
		step(.12f);
		interruptPackets.clear();
	};
	const auto samples = [&](int slot)
	{
		std::vector<std::string> result;
		for (const auto &packet : interruptPackets)
		{
			if (packet.type == 101 && packet.recipient == &gEntities[slot - 1])
				result.push_back(packet.sample);
		}
		return result;
	};
	if (!check(loadHearingNav(runtime), "interruption fixture real NAV"))
		return false;
	begin();
	death(2);
	step(.12f);
	if (!check(samples(1) == std::vector<std::string>{"radio/test/down.wav"},
			"real CommanderDown is speaking before death"))
		return false;
	const auto probes = interruptWaveProbes;
	const auto commandsBeforeDeath = gClientCommandCount;
	interruptPackets.clear();
	self.v.health = 0;
	self.v.deadflag = DEAD_DEAD;
	death(1);
	const std::vector<std::string> expected = (interruptDefaultMissing || interruptSampleMissing)
		? std::vector<std::string>{}
		: std::vector<std::string>{"radio/test/pain.wav"};
	if (!check(samples(1) == expected, "speaking-at-death emits actual pain using globally defined profile speakable"))
		return false;
	if (!check(gClientCommandCount == commandsBeforeDeath, "pain has no standard-radio command fallback"))
	{
		return false;
	}
	if (!check(interruptWaveProbes == probes, "death interruption does not probe duration"))
		return false;
	for (const auto &packet : interruptPackets)
	{
		if (packet.type == 102 && !packet.numbers.empty() &&
			!check(packet.numbers[0] == 0, "pain never starts new voice feedback"))
			return false;
		if (packet.type == 101 &&
			!check(packet.numbers == std::vector<int>{1, 107}, "pain uses original speaker and profile pitch"))
			return false;
	}
	interruptPackets.clear();
	death(1);
	if (!check(
			samples(1).empty() && interruptStack.empty(), "repeated death cannot repeat pain and all messages close"))
	{
		return false;
	}
	if (interruptDefaultMissing || interruptSampleMissing)
	{
		return true;
	}
	unsigned speakCase = 0;
	const auto speak = [&]()
	{
		++speakCase;
		begin();
		death(2);
		step(.12f);
		const bool speaking = samples(1) == std::vector<std::string>{"radio/test/down.wav"};
		interruptPackets.clear();
		return check(speaking, "each case starts actual speech");
	};
	const auto die = [&]()
	{
		self.v.health = 0;
		self.v.deadflag = DEAD_DEAD;
		death(1);
	};
	for (const char *mode :
		{"normal", "minimal", "MINIMAL", "m", "unknown", "", "radio", "RADIO", "r", "off", "OFF", "o"})
	{
		if (!speak())
			return false;
		interruptMode = mode;
		die();
		const bool allowed = mode[0] != 'r' && mode[0] != 'R' && mode[0] != 'o' && mode[0] != 'O';
		const auto wanted = allowed ? std::vector<std::string>{"radio/test/pain.wav"} : std::vector<std::string>{};
		if (!check(samples(1) == wanted, "death uses native first-character verbosity without Important requirement"))
			return false;
	}
	begin();
	death(2);
	die();
	if (!check(samples(1).empty(), "pending unsaid statement does not produce pain"))
		return false;
	if (!speak())
		return false;
	globals.time += 2;
	die();
	if (!check(samples(1) == std::vector<std::string>{"radio/test/pain.wav"},
			"retained speaking node emits pain before its next update"))
		return false;
	if (!speak())
		return false;
	step(2);
	interruptPackets.clear();
	die();
	if (!check(samples(1).empty(), "finished speaking node removed by update does not emit pain"))
		return false;
	if (!speak())
		return false;
	gEntities[4].v.flags |= FL_SPECTATOR;
	gEntities[4].v.iuser1 = 4;
	gEntities[4].v.iuser2 = 1;
	team(5, "SPECTATOR");
	hooks.pfnMessageBegin(MSG_ONE, 98, nullptr, &gEntities[2]);
	hooks.pfnWriteByte(4);
	hooks.pfnWriteString("#Ignore_Radio");
	hooks.pfnMessageEnd();
	die();
	if (!check(samples(1) == std::vector<std::string>{"radio/test/pain.wav"} && samples(2) == samples(1) &&
				samples(5) == samples(1) && samples(3).empty() && samples(4).empty(),
			"pain reaches dead teammates and own-team spectators, respects enemy and ignore filters"))
		return false;
	hooks.pfnMessageBegin(MSG_ONE, 98, nullptr, &gEntities[2]);
	hooks.pfnWriteByte(4);
	hooks.pfnWriteString("#Accept_Radio");
	hooks.pfnMessageEnd();
	gEntities[4].v.flags &= ~FL_SPECTATOR;
	if (!speak())
		return false;
	const float beforeRollback = globals.time;
	globals.time -= 1;
	die();
	if (!check(samples(1).empty(), "clock rollback cancels old speaking death"))
		return false;
	globals.time = beforeRollback;
	if (!speak())
		return false;
	runtime.beginRoundGeneration();
	die();
	if (!check(samples(1).empty(), "new round clears death speech eligibility"))
		return false;
	if (!speak())
		return false;
	interruptResetDuringPain = true;
	die();
	if (!check(samples(1) == std::vector<std::string>{"radio/test/pain.wav"} && samples(2).empty(),
			"same-owner reset during pain stops old follow-up recipients"))
		return false;
	interruptPackets.clear();
	step(.12f);
	if (!check(samples(1) == std::vector<std::string>{"radio/test/down.wav"},
			"fresh statement queued during pain callback survives old completion"))
		return false;
	if (!speak())
		return false;
	interruptRoundDuringPain = true;
	die();
	if (!check(samples(1) == std::vector<std::string>{"radio/test/pain.wav"} && samples(2).empty(),
			"round replacement during pain stops old recipients"))
		return false;
	if (!speak())
		return false;
	death(1);
	if (!check(samples(1) == std::vector<std::string>{"radio/test/pain.wav"},
			"DeathMsg before deadflag change still interrupts speech"))
		return false;
	self.v.health = 100;
	self.v.deadflag = DEAD_NO;
	interruptPackets.clear();
	death(2);
	step(.12f);
	if (!check(samples(1) == std::vector<std::string>{"radio/test/down.wav"},
			"death releases own speaking lock before prior clip deadline"))
		return false;
	if (!speak())
		return false;
	death(1, MSG_ONE, &gEntities[2]);
	if (!check(samples(1).empty(), "DeathMsg targeted to unmanaged other cannot authorize own pain"))
		return false;
	if (!speak())
		return false;
	death(1, MSG_ALL, nullptr, 2);
	if (!check(samples(1).empty(), "invalid headshot field cannot authorize pain"))
		return false;
	if (!speak())
		return false;
	const int originalSerial = self.serialnumber;
	++self.serialnumber;
	die();
	if (!check(samples(1).empty(), "reused sender serial cannot borrow old speech"))
		return false;
	self.serialnumber = originalSerial;
	if (!speak())
		return false;
	death(1, MSG_ONE, &self);
	if (!check(samples(1) == std::vector<std::string>{"radio/test/pain.wav"},
			"own targeted DeathMsg can interrupt current speech"))
		return false;
	begin();
	interruptDeathDuringStatement = true;
	death(2);
	step(.12f);
	const auto actual = samples(1);
	if (!check(std::count(actual.begin(), actual.end(), "radio/test/pain.wav") == 1,
			"death during actual outgoing speech interrupts it with pain"))
		return false;
	bool reviewOk = true;
	begin();
	interruptDeathAfterFeedback = true;
	death(2);
	step(.12f);
	bool oldStarted = false;
	bool oldStopped = false;
	for (const auto &packet : interruptPackets)
	{
		if (packet.type == 102 && !packet.numbers.empty())
		{
			oldStarted = oldStarted || packet.numbers[0] == 1;
			oldStopped = oldStopped || packet.numbers[0] == 0;
		}
	}
	reviewOk =
		check(oldStarted && oldStopped, "review: death after a completed feedback start stops partial delivery") &&
		reviewOk;
	if (!speak())
		return false;
	interruptRefreshDuringStop = true;
	die();
	bool freshStarted = false;
	bool stopAfterFresh = false;
	for (const auto &packet : interruptPackets)
	{
		if (packet.type == 102 && !packet.numbers.empty())
		{
			if (packet.numbers[0] == 1)
				freshStarted = true;
			if (packet.numbers[0] == 0 && freshStarted)
				stopAfterFresh = true;
		}
	}
	reviewOk =
		check(freshStarted && !stopAfterFresh, "review: old feedback stop lookup cannot stop replacement speech") &&
		reviewOk;
	const auto humanGo = [&]()
	{
		hooks.pfnMessageBegin(MSG_ONE, 101, nullptr, &self);
		hooks.pfnWriteByte(3);
		hooks.pfnWriteString("%!MRAD_GO");
		hooks.pfnWriteShort(100);
		hooks.pfnMessageEnd();
	};
	begin();
	humanGo();
	step(2);
	if (!check(samples(1) == std::vector<std::string>{"radio/test/ack.wav"},
			"control: accepted public human Go command delivers acknowledgement"))
		return false;
	begin();
	humanGo();
	step(.12f);
	interruptPackets.clear();
	death(1);
	self.v.health = 100;
	self.v.deadflag = DEAD_NO;
	step(2);
	reviewOk = check(samples(1).empty(), "review: death cancels delayed acknowledgement across same-round revival") &&
		reviewOk;
	if (!speak())
		return false;
	interruptRefreshDuringIndex = true;
	const auto mutationsBefore = interruptIndexMutations;
	die();
	interruptPackets.clear();
	step(.12f);
	reviewOk = check(interruptIndexMutations == mutationsBefore + 1 &&
					   samples(1) == std::vector<std::string>{"radio/test/down.wav"},
				   "review: death callback identity replacement preserves fresh queued speech") &&
		reviewOk;
	begin();
	interruptVip = true;
	globals.mapname = 10001;
	gObjectiveEntitiesAvailable = true;
	gBombSite.free = false;
	gBombSite.v.classname = 10002;
	gBombSite.v.origin = Vector(300, 0, 36);
	gBombSite.v.absmin = Vector(260, -40, 0);
	gBombSite.v.absmax = Vector(340, 40, 72);
	for (int slot = 1; slot <= 5; ++slot)
		team(slot, slot == 4 ? "TERRORIST" : "CT");
	hooks.pfnMessageBegin(MSG_ALL, 104, nullptr, nullptr);
	hooks.pfnWriteByte(1);
	hooks.pfnWriteByte(4);
	hooks.pfnMessageEnd();
	gHookedGameDllTable.pfnClientCommand = &interruptRadioCommand;
	interruptDirectOpens = 0;
	interruptDirectSends = 0;
	step(.12f);
	const unsigned firstDirect = interruptDirectSends;
	reviewOk = check(static_cast<metamod::BotNavigationServices &>(runtime).behavior_scenarioDecisions(0).action ==
					   compat::ScenarioAction::Escape,
				   "control: public VIP role and safety zone produce escape decision") &&
		reviewOk;
	step(.3f);
	reviewOk = check(interruptDirectOpens == 1 && firstDirect == 1 && interruptDirectSends == 1,
				   "review: public VIP escape dispatches and consumes direct Follow me radio") &&
		reviewOk;
	return check(interruptStack.empty(), "reentrant death balances all opened messages") && reviewOk;
}
}
