#include "observation_adapter.hpp"

#include <chrono>
#include <cmath>
#include <cstring>

namespace astrabot
{
namespace metamod
{
namespace
{
constexpr int kC4WeaponBit = (1 << 6);

compat::ObservationContext makeContext(
	const char *semanticId,
	const world::ActorKey &actor,
	const world::FrameIdentity &frame,
	const compat::ObservationTimingContext &timing,
	compat::ObservationQuality quality,
	compat::ObservationFreshness freshness,
	compat::ObservationSource source,
	std::uint32_t delayTicks = 0U)
{
	compat::ObservationContext context = {};
	context.semanticId = semanticId;
	context.actor = actor;
	context.frame = frame;
	context.quality = quality;
	context.freshness = freshness;
	context.source = source;
	context.delayTicks = delayTicks;
	context.timing = timing;
	return context;
}

template <typename T>
compat::ObservationValue<T> publicValue(
	const char *semanticId,
	const world::ActorKey &actor,
	const world::FrameIdentity &frame,
	const compat::ObservationTimingContext &timing,
	const T &value)
{
	return {
		value,
		true,
		makeContext(semanticId, actor, frame, timing,
			compat::ObservationQuality::ExactEngineApi,
			compat::ObservationFreshness::SameTick,
			compat::ObservationSource::PublicEdict)};
}

template <typename T>
compat::ObservationValue<T> unavailableValue(
	const char *semanticId,
	const world::ActorKey &actor,
	const world::FrameIdentity &frame,
	const compat::ObservationTimingContext &timing)
{
	return {
		T(),
		false,
		makeContext(semanticId, actor, frame, timing,
			compat::ObservationQuality::Unavailable,
			compat::ObservationFreshness::Stale,
			compat::ObservationSource::None)};
}

compat::ObservationValue<bool> inferredPublicValue(
	const char *semanticId,
	const world::ActorKey &actor,
	const world::FrameIdentity &frame,
	const compat::ObservationTimingContext &timing,
	bool value)
{
	return {
		value,
		true,
		makeContext(semanticId, actor, frame, timing,
			compat::ObservationQuality::Inferred,
			compat::ObservationFreshness::SameTick,
			compat::ObservationSource::PublicEdict)};
}

void tracePlayer(
	const compat::CompatibilityObservation &observation,
	compat::IObservationTraceSink *sink)
{
	compat::emitObservationTrace(
		observation.player.health, compat::ObservationValueKind::Float, sink);
	compat::emitObservationTrace(
		observation.player.armor, compat::ObservationValueKind::Float, sink);
	compat::emitObservationTrace(
		observation.player.team, compat::ObservationValueKind::Integer, sink);
	compat::emitObservationTrace(
		observation.player.deadflag, compat::ObservationValueKind::Integer, sink);
	compat::emitObservationTrace(
		observation.player.origin, compat::ObservationValueKind::Vector, sink);
	compat::emitObservationTrace(
		observation.player.velocity, compat::ObservationValueKind::Vector, sink);
	compat::emitObservationTrace(
		observation.player.viewAngles, compat::ObservationValueKind::Vector, sink);
	compat::emitObservationTrace(
		observation.player.fov, compat::ObservationValueKind::Float, sink);
	compat::emitObservationTrace(
		observation.player.flags, compat::ObservationValueKind::Integer, sink);
	compat::emitObservationTrace(
		observation.player.onGround, compat::ObservationValueKind::Integer, sink);
	compat::emitObservationTrace(
		observation.player.waterLevel, compat::ObservationValueKind::Integer, sink);
	compat::emitObservationTrace(
		observation.player.maxSpeed, compat::ObservationValueKind::Float, sink);
	compat::emitObservationTrace(
		observation.player.buttons, compat::ObservationValueKind::Integer, sink);
	compat::emitObservationTrace(
		observation.player.oldButtons, compat::ObservationValueKind::Integer, sink);
	compat::emitObservationTrace(
		observation.player.solid, compat::ObservationValueKind::Integer, sink);
	compat::emitObservationTrace(
		observation.player.movetype, compat::ObservationValueKind::Integer, sink);
	compat::emitObservationTrace(
		observation.player.mins, compat::ObservationValueKind::Vector, sink);
	compat::emitObservationTrace(
		observation.player.maxs, compat::ObservationValueKind::Vector, sink);
	compat::emitObservationTrace(
		observation.objective.carryingC4, compat::ObservationValueKind::Boolean, sink);
}

class SequencedTraceSink : public compat::IObservationTraceSink
{
public:
	SequencedTraceSink(
		compat::IObservationTraceSink *downstream,
		std::uint64_t *sequence)
		: downstream_(downstream), sequence_(sequence)
	{
	}

