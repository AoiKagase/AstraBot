#include "bridge/regamedll_observation_provider.hpp"

// Generated verbatim from the bundled official interface, never a vtable prefix.
#include "astrabot_regamedll_api_view.hpp"

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#include <link.h>
#include <cstring>
#endif

namespace astrabot
{
namespace metamod
{
void ReGameDllObservationProvider::reset()
{
	status_ = ReGameDllProbeStatus::NotPresent;
	major_ = minor_ = 0;
}

void ReGameDllObservationProvider::probe(Factory factory)
{
	reset();
	if (factory == nullptr)
	{
		status_ = ReGameDllProbeStatus::NoFactory;
		return;
	}
	int result = 0;
	IReGameApi *api = static_cast<IReGameApi *>(factory(VRE_GAMEDLL_API_VERSION, &result));
	if (api == nullptr)
	{
		status_ = ReGameDllProbeStatus::NoInterface;
		return;
	}
	major_ = api->GetMajorVersion();
	minor_ = api->GetMinorVersion();
	if (major_ != REGAMEDLL_API_VERSION_MAJOR || minor_ < REGAMEDLL_API_VERSION_MINOR)
	{
		status_ = ReGameDllProbeStatus::IncompatibleVersion;
		return;
	}
	status_ = ReGameDllProbeStatus::MetadataOnly;
}

void ReGameDllObservationProvider::probeLoadedGameDll()
{
	reset();
#if defined(_WIN32)
	const char *names[] = {"mp.dll", "cs.dll"};
	for (const char *name : names)
	{
		HMODULE module = GetModuleHandleA(name);
		if (module != nullptr)
		{
			probe(reinterpret_cast<Factory>(GetProcAddress(module, "CreateInterface")));
			return;
		}
	}
#elif defined(RTLD_NOLOAD)
	// Resolve the actual already-loaded path. Do not load a second GameDLL or
	// depend on the server's current working directory / dynamic linker path.
	dl_iterate_phdr(
		[](dl_phdr_info *info, std::size_t, void *data) -> int
		{
			if (info->dlpi_name == nullptr || info->dlpi_name[0] == '\0')
				return 0;
			const char *base = std::strrchr(info->dlpi_name, '/');
			base = base != nullptr ? base + 1 : info->dlpi_name;
			if (std::strcmp(base, "cs.so") != 0 && std::strcmp(base, "cs_i386.so") != 0 &&
				std::strcmp(base, "mp.so") != 0 && std::strcmp(base, "mp_i386.so") != 0)
				return 0;
			void *module = dlopen(info->dlpi_name, RTLD_NOW | RTLD_NOLOAD);
			if (module == nullptr)
				return 0;
			auto *provider = static_cast<ReGameDllObservationProvider *>(data);
			provider->probe(reinterpret_cast<Factory>(dlsym(module, "CreateInterface")));
			dlclose(module);
			return 1;
		},
		this);
#endif
}

const char *ReGameDllObservationProvider::diagnostic() const
{
	switch (status_)
	{
	case ReGameDllProbeStatus::NotPresent:
		return "ReGameDLL absent: public Vanilla observations remain active";
	case ReGameDllProbeStatus::NoFactory:
		return "GameDLL has no CreateInterface: public Vanilla observations remain active";
	case ReGameDllProbeStatus::NoInterface:
		return "GameDLL has no ReGameDLL API: public Vanilla observations remain active";
	case ReGameDllProbeStatus::IncompatibleVersion:
		return "ReGameDLL API version unsupported: no supplemental reads";
	case ReGameDllProbeStatus::MetadataOnly:
		return "ReGameDLL API negotiated; no public per-actor weapon timer/accuracy getters; no supplemental reads";
	}
	return "ReGameDLL probe status unknown";
}

compat::ObservationCapabilities ReGameDllObservationProvider::capabilities() const
{
	return {};
}

void ReGameDllObservationProvider::supplement(compat::CompatibilityObservation *) const
{
	// All current supplemental fields would require private object access.
	// Preserve both known Vanilla fields and unknown fields verbatim.
}
}
}
