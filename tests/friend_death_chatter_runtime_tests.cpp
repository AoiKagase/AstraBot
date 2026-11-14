#include "plugin/plugin_runtime.hpp"
namespace
{
void setupDeathChatter(enginefuncs_t &, globalvars_t &);
bool testDeathChatter(astrabot::metamod::PluginRuntime &, enginefuncs_t &, globalvars_t &);
}
#define ASTRABOT_EXTRA_ACTOR_TEST testDeathChatter
#define ASTRABOT_ACTOR_ENGINE_SETUP setupDeathChatter
#define ASTRABOT_ACTOR_PROFILE_TEXT                                                                                    \
	"Any { Name = \"Death Speaker\" Team = any Difficulty = 1 Skill = 70 VoiceBank = death_speech.db }\nSecond { Name = \"Second Speaker\" Team = T Difficulty = 1 Skill = 70 VoiceBank = death_speech.db }\n"
#include "compat_actor_command_tests.cpp"
#include "fixtures/noise_runtime_nav.hpp"
#include <cstdlib>
namespace
{
const char *deathSpeechMode = "off";
int deathSpeechCareer = 0;
float deathSpeechZombie = 0;
bool deathSpeechDefaultMissing = false;
bool enemyDeathVisible = false;
astrabot::metamod::PluginRuntime *mutatingSpeechRuntime = nullptr;
enum class SpeechMutation
{
	None,
	Career,
	Delay
};
SpeechMutation speechMutation = SpeechMutation::None;
int speechMutationSlot = 0;
void mutateSpeechReceipt(SpeechMutation at)
{
	if (speechMutation != at || !mutatingSpeechRuntime || speechMutationSlot == 0)
		return;
	const int slot = speechMutationSlot;
	speechMutation = SpeechMutation::None;
	mutatingSpeechRuntime->onClientDisconnect(&gEntities[slot - 1]);
	mutatingSpeechRuntime->onClientPutInServer(&gEntities[slot - 1]);
}
float deathSpeechCvarFloat(const char *name)
{
	return std::strcmp(name, "bot_zombie") == 0 ? deathSpeechZombie : getCvarFloat(name);
}
bool deathVictimVisible = false;
unsigned deathSpeechProbes = 0;
bool resetDuringVoice = false;
bool resetDuringRadio = false;
void resetSpeakerAndQueue(int killer)
{
	auto &runtime = astrabot::metamod::PluginRuntime::instance();
	static_cast<astrabot::metamod::BotObservationServices &>(runtime).behavior_observePublicDeath(0);
	gEntities[0].v.health = 100;
	gEntities[0].v.deadflag = DEAD_NO;
	runtime.onMessageBegin(MSG_ONE, 103, nullptr, &gEntities[0]);
	runtime.onWriteByte(killer);
	runtime.onWriteByte(2);
	runtime.onWriteByte(0);
	runtime.onWriteString("ak47");
	runtime.onMessageEnd();
}
int deathSpeechMessage = 0;
unsigned deathVoiceStarts = 0;
unsigned deathMessageNumbers = 0;
edict_t *deathSpeechRecipient = nullptr;
std::vector<std::string> deathSpeechSamples;
std::vector<std::string> deathSpeechDraws;
struct DeathSpeechTrace : astrabot::compat::IRandomTraceSink
{
	void record(const astrabot::compat::RandomTraceRecord &r) override
	{
		deathSpeechDraws.push_back(r.request.semanticId);
	}
};
DeathSpeechTrace deathSpeechTrace;
const char *deathSpeechCvar(const char *name)
{
	if (std::strcmp(name, "bot_chatter") == 0)
		return deathSpeechMode;
	const auto *value = getCvar(name);
	return value && value->string ? value->string : "";
}
int deathSpeechIsCareer()
{
	mutateSpeechReceipt(SpeechMutation::Career);
	return deathSpeechCareer;
}
float deathSpeechRandom(float low, float high)
{
	if (low == .5f && high == 1)
	{
		mutateSpeechReceipt(SpeechMutation::Delay);
		return .75f;
	}
	if (low == 10 && high == 15)
		return 12;
	if (low == 0 && high == 100)
		return 50;
	if (low == 2 && high == 3)
		return 2;
	if (low == 2 && high == 4)
		return 4;
	return hearingFixtureRandomFloat(low, high);
}
unsigned char *deathSpeechLoad(char *name, int *length)
{
	const char *text = nullptr;
	if (std::strcmp(name, "BotChatter.db") == 0)
		text = deathSpeechDefaultMissing
			? "BaseDir radio/test/ Chatter NiceShotSir sir.wav End"
			: "BaseDir radio/test/ Chatter CommanderDown Important down.wav End Chatter KilledFriend Radio EVENT_RADIO_NEED_BACKUP killed.wav End Chatter NiceShotSir sir.wav End";
	if (std::strcmp(name, "death_speech.db") == 0)
		text =
			"BaseDir radio/test/ Chatter CommanderDown Count Many Place UNDEFINED down.wav Count 2 Place ANY wrong_count.wav Count Many Place Speaker wrong_place.wav End Chatter KilledFriend Important Count Many Place UNDEFINED killed.wav Count 2 Place ANY wrong_count.wav End Chatter NiceShotSir sir.wav End";
	if (!text)
	{
		*length = 0;
		return nullptr;
	}
	*length = static_cast<int>(std::strlen(text));
	auto *data = new unsigned char[static_cast<std::size_t>(*length) + 1];
	std::memcpy(data, text, static_cast<std::size_t>(*length) + 1);
	return data;
}
void deathSpeechFree(void *data)
{
	delete[] static_cast<unsigned char *>(data);
}
int deathSpeechSize(char *)
{
	++deathSpeechProbes;
	return 100;
}
unsigned deathSpeechLength(const char *)
{
	return 200;
}
int deathSpeechMessageId(plid_t plugin, const char *name, int *size)
{
	if (std::strcmp(name, "DeathMsg") == 0)
		return 103;
	if (std::strcmp(name, "SendAudio") == 0)
		return 101;
	if (std::strcmp(name, "BotVoice") == 0)
		return 102;
	return siteFixtureUserMessageId(plugin, name, size);
}
void deathSpeechBegin(int, int type, const float *, edict_t *recipient)
{
	deathSpeechMessage = type;
	deathMessageNumbers = 0;
	deathSpeechRecipient = recipient;
}
void deathSpeechNumber(int value)
{
	if (deathSpeechMessage == 102 && deathSpeechRecipient == &gEntities[0] && deathMessageNumbers++ == 0 && value == 1)
		++deathVoiceStarts;
}
void deathSpeechString(const char *sample)
{
	if (deathSpeechMessage != 101 || deathSpeechRecipient != &gEntities[0])
		return;
	deathSpeechSamples.push_back(sample);
	if (resetDuringVoice && std::strcmp(sample, "radio/test/down.wav") == 0)
	{
		resetDuringVoice = false;
		resetSpeakerAndQueue(0);
	}
}
void deathSpeechEnd()
{
}
void deathSpeechVisibility(const float *, const float *end, int flags, edict_t *, TraceResult *trace)
{
	*trace = {};
	trace->vecEndPos = Vector(end[0], end[1], end[2]);
	trace->flFraction =
		flags == 0x101 && ((deathVictimVisible && end[0] == -500) || (enemyDeathVisible && end[0] == 500)) ? 1.0f
																										   : 0.0f;
}

bool missingDefaultDeathSpeechFixture()
{
#ifdef _MSC_VER
	char *value = nullptr;
	std::size_t size = 0;
	if (_dupenv_s(&value, &size, "ASTRABOT_TEST_DEATH_DEFAULT_MISSING") != 0)
		return false;
	const bool missing = value && std::strcmp(value, "1") == 0;
	std::free(value);
	return missing;
#else
	const auto *value = std::getenv("ASTRABOT_TEST_DEATH_DEFAULT_MISSING");
	return value && std::strcmp(value, "1") == 0;
#endif
}

unsigned deathRadioRequests = 0;
void deathRadioClientCommand(edict_t *self)
{
	const char *command = astrabot::metamod::HookCommandArgv(0);
	if (std::strcmp(command, "radio3") == 0)
	{
		++deathRadioRequests;
		return;
	}
	if (std::strcmp(command, "menuselect") == 0)
	{
		if (std::strcmp(astrabot::metamod::HookCommandArgv(1), "3") == 0)
		{
			auto &runtime = astrabot::metamod::PluginRuntime::instance();
			runtime.onMessageBegin(MSG_ONE, 101, nullptr, self);
			runtime.onWriteByte(1);
			runtime.onWriteString("%!MRAD_BACKUP");
			runtime.onWriteShort(100);
			runtime.onMessageEnd();
			if (resetDuringRadio)
			{
				resetDuringRadio = false;
				resetSpeakerAndQueue(3);
			}
		}
		return;
	}
	hookedGameClientCommand(self);
}
void setupDeathChatter(enginefuncs_t &engine, globalvars_t &)
{
	deathSpeechDefaultMissing = missingDefaultDeathSpeechFixture();
	engine.pfnCVarGetFloat = &deathSpeechCvarFloat;
	engine.pfnCVarGetString = &deathSpeechCvar;
	engine.pfnIsCareerMatch = &deathSpeechIsCareer;
	engine.pfnLoadFileForMe = &deathSpeechLoad;
	engine.pfnFreeFile = &deathSpeechFree;
	engine.pfnGetFileSize = &deathSpeechSize;
	engine.pfnGetApproxWavePlayLen = &deathSpeechLength;
	engine.pfnMessageBegin = &deathSpeechBegin;
	engine.pfnWriteByte = &deathSpeechNumber;
	engine.pfnWriteShort = &deathSpeechNumber;
	engine.pfnWriteString = &deathSpeechString;
	engine.pfnMessageEnd = &deathSpeechEnd;
	gpMetaUtilFuncs->pfnGetUserMsgID = &deathSpeechMessageId;
}
bool testDeathChatter(astrabot::metamod::PluginRuntime &runtime, enginefuncs_t &engine, globalvars_t &globals)
{
	using namespace astrabot;
	setupDeathChatter(engine, globals);
	engine.pfnTraceLine = &deathSpeechVisibility;
	engine.pfnRandomFloat = &deathSpeechRandom;
	runtime.giveEnginePointers(&engine, &globals);
	setCvarFloat("bot_auto_vacate", 0);
	globals.maxClients = 5;
	gObjectiveEntitiesAvailable = false;
	auto &self = gEntities[0];
	self.v.flags = FL_CLIENT | FL_FAKECLIENT | FL_ONGROUND;
	self.v.health = 100;
	self.v.deadflag = DEAD_NO;
	self.v.origin = Vector(0, 0, 36);
	self.v.maxspeed = 240;
	for (unsigned i = 1; i < 5; ++i)
	{
		auto &player = gEntities[i];
		player.free = false;
		player.serialnumber = static_cast<int>(i);
		player.v.flags = FL_CLIENT | FL_ONGROUND;
		player.v.health = 100;
		player.v.deadflag = DEAD_NO;
		player.v.origin = Vector(-500, 0, 36);
	}
	gEntities[1].v.health = 0;
	gEntities[1].v.deadflag = DEAD_DEAD;
	gEntities[3].v.origin = Vector(500, 0, 36);
	enginefuncs_t hooks{};
	int version = ENGINE_INTERFACE_VERSION;
	if (!check(runtime.provideEngineFunctions(&hooks, &version), "registered death speech hooks"))
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
	const auto selectState = [&](compat::CompatibilityStateId state)
	{
		auto &ports = static_cast<metamod::BotBehaviorServices &>(runtime);
		auto &navigation = static_cast<metamod::BotNavigationServices &>(runtime);
		auto &machine = const_cast<compat::CompatibilityStateMachine &>(navigation.behavior_managedBotStateMachines(0));
		const auto &life = ports.service_lifecycle();
		const auto &handle = ports.service_managedBotHandles()[0];
		compat::StateUpdateContext context{};
		context.actor = {handle.actor.slot, handle.actor.actorGeneration};
		context.frame = {life.mapGeneration(), life.roundGeneration(), ports.service_adapterFrameCount()};
		context.timestamp = globals.time;
		context.fullUpdate = true;
		context.fullUpdateSequence = ports.service_managedBotFullUpdateSequences()[0];
		context.alive = true;
		context.roundActive = true;
		(void)machine.transition(state, compat::StateTransitionReason::ExplicitRequest,
			compat::StateTransitionId::ExplicitStateChange, context);
	};
	const auto weapon = [&](int id)
	{
		hooks.pfnMessageBegin(MSG_ONE, 94, nullptr, &self);
		hooks.pfnWriteByte(1);
		hooks.pfnWriteByte(id);
		hooks.pfnWriteByte(30);
		hooks.pfnMessageEnd();
	};
	const auto begin = [&]()
	{
		runtime.beginRoundGeneration();
		deathSpeechMode = "normal";
		deathSpeechCareer = 1;
		deathSpeechZombie = 0;
		enemyDeathVisible = false;
		speechMutation = SpeechMutation::None;
		engine.pfnIsCareerMatch = &deathSpeechIsCareer;
		deathVictimVisible = false;
		deathSpeechSamples.clear();
		deathSpeechDraws.clear();
		self.v.flags = FL_CLIENT | FL_FAKECLIENT | FL_ONGROUND;
		self.v.origin = Vector(0, 0, 36);
		self.v.health = 100;
		self.v.deadflag = DEAD_NO;
		self.v.weapons = 1 << 28;
		self.v.angles = Vector(0, 0, 0);
		self.v.v_angle = self.v.angles;
		gEntities[1].v.flags = FL_CLIENT | FL_ONGROUND;
		gEntities[2].v.flags = FL_CLIENT | FL_ONGROUND;
		team(1, "TERRORIST");
		team(2, "TERRORIST");
		team(3, "TERRORIST");
		team(4, "CT");
		team(5, "TERRORIST");
		for (unsigned i = 2; i < 5; ++i)
		{
			gEntities[i].v.health = 100;
			gEntities[i].v.deadflag = DEAD_NO;
		}
		weapon(28);
		step(.12f);
		step(.12f);
		deathSpeechSamples.clear();
		deathSpeechDraws.clear();
		selectState(compat::CompatibilityStateId::Hunt);
	};
	const auto death = [&](int killer = 3, int victim = 2, int destination = MSG_ALL, edict_t *recipient = nullptr)
	{
		hooks.pfnMessageBegin(destination, 103, nullptr, recipient);
		hooks.pfnWriteByte(killer);
		hooks.pfnWriteByte(victim);
		hooks.pfnWriteByte(0);
		hooks.pfnWriteString("ak47");
		hooks.pfnMessageEnd();
	};
	auto &services = static_cast<metamod::BotBehaviorServices &>(runtime);
	services.service_compatibilityRandomSource().setTraceSink(&deathSpeechTrace);
	if (!check(loadHearingNav(runtime), "speech fixture real NAV"))
		return false;
	begin();
	if (deathSpeechDefaultMissing)
	{
		death();
		step(1);
		services.service_compatibilityRandomSource().setTraceSink(nullptr);
		return check(deathSpeechSamples.empty(),
			"profile-only death phrases absent from default catalog cannot authorize speech");
	}
	const auto before = deathSpeechProbes;
	death();
	if (!check(deathSpeechProbes == before, "death speech admission has no profile audio probes"))
		return false;
	if (!check(deathSpeechDraws.size() == 1 && deathSpeechDraws[0] == "RNG-CSBOT-KILLED-FRIEND-DELAY",
			"rebuke delay follows immediate CommanderDown without other event RNG"))
		return false;
	step(.12f);
	if (!check(deathSpeechSamples.size() == 1 && deathSpeechSamples[0] == "radio/test/down.wav",
			"career teammate death delivers actual CommanderDown before rebuke"))
		return false;
	step(.5f);
	if (!check(deathSpeechSamples.size() == 1, "KilledFriend respects native delay"))
		return false;
	step(.2f);
	if (!check(deathSpeechSamples.size() == 2 && deathSpeechSamples[1] == "radio/test/killed.wav",
			"distinct delayed KilledFriend survives CommanderDown speech"))
		return false;
	for (int variant = 0; variant < 8; ++variant)
	{
		begin();
		int killer = 3;
		bool expectCommander = true;
		bool expectRebuke = true;
		if (variant == 0)
		{
			deathSpeechCareer = 0;
			expectCommander = false;
		}
		if (variant == 1)
		{
			engine.pfnIsCareerMatch = nullptr;
			expectCommander = false;
		}
		if (variant == 2)
		{
			gEntities[1].v.flags |= FL_FAKECLIENT;
			expectCommander = false;
		}
		if (variant == 3)
		{
			gEntities[2].v.flags |= FL_FAKECLIENT;
			expectRebuke = false;
		}
		if (variant == 4)
		{
			killer = 1;
			expectRebuke = false;
		}
		if (variant == 5)
		{
			killer = 0;
			expectRebuke = false;
		}
		if (variant == 6)
		{
			team(3, "CT");
			expectRebuke = false;
		}
		if (variant == 7)
		{
			team(2, "CT");
			expectCommander = false;
			expectRebuke = false;
		}
		death(killer);
		step(.12f);
		step(.8f);
		std::vector<std::string> expected;
		if (expectCommander)
			expected.push_back("radio/test/down.wav");
		if (expectRebuke)
			expected.push_back("radio/test/killed.wav");
		if (!check(deathSpeechSamples == expected, "career/human/friendly/self/world death speech branch matrix"))
			return false;
	}
	for (const char *mode : {"off", "minimal", "radio"})
	{
		begin();
		deathSpeechMode = mode;
		death();
		if (!check(deathSpeechDraws.size() == 1 && deathSpeechDraws[0] == "RNG-CSBOT-KILLED-FRIEND-DELAY",
				"KilledFriend draws delay before verbosity or missing mapping admission"))
			return false;
		step(.12f);
		step(.8f);
		const std::vector<std::string> expected = std::strcmp(mode, "minimal") == 0
			? std::vector<std::string>{"radio/test/down.wav"}
			: std::vector<std::string>{};
		if (!check(deathSpeechSamples == expected,
				"OFF/MINIMAL/radio use default Important flags and actual availability"))
			return false;
	}
	for (bool rejected : {false, true})
	{
		begin();
		team(3, "CT");
		deathVictimVisible = true;
		deathSpeechZombie = rejected ? 1.0f : 0.0f;
		death();
		if (!check(deathSpeechDraws.size() == 1 && deathSpeechDraws[0] == "FriendDeath.Hurry",
				"retaliation early return precedes speech even if Attack is rejected"))
			return false;
		step(1);
		if (!check(deathSpeechSamples.empty(), "retaliation early return suppresses actual CommanderDown"))
			return false;
	}
	begin();
	team(3, "CT");
	deathVictimVisible = true;
	self.v.weapons |= 1 << 18;
	death();
	step(.12f);
	if (!check(deathSpeechSamples == std::vector<std::string>{"radio/test/down.wav"},
			"inventory sniper skips retaliation and still reports human commander death"))
		return false;
	begin();
	team(3, "CT");
	deathVictimVisible = true;
	weapon(29);
	selectState(compat::CompatibilityStateId::Hide);
	death();
	step(.12f);
	if (!check(deathSpeechSamples == std::vector<std::string>{"radio/test/down.wav"},
			"declined knife ambush falls through to CommanderDown"))
		return false;
	begin();
	deathSpeechCareer = 0;
	death();
	death();
	if (!check(deathSpeechDraws.size() == 2, "duplicate KilledFriend still consumes each native delay draw"))
		return false;
	step(.8f);
	if (!check(deathSpeechSamples == std::vector<std::string>{"radio/test/killed.wav"},
			"pending duplicate rebuke is not repeated"))
		return false;
	begin();
	deathSpeechCareer = 0;
	death();
	step(.8f);
	death();
	step(.4f);
	step(.5f);
	if (!check(deathSpeechSamples == std::vector<std::string>{"radio/test/killed.wav"},
			"own currently spoken KilledFriend rejects new duplicate before next update"))
		return false;
	begin();
	death(0);
	step(.12f);
	deathSpeechCareer = 0;
	enemyDeathVisible = true;
	death(3, 4);
	gEntities[3].v.health = 0;
	gEntities[3].v.deadflag = DEAD_DEAD;
	step(2.1f);
	if (!check(deathSpeechSamples == std::vector<std::string>{"radio/test/down.wav"},
			"own currently spoken CommanderDown rejects new NiceShot intention"))
		return false;
	begin();
	deathSpeechCareer = 0;
	enemyDeathVisible = true;
	death(3, 4);
	gEntities[3].v.health = 0;
	gEntities[3].v.deadflag = DEAD_DEAD;
	deathSpeechCareer = 1;
	death(0);
	step(2.1f);
	if (!check(deathSpeechSamples == std::vector<std::string>{"radio/test/sir.wav"},
			"existing NiceShot intention rejects later CommanderDown"))
		return false;
	death(0, 5);
	gEntities[4].v.health = 0;
	gEntities[4].v.deadflag = DEAD_DEAD;
	step(.4f);
	if (!check(deathSpeechSamples == std::vector<std::string>{"radio/test/sir.wav"},
			"own currently spoken NiceShot rejects a new CommanderDown intention"))
		return false;
	begin();
	death(0);
	deathSpeechCareer = 0;
	enemyDeathVisible = true;
	death(3, 4);
	step(.12f);
	step(2.1f);
	if (!check(deathSpeechSamples == std::vector<std::string>{"radio/test/down.wav"},
			"CommanderDown intention rejects later NiceShot while preserving its event RNG"))
		return false;
	for (int cancellation = 0; cancellation < 5; ++cancellation)
	{
		begin();
		deathSpeechCareer = 0;
		death();
		if (cancellation == 0)
			runtime.beginRoundGeneration();
		if (cancellation == 1)
			static_cast<metamod::BotObservationServices &>(runtime).behavior_observePublicDeath(0);
		if (cancellation == 2)
			globals.time -= 1;
		if (cancellation == 3)
			++self.serialnumber;
		if (cancellation == 4)
		{
			gEntities[2].v.health = 0;
			gEntities[4].v.health = 0;
		}
		step(.8f);
		if (!check(deathSpeechSamples.empty(), "round/death/rollback/observer serial/no-friends suppress stale rebuke"))
			return false;
	}
	begin();
	death(0);
	step(20.01f);
	if (!check(deathSpeechSamples.empty(), "unpublished CommanderDown expires after20seconds"))
		return false;
	begin();
	deathSpeechCareer = 0;
	death();
	step(2.01f);
	if (!check(deathSpeechSamples.empty(), "unpublished KilledFriend expires from creation after2seconds"))
		return false;
	mutatingSpeechRuntime = &runtime;
	for (const auto mutation : {SpeechMutation::Career, SpeechMutation::Delay})
	{
		for (int slot : {2, 3})
		{
			begin();
			deathSpeechCareer = 0;
			runtime.onClientPutInServer(&gEntities[1]);
			runtime.onClientPutInServer(&gEntities[2]);
			team(2, "TERRORIST");
			team(3, "TERRORIST");
			speechMutationSlot = slot;
			speechMutation = mutation;
			death();
			step(1);
			if (!check(deathSpeechSamples.empty(),
					"same-serial source reconnect during career/delay callback cannot transfer speech"))
				return false;
		}
	}
	mutatingSpeechRuntime = nullptr;
	begin();
	deathSpeechMode = "radio";
	deathSpeechCareer = 0;
	deathRadioRequests = 0;
	const auto originalClientCommand = gHookedGameDllTable.pfnClientCommand;
	gHookedGameDllTable.pfnClientCommand = &deathRadioClientCommand;
	death();
	step(.8f);
	step(.4f);
	gHookedGameDllTable.pfnClientCommand = originalClientCommand;
	if (!check(deathRadioRequests == 1 && deathSpeechSamples.empty(),
			"mapped KilledFriend uses actual GameDLL radio and public SendAudio confirms it without repeating"))
		return false;
	bool reviewRegressions = true;
	begin();
	deathSpeechMode = "radio";
	deathRadioRequests = 0;
	gHookedGameDllTable.pfnClientCommand = &deathRadioClientCommand;
	death();
	step(.8f);
	step(.4f);
	gHookedGameDllTable.pfnClientCommand = originalClientCommand;
	reviewRegressions = check(deathRadioRequests == 1,
							"review: unavailable CommanderDown must release mapped KilledFriend before its expiry") &&
		reviewRegressions;
	begin();
	const auto beforeNamed = services.service_navPublisher().snapshot();
	nav::NavDocument unnamed;
	nav::NavDocument named;
	unnamed.setSourceIdentity(beforeNamed.document()->sourceIdentity());
	named.setSourceIdentity(beforeNamed.document()->sourceIdentity());
	nav::NavPlace speakerPlace{};
	speakerPlace.id = 1;
	std::memcpy(speakerPlace.name, "Speaker", sizeof("Speaker"));
	named.addPlace(speakerPlace);
	for (auto area : beforeNamed.document()->areas())
	{
		unnamed.addArea(area);
		area.placeId = 1;
		named.addArea(area);
	}
	auto &publisher = const_cast<nav::NavSnapshotPublisher &>(services.service_navPublisher());
	if (!check(publisher.publish(&named, runtime.snapshot().mapGeneration) == nav::NavSnapshotResult::Published,
			"review fixture publishes real named speaker NAV"))
		return false;
	nav::NavAreaMatch namedArea{};
	const auto namedSnapshot = publisher.snapshot();
	if (!check(nav::NavQuery(namedSnapshot).findContaining({0, 0, 36}, 64, &namedArea) == nav::NavQueryResult::Found &&
				namedSnapshot.document()->findArea(namedArea.area)->placeId == 1,
			"speaker is actually in named NAV area"))
		return false;
	death();
	step(.12f);
	step(.8f);
	reviewRegressions =
		check(deathSpeechSamples == std::vector<std::string>{"radio/test/down.wav", "radio/test/killed.wav"},
			"review: fixed death phrases select UNDEFINED rather than speaker NAV place") &&
		reviewRegressions;
	if (!check(publisher.publish(&unnamed, runtime.snapshot().mapGeneration) == nav::NavSnapshotResult::Published,
			"review fixture restores unnamed NAV"))
		return false;
	begin();
	resetDuringVoice = true;
	deathVoiceStarts = 0;
	death(0);
	step(.12f);
	step(.12f);
	reviewRegressions =
		check(deathSpeechSamples == std::vector<std::string>{"radio/test/down.wav", "radio/test/down.wav"},
			"review: stale voice completion cannot suppress fresh same-owner statement after reset") &&
		reviewRegressions;
	reviewRegressions =
		check(deathVoiceStarts == 1, "review: cancelled old voice must not publish new feedback start") &&
		reviewRegressions;
	begin();
	deathSpeechMode = "radio";
	deathSpeechCareer = 0;
	deathRadioRequests = 0;
	resetDuringRadio = true;
	gHookedGameDllTable.pfnClientCommand = &deathRadioClientCommand;
	death();
	step(.8f);
	step(.8f);
	gHookedGameDllTable.pfnClientCommand = originalClientCommand;
	reviewRegressions = check(deathRadioRequests == 2,
							"review: stale radio completion cannot suppress fresh same-owner statement after reset") &&
		reviewRegressions;
	if (!reviewRegressions)
		return false;
	begin();
	setCvarFloat("bot_quota", 2);
	runtime.onClientDisconnect(&gEntities[4]);
	gCreateCount = 4;
	if (!check(runtime.executeCompatibilityCommand(request("bot_add_t", 1U, "Second Speaker")) ==
				metamod::CompatibilityCommandResult::Handled,
			"second managed speaker is created for real team speech arbitration"))
		return false;
	gEntities[4].v.flags = FL_CLIENT | FL_FAKECLIENT | FL_ONGROUND;
	gEntities[4].v.health = 100;
	gEntities[4].v.deadflag = DEAD_NO;
	gEntities[4].v.maxspeed = 240;
	gEntities[4].v.solid = SOLID_SLIDEBOX;
	gEntities[4].v.movetype = MOVETYPE_WALK;
	gEntities[4].v.mins = Vector(-16, -16, -36);
	gEntities[4].v.maxs = Vector(16, 16, 36);
	gEntities[4].v.view_ofs = Vector(0, 0, 17);
	runtime.setCompatibilityFloat("bot_quota", 2);
	setCvarFloat("bot_quota", 2);
	team(5, "TERRORIST");
	for (int frame = 0; frame < 12; ++frame)
		step(.12f);
	deathSpeechSamples.clear();
	death(0, 2, MSG_ONE, &gEntities[4]);
	step(.12f);
	if (!check(deathSpeechSamples == std::vector<std::string>{"radio/test/down.wav"},
			"second managed observer independently delivers targeted CommanderDown"))
		return false;
	begin();
	death(0);
	step(.12f);
	step(.4f);
	if (!check(services.service_managedBotSlots()[4] && services.service_managedBotFullUpdateSequences()[4] != 0 &&
				deathSpeechSamples == std::vector<std::string>{"radio/test/down.wav"},
			"two active managed observers produce one actual CommanderDown announcement"))
		return false;
	services.service_compatibilityRandomSource().setTraceSink(nullptr);
	return true;
}
}
