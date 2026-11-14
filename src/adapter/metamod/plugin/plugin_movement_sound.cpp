#include "plugin/plugin_runtime.hpp"
#include "runtime/shared/bot_runtime_support.hpp"
#include <cmath>

namespace astrabot
{
namespace metamod
{
void PluginRuntime::onPmMove(playermove_t *move, int server)
{
	constexpr int kMaximumClientSlots = 32;
	edict_t *source = nullptr;
	if (state_ == State::ActiveMap && lifecycle_.isMapActive() && server != FALSE && move && globals_ &&
		engineFunctions_ && engineFunctions_->pfnPEntityOfEntIndex && move->player_index >= 0 &&
		move->player_index < globals_->maxClients && move->player_index < kMaximumClientSlots &&
		std::isfinite(globals_->time))
	{
		source = engineFunctions_->pfnPEntityOfEntIndex(move->player_index + 1);
		if (source && (source->free || source->serialnumber < 0 || (source->v.flags & FL_CLIENT) == 0 ||
			(source->v.flags & FL_SPECTATOR) != 0 || source->v.deadflag != DEAD_NO || source->v.health <= 0))
		{
			source = nullptr;
		}
	}
	movementSounds_.begin(move, source, lifecycle_.mapGeneration(), lifecycle_.roundGeneration(),
		server != FALSE, &HookMovementSound, globals_ ? globals_->time : 0);
}

void PluginRuntime::onPmMovePost(playermove_t *move, int server)
{
	(void)server;
	movementSounds_.end(move);
}

bool PluginRuntime::movementSoundOrigin(const edict_t *source, world::WorldVector *origin) const
{
	const auto *active = movementSounds_.current();
	if (!active || !active->eligible || !source || !origin || active->source != source || !active->move ||
		!globals_ || !engineFunctions_ || !engineFunctions_->pfnPEntityOfEntIndex)
	{
		return false;
	}
	const auto frame = *active;
	const auto *move = frame.move;
	if (state_ != State::ActiveMap || !lifecycle_.isMapActive() || frame.map != lifecycle_.mapGeneration() ||
		frame.round != lifecycle_.roundGeneration() || !std::isfinite(globals_->time) ||
		globals_->time < frame.seconds || frame.playerIndex < 0 || frame.playerIndex >= globals_->maxClients ||
		frame.playerIndex >= 32 || move->player_index != frame.playerIndex || !move->server ||
		!move->runfuncs || move->dead || move->deadflag != DEAD_NO || move->spectator)
	{
		return false;
	}
	const auto *current = engineFunctions_->pfnPEntityOfEntIndex(frame.playerIndex + 1);
	active = movementSounds_.current();
	if (!active || !active->eligible || active->id != frame.id || frame.map != lifecycle_.mapGeneration() ||
		frame.round != lifecycle_.roundGeneration() || current != source || source->free ||
		source->serialnumber != frame.serial || (source->v.flags & FL_CLIENT) == 0 ||
		(source->v.flags & FL_SPECTATOR) != 0 || source->v.deadflag != DEAD_NO || source->v.health <= 0)
	{
		return false;
	}
	const world::WorldVector position{move->origin[0], move->origin[1], move->origin[2]};
	constexpr float kMaximumCoordinate = world::WorldLimits::kMaximumCoordinate;
	if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z) ||
		std::fabs(position.x) > kMaximumCoordinate || std::fabs(position.y) > kMaximumCoordinate ||
		std::fabs(position.z) > kMaximumCoordinate)
	{
		return false;
	}
	*origin = position;
	return true;
}

void PluginRuntime::onMovementSound(int channel, const char *sample, float volume, float attenuation, int flags, int pitch)
{
	constexpr int kStopSoundFlag = 1 << 5;
	const auto *frame = movementSounds_.capture();
	if (frame && (flags & kStopSoundFlag) == 0 && compat::isStandardFootstepSample(sample))
	{
		auto *source = frame->source;
		world::WorldVector origin{};
		if (movementSoundOrigin(source, &origin))
		{
			observation_.onEmitSoundAt(source, sample, volume, attenuation, origin);
		}
	}
	movementSounds_.forward(channel, sample, volume, attenuation, flags, pitch);
}

void HookPmMove(playermove_t *move, int server)
{
	PluginRuntime::instance().onPmMove(move, server);
	runtime_detail::setMetaResult(MRES_IGNORED);
}

void HookPmMovePost(playermove_t *move, int server)
{
	PluginRuntime::instance().onPmMovePost(move, server);
	runtime_detail::setMetaResult(MRES_IGNORED);
}

void HookMovementSound(int channel, const char *sample, float volume, float attenuation, int flags, int pitch)
{
	PluginRuntime::instance().onMovementSound(channel, sample, volume, attenuation, flags, pitch);
}
}
}
