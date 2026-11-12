#ifndef ASTRABOT_METAMOD_REGAMEDLL_OBSERVATION_PROVIDER_HPP
#define ASTRABOT_METAMOD_REGAMEDLL_OBSERVATION_PROVIDER_HPP

#include "astrabot/compat/public_observation.hpp"

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

	void reset();
	void probe(Factory factory);
	void probeLoadedGameDll();
	ReGameDllProbeStatus status() const { return status_; }
	int majorVersion() const { return major_; }
	int minorVersion() const { return minor_; }
	const char *diagnostic() const;
	compat::ObservationCapabilities capabilities() const override;
	void supplement(compat::CompatibilityObservation *observation) const override;

private:
	ReGameDllProbeStatus status_ = ReGameDllProbeStatus::NotPresent;
	int major_ = 0;
	int minor_ = 0;
};
}
}

#endif
