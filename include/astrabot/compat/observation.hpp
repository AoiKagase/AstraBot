#ifndef ASTRABOT_COMPAT_OBSERVATION_HPP
#define ASTRABOT_COMPAT_OBSERVATION_HPP

#include "astrabot/combat/weapon_state.hpp"
#include "astrabot/world/world_snapshot.hpp"

#include <cstdint>

namespace astrabot
{
namespace compat
{
enum class ObservationQuality
{
	Exact,
	ExactEngineApi,
	ExactGameApi,
	Delayed,
	Inferred,
	Approximated,
	Unavailable,
	NotYetImplemented,
	NotRequired,
	Unknown
};

enum class ObservationFreshness
{
	SameTick,
	CurrentFullUpdate,
	EventDrivenCached,
	Stale
};

enum class ObservationSource
{
	PublicEdict,
	PublicEngineApi,
	PublicGameDll,
	AdapterCache,
	Fixture,
	Synthetic,
	None
};

enum class ObservationValueKind
{
	Boolean,
	Integer,
	Float,
	Vector
};

struct ObservationTimingContext
{
	std::uint32_t commandSequence;
	std::uint32_t upkeepSequence;
	std::uint32_t fullUpdateSequence;
};

struct ObservationContext
{
	const char *semanticId;
	world::ActorKey actor;
	world::FrameIdentity frame;
	ObservationQuality quality;
	ObservationFreshness freshness;
	ObservationSource source;
	std::uint32_t delayTicks;
	ObservationTimingContext timing;

	bool isValid() const;
	bool isCurrent(const world::FrameIdentity &frameValue) const;
};

bool isAvailableQuality(ObservationQuality quality);
const char *toString(ObservationQuality quality);
const char *toString(ObservationFreshness freshness);
const char *toString(ObservationSource source);

template <typename T>
struct ObservationValue
{
	T value;
	bool valid;
	ObservationContext context;

	bool isAvailable() const
	{
		return valid && context.isValid() && isAvailableQuality(context.quality);
	}

	bool isCurrent(const world::FrameIdentity &frameValue) const
	{
		return isAvailable() && context.freshness != ObservationFreshness::Stale &&
			context.isCurrent(frameValue);
	}
};

struct PlayerObservation
{
	ObservationValue<float> health;
	ObservationValue<float> armor;
	ObservationValue<std::int32_t> team;
	ObservationValue<std::int32_t> deadflag;
	ObservationValue<world::WorldVector> origin;
	ObservationValue<world::WorldVector> velocity;
	ObservationValue<world::WorldVector> viewAngles;
	ObservationValue<float> fov;
	ObservationValue<std::int32_t> flags;
	ObservationValue<std::int32_t> onGround;
	ObservationValue<std::int32_t> waterLevel;
	ObservationValue<float> maxSpeed;
	ObservationValue<std::int32_t> buttons;
	ObservationValue<std::int32_t> oldButtons;
	ObservationValue<std::int32_t> solid;
	ObservationValue<std::int32_t> movetype;
	ObservationValue<world::WorldVector> mins;
	ObservationValue<world::WorldVector> maxs;
};

struct WeaponObservation
{
	ObservationValue<std::uint16_t> activeWeaponId;
	ObservationValue<std::uint16_t> clip;
	ObservationValue<std::uint16_t> reserve;
	ObservationValue<combat::ReloadState> reloadState;
	ObservationValue<float> nextPrimaryAttack;
	ObservationValue<float> nextSecondaryAttack;
	ObservationValue<float> accuracy;
	ObservationValue<bool> silencer;
	ObservationValue<bool> burst;
	ObservationValue<bool> zoomed;
};

struct ObjectiveObservation
{
	ObservationValue<bool> carryingC4;
	ObservationValue<bool> bombPlanted;
	ObservationValue<world::WorldVector> bombPosition;
	ObservationValue<float> bombTimer;
	ObservationValue<bool> defusing;
	ObservationValue<bool> hasDefuseKit;
	ObservationValue<bool> inBombZone;
	ObservationValue<bool> hostageAvailable;
	ObservationValue<bool> inRescueZone;
	ObservationValue<bool> vip;
};

struct CompatibilityObservation
{
	world::ActorKey actor;
	world::FrameIdentity frame;
	PlayerObservation player;
	WeaponObservation weapon;
	ObjectiveObservation objective;

	bool isValid() const;
};

struct ObservationTraceRecord
{
	std::uint64_t sequence;
	ObservationContext context;
	ObservationValueKind kind;
	float floatValue;
	std::int32_t integerValue;
	bool booleanValue;
	world::WorldVector vectorValue;
};

class IObservationTraceSink
{
public:
	virtual ~IObservationTraceSink() {}
	virtual void record(const ObservationTraceRecord &record) = 0;
};

void emitObservationTrace(
	const ObservationValue<float> &value,
	ObservationValueKind kind,
	IObservationTraceSink *sink);
void emitObservationTrace(
	const ObservationValue<std::int32_t> &value,
	ObservationValueKind kind,
	IObservationTraceSink *sink);
void emitObservationTrace(
	const ObservationValue<bool> &value,
	ObservationValueKind kind,
	IObservationTraceSink *sink);
void emitObservationTrace(
	const ObservationValue<world::WorldVector> &value,
	ObservationValueKind kind,
	IObservationTraceSink *sink);
}
}

#endif
