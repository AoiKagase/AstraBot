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
#include "astrabot/nav/nav_query.hpp"
#include "runtime/shared/public_nav_ladders.hpp"
#include "runtime/shared/public_native_ladder_probe.hpp"
#include <memory>
#include "fixtures/noise_runtime_nav.hpp"
namespace
{
bool queueEnemiesVisible = false;
bool queueHelpRecording = false;
bool queueLadderPhysics = false;
unsigned queueLadderClimbMoves = 0;
float queueLadderLowestPitch = 0;
void queueApplyLadderPhysics(edict_t *, const float *, float, float, unsigned char);
bool queueHelpResetAtRogue = false;
bool queueHelpMoveSenderAtRogue = false;
bool queueHelpForceCooperative = false;
globalvars_t *queueClock = nullptr;
decltype(enginefuncs_t::pfnRunPlayerMove) queueOriginalMove = nullptr;
std::vector<float> queueHelpMoves;
std::vector<std::pair<std::string, float>> queueHelpSamples;
void queueMove(edict_t *entity, const float *angles, float forward, float side, float up, unsigned short buttons,
	unsigned char impulse, unsigned char milliseconds)
{
	if (queueHelpRecording && entity == &gEntities[1])
		queueHelpMoves.push_back(std::sqrt(forward * forward + side * side));
	if (queueLadderPhysics)
		queueApplyLadderPhysics(entity, angles, forward, side, milliseconds);
	if (queueOriginalMove)
		queueOriginalMove(entity, angles, forward, side, up, buttons, impulse, milliseconds);
}
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
		text =
			"BaseDir radio/test/ Chatter Affirmative Radio EVENT_RADIO_AFFIRMATIVE ack.wav End "
			"Chatter Help Radio EVENT_RADIO_NEED_BACKUP help.wav End "
			"Chatter EscortingHostages escort.wav End Chatter AgreeWithPlan agree.wav End Chatter OnMyWay way.wav End "
			"Place BombsiteA site.wav End Chatter GoingToPlantBomb Place UNDEFINED plantplan_default.wav Place BombsiteA plantplan.wav End Chatter CoveringFriend coverfriend.wav End";
	}
	if (std::strcmp(name, "queue_speech.db") == 0)
	{
		text =
			"BaseDir radio/test/ Chatter Affirmative Place UNDEFINED Count 3 ack_wrong.wav Count Many ack.wav End "
			"Chatter Help Place UNDEFINED help.wav End Chatter OnMyWay Place UNDEFINED way.wav End "
			"Place BombsiteA site.wav End Chatter GoingToPlantBomb Place UNDEFINED plantplan_default.wav Place BombsiteA plantplan.wav End Chatter CoveringFriend coverfriend.wav End";
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
	if (std::strcmp(name, "DeathMsg") == 0)
		return 103;
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
		if (queueHelpRecording && queueClock)
			queueHelpSamples.emplace_back(sample, queueClock->time);
	}
}
void queueEnd()
{
}
void queueVisibility(const float *from, const float *end, int, edict_t *, TraceResult *trace)
{
	*trace = {};
	trace->vecEndPos = Vector(end[0], end[1], end[2]);
	trace->flFraction = queueEnemiesVisible ? 1.0f : 0.0f;
	if (queueHelpRecording && from[2] > 0 && end[2] < 0)
	{
		trace->flFraction = from[2] / (from[2] - end[2]);
		trace->vecEndPos.z = 0;
		trace->vecPlaneNormal = Vector(0, 0, 1);
	}
}
void setupStatementQueue(enginefuncs_t &engine, globalvars_t &globals)
{
	queueClock = &globals;
	if (engine.pfnRunPlayerMove != &queueMove)
		queueOriginalMove = engine.pfnRunPlayerMove;
	engine.pfnRunPlayerMove = &queueMove;
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
	if (queueHelpForceCooperative && low == 0 && high == 100)
	{
		queueHelpForceCooperative = false;
		return 100;
	}
	if (queueHelpMoveSenderAtRogue && low == 10 && high == 30)
	{
		for (const auto &sample : queueHelpSamples)
			if (sample.first == "radio/test/help.wav")
			{
				queueHelpMoveSenderAtRogue = false;
				queueHelpForceCooperative = true;
				gEntities[0].v.origin = Vector(4200, 140, 36);
				break;
			}
	}
	if (queueHelpResetAtRogue && low == 10 && high == 30)
	{
		bool heard = false;
		for (const auto &sample : queueHelpSamples)
			heard = heard || sample.first == "radio/test/help.wav";
		if (heard)
		{
			queueHelpResetAtRogue = false;
			astrabot::metamod::PluginRuntime::instance().beginRoundGeneration();
		}
	}
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

edict_t queueLadder{};
unsigned queueLadderFinds = 0, queueLadderTraces = 0;
bool queueLadderStale = false;
unsigned queueGroundMode = 0;
enginefuncs_t *queueLadderEngine = nullptr;
astrabot::metamod::PluginRuntime *queueLadderRuntime = nullptr;
unsigned queueLadderMutation = 0;
edict_t *queueFindLadder(edict_t *after, const char *, const char *value)
{
	++queueLadderFinds;
	return !after && std::strcmp(value, "func_ladder") == 0 ? &queueLadder : nullptr;
}
int queueLadderIndex(const edict_t *entity)
{
	if (entity == &queueLadder && queueLadderMutation == 6)
	{
		++queueLadder.serialnumber;
		queueLadderMutation = 0;
	}
	return entity == &queueLadder ? 6 : 0;
}
void queueLadderTrace(const float *, const float *to, int flags, edict_t *ignored, TraceResult *result)
{
	++queueLadderTraces;
	*result = {};
	result->flFraction = 1;
	result->vecEndPos = Vector(to[0], to[1], to[2]);
	if (flags == 1 && ignored == nullptr)
	{
		result->vecEndPos.z = 0;
		result->flFraction = queueGroundMode == 1 ? 2.0f : .5f;
		result->fStartSolid = queueGroundMode == 2;
		if (queueGroundMode == 3 && queueLadderEngine)
		{
			result->pHit = &queueLadder;
			queueLadderEngine->pfnSzFromIndex = nullptr;
		}
	}
	if (flags == 1 && ignored == &queueLadder)
	{
		if (queueLadderMutation == 1)
			++queueLadder.serialnumber;
		if (queueLadderMutation == 2)
			queueLadder.v.absmax.z += 1;
		if (queueLadderMutation == 3)
			queueLadderStale = true;
		if (queueLadderMutation == 4 && queueLadderEngine)
			queueLadderEngine->pfnTraceLine = nullptr;
		if (queueLadderMutation == 5 && queueLadderRuntime)
			queueLadderRuntime->beginRoundGeneration();
		queueLadderMutation = 0;
	}
}
bool testPublicNavLadders(astrabot::metamod::PluginRuntime &runtime, enginefuncs_t &engine)
{
	using namespace astrabot;
	const auto find = engine.pfnFindEntityByString;
	const auto index = engine.pfnIndexOfEdict;
	const auto name = engine.pfnSzFromIndex;
	const auto trace = engine.pfnTraceLine;
	engine.pfnFindEntityByString = queueFindLadder;
	engine.pfnIndexOfEdict = queueLadderIndex;
	engine.pfnTraceLine = queueLadderTrace;
	struct Restore
	{
		enginefuncs_t &engine;
		decltype(engine.pfnFindEntityByString) savedFind;
		decltype(engine.pfnTraceLine) savedTrace;
		decltype(engine.pfnIndexOfEdict) savedIndex;
		~Restore()
		{
			engine.pfnFindEntityByString = savedFind;
			engine.pfnTraceLine = savedTrace;
			engine.pfnIndexOfEdict = savedIndex;
		}
	} restore{engine, find, trace, index};
	queueLadder = {};
	queueLadder.serialnumber = 99;
	queueLadderEngine = &engine;
	queueLadderRuntime = &runtime;
	queueLadder.v.absmin = Vector(-20, -2, 0);
	queueLadder.v.absmax = Vector(20, 2, 100);
	auto &ports = static_cast<metamod::BotBehaviorServices &>(runtime);
	nav::NavLadderTopology topology;
	bool ok = check(ports.navigation_ladderTopology(&topology) && topology.ladders.size() == 1 &&
			topology.navRevision == runtime.navigationSnapshot().revision() && topology.ladders[0].bottomArea == 1 &&
			topology.ladders[0].entityIndex == 6 && topology.ladders[0].entityGeneration == 100,
		"real runtime service reconstructs public func_ladder topology against current NAV");
	nav::NavDocument document;
	document.setSourceIdentity({5, 100, 200});
	nav::NavArea bottom{}, top{};
	bottom.id = 10;
	bottom.extent = {{-100, -100, 0}, {100, 100, 0}};
	top.id = 20;
	top.extent = {{-20, -70, 100}, {20, 0, 100}};
	top.northEastZ = top.southWestZ = 100;
	document.addArea(bottom);
	document.addArea(top);
	nav::NavSnapshotPublisher publisher;
	publisher.publish(&document, 4);
	metamod::PublicNavLadderCache cache;
	queueLadderStale = false;
	const auto current = []
	{
		return !queueLadderStale;
	};
	ok = check(cache.read(&engine, publisher.snapshot(), current, &topology) && topology.ladders.size() == 1 &&
				 topology.ladders[0].topAreas[0] == 20,
			 "public adapter reconstructs actual upward connectivity") &&
		ok;
	float distance = -1;
	nav::NavQuery query(publisher.snapshot());
	ok = check(query.travelDistance(10, 20, nav::NavRouteType::Safest, topology, &distance) ==
					 nav::NavQueryResult::Found &&
				 std::fabs(distance - std::sqrt(10000.0f + 1225.0f)) < .001f,
			 "public adapter topology feeds the real native range query") &&
		ok;
	const auto finds = queueLadderFinds, traces = queueLadderTraces;
	ok = check(cache.read(&engine, publisher.snapshot(), current, &topology) && queueLadderFinds == finds &&
				 queueLadderTraces == traces,
			 "accepted map NAV topology reuses cache without per-bot public scans") &&
		ok;
	for (unsigned mutation : {1U, 2U, 3U, 4U, 6U})
	{
		metamod::PublicNavLadderCache interrupted;
		queueLadderMutation = mutation;
		queueLadderStale = false;
		ok = check(!interrupted.read(&engine, publisher.snapshot(), current, &topology) && topology.navRevision == 0 &&
					 topology.ladders.empty(),
				 "callback entity serial, bounds or frame replacement rejects the partial graph") &&
			ok;
		queueLadderStale = false;
		engine.pfnTraceLine = queueLadderTrace;
		ok = check(interrupted.read(&engine, publisher.snapshot(), current, &topology) && topology.ladders.size() == 1,
				 "failed build does not cache empty or stale topology") &&
			ok;
	}
	engine.pfnFindEntityByString = nullptr;
	ok = check(!cache.read(&engine, publisher.snapshot(), current, &topology) && topology.navRevision == 0,
			 "engine enumeration replacement cannot reuse an accepted graph") &&
		ok;
	engine.pfnFindEntityByString = queueFindLadder;
	publisher.publish(&document, 5);
	const auto previousFinds = queueLadderFinds;
	ok = check(cache.read(&engine, publisher.snapshot(), current, &topology) && topology.mapGeneration == 5 &&
				 queueLadderFinds > previousFinds,
			 "new NAV publication invalidates map graph cache") &&
		ok;
	ok = check(loadHearingNav(runtime), "reload actual NAV to require a new public ladder snapshot") && ok;
	queueLadderMutation = 5;
	ok = check(!ports.navigation_ladderTopology(&topology) && topology.navRevision == 0,
			 "actual round reset during trace invalidates runtime provider publication") &&
		ok;
	ok = check(ports.navigation_ladderTopology(&topology) && topology.ladders.size() == 1,
			 "runtime provider can rebuild for the new round after stale callback cancellation") &&
		ok;

	// Bottom is outside the containing rectangles, so native nearest lookup needs ground probes.
	nav::NavDocument fallbackDoc;
	fallbackDoc.setSourceIdentity({5, 100, 200});
	auto nearFloor = bottom;
	nearFloor.extent = {{110, -100, 0}, {190, 100, 0}};
	auto highExit = top;
	highExit.extent = {{180, -70, 100}, {220, 0, 100}};
	fallbackDoc.addArea(nearFloor);
	fallbackDoc.addArea(highExit);
	nav::NavSnapshotPublisher fallbackPublisher;
	fallbackPublisher.publish(&fallbackDoc, 6);
	queueLadder.v.absmin.x = 180;
	queueLadder.v.absmax.x = 220;
	for (unsigned badGround : {1U, 3U})
	{
		metamod::PublicNavLadderCache partial;
		queueGroundMode = badGround;
		ok = check(!partial.read(&engine, fallbackPublisher.snapshot(), current, &topology) &&
					 topology.navRevision == 0 && topology.ladders.empty(),
				 "invalid ground fallback must reject the whole public ladder snapshot") &&
			ok;
		queueGroundMode = 0;
		engine.pfnSzFromIndex = name;
		ok = check(partial.read(&engine, fallbackPublisher.snapshot(), current, &topology) &&
					 topology.ladders.size() == 1 && topology.ladders[0].bottomArea == 10 &&
					 topology.ladders[0].topAreas[0] == 20,
				 "valid ground retry rebuilds connectivity instead of reusing a poisoned empty bottom") &&
			ok;
	}
	metamod::PublicNavLadderCache noFloor;
	queueGroundMode = 2;
	ok = check(noFloor.read(&engine, fallbackPublisher.snapshot(), current, &topology) &&
				 topology.ladders.size() == 1 && topology.ladders[0].bottomArea == 0,
			 "completed start-solid ground probes legitimately retain an unconnected ladder bottom") &&
		ok;
	const auto noFloorTraces = queueLadderTraces;
	queueGroundMode = 0;
	ok =
		check(noFloor.read(&engine, fallbackPublisher.snapshot(), current, &topology) && topology.ladders.size() == 1 &&
				topology.ladders[0].bottomArea == 0 && queueLadderTraces == noFloorTraces,
			"legitimate no-ground result remains an accepted per-map cache entry") &&
		ok;

	queueLadderRuntime = nullptr;
	queueLadderEngine = nullptr;
	return ok;
}

bool queueNativeReceipt = true;
unsigned queueNativeMutation = 0, queueNativeTraces = 0;
bool queueNativeIgnored = false;
float queueNativeFraction = .25f;
decltype(enginefuncs_t::pfnSzFromIndex) queueNativeOriginalString = nullptr;
const char *queueNativeString(int index)
{
	return index == 600 ? "func_ladder" : queueNativeOriginalString ? queueNativeOriginalString(index) : "";
}
edict_t *queueNativeEntity(int index)
{
	return index == 6 ? &queueLadder : entityOfIndex(index);
}
void queueNativeTrace(const float *, const float *to, int flags, edict_t *ignored, TraceResult *result)
{
	++queueNativeTraces;
	queueNativeIgnored = ignored == &queueLadder && flags == 1;
	*result = {};
	result->flFraction = queueNativeFraction;
	result->vecEndPos = Vector(to[0], to[1], to[2]);
	if (queueNativeMutation == 1)
		++queueLadder.serialnumber;
	if (queueNativeMutation == 2)
		queueNativeReceipt = false;
	if (queueNativeMutation == 3)
		queueLadderEngine->pfnTraceLine = &queueVisibility;
}
bool testPublicNativeLadderTrace(enginefuncs_t &engine)
{
	using namespace astrabot;
	struct Restore
	{
		enginefuncs_t &engine;
		enginefuncs_t saved;
		~Restore()
		{
			engine = saved;
			queueNativeOriginalString = nullptr;
			queueNativeMutation = 0;
			queueNativeReceipt = true;
		}
	} restore{engine, engine};
	queueNativeOriginalString = engine.pfnSzFromIndex;
	engine.pfnSzFromIndex = &queueNativeString;
	engine.pfnPEntityOfEntIndex = &queueNativeEntity;
	engine.pfnIndexOfEdict = &queueLadderIndex;
	engine.pfnTraceLine = &queueNativeTrace;
	queueLadderEngine = &engine;
	queueLadderMutation = 0;
	queueLadder = {};
	queueLadder.serialnumber = 99;
	queueLadder.v.classname = 600;
	queueLadder.v.absmin = Vector(-20, -2, 0);
	queueLadder.v.absmax = Vector(20, 2, 900);
	nav::NavLadder ladder;
	ladder.entityIndex = 6;
	ladder.entityGeneration = 100;
	const auto current = []
	{
		return queueNativeReceipt;
	};
	bool ok = true;
	queueNativeMutation = 0;
	queueNativeReceipt = true;
	queueNativeTraces = 0;
	queueNativeFraction = .25f;
	metamod::PublicNativeLadderProbe valid(&engine, current);
	float fraction = -1;
	ok =
		check(valid.trace(ladder, {0, 0, 36}, {0, 0, 900}, &fraction) && fraction == .25f && queueNativeTraces == 1 &&
				queueNativeIgnored,
			"public native ladder endpoint trace ignores the exact serial-qualified func_ladder and retains fraction") &&
		ok;
	for (unsigned mutation : {1U, 2U, 3U})
	{
		engine.pfnTraceLine = &queueNativeTrace;
		queueLadder.serialnumber = 99;
		queueNativeReceipt = true;
		queueNativeMutation = mutation;
		metamod::PublicNativeLadderProbe probe(&engine, current);
		fraction = -1;
		ok = check(!probe.trace(ladder, {0, 0, 36}, {0, 0, 900}, &fraction) && fraction == 0 && !probe.current(),
				 "serial, owner or engine hook change during native ladder trace cannot publish stale fraction") &&
			ok;
	}
	queueNativeMutation = 0;
	queueNativeReceipt = true;
	engine.pfnTraceLine = &queueNativeTrace;
	queueLadder.serialnumber = 100;
	metamod::PublicNativeLadderProbe stale(&engine, current);
	fraction = -1;
	const auto before = queueNativeTraces;
	ok = check(!stale.trace(ladder, {0, 0, 36}, {0, 0, 900}, &fraction) && fraction == 0 && queueNativeTraces == before,
			 "reused ladder edict is rejected before any trace for its old source generation") &&
		ok;
	queueLadder.serialnumber = 99;
	queueNativeFraction = 1.01f;
	metamod::PublicNativeLadderProbe invalid(&engine, current);
	fraction = -1;
	ok = check(!invalid.trace(ladder, {0, 0, 36}, {0, 0, 900}, &fraction) && fraction == 0,
			 "invalid public trace fraction is unavailable instead of a ladder endpoint") &&
		ok;
	queueNativeFraction = .25f;
	return ok;
}

bool testNativeHelpDeathMemory(astrabot::metamod::PluginRuntime &runtime, enginefuncs_t &engine, globalvars_t &globals)
{
	using namespace astrabot;
	queueEnemiesVisible = false;
	queueMode = "off";
	runtime.setCompatibilityString("astrabot_mode", "compatibility");
	setCvarFloat("bot_stop", 0);
	setCvarFloat("bot_allow_rogues", 0);
	runtime.setCompatibilityFloat("bot_quota", 2);
	for (unsigned i = 0; i < 2; ++i)
	{
		auto &entity = gEntities[i];
		entity.free = false;
		entity.v.flags = FL_CLIENT | FL_FAKECLIENT | FL_ONGROUND;
		entity.v.health = 100;
		entity.v.deadflag = DEAD_NO;
		entity.v.solid = SOLID_SLIDEBOX;
		entity.v.movetype = MOVETYPE_WALK;
		entity.v.view_ofs = Vector(0, 0, 17);
		entity.v.origin = Vector(static_cast<float>(i * 128), 0, 36);
		entity.v.velocity = Vector(0, 0, 0);
		entity.v.team = 1;
		runtime.onMessageBegin(MSG_ALL, 86, nullptr, nullptr);
		runtime.onWriteByte(static_cast<int>(i + 1));
		runtime.onWriteString("TERRORIST");
		runtime.onMessageEnd();
	}
	if (!check(loadHearingNav(runtime), "native death-memory fixture loads actual NAV"))
		return false;
	runtime.beginRoundGeneration();
	globals.time += 10;
	auto &ports = static_cast<metamod::BotBehaviorServices &>(runtime);
	auto &navigation = static_cast<metamod::BotNavigationServices &>(runtime);
	const bool peerInitializedBefore = navigation.behavior_managedBotStateMachines(1).isInitialized();
	const auto peerStateBefore = navigation.behavior_managedBotStateMachines(1).state();
	const auto peerFullBefore = ports.service_managedBotFullUpdateSequences()[1];
	bool observedNewPeer = false;
	bool peerEnteredBuy = false;
	compat::CompatibilityStateId firstPeerState = compat::CompatibilityStateId::None;
	for (unsigned i = 0; i < 16; ++i)
	{
		globals.time += .125f;
		runtime.onStartFrame();
		runtime.onStartFramePost();
		const auto &peer = navigation.behavior_managedBotStateMachines(1);
		if (!observedNewPeer && peer.isInitialized())
		{
			observedNewPeer = true;
			firstPeerState = peer.state();
			peerEnteredBuy = peer.state() == compat::CompatibilityStateId::Buy;
		}
	}
	const auto handle = ports.service_managedBotHandles()[0];
	const world::ActorKey actor{handle.actor.slot, handle.actor.actorGeneration};
	const auto frame = [&]
	{
		return world::FrameIdentity{ports.service_lifecycle().mapGeneration(),
			ports.service_lifecycle().roundGeneration(), ports.service_adapterFrameCount()};
	};
	const auto revision = runtime.navigationSnapshot().revision();
	const auto beforeArea = ports.navigation_knownArea(0, actor, frame(), revision);
	const auto &machine = navigation.behavior_managedBotStateMachines(0);
	if (!check(machine.isInitialized() && beforeArea == 1,
			"real full updates establish owned state and last known area before death"))
	{
		std::fprintf(stderr, "death fixture state=%u area=%u full=%u stop=%f\n", static_cast<unsigned>(machine.state()),
			beforeArea, ports.service_managedBotFullUpdateSequences()[0], getCvarFloat("bot_stop"));
		return false;
	}
	const auto state = machine.state();
	const auto task = machine.task();
	const auto epoch = machine.normalStateEpoch();
	const auto attacking = machine.attackOverlayActive();
	enginefuncs_t hooks{};
	int version = ENGINE_INTERFACE_VERSION;
	if (!check(runtime.provideEngineFunctions(&hooks, &version), "actual registered death hooks available"))
		return false;
	gEntities[0].v.health = 0;
	gEntities[0].v.deadflag = DEAD_DEAD;
	hooks.pfnMessageBegin(MSG_ALL, 103, nullptr, nullptr);
	hooks.pfnWriteByte(3);
	hooks.pfnWriteByte(1);
	hooks.pfnWriteByte(0);
	hooks.pfnWriteString("ak47");
	hooks.pfnMessageEnd();
	bool ok = check(observedNewPeer && peerEnteredBuy, "native spawned recipient enters Buy before it can answer Help");
	if (!ok)
		std::fprintf(stderr, "peer initBefore=%d stateBefore=%s firstAfter=%s fullBefore=%u fullAfter=%u mode=%u\n",
			peerInitializedBefore ? 1 : 0, compat::toString(peerStateBefore), compat::toString(firstPeerState),
			peerFullBefore, ports.service_managedBotFullUpdateSequences()[1],
			static_cast<unsigned>(ports.service_compatibilitySurface().configuration().mode));
	if (!ok)
		for (unsigned slot = 0; slot < 5; ++slot)
			std::fprintf(stderr, "slot=%u managed=%d handleSlot=%u full=%u flags=%d free=%d quotaEngine=%f\n", slot + 1,
				ports.service_managedBotSlots()[slot] ? 1 : 0, ports.service_managedBotHandles()[slot].actor.slot,
				ports.service_managedBotFullUpdateSequences()[slot], gEntities[slot].v.flags, gEntities[slot].free,
				getCvarFloat("bot_quota"));
	ok = check(ports.navigation_knownArea(0, actor, frame(), revision) == beforeArea,
			 "CSBot death receipt retains last known NAV area for dead Help recipients") &&
		ok;
	globals.time += .125f;
	runtime.onStartFrame();
	runtime.onStartFramePost();
	ok = check(machine.state() == state && machine.task() == task && machine.normalStateEpoch() == epoch &&
				 machine.attackOverlayActive() == attacking,
			 "native dead update retains task, normal state and attack overlay for exact Help busy checks") &&
		ok;
	ok = check(ports.navigation_knownArea(0, actor, frame(), revision) == beforeArea,
			 "native dead upkeep does not erase or replace remembered NAV area") &&
		ok;
	gEntities[0].v.health = 100;
	gEntities[0].v.deadflag = DEAD_NO;
	runtime.beginRoundGeneration();
	(void)engine;
	return ok;
}

bool loadHelpRangeNav(astrabot::metamod::PluginRuntime &runtime)
{
	const char *path = "astrabot_help_range_runtime.nav";
	ExistingProfileGuard preserve;
	if (!preserve.capture(path))
		return false;
	std::vector<std::uint8_t> bytes;
	const auto u32 = [&](std::uint32_t value)
	{
		for (unsigned shift = 0; shift < 32; shift += 8)
			bytes.push_back(static_cast<std::uint8_t>(value >> shift));
	};
	const auto f32 = [&](float value)
	{
		std::uint32_t raw = 0;
		std::memcpy(&raw, &value, sizeof(raw));
		u32(raw);
	};
	u32(0xFEEDFACEU);
	u32(1);
	u32(2);
	for (unsigned id = 1; id <= 2; ++id)
	{
		u32(id);
		bytes.push_back(0);
		for (float value :
			{id == 1 ? -2000.0f : 4000.0f, -128.0f, 0.0f, id == 1 ? 1000.0f : 4500.0f, 512.0f, 0.0f, 0.0f, 0.0f})
			f32(value);
		for (unsigned direction = 0; direction < 4; ++direction)
			u32(0);
		bytes.push_back(0);
		bytes.push_back(0);
		u32(0);
	}
	auto *file = openFixture(path, "wb");
	if (!file)
		return false;
	const bool written = std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();
	const bool closed = std::fclose(file) == 0;
	const astrabot::metamod::NavLoadRequest request{
		path, "astrabot_help_range_runtime", runtime.snapshot().mapGeneration, false, 0, false, 0};
	const bool loaded =
		written && closed && runtime.loadNavigationFile(&request) == astrabot::metamod::NavLoadResult::Loaded;
	std::remove(path);
	return loaded;
}

bool testActualHelpMovement(astrabot::metamod::PluginRuntime &runtime, enginefuncs_t &, globalvars_t &globals,
	bool deadReceiver = false, bool resetDuringRogue = false, bool ctBomb = false, bool senderRangeMove = false,
	bool attackRetained = false)
{
	using namespace astrabot;
	struct Restore
	{
		globalvars_t &globals;
		string_t map;
		~Restore()
		{
			globals.mapname = map;
			queueHelpRecording = false;
			queueEnemiesVisible = false;
			queueMode = "off";
			queueHelpResetAtRogue = false;
			queueHelpMoveSenderAtRogue = false;
		}
	} restore{globals, globals.mapname};
	if (ctBomb)
		globals.mapname = 6;
	queueMode = "normal";
	queueEnemiesVisible = false;
	queueHelpRecording = true;
	setCvarFloat("mp_buytime", 0);
	setCvarFloat("bot_stop", 0);
	setCvarFloat("bot_allow_rogues", 0);
	gObjectiveEntitiesAvailable = false;
	for (unsigned i = 0; i < 5; ++i)
	{
		auto &entity = gEntities[i];
		entity.free = false;
		entity.v.flags = FL_CLIENT | FL_ONGROUND | (i < 2 ? FL_FAKECLIENT : 0);
		entity.v.health = 100;
		entity.v.deadflag = DEAD_NO;
		entity.v.solid = SOLID_SLIDEBOX;
		entity.v.movetype = MOVETYPE_WALK;
		entity.v.view_ofs = Vector(0, 0, 17);
		entity.v.v_angle = Vector(0, i == 1 ? 180.0f : 0.0f, 0);
		entity.v.angles = entity.v.v_angle;
		entity.v.fov = 90;
		entity.v.maxspeed = 250;
		entity.v.velocity = Vector(0, 0, 0);
		entity.v.team = i == 2 || i == 3 ? (ctBomb ? 1 : 2) : (ctBomb ? 2 : 1);
		entity.v.origin = Vector(i == 0 ? 320.0f
				: i == 1				? -1560.0f
				: i == 4				? -15000.0f
										: 500.0f + 40.0f * i,
			140, 36);
		runtime.onMessageBegin(MSG_ALL, 86, nullptr, nullptr);
		runtime.onWriteByte(static_cast<int>(i + 1));
		runtime.onWriteString(entity.v.team == 1 ? "TERRORIST" : "CT");
		runtime.onMessageEnd();
	}
	if (!check(senderRangeMove ? loadHelpRangeNav(runtime) : loadHearingNav(runtime),
			"actual Help movement has a real NAV snapshot"))
		return false;
	runtime.beginRoundGeneration();
	globals.time += senderRangeMove ? 40 : 10;
	const auto step = [&]
	{
		globals.time += .125f;
		runtime.onStartFrame();
		runtime.onStartFramePost();
	};
	for (unsigned i = 0; i < 32; ++i)
		step();
	auto &ports = static_cast<metamod::BotBehaviorServices &>(runtime);
	auto &navigation = static_cast<metamod::BotNavigationServices &>(runtime);
	const auto &receiver = navigation.behavior_managedBotStateMachines(1);
	if (!check(receiver.isInitialized() && receiver.state() != compat::CompatibilityStateId::Buy &&
				ports.service_managedBotFullUpdateSequences()[1] > 0,
			"real receiver joined and completed buying before Help"))
		return false;
	const auto oldTask = receiver.task();
	if (deadReceiver)
	{
		gEntities[1].v.health = 0;
		gEntities[1].v.deadflag = DEAD_DEAD;
		enginefuncs_t hooks{};
		int version = ENGINE_INTERFACE_VERSION;
		if (!runtime.provideEngineFunctions(&hooks, &version))
			return false;
		hooks.pfnMessageBegin(MSG_ALL, 103, nullptr, nullptr);
		hooks.pfnWriteByte(3);
		hooks.pfnWriteByte(2);
		hooks.pfnWriteByte(0);
		hooks.pfnWriteString("ak47");
		hooks.pfnMessageEnd();
		step();
	}
	if (senderRangeMove)
	{
		setCvarFloat("bot_allow_rogues", 1);
		queueHelpMoveSenderAtRogue = true;
	}
	if (resetDuringRogue)
	{
		setCvarFloat("bot_allow_rogues", 1);
		queueHelpResetAtRogue = true;
	}
	queueSamples.clear();
	queueHelpSamples.clear();
	queueHelpMoves.clear();
	queueEnemiesVisible = true;
	bool moved = false;
	for (unsigned i = 0; i < 32; ++i)
	{
		step();
		const auto &goal = navigation.behavior_managedBotObjectiveTargets(1);
		if (receiver.state() == compat::CompatibilityStateId::MoveTo && goal.valid &&
			std::fabs(goal.target.x - 320) < .01f && std::fabs(goal.target.y - 140) < .01f)
		{
			moved = true;
			queueHelpMoves.clear();
			break;
		}
	}
	const bool rejectedMove = resetDuringRogue || senderRangeMove;
	bool ok = check(senderRangeMove ? (!moved && !queueHelpMoveSenderAtRogue && !queueHelpForceCooperative &&
										  !navigation.behavior_managedBotObjectiveTargets(1).valid)
			: resetDuringRogue		? (!moved && !queueHelpResetAtRogue)
									: (moved && receiver.task() == oldTask),
		"natural Help completion reaches the actual receiver MoveTo and copied sender goal while preserving task");
	float helpAt = -1, wayAt = -1;
	for (const auto &sample : queueHelpSamples)
	{
		if (sample.first == "radio/test/help.wav" && helpAt < 0)
			helpAt = sample.second;
		if (sample.first == "radio/test/way.wav" && wayAt < 0)
			wayAt = sample.second;
	}
	ok = check(helpAt >= 0, "actual sender emitted Help through the normal production playback path") && ok;
	if (moved)
	{
		gEntities[0].v.origin = Vector(640, 340, 36);
		for (unsigned i = 0; i < 3; ++i)
			step();
		const auto &goal = navigation.behavior_managedBotObjectiveTargets(1);
		ok = check(goal.valid && std::fabs(goal.target.x - 320) < .01f && std::fabs(goal.target.y - 140) < .01f,
				 "actual assistance target remains copied after sender movement") &&
			ok;
		for (const auto &sample : queueHelpSamples)
			if (sample.first == "radio/test/way.wav" && wayAt < 0)
				wayAt = sample.second;
	}
	ok = check((deadReceiver || resetDuringRogue) ? wayAt < 0 : (wayAt >= helpAt && helpAt >= 0 && wayAt - helpAt < 1),
			 "actual OnMyWay starts immediately after Help instead of waiting for human-order reaction delay") &&
		ok;
	bool movingCommand = false;
	for (float value : queueHelpMoves)
		movingCommand = movingCommand || value > 1;
	ok = check(rejectedMove ? (!moved && !movingCommand) : (moved && (deadReceiver ? !movingCommand : movingCommand)),
			 "actual assistance dispatch moves living recipients and leaves dead recipients motionless") &&
		ok;
	if (ctBomb && moved)
	{
		if (attackRetained)
		{
			gEntities[1].v.weapons = 1 << 22;
			runtime.onMessageBegin(MSG_ONE, 94, nullptr, &gEntities[1]);
			runtime.onWriteByte(1);
			runtime.onWriteByte(22);
			runtime.onWriteByte(30);
			runtime.onMessageEnd();
			gEntities[1].v.v_angle = Vector(0, 0, 0);
			gEntities[1].v.angles = gEntities[1].v.v_angle;
			for (unsigned i = 0; i < 24 && !receiver.attackOverlayActive(); ++i)
				step();
			if (!check(receiver.attackOverlayActive() && receiver.state() == compat::CompatibilityStateId::MoveTo &&
						navigation.behavior_managedBotObjectiveTargets(1).valid,
					"actual Help recipient begins attack while retaining its suspended MoveTo goal"))
			{
				std::fprintf(stderr, "attack setup state=%s task=%s overlay=%d goal=%d weapon=%d health=%f yaw=%f\n",
					compat::toString(receiver.state()), compat::toString(receiver.task()),
					receiver.attackOverlayActive() ? 1 : 0,
					navigation.behavior_managedBotObjectiveTargets(1).valid ? 1 : 0,
					static_cast<int>(ports.observation_managedBotCurrentWeapons(1).weaponId), gEntities[1].v.health,
					gEntities[1].v.v_angle.y);
				return false;
			}
		}
		runtime.onMessageBegin(MSG_ALL, 98, nullptr, nullptr);
		runtime.onWriteByte(4);
		runtime.onWriteString("#Bomb_Planted");
		runtime.onMessageEnd();
		step();
		if (attackRetained)
		{
			ok = check(receiver.attackOverlayActive() && receiver.state() == compat::CompatibilityStateId::MoveTo &&
						 navigation.behavior_managedBotObjectiveTargets(1).valid,
					 "planted-bomb notice does not cancel combat or update the suspended Help MoveTo task") &&
				ok;
			for (unsigned enemy = 2; enemy <= 3; ++enemy)
			{
				gEntities[enemy].v.health = 0;
				gEntities[enemy].v.deadflag = DEAD_DEAD;
				runtime.onMessageBegin(MSG_ALL, 103, nullptr, nullptr);
				runtime.onWriteByte(2);
				runtime.onWriteByte(static_cast<int>(enemy + 1));
				runtime.onWriteByte(0);
				runtime.onWriteString("ak47");
				runtime.onMessageEnd();
			}
			for (unsigned i = 0; i < 8 && receiver.attackOverlayActive(); ++i)
				step();
			ok = check(!receiver.attackOverlayActive() && !navigation.behavior_managedBotObjectiveTargets(1).valid,
					 "retained CT MoveTo task observes planted bomb after actual enemy death ends attack") &&
				ok;
		}
		else
			ok = check(receiver.state() == compat::CompatibilityStateId::Idle &&
						 !navigation.behavior_managedBotObjectiveTargets(1).valid,
					 "actual CT planted-bomb notice abandons a preserved ordinary assistance task before navigation") &&
				ok;
	}
	if (deadReceiver && moved)
	{
		gEntities[1].v.health = 100;
		gEntities[1].v.deadflag = DEAD_NO;
		step();
		ok = check(receiver.state() == compat::CompatibilityStateId::Buy &&
					 !navigation.behavior_managedBotObjectiveTargets(1).valid,
				 "native respawn enters Buy and invalidates assistance accepted during death") &&
			ok;
	}
	if (!ok)
	{
		const auto &goal = navigation.behavior_managedBotObjectiveTargets(1);
		std::fprintf(stderr, "help runtime receiver=%s task=%s goal=%d (%f,%f,%f) samples=%zu moves=%zu\n",
			compat::toString(receiver.state()), compat::toString(receiver.task()), goal.valid ? 1 : 0, goal.target.x,
			goal.target.y, goal.target.z, queueHelpSamples.size(), queueHelpMoves.size());
		world::WorldSnapshot belief;
		metamod::ThreatObservation threats{};
		const bool observed = ports.observation_collectManagedPerception(0, &belief, &threats);
		std::fprintf(stderr,
			"sender observed=%d available=%d threats=%zu updates=%u origin=(%f,%f) view=(%f,%f) team=%d\n",
			observed ? 1 : 0, threats.observerAvailable ? 1 : 0, threats.count,
			ports.service_managedBotFullUpdateSequences()[0], gEntities[0].v.origin.x, gEntities[0].v.origin.y,
			gEntities[0].v.v_angle.x, gEntities[0].v.v_angle.y, gEntities[0].v.team);
		for (const auto &sample : queueHelpSamples)
			std::fprintf(stderr, "help sample %s at %f\n", sample.first.c_str(), sample.second);
	}
	return ok;
}

void queueApplyLadderPhysics(
	edict_t *entity, const float *angles, float forward, float side, unsigned char milliseconds)
{
	if (entity != &gEntities[1] || entity->v.deadflag != DEAD_NO || entity->v.health <= 0)
		return;
	queueLadderLowestPitch = (std::min)(queueLadderLowestPitch, angles[0]);
	const float yaw = angles[1] * .017453292519943295f, dt = milliseconds * .001f;
	float x = forward * std::cos(yaw) + side * std::sin(yaw), y = forward * std::sin(yaw) - side * std::cos(yaw);
	const float magnitude = std::hypot(x, y), limit = entity->v.maxspeed;
	if (magnitude > limit && limit > 0)
	{
		x *= limit / magnitude;
		y *= limit / magnitude;
	}
	if (entity->v.movetype == MOVETYPE_FLY)
	{
		const float climb = (std::max)(0.0f, -std::sin(angles[0] * .017453292519943295f) * forward);
		if (climb > 1)
		{
			++queueLadderClimbMoves;
			entity->v.origin.z = (std::min)(936.0f, entity->v.origin.z + climb * dt);
		}
		entity->v.velocity = Vector(0, 0, climb);
		if (entity->v.origin.z >= 936)
		{
			entity->v.origin.x += x * dt;
			entity->v.origin.y += y * dt;
			if (entity->v.origin.y <= 0)
			{
				entity->v.movetype = MOVETYPE_WALK;
				entity->v.flags |= FL_ONGROUND;
			}
		}
	}
	else
	{
		entity->v.origin.x += x * dt;
		entity->v.origin.y += y * dt;
		entity->v.velocity = Vector(x, y, 0);
		if (entity->v.origin.z < 936 && std::fabs(entity->v.origin.x) <= 20 && entity->v.origin.y <= 18 &&
			entity->v.origin.y >= -18 && y < 0)
		{
			entity->v.movetype = MOVETYPE_FLY;
			entity->v.flags &= ~FL_ONGROUND;
		}
	}
}
void queueHelpLadderTrace(const float *from, const float *to, int, edict_t *ignored, TraceResult *trace)
{
	*trace = {};
	trace->flFraction = 1;
	trace->vecEndPos = Vector(to[0], to[1], to[2]);
	if (ignored == &queueLadder)
		return;
	if (from[2] > to[2] && std::fabs(from[0] - to[0]) < .001f && std::fabs(from[1] - to[1]) < .001f)
	{
		const float floor =
			from[2] >= 900 && from[0] >= -100 && from[0] <= 1024 && from[1] >= -128 && from[1] <= 0 ? 900.0f : 0.0f;
		if (to[2] <= floor && from[2] >= floor)
		{
			trace->flFraction = (from[2] - floor) / (from[2] - to[2]);
			trace->vecEndPos.z = floor;
			trace->vecPlaneNormal = Vector(0, 0, 1);
		}
		return;
	}
	if (!queueEnemiesVisible || (ignored == &gEntities[1] && to[0] >= 500 && to[2] > 850))
		trace->flFraction = 0;
}
bool loadActualHelpLadderNav(astrabot::metamod::PluginRuntime &runtime)
{
	const char *path = "astrabot_help_ladder_runtime.nav";
	ExistingProfileGuard preserve;
	if (!preserve.capture(path))
		return false;
	std::vector<std::uint8_t> bytes;
	const auto u32 = [&](std::uint32_t value)
	{
		for (unsigned shift = 0; shift < 32; shift += 8)
			bytes.push_back(static_cast<std::uint8_t>(value >> shift));
	};
	const auto f32 = [&](float value)
	{
		std::uint32_t raw = 0;
		std::memcpy(&raw, &value, sizeof(raw));
		u32(raw);
	};
	u32(0xFEEDFACEU);
	u32(1);
	u32(2);
	for (unsigned id = 1; id <= 2; ++id)
	{
		u32(id);
		bytes.push_back(0);
		const float z = id == 1 ? 0.0f : 900.0f;
		for (float value : {-100.0f, -128.0f, z, id == 1 ? 100.0f : 1024.0f, id == 1 ? 256.0f : 0.0f, z, z, z})
			f32(value);
		for (unsigned direction = 0; direction < 4; ++direction)
			u32(0);
		bytes.push_back(0);
		bytes.push_back(0);
		u32(0);
	}
	auto *file = openFixture(path, "wb");
	if (!file)
		return false;
	const bool written = std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();
	const bool closed = std::fclose(file) == 0;
	const astrabot::metamod::NavLoadRequest request{
		path, "astrabot_help_ladder_runtime", runtime.snapshot().mapGeneration, false, 0, false, 0};
	const bool loaded =
		written && closed && runtime.loadNavigationFile(&request) == astrabot::metamod::NavLoadResult::Loaded;
	std::remove(path);
	return loaded;
}
bool testActualLadderHelpMovement(
	astrabot::metamod::PluginRuntime &runtime, enginefuncs_t &engine, globalvars_t &globals)
{
	using namespace astrabot;
	struct Restore
	{
		enginefuncs_t &engine;
		enginefuncs_t saved;
		~Restore()
		{
			engine = saved;
			queueNativeOriginalString = nullptr;
			queueLadderPhysics = false;
			queueHelpRecording = false;
			queueEnemiesVisible = false;
			queueMode = "off";
		}
	} restore{engine, engine};
	queueNativeOriginalString = engine.pfnSzFromIndex;
	engine.pfnSzFromIndex = &queueNativeString;
	engine.pfnFindEntityByString = &queueFindLadder;
	engine.pfnPEntityOfEntIndex = &queueNativeEntity;
	engine.pfnIndexOfEdict = &queueLadderIndex;
	engine.pfnTraceLine = &queueHelpLadderTrace;
	queueLadderMutation = 0;
	queueLadderEngine = &engine;
	queueLadder = {};
	queueLadder.serialnumber = 99;
	queueLadder.v.classname = 600;
	queueLadder.v.solid = SOLID_BSP;
	queueLadder.v.absmin = Vector(-20, -2, 0);
	queueLadder.v.absmax = Vector(20, 2, 900);
	queueMode = "normal";
	queueEnemiesVisible = false;
	queueHelpRecording = true;
	queueLadderPhysics = false;
	setCvarFloat("mp_buytime", 0);
	setCvarFloat("bot_stop", 0);
	setCvarFloat("bot_allow_rogues", 0);
	gObjectiveEntitiesAvailable = false;
	for (unsigned i = 0; i < 5; ++i)
	{
		auto &entity = gEntities[i];
		entity.free = false;
		entity.v.flags = FL_CLIENT | FL_ONGROUND | (i < 2 ? FL_FAKECLIENT : 0);
		entity.v.health = 100;
		entity.v.deadflag = DEAD_NO;
		entity.v.solid = SOLID_SLIDEBOX;
		entity.v.movetype = MOVETYPE_WALK;
		entity.v.mins = Vector(-16, -16, -36);
		entity.v.maxs = Vector(16, 16, 36);
		entity.v.view_ofs = Vector(0, 0, 17);
		entity.v.origin = i == 0 ? Vector(320, -80, 936)
			: i == 1			 ? Vector(0, 100, 36)
			: i == 4			 ? Vector(-15000, 100, 36)
								 : Vector(500.0f + 40.0f * i, -80, 936);
		entity.v.v_angle = Vector(0, i == 1 ? -90.0f : 0.0f, 0);
		entity.v.angles = entity.v.v_angle;
		entity.v.velocity = Vector(0, 0, 0);
		entity.v.fov = 90;
		entity.v.maxspeed = 250;
		entity.v.team = i == 2 || i == 3 ? 2 : 1;
		runtime.onMessageBegin(MSG_ALL, 86, nullptr, nullptr);
		runtime.onWriteByte(static_cast<int>(i + 1));
		runtime.onWriteString(entity.v.team == 1 ? "TERRORIST" : "CT");
		runtime.onMessageEnd();
	}
	if (!check(loadActualHelpLadderNav(runtime),
			"actual Help ladder fixture loads disconnected lower and upper NAV floors"))
		return false;
	auto &behavior = static_cast<metamod::BotBehaviorServices &>(runtime);
	auto &navigation = static_cast<metamod::BotNavigationServices &>(runtime);
	nav::NavLadderTopology topology;
	if (!check(behavior.navigation_ladderTopology(&topology) && topology.ladders.size() == 1 &&
				topology.ladders[0].bottomArea == 1 && topology.ladders[0].topAreas[0] == 2 &&
				topology.ladders[0].top.z == 900,
			"actual public brush yields the required upward ladder to the isolated top floor"))
	{
		if (!topology.ladders.empty())
			std::fprintf(stderr, "ladder fixture direction=%u bottom=%u forward=%u top=%f\n",
				topology.ladders[0].direction, topology.ladders[0].bottomArea, topology.ladders[0].topAreas[0],
				topology.ladders[0].top.z);
		return false;
	}
	nav::NavQuery query(runtime.navigationSnapshot());
	nav::NavCorridor ground;
	if (!check(query.buildCorridor(1, 2, &ground) == nav::NavQueryResult::NoRoute,
			"runtime acceptance cannot pass through an invented ground link between the floors"))
		return false;
	runtime.beginRoundGeneration();
	globals.time += 10;
	const auto step = [&](float seconds)
	{
		globals.time += seconds;
		runtime.onStartFrame();
		runtime.onStartFramePost();
	};
	for (unsigned frame = 0; frame < 32; ++frame)
		step(.125f);
	const auto &receiver = navigation.behavior_managedBotStateMachines(1);
	if (!check(receiver.isInitialized() && receiver.state() != compat::CompatibilityStateId::Buy &&
				behavior.service_managedBotFullUpdateSequences()[1] > 0,
			"real ladder recipient is joined and has completed native Buy before receiving Help"))
		return false;
	gEntities[0].v.origin = Vector(320, -80, 936);
	gEntities[1].v.origin = Vector(0, 100, 36);
	gEntities[1].v.v_angle = Vector(0, -90, 0);
	queueSamples.clear();
	queueHelpSamples.clear();
	queueHelpMoves.clear();
	queueLadderClimbMoves = 0;
	queueLadderLowestPitch = 0;
	queueEnemiesVisible = true;
	queueLadderPhysics = true;
	bool copiedGoal = false, upperMovement = false, completedSpeechPause = false;
	for (unsigned frame = 0; frame < 600; ++frame)
	{
		// Complete natural speech after the native 0.4..0.6 s path throttle has
		// elapsed. An earlier Hunt search may legitimately reject MoveTo entry.
		bool helpStarted = false;
		for (const auto &sample : queueHelpSamples)
			helpStarted = helpStarted || sample.first == "radio/test/help.wav";
		const bool finishSpeech = helpStarted && !completedSpeechPause;
		step(finishSpeech ? .7f : .02f);
		completedSpeechPause = completedSpeechPause || finishSpeech;
		const auto &goal = navigation.behavior_managedBotObjectiveTargets(1);
		copiedGoal =
			copiedGoal || (goal.valid && std::fabs(goal.target.x - 320) < .01f && std::fabs(goal.target.y + 80) < .01f);
		if (gEntities[1].v.origin.z >= 935 && gEntities[1].v.origin.x > 100)
		{
			upperMovement = true;
			break;
		}
	}
	bool help = false, way = false;
	for (const auto &sample : queueHelpSamples)
	{
		help = help || sample.first == "radio/test/help.wav";
		way = way || sample.first == "radio/test/way.wav";
	}
	bool ok = check(help && way, "actual ladder case naturally transmits Help and queues immediate OnMyWay") &&
		check(copiedGoal, "actual ladder Help retains sender target across disconnected floors") &&
		check(queueLadderClimbMoves > 0 && queueLadderLowestPitch < -40,
			"real RunPlayerMove uses native upward pitch and forward ladder commands") &&
		check(upperMovement,
			"actual assistance climbs, dismounts and resumes movement on the upper floor toward copied sender goal");
	if (upperMovement)
	{
		gEntities[0].v.origin.x = 640;
		for (unsigned frame = 0; frame < 3; ++frame)
			step(.02f);
		const auto &goal = navigation.behavior_managedBotObjectiveTargets(1);
		ok = check(goal.valid && std::fabs(goal.target.x - 320) < .01f,
				 "sender movement after ladder dismount does not retarget the accepted Help copy") &&
			ok;
	}
	if (!ok)
		std::fprintf(stderr,
			"actual ladder help=%d way=%d copy=%d climbs=%u pitch=%f pose=(%f,%f,%f) state=%s task=%s\n", help ? 1 : 0,
			way ? 1 : 0, copiedGoal ? 1 : 0, queueLadderClimbMoves, queueLadderLowestPitch, gEntities[1].v.origin.x,
			gEntities[1].v.origin.y, gEntities[1].v.origin.z, compat::toString(receiver.state()),
			compat::toString(receiver.task()));
	return ok;
}

bool testProductionNavCenters(astrabot::metamod::PluginRuntime &runtime)
{
	using namespace astrabot;
	auto &ports = static_cast<metamod::BotBehaviorServices &>(runtime);
	const auto previous = ports.service_compatibilitySurface().configuration().mode;
	struct RestoreMode
	{
		metamod::PluginRuntime &runtime;
		compat::RuntimeMode mode;
		~RestoreMode()
		{
			runtime.setCompatibilityString(
				"astrabot_mode", mode == compat::RuntimeMode::Compatibility ? "compatibility" : "enhanced");
		}
	} restore{runtime, previous};
	nav::NavDocument document;
	document.setSourceIdentity({5U, 100U, 200U});
	const auto area = [](nav::AreaId id, float x, float y)
	{
		nav::NavArea item{};
		item.id = id;
		item.extent.lo = {x, y, 0};
		item.extent.hi = {x + 64, y + 64, 0};
		return item;
	};
	auto first = area(101, 0, 0), saddle = area(102, 64, 0), detour = area(103, 64, 64), goal = area(104, 128, 0);
	saddle.northEastZ = saddle.southWestZ = 200;
	first.connections[1] = {102, 103};
	saddle.connections[3] = {101};
	saddle.connections[1] = {104};
	detour.connections[0] = {101};
	detour.connections[1] = {104};
	goal.connections[3] = {102, 103};
	for (const auto &item : {first, saddle, detour, goal})
		document.addArea(item);
	nav::NavSnapshotPublisher publisher;
	publisher.publish(&document, runtime.snapshot().mapGeneration);
	struct Case
	{
		const char *mode;
		std::vector<nav::AreaId> expected;
	};
	const Case cases[] = {
		{"compatibility", {101, 102, 104}}, {"enhanced", {101, 103, 104}}, {"compatibility", {101, 102, 104}}};
	bool ok = true;
	for (const auto &item : cases)
	{
		runtime.setCompatibilityString("astrabot_mode", item.mode);
		nav::NavQuery query(publisher.snapshot());
		query.setPathCostContext(ports.navigation_managedPathCostContext(0));
		nav::NavCorridor route{};
		ok = check(query.buildCorridor(101, 104, nav::NavRouteType::Safest, &route) == nav::NavQueryResult::Found &&
					 route.areas == item.expected,
				 "production mode cost context selects the actual native or Enhanced route") &&
			ok;
	}
	return ok;
}
bool testActualFollowMovement(astrabot::metamod::PluginRuntime &runtime, globalvars_t &globals, float arrivalAge = 0.0f)
{
	using namespace astrabot;
	globals.mapname = 6;
	globals.maxEntities = 9;
	gObjectiveEntitiesAvailable = true;
	gBombSite = {};
	gBombSite.v.classname = 1;
	gBombSite.v.solid = SOLID_TRIGGER;
	gBombSite.v.origin = Vector(224, 32, 0);
	gBombSite.v.absmin = Vector(216, 24, -4);
	gBombSite.v.absmax = Vector(232, 40, 4);
	gSecondBombSite = {};
	gPlantedBomb = {};
	queueMode = "normal";
	queueEnemiesVisible = false;
	queueHelpRecording = true;
	setCvarFloat("bot_stop", 0);
	setCvarFloat("bot_allow_rogues", 0);
	setCvarFloat("mp_buytime", 0);
	gMpRoundtime = 0.1f;
	for (unsigned index = 0; index < 5; ++index)
	{
		auto &entity = gEntities[index];
		entity.free = index >= 2;
		entity.v.flags = FL_CLIENT | FL_FAKECLIENT | FL_ONGROUND;
		entity.v.health = 100;
		entity.v.deadflag = DEAD_NO;
		entity.v.solid = SOLID_SLIDEBOX;
		entity.v.movetype = MOVETYPE_WALK;
		entity.v.team = 1;
		entity.v.origin = Vector(index == 0 ? 112.0f : 24.0f, 32, 36);
		entity.v.mins = Vector(-16, -16, -36);
		entity.v.maxs = Vector(16, 16, 36);
		entity.v.velocity = Vector(0, 0, 0);
		entity.v.maxspeed = 240;
		entity.v.v_angle = Vector(0, 0, 0);
		entity.v.angles = entity.v.v_angle;
		entity.v.weapons = index == 0 ? (1 << 6) : 0;
	}
	const char *path = "astrabot_follow_move.nav";
	if (!check(writeTwoSiteNavigation(path, true, false, "BombsiteA"), "follow test NAV fixture written"))
		return false;
	const metamod::NavLoadRequest load = {
		path, "astrabot_follow_move", runtime.snapshot().mapGeneration, true, 123U, false, 0U};
	const auto loaded = runtime.loadNavigationFile(&load);
	std::remove(path);
	if (!check(loaded == metamod::NavLoadResult::Loaded, "production follow uses a real NAV corridor"))
		return false;
	runtime.onRoundLifecycleMessage("World triggered \"Round_End\"\n");
	runtime.onRoundLifecycleMessage("World triggered \"Round_Start\"\n");
	for (int slot : {1, 2})
	{
		runtime.onMessageBegin(MSG_ALL, 86, nullptr, nullptr);
		runtime.onWriteByte(slot);
		runtime.onWriteString("TERRORIST");
		runtime.onMessageEnd();
	}
	const auto tick = [&]()
	{
		for (unsigned i : {0U, 1U})
		{
			auto &entity = gEntities[i];
			entity.v.absmin = entity.v.origin + entity.v.mins;
			entity.v.absmax = entity.v.origin + entity.v.maxs;
		}
		globals.time += 0.125f;
		runtime.onStartFrame();
		runtime.onStartFramePost();
	};
	queueHelpSamples.clear();
	queueHelpMoves.clear();
	auto &navigation = static_cast<metamod::BotNavigationServices &>(runtime);
	const auto &sender = navigation.behavior_managedBotStateMachines(0);
	const auto &receiver = navigation.behavior_managedBotStateMachines(1);
	bool sawPlantMove = false;
	bool sawPlantState = false;
	bool arrived = false;
	bool followed = false;
	for (int i = 0; i < 120; ++i)
	{
		if (sawPlantMove && arrivalAge > 0 && !arrived && sender.state() == compat::CompatibilityStateId::MoveTo)
		{
			const float nextAge = globals.time + .125f - sender.stateTimestamp();
			if (nextAge >= arrivalAge)
			{
				gEntities[0].v.origin.x = 224;
				arrived = true;
			}
			else if (nextAge >= arrivalAge - .25f)
				gEntities[0].v.origin.x = 194;
		}
		tick();
		sawPlantState |= sender.state() == compat::CompatibilityStateId::PlantBomb;
		sawPlantMove = sawPlantMove ||
			(sender.state() == compat::CompatibilityStateId::MoveTo &&
				sender.task() == compat::CompatibilityTaskId::PlantBomb);
		if (receiver.state() == compat::CompatibilityStateId::Follow &&
			receiver.task() == compat::CompatibilityTaskId::Follow)
		{
			followed = true;
			break;
		}
	}
	if (arrivalAge > 0 && arrivalAge <= 2.0f)
	{
		queueHelpRecording = false;
		return check(sawPlantMove && sawPlantState && arrived && !followed,
			"arriving at exactly two seconds does not broadcast the GoingToPlantBomb plan");
	}
	if (arrivalAge > 2.0f && !check(sawPlantState && arrived, "the carrier enters PlantBomb on the plan frame"))
		return false;
	if (!check(sawPlantMove, "real C4 carrier enters MoveTo with the PlantBomb task") ||
		!check(followed, "natural completion of the production plant plan starts teammate Follow"))
		return false;
	gEntities[0].v.origin = Vector(168, 32, 36);
	queueHelpMoves.clear();
	bool retargeted = false;
	for (int i = 0; i < 20; ++i)
	{
		tick();
		const auto &decision = navigation.behavior_scenarioDecisions(1);
		retargeted |= decision.action == compat::ScenarioAction::Follow && decision.hasTarget &&
			decision.target.position.x == 168;
	}
	const auto contains = [&](const char *sample)
	{
		for (const auto &played : queueHelpSamples)
			if (played.first == sample)
				return true;
		return false;
	};
	bool moved = false;
	for (const float speed : queueHelpMoves)
		moved |= speed > 10;
	bool ok = check(contains("radio/test/plantplan.wav") && contains("radio/test/coverfriend.wav"),
				  "real voice transport plays the plant plan and CoveringFriend") &&
		check(retargeted && moved, "follower routes to the moving source and dispatches actual movement");
	if (arrivalAge == 0)
		++gEntities[0].serialnumber;
	for (int i = 0; i < 4; ++i)
		tick();
	ok = check(arrivalAge > 0 || receiver.state() != compat::CompatibilityStateId::Follow,
			 "replaced leader serial stops following instead of retargeting the slot") &&
		ok;
	queueHelpRecording = false;
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
	return testQueueRadioReset(runtime, engine) && testPlanAgreementRuntime(runtime, globals) &&
		testProductionNavCenters(runtime) && testPublicNavLadders(runtime, engine) &&
		testPublicNativeLadderTrace(engine) && testNativeHelpDeathMemory(runtime, engine, globals) &&
		testActualHelpMovement(runtime, engine, globals) && testActualHelpMovement(runtime, engine, globals, true) &&
		testActualHelpMovement(runtime, engine, globals, true, true) &&
		testActualHelpMovement(runtime, engine, globals, false, false, true) &&
		testActualHelpMovement(runtime, engine, globals, true, false, false, true) &&
		testActualLadderHelpMovement(runtime, engine, globals) &&
		testActualHelpMovement(runtime, engine, globals, false, false, true, false, true) &&
		testActualFollowMovement(runtime, globals, 2.0f) && testActualFollowMovement(runtime, globals, 2.125f) &&
		testActualFollowMovement(runtime, globals);
}
}
