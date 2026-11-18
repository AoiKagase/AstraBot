#ifndef ASTRABOT_ADAPTER_METAMOD_OBSERVATION_ADAPTER_HPP
#define ASTRABOT_ADAPTER_METAMOD_OBSERVATION_ADAPTER_HPP

#include "astrabot/compat/observation.hpp"
#include "astrabot/compat/public_observation.hpp"
#include "astrabot/compat/smoke_observation.hpp"
#include "astrabot/metamod/abi_contract.hpp"
#include "astrabot/metamod/runtime_profiler.hpp"
#include "astrabot/perception/perception.hpp"

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
	static bool isLooseBombBox(const edict_t *entity, const char *classname, const char *model);
	ObservationAdapter();

	void configure(enginefuncs_t *engineFunctions, globalvars_t *globals);
	ObservationAdapterResult collectActor(edict_t *entity, const world::ActorKey &actor,
		const world::FrameIdentity &frame, const compat::ObservationTimingContext &timing,
		compat::CompatibilityObservation *observation) const;
	ObservationAdapterResult collectVisibility(edict_t *observer, edict_t *target, const world::ActorKey &targetActor,
		const world::FrameIdentity &frame, perception::VisionObservation *observation, bool checkFov = true) const;
	ObservationAdapterResult collectPlantedBomb(edict_t *entity, const char *classname, const char *model,
		float currentTime, const world::ActorKey &actor, const world::FrameIdentity &frame,
		const compat::ObservationTimingContext &timing, compat::ObjectiveObservation *observation) const;
	ObservationAdapterResult collectDroppedC4(edict_t *entity, const char *classname, const char *model,
		const world::ActorKey &actor, const world::FrameIdentity &frame, const compat::ObservationTimingContext &timing,
		compat::DroppedC4Observation *observation) const;
	void setTraceSink(compat::IObservationTraceSink *sink);
	void setSmokeObservations(const compat::SmokeObservations *smoke);
	void setObservationProviders(const compat::IObservationProvider *publicProvider,
		const compat::IObservationProvider *optionalGameProvider = nullptr);
	void setProfiler(RuntimeProfiler *profiler);
	void setPerformanceToggles(bool disableVision, bool disableTrace);

private:
	enginefuncs_t *engineFunctions_;
	globalvars_t *globals_;
	compat::IObservationTraceSink *traceSink_;
	mutable std::uint64_t traceSequence_;
	RuntimeProfiler *profiler_;
	bool disableVision_;
	bool disableTrace_;
	const compat::IObservationProvider *publicProvider_ = nullptr;
	const compat::IObservationProvider *optionalGameProvider_ = nullptr;
	const compat::SmokeObservations *smoke_ = nullptr;
};
}
}

#endif
