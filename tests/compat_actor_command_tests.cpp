#include "astrabot/metamod/abi_contract.hpp"
#include "plugin/plugin_runtime.hpp"
#include "runtime_characterization.hpp"

#include <entity_state.h>
#include <weaponinfo.h>
#include <cstdio>
#include <cerrno>
#include <cstring>
#include <cmath>
#include <vector>
#include <cstdarg>
#include <string>
#include <chrono>
#include <sstream>
#include <cstdlib>

namespace
{
edict_t gEntities[5]{};
std::string gEntityNames[5];
edict_t gBombSite{};
edict_t gSecondBombSite{};
edict_t gPlantedBomb{};
bool gObjectiveEntitiesAvailable = false;
bool gObjectiveTraceVisible = false;
void traceObjectiveVisibility(const float *, const float *end, int, edict_t *, TraceResult *result)
{
	astrabot_test::runtimeCharacterization().record(
		"call traceObjectiveVisibility end=(%a,%a,%a)", end[0], end[1], end[2]);
	*result = {};
	result->flFraction = gObjectiveTraceVisible ? 1.0f : 0.0f;
	result->vecEndPos = Vector(end[0], end[1], end[2]);
}

bool gCaptureMovementLogs = false;
bool gProfileEnabled = false;
bool gInjectProfilerDelay = false;
void injectProfilerDelay()
{
	if (!gInjectProfilerDelay)
		return;
	const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(1);
	while (std::chrono::steady_clock::now() < until)
	{
	}
}

std::vector<std::string> gMovementLogs;
void captureMovementLog(plid_t, const char *format, ...)
{
	if (!gCaptureMovementLogs)
		return;
	injectProfilerDelay();
	char message[4096] = {};
	va_list args;
	va_start(args, format);
#ifdef _WIN32
	vsnprintf_s(message, sizeof(message), _TRUNCATE, format, args);
#else
	std::vsnprintf(message, sizeof(message), format, args);
#endif
	va_end(args);
	if (std::strncmp(message, "profile navigationStep ", 23U) == 0 ||
		std::strncmp(message, "profile currentWeapon ", 22U) == 0 ||
		std::strncmp(message, "profile navigationFailure ", 26U) == 0 ||
		std::strncmp(message, "profile objectiveExit ", 22U) == 0)
	{
		astrabot_test::runtimeCharacterization().record("%s", message);
	}
	gMovementLogs.emplace_back(message);
}

int gCreateCount = 0;
int gKillCount = 0;
int gClientCommandCount = 0;
int gHookedClientCommandCount = 0;
int gUnhookedClientCommandCount = 0;
int gRunPlayerMoveCount = 0;
int gSetClientMaxspeedCount = 0;
float gLastClientMaxspeed = 0.0f;
unsigned short gLastRunPlayerMoveButtons = 0U;
byte gLastRunPlayerMoveMsec = 0U;
float gLastRunPlayerMoveForward = 0.0f;
float gLastRunPlayerMoveSide = 0.0f;
float gLastRunPlayerMoveUp = 0.0f;
float gLastRunPlayerMoveYaw = 0.0f;
int gLastRunPlayerMoveActor = 0;
float gAvoidanceSide[5] = {};
float gAvoidanceForward[5] = {};
bool gAvoidanceNeutralPath = false;
bool gDirectJoinCommandSeen = false;
bool gServerSetsEntityTeam = true;
char gLastClientCommand[32] = {};
int gClientKeyValueCount = 0;
bool gMenuTeamContextValid = false;
bool gMenuClassContextValid = false;
DLL_FUNCTIONS gHookedGameDllTable{};
bool gNativeControlsAvailable = false;
bool gCompatibilityCvarsRegistered[7] = {};
bool gPerformanceCvarsRegistered[3] = {};
int gCvarRegisterCount = 0;
float gBotEnable = 0.0f;
float gBotQuota = 0.0f;
float gMpFreezetime = 6.0f;
float gMpRoundtime = 5.0f;
float gTestC4Timer = 0.0f;
float gMpRoundRestartDelay = 5.0f;
cvar_t gBotEnableCvar{};
cvar_t gBotStopCvar{};
cvar_t gBotDifficultyCvar{};
cvar_t gBotQuotaCvar{};
cvar_t gBotJoinTeamCvar{};
cvar_t gAstrabotModeCvar{};
cvar_t gAstrabotProfileCvar{};
cvar_t gMpFreezetimeCvar{};
cvar_t gMpRoundtimeCvar{};
cvar_t gMpRoundRestartDelayCvar{};
cvar_t gAstrabotPerfCvars[3]{};
const char *const gManagerCvarNames[] = {
	"bot_quota_mode", "bot_join_after_player", "bot_auto_vacate", "bot_join_delay", "bot_allow_rogues",
	"bot_defer_to_human", "bot_excellent_morale"};
constexpr std::size_t kManagerCvarCount = sizeof(gManagerCvarNames) / sizeof(gManagerCvarNames[0]);
cvar_t *gManagerCvars[kManagerCvarCount]{};
const char *const gWeaponPresetCvarNames[] = {"bot_allow_pistols", "bot_allow_shotguns", "bot_allow_sub_machine_guns",
	"bot_allow_rifles", "bot_allow_machine_guns", "bot_allow_grenades", "bot_allow_snipers", "bot_allow_shield"};
constexpr std::size_t kWeaponPresetCvarCount = sizeof(gWeaponPresetCvarNames) / sizeof(gWeaponPresetCvarNames[0]);
cvar_t *gWeaponPresetCvars[kWeaponPresetCvarCount]{};

bool check(bool condition, const char *description)
{
	if (condition)
	{
		return true;
	}

	std::fprintf(stderr, "check failed: %s\n", description);
	return false;
}

std::FILE *openFixture(const char *path, const char *mode)
{
	std::FILE *file = nullptr;
#ifdef _WIN32
	if (fopen_s(&file, path, mode) != 0)
	{
		return nullptr;
	}
#else
	file = std::fopen(path, mode);
#endif
	return file;
}

class ExistingProfileGuard
{
public:
	bool capture(const char *path)
	{
		path_ = path;
		errno = 0;
		std::FILE *file = openFixture(path, "rb");
		if (file == nullptr)
			return errno == ENOENT;
		char buffer[4096];
		std::size_t count = 0U;
		while ((count = std::fread(buffer, 1U, sizeof(buffer), file)) != 0U)
			contents_.insert(contents_.end(), buffer, buffer + count);
		const bool readOk = std::ferror(file) == 0;
		const bool closeOk = std::fclose(file) == 0;
		restore_ = readOk && closeOk;
		return restore_;
	}

