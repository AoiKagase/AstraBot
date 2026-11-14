#ifndef ASTRABOT_TEST_RUNTIME_COMMAND_SCENARIO_FIXTURE_HPP
#define ASTRABOT_TEST_RUNTIME_COMMAND_SCENARIO_FIXTURE_HPP
#include "runtime/command/bot_command_runtime.hpp"
#include <cstdio>
#include <cstdlib>

mutil_funcs_t *gpMetaUtilFuncs = nullptr;

namespace
{
using namespace astrabot;
using namespace astrabot::metamod;
class ScenarioServices;
ScenarioServices *gScenario = nullptr;
void scenarioMove(edict_t *, const float *, float, float, float, unsigned short, byte, byte);
bool check(bool condition, const char *message)
{
	if (!condition)
	{
		std::fprintf(stderr, "scenario failed: %s\n", message);
	}
	return condition;
}

class ScenarioServices : public BotCommandServices
{
public:
	runtime::LifecycleSession lifecycle;
	runtime::ActorRegistry registry;
	InputDispatcher dispatcher{lifecycle, registry};
	enginefuncs_t engine{};
	enginefuncs_t *enginePointer = &engine;
	globalvars_t globals{};
	globalvars_t *globalsPointer = &globals;
	CompatibilitySurface surface;
	RuntimeProfiler profiler;
	Service_joinControllers joins{};
	Service_managedBotHandles handles{};
	Service_managedBotSlots slots{};
	Service_managedBotFullUpdateSequences updates{};
	plid_t pluginId = nullptr;
	std::uint32_t frame = 10U;
	float freezeUntil = 0.0f;
	mutable unsigned freezeReads = 0U;
	mutable bool freezeReadAfterMove = false;
	bool freeze = false;
	bool changeFreezeDuringMove = false;
	bool attackNoticeSawMovedOrigin = false;
	unsigned attackMovesBefore = 0U;
	float attackOriginBefore = 0.0f;
	unsigned moves = 0U;
	unsigned short lastButtons = 0U;
	edict_t actorEntity{};

	ScenarioServices()
	{
		gScenario = this;
		if (!lifecycle.activateMap() || !lifecycle.connectSlot(1U))
			std::abort();
		runtime::ActorId actor{};
		if (registry.reserve(1U, lifecycle.tokenForSlot(1U), &actor) != runtime::ActorResult::Accepted ||
			registry.beginJoin(actor) != runtime::ActorResult::Accepted ||
			registry.markJoined(actor) != runtime::ActorResult::Accepted)
			std::abort();
		handles[0].actor = actor;
		handles[0].entity = &actorEntity;
		slots[0] = true;
		actorEntity.v.flags = FL_CLIENT | FL_FAKECLIENT | FL_ONGROUND;
		actorEntity.v.health = 100.0f;
		actorEntity.v.deadflag = DEAD_NO;
		actorEntity.v.movetype = MOVETYPE_WALK;
		actorEntity.v.maxspeed = 240.0f;
		globals.time = 1.0f;
		engine.pfnRunPlayerMove = &scenarioMove;
		dispatcher.configure(&engine);
		if (!dispatcher.bindActor(actor, &actorEntity))
			std::abort();
	}

	std::uint8_t consumeCommandMsec(std::size_t, float) override
	{
		return 10U;
	}

	bool dispatchClientCommand(edict_t *, const char *, const char *) override
	{
		return {};
	}

	std::uint32_t publicPlayerPriority(const edict_t *) const override
	{
		return {};
	}

	bool roundFreezeActive() const override
	{
		++freezeReads;
		freezeReadAfterMove = freezeReadAfterMove || (freeze && moves != 0U);
		return freeze;
	}

	bool runtimeIsActive() const override
	{
		return true;
	}

	const Service_adapterFrameCount &service_adapterFrameCount() override
	{
		return frame;
	}

	const Service_compatibilitySurface &service_compatibilitySurface() override
	{
		return surface;
	}

	const Service_engineFunctions &service_engineFunctions() override
	{
		return enginePointer;
	}

	const Service_globals &service_globals() override
	{
		return globalsPointer;
	}

	Service_inputDispatcher &service_inputDispatcher() override
	{
		return dispatcher;
	}

	const Service_joinControllers &service_joinControllers() override
	{
		return joins;
	}

	const Service_lifecycle &service_lifecycle() override
	{
		return lifecycle;
	}

	const Service_managedBotFullUpdateSequences &service_managedBotFullUpdateSequences() override
	{
		return updates;
	}

