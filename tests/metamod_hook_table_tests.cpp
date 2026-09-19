#include "astrabot/metamod/abi_contract.hpp"
#include "astrabot/metamod/native_bot_guard.hpp"
#include "plugin_runtime.hpp"

#include <cstdio>
#include <cstring>

namespace
{
	const edict_t *gClientEntity = nullptr;
	edict_t *gNativeEntity = nullptr;
	bool gNativeControlsAvailable = true;
	float gBotEnable = 1.0f;
	float gBotQuota = 2.0f;
	cvar_t gBotEnableCvar{};
	cvar_t gBotQuotaCvar{};

	cvar_t *getCvar(const char *name)
	{
		if (!gNativeControlsAvailable)
		{
			return nullptr;
		}
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

	edict_t *entityOfIndex(int index) { return index == 1 ? gNativeEntity : nullptr; }

	int indexOfEdict(const edict_t *entity) { return entity == gClientEntity ? 1 : -1; }

	bool check(bool condition, const char *description)
	{
		if (condition)
		{
			return true;
		}

		std::fprintf(stderr, "check failed: %s\n", description);
		return false;
	}

	bool checkLifecycleHooks(const DLL_FUNCTIONS &functionTable)
	{
		return check(functionTable.pfnClientDisconnect != nullptr, "ClientDisconnect hook") &&
			   check(functionTable.pfnClientPutInServer != nullptr, "ClientPutInServer hook") &&
			   check(functionTable.pfnServerActivate != nullptr, "ServerActivate hook") &&
			   check(functionTable.pfnServerDeactivate != nullptr, "ServerDeactivate hook") &&
			   check(functionTable.pfnStartFrame != nullptr, "StartFrame hook");
	}
} // namespace

int main()
{
	char interfaceVersionText[] = META_INTERFACE_VERSION;
	plugin_info_t *pluginInfo = nullptr;
	META_FUNCTIONS metaFunctions{};
	meta_globals_t metaGlobals{};
	gamedll_funcs_t gameDllFunctions{};
	mutil_funcs_t metaUtils{};
	if (!check(astrabot::metamod::Meta_Query(interfaceVersionText, &pluginInfo, &metaUtils) == TRUE,
			   "Meta_Query accepts supported inputs"))
	{
		return 1;
	}
	if (!check(pluginInfo != nullptr, "Meta_Query returns plugin metadata"))
	{
		return 1;
	}

	if (!check(astrabot::metamod::Meta_Attach(PT_ANYTIME, nullptr, &metaGlobals,
											  &gameDllFunctions) == FALSE,
			   "Meta_Attach rejects null function table"))
	{
		return 1;
	}
	if (!check(astrabot::metamod::Meta_Attach(PT_ANYTIME, &metaFunctions, &metaGlobals,
											  &gameDllFunctions) == TRUE,
			   "Meta_Attach accepts valid inputs"))
	{
		return 1;
	}
	if (!check(metaFunctions.pfnGetEntityAPI2 != nullptr, "GetEntityAPI2 hook provider"))
	{
		return 1;
	}
	if (!check(metaFunctions.pfnGetEngineFunctions != nullptr, "GetEngineFunctions hook provider"))
	{
		return 1;
	}

	int entityVersion = INTERFACE_VERSION;
	DLL_FUNCTIONS entityFunctions{};
	if (!check(metaFunctions.pfnGetEntityAPI2(&entityFunctions, &entityVersion) == TRUE,
			   "GetEntityAPI2 accepts supported version"))
	{
		return 1;
	}
	if (!check(checkLifecycleHooks(entityFunctions), "all lifecycle hooks are published"))
	{
		return 1;
	}

	int engineVersion = ENGINE_INTERFACE_VERSION;
	enginefuncs_t engineFunctions{};
	if (!check(metaFunctions.pfnGetEngineFunctions(&engineFunctions, &engineVersion) == TRUE,
			   "GetEngineFunctions accepts supported version"))
	{
		return 1;
	}
	if (!check(engineFunctions.pfnMessageBegin != nullptr &&
			   engineFunctions.pfnMessageEnd != nullptr &&
			   engineFunctions.pfnWriteByte != nullptr &&
			   engineFunctions.pfnWriteShort != nullptr &&
			   engineFunctions.pfnWriteString != nullptr,
			   "fake-client menu message hooks are published"))
	{
		return 1;
	}

	const auto attachedSnapshot = astrabot::metamod::PluginRuntime::instance().snapshot();
	if (!check(attachedSnapshot.state == astrabot::metamod::PluginRuntime::State::Attached,
			   "attach leaves runtime attached"))
	{
		return 1;
	}
	globalvars_t globals{};
	globals.time = 1.0f;
	globals.maxClients = 32;
	engineFunctions.pfnIndexOfEdict = &indexOfEdict;
	engineFunctions.pfnCVarGetPointer = &getCvar;
	engineFunctions.pfnCVarGetFloat = &getCvarFloat;
	engineFunctions.pfnCVarSetFloat = &setCvarFloat;
	engineFunctions.pfnPEntityOfEntIndex = &entityOfIndex;
	astrabot::metamod::GiveFnptrsToDll(&engineFunctions, &globals);
	entityFunctions.pfnServerActivate(nullptr, 0, 32);
	const auto activeSnapshot = astrabot::metamod::PluginRuntime::instance().snapshot();
	if (!check(activeSnapshot.state == astrabot::metamod::PluginRuntime::State::ActiveMap,
			"server activation enters active map state"))
	{
		return 1;
	}
	if (!check(activeSnapshot.mode == astrabot::compat::RuntimeMode::Compatibility,
			"server activation defaults to compatibility mode"))
	{
		return 1;
	}
	if (!check(activeSnapshot.mapGeneration != 0U, "server activation creates map generation"))
	{
		return 1;
	}
	if (!check(activeSnapshot.nativeGuardState ==
				   astrabot::metamod::NativeBotGuardState::Suppressed,
			   "server activation suppresses native controls"))
	{
		return 1;
	}
	if (!check(activeSnapshot.managedBotCreationAllowed,
			   "suppressed native controls allow managed creation"))
	{
		return 1;
	}
	if (!check(gBotEnable == 0.0f && gBotQuota == 0.0f,
			   "server activation sets native bot controls to zero"))
	{
		return 1;
	}
	const auto initialRoundGeneration = activeSnapshot.roundGeneration;
	entityFunctions.pfnStartFrame();
	globals.time = 0.0f;
	entityFunctions.pfnStartFrame();
	const auto resetSnapshot = astrabot::metamod::PluginRuntime::instance().snapshot();
	if (!check(resetSnapshot.roundGeneration != initialRoundGeneration,
			   "frame reset advances round generation"))
	{
		return 1;
	}
	edict_t nativeEntity{};
	nativeEntity.v.flags = FL_FAKECLIENT;
	gNativeEntity = &nativeEntity;
	globals.time = 1.2f;
	entityFunctions.pfnStartFrame();
	const auto conflictSnapshot = astrabot::metamod::PluginRuntime::instance().snapshot();
	if (!check(conflictSnapshot.nativeGuardState ==
				   astrabot::metamod::NativeBotGuardState::Conflict,
			   "native fake client creates a guard conflict"))
	{
		return 1;
	}
	if (!check(!conflictSnapshot.managedBotCreationAllowed,
			   "native fake client disables managed creation"))
	{
		return 1;
	}
	gNativeEntity = nullptr;
	gNativeControlsAvailable = false;
	globals.time = 1.3f;
	entityFunctions.pfnStartFrame();
	const auto vanillaSnapshot = astrabot::metamod::PluginRuntime::instance().snapshot();
	if (!check(vanillaSnapshot.nativeGuardState == astrabot::metamod::NativeBotGuardState::Clean &&
				   vanillaSnapshot.managedBotCreationAllowed,
			   "missing native bot controls allow Vanilla CS creation"))
	{
		return 1;
	}
	char nativeCommand[] = "bot_add";
	metaGlobals.mres = MRES_UNSET;
	engineFunctions.pfnAddServerCommand(nativeCommand, nullptr);
	if (!check(metaGlobals.mres == MRES_SUPERCEDE, "native bot command registration is superceded"))
	{
		return 1;
	}
	char unrelatedCommand[] = "say";
	metaGlobals.mres = MRES_UNSET;
	engineFunctions.pfnAddServerCommand(unrelatedCommand, nullptr);
	if (!check(metaGlobals.mres == MRES_IGNORED, "unrelated command registration is ignored"))
	{
		return 1;
	}
	edict_t clientEntity{};
	gClientEntity = &clientEntity;
	entityFunctions.pfnClientPutInServer(&clientEntity);
	const auto clientToken = astrabot::metamod::PluginRuntime::instance().tokenForSlot(1U);
	if (!check(clientToken.slotGeneration != 0U, "client hook connects resolved slot"))
	{
		return 1;
	}
	entityFunctions.pfnClientDisconnect(&clientEntity);
	const auto disconnectedToken = astrabot::metamod::PluginRuntime::instance().tokenForSlot(1U);
	if (!check(disconnectedToken.slotGeneration == 0U, "client disconnect invalidates slot token"))
	{
		return 1;
	}
	entityFunctions.pfnServerDeactivate();
	const auto inactiveSnapshot = astrabot::metamod::PluginRuntime::instance().snapshot();
	if (!check(inactiveSnapshot.state == astrabot::metamod::PluginRuntime::State::Attached,
			   "server deactivation leaves runtime attached"))
	{
		return 1;
	}

	DLL_FUNCTIONS unchanged{};
	std::memset(&unchanged, 0xA5, sizeof(unchanged));
	const DLL_FUNCTIONS before = unchanged;
	entityVersion = INTERFACE_VERSION + 1;
	if (!check(astrabot::metamod::GetEntityAPI2(&unchanged, &entityVersion) == FALSE,
			   "GetEntityAPI2 rejects unsupported version"))
	{
		return 1;
	}
	if (!check(std::memcmp(&unchanged, &before, sizeof(unchanged)) == 0,
			   "unsupported version leaves caller table untouched"))
	{
		return 1;
	}

	if (!check(astrabot::metamod::Meta_Detach(PT_ANYTIME, PNL_NULL) == TRUE,
			   "Meta_Detach accepts valid inputs"))
	{
		return 1;
	}
	if (!check(gBotEnable == 1.0f && gBotQuota == 2.0f, "detach restores native bot controls"))
	{
		return 1;
	}
	entityFunctions.pfnStartFrame();
	(void)metaUtils;
	return 0;
}
