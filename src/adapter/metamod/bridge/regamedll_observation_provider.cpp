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
ReGameDllObservationProvider *ReGameDllObservationProvider::eventOwner_ = nullptr;
ReGameDllObservationProvider::DamageRegistry *ReGameDllObservationProvider::deferredDamageRegistry_ = nullptr;
unsigned ReGameDllObservationProvider::damageDispatchDepth_ = 0;
ReGameDllObservationProvider::WeaponRegistry *ReGameDllObservationProvider::deferredWeaponRegistry_ = nullptr;
unsigned ReGameDllObservationProvider::weaponDispatchDepth_ = 0;

namespace
{
constexpr int kMinimumPublicApiMinor = 3;
constexpr int kWeaponEventApiMinor = 30;
}

ReGameDllObservationProvider::~ReGameDllObservationProvider()
{
	reset();
}

void ReGameDllObservationProvider::reset()
{
	auto *blindRegistry = blindRegistry_;
	auto *damageRegistry = damageRegistry_;
	auto *weaponRegistry = weaponRegistry_;
	if (eventOwner_ == this)
		eventOwner_ = nullptr;
	blindRegistry_ = nullptr;
	blindListener_ = nullptr;
	blindContext_ = nullptr;
	damageRegistry_ = nullptr;
	damageListener_ = nullptr;
	damageContext_ = nullptr;
	weaponRegistry_ = nullptr;
	weaponListener_ = nullptr;
	weaponContext_ = nullptr;
	functions_ = nullptr;
	api_ = nullptr;
	++eventRevision_;
	status_ = ReGameDllProbeStatus::NotPresent;
	major_ = minor_ = 0;
	if (blindRegistry)
		blindRegistry->unregisterHook(&onBlind);
	if (damageRegistry)
	{
		// The native chain borrows the registry's array. Removing its active
		// entry here would shift the next hook before callNext can read it.
		if (damageDispatchDepth_ != 0)
			deferredDamageRegistry_ = damageRegistry;
		else
			damageRegistry->unregisterHook(&onDamage);
	}
	if (weaponRegistry)
	{
		if (weaponDispatchDepth_ != 0)
			deferredWeaponRegistry_ = weaponRegistry;
		else
			weaponRegistry->unregisterHook(&onWeapon);
	}
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
	if (major_ != REGAMEDLL_API_VERSION_MAJOR || minor_ < kMinimumPublicApiMinor)
	{
		status_ = ReGameDllProbeStatus::IncompatibleVersion;
		return;
	}
	status_ = ReGameDllProbeStatus::MetadataOnly;
	api_ = api;
}

bool ReGameDllObservationProvider::connectBlindEvents(BlindListener listener, void *context)
{
	if (blindRegistry_)
		return listener == blindListener_ && context == blindContext_;
	if (!api_ || !listener || (eventOwner_ && eventOwner_ != this))
		return false;
	const auto *functions = api_->GetFuncs();
	auto *hooks = api_->GetHookchains();
	if (!functions || !functions->UTIL_FindEntityByString || !hooks)
		return false;
	auto *registry = hooks->CBasePlayer_Blind();
	if (!registry)
		return false;
	functions_ = functions;
	blindRegistry_ = registry;
	blindListener_ = listener;
	blindContext_ = context;
	eventOwner_ = this;
	registry->registerHook(&onBlind, HC_PRIORITY_LOW);
	return eventOwner_ == this && blindRegistry_ == registry;
}

bool ReGameDllObservationProvider::connectDamageEvents(DamageListener listener, void *context)
{
	if (damageRegistry_)
		return listener == damageListener_ && context == damageContext_;
	if (!api_ || !listener || (eventOwner_ && eventOwner_ != this))
		return false;
	const auto *functions = api_->GetFuncs();
	auto *hooks = api_->GetHookchains();
	if (!functions || !functions->UTIL_FindEntityByString || !hooks)
		return false;
	auto *registry = hooks->CBasePlayer_TakeDamage();
	if (!registry || (deferredDamageRegistry_ && deferredDamageRegistry_ != registry))
		return false;
	functions_ = functions;
	damageRegistry_ = registry;
	damageListener_ = listener;
	damageContext_ = context;
	eventOwner_ = this;
	if (deferredDamageRegistry_)
		deferredDamageRegistry_ = nullptr;
	else
		registry->registerHook(&onDamage, HC_PRIORITY_UNINTERRUPTABLE);
	return eventOwner_ == this && damageRegistry_ == registry;
}

