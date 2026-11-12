#ifndef ASTRABOT_RUNTIME_PLAYER_AVOIDANCE_HPP
#define ASTRABOT_RUNTIME_PLAYER_AVOIDANCE_HPP

#include "astrabot/runtime/actor_registry.hpp"
#include "astrabot/world/world_snapshot.hpp"

#include <cstdint>
#include <cmath>
#include <limits>

namespace astrabot
{
namespace runtime
{
// ReGameDLL cs_bot_manager.cpp GetPlayerPriority: smaller values rank first.
// Ordinary bot IDs are monotonic creation IDs, not player entity slots.
inline std::uint32_t playerAvoidancePriority(bool human, bool scenarioImportant, std::uint32_t botId)
{
	if (human) return 0U;
	if (scenarioImportant) return 1U;
	if (botId == 0U || botId == std::numeric_limits<std::uint32_t>::max())
		return std::numeric_limits<std::uint32_t>::max();
	return 1U + botId;
}

struct PlayerAvoidanceTouch
{
	ActorId actor;
	world::FrameIdentity frame;
	ActorId other;
	std::uint32_t selfPriority;
	std::uint32_t otherPriority;
	float now;
	bool defusing;
	// BotTouch recomputes its incumbent's priority (for example after a C4 drop).
	std::uint32_t currentTargetPriority = std::numeric_limits<std::uint32_t>::max();
	bool currentTargetPriorityKnown = false;
};

struct PlayerAvoidanceMove
{
	float forward;
	float side;
	bool valid;
};

// REFERENCE.md freezes a REGAMEDLL_FIXES build: make-way can replace
// forward/back input as well as lateral input, using a 0.5 projection cutoff.
inline PlayerAvoidanceMove playerAvoidanceMove(float yawDegrees, float otherX,
	float otherY, float forward, float side, float speed = 240.0f)
{
	PlayerAvoidanceMove result{forward,side,false};
	const float distance=std::hypot(otherX,otherY);
	if (!std::isfinite(yawDegrees) || !std::isfinite(distance) || distance<=0.0f ||
		!std::isfinite(forward) || !std::isfinite(side) || !std::isfinite(speed) || speed<=0.0f) return result;
	const float yaw=yawDegrees*0.017453292519943295f;
	const float along=(otherX*std::cos(yaw)+otherY*std::sin(yaw))/distance;
	const float across=(-otherX*std::sin(yaw)+otherY*std::cos(yaw))/distance;
	if (along>0.5f) result.forward=-speed;
	else if (along< -0.5f) result.forward=speed;
	if (across>=0.5f) result.side=speed;
	else if (across<= -0.5f) result.side=-speed;
	result.valid=true;
	return result;
}

class PlayerAvoidance
{
public:
	bool touch(const PlayerAvoidanceTouch &contact)
	{
		if (!validActor(contact.actor) || !validActor(contact.other) ||
			contact.frame.mapGeneration == 0U || contact.frame.roundGeneration == 0U ||
			!std::isfinite(contact.now) || sameActor(contact.actor, contact.other)) return false;
		// The upstream defusing early return leaves existing avoidance untouched.
		if (contact.defusing) return false;
		refresh(contact.actor, contact.frame, contact.now);
		if (contact.selfPriority < contact.otherPriority) return false;
		if (active_)
		{
			if (contact.currentTargetPriorityKnown) priority_ = contact.currentTargetPriority;
			if (priority_ < contact.otherPriority) return false;
		}
		owner_ = contact.actor;
		frame_ = contact.frame;
		other_ = contact.other;
		priority_ = contact.otherPriority;
		contactAt_ = contact.now;
		active_ = true;
		return true;
	}

	// Resolve the returned generation-safe identity to its current public origin
	// at the adapter; retaining a contact position would diverge from m_avoid.
	bool target(const ActorId &actor, const world::FrameIdentity &frame, float now, ActorId *other)
	{
		refresh(actor, frame, now);
		if (!active_ || other == nullptr) return false;
		*other = other_;
		return true;
	}

	void reset()
	{
		active_ = false;
		owner_ = {};
		frame_ = {};
		other_ = {};
		priority_ = 0U;
		contactAt_ = 0.0f;
	}

private:
	static bool validActor(const ActorId &actor)
	{
		return actor.slot != 0U && actor.actorGeneration != 0U;
	}
	static bool sameActor(const ActorId &a, const ActorId &b)
	{
		return a.slot == b.slot && a.actorGeneration == b.actorGeneration;
	}
	void refresh(const ActorId &actor, const world::FrameIdentity &frame, float now)
	{
		if (active_ && (!sameActor(actor, owner_) || frame.mapGeneration != frame_.mapGeneration ||
			frame.roundGeneration != frame_.roundGeneration || !std::isfinite(now) ||
			now < contactAt_ || now - contactAt_ >= 0.33f)) reset();
	}

	ActorId owner_{};
	world::FrameIdentity frame_{};
	ActorId other_{};
	std::uint32_t priority_ = 0U;
	float contactAt_ = 0.0f;
	bool active_ = false;
};
}
}

#endif
