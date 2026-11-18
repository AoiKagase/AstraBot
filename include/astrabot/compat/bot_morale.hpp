#pragma once

#include <algorithm>

namespace astrabot
{
namespace compat
{
struct BotMorale
{
	int value = 1;
	bool diedSinceSpawn = false;

	void roundResult(bool won)
	{
		value = (std::max)(-3, (std::min)(3, value + (won ? 1 : -1)));
	}
	void observeLife(bool alive)
	{
		if (alive && diedSinceSpawn)
			roundResult(false);
		diedSinceSpawn = !alive;
	}
	void newMatch() { value = 1; }
};
}
}
