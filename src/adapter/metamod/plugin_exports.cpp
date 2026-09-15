#include "astrabot/metamod/abi_contract.hpp"

#include "plugin_runtime.hpp"

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

	gpMetaUtilFuncs = metaUtils;
	*pluginInfo = &gPluginInfo;
	return TRUE;
}

C_DLLEXPORT int Meta_Attach(PLUG_LOADTIME loadTime, META_FUNCTIONS *functionTable,
		meta_globals_t *metaGlobals, gamedll_funcs_t *gameDllFunctions)
{
	return PluginRuntime::instance().attach(
			loadTime, functionTable, metaGlobals, gameDllFunctions, &gPluginInfo) ? TRUE : FALSE;
}

C_DLLEXPORT int Meta_Detach(PLUG_LOADTIME loadTime, PL_UNLOAD_REASON reason)
{
	return PluginRuntime::instance().detach(loadTime, reason) ? TRUE : FALSE;
}

C_DLLEXPORT FORCE_STACK_ALIGN int GetEntityAPI2(DLL_FUNCTIONS *functionTable,
		int *interfaceVersion)
{
	return PluginRuntime::instance().provideEntityApi(functionTable, interfaceVersion)
			? TRUE : FALSE;
}

C_DLLEXPORT int GetEngineFunctions(enginefuncs_t *engineFunctions, int *interfaceVersion)
{
	return PluginRuntime::instance().provideEngineFunctions(engineFunctions, interfaceVersion)
			? TRUE : FALSE;
}

#ifdef _WIN32
void WINAPI GiveFnptrsToDll(enginefuncs_t *engineFunctions, globalvars_t *globals)
#else
C_DLLEXPORT void WINAPI GiveFnptrsToDll(enginefuncs_t *engineFunctions, globalvars_t *globals)
#endif
{
	PluginRuntime::instance().giveEnginePointers(engineFunctions, globals);
}

} // namespace metamod
} // namespace astrabot
