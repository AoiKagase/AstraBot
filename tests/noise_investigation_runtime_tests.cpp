#include "plugin/plugin_runtime.hpp"
namespace
{
bool testInvestigation(astrabot::metamod::PluginRuntime &, enginefuncs_t &, globalvars_t &);
void setupHeardNoise(enginefuncs_t &, globalvars_t &);
}
#define ASTRABOT_EXTRA_ACTOR_TEST testInvestigation
#define ASTRABOT_ACTOR_ENGINE_SETUP setupHeardNoise
#define ASTRABOT_ACTOR_PROFILE_TEXT                                                                                    \
	"Any { Name = \"Investigation Bot\" Team = any Difficulty = 1 Skill = 70 Aggression = 50 ReactionTime = 0.125 }\n"
#include "compat_actor_command_tests.cpp"
#include "fixtures/noise_runtime_nav.hpp"
namespace
{
bool closeNoiseVisible = false;
unsigned heardReportDraws = 0;
int heardMessageType = 0;
const char *heardMode = "normal";
std::vector<std::string> heardSamples;

struct HeardTrace : astrabot::compat::IRandomTraceSink
{
	void record(const astrabot::compat::RandomTraceRecord &record) override
	{
		if (std::strcmp(record.request.semanticId, "RNG-CSBOT-HEARD-NOISE-REPORT") == 0)
			++heardReportDraws;
	}
};
HeardTrace heardTrace;

float heardRandom(float low, float high)
{
	if (low == 0 && high == 100)
	{
		return 0;
	}
	return hearingFixtureRandomFloat(low, high);
}

unsigned char *heardLoad(char *name, int *length)
{
	if (std::strcmp(name, "BotChatter.db") != 0)
	{
		*length = 0;
		return nullptr;
	}
	const char text[] =
		"BaseDir radio/test/ Place Speaker End Place Noise End Chatter HeardNoise Place Speaker wrong.wav Place Noise heard.wav End";
	*length = static_cast<int>(sizeof(text) - 1);
	auto *bytes = new unsigned char[sizeof(text)];
	std::memcpy(bytes, text, sizeof(text));
	return bytes;
}

void heardFree(void *data)
{
	delete[] static_cast<unsigned char *>(data);
}

int heardFileSize(char *)
{
	return 100;
}

unsigned heardWaveLength(const char *)
{
	return 200;
}

int heardMessageId(plid_t id, const char *name, int *size)
{
	if (std::strcmp(name, "SendAudio") == 0)
		return 101;
	if (std::strcmp(name, "BotVoice") == 0)
		return 102;
	return siteFixtureUserMessageId(id, name, size);
}

void heardBegin(int, int type, const float *, edict_t *)
{
	heardMessageType = type;
}

void heardNumber(int)
{
}

void heardString(const char *sample)
{
	if (heardMessageType == 101)
		heardSamples.push_back(sample);
}

void heardEnd()
{
}

void investigationTrace(const float *, const float *end, int flags, edict_t *, TraceResult *trace)
{
	*trace = {};
	trace->flFraction = flags == 0x101 && closeNoiseVisible ? 1.0f : 0.0f;
	trace->vecEndPos = Vector(end[0], end[1], end[2]);
}
const char *heardCvar(const char *name)
{
	if (std::strcmp(name, "bot_chatter") == 0)
		return heardMode;
	const auto *value = getCvar(name);
	return value && value->string ? value->string : "";
}

void setupHeardNoise(enginefuncs_t &engine, globalvars_t &)
{
	engine.pfnCVarGetString = &heardCvar;
	engine.pfnLoadFileForMe = &heardLoad;
	engine.pfnFreeFile = &heardFree;
	engine.pfnGetFileSize = &heardFileSize;
	engine.pfnGetApproxWavePlayLen = &heardWaveLength;
	engine.pfnMessageBegin = &heardBegin;
	engine.pfnWriteByte = &heardNumber;
	engine.pfnWriteShort = &heardNumber;
	engine.pfnWriteString = &heardString;
	engine.pfnMessageEnd = &heardEnd;
	gpMetaUtilFuncs->pfnGetUserMsgID = &heardMessageId;
}

bool testInvestigation(astrabot::metamod::PluginRuntime &runtime, enginefuncs_t &engine, globalvars_t &globals)
{
	using namespace astrabot;
	engine.pfnTraceLine = &investigationTrace;
	engine.pfnRandomFloat = &heardRandom;
	setupHeardNoise(engine, globals);
	runtime.giveEnginePointers(&engine, &globals);
	gObjectiveEntitiesAvailable = false;
	for (unsigned i = 1; i < 4; ++i)
	{
		gEntities[i].v.deadflag = DEAD_DEAD;
		gEntities[i].v.health = 0;
	}
	auto &self = gEntities[0];
	self.v.flags = FL_CLIENT | FL_FAKECLIENT | FL_ONGROUND;
	self.v.health = 100;
	self.v.deadflag = DEAD_NO;
	self.v.origin = Vector(0, 0, 36);
	self.v.view_ofs = Vector(0, 0, 17);
	self.v.v_angle = Vector(0, 0, 0);
	self.v.maxspeed = 240;
	auto &source = gEntities[4];
	source = {};
	source.serialnumber = 44;
	source.v.flags = FL_CLIENT | FL_ONGROUND;
	source.v.health = 100;
	source.v.deadflag = DEAD_NO;
	source.v.origin = Vector(0, 500, 36);
	runtime.onMessageBegin(MSG_ALL, 86, nullptr, nullptr);
	runtime.onWriteByte(5);
	runtime.onWriteString("CT");
	runtime.onMessageEnd();
	auto &behavior = static_cast<metamod::BotBehaviorServices &>(runtime);
	static_cast<compat::EngineRandomSource &>(behavior.service_compatibilityRandomSource()).setTraceSink(&heardTrace);
	auto &navigation = static_cast<metamod::BotNavigationServices &>(runtime);
	world::WorldSnapshot belief;
	nav::NavVector target{};
	const auto step = [&](float dt)
	{
		globals.time += dt;
		runtime.onStartFrame();
		runtime.onStartFramePost();
		behavior.observation_readManagedWorldSnapshot(0, &belief);
	};
	const auto emit = [&]()
	{
		runtime.onEmitSound(&source, "weapons/ak47-1.wav", 1, 1);
	};
	const auto investigating = [&]()
	{
		return navigation.behavior_managedBotStateMachines(0).state() == compat::CompatibilityStateId::InvestigateNoise;
	};
	runtime.beginRoundGeneration();
	emit();
	step(.2f);
	if (!check(!belief.noise().active, "enemy sound without NAV cannot create a destination"))
		return false;
	if (!check(loadInvestigationNav(runtime), "investigation runtime loads real NAV"))
		return false;
	runtime.beginRoundGeneration();
	emit();
	step(.2f);
	if (!check(
			belief.noise().active && !investigating(), "safe time hears sound but does not leave task to investigate"))
		return false;
	step(30);
	// Native ComputePath retains its0.4–0.6s throttle after a previous Hunt attempt.
	// Make the reaction-delayed receipt eligible after that existing retry window.
	step(.25f);
	emit();
	step(.15f); // Publish a reaction-ready receipt while the old path timer is still pending.
	step(.3f); // Decide with that receipt after the native path throttle has elapsed.
	for (int i = 0; i < 6 && !investigating(); ++i)
		step(.1f);
	if (!check(
			investigating() && !belief.noise().active, "eligible heard sound enters investigation and consumes memory"))
		return false;
	if (!check(navigation.behavior_buildManagedObjectiveTarget(0, &target, nullptr) && target.y == 500 && target.z == 0,
			"consumed sound retains corrected NAV target"))
		return false;
	step(.12f);
	if (!check(navigation.behavior_noiseInvestigationWalking(0) &&
				std::hypot(gLastRunPlayerMoveForward, gLastRunPlayerMoveSide) > 0 &&
				std::hypot(gLastRunPlayerMoveForward, gLastRunPlayerMoveSide) <= 96.01f,
			"near noise with few teammates selects quiet walking"))
		return false;
	step(.3f);
	if (!check(
			investigating() && navigation.behavior_buildManagedObjectiveTarget(0, &target, nullptr) && target.y == 500,
			"investigation target persists without new sound"))
		return false;
	source.v.origin.y = 450;
	emit();
	step(.3f);
	if (!check(navigation.behavior_buildManagedObjectiveTarget(0, &target, nullptr) && target.y == 500,
			"slightly closer sound does not redirect current investigation"))
		return false;
	source.v.origin.y = 250;
	emit();
	for (int i = 0; i < 6; ++i)
	{
		step(.12f);
		navigation.behavior_buildManagedObjectiveTarget(0, &target, nullptr);
		if (target.y == 250)
			break;
	}
	if (!check(investigating() && target.y == 250 && !belief.noise().active,
			"much closer sound redirects and is consumed"))
		return false;
	self.v.origin.y = 130;
	self.v.v_angle = Vector(24, 90, 0);
	self.v.angles = self.v.v_angle;
	closeNoiseVisible = true;
	step(.12f);
	if (!check(!investigating() && !navigation.behavior_noiseInvestigationWalking(0),
			"close viewed visible point completes investigation and resets walking"))
		return false;
	closeNoiseVisible = false;
	self.v.origin.y = 0;
	source.v.origin.y = 500;
	step(.15f);
	emit();
	step(.15f);
	step(.3f);
	if (!check(investigating(), "new sound can start a later investigation"))
		return false;
	// Inject the SDK-free core overlay via its public API to isolate lifetime validation from combat RNG.
	auto &machine = const_cast<compat::CompatibilityStateMachine &>(navigation.behavior_managedBotStateMachines(0));
	compat::StateUpdateContext attack{};
	attack.actor = belief.identity().observer;
	attack.frame = belief.identity().frame;
	attack.timestamp = globals.time;
	attack.fullUpdateSequence = navigation.service_managedBotFullUpdateSequences()[0];
	attack.fullUpdate = true;
	attack.lifeAvailability = compat::ObservationAvailability::Available;
	attack.alive = true;
	attack.roundAvailability = compat::ObservationAvailability::Available;
	attack.roundActive = true;
	attack.transitionRequest = compat::ObservationAvailability::Available;
	attack.attackObservation = compat::ObservationAvailability::Available;
	attack.rngAvailability = compat::ObservationAvailability::Available;
	if (!check(machine.beginAttack(attack) == compat::StateResult::Transitioned,
			"lifecycle regression enters attack overlay through core public API"))
		return false;
	if (!check(loadInvestigationNav(runtime), "NAV can be replaced during combat"))
		return false;
	if (!check(!navigation.behavior_buildManagedObjectiveTarget(0, &target, nullptr),
			"combat cannot export a target from the previous NAV revision"))
		return false;
	step(.12f);
	if (!check(!navigation.behavior_buildManagedObjectiveTarget(0, &target, nullptr),
			"combat full update invalidates stale retained investigation"))
		return false;
	attack.timestamp = globals.time;
	attack.frame = belief.identity().frame;
	machine.stopAttack(attack);
	runtime.beginRoundGeneration();
	step(30);
	step(.25f);
	emit();
	step(.15f);
	step(.5f);
	if (!check(investigating() && navigation.behavior_noiseInvestigationWalking(0),
			"fresh round creates live walking investigation before death"))
		return false;
	self.v.deadflag = DEAD_DEAD;
	self.v.health = 0;
	step(.12f);
	if (!check(!navigation.behavior_noiseInvestigationWalking(0), "death resets held investigation posture"))
		return false;
	if (!check(heardReportDraws == 0 && heardSamples.empty(), "high-priority gunshots never generate heard-noise chatter"))
		return false;
	self.v.deadflag = DEAD_NO;
	self.v.health = 100;
	self.v.origin = Vector(0, 0, 36);
	runtime.beginRoundGeneration();
	// Enrich the existing two-area NAV through the SDK-free publisher's public API.
	const auto original = behavior.service_navPublisher().snapshot();
	nav::NavDocument placed;
	placed.setSourceIdentity(original.document()->sourceIdentity());
	nav::NavPlace speakerPlace{};
	speakerPlace.id = 1;
	std::memcpy(speakerPlace.name, "Speaker", sizeof("Speaker"));
	nav::NavPlace noisePlace{};
	noisePlace.id = 2;
	std::memcpy(noisePlace.name, "Noise", sizeof("Noise"));
	placed.addPlace(speakerPlace);
	placed.addPlace(noisePlace);
	for (auto area : original.document()->areas())
	{
		area.placeId = static_cast<std::uint16_t>(area.id);
		placed.addArea(area);
	}
	auto &publisher = const_cast<nav::NavSnapshotPublisher &>(behavior.service_navPublisher());
	if (!check(publisher.publish(&placed, runtime.snapshot().mapGeneration) == nav::NavSnapshotResult::Published,
			"public NAV test has different speaker and noise places"))
		return false;
	// A native non-emote statement requires a living teammate at playback.
	auto &teammate = gEntities[1];
	teammate.v.flags = FL_CLIENT | FL_ONGROUND;
	teammate.v.health = 100;
	teammate.v.deadflag = DEAD_NO;
	teammate.v.origin = Vector(-500, 0, 36);
	runtime.onMessageBegin(MSG_ALL, 86, nullptr, nullptr);
	runtime.onWriteByte(2);
	runtime.onWriteString("TERRORIST");
	runtime.onMessageEnd();
	step(31);
	step(.25f);
	source.v.origin.y = 500;
	runtime.onEmitSound(&source, "player/pl_step1.wav", 1, 1);
	step(.15f);
	step(.5f);
	for (int i = 0; i < 6 && heardSamples.empty(); ++i)
		step(.12f);
	if (!check(heardReportDraws == 1 && !heardSamples.empty(),
			"attending medium noise emits a probabilistic teammate report"))
		return false;
	for (const auto &sample : heardSamples)
		if (!check(sample == "radio/test/heard.wav", "report voice uses the noise place rather than speaker place"))
			return false;
	const auto sent = heardSamples.size();
	source.v.origin.y = 250;
	runtime.onEmitSound(&source, "player/pl_step1.wav", 1, 1);
	step(.15f);
	step(.5f);
	if (!check(heardReportDraws == 1 && heardSamples.size() == sent,
			"closer attended sound inside20 seconds does not draw or repeat chatter"))
		return false;
	runtime.beginRoundGeneration();
	heardMode = "off";
	step(31);
	step(.25f);
	source.v.origin.y = 500;
	runtime.onEmitSound(&source, "player/pl_step1.wav", 1, 1);
	step(.15f);
	step(.5f);
	if (!check(heardReportDraws == 2 && heardSamples.size() == sent,
			"chatter off still consumes report chance and cooldown without sending audio"))
		return false;
	heardMode = "normal";
	source.v.origin.y = 250;
	runtime.onEmitSound(&source, "player/pl_step1.wav", 1, 1);
	step(.15f);
	step(.5f);
	if (!check(heardReportDraws == 2 && heardSamples.size() == sent,
			"enabling chatter cannot replay the suppressed report inside cooldown"))
		return false;
	runtime.onRoundLifecycleMessage("World triggered \"Round_Start\"\n");
	runtime.onRoundLifecycleMessage("World triggered \"Round_End\"\n");
	step(31);
	source.v.origin.y = 500;
	runtime.onEmitSound(&source, "player/pl_step1.wav", 1, 1);
	step(.15f);
	step(.5f);
	if (!check(heardReportDraws == 2 && heardSamples.size() == sent,
			"public round end suppresses new noise reports until round start"))
		return false;
	runtime.onRoundLifecycleMessage("World triggered \"Round_Start\"\n");
	step(31);
	step(.25f);
	runtime.onEmitSound(&source, "player/pl_step1.wav", 1, 1);
	step(.15f);
	step(.5f);
	return check(heardReportDraws == 3 && heardSamples.size() > sent,
		"public round start permits fresh heard-noise reports again");
}
}
