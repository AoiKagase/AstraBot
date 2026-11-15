#include "runtime/shared/public_nav_probe.hpp"
#include <cstdio>
#include <cstring>
#include <vector>
namespace
{
using namespace astrabot;
struct Call
{
	float fromZ, toZ;
	int flags;
	edict_t *ignore;
};
std::vector<Call> calls;
edict_t brush{};
bool receipt = true, resetTrace = false, resetName = false;
unsigned nameCalls = 0;
enum class Mode
{
	Layers,
	Breakable,
	SolidBreakable,
	Clear
};
Mode mode = Mode::Layers;
const char *nameLookup(int)
{
	++nameCalls;
	if (resetName)
		receipt = false;
	return mode == Mode::Layers ? "func_door" : "func_breakable";
}
void trace(const float *from, const float *to, int flags, edict_t *ignore, TraceResult *out)
{
	const auto index = calls.size();
	calls.push_back({from[2], to[2], flags, ignore});
	*out = {};
	out->flFraction = mode == Mode::Clear ? 1.0f : .5f;
	out->vecEndPos = Vector(to[0], to[1], 0);
	if (mode == Mode::Layers)
	{
		if (index == 0)
		{
			out->pHit = &brush;
			out->vecEndPos.z = 90;
		}
		else if (index == 1)
		{
			out->fStartSolid = 1;
			out->vecEndPos.z = 10;
		}
		else if (index == 2)
			out->vecEndPos.z = 0;
		else if (index == 3)
			out->vecEndPos.z = 10;
		else if (index == 4)
			out->vecEndPos.z = 20;
		else
			out->vecEndPos.z = 60;
	}
	else if (mode == Mode::Breakable || mode == Mode::SolidBreakable)
	{
		out->pHit = &brush;
		out->vecEndPos.z = 90;
	}
	if (resetTrace)
		receipt = false;
}
bool check(bool ok, const char *text)
{
	if (!ok)
		std::fprintf(stderr, "check failed: %s\n", text);
	return ok;
}
void reset(Mode next)
{
	calls.clear();
	mode = next;
	receipt = true;
	resetTrace = resetName = false;
	nameCalls = 0;
	brush.free = false;
	brush.v.classname = 1;
}
}
int main()
{
	using namespace astrabot;
	enginefuncs_t engine{};
	engine.pfnTraceLine = &trace;
	engine.pfnSzFromIndex = &nameLookup;
	metamod::PublicNavProbe probe(&engine,
		[]
		{
			return receipt;
		});
	float ground = -1;
	reset(Mode::Layers);
	if (!check(probe.groundHeight({3, 4, 100}, &ground) && ground == 20 && calls.size() == 10,
			"public brush traces implement native increasing ground layers and headroom selection"))
		return 1;
	const float expectedEnd = 100.0f - 9999.9f;
	if (!check(calls.front().fromZ == 101 && calls.back().fromZ == 191 && calls.front().toZ == expectedEnd &&
				calls.front().flags == 1 && calls.front().ignore == nullptr && calls[1].ignore == &brush &&
				calls.back().ignore == &brush,
			"ground uses native offset/depth and ignores monsters while retaining glass and walk-through brush receipt"))
		return 1;
	reset(Mode::Breakable);
	brush.v.takedamage = DAMAGE_YES;
	if (!check(!probe.groundHeight({0, 0, 100}, &ground) && calls.size() == 10 && calls[1].ignore == &brush,
			"damageable breakables are not treated as solid ground"))
		return 1;
	reset(Mode::SolidBreakable);
	brush.v.takedamage = DAMAGE_NO;
	if (!check(probe.groundHeight({0, 0, 100}, &ground) && ground == 90 && calls.back().ignore == nullptr,
			"non-damageable breakable stays a ground surface"))
		return 1;
	reset(Mode::Clear);
	if (!check(probe.clearLine({0, 0, 36}, {100, 0, 36}) && calls.size() == 1 && calls[0].flags == 0x101 &&
				!calls[0].ignore,
			"nearest-area line of sight ignores monsters and glass"))
		return 1;
	reset(Mode::Layers);
	resetTrace = true;
	if (!check(!probe.groundHeight({0, 0, 100}, &ground) && calls.size() == 1 && nameCalls == 0,
			"owner invalidation in trace aborts before brush classification or another trace"))
		return 1;
	reset(Mode::Layers);
	resetName = true;
	if (!check(!probe.groundHeight({0, 0, 100}, &ground) && calls.size() == 1 && nameCalls == 1,
			"owner invalidation in public classname lookup cannot continue an old ground query"))
		return 1;
	metamod::PublicNavProbe absent(nullptr);
	if (!check(!absent.groundHeight({0, 0, 0}, &ground) && !absent.clearLine({0, 0, 0}, {1, 1, 1}),
			"missing public trace API fails closed"))
		return 1;
	return 0;
}
