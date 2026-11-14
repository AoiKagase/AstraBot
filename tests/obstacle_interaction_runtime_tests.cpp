#include "plugin/plugin_runtime.hpp"
#include "astrabot/compat/obstacle_interaction.hpp"
namespace
{
bool testObstacleInteraction(astrabot::metamod::PluginRuntime &, enginefuncs_t &, globalvars_t &);
}
#define ASTRABOT_EXTRA_ACTOR_TEST testObstacleInteraction
#include "compat_actor_command_tests.cpp"
namespace
{
unsigned obstacleSwitchRequests = 0;
unsigned obstaclePistolRequests = 0;
void obstacleClientCommand(edict_t *self)
{
	if (self == &gEntities[0] && std::strcmp(astrabot::metamod::HookCommandArgv(0), "slot1") == 0)
		++obstacleSwitchRequests;
	if (self == &gEntities[0] && std::strcmp(astrabot::metamod::HookCommandArgv(0), "slot2") == 0)
		++obstaclePistolRequests;
	hookedGameClientCommand(self);
}

int equipmentMessageIdForObstacle(plid_t id, const char *name, int *size)
{
	if (std::strcmp(name, "WeaponList") == 0)
		return 101;
	if (std::strcmp(name, "AmmoX") == 0)
		return 102;
	return siteFixtureUserMessageId(id, name, size);
}

const char *obstacleString(int index)
{
	return index == 20 ? "func_breakable"
		: index == 21  ? "func_door"
		: index == 22  ? "func_door_rotating"
					   : szFromIndex(index);
}

bool testObstacleInteraction(astrabot::metamod::PluginRuntime &runtime, enginefuncs_t &engine, globalvars_t &globals)
{
	using astrabot::compat::breakableBlocksPath;
	if (!check(!breakableBlocksPath({0, 0, 36}, {100, 0, 36}, {16, 8, 0}, {40, 32, 80}) &&
				breakableBlocksPath({0, 0, 36}, {100, 0, 36}, {16, 0, 0}, {40, 32, 80}),
			"local path rejects a contacted box on the same outside side, including boundary contact"))
		return false;
	if (!check(breakableBlocksPath({0, 0, 0}, {10, 10, 0}, {8, 0, -1}, {10, 2, 1}),
			"preserve the reference conservative box decision even when the diagonal ray misses"))
		return false;
	DLL_FUNCTIONS hooks{};
	int version = INTERFACE_VERSION;
	if (!check(runtime.provideEntityApi(&hooks, &version) && hooks.pfnKeyValue && hooks.pfnTouch,
			"public KeyValue and Touch callbacks support breakable material provenance"))
		return false;
	engine.pfnSzFromIndex = &obstacleString;
	engine.pfnTraceLine = &traceObjectiveVisibility;
	runtime.giveEnginePointers(&engine, &globals);
	gObjectiveEntitiesAvailable = false;
	gObjectiveTraceVisible = false;
	gEntities[1].v.deadflag = DEAD_DEAD;
	gEntities[1].v.health = 0;
	auto &self = gEntities[0];
	self.v.flags = FL_CLIENT | FL_FAKECLIENT | FL_ONGROUND;
	self.v.health = 100;
	self.v.deadflag = DEAD_NO;
	self.v.origin = Vector(0, 0, 36);
	self.v.view_ofs = Vector(0, 0, 28);
	self.v.v_angle = Vector(0, 0, 0);
	self.v.maxspeed = 240;
	self.v.weapons = (1 << 22) | (1 << 29);
	runtime.onMessageBegin(1, 94, nullptr, &self);
	runtime.onWriteByte(1);
	runtime.onWriteByte(22);
	runtime.onWriteByte(30);
	runtime.onMessageEnd();
	auto &obstacle = gEntities[4];
	obstacle = {};
	obstacle.serialnumber = 42;
	obstacle.v.classname = 20;
	obstacle.v.takedamage = DAMAGE_YES;
	obstacle.v.health = 50;
	obstacle.v.solid = SOLID_BSP;
	obstacle.v.absmin = Vector(16, 8, 0);
	obstacle.v.absmax = Vector(40, 32, 80);
	const auto material = [&](const char *value)
	{
		KeyValueData data{};
		data.szClassName = const_cast<char *>("func_breakable");
		data.szKeyName = const_cast<char *>("material");
		data.szValue = const_cast<char *>(value);
		hooks.pfnKeyValue(&obstacle, &data);
		return data.fHandled == 0;
	};
	const auto step = [&]()
	{
		globals.time += .04f;
		runtime.onStartFrame();
		runtime.onStartFramePost();
	};
	if (!check(material("1"), "material observation never consumes the GameDLL KeyValue"))
		return false;
	hooks.pfnTouch(&self, &obstacle);
	step();
	if (!check((gLastRunPlayerMoveButtons & IN_ATTACK) && self.v.v_angle.y > 0,
			"contacted damageable wood is aimed at and attacked through RunPlayerMove"))
		return false;
	obstacle.free = TRUE;
	step();
	if (!check((gLastRunPlayerMoveButtons & IN_ATTACK) == 0,
			"destroyed obstacle cannot keep attacking from cached contact"))
		return false;
	obstacle.free = FALSE;
	if (!check(material("7"), "unbreakable material is observed publicly"))
		return false;
	hooks.pfnTouch(&obstacle, &self);
	step();
	if (!check((gLastRunPlayerMoveButtons & IN_ATTACK) == 0, "unbreakable glass is never shot"))
		return false;
	material("1");
	obstacle.v.takedamage = DAMAGE_NO;
	hooks.pfnTouch(&self, &obstacle);
	step();
	if (!check((gLastRunPlayerMoveButtons & IN_ATTACK) == 0, "trigger-only breakable is not attacked"))
		return false;
	obstacle.v.takedamage = DAMAGE_YES;
	++obstacle.serialnumber;
	hooks.pfnTouch(&self, &obstacle);
	step();
	if (!check((gLastRunPlayerMoveButtons & IN_ATTACK) == 0, "reused edict cannot inherit material provenance"))
		return false;
	for (const int classname : {21, 22})
	{
		obstacle.v.classname = classname;
		hooks.pfnTouch(&self, &obstacle);
		step();
		astrabot::runtime::BotCommand doorCommand{};
		doorCommand.movement = {120.0f, 15.0f, 0.0f, IN_FORWARD, 0, 33};
		static_cast<astrabot::metamod::BotCommandServices &>(runtime).behavior_applyObstacleCommand(0, doorCommand);
		if (!check(doorCommand.movement.forward == 120.0f && doorCommand.movement.side == 15.0f,
				"touch-opening door preserves the supplied walking command"))
			return false;
		if (!check((gLastRunPlayerMoveButtons & (IN_ATTACK | IN_USE)) == 0,
				"touch-opening doors receive neither attack nor invented button-use commands"))
			return false;
	}
	obstacle.v.classname = 20;
	material("1");
	hooks.pfnTouch(&obstacle, &self);
	step();
	if (!check((gLastRunPlayerMoveButtons & IN_ATTACK) != 0, "reverse Touch callback order reaches the managed bot"))
		return false;
	// Repeated grenade contacts may not reset the reference five-second equip throttle.
	gHookedGameDllTable.pfnClientCommand = &obstacleClientCommand;
	self.v.weapons |= (1 << 4);
	runtime.onMessageBegin(1, 94, nullptr, &self);
	runtime.onWriteByte(1);
	runtime.onWriteByte(4);
	runtime.onWriteByte(1);
	runtime.onMessageEnd();
	for (unsigned i = 0; i < 32; ++i)
	{
		hooks.pfnTouch(&self, &obstacle);
		step();
		if (!check((gLastRunPlayerMoveButtons & (IN_ATTACK | IN_ATTACK2)) == 0,
				"contact never throws the still-active grenade"))
			return false;
	}
	if (!check(obstacleSwitchRequests == 1, "look expiry cannot bypass the five-second weapon equip throttle"))
		return false;
	runtime.onMessageBegin(1, 94, nullptr, &self);
	runtime.onWriteByte(1);
	runtime.onWriteByte(22);
	runtime.onWriteByte(30);
	runtime.onMessageEnd();
	hooks.pfnTouch(&self, &obstacle);
	step();
	if (!check((gLastRunPlayerMoveButtons & IN_ATTACK) != 0,
			"primary receipt permits contact attack after grenade switch"))
		return false;
	material("7");
	step();
	if (!check((gLastRunPlayerMoveButtons & IN_ATTACK) == 0, "changed material invalidates an already active look"))
		return false;
	material("1");
	// Public receipts distinguish a fully empty primary from a usable pistol.
	gpMetaUtilFuncs->pfnGetUserMsgID = &equipmentMessageIdForObstacle;
	for (const int id : {22, 17})
	{
		runtime.onMessageBegin(1, 101, nullptr, &self);
		runtime.onWriteString(id == 22 ? "weapon_m4a1" : "weapon_glock18");
		for (int value : {id == 22 ? 3 : 10, 90, -1, -1, id == 22 ? 0 : 1, 0, id, 0})
			runtime.onWriteByte(value);
		runtime.onMessageEnd();
		runtime.onMessageBegin(1, 94, nullptr, &self);
		runtime.onWriteByte(1);
		runtime.onWriteByte(id);
		runtime.onWriteByte(id == 22 ? 0 : 20);
		runtime.onMessageEnd();
	}
	runtime.onMessageBegin(1, 102, nullptr, &self);
	runtime.onWriteByte(3);
	runtime.onWriteByte(0);
	runtime.onMessageEnd();
	self.v.weapons |= (1 << 17);
	runtime.onMessageBegin(1, 94, nullptr, &self);
	runtime.onWriteByte(1);
	runtime.onWriteByte(4);
	runtime.onWriteByte(1);
	runtime.onMessageEnd();
	globals.time += 6.0f;
	hooks.pfnTouch(&self, &obstacle);
	step();
	if (!check(obstaclePistolRequests == 1, "grenade fallback skips a publicly known empty primary for usable pistol"))
		return false;
	// Enemy attack owns the look even if a breakable contact is already pending.
	auto &behaviorServices = static_cast<astrabot::metamod::BotBehaviorServices &>(runtime);
	auto &machine = const_cast<astrabot::compat::CompatibilityStateMachine &>(
		static_cast<astrabot::metamod::BotCommandServices &>(runtime).behavior_managedBotStateMachines(0));
	const auto &handle = behaviorServices.service_managedBotHandles()[0];
	astrabot::compat::StateUpdateContext attackContext{};
	attackContext.actor = {handle.actor.slot, handle.actor.actorGeneration};
	attackContext.frame = {behaviorServices.service_lifecycle().mapGeneration(),
		behaviorServices.service_lifecycle().roundGeneration(), behaviorServices.service_adapterFrameCount()};
	attackContext.timestamp = globals.time;
	attackContext.attackObservation = astrabot::compat::ObservationAvailability::Available;
	if (!check(machine.beginAttack(attackContext) == astrabot::compat::StateResult::Transitioned,
			"enemy attack overlay is active for contact precedence scenario"))
		return false;
	hooks.pfnTouch(&self, &obstacle);
	astrabot::runtime::BotCommand enemyCommand{};
	enemyCommand.viewAngles = {-10, -90, 0};
	enemyCommand.movement.buttons = IN_ATTACK;
	static_cast<astrabot::metamod::BotCommandServices &>(runtime).behavior_applyObstacleCommand(0, enemyCommand);
	if (!check(enemyCommand.viewAngles.pitch == -10 && enemyCommand.viewAngles.yaw == -90 &&
				enemyCommand.movement.buttons == IN_ATTACK && !enemyCommand.synchronizeCombatView,
			"enemy attack keeps its aim and fire without a breakable override"))
		return false;
	(void)machine.stopAttack(attackContext);
	self.v.flags |= FL_FROZEN;
	hooks.pfnTouch(&self, &obstacle);
	step();
	if (!check(gLastRunPlayerMoveButtons == 0, "freeze gate dominates obstacle attack"))
		return false;
	self.v.flags &= ~FL_FROZEN;
	obstacle.free = TRUE;
	step();
	obstacle.free = FALSE;
	runtime.onMessageBegin(1, 94, nullptr, &self);
	runtime.onWriteByte(1);
	runtime.onWriteByte(22);
	runtime.onWriteByte(30);
	runtime.onMessageEnd();
	hooks.pfnTouch(&self, &obstacle);
	astrabot::runtime::BotCommand walking{};
	walking.viewAngles = {0, 90, 0};
	walking.movement = {120, 15, 0, IN_FORWARD, 0, 33};
	auto &commandServices = static_cast<astrabot::metamod::BotCommandServices &>(runtime);
	commandServices.behavior_applyObstacleCommand(0, walking);
	const float radians = walking.viewAngles.yaw * 0.017453292519943295f;
	const float worldX = walking.movement.forward * std::cos(radians) + walking.movement.side * std::sin(radians);
	const float worldY = walking.movement.forward * std::sin(radians) - walking.movement.side * std::cos(radians);
	if (!check(std::fabs(worldX - 15) < .001f && std::fabs(worldY - 120) < .001f,
			"looking at a breakable preserves movement in world space"))
		return false;
	++obstacle.serialnumber;
	astrabot::runtime::BotCommand reused{};
	commandServices.behavior_applyObstacleCommand(0, reused);
	if (!check(reused.movement.buttons == 0 && !reused.synchronizeCombatView,
			"reused target invalidates an already pending attack and look"))
		return false;
	material("1");
	hooks.pfnTouch(&self, &obstacle);
	runtime.onRoundLifecycleMessage("World triggered \"Round_End\"\n");
	runtime.onRoundLifecycleMessage("World triggered \"Round_Start\"\n");
	step();
	return check((gLastRunPlayerMoveButtons & IN_ATTACK) == 0, "round reset invalidates pending contact attack");
}
}