bool ReGameDllObservationProvider::connectWeaponEvents(WeaponListener listener, void *context)
{
	if (weaponRegistry_)
		return listener == weaponListener_ && context == weaponContext_;
	if (!api_ || minor_ < kWeaponEventApiMinor || !listener || (eventOwner_ && eventOwner_ != this))
		return false;
	auto *hooks = api_->GetHookchains();
	auto *registry = hooks ? hooks->CBotManager_OnEvent() : nullptr;
	if (!registry || (deferredWeaponRegistry_ && deferredWeaponRegistry_ != registry))
		return false;
	weaponRegistry_ = registry;
	weaponListener_ = listener;
	weaponContext_ = context;
	eventOwner_ = this;
	if (deferredWeaponRegistry_)
		deferredWeaponRegistry_ = nullptr;
	else
		registry->registerHook(&onWeapon, HC_PRIORITY_LOW);
	return eventOwner_ == this && weaponRegistry_ == registry;
}

void ReGameDllObservationProvider::onWeapon(
	WeaponChain *chain, GameEventType event, CBaseEntity *source, CBaseEntity *other)
{
	++weaponDispatchDepth_;
	const auto *owner = eventOwner_;
	if (source && owner && owner->weaponListener_)
	{
		const auto listener = owner->weaponListener_;
		void *context = owner->weaponContext_;
		switch (event)
		{
		case EVENT_WEAPON_FIRED:
			listener(context, NativeWeaponEvent::Fired, source);
			break;
		case EVENT_WEAPON_FIRED_ON_EMPTY:
			listener(context, NativeWeaponEvent::FiredOnEmpty, source);
			break;
		case EVENT_WEAPON_RELOADED:
			listener(context, NativeWeaponEvent::Reloaded, source);
			break;
		default:
			break;
		}
	}
	chain->callNext(event, source, other);
	if (--weaponDispatchDepth_ == 0 && deferredWeaponRegistry_)
	{
		auto *registry = deferredWeaponRegistry_;
		deferredWeaponRegistry_ = nullptr;
		registry->unregisterHook(&onWeapon);
	}
}

void ReGameDllObservationProvider::onBlind(
	BlindChain *chain, CBasePlayer *player, float duration, float hold, float fade, int alpha)
{
	auto *owner = eventOwner_;
	const auto revision = owner ? owner->eventRevision_ : 0U;
	chain->callNext(player, duration, hold, fade, alpha);
	if (player && owner && owner == eventOwner_ && owner->eventRevision_ == revision && owner->blindListener_)
		owner->blindListener_(owner->blindContext_, player, duration);
}

int ReGameDllObservationProvider::onDamage(DamageChain *chain, CBasePlayer *player, entvars_s *inflictor,
	entvars_s *attacker, float &damage, int damageType)
{
	++damageDispatchDepth_;
	const auto *owner = eventOwner_;
	if (player && inflictor && owner && owner->damageListener_)
		owner->damageListener_(owner->damageContext_, player, inflictor);
	const int result = chain->callNext(player, inflictor, attacker, damage, damageType);
	if (--damageDispatchDepth_ == 0 && deferredDamageRegistry_)
	{
		auto *registry = deferredDamageRegistry_;
		deferredDamageRegistry_ = nullptr;
		registry->unregisterHook(&onDamage);
	}
	return result;
}

const void *ReGameDllObservationProvider::findClientObject(const char *name) const
{
	return functions_ && name && *name ? functions_->UTIL_FindEntityByString(nullptr, "netname", name) : nullptr;
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
		if (weaponRegistry_)
			return "ReGameDLL API negotiated; public bot weapon event hook active";
		if (blindRegistry_ || damageRegistry_)
			return "ReGameDLL API negotiated; public player event hooks active";
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