	void record(const compat::ObservationTraceRecord &record) override
	{
		if (downstream_ == nullptr || sequence_ == nullptr)
		{
			return;
		}
		compat::ObservationTraceRecord sequenced = record;
		sequenced.sequence = ++(*sequence_);
		downstream_->record(sequenced);
	}

private:
	compat::IObservationTraceSink *downstream_;
	std::uint64_t *sequence_;
};
}

ObservationAdapter::ObservationAdapter()
	: engineFunctions_(nullptr),
	  globals_(nullptr),
	  traceSink_(nullptr),
	  traceSequence_(0U),
	  profiler_(nullptr)
{
}

void ObservationAdapter::configure(
	enginefuncs_t *engineFunctions,
	globalvars_t *globals)
{
	engineFunctions_ = engineFunctions;
	globals_ = globals;
}

ObservationAdapterResult ObservationAdapter::collectActor(
	edict_t *entity,
	const world::ActorKey &actor,
	const world::FrameIdentity &frame,
	const compat::ObservationTimingContext &timing,
	compat::CompatibilityObservation *observation) const
{
	if (observation == nullptr || !actor.isValid() || !frame.isValid())
	{
		return ObservationAdapterResult::InvalidArgument;
	}
	if (engineFunctions_ == nullptr || globals_ == nullptr)
	{
		return ObservationAdapterResult::EngineUnavailable;
	}
	if (entity == nullptr || entity->free != 0)
	{
		return ObservationAdapterResult::InvalidEntity;
	}

	*observation = {};
	observation->actor = actor;
	observation->frame = frame;
	observation->player.health = publicValue(
		"OBS-PLAYER-HEALTH", actor, frame, timing, entity->v.health);
	observation->player.armor = publicValue(
		"OBS-PLAYER-ARMOR", actor, frame, timing, entity->v.armorvalue);
	observation->player.team = publicValue(
		"OBS-PLAYER-TEAM", actor, frame, timing, static_cast<std::int32_t>(entity->v.team));
	observation->player.deadflag = publicValue(
		"OBS-PLAYER-DEADFLAG", actor, frame, timing,
		static_cast<std::int32_t>(entity->v.deadflag));
	observation->player.origin = publicValue(
		"OBS-PLAYER-ORIGIN", actor, frame, timing,
		world::WorldVector{entity->v.origin[0], entity->v.origin[1], entity->v.origin[2]});
	observation->player.velocity = publicValue(
		"OBS-PLAYER-VELOCITY", actor, frame, timing,
		world::WorldVector{entity->v.velocity[0], entity->v.velocity[1], entity->v.velocity[2]});
	observation->player.viewAngles = publicValue(
		"OBS-PLAYER-VIEW-ANGLES", actor, frame, timing,
		world::WorldVector{entity->v.v_angle[0], entity->v.v_angle[1], entity->v.v_angle[2]});
	observation->player.fov = publicValue(
		"OBS-PLAYER-FOV", actor, frame, timing, entity->v.fov);
	observation->player.flags = publicValue(
		"OBS-PLAYER-FLAGS", actor, frame, timing,
		static_cast<std::int32_t>(entity->v.flags));
	observation->player.onGround = publicValue(
		"OBS-PLAYER-ON-GROUND", actor, frame, timing,
		static_cast<std::int32_t>((entity->v.flags & FL_ONGROUND) != 0));
	observation->player.waterLevel = publicValue(
		"OBS-PLAYER-WATER-LEVEL", actor, frame, timing,
		static_cast<std::int32_t>(entity->v.waterlevel));
	observation->player.maxSpeed = publicValue(
		"OBS-PLAYER-MAX-SPEED", actor, frame, timing, entity->v.maxspeed);
	observation->player.buttons = publicValue(
		"OBS-PLAYER-BUTTONS", actor, frame, timing,
		static_cast<std::int32_t>(entity->v.button));
	observation->player.oldButtons = publicValue(
		"OBS-PLAYER-OLD-BUTTONS", actor, frame, timing,
		static_cast<std::int32_t>(entity->v.oldbuttons));
	observation->player.solid = publicValue(
		"OBS-PLAYER-SOLID", actor, frame, timing,
		static_cast<std::int32_t>(entity->v.solid));
	observation->player.movetype = publicValue(
		"OBS-PLAYER-MOVETYPE", actor, frame, timing,
		static_cast<std::int32_t>(entity->v.movetype));
	observation->player.mins = publicValue(
		"OBS-PLAYER-MINS", actor, frame, timing,
		world::WorldVector{entity->v.mins[0], entity->v.mins[1], entity->v.mins[2]});
	observation->player.maxs = publicValue(
		"OBS-PLAYER-MAXS", actor, frame, timing,
		world::WorldVector{entity->v.maxs[0], entity->v.maxs[1], entity->v.maxs[2]});

	observation->weapon.activeWeaponId = unavailableValue<std::uint16_t>(
		"OBS-PLAYER-ACTIVE-WEAPON", actor, frame, timing);
	observation->weapon.clip = unavailableValue<std::uint16_t>(
		"OBS-WEAPON-CLIP", actor, frame, timing);
	observation->weapon.reserve = unavailableValue<std::uint16_t>(
		"OBS-WEAPON-RESERVE-AMMO", actor, frame, timing);
	observation->weapon.reloadState = unavailableValue<combat::ReloadState>(
		"OBS-WEAPON-RELOAD", actor, frame, timing);
	observation->weapon.nextPrimaryAttack = unavailableValue<float>(
		"OBS-WEAPON-NEXT-PRIMARY", actor, frame, timing);
	observation->weapon.nextSecondaryAttack = unavailableValue<float>(
		"OBS-WEAPON-NEXT-SECONDARY", actor, frame, timing);
	observation->weapon.accuracy = unavailableValue<float>(
		"OBS-WEAPON-ACCURACY", actor, frame, timing);
	observation->weapon.silencer = unavailableValue<bool>(
		"OBS-WEAPON-SILENCER", actor, frame, timing);
	observation->weapon.burst = unavailableValue<bool>(
		"OBS-WEAPON-BURST", actor, frame, timing);
	observation->weapon.zoomed = unavailableValue<bool>(
		"OBS-WEAPON-ZOOM", actor, frame, timing);

	observation->objective.carryingC4 = inferredPublicValue(
		"OBS-OBJECTIVE-C4-POSSESSION", actor, frame, timing,
		(entity->v.weapons & kC4WeaponBit) != 0);
	observation->objective.bombPlanted = unavailableValue<bool>(
		"OBS-OBJECTIVE-BOMB-PLANTED", actor, frame, timing);
	observation->objective.bombPosition = unavailableValue<world::WorldVector>(
		"OBS-OBJECTIVE-BOMB-POSITION", actor, frame, timing);
	observation->objective.bombTimer = unavailableValue<float>(
		"OBS-OBJECTIVE-BOMB-TIMER", actor, frame, timing);
	observation->objective.defusing = unavailableValue<bool>(
		"OBS-OBJECTIVE-DEFUSING", actor, frame, timing);
	observation->objective.hasDefuseKit = unavailableValue<bool>(
		"OBS-OBJECTIVE-DEFUSE-KIT", actor, frame, timing);
	observation->objective.inBombZone = unavailableValue<bool>(
		"OBS-OBJECTIVE-BOMB-ZONE", actor, frame, timing);
	observation->objective.hostageAvailable = unavailableValue<bool>(
		"OBS-OBJECTIVE-HOSTAGE", actor, frame, timing);
	observation->objective.inRescueZone = unavailableValue<bool>(
		"OBS-OBJECTIVE-RESCUE-ZONE", actor, frame, timing);
	observation->objective.vip = unavailableValue<bool>(
		"OBS-OBJECTIVE-VIP", actor, frame, timing);

	SequencedTraceSink sequencedTrace(traceSink_, &traceSequence_);
	tracePlayer(*observation, traceSink_ == nullptr ? nullptr : &sequencedTrace);
	return ObservationAdapterResult::Accepted;
}

ObservationAdapterResult ObservationAdapter::collectVisibility(
	edict_t *observer,
	edict_t *target,
	const world::ActorKey &targetActor,
	const world::FrameIdentity &frame,
	perception::VisionObservation *observation) const
{
	if (observation == nullptr || !targetActor.isValid() || !frame.isValid())
		return ObservationAdapterResult::InvalidArgument;
	if (engineFunctions_ == nullptr || globals_ == nullptr ||
		engineFunctions_->pfnTraceLine == nullptr)
		return ObservationAdapterResult::EngineUnavailable;
	if (observer == nullptr || target == nullptr || observer->free != 0 || target->free != 0)
		return ObservationAdapterResult::InvalidEntity;
	const bool profileVision = profiler_ != nullptr && profiler_->enabled();
	const auto visionStart = profileVision
		? std::chrono::steady_clock::now()
		: std::chrono::steady_clock::time_point();

	*observation = {};
	observation->target = targetActor;
	if ((target->v.flags & FL_NOTARGET) != 0 ||
		(target->v.effects & EF_NODRAW) != 0)
		return ObservationAdapterResult::Accepted;

	const float observerX = observer->v.origin[0];
	const float observerY = observer->v.origin[1];
	const float observerZ = observer->v.origin[2];
	const float eye[3] = {
		observerX + observer->v.view_ofs[0],
		observerY + observer->v.view_ofs[1],
		observerZ + observer->v.view_ofs[2]};
	const float yawRadians = observer->v.angles[1] *
		3.14159265358979323846f / 180.0f;
	const float forwardX = std::cos(yawRadians);
	const float forwardY = std::sin(yawRadians);
	std::uint64_t traceCalls = 0U;

	auto inViewCone = [&](const float point[3]) -> bool
	{
		const float dx = point[0] - observerX;
		const float dy = point[1] - observerY;
		const float length = std::sqrt(dx * dx + dy * dy);
		if (!std::isfinite(length) || length <= 0.001f)
			return true;
		const float dot = (dx / length) * forwardX + (dy / length) * forwardY;
		return dot > 0.5f;
	};

	auto traceVisible = [&](const float point[3]) -> bool
	{
		TraceResult result = {};
		++traceCalls;
		const auto traceStart = profileVision
			? std::chrono::steady_clock::now()
			: std::chrono::steady_clock::time_point();
		engineFunctions_->pfnTraceLine(eye, point, 1, observer, &result);
		if (profileVision)
		{
			const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
				std::chrono::steady_clock::now() - traceStart).count();
			profiler_->record(
				RuntimeProfilerStage::TraceLine,
				elapsed > 0 ? static_cast<std::uint64_t>(elapsed) : 0U);
		}
		return result.flFraction == 1.0f;
	};

