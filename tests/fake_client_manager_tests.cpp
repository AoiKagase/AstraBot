#include "astrabot/metamod/fake_client_manager.hpp"

#include <cstddef>
#include <cstdio>
#include <cstring>

namespace
{
	edict_t gEntities[2]{};
	int gCreateCallCount = 0;
	int gClientConnectCount = 0;
	int gFreePrivateDataCount = 0;
	int gPutInServerCount = 0;
	int gDisconnectCount = 0;
	int gServerCommandCount = 0;
	int gServerExecuteCount = 0;
	char gLastServerCommand[64]{};
	bool gCreateSucceeds = true;
	char gInfoBuffer[128]{};
bool gBotMarkerPresent = false;
bool gBotMarkerObservedDuringPutInServer = false;
int gPlayerFactoryCallCount = 0;
bool gPlayerFactorySawOriginalOrigin = false;

	bool check(bool condition, const char *description)
	{
		if (condition)
		{
			return true;
		}

		std::fprintf(stderr, "check failed: %s\n", description);
		return false;
	}

	void resetHost()
	{
		gCreateCallCount = 0;
		gPutInServerCount = 0;
		gDisconnectCount = 0;
		gServerCommandCount = 0;
		gServerExecuteCount = 0;
		gLastServerCommand[0] = '\0';
	gCreateSucceeds = true;
	gClientConnectCount = 0;
	gFreePrivateDataCount = 0;
		gInfoBuffer[0] = '\0';
	gBotMarkerPresent = false;
	gBotMarkerObservedDuringPutInServer = false;
	gPlayerFactoryCallCount = 0;
	gPlayerFactorySawOriginalOrigin = false;
		for (edict_t &entity : gEntities)
		{
			entity = edict_t{};
		}
	}

edict_t *createFakeClient(const char *)
	{
		if (!gCreateSucceeds || gCreateCallCount >= 3)
		{
			return nullptr;
		}
		const int slot = gCreateCallCount == 1 ? 1 : 0;
		++gCreateCallCount;
		gEntities[slot].v.netname = static_cast<string_t>(slot + 1);
		gEntities[slot].v.origin.x = 123.0f;
		gEntities[slot].v.origin.y = 456.0f;
		gEntities[slot].v.origin.z = 789.0f;
		gEntities[slot].v.pContainingEntity = &gEntities[0];
		gEntities[slot].pvPrivateData = reinterpret_cast<void *>(1);
		return &gEntities[slot];
	}

	void freeEntPrivateData(edict_t *entity)
	{
		++gFreePrivateDataCount;
		if (entity != nullptr)
		{
			entity->pvPrivateData = nullptr;
		}
	}

