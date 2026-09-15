#include "astrabot/metamod/abi_contract.hpp"

#include <cstring>

namespace astrabot
{
namespace metamod
{
namespace
{

char kInterfaceVersion[] = META_INTERFACE_VERSION;
char kPluginName[] = "AstraBot";
char kPluginVersion[] = "0.1.0";
char kPluginDate[] = "2026-09-15";
char kPluginAuthor[] = "AstraBot contributors";
char kPluginUrl[] = "";
char kPluginLogTag[] = "ASTRABOT";

plugin_info_t gPluginInfo = {
	kInterfaceVersion,
	kPluginName,
	kPluginVersion,
	kPluginDate,
	kPluginAuthor,
	kPluginUrl,
	kPluginLogTag,
	PT_ANYTIME,
	PT_ANYTIME
};

} // namespace

C_DLLEXPORT int Meta_Query(char *interfaceVersion, plugin_info_t **pluginInfo,
	mutil_funcs_t *metaUtils)
{
	if (interfaceVersion == nullptr || pluginInfo == nullptr || metaUtils == nullptr)
	{
		return FALSE;
	}

	if (std::strcmp(interfaceVersion, META_INTERFACE_VERSION) != 0)
	{
		return FALSE;
	}

	*pluginInfo = &gPluginInfo;
	return TRUE;
}

C_DLLEXPORT int Meta_Attach(PLUG_LOADTIME loadTime, META_FUNCTIONS *functionTable,
	meta_globals_t *metaGlobals, gamedll_funcs_t *gameDllFunctions)
{
	if (functionTable == nullptr || metaGlobals == nullptr || gameDllFunctions == nullptr)
	{
		return FALSE;
	}

	(void)loadTime;
	return TRUE;
}

C_DLLEXPORT int Meta_Detach(PLUG_LOADTIME loadTime, PL_UNLOAD_REASON reason)
{
	(void)loadTime;
	(void)reason;
	return TRUE;
}

C_DLLEXPORT FORCE_STACK_ALIGN int GetEntityAPI2(DLL_FUNCTIONS *functionTable,
	int *interfaceVersion)
{
	if (interfaceVersion == nullptr)
	{
		return FALSE;
	}

	if (*interfaceVersion != INTERFACE_VERSION)
	{
		*interfaceVersion = INTERFACE_VERSION;
		return FALSE;
	}

	if (functionTable == nullptr)
	{
		return FALSE;
	}

	std::memset(functionTable, 0, sizeof(*functionTable));
	return TRUE;
}

C_DLLEXPORT int GetEngineFunctions(enginefuncs_t *engineFunctions, int *interfaceVersion)
{
	if (interfaceVersion == nullptr)
	{
		return FALSE;
	}

	if (*interfaceVersion != ENGINE_INTERFACE_VERSION)
	{
		*interfaceVersion = ENGINE_INTERFACE_VERSION;
		return FALSE;
	}

	if (engineFunctions == nullptr)
	{
		return FALSE;
	}

	std::memset(engineFunctions, 0, sizeof(*engineFunctions));
	return TRUE;
}

void WINAPI GiveFnptrsToDll(enginefuncs_t *engineFunctions, globalvars_t *globals)
{
	(void)engineFunctions;
	(void)globals;
}

} // namespace metamod
} // namespace astrabot
