#ifndef ASTRABOT_ADAPTER_BOT_LOOK_GEOMETRY_HPP
#define ASTRABOT_ADAPTER_BOT_LOOK_GEOMETRY_HPP
#include "runtime/shared/bot_runtime_types.hpp"
#include <functional>
namespace astrabot
{
namespace metamod
{
bool bendLookApproach(
	enginefuncs_t &engine, edict_t *self, const nav::NavVector &eye, const nav::NavVector &point, nav::NavVector *bend,
	const std::function<bool()> *current = nullptr);
}
}
#endif
