#ifndef ASTRABOT_NAV_SPECIAL_TRAVERSAL_HPP
#define ASTRABOT_NAV_SPECIAL_TRAVERSAL_HPP

#include "astrabot/nav/nav_query.hpp"

#include <cstdint>

namespace astrabot
{
namespace nav
{
enum class SpecialTraversalKind
{
	Ladder,
	Door,
	NarrowPassage
};

enum class SpecialTraversalAvailability
{
	Unknown,
	Closed,
	Open
};

enum class SpecialTraversalPosture
{
	Standing,
	Crouching
};

enum class SpecialTraversalIntentPhase
{
	Enter,
	Maintain,
	Exit
};

enum class SpecialTraversalState
{
	Idle,
	Entering,
	Maintaining,
	Exiting,
	Completed,
	Recovering,
	Invalidated
};

enum class SpecialTraversalFailureReason
{
	None,
	UnknownAvailability,
	Unavailable,
	MissingContact,
	InsufficientClearance,
	NoProgress
};

enum class SpecialTraversalResult
{
	Ready,
	EnterIntent,
	MaintainIntent,
	ExitIntent,
	Completed,
	RecoverableFailure,
	TimedOut,
	Invalidated,
	InvalidObservation,
	Inactive,
	InvalidArgument,
	InvalidConfig,
	InvalidSnapshot,
	InvalidCorridor,
	InvalidCapability
};

struct SpecialTraversalPoint
{
	NavVector position;
	AreaId area;
};

struct SpecialTraversalCapability
{
	static constexpr float kMaximumRequiredClearance = 4096.0f;

	SpecialTraversalKind kind;
	SpecialTraversalPoint entry;
	SpecialTraversalPoint exit;
	std::uint8_t sourceDirection;
	float requiredClearance;
	SpecialTraversalPosture minimumPosture;
};

struct SpecialTraversalConfig
{
	static constexpr float kMaximumTolerance = 4096.0f;
	static constexpr float kMaximumSpeed = 1000.0f;
	static constexpr std::uint32_t kMaximumTraversalFrames = 4096U;
	static constexpr std::uint32_t kMaximumNoProgressSamples = 1024U;
	static constexpr float kMinimumProgressDistance = 0.01f;
	static constexpr float kDefaultPositionTolerance = 8.0f;
	static constexpr float kDefaultSpeed = 100.0f;
	static constexpr std::uint32_t kDefaultTraversalFrames = 4U;
	static constexpr std::uint32_t kDefaultNoProgressSamples = 3U;
	static constexpr float kDefaultMinimumProgressDistance = 0.5f;

	float positionTolerance;
	float maximumSpeed;
	std::uint32_t maximumTraversalFrames;
	std::uint32_t maximumNoProgressSamples;
	float minimumProgressDistance;
};

struct SpecialTraversalObservation
{
	NavVector position;
	std::uint32_t frame;
	std::uint32_t actorGeneration;
	float standingClearance;
	float crouchingClearance;
	SpecialTraversalAvailability availability;
	bool ladderContact;
	bool entryConfirmed;
	bool exitConfirmed;
};

struct SpecialTraversalIntent
{
	SpecialTraversalKind kind;
	SpecialTraversalIntentPhase phase;
	SpecialTraversalPosture posture;
	NavVector direction;
	float speed;
	AreaId entryArea;
	AreaId exitArea;
	std::uint8_t sourceDirection;
};

class SpecialTraversalController
{
public:
	SpecialTraversalController();
	explicit SpecialTraversalController(const SpecialTraversalConfig &config);

	SpecialTraversalResult start(
		const NavCorridor &corridor,
		const SpecialTraversalCapability &capability,
		std::uint32_t actorGeneration);
	SpecialTraversalResult update(
		const NavSnapshot &snapshot,
		const SpecialTraversalObservation &observation,
		SpecialTraversalIntent *intent);
	bool isActive() const;
	SpecialTraversalState state() const;
	SpecialTraversalFailureReason failureReason() const;

private:
	static bool isValidConfig(const SpecialTraversalConfig &config);
	static bool isFiniteVector(const NavVector &value);
	static bool isValidCapability(
		const SpecialTraversalCapability &capability);
	static bool isFiniteObservation(
		const SpecialTraversalObservation &observation);
	static float horizontalDistance(
		const NavVector &from,
		const NavVector &to);
	static float floorDistance(const NavArea &area, float height);
	static bool isPointInArea(
		const NavArea &area,
		const SpecialTraversalPoint &point,
		float verticalTolerance);
	static bool isNearPoint(
		const NavVector &position,
		const SpecialTraversalPoint &point,
		float tolerance);
	static bool hasDirectedLink(
		const NavArea &area,
		std::uint8_t direction,
		AreaId target);
	bool validateCapability(const NavSnapshot &snapshot) const;
	bool isAtPoint(
		const NavSnapshot &snapshot,
		const NavVector &position,
		const SpecialTraversalPoint &point) const;
	bool selectPosture(
		const SpecialTraversalObservation &observation,
		SpecialTraversalPosture *posture) const;
	SpecialTraversalResult recover(
		SpecialTraversalFailureReason reason);
	SpecialTraversalResult emitIntent(
		const SpecialTraversalObservation &observation,
		const SpecialTraversalPoint &target,
		SpecialTraversalIntentPhase phase,
		SpecialTraversalPosture posture,
		SpecialTraversalIntent *intent) const;
	SpecialTraversalResult updateEntering(
		const NavSnapshot &snapshot,
		const SpecialTraversalObservation &observation,
		SpecialTraversalPosture posture,
		SpecialTraversalIntent *intent);
	SpecialTraversalResult updateMaintaining(
		const NavSnapshot &snapshot,
		const SpecialTraversalObservation &observation,
		SpecialTraversalPosture posture,
		SpecialTraversalIntent *intent);
	SpecialTraversalResult updateExiting(
		const NavSnapshot &snapshot,
		const SpecialTraversalObservation &observation,
		SpecialTraversalPosture posture,
		SpecialTraversalIntent *intent);

	SpecialTraversalConfig config_;
	SpecialTraversalCapability capability_;
	AreaId firstNextArea_;
	std::uint64_t navRevision_;
	std::uint32_t mapGeneration_;
	std::uint32_t actorGeneration_;
	std::uint32_t startFrame_;
	std::uint32_t noProgressSamples_;
	NavVector lastPosition_;
	SpecialTraversalState state_;
	SpecialTraversalFailureReason failureReason_;
	bool hasStartFrame_;
	bool hasLastPosition_;
	bool active_;
};
}
}

#endif
