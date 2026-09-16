#ifndef ASTRABOT_NAV_JUMP_DROP_HPP
#define ASTRABOT_NAV_JUMP_DROP_HPP

#include "astrabot/nav/nav_query.hpp"

#include <cstdint>

namespace astrabot
{
namespace nav
{
enum class JumpDropKind
{
	Jump,
	Drop
};

enum class JumpDropResult
{
	Ready,
	Emitted,
	Landed,
	Unsafe,
	TimedOut,
	Invalidated,
	InvalidObservation,
	Inactive,
	InvalidArgument,
	InvalidConfig,
	InvalidSnapshot,
	InvalidCorridor,
	InvalidEnvelope
};

struct JumpDropPoint
{
	NavVector position;
	AreaId area;
};

struct JumpDropDamageRisk
{
	static constexpr float kMaximumSafeDropHeight = 4096.0f;
	static constexpr float kMaximumLandingDamage = 100.0f;

	float maximumSafeDropHeight;
	float maximumLandingDamage;
};

struct JumpDropEnvelope
{
	static constexpr float kMaximumRise = 4096.0f;
	static constexpr float kMaximumDrop = 4096.0f;
	static constexpr float kMaximumHorizontalReach = 4096.0f;

	JumpDropKind kind;
	JumpDropPoint launch;
	JumpDropPoint landing;
	float maximumRise;
	float maximumDrop;
	float horizontalReach;
	JumpDropDamageRisk damageRisk;
};

struct JumpDropConfig
{
	static constexpr float kMaximumTolerance = 4096.0f;
	static constexpr float kMaximumIntentSpeed = 1000.0f;
	static constexpr std::uint32_t kMaximumTraversalFrames = 4096U;
	static constexpr float kDefaultLaunchHorizontalTolerance = 8.0f;
	static constexpr float kDefaultLaunchVerticalTolerance = 8.0f;
	static constexpr float kDefaultLandingTolerance = 8.0f;
	static constexpr float kDefaultIntentSpeed = 100.0f;
	static constexpr std::uint32_t kDefaultTraversalFrames = 4U;

	float launchHorizontalTolerance;
	float launchVerticalTolerance;
	float landingTolerance;
	float maximumIntentSpeed;
	std::uint32_t maximumTraversalFrames;
};

struct JumpDropObservation
{
	NavVector position;
	std::uint32_t frame;
	std::uint32_t actorGeneration;
	bool airborne;
	bool landingConfirmed;
	bool hasLandingDamage;
	float landingDamage;
};

struct JumpDropIntent
{
	JumpDropKind kind;
	NavVector direction;
	float speed;
	AreaId launchArea;
	AreaId landingArea;
};

class JumpDropController
{
public:
	JumpDropController();
	explicit JumpDropController(const JumpDropConfig &config);

	JumpDropResult start(
		const NavCorridor &corridor,
		const JumpDropEnvelope &envelope,
		std::uint32_t actorGeneration);
	JumpDropResult update(
		const NavSnapshot &snapshot,
		const JumpDropObservation &observation,
		JumpDropIntent *intent);
	bool isActive() const;

private:
	static bool isValidConfig(const JumpDropConfig &config);
	static bool isFiniteVector(const NavVector &value);
	static bool isValidEnvelope(const JumpDropEnvelope &envelope);
	static bool exceedsEnvelope(const JumpDropEnvelope &envelope);
	static bool isFiniteObservation(
		const JumpDropObservation &observation);
	static float floorDistance(const NavArea &area, float height);
	static float horizontalDistance(
		const NavVector &from,
		const NavVector &to);
	static bool isNearPoint(
		const NavVector &position,
		const JumpDropPoint &point,
		float horizontalTolerance,
		float verticalTolerance);
	static bool isPointInArea(
		const NavArea &area,
		const JumpDropPoint &point,
		float verticalTolerance);
	bool validateEnvelopeAreas(const NavSnapshot &snapshot) const;
	JumpDropResult emitLaunchIntent(
		const JumpDropObservation &observation,
		JumpDropIntent *intent);

	JumpDropConfig config_;
	JumpDropEnvelope envelope_;
	std::uint64_t navRevision_;
	std::uint32_t mapGeneration_;
	std::uint32_t actorGeneration_;
	std::uint32_t launchFrame_;
	bool hasLaunchFrame_;
	bool launchEmitted_;
	bool active_;
};
}
}

#endif
