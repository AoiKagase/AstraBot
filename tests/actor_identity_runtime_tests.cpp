#include "plugin/plugin_runtime.hpp"
namespace
{
bool testActorIdentity(astrabot::metamod::PluginRuntime &, enginefuncs_t &, globalvars_t &);
}
#define ASTRABOT_EXTRA_ACTOR_TEST testActorIdentity
#define ASTRABOT_ACTOR_PROFILE_TEXT                                                                                    \
	"TProfile { Name = \"T Bot\" Team = T Difficulty = 1 Skill = 70 }\nCtProfile { Name = \"CT Bot\" Team = CT Difficulty = 1 Skill = 70 }\n"
#include "compat_actor_command_tests.cpp"
namespace
{
std::array<unsigned, 2> identityShots{};
std::array<float, 2> identityYaw{};
void identityCommand(edict_t *entity)
{
	const bool joining = std::strcmp(astrabot::metamod::HookCommandArgv(0), "jointeam") == 0;
	if (joining && entity == &gEntities[1])
	{
		++gHookedClientCommandCount;
		++gClientCommandCount;
		gDirectJoinCommandSeen = true;
		entity->v.team = 2;
		auto &runtime = astrabot::metamod::PluginRuntime::instance();
		runtime.onMessageBegin(2, 86, nullptr, nullptr);
		runtime.onWriteByte(2);
		runtime.onWriteString("CT");
		runtime.onMessageEnd();
		return;
	}
	hookedGameClientCommand(entity);
}

void identityMove(edict_t *entity, const float *angles, float forward, float side, float up, unsigned short buttons,
	byte impulse, byte msec)
{
	for (unsigned i = 0; i < 2; ++i)
		if (entity == &gEntities[i])
		{
			identityYaw[i] = angles[1];
			if ((buttons & IN_ATTACK) != 0)
				++identityShots[i];
		}
	runPlayerMove(entity, angles, forward, side, up, buttons, impulse, msec);
}

bool testActorIdentity(astrabot::metamod::PluginRuntime &runtime, enginefuncs_t &engine, globalvars_t &globals)
{
	using namespace astrabot;
	engine.pfnTraceLine = &traceObjectiveVisibility;
	engine.pfnRunPlayerMove = &identityMove;
	engine.pfnRandomFloat = &siteFixtureRandomFloat;
	engine.pfnRandomLong = &siteFixtureRandomLong;
	gHookedGameDllTable.pfnClientCommand = &identityCommand;
	runtime.giveEnginePointers(&engine, &globals);
	gServerSetsEntityTeam = true;
	gObjectiveEntitiesAvailable = false;
	gObjectiveTraceVisible = true;
	runtime.setCompatibilityFloat("bot_quota", 0);
	auto &ports = static_cast<metamod::BotObservationServices &>(runtime);
	auto &views = static_cast<metamod::BotBehaviorServices &>(runtime);
	std::array<world::ActorKey, 2> previousContacts{};
	bool passed = true, sawDifferentCounters = false;
	for (unsigned cycle = 0; cycle < 3; ++cycle)
	{
		runtime.executeCompatibilityCommand(request("bot_kick", 1U, "all"));
		resetEntities();
		gCreateCount = 0;
		if (!check(runtime.executeCompatibilityCommand(request("bot_add_t", 1U, "T Bot")) ==
						metamod::CompatibilityCommandResult::Handled &&
					runtime.executeCompatibilityCommand(request("bot_add_ct", 1U, "CT Bot")) ==
						metamod::CompatibilityCommandResult::Handled,
				"opposing managed bots can reuse their slots"))
			return false;
		identityShots = {};
		for (unsigned i = 0; i < 2; ++i)
		{
			auto &e = gEntities[i];
			e.v.flags = FL_CLIENT | FL_FAKECLIENT | FL_ONGROUND;
			e.v.team = static_cast<int>(i + 1);
			e.v.health = 100;
			e.v.deadflag = DEAD_NO;
			e.v.solid = SOLID_SLIDEBOX;
			e.v.movetype = MOVETYPE_WALK;
			e.v.origin = Vector(i ? 280.0f : 80.0f, 32, 36);
			e.v.view_ofs = Vector(0, 0, 17);
			e.v.v_angle = Vector(0, i ? 180.0f : 0.0f, 0);
			e.v.angles = e.v.v_angle; // Initialize both engine pose fields used by aim and vision.
			e.v.maxspeed = 240;
			e.v.fov = 90;
			e.v.weapons = 1 << 22;
			runtime.onMessageBegin(2, 86, nullptr, nullptr);
			runtime.onWriteByte(static_cast<int>(i + 1));
			runtime.onWriteString(i ? "CT" : "TERRORIST");
			runtime.onMessageEnd();
			const auto &handle = ports.service_managedBotHandles()[i];
			const auto token = ports.service_lifecycle().tokenForSlot(i + 1);
			sawDifferentCounters = sawDifferentCounters || handle.actor.actorGeneration != token.slotGeneration;
		}
		const auto tick = [&](float dt)
		{
			for (unsigned i = 0; i < 2; ++i)
			{
				runtime.onMessageBegin(1, 94, nullptr, &gEntities[i]);
				runtime.onWriteByte(1);
				runtime.onWriteByte(22);
				runtime.onWriteByte(30);
				runtime.onMessageEnd();
			}
			globals.time += dt;
			runtime.onStartFrame();
			runtime.onStartFramePost();
		};
		for (unsigned frame = 0; frame < 64; ++frame)
			tick(.125f);
		for (unsigned i = 0; i < 2; ++i)
		{
			const auto &handle = ports.service_managedBotHandles()[i];
			const world::ActorKey actor = {handle.actor.slot, handle.actor.actorGeneration};
			const auto &enemy = ports.service_managedBotHandles()[1 - i];
			const auto targetToken = ports.service_lifecycle().tokenForSlot(enemy.actor.slot);
			const world::ActorKey enemyContact = {enemy.actor.slot, targetToken.slotGeneration};
			const auto &belief = views.observation_managedBotPerception(i).snapshot();
			const world::ActorObservation *self = nullptr, *target = nullptr;
			const bool selfFound =
				belief.findActor(actor, &self) == world::ContactLookupResult::Found && self && self->visible;
			if (!selfFound || belief.findActor(enemyContact, &target) != world::ContactLookupResult::Found)
			{
				std::fprintf(stderr, "cycle=%u slot=%u managed=%u snapshotObserver=%u:%u shots=%u\n", cycle, actor.slot,
					actor.generation, belief.identity().observer.slot, belief.identity().observer.generation,
					identityShots[i]);
				for (std::size_t n = 0; n < belief.actorCount(); ++n)
				{
					const auto *a = belief.actorAt(n);
					std::fprintf(stderr, "actor=%u:%u visible=%d relation=%d\n", a->actor.slot, a->actor.generation,
						a->visible ? 1 : 0, static_cast<int>(a->relation));
				}
			}
			passed = check(belief.identity().observer == actor && selfFound,
						 "snapshot observer and self observation share the managed actor generation") &&
				passed;
			passed = check(belief.findActor(enemyContact, &target) == world::ContactLookupResult::Found && target,
						 "enemy contacts retain their connection generation for safe target tracking") &&
				passed;
			passed =
				check(identityShots[i] > 0, "both reused managed bots recognize and fire on their enemy") && passed;
			if (cycle && previousContacts[i].isValid())
				passed = check(belief.findActor(previousContacts[i], &target) != world::ContactLookupResult::Found,
							 "old target generation never survives removal and re-add") &&
					passed;
			previousContacts[i] = enemyContact;
		}
		bool upkeepMoved = false;
		for (unsigned step = 0; step < 40; ++step)
		{
			const auto before = ports.service_managedBotFullUpdateSequences()[0];
			const float yaw = identityYaw[0];
			gEntities[1].v.origin.y += 4;
			tick(1.0f / 30);
			if (before == ports.service_managedBotFullUpdateSequences()[0] && std::fabs(identityYaw[0] - yaw) > .001f)
				upkeepMoved = true;
		}
		passed =
			check(upkeepMoved, "upkeep retains and tracks the reused enemy actor between full decisions") && passed;
		if (cycle == 0)
		{
			const auto observerBefore = ports.service_managedBotHandles()[0].actor;
			runtime.executeCompatibilityCommand(request("bot_kick", 1U, "CT Bot"));
			gEntities[1].v.flags = FL_CLIENT;
			gEntities[1].v.health = 100;
			gEntities[1].v.deadflag = DEAD_NO;
			gEntities[1].v.team = 2;
			gEntities[1].v.origin = Vector(280, 32, 36);
			gEntities[0].v.v_angle = Vector(0, 0, 0);
			runtime.onClientPutInServer(&gEntities[1]);
			runtime.onMessageBegin(2, 86, nullptr, nullptr);
			runtime.onWriteByte(2);
			runtime.onWriteString("CT");
			runtime.onMessageEnd();
			for (unsigned n = 0; n < 16; ++n)
				tick(.125f);
			const world::ActorKey oldContact = {2, ports.service_lifecycle().tokenForSlot(2).slotGeneration};
			runtime.onClientDisconnect(&gEntities[1]);
			gEntities[1] = {};
			gCreateCount = 1;
			passed = check(runtime.executeCompatibilityCommand(request("bot_add_ct", 1U, "CT Bot")) ==
							 metamod::CompatibilityCommandResult::Handled,
						 "target slot can change from unmanaged client to managed bot") &&
				passed;
			auto &enemy = gEntities[1];
			enemy.v.flags = FL_CLIENT | FL_FAKECLIENT | FL_ONGROUND;
			enemy.v.health = 100;
			enemy.v.deadflag = DEAD_NO;
			enemy.v.team = 2;
			enemy.v.origin = Vector(280, 32, 36);
			enemy.v.view_ofs = Vector(0, 0, 17);
			enemy.v.v_angle = Vector(0, 180, 0);
			enemy.v.angles = enemy.v.v_angle;
			enemy.v.solid = SOLID_SLIDEBOX;
			enemy.v.movetype = MOVETYPE_WALK;
			enemy.v.maxspeed = 240;
			enemy.v.fov = 90;
			enemy.v.weapons = 1 << 22;
			gEntities[0].v.v_angle = Vector(0, 0, 0);
			identityShots = {};
			for (unsigned n = 0; n < 48; ++n)
				tick(.125f);
			const auto &belief = views.observation_managedBotPerception(0).snapshot();
			const world::ActorObservation *target = nullptr;
			passed =
				check(ports.service_managedBotHandles()[0].actor.slot == observerBefore.slot &&
						ports.service_managedBotHandles()[0].actor.actorGeneration == observerBefore.actorGeneration,
					"observer stays alive while only the target slot is replaced") &&
				passed;
			passed = check(ports.service_lifecycle().tokenForSlot(2).slotGeneration != oldContact.generation &&
							 belief.findActor(oldContact, &target) != world::ContactLookupResult::Found,
						 "old unmanaged target key cannot become the new bot key") &&
				passed;
			passed = check(identityShots[0] > 0 && identityShots[1] > 0,
						 "continuing observer and replacement bot recognize each other anew") &&
				passed;
		}
	}
	return check(sawDifferentCounters, "fixture reproduces distinct actor and connection generations") && passed;
}
}
