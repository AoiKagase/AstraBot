#include "plugin/plugin_runtime.hpp"
namespace
{
bool testInvestigation(astrabot::metamod::PluginRuntime &, enginefuncs_t &, globalvars_t &);
}
#define ASTRABOT_EXTRA_ACTOR_TEST testInvestigation
#define ASTRABOT_ACTOR_PROFILE_TEXT                                                                                    \
	"Any { Name = \"Investigation Bot\" Team = any Difficulty = 1 Skill = 70 Aggression = 50 ReactionTime = 0.125 }\n"
#include "compat_actor_command_tests.cpp"
#include "fixtures/noise_runtime_nav.hpp"
namespace
{
bool closeNoiseVisible = false;
void investigationTrace(const float *, const float *end, int flags, edict_t *, TraceResult *trace)
{
	*trace = {};
	trace->flFraction = flags == 0x101 && closeNoiseVisible ? 1.0f : 0.0f;
	trace->vecEndPos = Vector(end[0], end[1], end[2]);
}
bool testInvestigation(astrabot::metamod::PluginRuntime &runtime, enginefuncs_t &engine, globalvars_t &globals)
{
	using namespace astrabot;
	engine.pfnTraceLine = &investigationTrace;
	engine.pfnRandomFloat = &hearingFixtureRandomFloat;
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
	return check(!navigation.behavior_noiseInvestigationWalking(0), "death resets held investigation posture");
}
}