	const Service_managedBotHandles &service_managedBotHandles() override
	{
		return handles;
	}

	const Service_managedBotSlots &service_managedBotSlots() override
	{
		return slots;
	}

	const Service_pluginId &service_pluginId() override
	{
		return pluginId;
	}

	const Service_roundFreezeScheduled &service_roundFreezeScheduled() override
	{
		return freeze;
	}

	const Service_roundFreezeUntil &service_roundFreezeUntil() override
	{
		return freezeUntil;
	}

	Service_runtimeProfiler &service_runtimeProfiler() override
	{
		return profiler;
	}

	const ManagedObjectiveTargetCache &behavior_managedBotObjectiveTargets(std::size_t) const override
	{
		static ManagedObjectiveTargetCache value{};
		return value;
	}

	const compat::CompatibilityStateMachine &behavior_managedBotStateMachines(std::size_t) const override
	{
		static compat::CompatibilityStateMachine value{};
		return value;
	}

	bool behavior_noiseInvestigationWalking(std::size_t) const override
	{
		return false;
	}
	void behavior_applyNoiseLookCommand(std::size_t, runtime::BotCommand &) override
	{
	}

	void behavior_applyObstacleCommand(std::size_t, runtime::BotCommand &) override
	{
	}

	void behavior_observeC4AttackDispatched(std::size_t, const FakeClientHandle &handle, std::uint16_t) override
	{
		attackNoticeSawMovedOrigin = moves == attackMovesBefore + 1U && handle.entity->v.origin.x > attackOriginBefore;
	}

	void behavior_observeC4SelectionDispatched(std::size_t, const FakeClientHandle &) override
	{
	}

	void behavior_observeEquipmentDispatched(std::size_t, std::uint16_t, bool) override
	{
	}

	void behavior_observeNeutralC4AttackDispatched(std::size_t, const FakeClientHandle &) override
	{
	}

	void navigation_logMovementDiagnostic(std::size_t, const char *, const runtime::NavRoamDecision *) override
	{
	}

	void navigation_logMovementReversalDiagnostic(std::size_t, const runtime::MovementPhysicsState &,
		const runtime::NavRoamDecision &, const nav::LocomotionIntent &) override
	{
	}

	void navigation_logTraversalDiagnostic(std::size_t, const runtime::MovementPhysicsState &,
		const runtime::NavRoamDecision &, const nav::LocomotionIntent &, std::uint16_t,
		const nav::LocomotionObservation &) override
	{
	}

	void navigation_logTraversalOutcome(std::size_t, const runtime::MovementPhysicsState &,
		const runtime::NavRoamDecision &, const runtime::LandingDamageObservation &) override
	{
	}

	const compat::NavigationStep &navigation_managedNavigationSteps(std::size_t) const override
	{
		static compat::NavigationStep value{};
		return value;
	}

	const std::uint8_t &navigation_movementDiagnosticSamples(std::size_t) const override
	{
		static const std::uint8_t samples = 255U;
		return samples;
	}

	void navigation_noteMovementDiagnosticSample(std::size_t) override
	{
	}

	void navigation_recordMovementPhysicsSample(
		std::size_t, const runtime::MovementPhysicsState &, const runtime::CommandReceipt &) override
	{
	}

	runtime::MovementPhysicsState observation_captureMovementPhysicsState(
		const edict_t *entity, bool, bool) const override
	{
		runtime::MovementPhysicsState state{};
		state.dead = entity->v.deadflag != DEAD_NO;
		state.grounded = (entity->v.flags & FL_ONGROUND) != 0;
		state.origin = {entity->v.origin.x, entity->v.origin.y, entity->v.origin.z};
		return state;
	}

	bool observation_hasObservedActiveWeapon(std::size_t) const override
	{
		return true;
	}

	const ManagedCurrentWeaponObservation &observation_managedBotCurrentWeapons(std::size_t) const override
	{
		static ManagedCurrentWeaponObservation value{};
		return value;
	}

	void observation_observeReloadDispatched(world::ActorKey, world::FrameIdentity, float) override
	{
	}

	runtime::ActorId observation_publicPlayerIdentity(const edict_t *) const override
	{
		return {};
	}
};
void scenarioMove(edict_t *entity, const float *, float, float, float, unsigned short buttons, byte, byte)
{
	++gScenario->moves;
	gScenario->lastButtons = buttons;
	entity->v.origin.x += 10.0f;
	if (gScenario->changeFreezeDuringMove)
		gScenario->freeze = true;
}
} // namespace
#endif
