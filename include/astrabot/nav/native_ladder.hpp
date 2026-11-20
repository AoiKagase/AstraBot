#ifndef ASTRABOT_NAV_NATIVE_LADDER_HPP
#define ASTRABOT_NAV_NATIVE_LADDER_HPP
#include "astrabot/nav/nav_query.hpp"
#include "astrabot/compat/random_source.hpp"
#include <functional>
#include "astrabot/world/world_snapshot.hpp"
#include <algorithm>
#include <cmath>
namespace astrabot
{
namespace nav
{
enum class NativeLadderPhase
{
	None,
	ApproachUp,
	FaceUp,
	MountUp,
	Ascend,
	DismountUp,
	ApproachDown,
	FaceDown,
	MountDown,
	Descend,
	MoveToDestination
};
enum class NativeLadderResult
{
	Active,
	Complete,
	Failed,
	Unavailable,
	Inactive,
	Stale
};
struct NativeLadderObservation
{
	world::ActorKey actor{};
	world::FrameIdentity frame{};
	NavVector origin{};
	float feetZ = 0, now = 0, yaw = 0, pitch = 0;
	bool onLadder = false, stuck = false, crouching = false;
	AreaId lastKnownArea = 0;
};
struct NativeLadderLook
{
	bool active = false, pitchOverride = false;
	float yaw = 0, pitch = 0;
	NativeLadderPhase phase = NativeLadderPhase::None;
};
struct NativeLadderIntent
{
	NavVector goal{};
	float forward = 0, side = 0;
	bool walking = false, crouching = false, mustJump = false, wiggle = false, resetProgress = false;
};
class INativeLadderProbe
{
public:
	virtual ~INativeLadderProbe() = default;
	virtual bool current() const = 0;
	virtual bool trace(const NavLadder &ladder, const NavVector &from, const NavVector &to, float *fraction) = 0;
};
class NativeLadderRecovery
{
public:
	void recordJump(float now)
	{
		jumpAt_ = now;
	}
	bool apply(const NativeLadderObservation &in, bool grounded, std::uint8_t attributes,
		compat::ICompatibilityRandomSource *random, compat::RandomTimingContext timing,
		const std::function<bool()> &current, NativeLadderIntent *intent)
	{
		if (!intent || !in.actor.isValid() || !std::isfinite(in.now) || !current || !current())
			return false;
		auto next = *this;
		auto out = *intent;
		const bool mustJump = out.mustJump, wiggle = out.wiggle;
		out.mustJump = false;
		out.wiggle = false;
		const auto jump = [&](bool must)
		{
			const float elapsed = in.now - next.jumpAt_;
			const bool jumping = elapsed <= 3 && (elapsed < 1 || !grounded);
			if (in.crouching || jumping || (!must && elapsed < .9f) || elapsed < .3f ||
				((attributes & NavArea::kCrouch) && !(attributes & NavArea::kJump)))
				return false;
			next.jumpAt_ = in.now;
			out.mustJump = true;
			return true;
		};
		if (mustJump)
			jump(true);
		if (wiggle && !in.crouching)
		{
			if (!random)
				return false;
			const compat::RandomActor actor{in.actor.slot, in.actor.generation};
			if (in.now >= next.wiggleAt_)
			{
				const auto direction =
					random->nextLong(compat::RandomRequest::longRequest("CSBOT-WIGGLE-DIRECTION", actor, timing, 0, 3));
				if (!current() || direction.status != compat::RandomStatus::Ok || direction.value < 0 ||
					direction.value > 3)
					return false;
				const auto duration = random->nextFloat(
					compat::RandomRequest::floatRequest("CSBOT-WIGGLE-DURATION", actor, timing, .5f, 1.5f));
				if (!current() || duration.status != compat::RandomStatus::Ok || !std::isfinite(duration.value) ||
					duration.value < .5f || duration.value > 1.5f)
					return false;
				next.direction_ = direction.value;
				next.wiggleAt_ = in.now + duration.value;
			}
			if (next.direction_ == 0)
				out.forward = 1;
			else if (next.direction_ == 1)
				out.side = 1;
			else if (next.direction_ == 2)
				out.forward = -1;
			else
				out.side = -1;
			if (in.now >= next.nextJumpAt_ && jump(false))
			{
				const auto interval = random->nextFloat(
					compat::RandomRequest::floatRequest("CSBOT-WIGGLE-JUMP-INTERVAL", actor, timing, 1, 2));
				if (!current() || interval.status != compat::RandomStatus::Ok || !std::isfinite(interval.value) ||
					interval.value < 1 || interval.value > 2)
					return false;
				next.nextJumpAt_ = in.now + interval.value;
			}
		}
		if (!current())
			return false;
		*this = next;
		*intent = out;
		return true;
	}

private:
	float jumpAt_ = 0, wiggleAt_ = 0, nextJumpAt_ = 0;
	int direction_ = 0;
};
class NativeLadderController
{
public:
	bool start(const NavSnapshot &snapshot, const NavDirectedLink &link, const NavLadder &ladder,
		const NativeLadderObservation &in, INativeLadderProbe &probe)
	{
		reset();
		const auto operation = revision_;
		if (!probe.current() || !snapshot.isValid() || !valid(in) ||
			snapshot.mapGeneration() != in.frame.mapGeneration || !snapshot.document()->findArea(link.fromArea) ||
			!snapshot.document()->findArea(link.toArea) || !finite(ladder.top) || !finite(ladder.bottom) ||
			!std::isfinite(ladder.length) || ladder.length < 0 || ladder.direction >= 4 ||
			link.direction != ladder.direction || (link.how != 4 && link.how != 5))
			return false;
		const auto end = link.how == 4 ? ladder.topAreas.begin() + 3 : ladder.topAreas.end();
		if (link.how == 4
				? (ladder.dangling || link.fromArea != ladder.bottomArea ||
					  std::find(ladder.topAreas.begin(), end, link.toArea) == end)
				: (link.toArea != ladder.bottomArea || std::find(ladder.topAreas.begin(), end, link.fromArea) == end))
			return false;
		NativeLadderController next;
		next.revision_ = operation;
		next.ladder_ = ladder;
		next.destination_ = link.toArea;
		next.actor_ = in.actor;
		next.frame_ = in.frame;
		next.navRevision_ = snapshot.revision();
		next.started_ = next.lastTime_ = in.now;
		next.crouching_ = in.crouching;
		next.faceIn_ = true;
		next.active_ = true;
		if (link.how == 4)
		{
			next.phase_ = NativeLadderPhase::ApproachUp;
			next.goal_ = offset(ladder.bottom, ladder.direction, 32);
		}
		else
		{
			next.phase_ = NativeLadderPhase::ApproachDown;
			next.goal_ = offset(ladder.top, (ladder.direction + 2) % 4, 32);
			float fraction = 0;
			if (!probe.trace(ladder, ladder.top, next.goal_, &fraction) || !probe.current() || revision_ != operation ||
				!validFraction(fraction))
				return false;
			next.faceIn_ = fraction != 1;
			if (next.faceIn_)
				next.goal_ = offset(ladder.top, ladder.direction, 16);
		}
		if (!probe.current() || revision_ != operation)
			return false;
		*this = next;
		return true;
	}
	NativeLadderResult update(const NavSnapshot &snapshot, const NativeLadderObservation &in, INativeLadderProbe &probe,
		NativeLadderIntent *intent)
	{
		if (!intent)
			return NativeLadderResult::Unavailable;
		*intent = {};
		if (!active_)
			return NativeLadderResult::Inactive;
		if (!sameOwner(snapshot, in) || in.now < lastTime_)
		{
			reset();
			return NativeLadderResult::Stale;
		}
		if (!valid(in) || !probe.current() || (hasUpdated_ && in.frame.tick == frame_.tick))
			return NativeLadderResult::Unavailable;
		bump();
		const auto operation = revision_;
		auto next = *this;
		NativeLadderIntent output{};
		const auto result = next.advance(snapshot, in, probe, &output);
		if (!probe.current() || revision_ != operation)
			return NativeLadderResult::Unavailable;
		if (result == NativeLadderResult::Unavailable)
			return result;
		next.frame_ = in.frame;
		next.hasUpdated_ = true;
		next.lastTime_ = in.now;
		*this = next;
		*intent = output;
		return result;
	}
	NativeLadderLook look(const NavSnapshot &snapshot, const NativeLadderObservation &in) const
	{
		if (!active_ || !sameOwner(snapshot, in) || !valid(in) || in.now < lastTime_)
			return {};
		NativeLadderLook result;
		if (phase_ == NativeLadderPhase::MoveToDestination)
		{
			if (destination_ == ladder_.bottomArea)
			{
				result.phase = phase_;
				result.yaw = directionYaw(faceIn_ ? (ladder_.direction + 2U) % 4U : ladder_.direction);
			}
			return result;
		}
		result.phase = phase_;
		result.active = true;
		result.yaw = yawTo(in.origin, ladder_.top);
		result.pitch = in.pitch;
		const bool up = phase_ == NativeLadderPhase::ApproachUp || phase_ == NativeLadderPhase::FaceUp ||
			phase_ == NativeLadderPhase::MountUp || phase_ == NativeLadderPhase::Ascend;
		if (phase_ == NativeLadderPhase::ApproachUp || phase_ == NativeLadderPhase::ApproachDown)
		{
			const float x = goal_.x - in.origin.x, y = goal_.y - in.origin.y, z = goal_.z - in.origin.z;
			result.pitchOverride = x * x + y * y + z * z < 100.0f * 100.0f;
		}
		else
			result.pitchOverride = phase_ != NativeLadderPhase::DismountUp;
		if (result.pitchOverride)
			result.pitch = up ? -60.0f : 60.0f;
		if (phase_ == NativeLadderPhase::MountUp || phase_ == NativeLadderPhase::MountDown ||
			phase_ == NativeLadderPhase::Ascend || phase_ == NativeLadderPhase::Descend ||
			phase_ == NativeLadderPhase::DismountUp)
		{
			const unsigned facing = faceIn_ ? (ladder_.direction + 2) % 4 : ladder_.direction;
			result.yaw = directionYaw(facing);
			if (phase_ != NativeLadderPhase::DismountUp)
			{
				const auto quadrant = facingDirection(in.yaw);
				const float x = ladder_.top.x - in.origin.x, y = ladder_.top.y - in.origin.y;
				result.yaw += quadrant == 0 ? x : quadrant == 1 ? y : quadrant == 2 ? -x : -y;
			}
		}
		result.yaw = normalize(result.yaw);
		return result;
	}
	const NavLadder &source() const
	{
		return ladder_;
	}
	void reset()
	{
		const auto next = revision_ + 1;
		*this = NativeLadderController();
		revision_ = next ? next : 1;
	}
	bool active() const
	{
		return active_;
	}
	NativeLadderPhase phase() const
	{
		return phase_;
	}
	bool crouching() const
	{
		return crouching_;
	}

private:
	static bool finite(const NavVector &v)
	{
		return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
	}
	static bool validFraction(float value)
	{
		return std::isfinite(value) && value >= 0 && value <= 1;
	}
	static bool valid(const NativeLadderObservation &in)
	{
		return in.actor.isValid() && in.frame.isValid() && finite(in.origin) && std::isfinite(in.feetZ) &&
			std::isfinite(in.now) && in.now >= 0 && std::isfinite(in.yaw) && std::isfinite(in.pitch);
	}
	bool sameOwner(const NavSnapshot &snapshot, const NativeLadderObservation &in) const
	{
		return snapshot.isValid() && snapshot.revision() == navRevision_ &&
			snapshot.mapGeneration() == frame_.mapGeneration && in.actor == actor_ &&
			in.frame.mapGeneration == frame_.mapGeneration && in.frame.roundGeneration == frame_.roundGeneration &&
			in.frame.tick >= frame_.tick;
	}
	void bump()
	{
		if (++revision_ == 0)
			++revision_;
	}
	static NavVector direction(unsigned index)
	{
		return index == 0 ? NavVector{0, -1, 0}
			: index == 1  ? NavVector{1, 0, 0}
			: index == 2  ? NavVector{0, 1, 0}
						  : NavVector{-1, 0, 0};
	}
	static NavVector offset(NavVector point, unsigned facing, float distance)
	{
		const auto axis = direction(facing);
		point.x += axis.x * distance;
		point.y += axis.y * distance;
		return point;
	}
	static float normalize(float yaw)
	{
		yaw = std::fmod(yaw + 180.0f, 360.0f);
		if (yaw < 0)
			yaw += 360.0f;
		return yaw - 180.0f;
	}
	static float directionYaw(unsigned facing)
	{
		return facing == 0 ? -90.0f : facing == 1 ? 0.0f : facing == 2 ? 90.0f : 180.0f;
	}
	static unsigned facingDirection(float yaw)
	{
		yaw = std::fmod(yaw, 360.0f);
		if (yaw < 0)
			yaw += 360.0f;
		return yaw < 45 || yaw > 315 ? 1 : yaw < 135 ? 2 : yaw < 225 ? 3 : 0;
	}
	static float yawTo(const NavVector &from, const NavVector &to)
	{
		return std::atan2(to.y - from.y, to.x - from.x) * 57.29577951308232f;
	}
	static NavVector closest(const NavArea &area, NavVector point)
	{
		point.x = (std::max)(area.extent.lo.x, (std::min)(point.x, area.extent.hi.x));
		point.y = (std::max)(area.extent.lo.y, (std::min)(point.y, area.extent.hi.y));
		point.z = surfaceZAt(area, point.x, point.y);
		return point;
	}
	static bool overlaps(const NavArea &area, const NavVector &point)
	{
		return point.x >= area.extent.lo.x && point.x <= area.extent.hi.x && point.y >= area.extent.lo.y &&
			point.y <= area.extent.hi.y;
	}
	static bool contains(const NavDocument &document, const NavArea &area, const NavVector &point)
	{
		if (!overlaps(area, point))
			return false;
		const float z = surfaceZAt(area, point.x, point.y);
		if (z > point.z)
			return false;
		for (const auto &other : document.areas())
			if (other.id != area.id && overlaps(other, point))
			{
				const float otherZ = surfaceZAt(other, point.x, point.y);
				if (otherZ <= point.z && otherZ > z)
					return false;
			}
		return true;
	}
	void moveTowards(const NativeLadderObservation &in, NativeLadderIntent *out) const
	{
		const float x = goal_.x - in.origin.x, y = goal_.y - in.origin.y, length = std::hypot(x, y);
		if (length == 0)
			return;
		const float yaw = in.yaw * .017453292519943295f, c = std::cos(yaw), s = std::sin(yaw);
		const float forward = (x * c + y * s) / length, lateral = (-x * s + y * c) / length;
		out->forward = forward > .25f ? 1.0f : forward < -.25f ? -1.0f : 0.0f;
		out->side = lateral >= .25f ? -1.0f : lateral <= -.25f ? 1.0f : 0.0f;
	}
	bool endpoint(bool ascending, const NativeLadderObservation &in, INativeLadderProbe &probe)
	{
		auto from = ascending ? ladder_.bottom : ladder_.top;
		from.z = in.origin.z;
		const auto to = ascending ? ladder_.top : ladder_.bottom;
		float fraction = 0;
		if (!probe.trace(ladder_, from, to, &fraction) || !probe.current() || !validFraction(fraction))
			return false;
		endpoint_ = from.z + fraction * (to.z - from.z);
		return true;
	}
	NativeLadderResult fail(NativeLadderIntent *out, bool jump, bool wiggle)
	{
		active_ = false;
		phase_ = NativeLadderPhase::None;
		walking_ = false;
		out->mustJump = jump;
		out->wiggle = wiggle;
		out->resetProgress = true;
		out->crouching = crouching_;
		return NativeLadderResult::Failed;
	}
	NativeLadderResult advance(const NavSnapshot &snapshot, const NativeLadderObservation &in,
		INativeLadderProbe &probe, NativeLadderIntent *out)
	{
		using Phase = NativeLadderPhase;
		const bool canCheckStuck = phase_ != Phase::FaceUp && phase_ != Phase::FaceDown && phase_ != Phase::MountUp &&
			phase_ != Phase::MountDown;
		if (in.now - started_ > 10 || (canCheckStuck && in.stuck))
			return fail(out, true, true);
		out->resetProgress = true;
		if (phase_ == Phase::MountUp || phase_ == Phase::MountDown || phase_ == Phase::Ascend ||
			phase_ == Phase::Descend)
		{
			const float x = ladder_.top.x - in.origin.x, y = ladder_.top.y - in.origin.y;
			if (x * x + y * y > 200.0f * 200.0f)
				return fail(out, true, false);
		}
		const auto area = snapshot.document()->findArea(destination_);
		if (!area)
			return fail(out, false, false);
		const float x = in.origin.x - goal_.x, y = in.origin.y - goal_.y, distance = std::hypot(x, y);
		const auto axis = direction(ladder_.direction);
		const float along = x * axis.x + y * axis.y, lateral = -x * axis.y + y * axis.x;
		switch (phase_)
		{
		case Phase::ApproachUp:
			if (distance < 50)
			{
				walking_ = true;
				crouching_ = false;
			}
			if (in.onLadder)
			{
				if (!endpoint(true, in, probe))
					return NativeLadderResult::Unavailable;
				phase_ = Phase::Ascend;
			}
			else if (along < 0 && std::fabs(std::trunc(lateral)) < 10 && distance < 25)
				phase_ = Phase::FaceUp;
			else
				moveTowards(in, out);
			break;
		case Phase::ApproachDown:
			if (in.feetZ <= ladder_.bottom.z + 36)
			{
				phase_ = Phase::MoveToDestination;
				goal_ = offset(closest(*area, ladder_.bottom), ladder_.direction, 16);
			}
			else
			{
				if (in.lastKnownArea != ladder_.topAreas[3] && !in.crouching && distance < 150)
					walking_ = true;
				if (in.onLadder)
				{
					if (!endpoint(false, in, probe))
						return NativeLadderResult::Unavailable;
					phase_ = Phase::Descend;
					walking_ = false;
				}
				else if (along > 0 && std::fabs(std::trunc(lateral)) < 10 && distance < 25)
					phase_ = Phase::FaceDown;
				else
					moveTowards(in, out);
			}
			break;
		case Phase::FaceUp:
		case Phase::FaceDown:
			if (std::fabs(
					normalize(yawTo(in.origin, phase_ == Phase::FaceUp ? ladder_.bottom : ladder_.top) - in.yaw)) < 5)
			{
				if (phase_ == Phase::FaceUp)
				{
					phase_ = Phase::MountUp;
					walking_ = false;
				}
				else
					phase_ = Phase::MountDown;
			}
			break;
		case Phase::MountUp:
			if (in.onLadder)
			{
				if (!endpoint(true, in, probe))
					return NativeLadderResult::Unavailable;
				phase_ = Phase::Ascend;
			}
			out->forward = 1;
			break;
		case Phase::MountDown:
			if (in.feetZ <= ladder_.bottom.z + 36)
			{
				phase_ = Phase::MoveToDestination;
				goal_ = offset(closest(*area, ladder_.bottom), ladder_.direction, 16);
			}
			else
			{
				if (in.onLadder)
				{
					if (!endpoint(false, in, probe))
						return NativeLadderResult::Unavailable;
					phase_ = Phase::Descend;
				}
				out->forward = 1;
			}
			break;
		case Phase::Ascend:
			walking_ = false;
			if (area->attributes & NavArea::kCrouch)
				crouching_ = true;
			if (in.feetZ >= endpoint_)
			{
				phase_ = Phase::DismountUp;
				dismountAt_ = in.now;
				dismount_ = destination_ == ladder_.topAreas[0] ? 0 : destination_ == ladder_.topAreas[1] ? -1 : 1;
			}
			else if (!in.onLadder)
				return fail(out, false, false);
			out->forward = 1;
			break;
		case Phase::Descend:
			walking_ = false;
			if (!in.onLadder || in.feetZ <= endpoint_ + 36)
			{
				phase_ = Phase::MoveToDestination;
				goal_ = offset(closest(*area, ladder_.bottom), ladder_.direction, 16);
			}
			out->forward = 1;
			break;
		case Phase::DismountUp:
			if (in.now - dismountAt_ >= .4f)
			{
				phase_ = Phase::MoveToDestination;
				goal_ = closest(*area, in.origin);
			}
			if (dismount_ == 0)
				out->forward = faceIn_ ? 1.0f : -1.0f;
			else
				out->side = static_cast<float>(faceIn_ ? dismount_ : -dismount_);
			break;
		case Phase::MoveToDestination:
			if (contains(*snapshot.document(), *area, in.origin))
			{
				active_ = false;
				phase_ = Phase::None;
				return NativeLadderResult::Complete;
			}
			moveTowards(in, out);
			break;
		default:
			return fail(out, false, false);
		}
		out->goal = goal_;
		out->walking = walking_;
		out->crouching = crouching_;
		return NativeLadderResult::Active;
	}
	std::uint64_t revision_ = 1, navRevision_ = 0;
	world::ActorKey actor_{};
	world::FrameIdentity frame_{};
	NavLadder ladder_{};
	AreaId destination_ = 0;
	NavVector goal_{};
	NativeLadderPhase phase_ = NativeLadderPhase::None;
	float started_ = 0, lastTime_ = 0, endpoint_ = 0, dismountAt_ = 0;
	int dismount_ = 0;
	bool faceIn_ = true, walking_ = false, crouching_ = false, active_ = false;
	bool hasUpdated_ = false;
};
}
}
#endif
