#include "astrabot/metamod/abi_contract.hpp"

#include <cassert>
#include <cstring>
#include <type_traits>

int main()
{
	const astrabot::metamod::MetaQueryFunction query = &astrabot::metamod::Meta_Query;
	const astrabot::metamod::MetaAttachFunction attach = &astrabot::metamod::Meta_Attach;
	const astrabot::metamod::MetaDetachFunction detach = &astrabot::metamod::Meta_Detach;
	const astrabot::metamod::EntityApiFunction entityApi = &astrabot::metamod::GetEntityAPI2;
	const astrabot::metamod::EngineFunction engine = &astrabot::metamod::GetEngineFunctions;
	char interfaceVersion[] = META_INTERFACE_VERSION;
	plugin_info_t *info = nullptr;
	mutil_funcs_t metaUtils{};

	assert(query != nullptr);
	assert(attach != nullptr);
	assert(detach != nullptr);
	assert(entityApi != nullptr);
	assert(engine != nullptr);
	static_assert(std::is_same<decltype(&astrabot::metamod::GiveFnptrsToDll),
		astrabot::metamod::EngineBootstrapFunction>::value,
		"GiveFnptrsToDll must retain the SDK WINAPI signature");
	assert(astrabot::metamod::Meta_Query(interfaceVersion, &info, &metaUtils) == TRUE);
	assert(info != nullptr);
	assert(info->name != nullptr && std::strlen(info->name) > 0U);
	assert(info->version != nullptr && std::strlen(info->version) > 0U);
	assert(std::strcmp(META_INTERFACE_VERSION, "5:13") == 0);
	assert(ENGINE_INTERFACE_VERSION == 138);

	return 0;
}
