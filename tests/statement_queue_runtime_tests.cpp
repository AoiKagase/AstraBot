#include "plugin/plugin_runtime.hpp"
namespace
{
bool testStatementQueue(astrabot::metamod::PluginRuntime &, enginefuncs_t &, globalvars_t &);
void setupStatementQueue(enginefuncs_t &, globalvars_t &);
}
#define ASTRABOT_EXTRA_ACTOR_TEST testStatementQueue
#define ASTRABOT_ACTOR_ENGINE_SETUP setupStatementQueue
#define ASTRABOT_ACTOR_PROFILE_TEXT                                                                                    \
	"Any { Name = \"Queue Speaker\" Team = any Difficulty = 1 Skill = 70 VoiceBank = queue_speech.db }\n"              \
	"Any { Name = \"Queue Peer\" Team = any Difficulty = 1 Skill = 70 VoiceBank = queue_speech.db }\n"
#include "compat_actor_command_tests.cpp"
#include "runtime/observation/bot_observation_runtime.hpp"
#include "runtime/behavior/bot_behavior_runtime.hpp"
#include <memory>
#include "fixtures/noise_runtime_nav.hpp"
namespace
{
bool queueEnemiesVisible = false;
const char *queueMode = "normal";
astrabot::metamod::BotObservationRuntime *queueInputObserver = nullptr;
unsigned queueInputMutation = 0;
bool queueInputBeginReset = false;
astrabot::metamod::BotBehaviorRuntime *queuePlanBehavior = nullptr;
unsigned queuePlanMutation = 0;
float queuePlanRandom(float low, float high);
int queueMessage = 0;
edict_t *queueRecipient = nullptr;
std::vector<std::string> queueSamples;
unsigned queueWaveProbes = 0;
std::vector<std::string> queueLoads;
const char *queueCvar(const char *name)
{
	if (std::strcmp(name, "bot_chatter") == 0)
	{
		return queueMode;
	}
	const auto *value = getCvar(name);
	return value && value->string ? value->string : "";
}
unsigned char *queueLoad(char *name, int *length)
{
	queueLoads.emplace_back(name);
	const char *text = nullptr;
	if (std::strcmp(name, "BotChatter.db") == 0)
	{
		text = "BaseDir radio/test/ Chatter Affirmative Radio EVENT_RADIO_AFFIRMATIVE ack.wav End "
			   "Chatter Help Radio EVENT_RADIO_NEED_BACKUP help.wav End "
			   "Chatter EscortingHostages escort.wav End Chatter AgreeWithPlan agree.wav End";
	}
	if (std::strcmp(name, "queue_speech.db") == 0)
	{
		text = "BaseDir radio/test/ Chatter Affirmative Place UNDEFINED Count 3 ack_wrong.wav Count Many ack.wav End "
			   "Chatter Help Place UNDEFINED help.wav End";
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
void queueFree(void *memory)
{
	delete[] static_cast<unsigned char *>(memory);
}
int queueFileSize(char *)
{
	++queueWaveProbes;
	return 100;
}
unsigned queueWaveLength(const char *)
{
	return 200;
}
int queueMessageId(plid_t id, const char *name, int *size)
{
	if (std::strcmp(name, "SendAudio") == 0)
	{
		if (queueInputBeginReset && queueInputObserver)
		{
			queueInputBeginReset = false;
			queueInputObserver->resetActorExecutionState(0);
		}
		return 101;
	}
	if (std::strcmp(name, "BotVoice") == 0)
	{
		return 102;
	}
	return siteFixtureUserMessageId(id, name, size);
}
void queueBegin(int, int type, const float *, edict_t *recipient)
{
	queueMessage = type;
	queueRecipient = recipient;
}
void queueNumber(int)
{
}
void queueString(const char *sample)
{
	if (queueMessage == 101 && queueRecipient == &gEntities[0])
	{
		queueSamples.emplace_back(sample);
	}
}
void queueEnd()
{
}
void queueVisibility(const float *, const float *end, int, edict_t *, TraceResult *trace)
{
	*trace = {};
	trace->vecEndPos = Vector(end[0], end[1], end[2]);
	trace->flFraction = queueEnemiesVisible ? 1.0f : 0.0f;
}
void setupStatementQueue(enginefuncs_t &engine, globalvars_t &)
{
	engine.pfnRandomFloat = &queuePlanRandom;
	engine.pfnCVarGetString = &queueCvar;
	engine.pfnLoadFileForMe = &queueLoad;
	engine.pfnFreeFile = &queueFree;
	engine.pfnGetFileSize = &queueFileSize;
	engine.pfnGetApproxWavePlayLen = &queueWaveLength;
	engine.pfnMessageBegin = &queueBegin;
	engine.pfnWriteByte = &queueNumber;
	engine.pfnWriteShort = &queueNumber;
	engine.pfnWriteString = &queueString;
	engine.pfnMessageEnd = &queueEnd;
	engine.pfnTraceLine = &queueVisibility;
	gpMetaUtilFuncs->pfnGetUserMsgID = &queueMessageId;
}
void queueInputRadio(astrabot::metamod::BotObservationRuntime &observer, const char *token)
{
	observer.onMessageBegin(MSG_ONE, 101, nullptr, &gEntities[0]);
	observer.onWriteByte(5);
	observer.onWriteString(token);
	observer.onWriteShort(100);
	observer.onMessageEnd();
}
edict_t *queueInputEntity(int slot)
{
	if (slot == 5 && queueInputObserver && queueInputMutation)
	{
		const auto mutation = queueInputMutation;
		queueInputMutation = 0;
		if (mutation != 4)
			queueInputObserver->resetActorExecutionState(0);
		if (mutation == 2 || mutation == 4)
			queueInputRadio(*queueInputObserver, "%!MRAD_BACKUP");
		if (mutation == 3)
			queueInputRadio(*queueInputObserver, "radio/test/unrelated.wav");
	}
	return entityOfIndex(slot);
}
bool testQueueRadioReset(astrabot::metamod::PluginRuntime &runtime, enginefuncs_t &engine)
{
	using namespace astrabot;
	auto observer = std::make_unique<metamod::BotObservationRuntime>();
	observer->bindServices(static_cast<metamod::BotObservationServices &>(runtime));
	auto &services = static_cast<metamod::BotObservationServices &>(runtime);
	const auto actor = services.service_managedBotHandles()[0].actor;
	const auto frame = runtime.snapshot();
	const auto frameTick = services.service_adapterFrameCount();
	const int serial = gEntities[0].serialnumber;
	observer->notifyTeamInfo(1, "TERRORIST");
	observer->notifyTeamInfo(5, "TERRORIST");
	queueInputObserver = observer.get();
	struct RestoreInputEngine
	{
		enginefuncs_t &engine;
		edict_t *(*original)(int);
		~RestoreInputEngine()
		{
			engine.pfnPEntityOfEntIndex = original;
			queueInputObserver = nullptr;
			queueInputMutation = 0;
			queueInputBeginReset = false;
		}
	} restore{engine, engine.pfnPEntityOfEntIndex};
	engine.pfnPEntityOfEntIndex = &queueInputEntity;
	bool ok = true;
	queueInputRadio(*observer, "%!MRAD_GO");
	const auto first = observer->scenarioRadio(0).sequence;
	ok = check(first != 0 && observer->scenarioRadio(0).command == compat::ScenarioRadioCommand::GoGoGo,
			 "actual public decoder records the current human order before any reset") &&
		ok;
	observer->resetActorExecutionState(0);
	ok = check(observer->scenarioRadio(0).command == compat::ScenarioRadioCommand::None,
			 "same-owner actor execution reset clears an unprepared human order") &&
		ok;
	queueInputRadio(*observer, "%!MRAD_GO");
	ok = check(observer->scenarioRadio(0).sequence > first &&
				 observer->scenarioRadio(0).command == compat::ScenarioRadioCommand::GoGoGo,
			 "same-time identical new order after reset is not suppressed as the old duplicate") &&
		ok;
	observer->onActorRemembered(0, services.service_managedBotHandles()[0]);
	ok = check(observer->scenarioRadio(0).command == compat::ScenarioRadioCommand::None,
			 "same-serial actor refresh cannot retain an old radio subject") &&
		ok;
	observer->notifyTeamInfo(1, "TERRORIST");
	observer->notifyTeamInfo(5, "TERRORIST");
	queueInputRadio(*observer, "%!MRAD_GO");
	observer->resetSharedExecutionState();
	ok = check(observer->scenarioRadio(0).command == compat::ScenarioRadioCommand::None,
			 "shared execution reset invalidates all old unprepared radio input") &&
		ok;
	observer->onMessageBegin(MSG_ONE, 101, nullptr, &gEntities[0]);
	observer->onWriteByte(5);
	observer->onWriteString("%!MRAD_GO");
	observer->onWriteShort(100);
	observer->resetActorExecutionState(0);
	observer->onMessageEnd();
	ok = check(observer->scenarioRadio(0).command == compat::ScenarioRadioCommand::None,
			 "radio packet begun before reset cannot complete into the new input life") &&
		ok;
	observer->resetActorExecutionState(0);
	queueInputBeginReset = true;
	queueInputRadio(*observer, "%!MRAD_GO");
	ok = check(!queueInputBeginReset && observer->scenarioRadio(0).command == compat::ScenarioRadioCommand::None,
			 "message-ID callback reset cannot requalify the packet that was already beginning") &&
		ok;
	for (unsigned mutation = 1; mutation <= 4; ++mutation)
	{
		observer->resetActorExecutionState(0);
		queueInputMutation = mutation;
		queueInputRadio(*observer, "%!MRAD_GO");
		const auto expected = (mutation == 2 || mutation == 4) ? compat::ScenarioRadioCommand::NeedBackup
															   : compat::ScenarioRadioCommand::None;
		ok = check(queueInputMutation == 0 && observer->scenarioRadio(0).command == expected,
				 "reset during sender lookup drops old input and preserves only a fresh current command") &&
			ok;
	}
	queueInputRadio(*observer, "%!MRAD_GO");
	ok = check(observer->scenarioRadio(0).command == compat::ScenarioRadioCommand::GoGoGo,
			 "current source remains usable after all reset callback cases") &&
		ok;
	const auto &current = services.service_managedBotHandles()[0].actor;
	const auto after = runtime.snapshot();
	ok = check(actor.slot == current.slot && actor.actorGeneration == current.actorGeneration &&
				 serial == gEntities[0].serialnumber && frame.mapGeneration == after.mapGeneration &&
				 frame.roundGeneration == after.roundGeneration && frameTick == services.service_adapterFrameCount(),
			 "reset controls retain the same real actor key, serial and map/round") &&
		ok;
	return ok;
}
float queuePlanRandom(float low, float high)
{
	if (low == .5f && high == 1 && queuePlanMutation && queuePlanBehavior)
	{
		const auto mutation = queuePlanMutation;
		queuePlanMutation = 0;
		if (mutation == 1)
			queuePlanBehavior->resetSharedExecutionState();
		else
		{
			gEntities[0].v.team = 2;
			auto &runtime = astrabot::metamod::PluginRuntime::instance();
			runtime.onMessageBegin(MSG_ALL, 86, nullptr, nullptr);
			runtime.onWriteByte(1);
			runtime.onWriteString("CT");
			runtime.onMessageEnd();
		}
	}
	return hearingFixtureRandomFloat(low, high);
}
bool testPlanAgreementRuntime(astrabot::metamod::PluginRuntime &runtime, globalvars_t &globals)
{
	using namespace astrabot;
	queueEnemiesVisible = false;
	queueMode = "normal";
	runtime.setCompatibilityFloat("bot_quota", 2);
	if (!check(runtime.executeCompatibilityCommand(request("bot_add_t", 1U, "Queue Peer")) ==
				metamod::CompatibilityCommandResult::Handled,
			"second real managed owner is created through the public command"))
		return false;
	runtime.beginRoundGeneration();
	auto &ports = static_cast<metamod::BotBehaviorServices &>(runtime);
	bool ok = true;
	struct RestorePlanMutation
	{
		~RestorePlanMutation()
		{
			queuePlanBehavior = nullptr;
			queuePlanMutation = 0;
		}
	} restore;
	for (unsigned mutation = 0; mutation < 3; ++mutation)
	{
		auto behavior = std::make_unique<metamod::BotBehaviorRuntime>();
		behavior->bindServices(ports);
		queuePlanBehavior = behavior.get();
		for (std::size_t i = 0; i < 2; ++i)
		{
			auto &entity = gEntities[i];
			entity.v.flags = FL_CLIENT | FL_FAKECLIENT | FL_ONGROUND;
			entity.v.health = 100;
			entity.v.deadflag = DEAD_NO;
			entity.v.team = 1;
			runtime.onMessageBegin(MSG_ALL, 86, nullptr, nullptr);
			runtime.onWriteByte(static_cast<int>(i + 1));
			runtime.onWriteString("TERRORIST");
			runtime.onMessageEnd();
			behavior->onActorRemembered(i, ports.service_managedBotHandles()[i]);
			// Existing typed state input supplies the represented escort trigger.
			// This component test does not claim to exercise the hostage producer.
			const_cast<compat::ScenarioBehaviorDecision &>(behavior->scenarioDecisions(i)).escortInferred = true;
		}
		globals.time += 11;
		queueSamples.clear();
		queuePlanMutation = 0;
		behavior->updateOutgoingPublicRadio(0);
		if (queueSamples != std::vector<std::string>{"radio/test/escort.wav"})
		{
			bool selectedCurrent = false;
			const auto selected = behavior->activeTeamChatter(0, 1, &selectedCurrent);
			std::fprintf(stderr,
				"plan fixture mutation=%u current=%u selected=%llu topic=%u start=%g now=%g samples=%zu\n", mutation,
				selectedCurrent ? 1U : 0U, static_cast<unsigned long long>(selected.order),
				static_cast<unsigned>(selected.topic), selected.start, globals.time, queueSamples.size());
			for (const auto &sample : queueSamples)
				std::fprintf(stderr, "plan sample: %s\n", sample.c_str());
		}
		ok = check(queueSamples == std::vector<std::string>{"radio/test/escort.wav"},
				 "first real plan audio is the control before conversion") &&
			ok;
		queuePlanMutation = mutation;
		behavior->updateOutgoingPublicRadio(1);
		if (mutation)
		{
			bool current = false;
			const auto remaining = behavior->activeTeamChatter(1, 1, &current);
			ok = check(queuePlanMutation == 0 && current &&
						 (mutation == 1 ? remaining.order == 0 : remaining.order != 0 && remaining.firstPhrase == 3),
					 "actual RNG reset or peer team change rejects the obsolete conversion receipt") &&
				ok;
			continue;
		}
		globals.time += .375f;
		behavior->updateOutgoingPublicRadio(0);
		behavior->updateOutgoingPublicRadio(1);
		ok = check(queueSamples == std::vector<std::string>{"radio/test/escort.wav"},
				 "converted production plan waits until the half-second start boundary") &&
			ok;
		globals.time += .125f;
		behavior->updateOutgoingPublicRadio(1);
		ok = check(queueSamples == std::vector<std::string>{"radio/test/escort.wav", "radio/test/agree.wav"},
				 "actual second-team-selection maintenance plays AgreeWithPlan instead of repeating the plan") &&
			ok;
	}
	return ok;
}
bool testStatementQueue(astrabot::metamod::PluginRuntime &runtime, enginefuncs_t &engine, globalvars_t &globals)
{
	using namespace astrabot;
	setupStatementQueue(engine, globals);
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
		auto &peer = gEntities[i];
		peer.free = false;
		peer.serialnumber = static_cast<int>(i + 10);
		peer.v.flags = FL_CLIENT | FL_ONGROUND;
		peer.v.health = 100;
		peer.v.deadflag = DEAD_NO;
		peer.v.team = i == 4 ? 1 : 2;
		peer.v.origin = Vector(i == 4 ? -120.0f : 120.0f, static_cast<float>(i * 4), 36);
	}
	enginefuncs_t hooks{};
	int version = ENGINE_INTERFACE_VERSION;
	if (!check(runtime.provideEngineFunctions(&hooks, &version), "registered queue receipt hooks"))
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
	if (!check(loadHearingNav(runtime), "real queue fixture NAV"))
	{
		return false;
	}
	const auto step = [&](float seconds)
	{
		globals.time += seconds;
		runtime.onStartFrame();
		runtime.onStartFramePost();
	};
	runtime.beginRoundGeneration();
	hooks.pfnMessageBegin(MSG_ONE, 94, nullptr, &self);
	hooks.pfnWriteByte(1);
	hooks.pfnWriteByte(22);
	hooks.pfnWriteByte(30);
	hooks.pfnMessageEnd();
	step(.125f);
	step(.125f);
	queueSamples.clear();
	hooks.pfnMessageBegin(MSG_ONE, 101, nullptr, &self);
	hooks.pfnWriteByte(5);
	hooks.pfnWriteString("%!MRAD_GO");
	hooks.pfnWriteShort(100);
	hooks.pfnMessageEnd();
	step(.125f);
	queueEnemiesVisible = true;
	step(2.0f);
	const bool acknowledgementControl = check(queueSamples == std::vector<std::string>{"radio/test/ack.wav"},
		"public human Go is acknowledged before the concurrently admitted Help");
	queueMode = "OFF";
	for (unsigned frame = 0; frame < 8; ++frame)
	{
		step(.125f);
	}
	if (!acknowledgementControl)
	{
		auto &ports = static_cast<metamod::BotBehaviorServices &>(runtime);
		const auto &order = ports.observation_scenarioRadio(0);
		const auto &decision = static_cast<metamod::BotNavigationServices &>(runtime).behavior_scenarioDecisions(0);
		std::fprintf(stderr,
			"queue diagnostic updates=%u team=%u order=%u orderTeam=%u human=%u action=%u stop=%f probes=%u\n",
			ports.service_managedBotFullUpdateSequences()[0], static_cast<unsigned>(ports.observation_teamInfoTeams(0)),
			order.sequence, static_cast<unsigned>(order.team), order.senderHuman ? 1U : 0U,
			static_cast<unsigned>(decision.action), getCvarFloat("bot_stop"), queueWaveProbes);
		for (const auto &path : queueLoads)
			std::fprintf(stderr, "loaded %s\n", path.c_str());
		std::fprintf(stderr, "queue samples:");
		for (const auto &sample : queueSamples)
		{
			std::fprintf(stderr, " %s", sample.c_str());
		}
		std::fprintf(stderr, "\n");
	}
	if (!(acknowledgementControl &&
			check(queueSamples == std::vector<std::string>{"radio/test/ack.wav", "radio/test/help.wav"},
				"both statements survive simultaneous admission, fixed count criteria and later OFF mode")))
		return false;
	return testQueueRadioReset(runtime, engine) && testPlanAgreementRuntime(runtime, globals);
}
}
