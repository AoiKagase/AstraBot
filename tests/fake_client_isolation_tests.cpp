#include "astrabot/metamod/abi_contract.hpp"
#include "plugin_runtime.hpp"

#include <cstdio>
#include <cstring>

namespace
{
	edict_t gEntities[2]{};
	int gCreateCount = 0;
	int gMoveCount = 0;
	edict_t *gLastMoveEntity = nullptr;
	float gBotEnable = 0.0f;
	float gBotQuota = 0.0f;
	cvar_t gBotEnableCvar{};
	cvar_t gBotQuotaCvar{};

	bool check(bool condition, const char *description)
	{
		if (condition)
		{
			return true;
		}

		std::fprintf(stderr, "check failed: %s\n", description);
		return false;
	}

	edict_t *createFakeClient(const char *)
	{
		if (gCreateCount >= 2)
		{
			return nullptr;
		}
		edict_t *entity = &gEntities[gCreateCount++];
		entity->v.flags |= FL_FAKECLIENT;
		return entity;
	}

	int indexOfEdict(const edict_t *entity)
	{
		if (entity == &gEntities[0])
		{
			return 1;
		}
		if (entity == &gEntities[1])
		{
			return 2;
		}
		return -1;
	}

	edict_t *entityOfIndex(int index)
	{
		return index >= 1 && index <= 2 ? &gEntities[index - 1] : nullptr;
	}

