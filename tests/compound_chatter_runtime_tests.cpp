#include "plugin/plugin_runtime.hpp"
namespace
{
bool testCompoundChatter(astrabot::metamod::PluginRuntime &, enginefuncs_t &, globalvars_t &);
void setupCompoundChatter(enginefuncs_t &, globalvars_t &);
}
#define ASTRABOT_EXTRA_ACTOR_TEST testCompoundChatter
#define ASTRABOT_ACTOR_ENGINE_SETUP setupCompoundChatter
#define ASTRABOT_ACTOR_PROFILE_TEXT                                                                                    \
	"Any { Name = \"Compound Speaker\" Team = any Difficulty = 1 Skill = 70 VoiceBank = compound_speech.db }\n"
#include "compat_actor_command_tests.cpp"
#include <cstdlib>
#include "fixtures/noise_runtime_nav.hpp"
namespace
{
edict_t compoundExtraEntities[2]{};
edict_t *compoundEntityOfIndex(int slot)
{
	if (slot >= 6 && slot <= 7)
		return &compoundExtraEntities[slot - 6];
	return entityOfIndex(slot);
}
int compoundIndexOfEdict(const edict_t *entity)
{
	for (unsigned i = 0; i < 2; ++i)
		if (entity == &compoundExtraEntities[i])
			return static_cast<int>(i + 6);
	return indexOfEdict(entity);
}
bool compoundEnemiesVisible = false;
bool compoundRoundDuringBankLoad = false;
bool compoundRoundDuringBankFree = false;
bool compoundBankResetRequested = false;
unsigned compoundProfileLoads = 0;
void *compoundProfileBytes = nullptr;
const char *compoundMode = "normal";
int compoundMessage = 0;
edict_t *compoundRecipient = nullptr;
std::vector<std::string> compoundSamples;
std::vector<float> compoundAudioTimes;
std::vector<int> compoundSpeakers;
globalvars_t *compoundGlobals = nullptr;
int compoundField = 0, compoundSpeaker = 0;
unsigned compoundWaveProbes = 0;
unsigned compoundThreeWaveProbes = 0;
unsigned compoundGroundTraces = 0;
bool compoundRoundDuringGround = false;
bool compoundRoundDuringCountWave = false;
bool compoundCareer = false;
int compoundCareerMatch()
{
	return compoundCareer ? 1 : 0;
}
std::vector<std::string> compoundLoads;
const char *compoundCvar(const char *name)
{
	if (std::strcmp(name, "bot_chatter") == 0)
	{
		return compoundMode;
	}
	const auto *value = getCvar(name);
	return value && value->string ? value->string : "";
}
unsigned char *compoundLoad(char *name, int *length)
{
	compoundLoads.emplace_back(name);
	const char *text = nullptr;
	if (std::strcmp(name, "BotChatter.db") == 0)
	{
		text =
			"BaseDir radio/test/ Place Arena Important Radio EVENT_RADIO_SECTOR_CLEAR arena.wav End "
			"Chatter pain pain.wav End Chatter Help Radio EVENT_RADIO_NEED_BACKUP help.wav End "
			"Chatter EnemySpotted Radio EVENT_RADIO_ENEMY_SPOTTED Count 2 two.wav Count 3 three.wav Count Many many.wav End Chatter CommanderDown Important down.wav End";
	}
	if (std::strcmp(name, "compound_speech.db") == 0)
	{
		++compoundProfileLoads;
		if (compoundRoundDuringBankLoad)
		{
			compoundRoundDuringBankLoad = false;
			astrabot::metamod::PluginRuntime::instance().beginRoundGeneration();
		}
		text =
			"BaseDir radio/test/ Place Arena arena.wav End Chatter pain pain.wav End Chatter Help Place ANY help.wav End "
			"Chatter EnemySpotted Place UNDEFINED Count 2 two.wav Count 3 three.wav Count Many many.wav End Chatter CommanderDown down.wav End";
	}
	if (!text)
	{
		*length = 0;
		return nullptr;
	}
	*length = static_cast<int>(std::strlen(text));
	auto *bytes = new unsigned char[static_cast<std::size_t>(*length) + 1];
	std::memcpy(bytes, text, static_cast<std::size_t>(*length) + 1);
	if (std::strcmp(name, "compound_speech.db") == 0)
		compoundProfileBytes = bytes;
	return bytes;
}
void compoundFree(void *memory)
{
	const bool profile = memory == compoundProfileBytes;
	if (profile)
		compoundProfileBytes = nullptr;
	delete[] static_cast<unsigned char *>(memory);
	if (profile && compoundRoundDuringBankFree)
	{
		compoundRoundDuringBankFree = false;
		astrabot::metamod::PluginRuntime::instance().beginRoundGeneration();
	}
}
int compoundFileSize(char *)
{
	++compoundWaveProbes;
	return 100;
}
unsigned compoundWaveLength(const char *path)
{
	if (path && std::strcmp(path, "sound/radio/test/three.wav") == 0)
	{
		++compoundThreeWaveProbes;
		if (compoundRoundDuringCountWave)
		{
			compoundRoundDuringCountWave = false;
			auto &runtime = astrabot::metamod::PluginRuntime::instance();
			runtime.beginRoundGeneration();
			gEntities[4].v.health = 0;
			gEntities[4].v.deadflag = DEAD_DEAD;
			runtime.onMessageBegin(MSG_ALL, 103, nullptr, nullptr);
			runtime.onWriteByte(0);
			runtime.onWriteByte(5);
			runtime.onWriteByte(0);
			runtime.onWriteString("world");
			runtime.onMessageEnd();
		}
	}
	return 200;
}
int compoundMessageId(plid_t id, const char *name, int *size)
{
	if (std::strcmp(name, "DeathMsg") == 0)
		return 103;
	if (std::strcmp(name, "SendAudio") == 0)
	{
		return 101;
	}
	if (std::strcmp(name, "BotVoice") == 0)
	{
		return 102;
	}
	return siteFixtureUserMessageId(id, name, size);
}
void compoundBegin(int, int type, const float *, edict_t *recipient)
{
	compoundMessage = type;
	compoundRecipient = recipient;
	compoundField = 0;
	compoundSpeaker = 0;
}
void compoundNumber(int value)
{
	if (compoundMessage == 101 && compoundField++ == 0)
		compoundSpeaker = value;
}
void compoundString(const char *sample)
{
	if (compoundMessage == 101 && compoundRecipient == &gEntities[0])
	{
		compoundSamples.emplace_back(sample);
		compoundAudioTimes.push_back(compoundGlobals ? compoundGlobals->time : -1);
		compoundSpeakers.push_back(compoundSpeaker);
	}
}
void compoundEnd()
{
}
void compoundVisibility(const float *from, const float *end, int flags, edict_t *, TraceResult *trace)
{
	*trace = {};
	trace->vecEndPos = Vector(end[0], end[1], end[2]);
	if (flags == 1 && from[2] - end[2] > 9000)
	{
		++compoundGroundTraces;
		trace->flFraction = .5f;
		trace->vecEndPos.z = 0;
		if (compoundRoundDuringGround)
		{
			compoundRoundDuringGround = false;
			auto &runtime = astrabot::metamod::PluginRuntime::instance();
			runtime.beginRoundGeneration();
			gEntities[4].v.health = 0;
			gEntities[4].v.deadflag = DEAD_DEAD;
			runtime.onMessageBegin(MSG_ALL, 103, nullptr, nullptr);
			runtime.onWriteByte(0);
			runtime.onWriteByte(5);
			runtime.onWriteByte(0);
			runtime.onWriteString("world");
			runtime.onMessageEnd();
		}
		return;
	}
	trace->flFraction = compoundEnemiesVisible ? 1.0f : 0.0f;
}
std::vector<std::string> compoundRadioCommands;
std::vector<float> compoundRadioTimes;
unsigned compoundRoundOnRadioCommand = 0;
void compoundRadioClientCommand(edict_t *self)
{
	const auto *command = astrabot::metamod::HookCommandArgv(0);
	const bool menu = std::strcmp(command, "radio1") == 0 || std::strcmp(command, "radio2") == 0 ||
		std::strcmp(command, "radio3") == 0;
	if (!menu && std::strcmp(command, "menuselect") != 0)
	{
		hookedGameClientCommand(self);
		return;
	}
	compoundRadioCommands.push_back(
		menu ? command : std::string("menuselect:") + astrabot::metamod::HookCommandArgv(1));
	if (menu)
		compoundRadioTimes.push_back(compoundGlobals->time);
	// Deliberately no SendAudio: the host quota can decline a valid attempt.
	if (compoundRoundOnRadioCommand && compoundRadioCommands.size() == compoundRoundOnRadioCommand)
	{
		compoundRoundOnRadioCommand = 0;
		compoundMode = "normal";
		auto &runtime = astrabot::metamod::PluginRuntime::instance();
		runtime.beginRoundGeneration();
		gEntities[4].v.health = 0;
		gEntities[4].v.deadflag = DEAD_DEAD;
		runtime.onMessageBegin(MSG_ALL, 103, nullptr, nullptr);
		runtime.onWriteByte(0);
		runtime.onWriteByte(5);
		runtime.onWriteByte(0);
		runtime.onWriteString("world");
		runtime.onMessageEnd();
	}
}
void setupCompoundChatter(enginefuncs_t &engine, globalvars_t &globals)
{
	static bool configured = false;
	if (!configured)
	{
		configured = true;
#ifdef _MSC_VER
		char *reset = nullptr;
		std::size_t length = 0;
		(void)_dupenv_s(&reset, &length, "ASTRABOT_TEST_BANK_RESET");
#else
		const auto *reset = std::getenv("ASTRABOT_TEST_BANK_RESET");
#endif
		compoundRoundDuringBankLoad = reset && std::strcmp(reset, "load") == 0;
		compoundRoundDuringBankFree = reset && std::strcmp(reset, "free") == 0;
		compoundBankResetRequested = compoundRoundDuringBankLoad || compoundRoundDuringBankFree;
#ifdef _MSC_VER
		std::free(reset);
#endif
	}
	compoundGlobals = &globals;
	engine.pfnPEntityOfEntIndex = &compoundEntityOfIndex;
	engine.pfnIndexOfEdict = &compoundIndexOfEdict;
	engine.pfnIsCareerMatch = &compoundCareerMatch;
	engine.pfnRandomFloat = &hearingFixtureRandomFloat;
	engine.pfnCVarGetString = &compoundCvar;
	engine.pfnLoadFileForMe = &compoundLoad;
	engine.pfnFreeFile = &compoundFree;
	engine.pfnGetFileSize = &compoundFileSize;
	engine.pfnGetApproxWavePlayLen = &compoundWaveLength;
	engine.pfnMessageBegin = &compoundBegin;
	engine.pfnWriteByte = &compoundNumber;
	engine.pfnWriteShort = &compoundNumber;
	engine.pfnWriteString = &compoundString;
	engine.pfnMessageEnd = &compoundEnd;
	engine.pfnTraceLine = &compoundVisibility;
	gpMetaUtilFuncs->pfnGetUserMsgID = &compoundMessageId;
}
bool testCompoundChatter(astrabot::metamod::PluginRuntime &runtime, enginefuncs_t &engine, globalvars_t &globals)
{
	using namespace astrabot;
	setupCompoundChatter(engine, globals);
	runtime.giveEnginePointers(&engine, &globals);
	setCvarFloat("bot_auto_vacate", 0);
	setCvarFloat("bot_allow_rogues", 0);
	gObjectiveEntitiesAvailable = false;
	globals.maxClients = 5;
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
		auto &peer = *compoundEntityOfIndex(static_cast<int>(i + 1));
		peer.free = false;
		peer.serialnumber = static_cast<int>(i + 10);
		peer.v.netname = gEntities[4].v.netname;
		peer.v.flags = FL_CLIENT | FL_ONGROUND;
		peer.v.health = 100;
		peer.v.deadflag = DEAD_NO;
		peer.v.team = i == 4 ? 1 : 2;
		peer.v.origin = Vector(i == 4 ? -120.0f : 120.0f, static_cast<float>(i * 4), 36);
	}
	enginefuncs_t hooks{};
	int version = ENGINE_INTERFACE_VERSION;
	if (!check(runtime.provideEngineFunctions(&hooks, &version), "registered compound receipt hooks"))
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
	for (int slot = 1; slot <= 5; ++slot)
	{
		team(slot, slot == 1 || slot == 5 ? "TERRORIST" : "CT");
	}
	if (!check(loadHearingNav(runtime), "real compound fixture NAV"))
	{
		return false;
	}
	auto &ports = static_cast<metamod::BotBehaviorServices &>(runtime);
	const auto source = ports.service_navPublisher().snapshot();
	nav::NavDocument named;
	named.setSourceIdentity(source.document()->sourceIdentity());
	nav::NavPlace arena{};
	arena.id = 77;
	std::memcpy(arena.name, "Arena", sizeof("Arena"));
	named.addPlace(arena);
	for (auto area : source.document()->areas())
	{
		area.placeId = 77;
		area.extent.lo.x = area.extent.lo.y = -256;
		area.extent.hi.x = area.extent.hi.y = 256;
		named.addArea(area);
	}
	auto &publisher = const_cast<nav::NavSnapshotPublisher &>(ports.service_navPublisher());
	if (!check(publisher.publish(&named, runtime.snapshot().mapGeneration) == nav::NavSnapshotResult::Published,
			"real compound speaker NAV place"))
		return false;
	const auto step = [&](float seconds)
	{
		globals.time += seconds;
		runtime.onStartFrame();
		runtime.onStartFramePost();
	};
	// Start where the native NeedBackup interval from -1 has elapsed.
	globals.time = (std::max)(globals.time, 9.0f);
	runtime.beginRoundGeneration();
	hooks.pfnMessageBegin(MSG_ONE, 94, nullptr, &self);
	hooks.pfnWriteByte(1);
	hooks.pfnWriteByte(22);
	hooks.pfnWriteByte(30);
	hooks.pfnMessageEnd();
	step(.125f);
	step(.125f);
	const auto &managed = ports.service_managedBotHandles()[0];
	const world::ActorKey ownKey{managed.actor.slot, managed.actor.actorGeneration};
	const world::FrameIdentity ownFrame{
		runtime.snapshot().mapGeneration, runtime.snapshot().roundGeneration, ports.service_adapterFrameCount()};
	const auto navRevision = ports.service_navPublisher().snapshot().revision();
	if (!check(ports.navigation_knownArea(0, ownKey, ownFrame, navRevision) == named.areas().front().id &&
				ports.navigation_knownArea(0, ownKey, ownFrame, navRevision + 1) == 0,
			"actual full-update producer publishes only the matching managed actor/frame/NAV last area"))
		return false;
	if (compoundBankResetRequested &&
		!check(!compoundRoundDuringBankLoad && !compoundRoundDuringBankFree && compoundProfileLoads >= 2,
			"round interruption during first profile load/free leaves the bank retryable in the next round"))
		return false;
	compoundSamples.clear();
	compoundEnemiesVisible = true;
	step(.5f);
	const bool locationFirst = check(compoundSamples == std::vector<std::string>{"radio/test/arena.wav"},
		"outnumbered native NeedBackup speaks its implicit Place before Help");
	for (unsigned frame = 0; frame < 8; ++frame)
		step(.125f);
	const bool complete =
		check(compoundSamples == std::vector<std::string>{"radio/test/arena.wav", "radio/test/help.wav"},
			"NeedBackup preserves Place then Help as parts of one statement");
	if (!locationFirst || !complete)
	{
		for (const auto &sample : compoundSamples)
			std::fprintf(stderr, "compound sample: %s\n", sample.c_str());
	}
	const bool helpControl =
		check(std::find(compoundSamples.begin(), compoundSamples.end(), "radio/test/help.wav") != compoundSamples.end(),
			"observed outnumbering actually delivers the existing Help control clip");
	if (!(helpControl && locationFirst && complete))
		return false;
	if (!check(compoundSpeakers.size() >= 2 && compoundSpeakers[0] == 1 && compoundSpeakers[1] == 1 &&
				compoundAudioTimes[1] > compoundAudioTimes[0],
			"actual multipart packets retain original sender and ordered timestamps"))
		return false;
	// A second NeedBackup is admitted after its10sec cooldown, but before the
	// manager's20sec Place interval. The silent Place still reserves its .2+.1.
	compoundEnemiesVisible = false;
	// Keep sampling during the quiet interval so the delayed contact can clear.
	for (unsigned frame = 0; frame < 96; ++frame)
		step(.125f);
	compoundSamples.clear();
	compoundEnemiesVisible = true;
	step(.5f);
	if (!check(compoundSamples.empty(), "recent Place is suppressed in production while statement remains active"))
		return false;
	step(.125f);
	step(.125f);
	if (!check(compoundSamples.empty(), "suppressed Place reserves selected duration before Help"))
		return false;
	step(.125f);
	if (!check(compoundSamples == std::vector<std::string>{"radio/test/help.wav"},
			"suppressed Place advances to Help only after its reserved gap"))
		return false;
	for (unsigned i = 0; i < 4; ++i)
		step(.125f);
	const auto weapon = [&]()
	{
		hooks.pfnMessageBegin(MSG_ONE, 94, nullptr, &self);
		hooks.pfnWriteByte(1);
		hooks.pfnWriteByte(22);
		hooks.pfnWriteByte(30);
		hooks.pfnMessageEnd();
	};
	const auto death = [&](int slot)
	{
		hooks.pfnMessageBegin(MSG_ALL, 103, nullptr, nullptr);
		hooks.pfnWriteByte(0);
		hooks.pfnWriteByte(slot);
		hooks.pfnWriteByte(0);
		hooks.pfnWriteString("world");
		hooks.pfnMessageEnd();
	};
	bool offmeshEnemies = false;
	unsigned initialVisibleEnemies = 3;
	unsigned initialVisibleFriends = 2;
	const auto beginCount = [&]() -> bool
	{
		compoundEnemiesVisible = false;
		globals.maxClients = 7;
		self.v.health = 100;
		self.v.deadflag = DEAD_NO;
		self.v.flags = FL_CLIENT | FL_FAKECLIENT | FL_ONGROUND;
		self.v.v_angle = Vector(0, 0, 0);
		self.v.angles = self.v.v_angle;
		for (unsigned i = 1; i < 7; ++i)
		{
			auto &peer = *compoundEntityOfIndex(static_cast<int>(i + 1));
			peer.free = false;
			peer.serialnumber = static_cast<int>(i + 10);
			peer.v.flags = FL_CLIENT | FL_ONGROUND;
			peer.v.health = i >= 4 ? 0.0f : 100.0f;
			peer.v.deadflag = i >= 4 ? DEAD_DEAD : DEAD_NO;
			peer.v.team = i < 4 ? 2 : 1;
			peer.v.origin =
				Vector(i == 6 || (i == 5 && initialVisibleFriends == 1) || (i == 3 && initialVisibleEnemies == 2)
						? -120.0f
						: (offmeshEnemies && i < 4 ? 500.0f : 120.0f),
					static_cast<float>(i * 4), 36);
		}
		runtime.beginRoundGeneration();
		for (int slot = 1; slot <= 7; ++slot)
			team(slot, slot == 1 || slot >= 5 ? "TERRORIST" : "CT");
		weapon();
		step(.125f);
		step(.125f);
		// Establish Attack against one opponent. Extra opponents enter only once
		// the teammates are alive, so this prelude cannot trigger ScaredEmote.
		const float secondEnemyX = gEntities[2].v.origin.x;
		const float thirdEnemyX = gEntities[3].v.origin.x;
		gEntities[2].v.origin.x = gEntities[3].v.origin.x = -200;
		compoundEnemiesVisible = true;
		// The 0.3 second profile needs three full updates after the first sighting.
		for (unsigned frame = 0; frame < 8; ++frame)
			step(.125f);
		gEntities[2].v.origin.x = secondEnemyX;
		gEntities[3].v.origin.x = thirdEnemyX;
		if (!check(static_cast<metamod::BotCommandServices &>(runtime)
					   .behavior_managedBotStateMachines(0)
					   .attackOverlayActive(),
				"actual combat is established before admitting the conditional enemy report"))
			return false;
		for (unsigned slot = 5; slot <= 7; ++slot)
		{
			auto *peer = compoundEntityOfIndex(static_cast<int>(slot));
			peer->v.health = 100;
			peer->v.deadflag = DEAD_NO;
		}
		compoundSamples.clear();
		step(.125f);
		return true;
	};
	if (!beginCount())
		return false;
	if (!check(compoundSamples == std::vector<std::string>{"radio/test/arena.wav"},
			"balanced initial contacts admit EnemySpotted with current visible enemy Place"))
		return false;
	gEntities[3].v.health = 0;
	gEntities[3].v.deadflag = DEAD_DEAD;
	death(4);
	for (unsigned frame = 0; frame < 16; ++frame)
		step(.125f);
	if (!check(compoundSamples == std::vector<std::string>{"radio/test/arena.wav"},
			"accumulation retains strict two-second boundary measured from initial speech"))
		return false;
	step(.125f);
	if (!check(compoundSamples == std::vector<std::string>{"radio/test/arena.wav", "radio/test/two.wav"},
			"current-count part reads changed remaining enemies and explicit undefined place at playback time"))
		return false;
	if (!beginCount())
		return false;
	for (unsigned frame = 0; frame < 4; ++frame)
		step(.125f);
	self.v.health = 0;
	self.v.deadflag = DEAD_DEAD;
	death(1);
	if (!check(compoundSamples == std::vector<std::string>{"radio/test/arena.wav", "radio/test/pain.wav"},
			"death during silent accumulation interrupts logical speaking with actual native pain"))
		return false;
	initialVisibleEnemies = 2;
	initialVisibleFriends = 1;
	if (!beginCount())
		return false;
	gEntities[3].v.origin.x = 120;
	for (unsigned frame = 0; frame < 17; ++frame)
		step(.125f);
	if (!check(compoundSamples == std::vector<std::string>{"radio/test/arena.wav", "radio/test/help.wav"},
			"current-count context chooses Help from later outnumbering without an admission-time NeedBackup"))
		return false;
	initialVisibleEnemies = 3;
	initialVisibleFriends = 2;
	if (!beginCount())
		return false;
	compoundCareer = true;
	compoundRoundDuringCountWave = true;
	const auto beforeThreeProbe = compoundThreeWaveProbes;
	for (unsigned frame = 0; frame < 18; ++frame)
		step(.125f);
	compoundCareer = false;
	if (!check(!compoundRoundDuringCountWave && compoundThreeWaveProbes == beforeThreeProbe + 1 &&
				compoundSamples == std::vector<std::string>{"radio/test/arena.wav", "radio/test/down.wav"},
			"round mutation during waveform probe cancels old part and preserves fresh same-owner CommanderDown admission"))
		return false;
	if (!beginCount())
		return false;
	for (unsigned frame = 0; frame < 17; ++frame)
		step(.125f);
	if (!check(compoundThreeWaveProbes == beforeThreeProbe + 2 &&
				compoundSamples == std::vector<std::string>{"radio/test/arena.wav", "radio/test/three.wav"},
			"stale waveform callback does not cache duration into a later round's same-named sample"))
		return false;
	offmeshEnemies = true;
	const auto beforeGround = compoundGroundTraces;
	if (!beginCount())
		return false;
	if (!check(compoundGroundTraces == beforeGround + 30 &&
				compoundSamples == std::vector<std::string>{"radio/test/arena.wav"},
			"off-mesh visible humans use native public ground traces before their Place statement"))
		return false;
	compoundCareer = true;
	compoundRoundDuringGround = true;
	const auto beforeCancelledGround = compoundGroundTraces;
	if (!beginCount())
		return false;
	if (!check(
			!compoundRoundDuringGround && compoundGroundTraces == beforeCancelledGround + 1 && compoundSamples.empty(),
			"round change during first ground trace cancels old Place construction immediately"))
		return false;
	step(.125f);
	compoundCareer = false;
	if (!check(compoundSamples == std::vector<std::string>{"radio/test/down.wav"},
			"fresh same-owner CommanderDown admitted during ground query survives cancelled old construction"))
		return false;
	offmeshEnemies = false;
	const auto originalRadioCommand = gHookedGameDllTable.pfnClientCommand;
	gHookedGameDllTable.pfnClientCommand = &compoundRadioClientCommand;
	for (bool fallback : {false, true})
	{
		compoundMode = fallback ? "normal" : "radio";
		engine.pfnWriteString = fallback ? nullptr : &compoundString;
		compoundRadioCommands.clear();
		compoundRadioTimes.clear();
		if (!beginCount())
			return false;
		const std::vector<std::string> firstPart{"radio3", "menuselect:4", "menuselect:10"};
		if (!check(compoundRadioCommands == firstPart && compoundSamples.empty(),
				"mapped Place issues one native menu/select/close without a public receipt"))
			return false;
		const float first = compoundRadioTimes.front();
		for (unsigned tick = 0; tick < 3; ++tick)
			step(.25f);
		if (!check(compoundRadioCommands == firstPart, "quota-declined radio does not retry the selected part"))
			return false;
		globals.time = first + 2.0f + .1f;
		step(0);
		if (!check(compoundRadioCommands == firstPart,
				"radio part retains strict duration2 plus gap.1 even without receipt"))
			return false;
		step(.125f);
		if (!check(compoundRadioCommands == firstPart,
				"entering accumulation consumes its own update after the radio deadline"))
			return false;
		step(.125f);
		if (!check(compoundRadioCommands ==
					std::vector<std::string>{
						"radio3", "menuselect:4", "menuselect:10", "radio3", "menuselect:2", "menuselect:10"},
				"later current-count radio proceeds once after the declined Place reservation"))
			return false;
	}
	engine.pfnWriteString = &compoundString;
	for (unsigned boundary = 1; boundary <= 3; ++boundary)
	{
		compoundMode = "radio";
		compoundCareer = true;
		compoundRadioCommands.clear();
		compoundRadioTimes.clear();
		compoundRoundOnRadioCommand = boundary;
		if (!beginCount())
			return false;
		if (!check(
				compoundRoundOnRadioCommand == 0 && compoundRadioCommands.size() == boundary && compoundSamples.empty(),
				"round mutation at a radio command boundary stops stale continuation"))
			return false;
		step(.125f);
		compoundCareer = false;
		if (!check(compoundSamples == std::vector<std::string>{"radio/test/down.wav"},
				"fresh same-owner CommanderDown survives cancelled radio completion"))
			return false;
	}
	compoundMode = "normal";
	gHookedGameDllTable.pfnClientCommand = originalRadioCommand;
	return true;
}
}