	auto testPoint = [&](const float point[3], std::uint8_t bit) -> void
	{
		if (!inViewCone(point))
			return;
		observation->fovPassed = true;
		if (traceVisible(point))
		{
			observation->losPassed = true;
			observation->visibleParts = static_cast<std::uint8_t>(
				observation->visibleParts | bit);
		}
	};

	float point[3] = {target->v.origin[0], target->v.origin[1], target->v.origin[2]};
	// Keep the reference order: chest, head, feet, left edge, right edge.
	testPoint(point, world::VisibleChest);
	point[2] = target->v.origin[2] + 25.0f;
	testPoint(point, world::VisibleHead);
	point[2] = target->v.origin[2] -
		((target->v.flags & FL_DUCKING) != 0 ? 14.0f : 34.0f);
	testPoint(point, world::VisibleFeet);

	const float dx = target->v.origin[0] - observer->v.origin[0];
	const float dy = target->v.origin[1] - observer->v.origin[1];
	const float horizontalLength = std::sqrt(dx * dx + dy * dy);
	if (horizontalLength > 0.001f && std::isfinite(horizontalLength))
	{
		const float perpX = -dy / horizontalLength;
		const float perpY = dx / horizontalLength;
		point[0] = target->v.origin[0] + perpX * 13.0f;
		point[1] = target->v.origin[1] + perpY * 13.0f;
		point[2] = target->v.origin[2];
		testPoint(point, world::VisibleLeftSide);
		point[0] = target->v.origin[0] - perpX * 13.0f;
		point[1] = target->v.origin[1] - perpY * 13.0f;
		testPoint(point, world::VisibleRightSide);
	}