	~ExistingProfileGuard()
	{
		if (!restore_)
			return;
		std::FILE *file = openFixture(path_, "wb");
		if (file == nullptr)
			std::abort();
		const bool written =
			contents_.empty() || std::fwrite(contents_.data(), 1U, contents_.size(), file) == contents_.size();
		const bool closed = std::fclose(file) == 0;
		if (!written || !closed)
			std::abort();
	}

private:
	const char *path_ = nullptr;
	bool restore_ = false;
	std::vector<char> contents_;
};

edict_t *createFakeClient(const char *name)
{
	astrabot_test::runtimeCharacterization().record("call createFakeClient name=%s", name == nullptr ? "<null>" : name);
	if (gCreateCount >= 5)
	{
		return nullptr;
	}
	const int slotIndex = gCreateCount++;
	edict_t *entity = &gEntities[slotIndex];
	gEntityNames[slotIndex] = name == nullptr ? "" : name;
	entity->v.netname = 100 + slotIndex;
	entity->v.flags |= FL_FAKECLIENT;
	return entity;
}

int indexOfEdict(const edict_t *entity)
{
	astrabot_test::runtimeCharacterization().record("call indexOfEdict");
	if (entity == &gBombSite)
	{
		return 6;
	}
	if (entity == &gPlantedBomb)
	{
		return 7;
	}
	if (entity == &gSecondBombSite)
		return 8;
	for (int index = 0; index < 5; ++index)
	{
		if (entity == &gEntities[index])
		{
			return index + 1;
		}
	}
	return -1;
}

int gFindEntityByStringCount = 0;

edict_t *findEntityByString(edict_t *start, const char *field, const char *value)
{
	astrabot_test::runtimeCharacterization().record("call findEntityByString start=%d field=%s value=%s",
		start != nullptr ? 1 : 0, field == nullptr ? "<null>" : field, value == nullptr ? "<null>" : value);
	++gFindEntityByStringCount;
	if (!gObjectiveEntitiesAvailable || start != nullptr || field == nullptr || value == nullptr ||
		std::strcmp(field, "classname") != 0)
	{
		return nullptr;
	}
	if (std::strcmp(value, "func_bomb_target") == 0)
	{
		return &gBombSite;
	}
	if (std::strcmp(value, "grenade") == 0)
	{
		return &gPlantedBomb;
	}
	return nullptr;
}

const char *szFromIndex(int index)
{
	astrabot_test::runtimeCharacterization().record("call szFromIndex index=%d", index);
	if (index >= 100 && index < 105)
		return gEntityNames[index - 100].c_str();
	return index == 1 ? "func_bomb_target"
		: index == 2  ? "grenade"
		: index == 3  ? "models/w_c4.mdl"
		: index == 4  ? "weaponbox"
		: index == 5  ? "models/w_backpack.mdl"
		: index == 6  ? "de_test"
					  : "";
}

edict_t *entityOfIndex(int index)
{
	astrabot_test::runtimeCharacterization().record("call entityOfIndex index=%d", index);
	if (index >= 1 && index <= 5)
	{
		return &gEntities[index - 1];
	}
	if (index == 6)
	{
		return &gBombSite;
	}
	if (index == 7)
	{
		return &gPlantedBomb;
	}
	if (index == 8)
		return &gSecondBombSite;
	return nullptr;
}

void getGameDir(char *buffer)
{
	astrabot_test::runtimeCharacterization().record("call getGameDir");
	if (buffer != nullptr)
	{
		buffer[0] = '.';
		buffer[1] = '\0';
	}
}

cvar_t *getCvar(const char *name)
{
	astrabot_test::runtimeCharacterization().record("call getCvar name=%s", name);
	for (std::size_t index = 0U; index < kWeaponPresetCvarCount; ++index)
	{
		if (std::strcmp(name, gWeaponPresetCvarNames[index]) == 0)
		{
			return gWeaponPresetCvars[index];
		}
	}
	static cvar_t c4Timer{};
	if (std::strcmp(name, "mp_c4timer") == 0 && gTestC4Timer > 0.0f)
		return &c4Timer;
	for (std::size_t index = 0U; index < kManagerCvarCount; ++index)
		if (std::strcmp(name, gManagerCvarNames[index]) == 0)
			return gManagerCvars[index];
	if (std::strcmp(name, "mp_roundtime") == 0)
		return &gMpRoundtimeCvar;
	if (std::strcmp(name, "mp_freezetime") == 0)
	{
		return &gMpFreezetimeCvar;
	}
	if (std::strcmp(name, "mp_round_restart_delay") == 0)
	{
		return &gMpRoundRestartDelayCvar;
	}
	if (std::strcmp(name, "bot_enable") == 0)
	{
		return gNativeControlsAvailable || gCompatibilityCvarsRegistered[0] ? &gBotEnableCvar : nullptr;
	}
	if (std::strcmp(name, "bot_stop") == 0)
	{
		return gNativeControlsAvailable || gCompatibilityCvarsRegistered[1] ? &gBotStopCvar : nullptr;
	}
	if (std::strcmp(name, "bot_difficulty") == 0)
	{
		return gNativeControlsAvailable || gCompatibilityCvarsRegistered[2] ? &gBotDifficultyCvar : nullptr;
	}
	if (std::strcmp(name, "bot_quota") == 0)
	{
		return gNativeControlsAvailable || gCompatibilityCvarsRegistered[3] ? &gBotQuotaCvar : nullptr;
	}
	if (std::strcmp(name, "bot_join_team") == 0)
	{
		return gNativeControlsAvailable || gCompatibilityCvarsRegistered[4] ? &gBotJoinTeamCvar : nullptr;
	}
	if (std::strcmp(name, "astrabot_mode") == 0)
	{
		return gNativeControlsAvailable || gCompatibilityCvarsRegistered[5] ? &gAstrabotModeCvar : nullptr;
	}
	if (std::strcmp(name, "astrabot_profile") == 0)
	{
		return gNativeControlsAvailable || gCompatibilityCvarsRegistered[6] ? &gAstrabotProfileCvar : nullptr;
	}
	const char *const performanceNames[] = {
		"astrabot_perf_disable_vision", "astrabot_perf_disable_pathsearch", "astrabot_perf_disable_trace"};
	for (std::size_t index = 0U; index < 3U; ++index)
	{
		if (std::strcmp(name, performanceNames[index]) == 0)
		{
			return gNativeControlsAvailable || gPerformanceCvarsRegistered[index] ? &gAstrabotPerfCvars[index]
																				  : nullptr;
		}
	}
	return nullptr;
}

float getCvarFloat(const char *name)
{
	astrabot_test::runtimeCharacterization().record("call getCvarFloat name=%s", name);
	if (std::strcmp(name, "astrabot_perf_disable_pathsearch") == 0)
		return gAvoidanceNeutralPath ? 1.0f : 0.0f;
	if (std::strcmp(name, "mp_c4timer") == 0)
		return gTestC4Timer;
	if (std::strcmp(name, "bot_difficulty") == 0)
		return 1.0f;
	for (std::size_t index = 0U; index < kManagerCvarCount; ++index)
		if (std::strcmp(name, gManagerCvarNames[index]) == 0)
			return gManagerCvars[index] == nullptr ? 0.0f : gManagerCvars[index]->value;
	if (std::strcmp(name, "mp_roundtime") == 0)
		return gMpRoundtime;
	if (std::strcmp(name, "mp_freezetime") == 0)
	{
		return gMpFreezetime;
	}
	if (std::strcmp(name, "mp_round_restart_delay") == 0)
	{
		return gMpRoundRestartDelay;
	}
	if (std::strcmp(name, "bot_enable") == 0)
	{
		return gBotEnable;
	}
	if (std::strcmp(name, "bot_quota") == 0)
	{
		return gBotQuota;
	}
	if (std::strcmp(name, "astrabot_profile") == 0)
	{
		return gProfileEnabled ? 1.0f : 0.0f;
	}
	return 0.0f;
}

void setCvarFloat(const char *name, float value)
{
	astrabot_test::runtimeCharacterization().record("call setCvarFloat name=%s value=%a", name, value);
	for (std::size_t index = 0U; index < kWeaponPresetCvarCount; ++index)
	{
		if (std::strcmp(name, gWeaponPresetCvarNames[index]) == 0)
		{
			if (gWeaponPresetCvars[index] != nullptr)
			{
				gWeaponPresetCvars[index]->value = value;
			}
			return;
		}
	}
	for (std::size_t index = 0U; index < kManagerCvarCount; ++index)
		if (std::strcmp(name, gManagerCvarNames[index]) == 0 && gManagerCvars[index] != nullptr)
			gManagerCvars[index]->value = value;
	if (std::strcmp(name, "bot_enable") == 0)
	{
		gBotEnable = value;
	}
	else if (std::strcmp(name, "bot_quota") == 0)
	{
		gBotQuota = value;
	}
}

void registerCvar(cvar_t *variable)
{
	astrabot_test::runtimeCharacterization().record("call registerCvar");
	++gCvarRegisterCount;
	if (variable == nullptr || variable->name == nullptr)
	{
		return;
	}
	for (std::size_t index = 0U; index < kWeaponPresetCvarCount; ++index)
	{
		if (std::strcmp(variable->name, gWeaponPresetCvarNames[index]) == 0)
		{
			gWeaponPresetCvars[index] = variable;
			variable->value = std::strtof(variable->string, nullptr);
			return;
		}
	}
	for (std::size_t index = 0U; index < kManagerCvarCount; ++index)
	{
		if (std::strcmp(variable->name, gManagerCvarNames[index]) == 0)
		{
			gManagerCvars[index] = variable;
			variable->value = variable->string == nullptr ? 0.0f : std::strtof(variable->string, nullptr);
			return;
		}
	}
	const char *const names[] = {"bot_enable", "bot_stop", "bot_difficulty", "bot_quota", "bot_join_team",
		"astrabot_mode", "astrabot_profile", "astrabot_perf_disable_vision", "astrabot_perf_disable_pathsearch",
		"astrabot_perf_disable_trace"};
	for (std::size_t index = 0U; index < 10U; ++index)
	{
		if (std::strcmp(variable->name, names[index]) == 0)
		{
			if (index < 7U)
				gCompatibilityCvarsRegistered[index] = true;
			else
				gPerformanceCvarsRegistered[index - 7U] = true;
			return;
		}
	}
}

int playerUserId(edict_t *entity)
{
	astrabot_test::runtimeCharacterization().record("call playerUserId");
	return indexOfEdict(entity) + 100;
}

void putInServer(edict_t *)
{
	astrabot_test::runtimeCharacterization().record("call putInServer");
}

qboolean clientConnect(edict_t *, const char *, const char *, char[128])
{
	astrabot_test::runtimeCharacterization().record("call clientConnect");
	return true;
}

void disconnect(edict_t *entity)
{
	astrabot_test::runtimeCharacterization().record("call disconnect");
	if (entity != nullptr)
	{
		entity->v.flags &= ~FL_FAKECLIENT;
	}
}

void serverCommand(char *)
{
	astrabot_test::runtimeCharacterization().record("call serverCommand");
}

void serverExecute()
{
	astrabot_test::runtimeCharacterization().record("call serverExecute");
}

void resetEntities()
{
	for (std::string &name : gEntityNames)
		name.clear();
	for (edict_t &entity : gEntities)
	{
		entity = edict_t{};
	}
}

void clientCommand(edict_t *, char *command, ...)
{
	++gClientCommandCount;
	if (command != nullptr)
	{
		const std::size_t length = std::strlen(command);
		const std::size_t copyLength =
			length < sizeof(gLastClientCommand) - 1U ? length : sizeof(gLastClientCommand) - 1U;
		std::memcpy(gLastClientCommand, command, copyLength);
		gLastClientCommand[copyLength] = '\0';
	}
}

void runPlayerMove(
	edict_t *entity, const float *angles, float forward, float side, float up, unsigned short buttons, byte, byte msec)
{
	astrabot_test::runtimeCharacterization().record("move actor=%d view=(%a,%a,%a) input=(%a,%a,%a) buttons=%u msec=%u",
		indexOfEdict(entity), static_cast<double>(angles[0]), static_cast<double>(angles[1]),
		static_cast<double>(angles[2]), static_cast<double>(forward), static_cast<double>(side),
		static_cast<double>(up), static_cast<unsigned>(buttons), static_cast<unsigned>(msec));
	injectProfilerDelay();
	++gRunPlayerMoveCount;
	gLastRunPlayerMoveButtons = buttons;
	gLastRunPlayerMoveMsec = msec;
	gLastRunPlayerMoveForward = forward;
	gLastRunPlayerMoveSide = side;
	gLastRunPlayerMoveUp = up;
	gLastRunPlayerMoveYaw = angles[1];
	gLastRunPlayerMoveActor = indexOfEdict(entity);
	if (gLastRunPlayerMoveActor >= 1 && gLastRunPlayerMoveActor <= 5)
	{
		gAvoidanceSide[gLastRunPlayerMoveActor - 1] = side;
		gAvoidanceForward[gLastRunPlayerMoveActor - 1] = forward;
	}
}

void setClientMaxspeed(const edict_t *entity, float maxspeed)
{
	astrabot_test::runtimeCharacterization().record("call setClientMaxspeed speed=%a", maxspeed);
	++gSetClientMaxspeedCount;
	gLastClientMaxspeed = maxspeed;
	if (entity != nullptr)
	{
		const_cast<edict_t *>(entity)->v.maxspeed = maxspeed;
	}
}

char *getInfoKeyBuffer(edict_t *)
{
	static char buffer[128] = {};
	return buffer;
}

void setClientKeyValue(int, char *, char *, char *)
{
	++gClientKeyValueCount;
}

void gameClientCommand(edict_t *entity)
{
	astrabot_test::runtimeCharacterization().record("client actor=%d argc=%d command=%s arg=%s", indexOfEdict(entity),
		astrabot::metamod::HookCommandArgc(), astrabot::metamod::HookCommandArgv(0),
		astrabot::metamod::HookCommandArgv(1));
	++gClientCommandCount;
	if (astrabot::metamod::HookCommandArgc() == 2)
	{
		const char *const command = astrabot::metamod::HookCommandArgv(0);
		const char *const selection = astrabot::metamod::HookCommandArgv(1);
		if (std::strcmp(command, "jointeam") == 0)
		{
			gDirectJoinCommandSeen = true;
			if (entity != nullptr && gServerSetsEntityTeam)
			{
				entity->v.team = 1;
			}
			astrabot::metamod::PluginRuntime &runtime = astrabot::metamod::PluginRuntime::instance();
			runtime.onMessageBegin(0, 86, nullptr, nullptr);
			runtime.onWriteByte(indexOfEdict(entity));
			runtime.onWriteString("TERRORIST");
			runtime.onMessageEnd();
		}
		else if (std::strcmp(command, "joinclass") == 0)
		{
			gDirectJoinCommandSeen = true;
		}
		else if (std::strcmp(command, "menuselect") == 0)
		{
			if (std::strcmp(selection, "1") == 0 || std::strcmp(selection, "2") == 0 ||
				std::strcmp(selection, "5") == 0)
			{
				gMenuTeamContextValid = gMenuTeamContextValid || gClientCommandCount == 1;
				gMenuClassContextValid = gMenuClassContextValid || gClientCommandCount >= 2;
			}
		}
	}
}

void unhookedGameClientCommand(edict_t *)
{
	++gUnhookedClientCommandCount;
}

void hookedGameClientCommand(edict_t *entity)
{
	++gHookedClientCommandCount;
	gameClientCommand(entity);
}

void getHookTables(
	plid_t, enginefuncs_t **engineFunctions, DLL_FUNCTIONS **gameDllFunctions, NEW_DLL_FUNCTIONS **newDllFunctions)
{
	if (engineFunctions != nullptr)
	{
		*engineFunctions = nullptr;
	}
	if (gameDllFunctions != nullptr)
	{
		*gameDllFunctions = &gHookedGameDllTable;
	}
	if (newDllFunctions != nullptr)
	{
		*newDllFunctions = nullptr;
	}
}

void clientKill(edict_t *)
{
	++gKillCount;
}

astrabot::compat::CommandRequest request(
	const char *name, std::size_t argumentCount = 0U, const char *firstArgument = nullptr)
{
	return {name, argumentCount, {firstArgument, nullptr}};
}

int32 jumpFixtureRandomLong(int32 low, int32 high)
{
	return astrabot_test::recordRandomLong(low, high, high);
}

int32 recoveryFixtureRandomLong(int32 low, int32 high)
{
	return astrabot_test::recordRandomLong(low, high, low);
}

float jumpFixtureRandomFloat(float low, float high)
{
	return astrabot_test::recordRandomFloat(low, high, low);
}

int gGroundProbeMode = 0;
int gFarGroundProbeCount = 0;
int gNearGroundProbeCount = 0;

void traceJumpGround(const float *start, const float *end, int, edict_t *actor, TraceResult *trace)
{
	astrabot_test::runtimeCharacterization().record(
		"call traceJumpGround start=(%a,%a,%a)", start[0], start[1], start[2]);
	*trace = {};
	trace->flFraction = 0.5f;
	trace->vecEndPos[0] = start[0];
	trace->vecEndPos[1] = start[1];
	trace->vecEndPos[2] = 0.0f;
	trace->vecPlaneNormal[2] = 1.0f;
	if (actor == nullptr)
	{
		// Hunt also probes the world floor at the endpoint and checks its path rays.
		if (start[0] != end[0] || start[1] != end[1] || start[2] - end[2] < 100.0f)
			trace->flFraction = 1.0f;
		return;
	}
	const float distance = std::fabs(start[0] - actor->v.origin.x);
	if (distance > 70.0f)
	{
		++gFarGroundProbeCount;
		if (gGroundProbeMode == 0)
		{
			trace->flFraction = 1.0f;
		}
		else
		{
			trace->vecPlaneNormal[2] = 0.8f;
		}
	}
	else if (distance > 20.0f)
	{
		++gNearGroundProbeCount;
		trace->vecEndPos[2] = 32.0f;
	}
}

bool writeJumpNavigation(const char *path, bool recovery = false)
{
	std::vector<std::uint8_t> bytes;
	const auto appendU32 = [&](std::uint32_t value)
	{
		for (unsigned int shift = 0U; shift < 32U; shift += 8U)
		{
			bytes.push_back(static_cast<std::uint8_t>(value >> shift));
		}
	};
	const auto appendFloat = [&](float value)
	{
		std::uint32_t bits = 0U;
		std::memcpy(&bits, &value, sizeof(bits));
		appendU32(bits);
	};
	appendU32(0xFEEDFACEU);
	appendU32(1U);
	appendU32(2U);
	for (std::uint32_t id = 1U; id <= 2U; ++id)
	{
		appendU32(id);
		bytes.push_back(0U);
		const float lowX = static_cast<float>(id - 1U) * 64.0f;
		const float coordinates[] = {lowX, 0.0f, 0.0f, lowX + 64.0f, 64.0f, 0.0f, 0.0f, 0.0f};
		for (float value : coordinates)
		{
			appendFloat(value);
		}
		for (unsigned int direction = 0U; direction < 4U; ++direction)
		{
			const unsigned int linkDirection = recovery ? (id == 1U ? 1U : 3U) : 0U;
			appendU32(direction == linkDirection ? 1U : 0U);
			if (direction == linkDirection)
			{
				appendU32(3U - id);
			}
		}
		bytes.push_back(0U);
		bytes.push_back(0U);
		appendU32(0U);
	}
	std::FILE *file = openFixture(path, "wb");
	if (file == nullptr)
	{
		return false;
	}
	const auto written = std::fwrite(bytes.data(), 1U, bytes.size(), file);
	const int closed = std::fclose(file);
	return written == bytes.size() && closed == 0;
}

bool gAvoidanceGroundAvailable = true;
void traceAvoidanceFloor(const float *, const float *end, int, edict_t *, TraceResult *result)
{
	astrabot_test::runtimeCharacterization().record("call traceAvoidanceFloor end=(%a,%a,%a)", end[0], end[1], end[2]);
	*result = {};
	result->flFraction = gAvoidanceGroundAvailable ? 0.5f : 1.0f;
	result->vecEndPos = Vector(end[0], end[1], 0.0f);
}

bool testRuntimeFriendBeyondPortal(
	astrabot::metamod::PluginRuntime &runtime, enginefuncs_t &engine, globalvars_t &globals)
{
	const char *path = "astrabot_friend_portal.nav";
	if (!writeJumpNavigation(path, true))
		return false;
	const astrabot::metamod::NavLoadRequest load = {
		path, "astrabot_friend_portal", runtime.snapshot().mapGeneration, false, 0U, false, 0U};
	const auto loaded = runtime.loadNavigationFile(&load);
	std::remove(path);
	if (loaded != astrabot::metamod::NavLoadResult::Loaded)
		return false;
	runtime.executeCompatibilityCommand(request("bot_kick", 1U, "all"));
	resetEntities();
	gCreateCount = 0;
	gObjectiveEntitiesAvailable = false;
	gServerSetsEntityTeam = true;
	const auto savedTrace = engine.pfnTraceLine;
	engine.pfnTraceLine = nullptr;
	runtime.giveEnginePointers(&engine, &globals);
	runtime.onRoundLifecycleMessage("World triggered \"Round_Start\"\n");
	runtime.setCompatibilityFloat("bot_enable", 1.0f);
	runtime.setCompatibilityFloat("bot_quota", 0.0f);
	if (runtime.executeCompatibilityCommand(request("bot_add_t")) !=
		astrabot::metamod::CompatibilityCommandResult::Handled)
		return false;
	for (int i = 0; i < 2; ++i)
	{
		auto &v = gEntities[i].v;
		v.flags = FL_CLIENT | FL_ONGROUND | (i == 0 ? FL_FAKECLIENT : 0);
		v.team = 1;
		v.health = 100.0f;
		v.deadflag = DEAD_NO;
		v.solid = SOLID_SLIDEBOX;
		v.movetype = MOVETYPE_WALK;
		v.origin = Vector(i == 0 ? 16.0f : 104.0f, 32.0f, 36.0f);
		v.mins = Vector(-16.0f, -16.0f, -36.0f);
		v.maxs = Vector(16.0f, 16.0f, 36.0f);
		v.maxspeed = 240.0f;
	}
	globals.time += gMpFreezetime + 1.0f;
	for (int i = 0; i < 20; ++i)
	{
		globals.time += 0.1f;
		runtime.onStartFrame();
		runtime.onStartFramePost();
	}
	bool moving = false;
	for (int i = 0; i < 3; ++i)
	{
		globals.time += 0.1f;
		runtime.onStartFrame();
		runtime.onStartFramePost();
		moving = moving || std::hypot(gLastRunPlayerMoveForward, gLastRunPlayerMoveSide) > 0.0f;
	}
	const bool passed = check(
		moving, "friend beyond a short floor-level portal does not create a false polite wait using feet coordinates");
	runtime.executeCompatibilityCommand(request("bot_kick", 1U, "all"));
	gEntities[1].v.flags = 0;
	engine.pfnTraceLine = savedTrace;
	runtime.giveEnginePointers(&engine, &globals);
	return passed;
}

bool testRuntimeTouchAvoidance(astrabot::metamod::PluginRuntime &runtime, enginefuncs_t &engine, globalvars_t &globals)
{
	const char *path = "astrabot_touch_avoidance.nav";
	if (!writeJumpNavigation(path))
		return false;
	const astrabot::metamod::NavLoadRequest load = {
		path, "astrabot_touch_avoidance", runtime.snapshot().mapGeneration, false, 0U, false, 0U};
	const auto loaded = runtime.loadNavigationFile(&load);
	std::remove(path);
	if (!check(loaded == astrabot::metamod::NavLoadResult::Loaded, "touch NAV loaded"))
		return false;
	runtime.executeCompatibilityCommand(request("bot_kick", 1U, "all"));
	resetEntities();
	gCreateCount = 0;
	gObjectiveEntitiesAvailable = false;
	gServerSetsEntityTeam = true;
	gCaptureMovementLogs = true;
	gProfileEnabled = true;
	const auto savedTrace = engine.pfnTraceLine;
	engine.pfnTraceLine = &traceAvoidanceFloor;
	gAvoidanceNeutralPath = true; // Make-way must also run while path movement waits.
	runtime.giveEnginePointers(&engine, &globals);
	runtime.onRoundLifecycleMessage("World triggered \"Round_Start\"\n");
	runtime.setCompatibilityFloat("bot_enable", 1.0f);
	runtime.setCompatibilityFloat("bot_quota", 0.0f);
	for (int i = 0; i < 2; ++i)
	{
		if (runtime.executeCompatibilityCommand(request("bot_add_t")) !=
			astrabot::metamod::CompatibilityCommandResult::Handled)
			return false;
		auto &v = gEntities[i].v;
		v.flags = FL_CLIENT | FL_FAKECLIENT | FL_ONGROUND;
		v.team = 1;
		v.health = 100.0f;
		v.deadflag = DEAD_NO;
		v.solid = SOLID_SLIDEBOX;
		v.movetype = MOVETYPE_WALK;
		v.origin = Vector(16.0f + i * 32.0f, 32.0f, 36.0f);
		v.mins = Vector(-16.0f, -16.0f, -36.0f);
		v.maxs = Vector(16.0f, 16.0f, 36.0f);
		v.maxspeed = 240.0f;
	}
	// The later-created bot outranks the earlier one only because it carries C4.
	gEntities[1].v.weapons = 1 << astrabot::metamod::ActionAdapter::kC4WeaponId;
	globals.time += gMpFreezetime + 1.0f;
	for (int i = 0; i < 20; ++i)
	{
		globals.time += 0.05f;
		runtime.onStartFrame();
		runtime.onStartFramePost();
	}
	DLL_FUNCTIONS hooks{};
	int version = INTERFACE_VERSION;
	if (!runtime.provideEntityApi(&hooks, &version) || hooks.pfnTouch == nullptr)
		return false;
	gAvoidanceGroundAvailable = true;
	gMovementLogs.clear();
	bool moved = false;
	for (int i = 0; i < 24; ++i)
	{
		hooks.pfnTouch(&gEntities[0], &gEntities[1]);
		globals.time += 0.025f;
		runtime.onStartFrame();
		runtime.onStartFramePost();
		moved = moved || (gAvoidanceForward[0] < -200.0f && gAvoidanceSide[0] == 0.0f);
	}
	// Diagnostic frame%16 need not intersect this actor's full-update phase.
	// Assert actual public-engine commands instead of sampled log presence.
	bool passed = check(
		moved, "pinned REGAMEDLL_FIXES make-way retreats from a player directly ahead even with neutral path input");
	gAvoidanceGroundAvailable = false;
	gMovementLogs.clear();
	bool vetoed = false;
	for (int i = 0; i < 24; ++i)
	{
		hooks.pfnTouch(&gEntities[0], &gEntities[1]);
		globals.time += 0.025f;
		runtime.onStartFrame();
		runtime.onStartFramePost();
		vetoed = vetoed || (gAvoidanceSide[0] == 0.0f && gAvoidanceForward[0] == 0.0f);
	}
	passed = check(vetoed, "touch avoidance does not sidestep off an unobserved ledge") && passed;
	runtime.executeCompatibilityCommand(request("bot_kick", 1U, "all"));
	engine.pfnTraceLine = savedTrace;
	runtime.giveEnginePointers(&engine, &globals);
	gAvoidanceNeutralPath = false;
	gAvoidanceGroundAvailable = true;
	gCaptureMovementLogs = false;
	gProfileEnabled = false;
	return passed;
}

bool testRuntimeNearJumpFallback(
	astrabot::metamod::PluginRuntime &runtime, enginefuncs_t &engine, globalvars_t &globals)
{
	const char *path = "astrabot_actor_jump.nav";
	if (!check(writeJumpNavigation(path, true), "near-jump NAV fixture written with an eastbound link"))
	{
		return false;
	}
	const astrabot::metamod::NavLoadRequest load = {
		path, "astrabot_actor_jump", runtime.snapshot().mapGeneration, false, 0U, false, 0U};
	const auto loaded = runtime.loadNavigationFile(&load);
	std::remove(path);
	if (!check(loaded == astrabot::metamod::NavLoadResult::Loaded, "near-jump NAV fixture loaded"))
	{
		return false;
	}
	engine.pfnTraceLine = &traceJumpGround;
	engine.pfnRandomLong = &jumpFixtureRandomLong;
	engine.pfnRandomFloat = &jumpFixtureRandomFloat;
	runtime.giveEnginePointers(&engine, &globals);
	runtime.setCompatibilityFloat("bot_enable", 1.0f);
	runtime.setCompatibilityFloat("bot_quota", 0.0f);
	gObjectiveEntitiesAvailable = false;
	gServerSetsEntityTeam = true;
	gCaptureMovementLogs = true;
	gProfileEnabled = true;
	bool passed = true;
	for (int mode = 0; mode < 12; ++mode)
	{
		runtime.executeCompatibilityCommand(request("bot_kick", 1U, "all"));
		resetEntities();
		gCreateCount = 0;
		gGroundProbeMode = mode;
		gFarGroundProbeCount = 0;
		gNearGroundProbeCount = 0;
		runtime.setCompatibilityFloat("bot_quota", 0.0f);
		if (!check(runtime.executeCompatibilityCommand(request("bot_add_t")) ==
					astrabot::metamod::CompatibilityCommandResult::Handled,
				"near-jump actor added"))
		{
			return false;
		}
		gEntities[0].v.flags = FL_CLIENT | FL_FAKECLIENT | FL_ONGROUND;
		gEntities[0].v.team = 1;
		gEntities[0].v.deadflag = DEAD_NO;
		gEntities[0].v.health = 100.0f;
		gEntities[0].v.solid = SOLID_SLIDEBOX;
		gEntities[0].v.movetype = MOVETYPE_WALK;
		gEntities[0].v.origin = Vector(16.0f, 32.0f, 36.0f);
		gEntities[0].v.mins = Vector(-16.0f, -16.0f, -36.0f);
		gEntities[0].v.maxs = Vector(16.0f, 16.0f, 36.0f);
		gEntities[0].v.maxspeed = 240.0f;
		gEntities[0].v.flags &= ~FL_ONGROUND;
		const int movesBeforeSpawn = gRunPlayerMoveCount;
		for (int frame = 0; frame < 8; ++frame)
		{
			const int movesBefore = gRunPlayerMoveCount;
			globals.time += 0.1f;
			runtime.onStartFrame();
			runtime.onStartFramePost();
			if (gRunPlayerMoveCount == movesBefore)
				continue;
			passed = check((gLastRunPlayerMoveButtons & static_cast<unsigned short>(IN_JUMP)) == 0U &&
							 gLastRunPlayerMoveForward == 0.0f && gLastRunPlayerMoveSide == 0.0f,
						 "initial airborne spawn keeps movement neutral until grounded") &&
				passed;
		}
		passed =
			check(gRunPlayerMoveCount > movesBeforeSpawn, "initial airborne spawn receives join heartbeats") && passed;
		gEntities[0].v.flags |= FL_ONGROUND;
		gMovementLogs.clear();
		bool jumped = false;
		for (int frame = 0; frame < 300 && !jumped; ++frame)
		{
			gLastRunPlayerMoveButtons = 0U;
			const float frameSteps[] = {0.1f, 1.0f / 60.0f, 1.0f / 90.0f, 0.01f, 1.0f / 128.0f};
			const int cadence = mode / 2;
			const float variableSteps[] = {1.0f / 128.0f, 1.0f / 60.0f, 0.01f, 1.0f / 90.0f, 0.035f};
			globals.time += cadence < 5 ? frameSteps[cadence] : variableSteps[frame % 5];
			runtime.onStartFrame();
			runtime.onStartFramePost();
			jumped = (gLastRunPlayerMoveButtons & static_cast<unsigned short>(IN_JUMP)) != 0U;
		}
		if (!jumped)
		{
			std::fprintf(stderr, "near-jump mode=%d far=%d near=%d buttons=%u moves=%d readiness=%d\n", mode,
				gFarGroundProbeCount, gNearGroundProbeCount, gLastRunPlayerMoveButtons, gRunPlayerMoveCount,
				static_cast<int>(runtime.movementPhysicsSample(1U).readiness));
		}
		passed = check(gFarGroundProbeCount > 0 && gNearGroundProbeCount > 0 && jumped,
					 mode == 0 ? "runtime dispatches near jump with unknown far ground"
							   : "runtime dispatches near jump with sloped far ground") &&
			passed;
		passed =
			check(gCreateCount == 1, "single-actor movement fixture does not trigger extra quota population") && passed;
		const float takeoffYaw = gLastRunPlayerMoveYaw;
		const int probesAtTakeoff = gFarGroundProbeCount + gNearGroundProbeCount;
		gEntities[0].v.flags &= ~FL_ONGROUND;
		gEntities[0].v.origin.z = 52.0f;
		// Keep this short NAV fixture outside its final arrival tolerance while
		// checking flight continuity. Reaching the goal correctly stops movement.
		gEntities[0].v.velocity = Vector(20.0f, 0.0f, 120.0f);
		bool airborneMoved = false;
		float airborneElapsed = 0.0f;
		const float airborneGravity = 800.0f; // This fixture uses the default sv_gravity.
		for (int frame = 0; airborneElapsed < 0.4f; ++frame)
		{
			const int movesBefore = gRunPlayerMoveCount;
			const float frameSteps[] = {0.1f, 1.0f / 60.0f, 1.0f / 90.0f, 0.01f, 1.0f / 128.0f};
			const float variableSteps[] = {1.0f / 128.0f, 1.0f / 60.0f, 0.01f, 1.0f / 90.0f, 0.035f};
			const int cadence = mode / 2;
			const float delta = cadence < 5 ? frameSteps[cadence] : variableSteps[frame % 5];
			globals.time += delta;
			airborneElapsed += delta;
			gEntities[0].v.origin.x += gEntities[0].v.velocity.x * delta;
			gEntities[0].v.origin.y += gEntities[0].v.velocity.y * delta;
			gEntities[0].v.origin.z += gEntities[0].v.velocity.z * delta - 0.5f * airborneGravity * delta * delta;
			gEntities[0].v.velocity.z -= airborneGravity * delta;
			runtime.onStartFrame();
			runtime.onStartFramePost();
			if (gRunPlayerMoveCount == movesBefore)
				continue;
			airborneMoved = true;
			if (!(std::hypot(gLastRunPlayerMoveForward, gLastRunPlayerMoveSide) > 0.0f &&
					std::fabs(gLastRunPlayerMoveYaw - takeoffYaw) < 0.01f))
			{
				std::fprintf(stderr,
					"airborne mode=%d frame=%d elapsed=%g actor=%d forward=%g side=%g yaw=%g takeoffYaw=%g buttons=%u msec=%u origin=(%g,%g,%g) velocity=(%g,%g,%g)\n",
					mode, frame, airborneElapsed, gLastRunPlayerMoveActor, gLastRunPlayerMoveForward,
					gLastRunPlayerMoveSide, gLastRunPlayerMoveYaw, takeoffYaw, gLastRunPlayerMoveButtons,
					static_cast<unsigned int>(gLastRunPlayerMoveMsec), gEntities[0].v.origin.x, gEntities[0].v.origin.y,
					gEntities[0].v.origin.z, gEntities[0].v.velocity.x, gEntities[0].v.velocity.y,
					gEntities[0].v.velocity.z);
				const std::size_t begin = gMovementLogs.size() > 4U ? gMovementLogs.size() - 4U : 0U;
				for (std::size_t entry = begin; entry < gMovementLogs.size(); ++entry)
					std::fprintf(stderr, "airborne log: %s\n", gMovementLogs[entry].c_str());
			}
			passed = check(std::hypot(gLastRunPlayerMoveForward, gLastRunPlayerMoveSide) > 0.0f &&
							 std::fabs(gLastRunPlayerMoveYaw - takeoffYaw) < 0.01f,
						 "public airborne RunPlayerMove preserves routed direction across full updates") &&
				passed;
			passed = check((gLastRunPlayerMoveButtons & static_cast<unsigned short>(IN_JUMP)) == 0U,
						 "public airborne commands never repeat the takeoff jump") &&
				passed;
		}
		passed = check(airborneMoved && gFarGroundProbeCount + gNearGroundProbeCount == probesAtTakeoff,
					 "airborne route continues without ground lookahead probes") &&
			passed;
		gEntities[0].v.flags |= FL_ONGROUND;
		gEntities[0].v.origin.z = 36.0f;
		gEntities[0].v.velocity.z = 0.0f;
		gEntities[0].v.health = 90.0f;
		globals.time += 0.1f;
		runtime.onStartFrame();
		runtime.onStartFramePost();
		passed = check(std::hypot(gLastRunPlayerMoveForward, gLastRunPlayerMoveSide) > 0.0f,
					 "landing resumes routed movement without another spawn warmup") &&
			passed;
		bool measuredLanding = false;
		for (const auto &message : gMovementLogs)
		{
			if (message.find("profile landingFeedback ") != std::string::npos &&
				message.find("landing_damage_available=1 landing_damage=10.0") != std::string::npos)
			{
				measuredLanding = true;
			}
		}
		passed =
			check(measuredLanding, "public runtime retains airborne health and measures damage on landing") && passed;
		runtime.onRoundLifecycleMessage("World triggered \"Round_End\"\n");
		const float freezeStart = globals.time + gMpRoundRestartDelay;
		const int probesBeforeFreeze = gFarGroundProbeCount + gNearGroundProbeCount;
		gRunPlayerMoveCount = 0;
		gEntities[0].v.velocity = Vector(100.0f, -80.0f, 4.0f);
		for (int frame = 0; frame < 3; ++frame)
		{
			globals.time = freezeStart + static_cast<float>(frame) * 0.1f;
			runtime.onStartFrame();
			runtime.onStartFramePost();
		}
		passed = check(runtime.snapshot().roundFreezeActive && gRunPlayerMoveCount > 0 &&
						 gLastRunPlayerMoveButtons == 0U && gLastRunPlayerMoveMsec > 0U &&
						 gLastRunPlayerMoveForward == 0.0f && gLastRunPlayerMoveSide == 0.0f &&
						 gLastRunPlayerMoveUp == 0.0f && gEntities[0].v.velocity.x == 0.0f &&
						 gEntities[0].v.velocity.y == 0.0f && gEntities[0].v.velocity.z == 4.0f &&
						 gFarGroundProbeCount + gNearGroundProbeCount == probesBeforeFreeze,
					 "scheduled FreezeTime clears movement and skips probes while preserving timing and Z velocity") &&
			passed;
		runtime.onRoundLifecycleMessage("World triggered \"Round_Start\"\n");
		bool resumed = false;
		for (int frame = 0; frame < 6 && !resumed; ++frame)
		{
			globals.time += 0.1f;
			runtime.onStartFrame();
			runtime.onStartFramePost();
			resumed = gLastRunPlayerMoveMsec > 0U &&
				(std::fabs(gLastRunPlayerMoveForward) > 0.0f || std::fabs(gLastRunPlayerMoveSide) > 0.0f);
		}
		passed = check(!runtime.snapshot().roundFreezeActive && resumed,
					 "Round_Start resumes public movement immediately rather than adding another freeze") &&
			passed;
	}
	runtime.executeCompatibilityCommand(request("bot_kick", 1U, "all"));
	gCaptureMovementLogs = false;
	gProfileEnabled = false;
	engine.pfnTraceLine = nullptr;
	return passed;
}

bool testRuntimeRecoveryReselection(
	astrabot::metamod::PluginRuntime &runtime, enginefuncs_t &engine, globalvars_t &globals)
{
	const char *path = "astrabot_actor_recovery.nav";
	if (!writeJumpNavigation(path, true))
		return false;
	const astrabot::metamod::NavLoadRequest load = {
		path, "astrabot_actor_recovery", runtime.snapshot().mapGeneration, false, 0U, false, 0U};
	const auto loaded = runtime.loadNavigationFile(&load);
	std::remove(path);
	if (!check(loaded == astrabot::metamod::NavLoadResult::Loaded, "recovery NAV loaded"))
		return false;
	runtime.executeCompatibilityCommand(request("bot_kick", 1U, "all"));
	resetEntities();
	gCreateCount = 0;
	gObjectiveEntitiesAvailable = false;
	gServerSetsEntityTeam = true;
	engine.pfnTraceLine = nullptr;
	engine.pfnRandomLong = &recoveryFixtureRandomLong;
	runtime.giveEnginePointers(&engine, &globals);
	runtime.setCompatibilityFloat("bot_quota", 0.0f);
	runtime.onRoundLifecycleMessage("World triggered \"Round_Start\"\n");
	if (runtime.executeCompatibilityCommand(request("bot_add_t")) !=
		astrabot::metamod::CompatibilityCommandResult::Handled)
		return false;
	gEntities[0].v.flags = FL_CLIENT | FL_FAKECLIENT | FL_ONGROUND;
	gEntities[0].v.team = 1;
	gEntities[0].v.deadflag = DEAD_NO;
	gEntities[0].v.health = 100.0f;
	gEntities[0].v.solid = SOLID_SLIDEBOX;
	gEntities[0].v.movetype = MOVETYPE_WALK;
	gEntities[0].v.origin = Vector(32.0f, -8.0f, 76.0f);
	gEntities[0].v.mins = Vector(-16.0f, -16.0f, -36.0f);
	gEntities[0].v.maxs = Vector(16.0f, 16.0f, 36.0f);
	gEntities[0].v.maxspeed = 240.0f;
	bool first = false;
	bool alternate = false;
	bool continuedAfterFailures = false;
	for (int frame = 0; frame < 100; ++frame)
	{
		globals.time += 0.12f;
		runtime.onStartFrame();
		runtime.onStartFramePost();
		if (frame >= 80 && (gLastRunPlayerMoveForward != 0.0f || gLastRunPlayerMoveSide != 0.0f))
			continuedAfterFailures = true;
		if (gLastRunPlayerMoveForward > 200.0f)
		{
			first = first || std::fabs(gLastRunPlayerMoveYaw - 90.0f) < 1.0f;
			alternate = alternate || (first && gLastRunPlayerMoveYaw > 0.0f && gLastRunPlayerMoveYaw < 40.0f);
		}
	}
	bool passed = check(first && alternate,
		"public RunPlayerMove changes recovery direction to the alternate NAV candidate after failure");
	passed = check(continuedAfterFailures,
				 "public compatibility recovery keeps issuing movement after candidate failures") &&
		passed;
	gEntities[0].v.origin = Vector(96.0f, 32.0f, 36.0f);
	bool resumed = false;
	for (int frame = 0; frame < 6; ++frame)
	{
		globals.time += 0.12f;
		runtime.onStartFrame();
		runtime.onStartFramePost();
		resumed = resumed || gLastRunPlayerMoveForward > 0.0f || gLastRunPlayerMoveSide != 0.0f;
	}
	passed = check(resumed, "public NAV re-entry resumes routed movement") && passed;
	runtime.executeCompatibilityCommand(request("bot_kick", 1U, "all"));
	engine.pfnRandomLong = &jumpFixtureRandomLong;
	return passed;
}

bool testRuntimeProfilerBoundaries(
	astrabot::metamod::PluginRuntime &runtime, enginefuncs_t &engine, globalvars_t &globals)
{
	const char *path = "astrabot_profiler_boundaries.nav";
	if (!writeJumpNavigation(path, true))
		return false;
	const astrabot::metamod::NavLoadRequest load = {
		path, "astrabot_profiler_boundaries", runtime.snapshot().mapGeneration, false, 0U, false, 0U};
	const auto loaded = runtime.loadNavigationFile(&load);
	std::remove(path);
	if (loaded != astrabot::metamod::NavLoadResult::Loaded)
		return false;
	runtime.executeCompatibilityCommand(request("bot_kick", 1U, "all"));
	resetEntities();
	gCreateCount = 0;
	gObjectiveEntitiesAvailable = false;
	gServerSetsEntityTeam = true;
	engine.pfnTraceLine = nullptr;
	runtime.giveEnginePointers(&engine, &globals);
	runtime.setCompatibilityFloat("bot_quota", 0.0f);
	runtime.onRoundLifecycleMessage("World triggered \"Round_Start\"\n");
	if (runtime.executeCompatibilityCommand(request("bot_add_t")) !=
		astrabot::metamod::CompatibilityCommandResult::Handled)
		return false;
	gEntities[0].v.flags = FL_CLIENT | FL_FAKECLIENT | FL_ONGROUND;
	gEntities[0].v.team = 1;
	gEntities[0].v.deadflag = DEAD_NO;
	gEntities[0].v.health = 100.0f;
	gEntities[0].v.solid = SOLID_SLIDEBOX;
	gEntities[0].v.movetype = MOVETYPE_WALK;
	gEntities[0].v.origin = Vector(16.0f, 32.0f, 36.0f);
	gEntities[0].v.mins = Vector(-16.0f, -16.0f, -36.0f);
	gEntities[0].v.maxs = Vector(16.0f, 16.0f, 36.0f);
	gEntities[0].v.maxspeed = 240.0f;
	gMovementLogs.clear();
	gCaptureMovementLogs = true;
	gProfileEnabled = true;
	gInjectProfilerDelay = true;
	for (int frame = 0; frame < 240; ++frame)
	{
		globals.time += 1.0f / 90.0f;
		runtime.onStartFrame();
		runtime.onStartFramePost();
	}
	gInjectProfilerDelay = false;
	unsigned int windows = 0U;
	unsigned int gateLogs = 0U;
	unsigned long long navMax = 0U, moveMax = 0U, logMax = 0U;
	bool windowFields = true;
	for (const auto &message : gMovementLogs)
	{
		if (message.find("profile window ") == 0U)
		{
			double gameSeconds = 0.0, wallSeconds = 0.0;
			unsigned int frames = 0U;
			std::istringstream fields(message.substr(15U));
			std::string gameField, wallField, frameField;
			fields >> gameField >> wallField >> frameField;
			gameSeconds = std::stod(gameField.substr(13U));
			wallSeconds = std::stod(wallField.substr(13U));
			frames = static_cast<unsigned int>(std::stoul(frameField.substr(7U)));
			windowFields = windowFields && gameSeconds >= 1.0 && wallSeconds > 0.0 && frames > 0U;
			++windows;
		}
		if (message.find("profile movementGate ") == 0U)
			++gateLogs;
		if (message.find("profile stage=") == 0U)
		{
			const auto maximumAt = message.find("max_usec=");
			const auto stageEnd = message.find(' ', 14U);
			const std::string stage = message.substr(14U, stageEnd - 14U);
			const auto maximum = std::stoull(message.substr(maximumAt + 9U));
			if (stage == "NavMovement" && maximum > navMax)
				navMax = maximum;
			if (stage == "RunPlayerMove" && maximum > moveMax)
				moveMax = maximum;
			if (stage == "SynchronousLog" && maximum > logMax)
				logMax = maximum;
		}
	}
	bool passed = check(
		windows >= 2U && windowFields, "public profiler serializes actual game/wall window duration and frame count");
	const auto huntStep = [](const std::string &message)
	{
		return message.find("profile navigationStep bot_id=1 ") == 0U &&
			message.find(" state_from=STATE-HUNT ") != std::string::npos;
	};
	const auto firstHunt = std::find_if(gMovementLogs.begin(), gMovementLogs.end(), huntStep);
	passed = check(firstHunt != gMovementLogs.end() && firstHunt->find(" path=0 ") != std::string::npos &&
			std::any_of(firstHunt + 1, gMovementLogs.end(), [&](const std::string &message)
			{ return huntStep(message) && message.find(" path=0 ") == std::string::npos; }),
		"Hunt entry leaves the path empty and plans on a subsequent full update") && passed;
	bool scenarioStage = false, guardStage = false, costStage = false;
	for (const auto &message : gMovementLogs)
	{
		scenarioStage = scenarioStage || message.find("profile stage=ScenarioUpdate ") == 0U;
		guardStage = guardStage || message.find("profile stage=BombGuardSelection ") == 0U;
		costStage = costStage || message.find("profile stage=PathCostContext ") == 0U;
	}
	passed = check(scenarioStage && guardStage && costStage,
				 "postplant state, guard search, and path-cost preparation have separate profiler boundaries") &&
		passed;
	passed = check(moveMax >= 500U && logMax >= 500U && navMax < moveMax && navMax < logMax,
				 "slow engine/log callbacks are measured separately from core NAV") &&
		passed;
	passed = check(gateLogs <= windows + 2U, "movement-gate diagnostic is limited to one record per bot per second") &&
		passed;
	gMovementLogs.clear();
	gCaptureMovementLogs = true;
	gProfileEnabled = false;
	for (int frame = 0; frame < 100; ++frame)
	{
		globals.time += 1.0f / 90.0f;
		runtime.onStartFrame();
		runtime.onStartFramePost();
	}
	bool hasProfileLog = false;
	for (const auto &message : gMovementLogs)
		hasProfileLog =
			hasProfileLog || (message.find("profile ") == 0U && message.find("profile capture enabled=0 ") != 0U);
	passed = check(!hasProfileLog,
				 "disabled profiler emits only the capture-stop boundary through an active capture callback") &&
		passed;
	gCaptureMovementLogs = false;
	gProfileEnabled = false;
	runtime.executeCompatibilityCommand(request("bot_kick", 1U, "all"));
	return passed;
}

std::uint32_t gSiteFixtureSeed = 1U;
std::uint32_t nextSiteFixtureRandom()
{
	gSiteFixtureSeed = gSiteFixtureSeed * 1664525U + 1013904223U;
	return gSiteFixtureSeed;
}

int32 siteFixtureRandomLong(int32 low, int32 high)
{
	return astrabot_test::recordRandomLong(
		low, high, low + static_cast<int32>(nextSiteFixtureRandom() % (static_cast<std::uint32_t>(high - low) + 1U)));
}

float siteFixtureRandomFloat(float low, float high)
{
	return astrabot_test::recordRandomFloat(
		low, high, low + static_cast<float>(nextSiteFixtureRandom() >> 8U) / 16777216.0f * (high - low));
}

int siteFixtureUserMessageId(plid_t, const char *name, int *)
{
	astrabot_test::runtimeCharacterization().record("call siteFixtureUserMessageId name=%s", name);
	return std::strcmp(name, "CurWeapon") == 0 ? 94
		: std::strcmp(name, "TeamInfo") == 0   ? 86
		: std::strcmp(name, "BombDrop") == 0   ? 95
		: std::strcmp(name, "BombPickup") == 0 ? 97
		: std::strcmp(name, "TextMsg") == 0	   ? 98
											   : 0;
}

bool writeTwoSiteNavigation(const char *path, bool rightReachable, bool withCover = false, const char *place = nullptr)
{
	std::vector<std::uint8_t> bytes;
	const auto u32 = [&](std::uint32_t value)
	{
		for (unsigned int shift = 0U; shift < 32U; shift += 8U)
			bytes.push_back(static_cast<std::uint8_t>(value >> shift));
	};
	const auto f32 = [&](float value)
	{
		std::uint32_t bits = 0U;
		std::memcpy(&bits, &value, sizeof(bits));
		u32(bits);
	};
	u32(0xFEEDFACEU);
	u32(place ? 5U : 1U);
	if (place)
	{
		const auto length = std::strlen(place) + 1U;
		u32(123U); // Synthetic BSP size matched by the fixture load request.
		bytes.push_back(1U);
		bytes.push_back(0U);
		bytes.push_back(static_cast<std::uint8_t>(length));
		bytes.push_back(static_cast<std::uint8_t>(length >> 8U));
		bytes.insert(bytes.end(), place, place + length);
	}
	u32(4U);
	for (std::uint32_t id = 1U; id <= 4U; ++id)
	{
		u32(id);
		bytes.push_back(0U);
		const float low = static_cast<float>(id - 1U) * 64.0f;
		for (float value : {low, 0.0f, 0.0f, low + 64.0f, 64.0f, 0.0f, 0.0f, 0.0f})
			f32(value);
		for (unsigned int direction = 0U; direction < 4U; ++direction)
		{
			const bool east = direction == 1U && id < 4U && (rightReachable || id != 3U);
			const bool west = direction == 3U && id > 1U && (rightReachable || id != 4U);
			u32(east || west ? 1U : 0U);
			if (east || west)
				u32(east ? id + 1U : id - 1U);
		}
		const bool cover = withCover && (id == 1U || id == 4U);
		bytes.push_back(cover ? 1U : 0U);
		if (cover)
		{
			if (place)
				u32(id);
			f32(low + 32.0f);
			f32(50.0f);
			f32(0.0f);
			if (place)
				bytes.push_back(1U);
		}
		bytes.push_back(0U);
		u32(0U);
		if (place)
		{
			bytes.push_back(id == 4 ? 1U : 0U);
			bytes.push_back(0U);
		}
	}
	std::FILE *file = openFixture(path, "wb");
	if (file == nullptr)
		return false;
	const auto written = std::fwrite(bytes.data(), 1U, bytes.size(), file);
	const int closed = std::fclose(file);
	return written == bytes.size() && closed == 0;
}

std::uint32_t latestSelectedBombSite()
{
	for (auto message = gMovementLogs.rbegin(); message != gMovementLogs.rend(); ++message)
	{
		if (message->find("profile goalAssignmentCorrelation ") != 0U)
			continue;
		const auto at = message->find("selected_site_id=");
		if (at != std::string::npos)
			return static_cast<std::uint32_t>(std::stoul(message->substr(at + 17U)));
	}
	return 0U;
}

bool testRuntimeBombSiteApproach(
	astrabot::metamod::PluginRuntime &runtime, enginefuncs_t &engine, globalvars_t &globals, DLL_FUNCTIONS &gameDll)
{
	const char *path = "astrabot_two_site_approach.nav";
	const auto loadNavigation = [&](bool rightReachable, bool withCover = false)
	{
		if (!writeTwoSiteNavigation(path, rightReachable, withCover))
			return false;
		const astrabot::metamod::NavLoadRequest load = {
			path, "astrabot_two_site_approach", runtime.snapshot().mapGeneration, false, 0U, false, 0U};
		const auto result = runtime.loadNavigationFile(&load);
		std::remove(path);
		return result == astrabot::metamod::NavLoadResult::Loaded;
	};
	if (!check(loadNavigation(true), "two reachable unequal-cost bomb sites NAV fixture loaded"))
		return false;
	runtime.executeCompatibilityCommand(request("bot_kick", 1U, "all"));
	resetEntities();
	gCreateCount = 0;
	gBombSite = {};
	gSecondBombSite = {};
	gPlantedBomb = {};
	gBombSite.v.classname = gSecondBombSite.v.classname = 1;
	gBombSite.v.absmin = Vector(24.0f, 24.0f, -4.0f);
	gBombSite.v.absmax = Vector(40.0f, 40.0f, 4.0f);
	gSecondBombSite.v.absmin = Vector(216.0f, 24.0f, -4.0f);
	gSecondBombSite.v.absmax = Vector(232.0f, 40.0f, 4.0f);
	gBombSite.v.origin = Vector(32.0f, 32.0f, 0.0f);
	gSecondBombSite.v.origin = Vector(224.0f, 32.0f, 0.0f);
	gObjectiveEntitiesAvailable = true;
	gServerSetsEntityTeam = true;
	globals.maxEntities = 8;
	engine.pfnRandomLong = &siteFixtureRandomLong;
	engine.pfnRandomFloat = &siteFixtureRandomFloat;
	engine.pfnTraceLine = nullptr;
	runtime.giveEnginePointers(&engine, &globals);
	runtime.setCompatibilityFloat("bot_enable", 1.0f);
	runtime.setCompatibilityFloat("bot_quota", 0.0f);
	if (runtime.executeCompatibilityCommand(request("bot_add_t")) !=
		astrabot::metamod::CompatibilityCommandResult::Handled)
		return false;
	gEntities[0].v.flags = FL_CLIENT | FL_FAKECLIENT | FL_ONGROUND;
	gEntities[0].v.team = 1;
	gEntities[0].v.health = 100.0f;
	gEntities[0].v.solid = SOLID_SLIDEBOX;
	gEntities[0].v.movetype = MOVETYPE_WALK;
	gEntities[0].v.origin = Vector(80.0f, 32.0f, 36.0f);
	gEntities[0].v.mins = Vector(-16.0f, -16.0f, -36.0f);
	gEntities[0].v.maxs = Vector(16.0f, 16.0f, 36.0f);
	gEntities[0].v.maxspeed = 240.0f;
	gEntities[0].v.weapons = 1 << 6;
	gCaptureMovementLogs = true;
	gProfileEnabled = true;
	const auto tick = [&](float delta)
	{
		// GoldSrc updates absolute collision bounds when the entity moves.
		gEntities[0].v.absmin = gEntities[0].v.origin + gEntities[0].v.mins;
		gEntities[0].v.absmax = gEntities[0].v.origin + gEntities[0].v.maxs;
		globals.time += delta;
		runtime.onStartFrame();
		runtime.onStartFramePost();
	};
	const auto freshRound = [&](std::uint32_t seed, float startX, float roundMinutes)
	{
		gSiteFixtureSeed = seed;
		gMpRoundtime = roundMinutes;
		gEntities[0].v.origin = Vector(startX, 32.0f, 36.0f);
		gEntities[0].v.velocity = Vector(0.0f, 0.0f, 0.0f);
		gEntities[0].v.weapons = 1 << 6;
		gEntities[0].v.team = 1;
		runtime.onRoundLifecycleMessage("World triggered \"Round_End\"\n");
		runtime.onRoundLifecycleMessage("World triggered \"Round_Start\"\n");
		gMovementLogs.clear();
		for (int frame = 0; frame < 12; ++frame)
			tick(0.1f);
		return latestSelectedBombSite();
	};
	bool passed = true;
	freshRound(3U, 80.0f, 5.0f);
	passed = check(latestSelectedBombSite() == 0U, "carrier hunts before shared planting decision time") && passed;
	gMovementLogs.clear();
	tick(31.0f);
	passed = check(latestSelectedBombSite() == 1U,
				 "planting decision chooses closest reachable site after shared deadline") &&
		passed;
	gAvoidanceNeutralPath = true;
	gMovementLogs.clear();
	for (int frame = 0; frame < 8; ++frame)
		tick(0.1f);
	bool deliveredFailure = false;
	for (const auto &message : gMovementLogs)
		deliveredFailure = deliveredFailure ||
			(message.find("profile navigationFailure ") == 0U &&
				message.find("state_from=STATE-MOVE-TO state_to=STATE-IDLE") != std::string::npos);
	passed =
		check(deliveredFailure, "public navigation failure is delivered to the owning MoveTo state before retry") &&
		passed;
	if (!deliveredFailure)
		for (const auto &message : gMovementLogs)
			if (message.find("navigationFailure") != std::string::npos || message.find("profile goalAssignment ") == 0U)
				std::fprintf(stderr, "%s\n", message.c_str());
	gAvoidanceNeutralPath = false;
	freshRound(5U, 32.0f, 5.0f);
	runtime.onMessageBegin(0, 94, nullptr, &gEntities[0]);
	runtime.onWriteByte(1);
	runtime.onWriteByte(6);
	runtime.onWriteByte(1);
	runtime.onMessageEnd();
	bool plantedAtSite = false;
	for (int frame = 0; frame < 6; ++frame)
	{
		tick(0.1f);
		plantedAtSite = plantedAtSite ||
			((gLastRunPlayerMoveButtons & static_cast<unsigned short>(IN_ATTACK | IN_DUCK)) == (IN_ATTACK | IN_DUCK));
	}
	passed = check(plantedAtSite, "reference planting crouches while attacking inside the bombsite") && passed;
	runtime.onMessageBegin(0, 94, nullptr, &gEntities[0]);
	runtime.onWriteByte(0);
	runtime.onWriteByte(4);
	runtime.onWriteByte(0);
	runtime.onMessageEnd();
	tick(0.1f);
	passed = check((gLastRunPlayerMoveButtons & (IN_ATTACK | IN_DUCK)) == (IN_ATTACK | IN_DUCK),
				 "inactive grenade notification does not replace the observed active C4") &&
		passed;
	runtime.onMessageBegin(0, 94, nullptr, &gEntities[0]);
	runtime.onWriteByte(0);
	runtime.onWriteByte(6);
	runtime.onWriteByte(1);
	runtime.onMessageEnd();
	tick(0.1f);
	passed = check((gLastRunPlayerMoveButtons & IN_ATTACK) == 0U,
				 "deactivation of the current C4 still blocks attack until selection is observed") &&
		passed;
	runtime.onMessageBegin(0, 94, nullptr, &gEntities[0]);
	runtime.onWriteByte(1);
	runtime.onWriteByte(6);
	runtime.onWriteByte(1);
	runtime.onMessageEnd();
	// The state may begin at another valid part of a large trigger, farther
	// than the legacy action radius from its selected route destination.
	gBombSite.v.absmin = Vector(-160.0f, -160.0f, -4.0f);
	gBombSite.v.absmax = Vector(224.0f, 224.0f, 4.0f);
	gEntities[0].v.origin = Vector(180.0f, 32.0f, 36.0f);
	bool plantedAwayFromRouteTarget = false;
	for (int frame = 0; frame < 6; ++frame)
	{
		tick(0.1f);
		plantedAwayFromRouteTarget = plantedAwayFromRouteTarget ||
			((gLastRunPlayerMoveButtons & (IN_ATTACK | IN_DUCK)) == (IN_ATTACK | IN_DUCK));
	}
	passed = check(plantedAwayFromRouteTarget,
				 "PlantBomb owns attack anywhere inside a public bombsite, independent of the old route target") &&
		passed;
	if (!astrabot_test::runtimeCharacterization().enabled())
	{
		const auto previousMapname = globals.mapname;
		globals.mapname = 6;
		runtime.onMessageBegin(0, 86, nullptr, nullptr);
		runtime.onWriteByte(1);
		runtime.onWriteString("TERRORIST");
		runtime.onMessageEnd();
		gPlantedBomb.v.classname = 2;
		gPlantedBomb.v.model = 3;
		gPlantedBomb.serialnumber = 1;
		gPlantedBomb.v.origin = gEntities[0].v.origin;
		gPlantedBomb.v.dmgtime = globals.time + 35.0f;
		gEntities[0].v.weapons = 0;
		runtime.onMessageBegin(MSG_ALL, 98, nullptr, nullptr);
		runtime.onWriteByte(4);
		runtime.onWriteString("#Bomb_Planted");
		runtime.onMessageEnd();
		gMovementLogs.clear();
		for (int frame = 0; frame < 12; ++frame)
			tick(0.1f);
		bool guardingAfterPlant = false;
		for (const auto &message : gMovementLogs)
		{
			if (message.find("state=STATE-HIDE task=TASK-GUARD-TICKING-BOMB") != std::string::npos)
				guardingAfterPlant = true;
		}
		passed = check(guardingAfterPlant, "known lost C4 and public planted evidence assign the guard task on Hide entry") &&
			passed;
		gPlantedBomb = {};
		globals.mapname = previousMapname;
	}
	gBombSite.v.absmin = Vector(24.0f, 24.0f, -4.0f);
	gBombSite.v.absmax = Vector(40.0f, 40.0f, 4.0f);
	const auto urgent = freshRound(7U, 80.0f, 0.1f);
	passed = check(urgent == 1U, "urgent carrier falls back to the closest reachable plant site") && passed;
	passed = check(loadNavigation(false), "unreachable alternative site fixture loaded") && passed;
	freshRound(3U, 80.0f, 5.0f);
	gMovementLogs.clear();
	tick(31.0f);
	passed =
		check(latestSelectedBombSite() == 1U, "planting decision excludes disconnected alternative site") && passed;
	passed = check(loadNavigation(true), "both sites restored before planted C4 convergence") && passed;
	freshRound(3U, 80.0f, 5.0f);
	gEntities[0].v.weapons = 0;
	gEntities[0].v.team = 2;
	runtime.onMessageBegin(0, 86, nullptr, nullptr);
	runtime.onWriteByte(1);
	runtime.onWriteString("CT");
	runtime.onMessageEnd();
	gPlantedBomb.v.classname = 2;
	gPlantedBomb.v.model = 3;
	gPlantedBomb.serialnumber = 1;
	gPlantedBomb.v.origin = Vector(224.0f, 32.0f, 36.0f);
	gPlantedBomb.v.dmgtime = globals.time + 35.0f;
	gEntities[1].v.flags = FL_CLIENT | FL_ONGROUND;
	gEntities[1].v.team = 1;
	gEntities[1].v.health = 100.0f;
	gEntities[1].v.origin = gPlantedBomb.v.origin;
	const Vector ctBeforeDiscovery = gEntities[0].v.origin;
	gEntities[0].v.origin = gPlantedBomb.v.origin;
	for (int frame = 0; frame < 6; ++frame)
	{
		tick(0.1f);
		passed = check((gLastRunPlayerMoveButtons & static_cast<unsigned short>(IN_USE)) == 0U,
					 "CT cannot defuse unobserved planted entity even at its hidden position") &&
			passed;
	}
	gEntities[0].v.origin = ctBeforeDiscovery;
	runtime.onEmitSound(&gPlantedBomb, "weapons/c4_beep1.wav", 1.0f, 1.0f);
	gMovementLogs.clear();
	bool movedTowardObservedC4 = false;
	for (int frame = 0; frame < 24; ++frame)
	{
		tick(0.1f);
		// This stub reports no displacement. Its later stuck/Wiggle command
		// is tested separately; allow the sampled recovery/path-retry intervals
		// (up to 1.5 + 0.6 seconds) and verify the routed C4 command here.
		if (gLastRunPlayerMoveForward > 0.0f && std::fabs(gLastRunPlayerMoveYaw) < 0.01f)
			for (const auto &message : gMovementLogs)
				if ((message.find("goal_source=objective") != std::string::npos ||
						message.find("profile navigationStep ") == 0U) &&
					message.find("goal_position=(224.0 32.0 36.0)") != std::string::npos)
					movedTowardObservedC4 = true;
	}
	passed = check(movedTowardObservedC4, "public CT moves toward human-planted C4 after hearing its beep") && passed;
	if (!movedTowardObservedC4)
		for (const auto &message : gMovementLogs)
			if (message.find("goal_") != std::string::npos || message.find("objective sensor") != std::string::npos ||
				message.find("movementGate") != std::string::npos || message.find("traversal") != std::string::npos)
				std::fprintf(stderr, "%s\n", message.c_str());
	// Remembering a beep must not freeze a MoveTo actor at the legacy
	// action adapter's wider use radius, even with an occluded bomb.
	gEntities[0].v.origin = Vector(144.0f, 32.0f, 36.0f);
	engine.pfnTraceLine = &traceObjectiveVisibility;
	gObjectiveTraceVisible = false;
	runtime.giveEnginePointers(&engine, &globals);
	for (int frame = 0; frame < 6; ++frame)
	{
		tick(0.1f);
		passed = check((gLastRunPlayerMoveButtons & static_cast<unsigned short>(IN_USE)) == 0U,
					 "remembered occluded C4 must not stop MoveTo with premature defuse") &&
			passed;
	}
	gEntities[0].v.origin = gPlantedBomb.v.origin;
	gObjectiveTraceVisible = true;
	gMovementLogs.clear();
	const auto updateClientData = gameDll.pfnUpdateClientData;
	const auto getWeaponData = gameDll.pfnGetWeaponData;
	const bool checkNativeReload = !astrabot_test::runtimeCharacterization().enabled();
	if (checkNativeReload)
	{
		gameDll.pfnUpdateClientData = [](const edict_t *, int, clientdata_t *client) { client->m_iId = 17; };
		gameDll.pfnGetWeaponData = [](edict_t *, weapon_data_t *weapons) {
			weapons[17].m_iId = 17;
			weapons[17].m_fInReload = 1;
			return 1;
		};
		bool returnedToIdle = false, usedWhileReloading = false;
		for (int frame = 0; frame < 6 && !returnedToIdle; ++frame)
		{
			tick(0.1f);
			usedWhileReloading = usedWhileReloading || (gLastRunPlayerMoveButtons & IN_USE) != 0U;
			for (const auto &message : gMovementLogs)
				if (message.find("profile navigationStep ") == 0U &&
					message.find("state_from=STATE-MOVE-TO state_to=STATE-IDLE reload_known=1") != std::string::npos)
					returnedToIdle = true;
		}
		passed = check(returnedToIdle && !usedWhileReloading,
			"an active GameDLL reload sends MoveTo back to Idle at visible C4 without issuing Use") && passed;
		gameDll.pfnGetWeaponData = [](edict_t *, weapon_data_t *weapons) {
			weapons[17].m_iId = 17;
			return 1;
		};
		gMovementLogs.clear();
	}
	bool defused = false;
	for (int frame = 0; frame < (checkNativeReload ? 12 : 6); ++frame)
	{
		tick(0.1f);
		defused = defused || (gLastRunPlayerMoveButtons & static_cast<unsigned short>(IN_USE)) != 0U;
	}
	gameDll.pfnUpdateClientData = updateClientData;
	gameDll.pfnGetWeaponData = getWeaponData;
	passed = check(defused, "public CT converges to existing IN_USE defuse at human-planted C4") && passed;
	bool defuseState = false;
	for (const auto &message : gMovementLogs)
		defuseState = defuseState ||
			(message.find("profile goalAssignment ") == 0U &&
				message.find("state=STATE-DEFUSE-BOMB") != std::string::npos) ||
			(message.find("profile navigationStep ") == 0U &&
				message.find("state_to=STATE-DEFUSE-BOMB") != std::string::npos);
	passed = check(defuseState, "visible C4 use belongs to DefuseBomb state, not outer MoveTo action") && passed;
	if (!defused && checkNativeReload)
		for (const auto &message : gMovementLogs)
			if (message.find("profile navigationStep ") == 0U)
				std::fprintf(stderr, "%s\n", message.c_str());
	// A dropped bomb is visible on T radar even behind an opaque wall.
	gPlantedBomb.v.classname = 4;
	gPlantedBomb.v.model = 5;
	gPlantedBomb.serialnumber = 2;
	gEntities[1].v.flags = 0;
	engine.pfnTraceLine = &traceObjectiveVisibility;
	gObjectiveTraceVisible = false;
	runtime.giveEnginePointers(&engine, &globals);
	freshRound(3U, 80.0f, 5.0f);
	gEntities[0].v.weapons = 0;
	const auto teamNotice = [&](const char *teamName)
	{
		runtime.onMessageBegin(MSG_ALL, 86, nullptr, nullptr);
		runtime.onWriteByte(1);
		runtime.onWriteString(teamName);
		runtime.onMessageEnd();
	};
	const auto radarDrop = [&](edict_t *recipient, int fields)
	{
		runtime.onMessageBegin(MSG_ONE, 95, nullptr, recipient);
		runtime.onWriteCoord(224.0f);
		runtime.onWriteCoord(32.0f);
		if (fields == 4)
		{
			runtime.onWriteCoord(36.0f);
			runtime.onWriteByte(0);
		}
		runtime.onMessageEnd();
	};
	const auto sawDroppedAssignment = [&]()
	{
		for (const auto &message : gMovementLogs)
			if (message.find("team_objective ") == 0U && message.find("dropped=1") != std::string::npos)
				return true;
		return false;
	};
	teamNotice("CT");
	gEntities[0].v.team = 2;
	gMovementLogs.clear();
	radarDrop(&gEntities[0], 4);
	for (int frame = 0; frame < 6; ++frame)
		tick(0.1f);
	passed = check(!sawDroppedAssignment(), "CT cannot receive hidden dropped C4 from T radar") && passed;
	teamNotice("TERRORIST");
	gEntities[0].v.team = 1;
	gMovementLogs.clear();
	radarDrop(&gEntities[1], 4);
	radarDrop(&gEntities[0], 2);
	for (int frame = 0; frame < 6; ++frame)
		tick(0.1f);
	passed =
		check(!sawDroppedAssignment(), "wrong recipient and incomplete radar packet reveal no C4 location") && passed;
	gMovementLogs.clear();
	radarDrop(&gEntities[0], 4);
	for (int frame = 0; frame < 24; ++frame)
		tick(0.1f);
	passed = check(sawDroppedAssignment(), "T fetches hidden dropped C4 after its public radar update") && passed;
	runtime.onMessageBegin(MSG_ONE, 97, nullptr, &gEntities[0]);
	runtime.onMessageEnd();
	gPlantedBomb.free = 1;
	gMovementLogs.clear();
	for (int frame = 0; frame < 6; ++frame)
		tick(0.1f);
	passed = check(!sawDroppedAssignment(), "pickup removes dropped C4 assignment") && passed;
	gPlantedBomb.free = 0;
	freshRound(3U, 80.0f, 5.0f);
	gEntities[0].v.weapons = 0;
	gMovementLogs.clear();
	for (int frame = 0; frame < 6; ++frame)
		tick(0.1f);
	passed = check(!sawDroppedAssignment(), "new round cannot reuse old radar location") && passed;
	passed = check(loadNavigation(true, true), "planted guard NAV includes cover positions") && passed;
	globals.mapname = 6;
	gPlantedBomb.v.classname = 2;
	gPlantedBomb.v.model = 3;
	gPlantedBomb.serialnumber = 3;
	freshRound(3U, 80.0f, 5.0f);
	gEntities[0].v.weapons = 0;
	gPlantedBomb.v.dmgtime = globals.time + 35.0f;
	runtime.onMessageBegin(MSG_ALL, 98, nullptr, nullptr);
	runtime.onWriteByte(4);
	runtime.onWriteString("#Bomb_Planted");
	runtime.onMessageEnd();
	gMovementLogs.clear();
	bool movedToCover = false;
	float guardX = 0.0f;
	for (int frame = 0; frame < 24; ++frame)
	{
		tick(0.1f);
		for (const auto &message : gMovementLogs)
			if (message.find("state=STATE-HIDE") != std::string::npos &&
				message.find("goal_source=objective") != std::string::npos &&
				(message.find("goal_position=(32.0 50.0 0.0)") != std::string::npos ||
					message.find("goal_position=(224.0 50.0 0.0)") != std::string::npos))
			{
				movedToCover = true;
				guardX = message.find("goal_position=(32.0 50.0 0.0)") != std::string::npos ? 32.0f : 224.0f;
			}
	}
	passed = check(movedToCover, "T Hide state routes to NAV cover rather than the planted C4") && passed;
	gEntities[0].v.origin = Vector(guardX, 50.0f, 36.0f);
	for (int frame = 0; frame < 6; ++frame)
		tick(0.1f);
	passed = check(gLastRunPlayerMoveForward == 0.0f && gLastRunPlayerMoveSide == 0.0f,
				 "T holds its camping position after reaching NAV cover") &&
		passed;
	// A stopped escape path must select again, including patience expiry.
	gTestC4Timer = 35.0f;
	// Start this independent case on a draw toward the adjacent escape area.
	gSiteFixtureSeed = 3U;
	gEntities[0].v.origin = Vector(80.0f, 32.0f, 36.0f);
	for (int other = 1; other <= 2; ++other)
	{
		gEntities[other].v.flags = FL_CLIENT | FL_ONGROUND;
		gEntities[other].v.health = 100.0f;
		gEntities[other].v.team = 1;
		gEntities[other].v.origin = Vector(other == 1 ? 60.0f : 100.0f, 32.0f, 36.0f);
	}
	gMovementLogs.clear();
	tick(28.0f);
	for (int frame = 0; frame < 100; ++frame)
		tick(0.1f);
	bool escapeRetry = false, escaping = false;
	for (const auto &message : gMovementLogs)
	{
		escaping = escaping || message.find("state=STATE-ESCAPE-BOMB") != std::string::npos;
		escapeRetry = escapeRetry || message.find("profile bombEscapeRetry ") == 0U;
	}
	passed = check(escaping && escapeRetry,
				 "escape path failure or patience expiry invalidates cached escape destination for next update") &&
		passed;
	gTestC4Timer = 0.0f;
	gEntities[1].v.flags = 0;
	gEntities[2].v.flags = 0;
	globals.mapname = 0;
	runtime.executeCompatibilityCommand(request("bot_kick", 1U, "all"));
	gCaptureMovementLogs = false;
	gProfileEnabled = false;
	gObjectiveEntitiesAvailable = false;
	gBombSite = {};
	gSecondBombSite = {};
	gPlantedBomb = {};
	gMpRoundtime = 5.0f;
	globals.maxEntities = 7;
	engine.pfnRandomLong = &jumpFixtureRandomLong;
	engine.pfnRandomFloat = &jumpFixtureRandomFloat;
	return passed;
}

} // namespace

