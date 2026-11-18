#ifndef ASTRABOT_METAMOD_REGAMEDLL_OBSERVATION_PROVIDER_HPP
#define ASTRABOT_METAMOD_REGAMEDLL_OBSERVATION_PROVIDER_HPP

#include "astrabot/compat/public_observation.hpp"
#include <cstdint>

class CBasePlayer;
class IReGameApi;
struct ReGameFuncs_t;
struct entvars_s;
template<typename Result, typename Entity, typename... Args> class IHookChainClass;
template<typename Result, typename Entity, typename... Args> class IHookChainRegistryClass;
template<typename Player, typename... Args> class IVoidHookChainClass;
template<typename Player, typename... Args> class IVoidHookChainRegistryClass;

namespace astrabot
{
namespace metamod
{
enum class ReGameDllProbeStatus
{
	NotPresent,
	NoFactory,
	NoInterface,
	IncompatibleVersion,
	MetadataOnly
};

// This provider never dereferences GameDLL player, weapon or gamerules layouts.
// The negotiated API exposes metadata, but no safe getters for the per-actor
// private weapon fields in CompatibilityObservation.
class ReGameDllObservationProvider final : public compat::IObservationProvider
{
public:
	using Factory = void *(*)(const char *, int *);
	using BlindListener = void (*)(void *, const void *, float);
	using DamageListener = void (*)(void *, const void *, const entvars_s *);
	ReGameDllObservationProvider() = default;
	~ReGameDllObservationProvider() override;
	ReGameDllObservationProvider(const ReGameDllObservationProvider &) = delete;
	ReGameDllObservationProvider &operator=(const ReGameDllObservationProvider &) = delete;

	void reset();
	void probe(Factory factory);
	void probeLoadedGameDll();
	bool connectBlindEvents(BlindListener listener, void *context);
	bool connectDamageEvents(DamageListener listener, void *context);
	bool dispatchingDamage() const { return damageDispatchDepth_ != 0; }
	const void *findClientObject(const char *name) const;
	ReGameDllProbeStatus status() const
	{
		return status_;
	}

	int majorVersion() const
	{
		return major_;
	}

	int minorVersion() const
	{
		return minor_;
	}

	const char *diagnostic() const;
	compat::ObservationCapabilities capabilities() const override;
	void supplement(compat::CompatibilityObservation *observation) const override;

private:
	using BlindChain = IVoidHookChainClass<CBasePlayer, float, float, float, int>;
	using BlindRegistry = IVoidHookChainRegistryClass<CBasePlayer, float, float, float, int>;
	using DamageChain = IHookChainClass<int, CBasePlayer, entvars_s *, entvars_s *, float &, int>;
	using DamageRegistry = IHookChainRegistryClass<int, CBasePlayer, entvars_s *, entvars_s *, float &, int>;
	static void onBlind(BlindChain *chain, CBasePlayer *player, float duration, float hold, float fade, int alpha);
	static int onDamage(DamageChain *chain, CBasePlayer *player, entvars_s *inflictor, entvars_s *attacker,
		float &damage, int damageType);
	static ReGameDllObservationProvider *eventOwner_;
	static DamageRegistry *deferredDamageRegistry_;
	static unsigned damageDispatchDepth_;
	IReGameApi *api_ = nullptr;
	const ReGameFuncs_t *functions_ = nullptr;
	BlindRegistry *blindRegistry_ = nullptr;
	BlindListener blindListener_ = nullptr;
	void *blindContext_ = nullptr;
	DamageRegistry *damageRegistry_ = nullptr;
	DamageListener damageListener_ = nullptr;
	void *damageContext_ = nullptr;
	std::uint64_t eventRevision_ = 0;
	ReGameDllProbeStatus status_ = ReGameDllProbeStatus::NotPresent;
	int major_ = 0;
	int minor_ = 0;
};
}
}

#endif
