// Deterministic host for unmodified pinned function bodies. NAV parent chains,
// flat portals and visibility are explicit inputs, not GameDLL observations.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <vector>
using vec_t = float;
using real_t = float;
#define Q_sqrt std::sqrt
#include "pinned-vector.h"
#define Q_memmove std::memmove
#define Q_memset std::memset
#define Q_min (std::min)
enum NavDirType { NORTH, EAST, SOUTH, WEST, NUM_DIRECTIONS };
enum NavTraverseType { GO_NORTH, GO_EAST, GO_SOUTH, GO_WEST, GO_LADDER_UP, GO_LADDER_DOWN, NUM_TRAVERSE_TYPES };
enum RouteType { FASTEST_ROUTE };
enum { LADDER_UP, LADDER_DOWN, dont_ignore_monsters, ignore_glass };
constexpr float HalfHumanHeight = 36, HalfHumanWidth = 16;
struct CNavArea;
struct CNavLadder
{
    CNavArea *m_topForwardArea = nullptr, *m_topLeftArea = nullptr, *m_topRightArea = nullptr, *m_bottomArea = nullptr;
    Vector m_bottom, m_top;
    NavDirType m_dir = EAST;
};
using NavLadderList = std::vector<CNavLadder *>;
struct CNavArea
{
    Vector center;
    CNavArea *parent = nullptr;
    float west = 0;
    NavLadderList ladders;
    const Vector *GetCenter() const { return &center; }
    CNavArea *GetParent() const { return parent; }
    NavTraverseType GetParentHow() const { return GO_EAST; }
    float GetZ(const Vector *) const { return 0; }
    bool IsConnected(CNavArea *, NavDirType) const { return true; }
    const NavLadderList *GetLadderList(int) const { return &ladders; }
    void ComputeClosestPointInPortal(CNavArea *to, NavDirType, const Vector *from, Vector *out) const
    { *out = Vector(to->west, from->y, 0); }
};
void AddDirectionVector(Vector *point, NavDirType dir, float distance)
{
    if (dir == EAST) point->x += distance;
    else if (dir == WEST) point->x -= distance;
    else if (dir == NORTH) point->y -= distance;
    else point->y += distance;
}
void DirectionToVector2D(NavDirType dir, Vector2D *out)
{
    *out = Vector2D(dir == EAST ? 1.0f : dir == WEST ? -1.0f : 0.0f,
                    dir == SOUTH ? 1.0f : dir == NORTH ? -1.0f : 0.0f);
}
NavDirType OppositeDirection(NavDirType dir) { return static_cast<NavDirType>((dir + 2) % 4); }
struct Globals { float time = 1; } globals;
Globals *gpGlobals = &globals;
struct Vars { Vector origin; };
#define ENT(value) (value)
struct TraceResult { float flFraction; Vector vecEndPos; };
unsigned wallTraces = 0;
void UTIL_TraceLine(const Vector &, const Vector &end, int, int, Vars *, TraceResult *out)
{ ++wallTraces; out->flFraction = 1; out->vecEndPos = end; }
float RANDOM_FLOAT(float lower, float) { return lower; }
struct Timer { bool IsElapsed() const { return true; } void Start(float) {} };
struct PathCost { PathCost(void *, RouteType) {} };
bool NavAreaBuildPath(CNavArea *, CNavArea *goal, const Vector *, PathCost &, CNavArea **closest)
{ *closest = goal; return true; }

struct CCSBot
{
    enum { MAX_PATH_LENGTH = 256 };
    struct ConnectInfo { CNavArea *area = nullptr; NavTraverseType how = GO_EAST; Vector pos; const CNavLadder *ladder = nullptr; };
    // Substitute only indexed access so the max-length case records the attempted
    // out-of-array read without executing undefined behavior in this audit host.
    struct CheckedPath
    {
        std::array<ConnectInfo, MAX_PATH_LENGTH> slots;
        int lastRead = -1;
        ConnectInfo &operator[](int index)
        {
            lastRead = index;
            if (index < 0 || index >= MAX_PATH_LENGTH) throw std::out_of_range("path slot");
            return slots[static_cast<std::size_t>(index)];
        }
    } m_path;
    int m_pathLength = 0, m_pathIndex = 0;
    const CNavLadder *m_pathLadder = nullptr;
    CNavArea *m_lastKnownArea = nullptr;
    float m_areaEnteredTimestamp = 0;
    void *m_spotEncounter = nullptr;
    Vector m_goalPosition;
    Timer m_repathTimer;
    Vars vars;
    Vars *pev = &vars;
    float visibilityLimit = 1.0e9f;
    unsigned visibilityCalls = 0;
    bool HasPath() const;
    void DestroyPath();
    bool ComputePathPositions();
    void BuildTrivialPath(const Vector *goal);
    bool ComputePath(CNavArea *goalArea, const Vector *goal, RouteType route);
    void RemovePath(CNavArea *area);
    bool FindGrenadeTossPathTarget(Vector *pos);
    bool FVisible(const Vector &point) { ++visibilityCalls; return point.x < visibilityLimit; }
    void GetGroundHeight(Vector *, float *out) { *out = 0; }
    void SetupLadderMovement() { std::abort(); }
    void PrintIfWatched(const char *) {}
};
#include "pinned-functions.inc"

