#ifndef ASTRABOT_ADAPTER_METAMOD_OBSERVATION_ADAPTER_HPP
#define ASTRABOT_ADAPTER_METAMOD_OBSERVATION_ADAPTER_HPP

#include "astrabot/compat/observation.hpp"
#include "astrabot/metamod/abi_contract.hpp"

namespace astrabot
{
namespace metamod
{
enum class ObservationAdapterResult
{
	Accepted,
	InvalidArgument,
	EngineUnavailable,
	InvalidEntity
};

class ObservationAdapter
{
public:
	ObservationAdapter();

	void configure(enginefuncs_t *engineFunctions, globalvars_t *globals);
	ObservationAdapterResult collectActor(
		edict_t *entity,
		const world::ActorKey &actor,
		const world::FrameIdentity &frame,
		const compat::ObservationTimingContext &timing,
		compat::CompatibilityObservation *observation) const;
	ObservationAdapterResult collectPlantedBomb(
		edict_t *entity,
		const char *classname,
		const char *model,
		float currentTime,
		const world::ActorKey &actor,
		const world::FrameIdentity &frame,
		const compat::ObservationTimingContext &timing,
		compat::ObjectiveObservation *observation) const;
	void setTraceSink(compat::IObservationTraceSink *sink);

private:
	enginefuncs_t *engineFunctions_;
	globalvars_t *globals_;
	compat::IObservationTraceSink *traceSink_;
};
}
}

#endif
