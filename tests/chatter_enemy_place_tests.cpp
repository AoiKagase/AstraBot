#include "astrabot/compat/chatter_enemy_place.hpp"
#include <cstdio>
#include <limits>
int main()
{
	using namespace astrabot::compat;
	const auto check = [](bool value, const char *text)
	{
		if (!value)
			std::fprintf(stderr, "check failed: %s\n", text);
		return value;
	};
	const ChatterVisiblePlace dominant[] = {{7, 1}, {9, 3}, {9, 2}};
	if (!check(chooseChatterEnemyPlace(dominant, 3) == 9,
			"most current visible threats wins over nearest selected enemy Place"))
		return 1;
	const ChatterVisiblePlace tie[] = {{7, 1}, {7, 4}, {9, 2}, {9, 3}};
	if (!check(
			chooseChatterEnemyPlace(tie, 4) == 9, "stable distance order retains first leader on final equal counts"))
		return 1;
	const ChatterVisiblePlace sameRange[] = {{7, 1}, {9, 1}, {9, 1}, {7, 1}};
	if (!check(chooseChatterEnemyPlace(sameRange, 4) == 9,
			"equal distances preserve input slot order during online leader count"))
		return 1;
	const ChatterVisiblePlace absent[] = {{0, 1}, {7, -1}, {9, (std::numeric_limits<float>::quiet_NaN)()}};
	if (!check(!chooseChatterEnemyPlace(absent, 3) && !chooseChatterEnemyPlace(nullptr, 3),
			"undefined Place and invalid range supply no enemy location"))
		return 1;
	ChatterKnownArea knowledge;
	const astrabot::world::ActorKey owner{2, 3};
	const astrabot::world::FrameIdentity frame{4, 5, 6};
	knowledge.observe(owner, frame, 7, 99);
	if (!check(knowledge.read(owner, frame, 7) == 99 && knowledge.current(owner, frame, 7) == 99,
			"managed last known public NAV area is retained"))
		return 1;
	auto next = frame;
	++next.tick;
	knowledge.observe(owner, next, 7, 0);
	if (!check(knowledge.read(owner, next, 7) == 99 && !knowledge.current(owner, next, 7),
			"off-mesh update preserves last known area"))
		return 1;
	if (!check(!knowledge.read(owner, next, 8), "NAV document replacement cannot reuse an old area ID"))
		return 1;
	auto other = owner;
	++other.generation;
	if (!check(!knowledge.read(other, next, 7) && !knowledge.read(owner, frame, 7),
			"another actor or earlier frame cannot borrow NAV area"))
		return 1;
	knowledge.observe(owner, next, 8, 0);
	if (!check(!knowledge.read(owner, next, 8), "new NAV revision clears old held area before a first match"))
		return 1;
	knowledge.observe(owner, next, 8, 101);
	knowledge.reset();
	if (!check(!knowledge.read(owner, next, 8), "explicit lifecycle reset clears last known NAV area"))
		return 1;
	return 0;
}