	qboolean clientConnect(edict_t *, const char *, const char *, char[128])
	{
		++gClientConnectCount;
		return true;
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

	int playerUserId(edict_t *entity) { return indexOfEdict(entity) + 100; }

	char *getInfoKeyBuffer(edict_t *) { return gInfoBuffer; }

	void setClientKeyValue(int, char *, char *key, char *value)
	{
		if (key != nullptr && value != nullptr && std::strcmp(key, "*bot") == 0 &&
			std::strcmp(value, "1") == 0)
		{
			gBotMarkerPresent = true;
		}
	}

	void putInServer(edict_t *entity)
	{
		++gPutInServerCount;
		gBotMarkerObservedDuringPutInServer = gBotMarkerPresent;
		if (entity != nullptr)
		{
			entity->v.flags &= ~(FL_CLIENT | FL_FAKECLIENT);
		}
	}

	void disconnect(edict_t *) { ++gDisconnectCount; }

	void serverCommand(char *command)
	{
		++gServerCommandCount;
		std::size_t index = 0U;
		while (command[index] != '\0' && index + 1U < sizeof(gLastServerCommand))
		{
			gLastServerCommand[index] = command[index];
			++index;
		}
		gLastServerCommand[index] = '\0';
	}

	void serverExecute() { ++gServerExecuteCount; }

} // namespace

int main()
{
	using astrabot::metamod::FakeClientHandle;
	using astrabot::metamod::FakeClientManager;
	using astrabot::metamod::FakeClientResult;
	using astrabot::runtime::ActorRegistry;
	using astrabot::runtime::ActorState;
	using astrabot::runtime::LifecycleSession;

	resetHost();
	LifecycleSession lifecycle;
	ActorRegistry registry;
	if (!check(lifecycle.activateMap(), "map activation succeeds"))
	{
		return 1;
	}
	enginefuncs_t engineFunctions{};
	engineFunctions.pfnCreateFakeClient = &createFakeClient;
	engineFunctions.pfnFreeEntPrivateData = &freeEntPrivateData;
	engineFunctions.pfnIndexOfEdict = &indexOfEdict;
	engineFunctions.pfnGetPlayerUserId = &playerUserId;
	engineFunctions.pfnGetInfoKeyBuffer = &getInfoKeyBuffer;
	engineFunctions.pfnSetClientKeyValue = &setClientKeyValue;
	engineFunctions.pfnServerCommand = &serverCommand;
	engineFunctions.pfnServerExecute = &serverExecute;
	DLL_FUNCTIONS dllFunctions{};
	dllFunctions.pfnClientConnect = &clientConnect;
	dllFunctions.pfnClientPutInServer = &putInServer;
	dllFunctions.pfnClientDisconnect = &disconnect;
	gamedll_funcs_t gameDllFunctions{};
	gameDllFunctions.dllapi_table = &dllFunctions;

	FakeClientManager manager(lifecycle, registry);
	manager.configure(&engineFunctions, &gameDllFunctions, true);
	FakeClientHandle firstHandle{};
	if (!check(manager.create("Astra-1", &firstHandle) == FakeClientResult::Created,
			   "first FakeClient creation succeeds"))
	{
		return 1;
	}
	if (!check(gPutInServerCount == 1, "first FakeClient joins once"))
	{
		return 1;
	}

	if (!check(gClientConnectCount == 1,
			   "ClientConnect runs before the first FakeClient joins"))
	{
		return 1;
	}

	if (!check(gFreePrivateDataCount == 1,
			   "stale FakeClient private data is released before the GameDLL join"))
	{
		return 1;
	}

	if (!check(firstHandle.entity->v.origin.x == 0.0f,
			   "stale FakeClient origin x is reset"))
	{
		return 1;
	}
	if (!check(firstHandle.entity->v.origin.y == 0.0f,
			   "stale FakeClient origin y is reset"))
	{
		return 1;
	}
	if (!check(firstHandle.entity->v.origin.z == 0.0f,
			   "stale FakeClient origin z is reset"))
	{
		return 1;
	}
	if (!check(firstHandle.entity->v.pContainingEntity == firstHandle.entity,
			   "FakeClient containing entity is restored"))
	{
		return 1;
	}
	if (!check(firstHandle.entity->v.netname == static_cast<string_t>(1),
			   "FakeClient netname is retained"))
	{
		return 1;
	}
	if (!check(gBotMarkerObservedDuringPutInServer, "bot marker is available before GameDLL join"))
	{
		return 1;
	}
	if (!check((firstHandle.entity->v.flags & (FL_CLIENT | FL_FAKECLIENT)) ==
				(FL_CLIENT | FL_FAKECLIENT),
			"client and fake client flags are retained after GameDLL join"))
	{
		return 1;
	}
	if (!check(registry.state(firstHandle.actor) == ActorState::Joining,
			 "first actor remains joining until menu and spawn confirmation"))
	{
		return 1;
	}
	FakeClientHandle secondHandle{};
	if (!check(manager.create("Astra-2", &secondHandle) == FakeClientResult::Created,
			   "second FakeClient creation succeeds"))
	{
		return 1;
	}
	if (!check(firstHandle.actor.slot != secondHandle.actor.slot, "two actors use isolated slots"))
	{
		return 1;
	}
	if (!check(manager.remove(&firstHandle) == FakeClientResult::Removed,
			   "first FakeClient removal succeeds"))
	{
		return 1;
	}
	if (!check(gDisconnectCount == 1 && gServerCommandCount == 1 && gServerExecuteCount == 1,
			   "removal performs public cleanup"))
	{
		return 1;
	}
	if (!check(!registry.isCurrent(firstHandle.actor, lifecycle.tokenForSlot(1U)),
			   "removed actor is stale"))
	{
		return 1;
	}
	if (!check(manager.remove(&firstHandle) == FakeClientResult::NotFound,
			   "repeated removal is harmless"))
	{
		return 1;
	}

	manager.configure(&engineFunctions, &gameDllFunctions, false);
	const int createCountBeforeDenied = gCreateCallCount;
	FakeClientHandle deniedHandle{};
	if (!check(manager.create("denied", &deniedHandle) == FakeClientResult::NativeGuardDenied,
			   "native guard denial rejects creation"))
	{
		return 1;
	}
	if (!check(gCreateCallCount == createCountBeforeDenied,
			   "native guard denial makes no engine call"))
	{
		return 1;
	}

	manager.configure(&engineFunctions, &gameDllFunctions, true);
	FakeClientHandle reusedHandle{};
	if (!check(manager.create("Astra-reused", &reusedHandle) == FakeClientResult::Created,
			   "released slot can be reused"))
	{
		return 1;
	}
	if (!check(reusedHandle.actor.actorGeneration != firstHandle.actor.actorGeneration,
			   "reused slot receives a new actor generation"))
	{
		return 1;
	}

	gCreateSucceeds = false;
	FakeClientHandle failedHandle{};
	if (!check(manager.create("failed", &failedHandle) == FakeClientResult::CreateFailed,
			   "engine create failure is returned"))
	{
		return 1;
	}
	return 0;
}

qboolean callGameEntity(plid_t, const char *, entvars_t *variables)
{
	++gPlayerFactoryCallCount;
	gPlayerFactorySawOriginalOrigin = variables != nullptr && variables->origin.x == 123.0f;
	return TRUE;
}
