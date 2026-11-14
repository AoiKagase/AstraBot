#include "plugin/plugin_runtime.hpp"
namespace
{
bool testEnemyCount(astrabot::metamod::PluginRuntime &, enginefuncs_t &, globalvars_t &);
void setupEnemyCount(enginefuncs_t &, globalvars_t &);
}
#define ASTRABOT_EXTRA_ACTOR_TEST testEnemyCount
#define ASTRABOT_ACTOR_ENGINE_SETUP setupEnemyCount
#include "compat_actor_command_tests.cpp"
namespace
{
const char *countMode = "normal";
int countMessage = 0;
edict_t *countRecipient = nullptr;
std::vector<std::string> countSamples;
unsigned countDraws = 0;
struct CountTrace : astrabot::compat::IRandomTraceSink
{
	void record(const astrabot::compat::RandomTraceRecord &r) override
	{
		if (std::strcmp(r.request.semanticId, "RNG-CSBOT-ENEMIES-REMAINING-DELAY") == 0)
			++countDraws;
	}
};
CountTrace countTrace;
const char *countCvar(const char *name)
{
	if (std::strcmp(name, "bot_chatter") == 0)
		return countMode;
	const auto *value = getCvar(name);
	return value && value->string ? value->string : "";
}
float countRandom(float low, float)
{
	return low;
}
unsigned char *countLoad(char *name, int *length)
{
	if (std::strcmp(name, "BotChatter.db") != 0)
	{
		*length = 0;
		return nullptr;
	}
	const char text[] =
		"BaseDir radio/test/ Chatter NoEnemiesLeft zero.wav End Chatter OneEnemyLeft Important one.wav End Chatter TwoEnemiesLeft two.wav End Chatter ThreeEnemiesLeft three.wav End";
	*length = sizeof(text) - 1;
	auto *bytes = new unsigned char[sizeof(text)];
	std::memcpy(bytes, text, sizeof(text));
	return bytes;
}
void countFree(void *bytes)
{
	delete[] static_cast<unsigned char *>(bytes);
}
int countFileSize(char *)
{
	return 100;
}
unsigned countWaveLength(const char *)
{
	return 200;
}
int countMessageId(plid_t plugin, const char *name, int *size)
{
	if (std::strcmp(name, "DeathMsg") == 0)
		return 103;
	if (std::strcmp(name, "SendAudio") == 0)
		return 101;
	if (std::strcmp(name, "BotVoice") == 0)
		return 102;
	return siteFixtureUserMessageId(plugin, name, size);
}
void countBegin(int, int type, const float *, edict_t *recipient)
{
	countMessage = type;
	countRecipient = recipient;
}
void countNumber(int)
{
}
void countString(const char *sample)
{
	if (countMessage == 101 && countRecipient == &gEntities[0])
		countSamples.push_back(sample);
}
void countEnd()
{
}
void setupEnemyCount(enginefuncs_t &engine, globalvars_t &)
{
	engine.pfnCVarGetString = &countCvar;
	engine.pfnLoadFileForMe = &countLoad;
	engine.pfnFreeFile = &countFree;
	engine.pfnGetFileSize = &countFileSize;
	engine.pfnGetApproxWavePlayLen = &countWaveLength;
	engine.pfnMessageBegin = &countBegin;
	engine.pfnWriteByte = &countNumber;
	engine.pfnWriteShort = &countNumber;
	engine.pfnWriteString = &countString;
	engine.pfnMessageEnd = &countEnd;
	gpMetaUtilFuncs->pfnGetUserMsgID = &countMessageId;
}
bool testEnemyCount(astrabot::metamod::PluginRuntime &runtime, enginefuncs_t &engine, globalvars_t &globals)
{
	using namespace astrabot;
	setupEnemyCount(engine, globals);
	engine.pfnTraceLine = &traceObjectiveVisibility;
	engine.pfnRandomFloat = &countRandom;
	runtime.giveEnginePointers(&engine, &globals);
	setCvarFloat("bot_auto_vacate", 0);
	gObjectiveEntitiesAvailable = false;
	gObjectiveTraceVisible = false;
	globals.maxClients = 5;
	auto &self = gEntities[0];
	self.v.flags = FL_CLIENT | FL_FAKECLIENT | FL_ONGROUND;
	self.v.health = 100;
	self.v.deadflag = DEAD_NO;
	self.v.maxspeed = 240;
	self.v.origin = Vector(0, 0, 36);
	self.v.view_ofs = Vector(0, 0, 17);
	for (unsigned i = 1; i < 5; ++i)
	{
		auto &p = gEntities[i];
		p.serialnumber = static_cast<int>(i);
		p.free = false;
		p.v.flags = FL_CLIENT | FL_ONGROUND;
		p.v.health = 100;
		p.v.deadflag = DEAD_NO;
		p.v.team = i == 1 ? 1 : 2;
		p.v.origin = Vector(500, 0, 36);
	}
	gEntities[4].v.health = 0;
	gEntities[4].v.deadflag = DEAD_DEAD;
	enginefuncs_t hooks{};
	int version = ENGINE_INTERFACE_VERSION;
	if (!check(runtime.provideEngineFunctions(&hooks, &version) && hooks.pfnMessageBegin && hooks.pfnMessageEnd,
			"actual public engine message hooks are registered"))
		return false;
	const auto team = [&](int slot, const char *name)
	{
		hooks.pfnMessageBegin(MSG_ALL, 86, nullptr, nullptr);
		hooks.pfnWriteByte(slot);
		hooks.pfnWriteString(name);
		hooks.pfnMessageEnd();
	};
	team(2, "TERRORIST");
	team(3, "CT");
	team(4, "CT");
	team(5, "CT");
	auto &services = static_cast<metamod::BotBehaviorServices &>(runtime);
	services.service_compatibilityRandomSource().setTraceSink(&countTrace);
	const auto step = [&](float dt)
	{
		globals.time += dt;
		runtime.onStartFrame();
		runtime.onStartFramePost();
	};
	const auto death = [&](int killer, int victim, int destination = MSG_ALL, edict_t *recipient = nullptr)
	{
		hooks.pfnMessageBegin(destination, 103, nullptr, recipient);
		hooks.pfnWriteByte(killer);
		hooks.pfnWriteByte(victim);
		hooks.pfnWriteByte(0);
		hooks.pfnWriteString("ak47");
		hooks.pfnMessageEnd();
	};
	const auto begin = [&]()
	{
		runtime.beginRoundGeneration();
		countSamples.clear();
		self.v.health = 100;
		self.v.deadflag = DEAD_NO;
		gEntities[2].v.health = 0;
		gEntities[2].v.deadflag = DEAD_DEAD;
		gEntities[3].v.health = 100;
		gEntities[3].v.deadflag = DEAD_NO;
		step(.2f);
	};
	begin();
	const auto initialDraws = countDraws;
	death(2, 3);
	step(1.5f);
	if (!check(countSamples.empty(), "death count report waits two seconds"))
		return false;
	gEntities[3].v.health = 0;
	gEntities[3].v.deadflag = DEAD_DEAD;
	step(.6f);
	if (!check(countDraws == initialDraws + 1 && countSamples.size() == 1 && countSamples[0] == "radio/test/zero.wav",
			"registered DeathMsg schedules dynamic remaining count using actual host audio"))
		return false;
	if (!check((gLastRunPlayerMoveButtons & IN_ATTACK) == 0, "hidden death report creates no enemy aim or firing"))
		return false;
	step(.5f);
	if (!check(countSamples.size() == 1, "sent count does not replay"))
		return false;
	bool edgePassed = true;
	begin();
	hooks.pfnMessageBegin(MSG_ONE, 103, nullptr, &self);
	hooks.pfnWriteByte(2);
	hooks.pfnWriteByte(3);
	hooks.pfnWriteByte(0);
	hooks.pfnWriteString("ak47");
	const unsigned recipientDraws = countDraws;
	const int originalSerial = self.serialnumber;
	++self.serialnumber;
	hooks.pfnMessageEnd();
	self.serialnumber = originalSerial;
	step(2.1f);
	edgePassed = check(countSamples.empty() && countDraws == recipientDraws,
					 "reused recipient during message cannot receive an old death report or consume its RNG") &&
		edgePassed;
	begin();
	gEntities[4].v.health = 100;
	gEntities[4].v.deadflag = DEAD_NO;
	gEntities[4].v.flags |= FL_SPECTATOR;
	death(2, 3);
	step(2.1f);
	edgePassed = check(countSamples.size() == 1 && countSamples[0] == "radio/test/one.wav",
					 "spectator does not inflate remaining enemy count") &&
		edgePassed;
	gEntities[4].v.health = 0;
	gEntities[4].v.deadflag = DEAD_DEAD;
	gEntities[4].v.flags &= ~FL_SPECTATOR;
	if (!edgePassed)
		return false;
	for (unsigned variant = 0; variant < 7; ++variant)
	{
		begin();
		auto before = countDraws;
		if (variant == 0)
			death(0, 3);
		if (variant == 1)
			death(4, 3);
		if (variant == 2)
			death(2, 2);
		if (variant == 3)
			death(2, 3, MSG_ONE, &gEntities[1]);
		if (variant == 4)
		{
			countMode = "off";
			death(2, 3);
			countMode = "normal";
		}
		if (variant == 5)
		{
			countMode = "minimal";
			death(2, 3);
			countMode = "normal";
		}
		if (variant == 6)
		{
			death(2, 3);
			runtime.beginRoundGeneration();
		}
		step(2.1f);
		if (!check(countSamples.empty(), "invalid,unreceived,muted or obsolete death cannot produce audio"))
			return false;
		if (variant < 4 && !check(countDraws == before, "invalid death consumes no report RNG"))
			return false;
		if (variant >= 4 &&
			!check(countDraws == before + 1, "mute and later cancellation preserve native creation draw"))
			return false;
	}
	begin();
	death(2, 3, MSG_ONE, &self);
	step(2.1f);
	if (!check(countSamples.size() == 1 && countSamples[0] == "radio/test/one.wav",
			"targeted receipt reaches only its observer"))
		return false;
	begin();
	death(2, 3);
	self.v.health = 0;
	self.v.deadflag = DEAD_DEAD;
	death(4, 1);
	self.v.health = 100;
	self.v.deadflag = DEAD_NO;
	step(2.1f);
	if (!check(countSamples.empty(), "public observer death cancels queued report even if revived before full update"))
		return false;
	begin();
	gEntities[4].v.health = 100;
	gEntities[4].v.deadflag = DEAD_NO;
	death(2, 3);
	step(2.1f);
	if (!check(countSamples.size() == 1 && countSamples[0] == "radio/test/two.wav",
			"two remaining enemies select their own host phrase"))
		return false;
	gEntities[4].v.health = 0;
	gEntities[4].v.deadflag = DEAD_DEAD;
	begin();
	const int killerSerial = gEntities[1].serialnumber;
	const int victimSerial = gEntities[2].serialnumber;
	gEntities[1].serialnumber = 0;
	gEntities[2].serialnumber = 0;
	death(2, 3);
	step(2.1f);
	if (!check(countSamples.size() == 1, "zero source serials remain valid public generations"))
		return false;
	gEntities[1].serialnumber = killerSerial;
	gEntities[2].serialnumber = victimSerial;
	begin();
	gEntities[2].v.health = 100;
	gEntities[2].v.deadflag = DEAD_NO;
	gEntities[4].v.health = 100;
	gEntities[4].v.deadflag = DEAD_NO;
	gObjectiveTraceVisible = true;
	step(.2f);
	gEntities[2].v.health = 0;
	gEntities[2].v.deadflag = DEAD_DEAD;
	const auto crowdedDraws = countDraws;
	death(2, 3);
	gObjectiveTraceVisible = false;
	step(2.1f);
	if (!check(countDraws == crowdedDraws && countSamples.empty(),
			"more than one watched living enemy suppresses the report before RNG"))
		return false;
	gEntities[4].v.health = 0;
	gEntities[4].v.deadflag = DEAD_DEAD;
	begin();
	death(2, 3);
	engine.pfnWriteString = nullptr;
	step(2.1f);
	engine.pfnWriteString = &countString;
	step(.2f);
	if (!check(
			countSamples.empty(), "unavailable voice transport discards count without inventing a fallback or replay"))
		return false;
	services.service_compatibilityRandomSource().setTraceSink(nullptr);
	return true;
}
}