	observation->visible = observation->visibleParts != world::VisibleNone;
	if (profiler_ != nullptr)
	{
		profiler_->recordTraceLine(1U, 1U, traceCalls);
		if (profileVision)
		{
			const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
				std::chrono::steady_clock::now() - visionStart).count();
			profiler_->record(
				RuntimeProfilerStage::Vision,
				elapsed > 0 ? static_cast<std::uint64_t>(elapsed) : 0U);
		}
	}
	return ObservationAdapterResult::Accepted;
}

void ObservationAdapter::setTraceSink(compat::IObservationTraceSink *sink)
{
	traceSink_ = sink;
}

void ObservationAdapter::setProfiler(RuntimeProfiler *profiler)
{
	profiler_ = profiler;
}

ObservationAdapterResult ObservationAdapter::collectPlantedBomb(
	edict_t *entity,
	const char *classname,
	const char *model,
	float currentTime,
	const world::ActorKey &actor,
	const world::FrameIdentity &frame,
	const compat::ObservationTimingContext &timing,
	compat::ObjectiveObservation *observation) const
{
	if (observation == nullptr || !actor.isValid() || !frame.isValid())
	{
		return ObservationAdapterResult::InvalidArgument;
	}
	if (engineFunctions_ == nullptr || globals_ == nullptr)
	{
		return ObservationAdapterResult::EngineUnavailable;
	}
	if (entity == nullptr || entity->free != 0 || classname == nullptr ||
		model == nullptr || !std::isfinite(currentTime) ||
		!std::isfinite(entity->v.dmgtime) ||
		std::strcmp(classname, "grenade") != 0 ||
		entity->v.dmgtime <= currentTime ||
		(model[0] != '\0' && std::strstr(model, "w_c4.mdl") == nullptr))
	{
		return ObservationAdapterResult::InvalidEntity;
	}

	*observation = {};
	SequencedTraceSink sequencedTrace(traceSink_, &traceSequence_);
	compat::IObservationTraceSink *traceSink =
		traceSink_ == nullptr ? nullptr : &sequencedTrace;
	const compat::ObservationContext plantedContext = makeContext(
		"OBS-OBJECTIVE-BOMB-PLANTED", actor, frame, timing,
		compat::ObservationQuality::Inferred,
		compat::ObservationFreshness::SameTick,
		compat::ObservationSource::PublicEdict);
	const compat::ObservationContext positionContext = makeContext(
		"OBS-OBJECTIVE-BOMB-POSITION", actor, frame, timing,
		compat::ObservationQuality::Inferred,
		compat::ObservationFreshness::SameTick,
		compat::ObservationSource::PublicEdict);
	const compat::ObservationContext timerContext = makeContext(
		"OBS-OBJECTIVE-BOMB-TIMER", actor, frame, timing,
		compat::ObservationQuality::Inferred,
		compat::ObservationFreshness::SameTick,
		compat::ObservationSource::PublicEdict);
	observation->bombPlanted = {true, true, plantedContext};
	observation->bombPosition = {
		world::WorldVector{entity->v.origin[0], entity->v.origin[1], entity->v.origin[2]},
		true,
		positionContext};
	observation->bombTimer = {entity->v.dmgtime, true, timerContext};
	compat::emitObservationTrace(
		observation->bombPlanted, compat::ObservationValueKind::Boolean, traceSink);
	compat::emitObservationTrace(
		observation->bombPosition, compat::ObservationValueKind::Vector, traceSink);
	compat::emitObservationTrace(
		observation->bombTimer, compat::ObservationValueKind::Float, traceSink);
	return ObservationAdapterResult::Accepted;
}
}
}
