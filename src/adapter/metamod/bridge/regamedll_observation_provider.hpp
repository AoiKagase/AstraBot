#ifndef ASTRABOT_METAMOD_REGAMEDLL_OBSERVATION_PROVIDER_HPP
#define ASTRABOT_METAMOD_REGAMEDLL_OBSERVATION_PROVIDER_HPP

#include "astrabot/compat/public_observation.hpp"
#include <cstdint>

class CBasePlayer;
class IReGameApi;
struct ReGameFuncs_t;
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
	ReGameDllObservationProvider() = default;
	~ReGameDllObservationProvider() override;
	ReGameDllObservationProvider(const ReGameDllObservationProvider &) = delete;
	ReGameDllObservationProvider &operator=(const ReGameDllObservationProvider &) = delete;

	void reset();
	void probe(Factory factory);
	void probeLoadedGameDll();
	bool connectBlindEvents(BlindListener listener, void *context);
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
	static void onBlind(BlindChain *chain, CBasePlayer *player, float duration, float hold, float fade, int alpha);
	static ReGameDllObservationProvider *blindOwner_;
	IReGameApi *api_ = nullptr;
	const ReGameFuncs_t *functions_ = nullptr;
	BlindRegistry *blindRegistry_ = nullptr;
	BlindListener blindListener_ = nullptr;
	void *blindContext_ = nullptr;
	std::uint64_t blindRevision_ = 0;
	ReGameDllProbeStatus status_ = ReGameDllProbeStatus::NotPresent;
	int major_ = 0;
	int minor_ = 0;
};
}
}

#endif
