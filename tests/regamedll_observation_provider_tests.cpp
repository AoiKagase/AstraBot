#include "bridge/regamedll_observation_provider.hpp"
#include "astrabot_regamedll_api_view.hpp"
#include "astrabot_regamedll_test_hooks.hpp"

#include <cstdio>
#include <cstring>

namespace
{
class TestBlindRegistry final : public IReGameHookRegistry_CBasePlayer_Blind
{
public:
	hookfunc_t hook = nullptr;
	void registerHook(hookfunc_t callback, int) override { hook = callback; }
	void unregisterHook(hookfunc_t callback) override
	{
		if (hook == callback)
			hook = nullptr;
	}
};

class TestDamageRegistry final : public IReGameHookRegistry_CBasePlayer_TakeDamage
{
public:
	hookfunc_t hook = nullptr;
	int priority = -1;
	void registerHook(hookfunc_t callback, int order) override { hook = callback; priority = order; }
	void unregisterHook(hookfunc_t callback) override
	{
		if (hook == callback)
			hook = nullptr;
	}
};

class TestHookchains final : public EmptyReGameHookchains
{
public:
	class WeaponRegistry final : public IReGameHookRegistry_CBotManager_OnEvent
	{
	public:
		hookfunc_t hook = nullptr;
		void registerHook(hookfunc_t callback, int) override { hook = callback; }
		void unregisterHook(hookfunc_t callback) override
		{
			if (hook == callback)
				hook = nullptr;
		}
	} weapon;
	TestBlindRegistry blind;
	TestDamageRegistry damage;
	IReGameHookRegistry_CBotManager_OnEvent *CBotManager_OnEvent() override { return &weapon; }
	IReGameHookRegistry_CBasePlayer_Blind *CBasePlayer_Blind() override { return &blind; }
	IReGameHookRegistry_CBasePlayer_TakeDamage *CBasePlayer_TakeDamage() override { return &damage; }
};

class TestBlindChain final : public IReGameHook_CBasePlayer_Blind
{
public:
	int calls = 0;
	astrabot::metamod::ReGameDllObservationProvider *resetOnNext = nullptr;
	void callNext(CBasePlayer *, float, float, float, int) override
	{
		++calls;
		if (resetOnNext)
			resetOnNext->reset();
	}
	void callOriginal(CBasePlayer *, float, float, float, int) override { calls += 100; }
};

// Implement the SDK interface itself, not a guessed binary layout.
class TestApi final : public IReGameApi
{
public:
	int major = REGAMEDLL_API_VERSION_MAJOR;
	int minor = REGAMEDLL_API_VERSION_MINOR;
	TestHookchains hooks;
	ReGameFuncs_t funcs{};
	bool enableHooks = false;
	int GetMajorVersion() override
	{
		return major;
	}

	int GetMinorVersion() override
	{
		return minor;
	}

	const ReGameFuncs_t *GetFuncs() override
	{
		return enableHooks ? &funcs : nullptr;
	}

	IReGameHookchains *GetHookchains() override
	{
		return enableHooks ? &hooks : nullptr;
	}

	CGameRules *GetGameRules() override
	{
		return nullptr;
	}

	WeaponInfoStruct *GetWeaponInfo(int) override
	{
		return nullptr;
	}

	WeaponInfoStruct *GetWeaponInfo(const char *) override
	{
		return nullptr;
	}

	playermove_s *GetPlayerMove() override
	{
		return nullptr;
	}

	WeaponSlotInfo *GetWeaponSlot(WeaponIdType) override
	{
		return nullptr;
	}

	WeaponSlotInfo *GetWeaponSlot(const char *) override
	{
		return nullptr;
	}

	ItemInfo *GetItemInfo(WeaponIdType) override
	{
		return nullptr;
	}

