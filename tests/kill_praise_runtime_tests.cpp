#include "plugin/plugin_runtime.hpp"
namespace
{
bool testKillPraise(astrabot::metamod::PluginRuntime &, enginefuncs_t &, globalvars_t &);
void setupKillPraise(enginefuncs_t &, globalvars_t &);
}
#define ASTRABOT_EXTRA_ACTOR_TEST testKillPraise
#define ASTRABOT_ACTOR_ENGINE_SETUP setupKillPraise
#define ASTRABOT_ACTOR_PROFILE_TEXT                                                                                    \
	"Any { Name = \"Praise Bot\" Team = any Difficulty = 1 Skill = 70 VoiceBank = praise.db }\n"
#include "compat_actor_command_tests.cpp"
#include "fixtures/noise_runtime_nav.hpp"
#include <cstdlib>
namespace
{
const char *praiseMode = "off";
bool praiseDefaultMissing = false;
unsigned praiseWaveProbes = 0;
int praiseCareer = 0;
float praiseChance = 0;
bool praiseLos = true;
bool reuseVictimOnCount = false;
int praiseMessage = 0;
edict_t *praiseRecipient = nullptr;
std::vector<std::string> praiseSamples;
std::vector<std::string> praiseIds;
struct PraiseTrace : astrabot::compat::IRandomTraceSink
{
	void record(const astrabot::compat::RandomTraceRecord &r) override
	{
		praiseIds.push_back(r.request.semanticId);
	}
};
PraiseTrace praiseTrace;
float praiseRandom(float low, float high)
{
	if (low == 2 && high == 4)
	{
		if (reuseVictimOnCount)
			++gEntities[2].serialnumber;
		return 4;
	}
	if (low == 0 && high == 100)
		return praiseChance;
	if (low == 2 && high == 3)
		return 2;
	return hearingFixtureRandomFloat(low, high);
}
int careerMatch()
{
	return praiseCareer;
}
const char *praiseCvar(const char *name)
{
	if (std::strcmp(name, "bot_chatter") == 0)
		return praiseMode;
	const auto *v = getCvar(name);
	return v && v->string ? v->string : "";
}
unsigned char *praiseLoad(char *name, int *length)
{
	const char *text = nullptr;
	if (std::strcmp(name, "BotChatter.db") == 0)
		text = praiseDefaultMissing
			? "BaseDir radio/test/ Chatter OneEnemyLeft Count Many Place UNDEFINED one.wav Count 2 Place ANY wrong_counter.wav End Chatter NiceShotSir sir.wav End Chatter NiceShotCommander Important commander.wav End"
			: "BaseDir radio/test/ Chatter OneEnemyLeft Count Many Place UNDEFINED one.wav Count 2 Place ANY wrong_counter.wav End Chatter NiceShot bot.wav End Chatter NiceShotSir sir.wav End Chatter NiceShotCommander Important commander.wav End";
	if (std::strcmp(name, "praise.db") == 0)
		text =
			"BaseDir radio/test/ Chatter OneEnemyLeft Count Many Place UNDEFINED one.wav Count 2 Place ANY wrong_counter.wav End Chatter NiceShot bot.wav End Chatter NiceShotSir Important Place UNDEFINED sir.wav sir2.wav Count 2 Place ANY wrong_count.wav Count Many Place Speaker wrong_place.wav End Chatter NiceShotCommander commander.wav End";
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
void praiseFree(void *data)
{
	delete[] static_cast<unsigned char *>(data);
}
int praiseSize(char *)
{
	++praiseWaveProbes;
	return 100;
}
unsigned praiseLength(const char *)
{
	return 200;
}
int praiseMessageId(plid_t p, const char *name, int *size)
{
	if (std::strcmp(name, "DeathMsg") == 0)
		return 103;
	if (std::strcmp(name, "ScreenFade") == 0)
		return 104;
	if (std::strcmp(name, "SendAudio") == 0)
		return 101;
	if (std::strcmp(name, "BotVoice") == 0)
		return 102;
	return siteFixtureUserMessageId(p, name, size);
}
void praiseBegin(int, int type, const float *, edict_t *recipient)
{
	praiseMessage = type;
	praiseRecipient = recipient;
}
void praiseNumber(int)
{
}
void praiseString(const char *s)
{
	if (praiseMessage == 101 && praiseRecipient == &gEntities[0])
		praiseSamples.push_back(s);
}
void praiseEnd()
{
}
void praiseTraceLine(const float *, const float *end, int flags, edict_t *, TraceResult *trace)
{
	*trace = {};
	trace->vecEndPos = Vector(end[0], end[1], end[2]);
	trace->flFraction = (praiseLos && flags == 0x101 && end[0] == 500 && end[2] == 36) ? 1.0f : 0.0f;
}
bool missingDefaultPraiseFixture()
{
#ifdef _MSC_VER
	char *value = nullptr;
	std::size_t size = 0;
	if (_dupenv_s(&value, &size, "ASTRABOT_TEST_PRAISE_DEFAULT_MISSING") != 0)
		return false;
	const bool missing = value && std::strcmp(value, "1") == 0;
	std::free(value);
	return missing;
#else
	const auto *value = std::getenv("ASTRABOT_TEST_PRAISE_DEFAULT_MISSING");
	return value && std::strcmp(value, "1") == 0;
#endif
}

void setupKillPraise(enginefuncs_t &engine, globalvars_t &)
{
	praiseDefaultMissing = missingDefaultPraiseFixture();
	engine.pfnCVarGetString = &praiseCvar;
	engine.pfnLoadFileForMe = &praiseLoad;
	engine.pfnFreeFile = &praiseFree;
	engine.pfnGetFileSize = &praiseSize;
	engine.pfnGetApproxWavePlayLen = &praiseLength;
	engine.pfnMessageBegin = &praiseBegin;
	engine.pfnWriteByte = &praiseNumber;
	engine.pfnWriteShort = &praiseNumber;
	engine.pfnWriteString = &praiseString;
	engine.pfnMessageEnd = &praiseEnd;
	engine.pfnIsCareerMatch = &careerMatch;
	gpMetaUtilFuncs->pfnGetUserMsgID = &praiseMessageId;
}
bool testKillPraise(astrabot::metamod::PluginRuntime &runtime, enginefuncs_t &engine, globalvars_t &globals)
{
	using namespace astrabot;
	setupKillPraise(engine, globals);
	engine.pfnTraceLine = &praiseTraceLine;
	engine.pfnRandomFloat = &praiseRandom;
	runtime.giveEnginePointers(&engine, &globals);
	setCvarFloat("bot_auto_vacate", 0);
	gObjectiveEntitiesAvailable = false;
	globals.maxClients = 5;
	auto &self = gEntities[0];
	self.v.flags = FL_CLIENT | FL_FAKECLIENT | FL_ONGROUND;
	self.v.health = 100;
	self.v.deadflag = DEAD_NO;
	self.v.origin = Vector(0, 0, 36);
	self.v.view_ofs = Vector(0, 0, 17);
	self.v.maxspeed = 240;
	for (unsigned i = 1; i < 5; ++i)
	{
		auto &p = gEntities[i];
		p.serialnumber = static_cast<int>(i);
		p.free = false;
		p.v.flags = FL_CLIENT | FL_ONGROUND;
		p.v.team = i == 1 ? 1 : 2;
		p.v.origin = Vector(-500, 0, 36);
		p.v.health = 100;
		p.v.deadflag = DEAD_NO;
	}
	gEntities[2].v.origin = Vector(500, 0, 36);
	gEntities[2].v.health = 0;
	gEntities[2].v.deadflag = DEAD_DEAD;
	gEntities[4].v.health = 0;
	gEntities[4].v.deadflag = DEAD_DEAD;
	enginefuncs_t hooks{};
	int version = ENGINE_INTERFACE_VERSION;
	if (!check(runtime.provideEngineFunctions(&hooks, &version), "public engine hooks register"))
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
	services.service_compatibilityRandomSource().setTraceSink(&praiseTrace);
	const auto step = [&](float dt)
	{
		globals.time += dt;
		runtime.onStartFrame();
		runtime.onStartFramePost();
	};
	const auto death = [&](int killer = 2, int destination = MSG_ALL, edict_t *recipient = nullptr)
	{
		self.v.angles = Vector(0, 0, 0);
		self.v.v_angle = self.v.angles;
		hooks.pfnMessageBegin(destination, 103, nullptr, recipient);
		hooks.pfnWriteByte(killer);
		hooks.pfnWriteByte(3);
		hooks.pfnWriteByte(0);
		hooks.pfnWriteString("ak47");
		hooks.pfnMessageEnd();
	};
	const auto begin = [&]()
	{
		runtime.beginRoundGeneration();
		praiseSamples.clear();
		praiseIds.clear();
		self.v.health = 100;
		self.v.deadflag = DEAD_NO;
		gEntities[1].v.flags = FL_CLIENT | FL_ONGROUND;
		praiseMode = "normal";
		praiseCareer = 0;
		praiseChance = 0;
		praiseLos = true;
		gEntities[2].v.origin = Vector(500, 0, 36);
		engine.pfnIsCareerMatch = &careerMatch;
	};
	if (!check(loadInvestigationNav(runtime), "praise test places speaker on named NAV area"))
		return false;
	// Enrich the isolated SDK-free NAV fixture through its public publisher API.
	const auto originalNav = services.service_navPublisher().snapshot();
	nav::NavDocument placed;
	placed.setSourceIdentity(originalNav.document()->sourceIdentity());
	nav::NavPlace speakerPlace{};
	speakerPlace.id = 1;
	std::memcpy(speakerPlace.name, "Speaker", sizeof("Speaker"));
	placed.addPlace(speakerPlace);
	for (auto area : originalNav.document()->areas())
	{
		area.placeId = 1;
		placed.addArea(area);
	}
	auto &publisher = const_cast<nav::NavSnapshotPublisher &>(services.service_navPublisher());
	if (!check(publisher.publish(&placed, runtime.snapshot().mapGeneration) == nav::NavSnapshotResult::Published,
			"named speaker NAV is published"))
		return false;
	nav::NavAreaMatch speakerArea{};
	const auto namedNav = publisher.snapshot();
	if (!check(nav::NavQuery(namedNav).findContaining({0, 0, 36}, 64, &speakerArea) == nav::NavQueryResult::Found &&
				namedNav.document()->findArea(speakerArea.area)->placeId == 1,
			"speaker is actually inside the named NAV place"))
		return false;
	if (praiseDefaultMissing)
	{
		begin();
		gEntities[1].v.flags |= FL_FAKECLIENT;
		death();
		step(2.1f);
		const bool noPraise = praiseSamples.empty();
		services.service_compatibilityRandomSource().setTraceSink(nullptr);
		return check(noPraise, "profile-only phrase absent from default catalog cannot authorize praise");
	}
	begin();
	const auto beforeAdmission = praiseWaveProbes;
	death();
	const bool metadataOnly =
		check(praiseWaveProbes == beforeAdmission, "death admission performs no wave availability probes");
	if (!check(praiseIds.size() == 2 && praiseIds[0] == "RNG-CSBOT-ENEMIES-REMAINING-DELAY" &&
				praiseIds[1] == "RNG-CSBOT-KILL-PRAISE-DELAY",
			"death event consumes count then praise delay without event-time bank shuffle"))
		return false;
	step(1.5f);
	if (!check(praiseSamples.empty(), "praise is not immediate"))
		return false;
	step(.6f);
	if (!check(praiseSamples.size() == 1 &&
				(praiseSamples[0] == "radio/test/sir.wav" || praiseSamples[0] == "radio/test/sir2.wav"),
			"fixed human praise uses undefined place and rejects Count2/Speaker-only clips"))
		return false;
	step(2.0f);
	if (!check(praiseSamples.size() == 1, "native praise completion consumes one update before remaining count"))
		return false;
	step(.12f);
	if (!check(praiseSamples.size() == 2 && praiseSamples[1] == "radio/test/one.wav",
			"praise and remaining-count statements retain each other"))
		return false;
	begin();
	praiseCareer = 1;
	death();
	step(2.1f);
	if (!check(praiseSamples.size() == 1 && praiseSamples[0] == "radio/test/commander.wav",
			"public Career true selects Commander audio"))
		return false;
	begin();
	gEntities[1].v.flags |= FL_FAKECLIENT;
	death();
	if (!check(praiseIds.size() == 3 && praiseIds[2] == "RNG-CSBOT-KILL-PRAISE-BOT-CHANCE",
			"Bot killer consumes chance after both delay draws"))
		return false;
	step(2.1f);
	if (!check(praiseSamples.size() == 1 && praiseSamples[0] == "radio/test/bot.wav",
			"Bot killer below40 gets actual NiceShot audio"))
		return false;
	for (unsigned variant = 0; variant < 7; ++variant)
	{
		begin();
		if (variant == 0)
		{
			gEntities[1].v.flags |= FL_FAKECLIENT;
			praiseChance = 40;
		}
		if (variant == 1)
			praiseLos = false;
		if (variant == 2)
			gEntities[2].v.origin = Vector(-500, 0, 36);
		if (variant == 3)
			engine.pfnIsCareerMatch = nullptr;
		if (variant == 4)
			praiseMode = "off";
		if (variant == 5)
			death(2, MSG_ONE, &gEntities[1]);
		if (variant != 5)
			death(variant == 6 ? 1 : 2);
		praiseMode = "normal";
		step(2.1f);
		if (!check(praiseSamples.empty(),
				"chance,hidden,rear,unknown Career,OFF,unreceived and self kills create no praise"))
			return false;
	}
	begin();
	praiseMode = "minimal";
	death();
	praiseMode = "normal";
	step(2.1f);
	if (!check(praiseSamples.empty(), "profile-only Important cannot authorize MINIMAL praise or replay later"))
		return false;
	begin();
	praiseCareer = 1;
	praiseMode = "minimal";
	death();
	step(2.1f);
	if (!check(praiseSamples.size() == 1 && praiseSamples[0] == "radio/test/commander.wav",
			"default Important authorizes MINIMAL despite profile omission"))
		return false;
	begin();
	death();
	runtime.beginRoundGeneration();
	step(2.1f);
	if (!check(praiseSamples.empty(), "round reset cancels pending praise"))
		return false;
	begin();
	death();
	hooks.pfnMessageBegin(MSG_ALL, 103, nullptr, nullptr);
	hooks.pfnWriteByte(4);
	hooks.pfnWriteByte(1);
	hooks.pfnWriteByte(0);
	hooks.pfnWriteString("ak47");
	hooks.pfnMessageEnd();
	step(2.1f);
	if (!check(praiseSamples.empty(), "public observer death cancels before any later full update or revival"))
		return false;
	begin();
	const int previousVictimSerial = gEntities[2].serialnumber;
	reuseVictimOnCount = true;
	death();
	reuseVictimOnCount = false;
	gEntities[2].serialnumber = previousVictimSerial;
	step(2.1f);
	if (!check(praiseIds.size() >= 1 && praiseIds[0] == "RNG-CSBOT-ENEMIES-REMAINING-DELAY" &&
				std::find(praiseIds.begin(), praiseIds.end(), "RNG-CSBOT-KILL-PRAISE-DELAY") == praiseIds.end() &&
				praiseSamples.empty(),
			"source reuse during first RNG callback cannot create praise for a different victim"))
		return false;
	begin();
	death();
	engine.pfnWriteString = nullptr;
	step(2.1f);
	engine.pfnWriteString = &praiseString;
	step(.2f);
	if (!check(praiseSamples.empty(),
			"failed voice transport consumes praise without a fabricated fallback or later replay"))
		return false;
	begin();
	hooks.pfnMessageBegin(MSG_ONE, 104, nullptr, &self);
	hooks.pfnWriteShort(8192);
	hooks.pfnWriteShort(4096);
	hooks.pfnWriteShort(0);
	for (unsigned i = 0; i < 4; ++i)
		hooks.pfnWriteByte(255);
	hooks.pfnMessageEnd();
	death();
	step(2.1f);
	if (!check(praiseSamples.empty() &&
				std::find(praiseIds.begin(), praiseIds.end(), "RNG-CSBOT-KILL-PRAISE-DELAY") == praiseIds.end(),
			"public flash blindness rejects praise before its delay draw"))
		return false;
	step(1.1f);
	begin();
	gEntities[4].v.origin = Vector(250, 0, 36);
	runtime.onEmitSound(&gEntities[4], "weapons/sg_explode.wav", 1, 1);
	death();
	step(2.1f);
	if (!check(praiseSamples.empty() &&
				std::find(praiseIds.begin(), praiseIds.end(), "RNG-CSBOT-KILL-PRAISE-DELAY") == praiseIds.end(),
			"public smoke blocks victim-point praise even with clear world LOS"))
		return false;
	services.service_compatibilityRandomSource().setTraceSink(nullptr);
	return metadataOnly;
}
}