int main(int argc, char **argv)
{
	(void)argc;
#ifdef ASTRABOT_ACTOR_TEST_ARGS
	ASTRABOT_ACTOR_TEST_ARGS(argc, argv);
#else
	(void)argv;
#endif
	using astrabot::metamod::CompatibilityCommandResult;
	using astrabot::metamod::FakeClientHandle;
	using astrabot::metamod::FakeClientResult;
	using astrabot::metamod::PluginRuntime;

	const char *const profilePath = "astrabot_actor_command_profile.db";
	const char profileText[] =
#ifdef ASTRABOT_ACTOR_PROFILE_TEXT
		ASTRABOT_ACTOR_PROFILE_TEXT;
#else
		"AnyProfile { Name = \"Any Bot\" Team = any Difficulty = 1 Skill = 50 }\n"
		"CtProfile { Name = \"CT Bot\" Team = CT Difficulty = 1 Skill = 60 }\n"
		"TProfile { Name = \"T Bot\" Team = T Difficulty = 1 Skill = 70 }\n"
		"NamedProfile { Name = \"Named Bot\" Team = any Difficulty = 1 Skill = 80 }\n"
		"ExtraProfile { Name = \"Extra Bot\" Team = any Difficulty = 1 Skill = 55 }\n";
#endif
	std::FILE *profileFile = openFixture(profilePath, "wb");
	if (!check(profileFile != nullptr, "actor command profile fixture opens"))
	{
		return 1;
	}
	const std::size_t profileLength = std::strlen(profileText);
	const std::size_t profileWritten = std::fwrite(profileText, 1U, profileLength, profileFile);
	const int profileCloseResult = std::fclose(profileFile);
	if (!check(profileWritten == profileLength && profileCloseResult == 0, "actor command profile fixture is written"))
	{
		std::remove(profilePath);
		return 1;
	}

	const char *const defaultProfilePath = "BotProfile.db";
	ExistingProfileGuard existingDefaultProfile;
	if (!check(existingDefaultProfile.capture(defaultProfilePath),
			"existing default profile is preserved before writing the fixture"))
		return 1;
	std::FILE *defaultProfileFile = openFixture(defaultProfilePath, "wb");
	if (!check(defaultProfileFile != nullptr, "default BotProfile fixture opens"))
	{
		std::remove(profilePath);
		return 1;
	}
	const std::size_t defaultProfileWritten = std::fwrite(profileText, 1U, profileLength, defaultProfileFile);
	const int defaultProfileCloseResult = std::fclose(defaultProfileFile);
	if (!check(defaultProfileWritten == profileLength && defaultProfileCloseResult == 0,
			"default BotProfile fixture is written"))
	{
		std::remove(profilePath);
		std::remove(defaultProfilePath);
		return 1;
	}

	meta_globals_t metaGlobals{};
	DLL_FUNCTIONS gameDllTable{};
	gameDllTable.pfnClientConnect = &clientConnect;
	gameDllTable.pfnClientPutInServer = &putInServer;
	gameDllTable.pfnClientDisconnect = &disconnect;
	gameDllTable.pfnClientKill = &clientKill;
	gameDllTable.pfnClientCommand = &gameClientCommand;
#ifdef ASTRABOT_ACTOR_GAMEDLL_SETUP
	ASTRABOT_ACTOR_GAMEDLL_SETUP(gameDllTable);
#endif
	gHookedGameDllTable = gameDllTable;
	gHookedGameDllTable.pfnClientCommand = &hookedGameClientCommand;
	gamedll_funcs_t gameDllFunctions{};
	gameDllFunctions.dllapi_table = &gameDllTable;
	META_FUNCTIONS metaFunctions{};
	mutil_funcs_t metaUtils{};
	metaUtils.pfnGetHookTables = &getHookTables;
	metaUtils.pfnLogConsole = &captureMovementLog;
	metaUtils.pfnGetUserMsgID = &siteFixtureUserMessageId;
	char metaInterfaceVersion[] = META_INTERFACE_VERSION;
	plugin_info_t *pluginInfo = nullptr;
	if (!check(astrabot::metamod::Meta_Query(metaInterfaceVersion, &pluginInfo, &metaUtils) == TRUE,
			"Meta_Query installs the hook-table test boundary"))
	{
		std::remove(profilePath);
		std::remove(defaultProfilePath);
		return 1;
	}
	PluginRuntime &runtime = PluginRuntime::instance();
	globalvars_t globals{};
	globals.maxClients = 5;
	globals.maxEntities = 7;
	globals.time = 1.0f;
	runtime.giveEnginePointers(nullptr, &globals);
	if (!check(runtime.attach(PT_ANYTIME, &metaFunctions, &metaGlobals, &gameDllFunctions, pluginInfo),
			"runtime attaches for actor command test"))
	{
		std::remove(profilePath);
		return 1;
	}

	enginefuncs_t requestedEngineFunctions{};
	int requestedEngineInterfaceVersion = ENGINE_INTERFACE_VERSION;
	if (!check(runtime.provideEngineFunctions(&requestedEngineFunctions, &requestedEngineInterfaceVersion),
			"runtime provides engine hook table") ||
		!check(
			requestedEngineFunctions.pfnWriteCoord != nullptr, "public radar bomb coordinates have an engine hook") ||
		!check(requestedEngineFunctions.pfnAlertMessage != nullptr,
			"public alert hook observes logged round lifecycle messages"))
	{
		std::remove(profilePath);
		return 1;
	}

	enginefuncs_t engineFunctions{};
	engineFunctions.pfnCreateFakeClient = &createFakeClient;
	engineFunctions.pfnIndexOfEdict = &indexOfEdict;
	engineFunctions.pfnPEntityOfEntIndex = &entityOfIndex;
	engineFunctions.pfnFindEntityByString = &findEntityByString;
	engineFunctions.pfnSzFromIndex = &szFromIndex;
	engineFunctions.pfnCVarGetPointer = &getCvar;
	engineFunctions.pfnCVarGetFloat = &getCvarFloat;
	engineFunctions.pfnCVarSetFloat = &setCvarFloat;
	engineFunctions.pfnCvar_RegisterVariable = &registerCvar;
	engineFunctions.pfnClientCommand = &clientCommand;
	engineFunctions.pfnRunPlayerMove = &runPlayerMove;
	engineFunctions.pfnSetClientMaxspeed = &setClientMaxspeed;
	engineFunctions.pfnGetInfoKeyBuffer = &getInfoKeyBuffer;
	engineFunctions.pfnSetClientKeyValue = &setClientKeyValue;
	engineFunctions.pfnGetGameDir = &getGameDir;
	engineFunctions.pfnGetPlayerUserId = &playerUserId;
	engineFunctions.pfnServerCommand = &serverCommand;
	engineFunctions.pfnServerExecute = &serverExecute;
	engineFunctions.pfnRandomLong = &recoveryFixtureRandomLong;
	engineFunctions.pfnRandomFloat = &jumpFixtureRandomFloat;
#ifdef ASTRABOT_ACTOR_ENGINE_SETUP
	ASTRABOT_ACTOR_ENGINE_SETUP(engineFunctions, globals);
#endif
	runtime.giveEnginePointers(&engineFunctions, &globals);
	gBotEnable = 1.0f;
	gBotQuota = 1.0f;
	setCvarFloat("bot_join_after_player", 0.0f);
	runtime.onServerActivate(nullptr, 0, 4);
	runtime.onStartFrame();
	if (!check(gCreateCount == 1 && gCvarRegisterCount == 10 + static_cast<int>(kManagerCvarCount) &&
				getCvar("bot_allow_rogues") != nullptr && gClientCommandCount == 0 && gClientKeyValueCount >= 2,
			"quota automatically loads the default profile during activation"))
	{
		std::fprintf(stderr, "activation counts: created=%d registered=%d commands=%d keys=%d quota=%g\n", gCreateCount,
			gCvarRegisterCount, gClientCommandCount, gClientKeyValueCount, gBotQuota);
		std::remove(profilePath);
		std::remove(defaultProfilePath);
		return 1;
	}
	gServerSetsEntityTeam = false;
	gRunPlayerMoveCount = 0;
	gLastRunPlayerMoveButtons = 0U;
	globals.time = 1.1f;
	runtime.onStartFrame();
	runtime.onStartFramePost();
	if (!check(gRunPlayerMoveCount == 1 && gLastRunPlayerMoveButtons == static_cast<unsigned short>(IN_ATTACK) &&
				runtime.movementPhysicsSample(1U).readiness == astrabot::runtime::SpawnReadiness::NotReady,
			"initial joining heartbeat advances the public player lifecycle"))
	{
		std::remove(profilePath);
		std::remove(defaultProfilePath);
		return 1;
	}
	globals.time = 1.1f;
	for (int frame = 0; frame < 8; ++frame)
	{
		runtime.onStartFrame();
		runtime.onStartFramePost();
	}
	if (!check(gClientCommandCount == 2 && gDirectJoinCommandSeen && !gMenuTeamContextValid && !gMenuClassContextValid,
			"public jointeam and joinclass commands are sent after bounded frame delays"))
	{
		std::remove(profilePath);
		std::remove(defaultProfilePath);
		return 1;
	}
	gRunPlayerMoveCount = 0;
	globals.time += 0.1f;
	runtime.onStartFrame();
	runtime.onStartFramePost();
	if (!check(gRunPlayerMoveCount == 1 && gLastRunPlayerMoveButtons == 0U &&
				runtime.movementPhysicsSample(1U).readiness == astrabot::runtime::SpawnReadiness::NotReady,
			"joining actor receives a neutral heartbeat while awaiting physical spawn"))
	{
		std::remove(profilePath);
		std::remove(defaultProfilePath);
		return 1;
	}
	gEntities[0].v.flags &= ~FL_SPECTATOR;
	gEntities[0].v.flags |= (FL_CLIENT | FL_FAKECLIENT | FL_ONGROUND);
	gEntities[0].v.deadflag = DEAD_NO;
	gEntities[0].v.health = 100.0f;
	gEntities[0].v.solid = SOLID_SLIDEBOX;
	gEntities[0].v.movetype = MOVETYPE_WALK;
	gEntities[0].v.flags &= ~FL_FAKECLIENT;
	runtime.onMessageBegin(0, 86, nullptr, nullptr);
	runtime.onWriteByte(1);
	runtime.onWriteString("TERRORIST");
	runtime.onMessageEnd();
	gEntities[0].v.team = 0;
	gEntities[1].v.flags = FL_CLIENT | FL_ONGROUND;
	gEntities[1].v.team = 2;
	gEntities[1].v.deadflag = DEAD_NO;
	gEntities[1].v.health = 100.0f;
	gEntities[1].v.origin.x = 64.0f;
	gEntities[1].v.origin.y = 0.0f;
	gEntities[1].v.origin.z = 0.0f;
	gRunPlayerMoveCount = 0;
	for (int frame = 0; frame < 6; ++frame)
	{
		globals.time += 0.1f;
		runtime.onStartFrame();
		runtime.onStartFramePost();
	}
	if (!check(gRunPlayerMoveCount == 5 && (gLastRunPlayerMoveButtons & static_cast<unsigned short>(IN_ATTACK)) == 0U,
			"joined actor starts command cadence after the initial scheduler frame"))
	{
		std::remove(profilePath);
		std::remove(defaultProfilePath);
		return 1;
	}
	const auto physicsSample = runtime.movementPhysicsSample(1U);
	if (!check(physicsSample.dispatched && physicsSample.readiness == astrabot::runtime::SpawnReadiness::Ready,
			"heartbeat records dispatched spawn-ready physics sample"))
	{
		std::remove(profilePath);
		std::remove(defaultProfilePath);
		return 1;
	}
#ifdef ASTRABOT_EXTRA_ACTOR_TEST
	if (argc > 1)
	{
		const bool extraPassed = ASTRABOT_EXTRA_ACTOR_TEST(runtime, engineFunctions, globals);
		std::remove(profilePath);
		std::remove(defaultProfilePath);
		runtime.onServerDeactivate();
		runtime.detach(PT_ANYTIME, PNL_NULL);
		return extraPassed ? 0 : 1;
	}
#endif
	const float timingStart = globals.time;
	gRunPlayerMoveCount = 0;
	globals.time = timingStart + 0.010f;
	runtime.onStartFrame();
	runtime.onStartFramePost();
	if (!check(gRunPlayerMoveCount == 0, "joined runtime does not submit before the 30Hz command deadline"))
	{
		std::remove(profilePath);
		std::remove(defaultProfilePath);
		return 1;
	}
	globals.time = timingStart + 0.034f;
	runtime.onStartFrame();
	runtime.onStartFramePost();
	if (!check(gRunPlayerMoveCount == 1, "joined runtime submits once when the 30Hz command deadline is due"))
	{
		std::remove(profilePath);
		std::remove(defaultProfilePath);
		return 1;
	}
	gEntities[0].v.flags |= FL_FROZEN;
	gRunPlayerMoveCount = 0;
	gLastRunPlayerMoveButtons = 0U;
	gLastRunPlayerMoveMsec = 255U;
	globals.time = timingStart + 0.070f;
	runtime.onStartFrame();
	runtime.onStartFramePost();
	if (!check(gRunPlayerMoveCount == 1 && gLastRunPlayerMoveButtons == 0U && gLastRunPlayerMoveMsec == 0U,
			"frozen scheduled command clears buttons and submits zero msec"))
	{
		std::remove(profilePath);
		std::remove(defaultProfilePath);
		return 1;
	}
	gEntities[0].v.flags &= ~FL_FROZEN;
	gRunPlayerMoveCount = 0;
	globals.time = timingStart + 0.110f;
	runtime.onStartFrame();
	runtime.onStartFramePost();
	const auto postFrozenSample = runtime.movementPhysicsSample(1U);
	if (!check(gRunPlayerMoveCount == 1 && postFrozenSample.commandSequence > 0U,
			"post-frozen command execution refreshes the command sequence"))
	{
		std::remove(profilePath);
		std::remove(defaultProfilePath);
		return 1;
	}
	gEntities[0].v.team = 2;
	runtime.onMessageBegin(0, 86, nullptr, nullptr);
	runtime.onWriteByte(1);
	runtime.onWriteString("CT");
	runtime.onMessageEnd();
	gEntities[1].v.team = 1;
	gEntities[1].v.flags = FL_CLIENT | FL_ONGROUND;
	gEntities[1].v.deadflag = DEAD_NO;
	gEntities[1].v.health = 100.0f;
	const float observedFovBeforeObjective = gEntities[0].v.fov;
	const float observedOriginXBeforeObjective = gEntities[0].v.origin.x;
	const int observedTeamBeforeObjective = static_cast<int>(gEntities[0].v.team);
	gBombSite.v.origin = gEntities[0].v.origin;
	gBombSite.v.classname = 1;
	// Public scenario updates need a bomb-map name and an entity count that
	// includes index 7 (the engine's count is exclusive of its upper bound).
	globals.mapname = 6;
	globals.maxEntities = 8;
	const char *defuseNav = "astrabot_defuse_state.nav";
	if (!check(writeJumpNavigation(defuseNav, true), "defuse state NAV fixture written"))
		return 1;
	const astrabot::metamod::NavLoadRequest defuseLoad = {
		defuseNav, "astrabot_defuse_state", runtime.snapshot().mapGeneration, false, 0U, false, 0U};
	const auto defuseLoaded = runtime.loadNavigationFile(&defuseLoad);
	std::remove(defuseNav);
	if (!check(defuseLoaded == astrabot::metamod::NavLoadResult::Loaded,
			"defuse uses actual path completion instead of a no-NAV action bypass"))
		return 1;
	gPlantedBomb.v.classname = 2;
	gPlantedBomb.v.model = 3;
	gPlantedBomb.serialnumber = 1;
	gPlantedBomb.v.origin = gEntities[0].v.origin;
	gPlantedBomb.v.dmgtime = 0.0f;
	gEntities[1].v.team = 2;
	gEntities[1].serialnumber = 2;
	gEntities[1].v.origin = gPlantedBomb.v.origin;
	gEntities[0].v.maxspeed = 0.0f;
	gObjectiveEntitiesAvailable = true;
	gFindEntityByStringCount = 0;
	engineFunctions.pfnTraceLine = &traceObjectiveVisibility;
	gObjectiveTraceVisible = false;
	const float beforeHiddenBomb = globals.time;
	gCaptureMovementLogs = true;
	gProfileEnabled = true;
	gMovementLogs.clear();
	for (int frame = 0; frame < 4; ++frame)
	{
		globals.time = beforeHiddenBomb + (frame + 1) * 0.1f;
		runtime.onStartFrame();
		runtime.onStartFramePost();
		if (!check((gLastRunPlayerMoveButtons & static_cast<unsigned short>(IN_USE)) == 0U,
				"undiscovered C4 entity cannot make CT issue defuse use"))
			return 1;
	}
	gObjectiveTraceVisible = true;
	runtime.onCmdStart(&gEntities[1], static_cast<std::uint16_t>(IN_USE));
	gFindEntityByStringCount = 0;
	gRunPlayerMoveCount = 0;
	globals.time += 0.1f;
	runtime.onStartFrame();
	runtime.onStartFramePost();
	if (!check(gRunPlayerMoveCount == 1 && (gLastRunPlayerMoveButtons & static_cast<unsigned short>(IN_USE)) == 0U &&
				gEntities[0].v.fov == observedFovBeforeObjective &&
				gEntities[0].v.origin.x == observedOriginXBeforeObjective &&
				static_cast<int>(gEntities[0].v.team) == observedTeamBeforeObjective,
			"nearby CT using planted C4 suppresses Bot IN_USE"))
	{
		std::remove(profilePath);
		std::remove(defaultProfilePath);
		return 1;
	}
	const float maxspeedBeforeBotTakeover = gEntities[0].v.maxspeed;
	const int maxspeedCallsBeforeBotTakeover = gSetClientMaxspeedCount;
	runtime.onCmdStart(&gEntities[1], 0U);
	globals.time += 0.30f;
	gRunPlayerMoveCount = 0;
	runtime.onStartFrame();
	runtime.onStartFramePost();
	bool resumedDefuse = (gLastRunPlayerMoveButtons & static_cast<unsigned short>(IN_USE)) != 0U;
	for (int frame = 0; frame < 6 && !resumedDefuse; ++frame)
	{
		globals.time += 0.1f;
		runtime.onStartFrame();
		runtime.onStartFramePost();
		resumedDefuse = (gLastRunPlayerMoveButtons & static_cast<unsigned short>(IN_USE)) != 0U;
	}
	if (!check(gRunPlayerMoveCount >= 1 && resumedDefuse, "Bot defuse resumes after external CT releases C4 use"))
	{
		std::fprintf(stderr, "defuse fixture solid=%d movetype=%d health=%.1f origin=(%.1f %.1f %.1f)\n",
			static_cast<int>(gEntities[0].v.solid), static_cast<int>(gEntities[0].v.movetype), gEntities[0].v.health,
			gEntities[0].v.origin.x, gEntities[0].v.origin.y, gEntities[0].v.origin.z);
		for (const auto &message : gMovementLogs)
			if (message.find("profile navigationStep ") == 0U || message.find("profile goalAssignment ") == 0U ||
				message.find("movement unavailable") == 0U || message.find("profile movementGate ") == 0U)
				std::fprintf(stderr, "%s\n", message.c_str());
		std::remove(profilePath);
		std::remove(defaultProfilePath);
		return 1;
	}
	if (!check(gEntities[0].v.maxspeed == maxspeedBeforeBotTakeover &&
				gSetClientMaxspeedCount == maxspeedCallsBeforeBotTakeover,
			"defuse stop action does not restore movement maxspeed"))
	{
		std::remove(profilePath);
		std::remove(defaultProfilePath);
		return 1;
	}
	if (!check(gFindEntityByStringCount == 0, "planted bomb objective avoids Metamod FindEntityByString re-entry"))
	{
		std::remove(profilePath);
		std::remove(defaultProfilePath);
		return 1;
	}
	gObjectiveEntitiesAvailable = false;
	engineFunctions.pfnTraceLine = nullptr;
	gBombSite.v.classname = 0;
	gPlantedBomb.v.classname = 0;
	gPlantedBomb.v.dmgtime = 0.0f;
	gEntities[0].v.maxspeed = 1.0f;
	const int maxspeedCallsBeforeRecovery = gSetClientMaxspeedCount;
	gRunPlayerMoveCount = 0;
	gLastRunPlayerMoveButtons = 0U;
	globals.time += 0.12f;
	runtime.onStartFrame();
	runtime.onStartFramePost();
	if (!check(gRunPlayerMoveCount == 1 && gEntities[0].v.maxspeed == 240.0f &&
				gSetClientMaxspeedCount == maxspeedCallsBeforeRecovery + 1 && gLastClientMaxspeed == 240.0f,
			"stale maxspeed restores on a live non-objective command"))
	{
		std::remove(profilePath);
		std::remove(defaultProfilePath);
		return 1;
	}
	gRunPlayerMoveCount = 0;
	globals.time += 0.1f;
	runtime.onStartFrame();
	runtime.onStartFramePost();
	if (!check((gLastRunPlayerMoveButtons & static_cast<unsigned short>(IN_ATTACK)) == 0U,
			"live combat does not dispatch attack without an active weapon boundary"))
	{
		std::remove(profilePath);
		std::remove(defaultProfilePath);
		return 1;
	}
	gServerSetsEntityTeam = true;
	gEntities[0].v.flags |= FL_SPECTATOR;
	gEntities[0].v.deadflag = DEAD_DEAD;
	gEntities[0].v.health = 0.0f;
	gRunPlayerMoveCount = 0;
	globals.time += 0.1f;
	runtime.onStartFrame();
	runtime.onStartFramePost();
	if (!check(gRunPlayerMoveCount == 1 &&
				runtime.movementPhysicsSample(1U).readiness == astrabot::runtime::SpawnReadiness::NotReady,
			"joined spectator/dead actor receives one scheduled neutral RunPlayerMove"))
	{
		std::remove(profilePath);
		std::remove(defaultProfilePath);
		return 1;
	}
	gEntities[0].v.flags &= ~FL_SPECTATOR;
	gEntities[0].v.deadflag = DEAD_NO;
	gEntities[0].v.health = 100.0f;
	gEntities[0].v.flags &= ~FL_ONGROUND;
	gRunPlayerMoveCount = 0;
	globals.time += 0.1f;
	runtime.onStartFrame();
	runtime.onStartFramePost();
	if (!check(gRunPlayerMoveCount == 1,
			"respawning actor receives one scheduled neutral command before grounded readiness"))
	{
		std::remove(profilePath);
		std::remove(defaultProfilePath);
		return 1;
	}
	if (!check(
			runtime.executeCompatibilityCommand(request("bot_kick", 1U, "all")) == CompatibilityCommandResult::Handled,
			"default profile actor can be removed before explicit profile test"))
		gCreateCount = 0;
	gClientCommandCount = 0;
	gDirectJoinCommandSeen = false;
	gMenuTeamContextValid = false;
	gMenuClassContextValid = false;
	resetEntities();
	runtime.setCompatibilityFloat("bot_enable", 1.0f);
	runtime.setCompatibilityFloat("bot_quota", 1.0f);
	gBotEnable = 1.0f;
	gBotQuota = 1.0f;
	globals.time = 1.4f;
	runtime.onStartFrame();
	const CompatibilityCommandResult fallbackResult = runtime.executeCompatibilityCommand(request("bot_add_t"));
	if (!check(
			fallbackResult == CompatibilityCommandResult::Handled, "fallback test accepts the terrorist add command"))
	{
		std::remove(profilePath);
		std::remove(defaultProfilePath);
		return 1;
	}
	if (!check(gCreateCount > 0, "fallback test creates a pending terrorist actor"))
	{
		std::remove(profilePath);
		std::remove(defaultProfilePath);
		return 1;
	}
	for (int frame = 0; frame < 8; ++frame)
	{
		globals.time += 0.1f;
		runtime.onStartFrame();
		runtime.onStartFramePost();
	}
	if (!check(gClientCommandCount == 2 && gDirectJoinCommandSeen && !gMenuTeamContextValid && !gMenuClassContextValid,
			"fallback uses public join commands when menu messages are absent"))
	{
		std::remove(profilePath);
		std::remove(defaultProfilePath);
		return 1;
	}
	runtime.executeCompatibilityCommand(request("bot_kick", 1U, "all"));
	gCreateCount = 0;
	gClientCommandCount = 0;
	resetEntities();
	gBotQuota = 1.0f;
	gameDllTable.pfnClientCommand = nullptr;
	gHookedGameDllTable.pfnClientCommand = nullptr;
	globals.time += 0.1f;
	runtime.onStartFrame();
	if (!check(runtime.executeCompatibilityCommand(request("bot_add")) == CompatibilityCommandResult::Handled,
			"failed join scenario creates a managed actor"))
	{
		std::remove(profilePath);
		std::remove(defaultProfilePath);
		return 1;
	}
	for (int frame = 0; frame < 8; ++frame)
	{
		globals.time += 0.1f;
		runtime.onStartFrame();
		runtime.onStartFramePost();
	}
	gameDllTable.pfnClientCommand = &gameClientCommand;
	gHookedGameDllTable.pfnClientCommand = &hookedGameClientCommand;
	if (!check(runtime.executeCompatibilityCommand(request("bot_add")) == CompatibilityCommandResult::Handled,
			"failed join cleanup releases managed actor for retry"))
	{
		std::remove(profilePath);
		std::remove(defaultProfilePath);
		return 1;
	}
	runtime.executeCompatibilityCommand(request("bot_kick", 1U, "all"));
	gCreateCount = 0;
	gKillCount = 0;
	gClientCommandCount = 0;
	std::memset(gLastClientCommand, 0, sizeof(gLastClientCommand));
	gClientKeyValueCount = 0;
	resetEntities();
	runtime.setCompatibilityFloat("bot_enable", 0.0f);
	runtime.setCompatibilityFloat("bot_quota", 0.0f);
	gBotEnable = 1.0f;
	gBotQuota = 2.0f;
	globals.time = 1.5f;
	runtime.onStartFrame();
	if (!check(runtime.executeCompatibilityCommand(request("bot_add")) == CompatibilityCommandResult::Handled &&
				gCreateCount == 1,
			"runtime adopts changed native bot controls before command execution"))
	{
		std::remove(profilePath);
		std::remove(defaultProfilePath);
		return 1;
	}
	if (!check(
			runtime.executeCompatibilityCommand(request("bot_kick", 1U, "all")) == CompatibilityCommandResult::Handled,
			"changed native control actor can be removed"))
	{
		std::remove(profilePath);
		std::remove(defaultProfilePath);
		return 1;
	}
	runtime.executeCompatibilityCommand(request("bot_kick", 1U, "all"));
	gCreateCount = 0;
	gClientCommandCount = 0;
	gHookedClientCommandCount = 0;
	gUnhookedClientCommandCount = 0;
	gDirectJoinCommandSeen = false;
	gMenuTeamContextValid = false;
	gMenuClassContextValid = false;
	resetEntities();
	gBotQuota = 2.0f;
	gameDllTable.pfnClientCommand = &unhookedGameClientCommand;
	gHookedGameDllTable.pfnClientCommand = &hookedGameClientCommand;
	globals.time += 0.1f;
	runtime.onStartFrame();
	if (!check(runtime.executeCompatibilityCommand(request("bot_add_t")) == CompatibilityCommandResult::Handled,
			"hook-table dispatch test creates a terrorist actor"))
	{
		std::remove(profilePath);
		std::remove(defaultProfilePath);
		return 1;
	}
	for (int frame = 0; frame < 8; ++frame)
	{
		globals.time += 0.1f;
		runtime.onStartFrame();
		runtime.onStartFramePost();
	}
	if (!check(gHookedClientCommandCount == 2 && gUnhookedClientCommandCount == 0,
			"join menu selections use the Metamod hook-table GameDLL dispatcher"))
	{
		std::remove(profilePath);
		std::remove(defaultProfilePath);
		return 1;
	}
	runtime.executeCompatibilityCommand(request("bot_kick", 1U, "all"));
	gameDllTable.pfnClientCommand = &gameClientCommand;
	gHookedGameDllTable.pfnClientCommand = &hookedGameClientCommand;
	gCreateCount = 0;
	gClientCommandCount = 0;
	gHookedClientCommandCount = 0;
	gUnhookedClientCommandCount = 0;
	resetEntities();
	gBotEnable = 1.0f;
	gBotQuota = 1.0f;
	globals.time += 0.1f;
	runtime.onStartFrame();
	gameDllTable.pfnClientCommand = &gameClientCommand;
	gHookedGameDllTable.pfnClientCommand = nullptr;
	if (!check(runtime.executeCompatibilityCommand(request("bot_add_t")) == CompatibilityCommandResult::Handled,
			"partial hook-table dispatch test creates a terrorist actor"))
	{
		std::remove(profilePath);
		std::remove(defaultProfilePath);
		return 1;
	}
	for (int frame = 0; frame < 8; ++frame)
	{
		globals.time += 0.1f;
		runtime.onStartFrame();
		runtime.onStartFramePost();
	}
	if (!check(gClientCommandCount == 2 && gHookedClientCommandCount == 0 && gUnhookedClientCommandCount == 0,
			"join dispatch falls back when the hook table lacks pfnClientCommand"))
	{
		std::remove(profilePath);
		std::remove(defaultProfilePath);
		return 1;
	}
	runtime.executeCompatibilityCommand(request("bot_kick", 1U, "all"));
	gHookedGameDllTable.pfnClientCommand = &hookedGameClientCommand;
	gCreateCount = 0;
	resetEntities();
	gBotEnable = 0.0f;
	gBotQuota = 0.0f;
	runtime.setCompatibilityFloat("bot_enable", 0.0f);
	runtime.setCompatibilityFloat("bot_quota", 0.0f);
	if (!check(runtime.snapshot().managedBotCreationAllowed, "native guard allows managed actor creation"))
	{
		std::remove(profilePath);
		return 1;
	}
	if (!check(runtime.loadCompatibilityProfiles(profilePath) == astrabot::metamod::ProfileLoadResult::Loaded,
			"runtime loads actor command profiles"))
	{
		std::remove(profilePath);
		return 1;
	}
	const auto setDesiredFloat = [&runtime](const char *name, float value)
	{
		const auto result = runtime.setCompatibilityFloat(name, value);
		return result == astrabot::compat::CvarUpdateResult::Updated ||
			result == astrabot::compat::CvarUpdateResult::NoChange;
	};
	if (!check(setDesiredFloat("bot_enable", 1.0f) && setDesiredFloat("bot_quota", 4.0f) &&
				setDesiredFloat("bot_difficulty", 1.0f),
			"runtime accepts desired actor configuration"))
	{
		std::remove(profilePath);
		return 1;
	}

	if (!check(runtime.executeCompatibilityCommand(request("bot_add")) == CompatibilityCommandResult::Handled &&
				gCreateCount == 1,
			"bot_add creates an actor through runtime"))
	{
		std::remove(profilePath);
		return 1;
	}
	if (!check(runtime.executeCompatibilityCommand(request("bot_add_ct")) == CompatibilityCommandResult::Handled &&
				gCreateCount == 2,
			"bot_add_ct selects the counter-terrorist profile"))
	{
		std::remove(profilePath);
		return 1;
	}
	if (!check(runtime.executeCompatibilityCommand(request("bot_add_t")) == CompatibilityCommandResult::Handled &&
				gCreateCount == 3,
			"bot_add_t selects the terrorist profile"))
	{
		std::remove(profilePath);
		return 1;
	}
	if (!check(runtime.executeCompatibilityCommand(request("bot_add", 1U, "Named Bot")) ==
					CompatibilityCommandResult::Handled &&
				gCreateCount == 4,
			"bot_add accepts an explicit profile name"))
	{
		std::remove(profilePath);
		return 1;
	}
	if (!check(setDesiredFloat("bot_quota", 5.0f), "quota allows identity collision check"))
	{
		std::remove(profilePath);
		return 1;
	}
	const int createCountBeforeDuplicate = gCreateCount;
	if (!check(runtime.executeCompatibilityCommand(request("bot_add", 1U, "Named Bot")) ==
					CompatibilityCommandResult::NameTaken &&
				gCreateCount == createCountBeforeDuplicate,
			"duplicate managed names are rejected before engine creation"))
	{
		std::remove(profilePath);
		return 1;
	}
	if (!check(setDesiredFloat("bot_quota", 4.0f), "quota is restored for capacity check"))
	{
		std::remove(profilePath);
		return 1;
	}
	if (!check(runtime.executeCompatibilityCommand(request("bot_add")) == CompatibilityCommandResult::Handled &&
				gCreateCount == 5,
			"manual add exceeds previous quota and creates an unused profile"))
	{
		std::remove(profilePath);
		return 1;
	}
	if (!check(runtime.executeCompatibilityCommand(request("bot_kill", 1U, "all")) ==
					CompatibilityCommandResult::Handled &&
				gKillCount == 5,
			"bot_kill all uses the actor kill operation"))
	{
		std::remove(profilePath);
		return 1;
	}
	// Keep new scenarios separate from the historical callback fixture.
	if (astrabot_test::runtimeCharacterization().enabled())
	{
		if (!check(runtime.executeCompatibilityCommand(request("bot_kick", 1U, "CT Bot")) ==
					CompatibilityCommandResult::Handled, "bot_kick removes a named managed actor") ||
			!check(runtime.executeCompatibilityCommand(request("bot_kick", 1U, "all")) ==
					CompatibilityCommandResult::Handled, "bot_kick all removes remaining managed actors") ||
			!check(runtime.executeCompatibilityCommand(request("bot_kick", 1U, "all")) ==
					CompatibilityCommandResult::NoTarget, "repeated bot_kick reports no target"))
		{
			std::remove(profilePath);
			return 1;
		}
	}
	else
	{
		struct WeaponPresetCase
		{
			const char *command;
			float expected[kWeaponPresetCvarCount];
		};
		const WeaponPresetCase weaponPresets[] = {{"bot_all_weapons", {1, 1, 1, 1, 1, 1, 1, 1}},
			{"bot_knives_only", {0, 0, 0, 0, 0, 0, 0, 0}}, {"bot_snipers_only", {1, 0, 0, 0, 0, 0, 1, 0}},
			{"bot_pistols_only", {1, 0, 0, 0, 0, 0, 0, 0}}};
		bool managementPassed = true;
		cvar_t enginePistolAllowance{};
		gWeaponPresetCvars[0] = &enginePistolAllowance;
		for (const auto &preset : weaponPresets)
		{
			const auto result = runtime.executeCompatibilityCommand(request(preset.command));
			bool valuesMatch = true;
			for (std::size_t index = 0U; index < kWeaponPresetCvarCount; ++index)
			{
				valuesMatch &= gWeaponPresetCvars[index] != nullptr &&
					gWeaponPresetCvars[index]->value == preset.expected[index];
			}
			managementPassed &= check(result == CompatibilityCommandResult::Handled && valuesMatch, preset.command);
		}
		managementPassed &= check(gWeaponPresetCvars[0] == &enginePistolAllowance,
			"weapon presets reuse a GameDLL-owned allowance and register missing allowances");

		const auto oldCvarGetString = engineFunctions.pfnCVarGetString;
		engineFunctions.pfnCVarGetString = [](const char *name) -> const char *
		{
			return std::strcmp(name, "bot_quota_mode") == 0 ? "fill" : "";
		};
		gBotQuota = 5.0f;
		const auto namedKick = runtime.executeCompatibilityCommand(request("bot_kick", 1U, "CT Bot"));
		engineFunctions.pfnCVarGetString = oldCvarGetString;
		managementPassed &= check(namedKick == CompatibilityCommandResult::Handled && gBotQuota == 5.0f,
			"named bot_kick in fill mode removes the managed bot and preserves quota");
		runtime.setCompatibilityString("bot_quota_mode", "normal");
		setDesiredFloat("bot_quota", 5.0f);
		const auto normalKick = runtime.executeCompatibilityCommand(request("bot_kick", 1U, "Named Bot"));
		managementPassed &= check(normalKick == CompatibilityCommandResult::Handled && gBotQuota == 4.0f,
			"named bot_kick in normal mode decrements quota");
		setDesiredFloat("bot_quota", 5.0f);
		gBotQuota = 5.0f;
		const auto allKick = runtime.executeCompatibilityCommand(request("bot_kick", 1U, "all"));
		managementPassed &= check(allKick == CompatibilityCommandResult::Handled && gBotQuota == 0.0f,
			"bot_kick all removes the remaining managed bots and clears quota");
		setDesiredFloat("bot_quota", 2.0f);
		gBotQuota = 2.0f;
		const auto emptyKick = runtime.executeCompatibilityCommand(request("bot_kick"));
		managementPassed &= check(emptyKick == CompatibilityCommandResult::NoTarget && gBotQuota == 0.0f,
			"bot_kick without arguments clears quota even on an empty roster");
		for (auto &allowance : gWeaponPresetCvars)
			allowance = nullptr;
		if (!managementPassed)
		{
			std::remove(profilePath);
			return 1;
		}

	}

	const int createCountBeforeInvalid = gCreateCount;
	if (!check(runtime.executeCompatibilityCommand(request("bot_add", 2U, "one")) ==
					CompatibilityCommandResult::InvalidArguments &&
				gCreateCount == createCountBeforeInvalid,
			"invalid add arguments do not create an actor"))
	{
		std::remove(profilePath);
		return 1;
	}
	if (!check(runtime.setCompatibilityFloat("bot_enable", 0.0f) == astrabot::compat::CvarUpdateResult::Updated &&
				runtime.executeCompatibilityCommand(request("bot_add")) == CompatibilityCommandResult::Disabled,
			"disabled configuration rejects bot_add"))
	{
		std::remove(profilePath);
		return 1;
	}
	if (!check(runtime.setCompatibilityFloat("bot_enable", 1.0f) == astrabot::compat::CvarUpdateResult::Updated &&
				runtime.setCompatibilityFloat("bot_stop", 1.0f) == astrabot::compat::CvarUpdateResult::Updated &&
				runtime.executeCompatibilityCommand(request("bot_add")) == CompatibilityCommandResult::Stopped,
			"stopped configuration rejects bot_add"))
	{
		std::remove(profilePath);
		return 1;
	}
	if (!check(runtime.setCompatibilityFloat("bot_stop", 0.0f) == astrabot::compat::CvarUpdateResult::Updated,
			"stop configuration is cleared"))
	{
		std::remove(profilePath);
		return 1;
	}

	gBotEnable = 1.0f;
	gEntities[0].v.flags |= FL_FAKECLIENT;
	globals.time = 2.0f;
	runtime.onStartFrame();
	const int createCountBeforeConflict = gCreateCount;
	if (!check(!runtime.snapshot().managedBotCreationAllowed &&
				runtime.executeCompatibilityCommand(request("bot_add")) ==
					CompatibilityCommandResult::NativeGuardDenied &&
				gCreateCount == createCountBeforeConflict,
			"native conflict denies bot_add before engine creation"))
	{
		std::remove(profilePath);
		return 1;
	}

	gEntities[0].v.flags &= ~FL_FAKECLIENT;
	gBotEnable = 1.0f;
	gBotQuota = 0.0f;
	globals.time = 3.0f;
	runtime.onStartFrame();
	// The mock allocator is monotonic; explicitly make the final released slot reusable.
	gCreateCount = 4;
	gEntities[4] = {};
	FakeClientHandle staleHandle{};
	if (!check(runtime.createFakeClient("stale", &staleHandle) == FakeClientResult::Created &&
				runtime.removeFakeClient(&staleHandle) == FakeClientResult::Removed &&
				runtime.removeFakeClient(&staleHandle) == FakeClientResult::NotFound,
			"stale and repeated handles are rejected"))
	{
		std::remove(profilePath);
		return 1;
	}
	if (!check(runtime.executeCompatibilityCommand(request("bot_add")) ==
					CompatibilityCommandResult::ActorOperationFailed &&
				gCreateCount == 5,
			"full actor slots report an operation failure"))
	{
		std::remove(profilePath);
		return 1;
	}

	const astrabot::runtime::LifecycleGeneration roundBeforeLifecycleLog = runtime.snapshot().roundGeneration;
	const float lifecycleLogStart = globals.time;
	char alertFormat[] = "%s";
	char restartRoundLog[] = "World triggered \"Restart_Round_(5_seconds)\"\n";
	requestedEngineFunctions.pfnAlertMessage(at_logged, alertFormat, restartRoundLog);
	const auto scheduledRound = runtime.snapshot();
	if (!check(scheduledRound.roundGeneration == roundBeforeLifecycleLog + 1U && !scheduledRound.roundFreezeActive,
			"logged round restart advances generation and schedules a future freeze"))
	{
		std::remove(profilePath);
		return 1;
	}
	globals.time = lifecycleLogStart + 4.9f;
	if (!check(
			!runtime.snapshot().roundFreezeActive, "scheduled freeze does not begin before the restart delay expires"))
	{
		std::remove(profilePath);
		return 1;
	}
	globals.time = lifecycleLogStart + 5.1f;
	if (!check(runtime.snapshot().roundFreezeActive, "scheduled freeze begins at the expected restart time"))
	{
		std::remove(profilePath);
		return 1;
	}
	globals.time = lifecycleLogStart + 10.9f;
	if (!check(runtime.snapshot().roundFreezeActive, "scheduled freeze remains active before its absolute deadline"))
	{
		std::remove(profilePath);
		return 1;
	}
	globals.time = lifecycleLogStart + 11.1f;
	if (!check(
			!runtime.snapshot().roundFreezeActive, "tracked freeze interval expires when its absolute deadline passes"))
	{
		std::remove(profilePath);
		return 1;
	}
	char immediateRestartLog[] = "World triggered \"Restart_Round_(0_seconds)\"\n";
	requestedEngineFunctions.pfnAlertMessage(at_logged, alertFormat, immediateRestartLog);
	const auto immediateRound = runtime.snapshot();
	if (!check(immediateRound.roundFreezeActive, "zero-delay restart starts the configured freeze period"))
	{
		std::remove(profilePath);
		return 1;
	}
	const astrabot::runtime::LifecycleGeneration activeRoundGeneration = immediateRound.roundGeneration;
	char roundStartedLog[] = "World triggered \"Round_Start\"\n";
	const float actualRoundStartTime = globals.time;
	globals.time = actualRoundStartTime + gMpFreezetime + 0.1f;
	requestedEngineFunctions.pfnAlertMessage(at_logged, alertFormat, roundStartedLog);
	if (!check(!runtime.snapshot().roundFreezeActive && runtime.snapshot().roundGeneration == activeRoundGeneration,
			"Round_Start ends the scheduled freeze without extending it"))
	{
		std::remove(profilePath);
		return 1;
	}
	globals.time = actualRoundStartTime + gMpFreezetime + 0.1f;
	if (!check(!runtime.snapshot().roundFreezeActive, "Round_Start freezetime expires at the configured boundary"))
	{
		std::remove(profilePath);
		return 1;
	}
	requestedEngineFunctions.pfnAlertMessage(at_logged, alertFormat, roundStartedLog);
	if (!check(runtime.snapshot().roundGeneration == activeRoundGeneration && !runtime.snapshot().roundFreezeActive,
			"duplicate Round_Start neither advances generation nor extends freezetime"))
	{
		std::remove(profilePath);
		return 1;
	}
	char roundEndedLog[] = "World triggered \"Round_End\"\n";
	gMpRoundRestartDelay = 5.0f;
	const float roundEndedAt = globals.time + 1.0f;
	globals.time = roundEndedAt;
	requestedEngineFunctions.pfnAlertMessage(at_logged, alertFormat, roundEndedLog);
	if (!check(!runtime.snapshot().roundFreezeActive, "Round_End leaves the restart delay unsuppressed"))
	{
		std::remove(profilePath);
		return 1;
	}
	globals.time = roundEndedAt + gMpRoundRestartDelay - 0.1f;
	if (!check(!runtime.snapshot().roundFreezeActive, "FreezeTime is not scheduled before mp_round_restart_delay"))
	{
		std::remove(profilePath);
		return 1;
	}
	globals.time = roundEndedAt + gMpRoundRestartDelay;
	if (!check(runtime.snapshot().roundFreezeActive, "FreezeTime starts after mp_round_restart_delay"))
	{
		std::remove(profilePath);
		return 1;
	}
	globals.time = roundEndedAt + gMpRoundRestartDelay + gMpFreezetime - 0.1f;
	if (!check(runtime.snapshot().roundFreezeActive, "FreezeTime remains active until the Round_Start boundary"))
	{
		std::remove(profilePath);
		return 1;
	}
	globals.time = roundEndedAt + gMpRoundRestartDelay + 2.0f;
	requestedEngineFunctions.pfnAlertMessage(at_logged, alertFormat, roundEndedLog);
	globals.time = roundEndedAt + gMpRoundRestartDelay + gMpFreezetime;
	if (!check(!runtime.snapshot().roundFreezeActive, "duplicate Round_End does not extend the freeze window"))
	{
		std::remove(profilePath);
		return 1;
	}
	requestedEngineFunctions.pfnAlertMessage(at_logged, alertFormat, roundStartedLog);
	if (!check(!runtime.snapshot().roundFreezeActive, "Round_Start ends the Round_End scheduled freeze window"))
	{
		std::remove(profilePath);
		return 1;
	}
	const astrabot::runtime::LifecycleGeneration roundEndedGeneration = runtime.snapshot().roundGeneration;
	const float commencingAt = globals.time;
	char gameCommencingLog[] = "World triggered \"Game_Commencing\"\n";
	requestedEngineFunctions.pfnAlertMessage(at_logged, alertFormat, gameCommencingLog);
	const auto commencingRound = runtime.snapshot();
	if (!check(commencingRound.roundGeneration == roundEndedGeneration + 1U && !commencingRound.roundFreezeActive,
			"Game_Commencing begins a round generation without starting freeze early"))
	{
		std::remove(profilePath);
		return 1;
	}
	requestedEngineFunctions.pfnAlertMessage(at_logged, alertFormat, roundEndedLog);
	if (!check(runtime.snapshot().roundGeneration == commencingRound.roundGeneration &&
				!runtime.snapshot().roundFreezeActive,
			"Game_Commencing Round_End preserves the initial three-second delay"))
	{
		std::remove(profilePath);
		return 1;
	}
	globals.time = commencingAt + 2.9f;
	if (!check(!runtime.snapshot().roundFreezeActive, "initial FreezeTime remains inactive before three seconds"))
	{
		std::remove(profilePath);
		return 1;
	}
	globals.time = commencingAt + 3.0f;
	if (!check(runtime.snapshot().roundFreezeActive,
			"initial FreezeTime begins after the Game_Commencing three-second delay"))
	{
		std::remove(profilePath);
		return 1;
	}
	globals.time = commencingAt + 3.5f;
	requestedEngineFunctions.pfnAlertMessage(at_logged, alertFormat, gameCommencingLog);
	requestedEngineFunctions.pfnAlertMessage(at_logged, alertFormat, roundEndedLog);
	if (!check(runtime.snapshot().roundGeneration == commencingRound.roundGeneration &&
				runtime.snapshot().roundFreezeActive,
			"duplicate initial lifecycle events preserve the active freeze"))
	{
		std::remove(profilePath);
		return 1;
	}
	globals.time = commencingAt + 3.0f + gMpFreezetime - 0.1f;
	if (!check(runtime.snapshot().roundFreezeActive, "initial FreezeTime retains the configured duration"))
	{
		std::remove(profilePath);
		return 1;
	}
	globals.time = commencingAt + 3.0f + gMpFreezetime;
	if (!check(!runtime.snapshot().roundFreezeActive,
			"duplicate initial lifecycle events do not extend the freeze deadline"))
	{
		std::remove(profilePath);
		return 1;
	}
	char unrelatedLog[] = "World triggered \"CTs_Win\"\n";
	requestedEngineFunctions.pfnAlertMessage(at_logged, alertFormat, unrelatedLog);
	if (!check(runtime.snapshot().roundGeneration == commencingRound.roundGeneration &&
				!runtime.snapshot().roundFreezeActive,
			"unrelated logged event leaves round freeze state unchanged"))
	{
		std::remove(profilePath);
		return 1;
	}
	requestedEngineFunctions.pfnAlertMessage(at_logged, alertFormat, roundStartedLog);
	if (!check(runtime.snapshot().roundGeneration == commencingRound.roundGeneration &&
				!runtime.snapshot().roundFreezeActive,
			"Round_Start after Game_Commencing does not start a new freeze interval"))
	{
		std::remove(profilePath);
		return 1;
	}

	if (!testRuntimeNearJumpFallback(runtime, engineFunctions, globals) ||
		!testRuntimeRecoveryReselection(runtime, engineFunctions, globals) ||
		!testRuntimeProfilerBoundaries(runtime, engineFunctions, globals) ||
		!testRuntimeBombSiteApproach(runtime, engineFunctions, globals, gameDllTable))
	{
		std::remove(profilePath);
		return 1;
	}
	if (!testRuntimeFriendBeyondPortal(runtime, engineFunctions, globals) ||
		!testRuntimeTouchAvoidance(runtime, engineFunctions, globals))
		return 1;

	// CS exposes human teams through TeamInfo; entvars.team may remain zero.
	runtime.executeCompatibilityCommand(request("bot_kick", 1U, "all"));
	resetEntities();
	gCreateCount = 0;
	gEntities[4].v.flags = FL_CLIENT;
	gEntities[4].v.team = 0;
	runtime.onClientPutInServer(&gEntities[4]);
	const auto humanTeamInfo = [&runtime](const char *team)
	{
		runtime.onMessageBegin(0, 86, nullptr, nullptr);
		runtime.onWriteByte(5);
		runtime.onWriteString(team);
		runtime.onMessageEnd();
	};
	humanTeamInfo("CT");
	setCvarFloat("bot_join_after_player", 1.0f);
	setCvarFloat("bot_enable", 1.0f);
	setCvarFloat("bot_quota", 1.0f);
	runtime.setCompatibilityFloat("bot_enable", 1.0f);
	runtime.setCompatibilityFloat("bot_quota", 1.0f);
	globals.time += 1.0f;
	runtime.onStartFrame();
	if (!check(gCreateCount == 1 && (gEntities[0].v.flags & FL_FAKECLIENT) != 0,
			"public human TeamInfo satisfies join-after-player when entvars team is zero"))
		return 1;
	gEntities[4].v.team = 2; // A stale engine value must not override a newer message.
	humanTeamInfo("SPECTATOR");
	globals.time += 1.0f;
	runtime.onStartFrame();
	if (!check(
			(gEntities[0].v.flags & FL_FAKECLIENT) == 0, "spectator TeamInfo no longer satisfies active-human quota"))
		return 1;
	humanTeamInfo("TERRORIST");
	runtime.onClientDisconnect(&gEntities[4]);
	gEntities[4].v.team = 0;
	runtime.onClientPutInServer(&gEntities[4]);
	globals.time += 1.0f;
	runtime.onStartFrame();
	if (!check(gCreateCount == 1 && (gEntities[0].v.flags & FL_FAKECLIENT) == 0,
			"reused human slot does not retain the disconnected team"))
		return 1;
	std::remove(profilePath);
	runtime.onServerDeactivate();
	if (!check(runtime.detach(PT_ANYTIME, PNL_NULL), "runtime detaches after actor command test"))
	{
		return 1;
	}
	return 0;
}