	AmmoInfo *GetAmmoInfo(AmmoType) override
	{
		return nullptr;
	}
	AmmoInfoStruct *GetAmmoInfoEx(AmmoType) override { return nullptr; }
	AmmoInfoStruct *GetAmmoInfoEx(const char *) override { return nullptr; }
	bool BGetICSEntity(const char *) const override { return false; }
	bool BGetIGameRules(const char *) const override { return false; }
};
TestApi api;
int botObject = 0;
bool correctName = false;
void *factory(const char *name, int *result)
{
	correctName = std::strcmp(name, VRE_GAMEDLL_API_VERSION) == 0;
	*result = 0;
	return &api;
}

void *absent(const char *, int *result)
{
	*result = 1;
	return nullptr;
}

bool testBlindEvents(astrabot::metamod::ReGameDllObservationProvider &provider)
{
	api.enableHooks = true;
	api.funcs.UTIL_FindEntityByString = [](CBaseEntity *start, const char *key, const char *name) -> CBaseEntity *
	{
		return !start && std::strcmp(key, "netname") == 0 && std::strcmp(name, "Test Bot") == 0
			? reinterpret_cast<CBaseEntity *>(&botObject) : nullptr;
	};
	provider.probe(factory);
	struct Notice { int count = 0; const void *player = nullptr; float duration = 0; } notice;
	if (!provider.connectBlindEvents([](void *context, const void *player, float duration) {
			auto &event = *static_cast<Notice *>(context);
			++event.count;
			event.player = player;
			event.duration = duration;
		}, &notice) || !api.hooks.blind.hook)
	{
		std::fputs("failed to subscribe to the public ReGameDLL player blindness hook\n", stderr);
		return false;
	}
	TestBlindChain chain;
	auto *player = reinterpret_cast<CBasePlayer *>(&botObject);
	api.hooks.blind.hook(&chain, player, 1.25f, 2.0f, 3.0f, 255);
	if (chain.calls != 1 || notice.count != 1 || notice.player != player || notice.duration != 1.25f ||
		provider.findClientObject("Test Bot") != player || provider.findClientObject("Other Bot") != nullptr)
		return false;
	chain.resetOnNext = &provider;
	api.hooks.blind.hook(&chain, player, 1.0f, 1.0f, 1.0f, 200);
	api.enableHooks = false;
	return chain.calls == 2 && notice.count == 1 && !api.hooks.blind.hook;
}

bool testDamageEvents(astrabot::metamod::ReGameDllObservationProvider &provider)
{
	api.enableHooks = true;
	provider.probe(factory);
	if (!provider.connectBlindEvents([](void *, const void *, float) {}, nullptr) || !api.hooks.blind.hook)
		return false;
	struct Notice
	{
		const void *victim = nullptr;
		const entvars_s *source = nullptr;
		int count = 0;
		astrabot::metamod::ReGameDllObservationProvider *resetProvider = nullptr;
	} notice;
	if (!provider.connectDamageEvents([](void *context, const void *victim, const entvars_s *source) {
			auto &event = *static_cast<Notice *>(context);
			event.victim = victim;
			event.source = source;
			++event.count;
			if (event.resetProvider)
				event.resetProvider->reset();
		}, &notice) || !api.hooks.damage.hook || api.hooks.damage.priority != HC_PRIORITY_UNINTERRUPTABLE)
		return false;
	int inflictorObject = 0, attackerObject = 0;
	class Chain final : public IReGameHook_CBasePlayer_TakeDamage
	{
	public:
		explicit Chain(const Notice &event) : event(event) {}
		const Notice &event;
		const entvars_s *expectedAttacker = nullptr;
		int calls = 0;
		bool observedBeforeRules = false;
		int callNext(CBasePlayer *victim, entvars_s *source, entvars_s *attacker, float &amount, int type) override
		{
			++calls;
			observedBeforeRules = api.hooks.damage.hook && event.count == calls && event.victim == victim && event.source == source &&
				attacker == expectedAttacker && amount == 7.0f && type == 2;
			amount = 3.0f;
			return 0;
		}
		int callOriginal(CBasePlayer *, entvars_s *, entvars_s *, float &, int) override { calls += 100; return 1; }
	} chain(notice);
	auto *victim = reinterpret_cast<CBasePlayer *>(&botObject);
	auto *inflictor = reinterpret_cast<entvars_s *>(&inflictorObject);
	auto *attacker = reinterpret_cast<entvars_s *>(&attackerObject);
	chain.expectedAttacker = attacker;
	float amount = 7.0f;
	const int result = api.hooks.damage.hook(&chain, victim, inflictor, attacker, amount, 2);
	if (result != 0 || chain.calls != 1 || !chain.observedBeforeRules || amount != 3.0f)
		return false;
	notice.resetProvider = &provider;
	amount = 7.0f;
	const int resetResult = api.hooks.damage.hook(&chain, victim, inflictor, attacker, amount, 2);
	api.enableHooks = false;
	return resetResult == 0 && chain.calls == 2 && chain.observedBeforeRules && amount == 3.0f &&
		!api.hooks.damage.hook && !api.hooks.blind.hook;
}
}

