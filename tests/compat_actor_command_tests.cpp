#include "astrabot/metamod/abi_contract.hpp"
#include "plugin_runtime.hpp"

#include <cstdio>
#include <cstring>

namespace
{
edict_t gEntities[5]{};
edict_t gBombSite{};
edict_t gPlantedBomb{};
bool gObjectiveEntitiesAvailable = false;
	int gCreateCount = 0;
	int gKillCount = 0;
	int gClientCommandCount = 0;
	int gHookedClientCommandCount = 0;
	int gUnhookedClientCommandCount = 0;
	int gRunPlayerMoveCount = 0;
	unsigned short gLastRunPlayerMoveButtons = 0U;
	bool gDirectJoinCommandSeen = false;
	bool gServerSetsEntityTeam = true;
	char gLastClientCommand[32] = {};
	int gClientKeyValueCount = 0;
	bool gMenuTeamContextValid = false;
	bool gMenuClassContextValid = false;
	DLL_FUNCTIONS gHookedGameDllTable{};
	bool gNativeControlsAvailable = false;
	bool gCompatibilityCvarsRegistered[6] = {};
	int gCvarRegisterCount = 0;
	float gBotEnable = 0.0f;
	float gBotQuota = 0.0f;
	cvar_t gBotEnableCvar{};
	cvar_t gBotStopCvar{};
	cvar_t gBotDifficultyCvar{};
	cvar_t gBotQuotaCvar{};
	cvar_t gBotJoinTeamCvar{};
	cvar_t gAstrabotModeCvar{};

	bool check(bool condition, const char *description)
	{
		if (condition)
		{
			return true;
		}

		std::fprintf(stderr, "check failed: %s\n", description);
		return false;
	}

	std::FILE *openFixture(const char *path, const char *mode)
	{
		std::FILE *file = nullptr;
#ifdef _WIN32
		if (fopen_s(&file, path, mode) != 0)
		{
			return nullptr;
		}
#else
		file = std::fopen(path, mode);
#endif
		return file;
	}

	edict_t *createFakeClient(const char *)
	{
		if (gCreateCount >= 5)
		{
			return nullptr;
		}
		edict_t *entity = &gEntities[gCreateCount++];
		entity->v.flags |= FL_FAKECLIENT;
		return entity;
	}

int indexOfEdict(const edict_t *entity)
	{
		for (int index = 0; index < 5; ++index)
		{
			if (entity == &gEntities[index])
			{
				return index + 1;
			}
		}
	return -1;
}

int gFindEntityByStringCount = 0;

edict_t *findEntityByString(edict_t *start, const char *field, const char *value)
{
	++gFindEntityByStringCount;
	if (!gObjectiveEntitiesAvailable || start != nullptr || field == nullptr || value == nullptr ||
		std::strcmp(field, "classname") != 0)
	{
		return nullptr;
	}
	if (std::strcmp(value, "func_bomb_target") == 0)
	{
		return &gBombSite;
	}
	if (std::strcmp(value, "grenade") == 0)
	{
		return &gPlantedBomb;
	}
	return nullptr;
}

	const char *szFromIndex(int index)
	{
		return index == 1 ? "func_bomb_target" : index == 2 ? "grenade" : "";
	}

	edict_t *entityOfIndex(int index)
	{
		if (index >= 1 && index <= 5)
		{
			return &gEntities[index - 1];
		}
		if (index == 6)
		{
			return &gBombSite;
		}
		if (index == 7)
		{
			return &gPlantedBomb;
		}
		return nullptr;
	}

	void getGameDir(char *buffer)
	{
	if (buffer != nullptr)
	{
		buffer[0] = '.';
		buffer[1] = '\0';
	}
	}

	cvar_t *getCvar(const char *name)
	{
		if (std::strcmp(name, "bot_enable") == 0)
		{
			return gNativeControlsAvailable || gCompatibilityCvarsRegistered[0] ? &gBotEnableCvar
																				: nullptr;
		}
		if (std::strcmp(name, "bot_stop") == 0)
		{
			return gNativeControlsAvailable || gCompatibilityCvarsRegistered[1] ? &gBotStopCvar
																				: nullptr;
		}
		if (std::strcmp(name, "bot_difficulty") == 0)
		{
			return gNativeControlsAvailable || gCompatibilityCvarsRegistered[2]
					   ? &gBotDifficultyCvar
					   : nullptr;
		}
		if (std::strcmp(name, "bot_quota") == 0)
		{
			return gNativeControlsAvailable || gCompatibilityCvarsRegistered[3] ? &gBotQuotaCvar
																				: nullptr;
		}
	if (std::strcmp(name, "bot_join_team") == 0)
	{
		return gNativeControlsAvailable || gCompatibilityCvarsRegistered[4] ? &gBotJoinTeamCvar
			: nullptr;
	}
	if (std::strcmp(name, "astrabot_mode") == 0)
	{
		return gNativeControlsAvailable || gCompatibilityCvarsRegistered[5] ? &gAstrabotModeCvar
			: nullptr;
	}
		return nullptr;
	}

