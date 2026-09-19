#pragma once

#include <extdll.h>
#include <meta_api.h>
#include <h_export.h>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace astrabot
{
namespace metamod
{

using MetaQueryFunction = META_QUERY_FN;
using MetaAttachFunction = META_ATTACH_FN;
using MetaDetachFunction = META_DETACH_FN;
using EntityApiFunction = GETENTITYAPI2_FN;
using EngineFunction = GET_ENGINE_FUNCTIONS_FN;
using EngineBootstrapFunction = GIVE_ENGINE_FUNCTIONS_FN;

extern "C"
{
	C_DLLEXPORT int Meta_Query(char *interfaceVersion, plugin_info_t **pluginInfo,
	mutil_funcs_t *metaUtils);
	C_DLLEXPORT int Meta_Attach(PLUG_LOADTIME loadTime, META_FUNCTIONS *functionTable,
	meta_globals_t *metaGlobals, gamedll_funcs_t *gameDllFunctions);
	C_DLLEXPORT int Meta_Detach(PLUG_LOADTIME loadTime, PL_UNLOAD_REASON reason);
	C_DLLEXPORT FORCE_STACK_ALIGN int GetEntityAPI2(DLL_FUNCTIONS *functionTable,
		int *interfaceVersion);
	C_DLLEXPORT FORCE_STACK_ALIGN int GetEntityAPI2_Post(DLL_FUNCTIONS *functionTable,
		int *interfaceVersion);
	C_DLLEXPORT int GetEngineFunctions(enginefuncs_t *engineFunctions, int *interfaceVersion);
#ifdef _WIN32
void WINAPI GiveFnptrsToDll(enginefuncs_t *engineFunctions, globalvars_t *globals);
#else
C_DLLEXPORT void WINAPI GiveFnptrsToDll(enginefuncs_t *engineFunctions, globalvars_t *globals);
#endif
}

} // namespace metamod
} // namespace astrabot
