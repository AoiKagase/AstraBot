#ifndef ASTRABOT_COMBAT_COMBAT_INTENT_HPP
#define ASTRABOT_COMBAT_COMBAT_INTENT_HPP

#include "astrabot/combat/weapon_state.hpp"

#include <cstdint>

namespace astrabot
{
namespace combat
{
enum class TargetBeliefState
{
	Unknown,
	Visible,
	Remembered,
	Unavailable
};

struct TargetBelief
{
	world::ActorKey target;
	TargetBeliefState state;
	world::WorldPosition position;
	float confidence;
	std::uint32_t ageTicks;
	bool friendlyFireRisk;

	bool isValid() const;
	bool isUsable() const;
};

struct CombatDecisionContext
{
	world::ActorKey actor;
	world::FrameIdentity frame;
	world::WorldPosition origin;

	bool isValid() const;
};

struct AimIntent
{
	world::ActorKey actor;
	world::FrameIdentity frame;
	world::ActorKey target;
	world::WorldPosition point;
	float pitch;
	float yaw;

	bool isValid() const;
	bool isIntent() const;
	bool isDispatchReceipt() const;
};

struct FireIntent
{
	world::ActorKey actor;
	world::FrameIdentity frame;
	world::ActorKey target;
	WeaponId weapon;

	bool isValid() const;
	bool isIntent() const;
	bool isDamageConfirmation() const;
};

struct ReloadIntent
{
	world::ActorKey actor;
	world::FrameIdentity frame;
	WeaponId weapon;

	bool isValid() const;
	bool isIntent() const;
	bool isAmmoConfirmation() const;
};

struct CombatDecision
{
	world::ActorKey actor;
	world::FrameIdentity frame;
	bool hasAim;
	AimIntent aim;
	bool hasFire;
	FireIntent fire;
	bool hasReload;
	ReloadIntent reload;
};

enum class DamageObservationState
{
	Unknown,
	Observed,
	Unavailable
};

struct DamageObservation
{
	world::ActorKey actor;
	world::ActorKey target;
	world::FrameIdentity frame;
	DamageObservationState state;
	float amount;
	bool targetDied;

	bool isValid() const;
	bool isConfirmed() const;
	bool isConfirmedDeath() const;
};

enum class CombatResult
{
	FireIntentReady,
	ReloadIntentReady,
	NoAction,
	DamageObserved,
	DeathObserved,
	InvalidArgument,
	InvalidConfig,
	InvalidSnapshot,
	InvalidObservation,
	InvalidTarget,
	FriendlyFireRisk,
	StaleGeneration,
	ActorGenerationChanged,
	DuplicateFrame,
	StaleFrame
};

struct CombatConfig
{
	std::uint32_t maximumTargetAgeTicks;
	float minimumTargetConfidence;

	CombatConfig();
	bool isValid() const;
};

class CombatController
{
public:
	CombatController();
	explicit CombatController(const world::ActorKey &actor);
	CombatController(
		const world::ActorKey &actor,
		const CombatConfig &config);

	CombatResult decide(
		const world::WorldSnapshot &snapshot,
		const WeaponInventory &inventory,
		const TargetBelief &target,
		const CombatDecisionContext &context,
		CombatDecision *decision);
	CombatResult observeDamage(const DamageObservation &observation) const;

	const world::ActorKey &actor() const;
	bool isInitialized() const;

private:
	static bool isValidTargetState(TargetBeliefState state);
	static bool isFiniteVector(const world::WorldVector &value);
	static bool isBoundedPosition(const world::WorldPosition &value);
	static bool sameFrame(
		const world::FrameIdentity &left,
		const world::FrameIdentity &right);
	static bool isFrameAfter(
		const world::FrameIdentity &candidate,
		const world::FrameIdentity &current);
	static bool buildAim(
		const CombatDecisionContext &context,
		const TargetBelief &target,
		AimIntent *intent);

	CombatConfig config_;
	world::ActorKey actor_;
	world::FrameIdentity lastFrame_;
	bool initialized_;
};
}
}

#endif