bool testWeaponEvents(astrabot::metamod::ReGameDllObservationProvider &provider)
{
	using astrabot::metamod::NativeWeaponEvent;
	class Chain final : public IReGameHook_CBotManager_OnEvent
	{
	public:
		int count = 0;
		void callNext(GameEventType, CBaseEntity *, CBaseEntity *) override { ++count; }
		void callOriginal(GameEventType, CBaseEntity *, CBaseEntity *) override {}
	} chain;
	struct Notice { int count = 0; NativeWeaponEvent event{}; const void *source = nullptr; } notice;
	const auto receive = [](void *context, NativeWeaponEvent kind, const void *source) {
		auto &event = *static_cast<Notice *>(context);
		++event.count;
		event.event = kind;
		event.source = source;
	};
	api.enableHooks = true;
	api.minor = 3;
	provider.probe(factory);
	if (provider.connectWeaponEvents(receive, &notice) || api.hooks.weapon.hook)
		return false;
	api.minor = REGAMEDLL_API_VERSION_MINOR;
	provider.probe(factory);
	if (!provider.connectWeaponEvents(receive, &notice) || !api.hooks.weapon.hook)
		return false;
	auto *source = reinterpret_cast<CBaseEntity *>(&botObject);
	api.hooks.weapon.hook(&chain, EVENT_WEAPON_RELOADED, source, nullptr);
	if (notice.count != 1 || notice.event != NativeWeaponEvent::Reloaded || notice.source != source)
		return false;
	api.hooks.weapon.hook(&chain, EVENT_WEAPON_FIRED, source, nullptr);
	if (notice.count != 2 || notice.event != NativeWeaponEvent::Fired)
		return false;
	api.hooks.weapon.hook(&chain, EVENT_WEAPON_FIRED_ON_EMPTY, source, nullptr);
	if (notice.count != 3 || notice.event != NativeWeaponEvent::FiredOnEmpty)
		return false;
	api.hooks.weapon.hook(&chain, EVENT_GRENADE_BOUNCED, source, nullptr);
	api.hooks.weapon.hook(&chain, EVENT_WEAPON_RELOADED, nullptr, nullptr);
	provider.reset();
	if (chain.count != 5 || notice.count != 3 || api.hooks.weapon.hook)
		return false;

	provider.probe(factory);
	bool remainedRegistered = false;
	struct ResetNotice
	{
		astrabot::metamod::ReGameDllObservationProvider *provider;
		bool *remainedRegistered;
	} resetNotice{&provider, &remainedRegistered};
	if (!provider.connectWeaponEvents([](void *context, NativeWeaponEvent, const void *) {
			const auto &reset = *static_cast<ResetNotice *>(context);
			reset.provider->reset();
			*reset.remainedRegistered = api.hooks.weapon.hook && reset.provider->dispatchingEvents();
		}, &resetNotice))
		return false;
	api.hooks.weapon.hook(&chain, EVENT_WEAPON_RELOADED, source, nullptr);
	return chain.count == 6 && remainedRegistered && !api.hooks.weapon.hook && !provider.dispatchingEvents();
}

int main()
{
	using namespace astrabot;
	using namespace metamod;
	ReGameDllObservationProvider provider;
	if (!testBlindEvents(provider))
		return 11;
	if (!testDamageEvents(provider))
		return 12;
	if (!testWeaponEvents(provider))
	{
		std::fputs("native weapon events must subscribe, filter, forward and disconnect\n", stderr);
		return 13;
	}
	compat::CompatibilityObservation observation{};
	observation.actor = {1U, 9U};
	observation.frame = {2U, 3U, 10U};
	compat::PublicObservationStore publicProvider;
	publicProvider.bind(observation.actor, observation.frame);
	publicProvider.currentWeapon(observation.actor, observation.frame, 1, 22, 17);
	publicProvider.supplement(&observation);
	if (!observation.weapon.clip.valid || observation.weapon.clip.value != 17U)
		return 10;
	unsigned char before[sizeof(observation)];
	std::memcpy(before, &observation, sizeof(observation));
	auto unchanged = [&]()
	{
		provider.supplement(&observation);
		provider.supplement(nullptr);
		const auto caps = provider.capabilities();
		return std::memcmp(before, &observation, sizeof(observation)) == 0 && !caps.weaponMessages &&
			!caps.economyMessages && !caps.statusMessages && !caps.sameTickWeaponTimers && !caps.privateAccuracy;
	};
	if (provider.status() != ReGameDllProbeStatus::NotPresent || !unchanged())
		return 1;
	provider.probe(nullptr);
	if (provider.status() != ReGameDllProbeStatus::NoFactory || !unchanged())
		return 2;
	provider.probe(absent);
	if (provider.status() != ReGameDllProbeStatus::NoInterface || !unchanged())
		return 3;
	api.major = REGAMEDLL_API_VERSION_MAJOR + 1;
	provider.probe(factory);
	if (!correctName || provider.status() != ReGameDllProbeStatus::IncompatibleVersion || !unchanged())
		return 4;
	api.major = REGAMEDLL_API_VERSION_MAJOR;
	api.minor = 2;
	provider.probe(factory);
	if (provider.status() != ReGameDllProbeStatus::IncompatibleVersion || !unchanged())
		return 5;
	api.minor = 3;
	provider.probe(factory);
	if (provider.status() != ReGameDllProbeStatus::MetadataOnly || !unchanged())
		return 6;
	api.minor += 1;
	provider.probe(factory);
	if (provider.status() != ReGameDllProbeStatus::MetadataOnly || !unchanged())
		return 7;
	provider.reset();
	if (provider.status() != ReGameDllProbeStatus::NotPresent || provider.majorVersion() != 0 ||
		provider.minorVersion() != 0 || !unchanged())
		return 8;
	// A standalone test process has no GameDLL; probe must not load one.
	provider.probeLoadedGameDll();
	if (provider.status() != ReGameDllProbeStatus::NotPresent || !unchanged())
		return 9;
	std::puts("ReGameDLL optional provider: fallback, versions and preservation passed");
	return 0;
}