	float getCvarFloat(const char *name)
	{
		if (std::strcmp(name, "bot_enable") == 0)
		{
			return gBotEnable;
		}
		if (std::strcmp(name, "bot_quota") == 0)
		{
			return gBotQuota;
		}
		return 0.0f;
	}

	void setCvarFloat(const char *name, float value)
	{
		if (std::strcmp(name, "bot_enable") == 0)
		{
			gBotEnable = value;
		}
		else if (std::strcmp(name, "bot_quota") == 0)
		{
			gBotQuota = value;
		}
	}

	void registerCvar(cvar_t *variable)
	{
		++gCvarRegisterCount;
		if (variable == nullptr || variable->name == nullptr)
		{
			return;
		}
		const char *const names[] = {"bot_enable", "bot_stop", "bot_difficulty", "bot_quota",
			"bot_join_team", "astrabot_mode"};
		for (std::size_t index = 0U; index < 6U; ++index)
		{
			if (std::strcmp(variable->name, names[index]) == 0)
			{
				gCompatibilityCvarsRegistered[index] = true;
				return;
			}
		}
	}

	int playerUserId(edict_t *entity) { return indexOfEdict(entity) + 100; }

	void putInServer(edict_t *) {}
	qboolean clientConnect(edict_t *, const char *, const char *, char[128]) { return true; }

	void disconnect(edict_t *entity)
	{
		if (entity != nullptr)
		{
			entity->v.flags &= ~FL_FAKECLIENT;
		}
	}

	void serverCommand(char *) {}

	void serverExecute() {}

	void resetEntities()
	{
		for (edict_t &entity : gEntities)
		{
			entity = edict_t{};
		}
	}

	void clientCommand(edict_t *, char *command, ...)
	{
		++gClientCommandCount;
		if (command != nullptr)
		{
			const std::size_t length = std::strlen(command);
			const std::size_t copyLength =
				length < sizeof(gLastClientCommand) - 1U ? length : sizeof(gLastClientCommand) - 1U;
			std::memcpy(gLastClientCommand, command, copyLength);
		gLastClientCommand[copyLength] = '\0';
	}
}

void runPlayerMove(
	edict_t *, const float *, float, float, float, unsigned short buttons, byte, byte)
{
	++gRunPlayerMoveCount;
	gLastRunPlayerMoveButtons = buttons;
}

char *getInfoKeyBuffer(edict_t *)
	{
		static char buffer[128] = {};
		return buffer;
	}

	void setClientKeyValue(int, char *, char *, char *) { ++gClientKeyValueCount; }

	void gameClientCommand(edict_t *entity)
	{
		++gClientCommandCount;
		if (astrabot::metamod::HookCommandArgc() == 2)
		{
			const char *const command = astrabot::metamod::HookCommandArgv(0);
			const char *const selection = astrabot::metamod::HookCommandArgv(1);
		if (std::strcmp(command, "jointeam") == 0)
		{
			gDirectJoinCommandSeen = true;
		if (entity != nullptr && gServerSetsEntityTeam)
		{
			entity->v.team = 1;
			}
			astrabot::metamod::PluginRuntime &runtime =
					astrabot::metamod::PluginRuntime::instance();
			runtime.onMessageBegin(0, 86, nullptr, nullptr);
			runtime.onWriteByte(indexOfEdict(entity));
			runtime.onWriteString("TERRORIST");
			runtime.onMessageEnd();
		}
		else if (std::strcmp(command, "joinclass") == 0)
		{
			gDirectJoinCommandSeen = true;
		}
		else if (std::strcmp(command, "menuselect") == 0)
		{
			if (std::strcmp(selection, "1") == 0 || std::strcmp(selection, "2") == 0 ||
					std::strcmp(selection, "5") == 0)
			{
				gMenuTeamContextValid = gMenuTeamContextValid || gClientCommandCount == 1;
				gMenuClassContextValid = gMenuClassContextValid || gClientCommandCount >= 2;
			}
			}
		}
	}

	void unhookedGameClientCommand(edict_t *)
	{
		++gUnhookedClientCommandCount;
	}

	void hookedGameClientCommand(edict_t *entity)
	{
		++gHookedClientCommandCount;
		gameClientCommand(entity);
	}

	void getHookTables(plid_t, enginefuncs_t **engineFunctions,
						 DLL_FUNCTIONS **gameDllFunctions, NEW_DLL_FUNCTIONS **newDllFunctions)
	{
		if (engineFunctions != nullptr)
		{
			*engineFunctions = nullptr;
		}
		if (gameDllFunctions != nullptr)
		{
			*gameDllFunctions = &gHookedGameDllTable;
		}
		if (newDllFunctions != nullptr)
		{
			*newDllFunctions = nullptr;
		}
	}

