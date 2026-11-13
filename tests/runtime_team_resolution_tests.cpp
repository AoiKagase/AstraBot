#include "plugin_runtime.hpp"

namespace
{
bool testRuntimeTeams(astrabot::metamod::PluginRuntime &, enginefuncs_t &, globalvars_t &);
}

#define ASTRABOT_EXTRA_ACTOR_TEST testRuntimeTeams
#include "compat_actor_command_tests.cpp"

namespace
{
bool testRuntimeTeams(astrabot::metamod::PluginRuntime &runtime,
	enginefuncs_t &engine, globalvars_t &globals)
{
	using namespace astrabot;
	const char *path = "astrabot_runtime_teams.nav";
	if (!writeTwoSiteNavigation(path, true, true))
	{
		return false;
	}
	const metamod::NavLoadRequest load = {path, "astrabot_runtime_teams",
		runtime.snapshot().mapGeneration, false, 0U, false, 0U};
	const auto loaded = runtime.loadNavigationFile(&load);
	std::remove(path);
	if (!check(loaded == metamod::NavLoadResult::Loaded, "team regression NAV loaded"))
	{
		return false;
	}
	runtime.executeCompatibilityCommand(request("bot_kick", 1U, "all"));
	resetEntities();
	gCreateCount = 0;
	gServerSetsEntityTeam = true;
	if (!check(runtime.executeCompatibilityCommand(request("bot_add_t", 1U, "T Bot")) ==
		metamod::CompatibilityCommandResult::Handled, "fresh team regression bot created"))
	{
		return false;
	}
	engine.pfnTraceLine = &traceObjectiveVisibility;
	engine.pfnRandomFloat = &siteFixtureRandomFloat;
	engine.pfnRandomLong = &siteFixtureRandomLong;
	runtime.giveEnginePointers(&engine, &globals);
	globals.mapname = 6;
	globals.maxEntities = 8;
	gObjectiveEntitiesAvailable = true;
	gObjectiveTraceVisible = true;
	gCaptureMovementLogs = true;
	gProfileEnabled = true;
	for (unsigned i = 1U; i < 5U; ++i)
	{
		gEntities[i].v.flags = 0;
		gEntities[i].v.health = 0;
	}
	auto &self = gEntities[0];
	self.v.flags = FL_CLIENT | FL_FAKECLIENT | FL_ONGROUND;
	self.v.health = 100;
	self.v.deadflag = DEAD_NO;
	self.v.team = 1;
	self.v.solid = SOLID_SLIDEBOX;
	self.v.movetype = MOVETYPE_WALK;
	self.v.mins = Vector(-16, -16, -36);
	self.v.maxs = Vector(16, 16, 36);
	self.v.origin = Vector(80, 32, 36);
	self.v.view_ofs = Vector(0, 0, 17);
	self.v.maxspeed = 240;
	self.v.weapons = 1 << 22;
	gPlantedBomb = {};
	gPlantedBomb.v.classname = 2;
	gPlantedBomb.v.model = 3;
	gPlantedBomb.serialnumber = 5;
	gPlantedBomb.v.origin = Vector(224, 32, 36);
	gPlantedBomb.v.dmgtime = globals.time + 100;
	runtime.onMessageBegin(MSG_ALL, 98, nullptr, nullptr);
	runtime.onWriteByte(4);
	runtime.onWriteString("#Bomb_Planted");
	runtime.onMessageEnd();
	gMovementLogs.clear();
	for (unsigned i = 0U; i < 24U; ++i)
	{
		globals.time += 0.125f;
		runtime.onStartFrame();
		runtime.onStartFramePost();
	}
	bool scenarioGoalObserved = false;
	for (const auto &message : gMovementLogs)
	{
		if (message.find("profile goalAssignment ") == 0U &&
			message.find("goal_source=objective") != std::string::npos)
		{
			scenarioGoalObserved = true;
			if (!check(message.find("effective_team=1 ") != std::string::npos,
				"scenario target without an objective cache still resolves the navigation team"))
			{
				return false;
			}
		}
	}
	if (!check(scenarioGoalObserved, "regression exercises a scenario-owned navigation goal"))
	{
		for (const auto &message : gMovementLogs)
		{
			std::fprintf(stderr, "%s\n", message.c_str());
		}
		return false;
	}
	self.v.team = 2;
	globals.time += 0.125f;
	runtime.onStartFrame();
	runtime.onStartFramePost();
	auto &views = static_cast<metamod::BotBehaviorServices &>(runtime);
	const auto &belief = views.observation_managedBotPerception(0U).snapshot();
	const world::ActorObservation *observer = nullptr;
	if (!check(belief.findActor(belief.identity().observer, &observer) ==
		world::ContactLookupResult::Found && observer != nullptr &&
		observer->relation == world::TeamRelation::Friendly,
		"TeamInfo wins over stale raw team for the snapshot observer too"))
	{
		return false;
	}
	auto &human = gEntities[4];
	human.v.flags = FL_CLIENT;
	human.v.team = 1;
	runtime.onClientPutInServer(&human);
	const auto teamInfo = [&runtime](const char *name)
	{
		runtime.onMessageBegin(MSG_ALL, 86, nullptr, nullptr);
		runtime.onWriteByte(5);
		runtime.onWriteString(name);
		runtime.onMessageEnd();
	};
	if (!check(views.observation_resolvePlayerTeam(&human) == 1,
		"a player without TeamInfo uses the valid public raw team"))
	{
		return false;
	}
	teamInfo("CT");
	if (!check(views.observation_resolvePlayerTeam(&human) == 2,
		"human TeamInfo overrides a conflicting raw team"))
	{
		return false;
	}
	teamInfo("SPECTATOR");
	if (!check(views.observation_resolvePlayerTeam(&human) == 0,
		"known spectator TeamInfo overrides a stale active raw team"))
	{
		return false;
	}
	runtime.onClientDisconnect(&human);
	runtime.onClientPutInServer(&human);
	if (!check(views.observation_resolvePlayerTeam(&human) == 1,
		"a reused human slot cannot retain the previous TeamInfo"))
	{
		return false;
	}
	human.v.team = 0;
	return check(views.observation_resolvePlayerTeam(&human) == 0,
		"unknown membership is never replaced with an admission request");
}
}
