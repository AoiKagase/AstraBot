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

struct LandingDamageObservation
{
	bool landingConfirmed;
	bool hasLandingDamage;
	float landingDamage;
};

class FallLandingTracker
{
public:
	LandingDamageObservation observe(
		std::uint32_t actorGeneration,
		const MovementPhysicsState &state) noexcept;
	void reset() noexcept;

private:
	std::uint32_t actorGeneration_ = 0U;
	bool hasActorGeneration_ = false;
	bool airborne_ = false;
	bool hasAirborneHealth_ = false;
	float airborneHealth_ = 0.0f;
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

		// Continuing movement eligibility does not require contact with the ground.
		SpawnReadiness movementReadiness(const MovementPhysicsState &state) noexcept;
		SpawnReadiness spawnReadiness(const MovementPhysicsState &state) noexcept;
		bool isMovementDirectionReversal(
			const PhysicsVector &previous,
			const PhysicsVector &current) noexcept;
	}
}

#endif
