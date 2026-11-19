#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

// Host shims for the pinned bodies. This fixture supplies one flat NAV area,
// a stationary bot, no teammates, and an already classified audible event.
struct Vector
{
	float x = 0, y = 0, z = 0;
	Vector operator-(const Vector &other) const { return {x - other.x, y - other.y, z - other.z}; }
	float Length() const { return std::sqrt(x * x + y * y + z * z); }
	float LengthSquared() const { return x * x + y * y + z * z; }
	bool IsLengthGreaterThan(float distance) const { return LengthSquared() > distance * distance; }
};
struct PlayerVars { Vector origin; const char *netname = "source"; };
struct Player { PlayerVars *pev; };
struct Globals { float time = 0; };
Globals replayGlobals;
Globals *gpGlobals = &replayGlobals;
struct Profile
{
	float GetAggression() const { return .5f; }
	float GetReactionTime() const { return .125f; }
};
struct Manager { float GetElapsedRoundTime() const { return gpGlobals->time; } };
Manager manager;
Manager *TheCSBots() { return &manager; }
struct Debug { float value = 0; } cv_bot_debug;
enum { EVENT_WEAPON_FIRED, EVENT_PLAYER_FOOTSTEP, ENGAGE_AND_INVESTIGATE, SAFEST_ROUTE };
enum PriorityType { PRIORITY_LOW = 1, PRIORITY_MEDIUM, PRIORITY_HIGH };
constexpr float HalfHumanHeight = 36.0f;
struct Area
{
	void GetClosestPointOnArea(const Vector *point, Vector *result) { *result = {point->x, point->y, 0}; }
};
using CNavArea = Area;
struct Grid
{
	Area area;
	Area *GetNavArea(const Vector *) { return &area; }
	Area *GetNearestNavArea(const Vector *) { std::abort(); }
} TheNavAreaGrid;
struct Chatter { void HeardNoise(const Vector *) { std::abort(); } };

struct CCSBot
{
	PlayerVars vars;
	PlayerVars *pev = &vars;
	Profile profile;
	float m_safeTime = 17.5f;
	float m_noiseTimestamp = 0, m_noiseCheckTimestamp = 0;
	bool m_isNoiseTravelRangeChecked = false;
	Vector m_noisePosition;
	Area *m_noiseArea = nullptr;
	Chatter chatter;
	PriorityType m_noisePriority = PRIORITY_LOW;
	bool IsWellPastSafe() const;
	bool IsNoiseHeard() const;
	void ForgetNoise();
	bool ShouldInvestigateNoise(float *);
	bool CanHearNearbyEnemyGunfire(float range) const;
	void InhibitLookAround(float duration);
	void replayStartle();
	void deliver(Player *pPlayer, int event, PriorityType priority, float range);
	int GetDisposition() const { return ENGAGE_AND_INVESTIGATE; }
	bool IsAtHidingSpot() const { return false; }
	int GetFriendsRemaining() const { return 0; }
	bool IsLocalPlayerWatchingMe() const { return false; }
	bool IsUsingSniperRifle() const { return false; }
	bool IsEnemyVisible() const { return false; }
	bool IsAttacking() const { return false; }
	bool CanSeeNoisePosition() const { return clearLine; }
	void SetLookAt(const char *, const Vector *, PriorityType, float) { lookRequested = true; }
	void EquipPistol() { std::abort(); }
	void PrintIfWatched(const char *, ...) {}
	const Profile *GetProfile() const { return &profile; }
	const Vector *GetNoisePosition() const { return &m_noisePosition; }
	PriorityType GetNoisePriority() const { return m_noisePriority; }
	bool HasNotSeenEnemyForLongTime() const { return false; }
	Chatter *GetChatter() { return &chatter; }
	Area *GetNoiseArea() const { return m_noiseArea; }
	void ComputePath(Area *, const Vector *, int) {}
	Area *m_lastKnownArea = nullptr;
	Player *m_enemy = nullptr;
	bool clearLine = false, lookRequested = false;
	float m_inhibitLookAroundTimestamp = 0;
};
struct InvestigateNoiseState { Vector m_checkNoisePosition; void AttendCurrentNoise(CCSBot *me); };
struct PathCost { explicit PathCost(CCSBot *) {} };
float NavAreaTravelDistance(Area *, Area *, const PathCost &) { std::abort(); }
float Q_abs(float x) { return std::fabs(x); }
const char *STRING(const char *value) { return value; }

enum class Step { Intake, Decision, Look };
Step step;
unsigned positionDraws;
float decisionDraw;
float RANDOM_FLOAT(float lower, float upper)
{
	const char *site = step == Step::Look ? "NoiseLook.StartleInhibit" : step == Step::Decision ?
		"noise.investigate" : positionDraws++ == 0 ? "noise.position.x" : "noise.position.y";
	const float value = step == Step::Intake ? 0 : decisionDraw;
	std::printf("rng %s %.9g %.9g %.9g\n", site, lower, upper, value);
	return value;
}

#include "csbot_noise_reference.inc"

int main()
{
	CCSBot bot;
	CCSBot lookBot;
	InvestigateNoiseState investigate;
	float time, x;
	int event, priority;
	unsigned tick = 0;
	for (std::string row; std::getline(std::cin, row);)
	{
		if (row.empty() || row[0] == '#')
			continue;
		if (row[0] == 'L')
		{
			int visible;
			if (!(std::istringstream(row.substr(1)) >> time >> x >> visible >> decisionDraw))
				return 2;
			gpGlobals->time = time;
			std::printf("tick %u %.9g\n", ++tick, time);
			std::printf("look-input %.9g %d\n", x, visible);
			step = Step::Look;
			lookBot.m_noisePosition = {x, 0, 0};
			lookBot.m_noiseTimestamp = time;
			lookBot.m_noisePriority = PRIORITY_HIGH;
			lookBot.clearLine = visible != 0;
			lookBot.lookRequested = false;
			lookBot.replayStartle();
			std::printf("look %d %.9g\n", lookBot.lookRequested, lookBot.m_inhibitLookAroundTimestamp);
			continue;
		}
		if (!(std::istringstream(row) >> time >> event >> x >> priority >> decisionDraw))
			return 2;
		gpGlobals->time = time;
		std::printf("tick %u %.9g\n", ++tick, time);
		step = Step::Intake;
		positionDraws = 0;
		PlayerVars source{{x, 0, 0}};
		Player player{&source};
		if (event)
		{
			std::printf("event %.9g %d\n", x, priority);
			bot.deliver(&player, EVENT_WEAPON_FIRED, static_cast<PriorityType>(priority), 2000);
		}
		const bool active = bot.m_noiseTimestamp > 0;
		std::printf("memory %d %d %.9g %.9g %d\n", active, bot.IsNoiseHeard(),
			active ? bot.m_noiseTimestamp : 0, active ? bot.m_noisePosition.x : 0,
			active ? static_cast<int>(bot.GetNoisePriority()) : 0);
		step = Step::Decision;
		const bool attend = bot.ShouldInvestigateNoise(nullptr);
		std::printf("decision %d\n", attend);
		if (attend)
		{
			investigate.AttendCurrentNoise(&bot);
			std::printf("consume %d\n", bot.m_noiseTimestamp <= 0);
		}
	}
}
