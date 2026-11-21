#include "plugin/plugin_runtime.hpp"
#include <cstring>
namespace
{
bool reportingInFixture = false;
bool bombsiteClearFixture = false;
bool plantedSiteFixture = false;
bool requestedSiteFixture = false;
bool carrierFixture = false;
bool looseBombFixture = false;
bool looseGuardFixture = false;
bool deferredSiteFixture = false;
bool encourageDefuseFixture = false;
bool barelyDefusedFixture = false;
bool blindedChatterFixture = false;
float blindedChatterRoll = 100.0f;
float blindedFireRoll = 100.0f;
int blindedMoveRoll = 1;
bool blindedDrawing = false;
bool blindedMoveDrawn = false;
std::function<void()> blindedMoveMutation;
struct BlindFixtureInput
{
	float forward = 0, side = 0;
	unsigned short buttons = 0;
	unsigned count = 0;
} blindFixtureInput;
bool duplicateBlindNetname = false;
bool friendlyFireChatterFixture = false;
bool enemyDamageHideFixture = false;
bool damageRetargetFixture = false;
unsigned friendlyFireDelayDraws = 0;
cvar_t queueFreeForAll{};
cvar_t queueDefuseFuse{};
bool preplantGuardFixture = false;
bool waitingHereFixture = false;
bool hostageUseFixture = false;
bool sniperWarningFixture = false;
bool scaredFixture = false;
int sniperViewer = -1;
int activeSniperFixtureWeapon = 0;
int sniperWeaponQueries = 0;
bool hostageGuardRolls = false;
bool chooseTeamRush = false;
float teamRushRoll = 100.0f;
float preplantIdleRoll = -1.0f;
int carrierViewer = -1;
bool looseBombLineHitsBox = false;
const char *bombFixtureMap()
{
	if (bombsiteClearFixture) return "astrabot_bombsite_clear";
	if (plantedSiteFixture) return "astrabot_bombsite_planted";
	if (requestedSiteFixture) return "astrabot_bombsite_request";
	if (carrierFixture) return "astrabot_bomb_carrier";
	if (looseBombFixture) return "astrabot_loose_bomb";
	if (looseGuardFixture) return "astrabot_guard_loose_bomb";
	if (deferredSiteFixture) return "astrabot_guard_planted_bombsite";
	if (preplantGuardFixture) return "astrabot_guard_unplanted_bombsite";
	return nullptr;
}
bool fallBackFixture = false;
bool victoryFixture = false;
bool autoFollowChatterFixture = false;
bool queueCareer = false;

void selectStatementFixture(int argc, char **argv)
{
	reportingInFixture = argc == 2 && std::strcmp(argv[1], "--reporting-in") == 0;
	bombsiteClearFixture = argc == 2 && std::strcmp(argv[1], "--bombsite-clear") == 0;
	plantedSiteFixture = argc == 2 && std::strcmp(argv[1], "--bombsite-planted") == 0;
	requestedSiteFixture = argc == 2 && std::strcmp(argv[1], "--bombsite-request") == 0;
	carrierFixture = argc == 2 && std::strcmp(argv[1], "--bomb-carrier") == 0;
	looseBombFixture = argc == 2 && std::strcmp(argv[1], "--loose-bomb") == 0;
	looseGuardFixture = argc == 2 && std::strcmp(argv[1], "--guard-loose-bomb") == 0;
	encourageDefuseFixture = argc == 2 && std::strcmp(argv[1], "--encourage-defuse") == 0;
	enemyDamageHideFixture = argc == 2 && std::strcmp(argv[1], "--enemy-damage-hide") == 0;
	damageRetargetFixture = argc == 2 && std::strcmp(argv[1], "--damage-retarget") == 0;
	barelyDefusedFixture = argc == 2 && std::strcmp(argv[1], "--barely-defused") == 0;
	blindedChatterFixture = argc == 2 && std::strcmp(argv[1], "--blinded-chatter") == 0;
	friendlyFireChatterFixture = argc == 2 && std::strcmp(argv[1], "--friendly-fire-chatter") == 0;
	deferredSiteFixture = encourageDefuseFixture || enemyDamageHideFixture ||
		(argc == 2 && std::strcmp(argv[1], "--guard-planted-bombsite") == 0);
	waitingHereFixture = argc == 2 && std::strcmp(argv[1], "--waiting-here") == 0;
	preplantGuardFixture = waitingHereFixture ||
		(argc == 2 && std::strcmp(argv[1], "--guard-unplanted-bombsite") == 0);
	hostageUseFixture = argc == 2 && std::strcmp(argv[1], "--hostage-use-report") == 0;
	sniperWarningFixture = argc == 2 && std::strcmp(argv[1], "--sniper-warning") == 0;
	scaredFixture = argc == 2 && std::strcmp(argv[1], "--scared-outnumbered") == 0;
	fallBackFixture = argc == 2 && std::strcmp(argv[1], "--team-fall-back") == 0;
	victoryFixture = argc == 2 && std::strcmp(argv[1], "--round-victory") == 0;
	autoFollowChatterFixture = argc == 2 && std::strcmp(argv[1], "--auto-follow-chatter") == 0;
}
bool testStatementQueue(astrabot::metamod::PluginRuntime &, enginefuncs_t &, globalvars_t &);
void setupStatementQueue(enginefuncs_t &, globalvars_t &);
}
#include <entity_state.h>
#define ASTRABOT_EXTRA_ACTOR_TEST testStatementQueue
#define ASTRABOT_ACTOR_TEST_ARGS selectStatementFixture
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
bool hostageUseTargetVisible = false;
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
bool queueFollowLeaderVisible = false;
std::vector<std::pair<std::string, float>> queueHelpSamples;
void queueMove(edict_t *entity, const float *angles, float forward, float side, float up, unsigned short buttons,
	unsigned char impulse, unsigned char milliseconds)
{
	if (blindedChatterFixture && entity == &gEntities[0])
		blindFixtureInput = {forward, side, buttons, blindFixtureInput.count + 1U};
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
int queueSpeakerSlot = 0;
std::vector<std::pair<int, std::string>> reportAudio;
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
			"Place BombsiteA site.wav End Chatter GoingToPlantBomb Place UNDEFINED plantplan_default.wav Place BombsiteA plantplan.wav End Chatter CoveringFriend coverfriend.wav End "
			"Chatter PlantingBomb planting.wav End Chatter Clear clear.wav End";
	}
	if (std::strcmp(name, "queue_speech.db") == 0)
	{
		text =
			"BaseDir radio/test/ Chatter Affirmative Place UNDEFINED Count 3 ack_wrong.wav Count Many ack.wav End "
			"Chatter Help Place UNDEFINED help.wav End Chatter OnMyWay Place UNDEFINED way.wav End "
			"Place BombsiteA site.wav End Chatter GoingToPlantBomb Place UNDEFINED plantplan_default.wav Place BombsiteA plantplan.wav End Chatter CoveringFriend coverfriend.wav End "
			"Chatter PlantingBomb planting.wav End Chatter Clear clear.wav End";
	}
	if (!text)
	{
		*length = 0;
		return nullptr;
	}
	std::string contents(text);
	if (victoryFixture)
		contents += " Chatter WonRoundQuickly win_fast.wav End Chatter WonRound win_team.wav End "
			"Chatter LastManStanding win_alone.wav End";
	if (barelyDefusedFixture)
		contents += " Chatter BarelyDefused barely_defused.wav End";
	if (blindedChatterFixture)
		contents += " Chatter Blinded blinded.wav End";
	if (friendlyFireChatterFixture)
		contents += " Chatter FriendlyFire friendly_fire.wav End";
	if (autoFollowChatterFixture)
		contents += " Chatter FollowingSir follow_sir.wav End Chatter FollowingCommander follow_commander.wav End";
	if (scaredFixture)
		contents += " Chatter ScaredEmote scared.wav End";
	if (sniperWarningFixture)
		contents += " Chatter SniperWarning sniper.wav End";
	if (hostageUseFixture)
		contents += " Chatter HostagesBeingTaken taken.wav End Chatter HostagesTaken lost.wav End "
			"Chatter GoingToGuardHostageEscapeZone rescueplan.wav End";
	if (reportingInFixture)
		contents += " Chatter RequestReport Radio EVENT_RADIO_REPORT_IN_TEAM request.wav End";
	if (bombsiteClearFixture)
		contents += " Chatter BombsiteClear zoneclear.wav End";
	if (plantedSiteFixture || requestedSiteFixture || deferredSiteFixture)
		contents += " Chatter PlantedBombPlace planted.wav End";
	if (encourageDefuseFixture)
		contents += " Chatter BombsiteSecure secure.wav End Chatter WaitingForHumanToDefuseBomb waitdefuse.wav End "
			"Chatter WaitingForHumanToDefuseBombPanic urgentdefuse.wav End";
	if (requestedSiteFixture)
		contents += " Chatter WhereIsTheBomb where.wav End";
	if (carrierFixture)
		contents += " Chatter SpottedBomber carrier.wav End";
	if (looseBombFixture || looseGuardFixture || preplantGuardFixture)
		contents += " Chatter SpottedLooseBomb loose.wav End Chatter TheyPickedUpTheBomb pickup.wav End";
	if (preplantGuardFixture)
		contents += " Chatter GoingToDefendBombsite Place UNDEFINED siteplan.wav Place BombsiteA siteplan_a.wav End";
	if (waitingHereFixture)
		contents += " Chatter WaitingHere waiting_here.wav End";
	if (looseGuardFixture)
		contents += " Chatter GoingToGuardLooseBomb Place BombsiteA guardplan.wav End Chatter GuardingLooseBomb guard.wav End";
	*length = static_cast<int>(contents.size());
	auto *bytes = new unsigned char[static_cast<std::size_t>(*length) + 1];
	std::memcpy(bytes, contents.c_str(), static_cast<std::size_t>(*length) + 1);
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
	if ((victoryFixture || barelyDefusedFixture || blindedChatterFixture) && std::strcmp(name, "ResetHUD") == 0)
		return 104;
	if (blindedChatterFixture && std::strcmp(name, "ScreenFade") == 0)
		return 105;
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
void queueNumber(int number)
{
	queueSpeakerSlot = number;
}
void queueString(const char *sample)
{
	if (queueMessage == 101 && queueRecipient == &gEntities[0])
	{
		queueSamples.emplace_back(sample);
		if (reportingInFixture || hostageUseFixture || sniperWarningFixture || bombFixtureMap())
			reportAudio.emplace_back(queueSpeakerSlot, sample);
		if (queueHelpRecording && queueClock)
			queueHelpSamples.emplace_back(sample, queueClock->time);
	}
}
void queueEnd()
{
}
void queueVisibility(const float *from, const float *end, int flags, edict_t *observer, TraceResult *trace)
{
	*trace = {};
	trace->vecEndPos = Vector(end[0], end[1], end[2]);
	trace->flFraction = queueEnemiesVisible ? 1.0f : 0.0f;
	if (queueFollowLeaderVisible && observer == &gEntities[1] && flags == 0x101 &&
		end[0] == gEntities[0].v.origin.x && end[1] == gEntities[0].v.origin.y && end[2] == gEntities[0].v.origin.z)
		trace->flFraction = 1.0f;
	if (sniperWarningFixture && sniperViewer >= 0 && observer == &gEntities[sniperViewer] &&
		std::fabs(end[0] - 400.0f) < 24.0f && std::fabs(end[1]) < 24.0f)
		trace->flFraction = 1.0f;
	if (hostageUseFixture && queueClock && queueClock->mapname == 34 && observer == &gEntities[0] &&
		flags == 0x101 && end[0] == 100 && end[1] == 0 && end[2] == 36)
		trace->flFraction = 1.0f;
	if (hostageUseFixture && hostageUseTargetVisible && flags == 0x101 &&
		end[0] == gPlantedBomb.v.origin.x && end[1] == gPlantedBomb.v.origin.y &&
		end[2] == gPlantedBomb.v.origin.z + 54.0f)
		trace->flFraction = 1.0f;
	if ((looseGuardFixture || preplantGuardFixture) && observer == &gEntities[0] && carrierViewer == 0 && flags == 0x101 &&
		end[0] == gPlantedBomb.v.origin.x && end[1] == gPlantedBomb.v.origin.y && end[2] == gPlantedBomb.v.origin.z)
		trace->flFraction = 1.0f;
	if ((carrierFixture || looseBombFixture) && carrierViewer >= 0 && observer == &gEntities[carrierViewer])
	{
		trace->flFraction = 1.0f;
		if (looseBombFixture && end[0] == gPlantedBomb.v.origin.x && end[1] == gPlantedBomb.v.origin.y &&
			end[2] == gPlantedBomb.v.origin.z)
		{
			if (looseBombLineHitsBox)
			{
				trace->flFraction = .5f;
				trace->pHit = &gPlantedBomb;
			}
			else if (flags != 0x101)
				trace->flFraction = .5f; // A window between the observer and the C4.
		}
	}
	if ((requestedSiteFixture || deferredSiteFixture) && observer == &gEntities[0] && end[0] == 229 && end[1] == 32)
		trace->flFraction = 1.0f;
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
	if (blindedChatterFixture)
	{
		engine.pfnCVarGetFloat = [](const char *name) {
			return std::strcmp(name, "bot_stop") == 0 ? gBotStopCvar.value : getCvarFloat(name);
		};
		engine.pfnRandomLong = [](int32 low, int32 high) -> int32 {
			if (blindedDrawing && low == 1 && high == 5)
			{
				blindedMoveDrawn = true;
				if (blindedMoveMutation)
				{
					auto mutate = std::move(blindedMoveMutation);
					mutate();
				}
				return blindedMoveRoll;
			}
			return low;
		};
	}
	if (autoFollowChatterFixture)
		engine.pfnIsCareerMatch = []() { return queueCareer ? TRUE : FALSE; };
	if (encourageDefuseFixture || barelyDefusedFixture)
	{
		queueDefuseFuse.value = 70.0f;
		engine.pfnCVarGetPointer = [](const char *name) {
			return std::strcmp(name, "mp_c4timer") == 0 ? &queueDefuseFuse : getCvar(name);
		};
		engine.pfnCVarGetFloat = [](const char *name) {
			return std::strcmp(name, "mp_c4timer") == 0 ? queueDefuseFuse.value : getCvarFloat(name);
		};
	}
	if (friendlyFireChatterFixture || damageRetargetFixture)
	{
		engine.pfnCVarGetPointer = [](const char *name) {
			return std::strcmp(name, "mp_freeforall") == 0 ? &queueFreeForAll : getCvar(name);
		};
		engine.pfnCVarGetFloat = [](const char *name) {
			return std::strcmp(name, "mp_freeforall") == 0 ? queueFreeForAll.value : getCvarFloat(name);
		};
	}
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
	if (hostageUseFixture)
		engine.pfnSzFromIndex = [](int id) -> const char * { return id == 33 ? "hostage_entity" : id == 34 ? "cs_test" :
			id == 35 ? "func_hostage_rescue" : szFromIndex(id); };
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
	if (friendlyFireChatterFixture && low > .299f && low < .301f && high == .5f)
	{
		++friendlyFireDelayDraws;
		return .375f;
	}
	if (blindedChatterFixture && low == 0.0f && high == 100.0f)
	{
		if (blindedMoveDrawn)
		{
			blindedMoveDrawn = false;
			return blindedFireRoll;
		}
		return blindedChatterRoll;
	}
	if (autoFollowChatterFixture && low == 0.0f && high == 1.0f)
		return low;
	if (encourageDefuseFixture && ((low == 10.0f && high == 15.0f) ||
			(low == 5.0f && high == 8.0f) || (low == 3.0f && high == 4.0f)))
		return low;
	if (chooseTeamRush && low == 0.0f && high == 100.0f)
	{
		chooseTeamRush = false;
		return teamRushRoll;
	}
	if (hostageGuardRolls && low == 0 && high == 100)
		return 100;
	if (preplantGuardFixture && preplantIdleRoll >= 0 && low == 0 && high == 100)
		return preplantIdleRoll;
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
	// The encounter starts with the sender facing the enemies to the east.
	// Warm-up Hunt updates can have turned either bot before sight is enabled.
	gEntities[0].v.v_angle = gEntities[0].v.angles = Vector(0, 0, 0);
	gEntities[1].v.v_angle = gEntities[1].v.angles = Vector(0, 180, 0);
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
		const auto attackBeforePlant = receiver.attackEntryEpoch();
		runtime.onMessageBegin(MSG_ALL, 98, nullptr, nullptr);
		runtime.onWriteByte(4);
		runtime.onWriteString("#Bomb_Planted");
		runtime.onMessageEnd();
		step();
		ok = check((receiver.state() == compat::CompatibilityStateId::Idle ||
						receiver.state() == compat::CompatibilityStateId::Hunt) &&
					 receiver.task() == compat::CompatibilityTaskId::SeekAndDestroy &&
					 !navigation.behavior_managedBotObjectiveTargets(1).valid,
				 "the planted-bomb notice clears the CT's previous Help task and route") && ok;
		if (attackRetained)
		{
			ok = check(!receiver.attackOverlayActive() || receiver.attackEntryEpoch() != attackBeforePlant,
					 "the planted-bomb transition ends the attack that had suspended the CT's old task") && ok;
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
					 "enemy death ends any new attack without restoring the CT's old Help task") && ok;
		}
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
	gEntities[0].v.v_angle = gEntities[0].v.angles = Vector(0, 0, 0);
	gEntities[1].v.origin = Vector(0, 100, 36);
	gEntities[1].v.v_angle = gEntities[1].v.angles = Vector(0, -90, 0);
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
bool testActualFollowMovement(astrabot::metamod::PluginRuntime &runtime, globalvars_t &globals, float arrivalAge = 0.0f,
	bool defendAtSpot = false, bool stoppedCover = false)
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
	if (!check(writeTwoSiteNavigation(path, true, defendAtSpot || stoppedCover, "BombsiteA"), "follow test NAV fixture written"))
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
	bool sawDefendHide = false;
	bool movedToCapturedSpot = false;
	bool arrived = false;
	bool followed = false;
	float observeUntil = globals.time + 15.0f;
	while (globals.time < observeUntil)
	{
		if (sawPlantMove && arrivalAge > 0 && !arrived && sender.state() == compat::CompatibilityStateId::MoveTo)
		{
			const float nextAge = globals.time + .125f - sender.stateTimestamp();
			if (nextAge >= arrivalAge)
			{
				gEntities[0].v.origin.x = 224;
				arrived = true;
				observeUntil = (std::max)(observeUntil, globals.time + 12.0f);
			}
			else if (nextAge >= arrivalAge - .25f)
				gEntities[0].v.origin.x = 194;
		}
		tick();
		sawDefendHide |= arrived && receiver.state() == compat::CompatibilityStateId::Hide;
		if (defendAtSpot && arrived && !queueHelpSamples.empty())
		{
			gEntities[0].v.weapons &= ~(1 << 6);
			gEntities[0].v.origin.x = 64;
			const auto &guard = navigation.behavior_scenarioDecisions(1);
			if (receiver.task() == compat::CompatibilityTaskId::HoldPosition &&
				guard.action == compat::ScenarioAction::Guard && guard.hasTarget &&
				guard.target.position.x == 224 && guard.target.position.y == 50 &&
				!queueHelpMoves.empty() && queueHelpMoves.back() > 10)
				movedToCapturedSpot = true;
		}
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
		const bool heardPlant = std::any_of(queueHelpSamples.begin(), queueHelpSamples.end(),
			[](const std::pair<std::string, float> &sample) { return sample.first == "radio/test/planting.wav"; });
		const bool heardAck = std::any_of(queueHelpSamples.begin(), queueHelpSamples.end(),
			[](const std::pair<std::string, float> &sample) { return sample.first == "radio/test/ack.wav"; });
		if (defendAtSpot)
		{
			queueHelpRecording = true;
			gEntities[1].v.origin = Vector(204, 50, 36);
			tick();
			const bool atBoundary = navigation.behavior_scenarioDecisions(1).target.inUseRange;
			gEntities[1].v.origin.x = 205;
			queueHelpMoves.clear();
			tick();
			const auto &guard = navigation.behavior_scenarioDecisions(1);
			queueHelpRecording = false;
			if (!check(!atBoundary && guard.action == compat::ScenarioAction::Guard && guard.target.inUseRange &&
					!queueHelpMoves.empty() && queueHelpMoves.back() == 0,
				"defender stops inside the 20-unit feet radius and keeps moving at the boundary"))
				return false;
			const float entered = receiver.stateTimestamp();
			while (globals.time <= entered + 60.0f && receiver.state() == compat::CompatibilityStateId::Hide)
				tick();
			if (!check(receiver.state() != compat::CompatibilityStateId::Hide && globals.time > entered + 30.0f,
					"the defense assignment expires after its 30-to-60-second Hide duration"))
				return false;
		}
		return check(heardPlant, "carrier broadcasts the planting report on arrival") &&
			check(heardAck == !defendAtSpot, "only nearby Hide acknowledges the planting report") &&
			check(!defendAtSpot || movedToCapturedSpot, "direct Hide moves to the captured Place after the sender leaves") &&
			check(sawDefendHide, "natural planting report sends a teammate into Hide") &&
			check(sawPlantMove && sawPlantState && arrived && !followed,
			"arriving at exactly two seconds does not broadcast the GoingToPlantBomb plan");
	}
	if (arrivalAge > 2.0f && !check(sawPlantState && arrived, "the carrier enters PlantBomb on the plan frame"))
		return false;
	if (!followed)
	{
		std::fprintf(stderr, "arrival age %.3f, receiver %s/%s, heard:", arrivalAge,
			compat::toString(receiver.state()), compat::toString(receiver.task()));
		for (const auto &played : queueHelpSamples)
			std::fprintf(stderr, " %s@%.3f", played.first.c_str(), played.second);
		std::fprintf(stderr, "\n");
	}
	if (!check(sawPlantMove, "real C4 carrier enters MoveTo with the PlantBomb task") ||
		!check(followed, "natural completion of the production plant plan starts teammate Follow"))
		return false;
	gEntities[0].v.origin = Vector(168, 32, 36);
	if (stoppedCover)
	{
		auto &behavior = static_cast<metamod::BotBehaviorServices &>(runtime);
		struct CoverEntryTrace final : compat::IRandomTraceSink
		{
			explicit CoverEntryTrace(const compat::CompatibilityStateMachine &state) : machine(state) {}
			void record(const compat::RandomTraceRecord &entry) override
			{
				if (entry.request.actor.slot == 2U && entry.request.semanticId)
				{
					draws.emplace_back(entry.request.semanticId);
					if (draws.back() == "Hide.Duration")
						enteredAtDuration = machine.state() == compat::CompatibilityStateId::Hide;
				}
			}
			const compat::CompatibilityStateMachine &machine;
			std::vector<std::string> draws;
			bool enteredAtDuration = false;
		} trace(receiver);
		behavior.service_compatibilityRandomSource().setTraceSink(&trace);
		const float entered = globals.time;
		for (int i = 0; i < 28 && receiver.state() == compat::CompatibilityStateId::Follow; ++i)
		{
			behavior.applyNavigationRequest({1, metamod::NavigationRequestKind::DestroyPath});
			tick();
		}
		behavior.service_compatibilityRandomSource().setTraceSink(nullptr);
		const bool hid = receiver.state() == compat::CompatibilityStateId::Hide &&
			receiver.task() == compat::CompatibilityTaskId::Follow && globals.time - entered > 1.0f;
		const auto duration = std::find(trace.draws.begin(), trace.draws.end(), "Hide.Duration");
		const auto retry = std::find(trace.draws.begin(), duration, "CSBOT-PATH-RETRY");
		const bool coverPathReady = check(duration != trace.draws.end() && retry != duration && trace.enteredAtDuration &&
				behavior.navigation_hasActivePath(1),
			"Follow computes and retains the cover path before Hide draws its duration");
		tick();
		const auto &cover = navigation.behavior_scenarioDecisions(1);
		bool ok = check(hid && cover.hasTarget && cover.target.position.x == 32 && cover.target.position.y == 50,
			"a stopped leader without a path waits and sends Follow to the nearest unoccupied cover") && coverPathReady;
		gEntities[1].v.origin = Vector(32, 50, 36);
		gEntities[0].v.velocity.x = 200.0f;
		queueHelpMoves.clear();
		tick();
		ok = check(receiver.state() == compat::CompatibilityStateId::Hide && cover.target.inUseRange &&
				!queueHelpMoves.empty() && queueHelpMoves.back() == 0.0f,
			"arrival holds cover while the leader is exactly at the 200-unit run threshold") && ok;
		gEntities[0].v.velocity.x = 201.0f;
		tick();
		queueHelpRecording = false;
		return check(receiver.state() == compat::CompatibilityStateId::Follow,
			"a leader running over the threshold resumes Follow from cover") && ok;
	}
	gEntities[0].v.origin.x = 8.0f;
	gEntities[0].v.velocity.x = 141.0f;
	queueHelpMoves.clear();
	bool retargeted = false;
	bool routedAhead = false;
	for (int i = 0; i < 80; ++i)
	{
		tick();
		const auto &decision = navigation.behavior_scenarioDecisions(1);
		retargeted |= decision.action == compat::ScenarioAction::Follow && decision.hasTarget &&
			decision.target.position.x == 8;
		nav::NavVector routeGoal{};
		routedAhead |= navigation.behavior_buildManagedObjectiveTarget(1, &routeGoal, nullptr, nullptr, nullptr) &&
			routeGoal.x == 224.0f && routeGoal.y == 32.0f;
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
	const bool heardPlan = check(contains("radio/test/plantplan.wav"), "place-specific planting plan is audible");
	// Both responses are native Say/MyIntention. The pending defense acknowledgement
	// suppresses the later follow acknowledgement when cover and plan arrive together.
	const bool simultaneous = arrivalAge > 2.0f;
	const bool heardReply = check(contains("radio/test/coverfriend.wav") == !simultaneous &&
			contains("radio/test/ack.wav") == simultaneous,
		"the receiver speaks the first pending intention while still carrying out Follow");
	if (!heardPlan || !heardReply)
		for (const auto &sample : queueHelpSamples)
			std::fprintf(stderr, "arrival %.3f %s @ %.3f\n", arrivalAge, sample.first.c_str(), sample.second);
	bool ok = heardPlan && heardReply &&
		check(routedAhead, "Follow paths toward the nearest eligible NAV center ahead of the moving leader") &&
		check(retargeted && moved, "follower routes to the moving source and dispatches actual movement");
	if (arrivalAge > 2.0f)
	{
		ok = check(receiver.state() == compat::CompatibilityStateId::Follow,
			"carrier control begins in Follow with its live managed leader") && ok;
		gEntities[1].v.weapons |= 1 << 6;
		gEntities[1].v.origin = Vector(224, 32, 36);
		tick();
		ok = check(receiver.state() == compat::CompatibilityStateId::PlantBomb &&
			receiver.task() == compat::CompatibilityTaskId::PlantBomb && receiver.stateTimestamp() == globals.time,
			"Follow with a valid leader and carried C4 enters Plant directly at the site") && ok;
	}
	if (arrivalAge == 0)
	{
		auto &behavior = static_cast<metamod::BotBehaviorServices &>(runtime);
		behavior.applyNavigationRequest({1, metamod::NavigationRequestKind::DestroyPath});
		gEntities[1].v.velocity.x = 48.0f;
		const auto movementSpeed = [&]()
		{
			queueHelpMoves.clear();
			gEntities[1].v.origin.x += 6.0f;
			tick();
			return queueHelpMoves.empty() ? 0.0f : queueHelpMoves.back();
		};
		const auto walks = [](float speed) { return speed > 10.0f && speed < 150.0f; };
		// The harness holds origins fixed during the speech check. Wait for
		// a fresh active corridor before sampling the walk/run transitions.
		for (int i = 0; i < 8; ++i)
			if (movementSpeed() > 150.0f)
				break;
		queueFollowLeaderVisible = true;
		gEntities[0].v.velocity.x = 140.0f;
		const float first = movementSpeed();
		movementSpeed();
		const float atQuarterSecond = movementSpeed();
		const float afterQuarterSecond = movementSpeed();
		if (first <= 150.0f || atQuarterSecond <= 150.0f || !walks(afterQuarterSecond))
			std::fprintf(stderr, "Follow gait commands: %.1f %.1f %.1f\n", first, atQuarterSecond, afterQuarterSecond);
		ok = check(first > 150.0f && atQuarterSecond > 150.0f && walks(afterQuarterSecond),
			"a visible leader at 140 starts a walk only after more than a quarter second") && ok;
		gEntities[0].v.velocity.x = 10.0f;
		const float stoppedSpeed = movementSpeed();
		ok = check(navigation.behavior_followWalking(1) && stoppedSpeed < 150.0f,
			"a stopped leader leaves an active Follow walk unchanged") && ok;
		queueFollowLeaderVisible = false;
		gEntities[0].v.velocity.x = 141.0f;
		ok = check(walks(movementSpeed()), "an unseen running leader does not immediately cancel walking") && ok;
		queueFollowLeaderVisible = true;
		ok = check(movementSpeed() > 150.0f, "seeing the leader run restores full-speed Follow movement") && ok;
		gEntities[0].v.origin.x = 240.0f;
		behavior.applyNavigationRequest({1, metamod::NavigationRequestKind::DestroyPath});
		for (int i = 0; i < 6; ++i)
			tick();
		nav::NavVector routeGoal{};
		ok = check(receiver.state() == compat::CompatibilityStateId::Follow &&
				!behavior.navigation_hasActivePath(1) &&
				!navigation.behavior_buildManagedObjectiveTarget(1, &routeGoal, nullptr, nullptr, nullptr),
			"Follow does not recreate an expired route when no areas are ahead of the leader") && ok;
		queueFollowLeaderVisible = false;
		++gEntities[0].serialnumber;
	}
	for (int i = 0; i < 4; ++i)
		tick();
	ok = check(arrivalAge > 0 || receiver.state() != compat::CompatibilityStateId::Follow,
			 "replaced leader serial stops following instead of retargeting the slot") &&
		ok;
	queueHelpRecording = false;
	return ok;
}

