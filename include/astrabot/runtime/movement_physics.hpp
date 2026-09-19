#ifndef ASTRABOT_RUNTIME_MOVEMENT_PHYSICS_HPP
#define ASTRABOT_RUNTIME_MOVEMENT_PHYSICS_HPP

#include "astrabot/runtime/actor_registry.hpp"
#include "astrabot/world/world_snapshot.hpp"

#include <cstdint>

namespace astrabot
{
	namespace runtime
	{
		struct PhysicsVector
		{
			float x;
			float y;
			float z;
		};

		struct MovementPhysicsState
		{
			PhysicsVector origin;
			PhysicsVector velocity;
			bool entityValid;
			bool fakeClient;
			bool spectator;
			bool dead;
			bool grounded;
			bool ducked;
			bool onLadder;
			std::uint32_t groundEntityIndex;
			int flags;
	int deadflag;
	int team;
	bool teamConfirmed;
	int solid;
			int movetype;
			float health;
		};

		enum class SpawnReadiness : std::uint8_t
		{
			NotReady,
			Ready
		};

		struct MovementPhysicsSample
		{
			ActorId actor;
			world::FrameIdentity frame;
			std::uint32_t commandSequence;
			MovementPhysicsState before;
			MovementPhysicsState after;
			bool dispatched;
			SpawnReadiness readiness;
		};

		SpawnReadiness spawnReadiness(const MovementPhysicsState &state) noexcept;
	}
}

#endif
