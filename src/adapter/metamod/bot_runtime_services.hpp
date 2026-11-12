#ifndef ASTRABOT_ADAPTER_BOT_RUNTIME_SERVICES_HPP
#define ASTRABOT_ADAPTER_BOT_RUNTIME_SERVICES_HPP
#include "bot_runtime_types.hpp"
namespace astrabot
{
namespace metamod
{
class BotObservationRuntime;
class BotBehaviorRuntime;
class BotNavigationRuntime;
class BotCommandRuntime;
class BotRuntimeServices
{
  public:
	virtual ~BotRuntimeServices() = default;
	using Service_adapterFrameCount = std::uint32_t;
	using Service_compatibilityRandomSource = compat::EngineRandomSource;
	using Service_compatibilitySurface = CompatibilitySurface;
	using Service_engineFunctions = enginefuncs_t *;
	using Service_globals = globalvars_t *;
	using Service_inputDispatcher = InputDispatcher;
	using Service_joinControllers =
		std::array<JoinController, NativeBotObservation::kClientSlotCount>;
	using Service_lifecycle = runtime::LifecycleSession;
	using Service_managedBotFullUpdateSequences =
		std::array<std::uint32_t, NativeBotObservation::kClientSlotCount>;
	using Service_managedBotHandles =
		std::array<FakeClientHandle, NativeBotObservation::kClientSlotCount>;
	using Service_managedBotSlots = std::array<bool, NativeBotObservation::kClientSlotCount>;
	using Service_managedBotTiming =
		std::array<runtime::BotTimingScheduler, NativeBotObservation::kClientSlotCount>;
	using Service_navPublisher = nav::NavSnapshotPublisher;
	using Service_pluginId = plid_t;
	using Service_roundFreezeGeneration = runtime::LifecycleGeneration;
	using Service_roundFreezeScheduled = bool;
	using Service_roundFreezeStartTime = float;
	using Service_roundFreezeUntil = float;
	using Service_runtimeProfiler = RuntimeProfiler;
	virtual BotObservationRuntime &observation() = 0;
	virtual BotBehaviorRuntime &behavior() = 0;
	virtual BotNavigationRuntime &navigation() = 0;
	virtual BotCommandRuntime &command() = 0;
	virtual const Service_adapterFrameCount &service_adapterFrameCount() = 0;
	virtual Service_compatibilityRandomSource &service_compatibilityRandomSource() = 0;
	virtual const Service_compatibilitySurface &service_compatibilitySurface() = 0;
	virtual const Service_engineFunctions &service_engineFunctions() = 0;
	virtual const Service_globals &service_globals() = 0;
	virtual Service_inputDispatcher &service_inputDispatcher() = 0;
	virtual const Service_joinControllers &service_joinControllers() = 0;
	virtual const Service_lifecycle &service_lifecycle() = 0;
	virtual const Service_managedBotFullUpdateSequences &
	service_managedBotFullUpdateSequences() = 0;
	virtual const Service_managedBotHandles &service_managedBotHandles() = 0;
	virtual const Service_managedBotSlots &service_managedBotSlots() = 0;
	virtual const Service_managedBotTiming &service_managedBotTiming() = 0;
	virtual const Service_navPublisher &service_navPublisher() = 0;
	virtual const Service_pluginId &service_pluginId() = 0;
	virtual const Service_roundFreezeGeneration &service_roundFreezeGeneration() = 0;
	virtual const Service_roundFreezeScheduled &service_roundFreezeScheduled() = 0;
	virtual const Service_roundFreezeStartTime &service_roundFreezeStartTime() = 0;
	virtual const Service_roundFreezeUntil &service_roundFreezeUntil() = 0;
	virtual Service_runtimeProfiler &service_runtimeProfiler() = 0;
	virtual void observeJoinTeamInfo(std::size_t index, const char *teamName) = 0;
	virtual void applyNavigationRequest(const NavigationRequest &request) = 0;
	virtual std::uint8_t consumeCommandMsec(std::size_t index, float now) = 0;
	virtual bool runtimeIsActive() const = 0;
	virtual void deferJoinAction(std::size_t index, const JoinAction &action) = 0;
	virtual bool dispatchClientCommand(edict_t *entity, const char *name, const char *argument) = 0;
	virtual void notifyMenuReady(edict_t *entity, JoinMenuKind menu, std::uint16_t validSlots = 0U,
								 JoinMenuSource source = JoinMenuSource::Unknown) = 0;
	virtual std::uint32_t publicPlayerPriority(const edict_t *entity) const = 0;
	virtual bool roundFreezeActive() const = 0;
};
} // namespace metamod
} // namespace astrabot
#endif