bool testHostageUseReport(astrabot::metamod::PluginRuntime &runtime, enginefuncs_t &hooks, globalvars_t &globals)
{
	using namespace astrabot;
	queueMode = "normal";
	queueEnemiesVisible = false;
	setCvarFloat("mp_buytime", 0);
	setCvarFloat("bot_stop", 0);
	runtime.setCompatibilityFloat("bot_quota", 2);
	if (!check(runtime.executeCompatibilityCommand(request("bot_add_t", 1U, "Queue Peer")) ==
			metamod::CompatibilityCommandResult::Handled, "second T joins the hostage report fixture"))
		return false;
	for (int i = 0; i < 2; ++i)
	{
		auto &ent = gEntities[i];
		ent.v.team = 1;
		ent.v.flags = FL_CLIENT | FL_FAKECLIENT | FL_ONGROUND;
		ent.v.health = 100;
		ent.v.deadflag = DEAD_NO;
		ent.v.solid = SOLID_SLIDEBOX;
		ent.v.movetype = MOVETYPE_WALK;
		ent.v.maxspeed = 250;
		ent.v.view_ofs = Vector(0, 0, 17);
		hooks.pfnMessageBegin(MSG_ALL, 86, nullptr, nullptr);
		hooks.pfnWriteByte(i + 1);
		hooks.pfnWriteString("TERRORIST");
		hooks.pfnMessageEnd();
	}
	auto &ct = gEntities[2];
	gEntityNames[2] = "Rescuer";
	ct.v.netname = 102;
	ct.v.button = IN_USE;
	ct.v.origin = Vector(0, 0, 36);
	gEntities[0].v.origin = Vector(0, 0, 1236);
	gEntities[1].v.origin = Vector(1201, 0, 36);
	runtime.beginRoundGeneration();
	auto advance = [&](int frames) {
		for (int i = 0; i < frames; ++i)
		{
			globals.time += .125f;
			runtime.onStartFrame();
			runtime.onStartFramePost();
		}
	};
	advance(40);
	reportAudio.clear();
	auto count = [&](int speaker, const char *sample) {
		return std::count(reportAudio.begin(), reportAudio.end(), std::make_pair(speaker, std::string(sample)));
	};
	char firstTouch[] = "\"Rescuer<%d><STEAM_0:1:12><CT>\" triggered \"Touched_A_Hostage\"\n";
	hooks.pfnAlertMessage(at_logged, firstTouch, 999);
	advance(16);
	if (!check(!count(1, "radio/test/taken.wav"), "wrong player ID cannot announce a hostage"))
		return false;
	hooks.pfnAlertMessage(at_logged, firstTouch, 103);
	advance(48);
	if (!check(count(1, "radio/test/taken.wav") == 1 && count(2, "radio/test/taken.wav") == 0 &&
			count(2, "radio/test/ack.wav") == 1 && !count(1, "radio/test/ack.wav"),
			"the T at 1200 hears the CT and the T at 1201 acknowledges its teammate's report"))
		return false;
	DLL_FUNCTIONS pre{}, post{};
	int version = INTERFACE_VERSION;
	if (!check(runtime.provideEntityApi(&pre, &version) && runtime.provideEntityApiPost(&post, &version) &&
			pre.pfnPlayerPostThink && post.pfnPlayerPostThink, "hostage speech is tied to the actual player's use"))
		return false;
	auto &hostage = gPlantedBomb;
	hostage.free = false;
	hostage.serialnumber = 72;
	hostage.v.classname = 33;
	hostage.v.takedamage = DAMAGE_YES;
	hostage.v.origin = gEntities[1].v.origin;
	auto voice = [&](const char *sample, int channel = CHAN_VOICE, int flags = 0, float attenuation = ATTN_NORM) {
		hooks.pfnEmitSound(&hostage, channel, sample, VOL_NORM, attenuation, flags, PITCH_NORM);
	};
	reportAudio.clear();
	voice("hostage/hos1.wav");
	pre.pfnPlayerPostThink(&ct);
	pre.pfnPlayerPostThink(&gEntities[1]);
	voice("hostage/hos1.wav");
	post.pfnPlayerPostThink(&gEntities[1]);
	voice("hostage/hos1.wav", CHAN_WEAPON);
	voice("hostage/hos1.wav", CHAN_VOICE, 1 << 5);
	voice("hostage/hunuse/comeback.wav");
	post.pfnPlayerPostThink(&ct);
	advance(32);
	if (!check(!count(1, "radio/test/taken.wav") && !count(2, "radio/test/taken.wav"),
			"unattributed, nested non-use, stopped, other-channel and stop-follow samples do not produce reports"))
		return false;
	pre.pfnPlayerPostThink(&ct);
	voice("hostage/hos5.wav");
	hooks.pfnAlertMessage(at_logged, firstTouch, 103);
	post.pfnPlayerPostThink(&ct);
	advance(48);
	if (!check(count(1, "radio/test/taken.wav") == 1 && count(2, "radio/test/ack.wav") == 1 &&
			!count(2, "radio/test/taken.wav"), "native voice and the first-touch log describe one use at the CT's origin"))
		return false;
	reportAudio.clear();
	pre.pfnPlayerPostThink(&ct);
	voice("hostage/huse/illfollow.wav", CHAN_VOICE, 0, 1.0f);
	post.pfnPlayerPostThink(&ct);
	advance(48);
	if (!check(count(1, "radio/test/taken.wav") == 1 && count(2, "radio/test/ack.wav") == 1,
			"a later voiced improved-hostage follow is reported without another first-touch log"))
		return false;

	reportAudio.clear();
	auto &observations = static_cast<metamod::BotObservationServices &>(runtime);
	const auto &lifecycle = observations.service_lifecycle();
	const world::FrameIdentity useFrame{lifecycle.mapGeneration(), lifecycle.roundGeneration(),
		observations.service_adapterFrameCount()};
	auto staleUse = useFrame;
	++staleUse.tick;
	const world::ActorKey ctConnection{3U, lifecycle.slotGeneration(3U)};
	observations.reportNativeHostageUse(&ct, ctConnection, staleUse);
	observations.reportNativeHostageUse(&ct, {3U, ctConnection.generation + 1U}, useFrame);
	observations.reportNativeHostageUse(&gEntities[0], {1U, lifecycle.slotGeneration(1U)}, useFrame);
	advance(16);
	if (!check(!count(1, "radio/test/taken.wav"), "the native receipt must come from a current CT"))
		return false;
	observations.reportNativeHostageUse(&ct, ctConnection, {useFrame.mapGeneration, useFrame.roundGeneration,
		observations.service_adapterFrameCount()});
	advance(48);
	if (!check(count(1, "radio/test/taken.wav") == 1 && count(2, "radio/test/ack.wav") == 1,
			"a native follow receipt produces the report without a hostage voice or a log"))
		return false;

	globals.mapname = 34;
	globals.maxEntities = 8;
	hostage.v.origin = Vector(100, 0, 36);
	hostage.v.health = 100;
	gEntities[0].v.origin = Vector(0, 0, 36);
	gEntities[0].v.angles = Vector(0, 0, 0);
	gEntities[1].v.origin = Vector(0, 0, 36);
	runtime.beginRoundGeneration();
	advance(40);
	reportAudio.clear();
	hostage.v.origin.x += 50.0f;
	advance(32);
	if (!check(!count(1, "radio/test/lost.wav"), "a displacement of exactly 50 is within the native tolerance"))
		return false;
	hostage.v.origin.x += 0.25f;
	advance(40);
	if (!check(count(1, "radio/test/lost.wav") == 1 && !count(2, "radio/test/lost.wav") &&
			!count(2, "radio/test/ack.wav"),
			"the T who sees the old position reports the last missing hostage once without requesting a response"))
		return false;

	const char *path = "astrabot_hostage_guard.nav";
	if (!check(writeTwoSiteNavigation(path, true, true, "BombsiteA"), "hostage defense NAV fixture is written"))
		return false;
	const metamod::NavLoadRequest load{path, "astrabot_hostage_guard", runtime.snapshot().mapGeneration,
		true, 123U, false, 0U};
	const auto loaded = runtime.loadNavigationFile(&load);
	std::remove(path);
	if (!check(loaded == metamod::NavLoadResult::Loaded, "hostage defense uses the loaded NAV"))
		return false;
	gBombSite = {};
	gBombSite.v.classname = 35;
	gBombSite.v.solid = SOLID_TRIGGER;
	gBombSite.v.absmin = Vector(220, 27, -4);
	gBombSite.v.absmax = Vector(229, 38, 4);
	hostage.v.origin = Vector(112, 32, 36);
	for (auto i : {0, 1})
	{
		gEntities[i].v.origin = Vector(i == 0 ? 24.0f : 112.0f, 32, 36);
		gEntities[i].v.mins = Vector(-16, -16, -36);
		gEntities[i].v.maxs = Vector(16, 16, 36);
		gEntities[i].v.weapons = 0;
	}
	setCvarFloat("bot_allow_rogues", 0);
	setCvarFloat("mp_freezetime", 0);
	hostageGuardRolls = true;
	queueHelpRecording = true;
	runtime.onRoundLifecycleMessage("World triggered \"Round_End\"\n");
	runtime.onRoundLifecycleMessage("World triggered \"Round_Start\"\n");
	advance(40);
	auto &ports = static_cast<metamod::BotNavigationServices &>(runtime);
	using Task = compat::CompatibilityTaskId;
	const auto task = [&](int i) { return ports.behavior_managedBotStateMachines(i).task(); };
	if (!check(task(0) == Task::GuardHostages && task(1) == Task::GuardHostages,
			"before contact both Ts plan to guard the remembered hostage"))
		return false;
	ct.v.origin = Vector(24, 32, 36);
	hostageUseTargetVisible = true;
	const auto announceUse = [&] {
		reportAudio.clear();
		gEntities[1].v.origin.z += 1300;
		hooks.pfnAlertMessage(at_logged, firstTouch, 103);
		gEntities[1].v.origin.z -= 1300;
		advance(48);
	};
	announceUse();
	if (!check(count(2, "radio/test/ack.wav") == 1 && task(1) == Task::GuardHostages &&
			!count(2, "radio/test/rescueplan.wav"), "the safe period takes precedence over a completed report"))
		return false;
	advance(104);
	announceUse();
	if (!check(count(1, "radio/test/taken.wav") == 1 && count(2, "radio/test/ack.wav") == 1 &&
			count(2, "radio/test/rescueplan.wav") == 1 && task(1) == Task::GuardHostageRescueZone &&
			task(0) == Task::GuardHostages,
			"outside the safe period a teammate's completed report sends the recipient to guard the rescue zone"))
		return false;

	const auto resultMessage = [&](int destination, edict_t *recipient, int print, const char *token) {
		hooks.pfnMessageBegin(destination, 98, nullptr, recipient);
		hooks.pfnWriteByte(print);
		hooks.pfnWriteString(token);
		hooks.pfnMessageEnd();
	};
	resultMessage(MSG_ONE, &gEntities[0], HUD_PRINTCENTER, "#All_Hostages_Rescued");
	resultMessage(MSG_ALL, nullptr, HUD_PRINTCONSOLE, "#All_Hostages_Rescued");
	resultMessage(MSG_ALL, nullptr, HUD_PRINTCENTER, "#CTs_Win");
	advance(16);
	if (!check(task(0) == Task::GuardHostages, "only a centered broadcast of the rescue result can end a hostage guard"))
		return false;
	resultMessage(MSG_ALL, nullptr, HUD_PRINTCENTER, "#All_Hostages_Rescued");
	resultMessage(MSG_ALL, nullptr, HUD_PRINTCENTER, "#All_Hostages_Rescued");
	advance(16);
	if (!check(task(0) != Task::GuardHostages && task(1) == Task::GuardHostageRescueZone,
			"the global rescue result releases the hostage guard even after a duplicate CT result"))
		return false;
	runtime.onRoundLifecycleMessage("World triggered \"Round_End\"\n");
	advance(8);
	if (!check(task(0) != Task::GuardHostages, "the ending round does not resume guarding rescued hostages"))
		return false;
	runtime.onRoundLifecycleMessage("World triggered \"Round_Start\"\n");
	// Round_End prepares the generation, so the bots can already be in Hunt when it opens.
	for (int tick = 0; tick < 320 && (task(0) != Task::GuardHostages || task(1) != Task::GuardHostages); ++tick)
		advance(1);
	if (!check(task(0) == Task::GuardHostages && task(1) == Task::GuardHostages,
			"the first new-round replans may guard the new hostages"))
		return false;

	gEntities[1].v.origin.z += 1300;
	auto &behavior = static_cast<metamod::BotBehaviorServices &>(runtime);
	auto &machine = const_cast<compat::CompatibilityStateMachine &>(ports.behavior_managedBotStateMachines(0));
	const auto &handle = behavior.service_managedBotHandles()[0];
	const auto &life = behavior.service_lifecycle();
	compat::StateUpdateContext combat{};
	combat.actor = {handle.actor.slot, handle.actor.actorGeneration};
	combat.frame = {life.mapGeneration(), life.roundGeneration(), behavior.service_adapterFrameCount()};
	combat.timestamp = globals.time;
	combat.alive = combat.roundActive = true;
	combat.attackObservation = compat::ObservationAvailability::Available;
	if (!check(machine.beginAttack(combat) == compat::StateResult::Transitioned,
			"the T is attacking when the CT collects the hostage"))
		return false;
	const auto attackEpoch = machine.normalStateEpoch();
	hooks.pfnAlertMessage(at_logged, firstTouch, 103);
	if (!check(machine.attackOverlayActive() && machine.normalStateEpoch() == attackEpoch,
			"a visible hostage with unknown follow state retains the current attack"))
		return false;
	const auto voicedUse = [&] {
		pre.pfnPlayerPostThink(&ct);
		voice("hostage/huse/illfollow.wav", CHAN_VOICE, 0, 1.0f);
		post.pfnPlayerPostThink(&ct);
	};
	voicedUse();
	if (!check(!machine.attackOverlayActive() && task(0) == Task::GuardHostageRescueZone && task(1) == Task::GuardHostages,
			"the T who hears the last hostage accept the CT immediately switches to the rescue zone"))
		return false;
	const auto guardEpoch = ports.behavior_managedBotStateMachines(0).normalStateEpoch();
	voicedUse();
	gEntities[1].v.origin.z -= 1300;
	queueHelpRecording = false;
	return check(ports.behavior_managedBotStateMachines(0).normalStateEpoch() == guardEpoch,
		"already guarding the rescue zone does not restart the guard");
}

