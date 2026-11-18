#include "plugin/plugin_runtime.hpp"
namespace
{
bool testChatterVoice(astrabot::metamod::PluginRuntime &, enginefuncs_t &, globalvars_t &);
void setupChatterVoice(enginefuncs_t &, globalvars_t &);
}
#define ASTRABOT_EXTRA_ACTOR_TEST testChatterVoice
#define ASTRABOT_ACTOR_ENGINE_SETUP setupChatterVoice
#define ASTRABOT_ACTOR_PROFILE_TEXT                                                                                    \
	"Any { Name = \"Voice Bot\" Team = any Difficulty = 1 Skill = 70 VoicePitch = 95 VoiceBank = alternate.db }\n"
#include "compat_actor_command_tests.cpp"
namespace
{
struct VoiceMessage
{
	int destination = 0, type = 0;
	edict_t *recipient = nullptr;
	std::vector<int> numbers;
	std::string sample;
};
std::vector<VoiceMessage> voiceMessages;
VoiceMessage voiceMessage;
const char *voiceMode = "normal";
float voiceStop = 0.0f;
bool voiceFeedbackRegistered = true;
float voiceFloat(const char *name)
{
	return std::strcmp(name, "bot_stop") == 0 ? voiceStop : getCvarFloat(name);
}
unsigned voiceDbLoads = 0, voiceWaveReads = 0, voiceRadioRequests = 0;
const char *voiceCvar(const char *name)
{
	if (std::strcmp(name, "bot_chatter") == 0)
		return voiceMode;
	const auto *value = getCvar(name);
	return value && value->string ? value->string : "";
}
int voiceMessageId(plid_t id, const char *name, int *size)
{
	if (std::strcmp(name, "SendAudio") == 0)
		return 101;
	if (std::strcmp(name, "BotVoice") == 0)
		return voiceFeedbackRegistered ? 102 : 0;
	return siteFixtureUserMessageId(id, name, size);
}
unsigned char *voiceLoad(char *filename, int *length)
{
	const char *text = nullptr;
	if (std::strcmp(filename, "BotChatter.db") == 0)
		text = "Chatter Help Radio EVENT_RADIO_NEED_BACKUP End";
	if (std::strcmp(filename, "alternate.db") == 0)
		text = "BaseDir radio/test/ Chatter Help first.wav second.wav End";
	if (!text)
	{
		*length = 0;
		return nullptr;
	}
	++voiceDbLoads;
	*length = static_cast<int>(std::strlen(text));
	auto *bytes = new unsigned char[static_cast<std::size_t>(*length) + 1U];
	std::memcpy(bytes, text, static_cast<std::size_t>(*length) + 1U);
	return bytes;
}
void voiceFree(void *data)
{
	delete[] static_cast<unsigned char *>(data);
}
int voiceFileSize(char *path)
{
	return std::strstr(path, "first.wav") ? 0 : 100;
}
unsigned int voiceWaveLength(const char *path)
{
	++voiceWaveReads;
	return std::strstr(path, "radio/test/") ? 3200U : 0U;
}
void voiceBegin(int destination, int type, const float *, edict_t *recipient)
{
	voiceMessage = {};
	voiceMessage.destination = destination;
	voiceMessage.type = type;
	voiceMessage.recipient = recipient;
}
void voiceByte(int value)
{
	voiceMessage.numbers.push_back(value);
}
void voiceShort(int value)
{
	voiceMessage.numbers.push_back(value);
}
void voiceString(const char *value)
{
	voiceMessage.sample = value;
}
void voiceEnd()
{
	voiceMessages.push_back(voiceMessage);
}
void voiceClientCommand(edict_t *self)
{
	const char *command = astrabot::metamod::HookCommandArgv(0);
	if (std::strcmp(command, "radio3") == 0)
	{
		++voiceRadioRequests;
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
		}
		return;
	}
	hookedGameClientCommand(self);
}
void setupChatterVoice(enginefuncs_t &engine, globalvars_t &globals)
{
	(void)globals;
	engine.pfnLoadFileForMe = &voiceLoad;
	engine.pfnFreeFile = &voiceFree;
	engine.pfnGetApproxWavePlayLen = &voiceWaveLength;
	engine.pfnGetFileSize = &voiceFileSize;
	engine.pfnMessageBegin = &voiceBegin;
	engine.pfnWriteByte = &voiceByte;
	engine.pfnWriteShort = &voiceShort;
	engine.pfnWriteString = &voiceString;
	engine.pfnMessageEnd = &voiceEnd;
	engine.pfnCVarGetString = &voiceCvar;
	engine.pfnCVarGetFloat = &voiceFloat;
	engine.pfnTraceLine = &traceObjectiveVisibility;
	gpMetaUtilFuncs->pfnGetUserMsgID = &voiceMessageId;
	gHookedGameDllTable.pfnClientCommand = &voiceClientCommand;
}

