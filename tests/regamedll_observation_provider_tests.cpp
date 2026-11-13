#include "bridge/regamedll_observation_provider.hpp"
#include "astrabot_regamedll_api_view.hpp"

#include <cstdio>
#include <cstring>

namespace
{
// Implement the SDK interface itself, not a guessed binary layout.
class TestApi final : public IReGameApi
{
public:
	int major = REGAMEDLL_API_VERSION_MAJOR;
	int minor = REGAMEDLL_API_VERSION_MINOR;
	int GetMajorVersion() override { return major; }
	int GetMinorVersion() override { return minor; }
	const ReGameFuncs_t *GetFuncs() override { return nullptr; }
	IReGameHookchains *GetHookchains() override { return nullptr; }
	CGameRules *GetGameRules() override { return nullptr; }
	WeaponInfoStruct *GetWeaponInfo(int) override { return nullptr; }
	WeaponInfoStruct *GetWeaponInfo(const char *) override { return nullptr; }
	playermove_s *GetPlayerMove() override { return nullptr; }
	WeaponSlotInfo *GetWeaponSlot(WeaponIdType) override { return nullptr; }
	WeaponSlotInfo *GetWeaponSlot(const char *) override { return nullptr; }
	ItemInfo *GetItemInfo(WeaponIdType) override { return nullptr; }
	AmmoInfo *GetAmmoInfo(AmmoType) override { return nullptr; }
};
TestApi api;
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
}

int main()
{
	using namespace astrabot;
	using namespace metamod;
	ReGameDllObservationProvider provider;
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
		return std::memcmp(before, &observation, sizeof(observation)) == 0 &&
			!caps.weaponMessages && !caps.economyMessages && !caps.statusMessages &&
			!caps.sameTickWeaponTimers && !caps.privateAccuracy;
	};
	if (provider.status() != ReGameDllProbeStatus::NotPresent || !unchanged()) return 1;
	provider.probe(nullptr);
	if (provider.status() != ReGameDllProbeStatus::NoFactory || !unchanged()) return 2;
	provider.probe(absent);
	if (provider.status() != ReGameDllProbeStatus::NoInterface || !unchanged()) return 3;
	api.major = REGAMEDLL_API_VERSION_MAJOR + 1;
	provider.probe(factory);
	if (!correctName || provider.status() != ReGameDllProbeStatus::IncompatibleVersion || !unchanged()) return 4;
	api.major = REGAMEDLL_API_VERSION_MAJOR;
	api.minor = REGAMEDLL_API_VERSION_MINOR - 1;
	provider.probe(factory);
	if (provider.status() != ReGameDllProbeStatus::IncompatibleVersion || !unchanged()) return 5;
	api.minor = REGAMEDLL_API_VERSION_MINOR;
	provider.probe(factory);
	if (provider.status() != ReGameDllProbeStatus::MetadataOnly || !unchanged()) return 6;
	api.minor += 1;
	provider.probe(factory);
	if (provider.status() != ReGameDllProbeStatus::MetadataOnly || !unchanged()) return 7;
	provider.reset();
	if (provider.status() != ReGameDllProbeStatus::NotPresent || provider.majorVersion() != 0 ||
		provider.minorVersion() != 0 || !unchanged()) return 8;
	// A standalone test process has no GameDLL; probe must not load one.
	provider.probeLoadedGameDll();
	if (provider.status() != ReGameDllProbeStatus::NotPresent || !unchanged()) return 9;
	std::puts("ReGameDLL optional provider: fallback, versions and preservation passed");
	return 0;
}