bool testSniperWarning(astrabot::metamod::PluginRuntime &runtime, enginefuncs_t &hooks, globalvars_t &globals)
{
	queueMode = "normal";
	queueEnemiesVisible = false;
	setCvarFloat("mp_buytime", 0);
	setCvarFloat("bot_stop", 0);
	runtime.setCompatibilityFloat("bot_quota", 2);
	if (!check(runtime.executeCompatibilityCommand(request("bot_add_t", 1U, "Queue Peer")) ==
			astrabot::metamod::CompatibilityCommandResult::Handled, "second T joins the sniper fixture"))
		return false;
	for (int i = 0; i < 5; ++i)
	{
		auto &ent = gEntities[i];
		ent.free = i > 2;
		ent.v.team = i < 2 ? 1 : 2;
		ent.v.flags = FL_CLIENT | FL_ONGROUND | (i < 2 ? FL_FAKECLIENT : 0);
		ent.v.health = 100;
		ent.v.deadflag = DEAD_NO;
		ent.v.solid = SOLID_SLIDEBOX;
		ent.v.movetype = MOVETYPE_WALK;
		ent.v.view_ofs = Vector(0, 0, 17);
		ent.v.origin = Vector(i < 2 ? 0.0f : 400.0f, 0, 36);
		ent.v.weapons = i == 2 ? (1 << 18) | (1 << 28) : 0;
		hooks.pfnMessageBegin(MSG_ALL, 86, nullptr, nullptr);
		hooks.pfnWriteByte(i + 1);
		hooks.pfnWriteString(i < 2 ? "TERRORIST" : "CT");
		hooks.pfnMessageEnd();
	}
	auto *table = gpGamedllFuncs->dllapi_table;
	const auto original = table->pfnUpdateClientData;
	table->pfnUpdateClientData = [](const edict_t *entity, int sendweapons, clientdata_t *data) {
		if (entity == &gEntities[2] && sendweapons == 1)
		{
			++sniperWeaponQueries;
			data->m_iId = activeSniperFixtureWeapon;
		}
	};
	struct RestoreClientData
	{
		DLL_FUNCTIONS *table;
		decltype(DLL_FUNCTIONS::pfnUpdateClientData) callback;
		~RestoreClientData() { table->pfnUpdateClientData = callback; }
	} restore{table, original};
	runtime.beginRoundGeneration();
	const auto advance = [&](int ticks) {
		for (int i = 0; i < ticks; ++i)
		{
			for (int slot : {0, 1})
				gEntities[slot].v.v_angle = gEntities[slot].v.angles = Vector(0, 0, 0);
			globals.time += .125f;
			runtime.onStartFrame();
			runtime.onStartFramePost();
		}
	};
	const auto warnings = [&](int speaker) {
		return std::count(reportAudio.begin(), reportAudio.end(),
			std::make_pair(speaker, std::string("radio/test/sniper.wav")));
	};
	reportAudio.clear();
	sniperViewer = 0;
	activeSniperFixtureWeapon = 28;
	advance(40);
	if (!check(sniperWeaponQueries > 0 && !warnings(1) && !warnings(2),
			"a visible enemy carrying an AWP but using another weapon is not reported"))
		return false;
	activeSniperFixtureWeapon = 18;
	sniperViewer = -1;
	const int lastQuery = sniperWeaponQueries;
	advance(24);
	if (!check(sniperWeaponQueries == lastQuery && !warnings(1), "an occluded sniper does not trigger the probe or a callout"))
		return false;
	sniperViewer = 0;
	advance(40);
	if (!check(warnings(1) == 1 && !warnings(2), "a visible active AWP starts one sniper warning"))
		return false;
	advance(104);
	if (!check(warnings(1) == 1, "the observer does not repeat the warning inside 20 seconds"))
		return false;
	advance(64);
	if (!check(warnings(1) == 2, "a sustained sighting can be reported after 20 seconds"))
		return false;
	sniperViewer = 1;
	advance(320);
	if (!check(!warnings(2), "a teammate who heard the warning suppresses their own reports"))
		return false;
	advance(320);
	return check(warnings(2) >= 1, "the teammate can warn again after the 60-second suppression");
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
	// NeedBackup first becomes eligible ten seconds after its -1 timestamp.
	globals.time = (std::max)(globals.time, 9.0f);
	runtime.beginRoundGeneration();
	hooks.pfnMessageBegin(MSG_ONE, 94, nullptr, &self);
	hooks.pfnWriteByte(1);
	hooks.pfnWriteByte(22);
	hooks.pfnWriteByte(30);
	hooks.pfnMessageEnd();
	step(.125f);
	step(.125f);
	if (friendlyFireChatterFixture)
	{
		queueMode = "normal";
		queueEnemiesVisible = false;
		setCvarFloat("bot_stop", 0);
		auto observer = std::make_unique<metamod::BotObservationRuntime>();
		observer->bindServices(runtime);
		self.v.netname = 100;
		gEntityNames[0] = "Friendly Fire Test Bot";
		engine.pfnFindEntityByString = [](edict_t *start, const char *key, const char *value) -> edict_t * {
			if (std::strcmp(key, "netname") == 0)
				return !start && std::strcmp(value, "Friendly Fire Test Bot") == 0 ? &gEntities[0] : nullptr;
			return findEntityByString(start, key, value);
		};
		int botObject = 0;
		const auto notify = [&](edict_t &inflictor) {
			inflictor.v.pContainingEntity = &inflictor;
			observer->observeGameDllDamage(&botObject, &inflictor.v, [&](const char *name) -> const void * {
				return std::strcmp(name, "Friendly Fire Test Bot") == 0 ? &botObject : nullptr;
			});
		};
		for (int frame = 0; frame < 8; ++frame)
			step(.125f);
		const char *sample = "radio/test/friendly_fire.wav";
		queueSamples.clear();
		friendlyFireDelayDraws = 0;
		notify(gEntities[1]);
		notify(gPlantedBomb);
		auto &teammate = gEntities[4];
		teammate.v.flags |= FL_FAKECLIENT;
		notify(teammate);
		teammate.v.flags &= ~FL_FAKECLIENT;
		if (!check(friendlyFireDelayDraws == 0, "enemy, non-player, and fake-client inflictors do not start a rebuke"))
			return false;
		queueFreeForAll.value = 1.0f;
		notify(teammate);
		queueFreeForAll.value = 0.0f;
		if (!check(friendlyFireDelayDraws == 0, "free for all treats equal-team players as enemies"))
			return false;
		self.v.deadflag = DEAD_DEAD;
		notify(teammate);
		self.v.deadflag = DEAD_NO;
		if (!check(friendlyFireDelayDraws == 1, "a dead bot still draws before native chatter rejects its admission"))
			return false;
		for (int frame = 0; frame < 5; ++frame)
			step(.125f);
		if (!check(std::count(queueSamples.begin(), queueSamples.end(), sample) == 0, "a dead bot's rebuke is not queued"))
			return false;
		friendlyFireDelayDraws = 0;
		notify(teammate);
		notify(teammate);
		if (!check(friendlyFireDelayDraws == 2, "each direct human teammate damage attempt draws a rebuke delay"))
			return false;
		step(.125f);
		step(.125f);
		if (!check(std::count(queueSamples.begin(), queueSamples.end(), sample) == 0, "rebuke waits for the drawn delay"))
			return false;
		for (int frame = 0; frame < 8; ++frame)
			step(.125f);
		return check(std::count(queueSamples.begin(), queueSamples.end(), sample) == 1,
			"same-frame duplicate rebukes play once through the chatter queue");
	}
	if (blindedChatterFixture)
	{
		queueMode = "normal";
		queueEnemiesVisible = false;
		setCvarFloat("bot_stop", 0);
		metamod::BotObservationRuntime observer;
		observer.bindServices(runtime);
		auto &observationService = static_cast<metamod::BotObservationServices &>(runtime);
		const auto &handle = observationService.service_managedBotHandles()[0];
		const world::ActorKey managed{handle.actor.slot, handle.actor.actorGeneration};
		const auto &life = observationService.service_lifecycle();
		const auto frame = [&] { return world::FrameIdentity{
			life.mapGeneration(), life.roundGeneration(), observationService.service_adapterFrameCount()}; };
		observer.onActorRemembered(0, handle);
		self.v.netname = 100;
		gEntityNames[0] = "Blind Test Bot";
		engine.pfnFindEntityByString = [](edict_t *start, const char *key, const char *value) -> edict_t * {
			if (std::strcmp(key, "netname") != 0)
				return findEntityByString(start, key, value);
			if (std::strcmp(value, "Blind Test Bot") != 0)
				return nullptr;
			return !start ? &gEntities[0] : duplicateBlindNetname ? &gEntities[1] : nullptr;
		};
		const char *sample = "radio/test/blinded.wav";
		const auto playUpdates = [&] {
			for (int update = 0; update < 5; ++update)
				step(.125f);
			return std::count(queueSamples.begin(), queueSamples.end(), sample);
		};
		int botObject = 0;
		int otherObject = 0;
		const auto notify = [&](const void *victim, bool reuseSlot = false, float duration = 1.25f)
		{
			const int serial = self.serialnumber;
			const auto complete = observer.prepareGameDllBlind(victim, duration, [&](const char *name) -> const void * {
				if (std::strcmp(name, "Blind Test Bot") != 0)
					return nullptr;
				if (reuseSlot)
					++self.serialnumber;
				return &botObject;
			});
			if (complete)
			{
				blindedDrawing = true;
				complete();
				blindedDrawing = false;
				blindedMoveDrawn = false;
			}
			self.serialnumber = serial;
		};
		for (int update = 0; update < 8; ++update)
			step(.125f);
		queueSamples.clear();
		blindedChatterRoll = 0.0f;
		hooks.pfnMessageBegin(MSG_ONE, 105, nullptr, &self);
		hooks.pfnWriteShort(4096);
		hooks.pfnWriteShort(1024);
		hooks.pfnWriteShort(0);
		for (int field = 0; field < 4; ++field)
			hooks.pfnWriteByte(255);
		hooks.pfnMessageEnd();
		if (!check(playUpdates() == 0, "an ordinary white screen fade does not report Blinded"))
			return false;
		notify(&otherObject);
		if (!check(playUpdates() == 0, "an opaque object for another player does not report Blinded"))
			return false;
		duplicateBlindNetname = true;
		notify(&botObject);
		duplicateBlindNetname = false;
		if (!check(playUpdates() == 0, "a duplicate public name does not select a blinded actor"))
			return false;
		notify(&botObject, true);
		if (!check(playUpdates() == 0, "edict reuse during lookup invalidates the blindness receipt"))
			return false;
		struct BlindTrace final : compat::IRandomTraceSink
		{
			void record(const compat::RandomTraceRecord &entry) override
			{
				if (entry.request.semanticId)
					draws.emplace_back(entry.request.semanticId);
			}
			std::vector<std::string> draws;
		} trace;
		auto &random = static_cast<metamod::BotBehaviorServices &>(runtime).service_compatibilityRandomSource();
		blindedChatterRoll = 100.0f;
		random.setTraceSink(&trace);
		notify(&botObject);
		random.setTraceSink(nullptr);
		if (!check(trace.draws == std::vector<std::string>{"Chatter.BlindedChance", "Blind.MoveDirection", "Blind.FireChance"},
				"accepted gameplay Blind draws chatter, relative movement, then blind fire"))
			return false;
		// Like the native expression, this comparison may keep the literal in x87 precision.
		volatile float nativeBoundaryRoll = 33.3f;
		blindedChatterRoll = nativeBoundaryRoll;
		const int nativeBoundaryCount = nativeBoundaryRoll < 33.3f ? 1 : 0;
		notify(&botObject);
		if (!check(playUpdates() == nativeBoundaryCount, "the float32 boundary matches the native literal comparison"))
			return false;
		for (int update = 0; update < 16; ++update)
			step(.125f);
		queueSamples.clear();
		blindedChatterRoll = 33.4f;
		notify(&botObject);
		if (!check(playUpdates() == 0, "a blindness roll above the native threshold does not report Blinded"))
			return false;
		blindedChatterRoll = 33.2f;
		notify(&botObject);
		if (!check(playUpdates() == 1, "an accepted blindness receipt plays Blinded once"))
			return false;

		observer.onActorRemembered(0, handle);
		const auto fade = [&] {
			observer.onMessageBegin(MSG_ONE, 105, nullptr, &self);
			observer.onWriteShort(4096);
			observer.onWriteShort(1024);
			observer.onWriteShort(0);
			for (int field = 0; field < 4; ++field)
				observer.onWriteByte(255);
			observer.onMessageEnd();
		};
		const auto &observations = observer.publicObservations();
		const auto blinded = [&](float at, bool compatibility = true) {
			return observations.isBlinded(managed, frame(), at, compatibility);
		};
		fade();
		if (!check(blinded(globals.time + .25f) && !blinded(globals.time + 1.25f),
				"without a GameDLL hook the standard fallback uses the HUD deadline"))
			return false;
		blindedChatterRoll = 100.0f;
		notify(&botObject, false, .25f);
		const float deadline = globals.time + .25f;
		if (!check(blinded(std::nextafter(deadline, 0.0f)) && !blinded(deadline) && blinded(deadline, false),
				"the accepted GameDLL duration controls the gameplay deadline with strict expiry"))
			return false;
		notify(&botObject, false, 2.0f);
		globals.time += .125f;
		fade();
		notify(&botObject, false, .125f);
		fade();
		if (!check(blinded(globals.time) && !blinded(globals.time + .125f) && blinded(globals.time + .125f, false),
				"a shorter overlapping Blind replaces gameplay time and later HUD fades affect only the HUD timer"))
			return false;
		notify(&botObject, true, 4.0f);
		if (!check(!blinded(globals.time + .125f), "a reused edict cannot extend the recorded gameplay deadline"))
			return false;

		notify(&botObject, false, 2.0f);
		observer.onMessageBegin(MSG_ONE, 104, nullptr, &self);
		observer.onMessageEnd();
		if (!check(blinded(globals.time + .125f) && !blinded(globals.time, false),
				"a same-life ResetHUD clears the HUD deadline and preserves gameplay blindness"))
			return false;

		engine.pfnPvEntPrivateData = [](edict_t *entity) -> void * { return entity->pvPrivateData; };
		self.pvPrivateData = &botObject;
		duplicateBlindNetname = true;
		const auto stale = observer.prepareGameDllBlind(&botObject, 4.0f, {});
		if (!check(static_cast<bool>(stale), "opaque actor identity resolves independently of duplicate public names"))
			return false;
		observer.observeGameDllBotEvent(metamod::NativeBotEvent::PlayerSpawned, &botObject);
		stale();
		self.v.deadflag = DEAD_DEAD;
		notify(&botObject, false, 4.0f);
		self.v.deadflag = DEAD_NO;
		if (!check(!blinded(globals.time) && !blinded(globals.time, false),
				"spawn clears gameplay blindness, invalidates a pending receipt, and a dead actor cannot receive one"))
			return false;

		// The fixture observer uses the same receipt bridge but owns a separate observation store.
		auto &liveObservations = const_cast<compat::PublicObservationStore &>(
			static_cast<metamod::BotBehaviorServices &>(runtime).observation_publicObservations());
		const auto flash = [&](float duration, int direction, float fire) {
			blindedMoveRoll = direction;
			blindedFireRoll = fire;
			notify(&botObject, false, duration);
			return liveObservations.gameDllBlind(managed, frame(), globals.time + duration);
		};
		duplicateBlindNetname = false;
		self.v.maxspeed = 250.0f;
		if (!check(flash(.5f, 1, 9.0f), "real blind fixture publishes a current gameplay deadline"))
			return false;
		step(.125f);
		if (!check(blindFixtureInput.forward == 0 && blindFixtureInput.side == 250 &&
				blindFixtureInput.buttons == (IN_MOVERIGHT | IN_ATTACK), "blind right strafe and fire share a full command"))
			return false;
		const auto fullCommands = blindFixtureInput.count;
		step(.04f);
		if (!check(blindFixtureInput.count == fullCommands + 1U &&
				blindFixtureInput.forward == 0 && blindFixtureInput.side == 250 &&
				blindFixtureInput.buttons == (IN_MOVERIGHT | IN_ATTACK), "the light command retains local blind input"))
			return false;
		if (!flash(.5f, 5, 10.0f))
			return false;
		step(.125f);
		if (!check(blindFixtureInput.forward == 0 && blindFixtureInput.side == 0 &&
				blindFixtureInput.buttons == IN_DUCK, "a second flash replaces retreat and the exact fire threshold misses"))
			return false;
		if (!flash(.25f, 2, 100.0f))
			return false;
		step(.125f);
		if (!check(blindFixtureInput.forward == -250 && blindFixtureInput.side == 0 &&
				blindFixtureInput.buttons == (IN_BACK | IN_DUCK), "a subsequent retreat keeps the blind crouch posture"))
			return false;
		step(.125f);
		if (!check(blindFixtureInput.buttons != (IN_BACK | IN_DUCK), "exact expiry resumes the ordinary full update"))
			return false;
		if (!flash(.16f, 3, 9.0f))
			return false;
		step(.125f);
		const auto leftCommand = blindFixtureInput;
		step(.04f);
		if (!check(leftCommand.side < 0 && (leftCommand.buttons & (IN_MOVELEFT | IN_ATTACK)) == (IN_MOVELEFT | IN_ATTACK) &&
				blindFixtureInput.count == leftCommand.count + 1 && blindFixtureInput.side == leftCommand.side &&
				blindFixtureInput.buttons == leftCommand.buttons, "crossing the deadline on a light update holds the last axes"))
			return false;
		liveObservations.gameDllSpawn(managed, frame());
		step(.04f);
		if (!check(blindFixtureInput.buttons != leftCommand.buttons, "a new life cannot replay the previous flash command"))
			return false;

		trace.draws.clear();
		blindedMoveMutation = [&] { liveObservations.gameDllSpawn(managed, frame()); };
		random.setTraceSink(&trace);
		notify(&botObject);
		random.setTraceSink(nullptr);
		if (!check(trace.draws == std::vector<std::string>{"Chatter.BlindedChance", "Blind.MoveDirection"},
				"a spawn during the movement draw invalidates the rest of the Blind receipt"))
			return false;
		gBotStopCvar.value = 1.0f;
		if (!flash(.25f, 1, 0.0f))
			return false;
		step(.125f);
		if (!check(blindFixtureInput.forward == 0 && blindFixtureInput.side == 0 &&
				!(blindFixtureInput.buttons & IN_ATTACK), "bot_stop takes precedence over blind retreat and fire"))
			return false;

		compat::PublicObservationStore connected;
		connected.setGameDllBlindEventsAvailable(true);
		connected.bind(managed, frame());
		(void)connected.screenFade(managed, frame(), globals.time + 1.25f);
		return check(!connected.isBlinded(managed, frame(), globals.time, true) &&
				connected.isBlinded(managed, frame(), globals.time, false),
			"a connected GameDLL hook keeps ScreenFade independent before the first gameplay receipt");
	}
	if (barelyDefusedFixture)
	{
		queueMode = "normal";
		queueEnemiesVisible = false;
		setCvarFloat("bot_stop", 0);
		const char *sample = "radio/test/barely_defused.wav";
		const auto notice = [&](const char *token)
		{
			hooks.pfnMessageBegin(MSG_ALL, 98, nullptr, nullptr);
			hooks.pfnWriteByte(HUD_PRINTCENTER);
			hooks.pfnWriteString(token);
			hooks.pfnMessageEnd();
		};
		enum class Restart { Later, ResetHud, RoundStart };
		const auto run = [&](bool sawPlant, float elapsed, const char *token, Restart restart, int expected,
			const char *message)
		{
			runtime.onRoundLifecycleMessage("World triggered \"Round_Start\"\n");
			self.v.team = 2;
			team(1, "CT");
			team(2, "CT");
			for (int frame = 0; frame < 8; ++frame)
				step(.125f);
			queueSamples.clear();
			if (sawPlant)
				notice("#Bomb_Planted");
			globals.time += elapsed;
			notice(token);
			runtime.onRoundLifecycleMessage("World triggered \"Round_End\"\n");
			if (restart == Restart::ResetHud)
			{
				hooks.pfnMessageBegin(MSG_ONE, 104, nullptr, &self);
				hooks.pfnMessageEnd();
			}
			else if (restart == Restart::RoundStart)
				runtime.onRoundLifecycleMessage("World triggered \"Round_Start\"\n");
			for (int frame = 0; frame < 20; ++frame)
				step(.125f);
			return check(std::count(queueSamples.begin(), queueSamples.end(), sample) == expected, message);
		};
		return run(false, 68.25f, "#Bomb_Defused", Restart::Later, 0,
				"defuse without a known plant time does not report BarelyDefused") &&
			run(true, 68.0f, "#Bomb_Defused", Restart::Later, 0, "exactly two seconds left does not report BarelyDefused") &&
			run(true, 68.25f, "#Target_Bombed", Restart::Later, 0, "an explosion does not report BarelyDefused") &&
			run(true, 68.25f, "#Bomb_Defused", Restart::Later, 1,
				"late defuse is eligible for playback on the first update after Round_End") &&
			run(true, 68.25f, "#Bomb_Defused", Restart::ResetHud, 0, "respawn clears pending BarelyDefused") &&
			run(true, 68.25f, "#Bomb_Defused", Restart::RoundStart, 0, "old BarelyDefused does not cross Round_Start");
	}
	if (autoFollowChatterFixture)
	{
		queueMode = "normal";
		queueEnemiesVisible = true;
		setCvarFloat("bot_stop", 0);
		setCvarFloat("mp_buytime", 0);
		setCvarFloat("mp_freezetime", 0);
		for (int i = 1; i < 4; ++i)
			gEntities[i].free = true;
		auto &leader = gEntities[4];
		leader.v.origin = Vector(120, 0, 36);
		leader.v.netname = 104;
		gEntityNames[4] = "Human leader";
		const auto run = [&](bool career, const char *expected, const char *other) {
			queueCareer = career;
			runtime.onRoundLifecycleMessage("World triggered \"Round_End\"\n");
			runtime.onRoundLifecycleMessage("World triggered \"Round_Start\"\n");
			queueSamples.clear();
			auto &machine = static_cast<metamod::BotNavigationServices &>(runtime).behavior_managedBotStateMachines(0);
			for (int frame = 0; frame < 32; ++frame)
				step(.125f);
			if (!check(machine.state() != compat::CompatibilityStateId::Follow &&
					std::count(queueSamples.begin(), queueSamples.end(), expected) == 0,
					"auto-follow and its speech wait until after the first five seconds"))
				return false;
			for (int frame = 0; frame < 72; ++frame)
				step(.125f);
			if (!check(machine.state() == compat::CompatibilityStateId::Follow,
					"a reachable visible human is followed automatically"))
				return false;
			return check(std::count(queueSamples.begin(), queueSamples.end(), expected) == 1 &&
					std::count(queueSamples.begin(), queueSamples.end(), other) == 0,
					"automatic follow speaks the match-appropriate intention exactly once");
		};
		return run(false, "radio/test/follow_sir.wav", "radio/test/follow_commander.wav") &&
			run(true, "radio/test/follow_commander.wav", "radio/test/follow_sir.wav");
	}
	if (victoryFixture)
	{
		queueMode = "normal";
		queueEnemiesVisible = false;
		setCvarFloat("bot_stop", 0);
		engine.pfnRandomFloat = [](float low, float) { return low; };
		const auto advance = [&](unsigned frames) {
			for (unsigned frame = 0; frame < frames; ++frame)
				step(.125f);
		};
		const auto win = [&](const char *token) {
			hooks.pfnMessageBegin(MSG_ALL, 98, nullptr, nullptr);
			hooks.pfnWriteByte(HUD_PRINTCENTER);
			hooks.pfnWriteString(token);
			hooks.pfnMessageEnd();
		};
		auto &friendPlayer = gEntities[4];
		const auto run = [&](bool longRound, bool friendAlive, const char *sample)
		{
			runtime.onRoundLifecycleMessage("World triggered \"Round_Start\"\n");
			friendPlayer.v.health = friendAlive ? 100.0f : 0.0f;
			friendPlayer.v.deadflag = friendAlive ? DEAD_NO : DEAD_DEAD;
			if (longRound)
				step(45.0f);
			else
				advance(4);
			queueSamples.clear();
			win("#Terrorists_Win");
			win("#Terrorists_Win");
			runtime.onRoundLifecycleMessage("World triggered \"Round_End\"\n");
			advance(15);
			if (!check(std::find(queueSamples.begin(), queueSamples.end(), sample) == queueSamples.end(),
					"victory waits until the drawn two-second start"))
				return false;
			advance(10);
			return check(std::count(queueSamples.begin(), queueSamples.end(), sample) == 1,
				"the elected victory emote crosses Round_End and plays once");
		};
		if (!run(false, false, "radio/test/win_fast.wav") ||
			!run(true, false, "radio/test/win_alone.wav") ||
			!run(true, true, "radio/test/win_team.wav"))
			return false;
		runtime.onRoundLifecycleMessage("World triggered \"Round_Start\"\n");
		queueSamples.clear();
		win("#Terrorists_Win");
		runtime.onRoundLifecycleMessage("World triggered \"Round_End\"\n");
		hooks.pfnMessageBegin(MSG_ONE, 104, nullptr, &self);
		hooks.pfnMessageEnd();
		advance(25);
		if (!check(std::find(queueSamples.begin(), queueSamples.end(), "radio/test/win_fast.wav") ==
				queueSamples.end(), "player ResetHUD ends the pending victory emote"))
			return false;
		runtime.onRoundLifecycleMessage("World triggered \"Round_Start\"\n");
		self.v.health = 0;
		self.v.deadflag = DEAD_DEAD;
		queueSamples.clear();
		win("#Terrorists_Win");
		runtime.onRoundLifecycleMessage("World triggered \"Round_End\"\n");
		advance(25);
		return check(std::find(queueSamples.begin(), queueSamples.end(), "radio/test/win_fast.wav") ==
				queueSamples.end(), "a dead winner does not register a victory emote");
	}
	if (fallBackFixture)
	{
		queueMode = "normal";
		queueEnemiesVisible = false;
		setCvarFloat("bot_stop", 0);
		setCvarFloat("mp_buytime", 0);
		self.v.origin = Vector(80, 32, 36);
		const auto loadRetreat = [&](bool cover)
		{
			const char *path = "astrabot_radio_fallback.nav";
			if (!writeTwoSiteNavigation(path, true, cover))
				return false;
			const metamod::NavLoadRequest load{path, "astrabot_radio_fallback", runtime.snapshot().mapGeneration,
				false, 0U, false, 0U};
			const auto loaded = runtime.loadNavigationFile(&load);
			std::remove(path);
			return loaded == metamod::NavLoadResult::Loaded;
		};
		const auto order = [&] {
			hooks.pfnMessageBegin(MSG_ONE, 101, nullptr, &self);
			hooks.pfnWriteByte(5);
			hooks.pfnWriteString("%!MRAD_FALLBACK");
			hooks.pfnWriteShort(100);
			hooks.pfnMessageEnd();
		};
		const auto advance = [&](unsigned count) {
			for (unsigned tick = 0; tick < count; ++tick)
				step(.125f);
		};
		auto &ports = static_cast<metamod::BotNavigationServices &>(runtime);
		const auto &machine = ports.behavior_managedBotStateMachines(0);
		const auto ack = [&] { return std::count(queueSamples.begin(), queueSamples.end(), "radio/test/ack.wav"); };
		if (!check(loadRetreat(false), "Team Fall Back starts on a NAV with no hiding spots"))
			return false;
		advance(40);
		queueSamples.clear();
		order();
		advance(32);
		if (!check(machine.state() != compat::CompatibilityStateId::Hide && ack() == 0,
				"an order with no safe spot is consumed without an acknowledgement"))
			return false;
		if (!check(loadRetreat(true), "Team Fall Back loads a connected NAV with hiding spots"))
			return false;
		advance(24);
		if (!check(machine.state() != compat::CompatibilityStateId::Hide && ack() == 0,
				"loading cover does not replay the consumed order"))
			return false;
		order();
		advance(7);
		if (!check(machine.state() != compat::CompatibilityStateId::Hide && ack() == 0,
				"the public radio waits through the reaction delay"))
			return false;
		advance(25);
		const auto &retreat = ports.behavior_scenarioDecisions(0);
		return check(machine.state() == compat::CompatibilityStateId::Hide && retreat.hasTarget &&
				retreat.target.position.x == 32.0f && retreat.target.position.y == 50.0f && ack() == 1,
			"human Team Fall Back selects a safe point and acknowledges once after entering Hide");
	}
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
	if (hostageUseFixture)
		return testHostageUseReport(runtime, hooks, globals);
	if (sniperWarningFixture)
		return testSniperWarning(runtime, hooks, globals);
	if (damageRetargetFixture)
	{
		setCvarFloat("bot_stop", 0);
		setCvarFloat("mp_buytime", 0);
		gEntities[1].v.origin = Vector(140, 0, 36);
		gEntities[2].v.origin = Vector(230, 30, 36);
		gEntities[3].free = gEntities[4].free = true;
		static int visibleSlot = 1;
		static bool hitVisiblePlayer = false;
		engine.pfnTraceLine = [](const float *, const float *end, int mode, edict_t *, TraceResult *trace) {
			*trace = {};
			trace->vecEndPos = Vector(end[0], end[1], end[2]);
			trace->flFraction = std::fabs(end[0] - gEntities[visibleSlot].v.origin.x) < 24.0f ? 1.0f : 0.0f;
			if (hitVisiblePlayer && mode == 0 && trace->flFraction == 1.0f)
				trace->pHit = &gEntities[visibleSlot];
		};
		auto &behavior = static_cast<metamod::BotNavigationServices &>(runtime);
		auto &observation = static_cast<metamod::BotObservationServices &>(runtime);
		const auto acquired = [&] { return observation.behavior_currentEnemySource(0).id; };
		const auto faceForward = [&] { self.v.angles = self.v.v_angle = Vector(0, 0, 0); };
		for (int frame = 0; frame < 24; ++frame)
		{
			faceForward();
			step(.125f);
		}
		if (!check(acquired() == 2 && behavior.behavior_managedBotStateMachines(0).attackOverlayActive(),
				"the bot is already attacking the first visible CT"))
			return false;
		self.v.netname = 100;
		gEntityNames[0] = "Retarget Test Bot";
		engine.pfnFindEntityByString = [](edict_t *start, const char *key, const char *value) -> edict_t * {
			if (std::strcmp(key, "netname") == 0)
				return !start && std::strcmp(value, "Retarget Test Bot") == 0 ? &gEntities[0] : nullptr;
			return findEntityByString(start, key, value);
		};
		auto observer = std::make_unique<metamod::BotObservationRuntime>();
		observer->bindServices(runtime);
		int botObject = 0;
		const auto damage = [&](edict_t &attacker) {
			attacker.v.pContainingEntity = &attacker;
			observer->observeGameDllDamage(&botObject, &attacker.v, [&](const char *name) -> const void * {
				return std::strcmp(name, "Retarget Test Bot") == 0 ? &botObject : nullptr;
			});
		};
		faceForward();
		damage(gEntities[2]);
		step(.125f);
		if (!check(acquired() == 2, "a visible current enemy retains priority over a hidden attacker"))
			return false;
		faceForward();
		visibleSlot = 2;
		step(.125f);
		if (!check(acquired() == 3,
				"the visible recent attacker is selected before the ordinary reaction queue recognizes it"))
			return false;
		gEntities[1].v.team = 1;
		team(2, "TERRORIST");
		for (int frame = 0; frame < 12; ++frame)
		{
			faceForward();
			step(.125f);
		}
		visibleSlot = 1;
		faceForward();
		damage(gEntities[1]);
		step(.125f);
		if (!check(acquired() == 3, "team mode keeps a visible teammate out of damage retargeting"))
			return false;
		queueFreeForAll.value = 1.0f;
		faceForward();
		damage(gEntities[1]);
		step(.125f);
		if (!check(acquired() == 2, "FFA damage retargets a visible same-team inflictor"))
			return false;
		hitVisiblePlayer = true;
		bool fired = false;
		for (int frame = 0; frame < 12 && !fired; ++frame)
		{
			faceForward();
			gLastRunPlayerMoveButtons = 0U;
			step(.125f);
			fired = gLastRunPlayerMoveActor == 1 && (gLastRunPlayerMoveButtons & IN_ATTACK) != 0;
		}
		return check(fired, "FFA allows firing at the selected inflictor when the trace hits its same-team edict");
	}
	if (scaredFixture)
	{
		queueMode = "normal";
		queueEnemiesVisible = false;
		setCvarFloat("mp_buytime", 0);
		setCvarFloat("bot_stop", 0);
		// The existing lone managed T fights two visible CTs.
		for (int slot = 1; slot < 5; ++slot)
		{
			gEntities[slot].free = slot > 2;
			gEntities[slot].v.team = 2;
			gEntities[slot].v.origin = Vector(400, (slot - 1) * 48.0f, 36);
			team(slot + 1, "CT");
		}
		runtime.beginRoundGeneration();
		for (int i = 0; i < 24; ++i)
			step(.125f);
		hooks.pfnMessageBegin(MSG_ONE, 94, nullptr, &self);
		hooks.pfnWriteByte(1);
		hooks.pfnWriteByte(22);
		hooks.pfnWriteByte(30);
		hooks.pfnMessageEnd();
		queueSamples.clear();
		queueEnemiesVisible = true;
		for (int i = 0; i < 48; ++i)
		{
			self.v.angles = self.v.v_angle = Vector(0, 0, 0);
			step(.125f);
		}
		return check(std::count(queueSamples.begin(), queueSamples.end(), "radio/test/scared.wav") == 1 &&
				std::count(queueSamples.begin(), queueSamples.end(), "radio/test/help.wav") == 0,
			"the last living T voices fear once instead of asking for backup");
	}
	if (const char *map = bombFixtureMap())
	{
		queueMode = "normal";
		queueEnemiesVisible = false;
		setCvarFloat("bot_stop", 0);
		setCvarFloat("mp_buytime", 0);
		runtime.setCompatibilityFloat("bot_quota", 2);
		if (runtime.executeCompatibilityCommand(request("bot_add_ct", 1U, "Queue Peer")) !=
			metamod::CompatibilityCommandResult::Handled)
			return false;
		globals.mapname = 6;
		globals.maxEntities = 9;
		gObjectiveEntitiesAvailable = true;
		gBombSite = gSecondBombSite = gPlantedBomb = {};
		gBombSite.v.classname = gSecondBombSite.v.classname = 1;
		gBombSite.v.absmin = Vector(16, 24, -4);
		gBombSite.v.absmax = Vector(32, 40, 4);
		gSecondBombSite.v.absmin = Vector(216, 24, -4);
		gSecondBombSite.v.absmax = Vector(232, 40, 4);
		for (int i = 0; i < 2; ++i)
		{
			auto &bot = gEntities[i];
			bot.free = false;
			bot.v.flags = FL_CLIENT | FL_FAKECLIENT | FL_ONGROUND;
			bot.v.health = 100;
			bot.v.deadflag = DEAD_NO;
			bot.v.team = 2;
			bot.v.movetype = MOVETYPE_WALK;
			bot.v.solid = SOLID_SLIDEBOX;
			bot.v.maxspeed = 250;
			bot.v.origin = Vector(110, 32, 36);
			team(i + 1, "CT");
		}
		const std::string path = std::string(map) + ".nav";
		if (!writeTwoSiteNavigation(path.c_str(), true, looseGuardFixture || deferredSiteFixture || preplantGuardFixture, "BombsiteA"))
			return false;
		const metamod::NavLoadRequest load{path.c_str(), map, runtime.snapshot().mapGeneration,
			true, 123U, false, 0U};
		const auto loaded = runtime.loadNavigationFile(&load);
		std::remove(path.c_str());
		if (!check(loaded == metamod::NavLoadResult::Loaded, "bombsite test navigation loads"))
			return false;
		if (looseGuardFixture || preplantGuardFixture)
			setCvarFloat("bot_allow_rogues", 0);
		if (deferredSiteFixture)
		{
			setCvarFloat("bot_defer_to_human", 1);
			gEntityNames[2] = "Human CT";
			gEntities[2].v.netname = 102;
			gEntityNames[4] = "Human T";
			gEntities[4].v.netname = 104;
		}
		runtime.beginRoundGeneration();
		for (int i = 0; i < 40; ++i)
			step(.125f);
		if (preplantGuardFixture)
		{
			auto &nav = static_cast<metamod::BotNavigationServices &>(runtime);
			auto guarding = [&](int i) {
				const auto &machine = nav.behavior_managedBotStateMachines(i);
				return machine.state() == compat::CompatibilityStateId::Hide &&
					machine.task() == compat::CompatibilityTaskId::GuardBombZone;
			};
			auto advance = [&](int ticks) {
				for (int i = 0; i < ticks; ++i)
				{
					gEntities[0].v.angles.y = gEntities[0].v.v_angle.y = 0;
					step(.125f);
				}
			};
			auto roundStart = [&] { runtime.onRoundLifecycleMessage("World triggered \"Round_Start\"\n"); };
			auto resultMessage = [&](int destination, edict_t *recipient, const char *token) {
				hooks.pfnMessageBegin(destination, 98, nullptr, recipient);
				hooks.pfnWriteByte(HUD_PRINTCENTER);
				hooks.pfnWriteString(token);
				hooks.pfnMessageEnd();
			};
			auto restart = [&](float rushRoll) {
				teamRushRoll = rushRoll;
				chooseTeamRush = true;
				runtime.onRoundLifecycleMessage("World triggered \"Round_End\"\n");
				roundStart();
				advance(44);
			};
			auto loseAndRestart = [&](float rushRoll) {
				resultMessage(MSG_ALL, nullptr, "#Terrorists_Win");
				resultMessage(MSG_ALL, nullptr, "#Terrorists_Win");
				restart(rushRoll);
			};
			if (waitingHereFixture)
			{
				struct WaitingHereTrace : compat::IRandomTraceSink
				{
					explicit WaitingHereTrace(const globalvars_t &source) : clock(source) {}
					void record(const compat::RandomTraceRecord &record) override
					{
						if (time < 0.0f && record.request.actor.slot == 1 &&
							std::strcmp(record.request.semanticId, "Hunt.WaitingHereChance") == 0)
							time = clock.time;
					}
					const globalvars_t &clock;
					float time = -1.0f;
				} trace(globals);
				auto &services = static_cast<metamod::BotBehaviorServices &>(runtime);
				const auto &machine = nav.behavior_managedBotStateMachines(0);
				const auto &profile = nav.behavior_managedProfiles(0);
				const float safeTime = 15.0f + 5.0f * (static_cast<float>(profile.aggression) / 100.0f);
				const auto reach = [&](float elapsed) {
					const auto start = services.service_roundFreezeUntil();
					while (globals.time - start < elapsed)
						advance(1);
				};
				preplantIdleRoll = 100.0f;
				restart(100.0f);
				loseAndRestart(100.0f);
				loseAndRestart(33.3f);
				preplantIdleRoll = 0.0f;
				// The shared harness may already have taught this bot a shorter safe
				// time. Observe the first qualifying roll in the rushing round.
				services.service_compatibilityRandomSource().setTraceSink(&trace);
				reach(safeTime - .75f);
				services.service_compatibilityRandomSource().setTraceSink(nullptr);
				const float firstRoll = trace.time - services.service_roundFreezeUntil();
				if (!check(trace.time >= 0.0f && machine.state() == compat::CompatibilityStateId::Hunt,
						"a rushing defense keeps hunting despite low morale and a winning roll"))
					return false;

				preplantIdleRoll = 100.0f;
				restart(100.0f);
				preplantIdleRoll = 0.0f;
				reach(firstRoll - .25f);
				if (!check(machine.state() == compat::CompatibilityStateId::Hunt,
						"the wait cannot begin before the final two seconds of safety"))
				{
					const auto start = services.service_roundFreezeUntil();
					std::fprintf(stderr, "waiting-here fixture state=%d task=%d elapsed=%g entered=%g safe=%g\n",
						static_cast<int>(machine.state()), static_cast<int>(machine.task()),
						globals.time - start, machine.stateTimestamp() - start, safeTime);
					return false;
				}
				preplantIdleRoll = 40.0f;
				reach(firstRoll + .375f);
				if (!check(machine.state() == compat::CompatibilityStateId::Hunt,
						"a roll of 40 does not pass negative-one morale's strict threshold"))
					return false;

				preplantIdleRoll = 39.0f;
				queueSamples.clear();
				advance(4);
				if (!check(machine.state() == compat::CompatibilityStateId::Hide &&
						machine.task() == compat::CompatibilityTaskId::SeekAndDestroy,
						"low-morale hunters take cover in the final safe window"))
					return false;
				advance(9);
				const auto &defense = nav.behavior_scenarioDecisions(0);
				return check(defense.action == compat::ScenarioAction::Guard && defense.hasTarget,
						"the transitioned hunter keeps its selected hiding spot") &&
					check(std::find(queueSamples.begin(), queueSamples.end(), "radio/test/waiting_here.wav") !=
						queueSamples.end(), "the transition to cover plays WaitingHere");
			}
			teamRushRoll = 100.0f;
			chooseTeamRush = true;
			roundStart();
			advance(44);
			if (!check(!guarding(0) && !guarding(1), "fresh positive morale does not choose bombsite guard duty"))
				return false;
			resultMessage(MSG_ONE, &gEntities[0], "#Terrorists_Win");
			resultMessage(MSG_ALL, nullptr, "#Round_Draw");
			loseAndRestart(100.0f);
			if (!check(!guarding(0) && !guarding(1), "private, draw and duplicate result messages do not compound one defeat"))
				return false;
			loseAndRestart(33.3f);
			if (!check(!guarding(0) && !guarding(1), "both low-morale defenders respect the same team rush"))
				return false;
			preplantIdleRoll = 34.0f;
			restart(100);
			if (!check(!guarding(0) && !guarding(1), "the guard roll must be strictly below 34 at negative-one morale"))
				return false;
			preplantIdleRoll = 33.99f;
			restart(100);
			if (!check(guarding(0) && guarding(1), "a roll just below 34 starts both guards at negative-one morale"))
				return false;
			preplantIdleRoll = 0;
			reportAudio.clear();
			loseAndRestart(100.0f);
			if (!check(guarding(0) && guarding(1), "both CTs choose a preplant bombsite hide after losses on a non-rush round"))
				return false;
			const auto &guard = nav.behavior_scenarioDecisions(0);
			if (!check(guard.action == compat::ScenarioAction::Guard && guard.hasTarget && guard.target.position.y == 50,
					"site guard moves to a NAV hiding spot"))
				return false;
			const auto heard = [&](int speaker, const char *wave) {
				return std::count(reportAudio.begin(), reportAudio.end(), std::make_pair(speaker, std::string(wave)));
			};
			if (!check(heard(1, "radio/test/siteplan.wav") + heard(2, "radio/test/siteplan.wav") +
					heard(1, "radio/test/siteplan_a.wav") + heard(2, "radio/test/siteplan_a.wav") > 0,
					"the defenders announce their bombsite plan"))
				return false;
			gPlantedBomb.v.classname = 4;
			gPlantedBomb.v.model = 5;
			gPlantedBomb.v.origin = Vector(224, 32, 36);
			gPlantedBomb.serialnumber = 12;
			reportAudio.clear();
			gEntities[1].v.weapons |= (1 << 18);
			carrierViewer = 0;
			for (int i = 0; i < 24 && guarding(0); ++i)
				advance(1);
			if (!check(!guarding(0) && guarding(1), "direct sight releases only the CT that saw the dropped C4"))
				return false;
			advance(64);
			if (!check(!guarding(1) && heard(1, "radio/test/loose.wav") == 1 &&
				heard(2, "radio/test/ack.wav") == 1 && !heard(1, "radio/test/ack.wav"),
				"the other site guard leaves and acknowledges the completed loose-bomb report exactly once"))
				return false;
			const auto &peer = nav.behavior_managedBotStateMachines(1);
			const auto target = nav.behavior_scenarioDecisions(1).target.position;
			if (!check(peer.task() == compat::CompatibilityTaskId::MoveToSniperSpot && target.x == 224 && target.y == 50,
					"the receiver carries a sniper rifle and chooses cover by the reported drop"))
				return false;
			gEntities[1].v.origin = Vector(target.x, target.y, target.z + 36);
			advance(3);
			if (!check(peer.task() == compat::CompatibilityTaskId::Sniping, "arrival at the hiding spot changes the sniper's task"))
				return false;
			carrierViewer = -1;
			gPlantedBomb.free = true;
			setCvarFloat("bot_excellent_morale", 1);
			preplantIdleRoll = 75.0f;
			restart(0);
			if (!check((peer.task() == compat::CompatibilityTaskId::MoveToSniperSpot || peer.task() == compat::CompatibilityTaskId::Sniping) &&
					!guarding(0), "a sniper's inclusive 75 percent decision precedes the team's rush"))
				return false;
			preplantIdleRoll = 75.01f;
			restart(100);
			if (!check(!guarding(0) && !guarding(1) && peer.task() != compat::CompatibilityTaskId::MoveToSniperSpot &&
					peer.task() != compat::CompatibilityTaskId::Sniping, "excellent morale and a failed sniper roll both choose Hunt"))
				return false;
			gEntities[1].v.weapons &= ~(1 << 18);
			preplantIdleRoll = 0;
			setCvarFloat("bot_excellent_morale", 0);
			restart(100);
			if (!check(guarding(0) && guarding(1), "disabling excellent morale restores the defenders' actual loss history"))
				return false;
			teamRushRoll = 100;
			chooseTeamRush = true;
			runtime.onRoundLifecycleMessage("World triggered \"Game_Commencing\"\n");
			globals.time += 3;
			roundStart();
			advance(44);
			return check(!guarding(0) && !guarding(1), "a new match returns both defenders to positive morale");
		}
		if (looseGuardFixture)
		{
			auto &nav = static_cast<metamod::BotNavigationServices &>(runtime);
			const auto &first = nav.behavior_managedBotStateMachines(0);
			auto guarding = [&]() { return first.task() == compat::CompatibilityTaskId::GuardLooseBomb; };
			auto sample = [&](const char *name) {
				return std::find(reportAudio.begin(), reportAudio.end(), std::make_pair(1, std::string(name)));
			};
			gPlantedBomb.v.classname = 4;
			gPlantedBomb.v.model = 5;
			gPlantedBomb.v.origin = Vector(224, 32, 36);
			gPlantedBomb.serialnumber = 12;
			enginefuncs_t post{};
			int postVersion = ENGINE_INTERFACE_VERSION;
			if (!check(runtime.provideEngineFunctionsPost(&post, &postVersion) && post.pfnSetModel,
					"the native C4 drop has a public weaponbox model receipt"))
				return false;
			post.pfnSetModel(&gPlantedBomb, "models/w_backpack.mdl");
			// The box slides out of BombsiteA before the first CT can see it.
			gPlantedBomb.v.origin.x = 165;
			reportAudio.clear();
			for (int i = 0; i < 24; ++i)
				step(.125f);
			if (!check(!guarding(), "a hidden C4 does not start a guard task"))
				return false;
			carrierViewer = 0;
			bool movedToCover = false;
			for (int i = 0; i < 96; ++i)
			{
				gEntities[0].v.angles.y = gEntities[0].v.v_angle.y = 0;
				step(.125f);
				const auto &decision = nav.behavior_scenarioDecisions(0);
				movedToCover |= guarding() && decision.action == compat::ScenarioAction::Guard &&
					decision.hasTarget && decision.target.position.y == 50;
			}
			if (!check(guarding() && movedToCover && sample("radio/test/guardplan.wav") != reportAudio.end(),
					"a cooperative CT sees the moved C4 and announces the guard plan for its original drop area"))
				return false;
			if (!check(std::count(reportAudio.begin(), reportAudio.end(),
						std::make_pair(1, std::string("radio/test/guardplan.wav"))) == 1 &&
					sample("radio/test/guard.wav") == reportAudio.end() &&
					nav.behavior_managedBotStateMachines(1).task() != compat::CompatibilityTaskId::GuardLooseBomb,
					"the guard announces only its plan once and hearing the news does not make its teammate a guard"))
				return false;
			carrierViewer = -1;
			team(5, "CT");
			gEntities[4].v.team = 2;
			reportAudio.clear();
			hooks.pfnMessageBegin(MSG_ONE, 101, nullptr, &gEntities[0]);
			hooks.pfnWriteByte(5);
			hooks.pfnWriteString("%!MRAD_REPORTIN");
			hooks.pfnWriteShort(100);
			hooks.pfnMessageEnd();
			for (int i = 0; i < 64; ++i)
				step(.125f);
			const auto duty = sample("radio/test/guard.wav");
			const auto contact = sample("radio/test/clear.wav");
			if (!check(guarding() && duty < contact && contact != reportAudio.end(),
					"the guard answers ReportIn with its duty before reporting clear, even out of sight"))
				return false;
			reportAudio.clear();
			const float startedAt = first.stateTimestamp();
			for (int i = 0; i < 490 && guarding() && first.stateTimestamp() == startedAt; ++i)
				step(.125f);
			const float guardedFor = first.stateTimestamp() - startedAt;
			if (!check(guarding() && guardedFor > 30.0f && guardedFor <= 60.25f &&
					sample("radio/test/guardplan.wav") == reportAudio.end(),
					"timeout chooses cover again around the unseen box without repeating the guard plan"))
				return false;
			auto &commands = static_cast<metamod::BotCommandServices &>(runtime);
			const world::FrameIdentity frame{commands.service_lifecycle().mapGeneration(),
				commands.service_lifecycle().roundGeneration(), commands.service_adapterFrameCount()};
			commands.observation_observeReloadDispatched(first.actor(), frame, globals.time);
			gPlantedBomb.free = true;
			for (int i = 0; i < 8; ++i)
				step(.125f);
			if (!check(guarding() && sample("radio/test/pickup.wav") == reportAudio.end(),
					"the guard finishes its active reload before it acts on a missing C4"))
				return false;
			for (int i = 0; i < 64; ++i)
				step(.125f);
			return check(!guarding() && sample("radio/test/pickup.wav") != reportAudio.end(),
				"a guard discovers the missing C4, reports the pickup and leaves its guard task");
		}
		if (looseBombFixture)
		{
			auto listen = [&](int viewer, float bodyYaw = 0.0f)
			{
				carrierViewer = viewer;
				reportAudio.clear();
				for (int i = 0; i < 48; ++i)
				{
					gEntities[0].v.v_angle.y = gEntities[1].v.v_angle.y = 0;
					gEntities[0].v.angles.y = gEntities[1].v.angles.y = bodyYaw;
					step(.125f);
				}
			};
			auto count = [&](int speaker, const char *sample) {
				return std::count(reportAudio.begin(), reportAudio.end(), std::make_pair(speaker, std::string(sample)));
			};
			gPlantedBomb.v.classname = 4;
			gPlantedBomb.v.model = 5;
			gPlantedBomb.v.origin = Vector(224, 32, 36);
			gPlantedBomb.serialnumber = 12;
			listen(-1);
			if (!check(!count(1, "radio/test/loose.wav") && !count(2, "radio/test/loose.wav"),
				"a dropped C4 hidden from both CTs does not cause a report"))
				return false;
			listen(0, 180.0f);
			if (!check(!count(1, "radio/test/loose.wav"), "a CT facing away from the box cannot spot its C4"))
				return false;
			looseBombLineHitsBox = true;
			listen(0);
			if (!check(!count(1, "radio/test/loose.wav"), "a ray stopping short of the C4 origin cannot reveal it"))
				return false;
			looseBombLineHitsBox = false;
			listen(0);
			if (!check(count(1, "radio/test/loose.wav") == 1, "a CT spots the C4 through glass and reports it once"))
				return false;
			listen(1);
			if (!check(!count(2, "radio/test/loose.wav"), "a teammate already told about the drop does not repeat it"))
				return false;
			auto &picker = gEntities[4];
			gEntityNames[4] = "Picker";
			picker.v.netname = 104;
			picker.v.origin = Vector(1110, 32, 36);
			gEntities[0].v.origin = Vector(110, 32, 36);
			gEntities[1].v.origin = Vector(111, 32, 36);
			char playerLog[] = "\"Picker<%d><BOT><TERRORIST>\" triggered \"Got_The_Bomb\"\n";
			hooks.pfnAlertMessage(at_logged, playerLog, 104);
			listen(-1);
			if (!check(!count(1, "radio/test/pickup.wav") && !count(2, "radio/test/pickup.wav"),
				"a pickup log with a mismatched player ID does not notify the CTs"))
				return false;
			hooks.pfnAlertMessage(at_logged, playerLog, 105);
			// Keep the box present to isolate the pickup sound from a guard discovering it missing.
			listen(-1);
			return check(!count(1, "radio/test/pickup.wav") && count(2, "radio/test/pickup.wav") == 1,
				"only the CT inside the strict 1000-unit pickup radius reports the recovery");
		}
		if (carrierFixture)
		{
			auto &enemy = gEntities[4];
			enemy.v.origin = Vector(224, 32, 36);
			enemy.v.weapons = (1 << metamod::ActionAdapter::kC4WeaponId);
			reportAudio.clear();
			auto reports = [&](int viewer, float enemyX, int speaker)
			{
				carrierViewer = viewer;
				enemy.v.origin.x = enemyX;
				reportAudio.clear();
				for (int i = 0; i < 56; ++i)
				{
					gEntities[0].v.v_angle.y = gEntities[1].v.v_angle.y = 0;
					step(.125f);
				}
				return std::count(reportAudio.begin(), reportAudio.end(),
					std::make_pair(speaker, std::string("radio/test/carrier.wav")));
			};
			if (!check(reports(-1, 224, 1) == 0, "an unseen carrier is not reported") ||
				!check(reports(0, 224, 1) == 1, "the first CT spots and reports the carrier once") ||
				!check(reports(1, 1223, 2) == 0, "the second CT remembers the first CT's report across sightings"))
				return false;
			return check(reports(1, 1224, 2) == 1,
				"the second CT reports at exactly 1000 units from its teammate's original sighting");
		}
		auto plantAtSecondSite = [&]()
		{
			gPlantedBomb.v.classname = 2;
			gPlantedBomb.v.model = 3;
			gPlantedBomb.serialnumber = 5;
			gPlantedBomb.v.origin = Vector(229, 32, 0);
			gPlantedBomb.v.dmgtime = globals.time + 100;
		};
		if (requestedSiteFixture)
		{
			plantAtSecondSite();
			gEntities[0].v.v_angle.y = 0;
			reportAudio.clear();
		}
		const float plantedAt = globals.time;
		runtime.onMessageBegin(MSG_ALL, 98, nullptr, nullptr);
		runtime.onWriteByte(4);
		runtime.onWriteString("#Bomb_Planted");
		runtime.onMessageEnd();
		auto &nav = static_cast<metamod::BotNavigationServices &>(runtime);
		const auto searching = [&](int i) { return nav.behavior_managedBotStateMachines(i).task() ==
			compat::CompatibilityTaskId::FindTickingBomb; };
		if (requestedSiteFixture)
		{
			const auto requesterOrigin = gEntities[1].v.origin;
			gEntities[1].v.origin.x = -1000.0f;
			runtime.onEmitSound(&gPlantedBomb, "weapons/c4_beep1.wav", 1, 1);
			gEntities[1].v.origin = requesterOrigin;
			// Preserve the listener's knowledge while the unsolicited report expires.
			globals.time += 3.125f;
			for (int i = 0; i < 96; ++i)
			{
				gEntities[0].v.v_angle.y = 0;
				step(.125f);
			}
			auto where = std::find(reportAudio.begin(), reportAudio.end(),
				std::make_pair(2, std::string("radio/test/where.wav")));
			return check(where != reportAudio.end() && std::find(where, reportAudio.end(),
				std::make_pair(1, std::string("radio/test/planted.wav"))) != reportAudio.end(),
				"a searching CT asks where the bomb is and the CT that heard it replies");
		}
		for (int i = 0; i < 40 && !(searching(0) && searching(1)); ++i)
			step(.125f);
		if (!check(searching(0) && searching(1), "two CTs begin searching for the unseen C4"))
			return false;
		const auto &initial = nav.behavior_scenarioDecisions(1);
		if (!check(initial.hasTarget && initial.target.entity.id == 6, "the receiving CT initially targets the first site"))
			return false;
		reportAudio.clear();
		if (plantedSiteFixture || deferredSiteFixture)
		{
			plantAtSecondSite();
			gEntities[1].v.origin.x = -1000;
			runtime.onEmitSound(&gPlantedBomb, "weapons/c4_beep1.wav", 1, 1);
			runtime.onEmitSound(&gPlantedBomb, "weapons/c4_beep2.wav", 1, 1);
			gEntities[1].v.origin.x = 110;
			if (plantedSiteFixture)
			{
				struct SearchExitTrace final : compat::IRandomTraceSink
				{
					void record(const compat::RandomTraceRecord &entry) override
					{
						if (entry.request.actor.slot == 1U && entry.request.semanticId &&
							std::strcmp(entry.request.semanticId, "CSBOT-PATH-RETRY") == 0)
							++pathAttempts;
					}
					unsigned pathAttempts = 0;
				} trace;
				auto &behavior = static_cast<metamod::BotBehaviorServices &>(runtime);
				auto &random = behavior.service_compatibilityRandomSource();
				random.setTraceSink(&trace);
				step(.125f);
				random.setTraceSink(nullptr);
				const auto &listener = nav.behavior_managedBotStateMachines(0);
				if (!check(listener.state() == compat::CompatibilityStateId::Idle &&
						!behavior.navigation_hasActivePath(0) && trace.pathAttempts == 0 && searching(1),
						"only the CT hearing the beep exits through Idle before another path attempt"))
					return false;
				step(.125f);
				const auto &choice = nav.behavior_scenarioDecisions(0);
				if (!check(choice.hasTarget && choice.target.position.x == 229,
						"the next update selects the exact remembered C4 position"))
					return false;
			}
			if (deferredSiteFixture)
			{
				auto guardsSite = [&](int i) {
					const auto &machine = nav.behavior_managedBotStateMachines(i);
					return machine.state() == compat::CompatibilityStateId::Hide &&
						machine.task() == compat::CompatibilityTaskId::GuardBombZone;
				};
				bool foundCover[2]{};
				for (int frame = 0; frame < 80; ++frame)
				{
					gEntities[0].v.v_angle.y = 0;
					step(.125f);
					for (int i = 0; i < 2; ++i)
					{
						const auto &choice = nav.behavior_scenarioDecisions(i);
						foundCover[i] |= guardsSite(i) && choice.action == compat::ScenarioAction::Guard &&
							choice.hasTarget && choice.target.position.y == 50;
					}
				}
				if (!check(foundCover[0] && foundCover[1] && guardsSite(0) && guardsSite(1),
					"CTs knowing the C4 or just its reported site guard NAV cover for an unseen living human teammate"))
					return false;
				if (enemyDamageHideFixture)
				{
					auto observer = std::make_unique<metamod::BotObservationRuntime>();
					observer->bindServices(runtime);
					gEntities[0].v.netname = 100;
					gEntityNames[0] = "Hidden Damage Test Bot";
					engine.pfnFindEntityByString = [](edict_t *start, const char *key, const char *value) -> edict_t * {
						if (std::strcmp(key, "netname") == 0)
							return !start && std::strcmp(value, "Hidden Damage Test Bot") == 0 ? &gEntities[0] : nullptr;
						return findEntityByString(start, key, value);
					};
					int botObject = 0;
					auto &enemy = gEntities[4];
					enemy.v.flags |= FL_FAKECLIENT;
					enemy.v.pContainingEntity = &enemy;
					enemy.v.origin = Vector(-900, 0, 36);
					struct PanicTrace : compat::IRandomTraceSink
					{
						std::vector<std::string> calls;
						void record(const compat::RandomTraceRecord &entry) override
						{
							const char *name = entry.request.semanticId;
							if (entry.request.actor.slot == 1 && std::strncmp(name, "Damage.Panic", 12) == 0)
								calls.emplace_back(name);
						}
					} trace;
					auto &random = static_cast<metamod::BotBehaviorServices &>(runtime).service_compatibilityRandomSource();
					const auto damage = [&] {
						observer->observeGameDllDamage(&botObject, &enemy.v, [&](const char *name) -> const void * {
							return std::strcmp(name, "Hidden Damage Test Bot") == 0 ? &botObject : nullptr;
						});
					};
					random.setTraceSink(&trace);
					gEntities[0].v.angles = gEntities[0].v.v_angle = Vector(0, 0, 0);
					damage();
					damage();
					random.setTraceSink(nullptr);
					if (!check(trace.calls == std::vector<std::string>{"Damage.PanicSide", "Damage.PanicHeight", "Damage.PanicDelay"},
						"unseen direct damage without an attack consumes three panic draws once per shotgun receipt"))
						return false;
					runtime::BotCommand panicCommand{};
					static_cast<metamod::BotCommandServices &>(runtime).behavior_applyNoiseLookCommand(0, panicCommand);
					if (!check(panicCommand.synchronizeCombatView && std::fabs(panicCommand.viewAngles.yaw) > 0.0f,
						"the panic look steers the command toward the rear quadrant"))
						return false;
					step(.125f);
					if (!check(guardsSite(0), "damage while traveling to the hiding spot does not abandon Hide"))
						return false;
					for (int frame = 0; frame < 9; ++frame)
						step(.125f);
					const auto target = nav.behavior_scenarioDecisions(0).target.position;
					gEntities[0].v.origin = Vector(target.x, target.y, target.z + 36.0f);
					trace.calls.clear();
					random.setTraceSink(&trace);
					damage();
					random.setTraceSink(nullptr);
					if (!check(trace.calls.empty(), "a later hit inside the five-second surprise window consumes no panic draws"))
						return false;
					step(.125f);
					if (!check(guardsSite(0), "the arrival update still follows the traveling Hide branch"))
						return false;
					step(.125f);
					return check(nav.behavior_managedBotStateMachines(0).state() == compat::CompatibilityStateId::Idle,
						"a hiding bot becomes Idle after fresh damage from an unseen enemy bot");
				}
				if (encourageDefuseFixture)
				{
					const auto heard = [&](const char *sample) {
						return std::count(queueSamples.begin(), queueSamples.end(), sample);
					};
					if (!check(heard("radio/test/secure.wav") == 0, "moving to cover does not encourage the human"))
						return false;
					for (int i = 0; i < 2; ++i)
					{
						const auto target = nav.behavior_scenarioDecisions(i).target.position;
						gEntities[i].v.origin = Vector(target.x, target.y, target.z + 36.0f);
					}
					queueSamples.clear();
					const auto advance = [&](int frames) {
						for (int frame = 0; frame < frames; ++frame)
							step(.125f);
					};
					advance(24);
					if (!check(heard("radio/test/secure.wav") == 1,
							"only one CT announces a secure site above 30 seconds"))
						return false;
					queueDefuseFuse.value = globals.time - plantedAt + 30.0f;
					advance(24);
					if (!check(heard("radio/test/waitdefuse.wav") == 0,
							"crossing 30 seconds does not bypass the shared cooldown"))
						return false;
					advance(56);
					if (!check(heard("radio/test/waitdefuse.wav") == 1,
							"a CT asks the human to defuse once between 11 and 30 seconds"))
						return false;
					queueDefuseFuse.value = globals.time - plantedAt + 11.0f;
					for (int frame = 0; frame < 56 && heard("radio/test/urgentdefuse.wav") == 0; ++frame)
						step(.125f);
					return check(heard("radio/test/urgentdefuse.wav") == 1,
						"a CT gives the urgent warning once at 11 seconds or less");
				}
				gEntities[0].v.origin.x = 24;
				const auto &first = nav.behavior_managedBotStateMachines(0);
				const float started = first.stateTimestamp();
				while (guardsSite(0) && first.stateTimestamp() == started && globals.time - started <= 61.0f)
					step(.125f);
				const float elapsed = first.stateTimestamp() - started;
				step(.125f);
				const auto &renewed = nav.behavior_scenarioDecisions(0);
				if (!check(guardsSite(0) && elapsed > 30.0f && elapsed <= 60.25f && renewed.hasTarget &&
						renewed.target.position.x == 32 && renewed.target.position.y == 50,
					"guard timeout selects cover in the closest site without leaving Hide while the C4 is elsewhere"))
					return false;
				gEntities[2].v.health = 0;
				gEntities[2].v.deadflag = DEAD_DEAD;
				for (int i = 0; i < 24; ++i)
					step(.125f);
				return check(nav.behavior_managedBotStateMachines(0).task() == compat::CompatibilityTaskId::DefuseBomb &&
						nav.behavior_managedBotStateMachines(1).task() == compat::CompatibilityTaskId::DefuseBomb,
					"the CTs leave site defense to defuse after the only living human teammate dies");
			}
			bool movedToSite = false;
			for (int i = 0; i < 80; ++i)
			{
				step(.125f);
				const auto &decision = nav.behavior_scenarioDecisions(1);
				movedToSite |= decision.hasTarget && decision.target.entity.id == 8 &&
					decision.target.position.x == 224 && decision.action == compat::ScenarioAction::Move &&
					nav.behavior_managedBotStateMachines(1).task() == compat::CompatibilityTaskId::DefuseBomb;
			}
			const auto reports = std::count(reportAudio.begin(), reportAudio.end(),
				std::make_pair(1, std::string("radio/test/planted.wav")));
			return check(reports == 1, "CT reports the first audible C4 beep once") &&
				check(movedToSite, "out-of-earshot CT travels to the reported site center");
		}
		gEntities[0].v.origin.x = 24;
		bool sent = false;
		bool acknowledged = false;
		bool retargeted = false;
		for (int i = 0; i < 80; ++i)
		{
			step(.125f);
			for (const auto &audio : reportAudio)
			{
				sent |= audio.first == 1 && audio.second == "radio/test/zoneclear.wav";
				acknowledged |= sent && audio.first == 2 && audio.second == "radio/test/ack.wav";
			}
			reportAudio.clear();
			const auto &next = nav.behavior_scenarioDecisions(1);
			retargeted |= sent && next.hasTarget && next.target.entity.id == 8;
		}
		return check(sent && acknowledged && retargeted,
			"CT enters the first site, announces it, and its teammate acknowledges and searches the other site");
	}
	if (reportingInFixture)
	{
		queueEnemiesVisible = false;
		queueMode = "normal";
		runtime.beginRoundGeneration();
		step(12.0f);
		queueSamples.clear();
		hooks.pfnMessageBegin(MSG_ONE, 101, nullptr, &self);
		hooks.pfnWriteByte(5);
		hooks.pfnWriteString("%!MRAD_REPORTIN");
		hooks.pfnWriteShort(100);
		hooks.pfnMessageEnd();
		for (int i = 0; i < 48; ++i)
			step(.125f);
		if (!check(std::count(queueSamples.begin(), queueSamples.end(), "radio/test/clear.wav") == 1 &&
			std::count(queueSamples.begin(), queueSamples.end(), "radio/test/ack.wav") == 0,
			"a human report-in requests one audible clear report from a quiet teammate"))
			return false;
		runtime.setCompatibilityFloat("bot_quota", 2);
		if (runtime.executeCompatibilityCommand(request("bot_add_t", 1U, "Queue Peer")) !=
			metamod::CompatibilityCommandResult::Handled)
			return false;
		gEntities[1].v.team = 1;
		gEntities[1].v.flags = FL_CLIENT | FL_FAKECLIENT | FL_ONGROUND;
		gEntities[1].v.health = 100;
		gEntities[1].v.deadflag = DEAD_NO;
		gEntities[1].v.solid = SOLID_SLIDEBOX;
		gEntities[1].v.movetype = MOVETYPE_WALK;
		gEntities[1].v.maxspeed = 250;
		gEntities[1].v.view_ofs = Vector(0, 0, 17);
		setCvarFloat("mp_buytime", 0);
		setCvarFloat("bot_stop", 0);
		team(2, "TERRORIST");
		runtime.beginRoundGeneration();
		reportAudio.clear();
		int requester = 0;
		std::size_t read = 0;
		for (int i = 0; i < 400; ++i)
		{
			step(.125f);
			for (; read < reportAudio.size(); ++read)
			{
				const auto &spoken = reportAudio[read];
				if (!requester && spoken.second == "radio/test/request.wav")
					requester = spoken.first;
				if (requester && spoken.first != requester && spoken.second == "radio/test/clear.wav")
					return true;
			}
		}
		for (const auto &spoken : reportAudio)
			std::fprintf(stderr, "slot %d: %s\n", spoken.first, spoken.second.c_str());
		auto &ports = static_cast<metamod::BotBehaviorServices &>(runtime);
		std::fprintf(stderr, "receiver managed=%d name=%d created=%d team=%d updates=%u\n",
			int(ports.service_managedBotSlots()[1]), gEntities[1].v.netname, gCreateCount,
			ports.observation_resolvePlayerTeam(&gEntities[1]), ports.service_managedBotFullUpdateSequences()[1]);
		return check(false, "after team silence one bot asks for a report and its teammate audibly answers");
	}
	return testQueueRadioReset(runtime, engine) && testPlanAgreementRuntime(runtime, globals) &&
		testProductionNavCenters(runtime) && testPublicNavLadders(runtime, engine) &&
		testPublicNativeLadderTrace(engine) && testNativeHelpDeathMemory(runtime, engine, globals) &&
		testActualHelpMovement(runtime, engine, globals) && testActualHelpMovement(runtime, engine, globals, true) &&
		testActualHelpMovement(runtime, engine, globals, true, true) &&
		testActualHelpMovement(runtime, engine, globals, false, false, true) &&
		testActualHelpMovement(runtime, engine, globals, true, false, false, true) &&
		testActualLadderHelpMovement(runtime, engine, globals) &&
		testActualHelpMovement(runtime, engine, globals, false, false, true, false, true) &&
		testActualFollowMovement(runtime, globals, 2.0f) && testActualFollowMovement(runtime, globals, 2.0f, true) &&
		testActualFollowMovement(runtime, globals, 2.125f) &&
		testActualFollowMovement(runtime, globals) && testActualFollowMovement(runtime, globals, 0.0f, false, true);
}
}