void require(bool condition, const char *message)
{
    if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
void report(const char *name, CCSBot &bot, bool expected, Vector target, bool trapped = false)
{
    std::printf("{\"case\":\"%s\",\"accepted\":%s,\"target\":[%.9g,%.9g,%.9g],\"length\":%d,\"next\":%d,\"last_index\":%d,\"visibility_calls\":%u,\"wall_traces\":%u,\"trapped\":%s}\n",
        name, expected ? "true" : "false", target.x, target.y, target.z, bot.m_pathLength, bot.m_pathIndex,
        bot.m_path.lastRead, bot.visibilityCalls, wallTraces, trapped ? "true" : "false");
}
void sample(const char *name, CCSBot &bot, bool expected, Vector expectedTarget)
{
    Vector target(-999, -999, -999);
    wallTraces = bot.visibilityCalls = 0;
    const bool accepted = bot.FindGrenadeTossPathTarget(&target);
    require(accepted == expected, name);
    require(target == expectedTarget, name);
    require(wallTraces == (expected ? 4U : 0U), name);
    report(name, bot, accepted, target);
}
void trivial(CCSBot &bot, CNavArea &area, float goal)
{
    bot.m_lastKnownArea = &area;
    bot.vars.origin = Vector(100, 0, 0);
    const Vector end(goal, 0, 0);
    require(bot.ComputePath(&area, &end, FASTEST_ROUTE), "real ComputePath trivial branch");
    require(bot.m_pathLength == 2 && bot.m_pathIndex == 1, "trivial path counters");
}
void longPath(CCSBot &bot, std::array<CNavArea, 3> &areas)
{
    for (unsigned i = 0; i < areas.size(); ++i)
    {
        areas[i].center = Vector(100.0f + 200.0f * i, 0, 0);
        areas[i].west = 200.0f * i;
        areas[i].parent = i == 0 ? nullptr : &areas[i - 1];
    }
    bot.m_lastKnownArea = &areas[0];
    bot.vars.origin = Vector(100, 0, 0);
    const Vector end(550, 0, 0);
    require(bot.ComputePath(&areas[2], &end, FASTEST_ROUTE), "real ComputePath parent chain");
    require(bot.m_pathLength == 4 && bot.m_path[2].pos == Vector(405, 0, 0), "native portal writes prior slot 2");
}
int main()
{
    const Vector untouched(-999, -999, -999);
    CNavArea area;
    area.center = Vector(100, 0, 0);
    CCSBot empty;
    sample("no-path", empty, false, untouched);
    CCSBot fresh;
    trivial(fresh, area, 200);
    require(fresh.m_path[2].pos == Vector(0, 0, 0), "real Vector constructor zeros unused positions");
    sample("fresh-trivial", fresh, true, Vector(25, 0, 46));
    CCSBot multi;
    std::array<CNavArea, 3> areas;
    longPath(multi, areas);
    sample("fresh-multi", multi, true, Vector(25, 0, 46));
    CCSBot reused;
    longPath(reused, areas);
    trivial(reused, area, 200);
    require(reused.m_path[2].pos == Vector(405, 0, 0), "shortening preserves prior native portal");
    sample("reused-trivial", reused, true, Vector(400, 0, 46));
    reused.DestroyPath();
    require(reused.m_path[2].pos == Vector(405, 0, 0), "DestroyPath preserves positions");
    trivial(reused, area, 200);
    sample("destroy-rebuild", reused, true, Vector(400, 0, 46));
    trivial(reused, area, 405);
    sample("equal-tail", reused, true, Vector(405, 0, 10));
    trivial(reused, area, 200);
    reused.visibilityLimit = 150;
    sample("hidden-first", reused, false, untouched);
    reused.visibilityLimit = 300;
    sample("hidden-tail-segment", reused, true, Vector(275, 0, 46));
    CCSBot removed;
    longPath(removed, areas);
    removed.RemovePath(&areas[1]);
    require(removed.m_pathLength == 3 && removed.m_path[3].pos == Vector(0, 0, 0), "RemovePath zeros new tail");
    sample("remove-clears-tail", removed, true, Vector(25, 0, 46));
    CCSBot maximum;
    std::array<CNavArea, 255> maxAreas;
    for (unsigned i = 0; i < maxAreas.size(); ++i)
    {
        maxAreas[i].center = Vector(100.0f + 200.0f * i, 0, 0);
        maxAreas[i].west = 200.0f * i;
        maxAreas[i].parent = i == 0 ? nullptr : &maxAreas[i - 1];
    }
    maximum.m_lastKnownArea = &maxAreas[0];
    const Vector maxEnd(51000, 0, 0);
    require(maximum.ComputePath(&maxAreas.back(), &maxEnd, FASTEST_ROUTE) && maximum.m_pathLength == 256,
        "real ComputePath fills all 256 slots including endpoint");
    wallTraces = 0;
    Vector target = untouched;
    bool trapped = false;
    try { maximum.FindGrenadeTossPathTarget(&target); }
    catch (const std::out_of_range &) { trapped = true; }
    require(trapped && maximum.m_path.lastRead == 256 && target == untouched && wallTraces == 0,
        "all-visible maximum path attempts out-of-array access before wall traces");
    report("max-all-visible", maximum, false, target, trapped);
    maximum.visibilityLimit = 0;
    sample("max-first-hidden", maximum, false, untouched);
    return 0;
}
