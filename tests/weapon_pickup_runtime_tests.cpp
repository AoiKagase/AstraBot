#include "plugin/plugin_runtime.hpp"
#include "astrabot/compat/weapon_pickup.hpp"
namespace
{
bool testWeaponPickup(astrabot::metamod::PluginRuntime &, enginefuncs_t &, globalvars_t &);
}
#define ASTRABOT_EXTRA_ACTOR_TEST testWeaponPickup
#define ASTRABOT_ACTOR_PROFILE_TEXT                                                                                    \
	"AnyProfile { Name = \"Any Bot\" Team = any Difficulty = 1 Skill = 70 WeaponPreference = deagle WeaponPreference = ak47 WeaponPreference = m4a1 }\n"
#include "compat_actor_command_tests.cpp"
namespace
{
unsigned pickupDropCount = 0;
std::string pickupDropArgument;
void pickupClientCommand(edict_t *self)
{
	if (std::strcmp(astrabot::metamod::HookCommandArgv(0), "drop") == 0)
	{
		++pickupDropCount;
		pickupDropArgument = astrabot::metamod::HookCommandArgv(1);
		return; // The fixture deliberately leaves inventory unchanged until GameDLL continuation.
	}
	hookedGameClientCommand(self);
}
int pickupMessageId(plid_t id, const char *name, int *size)
{
	return std::strcmp(name, "ScoreAttrib") == 0 ? 101 : siteFixtureUserMessageId(id, name, size);
}
const char *pickupString(int index)
{
	switch (index)
	{
	case 20:
		return "weaponbox";
	case 21:
		return "models/w_ak47.mdl";
	case 22:
		return "models/w_m4a1.mdl";
	case 23:
		return "models/w_deagle.mdl";
	case 24:
		return "models/custom_ak47.mdl";
	case 25:
		return "models/w_backpack.mdl";
	case 26:
		return "armoury_entity";
	default:
		return szFromIndex(index);
	}
}
bool testWeaponPickup(astrabot::metamod::PluginRuntime &runtime, enginefuncs_t &engine, globalvars_t &globals)
{
	using namespace astrabot;
	compat::ProfileRecord profile{};
	profile.weaponPreferences[0] = 26;
	profile.weaponPreferences[1] = 28;
	profile.weaponPreferences[2] = 22;
	profile.weaponPreferenceCount = 3;
	if (!check(compat::shouldExchangePrimary(profile, 22, 28, 2.5f) &&
				!compat::shouldExchangePrimary(profile, 22, 28, 2.49f) &&
				!compat::shouldExchangePrimary(profile, 28, 22, 10) &&
				!compat::shouldExchangePrimary(profile, 22, 22, 10) &&
				!compat::shouldExchangePrimary(profile, 0, 28, 10) &&
				!compat::shouldExchangePrimary(profile, 22, 26, 10),
			"ordered primary preferences and the inclusive 2.5-second safe interval match CSBot"))
		return false;
	if (!check(
			compat::shouldExchangePrimary(profile, 19, 28, 10) && !compat::shouldExchangePrimary(profile, 19, 18, 10),
			"a listed dropped primary outranks an unlisted current gun; two unlisted guns do not exchange"))
		return false;
	profile.weaponPreferenceCount = 1;
	if (!check(!compat::shouldExchangePrimary(profile, 22, 28, 10),
			"pistol-only preference never exchanges primary weapons"))
		return false;
	if (!check(compat::primaryWeaponFromWorldModel("models/w_mp5.mdl")->id == 19 &&
				!compat::primaryWeaponFromWorldModel("models/w_shield.mdl") &&
				!compat::primaryWeaponFromWorldModel("models/custom_ak47.mdl") &&
				!compat::carriedPrimaryWeapon((1U << 22) | (1U << 28)),
			"standard MP5 alias is mapped; shield, custom models and ambiguous inventory are unknown"))
		return false;
	DLL_FUNCTIONS hooks{};
	int version = INTERFACE_VERSION;
	if (!check(runtime.provideEntityApi(&hooks, &version) && hooks.pfnTouch,
			"weapon pickup uses the actual public pre-Touch hook"))
		return false;
	gpMetaUtilFuncs->pfnGetUserMsgID = &pickupMessageId;
	engine.pfnSzFromIndex = &pickupString;
	engine.pfnTraceLine = &traceObjectiveVisibility;
	gHookedGameDllTable.pfnClientCommand = &pickupClientCommand;
	runtime.giveEnginePointers(&engine, &globals);
	gObjectiveEntitiesAvailable = false;
	gObjectiveTraceVisible = false;
	gEntities[1].v.deadflag = DEAD_DEAD;
	gEntities[1].v.health = 0;
	auto &self = gEntities[0];
	self.v.flags = FL_CLIENT | FL_FAKECLIENT | FL_ONGROUND;
	self.v.health = 100;
	self.v.deadflag = DEAD_NO;
	self.v.origin = Vector(0, 0, 36);
	self.v.view_ofs = Vector(0, 0, 17);
	self.v.v_angle = Vector(0, 0, 0);
	self.v.angles = self.v.v_angle;
	self.v.fov = 90;
	self.v.weapons = (1 << 22) | (1 << 26) | (1 << 29);
	runtime.onMessageBegin(1, 94, nullptr, &self);
	runtime.onWriteByte(1);
	runtime.onWriteByte(26);
	runtime.onWriteByte(7);
	runtime.onMessageEnd();
	auto &box = gEntities[4];
	box = {};
	box.serialnumber = 42;
	box.v.classname = 20;
	box.v.model = 21;
	box.v.flags = FL_ONGROUND;
	const auto step = [&](float dt)
	{
		globals.time += dt;
		runtime.onStartFrame();
		runtime.onStartFramePost();
	};
	step(3.0f);
	const auto &loaded = static_cast<metamod::BotObservationServices &>(runtime).behavior_managedProfiles(0);
	if (!check(loaded.weaponPreferenceCount == 3 && loaded.weaponPreferences[1] == 28,
			"contact decision uses the loaded ordered profile"))
		return false;
	hooks.pfnTouch(&box, &self);
	if (!check(pickupDropCount == 1 && pickupDropArgument == "weapon_m4a1" && (self.v.weapons & (1 << 22)),
			"pre-Touch synchronously drops the named primary while a pistol is active, without inventing inventory success"))
		return false;
	// Emulate GameDLL completing the pickup after our pre-hook returns.
	self.v.weapons = (1 << 28) | (1 << 26) | (1 << 29);
	hooks.pfnTouch(&box, &self);
	box.v.model = 22;
	hooks.pfnTouch(&box, &self);
	if (!check(pickupDropCount == 1, "same gun and lower-ranked ground gun keep the acquired primary"))
		return false;
	self.v.weapons = (1 << 22) | (1 << 26);
	box.v.model = 21;
	const auto score = [&](int flags)
	{
		runtime.onMessageBegin(0, 101, nullptr, nullptr);
		runtime.onWriteByte(1);
		runtime.onWriteByte(flags);
		runtime.onMessageEnd();
	};
	score(4);
	hooks.pfnTouch(&box, &self);
	score(0);
	runtime.setCompatibilityFloat("bot_stop", 1);
	hooks.pfnTouch(&box, &self);
	runtime.setCompatibilityFloat("bot_stop", 0);
	runtime.setCompatibilityFloat("bot_enable", 0);
	hooks.pfnTouch(&box, &self);
	runtime.setCompatibilityFloat("bot_enable", 1);
	hooks.pfnTouch(&self, &box);
	box.v.flags = 0;
	hooks.pfnTouch(&box, &self);
	box.v.flags = FL_ONGROUND;
	for (int model : {23, 24, 25})
	{
		box.v.model = model;
		hooks.pfnTouch(&box, &self);
	}
	box.v.model = 21;
	box.v.classname = 26;
	hooks.pfnTouch(&box, &self);
	box.v.classname = 20;
	self.v.flags |= FL_FROZEN;
	hooks.pfnTouch(&box, &self);
	self.v.flags &= ~FL_FROZEN;
	self.v.deadflag = DEAD_DEAD;
	hooks.pfnTouch(&box, &self);
	self.v.deadflag = DEAD_NO;
	box.free = TRUE;
	hooks.pfnTouch(&box, &self);
	box.free = FALSE;
	auto &human = gEntities[2];
	human.v.flags = FL_CLIENT | FL_ONGROUND;
	human.v.health = 100;
	human.v.deadflag = DEAD_NO;
	human.v.weapons = 1 << 22;
	hooks.pfnTouch(&box, &human);
	self.v.weapons = (1 << 26);
	hooks.pfnTouch(&box, &self);
	self.v.weapons = (1 << 22) | (1 << 28);
	hooks.pfnTouch(&box, &self);
	if (!check(pickupDropCount == 1,
			"reverse callbacks, airborne/invalid boxes, non-primary models, frozen/dead/unmanaged actors and empty/ambiguous primary slots never drop"))
		return false;
	self.v.weapons = (1 << 22) | (1 << 26);
	gEntities[1].v.flags = FL_CLIENT | FL_ONGROUND;
	gEntities[1].v.deadflag = DEAD_NO;
	gEntities[1].v.health = 100;
	gEntities[1].v.origin = Vector(120, 0, 36);
	gEntities[1].v.team = 2;
	gObjectiveTraceVisible = true;
	for (unsigned i = 0; i < 8; ++i)
		step(.125f);
	hooks.pfnTouch(&box, &self);
	if (!check(pickupDropCount == 1, "a publicly visible enemy suppresses weapon exchange"))
		return false;
	gEntities[1].v.deadflag = DEAD_DEAD;
	gEntities[1].v.health = 0;
	gObjectiveTraceVisible = false;
	step(2.0f);
	hooks.pfnTouch(&box, &self);
	if (!check(pickupDropCount == 1, "exchange remains suppressed during the post-visibility safe interval"))
		return false;
	step(1.0f);
	hooks.pfnTouch(&box, &self);
	if (!check(pickupDropCount == 2, "exchange resumes after the visible enemy has been absent for 2.5 seconds"))
		return false;
	runtime.onServerDeactivate();
	hooks.pfnTouch(&box, &self);
	return check(pickupDropCount == 2, "inactive map ignores weapon touches");
}
}