	void clientKill(edict_t *) { ++gKillCount; }

	astrabot::compat::CommandRequest request(const char *name, std::size_t argumentCount = 0U,
											 const char *firstArgument = nullptr)
	{
		return {name, argumentCount, {firstArgument, nullptr}};
	}
} // namespace

int main()
{
	using astrabot::metamod::CompatibilityCommandResult;
	using astrabot::metamod::FakeClientHandle;
	using astrabot::metamod::FakeClientResult;
	using astrabot::metamod::PluginRuntime;

	const char *const profilePath = "astrabot_actor_command_profile.db";
	const char profileText[] =
		"AnyProfile { Name = \"Any Bot\" Team = any Difficulty = 1 Skill = 50 }\n"
		"CtProfile { Name = \"CT Bot\" Team = CT Difficulty = 1 Skill = 60 }\n"
		"TProfile { Name = \"T Bot\" Team = T Difficulty = 1 Skill = 70 }\n"
		"NamedProfile { Name = \"Named Bot\" Team = any Difficulty = 1 Skill = 80 }\n";
	std::FILE *profileFile = openFixture(profilePath, "wb");
	if (!check(profileFile != nullptr, "actor command profile fixture opens"))
	{
		return 1;
	}
	const std::size_t profileLength = std::strlen(profileText);
	const std::size_t profileWritten = std::fwrite(profileText, 1U, profileLength, profileFile);
	const int profileCloseResult = std::fclose(profileFile);
	if (!check(profileWritten == profileLength && profileCloseResult == 0,
			   "actor command profile fixture is written"))
	{
		std::remove(profilePath);
		return 1;
	}

	const char *const defaultProfilePath = "BotProfile.db";
	std::FILE *defaultProfileFile = openFixture(defaultProfilePath, "wb");
	if (!check(defaultProfileFile != nullptr, "default BotProfile fixture opens"))
	{
		std::remove(profilePath);
		return 1;
	}
	const std::size_t defaultProfileWritten =
		std::fwrite(profileText, 1U, profileLength, defaultProfileFile);
	const int defaultProfileCloseResult = std::fclose(defaultProfileFile);
	if (!check(defaultProfileWritten == profileLength && defaultProfileCloseResult == 0,
			   "default BotProfile fixture is written"))
	{
		std::remove(profilePath);
		std::remove(defaultProfilePath);
		return 1;
	}

	meta_globals_t metaGlobals{};
	DLL_FUNCTIONS gameDllTable{};
	gameDllTable.pfnClientConnect = &clientConnect;
	gameDllTable.pfnClientPutInServer = &putInServer;
	gameDllTable.pfnClientDisconnect = &disconnect;
	gameDllTable.pfnClientKill = &clientKill;
	gameDllTable.pfnClientCommand = &gameClientCommand;
	gHookedGameDllTable = gameDllTable;
	gHookedGameDllTable.pfnClientCommand = &hookedGameClientCommand;
	gamedll_funcs_t gameDllFunctions{};
	gameDllFunctions.dllapi_table = &gameDllTable;
	META_FUNCTIONS metaFunctions{};
	mutil_funcs_t metaUtils{};
	metaUtils.pfnGetHookTables = &getHookTables;
	char metaInterfaceVersion[] = META_INTERFACE_VERSION;
	plugin_info_t *pluginInfo = nullptr;
	if (!check(astrabot::metamod::Meta_Query(metaInterfaceVersion, &pluginInfo, &metaUtils) == TRUE,
			   "Meta_Query installs the hook-table test boundary"))
	{
		std::remove(profilePath);
		std::remove(defaultProfilePath);
		return 1;
	}
	PluginRuntime &runtime = PluginRuntime::instance();
	globalvars_t globals{};
	globals.maxClients = 5;
	globals.maxEntities = 7;
	globals.time = 1.0f;
	runtime.giveEnginePointers(nullptr, &globals);
	if (!check(runtime.attach(PT_ANYTIME, &metaFunctions, &metaGlobals, &gameDllFunctions, nullptr),
			   "runtime attaches for actor command test"))
	{
		std::remove(profilePath);
		return 1;
	}

	enginefuncs_t engineFunctions{};
	engineFunctions.pfnCreateFakeClient = &createFakeClient;
	engineFunctions.pfnIndexOfEdict = &indexOfEdict;
	engineFunctions.pfnPEntityOfEntIndex = &entityOfIndex;
	engineFunctions.pfnFindEntityByString = &findEntityByString;
	engineFunctions.pfnSzFromIndex = &szFromIndex;
	engineFunctions.pfnCVarGetPointer = &getCvar;
	engineFunctions.pfnCVarGetFloat = &getCvarFloat;
	engineFunctions.pfnCVarSetFloat = &setCvarFloat;
	engineFunctions.pfnCvar_RegisterVariable = &registerCvar;
	engineFunctions.pfnClientCommand = &clientCommand;
	engineFunctions.pfnRunPlayerMove = &runPlayerMove;
	engineFunctions.pfnGetInfoKeyBuffer = &getInfoKeyBuffer;
	engineFunctions.pfnSetClientKeyValue = &setClientKeyValue;
	engineFunctions.pfnGetGameDir = &getGameDir;
	engineFunctions.pfnGetPlayerUserId = &playerUserId;
	engineFunctions.pfnServerCommand = &serverCommand;
	engineFunctions.pfnServerExecute = &serverExecute;
	runtime.giveEnginePointers(&engineFunctions, &globals);
	gBotEnable = 1.0f;
	gBotQuota = 1.0f;
	runtime.onServerActivate(nullptr, 0, 4);
	runtime.onStartFrame();
	if (!check(runtime.executeCompatibilityCommand(request("bot_add")) ==
				  CompatibilityCommandResult::Handled &&
				 gCreateCount == 1 && gCvarRegisterCount == 6 &&
				  gClientCommandCount == 0 && gClientKeyValueCount >= 2,
			   "bot_add loads the default BotProfile database during activation"))
	{
		std::remove(profilePath);
		std::remove(defaultProfilePath);
			return 1;
		}
		gServerSetsEntityTeam = false;
		gRunPlayerMoveCount = 0;
	gLastRunPlayerMoveButtons = 0U;
	globals.time = 1.1f;
	runtime.onStartFrame();
	runtime.onStartFramePost();
	if (!check(gRunPlayerMoveCount == 1 &&
				  gLastRunPlayerMoveButtons == static_cast<unsigned short>(IN_ATTACK) &&
				  runtime.movementPhysicsSample(1U).readiness ==
					  astrabot::runtime::SpawnReadiness::NotReady,
			  "initial joining heartbeat advances the public player lifecycle"))
	{
		std::remove(profilePath);
		std::remove(defaultProfilePath);
		return 1;
	}
	globals.time = 1.1f;
	for (int frame = 0; frame < 8; ++frame)
	{
		runtime.onStartFrame();
		runtime.onStartFramePost();
	}
	if (!check(gClientCommandCount == 2 && gDirectJoinCommandSeen && !gMenuTeamContextValid &&
				   !gMenuClassContextValid,
			   "public jointeam and joinclass commands are sent after bounded frame delays"))
	{
		std::remove(profilePath);
		std::remove(defaultProfilePath);
		return 1;
	}
	gRunPlayerMoveCount = 0;
	globals.time += 0.1f;
	runtime.onStartFrame();
	runtime.onStartFramePost();
	if (!check(gRunPlayerMoveCount == 1 && gLastRunPlayerMoveButtons == 0U &&
				  runtime.movementPhysicsSample(1U).readiness ==
					  astrabot::runtime::SpawnReadiness::NotReady,
			  "joining actor receives a neutral heartbeat while awaiting physical spawn"))
	{
		std::remove(profilePath);
		std::remove(defaultProfilePath);
		return 1;
	}
	gEntities[0].v.flags &= ~FL_SPECTATOR;
	gEntities[0].v.flags |= (FL_CLIENT | FL_FAKECLIENT | FL_ONGROUND);
	gEntities[0].v.deadflag = DEAD_NO;
	gEntities[0].v.health = 100.0f;
	gEntities[0].v.solid = SOLID_SLIDEBOX;
	gEntities[0].v.movetype = MOVETYPE_WALK;
	gEntities[0].v.flags &= ~FL_FAKECLIENT;
	runtime.onMessageBegin(0, 86, nullptr, nullptr);
	runtime.onWriteByte(1);
	runtime.onWriteString("TERRORIST");
	runtime.onMessageEnd();
	gEntities[0].v.team = 0;
	gEntities[1].v.flags = FL_CLIENT | FL_ONGROUND;
	gEntities[1].v.team = 2;
	gEntities[1].v.deadflag = DEAD_NO;
	gEntities[1].v.health = 100.0f;
	gEntities[1].v.origin.x = 64.0f;
	gEntities[1].v.origin.y = 0.0f;
	gEntities[1].v.origin.z = 0.0f;
	gRunPlayerMoveCount = 0;
	for (int frame = 0; frame < 6; ++frame)
	{
		globals.time += 0.1f;
		runtime.onStartFrame();
		runtime.onStartFramePost();
	}
	if (!check(gRunPlayerMoveCount == 6 &&
			(gLastRunPlayerMoveButtons & static_cast<unsigned short>(IN_ATTACK)) == 0U,
			"joined actor receives neutral heartbeat without live Fire"))
	{
		std::remove(profilePath);
		std::remove(defaultProfilePath);
		return 1;
	}
	const auto physicsSample = runtime.movementPhysicsSample(1U);
	if (!check(physicsSample.dispatched &&
				  physicsSample.readiness == astrabot::runtime::SpawnReadiness::Ready,
				  "heartbeat records dispatched spawn-ready physics sample"))
	{
		std::remove(profilePath);
		std::remove(defaultProfilePath);
		return 1;
	}
	gEntities[0].v.team = 2;
	gEntities[1].v.team = 1;
	gEntities[1].v.flags = FL_CLIENT | FL_ONGROUND;
	gEntities[1].v.deadflag = DEAD_NO;
	gEntities[1].v.health = 100.0f;
	gBombSite.v.origin = gEntities[0].v.origin;
	gBombSite.v.classname = 1;
	gPlantedBomb.v.classname = 2;
	gPlantedBomb.v.origin = gEntities[0].v.origin;
	gPlantedBomb.v.dmgtime = globals.time + 30.0f;
	gObjectiveEntitiesAvailable = true;
	gFindEntityByStringCount = 0;
	gRunPlayerMoveCount = 0;
	globals.time += 0.1f;
	runtime.onStartFrame();
	runtime.onStartFramePost();
	if (!check(gRunPlayerMoveCount == 1 &&
			(gLastRunPlayerMoveButtons & static_cast<unsigned short>(IN_USE)) != 0U,
			"planted bomb objective reaches RunPlayerMove as a use action"))
	{
		std::remove(profilePath);
		std::remove(defaultProfilePath);
		return 1;
	}
	if (!check(gFindEntityByStringCount == 0,
			"planted bomb objective avoids Metamod FindEntityByString re-entry"))
	{
		std::remove(profilePath);
		std::remove(defaultProfilePath);
		return 1;
	}
	gObjectiveEntitiesAvailable = false;
	gRunPlayerMoveCount = 0;
	gLastRunPlayerMoveButtons = 0U;
	globals.time += 0.1f;
	runtime.onStartFrame();
	runtime.onStartFramePost();
	if (!check((gLastRunPlayerMoveButtons & static_cast<unsigned short>(IN_ATTACK)) == 0U,
			"live combat does not dispatch attack without an active weapon boundary"))
	{
		std::remove(profilePath);
		std::remove(defaultProfilePath);
		return 1;
	}
	gServerSetsEntityTeam = true;
	gEntities[0].v.flags |= FL_SPECTATOR;
	gEntities[0].v.deadflag = DEAD_DEAD;
	gEntities[0].v.health = 0.0f;
	gRunPlayerMoveCount = 0;
	globals.time += 0.1f;
	runtime.onStartFrame();
	runtime.onStartFramePost();
	if (!check(gRunPlayerMoveCount == 0 &&
				  runtime.movementPhysicsSample(1U).readiness ==
					  astrabot::runtime::SpawnReadiness::NotReady,
				   "joined spectator/dead actor does not receive RunPlayerMove"))
	{
		std::remove(profilePath);
		std::remove(defaultProfilePath);
		return 1;
	}
	gEntities[0].v.flags &= ~FL_SPECTATOR;
	gEntities[0].v.deadflag = DEAD_NO;
	gEntities[0].v.health = 100.0f;
	gEntities[0].v.flags &= ~FL_ONGROUND;
	gRunPlayerMoveCount = 0;
	globals.time += 0.1f;
	runtime.onStartFrame();
	runtime.onStartFramePost();
	if (!check(gRunPlayerMoveCount == 0,
			   "respawning actor does not receive input before grounded readiness"))
	{
		std::remove(profilePath);
		std::remove(defaultProfilePath);
		return 1;
	}
	if (!check(runtime.executeCompatibilityCommand(request("bot_kick", 1U, "all")) ==
				   CompatibilityCommandResult::Handled,
			   "default profile actor can be removed before explicit profile test"))
	gCreateCount = 0;
	gClientCommandCount = 0;
	gDirectJoinCommandSeen = false;
	gMenuTeamContextValid = false;
	gMenuClassContextValid = false;
	resetEntities();
	runtime.setCompatibilityFloat("bot_enable", 1.0f);
	runtime.setCompatibilityFloat("bot_quota", 1.0f);
	gBotEnable = 1.0f;
	gBotQuota = 1.0f;
	globals.time = 1.4f;
	runtime.onStartFrame();
	const CompatibilityCommandResult fallbackResult =
		runtime.executeCompatibilityCommand(request("bot_add_t"));
		if (!check(fallbackResult == CompatibilityCommandResult::Handled,
				   "fallback test accepts the terrorist add command"))
	{
		std::remove(profilePath);
		std::remove(defaultProfilePath);
			return 1;
		}
		if (!check(gCreateCount > 0, "fallback test creates a pending terrorist actor"))
	{
		std::remove(profilePath);
		std::remove(defaultProfilePath);
		return 1;
	}
	for (int frame = 0; frame < 8; ++frame)
	{
		globals.time += 0.1f;
		runtime.onStartFrame();
		runtime.onStartFramePost();
	}
	if (!check(gClientCommandCount == 2 && gDirectJoinCommandSeen &&
				   !gMenuTeamContextValid && !gMenuClassContextValid,
				   "fallback uses public join commands when menu messages are absent"))
	{
		std::remove(profilePath);
		std::remove(defaultProfilePath);
			return 1;
		}
		runtime.executeCompatibilityCommand(request("bot_kick", 1U, "all"));
	gCreateCount = 0;
	gClientCommandCount = 0;
	resetEntities();
	gameDllTable.pfnClientCommand = nullptr;
	gHookedGameDllTable.pfnClientCommand = nullptr;
	globals.time += 0.1f;
	runtime.onStartFrame();
	if (!check(runtime.executeCompatibilityCommand(request("bot_add")) ==
				   CompatibilityCommandResult::Handled,
			   "failed join scenario creates a managed actor"))
		{
			std::remove(profilePath);
			std::remove(defaultProfilePath);
			return 1;
		}
	for (int frame = 0; frame < 8; ++frame)
	{
		globals.time += 0.1f;
		runtime.onStartFrame();
		runtime.onStartFramePost();
	}
	gameDllTable.pfnClientCommand = &gameClientCommand;
	gHookedGameDllTable.pfnClientCommand = &hookedGameClientCommand;
	if (!check(runtime.executeCompatibilityCommand(request("bot_add")) ==
				   CompatibilityCommandResult::Handled,
			   "failed join cleanup releases managed actor for retry"))
		{
			std::remove(profilePath);
			std::remove(defaultProfilePath);
			return 1;
		}
		runtime.executeCompatibilityCommand(request("bot_kick", 1U, "all"));
	gCreateCount = 0;
	gKillCount = 0;
	gClientCommandCount = 0;
	std::memset(gLastClientCommand, 0, sizeof(gLastClientCommand));
	gClientKeyValueCount = 0;
	resetEntities();
	runtime.setCompatibilityFloat("bot_enable", 0.0f);
	runtime.setCompatibilityFloat("bot_quota", 0.0f);
	gBotEnable = 1.0f;
	gBotQuota = 2.0f;
	globals.time = 1.5f;
	runtime.onStartFrame();
	if (!check(runtime.executeCompatibilityCommand(request("bot_add")) ==
					   CompatibilityCommandResult::Handled &&
				   gCreateCount == 1,
			   "runtime adopts changed native bot controls before command execution"))
	{
		std::remove(profilePath);
		std::remove(defaultProfilePath);
		return 1;
	}
	if (!check(runtime.executeCompatibilityCommand(request("bot_kick", 1U, "all")) ==
				   CompatibilityCommandResult::Handled,
			   "changed native control actor can be removed"))
	{
		std::remove(profilePath);
		std::remove(defaultProfilePath);
		return 1;
	}
	runtime.executeCompatibilityCommand(request("bot_kick", 1U, "all"));
	gCreateCount = 0;
	gClientCommandCount = 0;
	gHookedClientCommandCount = 0;
	gUnhookedClientCommandCount = 0;
	gDirectJoinCommandSeen = false;
	gMenuTeamContextValid = false;
	gMenuClassContextValid = false;
	resetEntities();
	gameDllTable.pfnClientCommand = &unhookedGameClientCommand;
	gHookedGameDllTable.pfnClientCommand = &hookedGameClientCommand;
	globals.time += 0.1f;
	runtime.onStartFrame();
	if (!check(runtime.executeCompatibilityCommand(request("bot_add_t")) ==
				   CompatibilityCommandResult::Handled,
				   "hook-table dispatch test creates a terrorist actor"))
	{
		std::remove(profilePath);
		std::remove(defaultProfilePath);
		return 1;
	}
	for (int frame = 0; frame < 8; ++frame)
	{
		globals.time += 0.1f;
		runtime.onStartFrame();
		runtime.onStartFramePost();
	}
	if (!check(gHookedClientCommandCount == 2 && gUnhookedClientCommandCount == 0,
			   "join menu selections use the Metamod hook-table GameDLL dispatcher"))
	{
		std::remove(profilePath);
		std::remove(defaultProfilePath);
		return 1;
	}
	runtime.executeCompatibilityCommand(request("bot_kick", 1U, "all"));
	gameDllTable.pfnClientCommand = &gameClientCommand;
	gHookedGameDllTable.pfnClientCommand = &hookedGameClientCommand;
	gCreateCount = 0;
	gClientCommandCount = 0;
	gHookedClientCommandCount = 0;
	gUnhookedClientCommandCount = 0;
	resetEntities();
	gBotEnable = 1.0f;
	gBotQuota = 1.0f;
	globals.time += 0.1f;
	runtime.onStartFrame();
	gameDllTable.pfnClientCommand = &gameClientCommand;
	gHookedGameDllTable.pfnClientCommand = nullptr;
	if (!check(runtime.executeCompatibilityCommand(request("bot_add_t")) ==
				   CompatibilityCommandResult::Handled,
				   "partial hook-table dispatch test creates a terrorist actor"))
	{
		std::remove(profilePath);
		std::remove(defaultProfilePath);
		return 1;
	}
	for (int frame = 0; frame < 8; ++frame)
	{
		globals.time += 0.1f;
		runtime.onStartFrame();
		runtime.onStartFramePost();
	}
	if (!check(gClientCommandCount == 2 && gHookedClientCommandCount == 0 &&
			   gUnhookedClientCommandCount == 0,
			   "join dispatch falls back when the hook table lacks pfnClientCommand"))
	{
		std::remove(profilePath);
		std::remove(defaultProfilePath);
		return 1;
	}
	runtime.executeCompatibilityCommand(request("bot_kick", 1U, "all"));
	gHookedGameDllTable.pfnClientCommand = &hookedGameClientCommand;
	gCreateCount = 0;
	resetEntities();
	gBotEnable = 0.0f;
	gBotQuota = 0.0f;
	runtime.setCompatibilityFloat("bot_enable", 0.0f);
	runtime.setCompatibilityFloat("bot_quota", 0.0f);
	if (!check(runtime.snapshot().managedBotCreationAllowed,
			   "native guard allows managed actor creation"))
	{
		std::remove(profilePath);
		return 1;
	}
	if (!check(runtime.loadCompatibilityProfiles(profilePath) ==
				   astrabot::metamod::ProfileLoadResult::Loaded,
			   "runtime loads actor command profiles"))
	{
		std::remove(profilePath);
		return 1;
	}
	if (!check(runtime.setCompatibilityFloat("bot_enable", 1.0f) ==
					   astrabot::compat::CvarUpdateResult::Updated &&
				   runtime.setCompatibilityFloat("bot_quota", 4.0f) ==
					   astrabot::compat::CvarUpdateResult::Updated &&
				   runtime.setCompatibilityFloat("bot_difficulty", 1.0f) ==
					   astrabot::compat::CvarUpdateResult::Updated,
			   "runtime accepts desired actor configuration"))
	{
		std::remove(profilePath);
		return 1;
	}

	if (!check(runtime.executeCompatibilityCommand(request("bot_add")) ==
					   CompatibilityCommandResult::Handled &&
				   gCreateCount == 1,
			   "bot_add creates an actor through runtime"))
	{
		std::remove(profilePath);
		return 1;
	}
	if (!check(runtime.executeCompatibilityCommand(request("bot_add_ct")) ==
					   CompatibilityCommandResult::Handled &&
				   gCreateCount == 2,
			   "bot_add_ct selects the counter-terrorist profile"))
	{
		std::remove(profilePath);
		return 1;
	}
	if (!check(runtime.executeCompatibilityCommand(request("bot_add_t")) ==
					   CompatibilityCommandResult::Handled &&
				   gCreateCount == 3,
			   "bot_add_t selects the terrorist profile"))
	{
		std::remove(profilePath);
		return 1;
	}
	if (!check(runtime.executeCompatibilityCommand(request("bot_add", 1U, "Named Bot")) ==
					   CompatibilityCommandResult::Handled &&
				   gCreateCount == 4,
			   "bot_add accepts an explicit profile name"))
	{
		std::remove(profilePath);
		return 1;
	}
	if (!check(runtime.setCompatibilityFloat("bot_quota", 5.0f) ==
				   astrabot::compat::CvarUpdateResult::Updated,
			   "quota allows identity collision check"))
	{
		std::remove(profilePath);
		return 1;
	}
	const int createCountBeforeDuplicate = gCreateCount;
	if (!check(runtime.executeCompatibilityCommand(request("bot_add", 1U, "Named Bot")) ==
					   CompatibilityCommandResult::NameTaken &&
				   gCreateCount == createCountBeforeDuplicate,
			   "duplicate managed names are rejected before engine creation"))
	{
		std::remove(profilePath);
		return 1;
	}
	if (!check(runtime.setCompatibilityFloat("bot_quota", 4.0f) ==
				   astrabot::compat::CvarUpdateResult::Updated,
			   "quota is restored for capacity check"))
	{
		std::remove(profilePath);
		return 1;
	}
	if (!check(runtime.executeCompatibilityCommand(request("bot_add")) ==
					   CompatibilityCommandResult::QuotaReached &&
				   gCreateCount == 4,
			   "quota rejects an additional actor before engine creation"))
	{
		std::remove(profilePath);
		return 1;
	}
	if (!check(runtime.executeCompatibilityCommand(request("bot_kill", 1U, "all")) ==
					   CompatibilityCommandResult::Handled &&
				   gKillCount == 4,
			   "bot_kill all uses the actor kill operation"))
	{
		std::remove(profilePath);
		return 1;
	}
	if (!check(runtime.executeCompatibilityCommand(request("bot_kick", 1U, "CT Bot")) ==
				   CompatibilityCommandResult::Handled,
			   "bot_kick removes a named managed actor"))
	{
		std::remove(profilePath);
		return 1;
	}
	if (!check(runtime.executeCompatibilityCommand(request("bot_kick", 1U, "all")) ==
				   CompatibilityCommandResult::Handled,
			   "bot_kick all removes remaining managed actors"))
	{
		std::remove(profilePath);
		return 1;
	}
	if (!check(runtime.executeCompatibilityCommand(request("bot_kick", 1U, "all")) ==
				   CompatibilityCommandResult::NoTarget,
			   "repeated bot_kick reports no target"))
	{
		std::remove(profilePath);
		return 1;
	}

	const int createCountBeforeInvalid = gCreateCount;
	if (!check(runtime.executeCompatibilityCommand(request("bot_add", 2U, "one")) ==
					   CompatibilityCommandResult::InvalidArguments &&
				   gCreateCount == createCountBeforeInvalid,
			   "invalid add arguments do not create an actor"))
	{
		std::remove(profilePath);
		return 1;
	}
	if (!check(runtime.setCompatibilityFloat("bot_enable", 0.0f) ==
					   astrabot::compat::CvarUpdateResult::Updated &&
				   runtime.executeCompatibilityCommand(request("bot_add")) ==
					   CompatibilityCommandResult::Disabled,
			   "disabled configuration rejects bot_add"))
	{
		std::remove(profilePath);
		return 1;
	}
	if (!check(runtime.setCompatibilityFloat("bot_enable", 1.0f) ==
					   astrabot::compat::CvarUpdateResult::Updated &&
				   runtime.setCompatibilityFloat("bot_stop", 1.0f) ==
					   astrabot::compat::CvarUpdateResult::Updated &&
				   runtime.executeCompatibilityCommand(request("bot_add")) ==
					   CompatibilityCommandResult::Stopped,
			   "stopped configuration rejects bot_add"))
	{
		std::remove(profilePath);
		return 1;
	}
	if (!check(runtime.setCompatibilityFloat("bot_stop", 0.0f) ==
				   astrabot::compat::CvarUpdateResult::Updated,
			   "stop configuration is cleared"))
	{
		std::remove(profilePath);
		return 1;
	}

	gBotEnable = 1.0f;
	gEntities[0].v.flags |= FL_FAKECLIENT;
	globals.time = 2.0f;
	runtime.onStartFrame();
	const int createCountBeforeConflict = gCreateCount;
	if (!check(!runtime.snapshot().managedBotCreationAllowed &&
				   runtime.executeCompatibilityCommand(request("bot_add")) ==
					   CompatibilityCommandResult::NativeGuardDenied &&
				   gCreateCount == createCountBeforeConflict,
			   "native conflict denies bot_add before engine creation"))
	{
		std::remove(profilePath);
		return 1;
	}

	gEntities[0].v.flags &= ~FL_FAKECLIENT;
	gBotEnable = 1.0f;
	gBotQuota = 5.0f;
	globals.time = 3.0f;
	runtime.onStartFrame();
	FakeClientHandle staleHandle{};
	if (!check(runtime.createFakeClient("stale", &staleHandle) == FakeClientResult::Created &&
				   runtime.removeFakeClient(&staleHandle) == FakeClientResult::Removed &&
				   runtime.removeFakeClient(&staleHandle) == FakeClientResult::NotFound,
			   "stale and repeated handles are rejected"))
	{
		std::remove(profilePath);
		return 1;
	}
	if (!check(runtime.executeCompatibilityCommand(request("bot_add")) ==
					   CompatibilityCommandResult::ActorOperationFailed &&
				   gCreateCount == 5,
			   "full actor slots report an operation failure"))
	{
		std::remove(profilePath);
		return 1;
	}

	std::remove(profilePath);
	runtime.onServerDeactivate();
	if (!check(runtime.detach(PT_ANYTIME, PNL_NULL), "runtime detaches after actor command test"))
	{
		return 1;
	}
	return 0;
}