bool testChatterVoice(astrabot::metamod::PluginRuntime &runtime, enginefuncs_t &engine, globalvars_t &globals)
{
	setupChatterVoice(engine, globals);
	runtime.giveEnginePointers(&engine, &globals);
	setCvarFloat("bot_auto_vacate", 0);
	gObjectiveEntitiesAvailable = false;
	gObjectiveTraceVisible = true;
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
	for (unsigned i = 1; i < 5; ++i)
	{
		auto &peer = gEntities[i];
		peer.v.flags = FL_CLIENT | FL_ONGROUND;
		peer.v.health = 100;
		peer.v.deadflag = DEAD_NO;
		peer.serialnumber = static_cast<int>(i + 10);
		peer.v.team = i == 4 ? 1 : 2;
		peer.v.origin = Vector(i == 4 ? -120.0f : 120.0f, static_cast<float>(i * 4), 36);
	}
	globals.maxClients = 8;
	gBombSite.v.flags = FL_CLIENT;
	gBombSite.serialnumber = 40;
	gBombSite.v.team = 1;
	gBombSite.v.deadflag = DEAD_DEAD;
	gPlantedBomb.v.flags = FL_CLIENT | FL_SPECTATOR;
	gPlantedBomb.serialnumber = 41;
	gPlantedBomb.v.team = 3;
	gPlantedBomb.v.iuser1 = 4;
	gPlantedBomb.v.iuser2 = 5;
	gSecondBombSite.v.flags = FL_CLIENT | FL_SPECTATOR;
	gSecondBombSite.serialnumber = 42;
	gSecondBombSite.v.team = 3;
	gSecondBombSite.v.iuser1 = 4;
	gSecondBombSite.v.iuser2 = 2;
	const auto step = [&](float dt)
	{
		globals.time += dt;
		runtime.onStartFrame();
		runtime.onStartFramePost();
	};
	// Start where the native NeedBackup interval from -1 has elapsed.
	globals.time = (std::max)(globals.time, 9.0f);
	const auto round = [&]()
	{
		runtime.beginRoundGeneration();
		runtime.onMessageBegin(MSG_ONE, 94, nullptr, &self);
		runtime.onWriteByte(1);
		runtime.onWriteByte(22);
		runtime.onWriteByte(30);
		runtime.onMessageEnd();
	};
	const auto audioCount = [&]()
	{
		unsigned count = 0;
		for (const auto &m : voiceMessages)
			if (m.type == 101)
				++count;
		return count;
	};
	const auto stops = [&]()
	{
		unsigned count = 0;
		for (const auto &m : voiceMessages)
			if (m.type == 102 && !m.numbers.empty() && m.numbers[0] == 0)
				++count;
		return count;
	};
	round();
	for (unsigned i = 0; i < 8; ++i)
		step(.125f);
	if (audioCount() != 4 || voiceDbLoads != 2 || voiceWaveReads != 1)
		std::fprintf(stderr, "voice counts audio=%u db=%u wave=%u radio=%u messages=%u\n", audioCount(), voiceDbLoads,
			voiceWaveReads, voiceRadioRequests, static_cast<unsigned>(voiceMessages.size()));
	if (!check(audioCount() == 4 && voiceDbLoads == 2 && voiceWaveReads == 1,
			"normal chatter loads the profile bank once and sends one clip to self, teammate, dead teammate and matching spectator"))
		return false;
	for (const auto &m : voiceMessages)
		if (m.type == 101)
			if (!check(m.destination == MSG_ONE && m.numbers.size() == 2 && m.numbers[0] == 1 && m.numbers[1] == 95 &&
						m.sample == "radio/test/second.wav" && m.recipient != &gEntities[1] &&
						m.recipient != &gSecondBombSite,
					"SendAudio carries speaker/filename/profile pitch and never leaks to enemies"))
				return false;
	const unsigned initialStops = stops();
	voiceStop = 1.0f;
	runtime.setCompatibilityFloat("bot_stop", 1);
	step(2.0f);
	if (!check(stops() == initialStops, "voice feedback lasts for actual clip duration plus one second"))
		return false;
	step(3.0f);
	if (!check(stops() == initialStops + 1 && voiceDbLoads == 2 && voiceWaveReads == 1,
			"feedback expires while AI is stopped without reloading assets"))
		return false;
	voiceStop = 0.0f;
	runtime.setCompatibilityFloat("bot_stop", 0);
	// Public radio preference belongs to the recipient's entity generation.
	runtime.onMessageBegin(MSG_ONE, 98, nullptr, &gEntities[4]);
	runtime.onWriteByte(4);
	runtime.onWriteString("#Ignore_Radio");
	runtime.onMessageEnd();
	voiceMessages.clear();
	round();
	for (unsigned i = 0; i < 8; ++i)
		step(.125f);
	if (!check(audioCount() == 3, "observed Ignore_Radio excludes that teammate from custom audio"))
		return false;
	runtime.onMessageBegin(MSG_ONE, 98, nullptr, &gEntities[4]);
	runtime.onWriteByte(4);
	runtime.onWriteString("#Accept_Radio");
	runtime.onMessageEnd();
	voiceMessages.clear();
	voiceMode = "minimal";
	round();
	for (unsigned i = 0; i < 8; ++i)
		step(.125f);
	if (!check(audioCount() == 0, "minimal verbosity suppresses a non-important Help phrase"))
		return false;
	voiceMessages.clear();
	voiceMode = "off";
	round();
	for (unsigned i = 0; i < 8; ++i)
		step(.125f);
	if (!check(audioCount() == 0 && voiceRadioRequests == 0, "off mode suppresses voice and standard radio"))
		return false;
	voiceMessages.clear();
	voiceMode = "radio";
	round();
	for (unsigned i = 0; i < 8; ++i)
		step(.125f);
	if (!check(audioCount() == 0 && voiceRadioRequests > 0, "radio verbosity keeps the public standard-radio path"))
		return false;
	voiceMessages.clear();
	voiceMode = "normal";
	round();
	for (unsigned i = 0; i < 8; ++i)
		step(.125f);
	if (!check(audioCount() == 4, "normal voice resumes on a new round"))
		return false;
	const unsigned beforeStop = stops();
	runtime.beginRoundGeneration();
	if (!check(stops() == beforeStop + 1, "round reset clears active voice feedback"))
		return false;
	for (auto *recipient : {&self, &gEntities[4], &gBombSite, &gPlantedBomb})
	{
		runtime.onMessageBegin(MSG_ONE, 98, nullptr, recipient);
		runtime.onWriteByte(4);
		runtime.onWriteString("#Ignore_Radio");
		runtime.onMessageEnd();
	}
	voiceMessages.clear();
	round();
	for (unsigned i = 0; i < 8; ++i)
		step(.125f);
	if (!check(audioCount() == 0 && !voiceMessages.empty(),
			"all ignored recipients can still see reference voice feedback"))
		return false;
	step(5.0f);
	if (!check(stops() == 1, "feedback ends even when no recipient accepted SendAudio"))
		return false;
	for (auto *recipient : {&self, &gEntities[4], &gBombSite, &gPlantedBomb})
	{
		runtime.onMessageBegin(MSG_ONE, 98, nullptr, recipient);
		runtime.onWriteByte(4);
		runtime.onWriteString("#Accept_Radio");
		runtime.onMessageEnd();
	}
	voiceMessages.clear();
	const unsigned radioBefore = voiceRadioRequests;
	engine.pfnWriteString = nullptr;
	round();
	for (unsigned i = 0; i < 8; ++i)
		step(.125f);
	if (!check(audioCount() == 0 && voiceRadioRequests > radioBefore,
			"unavailable custom-audio transport falls back to the mapped GameDLL radio"))
		return false;
	engine.pfnWriteString = &voiceString;
	voiceMessages.clear();
	voiceFeedbackRegistered = false;
	round();
	for (unsigned i = 0; i < 8; ++i)
		step(.125f);
	if (!check(audioCount() == 4 && stops() == 0 && voiceMessages.size() == 4,
			"absent BotVoice registration leaves audio usable without invented UI messages"))
		return false;
	voiceFeedbackRegistered = true;
	voiceMessages.clear();
	round();
	for (unsigned i = 0; i < 8; ++i)
		step(.125f);
	self.v.deadflag = DEAD_DEAD;
	self.v.health = 0;
	step(.125f);
	if (!check(stops() == 1, "death ends active voice feedback"))
		return false;
	self.v.deadflag = DEAD_NO;
	self.v.health = 100;
	voiceMessages.clear();
	round();
	for (unsigned i = 0; i < 8; ++i)
		step(.125f);
	const auto beforeMapEnd = stops();
	runtime.onServerDeactivate();
	if (!check(stops() == beforeMapEnd + 1, "map teardown ends active voice feedback"))
		return false;
	globals.maxClients = 5;
	gBombSite = {};
	gPlantedBomb = {};
	gSecondBombSite = {};
	return true;
}
}