	cvar_t *getCvar(const char *name)
	{
		if (std::strcmp(name, "bot_enable") == 0)
		{
			return &gBotEnableCvar;
		}
		if (std::strcmp(name, "bot_quota") == 0)
		{
			return &gBotQuotaCvar;
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

	int playerUserId(edict_t *entity)
	{
		return indexOfEdict(entity) + 100;
	}

	qboolean clientConnect(edict_t *, const char *, const char *, char[128])
	{
		return true;
	}

	void putInServer(edict_t *)
	{
	}

	void disconnect(edict_t *)
	{
	}

	void serverCommand(char *)
	{
	}

	void serverExecute()
	{
	}

	void runPlayerMove(
		edict_t *entity,
		const float *,
		float,
		float,
		float,
		unsigned short,
		byte,
		byte)
	{
		gLastMoveEntity = entity;
		++gMoveCount;
	}

	astrabot::runtime::BotCommand commandFor(
		const astrabot::runtime::ActorId &actor,
		const astrabot::runtime::LifecycleToken &token,
		std::uint32_t sequence)
	{
		astrabot::runtime::BotCommand command{};
		command.actor = actor;
		command.lifecycle = token;
		command.sequence = sequence;
		command.issueFrame = 1U;
		command.viewAngles = {0.0f, 90.0f, 0.0f};
		command.movement = {100.0f, 0.0f, 0.0f, 1U, 0U, 10U};
		return command;
	}
}

int main()
{
	using astrabot::metamod::FakeClientHandle;
	using astrabot::metamod::FakeClientResult;
	using astrabot::metamod::PluginRuntime;
	using astrabot::runtime::BotCommand;
	using astrabot::runtime::DispatchResult;
	using astrabot::runtime::QueueResult;

	char interfaceVersion[] = META_INTERFACE_VERSION;
	plugin_info_t *pluginInfo = nullptr;
	mutil_funcs_t metaUtils{};
	meta_globals_t metaGlobals{};
	gamedll_funcs_t gameDllFunctions{};
	DLL_FUNCTIONS gameDllTable{};
	gameDllTable.pfnClientConnect = &clientConnect;
	gameDllTable.pfnClientPutInServer = &putInServer;
	gameDllTable.pfnClientDisconnect = &disconnect;
	gameDllFunctions.dllapi_table = &gameDllTable;
	if (!check(astrabot::metamod::Meta_Query(
			interfaceVersion, &pluginInfo, &metaUtils) == TRUE,
			"Meta_Query succeeds"))
	{
		return 1;
	}
	META_FUNCTIONS metaFunctions{};
	if (!check(astrabot::metamod::Meta_Attach(
			PT_ANYTIME, &metaFunctions, &metaGlobals, &gameDllFunctions) == TRUE,
			"Meta_Attach succeeds"))
	{
		return 1;
	}
	int entityVersion = INTERFACE_VERSION;
	DLL_FUNCTIONS entityFunctions{};
	if (!check(metaFunctions.pfnGetEntityAPI2(&entityFunctions, &entityVersion) == TRUE,
			"entity hook table succeeds"))
	{
		return 1;
	}
	int engineVersion = ENGINE_INTERFACE_VERSION;
	enginefuncs_t engineFunctions{};
	if (!check(metaFunctions.pfnGetEngineFunctions(&engineFunctions, &engineVersion) == TRUE,
			"engine hook table succeeds"))
	{
		return 1;
	}
	engineFunctions.pfnCreateFakeClient = &createFakeClient;
	engineFunctions.pfnIndexOfEdict = &indexOfEdict;
	engineFunctions.pfnPEntityOfEntIndex = &entityOfIndex;
	engineFunctions.pfnCVarGetPointer = &getCvar;
	engineFunctions.pfnCVarGetFloat = &getCvarFloat;
	engineFunctions.pfnCVarSetFloat = &setCvarFloat;
	engineFunctions.pfnGetPlayerUserId = &playerUserId;
	engineFunctions.pfnServerCommand = &serverCommand;
	engineFunctions.pfnServerExecute = &serverExecute;
	engineFunctions.pfnRunPlayerMove = &runPlayerMove;
	globalvars_t globals{};
	globals.maxClients = 32;
	globals.time = 1.0f;
	astrabot::metamod::GiveFnptrsToDll(&engineFunctions, &globals);
	entityFunctions.pfnServerActivate(nullptr, 0, 32);
	const auto active = PluginRuntime::instance().snapshot();
	if (!check(active.state == PluginRuntime::State::ActiveMap &&
			active.nativeGuardState == astrabot::metamod::NativeBotGuardState::Suppressed,
			"activation arms native guard"))
	{
		return 1;
	}

	FakeClientHandle first{};
	FakeClientHandle second{};
	if (!check(PluginRuntime::instance().createFakeClient("Astra-1", &first) ==
			FakeClientResult::Created, "first managed FakeClient is created"))
	{
		return 1;
	}
	if (!check(PluginRuntime::instance().createFakeClient("Astra-2", &second) ==
			FakeClientResult::Created, "second managed FakeClient is created"))
	{
		return 1;
	}
	globals.time = 1.1f;
	entityFunctions.pfnStartFrame();
	const auto guarded = PluginRuntime::instance().snapshot();
	if (!check(guarded.managedBotCreationAllowed,
			"owned FakeClients remain allowed"))
	{
		return 1;
	}
	const auto firstToken = PluginRuntime::instance().tokenForSlot(first.actor.slot);
	const auto secondToken = PluginRuntime::instance().tokenForSlot(second.actor.slot);
	BotCommand firstCommand = commandFor(first.actor, firstToken, 1U);
	BotCommand secondCommand = commandFor(second.actor, secondToken, 1U);
	if (!check(PluginRuntime::instance().enqueueBotCommand(firstCommand) ==
			   QueueResult::StaleActor &&
			PluginRuntime::instance().enqueueBotCommand(secondCommand) ==
			QueueResult::StaleActor, "joining actors cannot queue movement commands"))
	{
		return 1;
	}
	if (!check(PluginRuntime::instance().dispatchBotInput(first.actor, 2U).result ==
			   DispatchResult::NoCommand,
			   "joining first actor does not dispatch movement"))
	{
		return 1;
	}
	if (!check(PluginRuntime::instance().dispatchBotInput(second.actor, 2U).result ==
			   DispatchResult::NoCommand,
			   "joining second actor does not dispatch movement"))
	{
		return 1;
	}
	const auto staleToken = PluginRuntime::instance().tokenForSlot(first.actor.slot);
	if (!check(PluginRuntime::instance().enqueueBotCommand(
			commandFor(first.actor, staleToken, 2U)) == QueueResult::StaleActor,
			"joining actor rejects a stale movement candidate"))
	{
		return 1;
	}
	globals.time = 0.0f;
	entityFunctions.pfnStartFrame();
	if (!check(PluginRuntime::instance().dispatchBotInput(first.actor, 3U).result ==
			DispatchResult::NoCommand,
			"round reset leaves no movement command for a joining actor"))
	{
		return 1;
	}
	entityFunctions.pfnClientDisconnect(first.entity);
	if (!check(PluginRuntime::instance().dispatchBotInput(first.actor, 4U).result ==
			DispatchResult::NoCommand,
			"external disconnect clears actor input"))
	{
		return 1;
	}
	if (!check(PluginRuntime::instance().removeFakeClient(&first) == FakeClientResult::NotFound,
			"external disconnect retires actor record"))
	{
		return 1;
	}
	if (!check(PluginRuntime::instance().removeFakeClient(&second) == FakeClientResult::Removed,
			"managed FakeClient removal succeeds"))
	{
		return 1;
	}

	gBotEnable = 1.0f;
	globals.time = 2.0f;
	entityFunctions.pfnStartFrame();
	const auto conflict = PluginRuntime::instance().snapshot();
	if (!check(!conflict.managedBotCreationAllowed,
			"native control re-enable disables managed creation"))
	{
		return 1;
	}
	const int createCountBeforeDenied = gCreateCount;
	FakeClientHandle denied{};
	if (!check(PluginRuntime::instance().createFakeClient("denied", &denied) ==
			FakeClientResult::NativeGuardDenied && gCreateCount == createCountBeforeDenied,
			"native conflict prevents engine creation"))
	{
		return 1;
	}
	entityFunctions.pfnServerDeactivate();
	if (!check(astrabot::metamod::Meta_Detach(PT_ANYTIME, PNL_NULL) == TRUE,
			"Meta_Detach succeeds"))
	{
		return 1;
	}
	return 0;
}
